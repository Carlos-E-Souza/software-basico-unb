/**
 * @file exec_invoke.c
 * @brief Invocação de métodos: `invokevirtual`, `invokespecial`,
 *        `invokestatic`, `invokeinterface` e `invokedynamic` (sem suporte,
 *        D9) (I-05, I-06).
 *
 * @details `invokevirtual` e `invokeinterface` usam a mesma busca: sobem a
 * hierarquia a partir da classe real do objeto procurando nome + descritor
 * (D19). `invokestatic` e `invokespecial` procuram a partir da classe do
 * `Methodref`.
 */
#include "classfile/constant_pool.h"
#include "vm/class_loader.h"
#include "vm/descriptor.h"
#include "vm/frame.h"
#include "vm/interpreter.h"

/** @brief Tipo de invocação. */
typedef enum InvokeKind {
    INVOKE_VIRTUAL,  /**< `invokevirtual` e `invokeinterface`. */
    INVOKE_SPECIAL,  /**< `invokespecial`. */
    INVOKE_STATIC    /**< `invokestatic`. */
} InvokeKind;

/**
 * @brief Confere as etiquetas dos argumentos com o descritor.
 *
 * @details **Algoritmo:** o receptor (se houver) e cada parâmetro de tipo
 * referência devem ter etiqueta #TAG_REF; isso garante que nenhum inteiro
 * chegue a um método como ponteiro (I-10).
 *
 * @param[in] descriptor   Descritor do método.
 * @param[in] tags         Etiquetas dos argumentos.
 * @param[in] has_receiver `true` se `tags[0]` é o `this`.
 * @return `true` se os tipos conferem.
 */
static bool check_arg_tags(const char *descriptor, const uint8_t *tags, bool has_receiver)
{
    const char *p = descriptor + 1;
    int slot = 0;
    if (has_receiver) {
        if (tags[0] != TAG_REF) {
            return false;
        }
        slot = 1;
    }
    while (*p != ')') {
        if (descriptor_is_ref(p) && tags[slot] != TAG_REF) {
            return false;
        }
        slot += descriptor_type_slots(p);
        p = descriptor_skip_type(p);
    }
    return true;
}

/**
 * @brief Procura o método a ser chamado conforme o tipo de invocação.
 *
 * @details **Algoritmo:** virtual: a partir da classe do receptor (arrays
 * usam `java/lang/Object`); especial e estático: a partir da classe do
 * `Methodref`. Ser estático (`ACC_STATIC` ou nativo estático) precisa
 * combinar com o tipo de invocação.
 *
 * @param[in,out] frame      Frame.
 * @param[in]     kind       Tipo de invocação.
 * @param[in]     class_name Classe do `Methodref`.
 * @param[in]     receiver   Receptor (não NULL nas invocações de instância).
 * @param[in]     name       Nome do método.
 * @param[in]     descriptor Descritor.
 * @param[out]    ref        Método encontrado.
 * @return `true` se encontrou (senão já houve erro fatal).
 */
static bool find_target(Frame *frame, InvokeKind kind, const char *class_name,
                        Object *receiver, const char *name, const char *descriptor,
                        MethodRef *ref)
{
    RuntimeClass *start;
    if (kind == INVOKE_VIRTUAL) {
        start = receiver->kind == OBJECT_ARRAY ? class_load(frame->vm, "java/lang/Object")
                                               : receiver->class_ref;
    } else {
        start = class_load(frame->vm, class_name);
    }
    if (start == NULL) {
        return false;
    }
    if (!class_find_method(start, name, descriptor, ref)) {
        vm_fatal(frame->vm, "metodo %s.%s%s nao encontrado", class_name, name, descriptor);
        return false;
    }
    if (ref->is_static != (kind == INVOKE_STATIC)) {
        vm_fatal(frame->vm, "metodo %s.%s%s com ACC_STATIC incompativel", class_name, name,
                 descriptor);
        return false;
    }
    return true;
}

/**
 * @brief Implementação comum das instruções `invoke*`.
 *
 * @details
 * **Algoritmo:**
 * 1. Resolve o `Methodref`/`InterfaceMethodref` do operando;
 * 2. Conta os slots dos argumentos (+1 para `this`) e confere as etiquetas;
 * 3. Receptor `null` → `NullPointerException`;
 * 4. Procura o método (find_target()); em `invokestatic`, inicializa a classe;
 * 5. Retira os argumentos da pilha, chama interpreter_call() e empilha o
 *    retorno conforme o descritor.
 *
 * @param[in,out] frame Frame.
 * @param[in]     kind  Tipo de invocação.
 */
static void invoke(Frame *frame, InvokeKind kind)
{
    const ClassFile *cf = frame->class_ref->class_file;
    uint16_t index = frame_u2(frame, 1);
    ConstantTag tag = cp_tag(cf, index);
    const char *class_name, *name, *descriptor, *ret;
    int slots;
    uint16_t total;
    const Slot *args;
    Object *receiver = NULL;
    MethodRef ref;
    Slot result[2];
    if ((tag != CONSTANT_Methodref && tag != CONSTANT_InterfaceMethodref) ||
        !cp_get_ref(cf, index, &class_name, &name, &descriptor) ||
        (slots = descriptor_arg_slots(descriptor)) < 0 ||
        (name[0] == '<' && (kind != INVOKE_SPECIAL || name[1] != 'i'))) {
        vm_fatal(frame->vm, "Methodref #%u invalido", (unsigned)index);
        return;
    }
    total = (uint16_t)(slots + (kind == INVOKE_STATIC ? 0 : 1));
    if (total > frame->sp) {
        vm_fatal(frame->vm, "argumentos insuficientes na pilha para %s", name);
        return;
    }
    args = &frame->stack[frame->sp - total];
    if (!check_arg_tags(descriptor, &frame->stack_tags[frame->sp - total],
                        kind != INVOKE_STATIC)) {
        vm_fatal(frame->vm, "argumentos de tipo invalido para %s%s", name, descriptor);
        return;
    }
    if (kind != INVOKE_STATIC) {
        receiver = (Object *)(uintptr_t)args[0];
        if (receiver == NULL) {
            frame->sp = (uint16_t)(frame->sp - total);
            vm_throw_new(frame->vm, "java/lang/NullPointerException", NULL);
            return;
        }
    }
    if (!find_target(frame, kind, class_name, receiver, name, descriptor, &ref) ||
        (kind == INVOKE_STATIC && !class_initialize(frame->vm, ref.owner))) {
        return;
    }
    frame->sp = (uint16_t)(frame->sp - total);
    if (!interpreter_call(frame->vm, &ref, descriptor, args,
                          &frame->stack_tags[frame->sp], total, result)) {
        return;
    }
    ret = descriptor_return(descriptor);
    if (descriptor_is_ref(ret)) {
        frame_push(frame, result[0], TAG_REF);
    } else if (ret[0] != 'V') {
        frame_push(frame, result[0], TAG_VALUE);
        if (descriptor_type_slots(ret) == 2) {
            frame_push(frame, result[1], TAG_VALUE);
        }
    }
}

/**
 * @brief `invokevirtual` e `invokeinterface` (mesma busca, D19).
 * @details **Algoritmo:** invoke() com #INVOKE_VIRTUAL.
 * @param[in,out] frame Frame.
 */
static void op_invokevirtual(Frame *frame)
{
    invoke(frame, INVOKE_VIRTUAL);
}

/**
 * @brief `invokespecial`: construtores, métodos privados e `super.metodo()`.
 * @details **Algoritmo:** invoke() com #INVOKE_SPECIAL.
 * @param[in,out] frame Frame.
 */
static void op_invokespecial(Frame *frame)
{
    invoke(frame, INVOKE_SPECIAL);
}

/**
 * @brief `invokestatic`.
 * @details **Algoritmo:** invoke() com #INVOKE_STATIC.
 * @param[in,out] frame Frame.
 */
static void op_invokestatic(Frame *frame)
{
    invoke(frame, INVOKE_STATIC);
}

/**
 * @brief `invokedynamic`: fora do escopo (D9), erro fatal.
 * @details **Algoritmo:** vm_fatal() com a mensagem "não suportado".
 * @param[in,out] frame Frame.
 */
static void op_invokedynamic(Frame *frame)
{
    vm_fatal(frame->vm, "invokedynamic nao suportado (lambdas e afins estao fora do escopo, D9)");
}

void exec_invoke_register(OpcodeHandler *table)
{
    table[0xB6] = op_invokevirtual;
    table[0xB7] = op_invokespecial;
    table[0xB8] = op_invokestatic;
    table[0xB9] = op_invokevirtual;
    table[0xBA] = op_invokedynamic;
}
