/**
 * @file byte_reader.h
 * @brief Leitor sequencial de bytes em big-endian com verificação de limites.
 *
 * @details O formato `.class` armazena todos os inteiros em big-endian
 * (byte mais significativo primeiro). Este módulo lê `u1`, `u2` e `u4`
 * byte a byte, de forma independente da arquitetura, e nunca acessa memória
 * fora do buffer: toda leitura que ultrapassaria o fim retorna `false` sem
 * avançar o cursor.
 */
#ifndef JVM_BYTE_READER_H
#define JVM_BYTE_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Cursor de leitura sobre um buffer de bytes (não é dono do buffer).
 */
typedef struct ByteReader {
    const uint8_t *data; /**< Início do buffer lido. */
    size_t size;         /**< Quantidade de bytes do buffer. */
    size_t pos;          /**< Posição atual de leitura (relativa a @c data). */
    size_t base;         /**< Deslocamento absoluto de @c data[0] no arquivo (para mensagens de erro). */
} ByteReader;

/**
 * @brief Inicializa um leitor sobre um buffer.
 *
 * @details
 * **Algoritmo:** grava o ponteiro e o tamanho do buffer e zera a posição e o
 * deslocamento base.
 *
 * @param[out] reader Leitor a ser inicializado.
 * @param[in]  data   Buffer de bytes (pode ser NULL se @p size for 0).
 * @param[in]  size   Tamanho do buffer em bytes.
 */
void br_init(ByteReader *reader, const uint8_t *data, size_t size);

/**
 * @brief Lê um byte sem sinal (`u1`).
 *
 * @details
 * **Algoritmo:**
 * 1. Verifica se resta ao menos 1 byte;
 * 2. Copia o byte da posição atual e avança 1 posição.
 *
 * @param[in,out] reader Leitor de bytes.
 * @param[out]    out    Valor lido (inalterado em caso de falha).
 * @return `true` em caso de sucesso; `false` se o buffer acabou.
 */
bool br_read_u1(ByteReader *reader, uint8_t *out);

/**
 * @brief Lê um inteiro sem sinal de 16 bits em big-endian (`u2`).
 *
 * @details
 * **Algoritmo:**
 * 1. Verifica se restam ao menos 2 bytes;
 * 2. Combina os bytes como `(b0 << 8) | b1`;
 * 3. Avança 2 posições.
 *
 * @param[in,out] reader Leitor de bytes.
 * @param[out]    out    Valor lido (inalterado em caso de falha).
 * @return `true` em caso de sucesso; `false` se o buffer acabou.
 */
bool br_read_u2(ByteReader *reader, uint16_t *out);

/**
 * @brief Lê um inteiro sem sinal de 32 bits em big-endian (`u4`).
 *
 * @details
 * **Algoritmo:**
 * 1. Verifica se restam ao menos 4 bytes;
 * 2. Combina os bytes como `(b0 << 24) | (b1 << 16) | (b2 << 8) | b3`;
 * 3. Avança 4 posições.
 *
 * @param[in,out] reader Leitor de bytes.
 * @param[out]    out    Valor lido (inalterado em caso de falha).
 * @return `true` em caso de sucesso; `false` se o buffer acabou.
 */
bool br_read_u4(ByteReader *reader, uint32_t *out);

/**
 * @brief Obtém uma visão de @p count bytes a partir da posição atual, sem copiar.
 *
 * @details
 * **Algoritmo:**
 * 1. Verifica se restam ao menos @p count bytes;
 * 2. Retorna um ponteiro para a posição atual e avança @p count posições.
 *
 * @param[in,out] reader Leitor de bytes.
 * @param[in]     count  Quantidade de bytes desejada.
 * @param[out]    out    Ponteiro para os bytes dentro do buffer original
 *                       (válido enquanto o buffer existir).
 * @return `true` em caso de sucesso; `false` se não há bytes suficientes.
 */
bool br_read_bytes(ByteReader *reader, size_t count, const uint8_t **out);

/**
 * @brief Cria um sub-leitor restrito aos próximos @p count bytes e avança o leitor original.
 *
 * @details Usado para atributos: o sub-leitor garante que o atributo não
 * leia além de `attribute_length`, e permite verificar se ele consumiu
 * exatamente esse tamanho.
 *
 * **Algoritmo:**
 * 1. Verifica se restam ao menos @p count bytes;
 * 2. Inicializa @p sub apontando para a posição atual, com tamanho @p count
 *    e deslocamento base absoluto correspondente;
 * 3. Avança o leitor original @p count posições.
 *
 * @param[in,out] reader Leitor de bytes original.
 * @param[in]     count  Tamanho do sub-leitor.
 * @param[out]    sub    Sub-leitor criado.
 * @return `true` em caso de sucesso; `false` se não há bytes suficientes.
 */
bool br_sub_reader(ByteReader *reader, size_t count, ByteReader *sub);

/**
 * @brief Informa quantos bytes ainda podem ser lidos.
 *
 * @details **Algoritmo:** retorna `size - pos`.
 *
 * @param[in] reader Leitor de bytes.
 * @return Quantidade de bytes restantes.
 */
size_t br_remaining(const ByteReader *reader);

/**
 * @brief Informa a posição absoluta atual no arquivo (para mensagens de erro).
 *
 * @details **Algoritmo:** retorna `base + pos`.
 *
 * @param[in] reader Leitor de bytes.
 * @return Deslocamento absoluto, em bytes, desde o início do arquivo.
 */
size_t br_offset(const ByteReader *reader);

#endif /* JVM_BYTE_READER_H */
