/**
 * @file native_object.c
 * @brief Métodos nativos de `java/lang/Object` e de `java/lang/Throwable`
 *        (herdados por todas as exceções, D23) (I-08, I-09).
 */
#include <stdio.h>
#include <string.h>

#include "vm/heap.h"
#include "vm/interpreter.h"
#include "vm/natives.h"

/** @brief Tamanho do buffer de `Object.toString()`. */
#define OBJECT_TEXT_SIZE 600

/**
 * @brief Monta, em @p buffer, o nome de um tipo com `.` no lugar de `/`.
 * @details **Algoritmo:** cópia caractere a caractere, truncada em @p size.
 * @param[out] buffer Destino.
 * @param[in]  size   Tamanho do destino.
 * @param[in]  name   Nome interno.
 */
static void dotted(char *buffer, size_t size, const char *name)
{
    size_t i;
    for (i = 0; i + 1 < size && name[i] != '\0'; i++) {
        buffer[i] = name[i] == '/' ? '.' : name[i];
    }
    buffer[i] = '\0';
}

/**
 * @brief `Object.<init>()V`: não faz nada.
 * @details **Algoritmo:** nenhum.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       Argumentos (`this`).
 * @param[out]    result     Sem retorno.
 */
static void object_init(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    (void)vm;
    (void)descriptor;
    (void)args;
    (void)result;
}

/**
 * @brief `Object.equals(Object)Z`: igualdade de referências.
 * @details **Algoritmo:** compara `args[0]` e `args[1]`.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this` e o outro objeto.
 * @param[out]    result     1 se forem o mesmo objeto, senão 0.
 */
static void object_equals(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    (void)vm;
    (void)descriptor;
    result[0] = args[0] == args[1] ? 1u : 0u;
}

/**
 * @brief `Object.hashCode()I`: identidade do objeto.
 * @details **Algoritmo:** usa o endereço do objeto (32 bits, D16).
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this`.
 * @param[out]    result     Código *hash*.
 */
static void object_hash_code(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    (void)vm;
    (void)descriptor;
    result[0] = args[0];
}

/**
 * @brief `Object.toString()`: `nome.da.Classe@hash` (hash em hexadecimal).
 * @details **Algoritmo:** chama o `hashCode()` virtual (pode ter sido
 * reescrito) e monta o texto com o nome do tipo.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this`.
 * @param[out]    result     Objeto `String`.
 */
static void object_to_string(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    Object *self = (Object *)(uintptr_t)args[0];
    const char *name = self->kind == OBJECT_ARRAY ? self->as.array.type : self->class_ref->name;
    char text[OBJECT_TEXT_SIZE];
    Slot hash[2];
    (void)descriptor;
    if (!interpreter_call_virtual(vm, self, "hashCode", "()I", hash)) {
        return;
    }
    dotted(text, OBJECT_TEXT_SIZE - 12, name);
    sprintf(text + strlen(text), "@%lx", (unsigned long)hash[0]);
    result[0] = (Slot)(uintptr_t)heap_new_string_utf8(vm, text);
}

/**
 * @brief `Throwable.<init>()V` e `Throwable.<init>(String)V`.
 * @details **Algoritmo:** com argumento, grava a mensagem no campo `detailMessage`.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor (`()V` ou `(Ljava/lang/String;)V`).
 * @param[in]     args       `this` e, opcionalmente, a mensagem.
 * @param[out]    result     Sem retorno.
 */
static void throwable_init(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    Field *field = natives_message_field((Object *)(uintptr_t)args[0]);
    (void)result;
    if (field == NULL) {
        vm_fatal(vm, "construtor de Throwable em objeto que nao e Throwable");
    } else if (descriptor[1] != ')') {
        field->value[0] = args[1];
    }
}

/**
 * @brief `Throwable.getMessage()`.
 * @details **Algoritmo:** devolve o campo `detailMessage` (pode ser `null`).
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this`.
 * @param[out]    result     Mensagem.
 */
static void throwable_get_message(Vm *vm, const char *descriptor, const Slot *args,
                                  Slot *result)
{
    Field *field = natives_message_field((Object *)(uintptr_t)args[0]);
    (void)descriptor;
    if (field == NULL) {
        vm_fatal(vm, "getMessage em objeto que nao e Throwable");
        return;
    }
    result[0] = field->value[0];
}

/**
 * @brief `Throwable.toString()`: `nome.da.Classe` ou `nome.da.Classe: mensagem`.
 * @details **Algoritmo:** monta o nome com pontos e acrescenta `": "` e a
 * mensagem, se ela não for `null`.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this`.
 * @param[out]    result     Objeto `String`.
 */
static void throwable_to_string(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    Object *self = (Object *)(uintptr_t)args[0];
    Field *field = natives_message_field(self);
    Object *message = field != NULL ? (Object *)(uintptr_t)field->value[0] : NULL;
    char name[OBJECT_TEXT_SIZE];
    Object *text;
    (void)descriptor;
    dotted(name, sizeof name - 2, self->class_ref->name);
    if (message != NULL) {
        strcat(name, ": ");
    }
    text = heap_new_string_utf8(vm, name);
    if (text != NULL && message != NULL && natives_text(vm, message) != NULL) {
        natives_text_append(vm, &text->as.text, message->as.text.chars, message->as.text.length);
    }
    result[0] = (Slot)(uintptr_t)text;
}

/**
 * @brief `Throwable.printStackTrace()`: imprime `toString()` em `stderr`.
 * @details **Algoritmo:** chama o `toString()` virtual e escreve o texto
 * seguido de nova linha (sem a pilha de chamadas, D9).
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this`.
 * @param[out]    result     Sem retorno.
 */
static void throwable_print_stack_trace(Vm *vm, const char *descriptor, const Slot *args,
                                        Slot *result)
{
    Slot text[2];
    Object *string;
    (void)descriptor;
    (void)result;
    fflush(vm->out);
    if (!interpreter_call_virtual(vm, (Object *)(uintptr_t)args[0], "toString",
                                  "()Ljava/lang/String;", text)) {
        return;
    }
    string = (Object *)(uintptr_t)text[0];
    if (string != NULL && natives_text(vm, string) != NULL) {
        natives_write_utf16(stderr, string->as.text.chars, string->as.text.length);
    }
    fputc('\n', stderr);
}

const NativeEntry native_object_methods[] = {
    {"java/lang/Object", "<init>", "()V", object_init, false},
    {"java/lang/Object", "equals", "(Ljava/lang/Object;)Z", object_equals, false},
    {"java/lang/Object", "hashCode", "()I", object_hash_code, false},
    {"java/lang/Object", "toString", "()Ljava/lang/String;", object_to_string, false},
    {"java/lang/Throwable", "<init>", "()V", throwable_init, false},
    {"java/lang/Throwable", "<init>", "(Ljava/lang/String;)V", throwable_init, false},
    {"java/lang/Throwable", "getMessage", "()Ljava/lang/String;", throwable_get_message, false},
    {"java/lang/Throwable", "toString", "()Ljava/lang/String;", throwable_to_string, false},
    {"java/lang/Throwable", "printStackTrace", "()V", throwable_print_stack_trace, false},
    {NULL, NULL, NULL, NULL, false}
};
