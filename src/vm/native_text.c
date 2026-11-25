/**
 * @file native_text.c
 * @brief Conversão de valores para texto: utilitários de texto UTF-16,
 *        escrita em UTF-8, `String.valueOf(x)` e `append(x)` de
 *        `StringBuilder` (Java 6/8) e `StringBuffer` (Java 2) (I-08).
 */
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "common/number_format.h"
#include "vm/frame.h"
#include "vm/heap.h"
#include "vm/interpreter.h"
#include "vm/natives.h"

Text *natives_text(Vm *vm, Object *object)
{
    if (object->kind != OBJECT_TEXT) {
        vm_fatal(vm, "objeto de texto esperado");
        return NULL;
    }
    return &object->as.text;
}

void natives_text_append(Vm *vm, Text *text, const uint16_t *chars, int32_t length)
{
    uint16_t *grown;
    if (length <= 0) {
        return;
    }
    if (text->length > INT32_MAX - length ||
        (size_t)(text->length + length) > HEAP_LIMIT / sizeof(uint16_t) ||
        vm->heap_bytes > HEAP_LIMIT - (size_t)length * sizeof(uint16_t)) {
        vm_fatal(vm, "memoria insuficiente para o texto");
        return;
    }
    grown = realloc(text->chars, (size_t)(text->length + length) * sizeof(uint16_t));
    if (grown == NULL) {
        vm_fatal(vm, "memoria insuficiente para o texto");
        return;
    }
    memcpy(grown + text->length, chars, (size_t)length * sizeof(uint16_t));
    text->chars = grown;
    text->length += length;
    vm->heap_bytes += (size_t)length * sizeof(uint16_t);
}

void natives_write_utf16(FILE *out, const uint16_t *chars, int32_t length)
{
    int32_t i;
    for (i = 0; i < length; i++) {
        uint32_t cp = chars[i];
        if (cp >= 0xD800u && cp <= 0xDBFFu && i + 1 < length &&
            chars[i + 1] >= 0xDC00u && chars[i + 1] <= 0xDFFFu) {
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (chars[i + 1] - 0xDC00u);
            i++;
        }
        if (cp < 0x80u) {
            fputc((int)cp, out);
        } else if (cp < 0x800u) {
            fputc((int)(0xC0u | (cp >> 6)), out);
            fputc((int)(0x80u | (cp & 0x3Fu)), out);
        } else if (cp < 0x10000u) {
            fputc((int)(0xE0u | (cp >> 12)), out);
            fputc((int)(0x80u | ((cp >> 6) & 0x3Fu)), out);
            fputc((int)(0x80u | (cp & 0x3Fu)), out);
        } else {
            fputc((int)(0xF0u | (cp >> 18)), out);
            fputc((int)(0x80u | ((cp >> 12) & 0x3Fu)), out);
            fputc((int)(0x80u | ((cp >> 6) & 0x3Fu)), out);
            fputc((int)(0x80u | (cp & 0x3Fu)), out);
        }
    }
}

/**
 * @brief Converte uma referência para `String` (`String.valueOf(Object)`).
 *
 * @details **Algoritmo:** `null` → "null"; `char[]` passado como `[C` →
 * o seu texto; uma `String` → ela mesma; demais → `toString()` virtual
 * (cujo resultado `null` também vira "null").
 *
 * @param[in,out] vm     Máquina virtual.
 * @param[in]     type   Descritor do tipo declarado.
 * @param[in]     object Objeto ou NULL.
 * @return Objeto `String`; NULL com exceção ou erro fatal.
 */
static Object *ref_to_string(Vm *vm, const char *type, Object *object)
{
    Slot result[2];
    if (object == NULL) {
        return heap_new_string_utf8(vm, "null");
    }
    if (type[0] == '[' && type[1] == 'C' && object->kind == OBJECT_ARRAY &&
        strcmp(object->as.array.type, "[C") == 0) {
        return heap_new_string(vm, object->as.array.data, object->as.array.length);
    }
    if (object->kind == OBJECT_TEXT && strcmp(object->class_ref->name, "java/lang/String") == 0) {
        return object;
    }
    if (!interpreter_call_virtual(vm, object, "toString", "()Ljava/lang/String;", result)) {
        return NULL;
    }
    object = (Object *)(uintptr_t)result[0];
    if (object == NULL) {
        return heap_new_string_utf8(vm, "null");
    }
    return natives_text(vm, object) != NULL ? object : NULL;
}

Object *natives_to_string(Vm *vm, const char *type, const Slot *value)
{
    char buffer[NUMBER_FORMAT_SIZE];
    uint16_t unit;
    float f;
    double d;
    int64_t bits;
    switch (type[0]) {
    case 'L':
    case '[':
        return ref_to_string(vm, type, (Object *)(uintptr_t)value[0]);
    case 'C':
        unit = (uint16_t)value[0];
        return heap_new_string(vm, &unit, 1);
    case 'Z':
        return heap_new_string_utf8(vm, value[0] != 0 ? "true" : "false");
    case 'J':
        sprintf(buffer, "%" PRId64, slots_to_long(value[0], value[1]));
        break;
    case 'F':
        memcpy(&f, &value[0], sizeof f);
        java_format_float(buffer, sizeof buffer, f);
        break;
    case 'D':
        bits = slots_to_long(value[0], value[1]);
        memcpy(&d, &bits, sizeof d);
        java_format_double(buffer, sizeof buffer, d);
        break;
    default: /* I, B, S */
        sprintf(buffer, "%ld", (long)slot_to_int(value[0]));
        break;
    }
    return heap_new_string_utf8(vm, buffer);
}

/**
 * @brief `String.valueOf(x)` (estático) para todos os tipos.
 * @details **Algoritmo:** natives_to_string() com o tipo do parâmetro.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor (define o tipo de `x`).
 * @param[in]     args       Valor.
 * @param[out]    result     `String`.
 */
static void string_value_of(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    result[0] = (Slot)(uintptr_t)natives_to_string(vm, descriptor + 1, args);
}

/**
 * @brief `append(x)` de `StringBuilder`/`StringBuffer` para todos os tipos.
 * @details **Algoritmo:** converte `x` com natives_to_string(), acrescenta
 * o texto ao de `this` e devolve `this` (permite encadear chamadas).
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor (define o tipo de `x`).
 * @param[in]     args       `this` e o valor.
 * @param[out]    result     `this`.
 */
static void builder_append(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    Text *self = natives_text(vm, (Object *)(uintptr_t)args[0]);
    Object *text = self != NULL ? natives_to_string(vm, descriptor + 1, args + 1) : NULL;
    if (text == NULL || natives_text(vm, text) == NULL) {
        return;
    }
    natives_text_append(vm, self, text->as.text.chars, text->as.text.length);
    result[0] = args[0];
}

/** @brief Gera as linhas `append` e `valueOf` para um tipo de parâmetro. */
#define TEXT_TYPE_ENTRIES(type)                                                          \
    {"java/lang/StringBuilder", "append", "(" type ")Ljava/lang/StringBuilder;", builder_append, false}, \
    {"java/lang/StringBuffer", "append", "(" type ")Ljava/lang/StringBuffer;", builder_append, false},   \
    {"java/lang/String", "valueOf", "(" type ")Ljava/lang/String;", string_value_of, true}

const NativeEntry native_text_methods[] = {
    TEXT_TYPE_ENTRIES("I"),
    TEXT_TYPE_ENTRIES("J"),
    TEXT_TYPE_ENTRIES("F"),
    TEXT_TYPE_ENTRIES("D"),
    TEXT_TYPE_ENTRIES("C"),
    TEXT_TYPE_ENTRIES("Z"),
    TEXT_TYPE_ENTRIES("[C"),
    TEXT_TYPE_ENTRIES("Ljava/lang/Object;"),
    {"java/lang/StringBuilder", "append", "(Ljava/lang/String;)Ljava/lang/StringBuilder;", builder_append, false},
    {"java/lang/StringBuffer", "append", "(Ljava/lang/String;)Ljava/lang/StringBuffer;", builder_append, false},
    {"java/lang/StringBuilder", "append", "(Ljava/lang/CharSequence;)Ljava/lang/StringBuilder;", builder_append, false},
    {NULL, NULL, NULL, NULL, false}
};
