/**
 * @file mutf8.h
 * @brief Validação e decodificação de UTF-8 modificado (MUTF-8) da JVM.
 *
 * @details As strings do pool de constantes (`CONSTANT_Utf8`) usam o
 * "UTF-8 modificado" da especificação Java (JVMS §4.4.7):
 * - o caractere U+0000 é codificado em dois bytes (`C0 80`), logo nunca há
 *   byte zero na string;
 * - caracteres fora do plano básico são codificados como dois *surrogates*
 *   UTF-16 de 3 bytes cada (não há sequências de 4 bytes).
 *
 * Como não existe byte zero, os bytes brutos acrescidos de '\\0' formam uma
 * string C válida, idêntica ao texto para nomes ASCII.
 */
#ifndef JVM_MUTF8_H
#define JVM_MUTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Verifica se uma sequência de bytes é MUTF-8 bem formado.
 *
 * @details
 * **Algoritmo:** percorre os bytes classificando o byte inicial:
 * 1. `01–7F`: caractere de 1 byte;
 * 2. `C0–DF`: exige 1 byte de continuação (`10xxxxxx`);
 * 3. `E0–EF`: exige 2 bytes de continuação;
 * 4. `00`, `80–BF` (continuação solta) e `F0–FF`: inválidos.
 *
 * @param[in] bytes  Bytes a validar.
 * @param[in] length Quantidade de bytes.
 * @return `true` se a sequência é válida; `false` caso contrário.
 */
bool mutf8_is_valid(const uint8_t *bytes, size_t length);

/**
 * @brief Decodifica MUTF-8 para unidades UTF-16 (o tipo `char` do Java).
 *
 * @details A sequência deve ter sido validada por mutf8_is_valid().
 * Se @p out for NULL, apenas conta as unidades (útil para alocar o vetor).
 *
 * **Algoritmo:** para cada caractere, junta os bits úteis do byte inicial
 * e dos bytes de continuação (`x & 0x1F`/`0x0F` e `x & 0x3F`), gerando uma
 * unidade UTF-16 por sequência.
 *
 * @param[in]  bytes  Bytes MUTF-8 válidos.
 * @param[in]  length Quantidade de bytes.
 * @param[out] out    Vetor de destino com espaço suficiente, ou NULL.
 * @return Quantidade de unidades UTF-16 da string.
 */
size_t mutf8_to_utf16(const uint8_t *bytes, size_t length, uint16_t *out);

#endif /* JVM_MUTF8_H */
