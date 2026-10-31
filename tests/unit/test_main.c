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

    printf("\n%d teste(s), %d falha(s)\n", test_count, test_failures);
    return test_failures == 0 ? 0 : 1;
}
