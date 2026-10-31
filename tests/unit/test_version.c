/**
 * @file test_version.c
 * @brief Testes unitários do módulo de versão.
 */
#include "test.h"
#include "version.h"

/** @brief A string de versão deve existir e corresponder às macros. */
static void test_version_string_matches_macros(void)
{
    char expected[32];
    sprintf(expected, "%d.%d.%d", JVM_VERSION_MAJOR, JVM_VERSION_MINOR,
            JVM_VERSION_PATCH);
    ASSERT_EQ_STR(expected, jvm_version_string());
}

/** @brief Registra os testes da suíte de versão. */
void suite_version(void)
{
    RUN_TEST(test_version_string_matches_macros);
}
