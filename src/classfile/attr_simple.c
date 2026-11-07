/**
 * @file attr_simple.c
 * @brief Leitura dos atributos de tamanho fixo e dos desconhecidos.
 *
 * @details ConstantValue, EnclosingMethod, Signature e SourceFile contêm apenas
 * índices para o pool; atributos não reconhecidos são copiados como bytes
 * brutos.
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/attr_parsers.h"
#include "classfile/constant_pool.h"

CfStatus attr_parse_constant_value(ByteReader *r, const ClassFile *class_file,
                                   AttributeInfo *attr, ClassFileError *err)
{
    ConstantTag tag;
    if (!br_read_u2(r, &attr->info.constantvalue_index)) {
        return attr_too_short(r, "ConstantValue", err);
    }
    tag = cp_tag(class_file, attr->info.constantvalue_index);
    if (tag != CONSTANT_Integer && tag != CONSTANT_Float && tag != CONSTANT_Long &&
        tag != CONSTANT_Double && tag != CONSTANT_String) {
        return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(r),
                       "ConstantValue aponta para #%u (%s), que nao e constante",
                       (unsigned)attr->info.constantvalue_index, cp_tag_name(tag));
    }
    return CF_OK;
}

CfStatus attr_parse_enclosing_method(ByteReader *r, const ClassFile *class_file,
                                     AttributeInfo *attr, ClassFileError *err)
{
    CfStatus s;
    if (!br_read_u2(r, &attr->info.enclosing_method.class_index) ||
        !br_read_u2(r, &attr->info.enclosing_method.method_index)) {
        return attr_too_short(r, "EnclosingMethod", err);
    }
    s = attr_check_index(class_file, r, attr->info.enclosing_method.class_index,
                         CONSTANT_Class, false, "EnclosingMethod.class", err);
    if (s != CF_OK) {
        return s;
    }
    return attr_check_index(class_file, r, attr->info.enclosing_method.method_index,
                            CONSTANT_NameAndType, true, "EnclosingMethod.method", err);
}

CfStatus attr_parse_utf8_index(ByteReader *r, const ClassFile *class_file,
                               const char *what, uint16_t *index,
                               ClassFileError *err)
{
    if (!br_read_u2(r, index)) {
        return attr_too_short(r, what, err);
    }
    return attr_check_index(class_file, r, *index, CONSTANT_Utf8, false, what, err);
}

CfStatus attr_parse_unknown(ByteReader *r, AttributeInfo *attr, ClassFileError *err)
{
    const uint8_t *bytes;
    if (attr->attribute_length == 0) {
        return CF_OK;
    }
    if (!br_read_bytes(r, attr->attribute_length, &bytes)) {
        return attr_too_short(r, "desconhecido", err);
    }
    attr->info.raw = malloc(attr->attribute_length);
    if (attr->info.raw == NULL) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para atributo");
    }
    memcpy(attr->info.raw, bytes, attr->attribute_length);
    return CF_OK;
}
