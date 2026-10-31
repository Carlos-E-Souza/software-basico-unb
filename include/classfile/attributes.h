/**
 * @file attributes.h
 * @brief Leitura, consulta e liberação de atributos (`attribute_info`).
 */
#ifndef JVM_ATTRIBUTES_H
#define JVM_ATTRIBUTES_H

#include <stdint.h>

#include "classfile/byte_reader.h"
#include "classfile/class_file.h"

/**
 * @brief Lê `attributes_count` seguido dos atributos.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê `attributes_count` e aloca o vetor zerado;
 * 2. Para cada atributo: lê `attribute_name_index` (deve ser Utf8) e
 *    `attribute_length`; cria um sub-leitor com exatamente esse tamanho;
 * 3. Identifica o tipo pelo nome e interpreta o conteúdo no sub-leitor;
 *    tipos desconhecidos são copiados como bytes brutos;
 * 4. Exige que o conteúdo consuma exatamente `attribute_length` bytes.
 *
 * @param[in,out] reader     Leitor posicionado em `attributes_count`.
 * @param[in]     class_file Classe (pool de constantes já lido e validado).
 * @param[in]     depth      Profundidade de aninhamento: 0 para classe/membros,
 *                           1 dentro de Code (um Code aninhado é tratado como desconhecido).
 * @param[out]    count      Quantidade de atributos lidos.
 * @param[out]    attributes Vetor alocado (liberar com attributes_free()).
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro. Em caso de erro, o que já foi lido
 *         continua em @p attributes / @p count e deve ser liberado.
 */
CfStatus attributes_parse(ByteReader *reader, const ClassFile *class_file,
                          unsigned depth, uint16_t *count,
                          AttributeInfo **attributes, ClassFileError *err);

/**
 * @brief Libera um vetor de atributos e todo o conteúdo interno.
 *
 * @details **Algoritmo:** para cada atributo, libera os vetores próprios do
 * seu tipo (Code libera recursivamente seus atributos aninhados) e, ao final,
 * o próprio vetor.
 *
 * @param[in] attributes Vetor de atributos (pode ser NULL).
 * @param[in] count      Quantidade de atributos no vetor.
 */
void attributes_free(AttributeInfo *attributes, uint16_t count);

/**
 * @brief Converte o nome de um atributo no tipo correspondente.
 *
 * @details **Algoritmo:** busca linear em uma tabela nome → tipo.
 *
 * @param[in] name Nome do atributo (ex.: "Code").
 * @return Tipo reconhecido ou #ATTR_UNKNOWN.
 */
AttributeKind attribute_kind_from_name(const char *name);

/**
 * @brief Retorna o primeiro atributo de um tipo em um vetor.
 *
 * @details **Algoritmo:** busca linear comparando @c kind.
 *
 * @param[in] attributes Vetor de atributos.
 * @param[in] count      Quantidade de atributos.
 * @param[in] kind       Tipo procurado.
 * @return Ponteiro para o atributo, ou NULL se não houver.
 */
const AttributeInfo *attribute_find(const AttributeInfo *attributes,
                                    uint16_t count, AttributeKind kind);

#endif /* JVM_ATTRIBUTES_H */
