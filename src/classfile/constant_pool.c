/**
 * @file constant_pool.c
 * @brief Implementação da leitura, validação e consulta do pool de constantes.
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

bool cp_check(const ClassFile *class_file, uint16_t index, ConstantTag tag)
{
    return index >= 1 && index < class_file->constant_pool_count &&
           class_file->constant_pool[index].tag == (uint8_t)tag;
}

ConstantTag cp_tag(const ClassFile *class_file, uint16_t index)
{
    if (index == 0 || index >= class_file->constant_pool_count) {
        return CONSTANT_Unusable;
    }
    return (ConstantTag)class_file->constant_pool[index].tag;
}

/**
 * @brief Verifica um índice usado por uma entrada do pool e registra erro se inválido.
 *
 * @details **Algoritmo:** chama cp_check() e, em caso de falha, preenche
 * @p err indicando a entrada de origem, o índice e a tag esperada.
 *
 * @param[in]  class_file Classe.
 * @param[in]  owner      Índice da entrada que contém a referência.
 * @param[in]  index      Índice referenciado.
 * @param[in]  tag        Tag esperada para @p index.
 * @param[out] err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou #CF_ERR_BAD_INDEX.
 */
static CfStatus check_ref(const ClassFile *class_file, uint16_t owner,
                          uint16_t index, ConstantTag tag, ClassFileError *err)
{
    if (cp_check(class_file, index, tag)) {
        return CF_OK;
    }
    return cf_fail(err, CF_ERR_BAD_INDEX, 0,
                   "constante #%u (%s) referencia #%u, que deveria ser %s",
                   (unsigned)owner,
                   cp_tag_name(cp_tag(class_file, owner)), (unsigned)index,
                   cp_tag_name(tag));
}

/**
 * @brief Verifica se a referência de um MethodHandle tem o tipo exigido pelo `reference_kind`.
 *
 * @details
 * **Algoritmo (JVMS 4.4.8):**
 * 1. `reference_kind` 1–4 (get/put field/static) exige Fieldref;
 * 2. 5 e 8 (invokeVirtual, newInvokeSpecial) exigem Methodref;
 * 3. 6 e 7 (invokeStatic, invokeSpecial) aceitam Methodref ou InterfaceMethodref;
 * 4. 9 (invokeInterface) exige InterfaceMethodref;
 * 5. Outros valores são inválidos.
 *
 * @param[in]  class_file Classe.
 * @param[in]  owner      Índice do MethodHandle.
 * @param[out] err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK, #CF_ERR_BAD_CONSTANT ou #CF_ERR_BAD_INDEX.
 */
static CfStatus check_method_handle(const ClassFile *class_file, uint16_t owner,
                                    ClassFileError *err)
{
    const CpMethodHandle *mh = &class_file->constant_pool[owner].info.method_handle;
    uint8_t kind = mh->reference_kind;
    uint16_t ref = mh->reference_index;

    if (kind >= 1 && kind <= 4) {
        return check_ref(class_file, owner, ref, CONSTANT_Fieldref, err);
    }
    if (kind == 5 || kind == 8) {
        return check_ref(class_file, owner, ref, CONSTANT_Methodref, err);
    }
    if (kind == 6 || kind == 7) {
        if (cp_check(class_file, ref, CONSTANT_InterfaceMethodref)) {
            return CF_OK;
        }
        return check_ref(class_file, owner, ref, CONSTANT_Methodref, err);
    }
    if (kind == 9) {
        return check_ref(class_file, owner, ref, CONSTANT_InterfaceMethodref, err);
    }
    return cf_fail(err, CF_ERR_BAD_CONSTANT, 0,
                   "MethodHandle #%u com reference_kind invalido (%u)",
                   (unsigned)owner, (unsigned)kind);
}

CfStatus cp_validate(const ClassFile *class_file, ClassFileError *err)
{
    uint16_t i;
    for (i = 1; i < class_file->constant_pool_count; i++) {
        const CpInfo *e = &class_file->constant_pool[i];
        CfStatus s = CF_OK;
        switch ((ConstantTag)e->tag) {
        case CONSTANT_Class:
            s = check_ref(class_file, i, e->info.class_name_index, CONSTANT_Utf8, err);
            break;
        case CONSTANT_String:
            s = check_ref(class_file, i, e->info.string_index, CONSTANT_Utf8, err);
            break;
        case CONSTANT_Fieldref:
        case CONSTANT_Methodref:
        case CONSTANT_InterfaceMethodref:
            s = check_ref(class_file, i, e->info.ref.class_index, CONSTANT_Class, err);
            if (s == CF_OK) {
                s = check_ref(class_file, i, e->info.ref.name_and_type_index,
                              CONSTANT_NameAndType, err);
            }
            break;
        case CONSTANT_NameAndType:
            s = check_ref(class_file, i, e->info.name_and_type.name_index,
                          CONSTANT_Utf8, err);
            if (s == CF_OK) {
                s = check_ref(class_file, i, e->info.name_and_type.descriptor_index,
                              CONSTANT_Utf8, err);
            }
            break;
        case CONSTANT_MethodHandle:
            s = check_method_handle(class_file, i, err);
            break;
        case CONSTANT_MethodType:
            s = check_ref(class_file, i, e->info.method_type_index, CONSTANT_Utf8, err);
            break;
        case CONSTANT_InvokeDynamic:
            s = check_ref(class_file, i, e->info.invoke_dynamic.name_and_type_index,
                          CONSTANT_NameAndType, err);
            break;
        default:
            break; /* Utf8, numéricos e posições inutilizáveis não têm referências */
        }
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

const char *cp_tag_name(ConstantTag tag)
{
    switch (tag) {
    case CONSTANT_Unusable:           return "(inutilizavel)";
    case CONSTANT_Utf8:               return "Utf8";
    case CONSTANT_Integer:            return "Integer";
    case CONSTANT_Float:              return "Float";
    case CONSTANT_Long:               return "Long";
    case CONSTANT_Double:             return "Double";
    case CONSTANT_Class:              return "Class";
    case CONSTANT_String:             return "String";
    case CONSTANT_Fieldref:           return "Fieldref";
    case CONSTANT_Methodref:          return "Methodref";
    case CONSTANT_InterfaceMethodref: return "InterfaceMethodref";
    case CONSTANT_NameAndType:        return "NameAndType";
    case CONSTANT_MethodHandle:       return "MethodHandle";
    case CONSTANT_MethodType:         return "MethodType";
    case CONSTANT_InvokeDynamic:      return "InvokeDynamic";
    }
    return "Desconhecida";
}

const char *cp_get_utf8(const ClassFile *class_file, uint16_t index)
{
    if (!cp_check(class_file, index, CONSTANT_Utf8)) {
        return NULL;
    }
    return class_file->constant_pool[index].info.utf8.bytes;
}

const char *cp_get_class_name(const ClassFile *class_file, uint16_t index)
{
    if (!cp_check(class_file, index, CONSTANT_Class)) {
        return NULL;
    }
    return cp_get_utf8(class_file,
                       class_file->constant_pool[index].info.class_name_index);
}

const char *cp_get_string(const ClassFile *class_file, uint16_t index)
{
    if (!cp_check(class_file, index, CONSTANT_String)) {
        return NULL;
    }
    return cp_get_utf8(class_file, class_file->constant_pool[index].info.string_index);
}

bool cp_get_name_and_type(const ClassFile *class_file, uint16_t index,
                          const char **name, const char **descriptor)
{
    const CpNameAndType *nt;
    if (!cp_check(class_file, index, CONSTANT_NameAndType)) {
        return false;
    }
    nt = &class_file->constant_pool[index].info.name_and_type;
    *name = cp_get_utf8(class_file, nt->name_index);
    *descriptor = cp_get_utf8(class_file, nt->descriptor_index);
    return *name != NULL && *descriptor != NULL;
}

bool cp_get_ref(const ClassFile *class_file, uint16_t index,
                const char **class_name, const char **name,
                const char **descriptor)
{
    ConstantTag tag = cp_tag(class_file, index);
    const CpRef *ref;
    if (tag != CONSTANT_Fieldref && tag != CONSTANT_Methodref &&
        tag != CONSTANT_InterfaceMethodref) {
        return false;
    }
    ref = &class_file->constant_pool[index].info.ref;
    *class_name = cp_get_class_name(class_file, ref->class_index);
    return *class_name != NULL &&
           cp_get_name_and_type(class_file, ref->name_and_type_index, name,
                                descriptor);
}

bool cp_get_int(const ClassFile *class_file, uint16_t index, int32_t *value)
{
    uint32_t bits;
    if (!cp_check(class_file, index, CONSTANT_Integer)) {
        return false;
    }
    bits = class_file->constant_pool[index].info.integer_bytes;
    memcpy(value, &bits, sizeof bits); /* complemento de dois */
    return true;
}

bool cp_get_float(const ClassFile *class_file, uint16_t index, float *value)
{
    uint32_t bits;
    if (!cp_check(class_file, index, CONSTANT_Float)) {
        return false;
    }
    bits = class_file->constant_pool[index].info.float_bytes;
    memcpy(value, &bits, sizeof bits);
    return true;
}

bool cp_get_long(const ClassFile *class_file, uint16_t index, int64_t *value)
{
    const CpWide *w;
    uint64_t bits;
    if (!cp_check(class_file, index, CONSTANT_Long)) {
        return false;
    }
    w = &class_file->constant_pool[index].info.wide;
    bits = ((uint64_t)w->high_bytes << 32) | (uint64_t)w->low_bytes;
    memcpy(value, &bits, sizeof bits);
    return true;
}

bool cp_get_double(const ClassFile *class_file, uint16_t index, double *value)
{
    const CpWide *w;
    uint64_t bits;
    if (!cp_check(class_file, index, CONSTANT_Double)) {
        return false;
    }
    w = &class_file->constant_pool[index].info.wide;
    bits = ((uint64_t)w->high_bytes << 32) | (uint64_t)w->low_bytes;
    memcpy(value, &bits, sizeof bits);
    return true;
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
