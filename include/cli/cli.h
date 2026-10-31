/**
 * @file cli.h
 * @brief Interpretação dos argumentos de linha de comando e resolução do
 *        caminho do arquivo `.class` a partir do classpath.
 */
#ifndef JVM_CLI_H
#define JVM_CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/** @brief Código de saída: sucesso. */
#define EXIT_CODE_OK 0
/** @brief Código de saída: argumentos inválidos. */
#define EXIT_CODE_USAGE 1
/** @brief Código de saída: funcionalidade ainda não implementada. */
#define EXIT_CODE_NOT_IMPLEMENTED 2
/** @brief Código de saída: erro ao ler o `.class` ou ao gravar a saída. */
#define EXIT_CODE_CLASS_ERROR 3

/** @brief Modo de operação escolhido na linha de comando. */
typedef enum CliMode {
    CLI_MODE_NONE = 0,    /**< Nenhum modo informado (erro de uso). */
    CLI_MODE_VIEWER,      /**< Leitor/exibidor. */
    CLI_MODE_INTERPRETER, /**< Interpretador. */
    CLI_MODE_HELP,        /**< Exibir ajuda. */
    CLI_MODE_VERSION      /**< Exibir versão. */
} CliMode;

/** @brief Opções obtidas da linha de comando (as strings apontam para `argv`). */
typedef struct CliOptions {
    CliMode mode;          /**< Modo de operação. */
    const char *classpath; /**< Diretório dos `.class` (NULL = não informado). */
    const char *target;    /**< Classe (ex.: `Exemplo`, `pkg.Exemplo`) ou arquivo `.class`. */
    const char *output;    /**< Arquivo de saída do exibidor (NULL = terminal). */
} CliOptions;

/** @brief Tamanho recomendado do buffer de mensagem de erro de cli_parse(). */
#define CLI_ERROR_SIZE 256

/**
 * @brief Interpreta os argumentos da linha de comando.
 *
 * @details
 * Opções aceitas (em qualquer ordem):
 * - `--mode viewer|interpreter`, ou os atalhos `-v`/`--viewer` e `-i`/`--interpreter`;
 * - `-cp <dir>`, `--classpath <dir>`;
 * - `-o <arquivo>`, `--output <arquivo>`;
 * - `-h`, `--help` e `--version`;
 * - exatamente um argumento posicional: a classe ou o arquivo `.class`.
 *
 * **Algoritmo:**
 * 1. Percorre `argv`, reconhecendo opções e seus valores;
 * 2. `--help`/`--version` encerram a análise imediatamente;
 * 3. Valida: modo informado uma única vez, valores presentes, um único alvo;
 * 4. Em caso de erro, escreve a mensagem em @p error e retorna `false`.
 *
 * @param[in]  argc       Quantidade de argumentos.
 * @param[in]  argv       Argumentos.
 * @param[out] options    Opções interpretadas.
 * @param[out] error      Mensagem de erro (pode ser NULL).
 * @param[in]  error_size Tamanho de @p error.
 * @return `true` se os argumentos são válidos.
 */
bool cli_parse(int argc, char **argv, CliOptions *options, char *error,
               size_t error_size);

/**
 * @brief Monta o caminho do arquivo `.class` a partir do alvo e do classpath.
 *
 * @details
 * **Algoritmo:**
 * 1. Se o alvo termina em `.class`, é um arquivo: com classpath informado,
 *    o caminho é `classpath/alvo`; sem classpath, o próprio alvo;
 * 2. Caso contrário é um nome de classe: os pontos do pacote viram `/` e o
 *    caminho é `classpath/Nome.class` (classpath padrão: diretório atual);
 * 3. Separadores `/` funcionam tanto no Linux quanto no Windows.
 *
 * @param[in]  options Opções da linha de comando (com @c target definido).
 * @param[out] buffer  Destino do caminho.
 * @param[in]  size    Tamanho do destino.
 * @return `true` em caso de sucesso; `false` se o caminho não cabe no buffer.
 */
bool cli_resolve_class_path(const CliOptions *options, char *buffer, size_t size);

/**
 * @brief Escreve a mensagem de ajuda.
 *
 * @details **Algoritmo:** escreve a sintaxe de cada modo e a lista de opções.
 *
 * @param[in] out          Fluxo de saída.
 * @param[in] program_name Nome do executável.
 */
void cli_print_usage(FILE *out, const char *program_name);

#endif /* JVM_CLI_H */
