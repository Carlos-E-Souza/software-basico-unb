/**
 * @file cp_query.c
 * @brief Consultas seguras ao pool de constantes (`cp_check`, `cp_tag`, `cp_get_*`).
 *
 * @details Todas as funções aceitam qualquer índice: um índice fora dos
 * limites ou com tag diferente da esperada resulta em NULL/`false`.
 */
#include <string.h>

#include "classfile/constant_pool.h"

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
