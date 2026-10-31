/**
 * @file format.h
 * @brief Funções auxiliares de formatação do exibidor: flags de acesso,
 *        texto MUTF-8 e descrição de constantes.
 */
#ifndef JVM_FORMAT_H
#define JVM_FORMAT_H

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

/**
 * @brief Converte flags de acesso em texto (ex.: "ACC_PUBLIC, ACC_STATIC").
 *
 * @details **Algoritmo:** percorre uma tabela (bit, nome, contextos válidos)
 * e concatena, separados por vírgula, os nomes dos bits ligados que são
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
 * @brief Escreve a descrição resolvida de uma entrada do pool (estilo `javap`).
 *
 * @details
 * **Algoritmo**, conforme a tag:
 * - Utf8/String: o texto; Integer/Long/Float/Double: o valor (com sufixo
 *   `l`, `f` ou `d` nos três últimos);
 * - Class: o nome; Fieldref/Methodref/InterfaceMethodref: `classe.nome:descritor`;
 * - NameAndType: `nome:descritor`; MethodType: o descritor;
 * - MethodHandle: `REF_tipo` + a referência; InvokeDynamic:
 *   `#bootstrap:nome:descritor`.
 *
 * Nomes que não são identificadores comuns (`<init>`, tipos de array) são
 * escritos entre aspas, como no `javap`.
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe.
 * @param[in] index      Índice no pool (índices inválidos escrevem "<invalido>").
 */
void print_constant(FILE *out, const ClassFile *class_file, uint16_t index);

/**
 * @brief Retorna o prefixo usado pelo `javap` nos comentários do bytecode
 *        (ex.: "Method", "Field", "class", "String").
 *
 * @details **Algoritmo:** `switch` sobre a tag da entrada.
 *
 * @param[in] class_file Classe.
 * @param[in] index      Índice no pool.
 * @return Prefixo (string estática, nunca NULL).
 */
const char *constant_kind_label(const ClassFile *class_file, uint16_t index);

/**
 * @brief Escreve @p count espaços (indentação).
 *
 * @details **Algoritmo:** `fprintf` com largura variável sobre string vazia.
 *
 * @param[in] out   Fluxo de saída.
 * @param[in] count Quantidade de espaços.
 */
void print_indent(FILE *out, int count);

#endif /* JVM_FORMAT_H */
