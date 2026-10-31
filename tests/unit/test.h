/**
 * @file test.h
 * @brief Mini-framework de testes unitários (sem dependências externas).
 *
 * @details Cada arquivo `test_*.c` define uma função de suíte
 * `void suite_<nome>(void)` que chama #RUN_TEST para cada caso de teste.
 * A suíte deve ser registrada em `test_main.c`.
 *
 * Exemplo:
 * @code
 * static void test_soma(void) { ASSERT_EQ_INT(4, 2 + 2); }
 * void suite_exemplo(void) { RUN_TEST(test_soma); }
 * @endcode
 */
#ifndef JVM_TEST_H
#define JVM_TEST_H

#include <stdio.h>
#include <string.h>

/** @brief Total de asserções que falharam (definido em test_main.c). */
extern int test_failures;
/** @brief Total de casos de teste executados (definido em test_main.c). */
extern int test_count;
/** @brief Indica se o caso de teste atual já falhou (definido em test_main.c). */
extern int test_current_failed;

/** @brief Registra uma falha com arquivo e linha. */
#define TEST_FAIL_(msg_fmt, ...)                                              \
    do {                                                                      \
        fprintf(stderr, "    FALHA %s:%d: " msg_fmt "\n", __FILE__, __LINE__, \
                __VA_ARGS__);                                                 \
        test_failures++;                                                      \
        test_current_failed = 1;                                              \
    } while (0)

/** @brief Verifica se a condição é verdadeira. */
#define ASSERT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) TEST_FAIL_("esperado verdadeiro: %s", #cond);            \
    } while (0)

/** @brief Verifica se dois inteiros são iguais. */
#define ASSERT_EQ_INT(expected, actual)                                       \
    do {                                                                      \
        long long e_ = (long long)(expected), a_ = (long long)(actual);       \
        if (e_ != a_) TEST_FAIL_("esperado %lld, obtido %lld", e_, a_);       \
    } while (0)

/** @brief Verifica se duas strings C são iguais. */
#define ASSERT_EQ_STR(expected, actual)                                       \
    do {                                                                      \
        const char *e_ = (expected), *a_ = (actual);                          \
        if (e_ == NULL || a_ == NULL || strcmp(e_, a_) != 0)                  \
            TEST_FAIL_("esperado \"%s\", obtido \"%s\"",                      \
                       e_ ? e_ : "(null)", a_ ? a_ : "(null)");               \
    } while (0)

/** @brief Executa um caso de teste e imprime OK/FALHOU. */
#define RUN_TEST(fn)                                                          \
    do {                                                                      \
        test_current_failed = 0;                                              \
        test_count++;                                                         \
        fn();                                                                 \
        printf("  [%s] %s\n", test_current_failed ? "FALHOU" : "OK", #fn);    \
    } while (0)

#endif /* JVM_TEST_H */
