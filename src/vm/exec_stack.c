/**
 * @file exec_stack.c
 * @brief Instruções de manipulação da pilha: `pop`, `pop2`, `dup`,
 *        `dup_x1`, `dup_x2`, `dup2`, `dup2_x1`, `dup2_x2`, `swap` (I-03).
 *
 * @details Todas operam sobre slots brutos (com as etiquetas), sem olhar o
 * tipo: as formas de categoria 1 e de categoria 2 da especificação (ex.:
 * `dup2` de dois `int` ou de um `long`) são a mesma operação sobre slots
 * de 32 bits (D6).
 */
#include "vm/frame.h"
#include "vm/interpreter.h"

/** @brief Reorganização feita por uma instrução de pilha. */
typedef struct StackShape {
    int count;           /**< Slots desempilhados. */
    const char *pattern; /**< Slots empilhados, de baixo para cima (0 = antigo topo). */
} StackShape;

/** @brief Formas das instruções 0x57 (`pop`) a 0x5F (`swap`), JVMS §6.5. */
static const StackShape SHAPES[] = {
    {1, ""},       /* pop:     v1 -> */
    {2, ""},       /* pop2:    v2 v1 -> */
    {1, "00"},     /* dup:     v1 -> v1 v1 */
    {2, "010"},    /* dup_x1:  v2 v1 -> v1 v2 v1 */
    {3, "0210"},   /* dup_x2:  v3 v2 v1 -> v1 v3 v2 v1 */
    {2, "1010"},   /* dup2:    v2 v1 -> v2 v1 v2 v1 */
    {3, "10210"},  /* dup2_x1: v3 v2 v1 -> v2 v1 v3 v2 v1 */
    {4, "103210"}, /* dup2_x2: v4 v3 v2 v1 -> v2 v1 v4 v3 v2 v1 */
    {2, "01"}      /* swap:    v2 v1 -> v1 v2 */
};

/**
 * @brief Executa `pop` ... `swap` conforme a tabela #SHAPES.
 *
 * @details
 * **Algoritmo:**
 * 1. Desempilha `count` slots (índice 0 = antigo topo) com as etiquetas;
 * 2. Empilha os slots na ordem do padrão, de baixo para cima.
 *
 * @param[in,out] frame Frame.
 */
static void op_stack(Frame *frame)
{
    const StackShape *shape = &SHAPES[frame_u1(frame, 0) - 0x57];
    Slot values[4];
    uint8_t tags[4];
    const char *p;
    int i;
    for (i = 0; i < shape->count; i++) {
        values[i] = frame_pop(frame, &tags[i]);
    }
    for (p = shape->pattern; *p != '\0'; p++) {
        frame_push(frame, values[*p - '0'], (SlotTag)tags[*p - '0']);
    }
}

void exec_stack_register(OpcodeHandler *table)
{
    int op;
    for (op = 0x57; op <= 0x5F; op++) {
        table[op] = op_stack;
    }
}
