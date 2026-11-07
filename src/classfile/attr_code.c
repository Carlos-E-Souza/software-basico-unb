/**
 * @file attr_code.c
 * @brief Leitura do atributo Code.
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/attr_parsers.h"
#include "classfile/attributes.h"

CfStatus attr_parse_code(ByteReader *r, const ClassFile *class_file,
                         unsigned depth, AttributeInfo *attr,
                         ClassFileError *err)
{
    CodeAttribute *code;
    const uint8_t *bytes;
    uint16_t length;
    uint16_t i;
    bool ok;

    code = calloc(1, sizeof *code);
    if (code == NULL) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para Code");
    }
    attr->info.code = code;

    if (!br_read_u2(r, &code->max_stack) || !br_read_u2(r, &code->max_locals) ||
        !br_read_u4(r, &code->code_length)) {
        return attr_too_short(r, "Code", err);
    }
    if (code->code_length == 0 || code->code_length > 65535u) {
        return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(r) - 4,
                       "code_length invalido (%lu)", (unsigned long)code->code_length);
    }
    if (!br_read_bytes(r, code->code_length, &bytes)) {
        return attr_too_short(r, "Code (bytecode)", err);
    }
    code->code = malloc(code->code_length);
    if (code->code == NULL) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para bytecode");
    }
    memcpy(code->code, bytes, code->code_length);

    if (!br_read_u2(r, &length)) {
        return attr_too_short(r, "Code (exception_table_length)", err);
    }
    code->exception_table = attr_alloc_array(length, sizeof(ExceptionTableEntry), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para exception_table");
    }
    code->exception_table_length = length;
    for (i = 0; i < length; i++) {
        ExceptionTableEntry *e = &code->exception_table[i];
        CfStatus s;
        if (!br_read_u2(r, &e->start_pc) || !br_read_u2(r, &e->end_pc) ||
            !br_read_u2(r, &e->handler_pc) || !br_read_u2(r, &e->catch_type)) {
            return attr_too_short(r, "Code (exception_table)", err);
        }
        if (e->start_pc >= e->end_pc || e->end_pc > code->code_length ||
            e->handler_pc >= code->code_length) {
            return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(r) - 8,
                           "entrada %u da exception_table fora do codigo", (unsigned)i);
        }
        s = attr_check_index(class_file, r, e->catch_type, CONSTANT_Class, true,
                             "exception_table.catch_type", err);
        if (s != CF_OK) {
            return s;
        }
    }
    return attributes_parse(r, class_file, depth + 1, &code->attributes_count,
                            &code->attributes, err);
}
