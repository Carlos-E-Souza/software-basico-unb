/**
 * @file number_format.c
 * @brief Implementação da formatação de números de ponto flutuante no estilo Java.
 */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/number_format.h"

/**
 * @brief Obtém os dígitos significativos mínimos e o expoente decimal de um valor finito e não nulo.
 *
 * @details
 * **Algoritmo:**
 * 1. Para p = 1 até @p max_digits, formata |valor| com `%.{p-1}e` e converte
 *    de volta; para no primeiro p que reproduz o valor (em precisão simples
 *    se @p is_float);
 * 2. Copia os dígitos (ignorando o ponto) até o 'e' e lê o expoente;
 * 3. Remove zeros à direita dos dígitos (mantendo ao menos um).
 *
 * Como no Java, valores subnormais usam ao menos 2 dígitos
 * (ex.: `Double.MIN_VALUE` é `4.9E-324`, não `5.0E-324`).
 *
 * @param[in]  magnitude  Valor absoluto (finito, maior que zero).
 * @param[in]  max_digits 9 para float, 17 para double.
 * @param[in]  is_float   Se a comparação de ida e volta é em `float`.
 * @param[out] digits     Dígitos significativos (até 17 + '\\0').
 * @param[out] exponent   Expoente decimal do primeiro dígito.
 */
static void shortest_digits(double magnitude, int max_digits, int is_float,
                            char digits[20], int *exponent)
{
    char text[40];
    const char *p;
    size_t n = 0;
    int precision;
    int subnormal = is_float ? (magnitude < FLT_MIN) : (magnitude < DBL_MIN);

    for (precision = subnormal ? 2 : 1; precision <= max_digits; precision++) {
        double back;
        snprintf(text, sizeof text, "%.*e", precision - 1, magnitude);
        back = strtod(text, NULL);
        if (is_float ? ((float)back == (float)magnitude) : (back == magnitude)) {
            break;
        }
    }
    for (p = text; *p != '\0' && *p != 'e' && n < 19; p++) {
        if (*p >= '0' && *p <= '9') {
            digits[n++] = *p;
        }
    }
    while (n > 1 && digits[n - 1] == '0') {
        n--;
    }
    digits[n] = '\0';
    *exponent = (*p == 'e') ? atoi(p + 1) : 0;
}

/**
 * @brief Monta o texto final a partir de sinal, dígitos e expoente.
 *
 * @details
 * **Algoritmo:**
 * - se 10⁻³ ≤ |x| < 10⁷ (expoente de −3 a 6): notação decimal; com
 *   expoente ≥ 0 os primeiros (expoente + 1) dígitos formam a parte inteira
 *   (completada com zeros); com expoente < 0 escreve `0.` seguido de zeros e
 *   dígitos; a parte fracionária vazia vira `0`;
 * - caso contrário: `d.ddd` + `E` + expoente.
 *
 * @param[out] buffer   Destino.
 * @param[in]  size     Tamanho do destino.
 * @param[in]  negative Se o valor é negativo.
 * @param[in]  digits   Dígitos significativos.
 * @param[in]  exponent Expoente decimal do primeiro dígito.
 */
static void build_text(char *buffer, size_t size, int negative,
                       const char *digits, int exponent)
{
    char out[NUMBER_FORMAT_SIZE + 16];
    size_t len = strlen(digits), pos = 0;
    int i;

    if (negative) {
        out[pos++] = '-';
    }
    if (exponent >= -3 && exponent < 7) {
        if (exponent >= 0) {
            for (i = 0; i <= exponent; i++) {
                out[pos++] = (size_t)i < len ? digits[i] : '0';
            }
            out[pos++] = '.';
            if ((size_t)exponent + 1 < len) {
                for (i = exponent + 1; (size_t)i < len; i++) {
                    out[pos++] = digits[i];
                }
            } else {
                out[pos++] = '0';
            }
        } else {
            out[pos++] = '0';
            out[pos++] = '.';
            for (i = 0; i < -exponent - 1; i++) {
                out[pos++] = '0';
            }
            for (i = 0; (size_t)i < len; i++) {
                out[pos++] = digits[i];
            }
        }
        out[pos] = '\0';
    } else {
        out[pos++] = digits[0];
        out[pos++] = '.';
        if (len > 1) {
            for (i = 1; (size_t)i < len; i++) {
                out[pos++] = digits[i];
            }
        } else {
            out[pos++] = '0';
        }
        snprintf(out + pos, sizeof out - pos, "E%d", exponent);
    }
    snprintf(buffer, size, "%s", out);
}

/**
 * @brief Implementação comum de java_format_double() e java_format_float().
 *
 * @details **Algoritmo:** trata os casos especiais e combina
 * shortest_digits() com build_text().
 *
 * @param[out] buffer     Destino.
 * @param[in]  size       Tamanho do destino.
 * @param[in]  value      Valor (float promovido a double sem perda).
 * @param[in]  max_digits 9 para float, 17 para double.
 * @param[in]  is_float   Se a comparação é em precisão simples.
 */
static void format_java(char *buffer, size_t size, double value, int max_digits,
                        int is_float)
{
    char digits[20];
    int exponent;
    if (isnan(value)) {
        snprintf(buffer, size, "NaN");
    } else if (isinf(value)) {
        snprintf(buffer, size, value > 0 ? "Infinity" : "-Infinity");
    } else if (value == 0.0) {
        snprintf(buffer, size, signbit(value) ? "-0.0" : "0.0");
    } else {
        shortest_digits(fabs(value), max_digits, is_float, digits, &exponent);
        build_text(buffer, size, value < 0, digits, exponent);
    }
}

void java_format_double(char *buffer, size_t size, double value)
{
    format_java(buffer, size, value, 17, 0);
}

void java_format_float(char *buffer, size_t size, float value)
{
    format_java(buffer, size, (double)value, 9, 1);
}
