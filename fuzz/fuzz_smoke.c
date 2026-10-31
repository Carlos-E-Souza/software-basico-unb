/**
 * @file fuzz_smoke.c
 * @brief Alvo de fuzzing de verificação do ambiente (atividade S-03).
 *
 * @details Contém um bug PROPOSITAL (escrita fora dos limites de um vetor)
 * que só ocorre quando a entrada começa com os bytes "JVM!". Serve apenas
 * para comprovar que o libFuzzer + AddressSanitizer estão funcionando:
 * `make fuzz-smoke` só tem sucesso se o fuzzer ENCONTRAR esse bug.
 */
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Função chamada pelo libFuzzer para cada entrada gerada.
 *
 * @details
 * **Algoritmo:**
 * 1. Se a entrada começar com "JVM!", escreve em um índice inválido do vetor
 *    local (bug proposital detectado pelo AddressSanitizer);
 * 2. Caso contrário, não faz nada.
 *
 * @param[in] data Bytes gerados pelo fuzzer.
 * @param[in] size Quantidade de bytes em `data`.
 * @return Sempre 0 (convenção do libFuzzer).
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    volatile char buffer[4];
    if (size >= 4 && data[0] == 'J' && data[1] == 'V' && data[2] == 'M' &&
        data[3] == '!') {
        size_t index = size + 4; /* sempre >= 8: fora dos limites */
        buffer[index] = 1;       /* BUG PROPOSITAL */
    }
    (void)buffer;
    return 0;
}
