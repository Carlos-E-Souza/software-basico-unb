/**
 * @file print_constant_pool.c
 * @brief Seção Constant Pool do exibidor.
 */
#include <inttypes.h>

#include "classfile/constant_pool.h"
#include "common/mutf8.h"
#include "viewer/printer_internal.h"

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
        fprintf(out, "  [%u] %s\n", (unsigned)i,
                constant_info_name((ConstantTag)cf->constant_pool[i].tag));
        print_constant_details(out, cf, i);
    }
}
