/**
 * @file main.c
 * @brief Ponto de entrada da JVM: interpreta a linha de comando e
 *        executa o modo escolhido (exibidor ou interpretador).
 */
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "classfile/class_file.h"
#include "cli/cli.h"
#include "version.h"
#include "viewer/viewer.h"

/** @brief Tamanho máximo do caminho de um arquivo `.class`. */
#define PATH_BUFFER_SIZE 4096

/**
 * @brief Configura o terminal do Windows para exibir UTF-8 corretamente.
 *
 * @details **Algoritmo:** no Windows, troca a página de código de saída do
 * console para UTF-8 (65001); nos demais sistemas não faz nada.
 */
static void setup_console(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

/**
 * @brief Executa o modo leitor/exibidor.
 *
 * @details
 * **Algoritmo:**
 * 1. Monta o caminho do `.class` (cli_resolve_class_path());
 * 2. Lê a classe (class_file_load()); em caso de erro, informa a mensagem e
 *    a posição no arquivo;
 * 3. Abre o arquivo de saída, se `--output` foi informado;
 * 4. Escreve as estruturas (viewer_print_class()) e libera a memória.
 *
 * @param[in] options Opções da linha de comando.
 * @return #EXIT_CODE_OK, #EXIT_CODE_USAGE ou #EXIT_CODE_CLASS_ERROR.
 */
static int run_viewer(const CliOptions *options)
{
    char path[PATH_BUFFER_SIZE];
    ClassFile *class_file = NULL;
    ClassFileError err;
    FILE *out = stdout;
    int status = EXIT_CODE_OK;

    if (!cli_resolve_class_path(options, path, sizeof path)) {
        fprintf(stderr, "Erro: caminho da classe muito longo.\n");
        return EXIT_CODE_USAGE;
    }
    if (class_file_load(path, &class_file, &err) != CF_OK) {
        fprintf(stderr, "Erro ao ler '%s': %s (%s, byte %lu)\n", path, err.message,
                cf_status_name(err.status), (unsigned long)err.offset);
        return EXIT_CODE_CLASS_ERROR;
    }
    if (options->output != NULL) {
        out = fopen(options->output, "w");
        if (out == NULL) {
            fprintf(stderr, "Erro: nao foi possivel criar '%s'.\n", options->output);
            class_file_free(class_file);
            return EXIT_CODE_CLASS_ERROR;
        }
    }

    viewer_print_class(out, class_file, path);

    if (ferror(out)) {
        fprintf(stderr, "Erro ao escrever a saida.\n");
        status = EXIT_CODE_CLASS_ERROR;
    }
    if (out != stdout && fclose(out) != 0) {
        status = EXIT_CODE_CLASS_ERROR;
    }
    class_file_free(class_file);
    return status;
}

/**
 * @brief Função principal: interpreta os argumentos e despacha para o modo escolhido.
 *
 * @details
 * **Algoritmo:**
 * 1. Configura o console (UTF-8 no Windows);
 * 2. Interpreta os argumentos (cli_parse()); em caso de erro, mostra a
 *    mensagem e a ajuda e retorna #EXIT_CODE_USAGE;
 * 3. Despacha: ajuda, versão, exibidor (run_viewer()) ou interpretador
 *    (ainda não implementado nesta etapa).
 *
 * @param[in] argc Quantidade de argumentos da linha de comando.
 * @param[in] argv Vetor com os argumentos da linha de comando.
 * @return Código de saída (ver `cli.h`).
 */
int main(int argc, char **argv)
{
    const char *program_name = (argc > 0 && argv[0] != NULL) ? argv[0] : "jvm";
    char error[CLI_ERROR_SIZE];
    CliOptions options;

    setup_console();

    if (!cli_parse(argc, argv, &options, error, sizeof error)) {
        fprintf(stderr, "Erro: %s\n\n", error);
        cli_print_usage(stderr, program_name);
        return EXIT_CODE_USAGE;
    }

    switch (options.mode) {
    case CLI_MODE_HELP:
        cli_print_usage(stdout, program_name);
        return EXIT_CODE_OK;
    case CLI_MODE_VERSION:
        printf("jvm %s\n", jvm_version_string());
        return EXIT_CODE_OK;
    case CLI_MODE_VIEWER:
        return run_viewer(&options);
    case CLI_MODE_INTERPRETER:
        fprintf(stderr, "Erro: o interpretador ainda nao foi implementado.\n");
        return EXIT_CODE_NOT_IMPLEMENTED;
    case CLI_MODE_NONE:
        break;
    }
    cli_print_usage(stderr, program_name);
    return EXIT_CODE_USAGE;
}
