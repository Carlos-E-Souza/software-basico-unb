/**
 * @file cp_validate.c
 * @brief Validação das referências internas do pool de constantes.
 *
 * @details Depois de lido por cp_parse(), o pool é conferido por
 * cp_validate(): cada índice guardado em uma entrada deve apontar para uma
 * entrada do tipo exigido pela JVMS §4.4.
 */
#include "classfile/constant_pool.h"

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
