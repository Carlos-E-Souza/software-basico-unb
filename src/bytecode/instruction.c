/**
 * @file instruction.c
 * @brief Decodificação de instruções: leitura de operandos com sinal e
 *        cálculo do tamanho de cada instrução no bytecode.
 *
 * @details Usa a tabela de opcodes.c (opcode_info()) para obter o formato dos
 * operandos; `tableswitch`, `lookupswitch` e `wide` têm tamanho variável e
 * são tratados à parte.
 */
#include <stdbool.h>

#include "bytecode/opcodes.h"

int16_t bytecode_read_s2(const uint8_t *p)
{
    uint16_t u = (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
    return (u <= 0x7FFFu) ? (int16_t)u : (int16_t)(-(int32_t)(0x10000u - u));
}

int32_t bytecode_read_s4(const uint8_t *p)
{
    uint32_t u = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                 ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    return (u <= 0x7FFFFFFFu) ? (int32_t)u : (int32_t)(-(int64_t)(0x100000000ull - u));
}

/**
 * @brief Calcula o tamanho de `tableswitch` ou `lookupswitch`.
 *
 * @details
 * **Algoritmo:**
 * 1. Calcula o *padding* de 0 a 3 bytes após o opcode, para que os operandos
 *    comecem em posição múltipla de 4;
 * 2. `tableswitch`: lê `low` e `high` (exige `low <= high`); tamanho =
 *    1 + padding + 12 + 4·(high − low + 1);
 * 3. `lookupswitch`: lê `npairs` (exige `npairs >= 0`); tamanho =
 *    1 + padding + 8 + 8·npairs;
 * 4. Retorna 0 se os campos fixos ou a tabela ultrapassarem o código.
 *
 * @param[in] code        Bytecode.
 * @param[in] code_length Tamanho do bytecode.
 * @param[in] pc          Posição do opcode.
 * @param[in] table       `true` para tableswitch, `false` para lookupswitch.
 * @return Tamanho da instrução ou 0 se inválida.
 */
static uint32_t switch_length(const uint8_t *code, uint32_t code_length,
                              uint32_t pc, bool table)
{
    uint32_t base = pc + 1 + ((4u - ((pc + 1) % 4u)) % 4u);
    uint64_t total;
    if ((uint64_t)base + 12u > code_length) {
        return 0;
    }
    if (table) {
        int32_t low = bytecode_read_s4(code + base + 4);
        int32_t high = bytecode_read_s4(code + base + 8);
        if (low > high) {
            return 0;
        }
        total = (uint64_t)(base - pc) + 12u + 4u * ((uint64_t)((int64_t)high - low) + 1u);
    } else {
        int32_t npairs = bytecode_read_s4(code + base + 4);
        if (npairs < 0) {
            return 0;
        }
        total = (uint64_t)(base - pc) + 8u + 8u * (uint64_t)npairs;
    }
    if ((uint64_t)pc + total > code_length) {
        return 0;
    }
    return (uint32_t)total;
}

uint32_t bytecode_instruction_length(const uint8_t *code, uint32_t code_length,
                                     uint32_t pc)
{
    uint32_t length;
    uint8_t next;
    if (pc >= code_length) {
        return 0;
    }
    switch (opcode_info(code[pc])->kind) {
    case OPK_NONE:            length = 1; break;
    case OPK_S1:
    case OPK_LOCAL:
    case OPK_CP_U1:
    case OPK_NEWARRAY:        length = 2; break;
    case OPK_S2:
    case OPK_CP_U2:
    case OPK_IINC:
    case OPK_BRANCH_S2:       length = 3; break;
    case OPK_MULTIANEWARRAY:  length = 4; break;
    case OPK_INVOKEINTERFACE:
    case OPK_INVOKEDYNAMIC:
    case OPK_BRANCH_S4:       length = 5; break;
    case OPK_TABLESWITCH:
        return switch_length(code, code_length, pc, true);
    case OPK_LOOKUPSWITCH:
        return switch_length(code, code_length, pc, false);
    case OPK_WIDE:
        if (pc + 1 >= code_length) {
            return 0;
        }
        next = code[pc + 1];
        if (next == OPCODE_IINC) {
            length = 6;
        } else if (opcode_info(next)->kind == OPK_LOCAL) {
            length = 4;
        } else {
            return 0;
        }
        break;
    case OPK_INVALID:
    default:
        return 0;
    }
    return ((uint64_t)pc + length <= code_length) ? length : 0;
}
