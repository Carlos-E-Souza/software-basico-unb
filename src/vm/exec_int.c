/**
 * @file exec_int.c
 * @brief Aritmética e lógica de `int` e `long` (I-04).
 *
 * @details Toda conta é feita em `uint32_t`/`uint64_t`: o *wrap-around* do
 * Java (complemento de dois) sai naturalmente e não há comportamento
 * indefinido de *overflow* com sinal. Casos especiais tratados à mão:
 * divisão por zero (`ArithmeticException`), `MIN / -1` e `MIN % -1`,
 * máscara do deslocamento (`0x1F`/`0x3F`) e deslocamento aritmético de
 * negativos (que em C depende da implementação).
 */
#include "vm/frame.h"
#include "vm/interpreter.h"

/** @brief Menor `int` em bits sem sinal. */
#define INT_MIN_BITS 0x80000000u
/** @brief Menor `long` em bits sem sinal. */
#define LONG_MIN_BITS 0x8000000000000000ull

/**
 * @brief Lança `ArithmeticException: / by zero`.
 * @details **Algoritmo:** vm_throw_new() com a mensagem da JVM real.
 * @param[in,out] frame Frame.
 */
static void throw_div_by_zero(Frame *frame)
{
    vm_throw_new(frame->vm, "java/lang/ArithmeticException", "/ by zero");
}

/**
 * @brief Deslocamento aritmético à direita de 32 bits (preenche com o sinal).
 * @details **Algoritmo:** para negativos, `~(~a >> n)`; senão `a >> n`.
 * @param[in] a Bits do valor.
 * @param[in] n Quantidade (0 a 31).
 * @return Bits do resultado.
 */
static uint32_t shr32(uint32_t a, unsigned n)
{
    return (a & INT_MIN_BITS) ? ~(~a >> n) : a >> n;
}

/**
 * @brief Deslocamento aritmético à direita de 64 bits (preenche com o sinal).
 * @details **Algoritmo:** para negativos, `~(~a >> n)`; senão `a >> n`.
 * @param[in] a Bits do valor.
 * @param[in] n Quantidade (0 a 63).
 * @return Bits do resultado.
 */
static uint64_t shr64(uint64_t a, unsigned n)
{
    return (a & LONG_MIN_BITS) ? ~(~a >> n) : a >> n;
}

/**
 * @brief `iadd`, `isub`, `imul`, `idiv`, `irem`, `ineg`, `ishl`, `ishr`,
 *        `iushr`, `iand`, `ior`, `ixor`.
 *
 * @details
 * **Algoritmo:** `ineg` desempilha um valor e empilha `0 - a`; as demais
 * desempilham `b` (topo) e `a` e empilham `a op b` calculado em `uint32_t`.
 * `idiv`/`irem` com `b = 0` lançam `ArithmeticException`; `MIN / -1 = MIN`
 * e `MIN % -1 = 0`; deslocamentos usam `b & 0x1F`.
 *
 * @param[in,out] frame Frame.
 */
static void op_int_arith(Frame *frame)
{
    uint8_t opcode = frame_u1(frame, 0);
    uint32_t a, b, r;
    if (opcode == 0x74) {
        frame_push(frame, 0u - frame_pop(frame, NULL), TAG_VALUE);
        return;
    }
    b = frame_pop(frame, NULL);
    a = frame_pop(frame, NULL);
    if ((opcode == 0x6C || opcode == 0x70) && b == 0) {
        throw_div_by_zero(frame);
        return;
    }
    switch (opcode) {
    case 0x60: r = a + b; break;
    case 0x64: r = a - b; break;
    case 0x68: r = a * b; break;
    case 0x6C:
        r = (a == INT_MIN_BITS && b == 0xFFFFFFFFu)
                ? a : (uint32_t)(slot_to_int(a) / slot_to_int(b));
        break;
    case 0x70:
        r = (b == 0xFFFFFFFFu) ? 0u : (uint32_t)(slot_to_int(a) % slot_to_int(b));
        break;
    case 0x78: r = a << (b & 0x1Fu); break;
    case 0x7A: r = shr32(a, b & 0x1Fu); break;
    case 0x7C: r = a >> (b & 0x1Fu); break;
    case 0x7E: r = a & b; break;
    case 0x80: r = a | b; break;
    default:   r = a ^ b; break;
    }
    frame_push(frame, r, TAG_VALUE);
}

/**
 * @brief `ladd`, `lsub`, `lmul`, `ldiv`, `lrem`, `lneg`, `lshl`, `lshr`,
 *        `lushr`, `land`, `lor`, `lxor`.
 *
 * @details
 * **Algoritmo:** igual a op_int_arith() em `uint64_t`; nos deslocamentos a
 * quantidade é um `int` (1 slot) mascarado com `0x3F`.
 *
 * @param[in,out] frame Frame.
 */
static void op_long_arith(Frame *frame)
{
    uint8_t opcode = frame_u1(frame, 0);
    uint64_t a, b, r;
    if (opcode == 0x75) {
        r = 0u - (uint64_t)frame_pop_long(frame);
        frame_push_long(frame, slots_to_long((Slot)(r >> 32), (Slot)r));
        return;
    }
    if (opcode == 0x79 || opcode == 0x7B || opcode == 0x7D) {
        unsigned n = frame_pop(frame, NULL) & 0x3Fu;
        a = (uint64_t)frame_pop_long(frame);
        r = opcode == 0x79 ? a << n : opcode == 0x7B ? shr64(a, n) : a >> n;
        frame_push_long(frame, slots_to_long((Slot)(r >> 32), (Slot)r));
        return;
    }
    b = (uint64_t)frame_pop_long(frame);
    a = (uint64_t)frame_pop_long(frame);
    if ((opcode == 0x6D || opcode == 0x71) && b == 0) {
        throw_div_by_zero(frame);
        return;
    }
    switch (opcode) {
    case 0x61: r = a + b; break;
    case 0x65: r = a - b; break;
    case 0x69: r = a * b; break;
    case 0x6D:
        r = (a == LONG_MIN_BITS && b == ~0ull)
                ? a : (uint64_t)(slots_to_long((Slot)(a >> 32), (Slot)a) /
                                 slots_to_long((Slot)(b >> 32), (Slot)b));
        break;
    case 0x71:
        r = (b == ~0ull) ? 0u : (uint64_t)(slots_to_long((Slot)(a >> 32), (Slot)a) %
                                            slots_to_long((Slot)(b >> 32), (Slot)b));
        break;
    case 0x7F: r = a & b; break;
    case 0x81: r = a | b; break;
    default:   r = a ^ b; break;
    }
    frame_push_long(frame, slots_to_long((Slot)(r >> 32), (Slot)r));
}

void exec_int_register(OpcodeHandler *table)
{
    int op;
    for (op = 0x60; op <= 0x83; op++) {
        if (op <= 0x77 && (op & 3) >= 2) {
            continue; /* fadd/dadd ... fneg/dneg ficam em exec_float.c */
        }
        table[op] = (op & 1) ? op_long_arith : op_int_arith;
    }
}
