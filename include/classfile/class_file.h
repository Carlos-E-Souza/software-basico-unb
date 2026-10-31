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
 * Organização da leitura:
 * - class_file.c: estrutura principal, interfaces, campos e métodos;
 * - constant_pool.c: pool de constantes e funções de consulta;
 * - attributes.c: atributos (Code, ConstantValue, StackMapTable, ...).
 */
#ifndef JVM_CLASS_FILE_H
#define JVM_CLASS_FILE_H

#include <stdint.h>

#include "classfile/cf_error.h"

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
/*                          Pool de constantes (JVMS 4.4)                   */
/* ======================================================================== */

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

/* ======================================================================== */
/*                               Atributos (JVMS 4.7)                       */
/* ======================================================================== */

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
