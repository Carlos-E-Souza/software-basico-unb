/**
 * @file file_io.h
 * @brief Leitura de arquivos inteiros para a memória (portável, C99).
 */
#ifndef JVM_FILE_IO_H
#define JVM_FILE_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Lê todo o conteúdo de um arquivo binário para um buffer alocado.
 *
 * @details
 * **Algoritmo:**
 * 1. Abre o arquivo em modo binário (`"rb"`, necessário no Windows);
 * 2. Descobre o tamanho com `fseek`/`ftell`;
 * 3. Aloca um buffer desse tamanho (mínimo 1 byte) e lê o conteúdo;
 * 4. Fecha o arquivo; em qualquer falha libera o buffer e retorna `false`.
 *
 * @param[in]  path Caminho do arquivo.
 * @param[out] data Buffer alocado com o conteúdo (liberar com `free`).
 * @param[out] size Tamanho do conteúdo em bytes.
 * @return `true` em caso de sucesso; `false` se o arquivo não pôde ser lido.
 */
bool file_read_all(const char *path, uint8_t **data, size_t *size);

#endif /* JVM_FILE_IO_H */
