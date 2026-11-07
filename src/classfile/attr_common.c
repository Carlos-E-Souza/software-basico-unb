/**
 * @file attr_common.c
 * @brief Utilitários comuns à leitura de atributos.
 */
#include <stdlib.h>

#include "classfile/attr_parsers.h"
#include "classfile/constant_pool.h"

CfStatus attr_too_short(const ByteReader *reader, const char *what,
                        ClassFileError *err)
{
    return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(reader),
                   "atributo %s menor que attribute_length indica", what);
}

CfStatus attr_check_index(const ClassFile *class_file, const ByteReader *reader,
                          uint16_t index, ConstantTag tag, bool allow_zero,
                          const char *what, ClassFileError *err)
{
    if ((allow_zero && index == 0) || cp_check(class_file, index, tag)) {
        return CF_OK;
    }
    return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(reader),
                   "%s: indice #%u deveria ser %s", what, (unsigned)index,
                   cp_tag_name(tag));
}

void *attr_alloc_array(size_t count, size_t size, bool *ok)
{
    void *p;
    if (count == 0) {
        *ok = true;
        return NULL;
    }
    p = calloc(count, size);
    *ok = (p != NULL);
    return p;
}
