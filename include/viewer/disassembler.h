/**
 * @file disassembler.h
 * @brief Desmontagem do bytecode de um método em mnemônicos legíveis,
 *        no estilo do jclasslib.
 */
#ifndef JVM_DISASSEMBLER_H
#define JVM_DISASSEMBLER_H

#include <stdio.h>

#include "classfile/class_file.h"

/**
 * @brief Escreve o bytecode de um atributo Code, uma instrução por linha.
 *
 * @details
 * Formato de cada linha (como no jclasslib): `pc mnemônico argumentos`, em que:
 * - argumentos que apontam para o pool aparecem como `#n <constante>`, com o
 *   valor da constante resolvido (ex.:
 *   `invokevirtual #4 <java/io/PrintStream.println : (I)V>`,
 *   `ldc #3 <divisao por zero>`, `new #2 <Flow$Failure>`);
 * - desvios mostram o destino absoluto e o deslocamento: `ifne 14 (+13)`;
 * - `iinc 1 by 5`, `newarray 10 (int)`, `invokeinterface ... count 2`,
 *   `multianewarray ... dim 2`;
 * - `tableswitch low to high` e `lookupswitch npairs` são seguidos de uma
 *   linha por caso e da linha `default`;
 * - instruções com prefixo `wide` aparecem como `wide iinc 300 by 1000`.
 *
 * **Algoritmo:**
 * 1. Começa em pc = 0 e, enquanto pc < `code_length`, calcula o tamanho da
 *    instrução (bytecode_instruction_length());
 * 2. Se o tamanho for 0 (opcode inválido ou instrução truncada), escreve um
 *    aviso e interrompe;
 * 3. Caso contrário, escreve o mnemônico e os argumentos conforme o
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
