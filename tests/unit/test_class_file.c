/**
 * @file test_class_file.c
 * @brief Testes do leitor de `.class`: classes montadas byte a byte (casos de
 *        erro) e classes reais compiladas pelo javac para Java 2, 6 e 8.
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/attributes.h"
#include "classfile/class_file.h"
#include "classfile/constant_pool.h"
#include "common/file_io.h"
#include "test.h"
#include "test_util.h"

/** @brief Versões testadas: diretório dos fixtures e major esperado. */
static const struct {
    const char *dir;
    uint16_t major;
} VERSIONS[] = {{"java2", 46}, {"java6", 50}, {"java8", 52}};

/** @brief Quantidade de versões testadas. */
#define VERSION_COUNT (sizeof VERSIONS / sizeof VERSIONS[0])

/* ------------------------------------------------------------ auxiliares -- */

/** @brief Interpreta um buffer e retorna o status (liberando a classe). */
static CfStatus parse_status(const ByteBuf *b)
{
    ClassFile *cf = NULL;
    ClassFileError err;
    CfStatus s = class_file_parse(b->data, b->size, &cf, &err);
    class_file_free(cf);
    return s;
}

/** @brief Carrega um fixture; em caso de erro, registra a falha com a mensagem. */
static ClassFile *load_fixture(const char *version, const char *name)
{
    char path[256];
    ClassFile *cf = NULL;
    ClassFileError err;
    sprintf(path, "%s/%s/%s.class", FIXTURES_DIR, version, name);
    if (class_file_load(path, &cf, &err) != CF_OK) {
        TEST_FAIL_("falha ao ler %s: %s", path, err.message);
        return NULL;
    }
    return cf;
}

/** @brief Procura um campo ou método pelo nome. */
static const MemberInfo *find_member(const ClassFile *cf, const MemberInfo *members,
                                     uint16_t count, const char *name)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        const char *n = cp_get_utf8(cf, members[i].name_index);
        if (n != NULL && strcmp(n, name) == 0) {
            return &members[i];
        }
    }
    return NULL;
}

/** @brief Retorna o atributo Code de um método (ou NULL). */
static const CodeAttribute *code_of(const MemberInfo *method)
{
    const AttributeInfo *a;
    if (method == NULL) {
        return NULL;
    }
    a = attribute_find(method->attributes, method->attributes_count, ATTR_CODE);
    return a != NULL ? a->info.code : NULL;
}

/** @brief Indica se algum byte do bytecode é igual a @p opcode (aproximação suficiente para os fixtures). */
static int code_contains(const CodeAttribute *code, uint8_t opcode)
{
    uint32_t i;
    for (i = 0; code != NULL && i < code->code_length; i++) {
        if (code->code[i] == opcode) {
            return 1;
        }
    }
    return 0;
}

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
    ASSERT_EQ_INT(CF_ERR_BAD_MAGIC, parse_status(&b));
    b.data[0] = 0xCA;
    for (major = 43; major <= 54; major++) {
        CfStatus expected = (major >= 45 && major <= 52) ? CF_OK
                                                         : CF_ERR_UNSUPPORTED_VERSION;
        b.data[6] = (uint8_t)(major >> 8);
        b.data[7] = (uint8_t)(major & 0xFF);
        ASSERT_EQ_INT(expected, parse_status(&b));
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
        if (parse_status(&b) != CF_ERR_TRUNCATED) {
            failures++;
        }
    }
    ASSERT_EQ_INT(0, failures);
    b.size = full;
    bb_u1(&b, 0x00);
    ASSERT_EQ_INT(CF_ERR_TRAILING_BYTES, parse_status(&b));
}

/** @brief Erros no pool: tag desconhecida, Long na última posição, índice para a 2ª metade de Long, MUTF-8 inválido. */
static void test_constant_pool_errors(void)
{
    ByteBuf b;

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 2); /* tag inexistente */
    bb_u2(&b, 0);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 5); /* Long como última entrada: falta a 2ª posição */
    bb_u4(&b, 0);
    bb_u4(&b, 1);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, parse_status(&b));

    bb_minimal_class_head(&b, 3);
    bb_u1(&b, 5); /* #6-#7 Long */
    bb_u4(&b, 0);
    bb_u4(&b, 1);
    bb_u1(&b, 8); /* #8 String -> #7 (inutilizável) */
    bb_u2(&b, 7);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));

    bb_minimal_class_head(&b, 1);
    bb_u1(&b, 1); /* Utf8 com byte zero */
    bb_u2(&b, 2);
    bb_u1(&b, 'a');
    bb_u1(&b, 0x00);
    bb_minimal_class_tail(&b);
    ASSERT_EQ_INT(CF_ERR_BAD_CONSTANT, parse_status(&b));
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
    ASSERT_EQ_INT(CF_ERR_BAD_INDEX, parse_status(&b));
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
        code = code_of(&cf->methods[0]);
        ASSERT_TRUE(code != NULL);
        if (code != NULL) {
            ASSERT_EQ_INT(1, code->code_length);
            ASSERT_EQ_INT(0xB1, code->code[0]);
            ASSERT_EQ_INT(1, code->max_locals);
        }
        class_file_free(cf);
    }

    build_class_with_code(&b, 1, 1);
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, parse_status(&b));

    build_class_with_code(&b, 0, 0);
    ASSERT_EQ_INT(CF_ERR_BAD_ATTRIBUTE, parse_status(&b));
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

/* ------------------------------------------------- classes reais (javac) -- */

/** @brief Constantes numéricas e de texto (ConstantValue) em Java 2, 6 e 8. */
static void test_fixture_constants(void)
{
    static const char expected_text[] = "Ola \xC3\xA9\xE4\xB8\xAD\xC0\x80"
                                        "\xED\xA0\xBD\xED\xB8\x80!";
    size_t v;
    for (v = 0; v < VERSION_COUNT; v++) {
        ClassFile *cf = load_fixture(VERSIONS[v].dir, "AllConstants");
        const MemberInfo *f;
        const AttributeInfo *cv;
        int32_t i32 = 0;
        int64_t i64 = 0;
        float f32 = 0;
        double f64 = 0;
        if (cf == NULL) {
            continue;
        }
        ASSERT_EQ_INT(VERSIONS[v].major, cf->major_version);
        ASSERT_EQ_STR("AllConstants", cp_get_class_name(cf, cf->this_class));

        f = find_member(cf, cf->fields, cf->fields_count, "BIG_INT");
        cv = f ? attribute_find(f->attributes, f->attributes_count, ATTR_CONSTANT_VALUE) : NULL;
        ASSERT_TRUE(cv != NULL && cp_get_int(cf, cv->info.constantvalue_index, &i32));
        ASSERT_EQ_INT(123456789, i32);

        f = find_member(cf, cf->fields, cf->fields_count, "BIG_LONG");
        cv = f ? attribute_find(f->attributes, f->attributes_count, ATTR_CONSTANT_VALUE) : NULL;
        ASSERT_TRUE(cv != NULL && cp_get_long(cf, cv->info.constantvalue_index, &i64));
        ASSERT_TRUE(i64 == 1234567890123LL);

        f = find_member(cf, cf->fields, cf->fields_count, "FLOAT_VALUE");
        cv = f ? attribute_find(f->attributes, f->attributes_count, ATTR_CONSTANT_VALUE) : NULL;
        ASSERT_TRUE(cv != NULL && cp_get_float(cf, cv->info.constantvalue_index, &f32));
        ASSERT_TRUE(f32 == 3.5f);

        f = find_member(cf, cf->fields, cf->fields_count, "DOUBLE_VALUE");
        cv = f ? attribute_find(f->attributes, f->attributes_count, ATTR_CONSTANT_VALUE) : NULL;
        ASSERT_TRUE(cv != NULL && cp_get_double(cf, cv->info.constantvalue_index, &f64));
        ASSERT_TRUE(f64 == 2.718281828);

        f = find_member(cf, cf->fields, cf->fields_count, "TEXT");
        cv = f ? attribute_find(f->attributes, f->attributes_count, ATTR_CONSTANT_VALUE) : NULL;
        ASSERT_TRUE(cv != NULL);
        if (cv != NULL) {
            ASSERT_EQ_STR(expected_text, cp_get_string(cf, cv->info.constantvalue_index));
        }

        f = find_member(cf, cf->fields, cf->fields_count, "instanceField");
        ASSERT_TRUE(f != NULL && (f->access_flags & ACC_STATIC) == 0);
        ASSERT_TRUE(find_member(cf, cf->methods, cf->methods_count, "main") != NULL);
        ASSERT_TRUE(find_member(cf, cf->methods, cf->methods_count, "<init>") != NULL);
        class_file_free(cf);
    }
}

/** @brief Herança, interfaces e flags de classe abstrata/interface. */
static void test_fixture_hierarchy(void)
{
    size_t v;
    for (v = 0; v < VERSION_COUNT; v++) {
        ClassFile *cf = load_fixture(VERSIONS[v].dir, "Hierarchy");
        ClassFile *base = load_fixture(VERSIONS[v].dir, "Base");
        ClassFile *shape = load_fixture(VERSIONS[v].dir, "Shape");
        if (cf != NULL) {
            ASSERT_EQ_STR("Base", cp_get_class_name(cf, cf->super_class));
            ASSERT_EQ_INT(2, cf->interfaces_count);
            if (cf->interfaces_count == 2) {
                ASSERT_EQ_STR("Shape", cp_get_class_name(cf, cf->interfaces[0]));
                ASSERT_EQ_STR("Named", cp_get_class_name(cf, cf->interfaces[1]));
            }
        }
        if (base != NULL) {
            ASSERT_TRUE((base->access_flags & ACC_ABSTRACT) != 0);
        }
        if (shape != NULL) {
            ASSERT_TRUE((shape->access_flags & ACC_INTERFACE) != 0);
        }
        class_file_free(cf);
        class_file_free(base);
        class_file_free(shape);
    }
}

/** @brief Exceções, switch, classes internas, tabelas de depuração e StackMapTable. */
static void test_fixture_flow(void)
{
    size_t v;
    for (v = 0; v < VERSION_COUNT; v++) {
        ClassFile *cf = load_fixture(VERSIONS[v].dir, "Flow");
        const MemberInfo *run, *divide;
        const CodeAttribute *code;
        const AttributeInfo *a;
        if (cf == NULL) {
            continue;
        }
        run = find_member(cf, cf->methods, cf->methods_count, "run");
        divide = find_member(cf, cf->methods, cf->methods_count, "divide");
        code = code_of(run);
        ASSERT_TRUE(code != NULL);
        if (code != NULL) {
            ASSERT_TRUE(code->exception_table_length >= 2); /* catch + finally */
            ASSERT_TRUE(code_contains(code, 0xC5));         /* multianewarray */
            ASSERT_TRUE(attribute_find(code->attributes, code->attributes_count,
                                       ATTR_LINE_NUMBER_TABLE) != NULL);
            ASSERT_TRUE(attribute_find(code->attributes, code->attributes_count,
                                       ATTR_LOCAL_VARIABLE_TABLE) != NULL);
            a = attribute_find(code->attributes, code->attributes_count,
                               ATTR_STACK_MAP_TABLE);
            /* StackMapTable só existe a partir do Java 6 (major 50). */
            ASSERT_EQ_INT(VERSIONS[v].major >= 50, a != NULL);
            if (a != NULL) {
                ASSERT_TRUE(a->info.stack_map_table.number_of_entries > 0);
            }
        }
        a = divide ? attribute_find(divide->attributes, divide->attributes_count,
                                    ATTR_EXCEPTIONS)
                   : NULL;
        ASSERT_TRUE(a != NULL);
        if (a != NULL && a->info.exceptions.number_of_exceptions == 1) {
            ASSERT_EQ_STR("Flow$Failure",
                          cp_get_class_name(cf, a->info.exceptions.exception_index_table[0]));
        }
        ASSERT_TRUE(code_contains(code_of(find_member(cf, cf->methods, cf->methods_count,
                                                      "dense")), 0xAA)); /* tableswitch */
        ASSERT_TRUE(code_contains(code_of(find_member(cf, cf->methods, cf->methods_count,
                                                      "sparse")), 0xAB)); /* lookupswitch */
        ASSERT_TRUE(attribute_find(cf->attributes, cf->attributes_count,
                                   ATTR_INNER_CLASSES) != NULL);
        ASSERT_TRUE(attribute_find(cf->attributes, cf->attributes_count,
                                   ATTR_SOURCE_FILE) != NULL);
        class_file_free(cf);
    }
}

/** @brief Java 8: invokedynamic, MethodHandle, BootstrapMethods e Signature. */
static void test_fixture_lambdas(void)
{
    ClassFile *cf = load_fixture("java8", "Lambdas");
    const AttributeInfo *a;
    const MemberInfo *f;
    uint16_t i;
    int indy = 0, handles = 0;
    if (cf == NULL) {
        return;
    }
    for (i = 1; i < cf->constant_pool_count; i++) {
        indy += cp_tag(cf, i) == CONSTANT_InvokeDynamic;
        handles += cp_tag(cf, i) == CONSTANT_MethodHandle;
    }
    ASSERT_TRUE(indy >= 2);
    ASSERT_TRUE(handles >= 1);
    a = attribute_find(cf->attributes, cf->attributes_count, ATTR_BOOTSTRAP_METHODS);
    ASSERT_TRUE(a != NULL && a->info.bootstrap.num_bootstrap_methods >= 1);
    f = find_member(cf, cf->fields, cf->fields_count, "names");
    ASSERT_TRUE(f != NULL &&
                attribute_find(f->attributes, f->attributes_count, ATTR_SIGNATURE) != NULL);
    class_file_free(cf);
}

/** @brief Todo prefixo de um `.class` real gera truncamento; byte extra gera erro. */
static void test_fixture_truncation(void)
{
    uint8_t *data = NULL;
    size_t size = 0, n;
    int failures = 0;
    ClassFile *cf = NULL;
    uint8_t *bigger;
    if (!file_read_all(FIXTURES_DIR "/java8/Flow.class", &data, &size)) {
        TEST_FAIL_("%s", "nao foi possivel ler Flow.class");
        return;
    }
    for (n = 0; n < size; n++) {
        if (class_file_parse(data, n, &cf, NULL) != CF_ERR_TRUNCATED) {
            failures++;
        }
        class_file_free(cf);
    }
    ASSERT_EQ_INT(0, failures);
    bigger = malloc(size + 1);
    if (bigger != NULL) {
        memcpy(bigger, data, size);
        bigger[size] = 0;
        ASSERT_EQ_INT(CF_ERR_TRAILING_BYTES, class_file_parse(bigger, size + 1, &cf, NULL));
        free(bigger);
    }
    free(data);
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
    RUN_TEST(test_fixture_constants);
    RUN_TEST(test_fixture_hierarchy);
    RUN_TEST(test_fixture_flow);
    RUN_TEST(test_fixture_lambdas);
    RUN_TEST(test_fixture_truncation);
}
