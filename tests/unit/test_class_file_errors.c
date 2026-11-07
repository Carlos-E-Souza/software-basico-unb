/**
 * @file test_class_file_errors.c
 * @brief Testes das validações estruturais do leitor que não são cobertas
 *        por test_class_file.c: super_class, interfaces, índices de
 *        campos e referências entre entradas do pool (incluindo MethodHandle).
 *
 * @details As validações de atributos estão em test_attribute_errors.c.
 * Todas as classes são montadas byte a byte sobre a classe mínima
 * de test_util.h, cujo pool é: #1 Utf8 "Min", #2 Class #1,
 * #3 Utf8 "java/lang/Object", #4 Class #3, #5 Utf8 "Code".
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/attributes.h"
#include "classfile/class_file.h"
#include "classfile/constant_pool.h"
#include "test.h"
#include "test_util.h"

/**
 * @brief Classe mínima com super_class e uma lista de interfaces escolhidas.
 * @param[out] b          Buffer.
 * @param[in]  super      Índice de super_class.
 * @param[in]  interfaces Índices das interfaces.
 * @param[in]  count      Quantidade de interfaces.
 */
static void build_class_header(ByteBuf *b, uint16_t super, const uint16_t *interfaces,
                               uint16_t count)
{
    uint16_t i;
    bb_minimal_class_head(b, 0);
    bb_u2(b, 0x0021);
    bb_u2(b, 2); /* this_class */
    bb_u2(b, super);
    bb_u2(b, count);
    for (i = 0; i < count; i++) {
        bb_u2(b, interfaces[i]);
    }
    bb_u2(b, 0); /* fields */
    bb_u2(b, 0); /* methods */
    bb_u2(b, 0); /* attributes */
}

/* ----------------------------------------------------------------- testes -- */

/** @brief super_class 0 é aceito (caso de java/lang/Object); se não for 0, precisa ser Class. */
static void test_super_class(void)
{
    ByteBuf b;
    ClassFile *cf = NULL;

    build_class_header(&b, 0, NULL, 0);
    ASSERT_EQ_INT(CF_OK, class_file_parse(b.data, b.size, &cf, NULL));
    if (cf != NULL) {
        ASSERT_EQ_INT(0, cf->super_class);
        class_file_free(cf);
    }

    build_class_header(&b, 3, NULL, 0); /* #3 é Utf8 */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));

    build_class_header(&b, 99, NULL, 0); /* fora do pool */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
}

/** @brief Cada interface precisa apontar para uma constante Class válida. */
static void test_interfaces(void)
{
    static const uint16_t ok[] = {4, 2};
    static const uint16_t utf8[] = {4, 1};
    static const uint16_t zero[] = {0};
    static const uint16_t out[] = {6};
    ByteBuf b;
    ClassFile *cf = NULL;

    build_class_header(&b, 4, ok, 2);
    ASSERT_EQ_INT(CF_OK, class_file_parse(b.data, b.size, &cf, NULL));
    if (cf != NULL) {
        ASSERT_EQ_INT(2, cf->interfaces_count);
        ASSERT_EQ_STR("Min", cp_get_class_name(cf, cf->interfaces[1]));
        class_file_free(cf);
    }

    build_class_header(&b, 4, utf8, 2);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
    build_class_header(&b, 4, zero, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
    build_class_header(&b, 4, out, 1); /* = constant_pool_count */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
}

/** @brief Nome e descritor de campos precisam ser Utf8. */
static void test_member_indexes(void)
{
    ByteBuf b;

    bb_field_class(&b, 1, 3, NULL, 0, 0);
    ASSERT_EQ_INT(CF_OK, bb_parse_status(&b));

    bb_field_class(&b, 2, 3, NULL, 0, 0); /* nome é Class */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));

    bb_field_class(&b, 1, 0, NULL, 0, 0); /* descritor 0 */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));

    bb_field_class(&b, 1, 7, NULL, 0, 0); /* descritor é Integer */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
}

/**
 * @brief Monta uma classe com #6 NameAndType(1, 3) e #7 Methodref(@p cls, @p nat).
 */
static void build_methodref_class(ByteBuf *b, uint16_t cls, uint16_t nat)
{
    bb_minimal_class_head(b, 2);
    bb_u1(b, 12); /* #6 NameAndType */
    bb_u2(b, 1);
    bb_u2(b, 3);
    bb_u1(b, 10); /* #7 Methodref */
    bb_u2(b, cls);
    bb_u2(b, nat);
    bb_minimal_class_tail(b);
}

/** @brief Referências entre entradas do pool são conferidas pelo tipo esperado. */
static void test_pool_references(void)
{
    ByteBuf b;

    build_methodref_class(&b, 4, 6);
    ASSERT_EQ_INT(CF_OK, bb_parse_status(&b));
    build_methodref_class(&b, 1, 6); /* classe é Utf8 */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
    build_methodref_class(&b, 4, 4); /* NameAndType é Class */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
    build_methodref_class(&b, 4, 0); /* índice 0 */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 8); /* #6 String -> #2 (Class) */
    bb_u2(&b, 2);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 7); /* #6 Class -> #200 (fora do pool) */
    bb_u2(&b, 200);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 16); /* #6 MethodType -> #2 (Class) */
    bb_u2(&b, 2);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
}

/**
 * @brief Monta uma classe com #6 NameAndType, #7 Fieldref, #8 Methodref e
 *        #9 MethodHandle(@p kind, @p ref).
 */
static void build_method_handle_class(ByteBuf *b, uint8_t kind, uint16_t ref)
{
    bb_minimal_class_head(b, 4);
    bb_u1(b, 12); /* #6 NameAndType */
    bb_u2(b, 1);
    bb_u2(b, 3);
    bb_u1(b, 9); /* #7 Fieldref */
    bb_u2(b, 4);
    bb_u2(b, 6);
    bb_u1(b, 10); /* #8 Methodref */
    bb_u2(b, 4);
    bb_u2(b, 6);
    bb_u1(b, 15); /* #9 MethodHandle */
    bb_u1(b, kind);
    bb_u2(b, ref);
    bb_minimal_class_tail(b);
}

/** @brief MethodHandle: o tipo da referência depende de reference_kind (JVMS 4.4.8). */
static void test_method_handle(void)
{
    ByteBuf b;
    uint8_t kind;

    for (kind = 1; kind <= 4; kind++) { /* get/put field/static */
        build_method_handle_class(&b, kind, 7);
        ASSERT_EQ_INT(CF_OK, bb_parse_status(&b));
        build_method_handle_class(&b, kind, 8);
        ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
    }
    for (kind = 5; kind <= 8; kind++) { /* invoke* e newInvokeSpecial */
        build_method_handle_class(&b, kind, 8);
        ASSERT_EQ_INT(CF_OK, bb_parse_status(&b));
        build_method_handle_class(&b, kind, 7);
        ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));
    }
    build_method_handle_class(&b, 9, 8); /* invokeInterface exige InterfaceMethodref */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, bb_parse_status(&b));

    build_method_handle_class(&b, 0, 7);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, bb_parse_status(&b));
    build_method_handle_class(&b, 10, 7);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, bb_parse_status(&b));
}

/** @brief Registra os testes desta suíte. */
void suite_class_file_errors(void)
{
    RUN_TEST(test_super_class);
    RUN_TEST(test_interfaces);
    RUN_TEST(test_member_indexes);
    RUN_TEST(test_pool_references);
    RUN_TEST(test_method_handle);
}
