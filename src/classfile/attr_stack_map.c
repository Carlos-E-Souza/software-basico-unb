/**
 * @file attr_stack_map.c
 * @brief Leitura do atributo StackMapTable (JVMS §4.7.4).
 *
 * @details Os quadros, em qualquer uma de suas formas compactas, são normalizados
 * na estrutura #StackMapFrame.
 */
#include "classfile/attr_parsers.h"
#include "classfile/constant_pool.h"

/**
 * @brief Lê um `verification_type_info` do StackMapTable.
 *
 * @details **Algoritmo:** lê a tag (0 a 8); para ITEM_Object lê e valida o
 * `cpool_index` (Class); para ITEM_Uninitialized lê o `offset`.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    info       Estrutura a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_verification_type(ByteReader *r, const ClassFile *class_file,
                                        VerificationTypeInfo *info,
                                        ClassFileError *err)
{
    if (!br_read_u1(r, &info->tag)) {
        return attr_too_short(r, "StackMapTable", err);
    }
    if (info->tag > ITEM_Uninitialized) {
        return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(r) - 1,
                       "verification_type_info com tag invalida (%u)",
                       (unsigned)info->tag);
    }
    if (info->tag == ITEM_Object || info->tag == ITEM_Uninitialized) {
        if (!br_read_u2(r, &info->value)) {
            return attr_too_short(r, "StackMapTable", err);
        }
        if (info->tag == ITEM_Object) {
            return attr_check_index(class_file, r, info->value, CONSTANT_Class, false,
                                    "StackMapTable Object", err);
        }
    }
    return CF_OK;
}

/**
 * @brief Lê um vetor de `verification_type_info`.
 *
 * @details **Algoritmo:** aloca @p count posições, grava o vetor em @p out
 * (para que seja liberado mesmo em caso de erro) e lê cada item com
 * parse_verification_type().
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     count      Quantidade de itens.
 * @param[out]    out        Vetor alocado.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_verification_list(ByteReader *r, const ClassFile *class_file,
                                        uint16_t count, VerificationTypeInfo **out,
                                        ClassFileError *err)
{
    uint16_t i;
    bool ok;
    *out = attr_alloc_array(count, sizeof(VerificationTypeInfo), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para StackMapTable");
    }
    for (i = 0; i < count; i++) {
        CfStatus s = parse_verification_type(r, class_file, &(*out)[i], err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

/**
 * @brief Lê um quadro (`stack_map_frame`) e o normaliza em #StackMapFrame.
 *
 * @details
 * **Algoritmo (JVMS 4.7.4)**, conforme `frame_type`:
 * - 0–63 *same*: `offset_delta = frame_type`;
 * - 64–127 *same_locals_1_stack_item*: `offset_delta = frame_type - 64`, 1 item de pilha;
 * - 128–246: reservados (erro);
 * - 247 *same_locals_1_stack_item_extended*: `offset_delta` (u2) + 1 item de pilha;
 * - 248–250 *chop* e 251 *same_frame_extended*: apenas `offset_delta` (u2);
 * - 252–254 *append*: `offset_delta` + (`frame_type - 251`) locais;
 * - 255 *full_frame*: `offset_delta`, locais e pilha completos.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    frame      Quadro a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_stack_map_frame(ByteReader *r, const ClassFile *class_file,
                                      StackMapFrame *frame, ClassFileError *err)
{
    uint8_t type;
    uint16_t n;
    CfStatus s;

    if (!br_read_u1(r, &type)) {
        return attr_too_short(r, "StackMapTable", err);
    }
    frame->frame_type = type;

    if (type <= 63) {
        frame->offset_delta = type;
        return CF_OK;
    }
    if (type <= 127) {
        frame->offset_delta = (uint16_t)(type - 64);
        frame->number_of_stack_items = 1;
        return parse_verification_list(r, class_file, 1, &frame->stack, err);
    }
    if (type <= 246) {
        return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(r) - 1,
                       "frame_type reservado (%u) no StackMapTable", (unsigned)type);
    }
    if (!br_read_u2(r, &frame->offset_delta)) {
        return attr_too_short(r, "StackMapTable", err);
    }
    if (type == 247) {
        frame->number_of_stack_items = 1;
        return parse_verification_list(r, class_file, 1, &frame->stack, err);
    }
    if (type <= 251) {
        return CF_OK; /* chop ou same_frame_extended */
    }
    if (type <= 254) {
        frame->number_of_locals = (uint16_t)(type - 251);
        return parse_verification_list(r, class_file, frame->number_of_locals,
                                       &frame->locals, err);
    }
    /* 255: full_frame */
    if (!br_read_u2(r, &n)) {
        return attr_too_short(r, "StackMapTable", err);
    }
    s = parse_verification_list(r, class_file, n, &frame->locals, err);
    frame->number_of_locals = (frame->locals != NULL) ? n : 0;
    if (s != CF_OK) {
        return s;
    }
    if (!br_read_u2(r, &n)) {
        return attr_too_short(r, "StackMapTable", err);
    }
    s = parse_verification_list(r, class_file, n, &frame->stack, err);
    frame->number_of_stack_items = (frame->stack != NULL) ? n : 0;
    return s;
}

CfStatus attr_parse_stack_map_table(ByteReader *r, const ClassFile *class_file,
                                    AttributeInfo *attr, ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return attr_too_short(r, "StackMapTable", err);
    }
    attr->info.stack_map_table.entries = attr_alloc_array(n, sizeof(StackMapFrame), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para StackMapTable");
    }
    attr->info.stack_map_table.number_of_entries = n;
    for (i = 0; i < n; i++) {
        CfStatus s = parse_stack_map_frame(r, class_file,
                                           &attr->info.stack_map_table.entries[i], err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}
