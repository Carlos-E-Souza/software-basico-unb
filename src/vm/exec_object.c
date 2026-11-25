/**
 * @file exec_object.c
 * @brief Objetos e campos: `new`, `getfield`, `putfield`, `getstatic`,
 *        `putstatic`, `instanceof`, `checkcast`, `athrow`, `monitorenter`
 *        e `monitorexit` (I-06, I-09, I-10).
 *
 * @details Campos são localizados por nome + descritor subindo a
 * hierarquia a cada execução (D19, D21). Os monitores não têm efeito
 * (sem threads, D9).
 */
#include <string.h>

#include "classfile/constant_pool.h"
#include "vm/class_loader.h"
#include "vm/descriptor.h"
#include "vm/frame.h"
#include "vm/heap.h"
#include "vm/interpreter.h"

/** @brief Tamanho do buffer da mensagem de `ClassCastException`. */
#define CAST_MESSAGE_SIZE 600

/**
 * @brief Resolve o `Fieldref` do operando u2 da instrução.
 *
 * @details **Algoritmo:** exige tag `CONSTANT_Fieldref`, lê classe, nome e
 * descritor e carrega a classe; qualquer falha é erro fatal.
 *
 * @param[in,out] frame      Frame.
 * @param[out]    name       Nome do campo.
 * @param[out]    descriptor Descritor do campo.
 * @return Classe do `Fieldref`; NULL em caso de erro fatal.
 */
static RuntimeClass *resolve_field(Frame *frame, const char **name, const char **descriptor)
{
    const ClassFile *cf = frame->class_ref->class_file;
    uint16_t index = frame_u2(frame, 1);
    const char *class_name;
    if (cp_tag(cf, index) != CONSTANT_Fieldref ||
        !cp_get_ref(cf, index, &class_name, name, descriptor) ||
        descriptor_skip_type(*descriptor) == NULL) {
        vm_fatal(frame->vm, "Fieldref #%u invalido", (unsigned)index);
        return NULL;
    }
    return class_load(frame->vm, class_name);
}

/**
 * @brief Empilha o valor de um campo conforme o descritor.
 * @details **Algoritmo:** referência → 1 slot #TAG_REF; `J`/`D` → 2 slots; demais → 1 slot.
 * @param[in,out] frame      Frame.
 * @param[in]     descriptor Descritor do campo.
 * @param[in]     field      Campo.
 */
static void push_field(Frame *frame, const char *descriptor, const Field *field)
{
    if (descriptor_is_ref(descriptor)) {
        frame_push(frame, field->value[0], TAG_REF);
    } else {
        frame_push(frame, field->value[0], TAG_VALUE);
        if (descriptor_type_slots(descriptor) == 2) {
            frame_push(frame, field->value[1], TAG_VALUE);
        }
    }
}

/**
 * @brief Desempilha um valor para um campo conforme o descritor.
 * @details **Algoritmo:** referência → frame_pop_ref(); `J`/`D` → baixo e alto; demais → 1 slot.
 * @param[in,out] frame      Frame.
 * @param[in]     descriptor Descritor do campo.
 * @param[out]    value      Valor desempilhado (2 slots).
 */
static void pop_field(Frame *frame, const char *descriptor, Slot *value)
{
    if (descriptor_is_ref(descriptor)) {
        value[0] = (Slot)(uintptr_t)frame_pop_ref(frame);
    } else if (descriptor_type_slots(descriptor) == 2) {
        value[1] = frame_pop(frame, NULL);
        value[0] = frame_pop(frame, NULL);
    } else {
        value[0] = frame_pop(frame, NULL);
    }
}

/**
 * @brief `new`: cria uma instância da classe do operando.
 *
 * @details **Algoritmo:** carrega a classe (arrays, interfaces e classes
 * abstratas são erro fatal), inicializa-a (`<clinit>`, I-02), cria o
 * objeto com os campos zerados e empilha a referência.
 *
 * @param[in,out] frame Frame.
 */
static void op_new(Frame *frame)
{
    const char *name = cp_get_class_name(frame->class_ref->class_file, frame_u2(frame, 1));
    RuntimeClass *class_ref = name != NULL && name[0] != '[' ? class_load(frame->vm, name) : NULL;
    const Object *object;
    if (class_ref == NULL) {
        vm_fatal(frame->vm, "new de classe invalida");
        return;
    }
    if (class_ref->class_file != NULL &&
        (class_ref->class_file->access_flags & (ACC_INTERFACE | ACC_ABSTRACT)) != 0) {
        vm_fatal(frame->vm, "new de classe abstrata ou interface '%s'", name);
        return;
    }
    if (!class_initialize(frame->vm, class_ref)) {
        return;
    }
    object = heap_new_object(frame->vm, class_ref);
    if (object != NULL) {
        frame_push_ref(frame, object);
    }
}

/**
 * @brief `getfield`/`putfield`: lê ou escreve um campo de instância.
 *
 * @details
 * **Algoritmo:**
 * 1. Resolve o `Fieldref` e procura a declaração na hierarquia;
 * 2. `putfield` desempilha o valor; ambos desempilham o objeto
 *    (`null` → `NullPointerException`);
 * 3. Localiza o campo no objeto (object_field()) e lê/escreve o valor.
 *
 * @param[in,out] frame Frame.
 */
static void op_field(Frame *frame)
{
    bool is_put = frame_u1(frame, 0) == 0xB5;
    const char *name, *descriptor;
    const RuntimeClass *class_ref = resolve_field(frame, &name, &descriptor);
    const Field *decl = class_ref != NULL ? class_find_field(class_ref, name, descriptor) : NULL;
    Slot value[2] = {0, 0};
    Object *object;
    Field *field;
    if (decl == NULL) {
        vm_fatal(frame->vm, "campo %s:%s nao encontrado", class_ref != NULL ? name : "?",
                 class_ref != NULL ? descriptor : "?");
        return;
    }
    if (is_put) {
        pop_field(frame, descriptor, value);
    }
    object = frame_pop_ref(frame);
    if (frame->vm->fatal) {
        return;
    }
    if (object == NULL) {
        vm_throw_new(frame->vm, "java/lang/NullPointerException", NULL);
        return;
    }
    field = object_field(object, decl);
    if (field == NULL) {
        vm_fatal(frame->vm, "objeto sem o campo '%s'", name);
    } else if (is_put) {
        field->value[0] = value[0];
        field->value[1] = value[1];
    } else {
        push_field(frame, descriptor, field);
    }
}

/**
 * @brief `getstatic`/`putstatic`: lê ou escreve um campo estático.
 *
 * @details **Algoritmo:** resolve o `Fieldref`, procura o campo na classe,
 * interfaces e superclasses, inicializa a classe que o declara e
 * empilha/desempilha o valor.
 *
 * @param[in,out] frame Frame.
 */
static void op_static(Frame *frame)
{
    const char *name, *descriptor;
    RuntimeClass *owner = NULL;
    RuntimeClass *class_ref = resolve_field(frame, &name, &descriptor);
    Field *field = class_ref != NULL
                       ? class_find_static(frame->vm, class_ref, name, descriptor, &owner)
                       : NULL;
    if (field == NULL) {
        vm_fatal(frame->vm, "campo estatico %s:%s nao encontrado",
                 class_ref != NULL ? name : "?", class_ref != NULL ? descriptor : "?");
        return;
    }
    if (!class_initialize(frame->vm, owner)) {
        return;
    }
    if (frame_u1(frame, 0) == 0xB3) {
        pop_field(frame, descriptor, field->value);
    } else {
        push_field(frame, descriptor, field);
    }
}

/**
 * @brief Copia um nome interno trocando `/` por `.` (formato das mensagens).
 * @details **Algoritmo:** cópia caractere a caractere, truncada em @p size.
 * @param[out] buffer Destino.
 * @param[in]  size   Tamanho do destino.
 * @param[in]  name   Nome interno.
 */
static void dotted_name(char *buffer, size_t size, const char *name)
{
    size_t i;
    for (i = 0; i + 1 < size && name[i] != '\0'; i++) {
        buffer[i] = name[i] == '/' ? '.' : name[i];
    }
    buffer[i] = '\0';
}

/**
 * @brief `instanceof`/`checkcast`: testa o tipo de uma referência.
 *
 * @details
 * **Algoritmo:** `instanceof` desempilha a referência e empilha 1 se não
 * for `null` e for instância do tipo, senão 0. `checkcast` mantém a
 * referência e lança `ClassCastException` (`A cannot be cast to B`) se
 * ela não for `null` nem instância do tipo.
 *
 * @param[in,out] frame Frame.
 */
static void op_type_check(Frame *frame)
{
    const char *target = cp_get_class_name(frame->class_ref->class_file, frame_u2(frame, 1));
    const Object *object = frame_pop_ref(frame);
    bool is_instance;
    if (target == NULL) {
        vm_fatal(frame->vm, "tipo invalido em instanceof/checkcast");
        return;
    }
    is_instance = object != NULL && object_is_instance(frame->vm, object, target);
    if (frame_u1(frame, 0) == 0xC1) {
        frame_push_int(frame, is_instance ? 1 : 0);
        return;
    }
    frame_push_ref(frame, object);
    if (object != NULL && !is_instance && !frame->vm->fatal) {
        char message[CAST_MESSAGE_SIZE];
        size_t used;
        dotted_name(message, sizeof message / 2, object_type_name(object));
        used = strlen(message);
        strcpy(message + used, " cannot be cast to ");
        used = strlen(message);
        dotted_name(message + used, sizeof message - used, target);
        vm_throw_new(frame->vm, "java/lang/ClassCastException", message);
    }
}

/**
 * @brief `athrow`: lança a exceção do topo da pilha.
 * @details **Algoritmo:** `null` → `NullPointerException`; objeto que não
 * herda de `Throwable` → erro fatal; senão vm_throw().
 * @param[in,out] frame Frame.
 */
static void op_athrow(Frame *frame)
{
    Object *exception = frame_pop_ref(frame);
    if (frame->vm->fatal) {
        return;
    }
    if (exception == NULL) {
        vm_throw_new(frame->vm, "java/lang/NullPointerException", NULL);
    } else if (exception->kind != OBJECT_INSTANCE ||
               !class_is_assignable(frame->vm, exception->class_ref, "java/lang/Throwable")) {
        vm_fatal(frame->vm, "athrow de objeto que nao e Throwable");
    } else {
        vm_throw(frame->vm, exception);
    }
}

/**
 * @brief `monitorenter`/`monitorexit`: sem efeito (monothread, D9).
 * @details **Algoritmo:** desempilha a referência; `null` → `NullPointerException`.
 * @param[in,out] frame Frame.
 */
static void op_monitor(Frame *frame)
{
    if (frame_pop_ref(frame) == NULL && !frame->vm->fatal) {
        vm_throw_new(frame->vm, "java/lang/NullPointerException", NULL);
    }
}

void exec_object_register(OpcodeHandler *table)
{
    table[0xB2] = table[0xB3] = op_static;
    table[0xB4] = table[0xB5] = op_field;
    table[0xBB] = op_new;
    table[0xBF] = op_athrow;
    table[0xC0] = table[0xC1] = op_type_check;
    table[0xC2] = table[0xC3] = op_monitor;
}
