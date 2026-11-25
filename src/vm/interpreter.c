/**
 * @file interpreter.c
 * @brief Laço *fetch–decode–execute* com a tabela de despacho `handlers[256]`
 *        (D18), chamada de métodos e desvio para tratadores de exceção (I-03, I-09).
 */
#include "bytecode/opcodes.h"
#include "classfile/constant_pool.h"
#include "vm/class_loader.h"
#include "vm/descriptor.h"
#include "vm/frame.h"
#include "vm/interpreter.h"

/** @brief Tabela de despacho indexada pelo opcode (D18). */
static OpcodeHandler handlers[256];
/** @brief Indica se a tabela já foi montada. */
static bool handlers_ready = false;

/**
 * @brief Instrução sem implementação: erro fatal com nome e *offset* (D18).
 *
 * @details **Algoritmo:** consulta o mnemônico na tabela de opcodes e
 * chama vm_fatal().
 *
 * @param[in,out] frame Frame em execução.
 */
static void op_unimplemented(Frame *frame)
{
    uint8_t opcode = frame->code[frame->pc];
    const char *mnemonic = opcode_info(opcode)->mnemonic;
    vm_fatal(frame->vm, "instrucao '%s' (0x%02X) nao suportada em pc=%lu",
             mnemonic != NULL ? mnemonic : "?", (unsigned)opcode,
             (unsigned long)frame->pc);
}

/**
 * @brief Monta a tabela de despacho na primeira utilização.
 *
 * @details **Algoritmo:** preenche as 256 posições com op_unimplemented()
 * e chama a função de registro de cada família (`exec_*_register`).
 */
static void build_handlers(void)
{
    int i;
    if (handlers_ready) {
        return;
    }
    for (i = 0; i < 256; i++) {
        handlers[i] = op_unimplemented;
    }
    exec_load_store_register(handlers);
    exec_stack_register(handlers);
    exec_int_register(handlers);
    exec_float_register(handlers);
    exec_branch_register(handlers);
    exec_object_register(handlers);
    exec_invoke_register(handlers);
    exec_array_register(handlers);
    handlers_ready = true;
}

OpcodeHandler interpreter_handler(uint8_t opcode)
{
    build_handlers();
    return handlers[opcode];
}

bool interpreter_is_unimplemented(OpcodeHandler handler)
{
    return handler == op_unimplemented;
}

/**
 * @brief Procura, no método do frame, o tratador da exceção pendente.
 *
 * @details
 * **Algoritmo (JVMS §2.10):** percorre a `exception_table` em ordem; a
 * primeira entrada cuja faixa `[start_pc, end_pc)` contém @p pc e cujo
 * `catch_type` é 0 (`finally`) ou uma superclasse da exceção é escolhida:
 * esvazia a pilha, empilha a exceção e desvia para `handler_pc`.
 *
 * @param[in,out] frame Frame.
 * @param[in]     pc    Offset da instrução que lançou a exceção.
 * @return `true` se encontrou um tratador.
 */
static bool handle_exception(Frame *frame, uint32_t pc)
{
    Vm *vm = frame->vm;
    const ClassFile *cf = frame->class_ref->class_file;
    const CodeAttribute *code = frame->code_attr;
    uint16_t i;
    for (i = 0; i < code->exception_table_length; i++) {
        const ExceptionTableEntry *entry = &code->exception_table[i];
        if (pc < entry->start_pc || pc >= entry->end_pc) {
            continue;
        }
        if (entry->catch_type != 0) {
            const char *catch_name = cp_get_class_name(cf, entry->catch_type);
            if (catch_name == NULL ||
                !class_is_assignable(vm, vm->exception->class_ref, catch_name)) {
                if (vm->fatal) {
                    return false;
                }
                continue;
            }
        }
        frame->sp = 0;
        frame_push_ref(frame, vm->exception);
        vm->exception = NULL;
        frame->pc = entry->handler_pc;
        return !vm->fatal;
    }
    return false;
}

bool interpreter_step(Frame *frame)
{
    Vm *vm = frame->vm;
    uint32_t pc = frame->pc;
    uint32_t length = bytecode_instruction_length(frame->code, frame->code_length, pc);
    build_handlers();
    if (vm->max_steps != 0 && ++vm->steps > vm->max_steps) {
        vm_fatal(vm, "limite de %lu instrucoes atingido", (unsigned long)vm->max_steps);
        return false;
    }
    if (length == 0) {
        vm_fatal(vm, "instrucao invalida ou fora do codigo em pc=%lu", (unsigned long)pc);
        return false;
    }
    frame->next_pc = pc + length;
    handlers[frame->code[pc]](frame);
    if (vm->fatal) {
        return false;
    }
    frame->pc = frame->next_pc;
    if (vm->exception != NULL) {
        return handle_exception(frame, pc);
    }
    return true;
}

/**
 * @brief Devolve o atributo `Code` de um método.
 *
 * @details **Algoritmo:** busca linear entre os atributos do método.
 *
 * @param[in] method Método.
 * @return Atributo `Code`; NULL se o método é abstrato ou nativo.
 */
static const CodeAttribute *find_code(const MethodInfo *method)
{
    uint16_t i;
    for (i = 0; i < method->attributes_count; i++) {
        if (method->attributes[i].kind == ATTR_CODE) {
            return method->attributes[i].info.code;
        }
    }
    return NULL;
}

bool interpreter_call(Vm *vm, const MethodRef *ref, const char *descriptor,
                      const Slot *args, const uint8_t *tags, uint16_t arg_count,
                      Slot *result)
{
    const CodeAttribute *code;
    Frame *frame;
    uint16_t i;
    bool ok = true;
    result[0] = result[1] = 0;
    if (ref->native != NULL) {
        ref->native(vm, descriptor, args, result);
        return !vm->fatal && vm->exception == NULL;
    }
    code = find_code(ref->method);
    if (code == NULL) {
        vm_fatal(vm, "metodo abstrato ou nativo sem implementacao em '%s'", ref->owner->name);
        return false;
    }
    if (vm->depth >= VM_MAX_DEPTH) {
        vm_throw_new(vm, "java/lang/StackOverflowError", NULL);
        return false;
    }
    if (arg_count > code->max_locals) {
        vm_fatal(vm, "argumentos demais para max_locals em '%s'", ref->owner->name);
        return false;
    }
    frame = frame_create(vm, ref->owner, ref->method, code);
    if (frame == NULL) {
        vm_fatal(vm, "memoria insuficiente para o frame");
        return false;
    }
    for (i = 0; i < arg_count; i++) {
        frame->locals[i] = args[i];
        frame->local_tags[i] = tags[i];
    }
    vm->depth++;
    while (ok && !frame->returned) {
        ok = interpreter_step(frame);
    }
    vm->depth--;
    if (ok && descriptor_is_ref(descriptor_return(descriptor)) &&
        frame->result_tag != TAG_REF) {
        vm_fatal(vm, "metodo de '%s' nao retornou uma referencia", ref->owner->name);
        ok = false;
    }
    if (ok) {
        result[0] = frame->result[0];
        result[1] = frame->result[1];
    }
    frame_free(frame);
    return ok;
}

bool interpreter_call_virtual(Vm *vm, Object *object, const char *name,
                              const char *descriptor, Slot *result)
{
    RuntimeClass *start = object->kind == OBJECT_ARRAY ? class_load(vm, "java/lang/Object")
                                                       : object->class_ref;
    MethodRef ref;
    Slot arg = (Slot)(uintptr_t)object;
    uint8_t tag = TAG_REF;
    if (start == NULL) {
        return false;
    }
    if (!class_find_method(start, name, descriptor, &ref)) {
        vm_fatal(vm, "metodo %s%s nao encontrado em '%s'", name, descriptor, start->name);
        return false;
    }
    return interpreter_call(vm, &ref, descriptor, &arg, &tag, 1, result);
}
