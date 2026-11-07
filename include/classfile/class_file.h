/**
 * @file class_file.h
 * @brief Estruturas de dados que representam um arquivo `.class` em memória
 *        e funções para carregá-lo e liberá-lo.
 *
 * @details As estruturas seguem fielmente o capítulo 4 da especificação da
 * JVM (JVMS SE 8). Os nomes dos campos são os mesmos da especificação, para
 * facilitar a consulta. Toda a memória pertence à estrutura #ClassFile e é
 * liberada por class_file_free().
 *
 * Organização dos headers:
 * - cp_info.h: entradas do pool de constantes (#CpInfo);
 * - attribute_info.h: atributos (#AttributeInfo, #CodeAttribute, ...);
 * - este arquivo: flags de acesso, campos/métodos (#MemberInfo) e a
 *   estrutura raiz #ClassFile.
 *
 * Organização da leitura:
 * - class_file.c: estrutura principal, interfaces, campos e métodos;
 * - constant_pool.c, cp_validate.c, cp_query.c: pool de constantes
 *   (leitura, validação e consulta);
 * - attributes.c e attr_*.c: atributos (Code, ConstantValue, StackMapTable, ...).
 */
#ifndef JVM_CLASS_FILE_H
#define JVM_CLASS_FILE_H

#include <stdint.h>

#include "classfile/attribute_info.h"
#include "classfile/cf_error.h"
#include "classfile/cp_info.h"

/** @brief Número mágico que inicia todo arquivo `.class`. */
#define CLASS_FILE_MAGIC 0xCAFEBABEu
/** @brief Menor *major version* aceita (45 = Java 1.0/1.1). */
#define CLASS_FILE_MIN_MAJOR 45
/** @brief Maior *major version* aceita (52 = Java 8). */
#define CLASS_FILE_MAX_MAJOR 52

/* ======================================================================== */
/*                          Flags de acesso (JVMS 4.1, 4.5, 4.6, 4.7.6)     */
/* ======================================================================== */

/** @brief Flags de acesso de classes, campos, métodos e classes internas. */
enum AccessFlag {
    ACC_PUBLIC       = 0x0001, /**< Público. */
    ACC_PRIVATE      = 0x0002, /**< Privado (campos, métodos, classes internas). */
    ACC_PROTECTED    = 0x0004, /**< Protegido (campos, métodos, classes internas). */
    ACC_STATIC       = 0x0008, /**< Estático (campos, métodos, classes internas). */
    ACC_FINAL        = 0x0010, /**< Final. */
    ACC_SUPER        = 0x0020, /**< (classe) semântica moderna de invokespecial. */
    ACC_SYNCHRONIZED = 0x0020, /**< (método) sincronizado. */
    ACC_VOLATILE     = 0x0040, /**< (campo) volátil. */
    ACC_BRIDGE       = 0x0040, /**< (método) método ponte gerado pelo compilador. */
    ACC_TRANSIENT    = 0x0080, /**< (campo) transiente. */
    ACC_VARARGS      = 0x0080, /**< (método) número variável de argumentos. */
    ACC_NATIVE       = 0x0100, /**< (método) nativo. */
    ACC_INTERFACE    = 0x0200, /**< (classe) interface. */
    ACC_ABSTRACT     = 0x0400, /**< Abstrato. */
    ACC_STRICT       = 0x0800, /**< (método) ponto flutuante estrito. */
    ACC_SYNTHETIC    = 0x1000, /**< Gerado pelo compilador. */
    ACC_ANNOTATION   = 0x2000, /**< (classe) tipo anotação. */
    ACC_ENUM         = 0x4000  /**< Enum ou constante de enum. */
};

/* ======================================================================== */
/*                     Campos, métodos e a classe (JVMS 4.1, 4.5, 4.6)      */
/* ======================================================================== */

/**
 * @brief Campo ou método (`field_info` e `method_info` têm o mesmo formato).
 */
typedef struct MemberInfo {
    uint16_t access_flags;     /**< Flags de acesso (#AccessFlag). */
    uint16_t name_index;       /**< Nome (`CONSTANT_Utf8`). */
    uint16_t descriptor_index; /**< Descritor (`CONSTANT_Utf8`), ex.: `(I)V`. */
    uint16_t attributes_count; /**< Quantidade de atributos. */
    AttributeInfo *attributes; /**< Atributos (Code, ConstantValue, ...). */
} MemberInfo;

/** @brief Campo de uma classe (`field_info`). */
typedef MemberInfo FieldInfo;
/** @brief Método de uma classe (`method_info`). */
typedef MemberInfo MethodInfo;

/**
 * @brief Representação em memória de um arquivo `.class` (estrutura `ClassFile`).
 *
 * @details O pool de constantes é indexado de 1 a `constant_pool_count - 1`,
 * exatamente como na especificação; a posição 0 e a segunda posição de cada
 * Long/Double têm tag #CONSTANT_Unusable.
 */
typedef struct ClassFile {
    uint32_t magic;               /**< Sempre #CLASS_FILE_MAGIC. */
    uint16_t minor_version;       /**< Versão secundária do formato. */
    uint16_t major_version;       /**< Versão principal (46 = Java 2, 50 = Java 6, 52 = Java 8). */
    uint16_t constant_pool_count; /**< Quantidade de entradas do pool + 1. */
    CpInfo *constant_pool;        /**< Pool de constantes (vetor com @c constant_pool_count posições). */
    uint16_t access_flags;        /**< Flags de acesso da classe. */
    uint16_t this_class;          /**< Esta classe (`CONSTANT_Class`). */
    uint16_t super_class;         /**< Superclasse (`CONSTANT_Class`) ou 0 (apenas `java/lang/Object`). */
    uint16_t interfaces_count;    /**< Quantidade de interfaces implementadas. */
    uint16_t *interfaces;         /**< Índices de `CONSTANT_Class` das interfaces. */
    uint16_t fields_count;        /**< Quantidade de campos. */
    FieldInfo *fields;            /**< Campos declarados. */
    uint16_t methods_count;       /**< Quantidade de métodos. */
    MethodInfo *methods;          /**< Métodos declarados. */
    uint16_t attributes_count;    /**< Quantidade de atributos da classe. */
    AttributeInfo *attributes;    /**< Atributos da classe (SourceFile, InnerClasses, ...). */
} ClassFile;

/* ======================================================================== */
/*                                   Funções                                */
/* ======================================================================== */

/**
 * @brief Interpreta um `.class` a partir de um buffer em memória.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê e valida `magic`, `minor_version` e `major_version` (45 a 52);
 * 2. Lê o pool de constantes (cp_parse()) e valida as referências internas
 *    (cp_validate());
 * 3. Lê `access_flags`, `this_class`, `super_class` e as interfaces,
 *    validando os índices;
 * 4. Lê campos, métodos e atributos da classe (attributes_parse());
 * 5. Verifica que não sobraram bytes no buffer;
 * 6. Em qualquer erro, libera o que foi alocado e preenche @p err.
 *
 * @param[in]  data Bytes do arquivo `.class`.
 * @param[in]  size Quantidade de bytes.
 * @param[out] out  Estrutura alocada (liberar com class_file_free()); NULL em caso de erro.
 * @param[out] err  Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou o código do erro encontrado.
 */
CfStatus class_file_parse(const uint8_t *data, size_t size, ClassFile **out,
                          ClassFileError *err);

/**
 * @brief Lê um arquivo `.class` do disco e o interpreta.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê o arquivo inteiro para a memória (file_read_all());
 * 2. Chama class_file_parse();
 * 3. Libera o buffer do arquivo (a estrutura não depende dele).
 *
 * @param[in]  path Caminho do arquivo `.class`.
 * @param[out] out  Estrutura alocada; NULL em caso de erro.
 * @param[out] err  Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou o código do erro encontrado.
 */
CfStatus class_file_load(const char *path, ClassFile **out, ClassFileError *err);

/**
 * @brief Libera toda a memória de uma estrutura #ClassFile.
 *
 * @details
 * **Algoritmo:** libera, nesta ordem, os atributos da classe, os atributos
 * de cada método e campo, os vetores de membros e interfaces, as strings do
 * pool de constantes, o pool e a própria estrutura. Aceita estruturas
 * parcialmente preenchidas (ponteiros NULL são ignorados).
 *
 * @param[in] class_file Estrutura a liberar (pode ser NULL).
 */
void class_file_free(ClassFile *class_file);

/**
 * @brief Retorna o nome da versão do Java correspondente a uma *major version*.
 *
 * @details **Algoritmo:** tabela de 45 ("Java 1.1") a 52 ("Java 8").
 *
 * @param[in] major_version Versão principal do arquivo.
 * @return Nome da versão (ex.: "Java 8") ou "desconhecida".
 */
const char *class_file_java_version_name(uint16_t major_version);

#endif /* JVM_CLASS_FILE_H */
