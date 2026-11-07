/**
 * @file version.h
 * @brief Informações de versão da JVM.
 *
 * @details Centraliza o número de versão do programa, exibido pela opção
 * `--version` da linha de comando e pela documentação.
 */
#ifndef JVM_VERSION_H
#define JVM_VERSION_H

/** @brief Número principal (major) da versão do programa. */
#define JVM_VERSION_MAJOR 0
/** @brief Número secundário (minor) da versão do programa. */
#define JVM_VERSION_MINOR 1
/** @brief Número de correção (patch) da versão do programa. */
#define JVM_VERSION_PATCH 0

/**
 * @brief Retorna a versão do programa no formato "MAJOR.MINOR.PATCH".
 *
 * @details
 * **Algoritmo:**
 * 1. Monta, em tempo de compilação, a string a partir das macros
 *    #JVM_VERSION_MAJOR, #JVM_VERSION_MINOR e #JVM_VERSION_PATCH;
 * 2. Retorna um ponteiro para essa string constante.
 *
 * @return Ponteiro para string estática terminada em `'\0'` (não deve ser liberada).
 */
const char *jvm_version_string(void);

#endif /* JVM_VERSION_H */
