/**
 * @file disassembler.h
 * @brief Desmontagem do bytecode de um método em mnemônicos legíveis.
 */
#ifndef JVM_DISASSEMBLER_H
#define JVM_DISASSEMBLER_H

#include <stdio.h>

#include "classfile/class_file.h"

/**
 * @brief Escreve o bytecode de um atributo Code, uma instrução por linha.
 *
 * @details
 * Formato de cada linha: `pc: mnemônico operandos // comentário`, em que:
 * - índices do pool aparecem como `#n` e o comentário mostra a constante
 *   resolvida (ex.: `// Method java/io/PrintStream.println:(I)V`);
 * - desvios mostram o endereço absoluto de destino;
 * - `tableswitch`/`lookupswitch` listam cada caso e o `default`;
 * - instruções com prefixo `wide` aparecem com sufixo `_w` (ex.: `iinc_w`),
 *   como no `javap`.
 *
 * **Algoritmo:**
 * 1. Começa em pc = 0 e, enquanto pc < `code_length`, calcula o tamanho da
 *    instrução (bytecode_instruction_length());
 * 2. Se o tamanho for 0 (opcode inválido ou instrução truncada), escreve um
 *    aviso e interrompe;
 * 3. Caso contrário, escreve o mnemônico e os operandos conforme o
 *    formato do opcode e avança pc.
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe dona do método (para resolver o pool).
 * @param[in] code       Atributo Code a desmontar.
 * @param[in] indent     Indentação, em espaços, de cada linha.
 */
void disassemble_code(FILE *out, const ClassFile *class_file,
                      const CodeAttribute *code, int indent);

#endif /* JVM_DISASSEMBLER_H */
