/**
 * @file test_class_file_errors.c
 * @brief Testes das validações do leitor que não são cobertas por
 *        test_class_file.c: referências entre estruturas, tabela de
 *        exceções, ConstantValue, StackMapTable e cabeçalhos de atributo.
 *
 * @details Todas as classes são montadas byte a byte sobre a classe mínima
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

/* ------------------------------------------------------------ auxiliares -- */

/** @brief Interpreta um buffer e retorna o status (liberando a classe). */
static CfStatus parse_status(const ByteBuf *b)
{
    ClassFile *cf = NULL;
    CfStatus s = class_file_parse(b->data, b->size, &cf, NULL);
    class_file_free(cf);
    return s;
}

/** @brief Acrescenta @p n bytes brutos ao buffer. */
static void bb_bytes(ByteBuf *b, const uint8_t *bytes, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        bb_u1(b, bytes[i]);
    }
}

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

/**
 * @brief Classe com um campo `static final` e os atributos brutos dados.
 *
 * @details Pool extra: #6 Utf8 "ConstantValue", #7 Integer 42.
 * @param[out] b           Buffer.
 * @param[in]  name        name_index do campo.
 * @param[in]  descriptor  descriptor_index do campo.
 * @param[in]  attrs       Atributos já serializados (cabeçalho + conteúdo).
 * @param[in]  attrs_size  Tamanho de @p attrs.
 * @param[in]  attrs_count attributes_count do campo.
 */
static void build_field_class(ByteBuf *b, uint16_t name, uint16_t descriptor,
                              const uint8_t *attrs, size_t attrs_size,
                              uint16_t attrs_count)
{
    bb_minimal_class_head(b, 2);
    bb_utf8(b, "ConstantValue"); /* #6 */
    bb_u1(b, 3);                 /* #7 Integer */
    bb_u4(b, 42);
    bb_u2(b, 0x0021);
    bb_u2(b, 2);
    bb_u2(b, 4);
    bb_u2(b, 0);      /* interfaces */
    bb_u2(b, 1);      /* fields */
    bb_u2(b, 0x0018); /* static final */
    bb_u2(b, name);
    bb_u2(b, descriptor);
    bb_u2(b, attrs_count);
    bb_bytes(b, attrs, attrs_size);
    bb_u2(b, 0); /* methods */
    bb_u2(b, 0); /* atributos da classe */
}

/**
 * @brief Classe com um método `m()V` cujo Code tem bytecode, tabela de
 *        exceções e sub-atributos escolhidos.
 *
 * @details Pool extra: #6 Utf8 "m", #7 Utf8 "()V", #8 Utf8 "StackMapTable".
 * @param[out] b           Buffer.
 * @param[in]  code        Bytecode.
 * @param[in]  code_length Tamanho do bytecode.
 * @param[in]  exc         Entradas da tabela de exceções, 4 valores cada
 *                         (start_pc, end_pc, handler_pc, catch_type).
 * @param[in]  exc_count   Quantidade de entradas.
 * @param[in]  sub         Sub-atributos do Code já serializados.
 * @param[in]  sub_size    Tamanho de @p sub.
 * @param[in]  sub_count   attributes_count do Code.
 */
static void build_code_class(ByteBuf *b, const uint8_t *code, uint32_t code_length,
                             const uint16_t *exc, uint16_t exc_count,
                             const uint8_t *sub, size_t sub_size, uint16_t sub_count)
{
    uint16_t i;
    bb_minimal_class_head(b, 3);
    bb_utf8(b, "m");             /* #6 */
    bb_utf8(b, "()V");           /* #7 */
    bb_utf8(b, "StackMapTable"); /* #8 */
    bb_u2(b, 0x0021);
    bb_u2(b, 2);
    bb_u2(b, 4);
    bb_u2(b, 0);      /* interfaces */
    bb_u2(b, 0);      /* fields */
    bb_u2(b, 1);      /* methods */
    bb_u2(b, 0x0009); /* public static */
    bb_u2(b, 6);
    bb_u2(b, 7);
    bb_u2(b, 1); /* attributes */
    bb_u2(b, 5); /* "Code" */
    bb_u4(b, (uint32_t)(2 + 2 + 4 + code_length + 2 + 8u * exc_count + 2 + sub_size));
    bb_u2(b, 1); /* max_stack */
    bb_u2(b, 1); /* max_locals */
    bb_u4(b, code_length);
    bb_bytes(b, code, code_length);
    bb_u2(b, exc_count);
    for (i = 0; i < 4u * exc_count; i++) {
        bb_u2(b, exc[i]);
    }
    bb_u2(b, sub_count);
    bb_bytes(b, sub, sub_size);
    bb_u2(b, 0); /* atributos da classe */
}

/** @brief Bytecode usado nos testes do Code: nop, nop, return. */
static const uint8_t CODE3[] = {0x00, 0x00, 0xB1};

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
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));

    build_class_header(&b, 99, NULL, 0); /* fora do pool */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
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
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    build_class_header(&b, 4, zero, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    build_class_header(&b, 4, out, 1); /* = constant_pool_count */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
}

/** @brief Nome e descritor de campos precisam ser Utf8. */
static void test_member_indexes(void)
{
    ByteBuf b;

    build_field_class(&b, 1, 3, NULL, 0, 0);
    ASSERT_EQ_INT(CF_OK, parse_status(&b));

    build_field_class(&b, 2, 3, NULL, 0, 0); /* nome é Class */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));

    build_field_class(&b, 1, 0, NULL, 0, 0); /* descritor 0 */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));

    build_field_class(&b, 1, 7, NULL, 0, 0); /* descritor é Integer */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
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
    ASSERT_EQ_INT(CF_OK, parse_status(&b));
    build_methodref_class(&b, 1, 6); /* classe é Utf8 */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    build_methodref_class(&b, 4, 4); /* NameAndType é Class */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    build_methodref_class(&b, 4, 0); /* índice 0 */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 8); /* #6 String -> #2 (Class) */
    bb_u2(&b, 2);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 7); /* #6 Class -> #200 (fora do pool) */
    bb_u2(&b, 200);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 16); /* #6 MethodType -> #2 (Class) */
    bb_u2(&b, 2);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
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
        ASSERT_EQ_INT(CF_OK, parse_status(&b));
        build_method_handle_class(&b, kind, 8);
        ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    }
    for (kind = 5; kind <= 8; kind++) { /* invoke* e newInvokeSpecial */
        build_method_handle_class(&b, kind, 8);
        ASSERT_EQ_INT(CF_OK, parse_status(&b));
        build_method_handle_class(&b, kind, 7);
        ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    }
    build_method_handle_class(&b, 9, 8); /* invokeInterface exige InterfaceMethodref */
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));

    build_method_handle_class(&b, 0, 7);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, parse_status(&b));
    build_method_handle_class(&b, 10, 7);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, parse_status(&b));
}

/** @brief Entradas da tabela de exceções precisam estar dentro do bytecode. */
static void test_exception_table(void)
{
    static const struct {
        uint16_t entry[4]; /* start_pc, end_pc, handler_pc, catch_type */
        CfStatus expected;
    } CASES[] = {
        {{0, 2, 2, 0}, CF_OK},                /* catch-all (finally) */
        {{0, 3, 2, 4}, CF_OK},                /* end_pc pode ser o fim do código */
        {{2, 2, 2, 0}, CF_ERR_BAD_ATTRIBUTE}, /* intervalo vazio */
        {{2, 1, 2, 0}, CF_ERR_BAD_ATTRIBUTE}, /* start_pc > end_pc */
        {{0, 4, 2, 0}, CF_ERR_BAD_ATTRIBUTE}, /* end_pc além do código */
        {{0, 2, 3, 0}, CF_ERR_BAD_ATTRIBUTE}, /* handler_pc fora do código */
        {{0, 2, 2, 1}, CF_ERR_BAD_INDEX},     /* catch_type é Utf8 */
        {{0, 2, 2, 99}, CF_ERR_BAD_INDEX},    /* catch_type fora do pool */
    };
    size_t i;
    for (i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        ByteBuf b;
        CfStatus s;
        build_code_class(&b, CODE3, sizeof CODE3, CASES[i].entry, 1, NULL, 0, 0);
        s = parse_status(&b);
        if (s != CASES[i].expected) {
            TEST_FAIL_("caso %u: esperado %s, obtido %s", (unsigned)i,
                       cf_status_name(CASES[i].expected), cf_status_name(s));
        }
    }
}

/** @brief code_length acima de 65535 é rejeitado antes de ler o bytecode. */
static void test_code_length_limit(void)
{
    ByteBuf b;
    bb_minimal_class_head(&b, 2);
    bb_utf8(&b, "m");
    bb_utf8(&b, "()V");
    bb_u2(&b, 0x0021);
    bb_u2(&b, 2);
    bb_u2(&b, 4);
    bb_u2(&b, 0);
    bb_u2(&b, 0);
    bb_u2(&b, 1);
    bb_u2(&b, 0x0009);
    bb_u2(&b, 6);
    bb_u2(&b, 7);
    bb_u2(&b, 1);
    bb_u2(&b, 5);  /* "Code" */
    bb_u4(&b, 8);  /* só max_stack, max_locals e code_length */
    bb_u2(&b, 1);
    bb_u2(&b, 1);
    bb_u4(&b, 65536u);
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, parse_status(&b));
}

/** @brief ConstantValue: válido, apontando para Utf8, curto demais e com bytes sobrando. */
static void test_constant_value(void)
{
    static const uint8_t VALID[] = {0, 6, 0, 0, 0, 2, 0, 7};
    static const uint8_t TO_UTF8[] = {0, 6, 0, 0, 0, 2, 0, 1};
    static const uint8_t SHORT[] = {0, 6, 0, 0, 0, 1, 0};
    static const uint8_t LONG[] = {0, 6, 0, 0, 0, 3, 0, 7, 0};
    ByteBuf b;
    ClassFile *cf = NULL;

    build_field_class(&b, 1, 3, VALID, sizeof VALID, 1);
    ASSERT_EQ_INT(CF_OK, class_file_parse(b.data, b.size, &cf, NULL));
    if (cf != NULL) {
        const AttributeInfo *a = attribute_find(cf->fields[0].attributes,
                                                cf->fields[0].attributes_count,
                                                ATTR_CONSTANT_VALUE);
        int32_t value = 0;
        ASSERT_TRUE(a != NULL && cp_get_int(cf, a->info.constantvalue_index, &value));
        ASSERT_EQ_INT(42, value);
        class_file_free(cf);
    }

    build_field_class(&b, 1, 3, TO_UTF8, sizeof TO_UTF8, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    build_field_class(&b, 1, 3, SHORT, sizeof SHORT, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, parse_status(&b));
    build_field_class(&b, 1, 3, LONG, sizeof LONG, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, parse_status(&b));
}

/** @brief attribute_name_index precisa ser Utf8; atributo maior que o arquivo é truncamento. */
static void test_attribute_header(void)
{
    static const uint8_t NAME_IS_CLASS[] = {0, 2, 0, 0, 0, 0};
    static const uint8_t NAME_ZERO[] = {0, 0, 0, 0, 0, 0};
    static const uint8_t TOO_LONG[] = {0, 6, 0, 0, 0, 50, 0, 7};
    ByteBuf b;

    build_field_class(&b, 1, 3, NAME_IS_CLASS, sizeof NAME_IS_CLASS, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    build_field_class(&b, 1, 3, NAME_ZERO, sizeof NAME_ZERO, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
    build_field_class(&b, 1, 3, TOO_LONG, sizeof TOO_LONG, 1);
    ASSERT_EQ_INT(CF_ERR_TRUNCATED, parse_status(&b));
}

/** @brief Um atributo "Code" dentro de outro Code é tratado como desconhecido. */
static void test_nested_code(void)
{
    static const uint8_t SUB[] = {0, 5, 0, 0, 0, 2, 0xAB, 0xCD};
    ByteBuf b;
    ClassFile *cf = NULL;

    build_code_class(&b, CODE3, sizeof CODE3, NULL, 0, SUB, sizeof SUB, 1);
    ASSERT_EQ_INT(CF_OK, class_file_parse(b.data, b.size, &cf, NULL));
    if (cf != NULL) {
        const AttributeInfo *a = attribute_find(cf->methods[0].attributes,
                                                cf->methods[0].attributes_count,
                                                ATTR_CODE);
        ASSERT_TRUE(a != NULL);
        if (a != NULL) {
            const CodeAttribute *code = a->info.code;
            ASSERT_EQ_INT(1, code->attributes_count);
            ASSERT_EQ_INT(ATTR_UNKNOWN, code->attributes[0].kind);
            ASSERT_EQ_INT(0xCD, code->attributes[0].info.raw[1]);
        }
        class_file_free(cf);
    }
}

/**
 * @brief Monta um StackMapTable com o conteúdo bruto dado e retorna o status.
 * @param[in] body Conteúdo do atributo (number_of_entries + quadros).
 * @param[in] size Tamanho de @p body.
 * @param[out] out Classe lida (NULL se não interessar).
 */
static CfStatus parse_stack_map(const uint8_t *body, size_t size, ClassFile **out)
{
    ByteBuf b;
    uint8_t sub[64];
    ClassFile *cf = NULL;
    CfStatus s;
    sub[0] = 0; /* "StackMapTable" = #8 */
    sub[1] = 8;
    sub[2] = 0;
    sub[3] = 0;
    sub[4] = (uint8_t)(size >> 8);
    sub[5] = (uint8_t)(size & 0xFF);
    memcpy(sub + 6, body, size);
    build_code_class(&b, CODE3, sizeof CODE3, NULL, 0, sub, 6 + size, 1);
    s = class_file_parse(b.data, b.size, &cf, NULL);
    if (out != NULL) {
        *out = cf;
    } else {
        class_file_free(cf);
    }
    return s;
}

/** @brief StackMapTable: formas válidas, frame_type reservado e verification_type inválido. */
static void test_stack_map_table(void)
{
    static const uint8_t SAME[] = {0, 2, 0, 251, 0, 0};          /* same + same_extended */
    static const uint8_t STACK_INT[] = {0, 1, 65, 1};             /* same_locals_1 (Integer) */
    static const uint8_t STACK_OBJ[] = {0, 1, 65, 7, 0, 4};       /* ... (Object #4) */
    static const uint8_t FULL[] = {0, 1, 255, 0, 1, 0, 1, 1, 0, 0}; /* full_frame */
    static const uint8_t RESERVED[] = {0, 1, 128};
    static const uint8_t BAD_ITEM[] = {0, 1, 64, 9};
    static const uint8_t OBJ_NOT_CLASS[] = {0, 1, 64, 7, 0, 1};
    static const uint8_t MISSING[] = {0, 2, 0};                  /* 2 quadros, só 1 */
    ClassFile *cf = NULL;

    ASSERT_EQ_INT(CF_OK, parse_stack_map(SAME, sizeof SAME, &cf));
    if (cf != NULL) {
        const CodeAttribute *code = cf->methods[0].attributes[0].info.code;
        const AttributeInfo *a = attribute_find(code->attributes, code->attributes_count,
                                                ATTR_STACK_MAP_TABLE);
        ASSERT_TRUE(a != NULL);
        if (a != NULL) {
            ASSERT_EQ_INT(2, a->info.stack_map_table.number_of_entries);
            ASSERT_EQ_INT(251, a->info.stack_map_table.entries[1].frame_type);
        }
        class_file_free(cf);
    }
    ASSERT_EQ_INT(CF_OK, parse_stack_map(STACK_INT, sizeof STACK_INT, NULL));
    ASSERT_EQ_INT(CF_OK, parse_stack_map(STACK_OBJ, sizeof STACK_OBJ, NULL));
    ASSERT_EQ_INT(CF_OK, parse_stack_map(FULL, sizeof FULL, NULL));

    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, parse_stack_map(RESERVED, sizeof RESERVED, NULL));
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, parse_stack_map(BAD_ITEM, sizeof BAD_ITEM, NULL));
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_stack_map(OBJ_NOT_CLASS, sizeof OBJ_NOT_CLASS, NULL));
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, parse_stack_map(MISSING, sizeof MISSING, NULL));
}

/** @brief Registra os testes desta suíte. */
void suite_class_file_errors(void)
{
    RUN_TEST(test_super_class);
    RUN_TEST(test_interfaces);
    RUN_TEST(test_member_indexes);
    RUN_TEST(test_pool_references);
    RUN_TEST(test_method_handle);
    RUN_TEST(test_exception_table);
    RUN_TEST(test_code_length_limit);
    RUN_TEST(test_constant_value);
    RUN_TEST(test_attribute_header);
    RUN_TEST(test_nested_code);
    RUN_TEST(test_stack_map_table);
}
