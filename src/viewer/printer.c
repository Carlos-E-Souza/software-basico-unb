/**
 * @file printer.c
 * @brief Implementação do exibidor das estruturas de um `.class`.
 *
 * @details Formato de saída próprio (decisão D12 do plano), hierárquico e
 * inspirado no `javap -v`. Os rótulos são escritos sem acentos para que a
 * saída seja legível em qualquer terminal, inclusive no Windows.
 */
#include <inttypes.h>
#include <string.h>

#include "classfile/attributes.h"
#include "classfile/constant_pool.h"
#include "viewer/disassembler.h"
#include "viewer/format.h"
#include "viewer/viewer.h"

/** @brief Quantidade máxima de bytes exibidos de um atributo desconhecido. */
#define RAW_DUMP_LIMIT 256

static void print_attributes(FILE *out, const ClassFile *cf,
                             const AttributeInfo *attributes, uint16_t count,
                             int indent);

/**
 * @brief Escreve `#n <valor resolvido>` para um índice do pool.
 *
 * @details **Algoritmo:** escreve o índice e, se não for 0, a constante
 * resolvida entre `<` e `>`; o índice 0 é exibido como "(nenhum)".
 *
 * @param[in] out   Fluxo de saída.
 * @param[in] cf    Classe.
 * @param[in] index Índice no pool.
 */
static void print_ref(FILE *out, const ClassFile *cf, uint16_t index)
{
    if (index == 0) {
        fputs("#0 (nenhum)", out);
        return;
    }
    fprintf(out, "#%u <", (unsigned)index);
    print_constant(out, cf, index);
    fputc('>', out);
}

/**
 * @brief Escreve flags no formato `0x0021 [ACC_PUBLIC, ACC_SUPER]`.
 *
 * @details **Algoritmo:** formata o valor hexadecimal e os nomes com
 * format_access_flags().
 *
 * @param[in] out     Fluxo de saída.
 * @param[in] flags   Flags de acesso.
 * @param[in] context Contexto (classe, campo, método ou classe interna).
 */
static void print_flags(FILE *out, uint16_t flags, FlagContext context)
{
    char names[FLAGS_BUFFER_SIZE];
    format_access_flags(names, sizeof names, flags, context);
    fprintf(out, "0x%04X [%s]", (unsigned)flags, names);
}

/**
 * @brief Escreve um título de seção.
 *
 * @details **Algoritmo:** linha em branco seguida de `[título]`.
 *
 * @param[in] out   Fluxo de saída.
 * @param[in] title Texto do título.
 */
static void print_section(FILE *out, const char *title)
{
    fprintf(out, "\n[%s]\n", title);
}

/**
 * @brief Escreve a seção de informações gerais.
 *
 * @details **Algoritmo:** escreve cada campo de tamanho fixo da estrutura
 * #ClassFile, com nome da versão do Java e flags decodificadas.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 */
static void print_general_info(FILE *out, const ClassFile *cf)
{
    print_section(out, "Informacoes gerais");
    fprintf(out, "  Magic:                0x%08" PRIX32 "\n", cf->magic);
    fprintf(out, "  Minor version:        %u\n", (unsigned)cf->minor_version);
    fprintf(out, "  Major version:        %u (%s)\n", (unsigned)cf->major_version,
            class_file_java_version_name(cf->major_version));
    fprintf(out, "  Constant pool count:  %u\n", (unsigned)cf->constant_pool_count);
    fputs("  Access flags:         ", out);
    print_flags(out, cf->access_flags, FLAGS_CLASS);
    fputs("\n  This class:           ", out);
    print_ref(out, cf, cf->this_class);
    fputs("\n  Super class:          ", out);
    print_ref(out, cf, cf->super_class);
    fprintf(out, "\n  Interfaces count:     %u\n", (unsigned)cf->interfaces_count);
    fprintf(out, "  Fields count:         %u\n", (unsigned)cf->fields_count);
    fprintf(out, "  Methods count:        %u\n", (unsigned)cf->methods_count);
    fprintf(out, "  Attributes count:     %u\n", (unsigned)cf->attributes_count);
}

/**
 * @brief Escreve as referências brutas de uma entrada do pool (ex.: `#13.#40`).
 *
 * @details **Algoritmo:** conforme a tag, escreve os índices que a entrada
 * contém; para Utf8 e constantes numéricas não escreve nada (o valor vai no
 * comentário).
 *
 * @param[in] out Fluxo de saída.
 * @param[in] e   Entrada do pool.
 * @return Quantidade de caracteres escrita (para alinhar o comentário).
 */
static int print_raw_refs(FILE *out, const CpInfo *e)
{
    switch ((ConstantTag)e->tag) {
    case CONSTANT_Class:
        return fprintf(out, "#%u", (unsigned)e->info.class_name_index);
    case CONSTANT_String:
        return fprintf(out, "#%u", (unsigned)e->info.string_index);
    case CONSTANT_Fieldref:
    case CONSTANT_Methodref:
    case CONSTANT_InterfaceMethodref:
        return fprintf(out, "#%u.#%u", (unsigned)e->info.ref.class_index,
                       (unsigned)e->info.ref.name_and_type_index);
    case CONSTANT_NameAndType:
        return fprintf(out, "#%u:#%u", (unsigned)e->info.name_and_type.name_index,
                       (unsigned)e->info.name_and_type.descriptor_index);
    case CONSTANT_MethodHandle:
        return fprintf(out, "%u:#%u", (unsigned)e->info.method_handle.reference_kind,
                       (unsigned)e->info.method_handle.reference_index);
    case CONSTANT_MethodType:
        return fprintf(out, "#%u", (unsigned)e->info.method_type_index);
    case CONSTANT_InvokeDynamic:
        return fprintf(out, "#%u:#%u",
                       (unsigned)e->info.invoke_dynamic.bootstrap_method_attr_index,
                       (unsigned)e->info.invoke_dynamic.name_and_type_index);
    default:
        return 0;
    }
}

/**
 * @brief Escreve a seção do pool de constantes.
 *
 * @details
 * **Algoritmo:** para cada índice de 1 a `count - 1` com entrada utilizável,
 * escreve `#n = Tipo  refs  // valor`. Entradas Utf8, Integer, Float, Long e
 * Double mostram o valor diretamente; as demais mostram as referências e o
 * valor resolvido no comentário. A 2ª posição de Long/Double é omitida.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 */
static void print_constant_pool(FILE *out, const ClassFile *cf)
{
    uint16_t i;
    print_section(out, "Constant pool");
    for (i = 1; i < cf->constant_pool_count; i++) {
        const CpInfo *e = &cf->constant_pool[i];
        char label[16];
        int width;
        if (e->tag == CONSTANT_Unusable) {
            continue;
        }
        snprintf(label, sizeof label, "#%u", (unsigned)i);
        fprintf(out, "  %6s = %-18s ", label, cp_tag_name((ConstantTag)e->tag));
        width = print_raw_refs(out, e);
        if (width == 0) {
            print_constant(out, cf, i);
        } else {
            fprintf(out, "%*s // ", width < 14 ? 14 - width : 0, "");
            print_constant(out, cf, i);
        }
        fputc('\n', out);
    }
}

/**
 * @brief Escreve a seção de interfaces implementadas.
 *
 * @details **Algoritmo:** escreve cada índice do vetor `interfaces` com o
 * nome resolvido.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 */
static void print_interfaces(FILE *out, const ClassFile *cf)
{
    uint16_t i;
    print_section(out, "Interfaces");
    if (cf->interfaces_count == 0) {
        fputs("  (nenhuma)\n", out);
    }
    for (i = 0; i < cf->interfaces_count; i++) {
        fprintf(out, "  [%u] ", (unsigned)i);
        print_ref(out, cf, cf->interfaces[i]);
        fputc('\n', out);
    }
}

/**
 * @brief Escreve a seção de campos ou de métodos.
 *
 * @details **Algoritmo:** para cada membro escreve nome, descritor e flags
 * (no contexto de campo ou método) e os atributos com print_attributes().
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
    print_section(out, title);
    if (count == 0) {
        fputs("  (nenhum)\n", out);
    }
    for (i = 0; i < count; i++) {
        const MemberInfo *m = &members[i];
        fprintf(out, "  [%u] ", (unsigned)i);
        print_constant(out, cf, m->name_index);
        fputs("\n      Name:          ", out);
        print_ref(out, cf, m->name_index);
        fputs("\n      Descriptor:    ", out);
        print_ref(out, cf, m->descriptor_index);
        fputs("\n      Access flags:  ", out);
        print_flags(out, m->access_flags, context);
        fputc('\n', out);
        print_attributes(out, cf, m->attributes, m->attributes_count, 6);
        if (i + 1 < count) {
            fputc('\n', out);
        }
    }
}

/**
 * @brief Escreve o conteúdo de um atributo Code.
 *
 * @details **Algoritmo:** escreve `max_stack`, `max_locals` e `code_length`,
 * o bytecode desmontado, a tabela de exceções (tipo "any" quando
 * `catch_type` é 0, isto é, `finally`) e os atributos aninhados.
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
    print_indent(out, indent);
    fprintf(out, "max_stack = %u, max_locals = %u, code_length = %lu\n",
            (unsigned)code->max_stack, (unsigned)code->max_locals,
            (unsigned long)code->code_length);
    print_indent(out, indent);
    fputs("Bytecode:\n", out);
    disassemble_code(out, cf, code, indent + 2);

    print_indent(out, indent);
    fprintf(out, "Exception table (%u):\n", (unsigned)code->exception_table_length);
    if (code->exception_table_length > 0) {
        print_indent(out, indent + 2);
        fputs(" start_pc   end_pc  handler_pc  catch_type\n", out);
    }
    for (i = 0; i < code->exception_table_length; i++) {
        const ExceptionTableEntry *e = &code->exception_table[i];
        print_indent(out, indent + 2);
        fprintf(out, "%9u %8u %11u  ", (unsigned)e->start_pc, (unsigned)e->end_pc,
                (unsigned)e->handler_pc);
        if (e->catch_type == 0) {
            fputs("any (finally)\n", out);
        } else {
            print_ref(out, cf, e->catch_type);
            fputc('\n', out);
        }
    }
    print_attributes(out, cf, code->attributes, code->attributes_count, indent);
}

/**
 * @brief Escreve um `verification_type_info` (ex.: `int`, `class java/lang/String`).
 *
 * @details **Algoritmo:** `switch` sobre a tag; ITEM_Object mostra a classe
 * e ITEM_Uninitialized o offset do `new`.
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
        fputs("class ", out);
        print_constant(out, cf, info->value);
    } else if (info->tag == ITEM_Uninitialized) {
        fprintf(out, "uninitialized(%u)", (unsigned)info->value);
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
        fputs(i > 0 ? ", " : " ", out);
        print_verification_type(out, cf, &items[i]);
    }
    fputs(count > 0 ? " ]" : "]", out);
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
    for (i = 0; i < attr->info.stack_map_table.number_of_entries; i++) {
        const StackMapFrame *f = &attr->info.stack_map_table.entries[i];
        pc = (i == 0) ? f->offset_delta : pc + f->offset_delta + 1u;
        print_indent(out, indent);
        fprintf(out, "frame_type = %u /* %s */, offset_delta = %u (pc %lu)\n",
                (unsigned)f->frame_type, frame_type_name(f->frame_type),
                (unsigned)f->offset_delta, (unsigned long)pc);
        if (f->number_of_locals > 0 || f->frame_type == 255) {
            print_indent(out, indent + 2);
            fputs("locals = ", out);
            print_verification_list(out, cf, f->locals, f->number_of_locals);
            fputc('\n', out);
        }
        if (f->number_of_stack_items > 0 || f->frame_type == 255) {
            print_indent(out, indent + 2);
            fputs("stack = ", out);
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
 * @brief Escreve o conteúdo de um atributo conforme seu tipo.
 *
 * @details **Algoritmo:** `switch` sobre @c kind; cada tipo tem um formato
 * próprio (tabelas para LineNumberTable/LocalVariableTable, listas para
 * InnerClasses/BootstrapMethods, bytecode para Code etc.).
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
        print_indent(out, indent);
        fputs("Value: ", out);
        print_ref(out, cf, attr->info.constantvalue_index);
        fputc('\n', out);
        break;
    case ATTR_CODE:
        print_code(out, cf, attr->info.code, indent);
        break;
    case ATTR_STACK_MAP_TABLE:
        print_stack_map_table(out, cf, attr, indent);
        break;
    case ATTR_EXCEPTIONS:
        for (i = 0; i < attr->info.exceptions.number_of_exceptions; i++) {
            print_indent(out, indent);
            fputs("throws ", out);
            print_ref(out, cf, attr->info.exceptions.exception_index_table[i]);
            fputc('\n', out);
        }
        break;
    case ATTR_INNER_CLASSES:
        for (i = 0; i < attr->info.inner_classes.number_of_classes; i++) {
            const InnerClassEntry *e = &attr->info.inner_classes.classes[i];
            print_indent(out, indent);
            fprintf(out, "[%u] inner: ", (unsigned)i);
            print_ref(out, cf, e->inner_class_info_index);
            fputs(", outer: ", out);
            print_ref(out, cf, e->outer_class_info_index);
            fputs(", name: ", out);
            print_ref(out, cf, e->inner_name_index);
            fputs("\n", out);
            print_indent(out, indent + 6);
            fputs("flags: ", out);
            print_flags(out, e->inner_class_access_flags, FLAGS_INNER_CLASS);
            fputc('\n', out);
        }
        break;
    case ATTR_ENCLOSING_METHOD:
        print_indent(out, indent);
        fputs("Class: ", out);
        print_ref(out, cf, attr->info.enclosing_method.class_index);
        fputs(", method: ", out);
        print_ref(out, cf, attr->info.enclosing_method.method_index);
        fputc('\n', out);
        break;
    case ATTR_SIGNATURE:
        print_indent(out, indent);
        fputs("Signature: ", out);
        print_ref(out, cf, attr->info.signature_index);
        fputc('\n', out);
        break;
    case ATTR_SOURCE_FILE:
        print_indent(out, indent);
        fputs("Source file: ", out);
        print_ref(out, cf, attr->info.sourcefile_index);
        fputc('\n', out);
        break;
    case ATTR_LINE_NUMBER_TABLE:
        for (i = 0; i < attr->info.line_numbers.line_number_table_length; i++) {
            const LineNumberEntry *e = &attr->info.line_numbers.line_number_table[i];
            print_indent(out, indent);
            fprintf(out, "line %u: pc %u\n", (unsigned)e->line_number,
                    (unsigned)e->start_pc);
        }
        break;
    case ATTR_LOCAL_VARIABLE_TABLE:
    case ATTR_LOCAL_VARIABLE_TYPE_TABLE:
        print_indent(out, indent);
        fprintf(out, "%5s %6s %4s  %-16s %s\n", "start", "length", "slot", "name",
                attr->kind == ATTR_LOCAL_VARIABLE_TABLE ? "descriptor" : "signature");
        for (i = 0; i < attr->info.local_variables.local_variable_table_length; i++) {
            const LocalVariableEntry *e = &attr->info.local_variables.local_variable_table[i];
            print_indent(out, indent);
            fprintf(out, "%5u %6u %4u  %-16s ", (unsigned)e->start_pc,
                    (unsigned)e->length, (unsigned)e->index,
                    cp_get_utf8(cf, e->name_index));
            print_constant(out, cf, e->descriptor_index);
            fputc('\n', out);
        }
        break;
    case ATTR_BOOTSTRAP_METHODS:
        for (i = 0; i < attr->info.bootstrap.num_bootstrap_methods; i++) {
            const BootstrapMethod *m = &attr->info.bootstrap.bootstrap_methods[i];
            print_indent(out, indent);
            fprintf(out, "[%u] ", (unsigned)i);
            print_ref(out, cf, m->bootstrap_method_ref);
            fputc('\n', out);
            for (j = 0; j < m->num_bootstrap_arguments; j++) {
                print_indent(out, indent + 4);
                fputs("argument: ", out);
                print_ref(out, cf, m->bootstrap_arguments[j]);
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
 * @details **Algoritmo:** escreve a contagem e, para cada atributo, o
 * cabeçalho `[i] Nome (name_index, length)` seguido do conteúdo
 * (print_attribute_body()) com indentação maior.
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
    print_indent(out, indent);
    fprintf(out, "Attributes (%u):\n", (unsigned)count);
    for (i = 0; i < count; i++) {
        const AttributeInfo *a = &attributes[i];
        print_indent(out, indent + 2);
        fprintf(out, "[%u] %s (name_index #%u, length %lu)\n", (unsigned)i,
                cp_get_utf8(cf, a->attribute_name_index),
                (unsigned)a->attribute_name_index, (unsigned long)a->attribute_length);
        print_attribute_body(out, cf, a, indent + 6);
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
    print_section(out, "Class attributes");
    print_attributes(out, class_file, class_file->attributes,
                     class_file->attributes_count, 2);
}
