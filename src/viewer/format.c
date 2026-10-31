/**
 * @file format.c
 * @brief Implementação das funções auxiliares de formatação do exibidor.
 */
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "classfile/constant_pool.h"
#include "common/mutf8.h"
#include "common/number_format.h"
#include "viewer/format.h"

/** @brief Bits de contexto usados na tabela de flags. */
enum {
    CTX_C = 1 << FLAGS_CLASS,      /**< Válida para classes. */
    CTX_F = 1 << FLAGS_FIELD,      /**< Válida para campos. */
    CTX_M = 1 << FLAGS_METHOD,     /**< Válida para métodos. */
    CTX_I = 1 << FLAGS_INNER_CLASS /**< Válida para classes internas. */
};

/** @brief Uma flag de acesso: bit, nome e contextos em que é válida. */
typedef struct FlagName {
    uint16_t bit;     /**< Valor do bit. */
    const char *name; /**< Nome exibido (como no jclasslib). */
    int contexts;     /**< Combinação de CTX_*. */
} FlagName;

/** @brief Tabela das flags de acesso, na ordem usada para exibição. */
static const FlagName FLAG_NAMES[] = {
    {ACC_PUBLIC, "public", CTX_C | CTX_F | CTX_M | CTX_I},
    {ACC_PRIVATE, "private", CTX_F | CTX_M | CTX_I},
    {ACC_PROTECTED, "protected", CTX_F | CTX_M | CTX_I},
    {ACC_STATIC, "static", CTX_F | CTX_M | CTX_I},
    {ACC_FINAL, "final", CTX_C | CTX_F | CTX_M | CTX_I},
    {ACC_SUPER, "super", CTX_C},
    {ACC_SYNCHRONIZED, "synchronized", CTX_M},
    {ACC_VOLATILE, "volatile", CTX_F},
    {ACC_BRIDGE, "bridge", CTX_M},
    {ACC_TRANSIENT, "transient", CTX_F},
    {ACC_VARARGS, "varargs", CTX_M},
    {ACC_NATIVE, "native", CTX_M},
    {ACC_INTERFACE, "interface", CTX_C | CTX_I},
    {ACC_ABSTRACT, "abstract", CTX_C | CTX_M | CTX_I},
    {ACC_STRICT, "strict", CTX_M},
    {ACC_SYNTHETIC, "synthetic", CTX_C | CTX_F | CTX_M | CTX_I},
    {ACC_ANNOTATION, "annotation", CTX_C | CTX_I},
    {ACC_ENUM, "enum", CTX_C | CTX_F | CTX_I},
};

void format_access_flags(char *buffer, size_t size, uint16_t flags,
                         FlagContext context)
{
    size_t i, used = 0;
    int mask = 1 << context;
    if (size == 0) {
        return;
    }
    buffer[0] = '\0';
    for (i = 0; i < sizeof FLAG_NAMES / sizeof FLAG_NAMES[0]; i++) {
        const FlagName *f = &FLAG_NAMES[i];
        if ((flags & f->bit) != 0 && (f->contexts & mask) != 0) {
            int n = snprintf(buffer + used, size - used, "%s%s",
                             used > 0 ? " " : "", f->name);
            if (n < 0 || (size_t)n >= size - used) {
                return; /* buffer cheio: resultado truncado */
            }
            used += (size_t)n;
        }
    }
}

void print_indent(FILE *out, int count)
{
    fprintf(out, "%*s", count, "");
}

/**
 * @brief Escreve um código Unicode em UTF-8.
 *
 * @details **Algoritmo:** escolhe 1, 2, 3 ou 4 bytes conforme a faixa do
 * código (até 0x7F, 0x7FF, 0xFFFF ou acima) e distribui os bits.
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] code_point Código Unicode (até 0x10FFFF).
 */
static void put_utf8(FILE *out, uint32_t code_point)
{
    if (code_point < 0x80u) {
        fputc((int)code_point, out);
    } else if (code_point < 0x800u) {
        fputc((int)(0xC0u | (code_point >> 6)), out);
        fputc((int)(0x80u | (code_point & 0x3Fu)), out);
    } else if (code_point < 0x10000u) {
        fputc((int)(0xE0u | (code_point >> 12)), out);
        fputc((int)(0x80u | ((code_point >> 6) & 0x3Fu)), out);
        fputc((int)(0x80u | (code_point & 0x3Fu)), out);
    } else {
        fputc((int)(0xF0u | (code_point >> 18)), out);
        fputc((int)(0x80u | ((code_point >> 12) & 0x3Fu)), out);
        fputc((int)(0x80u | ((code_point >> 6) & 0x3Fu)), out);
        fputc((int)(0x80u | (code_point & 0x3Fu)), out);
    }
}

void print_mutf8(FILE *out, const char *bytes, size_t length)
{
    const uint8_t *raw = (const uint8_t *)bytes;
    size_t count = mutf8_to_utf16(raw, length, NULL);
    uint16_t *units;
    size_t i;

    if (count == 0) {
        return;
    }
    units = malloc(count * sizeof *units);
    if (units == NULL) {
        fwrite(bytes, 1, length, out); /* sem memória: escreve os bytes brutos */
        return;
    }
    mutf8_to_utf16(raw, length, units);
    for (i = 0; i < count; i++) {
        uint16_t u = units[i];
        if (u >= 0xD800u && u <= 0xDBFFu && i + 1 < count &&
            units[i + 1] >= 0xDC00u && units[i + 1] <= 0xDFFFu) {
            uint32_t cp = 0x10000u + (((uint32_t)u - 0xD800u) << 10) +
                          ((uint32_t)units[i + 1] - 0xDC00u);
            put_utf8(out, cp);
            i++;
        } else if (u == '\n') {
            fputs("\\n", out);
        } else if (u == '\t') {
            fputs("\\t", out);
        } else if (u == '\r') {
            fputs("\\r", out);
        } else if (u < 0x20u || u == 0x7Fu || (u >= 0xD800u && u <= 0xDFFFu)) {
            fprintf(out, "\\u%04X", (unsigned)u);
        } else {
            put_utf8(out, u);
        }
    }
    free(units);
}

/**
 * @brief Escreve um texto Utf8 do pool a partir do seu índice.
 *
 * @details **Algoritmo:** obtém a entrada com cp_get_utf8() e a escreve com
 * print_mutf8(); índice inválido escreve "invalido".
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe.
 * @param[in] index      Índice de um `CONSTANT_Utf8`.
 */
static void print_utf8_at(FILE *out, const ClassFile *class_file, uint16_t index)
{
    const char *text = cp_get_utf8(class_file, index);
    if (text == NULL) {
        fputs("invalido", out);
        return;
    }
    print_mutf8(out, text, class_file->constant_pool[index].info.utf8.length);
}

/**
 * @brief Escreve `nome : descritor` de um NameAndType.
 *
 * @details **Algoritmo:** escreve os dois Utf8 apontados pela entrada,
 * separados por " : ", como no jclasslib.
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe.
 * @param[in] index      Índice de um NameAndType.
 */
static void print_name_and_type(FILE *out, const ClassFile *class_file,
                                uint16_t index)
{
    const CpNameAndType *nt;
    if (!cp_check(class_file, index, CONSTANT_NameAndType)) {
        fputs("invalido", out);
        return;
    }
    nt = &class_file->constant_pool[index].info.name_and_type;
    print_utf8_at(out, class_file, nt->name_index);
    fputs(" : ", out);
    print_utf8_at(out, class_file, nt->descriptor_index);
}

/** @brief Nomes de `reference_kind` de MethodHandle, indexados de 1 a 9. */
static const char *const REFERENCE_KINDS[] = {
    "?", "REF_getField", "REF_getStatic", "REF_putField", "REF_putStatic",
    "REF_invokeVirtual", "REF_invokeStatic", "REF_invokeSpecial",
    "REF_newInvokeSpecial", "REF_invokeInterface"
};

const char *method_handle_kind_name(uint8_t kind)
{
    return REFERENCE_KINDS[(kind >= 1 && kind <= 9) ? kind : 0];
}

void print_constant(FILE *out, const ClassFile *class_file, uint16_t index)
{
    const CpInfo *e;
    char number[NUMBER_FORMAT_SIZE];

    if (cp_tag(class_file, index) == CONSTANT_Unusable) {
        fputs("invalido", out);
        return;
    }
    e = &class_file->constant_pool[index];
    switch ((ConstantTag)e->tag) {
    case CONSTANT_Utf8:
        print_mutf8(out, e->info.utf8.bytes, e->info.utf8.length);
        break;
    case CONSTANT_Integer: {
        int32_t v = 0;
        cp_get_int(class_file, index, &v);
        fprintf(out, "%" PRId32, v);
        break;
    }
    case CONSTANT_Float: {
        float v = 0;
        cp_get_float(class_file, index, &v);
        java_format_float(number, sizeof number, v);
        fputs(number, out);
        break;
    }
    case CONSTANT_Long: {
        int64_t v = 0;
        cp_get_long(class_file, index, &v);
        fprintf(out, "%" PRId64, v);
        break;
    }
    case CONSTANT_Double: {
        double v = 0;
        cp_get_double(class_file, index, &v);
        java_format_double(number, sizeof number, v);
        fputs(number, out);
        break;
    }
    case CONSTANT_Class:
        print_utf8_at(out, class_file, e->info.class_name_index);
        break;
    case CONSTANT_String:
        print_utf8_at(out, class_file, e->info.string_index);
        break;
    case CONSTANT_Fieldref:
    case CONSTANT_Methodref:
    case CONSTANT_InterfaceMethodref:
        print_constant(out, class_file, e->info.ref.class_index);
        fputc('.', out);
        print_name_and_type(out, class_file, e->info.ref.name_and_type_index);
        break;
    case CONSTANT_NameAndType:
        print_name_and_type(out, class_file, index);
        break;
    case CONSTANT_MethodHandle: {
        uint8_t kind = e->info.method_handle.reference_kind;
        fprintf(out, "%s ", method_handle_kind_name(kind));
        print_constant(out, class_file, e->info.method_handle.reference_index);
        break;
    }
    case CONSTANT_MethodType:
        print_utf8_at(out, class_file, e->info.method_type_index);
        break;
    case CONSTANT_InvokeDynamic:
        print_name_and_type(out, class_file, e->info.invoke_dynamic.name_and_type_index);
        fprintf(out, ", BootstrapMethods #%u",
                (unsigned)e->info.invoke_dynamic.bootstrap_method_attr_index);
        break;
    case CONSTANT_Unusable:
        break;
    }
}

void print_cp_ref(FILE *out, const ClassFile *class_file, uint16_t index)
{
    fprintf(out, "cp_info #%u", (unsigned)index);
    if (index != 0) {
        fputs(" <", out);
        print_constant(out, class_file, index);
        fputc('>', out);
    }
}

const char *constant_info_name(ConstantTag tag)
{
    switch (tag) {
    case CONSTANT_Utf8:               return "CONSTANT_Utf8_info";
    case CONSTANT_Integer:            return "CONSTANT_Integer_info";
    case CONSTANT_Float:              return "CONSTANT_Float_info";
    case CONSTANT_Long:               return "CONSTANT_Long_info";
    case CONSTANT_Double:             return "CONSTANT_Double_info";
    case CONSTANT_Class:              return "CONSTANT_Class_info";
    case CONSTANT_String:             return "CONSTANT_String_info";
    case CONSTANT_Fieldref:           return "CONSTANT_Fieldref_info";
    case CONSTANT_Methodref:          return "CONSTANT_Methodref_info";
    case CONSTANT_InterfaceMethodref: return "CONSTANT_InterfaceMethodref_info";
    case CONSTANT_NameAndType:        return "CONSTANT_NameAndType_info";
    case CONSTANT_MethodHandle:       return "CONSTANT_MethodHandle_info";
    case CONSTANT_MethodType:         return "CONSTANT_MethodType_info";
    case CONSTANT_InvokeDynamic:      return "CONSTANT_InvokeDynamic_info";
    case CONSTANT_Unusable:           break;
    }
    return "(large numeric continued)";
}
