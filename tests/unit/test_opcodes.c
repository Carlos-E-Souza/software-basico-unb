/**
 * @file test_opcodes.c
 * @brief Testes da tabela de opcodes e do cálculo de tamanho das instruções.
 */
#include <string.h>

#include "bytecode/opcodes.h"
#include "test.h"

/** @brief Exatamente os 202 opcodes 0x00–0xC9 são válidos; 0xCA–0xFF são inválidos. */
static void test_opcode_table_coverage(void)
{
    int op, valid = 0, invalid_above = 0;
    for (op = 0; op < 256; op++) {
        const OpcodeInfo *info = opcode_info((uint8_t)op);
        if (info->kind != OPK_INVALID) {
            valid++;
            ASSERT_TRUE(info->mnemonic != NULL);
        }
        if (op > 0xC9 && info->kind != OPK_INVALID) {
            invalid_above++;
        }
    }
    ASSERT_EQ_INT(202, valid);
    ASSERT_EQ_INT(0, invalid_above);
}

/** @brief Mnemônicos e formatos de alguns opcodes representativos. */
static void test_opcode_samples(void)
{
    ASSERT_EQ_STR("nop", opcode_info(0x00)->mnemonic);
    ASSERT_EQ_STR("ldc2_w", opcode_info(0x14)->mnemonic);
    ASSERT_EQ_STR("dup2_x2", opcode_info(0x5E)->mnemonic);
    ASSERT_EQ_STR("iinc", opcode_info(0x84)->mnemonic);
    ASSERT_EQ_STR("invokedynamic", opcode_info(0xBA)->mnemonic);
    ASSERT_EQ_STR("jsr_w", opcode_info(0xC9)->mnemonic);
    ASSERT_EQ_INT(OPK_LOCAL, opcode_info(0xA9)->kind); /* ret */
    ASSERT_EQ_INT(OPK_CP_U1, opcode_info(0x12)->kind); /* ldc */
    ASSERT_EQ_STR("int", opcode_newarray_type_name(10));
    ASSERT_EQ_STR("boolean", opcode_newarray_type_name(4));
    ASSERT_TRUE(opcode_newarray_type_name(3) == NULL);
    ASSERT_TRUE(opcode_newarray_type_name(12) == NULL);
}

/** @brief Leitura de inteiros com sinal em big-endian. */
static void test_signed_reads(void)
{
    static const uint8_t neg2[] = {0xFF, 0xFE};
    static const uint8_t min4[] = {0x80, 0x00, 0x00, 0x00};
    static const uint8_t pos4[] = {0x00, 0x01, 0x00, 0x00};
    ASSERT_EQ_INT(-2, bytecode_read_s2(neg2));
    ASSERT_EQ_INT(-2147483647LL - 1, bytecode_read_s4(min4));
    ASSERT_EQ_INT(65536, bytecode_read_s4(pos4));
}

/** @brief Tamanhos de instruções de formato fixo e truncamento. */
static void test_fixed_lengths(void)
{
    static const uint8_t code[] = {
        0x00,             /* nop */
        0x10, 0x05,       /* bipush 5 */
        0x11, 0x01, 0x00, /* sipush 256 */
        0xB9, 0x00, 0x01, 0x02, 0x00, /* invokeinterface */
        0xC5, 0x00, 0x02, 0x03,       /* multianewarray */
        0xC8, 0x00, 0x00, 0x00, 0x00, /* goto_w */
        0xA8, 0x00                    /* jsr truncado */
    };
    ASSERT_EQ_INT(1, bytecode_instruction_length(code, sizeof code, 0));
    ASSERT_EQ_INT(2, bytecode_instruction_length(code, sizeof code, 1));
    ASSERT_EQ_INT(3, bytecode_instruction_length(code, sizeof code, 3));
    ASSERT_EQ_INT(5, bytecode_instruction_length(code, sizeof code, 6));
    ASSERT_EQ_INT(4, bytecode_instruction_length(code, sizeof code, 11));
    ASSERT_EQ_INT(5, bytecode_instruction_length(code, sizeof code, 15));
    ASSERT_EQ_INT(0, bytecode_instruction_length(code, sizeof code, 20));
    ASSERT_EQ_INT(0, bytecode_instruction_length(code, sizeof code, sizeof code));
}

/** @brief `wide`: 4 bytes para load/store/ret, 6 para iinc, inválido para outros. */
static void test_wide_lengths(void)
{
    static const uint8_t iload_w[] = {0xC4, 0x15, 0x01, 0x2C};
    static const uint8_t iinc_w[] = {0xC4, 0x84, 0x01, 0x2C, 0x03, 0xE8};
    static const uint8_t ret_w[] = {0xC4, 0xA9, 0x00, 0x01};
    static const uint8_t bad[] = {0xC4, 0x00, 0x00, 0x00};
    ASSERT_EQ_INT(4, bytecode_instruction_length(iload_w, sizeof iload_w, 0));
    ASSERT_EQ_INT(6, bytecode_instruction_length(iinc_w, sizeof iinc_w, 0));
    ASSERT_EQ_INT(4, bytecode_instruction_length(ret_w, sizeof ret_w, 0));
    ASSERT_EQ_INT(0, bytecode_instruction_length(bad, sizeof bad, 0));
    ASSERT_EQ_INT(0, bytecode_instruction_length(iinc_w, 5, 0));
}

/** @brief tableswitch/lookupswitch com alinhamento em cada posição possível. */
static void test_switch_lengths(void)
{
    uint8_t code[64];
    uint32_t pc;
    for (pc = 0; pc < 4; pc++) {
        uint32_t pad = (4 - ((pc + 1) % 4)) % 4;
        uint8_t *p;
        memset(code, 0, sizeof code);
        /* tableswitch low=1 high=3 */
        code[pc] = 0xAA;
        p = code + pc + 1 + pad;
        p[7] = 1;  /* low */
        p[11] = 3; /* high */
        ASSERT_EQ_INT(1 + pad + 12 + 3 * 4,
                      bytecode_instruction_length(code, sizeof code, pc));
        ASSERT_EQ_INT(0, bytecode_instruction_length(code, pc + 1 + pad + 12, pc));
        p[7] = 4; /* low > high: inválido */
        ASSERT_EQ_INT(0, bytecode_instruction_length(code, sizeof code, pc));

        /* lookupswitch npairs=2 */
        memset(code, 0, sizeof code);
        code[pc] = 0xAB;
        p = code + pc + 1 + pad;
        p[7] = 2;
        ASSERT_EQ_INT(1 + pad + 8 + 2 * 8,
                      bytecode_instruction_length(code, sizeof code, pc));
        p[4] = 0x80; /* npairs negativo */
        ASSERT_EQ_INT(0, bytecode_instruction_length(code, sizeof code, pc));
    }
}

/** @brief Opcodes reservados/indefinidos têm tamanho 0. */
static void test_invalid_opcodes(void)
{
    static const uint8_t code[] = {0xCA, 0xFE, 0xFF, 0xE0};
    uint32_t i;
    for (i = 0; i < sizeof code; i++) {
        ASSERT_EQ_INT(0, bytecode_instruction_length(code, sizeof code, i));
    }
}

/** @brief Registra os testes desta suíte. */
void suite_opcodes(void)
{
    RUN_TEST(test_opcode_table_coverage);
    RUN_TEST(test_opcode_samples);
    RUN_TEST(test_signed_reads);
    RUN_TEST(test_fixed_lengths);
    RUN_TEST(test_wide_lengths);
    RUN_TEST(test_switch_lengths);
    RUN_TEST(test_invalid_opcodes);
}
