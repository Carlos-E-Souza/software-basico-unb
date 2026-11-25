/**
 * @file printer_internal.h
 * @brief Funções internas do exibidor, compartilhadas entre os arquivos
 *        `src/viewer/`.
 *
 * @details Não faz parte da interface pública (veja viewer.h). Organização:
 * - format.c: formatação geral (flags de acesso, indentação, texto MUTF-8 e
 *   linhas no formato `Rotulo: valor`);
 * - printer.c: viewer_print_class() e as seções General Information,
 *   Interfaces, Fields e Methods;
 * - print_constant_pool.c: seção Constant Pool e representação textual das
 *   constantes no estilo do jclasslib;
 * - print_attributes.c: atributos (Code, tabelas, desconhecidos...);
 * - print_stack_map.c: quadros do atributo StackMapTable;
 * - disassembler.c: bytecode com mnemônicos (veja disassembler.h).
 */
#ifndef JVM_PRINTER_INTERNAL_H
#define JVM_PRINTER_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "classfile/class_file.h"

/** @brief Contexto das flags de acesso (o mesmo bit tem significados diferentes). */
typedef enum FlagContext {
    FLAGS_CLASS,       /**< Flags de uma classe ou interface. */
    FLAGS_FIELD,       /**< Flags de um campo. */
    FLAGS_METHOD,      /**< Flags de um método. */
    FLAGS_INNER_CLASS  /**< Flags de uma classe interna (atributo InnerClasses). */
} FlagContext;

/** @brief Tamanho de buffer suficiente para qualquer combinação de flags formatada. */
#define FLAGS_BUFFER_SIZE 256

/** @brief Largura da coluna dos rótulos ("Rotulo: valor"). */
#define LABEL_WIDTH 24

/* ------------------------------------------------------------ format.c --- */

/**
 * @brief Converte flags de acesso em texto no estilo do jclasslib (ex.: "public static").
 *
 * @details **Algoritmo:** percorre uma tabela (bit, nome, contextos válidos)
 * e concatena, separados por espaço, os nomes dos bits ligados que são
 * válidos no contexto. Bits desconhecidos são ignorados.
 *
 * @param[out] buffer  Destino (ao menos #FLAGS_BUFFER_SIZE bytes recomendados).
 * @param[in]  size    Tamanho do destino.
 * @param[in]  flags   Flags de acesso.
 * @param[in]  context Contexto de interpretação dos bits.
 */
void format_access_flags(char *buffer, size_t size, uint16_t flags,
                         FlagContext context);

/**
 * @brief Escreve @p count espaços (indentação).
 *
 * @details **Algoritmo:** `fprintf` com largura variável sobre string vazia.
 *
 * @param[in] out   Fluxo de saída.
 * @param[in] count Quantidade de espaços.
 */
void print_indent(FILE *out, int count);

/**
 * @brief Escreve um texto MUTF-8 como UTF-8 legível, escapando caracteres de controle.
 *
 * @details
 * **Algoritmo:**
 * 1. Decodifica o MUTF-8 em unidades UTF-16 (mutf8_to_utf16());
 * 2. Pares de *surrogates* válidos são combinados em um único código Unicode;
 * 3. `\n`, `\t`, `\r` são escritos como sequências de escape; demais
 *    caracteres de controle, NUL e *surrogates* isolados como `\uXXXX`;
 * 4. Os demais códigos são codificados em UTF-8 padrão (1 a 4 bytes).
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] bytes  Bytes MUTF-8 (já validados pelo leitor).
 * @param[in] length Quantidade de bytes.
 */
void print_mutf8(FILE *out, const char *bytes, size_t length);

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
 * `[n] CONSTANT_Xxx_info` (nome da tag com cp_tag_name()) e os campos
 * internos (print_constant_details()).
 * A segunda posição de Long/Double aparece como "(large numeric continued)",
 * como no jclasslib.
 *
 * @param[in] out Fluxo de saída.
 * @param[in] cf  Classe.
 */
void print_constant_pool(FILE *out, const ClassFile *cf);

/**
 * @brief Escreve o valor representado por uma entrada do pool, no estilo do jclasslib.
 *
 * @details
 * **Algoritmo**, conforme a tag:
 * - Utf8 e String: o texto;
 * - Integer, Long, Float e Double: o valor numérico (formato do Java);
 * - Class: o nome interno (ex.: `java/lang/Object`, `[[I`);
 * - Fieldref, Methodref e InterfaceMethodref: `classe.nome : descritor`;
 * - NameAndType: `nome : descritor`; MethodType: o descritor;
 * - MethodHandle: `REF_tipo classe.nome : descritor`;
 * - InvokeDynamic: `nome : descritor, BootstrapMethods #n`.
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe.
 * @param[in] index      Índice no pool (índices inválidos escrevem "invalido").
 */
void print_constant(FILE *out, const ClassFile *class_file, uint16_t index);

/**
 * @brief Escreve uma referência ao pool no formato `cp_info #n <constante>`.
 *
 * @details **Algoritmo:** escreve o índice e, entre `<` e `>`, a constante
 * resolvida com print_constant(). O índice 0 ("nenhum") é escrito apenas
 * como `cp_info #0`.
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe.
 * @param[in] index      Índice no pool.
 */
void print_cp_ref(FILE *out, const ClassFile *class_file, uint16_t index);

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
