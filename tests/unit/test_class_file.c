/**
 * @file test_class_file.c
 * @brief Testes do leitor de `.class` com classes montadas byte a byte
 *        (classe mínima, versões, truncamento, pool, Code, atributo
 *        desconhecido e arquivo inexistente).
 *
 * @details As classes reais compiladas pelo javac são testadas em
 * test_class_file_fixtures.c.
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/attributes.h"
#include "classfile/class_file.h"
#include "classfile/constant_pool.h"
#include "common/file_io.h"
#include "test.h"
#include "test_util.h"

/* ------------------------------------------------------------ auxiliares -- */

/** @brief Monta uma classe com um método estático `m()V` cujo Code tem @p extra bytes a mais. */
static void build_class_with_code(ByteBuf *b, uint32_t code_length, int extra)
{
    uint32_t i;
    bb_minimal_class_head(b, 2);
    bb_utf8(b, "m");   /* #6 */
    bb_utf8(b, "()V"); /* #7 */
    bb_u2(b, 0x0021);
    bb_u2(b, 2);
    bb_u2(b, 4);
    bb_u2(b, 0); /* interfaces */
    bb_u2(b, 0); /* fields */
    bb_u2(b, 1); /* methods */
    bb_u2(b, 0x0009);
    bb_u2(b, 6);
    bb_u2(b, 7);
    bb_u2(b, 1); /* attributes */
    bb_u2(b, 5); /* "Code" */
    bb_u4(b, 2 + 2 + 4 + code_length + 2 + 2 + (uint32_t)extra);
    bb_u2(b, 0); /* max_stack */
    bb_u2(b, 1); /* max_locals */
    bb_u4(b, code_length);
    for (i = 0; i < code_length; i++) {
        bb_u1(b, 0xB1); /* return */
    }
    bb_u2(b, 0); /* exception_table_length */
    bb_u2(b, 0); /* attributes_count */
    for (i = 0; i < (uint32_t)extra; i++) {
        bb_u1(b, 0);
    }
    bb_u2(b, 0); /* atributos da classe */
}

/* ------------------------------------------------- classes montadas à mão -- */

/** @brief Classe mínima válida é lida e as consultas ao pool funcionam. */
static void test_minimal_class(void)
{
    ByteBuf b;
    ClassFile *cf = NULL;
    ClassFileError err;
    bb_minimal_class_head(&b, 0);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_OK, class_file_parse(b.data, b.size, &cf, &err));
    if (cf == NULL) {
        return;
    }
    ASSERT_EQ_INT(52, cf->major_version);
    ASSERT_EQ_INT(6, cf->constant_pool_count);
    ASSERT_EQ_STR("Min", cp_get_class_name(cf, cf->this_class));
    ASSERT_EQ_STR("java/lang/Object", cp_get_class_name(cf, cf->super_class));
    ASSERT_TRUE(cp_get_class_name(cf, 1) == NULL); /* #1 é Utf8, não Class */
    ASSERT_TRUE(cp_get_utf8(cf, 0) == NULL);
    ASSERT_TRUE(cp_get_utf8(cf, 6) == NULL);
    ASSERT_EQ_INT(0, cf->methods_count);
    class_file_free(cf);
}

/** @brief Número mágico errado e versões fora de 45–52 são rejeitados. */
static void test_magic_and_version(void)
{
    ByteBuf b;
    uint16_t major;
    bb_minimal_class_head(&b, 0);
    bb_minimal_class_tail(&b);
    b.data[0] = 0xCB;
    ASSERT_EQ_INT(CF_ERR_BAD_MAGIC, bb_parse_status(&b));
    b.data[0] = 0xCA;
    for (major = 43; major <= 54; major++) {
        CfStatus expected = (major >= 45 && major <= 52) ? CF_OK
                                                         : CF_ERR_UNSUPPORTED_VERSION;
        b.data[6] = (uint8_t)(major >> 8);
        b.data[7] = (uint8_t)(major & 0xFF);
        ASSERT_EQ_INT(expected, bb_parse_status(&b));
    }
}

/** @brief Todo prefixo de uma classe válida gera erro de truncamento; byte extra gera erro. */
static void test_truncation_and_trailing(void)
{
    ByteBuf b;
    size_t full, n;
    int failures = 0;
    bb_minimal_class_head(&b, 0);
    bb_minimal_class_tail(&b);
    full = b.size;
    for (n = 0; n < full; n++) {
        b.size = n;
        if (bb_parse_status(&b) != CF_ERR_TRUNCATED) {
            failures++;
        }
    }
    ASSERT_EQ_INT(0, failures);
    b.size = full;
    bb_u1(&b, 0x00);
    ASSERT_EQ_INT(CF_ERR_TRAILING_BYTES, bb_parse_status(&b));
}

/** @brief Erros no pool: tag desconhecida, Long na última posição, índice para a 2ª metade de Long, MUTF-8 inválido. */
static void test_constant_pool_errors(void)
{
    ByteBuf b;

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 2); /* tag inexistente */
    bb_u2(&b, 0);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, bb_parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 5); /* Long como última entrada: falta a 2ª posição */
    bb_u4(&b, 0);
    bb_u4(&b, 1);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, bb_parse_status(&b));

    bb_minimal_class_head(&b, 3);
    bb_u1(&b, 5); /* #6-#7 Long */
    bb_u4(&b, 0);
    bb_u4(&b, 1);
    bb_u1(&b, 8); /* #8 String -> #7 (inutilizável) */
    bb_u2(&b, 7);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 1); /* Utf8 com byte zero */
    bb_u2(&b, 2);
    bb_u1(&b, 'a');
    bb_u1(&b, 0x00);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, bb_parse_status(&b));
}

/** @brief this_class apontando para Utf8 é rejeitado. */
static void test_bad_this_class(void)
{
    ByteBuf b;
    bb_minimal_class_head(&b, 0);
    bb_u2(&b, 0x0021);
    bb_u2(&b, 1); /* #1 é Utf8 */
    bb_u2(&b, 4);
    bb_u2(&b, 0);
    bb_u2(&b, 0);
    bb_u2(&b, 0);
    bb_u2(&b, 0);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
}

/** @brief Atributo Code: válido, com bytes sobrando e com code_length 0. */
static void test_code_attribute(void)
{
    ByteBuf b;
    ClassFile *cf = NULL;
    const CodeAttribute *code;

    build_class_with_code(&b, 1, 0);
    ASSERT_EQ_INT(CF_OK, class_file_parse(b.data, b.size, &cf, NULL));
    if (cf != NULL) {
        code = method_code(&cf->methods[0]);
        ASSERT_TRUE(code != NULL);
        if (code != NULL) {
            ASSERT_EQ_INT(1, code->code_length);
            ASSERT_EQ_INT(0xB1, code->code[0]);
            ASSERT_EQ_INT(1, code->max_locals);
        }
        class_file_free(cf);
    }

    build_class_with_code(&b, 1, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, bb_parse_status(&b));

    build_class_with_code(&b, 0, 0);
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, bb_parse_status(&b));
}

/** @brief Atributo desconhecido é preservado como bytes brutos. */
static void test_unknown_attribute(void)
{
    ByteBuf b;
    ClassFile *cf = NULL;
    bb_minimal_class_head(&b, 1);
    bb_utf8(&b, "Custom"); /* #6 */
    bb_u2(&b, 0x0021);
    bb_u2(&b, 2);
    bb_u2(&b, 4);
    bb_u2(&b, 0);
    bb_u2(&b, 0);
    bb_u2(&b, 0);
    bb_u2(&b, 1); /* 1 atributo */
    bb_u2(&b, 6);
    bb_u4(&b, 3);
    bb_u1(&b, 0x0A);
    bb_u1(&b, 0x0B);
    bb_u1(&b, 0x0C);
    ASSERT_EQ_INT(CF_OK, class_file_parse(b.data, b.size, &cf, NULL));
    if (cf != NULL) {
        ASSERT_EQ_INT(1, cf->attributes_count);
        ASSERT_EQ_INT(ATTR_UNKNOWN, cf->attributes[0].kind);
        ASSERT_EQ_INT(0x0C, cf->attributes[0].info.raw[2]);
        class_file_free(cf);
    }
}

/** @brief Arquivo inexistente gera CF_ERR_IO com mensagem. */
static void test_missing_file(void)
{
    ClassFile *cf = (ClassFile *)&cf;
    ClassFileError err;
    ASSERT_EQ_INT(CF_ERR_IO, class_file_load("nao/existe/X.class", &cf, &err));
    ASSERT_TRUE(cf == NULL);
    ASSERT_TRUE(strstr(err.message, "nao/existe/X.class") != NULL);
}

/** @brief Registra os testes desta suíte. */
void suite_class_file(void)
{
    RUN_TEST(test_minimal_class);
    RUN_TEST(test_magic_and_version);
    RUN_TEST(test_truncation_and_trailing);
    RUN_TEST(test_constant_pool_errors);
    RUN_TEST(test_bad_this_class);
    RUN_TEST(test_code_attribute);
    RUN_TEST(test_unknown_attribute);
    RUN_TEST(test_missing_file);
}
