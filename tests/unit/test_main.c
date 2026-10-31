/**
 * @file test_main.c
 * @brief Executor dos testes unitários: registra e roda todas as suítes.
 */
#include "test.h"

int test_failures = 0;
int test_count = 0;
int test_current_failed = 0;

/* Suítes registradas (uma por arquivo test_*.c). */
void suite_version(void);
void suite_byte_reader(void);
void suite_class_file(void);
void suite_opcodes(void);
void suite_number_format(void);
void suite_viewer(void);
void suite_cli(void);

/**
 * @brief Executa todas as suítes e informa o resultado.
 *
 * @details
 * **Algoritmo:**
 * 1. Chama cada função de suíte registrada;
 * 2. Imprime o total de testes e de falhas;
 * 3. Retorna 0 se não houve falhas, 1 caso contrário.
 *
 * @return 0 se todos os testes passaram; 1 caso contrário.
 */
int main(void)
{
    printf("Suite: version\n");
    suite_version();
    printf("Suite: byte_reader\n");
    suite_byte_reader();
    printf("Suite: class_file\n");
    suite_class_file();
    printf("Suite: opcodes\n");
    suite_opcodes();
    printf("Suite: number_format\n");
    suite_number_format();
    printf("Suite: viewer\n");
    suite_viewer();
    printf("Suite: cli\n");
    suite_cli();

    printf("\n%d teste(s), %d falha(s)\n", test_count, test_failures);
    return test_failures == 0 ? 0 : 1;
}
