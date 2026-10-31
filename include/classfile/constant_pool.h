/**
 * @file constant_pool.h
 * @brief Leitura, validação e consulta do pool de constantes.
 *
 * @details As funções de consulta (`cp_get_*`) são seguras: recebem qualquer
 * índice e retornam NULL/`false` se o índice for inválido ou apontar para
 * uma entrada de tipo diferente do esperado.
 */
#ifndef JVM_CONSTANT_POOL_H
#define JVM_CONSTANT_POOL_H

#include <stdbool.h>
#include <stdint.h>

#include "classfile/byte_reader.h"
#include "classfile/class_file.h"

/**
 * @brief Lê `constant_pool_count` e todas as entradas do pool.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê `constant_pool_count` (deve ser ≥ 1) e aloca o vetor zerado;
 * 2. Para cada índice de 1 a `count - 1`: lê a tag e o conteúdo conforme a
 *    tag; strings Utf8 são validadas como MUTF-8 e copiadas com '\\0' final;
 * 3. `CONSTANT_Long`/`CONSTANT_Double` consomem duas posições (a segunda fica
 *    #CONSTANT_Unusable);
 * 4. Tag desconhecida ou dado truncado gera erro.
 *
 * @param[in,out] reader     Leitor posicionado em `constant_pool_count`.
 * @param[in,out] class_file Estrutura onde o pool será gravado.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus cp_parse(ByteReader *reader, ClassFile *class_file, ClassFileError *err);

/**
 * @brief Verifica se todas as referências internas do pool apontam para entradas do tipo correto.
 *
 * @details
 * **Algoritmo:** para cada entrada, confere com cp_check() os índices que
 * ela contém (ex.: `Class.name_index` → Utf8; `Methodref.class_index` →
 * Class; `MethodHandle.reference_kind` entre 1 e 9 etc.).
 *
 * @param[in]  class_file Classe com o pool já lido.
 * @param[out] err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou #CF_ERR_BAD_INDEX / #CF_ERR_BAD_CONSTANT.
 */
CfStatus cp_validate(const ClassFile *class_file, ClassFileError *err);

/**
 * @brief Verifica se @p index é um índice válido do pool com a tag @p tag.
 *
 * @details **Algoritmo:** testa `1 <= index < constant_pool_count` e compara a tag.
 *
 * @param[in] class_file Classe.
 * @param[in] index      Índice a verificar.
 * @param[in] tag        Tag esperada.
 * @return `true` se o índice é válido e tem a tag esperada.
 */
bool cp_check(const ClassFile *class_file, uint16_t index, ConstantTag tag);

/**
 * @brief Retorna a tag da entrada @p index, ou #CONSTANT_Unusable se o índice é inválido.
 *
 * @details **Algoritmo:** verifica os limites e lê o campo @c tag.
 *
 * @param[in] class_file Classe.
 * @param[in] index      Índice no pool.
 * @return Tag da entrada.
 */
ConstantTag cp_tag(const ClassFile *class_file, uint16_t index);

/**
 * @brief Retorna o nome textual de uma tag (ex.: "Methodref").
 *
 * @details **Algoritmo:** `switch` sobre a tag.
 *
 * @param[in] tag Tag do pool.
 * @return Nome da tag, ou "Desconhecida".
 */
const char *cp_tag_name(ConstantTag tag);

/**
 * @brief Retorna o texto de uma entrada `CONSTANT_Utf8`.
 *
 * @details **Algoritmo:** valida o índice com cp_check() e retorna os bytes.
 *
 * @param[in] class_file Classe.
 * @param[in] index      Índice de um `CONSTANT_Utf8`.
 * @return String MUTF-8 terminada em '\\0', ou NULL se o índice for inválido.
 */
const char *cp_get_utf8(const ClassFile *class_file, uint16_t index);

/**
 * @brief Retorna o nome interno de uma classe (ex.: `java/lang/Object`).
 *
 * @details **Algoritmo:** valida que @p index é `CONSTANT_Class` e retorna o
 * Utf8 apontado por `name_index`.
 *
 * @param[in] class_file Classe.
 * @param[in] index      Índice de um `CONSTANT_Class`.
 * @return Nome da classe, ou NULL se o índice for inválido.
 */
const char *cp_get_class_name(const ClassFile *class_file, uint16_t index);

/**
 * @brief Retorna o texto de um `CONSTANT_String`.
 *
 * @details **Algoritmo:** valida o índice e retorna o Utf8 de `string_index`.
 *
 * @param[in] class_file Classe.
 * @param[in] index      Índice de um `CONSTANT_String`.
 * @return Texto MUTF-8, ou NULL se o índice for inválido.
 */
const char *cp_get_string(const ClassFile *class_file, uint16_t index);

/**
 * @brief Obtém nome e descritor de um `CONSTANT_NameAndType`.
 *
 * @details **Algoritmo:** valida o índice e resolve os dois Utf8.
 *
 * @param[in]  class_file Classe.
 * @param[in]  index      Índice de um `CONSTANT_NameAndType`.
 * @param[out] name       Nome (ex.: `println`).
 * @param[out] descriptor Descritor (ex.: `(Ljava/lang/String;)V`).
 * @return `true` em caso de sucesso; `false` se o índice for inválido.
 */
bool cp_get_name_and_type(const ClassFile *class_file, uint16_t index,
                          const char **name, const char **descriptor);

/**
 * @brief Resolve um Fieldref, Methodref ou InterfaceMethodref.
 *
 * @details **Algoritmo:** valida a tag da entrada, resolve o nome da classe
 * e o NameAndType.
 *
 * @param[in]  class_file Classe.
 * @param[in]  index      Índice de uma referência a campo/método.
 * @param[out] class_name Classe dona do membro.
 * @param[out] name       Nome do membro.
 * @param[out] descriptor Descritor do membro.
 * @return `true` em caso de sucesso; `false` se o índice for inválido.
 */
bool cp_get_ref(const ClassFile *class_file, uint16_t index,
                const char **class_name, const char **name,
                const char **descriptor);

/**
 * @brief Retorna o valor de um `CONSTANT_Integer`.
 *
 * @details **Algoritmo:** reinterpreta os 32 bits como inteiro com sinal
 * (complemento de dois), sem depender de conversão definida pela implementação.
 *
 * @param[in]  class_file Classe.
 * @param[in]  index      Índice de um `CONSTANT_Integer`.
 * @param[out] value      Valor lido.
 * @return `true` em caso de sucesso; `false` se o índice for inválido.
 */
bool cp_get_int(const ClassFile *class_file, uint16_t index, int32_t *value);

/**
 * @brief Retorna o valor de um `CONSTANT_Float`.
 *
 * @details **Algoritmo:** copia os 32 bits IEEE 754 para um `float` com `memcpy`.
 *
 * @param[in]  class_file Classe.
 * @param[in]  index      Índice de um `CONSTANT_Float`.
 * @param[out] value      Valor lido.
 * @return `true` em caso de sucesso; `false` se o índice for inválido.
 */
bool cp_get_float(const ClassFile *class_file, uint16_t index, float *value);

/**
 * @brief Retorna o valor de um `CONSTANT_Long`.
 *
 * @details **Algoritmo:** combina `(high << 32) | low` e reinterpreta como
 * inteiro de 64 bits com sinal.
 *
 * @param[in]  class_file Classe.
 * @param[in]  index      Índice de um `CONSTANT_Long`.
 * @param[out] value      Valor lido.
 * @return `true` em caso de sucesso; `false` se o índice for inválido.
 */
bool cp_get_long(const ClassFile *class_file, uint16_t index, int64_t *value);

/**
 * @brief Retorna o valor de um `CONSTANT_Double`.
 *
 * @details **Algoritmo:** combina `(high << 32) | low` e copia os 64 bits
 * IEEE 754 para um `double` com `memcpy`.
 *
 * @param[in]  class_file Classe.
 * @param[in]  index      Índice de um `CONSTANT_Double`.
 * @param[out] value      Valor lido.
 * @return `true` em caso de sucesso; `false` se o índice for inválido.
 */
bool cp_get_double(const ClassFile *class_file, uint16_t index, double *value);

/**
 * @brief Libera as strings e o vetor do pool de constantes.
 *
 * @details **Algoritmo:** percorre as entradas liberando os bytes das
 * entradas Utf8 e, ao final, o vetor; zera ponteiro e contador.
 *
 * @param[in,out] class_file Classe cujo pool será liberado.
 */
void cp_free(ClassFile *class_file);

#endif /* JVM_CONSTANT_POOL_H */
