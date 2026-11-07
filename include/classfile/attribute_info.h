/**
 * @file attribute_info.h
 * @brief Estruturas dos atributos (`attribute_info`, JVMS §4.7) já interpretados.
 *
 * @details Incluído por class_file.h. As funções de leitura e liberação de
 * atributos estão em attributes.h.
 */
#ifndef JVM_ATTRIBUTE_INFO_H
#define JVM_ATTRIBUTE_INFO_H

#include <stdint.h>

/** @brief Tipos de atributo reconhecidos pelo leitor. */
typedef enum AttributeKind {
    ATTR_UNKNOWN = 0,                 /**< Não reconhecido: guardado como bytes brutos. */
    ATTR_CONSTANT_VALUE,              /**< ConstantValue. */
    ATTR_CODE,                        /**< Code. */
    ATTR_STACK_MAP_TABLE,             /**< StackMapTable. */
    ATTR_EXCEPTIONS,                  /**< Exceptions. */
    ATTR_INNER_CLASSES,               /**< InnerClasses. */
    ATTR_ENCLOSING_METHOD,            /**< EnclosingMethod. */
    ATTR_SYNTHETIC,                   /**< Synthetic. */
    ATTR_SIGNATURE,                   /**< Signature. */
    ATTR_SOURCE_FILE,                 /**< SourceFile. */
    ATTR_LINE_NUMBER_TABLE,           /**< LineNumberTable. */
    ATTR_LOCAL_VARIABLE_TABLE,        /**< LocalVariableTable. */
    ATTR_LOCAL_VARIABLE_TYPE_TABLE,   /**< LocalVariableTypeTable. */
    ATTR_DEPRECATED,                  /**< Deprecated. */
    ATTR_BOOTSTRAP_METHODS            /**< BootstrapMethods. */
} AttributeKind;

struct AttributeInfo;

/** @brief Entrada da tabela de exceções do atributo Code. */
typedef struct ExceptionTableEntry {
    uint16_t start_pc;   /**< Início (inclusivo) da faixa protegida. */
    uint16_t end_pc;     /**< Fim (exclusivo) da faixa protegida. */
    uint16_t handler_pc; /**< Início do tratador. */
    uint16_t catch_type; /**< Classe capturada (`CONSTANT_Class`) ou 0 = qualquer (finally). */
} ExceptionTableEntry;

/** @brief Atributo Code: bytecode de um método e suas tabelas. */
typedef struct CodeAttribute {
    uint16_t max_stack;                   /**< Profundidade máxima da pilha de operandos. */
    uint16_t max_locals;                  /**< Quantidade de variáveis locais (slots). */
    uint32_t code_length;                 /**< Tamanho do bytecode (1 a 65535). */
    uint8_t *code;                        /**< Bytecode do método. */
    uint16_t exception_table_length;      /**< Quantidade de entradas em @c exception_table. */
    ExceptionTableEntry *exception_table; /**< Tratadores de exceção. */
    uint16_t attributes_count;            /**< Quantidade de atributos aninhados. */
    struct AttributeInfo *attributes;     /**< Atributos (LineNumberTable, StackMapTable, ...). */
} CodeAttribute;

/** @brief Entrada do atributo LineNumberTable. */
typedef struct LineNumberEntry {
    uint16_t start_pc;    /**< Offset no bytecode. */
    uint16_t line_number; /**< Linha correspondente no código-fonte. */
} LineNumberEntry;

/** @brief Entrada dos atributos LocalVariableTable e LocalVariableTypeTable. */
typedef struct LocalVariableEntry {
    uint16_t start_pc;         /**< Início do escopo da variável. */
    uint16_t length;           /**< Tamanho do escopo, em bytes de código. */
    uint16_t name_index;       /**< Nome da variável (`CONSTANT_Utf8`). */
    uint16_t descriptor_index; /**< Descritor (ou assinatura genérica, na TypeTable). */
    uint16_t index;            /**< Slot da variável local. */
} LocalVariableEntry;

/** @brief Entrada do atributo InnerClasses. */
typedef struct InnerClassEntry {
    uint16_t inner_class_info_index;   /**< Classe interna (`CONSTANT_Class`). */
    uint16_t outer_class_info_index;   /**< Classe externa ou 0. */
    uint16_t inner_name_index;         /**< Nome simples ou 0 (classe anônima). */
    uint16_t inner_class_access_flags; /**< Flags de acesso da classe interna. */
} InnerClassEntry;

/** @brief Tags de `verification_type_info` (StackMapTable). */
typedef enum VerificationTag {
    ITEM_Top = 0,               /**< Tipo indefinido. */
    ITEM_Integer = 1,           /**< int. */
    ITEM_Float = 2,             /**< float. */
    ITEM_Double = 3,            /**< double. */
    ITEM_Long = 4,              /**< long. */
    ITEM_Null = 5,              /**< null. */
    ITEM_UninitializedThis = 6, /**< this ainda não inicializado (em construtores). */
    ITEM_Object = 7,            /**< Objeto de uma classe (usa @c cpool_index). */
    ITEM_Uninitialized = 8      /**< Objeto criado por `new` ainda não inicializado (usa @c offset). */
} VerificationTag;

/** @brief Um `verification_type_info` do StackMapTable. */
typedef struct VerificationTypeInfo {
    uint8_t tag;    /**< #VerificationTag. */
    uint16_t value; /**< `cpool_index` (ITEM_Object), `offset` (ITEM_Uninitialized) ou 0. */
} VerificationTypeInfo;

/**
 * @brief Um quadro (`stack_map_frame`) do StackMapTable, em forma normalizada.
 *
 * @details Todas as formas da especificação (same, same_locals_1_stack_item,
 * chop, append, full_frame, ...) são representadas pela mesma estrutura;
 * @c frame_type identifica a forma original.
 */
typedef struct StackMapFrame {
    uint8_t frame_type;            /**< Tipo do quadro (0–255), conforme a especificação. */
    uint16_t offset_delta;         /**< Deslocamento em relação ao quadro anterior. */
    uint16_t number_of_locals;     /**< Locais listados (append/full_frame). */
    VerificationTypeInfo *locals;  /**< Tipos das variáveis locais listadas. */
    uint16_t number_of_stack_items; /**< Itens de pilha listados. */
    VerificationTypeInfo *stack;   /**< Tipos dos itens da pilha. */
} StackMapFrame;

/** @brief Um método de *bootstrap* do atributo BootstrapMethods. */
typedef struct BootstrapMethod {
    uint16_t bootstrap_method_ref;     /**< Índice de um `CONSTANT_MethodHandle`. */
    uint16_t num_bootstrap_arguments;  /**< Quantidade de argumentos estáticos. */
    uint16_t *bootstrap_arguments;     /**< Índices no pool dos argumentos. */
} BootstrapMethod;

/**
 * @brief Um atributo (`attribute_info`) já interpretado.
 *
 * @details O campo @c kind define qual membro da união @c info é válido.
 * Atributos desconhecidos (#ATTR_UNKNOWN) guardam os bytes brutos.
 */
typedef struct AttributeInfo {
    uint16_t attribute_name_index; /**< Nome do atributo (`CONSTANT_Utf8`). */
    uint32_t attribute_length;     /**< Tamanho do conteúdo em bytes. */
    AttributeKind kind;            /**< Tipo reconhecido. */
    /** @brief Conteúdo do atributo, conforme @c kind. */
    union {
        uint16_t constantvalue_index; /**< ConstantValue. */
        CodeAttribute *code;          /**< Code. */
        /** @brief Exceptions. */
        struct {
            uint16_t number_of_exceptions;   /**< Quantidade de exceções declaradas. */
            uint16_t *exception_index_table; /**< Índices de `CONSTANT_Class`. */
        } exceptions;
        /** @brief InnerClasses. */
        struct {
            uint16_t number_of_classes; /**< Quantidade de entradas. */
            InnerClassEntry *classes;   /**< Entradas. */
        } inner_classes;
        /** @brief EnclosingMethod. */
        struct {
            uint16_t class_index;  /**< Classe envolvente. */
            uint16_t method_index; /**< Método envolvente (`CONSTANT_NameAndType`) ou 0. */
        } enclosing_method;
        uint16_t signature_index;  /**< Signature. */
        uint16_t sourcefile_index; /**< SourceFile. */
        /** @brief LineNumberTable. */
        struct {
            uint16_t line_number_table_length; /**< Quantidade de entradas. */
            LineNumberEntry *line_number_table; /**< Entradas. */
        } line_numbers;
        /** @brief LocalVariableTable e LocalVariableTypeTable. */
        struct {
            uint16_t local_variable_table_length;   /**< Quantidade de entradas. */
            LocalVariableEntry *local_variable_table; /**< Entradas. */
        } local_variables;
        /** @brief StackMapTable. */
        struct {
            uint16_t number_of_entries; /**< Quantidade de quadros. */
            StackMapFrame *entries;     /**< Quadros. */
        } stack_map_table;
        /** @brief BootstrapMethods. */
        struct {
            uint16_t num_bootstrap_methods;     /**< Quantidade de métodos. */
            BootstrapMethod *bootstrap_methods; /**< Métodos. */
        } bootstrap;
        uint8_t *raw; /**< Bytes brutos (atributos desconhecidos). */
    } info;
} AttributeInfo;

#endif /* JVM_ATTRIBUTE_INFO_H */
