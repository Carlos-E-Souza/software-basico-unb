/**
 * @file version.c
 * @brief Implementação das funções de versão da JVM.
 */
#include "version.h"

/** @brief Converte o argumento de macro em literal de string (passo interno). */
#define JVM_STRINGIFY_IMPL(x) #x
/** @brief Converte o valor expandido de uma macro em literal de string. */
#define JVM_STRINGIFY(x) JVM_STRINGIFY_IMPL(x)

const char *jvm_version_string(void)
{
    return JVM_STRINGIFY(JVM_VERSION_MAJOR) "."
           JVM_STRINGIFY(JVM_VERSION_MINOR) "."
           JVM_STRINGIFY(JVM_VERSION_PATCH);
}
