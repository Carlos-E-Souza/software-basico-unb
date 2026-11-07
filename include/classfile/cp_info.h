/**
 * @file cp_info.h
 * @brief Estruturas das entradas do pool de constantes (`cp_info`, JVMS §4.4).
 *
 * @details Incluído por class_file.h. As funções de leitura e consulta do
 * pool estão em constant_pool.h.
 */
#ifndef JVM_CP_INFO_H
#define JVM_CP_INFO_H

#include <stdint.h>

/** @brief Tags das entradas do pool de constantes. */
typedef enum ConstantTag {
    CONSTANT_Unusable           = 0,  /**< Entrada inexistente: índice 0 ou 2ª metade de Long/Double. */
    CONSTANT_Utf8               = 1,  /**< String MUTF-8. */
    CONSTANT_Integer            = 3,  /**< Inteiro de 32 bits. */
    CONSTANT_Float              = 4,  /**< Float IEEE 754 de 32 bits. */
    CONSTANT_Long               = 5,  /**< Inteiro de 64 bits (ocupa 2 entradas). */
    CONSTANT_Double             = 6,  /**< Double IEEE 754 de 64 bits (ocupa 2 entradas). */
    CONSTANT_Class              = 7,  /**< Referência a classe ou interface. */
    CONSTANT_String             = 8,  /**< Literal String. */
    CONSTANT_Fieldref           = 9,  /**< Referência a campo. */
    CONSTANT_Methodref          = 10, /**< Referência a método de classe. */
    CONSTANT_InterfaceMethodref = 11, /**< Referência a método de interface. */
    CONSTANT_NameAndType        = 12, /**< Par nome + descritor. */
    CONSTANT_MethodHandle       = 15, /**< *Method handle* (Java 7+). */
    CONSTANT_MethodType         = 16, /**< Tipo de método (Java 7+). */
    CONSTANT_InvokeDynamic      = 18  /**< Sítio de chamada dinâmica (Java 7+). */
} ConstantTag;

/** @brief Conteúdo de `CONSTANT_Utf8_info`. */
typedef struct CpUtf8 {
    uint16_t length; /**< Quantidade de bytes (sem o '\\0' adicional). */
    char *bytes;     /**< Bytes MUTF-8 terminados em '\\0' (string C válida). */
} CpUtf8;

/** @brief Conteúdo de `CONSTANT_Long_info` e `CONSTANT_Double_info`. */
typedef struct CpWide {
    uint32_t high_bytes; /**< 32 bits mais significativos. */
    uint32_t low_bytes;  /**< 32 bits menos significativos. */
} CpWide;

/** @brief Conteúdo de `CONSTANT_Fieldref/Methodref/InterfaceMethodref_info`. */
typedef struct CpRef {
    uint16_t class_index;         /**< Índice de um `CONSTANT_Class`. */
    uint16_t name_and_type_index; /**< Índice de um `CONSTANT_NameAndType`. */
} CpRef;

/** @brief Conteúdo de `CONSTANT_NameAndType_info`. */
typedef struct CpNameAndType {
    uint16_t name_index;       /**< Índice do nome (`CONSTANT_Utf8`). */
    uint16_t descriptor_index; /**< Índice do descritor (`CONSTANT_Utf8`). */
} CpNameAndType;

/** @brief Conteúdo de `CONSTANT_MethodHandle_info`. */
typedef struct CpMethodHandle {
    uint8_t reference_kind;   /**< Tipo do *handle* (1 a 9, ex.: 6 = REF_invokeStatic). */
    uint16_t reference_index; /**< Índice da referência (Fieldref/Methodref/InterfaceMethodref). */
} CpMethodHandle;

/** @brief Conteúdo de `CONSTANT_InvokeDynamic_info`. */
typedef struct CpInvokeDynamic {
    uint16_t bootstrap_method_attr_index; /**< Índice no atributo BootstrapMethods. */
    uint16_t name_and_type_index;         /**< Índice de um `CONSTANT_NameAndType`. */
} CpInvokeDynamic;

/**
 * @brief Uma entrada do pool de constantes (`cp_info`).
 *
 * @details O campo @c tag define qual membro da união @c info é válido.
 */
typedef struct CpInfo {
    uint8_t tag; /**< Tipo da entrada (#ConstantTag). */
    /** @brief Dados da entrada, conforme @c tag. */
    union {
        CpUtf8 utf8;                    /**< `CONSTANT_Utf8`. */
        uint32_t integer_bytes;         /**< `CONSTANT_Integer`: bits do inteiro. */
        uint32_t float_bytes;           /**< `CONSTANT_Float`: bits IEEE 754. */
        CpWide wide;                    /**< `CONSTANT_Long` e `CONSTANT_Double`. */
        uint16_t class_name_index;      /**< `CONSTANT_Class`: índice do nome. */
        uint16_t string_index;          /**< `CONSTANT_String`: índice do texto. */
        CpRef ref;                      /**< `CONSTANT_Fieldref/Methodref/InterfaceMethodref`. */
        CpNameAndType name_and_type;    /**< `CONSTANT_NameAndType`. */
        CpMethodHandle method_handle;   /**< `CONSTANT_MethodHandle`. */
        uint16_t method_type_index;     /**< `CONSTANT_MethodType`: índice do descritor. */
        CpInvokeDynamic invoke_dynamic; /**< `CONSTANT_InvokeDynamic`. */
    } info;
} CpInfo;

#endif /* JVM_CP_INFO_H */
