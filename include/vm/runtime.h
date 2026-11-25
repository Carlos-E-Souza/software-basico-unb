/**
 * @file runtime.h
 * @brief Tipos de tempo de execução do interpretador: slots, campos,
 *        objetos, arrays, strings, classes carregadas, frames e a VM.
 *
 * @details Desenho mínimo da seção 3.2.1 do plano:
 * - pilha de operandos e variáveis locais em slots de 32 bits (D6);
 *   `long`/`double` ocupam 2 slots (primeiro slot = 32 bits altos);
 * - uma referência cabe em um slot: o slot guarda o próprio ponteiro (D6, D16);
 * - cada slot tem uma etiqueta (#SlotTag) que diz se ele guarda uma
 *   referência ou um `returnAddress`; é a verificação defensiva mínima
 *   (I-10) que impede que um inteiro seja usado como ponteiro;
 * - campos por nome com 2 slots (D21);
 * - `String` guarda `uint16_t *chars` + `length` (D22);
 * - sem GC: todo objeto entra na lista do heap e é liberado no fim (D8).
 */
#ifndef JVM_RUNTIME_H
#define JVM_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "classfile/class_file.h"
#include "common/platform.h"

/** @brief Um slot de 32 bits da pilha de operandos ou das variáveis locais (D6). */
typedef uint32_t Slot;

/** @brief Etiqueta de um slot: o que ele guarda (verificação mínima, I-10). */
typedef enum SlotTag {
    TAG_VALUE = 0,  /**< Valor primitivo (ou slot ainda não escrito). */
    TAG_REF = 1,    /**< Referência (ponteiro para #Object ou `null`). */
    TAG_RETADDR = 2 /**< `returnAddress` empilhado por `jsr`/`jsr_w` (D10). */
} SlotTag;

/** @brief Campo de instância ou estático: nome, descritor e valor (D21). */
typedef struct Field {
    const char *name;       /**< Nome do campo (aponta para o pool da classe). */
    const char *descriptor; /**< Descritor (ex.: `I`, `J`, `Ljava/lang/String;`). */
    Slot value[2];          /**< Valor: cat. 1 em `value[0]`; `long`/`double` nos 2 slots. */
} Field;

/** @brief Tipo de um objeto do heap. */
typedef enum ObjectKind {
    OBJECT_INSTANCE, /**< Instância comum: vetor de #Field. */
    OBJECT_ARRAY,    /**< Array: tipo, tamanho e dados contíguos. */
    OBJECT_TEXT      /**< `String`, `StringBuilder` ou `StringBuffer`: UTF-16 (D22). */
} ObjectKind;

struct RuntimeClass;

/** @brief Conteúdo de um array (JVMS §2.4). */
typedef struct Array {
    char *type;           /**< Descritor do array (ex.: `[I`, `[[Ljava/lang/String;`). */
    int32_t length;       /**< Quantidade de elementos. */
    uint32_t elem_size;   /**< Tamanho real de cada elemento em bytes (1, 2, 4 ou 8). */
    void *data;           /**< Elementos contíguos (referências = ponteiros de 32 bits). */
} Array;

/** @brief Conteúdo de uma `String` ou de um `StringBuilder`/`StringBuffer` (D22). */
typedef struct Text {
    uint16_t *chars; /**< Unidades UTF-16 (o `char` do Java). */
    int32_t length;  /**< Quantidade de unidades. */
} Text;

/** @brief Objeto do heap (instância, array ou texto). */
typedef struct Object {
    ObjectKind kind;                /**< Tipo do objeto (define o membro válido de @c as). */
    struct RuntimeClass *class_ref; /**< Classe (instâncias e textos); NULL para arrays. */
    struct Object *next;            /**< Próximo objeto na lista do heap (D8). */
    /** @brief Dados conforme @c kind. */
    union {
        /** @brief Campos de instância (da classe e das superclasses, D21). */
        struct {
            uint16_t count; /**< Quantidade de campos. */
            Field *items;   /**< Campos. */
        } fields;
        Array array; /**< Array. */
        Text text;   /**< Texto. */
    } as;
} Object;

/** @brief Estado de inicialização de uma classe (`<clinit>`, JVMS §5.5). */
typedef enum ClassState {
    CLASS_LOADED,       /**< Carregada, `<clinit>` ainda não executado. */
    CLASS_INITIALIZING, /**< `<clinit>` em execução (evita recursão). */
    CLASS_INITIALIZED   /**< Pronta para uso. */
} ClassState;

/** @brief Classe carregada (do usuário ou nativa, D7). */
typedef struct RuntimeClass {
    char *name;                       /**< Nome interno (ex.: `pkg/Classe`). */
    ClassFile *class_file;            /**< Estrutura lida do `.class`; NULL para nativas. */
    struct RuntimeClass *super_class; /**< Superclasse; NULL apenas em `java/lang/Object`. */
    ObjectKind instance_kind;         /**< Tipo dos objetos criados por `new` (texto para `StringBuilder`). */
    uint16_t field_count;             /**< Campos de instância declarados nesta classe. */
    Field *fields;                    /**< Modelos (valor zero) dos campos de instância declarados. */
    uint16_t static_count;            /**< Quantidade de campos estáticos. */
    Field *statics;                   /**< Campos estáticos (D21). */
    ClassState state;                 /**< Estado da inicialização. */
    struct RuntimeClass *next;        /**< Próxima classe na lista de carregadas (D19). */
} RuntimeClass;

struct Vm;

/** @brief Frame de um método em execução (JVMS §2.6). */
typedef struct Frame {
    struct Vm *vm;            /**< Máquina virtual. */
    RuntimeClass *class_ref;  /**< Classe dona do método. */
    const MethodInfo *method; /**< Método em execução. */
    const CodeAttribute *code_attr; /**< Atributo `Code` (tabela de exceções). */
    const uint8_t *code;      /**< Bytecode do método. */
    uint32_t code_length;     /**< Tamanho do bytecode. */
    uint32_t pc;              /**< Offset da instrução atual. */
    uint32_t next_pc;         /**< Offset da próxima instrução (desvios o alteram). */
    Slot *locals;             /**< Variáveis locais. */
    uint8_t *local_tags;      /**< Etiquetas (#SlotTag) das locais. */
    uint16_t max_locals;      /**< Quantidade de variáveis locais. */
    Slot *stack;              /**< Pilha de operandos. */
    uint8_t *stack_tags;      /**< Etiquetas (#SlotTag) da pilha. */
    uint16_t max_stack;       /**< Capacidade da pilha de operandos. */
    uint16_t sp;              /**< Quantidade de slots na pilha. */
    bool returned;            /**< `true` depois de uma instrução `*return`. */
    Slot result[2];           /**< Valor de retorno (cat. 2 usa os 2 slots). */
    uint8_t result_tag;       /**< Etiqueta do valor de retorno (#TAG_REF em `areturn`). */
} Frame;

/** @brief Estado global da máquina virtual (monothread, D9). */
typedef struct Vm {
    const char *classpath;    /**< Diretório dos `.class`. */
    FILE *out;                /**< Saída de `System.out` (stdout; arquivo nos testes). */
    RuntimeClass *classes;    /**< Lista de classes carregadas (D19). */
    Object *heap;             /**< Lista de todos os objetos alocados (D8). */
    size_t heap_bytes;        /**< Bytes alocados no heap (limite contra falta de memória). */
    Object **interned;        /**< Literais `String` internados. */
    size_t interned_count;    /**< Quantidade de literais internados. */
    Object *exception;        /**< Exceção sendo propagada (NULL = nenhuma). */
    bool fatal;               /**< Erro fatal: a execução para. */
    int exit_code;            /**< Código de saída do erro fatal. */
    bool quiet;               /**< Não imprime mensagens de erro fatal (fuzzing). */
    uint32_t depth;           /**< Profundidade atual da pilha de frames. */
    uint32_t max_steps;       /**< Limite de instruções (0 = sem limite; usado no fuzzing). */
    uint32_t steps;           /**< Instruções executadas. */
} Vm;

/** @brief Função que executa uma instrução (D18). */
typedef void (*OpcodeHandler)(Frame *frame);

/**
 * @brief Método nativo (D7): recebe a VM, o descritor e os argumentos (com
 *        `this` em `args[0]` nos métodos de instância) e grava o retorno em
 *        @c result (cat. 2 usa os 2 slots). Lança exceções com vm_throw().
 */
typedef void (*NativeMethod)(Vm *vm, const char *descriptor, const Slot *args,
                             Slot *result);

/** @brief Método resolvido: do `.class` (@c method) ou nativo (@c native). */
typedef struct MethodRef {
    RuntimeClass *owner;      /**< Classe que declara o método. */
    const MethodInfo *method; /**< Método do `.class` (NULL se nativo). */
    NativeMethod native;      /**< Função nativa (NULL se do `.class`). */
    bool is_static;           /**< Método estático (`ACC_STATIC` ou nativo estático). */
} MethodRef;

#endif /* JVM_RUNTIME_H */
