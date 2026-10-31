/**
 * @file opcodes.h
 * @brief Tabela das instruções da JVM: mnemônico, formato dos operandos e tamanho.
 *
 * @details Compartilhada pelo exibidor (desmontagem do bytecode) e pelo
 * interpretador. Cobre os 202 opcodes definidos de 0x00 (`nop`) a 0xC9
 * (`jsr_w`). Os opcodes reservados (0xCA `breakpoint`, 0xFE/0xFF `impdep`) e
 * os não atribuídos são tratados como inválidos.
 */
#ifndef JVM_OPCODES_H
#define JVM_OPCODES_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Formato dos operandos que seguem um opcode no bytecode. */
typedef enum OperandKind {
    OPK_INVALID = 0,     /**< Opcode não definido. */
    OPK_NONE,            /**< Sem operandos. */
    OPK_S1,              /**< 1 byte com sinal (`bipush`). */
    OPK_S2,              /**< 2 bytes com sinal (`sipush`). */
    OPK_LOCAL,           /**< Índice de variável local u1 (`iload`, `astore`, `ret`, ...). */
    OPK_CP_U1,           /**< Índice do pool u1 (`ldc`). */
    OPK_CP_U2,           /**< Índice do pool u2 (`ldc_w`, `getfield`, `invokevirtual`, `new`, ...). */
    OPK_IINC,            /**< Índice u1 + constante s1 (`iinc`). */
    OPK_BRANCH_S2,       /**< Deslocamento de desvio s2 (`ifeq`, `goto`, `jsr`, ...). */
    OPK_BRANCH_S4,       /**< Deslocamento de desvio s4 (`goto_w`, `jsr_w`). */
    OPK_NEWARRAY,        /**< Tipo primitivo u1 (`newarray`). */
    OPK_INVOKEINTERFACE, /**< Índice u2 + contagem u1 + zero u1. */
    OPK_INVOKEDYNAMIC,   /**< Índice u2 + dois bytes zero. */
    OPK_MULTIANEWARRAY,  /**< Índice u2 + dimensões u1. */
    OPK_TABLESWITCH,     /**< Alinhamento + default, low, high e tabela de deslocamentos. */
    OPK_LOOKUPSWITCH,    /**< Alinhamento + default, npairs e pares (chave, deslocamento). */
    OPK_WIDE             /**< Prefixo que estende o índice da instrução seguinte. */
} OperandKind;

/** @brief Descrição de um opcode. */
typedef struct OpcodeInfo {
    const char *mnemonic; /**< Mnemônico (NULL se o opcode é inválido). */
    OperandKind kind;     /**< Formato dos operandos. */
} OpcodeInfo;

/** @brief Opcode do prefixo `wide`. */
#define OPCODE_WIDE 0xC4u
/** @brief Opcode de `iinc` (forma estendida tem 2 operandos u2 quando precedido de `wide`). */
#define OPCODE_IINC 0x84u

/**
 * @brief Retorna a descrição de um opcode.
 *
 * @details **Algoritmo:** acesso direto a uma tabela de 256 posições.
 *
 * @param[in] opcode Byte do opcode.
 * @return Ponteiro para a descrição (nunca NULL; `mnemonic` é NULL e `kind`
 *         é #OPK_INVALID para opcodes inválidos).
 */
const OpcodeInfo *opcode_info(uint8_t opcode);

/**
 * @brief Retorna o nome de um tipo primitivo de `newarray` (4 = boolean ... 11 = long).
 *
 * @details **Algoritmo:** tabela indexada por `atype - 4`.
 *
 * @param[in] atype Código do tipo (JVMS §6.5 newarray).
 * @return Nome do tipo, ou NULL se @p atype for inválido.
 */
const char *opcode_newarray_type_name(uint8_t atype);

/**
 * @brief Lê um inteiro de 16 bits com sinal em big-endian.
 *
 * @details **Algoritmo:** combina os bytes em `uint16_t` e converte para
 * complemento de dois sem depender de comportamento definido pela implementação.
 *
 * @param[in] p Ponteiro para os 2 bytes.
 * @return Valor lido.
 */
int16_t bytecode_read_s2(const uint8_t *p);

/**
 * @brief Lê um inteiro de 32 bits com sinal em big-endian.
 *
 * @details **Algoritmo:** combina os 4 bytes em `uint32_t` e converte para
 * complemento de dois.
 *
 * @param[in] p Ponteiro para os 4 bytes.
 * @return Valor lido.
 */
int32_t bytecode_read_s4(const uint8_t *p);

/**
 * @brief Calcula o tamanho (em bytes) da instrução que começa em @p pc.
 *
 * @details
 * **Algoritmo:**
 * 1. Opcodes inválidos retornam 0;
 * 2. Formatos fixos retornam 1 + tamanho dos operandos;
 * 3. `tableswitch`/`lookupswitch`: calcula o alinhamento até o próximo
 *    múltiplo de 4 (contado desde o início do código), lê `low/high` ou
 *    `npairs` e soma o tamanho da tabela; valores incoerentes retornam 0;
 * 4. `wide`: 4 bytes para loads/stores/`ret` e 6 para `iinc`; outros, 0.
 * 5. Se a instrução ultrapassar @p code_length, retorna 0.
 *
 * @param[in] code        Bytecode do método.
 * @param[in] code_length Tamanho do bytecode.
 * @param[in] pc          Posição do opcode.
 * @return Tamanho da instrução, ou 0 se ela for inválida ou estiver truncada.
 */
uint32_t bytecode_instruction_length(const uint8_t *code, uint32_t code_length,
                                     uint32_t pc);

#endif /* JVM_OPCODES_H */
