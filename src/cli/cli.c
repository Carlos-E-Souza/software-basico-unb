/**
 * @file cli.c
 * @brief Implementação da interpretação da linha de comando.
 */
#include <stdio.h>
#include <string.h>

#include "cli/cli.h"

/**
 * @brief Registra uma mensagem de erro (se houver buffer) e retorna `false`.
 *
 * @details **Algoritmo:** formata com `snprintf` as duas partes da mensagem.
 *
 * @param[out] error      Buffer de erro (pode ser NULL).
 * @param[in]  error_size Tamanho do buffer.
 * @param[in]  message    Texto do erro.
 * @param[in]  detail     Detalhe (ex.: a opção problemática).
 * @return Sempre `false`.
 */
static bool fail(char *error, size_t error_size, const char *message,
                 const char *detail)
{
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s%s", message, detail);
    }
    return false;
}

/**
 * @brief Define o modo de operação, recusando um segundo modo diferente.
 *
 * @details **Algoritmo:** aceita se ainda não há modo ou se é o mesmo.
 *
 * @param[in,out] options Opções.
 * @param[in]     mode    Modo pedido.
 * @return `true` se o modo foi aceito.
 */
static bool set_mode(CliOptions *options, CliMode mode)
{
    if (options->mode != CLI_MODE_NONE && options->mode != mode) {
        return false;
    }
    options->mode = mode;
    return true;
}

bool cli_parse(int argc, char **argv, CliOptions *options, char *error,
               size_t error_size)
{
    int i;
    memset(options, 0, sizeof *options);

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        bool has_value = (i + 1 < argc);

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            options->mode = CLI_MODE_HELP;
            return true;
        }
        if (strcmp(arg, "--version") == 0) {
            options->mode = CLI_MODE_VERSION;
            return true;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--viewer") == 0) {
            if (!set_mode(options, CLI_MODE_VIEWER)) {
                return fail(error, error_size, "modo informado mais de uma vez: ", arg);
            }
        } else if (strcmp(arg, "-i") == 0 || strcmp(arg, "--interpreter") == 0) {
            if (!set_mode(options, CLI_MODE_INTERPRETER)) {
                return fail(error, error_size, "modo informado mais de uma vez: ", arg);
            }
        } else if (strcmp(arg, "--mode") == 0) {
            CliMode mode;
            if (!has_value) {
                return fail(error, error_size, "falta o valor de ", arg);
            }
            arg = argv[++i];
            if (strcmp(arg, "viewer") == 0) {
                mode = CLI_MODE_VIEWER;
            } else if (strcmp(arg, "interpreter") == 0) {
                mode = CLI_MODE_INTERPRETER;
            } else {
                return fail(error, error_size,
                            "modo invalido (use viewer ou interpreter): ", arg);
            }
            if (!set_mode(options, mode)) {
                return fail(error, error_size, "modo informado mais de uma vez: ", arg);
            }
        } else if (strcmp(arg, "-cp") == 0 || strcmp(arg, "--classpath") == 0) {
            if (!has_value) {
                return fail(error, error_size, "falta o diretorio de ", arg);
            }
            options->classpath = argv[++i];
        } else if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0) {
            if (!has_value) {
                return fail(error, error_size, "falta o arquivo de ", arg);
            }
            options->output = argv[++i];
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return fail(error, error_size, "opcao desconhecida: ", arg);
        } else if (options->target != NULL) {
            return fail(error, error_size, "mais de uma classe informada: ", arg);
        } else {
            options->target = arg;
        }
    }

    if (options->mode == CLI_MODE_NONE) {
        return fail(error, error_size,
                    "informe o modo: -v (exibidor) ou -i (interpretador)", "");
    }
    if (options->target == NULL) {
        return fail(error, error_size, "informe a classe ou o arquivo .class", "");
    }
    if (options->output != NULL && options->mode != CLI_MODE_VIEWER) {
        return fail(error, error_size, "--output so pode ser usado com o exibidor", "");
    }
    return true;
}

/**
 * @brief Indica se @p text termina com @p suffix.
 *
 * @details **Algoritmo:** compara os últimos `strlen(suffix)` caracteres.
 *
 * @param[in] text   Texto.
 * @param[in] suffix Sufixo.
 * @return `true` se @p text termina com @p suffix.
 */
static bool ends_with(const char *text, const char *suffix)
{
    size_t n = strlen(text), m = strlen(suffix);
    return n >= m && strcmp(text + n - m, suffix) == 0;
}

bool cli_resolve_class_path(const CliOptions *options, char *buffer, size_t size)
{
    const char *dir = options->classpath;
    const char *target = options->target;
    size_t dir_len, i, start;
    int n;

    if (ends_with(target, ".class")) {
        n = (dir == NULL) ? snprintf(buffer, size, "%s", target)
                          : snprintf(buffer, size, "%s/%s", dir, target);
        return n >= 0 && (size_t)n < size;
    }

    if (dir == NULL) {
        dir = ".";
    }
    n = snprintf(buffer, size, "%s/%s.class", dir, target);
    if (n < 0 || (size_t)n >= size) {
        return false;
    }
    /* pacote.Classe -> pacote/Classe (só na parte do nome da classe) */
    dir_len = strlen(dir);
    start = dir_len + 1;
    for (i = start; i + 6 < (size_t)n; i++) {
        if (buffer[i] == '.') {
            buffer[i] = '/';
        }
    }
    return true;
}

void cli_print_usage(FILE *out, const char *program_name)
{
    fprintf(out,
            "Uso:\n"
            "  %s -v [-cp <dir>] <Classe|arquivo.class> [-o <arquivo>]\n"
            "  %s -i [-cp <dir>] <ClassePrincipal>\n"
            "\n"
            "Opcoes:\n"
            "  -v, --viewer, --mode viewer           Leitor/exibidor de arquivos .class\n"
            "  -i, --interpreter, --mode interpreter Interpretador de bytecode\n"
            "  -cp, --classpath <dir>                Diretorio onde ficam os .class\n"
            "                                        (padrao: diretorio atual)\n"
            "  -o, --output <arquivo>                (exibidor) grava a saida em arquivo\n"
            "  -h, --help                            Mostra esta ajuda\n"
            "  --version                             Mostra a versao do programa\n"
            "\n"
            "Exemplos:\n"
            "  %s -v -cp out/java8 Exemplo\n"
            "  %s -v out/java8/Exemplo.class -o exemplo.txt\n"
            "  %s -i -cp out/java8 Exemplo\n",
            program_name, program_name, program_name, program_name, program_name);
}
