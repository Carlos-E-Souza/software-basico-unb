/**
 * @file main.c
 * @brief Ponto de entrada da JVM didática.
 *
 * @details Nesta fase (setup do ambiente) o programa reconhece apenas as
 * opções `--help` e `--version`. Os modos leitor/exibidor e interpretador
 * serão conectados aqui nas etapas 3 e 4 do plano (`docs/PLANO.md`).
 */
#include <stdio.h>
#include <string.h>

#include "version.h"

/** @brief Código de saída: execução bem-sucedida. */
#define EXIT_CODE_OK 0
/** @brief Código de saída: argumentos de linha de comando inválidos. */
#define EXIT_CODE_USAGE 1
/** @brief Código de saída: funcionalidade ainda não implementada. */
#define EXIT_CODE_NOT_IMPLEMENTED 2

/**
 * @brief Imprime a mensagem de ajuda com a sintaxe da linha de comando.
 *
 * @details
 * **Algoritmo:**
 * 1. Escreve no fluxo informado a sintaxe de uso de cada modo;
 * 2. Escreve a lista de opções disponíveis.
 *
 * @param[in] out          Fluxo de saída onde a ajuda será escrita (ex.: `stdout`).
 * @param[in] program_name Nome do executável, normalmente `argv[0]`.
 */
static void print_usage(FILE *out, const char *program_name)
{
    fprintf(out,
            "Uso:\n"
            "  %s --mode viewer      --classpath <dir> <Classe|arquivo.class> [--output <arquivo>]\n"
            "  %s --mode interpreter --classpath <dir> <ClassePrincipal>\n"
            "\n"
            "Opcoes:\n"
            "  -v, --mode viewer       Leitor/exibidor de arquivos .class\n"
            "  -i, --mode interpreter  Interpretador de bytecode\n"
            "  -cp, --classpath <dir>  Diretorio onde ficam os arquivos .class\n"
            "  --output <arquivo>      (exibidor) grava a saida em arquivo\n"
            "  -h, --help              Mostra esta ajuda\n"
            "  --version               Mostra a versao do programa\n",
            program_name, program_name);
}

/**
 * @brief Função principal: interpreta os argumentos e despacha para o modo escolhido.
 *
 * @details
 * **Algoritmo:**
 * 1. Sem argumentos, imprime a ajuda em `stderr` e retorna erro de uso;
 * 2. Com `-h`/`--help`, imprime a ajuda em `stdout` e retorna sucesso;
 * 3. Com `--version`, imprime a versão e retorna sucesso;
 * 4. Qualquer outro argumento ainda não é suportado nesta fase do projeto.
 *
 * @param[in] argc Quantidade de argumentos da linha de comando.
 * @param[in] argv Vetor com os argumentos da linha de comando.
 * @return #EXIT_CODE_OK, #EXIT_CODE_USAGE ou #EXIT_CODE_NOT_IMPLEMENTED.
 */
int main(int argc, char **argv)
{
    const char *program_name = (argc > 0 && argv[0] != NULL) ? argv[0] : "jvm";

    if (argc < 2) {
        print_usage(stderr, program_name);
        return EXIT_CODE_USAGE;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(stdout, program_name);
        return EXIT_CODE_OK;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("jvm %s\n", jvm_version_string());
        return EXIT_CODE_OK;
    }

    fprintf(stderr, "Erro: opcao '%s' ainda nao implementada.\n", argv[1]);
    return EXIT_CODE_NOT_IMPLEMENTED;
}
