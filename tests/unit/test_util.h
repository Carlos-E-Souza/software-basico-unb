/**
 * @file test_util.h
 * @brief Utilitários dos testes: montagem de arquivos `.class` byte a byte
 *        e auxiliares compartilhados pelas suítes do leitor.
 */
#ifndef JVM_TEST_UTIL_H
#define JVM_TEST_UTIL_H

#include <stddef.h>
#include <stdint.h>

#include "classfile/class_file.h"

/** @brief Buffer de bytes de tamanho fixo usado para montar classes de teste. */
typedef struct ByteBuf {
    uint8_t data[4096]; /**< Conteúdo. */
    size_t size;        /**< Bytes usados. */
} ByteBuf;

/** @brief Acrescenta um u1. */
void bb_u1(ByteBuf *b, uint8_t v);
/** @brief Acrescenta um u2 em big-endian. */
void bb_u2(ByteBuf *b, uint16_t v);
/** @brief Acrescenta um u4 em big-endian. */
void bb_u4(ByteBuf *b, uint32_t v);
/** @brief Acrescenta uma entrada CONSTANT_Utf8 com o texto ASCII dado. */
void bb_utf8(ByteBuf *b, const char *text);

/**
 * @brief Monta o início de uma classe mínima válida até o fim do pool.
 *
 * @details Pool: #1 Utf8 "Min", #2 Class #1, #3 Utf8 "java/lang/Object",
 * #4 Class #3, #5 Utf8 "Code". Versão 52.0.
 * @param[out] b Buffer (é zerado antes).
 * @param[in]  extra_cp Quantidade de entradas extras que o chamador vai
 *                      acrescentar logo em seguida (ajusta constant_pool_count).
 */
void bb_minimal_class_head(ByteBuf *b, uint16_t extra_cp);

/**
 * @brief Acrescenta o final de uma classe mínima: flags, this=#2, super=#4,
 *        sem interfaces, campos, métodos ou atributos.
 * @param[in,out] b Buffer.
 */
void bb_minimal_class_tail(ByteBuf *b);

/**
 * @brief Monta uma classe mínima com um método estático `m()V` cujo Code
 *        contém exatamente os bytes informados (sem tabela de exceções).
 * @param[out] b      Buffer (é zerado antes).
 * @param[in]  code   Bytecode do método.
 * @param[in]  length Tamanho do bytecode.
 */
void bb_class_with_code(ByteBuf *b, const uint8_t *code, uint16_t length);

/** @brief Interpreta um buffer e retorna o status (liberando a classe). */
CfStatus bb_parse_status(const ByteBuf *b);

/** @brief Acrescenta @p n bytes brutos ao buffer. */
void bb_bytes(ByteBuf *b, const uint8_t *bytes, size_t n);

/**
 * @brief Classe com um campo `static final` e os atributos brutos dados.
 *
 * @details Pool extra: #6 Utf8 "ConstantValue", #7 Integer 42.
 * @param[out] b           Buffer.
 * @param[in]  name        name_index do campo.
 * @param[in]  descriptor  descriptor_index do campo.
 * @param[in]  attrs       Atributos já serializados (cabeçalho + conteúdo).
 * @param[in]  attrs_size  Tamanho de @p attrs.
 * @param[in]  attrs_count attributes_count do campo.
 */
void bb_field_class(ByteBuf *b, uint16_t name, uint16_t descriptor,
                    const uint8_t *attrs, size_t attrs_size,
                    uint16_t attrs_count);

/** @brief Retorna o atributo Code de um método (ou NULL). */
const CodeAttribute *method_code(const MemberInfo *method);

#endif /* JVM_TEST_UTIL_H */
