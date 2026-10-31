/**
 * @file format.h
 * @brief Funções auxiliares de formatação do exibidor: flags de acesso,
 *        texto MUTF-8 e descrição de constantes no estilo do jclasslib.
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

/**
 * @brief Retorna o nome da estrutura de uma tag, como na especificação
 *        (ex.: "CONSTANT_Methodref_info").
 *
 * @details **Algoritmo:** `switch` sobre a tag.
 *
 * @param[in] tag Tag do pool.
 * @return Nome da estrutura (string estática, nunca NULL).
 */
const char *constant_info_name(ConstantTag tag);

/**
 * @brief Retorna o nome de um `reference_kind` de MethodHandle (ex.: "REF_invokeStatic").
 *
 * @details **Algoritmo:** tabela indexada de 1 a 9.
 *
 * @param[in] kind Valor de `reference_kind`.
 * @return Nome do tipo, ou "?" se @p kind estiver fora de 1–9.
 */
const char *method_handle_kind_name(uint8_t kind);

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
