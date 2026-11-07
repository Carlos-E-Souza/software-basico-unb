/**
 * @file printer.c
 * @brief Implementação do exibidor das estruturas de um `.class`.
 *
 * @details Este arquivo contém a função pública viewer_print_class(), os
 * auxiliares de linha (`Rotulo: valor`) e as seções General Information,
 * Interfaces, Fields e Methods; as demais seções ficam em
 * print_constant_pool.c, print_attributes.c e print_stack_map.c.
 *
 * A saída segue a organização do visualizador jclasslib: informações
 * gerais, pool de constantes, interfaces, campos, métodos e atributos, com os
 * mesmos rótulos. Toda referência ao pool é escrita como
 * `cp_info #n <constante>`, isto é, com o índice do slot e a constante que ele
 * representa. Os textos fixos são escritos sem acentos para que a saída seja
 * legível em qualquer terminal, inclusive no Windows.
 */
#include <inttypes.h>
#include <string.h>

#include "viewer/printer_internal.h"
#include "viewer/viewer.h"

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
