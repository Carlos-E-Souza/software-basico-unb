/**
 * @file attr_parsers.h
 * @brief Funções internas do leitor de atributos, compartilhadas entre os
 *        arquivos `src/classfile/attr_*.c`.
 *
 * @details Não faz parte da interface pública do leitor (veja attributes.h).
 * Cada função `attr_parse_*` lê o conteúdo de um tipo de atributo a partir de
 * um sub-leitor limitado a `attribute_length` bytes. Organização:
 * - attr_common.c: utilitários (erro de atributo curto, validação de índice,
 *   alocação de vetores);
 * - attr_simple.c: atributos de tamanho fixo (ConstantValue, EnclosingMethod,
 *   Signature, SourceFile) e desconhecidos;
 * - attr_code.c: atributo Code;
 * - attr_stack_map.c: atributo StackMapTable;
 * - attr_tables.c: atributos em forma de tabela (Exceptions, InnerClasses,
 *   LineNumberTable, LocalVariable(Type)Table, BootstrapMethods);
 * - attr_free.c: liberação de atributos.
 */
#ifndef JVM_ATTR_PARSERS_H
#define JVM_ATTR_PARSERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "classfile/byte_reader.h"
#include "classfile/class_file.h"

/* ------------------------------------------------------- attr_common.c --- */

/**
 * @brief Gera o erro padrão para atributo cujo conteúdo acabou antes do esperado.
 *
 * @details **Algoritmo:** chama cf_fail() com #CF_ERR_BAD_ATTRIBUTE e a
 * posição atual do sub-leitor.
 *
 * @param[in]  reader Sub-leitor do atributo.
 * @param[in]  what   Nome do atributo ou do campo que faltou.
 * @param[out] err    Detalhes do erro (pode ser NULL).
 * @return #CF_ERR_BAD_ATTRIBUTE.
 */
CfStatus attr_too_short(const ByteReader *reader, const char *what,
                        ClassFileError *err);

/**
 * @brief Verifica um índice do pool usado por um atributo.
 *
 * @details **Algoritmo:** se @p allow_zero e o índice é 0, aceita; senão
 * exige cp_check() com a tag dada.
 *
 * @param[in]  class_file Classe.
 * @param[in]  reader     Sub-leitor (para a posição do erro).
 * @param[in]  index      Índice a verificar.
 * @param[in]  tag        Tag esperada.
 * @param[in]  allow_zero Se o valor 0 ("nenhum") é permitido.
 * @param[in]  what       Descrição do campo (para a mensagem).
 * @param[out] err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou #CF_ERR_BAD_INDEX.
 */
CfStatus attr_check_index(const ClassFile *class_file, const ByteReader *reader,
                          uint16_t index, ConstantTag tag, bool allow_zero,
                          const char *what, ClassFileError *err);

/**
 * @brief Aloca um vetor zerado de @p count elementos (NULL quando @p count é 0).
 *
 * @details **Algoritmo:** chama `calloc` somente se @p count > 0 e informa
 * falha de memória por meio de @p ok.
 *
 * @param[in]  count Quantidade de elementos.
 * @param[in]  size  Tamanho de cada elemento.
 * @param[out] ok    `false` se a alocação falhou.
 * @return Ponteiro para o vetor, ou NULL.
 */
void *attr_alloc_array(size_t count, size_t size, bool *ok);

/* ------------------------------------------------------- attr_simple.c --- */

/**
 * @brief Interpreta o atributo ConstantValue.
 *
 * @details **Algoritmo:** lê `constantvalue_index` e exige que aponte para
 * Integer, Float, Long, Double ou String.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_constant_value(ByteReader *r, const ClassFile *class_file,
                                   AttributeInfo *attr, ClassFileError *err);

/**
 * @brief Interpreta o atributo EnclosingMethod.
 *
 * @details **Algoritmo:** lê `class_index` (Class) e `method_index`
 * (0 ou NameAndType) e valida os dois índices.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_enclosing_method(ByteReader *r, const ClassFile *class_file,
                                     AttributeInfo *attr, ClassFileError *err);

/**
 * @brief Lê um atributo cujo conteúdo é um único índice de `CONSTANT_Utf8`
 *        (Signature e SourceFile).
 *
 * @details **Algoritmo:** lê o u2 em @p index e exige que aponte para Utf8.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     what       Nome do atributo (para mensagens).
 * @param[out]    index      Índice lido.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_utf8_index(ByteReader *r, const ClassFile *class_file,
                               const char *what, uint16_t *index,
                               ClassFileError *err);

/**
 * @brief Copia os bytes de um atributo desconhecido.
 *
 * @details **Algoritmo:** se `attribute_length` > 0, lê os bytes do
 * sub-leitor e os copia para memória própria em `info.raw`.
 *
 * @param[in,out] r    Sub-leitor do atributo.
 * @param[out]    attr Atributo a preencher.
 * @param[out]    err  Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_unknown(ByteReader *r, AttributeInfo *attr, ClassFileError *err);

/* --------------------------------------------------------- attr_code.c --- */

/**
 * @brief Interpreta o atributo Code.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê `max_stack`, `max_locals` e `code_length` (1 a 65535) e copia o bytecode;
 * 2. Lê a tabela de exceções validando `start_pc < end_pc <= code_length`,
 *    `handler_pc < code_length` e `catch_type` (0 ou Class);
 * 3. Lê os atributos aninhados com profundidade @p depth + 1.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     depth      Profundidade atual.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_code(ByteReader *r, const ClassFile *class_file,
                         unsigned depth, AttributeInfo *attr,
                         ClassFileError *err);

/* ---------------------------------------------------- attr_stack_map.c --- */

/**
 * @brief Interpreta o atributo StackMapTable.
 *
 * @details **Algoritmo:** lê `number_of_entries`, aloca os quadros e lê cada
 * um com parse_stack_map_frame().
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_stack_map_table(ByteReader *r, const ClassFile *class_file,
                                    AttributeInfo *attr, ClassFileError *err);

/* ------------------------------------------------------- attr_tables.c --- */

/**
 * @brief Lê um vetor de índices u2 do pool, validando a tag de cada um.
 *
 * @details **Algoritmo:** lê a quantidade, aloca o vetor, grava em @p out e
 * @p count e lê/valida cada índice.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     tag        Tag exigida para cada índice.
 * @param[in]     what       Nome do atributo (para mensagens).
 * @param[out]    count      Quantidade lida.
 * @param[out]    out        Vetor alocado.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_index_list(ByteReader *r, const ClassFile *class_file,
                               ConstantTag tag, const char *what,
                               uint16_t *count, uint16_t **out,
                               ClassFileError *err);

/**
 * @brief Interpreta o atributo InnerClasses.
 *
 * @details **Algoritmo:** lê `number_of_classes` e, para cada entrada, os
 * quatro campos u2, validando: classe interna (Class), classe externa
 * (0 ou Class) e nome (0 ou Utf8).
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_inner_classes(ByteReader *r, const ClassFile *class_file,
                                  AttributeInfo *attr, ClassFileError *err);

/**
 * @brief Interpreta o atributo LineNumberTable.
 *
 * @details **Algoritmo:** lê o tamanho, aloca e lê os pares
 * (`start_pc`, `line_number`).
 *
 * @param[in,out] r    Sub-leitor do atributo.
 * @param[out]    attr Atributo a preencher.
 * @param[out]    err  Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_line_numbers(ByteReader *r, AttributeInfo *attr,
                                 ClassFileError *err);

/**
 * @brief Interpreta LocalVariableTable ou LocalVariableTypeTable (mesmo formato).
 *
 * @details **Algoritmo:** lê o tamanho e as entradas de 5 campos u2,
 * validando que nome e descritor/assinatura são Utf8.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_local_variables(ByteReader *r, const ClassFile *class_file,
                                    AttributeInfo *attr, ClassFileError *err);

/**
 * @brief Interpreta o atributo BootstrapMethods.
 *
 * @details **Algoritmo:** lê a quantidade de métodos; para cada um, lê e
 * valida `bootstrap_method_ref` (MethodHandle) e a lista de argumentos
 * (cada um deve ser um índice utilizável do pool).
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
CfStatus attr_parse_bootstrap_methods(ByteReader *r, const ClassFile *class_file,
                                      AttributeInfo *attr, ClassFileError *err);

#endif /* JVM_ATTR_PARSERS_H */
