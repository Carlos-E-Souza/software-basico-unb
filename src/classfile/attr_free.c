/**
 * @file attr_free.c
 * @brief Liberação da memória dos atributos.
 */
#include <stdlib.h>

#include "classfile/attributes.h"

/**
 * @brief Libera o conteúdo de um atributo Code.
 *
 * @details **Algoritmo:** libera o bytecode, a tabela de exceções, os
 * atributos aninhados (recursivamente) e a estrutura.
 *
 * @param[in] code Atributo Code (pode ser NULL).
 */
static void code_free(CodeAttribute *code)
{
    if (code == NULL) {
        return;
    }
    free(code->code);
    free(code->exception_table);
    attributes_free(code->attributes, code->attributes_count);
    free(code);
}

void attributes_free(AttributeInfo *attributes, uint16_t count)
{
    uint16_t i, j;
    if (attributes == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        AttributeInfo *a = &attributes[i];
        switch (a->kind) {
        case ATTR_CODE:
            code_free(a->info.code);
            break;
        case ATTR_STACK_MAP_TABLE:
            for (j = 0; j < a->info.stack_map_table.number_of_entries; j++) {
                free(a->info.stack_map_table.entries[j].locals);
                free(a->info.stack_map_table.entries[j].stack);
            }
            free(a->info.stack_map_table.entries);
            break;
        case ATTR_EXCEPTIONS:
            free(a->info.exceptions.exception_index_table);
            break;
        case ATTR_INNER_CLASSES:
            free(a->info.inner_classes.classes);
            break;
        case ATTR_LINE_NUMBER_TABLE:
            free(a->info.line_numbers.line_number_table);
            break;
        case ATTR_LOCAL_VARIABLE_TABLE:
        case ATTR_LOCAL_VARIABLE_TYPE_TABLE:
            free(a->info.local_variables.local_variable_table);
            break;
        case ATTR_BOOTSTRAP_METHODS:
            for (j = 0; j < a->info.bootstrap.num_bootstrap_methods; j++) {
                free(a->info.bootstrap.bootstrap_methods[j].bootstrap_arguments);
            }
            free(a->info.bootstrap.bootstrap_methods);
            break;
        case ATTR_UNKNOWN:
            free(a->info.raw);
            break;
        default:
            break; /* tipos sem memória própria */
        }
    }
    free(attributes);
}
