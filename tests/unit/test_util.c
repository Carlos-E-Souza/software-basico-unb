/**
 * @file test_util.c
 * @brief Implementação dos utilitários de montagem de classes de teste.
 */
#include <string.h>

#include "test_util.h"

void bb_u1(ByteBuf *b, uint8_t v)
{
    if (b->size < sizeof b->data) {
        b->data[b->size++] = v;
    }
}

void bb_u2(ByteBuf *b, uint16_t v)
{
    bb_u1(b, (uint8_t)(v >> 8));
    bb_u1(b, (uint8_t)(v & 0xFF));
}

void bb_u4(ByteBuf *b, uint32_t v)
{
    bb_u2(b, (uint16_t)(v >> 16));
    bb_u2(b, (uint16_t)(v & 0xFFFF));
}

void bb_utf8(ByteBuf *b, const char *text)
{
    size_t i, n = strlen(text);
    bb_u1(b, 1);
    bb_u2(b, (uint16_t)n);
    for (i = 0; i < n; i++) {
        bb_u1(b, (uint8_t)text[i]);
    }
}

void bb_minimal_class_head(ByteBuf *b, uint16_t extra_cp)
{
    b->size = 0;
    bb_u4(b, 0xCAFEBABEu);
    bb_u2(b, 0);   /* minor */
    bb_u2(b, 52);  /* major */
    bb_u2(b, (uint16_t)(6 + extra_cp));
    bb_utf8(b, "Min");               /* #1 */
    bb_u1(b, 7); bb_u2(b, 1);        /* #2 Class Min */
    bb_utf8(b, "java/lang/Object");  /* #3 */
    bb_u1(b, 7); bb_u2(b, 3);        /* #4 Class Object */
    bb_utf8(b, "Code");              /* #5 */
}

void bb_minimal_class_tail(ByteBuf *b)
{
    bb_u2(b, 0x0021); /* ACC_PUBLIC | ACC_SUPER */
    bb_u2(b, 2);      /* this_class */
    bb_u2(b, 4);      /* super_class */
    bb_u2(b, 0);      /* interfaces */
    bb_u2(b, 0);      /* fields */
    bb_u2(b, 0);      /* methods */
    bb_u2(b, 0);      /* attributes */
}

void bb_class_with_code(ByteBuf *b, const uint8_t *code, uint16_t length)
{
    uint16_t i;
    bb_minimal_class_head(b, 2);
    bb_utf8(b, "m");   /* #6 */
    bb_utf8(b, "()V"); /* #7 */
    bb_u2(b, 0x0021);
    bb_u2(b, 2);
    bb_u2(b, 4);
    bb_u2(b, 0);      /* interfaces */
    bb_u2(b, 0);      /* fields */
    bb_u2(b, 1);      /* methods */
    bb_u2(b, 0x0009); /* public static */
    bb_u2(b, 6);
    bb_u2(b, 7);
    bb_u2(b, 1);      /* attributes */
    bb_u2(b, 5);      /* "Code" */
    bb_u4(b, 2 + 2 + 4 + (uint32_t)length + 2 + 2);
    bb_u2(b, 2);      /* max_stack */
    bb_u2(b, 400);    /* max_locals */
    bb_u4(b, length);
    for (i = 0; i < length; i++) {
        bb_u1(b, code[i]);
    }
    bb_u2(b, 0);      /* exception_table_length */
    bb_u2(b, 0);      /* attributes_count */
    bb_u2(b, 0);      /* atributos da classe */
}
