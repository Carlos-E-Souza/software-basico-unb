/**
 * @file test_byte_reader.c
 * @brief Testes do leitor de bytes big-endian, de MUTF-8 e de leitura de arquivos.
 */
#include <stdlib.h>

#include "classfile/byte_reader.h"
#include "common/file_io.h"
#include "common/mutf8.h"
#include "test.h"

/** @brief u1/u2/u4 devem ser lidos em big-endian, avançando a posição. */
static void test_reads_big_endian(void)
{
    static const uint8_t data[] = {0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x34, 0x7F};
    ByteReader r;
    uint32_t u4 = 0;
    uint16_t u2 = 0;
    uint8_t u1 = 0;
    br_init(&r, data, sizeof data);
    ASSERT_TRUE(br_read_u4(&r, &u4));
    ASSERT_EQ_INT(0xCAFEBABEu, u4);
    ASSERT_TRUE(br_read_u2(&r, &u2));
    ASSERT_EQ_INT(52, u2);
    ASSERT_TRUE(br_read_u1(&r, &u1));
    ASSERT_EQ_INT(0x7F, u1);
    ASSERT_EQ_INT(0, br_remaining(&r));
}

/** @brief Leitura além do fim falha sem alterar a saída nem a posição. */
static void test_out_of_bounds_does_not_advance(void)
{
    static const uint8_t data[] = {0x01, 0x02, 0x03};
    ByteReader r;
    uint32_t u4 = 77;
    uint16_t u2 = 0;
    const uint8_t *bytes = NULL;
    br_init(&r, data, sizeof data);
    ASSERT_TRUE(!br_read_u4(&r, &u4));
    ASSERT_EQ_INT(77, u4);
    ASSERT_EQ_INT(0, r.pos);
    ASSERT_TRUE(br_read_u2(&r, &u2));
    ASSERT_TRUE(!br_read_u2(&r, &u2));
    ASSERT_TRUE(!br_read_bytes(&r, 2, &bytes));
    ASSERT_TRUE(br_read_bytes(&r, 1, &bytes));
    ASSERT_EQ_INT(0x03, bytes[0]);
}

/** @brief Sub-leitor fica limitado ao tamanho pedido e guarda o deslocamento absoluto. */
static void test_sub_reader(void)
{
    static const uint8_t data[] = {0xAA, 0x00, 0x01, 0x00, 0x02, 0xBB};
    ByteReader r, sub;
    uint8_t u1;
    uint16_t u2;
    br_init(&r, data, sizeof data);
    ASSERT_TRUE(br_read_u1(&r, &u1));
    ASSERT_TRUE(br_sub_reader(&r, 4, &sub));
    ASSERT_EQ_INT(1, br_offset(&sub));
    ASSERT_TRUE(br_read_u2(&sub, &u2));
    ASSERT_EQ_INT(1, u2);
    ASSERT_TRUE(br_read_u2(&sub, &u2));
    ASSERT_EQ_INT(2, u2);
    ASSERT_TRUE(!br_read_u1(&sub, &u1));
    ASSERT_TRUE(br_read_u1(&r, &u1));
    ASSERT_EQ_INT(0xBB, u1);
    ASSERT_TRUE(!br_sub_reader(&r, 1, &sub));
}

/** @brief MUTF-8 válido: ASCII, NUL em 2 bytes, 2 e 3 bytes e par de surrogates. */
static void test_mutf8_valid(void)
{
    static const uint8_t text[] = {
        'A', 0xC0, 0x80,                   /* 'A', U+0000 */
        0xC3, 0xA9,                        /* é */
        0xE4, 0xB8, 0xAD,                  /* 中 */
        0xED, 0xA0, 0xBD, 0xED, 0xB8, 0x80 /* U+1F600 como surrogates */
    };
    uint16_t units[8];
    ASSERT_TRUE(mutf8_is_valid(text, sizeof text));
    ASSERT_EQ_INT(6, mutf8_to_utf16(text, sizeof text, NULL));
    ASSERT_EQ_INT(6, mutf8_to_utf16(text, sizeof text, units));
    ASSERT_EQ_INT('A', units[0]);
    ASSERT_EQ_INT(0x0000, units[1]);
    ASSERT_EQ_INT(0x00E9, units[2]);
    ASSERT_EQ_INT(0x4E2D, units[3]);
    ASSERT_EQ_INT(0xD83D, units[4]);
    ASSERT_EQ_INT(0xDE00, units[5]);
}

/** @brief MUTF-8 inválido: byte zero, sequência de 4 bytes, continuação solta e truncada. */
static void test_mutf8_invalid(void)
{
    static const uint8_t zero[] = {'a', 0x00};
    static const uint8_t four[] = {0xF0, 0x9F, 0x98, 0x80};
    static const uint8_t lone[] = {0x80};
    static const uint8_t cut[] = {0xE4, 0xB8};
    static const uint8_t bad_cont[] = {0xC3, 0x41};
    ASSERT_TRUE(!mutf8_is_valid(zero, sizeof zero));
    ASSERT_TRUE(!mutf8_is_valid(four, sizeof four));
    ASSERT_TRUE(!mutf8_is_valid(lone, sizeof lone));
    ASSERT_TRUE(!mutf8_is_valid(cut, sizeof cut));
    ASSERT_TRUE(!mutf8_is_valid(bad_cont, sizeof bad_cont));
    ASSERT_TRUE(mutf8_is_valid(NULL, 0));
}

/** @brief Ler arquivo inexistente falha; ler um fixture retorna o conteúdo. */
static void test_file_read_all(void)
{
    uint8_t *data = NULL;
    size_t size = 0;
    ASSERT_TRUE(!file_read_all("arquivo/que/nao/existe.class", &data, &size));
    ASSERT_TRUE(data == NULL);
    ASSERT_TRUE(file_read_all(FIXTURES_DIR "/java8/AllConstants.class", &data, &size));
    ASSERT_TRUE(size > 10);
    if (data != NULL) {
        ASSERT_EQ_INT(0xCA, data[0]);
    }
    free(data);
}

/** @brief Registra os testes desta suíte. */
void suite_byte_reader(void)
{
    RUN_TEST(test_reads_big_endian);
    RUN_TEST(test_out_of_bounds_does_not_advance);
    RUN_TEST(test_sub_reader);
    RUN_TEST(test_mutf8_valid);
    RUN_TEST(test_mutf8_invalid);
    RUN_TEST(test_file_read_all);
}
