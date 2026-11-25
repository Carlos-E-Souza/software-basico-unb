/**
 * @file format.c
 * @brief Formatação geral do exibidor: flags de acesso, indentação, texto
 *        MUTF-8 na tela e linhas no formato `Rotulo: valor`.
 */
#include <stdlib.h>
#include <string.h>

#include "common/mutf8.h"
#include "viewer/printer_internal.h"

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

void print_label(FILE *out, int indent, const char *label)
{
    print_indent(out, indent);
    fprintf(out, "%s:%*s", label, (int)(LABEL_WIDTH - strlen(label)), "");
}

void print_ref_line(FILE *out, const ClassFile *cf, int indent,
                    const char *label, uint16_t index)
{
    print_label(out, indent, label);
    print_cp_ref(out, cf, index);
    fputc('\n', out);
}

void print_number_line(FILE *out, int indent, const char *label,
                       unsigned long value)
{
    print_label(out, indent, label);
    fprintf(out, "%lu\n", value);
}

void print_flags_line(FILE *out, int indent, uint16_t flags,
                      FlagContext context)
{
    char names[FLAGS_BUFFER_SIZE];
    format_access_flags(names, sizeof names, flags, context);
    print_label(out, indent, "Access flags");
    fprintf(out, "0x%04X [%s]\n", (unsigned)flags, names);
}

void print_section(FILE *out, const char *title, long count)
{
    if (count >= 0) {
        fprintf(out, "\n=== %s (count = %ld) ===\n", title, count);
    } else {
        fprintf(out, "\n=== %s ===\n", title);
    }
}
