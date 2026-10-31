/**
 * @file fuzz_reader.c
 * @brief Alvo do libFuzzer para o leitor e o exibidor de `.class`
 *        (atividades L-09 e E-05).
 *
 * @details Cada entrada gerada pelo fuzzer é tratada como um arquivo `.class`.
 * Se a leitura tiver sucesso, todas as funções de consulta do pool são
 * exercitadas, a classe inteira é escrita pelo exibidor (incluindo a
 * desmontagem de bytecode arbitrário) em um arquivo descartável, e a
 * estrutura é liberada. O AddressSanitizer/UBSan detectam qualquer acesso inválido,
 * vazamento ou comportamento indefinido.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "classfile/attributes.h"
#include "classfile/class_file.h"
#include "classfile/constant_pool.h"
#include "common/mutf8.h"
#include "viewer/viewer.h"

/** @brief Destino descartável da saída do exibidor (aberto uma única vez). */
static FILE *sink_file = NULL;

/**
 * @brief Percorre todas as entradas do pool chamando as funções de consulta.
 *
 * @details **Algoritmo:** para cada índice (inclusive inválidos 0 e
 * `constant_pool_count`), chama todas as funções `cp_get_*`; para Utf8,
 * também decodifica para UTF-16.
 *
 * @param[in] cf Classe lida com sucesso.
 * @return Valor acumulado (evita que o compilador elimine as chamadas).
 */
static unsigned exercise_constant_pool(const ClassFile *cf)
{
    unsigned acc = 0;
    uint32_t i;
    for (i = 0; i <= cf->constant_pool_count; i++) {
        uint16_t index = (uint16_t)i;
        const char *a, *b, *c;
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        acc += (unsigned)cp_tag(cf, index);
        acc += cp_tag_name(cp_tag(cf, index))[0];
        if ((a = cp_get_utf8(cf, index)) != NULL) {
            const CpUtf8 *u = &cf->constant_pool[index].info.utf8;
            acc += (unsigned)mutf8_to_utf16((const uint8_t *)u->bytes, u->length, NULL);
        }
        acc += cp_get_class_name(cf, index) != NULL;
        acc += cp_get_string(cf, index) != NULL;
        acc += cp_get_name_and_type(cf, index, &a, &b);
        acc += cp_get_ref(cf, index, &a, &b, &c);
        acc += cp_get_int(cf, index, &i32);
        acc += cp_get_float(cf, index, &f32);
        acc += cp_get_long(cf, index, &i64);
        acc += cp_get_double(cf, index, &f64);
    }
    return acc;
}

/**
 * @brief Função chamada pelo libFuzzer para cada entrada gerada.
 *
 * @details
 * **Algoritmo:**
 * 1. Chama class_file_parse() sobre os bytes;
 * 2. Em caso de sucesso, exercita o pool, procura o atributo Code de cada
 *    método e escreve a classe com viewer_print_class() em `/dev/null`;
 * 3. Libera a estrutura.
 *
 * @param[in] data Bytes gerados pelo fuzzer.
 * @param[in] size Quantidade de bytes em @p data.
 * @return Sempre 0 (convenção do libFuzzer).
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ClassFile *cf = NULL;
    ClassFileError err;
    volatile unsigned sink = 0;
    uint16_t i;

    if (class_file_parse(data, size, &cf, &err) == CF_OK) {
        sink += exercise_constant_pool(cf);
        for (i = 0; i < cf->methods_count; i++) {
            const MethodInfo *m = &cf->methods[i];
            sink += attribute_find(m->attributes, m->attributes_count, ATTR_CODE) != NULL;
        }
        sink += class_file_java_version_name(cf->major_version)[0];
        if (sink_file == NULL) {
            sink_file = fopen("/dev/null", "w");
        }
        if (sink_file != NULL) {
            viewer_print_class(sink_file, cf, "fuzz");
        }
    }
    sink += cf_status_name(err.status)[0];
    class_file_free(cf);
    (void)sink;
    return 0;
}
