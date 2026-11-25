/**
 * @file exec_float.c
 * @brief Aritmética de `float`/`double`, conversões entre tipos primitivos
 *        e comparações (`lcmp`, `fcmp*`, `dcmp*`) (I-04).
 *
 * @details Os cálculos usam SSE2 (D20), logo cada operação é arredondada
 * para 32/64 bits como exige o Java. Conversões para inteiro seguem a
 * JVMS §6.5: NaN vira 0 e valores fora da faixa saturam em MIN/MAX.
 */
#include <math.h>

#include "vm/frame.h"
#include "vm/interpreter.h"

/**
 * @brief `fadd`, `fsub`, `fmul`, `fdiv`, `frem`, `fneg`.
 *
 * @details **Algoritmo:** `fneg` troca o sinal do topo; as demais
 * desempilham `b` e `a` e empilham `a op b` em precisão simples; `frem`
 * usa `fmodf` (resto com o sinal do dividendo, como o Java).
 *
 * @param[in,out] frame Frame.
 */
static void op_float_arith(Frame *frame)
{
    uint8_t opcode = frame_u1(frame, 0);
    float a, b, r;
    if (opcode == 0x76) {
        frame_push_float(frame, -frame_pop_float(frame));
        return;
    }
    b = frame_pop_float(frame);
    a = frame_pop_float(frame);
    switch (opcode) {
    case 0x62: r = a + b; break;
    case 0x66: r = a - b; break;
    case 0x6A: r = a * b; break;
    case 0x6E: r = a / b; break;
    default:   r = fmodf(a, b); break;
    }
    frame_push_float(frame, r);
}

/**
 * @brief `dadd`, `dsub`, `dmul`, `ddiv`, `drem`, `dneg`.
 *
 * @details **Algoritmo:** igual a op_float_arith() em precisão dupla
 * (`fmod` no `drem`).
 *
 * @param[in,out] frame Frame.
 */
static void op_double_arith(Frame *frame)
{
    uint8_t opcode = frame_u1(frame, 0);
    double a, b, r;
    if (opcode == 0x77) {
        frame_push_double(frame, -frame_pop_double(frame));
        return;
    }
    b = frame_pop_double(frame);
    a = frame_pop_double(frame);
    switch (opcode) {
    case 0x63: r = a + b; break;
    case 0x67: r = a - b; break;
    case 0x6B: r = a * b; break;
    case 0x6F: r = a / b; break;
    default:   r = fmod(a, b); break;
    }
    frame_push_double(frame, r);
}

/**
 * @brief Converte `double` para `int` com as regras do Java.
 * @details **Algoritmo:** NaN → 0; ≥ 2³¹−1 → MAX; ≤ −2³¹ → MIN; senão trunca.
 * @param[in] value Valor.
 * @return Valor convertido.
 */
static int32_t double_to_int(double value)
{
    if (isnan(value)) {
        return 0;
    }
    if (value >= 2147483647.0) {
        return INT32_MAX;
    }
    if (value <= -2147483648.0) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

/**
 * @brief Converte `double` para `long` com as regras do Java.
 * @details **Algoritmo:** NaN → 0; ≥ 2⁶³ → MAX; ≤ −2⁶³ → MIN; senão trunca.
 * @param[in] value Valor.
 * @return Valor convertido.
 */
static int64_t double_to_long(double value)
{
    if (isnan(value)) {
        return 0;
    }
    if (value >= 9223372036854775808.0) {
        return INT64_MAX;
    }
    if (value <= -9223372036854775808.0) {
        return INT64_MIN;
    }
    return (int64_t)value;
}

/**
 * @brief Conversões `i2l` ... `i2s` (0x85 a 0x93).
 *
 * @details
 * **Algoritmo:** desempilha conforme o tipo de origem e empilha o valor
 * convertido: alargamentos são diretos (`l2f` converte do `long` direto
 * para `float`, sem passar por `double`); `l2i` fica com os 32 bits
 * baixos; `f2i`/`d2i`/`f2l`/`d2l` usam double_to_int()/double_to_long();
 * `i2b`/`i2s` estendem o sinal de 8/16 bits sem UB e `i2c` zera os 16
 * bits altos.
 *
 * @param[in,out] frame Frame.
 */
static void op_convert(Frame *frame)
{
    uint8_t opcode = frame_u1(frame, 0);
    int32_t i;
    int64_t l;
    switch (opcode) {
    case 0x85: frame_push_long(frame, frame_pop_int(frame)); break;
    case 0x86: frame_push_float(frame, (float)frame_pop_int(frame)); break;
    case 0x87: frame_push_double(frame, (double)frame_pop_int(frame)); break;
    case 0x88:
        l = frame_pop_long(frame);
        frame_push(frame, (Slot)(uint64_t)l, TAG_VALUE);
        break;
    case 0x89: frame_push_float(frame, (float)frame_pop_long(frame)); break;
    case 0x8A: frame_push_double(frame, (double)frame_pop_long(frame)); break;
    case 0x8B: frame_push_int(frame, double_to_int(frame_pop_float(frame))); break;
    case 0x8C: frame_push_long(frame, double_to_long(frame_pop_float(frame))); break;
    case 0x8D: frame_push_double(frame, (double)frame_pop_float(frame)); break;
    case 0x8E: frame_push_int(frame, double_to_int(frame_pop_double(frame))); break;
    case 0x8F: frame_push_long(frame, double_to_long(frame_pop_double(frame))); break;
    case 0x90: frame_push_float(frame, (float)frame_pop_double(frame)); break;
    case 0x91:
        i = (int32_t)((frame_pop(frame, NULL) & 0xFFu) ^ 0x80u) - 0x80;
        frame_push_int(frame, i);
        break;
    case 0x92: frame_push(frame, frame_pop(frame, NULL) & 0xFFFFu, TAG_VALUE); break;
    default:
        i = (int32_t)((frame_pop(frame, NULL) & 0xFFFFu) ^ 0x8000u) - 0x8000;
        frame_push_int(frame, i);
        break;
    }
}

/**
 * @brief `lcmp`, `fcmpl`, `fcmpg`, `dcmpl`, `dcmpg`.
 *
 * @details **Algoritmo:** desempilha `b` e `a` e empilha 1 se `a > b`, 0
 * se iguais e -1 se `a < b`; com NaN, `*cmpl` empilha -1 e `*cmpg` empilha 1.
 *
 * @param[in,out] frame Frame.
 */
static void op_compare(Frame *frame)
{
    uint8_t opcode = frame_u1(frame, 0);
    double a, b;
    if (opcode == 0x94) {
        int64_t lb = frame_pop_long(frame);
        int64_t la = frame_pop_long(frame);
        frame_push_int(frame, (la > lb) - (la < lb));
        return;
    }
    if (opcode <= 0x96) {
        b = frame_pop_float(frame);
        a = frame_pop_float(frame);
    } else {
        b = frame_pop_double(frame);
        a = frame_pop_double(frame);
    }
    if (isnan(a) || isnan(b)) {
        frame_push_int(frame, (opcode == 0x95 || opcode == 0x97) ? -1 : 1);
    } else {
        frame_push_int(frame, (a > b) - (a < b));
    }
}

void exec_float_register(OpcodeHandler *table)
{
    int op;
    for (op = 0x62; op <= 0x77; op += 4) {
        table[op] = op_float_arith;
        table[op + 1] = op_double_arith;
    }
    for (op = 0x85; op <= 0x93; op++) {
        table[op] = op_convert;
    }
    for (op = 0x94; op <= 0x98; op++) {
        table[op] = op_compare;
    }
}
