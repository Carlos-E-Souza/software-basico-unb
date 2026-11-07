/**
 * @file test_class_file_fixtures.c
 * @brief Testes do leitor de `.class` com classes reais compiladas pelo javac
 *        para Java 2, 6 e 8 (diretório de fixtures).
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
        code = method_code(run);
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
        ASSERT_TRUE(code_contains(method_code(find_member(cf, cf->methods, cf->methods_count,
                                              "dense")), 0xAA)); /* tableswitch */
        ASSERT_TRUE(code_contains(method_code(find_member(cf, cf->methods, cf->methods_count,
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
void suite_class_file_fixtures(void)
{
    RUN_TEST(test_fixture_constants);
    RUN_TEST(test_fixture_hierarchy);
    RUN_TEST(test_fixture_flow);
    RUN_TEST(test_fixture_lambdas);
    RUN_TEST(test_fixture_truncation);
}
