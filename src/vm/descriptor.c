/**
 * @file descriptor.c
 * @brief Leitura de descritores de campos e métodos (JVMS §4.3), usada para
 *        contar argumentos e tipos de retorno nas invocações (I-05, D6).
 */
#include <string.h>

#include "vm/descriptor.h"

const char *descriptor_skip_type(const char *p)
{
    while (*p == '[') {
        p++;
    }
    switch (*p) {
    case 'B': case 'C': case 'D': case 'F': case 'I':
    case 'J': case 'S': case 'Z':
        return p + 1;
    case 'L':
        p = strchr(p, ';');
        return p == NULL ? NULL : p + 1;
    default:
        return NULL;
    }
}

int descriptor_arg_slots(const char *descriptor)
{
    const char *p = descriptor;
    int slots = 0;
    if (*p != '(') {
        return -1;
    }
    p++;
    while (*p != ')') {
        const char *next = descriptor_skip_type(p);
        if (next == NULL) {
            return -1;
        }
        slots += descriptor_type_slots(p);
        p = next;
    }
    return slots;
}

const char *descriptor_return(const char *descriptor)
{
    const char *p = strchr(descriptor, ')');
    return p == NULL ? "V" : p + 1;
}

int descriptor_type_slots(const char *type)
{
    switch (type[0]) {
    case 'J': case 'D': return 2;
    case 'V':           return 0;
    default:            return 1;
    }
}

bool descriptor_is_ref(const char *type)
{
    return type[0] == 'L' || type[0] == '[';
}
