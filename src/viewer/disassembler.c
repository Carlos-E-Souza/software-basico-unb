/**
 * @file disassembler.c
 * @brief Implementação da desmontagem do bytecode no estilo do jclasslib.
 */
#include <inttypes.h>

#include "bytecode/opcodes.h"
#include "viewer/disassembler.h"
#include "viewer/format.h"

/**
 * @brief Escreve um operando que aponta para o pool: `#n <constante>`.
 *
 * @details **Algoritmo:** escreve o índice e a constante resolvida
 * (print_constant()) entre `<` e `>`.
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe.
 * @param[in] index      Índice no pool.
 */
static void print_cp_operand(FILE *out, const ClassFile *class_file, uint16_t index)
{
    fprintf(out, "#%u <", (unsigned)index);
    print_constant(out, class_file, index);
    fputc('>', out);
}

/**
 * @brief Escreve o destino de um desvio: `alvo (+deslocamento)`.
 *
 * @details **Algoritmo:** o alvo absoluto é `pc + deslocamento`; o
 * deslocamento é escrito com sinal explícito, como no jclasslib.
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] pc     Posição da instrução de desvio.
 * @param[in] offset Deslocamento relativo lido do bytecode.
 */
static void print_branch(FILE *out, uint32_t pc, int32_t offset)
{
    fprintf(out, "%" PRId64 " (%+" PRId32 ")", (int64_t)pc + offset, offset);
}

/**
 * @brief Escreve os casos de um `tableswitch` ou `lookupswitch`.
 *
 * @details
 * **Algoritmo:**
 * 1. Pula o alinhamento até o próximo múltiplo de 4 e lê `default`;
 * 2. `tableswitch`: lê `low`/`high`, escreve `low to high` na linha da
 *    instrução e uma linha `chave: destino (+deslocamento)` para cada chave;
 * 3. `lookupswitch`: lê `npairs`, escreve-o na linha da instrução e uma
 *    linha por par;
 * 4. Escreve a linha `default: destino (+deslocamento)`.
 *
 * O tamanho da instrução já foi validado por bytecode_instruction_length().
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] code   Bytecode.
 * @param[in] pc     Posição do opcode.
 * @param[in] table  `true` para tableswitch.
 * @param[in] indent Indentação das linhas de casos.
 */
static void print_switch(FILE *out, const uint8_t *code, uint32_t pc, bool table,
                         int indent)
{
    uint32_t base = pc + 1 + ((4u - ((pc + 1) % 4u)) % 4u);
    int32_t default_offset = bytecode_read_s4(code + base);
    int32_t i;

    if (table) {
        int32_t low = bytecode_read_s4(code + base + 4);
        int32_t high = bytecode_read_s4(code + base + 8);
        const uint8_t *offsets = code + base + 12;
        fprintf(out, "%" PRId32 " to %" PRId32 "\n", low, high);
        for (i = 0; (int64_t)low + i <= high; i++) {
            print_indent(out, indent + 8);
            fprintf(out, "%11" PRId64 ":  ", (int64_t)low + i);
            print_branch(out, pc, bytecode_read_s4(offsets + 4 * (uint32_t)i));
            fputc('\n', out);
        }
    } else {
        int32_t npairs = bytecode_read_s4(code + base + 4);
        const uint8_t *pairs = code + base + 8;
        fprintf(out, "%" PRId32 "\n", npairs);
        for (i = 0; i < npairs; i++) {
            print_indent(out, indent + 8);
            fprintf(out, "%11" PRId32 ":  ", bytecode_read_s4(pairs + 8 * (uint32_t)i));
            print_branch(out, pc, bytecode_read_s4(pairs + 8 * (uint32_t)i + 4));
            fputc('\n', out);
        }
    }
    print_indent(out, indent + 8);
    fprintf(out, "%11s:  ", "default");
    print_branch(out, pc, default_offset);
}

/**
 * @brief Escreve os operandos de uma instrução conforme o formato do opcode.
 *
 * @details **Algoritmo:** `switch` sobre o #OperandKind lendo os bytes que
 * seguem o opcode e escrevendo-os no formato do jclasslib: operandos do pool
 * como `#n <constante>`, desvios como `alvo (+deslocamento)`, `iinc` como
 * `índice by constante`, `newarray` como `código (tipo)`, `invokeinterface`
 * com `count` e `multianewarray` com `dim`.
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe.
 * @param[in] code       Bytecode.
 * @param[in] pc         Posição do opcode.
 * @param[in] info       Descrição do opcode.
 * @param[in] indent     Indentação (usada pelos switches).
 */
static void print_operands(FILE *out, const ClassFile *class_file,
                           const uint8_t *code, uint32_t pc,
                           const OpcodeInfo *info, int indent)
{
    const uint8_t *op = code + pc + 1;
    uint16_t index;
    switch (info->kind) {
    case OPK_S1:
        fprintf(out, "%d", op[0] <= 0x7F ? (int)op[0] : (int)op[0] - 256);
        break;
    case OPK_S2:
        fprintf(out, "%d", (int)bytecode_read_s2(op));
        break;
    case OPK_LOCAL:
        fprintf(out, "%u", (unsigned)op[0]);
        break;
    case OPK_CP_U1:
        print_cp_operand(out, class_file, op[0]);
        break;
    case OPK_CP_U2:
        index = (uint16_t)((op[0] << 8) | op[1]);
        print_cp_operand(out, class_file, index);
        break;
    case OPK_IINC:
        fprintf(out, "%u by %d", (unsigned)op[0],
                op[1] <= 0x7F ? (int)op[1] : (int)op[1] - 256);
        break;
    case OPK_BRANCH_S2:
        print_branch(out, pc, bytecode_read_s2(op));
        break;
    case OPK_BRANCH_S4:
        print_branch(out, pc, bytecode_read_s4(op));
        break;
    case OPK_NEWARRAY: {
        const char *type = opcode_newarray_type_name(op[0]);
        fprintf(out, "%u (%s)", (unsigned)op[0], type != NULL ? type : "tipo invalido");
        break;
    }
    case OPK_INVOKEINTERFACE:
        index = (uint16_t)((op[0] << 8) | op[1]);
        print_cp_operand(out, class_file, index);
        fprintf(out, " count %u", (unsigned)op[2]);
        break;
    case OPK_INVOKEDYNAMIC:
        index = (uint16_t)((op[0] << 8) | op[1]);
        print_cp_operand(out, class_file, index);
        break;
    case OPK_MULTIANEWARRAY:
        index = (uint16_t)((op[0] << 8) | op[1]);
        print_cp_operand(out, class_file, index);
        fprintf(out, " dim %u", (unsigned)op[2]);
        break;
    case OPK_TABLESWITCH:
        print_switch(out, code, pc, true, indent);
        break;
    case OPK_LOOKUPSWITCH:
        print_switch(out, code, pc, false, indent);
        break;
    case OPK_WIDE: {
        unsigned local = (unsigned)((op[1] << 8) | op[2]);
        fprintf(out, "%s ", opcode_info(op[0])->mnemonic);
        if (op[0] == OPCODE_IINC) {
            fprintf(out, "%u by %d", local, (int)bytecode_read_s2(op + 3));
        } else {
            fprintf(out, "%u", local);
        }
        break;
    }
    case OPK_NONE:
    case OPK_INVALID:
        break;
    }
}

void disassemble_code(FILE *out, const ClassFile *class_file,
                      const CodeAttribute *code, int indent)
{
    uint32_t pc = 0;
    while (pc < code->code_length) {
        uint32_t length = bytecode_instruction_length(code->code, code->code_length, pc);
        const OpcodeInfo *info = opcode_info(code->code[pc]);
        print_indent(out, indent);
        if (length == 0) {
            fprintf(out, "%5u <instrucao invalida ou truncada: opcode 0x%02X>\n",
                    (unsigned)pc, (unsigned)code->code[pc]);
            return;
        }
        fprintf(out, "%5u %s", (unsigned)pc, info->mnemonic);
        if (info->kind != OPK_NONE) {
            fputc(' ', out);
            print_operands(out, class_file, code->code, pc, info, indent);
        }
        fputc('\n', out);
        pc += length;
    }
}
