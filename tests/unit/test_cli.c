/**
 * @file test_cli.c
 * @brief Testes da interpretação da linha de comando e da resolução de caminhos.
 */
#include <string.h>

#include "cli/cli.h"
#include "test.h"

/** @brief Interpreta um vetor de argumentos terminado em NULL. */
static bool parse(const char **args, CliOptions *options, char *error)
{
    int argc = 0;
    while (args[argc] != NULL) {
        argc++;
    }
    return cli_parse(argc, (char **)args, options, error, CLI_ERROR_SIZE);
}

/** @brief Formas válidas: atalhos, --mode, classpath, saída e ordem livre. */
static void test_parse_valid(void)
{
    CliOptions o;
    char error[CLI_ERROR_SIZE];
    const char *a1[] = {"jvm", "-v", "-cp", "dir", "Hello", NULL};
    const char *a2[] = {"jvm", "Hello", "--mode", "interpreter", "--classpath", "d", NULL};
    const char *a3[] = {"jvm", "--viewer", "x/Hello.class", "-o", "out.txt", NULL};
    const char *a4[] = {"jvm", "-x", "--help", NULL};

    ASSERT_TRUE(parse(a1, &o, error));
    ASSERT_EQ_INT(CLI_MODE_VIEWER, o.mode);
    ASSERT_EQ_STR("dir", o.classpath);
    ASSERT_EQ_STR("Hello", o.target);
    ASSERT_TRUE(o.output == NULL);

    ASSERT_TRUE(parse(a2, &o, error));
    ASSERT_EQ_INT(CLI_MODE_INTERPRETER, o.mode);
    ASSERT_EQ_STR("d", o.classpath);

    ASSERT_TRUE(parse(a3, &o, error));
    ASSERT_EQ_STR("out.txt", o.output);
    ASSERT_TRUE(o.classpath == NULL);

    /* opção desconhecida antes de --help é erro (a análise é sequencial) */
    ASSERT_TRUE(!parse(a4, &o, error));
}

/** @brief Erros de uso geram mensagem e retornam false. */
static void test_parse_errors(void)
{
    CliOptions o;
    char error[CLI_ERROR_SIZE];
    const char *no_mode[] = {"jvm", "Hello", NULL};
    const char *no_target[] = {"jvm", "-v", NULL};
    const char *two_targets[] = {"jvm", "-v", "A", "B", NULL};
    const char *two_modes[] = {"jvm", "-v", "-i", "A", NULL};
    const char *missing_cp[] = {"jvm", "-v", "A", "-cp", NULL};
    const char *bad_mode[] = {"jvm", "--mode", "x", "A", NULL};
    const char *output_interp[] = {"jvm", "-i", "A", "-o", "f", NULL};
    const char *help[] = {"jvm", "--help", NULL};
    const char *version[] = {"jvm", "--version", NULL};

    ASSERT_TRUE(!parse(no_mode, &o, error));
    ASSERT_TRUE(strstr(error, "modo") != NULL);
    ASSERT_TRUE(!parse(no_target, &o, error));
    ASSERT_TRUE(!parse(two_targets, &o, error));
    ASSERT_TRUE(strstr(error, "B") != NULL);
    ASSERT_TRUE(!parse(two_modes, &o, error));
    ASSERT_TRUE(!parse(missing_cp, &o, error));
    ASSERT_TRUE(!parse(bad_mode, &o, error));
    ASSERT_TRUE(!parse(output_interp, &o, error));
    ASSERT_TRUE(parse(help, &o, error));
    ASSERT_EQ_INT(CLI_MODE_HELP, o.mode);
    ASSERT_TRUE(parse(version, &o, error));
    ASSERT_EQ_INT(CLI_MODE_VERSION, o.mode);
}

/** @brief Resolução: nome de classe, pacote, arquivo .class com e sem classpath. */
static void test_resolve_path(void)
{
    CliOptions o;
    char path[64];
    memset(&o, 0, sizeof o);

    o.target = "Hello";
    ASSERT_TRUE(cli_resolve_class_path(&o, path, sizeof path));
    ASSERT_EQ_STR("./Hello.class", path);

    o.classpath = "out/java8";
    ASSERT_TRUE(cli_resolve_class_path(&o, path, sizeof path));
    ASSERT_EQ_STR("out/java8/Hello.class", path);

    o.target = "pkg.sub.Hello";
    o.classpath = "../a.b";
    ASSERT_TRUE(cli_resolve_class_path(&o, path, sizeof path));
    ASSERT_EQ_STR("../a.b/pkg/sub/Hello.class", path);

    o.target = "x/Hello.class";
    o.classpath = NULL;
    ASSERT_TRUE(cli_resolve_class_path(&o, path, sizeof path));
    ASSERT_EQ_STR("x/Hello.class", path);

    o.classpath = "base";
    ASSERT_TRUE(cli_resolve_class_path(&o, path, sizeof path));
    ASSERT_EQ_STR("base/x/Hello.class", path);

    ASSERT_TRUE(!cli_resolve_class_path(&o, path, 8));
}

/** @brief Registra os testes desta suíte. */
void suite_cli(void)
{
    RUN_TEST(test_parse_valid);
    RUN_TEST(test_parse_errors);
    RUN_TEST(test_resolve_path);
}
