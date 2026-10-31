/**
 * @file cf_error.h
 * @brief Códigos e mensagens de erro da leitura de arquivos `.class`.
 *
 * @details Toda função de leitura retorna um #CfStatus e, em caso de erro,
 * preenche uma estrutura #ClassFileError com a posição no arquivo e uma
 * mensagem legível (em português) para o usuário.
 */
#ifndef JVM_CF_ERROR_H
#define JVM_CF_ERROR_H

#include <stddef.h>

/** @brief Resultado das operações de leitura de `.class`. */
typedef enum CfStatus {
    CF_OK = 0,                  /**< Sucesso. */
    CF_ERR_IO,                  /**< Falha ao abrir ou ler o arquivo. */
    CF_ERR_MEMORY,              /**< Falha de alocação de memória. */
    CF_ERR_TRUNCATED,           /**< O arquivo terminou antes do esperado. */
    CF_ERR_BAD_MAGIC,           /**< Número mágico diferente de 0xCAFEBABE. */
    CF_ERR_UNSUPPORTED_VERSION, /**< Versão do formato fora de 45–52 (Java 1.1 a 8). */
    CF_ERR_BAD_CONSTANT,        /**< Entrada inválida no pool de constantes. */
    CF_ERR_BAD_INDEX,           /**< Índice para o pool de constantes inválido ou do tipo errado. */
    CF_ERR_BAD_ATTRIBUTE,       /**< Atributo malformado. */
    CF_ERR_TRAILING_BYTES       /**< Há bytes sobrando após o fim da estrutura ClassFile. */
} CfStatus;

/** @brief Tamanho máximo da mensagem de erro, incluindo o '\\0'. */
#define CF_ERROR_MESSAGE_SIZE 256

/** @brief Detalhes de um erro de leitura. */
typedef struct ClassFileError {
    CfStatus status;                      /**< Código do erro. */
    size_t offset;                        /**< Posição (em bytes) no arquivo onde o erro foi detectado. */
    char message[CF_ERROR_MESSAGE_SIZE];  /**< Descrição legível do erro. */
} ClassFileError;

/**
 * @brief Registra um erro (se @p err não for NULL) e retorna o próprio código.
 *
 * @details Permite escrever `return cf_fail(err, CF_ERR_X, pos, "...", ...);`.
 *
 * **Algoritmo:**
 * 1. Se @p err for NULL, apenas retorna @p status;
 * 2. Caso contrário, grava @p status e @p offset e formata a mensagem com
 *    `vsnprintf` (truncada em #CF_ERROR_MESSAGE_SIZE).
 *
 * @param[out] err    Estrutura de erro a preencher (pode ser NULL).
 * @param[in]  status Código do erro.
 * @param[in]  offset Posição no arquivo onde o erro ocorreu.
 * @param[in]  format Formato da mensagem, no estilo `printf`.
 * @param[in]  ...    Argumentos do formato.
 * @return O próprio @p status.
 */
CfStatus cf_fail(ClassFileError *err, CfStatus status, size_t offset,
                 const char *format, ...);

/**
 * @brief Retorna uma descrição genérica de um código de erro.
 *
 * @details **Algoritmo:** consulta uma tabela indexada pelo código.
 *
 * @param[in] status Código de erro.
 * @return String estática com a descrição (nunca NULL).
 */
const char *cf_status_name(CfStatus status);

#endif /* JVM_CF_ERROR_H */
