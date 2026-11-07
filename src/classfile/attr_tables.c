/**
 * @file attr_tables.c
 * @brief Leitura dos atributos em forma de tabela.
 *
 * @details Exceptions, InnerClasses, LineNumberTable, LocalVariableTable,
 * LocalVariableTypeTable e BootstrapMethods: todos começam com a
 * quantidade de entradas, seguida de entradas de tamanho fixo.
 */
#include "classfile/attr_parsers.h"
#include "classfile/constant_pool.h"

CfStatus attr_parse_index_list(ByteReader *r, const ClassFile *class_file,
                               ConstantTag tag, const char *what,
                               uint16_t *count, uint16_t **out,
                               ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return attr_too_short(r, what, err);
    }
    *out = attr_alloc_array(n, sizeof(uint16_t), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para %s", what);
    }
    *count = n;
    for (i = 0; i < n; i++) {
        CfStatus s;
        if (!br_read_u2(r, &(*out)[i])) {
            return attr_too_short(r, what, err);
        }
        s = attr_check_index(class_file, r, (*out)[i], tag, false, what, err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

CfStatus attr_parse_inner_classes(ByteReader *r, const ClassFile *class_file,
                                  AttributeInfo *attr, ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return attr_too_short(r, "InnerClasses", err);
    }
    attr->info.inner_classes.classes = attr_alloc_array(n, sizeof(InnerClassEntry), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para InnerClasses");
    }
    attr->info.inner_classes.number_of_classes = n;
    for (i = 0; i < n; i++) {
        InnerClassEntry *e = &attr->info.inner_classes.classes[i];
        CfStatus s;
        if (!br_read_u2(r, &e->inner_class_info_index) ||
            !br_read_u2(r, &e->outer_class_info_index) ||
            !br_read_u2(r, &e->inner_name_index) ||
            !br_read_u2(r, &e->inner_class_access_flags)) {
            return attr_too_short(r, "InnerClasses", err);
        }
        s = attr_check_index(class_file, r, e->inner_class_info_index, CONSTANT_Class,
                             false, "InnerClasses.inner_class", err);
        if (s == CF_OK) {
            s = attr_check_index(class_file, r, e->outer_class_info_index, CONSTANT_Class,
                                 true, "InnerClasses.outer_class", err);
        }
        if (s == CF_OK) {
            s = attr_check_index(class_file, r, e->inner_name_index, CONSTANT_Utf8, true,
                                 "InnerClasses.inner_name", err);
        }
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

CfStatus attr_parse_line_numbers(ByteReader *r, AttributeInfo *attr,
                                 ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return attr_too_short(r, "LineNumberTable", err);
    }
    attr->info.line_numbers.line_number_table =
        attr_alloc_array(n, sizeof(LineNumberEntry), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para LineNumberTable");
    }
    attr->info.line_numbers.line_number_table_length = n;
    for (i = 0; i < n; i++) {
        LineNumberEntry *e = &attr->info.line_numbers.line_number_table[i];
        if (!br_read_u2(r, &e->start_pc) || !br_read_u2(r, &e->line_number)) {
            return attr_too_short(r, "LineNumberTable", err);
        }
    }
    return CF_OK;
}

CfStatus attr_parse_local_variables(ByteReader *r, const ClassFile *class_file,
                                    AttributeInfo *attr, ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return attr_too_short(r, "LocalVariableTable", err);
    }
    attr->info.local_variables.local_variable_table =
        attr_alloc_array(n, sizeof(LocalVariableEntry), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r),
                       "sem memoria para LocalVariableTable");
    }
    attr->info.local_variables.local_variable_table_length = n;
    for (i = 0; i < n; i++) {
        LocalVariableEntry *e = &attr->info.local_variables.local_variable_table[i];
        CfStatus s;
        if (!br_read_u2(r, &e->start_pc) || !br_read_u2(r, &e->length) ||
            !br_read_u2(r, &e->name_index) || !br_read_u2(r, &e->descriptor_index) ||
            !br_read_u2(r, &e->index)) {
            return attr_too_short(r, "LocalVariableTable", err);
        }
        s = attr_check_index(class_file, r, e->name_index, CONSTANT_Utf8, false,
                             "LocalVariableTable.name", err);
        if (s == CF_OK) {
            s = attr_check_index(class_file, r, e->descriptor_index, CONSTANT_Utf8, false,
                                 "LocalVariableTable.descriptor", err);
        }
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

CfStatus attr_parse_bootstrap_methods(ByteReader *r, const ClassFile *class_file,
                                      AttributeInfo *attr, ClassFileError *err)
{
    uint16_t n, i, j;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return attr_too_short(r, "BootstrapMethods", err);
    }
    attr->info.bootstrap.bootstrap_methods = attr_alloc_array(n, sizeof(BootstrapMethod), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para BootstrapMethods");
    }
    attr->info.bootstrap.num_bootstrap_methods = n;
    for (i = 0; i < n; i++) {
        BootstrapMethod *m = &attr->info.bootstrap.bootstrap_methods[i];
        uint16_t argc;
        CfStatus s;
        if (!br_read_u2(r, &m->bootstrap_method_ref) || !br_read_u2(r, &argc)) {
            return attr_too_short(r, "BootstrapMethods", err);
        }
        s = attr_check_index(class_file, r, m->bootstrap_method_ref, CONSTANT_MethodHandle,
                             false, "BootstrapMethods.bootstrap_method_ref", err);
        if (s != CF_OK) {
            return s;
        }
        m->bootstrap_arguments = attr_alloc_array(argc, sizeof(uint16_t), &ok);
        if (!ok) {
            return cf_fail(err, CF_ERR_MEMORY, br_offset(r),
                           "sem memoria para BootstrapMethods");
        }
        m->num_bootstrap_arguments = argc;
        for (j = 0; j < argc; j++) {
            if (!br_read_u2(r, &m->bootstrap_arguments[j])) {
                return attr_too_short(r, "BootstrapMethods", err);
            }
            if (cp_tag(class_file, m->bootstrap_arguments[j]) == CONSTANT_Unusable) {
                return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(r) - 2,
                               "argumento de bootstrap #%u invalido",
                               (unsigned)m->bootstrap_arguments[j]);
            }
        }
    }
    return CF_OK;
}
