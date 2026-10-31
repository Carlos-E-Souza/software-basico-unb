/**
 * @file printer.c
 * @brief Implementação do exibidor das estruturas de um `.class`.
 *
 * @details A saída segue a organização do visualizador jclasslib: informações
 * gerais, pool de constantes, interfaces, campos, métodos e atributos, com os
 * mesmos rótulos. Toda referência ao pool é escrita como
 * `cp_info #n <constante>`, isto é, com o índice do slot e a constante que ele
 * representa. Os textos fixos são escritos sem acentos para que a saída seja
 * legível em qualquer terminal, inclusive no Windows.
 */
#include <inttypes.h>
#include <string.h>

#include "classfile/attributes.h"
#include "classfile/constant_pool.h"
#include "common/mutf8.h"
#include "viewer/disassembler.h"
#include "viewer/format.h"
#include "viewer/viewer.h"

/** @brief Quantidade máxima de bytes exibidos de um atributo desconhecido. */
#define RAW_DUMP_LIMIT 256
/** @brief Largura da coluna dos rótulos ("Rotulo: valor"). */
#define LABEL_WIDTH 24

static void print_attributes(FILE *out, const ClassFile *cf,
                             const AttributeInfo *attributes, uint16_t count,
                             int indent);

/**
 * @brief Escreve o início de uma linha `Rotulo:   ` alinhada.
 *
 * @details **Algoritmo:** escreve a indentação, o rótulo seguido de ':' e
 * completa com espaços até #LABEL_WIDTH colunas.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] indent Indentação.
 * @param[in] label  Texto do rótulo.
 */
static void print_label(FILE *out, int indent, const char *label)
{
    print_indent(out, indent);
    fprintf(out, "%s:%*s", label, (int)(LABEL_WIDTH - strlen(label)), "");
}

/**
 * @brief Escreve uma linha `Rotulo: cp_info #n <constante>`.
 *
 * @details **Algoritmo:** print_label() seguido de print_cp_ref().
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] cf     Classe.
 * @param[in] indent Indentação.
 * @param[in] label  Texto do rótulo.
 * @param[in] index  Índice no pool.
 */
static void print_ref_line(FILE *out, const ClassFile *cf, int indent,
                           const char *label, uint16_t index)
{
    print_label(out, indent, label);
    print_cp_ref(out, cf, index);
    fputc('\n', out);
}

/**
 * @brief Escreve uma linha `Rotulo: número`.
 *
 * @details **Algoritmo:** print_label() seguido do valor em decimal.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] indent Indentação.
 * @param[in] label  Texto do rótulo.
 * @param[in] value  Valor.
 */
static void print_number_line(FILE *out, int indent, const char *label,
                              unsigned long value)
{
    print_label(out, indent, label);
    fprintf(out, "%lu\n", value);
}

/**
 * @brief Escreve uma linha `Rotulo: 0x0021 [public super]`.
 *
 * @details **Algoritmo:** formata o valor hexadecimal e os nomes com
 * format_access_flags().
 *
 * @param[in] out     Fluxo de saída.
 * @param[in] indent  Indentação.
 * @param[in] flags   Flags de acesso.
 * @param[in] context Contexto (classe, campo, método ou classe interna).
 */
static void print_flags_line(FILE *out, int indent, uint16_t flags,
                             FlagContext context)
{
    char names[FLAGS_BUFFER_SIZE];
    format_access_flags(names, sizeof names, flags, context);
    print_label(out, indent, "Access flags");
    fprintf(out, "0x%04X [%s]\n", (unsigned)flags, names);
}

/**
 * @brief Escreve o título de uma seção (ex.: `=== Methods (count = 5) ===`).
 *
 * @details **Algoritmo:** linha em branco e o título; se @p count não for
 * negativo, inclui a quantidade de itens.
 *
 * @param[in] out   Fluxo de saída.
 * @param[in] title Texto do título.
 * @param[in] count Quantidade de itens, ou −1 para omitir.
 */
static void print_section(FILE *out, const char *title, long count)
{
    if (count >= 0) {
        fprintf(out, "\n=== %s (count = %ld) ===\n", title, count);
    } else {
        fprintf(out, "\n=== %s ===\n", title);
    }
}

/**
 * @brief Escreve a seção General Information.
 *
 * @details **Algoritmo:** escreve cada campo de tamanho fixo da estrutura
 * #ClassFile, com o nome da versão do Java, as flags decodificadas e as
 * referências ao pool de `this_class` e `super_class`.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 */
static void print_general_info(FILE *out, const ClassFile *cf)
{
    print_section(out, "General Information", -1);
    print_label(out, 2, "Magic");
    fprintf(out, "0x%08" PRIX32 "\n", cf->magic);
    print_number_line(out, 2, "Minor version", cf->minor_version);
    print_label(out, 2, "Major version");
    fprintf(out, "%u [%s]\n", (unsigned)cf->major_version,
            class_file_java_version_name(cf->major_version));
    print_number_line(out, 2, "Constant pool count", cf->constant_pool_count);
    print_flags_line(out, 2, cf->access_flags, FLAGS_CLASS);
    print_ref_line(out, cf, 2, "This class", cf->this_class);
    print_ref_line(out, cf, 2, "Super class", cf->super_class);
    print_number_line(out, 2, "Interfaces count", cf->interfaces_count);
    print_number_line(out, 2, "Fields count", cf->fields_count);
    print_number_line(out, 2, "Methods count", cf->methods_count);
    print_number_line(out, 2, "Attributes count", cf->attributes_count);
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

/**
 * @brief Escreve a seção Constant Pool.
 *
 * @details **Algoritmo:** para cada índice de 1 a `count - 1`, escreve
 * `[n] CONSTANT_Xxx_info` e os campos internos (print_constant_details()).
 * A segunda posição de Long/Double aparece como "(large numeric continued)",
 * como no jclasslib.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 */
static void print_constant_pool(FILE *out, const ClassFile *cf)
{
    uint16_t i;
    print_section(out, "Constant Pool", cf->constant_pool_count);
    for (i = 1; i < cf->constant_pool_count; i++) {
        fprintf(out, "  [%u] %s\n", (unsigned)i,
                constant_info_name((ConstantTag)cf->constant_pool[i].tag));
        print_constant_details(out, cf, i);
    }
}

/**
 * @brief Escreve a seção Interfaces.
 *
 * @details **Algoritmo:** escreve cada índice do vetor `interfaces` como
 * referência ao pool.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 */
static void print_interfaces(FILE *out, const ClassFile *cf)
{
    uint16_t i;
    print_section(out, "Interfaces", cf->interfaces_count);
    for (i = 0; i < cf->interfaces_count; i++) {
        fprintf(out, "  [%u] Interface: ", (unsigned)i);
        print_cp_ref(out, cf, cf->interfaces[i]);
        fputc('\n', out);
    }
}

/**
 * @brief Escreve a seção Fields ou Methods.
 *
 * @details **Algoritmo:** para cada membro escreve `[i] nome`, as
 * referências de nome e descritor, as flags (no contexto de campo ou
 * método) e os atributos com print_attributes().
 *
 * @param[in] out     Fluxo de saída.
 * @param[in] cf      Classe.
 * @param[in] title   Título da seção.
 * @param[in] members Vetor de membros.
 * @param[in] count   Quantidade de membros.
 * @param[in] context #FLAGS_FIELD ou #FLAGS_METHOD.
 */
static void print_members(FILE *out, const ClassFile *cf, const char *title,
                          const MemberInfo *members, uint16_t count,
                          FlagContext context)
{
    uint16_t i;
    print_section(out, title, count);
    for (i = 0; i < count; i++) {
        const MemberInfo *m = &members[i];
        if (i > 0) {
            fputc('\n', out);
        }
        fprintf(out, "  [%u] ", (unsigned)i);
        print_constant(out, cf, m->name_index);
        fputc('\n', out);
        print_ref_line(out, cf, 8, "Name", m->name_index);
        print_ref_line(out, cf, 8, "Descriptor", m->descriptor_index);
        print_flags_line(out, 8, m->access_flags, context);
        print_attributes(out, cf, m->attributes, m->attributes_count, 8);
    }
}

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
 * @brief Escreve um `verification_type_info` (ex.: `int`, `Object cp_info #5 <[[I>`).
 *
 * @details **Algoritmo:** `switch` sobre a tag; ITEM_Object mostra a classe
 * como referência ao pool e ITEM_Uninitialized o offset do `new`.
 *
 * @param[in] out  Fluxo de saída.
 * @param[in] cf   Classe.
 * @param[in] info Item a escrever.
 */
static void print_verification_type(FILE *out, const ClassFile *cf,
                                    const VerificationTypeInfo *info)
{
    static const char *const names[] = {
        "top", "int", "float", "double", "long", "null", "uninitialized_this"
    };
    if (info->tag == ITEM_Object) {
        fputs("Object ", out);
        print_cp_ref(out, cf, info->value);
    } else if (info->tag == ITEM_Uninitialized) {
        fprintf(out, "uninitialized(offset %u)", (unsigned)info->value);
    } else {
        fputs(names[info->tag], out);
    }
}

/**
 * @brief Escreve uma lista de `verification_type_info` entre colchetes.
 *
 * @details **Algoritmo:** escreve os itens separados por vírgula.
 *
 * @param[in] out   Fluxo de saída.
 * @param[in] cf    Classe.
 * @param[in] items Itens.
 * @param[in] count Quantidade de itens.
 */
static void print_verification_list(FILE *out, const ClassFile *cf,
                                    const VerificationTypeInfo *items, uint16_t count)
{
    uint16_t i;
    fputs("[", out);
    for (i = 0; i < count; i++) {
        fputs(i > 0 ? ", " : "", out);
        print_verification_type(out, cf, &items[i]);
    }
    fputs("]", out);
}

/**
 * @brief Retorna o nome da forma de um quadro do StackMapTable.
 *
 * @details **Algoritmo:** classifica `frame_type` nas faixas da JVMS §4.7.4.
 *
 * @param[in] type `frame_type`.
 * @return Nome da forma (ex.: "same", "append").
 */
static const char *frame_type_name(uint8_t type)
{
    if (type <= 63) return "same";
    if (type <= 127) return "same_locals_1_stack_item";
    if (type == 247) return "same_locals_1_stack_item_extended";
    if (type >= 248 && type <= 250) return "chop";
    if (type == 251) return "same_frame_extended";
    if (type >= 252 && type <= 254) return "append";
    if (type == 255) return "full_frame";
    return "reservado";
}

/**
 * @brief Escreve os quadros de um StackMapTable.
 *
 * @details **Algoritmo:** para cada quadro, calcula o pc absoluto (o
 * primeiro é `offset_delta`; os seguintes, anterior + `offset_delta` + 1) e
 * escreve tipo, deslocamento, locais e pilha listados.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] cf     Classe.
 * @param[in] attr   Atributo StackMapTable.
 * @param[in] indent Indentação.
 */
static void print_stack_map_table(FILE *out, const ClassFile *cf,
                                  const AttributeInfo *attr, int indent)
{
    uint16_t i;
    uint32_t pc = 0;
    print_number_line(out, indent, "Number of entries",
                      attr->info.stack_map_table.number_of_entries);
    for (i = 0; i < attr->info.stack_map_table.number_of_entries; i++) {
        const StackMapFrame *f = &attr->info.stack_map_table.entries[i];
        pc = (i == 0) ? f->offset_delta : pc + f->offset_delta + 1u;
        print_indent(out, indent);
        fprintf(out, "[%u] pc %lu: %s (frame_type = %u, offset_delta = %u)\n",
                (unsigned)i, (unsigned long)pc, frame_type_name(f->frame_type),
                (unsigned)f->frame_type, (unsigned)f->offset_delta);
        if (f->number_of_locals > 0 || f->frame_type == 255) {
            print_indent(out, indent + 4);
            fputs("locals = ", out);
            print_verification_list(out, cf, f->locals, f->number_of_locals);
            fputc('\n', out);
        }
        if (f->number_of_stack_items > 0 || f->frame_type == 255) {
            print_indent(out, indent + 4);
            fputs("stack  = ", out);
            print_verification_list(out, cf, f->stack, f->number_of_stack_items);
            fputc('\n', out);
        }
    }
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

/**
 * @brief Escreve um vetor de atributos.
 *
 * @details **Algoritmo:** escreve a contagem e, para cada atributo, o nome,
 * as informações genéricas (`Attribute name index` como referência ao pool
 * e `Attribute length`) e o conteúdo específico (print_attribute_body()).
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] cf         Classe.
 * @param[in] attributes Vetor de atributos.
 * @param[in] count      Quantidade de atributos.
 * @param[in] indent     Indentação.
 */
static void print_attributes(FILE *out, const ClassFile *cf,
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

void viewer_print_class(FILE *out, const ClassFile *class_file,
                        const char *source_name)
{
    fputs("==================================================================\n", out);
    fprintf(out, " Arquivo: %s\n", source_name != NULL ? source_name : "(memoria)");
    fputs("==================================================================\n", out);
    print_general_info(out, class_file);
    print_constant_pool(out, class_file);
    print_interfaces(out, class_file);
    print_members(out, class_file, "Fields", class_file->fields,
                  class_file->fields_count, FLAGS_FIELD);
    print_members(out, class_file, "Methods", class_file->methods,
                  class_file->methods_count, FLAGS_METHOD);
    print_section(out, "Attributes", class_file->attributes_count);
    print_attributes(out, class_file, class_file->attributes,
                     class_file->attributes_count, 2);
}
