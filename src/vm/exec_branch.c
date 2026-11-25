/**
 * @file exec_branch.c
 * @brief Desvios condicionais e incondicionais, `tableswitch`,
 *        `lookupswitch`, `jsr`/`ret`/`jsr_w` (D10) e `*return` (I-05).
 *
 * @details Um desvio só altera `frame->next_pc`; o destino é relativo ao
 * início da instrução (`frame->pc`). Um destino fora do código é detectado
 * pelo laço na instrução seguinte (erro fatal).
 */
#include "bytecode/opcodes.h"
#include "vm/frame.h"
#include "vm/interpreter.h"

/**
 * @brief Avalia uma das 6 condições de desvio (`eq`, `ne`, `lt`, `ge`, `gt`, `le`).
 * @details **Algoritmo:** compara @p a com @p b conforme o índice (ordem dos opcodes).
 * @param[in] condition Índice da condição (0 a 5).
 * @param[in] a         Primeiro operando.
 * @param[in] b         Segundo operando.
 * @return Resultado da comparação.
 */
static bool test_condition(int condition, int32_t a, int32_t b)
{
    switch (condition) {
    case 0:  return a == b;
    case 1:  return a != b;
    case 2:  return a < b;
    case 3:  return a >= b;
    case 4:  return a > b;
    default: return a <= b;
    }
}

/**
 * @brief Desvia para `pc + deslocamento s2` (operando em `pc + 1`).
 * @details **Algoritmo:** lê o deslocamento com sinal e define `next_pc`.
 * @param[in,out] frame Frame.
 */
static void branch_s2(Frame *frame)
{
    frame->next_pc = frame->pc + (uint32_t)(int32_t)(int16_t)frame_u2(frame, 1);
}

/**
 * @brief `ifeq` ... `ifle`: compara o `int` do topo com zero.
 * @details **Algoritmo:** condição = `opcode - 0x99`; se verdadeira, desvia.
 * @param[in,out] frame Frame.
 */
static void op_if(Frame *frame)
{
    if (test_condition(frame_u1(frame, 0) - 0x99, frame_pop_int(frame), 0)) {
        branch_s2(frame);
    }
}

/**
 * @brief `if_icmpeq` ... `if_icmple`: compara dois `int`.
 * @details **Algoritmo:** desempilha `b` e `a`; condição = `opcode - 0x9F`.
 * @param[in,out] frame Frame.
 */
static void op_if_icmp(Frame *frame)
{
    int32_t b = frame_pop_int(frame);
    int32_t a = frame_pop_int(frame);
    if (test_condition(frame_u1(frame, 0) - 0x9F, a, b)) {
        branch_s2(frame);
    }
}

/**
 * @brief `if_acmpeq`/`if_acmpne`: compara duas referências.
 * @details **Algoritmo:** desempilha duas referências e compara os ponteiros.
 * @param[in,out] frame Frame.
 */
static void op_if_acmp(Frame *frame)
{
    const Object *b = frame_pop_ref(frame);
    const Object *a = frame_pop_ref(frame);
    if ((a == b) == (frame_u1(frame, 0) == 0xA5)) {
        branch_s2(frame);
    }
}

/**
 * @brief `ifnull`/`ifnonnull`: testa se a referência do topo é nula.
 * @details **Algoritmo:** desempilha a referência e desvia conforme o opcode.
 * @param[in,out] frame Frame.
 */
static void op_ifnull(Frame *frame)
{
    const Object *ref = frame_pop_ref(frame);
    if ((ref == NULL) == (frame_u1(frame, 0) == 0xC6)) {
        branch_s2(frame);
    }
}

/**
 * @brief `goto` (s2) e `goto_w` (s4).
 * @details **Algoritmo:** define `next_pc` com o deslocamento de 16 ou 32 bits.
 * @param[in,out] frame Frame.
 */
static void op_goto(Frame *frame)
{
    if (frame_u1(frame, 0) == 0xA7) {
        branch_s2(frame);
    } else {
        frame->next_pc = frame->pc + (uint32_t)bytecode_read_s4(frame->code + frame->pc + 1);
    }
}

/**
 * @brief `jsr` (s2) e `jsr_w` (s4): sub-rotina (D10).
 * @details **Algoritmo:** empilha o endereço da instrução seguinte como
 * `returnAddress` (#TAG_RETADDR) e desvia.
 * @param[in,out] frame Frame.
 */
static void op_jsr(Frame *frame)
{
    frame_push(frame, frame->next_pc, TAG_RETADDR);
    if (frame_u1(frame, 0) == 0xA8) {
        branch_s2(frame);
    } else {
        frame->next_pc = frame->pc + (uint32_t)bytecode_read_s4(frame->code + frame->pc + 1);
    }
}

/**
 * @brief `ret`: volta ao `returnAddress` guardado em uma local (D10).
 * @details **Algoritmo:** exige etiqueta #TAG_RETADDR na local e desvia.
 * @param[in,out] frame Frame.
 */
static void op_ret(Frame *frame)
{
    uint8_t tag;
    Slot target = frame_load(frame, frame_u1(frame, 1), &tag);
    if (tag != TAG_RETADDR) {
        vm_fatal(frame->vm, "ret sem returnAddress na local %u", (unsigned)frame_u1(frame, 1));
        return;
    }
    frame->next_pc = target;
}

/**
 * @brief `tableswitch` e `lookupswitch`.
 *
 * @details
 * **Algoritmo:** os operandos começam no primeiro múltiplo de 4 depois do
 * opcode. `tableswitch`: se `low <= chave <= high`, usa o deslocamento
 * `chave - low` da tabela; `lookupswitch`: busca linear pelo par com a
 * chave. Sem correspondência, usa o deslocamento `default`. (O tamanho da
 * instrução já foi validado pelo laço.)
 *
 * @param[in,out] frame Frame.
 */
static void op_switch(Frame *frame)
{
    const uint8_t *base = frame->code + ((frame->pc + 4u) & ~3u);
    int32_t key = frame_pop_int(frame);
    int32_t offset = bytecode_read_s4(base);
    if (frame_u1(frame, 0) == 0xAA) {
        int32_t low = bytecode_read_s4(base + 4);
        int32_t high = bytecode_read_s4(base + 8);
        if (key >= low && key <= high) {
            offset = bytecode_read_s4(base + 12 + 4 * ((int64_t)key - low));
        }
    } else {
        int32_t npairs = bytecode_read_s4(base + 4);
        int32_t i;
        for (i = 0; i < npairs; i++) {
            if (bytecode_read_s4(base + 8 + 8 * i) == key) {
                offset = bytecode_read_s4(base + 12 + 8 * i);
                break;
            }
        }
    }
    frame->next_pc = frame->pc + (uint32_t)offset;
}

/**
 * @brief `ireturn`, `lreturn`, `freturn`, `dreturn`, `areturn`, `return`.
 *
 * @details **Algoritmo:** desempilha o valor de retorno (2 slots em
 * `lreturn`/`dreturn`; referência verificada em `areturn`), guarda-o em
 * `frame->result` e marca o frame como encerrado.
 *
 * @param[in,out] frame Frame.
 */
static void op_return(Frame *frame)
{
    uint8_t opcode = frame_u1(frame, 0);
    frame->result_tag = TAG_VALUE;
    if (opcode == 0xAD || opcode == 0xAF) {
        frame->result[1] = frame_pop(frame, NULL);
        frame->result[0] = frame_pop(frame, NULL);
    } else if (opcode == 0xB0) {
        frame->result[0] = (Slot)(uintptr_t)frame_pop_ref(frame);
        frame->result_tag = TAG_REF;
    } else if (opcode != 0xB1) {
        frame->result[0] = frame_pop(frame, NULL);
    }
    frame->returned = true;
}

void exec_branch_register(OpcodeHandler *table)
{
    int op;
    for (op = 0x99; op <= 0x9E; op++) table[op] = op_if;
    for (op = 0x9F; op <= 0xA4; op++) table[op] = op_if_icmp;
    table[0xA5] = table[0xA6] = op_if_acmp;
    table[0xA7] = table[0xC8] = op_goto;
    table[0xA8] = table[0xC9] = op_jsr;
    table[0xA9] = op_ret;
    table[0xAA] = table[0xAB] = op_switch;
    for (op = 0xAC; op <= 0xB1; op++) table[op] = op_return;
    table[0xC6] = table[0xC7] = op_ifnull;
}
