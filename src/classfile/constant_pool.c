/**
 * @file constant_pool.c
 * @brief Leitura e liberação do pool de constantes.
 *
 * @details A validação das referências entre entradas fica em cp_validate.c
 * e as funções de consulta em cp_query.c.
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/constant_pool.h"
#include "common/mutf8.h"

/**
 * @brief Lê o conteúdo de uma entrada `CONSTANT_Utf8`.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê `length` e obtém os bytes;
 * 2. Valida o MUTF-8 (mutf8_is_valid());
 * 3. Copia os bytes para memória própria, acrescentando '\\0'.
 *
 * @param[in,out] reader Leitor posicionado após a tag.
 * @param[out]    entry  Entrada a preencher.
 * @param[in]     index  Índice da entrada (para mensagens de erro).
 * @param[out]    err    Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_utf8(ByteReader *reader, CpInfo *entry, uint16_t index,
                           ClassFileError *err)
{
    uint16_t length;
    const uint8_t *bytes;
    size_t start = br_offset(reader);

    if (!br_read_u2(reader, &length) ||
        !br_read_bytes(reader, length, &bytes)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "Utf8 #%u truncado", (unsigned)index);
    }
    if (!mutf8_is_valid(bytes, length)) {
        return cf_fail(err, CF_ERR_BAD_CONSTANT, start,
                       "Utf8 #%u nao e UTF-8 modificado valido", (unsigned)index);
    }
    entry->info.utf8.bytes = malloc((size_t)length + 1u);
    if (entry->info.utf8.bytes == NULL) {
        return cf_fail(err, CF_ERR_MEMORY, start, "sem memoria para Utf8 #%u",
                       (unsigned)index);
    }
    if (length > 0) {
        memcpy(entry->info.utf8.bytes, bytes, length);
    }
    entry->info.utf8.bytes[length] = '\0';
    entry->info.utf8.length = length;
    return CF_OK;
}

/**
 * @brief Lê o conteúdo de uma entrada do pool conforme sua tag.
 *
 * @details
 * **Algoritmo:** `switch` sobre a tag lendo os campos de tamanho fixo de cada
 * tipo (u1/u2/u4); Utf8 é delegado para parse_utf8(). Uma tag desconhecida
 * gera #CF_ERR_BAD_CONSTANT.
 *
 * @param[in,out] reader Leitor posicionado após a tag.
 * @param[in,out] entry  Entrada com a tag já preenchida.
 * @param[in]     index  Índice da entrada (para mensagens de erro).
 * @param[out]    err    Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_entry(ByteReader *reader, CpInfo *entry, uint16_t index,
                            ClassFileError *err)
{
    bool ok;
    switch ((ConstantTag)entry->tag) {
    case CONSTANT_Utf8:
        return parse_utf8(reader, entry, index, err);
    case CONSTANT_Integer:
        ok = br_read_u4(reader, &entry->info.integer_bytes);
        break;
    case CONSTANT_Float:
        ok = br_read_u4(reader, &entry->info.float_bytes);
        break;
    case CONSTANT_Long:
    case CONSTANT_Double:
        ok = br_read_u4(reader, &entry->info.wide.high_bytes) &&
             br_read_u4(reader, &entry->info.wide.low_bytes);
        break;
    case CONSTANT_Class:
        ok = br_read_u2(reader, &entry->info.class_name_index);
        break;
    case CONSTANT_String:
        ok = br_read_u2(reader, &entry->info.string_index);
        break;
    case CONSTANT_Fieldref:
    case CONSTANT_Methodref:
    case CONSTANT_InterfaceMethodref:
        ok = br_read_u2(reader, &entry->info.ref.class_index) &&
             br_read_u2(reader, &entry->info.ref.name_and_type_index);
        break;
    case CONSTANT_NameAndType:
        ok = br_read_u2(reader, &entry->info.name_and_type.name_index) &&
             br_read_u2(reader, &entry->info.name_and_type.descriptor_index);
        break;
    case CONSTANT_MethodHandle:
        ok = br_read_u1(reader, &entry->info.method_handle.reference_kind) &&
             br_read_u2(reader, &entry->info.method_handle.reference_index);
        break;
    case CONSTANT_MethodType:
        ok = br_read_u2(reader, &entry->info.method_type_index);
        break;
    case CONSTANT_InvokeDynamic:
        ok = br_read_u2(reader,
                        &entry->info.invoke_dynamic.bootstrap_method_attr_index) &&
             br_read_u2(reader, &entry->info.invoke_dynamic.name_and_type_index);
        break;
    default:
        return cf_fail(err, CF_ERR_BAD_CONSTANT, br_offset(reader) - 1,
                       "tag %u desconhecida na constante #%u",
                       (unsigned)entry->tag, (unsigned)index);
    }
    if (!ok) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "constante #%u (%s) truncada", (unsigned)index,
                       cp_tag_name((ConstantTag)entry->tag));
    }
    return CF_OK;
}

CfStatus cp_parse(ByteReader *reader, ClassFile *class_file, ClassFileError *err)
{
    uint16_t count;
    uint16_t i;

    if (!br_read_u2(reader, &count)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "constant_pool_count ausente");
    }
    if (count == 0) {
        return cf_fail(err, CF_ERR_BAD_CONSTANT, br_offset(reader) - 2,
                       "constant_pool_count nao pode ser 0");
    }
    class_file->constant_pool = calloc(count, sizeof(CpInfo));
    if (class_file->constant_pool == NULL) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(reader),
                       "sem memoria para o pool de constantes");
    }
    class_file->constant_pool_count = count;

    for (i = 1; i < count; i++) {
        CpInfo *entry = &class_file->constant_pool[i];
        CfStatus status;
        if (!br_read_u1(reader, &entry->tag)) {
            return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                           "tag da constante #%u ausente", (unsigned)i);
        }
        status = parse_entry(reader, entry, i, err);
        if (status != CF_OK) {
            return status; /* a entrada parcial é liberada por cp_free() */
        }
        if (entry->tag == CONSTANT_Long || entry->tag == CONSTANT_Double) {
            if (i + 1 >= count) {
                return cf_fail(err, CF_ERR_BAD_CONSTANT, br_offset(reader),
                               "constante #%u (%s) ocupa 2 posicoes mas e a ultima",
                               (unsigned)i, cp_tag_name((ConstantTag)entry->tag));
            }
            i++; /* a próxima posição fica CONSTANT_Unusable */
        }
    }
    return CF_OK;
}

void cp_free(ClassFile *class_file)
{
    uint16_t i;
    if (class_file->constant_pool == NULL) {
        return;
    }
    for (i = 1; i < class_file->constant_pool_count; i++) {
        if (class_file->constant_pool[i].tag == CONSTANT_Utf8) {
            free(class_file->constant_pool[i].info.utf8.bytes);
        }
    }
    free(class_file->constant_pool);
    class_file->constant_pool = NULL;
    class_file->constant_pool_count = 0;
}
