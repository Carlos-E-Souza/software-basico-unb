/**
 * @file opcodes.c
 * @brief Tabela dos 202 opcodes da JVM e cálculo do tamanho das instruções.
 */
#include <stddef.h>

#include "bytecode/opcodes.h"

/** @brief Tabela de opcodes indexada pelo byte do opcode (posições omitidas são inválidas). */
static const OpcodeInfo OPCODES[256] = {
    /* Constantes */
    [0x00] = {"nop", OPK_NONE},
    [0x01] = {"aconst_null", OPK_NONE},
    [0x02] = {"iconst_m1", OPK_NONE},
    [0x03] = {"iconst_0", OPK_NONE},
    [0x04] = {"iconst_1", OPK_NONE},
    [0x05] = {"iconst_2", OPK_NONE},
    [0x06] = {"iconst_3", OPK_NONE},
    [0x07] = {"iconst_4", OPK_NONE},
    [0x08] = {"iconst_5", OPK_NONE},
    [0x09] = {"lconst_0", OPK_NONE},
    [0x0A] = {"lconst_1", OPK_NONE},
    [0x0B] = {"fconst_0", OPK_NONE},
    [0x0C] = {"fconst_1", OPK_NONE},
    [0x0D] = {"fconst_2", OPK_NONE},
    [0x0E] = {"dconst_0", OPK_NONE},
    [0x0F] = {"dconst_1", OPK_NONE},
    [0x10] = {"bipush", OPK_S1},
    [0x11] = {"sipush", OPK_S2},
    [0x12] = {"ldc", OPK_CP_U1},
    [0x13] = {"ldc_w", OPK_CP_U2},
    [0x14] = {"ldc2_w", OPK_CP_U2},
    /* Loads */
    [0x15] = {"iload", OPK_LOCAL},
    [0x16] = {"lload", OPK_LOCAL},
    [0x17] = {"fload", OPK_LOCAL},
    [0x18] = {"dload", OPK_LOCAL},
    [0x19] = {"aload", OPK_LOCAL},
    [0x1A] = {"iload_0", OPK_NONE},
    [0x1B] = {"iload_1", OPK_NONE},
    [0x1C] = {"iload_2", OPK_NONE},
    [0x1D] = {"iload_3", OPK_NONE},
    [0x1E] = {"lload_0", OPK_NONE},
    [0x1F] = {"lload_1", OPK_NONE},
    [0x20] = {"lload_2", OPK_NONE},
    [0x21] = {"lload_3", OPK_NONE},
    [0x22] = {"fload_0", OPK_NONE},
    [0x23] = {"fload_1", OPK_NONE},
    [0x24] = {"fload_2", OPK_NONE},
    [0x25] = {"fload_3", OPK_NONE},
    [0x26] = {"dload_0", OPK_NONE},
    [0x27] = {"dload_1", OPK_NONE},
    [0x28] = {"dload_2", OPK_NONE},
    [0x29] = {"dload_3", OPK_NONE},
    [0x2A] = {"aload_0", OPK_NONE},
    [0x2B] = {"aload_1", OPK_NONE},
    [0x2C] = {"aload_2", OPK_NONE},
    [0x2D] = {"aload_3", OPK_NONE},
    [0x2E] = {"iaload", OPK_NONE},
    [0x2F] = {"laload", OPK_NONE},
    [0x30] = {"faload", OPK_NONE},
    [0x31] = {"daload", OPK_NONE},
    [0x32] = {"aaload", OPK_NONE},
    [0x33] = {"baload", OPK_NONE},
    [0x34] = {"caload", OPK_NONE},
    [0x35] = {"saload", OPK_NONE},
    /* Stores */
    [0x36] = {"istore", OPK_LOCAL},
    [0x37] = {"lstore", OPK_LOCAL},
    [0x38] = {"fstore", OPK_LOCAL},
    [0x39] = {"dstore", OPK_LOCAL},
    [0x3A] = {"astore", OPK_LOCAL},
    [0x3B] = {"istore_0", OPK_NONE},
    [0x3C] = {"istore_1", OPK_NONE},
    [0x3D] = {"istore_2", OPK_NONE},
    [0x3E] = {"istore_3", OPK_NONE},
    [0x3F] = {"lstore_0", OPK_NONE},
    [0x40] = {"lstore_1", OPK_NONE},
    [0x41] = {"lstore_2", OPK_NONE},
    [0x42] = {"lstore_3", OPK_NONE},
    [0x43] = {"fstore_0", OPK_NONE},
    [0x44] = {"fstore_1", OPK_NONE},
    [0x45] = {"fstore_2", OPK_NONE},
    [0x46] = {"fstore_3", OPK_NONE},
    [0x47] = {"dstore_0", OPK_NONE},
    [0x48] = {"dstore_1", OPK_NONE},
    [0x49] = {"dstore_2", OPK_NONE},
    [0x4A] = {"dstore_3", OPK_NONE},
    [0x4B] = {"astore_0", OPK_NONE},
    [0x4C] = {"astore_1", OPK_NONE},
    [0x4D] = {"astore_2", OPK_NONE},
    [0x4E] = {"astore_3", OPK_NONE},
    [0x4F] = {"iastore", OPK_NONE},
    [0x50] = {"lastore", OPK_NONE},
    [0x51] = {"fastore", OPK_NONE},
    [0x52] = {"dastore", OPK_NONE},
    [0x53] = {"aastore", OPK_NONE},
    [0x54] = {"bastore", OPK_NONE},
    [0x55] = {"castore", OPK_NONE},
    [0x56] = {"sastore", OPK_NONE},
    /* Pilha */
    [0x57] = {"pop", OPK_NONE},
    [0x58] = {"pop2", OPK_NONE},
    [0x59] = {"dup", OPK_NONE},
    [0x5A] = {"dup_x1", OPK_NONE},
    [0x5B] = {"dup_x2", OPK_NONE},
    [0x5C] = {"dup2", OPK_NONE},
    [0x5D] = {"dup2_x1", OPK_NONE},
    [0x5E] = {"dup2_x2", OPK_NONE},
    [0x5F] = {"swap", OPK_NONE},
    /* Aritmética e lógica */
    [0x60] = {"iadd", OPK_NONE},
    [0x61] = {"ladd", OPK_NONE},
    [0x62] = {"fadd", OPK_NONE},
    [0x63] = {"dadd", OPK_NONE},
    [0x64] = {"isub", OPK_NONE},
    [0x65] = {"lsub", OPK_NONE},
    [0x66] = {"fsub", OPK_NONE},
    [0x67] = {"dsub", OPK_NONE},
    [0x68] = {"imul", OPK_NONE},
    [0x69] = {"lmul", OPK_NONE},
    [0x6A] = {"fmul", OPK_NONE},
    [0x6B] = {"dmul", OPK_NONE},
    [0x6C] = {"idiv", OPK_NONE},
    [0x6D] = {"ldiv", OPK_NONE},
    [0x6E] = {"fdiv", OPK_NONE},
    [0x6F] = {"ddiv", OPK_NONE},
    [0x70] = {"irem", OPK_NONE},
    [0x71] = {"lrem", OPK_NONE},
    [0x72] = {"frem", OPK_NONE},
    [0x73] = {"drem", OPK_NONE},
    [0x74] = {"ineg", OPK_NONE},
    [0x75] = {"lneg", OPK_NONE},
    [0x76] = {"fneg", OPK_NONE},
    [0x77] = {"dneg", OPK_NONE},
    [0x78] = {"ishl", OPK_NONE},
    [0x79] = {"lshl", OPK_NONE},
    [0x7A] = {"ishr", OPK_NONE},
    [0x7B] = {"lshr", OPK_NONE},
    [0x7C] = {"iushr", OPK_NONE},
    [0x7D] = {"lushr", OPK_NONE},
    [0x7E] = {"iand", OPK_NONE},
    [0x7F] = {"land", OPK_NONE},
    [0x80] = {"ior", OPK_NONE},
    [0x81] = {"lor", OPK_NONE},
    [0x82] = {"ixor", OPK_NONE},
    [0x83] = {"lxor", OPK_NONE},
    [0x84] = {"iinc", OPK_IINC},
    /* Conversões */
    [0x85] = {"i2l", OPK_NONE},
    [0x86] = {"i2f", OPK_NONE},
    [0x87] = {"i2d", OPK_NONE},
    [0x88] = {"l2i", OPK_NONE},
    [0x89] = {"l2f", OPK_NONE},
    [0x8A] = {"l2d", OPK_NONE},
    [0x8B] = {"f2i", OPK_NONE},
    [0x8C] = {"f2l", OPK_NONE},
    [0x8D] = {"f2d", OPK_NONE},
    [0x8E] = {"d2i", OPK_NONE},
    [0x8F] = {"d2l", OPK_NONE},
    [0x90] = {"d2f", OPK_NONE},
    [0x91] = {"i2b", OPK_NONE},
    [0x92] = {"i2c", OPK_NONE},
    [0x93] = {"i2s", OPK_NONE},
    /* Comparações e desvios */
    [0x94] = {"lcmp", OPK_NONE},
    [0x95] = {"fcmpl", OPK_NONE},
    [0x96] = {"fcmpg", OPK_NONE},
    [0x97] = {"dcmpl", OPK_NONE},
    [0x98] = {"dcmpg", OPK_NONE},
    [0x99] = {"ifeq", OPK_BRANCH_S2},
    [0x9A] = {"ifne", OPK_BRANCH_S2},
    [0x9B] = {"iflt", OPK_BRANCH_S2},
    [0x9C] = {"ifge", OPK_BRANCH_S2},
    [0x9D] = {"ifgt", OPK_BRANCH_S2},
    [0x9E] = {"ifle", OPK_BRANCH_S2},
    [0x9F] = {"if_icmpeq", OPK_BRANCH_S2},
    [0xA0] = {"if_icmpne", OPK_BRANCH_S2},
    [0xA1] = {"if_icmplt", OPK_BRANCH_S2},
    [0xA2] = {"if_icmpge", OPK_BRANCH_S2},
    [0xA3] = {"if_icmpgt", OPK_BRANCH_S2},
    [0xA4] = {"if_icmple", OPK_BRANCH_S2},
    [0xA5] = {"if_acmpeq", OPK_BRANCH_S2},
    [0xA6] = {"if_acmpne", OPK_BRANCH_S2},
    [0xA7] = {"goto", OPK_BRANCH_S2},
    [0xA8] = {"jsr", OPK_BRANCH_S2},
    [0xA9] = {"ret", OPK_LOCAL},
    [0xAA] = {"tableswitch", OPK_TABLESWITCH},
    [0xAB] = {"lookupswitch", OPK_LOOKUPSWITCH},
    /* Retornos */
    [0xAC] = {"ireturn", OPK_NONE},
    [0xAD] = {"lreturn", OPK_NONE},
    [0xAE] = {"freturn", OPK_NONE},
    [0xAF] = {"dreturn", OPK_NONE},
    [0xB0] = {"areturn", OPK_NONE},
    [0xB1] = {"return", OPK_NONE},
    /* Campos, métodos e objetos */
    [0xB2] = {"getstatic", OPK_CP_U2},
    [0xB3] = {"putstatic", OPK_CP_U2},
    [0xB4] = {"getfield", OPK_CP_U2},
    [0xB5] = {"putfield", OPK_CP_U2},
    [0xB6] = {"invokevirtual", OPK_CP_U2},
    [0xB7] = {"invokespecial", OPK_CP_U2},
    [0xB8] = {"invokestatic", OPK_CP_U2},
    [0xB9] = {"invokeinterface", OPK_INVOKEINTERFACE},
    [0xBA] = {"invokedynamic", OPK_INVOKEDYNAMIC},
    [0xBB] = {"new", OPK_CP_U2},
    [0xBC] = {"newarray", OPK_NEWARRAY},
    [0xBD] = {"anewarray", OPK_CP_U2},
    [0xBE] = {"arraylength", OPK_NONE},
    [0xBF] = {"athrow", OPK_NONE},
    [0xC0] = {"checkcast", OPK_CP_U2},
    [0xC1] = {"instanceof", OPK_CP_U2},
    [0xC2] = {"monitorenter", OPK_NONE},
    [0xC3] = {"monitorexit", OPK_NONE},
    /* Estendidas */
    [0xC4] = {"wide", OPK_WIDE},
    [0xC5] = {"multianewarray", OPK_MULTIANEWARRAY},
    [0xC6] = {"ifnull", OPK_BRANCH_S2},
    [0xC7] = {"ifnonnull", OPK_BRANCH_S2},
    [0xC8] = {"goto_w", OPK_BRANCH_S4},
    [0xC9] = {"jsr_w", OPK_BRANCH_S4},
};

/** @brief Nomes dos tipos de `newarray`, indexados por `atype - 4`. */
static const char *const NEWARRAY_TYPES[] = {
    "boolean", "char", "float", "double", "byte", "short", "int", "long"
};

const OpcodeInfo *opcode_info(uint8_t opcode)
{
    return &OPCODES[opcode];
}

const char *opcode_newarray_type_name(uint8_t atype)
{
    if (atype < 4 || atype > 11) {
        return NULL;
    }
    return NEWARRAY_TYPES[atype - 4];
}

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
    switch (OPCODES[code[pc]].kind) {
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
        } else if (OPCODES[next].kind == OPK_LOCAL) {
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
