/**
 * @file native_string.c
 * @brief Métodos nativos de `java/lang/String` e construtores,
 *        `length()` e `toString()` de `StringBuilder`/`StringBuffer`
 *        (I-08, D22). `append` e `valueOf` ficam em native_text.c.
 */
#include <stdio.h>
#include <string.h>

#include "vm/frame.h"
#include "vm/heap.h"
#include "vm/interpreter.h"
#include "vm/natives.h"

/**
 * @brief Devolve o texto de um argumento, lançando `NullPointerException` se nulo.
 * @details **Algoritmo:** `null` → exceção e NULL; senão natives_text().
 * @param[in,out] vm   Máquina virtual.
 * @param[in]     slot Slot com a referência.
 * @return Texto; NULL com exceção ou erro fatal.
 */
static Text *arg_text(Vm *vm, Slot slot)
{
    Object *object = (Object *)(uintptr_t)slot;
    if (object == NULL) {
        vm_throw_new(vm, "java/lang/NullPointerException", NULL);
        return NULL;
    }
    return natives_text(vm, object);
}

/**
 * @brief Lança `StringIndexOutOfBoundsException` com a mensagem do Java 8.
 * @details **Algoritmo:** mensagem `String index out of range: <valor>`.
 * @param[in,out] vm    Máquina virtual.
 * @param[in]     value Valor inválido.
 */
static void throw_index(Vm *vm, int32_t value)
{
    char message[48];
    sprintf(message, "String index out of range: %ld", (long)value);
    vm_throw_new(vm, "java/lang/StringIndexOutOfBoundsException", message);
}

/**
 * @brief Construtores de `String`, `StringBuilder` e `StringBuffer`.
 * @details **Algoritmo:** `()` e `(I)` (capacidade, ignorada) deixam o texto
 * vazio; `(String)` copia o texto; `([C)` copia os caracteres do array.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor do construtor.
 * @param[in]     args       `this` e o argumento opcional.
 * @param[out]    result     Sem retorno.
 */
static void string_init(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    const Text *source;
    (void)result;
    if (self == NULL || descriptor[1] == ')' || descriptor[1] == 'I') {
        return;
    }
    if (descriptor[1] == '[') {
        const Object *chars = (const Object *)(uintptr_t)args[1];
        if (chars == NULL) {
            vm_throw_new(vm, "java/lang/NullPointerException", NULL);
        } else if (chars->kind == OBJECT_ARRAY && strcmp(chars->as.array.type, "[C") == 0) {
            natives_text_append(vm, self, chars->as.array.data, chars->as.array.length);
        }
        return;
    }
    source = arg_text(vm, args[1]);
    if (source != NULL) {
        natives_text_append(vm, self, source->chars, source->length);
    }
}

/**
 * @brief `length()I` de `String`, `StringBuilder` e `StringBuffer`.
 * @details **Algoritmo:** devolve a quantidade de unidades UTF-16.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this`.
 * @param[out]    result     Tamanho.
 */
static void string_length(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    const Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    (void)descriptor;
    if (self != NULL) {
        result[0] = (Slot)self->length;
    }
}

/**
 * @brief `String.charAt(I)C`.
 * @details **Algoritmo:** índice fora de `[0, length)` lança
 * `StringIndexOutOfBoundsException`; senão devolve a unidade.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this` e o índice.
 * @param[out]    result     Caractere.
 */
static void string_char_at(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    const Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    int32_t index = slot_to_int(args[1]);
    (void)descriptor;
    if (self == NULL) {
        return;
    }
    if (index < 0 || index >= self->length) {
        throw_index(vm, index);
        return;
    }
    result[0] = self->chars[index];
}

/**
 * @brief `String.equals(Object)Z`: mesmo conteúdo.
 * @details **Algoritmo:** falso se o outro for `null` ou não for `String`;
 * senão compara tamanho e unidades.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this` e o outro objeto.
 * @param[out]    result     1 se iguais, senão 0.
 */
static void string_equals(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    const Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    const Object *other = (const Object *)(uintptr_t)args[1];
    (void)descriptor;
    result[0] = 0;
    if (self == NULL || other == NULL || other->kind != OBJECT_TEXT ||
        strcmp(other->class_ref->name, "java/lang/String") != 0 ||
        other->as.text.length != self->length) {
        return;
    }
    result[0] = self->length == 0 ||
                memcmp(self->chars, other->as.text.chars,
                       (size_t)self->length * sizeof(uint16_t)) == 0;
}

/**
 * @brief `String.concat(String)`.
 * @details **Algoritmo:** cria uma `String` com o texto de `this` e acrescenta o argumento.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this` e a outra `String`.
 * @param[out]    result     Nova `String`.
 */
static void string_concat(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    const Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    const Text *other = self != NULL ? arg_text(vm, args[1]) : NULL;
    Object *created;
    (void)descriptor;
    if (other == NULL) {
        return;
    }
    created = heap_new_string(vm, self->chars, self->length);
    if (created != NULL) {
        natives_text_append(vm, &created->as.text, other->chars, other->length);
        result[0] = (Slot)(uintptr_t)created;
    }
}

/**
 * @brief `String.substring(I)` e `String.substring(II)`.
 * @details **Algoritmo:** valida como o Java 8 (`begin < 0`, `end > length`,
 * `begin > end` lançam `StringIndexOutOfBoundsException`) e copia a faixa.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor (com 1 ou 2 parâmetros).
 * @param[in]     args       `this`, início e, opcionalmente, fim.
 * @param[out]    result     Nova `String`.
 */
static void string_substring(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    const Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    int32_t begin = slot_to_int(args[1]);
    int32_t end;
    if (self == NULL) {
        return;
    }
    end = descriptor[2] == 'I' ? slot_to_int(args[2]) : self->length;
    if (begin < 0) {
        throw_index(vm, begin);
    } else if (end > self->length) {
        throw_index(vm, end);
    } else if (begin > end) {
        throw_index(vm, end - begin);
    } else {
        result[0] = (Slot)(uintptr_t)heap_new_string(vm, self->chars + begin, end - begin);
    }
}

/**
 * @brief `String.indexOf(I)I` e `String.indexOf(String)I`.
 * @details **Algoritmo:** busca linear do caractere ou da sequência; -1 se não achar.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this` e o caractere ou a `String` procurada.
 * @param[out]    result     Posição ou -1.
 */
static void string_index_of(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    const Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    const Text *needle;
    int32_t i;
    result[0] = (Slot)-1;
    if (self == NULL) {
        return;
    }
    if (descriptor[1] == 'I') {
        for (i = 0; i < self->length; i++) {
            if (self->chars[i] == args[1]) {
                result[0] = (Slot)i;
                return;
            }
        }
        return;
    }
    needle = arg_text(vm, args[1]);
    for (i = 0; needle != NULL && i + needle->length <= self->length; i++) {
        if (needle->length == 0 ||
            memcmp(self->chars + i, needle->chars, (size_t)needle->length * 2) == 0) {
            result[0] = (Slot)i;
            return;
        }
    }
}

/**
 * @brief `String.hashCode()I`: `s[0]*31^(n-1) + ... + s[n-1]`.
 * @details **Algoritmo:** `h = 31 * h + c` em `uint32_t` (*overflow* como no Java).
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this`.
 * @param[out]    result     Código *hash*.
 */
static void string_hash_code(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    const Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    uint32_t hash = 0;
    int32_t i;
    (void)descriptor;
    for (i = 0; self != NULL && i < self->length; i++) {
        hash = 31u * hash + self->chars[i];
    }
    result[0] = hash;
}

/**
 * @brief `toString()` de `String` (ela mesma) e de `StringBuilder`/`StringBuffer` (cópia).
 * @details **Algoritmo:** se `this` é `String`, devolve-o; senão cria uma `String`.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this`.
 * @param[out]    result     `String`.
 */
static void string_to_string(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    Object *self = (Object *)(uintptr_t)args[0];
    const Text *text = natives_text(vm, self);
    (void)descriptor;
    if (text == NULL) {
        return;
    }
    if (strcmp(self->class_ref->name, "java/lang/String") == 0) {
        result[0] = args[0];
    } else {
        result[0] = (Slot)(uintptr_t)heap_new_string(vm, text->chars, text->length);
    }
}

/** @brief Gera as linhas comuns a `StringBuilder` e `StringBuffer`. */
#define BUILDER_ENTRIES(cls)                                          \
    {cls, "<init>", "()V", string_init, false},                              \
    {cls, "<init>", "(I)V", string_init, false},                             \
    {cls, "<init>", "(Ljava/lang/String;)V", string_init, false},            \
    {cls, "length", "()I", string_length, false},                            \
    {cls, "toString", "()Ljava/lang/String;", string_to_string, false}

const NativeEntry native_string_methods[] = {
    {"java/lang/String", "<init>", "()V", string_init, false},
    {"java/lang/String", "<init>", "(Ljava/lang/String;)V", string_init, false},
    {"java/lang/String", "<init>", "([C)V", string_init, false},
    {"java/lang/String", "length", "()I", string_length, false},
    {"java/lang/String", "charAt", "(I)C", string_char_at, false},
    {"java/lang/String", "equals", "(Ljava/lang/Object;)Z", string_equals, false},
    {"java/lang/String", "concat", "(Ljava/lang/String;)Ljava/lang/String;", string_concat, false},
    {"java/lang/String", "substring", "(I)Ljava/lang/String;", string_substring, false},
    {"java/lang/String", "substring", "(II)Ljava/lang/String;", string_substring, false},
    {"java/lang/String", "indexOf", "(I)I", string_index_of, false},
    {"java/lang/String", "indexOf", "(Ljava/lang/String;)I", string_index_of, false},
    {"java/lang/String", "hashCode", "()I", string_hash_code, false},
    {"java/lang/String", "toString", "()Ljava/lang/String;", string_to_string, false},
    BUILDER_ENTRIES("java/lang/StringBuilder"),
    BUILDER_ENTRIES("java/lang/StringBuffer"),
    {NULL, NULL, NULL, NULL, false}
};
