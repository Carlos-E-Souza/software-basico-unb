/**
 * @file print_attributes.c
 * @brief Exibição dos atributos (`attribute_info`) e do atributo Code.
 *
 * @details Cada atributo é exibido com as informações genéricas (nome e
 * tamanho) seguidas do conteúdo específico do seu tipo, com os rótulos do
 * jclasslib.
 */
#include "classfile/constant_pool.h"
#include "viewer/disassembler.h"
#include "viewer/printer_internal.h"

/** @brief Quantidade máxima de bytes exibidos de um atributo desconhecido. */
#define RAW_DUMP_LIMIT 256

/**
 * @brief Escreve o conteúdo específico de um atributo Code.
 *
 * @details **Algoritmo:** escreve `max_stack`, `max_locals` e `code_length`,
 * o bytecode desmontado (disassemble_code()), a tabela de exceções (tipo
 * `any` quando `catch_type` é 0, isto é, `finally`) e os atributos aninhados.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] cf     Classe.
 * @param[in] code   Atributo Code.
 * @param[in] indent Indentação.
 */
static void print_code(FILE *out, const ClassFile *cf, const CodeAttribute *code,
                       int indent)
{
    uint16_t i;
    print_number_line(out, indent, "Maximum stack size", code->max_stack);
    print_number_line(out, indent, "Maximum local variables", code->max_locals);
    print_number_line(out, indent, "Code length", code->code_length);
    print_indent(out, indent);
    fputs("Bytecode:\n", out);
    disassemble_code(out, cf, code, indent + 2);

    print_indent(out, indent);
    fprintf(out, "Exception table (length = %u):\n", (unsigned)code->exception_table_length);
    if (code->exception_table_length > 0) {
        print_indent(out, indent + 2);
        fputs("Nr.  Start PC  End PC  Handler PC  Catch Type\n", out);
    }
    for (i = 0; i < code->exception_table_length; i++) {
        const ExceptionTableEntry *e = &code->exception_table[i];
        print_indent(out, indent + 2);
        fprintf(out, "%3u  %8u  %6u  %10u  ", (unsigned)i, (unsigned)e->start_pc,
                (unsigned)e->end_pc, (unsigned)e->handler_pc);
        if (e->catch_type == 0) {
            fputs("cp_info #0 <any>\n", out);
        } else {
            print_cp_ref(out, cf, e->catch_type);
            fputc('\n', out);
        }
    }
    print_attributes(out, cf, code->attributes, code->attributes_count, indent);
}

/**
 * @brief Escreve os bytes brutos de um atributo desconhecido em hexadecimal.
 *
 * @details **Algoritmo:** 16 bytes por linha, até #RAW_DUMP_LIMIT bytes.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] attr   Atributo desconhecido.
 * @param[in] indent Indentação.
 */
static void print_raw(FILE *out, const AttributeInfo *attr, int indent)
{
    uint32_t i, n = attr->attribute_length;
    if (n > RAW_DUMP_LIMIT) {
        n = RAW_DUMP_LIMIT;
    }
    for (i = 0; i < n; i++) {
        if (i % 16 == 0) {
            if (i > 0) {
                fputc('\n', out);
            }
            print_indent(out, indent);
        }
        fprintf(out, "%02X ", (unsigned)attr->info.raw[i]);
    }
    if (n > 0) {
        fputc('\n', out);
    }
    if (attr->attribute_length > n) {
        print_indent(out, indent);
        fprintf(out, "... (%lu bytes omitidos)\n",
                (unsigned long)(attr->attribute_length - n));
    }
}

/**
 * @brief Escreve as tabelas LocalVariableTable e LocalVariableTypeTable.
 *
 * @details **Algoritmo:** cabeçalho com as colunas do jclasslib e, para cada
 * entrada, `start_pc`, `length`, `index` e as referências ao pool do nome e
 * do descritor (ou assinatura genérica).
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] cf     Classe.
 * @param[in] attr   Atributo.
 * @param[in] indent Indentação.
 */
static void print_local_variables(FILE *out, const ClassFile *cf,
                                  const AttributeInfo *attr, int indent)
{
    uint16_t i;
    print_indent(out, indent);
    fprintf(out, "Nr.  Start PC  Length  Index  Name / %s\n",
            attr->kind == ATTR_LOCAL_VARIABLE_TABLE ? "Descriptor" : "Signature");
    for (i = 0; i < attr->info.local_variables.local_variable_table_length; i++) {
        const LocalVariableEntry *e = &attr->info.local_variables.local_variable_table[i];
        print_indent(out, indent);
        fprintf(out, "%3u  %8u  %6u  %5u  ", (unsigned)i, (unsigned)e->start_pc,
                (unsigned)e->length, (unsigned)e->index);
        print_cp_ref(out, cf, e->name_index);
        fputs("  ", out);
        print_cp_ref(out, cf, e->descriptor_index);
        fputc('\n', out);
    }
}

/**
 * @brief Escreve o conteúdo específico de um atributo conforme seu tipo.
 *
 * @details **Algoritmo:** `switch` sobre @c kind; cada tipo tem os campos e
 * rótulos usados pelo jclasslib (tabelas para LineNumberTable,
 * LocalVariableTable e Exceptions; referências ao pool para ConstantValue,
 * SourceFile, Signature etc.; bytecode para Code).
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] cf     Classe.
 * @param[in] attr   Atributo.
 * @param[in] indent Indentação do conteúdo.
 */
static void print_attribute_body(FILE *out, const ClassFile *cf,
                                 const AttributeInfo *attr, int indent)
{
    uint16_t i, j;
    switch (attr->kind) {
    case ATTR_CONSTANT_VALUE:
        print_ref_line(out, cf, indent, "Constant value index",
                       attr->info.constantvalue_index);
        break;
    case ATTR_CODE:
        print_code(out, cf, attr->info.code, indent);
        break;
    case ATTR_STACK_MAP_TABLE:
        print_stack_map_table(out, cf, attr, indent);
        break;
    case ATTR_EXCEPTIONS:
        print_indent(out, indent);
        fputs("Nr.  Exception\n", out);
        for (i = 0; i < attr->info.exceptions.number_of_exceptions; i++) {
            print_indent(out, indent);
            fprintf(out, "%3u  ", (unsigned)i);
            print_cp_ref(out, cf, attr->info.exceptions.exception_index_table[i]);
            fputc('\n', out);
        }
        break;
    case ATTR_INNER_CLASSES:
        for (i = 0; i < attr->info.inner_classes.number_of_classes; i++) {
            const InnerClassEntry *e = &attr->info.inner_classes.classes[i];
            print_indent(out, indent);
            fprintf(out, "[%u]\n", (unsigned)i);
            print_ref_line(out, cf, indent + 4, "Inner class", e->inner_class_info_index);
            print_ref_line(out, cf, indent + 4, "Outer class", e->outer_class_info_index);
            print_ref_line(out, cf, indent + 4, "Inner name", e->inner_name_index);
            print_flags_line(out, indent + 4, e->inner_class_access_flags,
                             FLAGS_INNER_CLASS);
        }
        break;
    case ATTR_ENCLOSING_METHOD:
        print_ref_line(out, cf, indent, "Class index", attr->info.enclosing_method.class_index);
        print_ref_line(out, cf, indent, "Method index", attr->info.enclosing_method.method_index);
        break;
    case ATTR_SIGNATURE:
        print_ref_line(out, cf, indent, "Signature index", attr->info.signature_index);
        break;
    case ATTR_SOURCE_FILE:
        print_ref_line(out, cf, indent, "Source file name index",
                       attr->info.sourcefile_index);
        break;
    case ATTR_LINE_NUMBER_TABLE:
        print_indent(out, indent);
        fputs("Nr.  Start PC  Line Number\n", out);
        for (i = 0; i < attr->info.line_numbers.line_number_table_length; i++) {
            const LineNumberEntry *e = &attr->info.line_numbers.line_number_table[i];
            print_indent(out, indent);
            fprintf(out, "%3u  %8u  %11u\n", (unsigned)i, (unsigned)e->start_pc,
                    (unsigned)e->line_number);
        }
        break;
    case ATTR_LOCAL_VARIABLE_TABLE:
    case ATTR_LOCAL_VARIABLE_TYPE_TABLE:
        print_local_variables(out, cf, attr, indent);
        break;
    case ATTR_BOOTSTRAP_METHODS:
        for (i = 0; i < attr->info.bootstrap.num_bootstrap_methods; i++) {
            const BootstrapMethod *m = &attr->info.bootstrap.bootstrap_methods[i];
            print_indent(out, indent);
            fprintf(out, "[%u] ", (unsigned)i);
            print_cp_ref(out, cf, m->bootstrap_method_ref);
            fputc('\n', out);
            print_indent(out, indent + 4);
            fputs("Method arguments:\n", out);
            for (j = 0; j < m->num_bootstrap_arguments; j++) {
                print_indent(out, indent + 6);
                print_cp_ref(out, cf, m->bootstrap_arguments[j]);
                fputc('\n', out);
            }
        }
        break;
    case ATTR_SYNTHETIC:
    case ATTR_DEPRECATED:
        break;
    case ATTR_UNKNOWN:
        print_raw(out, attr, indent);
        break;
    }
}

void print_attributes(FILE *out, const ClassFile *cf,
                      const AttributeInfo *attributes, uint16_t count,
                      int indent)
{
    uint16_t i;
    print_number_line(out, indent, "Attributes count", count);
    for (i = 0; i < count; i++) {
        const AttributeInfo *a = &attributes[i];
        print_indent(out, indent);
        fprintf(out, "[%u] %s\n", (unsigned)i, cp_get_utf8(cf, a->attribute_name_index));
        print_ref_line(out, cf, indent + 4, "Attribute name index", a->attribute_name_index);
        print_number_line(out, indent + 4, "Attribute length", a->attribute_length);
        print_attribute_body(out, cf, a, indent + 4);
    }
}
