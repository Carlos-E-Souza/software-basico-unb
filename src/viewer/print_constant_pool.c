/**
 * @file print_constant_pool.c
 * @brief Pool de constantes no exibidor: a seção Constant Pool e a
 *        representação textual de cada constante (`cp_info #n <constante>`),
 *        usada por todas as seções e pelo disassembler.
 */
#include <inttypes.h>

#include "classfile/constant_pool.h"
#include "common/mutf8.h"
#include "common/number_format.h"
#include "viewer/printer_internal.h"

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

/**
 * @brief Retorna o nome de um `reference_kind` de MethodHandle (ex.: "REF_invokeStatic").
 *
 * @details **Algoritmo:** tabela indexada de 1 a 9.
 *
 * @param[in] kind Valor de `reference_kind`.
 * @return Nome do tipo, ou "?" se @p kind estiver fora de 1–9.
 */
static const char *method_handle_kind_name(uint8_t kind)
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

/**
 * @brief Escreve os campos internos de uma entrada do pool.
 *
 * @details
 * **Algoritmo**, conforme a tag (rótulos do jclasslib):
 * - Utf8: tamanho em bytes, tamanho em caracteres (UTF-16) e o texto;
 * - Integer/Float: os 4 bytes em hexadecimal e o valor;
 * - Long/Double: `high_bytes`, `low_bytes` e o valor;
 * - entradas com referências: cada índice como `cp_info #n <constante>`;
 * - MethodHandle: o `reference_kind` com seu nome e a referência;
 * - InvokeDynamic: o índice no BootstrapMethods e o NameAndType.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 * @param[in] i   Índice da entrada.
 */
static void print_constant_details(FILE *out, const ClassFile *cf, uint16_t i)
{
    const CpInfo *e = &cf->constant_pool[i];
    const int ind = 8;
    switch ((ConstantTag)e->tag) {
    case CONSTANT_Utf8:
        print_number_line(out, ind, "Length of byte array", e->info.utf8.length);
        print_number_line(out, ind, "Length of string",
                          (unsigned long)mutf8_to_utf16((const uint8_t *)e->info.utf8.bytes,
                                                        e->info.utf8.length, NULL));
        print_label(out, ind, "String");
        print_constant(out, cf, i);
        fputc('\n', out);
        break;
    case CONSTANT_Integer:
    case CONSTANT_Float:
        print_label(out, ind, "Bytes");
        fprintf(out, "0x%08" PRIX32 "\n",
                e->tag == CONSTANT_Integer ? e->info.integer_bytes : e->info.float_bytes);
        print_label(out, ind, e->tag == CONSTANT_Integer ? "Integer" : "Float");
        print_constant(out, cf, i);
        fputc('\n', out);
        break;
    case CONSTANT_Long:
    case CONSTANT_Double:
        print_label(out, ind, "High bytes");
        fprintf(out, "0x%08" PRIX32 "\n", e->info.wide.high_bytes);
        print_label(out, ind, "Low bytes");
        fprintf(out, "0x%08" PRIX32 "\n", e->info.wide.low_bytes);
        print_label(out, ind, e->tag == CONSTANT_Long ? "Long" : "Double");
        print_constant(out, cf, i);
        fputc('\n', out);
        break;
    case CONSTANT_Class:
        print_ref_line(out, cf, ind, "Class name", e->info.class_name_index);
        break;
    case CONSTANT_String:
        print_ref_line(out, cf, ind, "String", e->info.string_index);
        break;
    case CONSTANT_Fieldref:
    case CONSTANT_Methodref:
    case CONSTANT_InterfaceMethodref:
        print_ref_line(out, cf, ind, "Class name", e->info.ref.class_index);
        print_ref_line(out, cf, ind, "Name and type", e->info.ref.name_and_type_index);
        break;
    case CONSTANT_NameAndType:
        print_ref_line(out, cf, ind, "Name", e->info.name_and_type.name_index);
        print_ref_line(out, cf, ind, "Descriptor", e->info.name_and_type.descriptor_index);
        break;
    case CONSTANT_MethodHandle:
        print_label(out, ind, "Reference kind");
        fprintf(out, "%u (%s)\n", (unsigned)e->info.method_handle.reference_kind,
                method_handle_kind_name(e->info.method_handle.reference_kind));
        print_ref_line(out, cf, ind, "Reference index", e->info.method_handle.reference_index);
        break;
    case CONSTANT_MethodType:
        print_ref_line(out, cf, ind, "Descriptor", e->info.method_type_index);
        break;
    case CONSTANT_InvokeDynamic:
        print_number_line(out, ind, "Bootstrap method",
                          e->info.invoke_dynamic.bootstrap_method_attr_index);
        print_ref_line(out, cf, ind, "Name and type",
                       e->info.invoke_dynamic.name_and_type_index);
        break;
    case CONSTANT_Unusable:
        break;
    }
}

void print_constant_pool(FILE *out, const ClassFile *cf)
{
    uint16_t i;
    print_section(out, "Constant Pool", cf->constant_pool_count);
    for (i = 1; i < cf->constant_pool_count; i++) {
        ConstantTag tag = (ConstantTag)cf->constant_pool[i].tag;
        if (tag == CONSTANT_Unusable) {
            fprintf(out, "  [%u] (large numeric continued)\n", (unsigned)i);
        } else {
            fprintf(out, "  [%u] CONSTANT_%s_info\n", (unsigned)i, cp_tag_name(tag));
        }
        print_constant_details(out, cf, i);
    }
}
