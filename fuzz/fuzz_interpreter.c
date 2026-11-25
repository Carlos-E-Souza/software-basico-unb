/**
 * @file fuzz_interpreter.c
 * @brief Alvo do libFuzzer para o interpretador (atividade I-10).
 *
 * @details Cada entrada é tratada como o `.class` da classe principal. Só as
 * classes que o leitor aceita são executadas: a classe é registrada na VM
 * (class_define()) e o seu `main` é chamado com um limite de instruções,
 * para que laços infinitos não travem o fuzzer. Outras classes referenciadas
 * não existem no *classpath* (erro fatal tratado); as nativas funcionam.
 * O AddressSanitizer/UBSan detectam acessos inválidos, vazamentos e
 * comportamento indefinido.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "classfile/class_file.h"
#include "vm/class_loader.h"
#include "vm/interpreter.h"

/** @brief Limite de instruções por execução. */
#define FUZZ_MAX_STEPS 100000u

/** @brief Destino descartável de `System.out` (aberto uma única vez). */
static FILE *sink_file = NULL;

/**
 * @brief Ponto de entrada chamado pelo libFuzzer para cada entrada gerada.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê a entrada com class_file_parse(); se falhar, retorna;
 * 2. Inicializa a VM com *classpath* inexistente e saída descartável;
 * 3. Registra a classe e chama o `main` com limite de instruções;
 * 4. Libera tudo (vm_free()).
 *
 * @param[in] data Bytes gerados pelo fuzzer.
 * @param[in] size Quantidade de bytes.
 * @return Sempre 0 (convenção do libFuzzer).
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ClassFile *cf = NULL;
    RuntimeClass *main_class;
    Vm vm;

    if (class_file_parse(data, size, &cf, NULL) != CF_OK) {
        return 0;
    }
    if (sink_file == NULL) {
        sink_file = fopen("/dev/null", "w");
    }
    vm_init(&vm, "build/fuzz/sem-classes", sink_file != NULL ? sink_file : stdout);
    vm.max_steps = FUZZ_MAX_STEPS;
    vm.quiet = true;
    main_class = class_define(&vm, cf);
    if (main_class != NULL) {
        vm_call_main(&vm, main_class);
    }
    vm_free(&vm);
    return 0;
}
