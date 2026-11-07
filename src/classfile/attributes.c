/**
 * @file attributes.c
 * @brief Leitura genérica de atributos: cabeçalho, sub-leitor e despacho por tipo.
 *
 * @details Cada tipo de atributo tem sua função `attr_parse_*` (distribuídas
 * nos arquivos `attr_*.c`, veja attr_parsers.h), que lê o conteúdo a partir
 * de um sub-leitor limitado a `attribute_length` bytes. Se o conteúdo for
 * menor que o declarado, a leitura falha no sub-leitor e o erro é
 * #CF_ERR_BAD_ATTRIBUTE; se sobrarem bytes, também.
 */
#include <string.h>

#include "classfile/attr_parsers.h"
#include "classfile/attributes.h"
#include "classfile/constant_pool.h"

/** @brief Associação entre nome e tipo de atributo. */
typedef struct AttributeName {
    const char *name;   /**< Nome na especificação. */
    AttributeKind kind; /**< Tipo correspondente. */
} AttributeName;

/** @brief Tabela dos atributos reconhecidos. */
static const AttributeName ATTRIBUTE_NAMES[] = {
    {"ConstantValue", ATTR_CONSTANT_VALUE},
    {"Code", ATTR_CODE},
    {"StackMapTable", ATTR_STACK_MAP_TABLE},
    {"Exceptions", ATTR_EXCEPTIONS},
    {"InnerClasses", ATTR_INNER_CLASSES},
    {"EnclosingMethod", ATTR_ENCLOSING_METHOD},
    {"Synthetic", ATTR_SYNTHETIC},
    {"Signature", ATTR_SIGNATURE},
    {"SourceFile", ATTR_SOURCE_FILE},
    {"LineNumberTable", ATTR_LINE_NUMBER_TABLE},
    {"LocalVariableTable", ATTR_LOCAL_VARIABLE_TABLE},
    {"LocalVariableTypeTable", ATTR_LOCAL_VARIABLE_TYPE_TABLE},
    {"Deprecated", ATTR_DEPRECATED},
    {"BootstrapMethods", ATTR_BOOTSTRAP_METHODS},
};

AttributeKind attribute_kind_from_name(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof ATTRIBUTE_NAMES / sizeof ATTRIBUTE_NAMES[0]; i++) {
        if (strcmp(ATTRIBUTE_NAMES[i].name, name) == 0) {
            return ATTRIBUTE_NAMES[i].kind;
        }
    }
    return ATTR_UNKNOWN;
}

const AttributeInfo *attribute_find(const AttributeInfo *attributes,
                                    uint16_t count, AttributeKind kind)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        if (attributes[i].kind == kind) {
            return &attributes[i];
        }
    }
    return NULL;
}

/**
 * @brief Interpreta o conteúdo de um atributo conforme seu tipo.
 *
 * @details **Algoritmo:** `switch` sobre @c attr->kind, delegando para a
 * função `attr_parse_*` do tipo; Synthetic e Deprecated não têm conteúdo;
 * desconhecidos são copiados.
 *
 * @param[in,out] r          Sub-leitor limitado a `attribute_length`.
 * @param[in]     class_file Classe.
 * @param[in]     depth      Profundidade de aninhamento.
 * @param[in,out] attr       Atributo com nome, tamanho e tipo já preenchidos.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_attribute_body(ByteReader *r, const ClassFile *class_file,
                                     unsigned depth, AttributeInfo *attr,
                                     ClassFileError *err)
{
    switch (attr->kind) {
    case ATTR_CONSTANT_VALUE:
        return attr_parse_constant_value(r, class_file, attr, err);
    case ATTR_CODE:
        return attr_parse_code(r, class_file, depth, attr, err);
    case ATTR_STACK_MAP_TABLE:
        return attr_parse_stack_map_table(r, class_file, attr, err);
    case ATTR_EXCEPTIONS:
        return attr_parse_index_list(r, class_file, CONSTANT_Class, "Exceptions",
                                     &attr->info.exceptions.number_of_exceptions,
                                     &attr->info.exceptions.exception_index_table, err);
    case ATTR_INNER_CLASSES:
        return attr_parse_inner_classes(r, class_file, attr, err);
    case ATTR_ENCLOSING_METHOD:
        return attr_parse_enclosing_method(r, class_file, attr, err);
    case ATTR_SIGNATURE:
        return attr_parse_utf8_index(r, class_file, "Signature",
                                     &attr->info.signature_index, err);
    case ATTR_SOURCE_FILE:
        return attr_parse_utf8_index(r, class_file, "SourceFile",
                                     &attr->info.sourcefile_index, err);
    case ATTR_LINE_NUMBER_TABLE:
        return attr_parse_line_numbers(r, attr, err);
    case ATTR_LOCAL_VARIABLE_TABLE:
    case ATTR_LOCAL_VARIABLE_TYPE_TABLE:
        return attr_parse_local_variables(r, class_file, attr, err);
    case ATTR_BOOTSTRAP_METHODS:
        return attr_parse_bootstrap_methods(r, class_file, attr, err);
    case ATTR_SYNTHETIC:
    case ATTR_DEPRECATED:
        return CF_OK;
    case ATTR_UNKNOWN:
        break;
    }
    return attr_parse_unknown(r, attr, err);
}

/**
 * @brief Lê um atributo completo (cabeçalho + conteúdo).
 *
 * @details
 * **Algoritmo:**
 * 1. Lê `attribute_name_index` (Utf8) e `attribute_length`;
 * 2. Cria um sub-leitor com `attribute_length` bytes (erro de truncamento se
 *    o arquivo for menor);
 * 3. Identifica o tipo pelo nome (Code aninhado vira desconhecido);
 * 4. Interpreta o conteúdo e exige que todos os bytes tenham sido consumidos.
 *
 * @param[in,out] reader     Leitor posicionado no início do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     depth      Profundidade de aninhamento.
 * @param[out]    attr       Atributo a preencher (zerado).
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_attribute(ByteReader *reader, const ClassFile *class_file,
                                unsigned depth, AttributeInfo *attr,
                                ClassFileError *err)
{
    ByteReader sub;
    const char *name;
    CfStatus s;

    if (!br_read_u2(reader, &attr->attribute_name_index) ||
        !br_read_u4(reader, &attr->attribute_length)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "cabecalho de atributo truncado");
    }
    name = cp_get_utf8(class_file, attr->attribute_name_index);
    if (name == NULL) {
        return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(reader) - 6,
                       "attribute_name_index #%u nao e Utf8",
                       (unsigned)attr->attribute_name_index);
    }
    if (!br_sub_reader(reader, attr->attribute_length, &sub)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "atributo %s truncado (%lu bytes declarados, %lu disponiveis)",
                       name, (unsigned long)attr->attribute_length,
                       (unsigned long)br_remaining(reader));
    }
    attr->kind = attribute_kind_from_name(name);
    if (attr->kind == ATTR_CODE && depth > 0) {
        attr->kind = ATTR_UNKNOWN; /* Code dentro de Code não é válido */
    }
    s = parse_attribute_body(&sub, class_file, depth, attr, err);
    if (s != CF_OK) {
        return s;
    }
    if (br_remaining(&sub) != 0) {
        return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(&sub),
                       "atributo %s tem %lu byte(s) alem do conteudo", name,
                       (unsigned long)br_remaining(&sub));
    }
    return CF_OK;
}

CfStatus attributes_parse(ByteReader *reader, const ClassFile *class_file,
                          unsigned depth, uint16_t *count,
                          AttributeInfo **attributes, ClassFileError *err)
{
    uint16_t n, i;
    bool ok;

    *count = 0;
    *attributes = NULL;
    if (!br_read_u2(reader, &n)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "attributes_count ausente");
    }
    *attributes = attr_alloc_array(n, sizeof(AttributeInfo), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(reader),
                       "sem memoria para atributos");
    }
    *count = n;
    for (i = 0; i < n; i++) {
        CfStatus s = parse_attribute(reader, class_file, depth, &(*attributes)[i], err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}
