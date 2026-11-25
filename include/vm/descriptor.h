/**
 * @file descriptor.h
 * @brief Leitura de descritores de campos e métodos (JVMS §4.3), usada para
 *        contar argumentos e tipos de retorno nas invocações (I-05, D6).
 */
#ifndef JVM_DESCRIPTOR_H
#define JVM_DESCRIPTOR_H

#include <stdbool.h>

/**
 * @brief Avança sobre um tipo de campo (`I`, `[[J`, `Lpkg/C;`...).
 *
 * @details **Algoritmo:** pula os `[`; se o caractere for `L`, avança até
 * o `;`; se for um tipo primitivo, avança um caractere.
 *
 * @param[in] p Início do tipo.
 * @return Posição logo após o tipo; NULL se o tipo é inválido.
 */
const char *descriptor_skip_type(const char *p);

/**
 * @brief Conta os slots dos parâmetros de um descritor de método.
 *
 * @details **Algoritmo:** percorre os tipos entre `(` e `)` somando 2 para
 * `J`/`D` e 1 para os demais.
 *
 * @param[in] descriptor Descritor do método (ex.: `(IJLjava/lang/String;)V`).
 * @return Quantidade de slots; -1 se o descritor é inválido.
 */
int descriptor_arg_slots(const char *descriptor);

/**
 * @brief Devolve o tipo de retorno de um descritor de método.
 *
 * @details **Algoritmo:** procura o `)` e devolve o ponteiro seguinte.
 *
 * @param[in] descriptor Descritor do método.
 * @return Tipo de retorno (ex.: `V`, `I`, `Ljava/lang/String;`); "V" se inválido.
 */
const char *descriptor_return(const char *descriptor);

/**
 * @brief Quantidade de slots de um tipo de campo.
 *
 * @details **Algoritmo:** `J`/`D` = 2, `V` = 0, demais = 1.
 *
 * @param[in] type Tipo (primeiro caractere é suficiente).
 * @return 0, 1 ou 2.
 */
int descriptor_type_slots(const char *type);

/**
 * @brief Indica se um tipo de campo é referência (`L...;` ou array).
 * @details **Algoritmo:** testa o primeiro caractere (`L` ou `[`).
 * @param[in] type Tipo.
 * @return `true` para referências.
 */
bool descriptor_is_ref(const char *type);

#endif /* JVM_DESCRIPTOR_H */
