/**
 * @file number_format.h
 * @brief Conversão de `float`/`double` para texto no formato do Java
 *        (`Float.toString` / `Double.toString`).
 *
 * @details Usada pelo exibidor (constantes do pool) e pelo interpretador
 * (simulação de `System.out.println`). Regras do Java:
 * - `NaN`, `Infinity`, `-Infinity`, `0.0` e `-0.0`;
 * - se 10⁻³ ≤ |x| < 10⁷: notação decimal com ao menos um dígito após o
 *   ponto (ex.: `100.0`, `0.001`, `3.14`);
 * - caso contrário: notação científica `d.dddE±n` (ex.: `1.0E10`, `1.5E-5`);
 * - usa a menor quantidade de dígitos que identifica o valor exatamente.
 */
#ifndef JVM_NUMBER_FORMAT_H
#define JVM_NUMBER_FORMAT_H

#include <stddef.h>

/** @brief Tamanho de buffer suficiente para qualquer número formatado. */
#define NUMBER_FORMAT_SIZE 40

/**
 * @brief Formata um `double` como `Double.toString` do Java.
 *
 * @details
 * **Algoritmo:**
 * 1. Trata NaN, infinitos e zeros (com sinal);
 * 2. Procura a menor quantidade de dígitos significativos p (1 a 17) tal que
 *    `%.{p-1}e` convertido de volta com `strtod` resulte no mesmo valor;
 * 3. Extrai desse texto os dígitos e o expoente decimal;
 * 4. Monta a notação decimal ou científica conforme a faixa do valor.
 *
 * @param[out] buffer Destino (ao menos #NUMBER_FORMAT_SIZE bytes).
 * @param[in]  size   Tamanho do destino.
 * @param[in]  value  Valor a formatar.
 */
void java_format_double(char *buffer, size_t size, double value);

/**
 * @brief Formata um `float` como `Float.toString` do Java.
 *
 * @details **Algoritmo:** igual a java_format_double(), mas a comparação de
 * ida e volta é feita em precisão simples (1 a 9 dígitos).
 *
 * @param[out] buffer Destino (ao menos #NUMBER_FORMAT_SIZE bytes).
 * @param[in]  size   Tamanho do destino.
 * @param[in]  value  Valor a formatar.
 */
void java_format_float(char *buffer, size_t size, float value);

#endif /* JVM_NUMBER_FORMAT_H */
