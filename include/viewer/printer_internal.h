/**
 * @file printer_internal.h
 * @brief Funções internas do exibidor, compartilhadas entre os arquivos
 *        `src/viewer/print*.c`.
 *
 * @details Não faz parte da interface pública (veja viewer.h). Organização:
 * - printer.c: linhas no formato `Rotulo: valor`, títulos de seção e as
 *   seções General Information, Interfaces, Fields e Methods;
 * - print_constant_pool.c: seção Constant Pool;
 * - print_attributes.c: atributos (Code, tabelas, desconhecidos...);
 * - print_stack_map.c: quadros do atributo StackMapTable.
 */
#ifndef JVM_PRINTER_INTERNAL_H
#define JVM_PRINTER_INTERNAL_H

#include <stdint.h>
#include <stdio.h>

#include "classfile/class_file.h"
#include "viewer/format.h"

/** @brief Largura da coluna dos rótulos ("Rotulo: valor"). */
#define LABEL_WIDTH 24

/* ----------------------------------------------------------- printer.c --- */

/**
 * @brief Escreve o início de uma linha `Rotulo:   ` alinhada.
 *
 * @details **Algoritmo:** escreve a indentação, o rótulo seguido de ':' e
 * completa com espaços até #LABEL_WIDTH colunas.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] indent Indentação.
 * @param[in] label  Texto do rótulo.
 */
void print_label(FILE *out, int indent, const char *label);

/**
 * @brief Escreve uma linha `Rotulo: cp_info #n <constante>`.
 *
 * @details **Algoritmo:** print_label() seguido de print_cp_ref().
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] cf     Classe.
 * @param[in] indent Indentação.
 * @param[in] label  Texto do rótulo.
 * @param[in] index  Índice no pool.
 */
void print_ref_line(FILE *out, const ClassFile *cf, int indent,
                    const char *label, uint16_t index);

/**
 * @brief Escreve uma linha `Rotulo: número`.
 *
 * @details **Algoritmo:** print_label() seguido do valor em decimal.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] indent Indentação.
 * @param[in] label  Texto do rótulo.
 * @param[in] value  Valor.
 */
void print_number_line(FILE *out, int indent, const char *label,
                       unsigned long value);

/**
 * @brief Escreve uma linha `Rotulo: 0x0021 [public super]`.
 *
 * @details **Algoritmo:** formata o valor hexadecimal e os nomes com
 * format_access_flags().
 *
 * @param[in] out     Fluxo de saída.
 * @param[in] indent  Indentação.
 * @param[in] flags   Flags de acesso.
 * @param[in] context Contexto (classe, campo, método ou classe interna).
 */
void print_flags_line(FILE *out, int indent, uint16_t flags,
                      FlagContext context);

/**
 * @brief Escreve o título de uma seção (ex.: `=== Methods (count = 5) ===`).
 *
 * @details **Algoritmo:** linha em branco e o título; se @p count não for
 * negativo, inclui a quantidade de itens.
 *
 * @param[in] out   Fluxo de saída.
 * @param[in] title Texto do título.
 * @param[in] count Quantidade de itens, ou −1 para omitir.
 */
void print_section(FILE *out, const char *title, long count);

/* ------------------------------------------------- print_constant_pool.c --- */

/**
 * @brief Escreve a seção Constant Pool.
 *
 * @details **Algoritmo:** para cada índice de 1 a `count - 1`, escreve
 * `[n] CONSTANT_Xxx_info` e os campos internos (print_constant_details()).
 * A segunda posição de Long/Double aparece como "(large numeric continued)",
 * como no jclasslib.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 */
void print_constant_pool(FILE *out, const ClassFile *cf);

/* ---------------------------------------------------- print_attributes.c --- */

/**
 * @brief Escreve um vetor de atributos.
 *
 * @details **Algoritmo:** escreve a contagem e, para cada atributo, o nome,
 * as informações genéricas (`Attribute name index` como referência ao pool
 * e `Attribute length`) e o conteúdo específico (print_attribute_body()).
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] cf         Classe.
 * @param[in] attributes Vetor de atributos.
 * @param[in] count      Quantidade de atributos.
 * @param[in] indent     Indentação.
 */
void print_attributes(FILE *out, const ClassFile *cf,
                      const AttributeInfo *attributes, uint16_t count,
                      int indent);

/* ----------------------------------------------------- print_stack_map.c --- */

/**
 * @brief Escreve os quadros de um StackMapTable.
 *
 * @details **Algoritmo:** para cada quadro, calcula o pc absoluto (o
 * primeiro é `offset_delta`; os seguintes, anterior + `offset_delta` + 1) e
 * escreve tipo, deslocamento, locais e pilha listados.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] cf     Classe.
 * @param[in] attr   Atributo StackMapTable.
 * @param[in] indent Indentação.
 */
void print_stack_map_table(FILE *out, const ClassFile *cf,
                           const AttributeInfo *attr, int indent);

#endif /* JVM_PRINTER_INTERNAL_H */
