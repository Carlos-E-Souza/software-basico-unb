/**
 * @file disassembler.c
 * @brief Implementação da desmontagem do bytecode.
 */
#include <inttypes.h>

#include "bytecode/opcodes.h"
#include "viewer/disassembler.h"
#include "viewer/format.h"

/** @brief Largura da coluna do mnemônico (alinha os operandos). */
#define MNEMONIC_WIDTH 16

/**
 * @brief Escreve o comentário `// Tipo constante` de um operando do pool.
 *
 * @details **Algoritmo:** escreve o rótulo do tipo (constant_kind_label())
 * e a constante resolvida (print_constant()).
 *
 * @param[in] out        Fluxo de saída.
 * @param[in] class_file Classe.
 * @param[in] index      Índice no pool.
 */
static void print_cp_comment(FILE *out, const ClassFile *class_file, uint16_t index)
{
    fprintf(out, " // %s ", constant_kind_label(class_file, index));
    print_constant(out, class_file, index);
}

/**
 * @brief Escreve os casos de um `tableswitch` ou `lookupswitch`.
 *
 * @details
 * **Algoritmo:**
 * 1. Pula o alinhamento até o próximo múltiplo de 4 e lê `default`;
 * 2. `tableswitch`: lê `low`/`high` e escreve `chave: destino` para cada
 *    chave de `low` a `high`;
 * 3. `lookupswitch`: lê `npairs` e escreve cada par `chave: destino`;
 * 4. Escreve `default: destino` e fecha o bloco. Destinos são absolutos
 *    (pc da instrução + deslocamento).
 *
 * O tamanho da instrução já foi validado por bytecode_instruction_length().
 *
 * @param[in] out    Fluxo de saída.
 * @param[in] code   Bytecode.
 * @param[in] pc     Posição do opcode.
 * @param[in] table  `true` para tableswitch.
 * @param[in] indent Indentação das linhas.
 */
static void print_switch(FILE *out, const uint8_t *code, uint32_t pc, bool table,
                         int indent)
{
    uint32_t base = pc + 1 + ((4u - ((pc + 1) % 4u)) % 4u);
    int64_t target_default = (int64_t)pc + bytecode_read_s4(code + base);
    int32_t i;

    if (table) {
        int32_t low = bytecode_read_s4(code + base + 4);
        int32_t high = bytecode_read_s4(code + base + 8);
        const uint8_t *offsets = code + base + 12;
        fprintf(out, "{ // %" PRId32 " a %" PRId32 "\n", low, high);
        for (i = 0; (int64_t)low + i <= high; i++) {
            print_indent(out, indent + 12);
            fprintf(out, "%11" PRId64 ": %" PRId64 "\n", (int64_t)low + i,
                    (int64_t)pc + bytecode_read_s4(offsets + 4 * (uint32_t)i));
        }
    } else {
        int32_t npairs = bytecode_read_s4(code + base + 4);
        const uint8_t *pairs = code + base + 8;
        fprintf(out, "{ // %" PRId32 " par(es)\n", npairs);
        for (i = 0; i < npairs; i++) {
            print_indent(out, indent + 12);
            fprintf(out, "%11" PRId32 ": %" PRId64 "\n",
                    bytecode_read_s4(pairs + 8 * (uint32_t)i),
                    (int64_t)pc + bytecode_read_s4(pairs + 8 * (uint32_t)i + 4));
        }
    }
    print_indent(out, indent + 12);
    fprintf(out, "%11s: %" PRId64 "\n", "default", target_default);
    print_indent(out, indent + 6);
    fputs("}", out);
}

/**
 * @brief Escreve os operandos de uma instrução conforme o formato do opcode.
 *
 * @details **Algoritmo:** `switch` sobre o #OperandKind lendo os bytes que
 * seguem o opcode (inteiros com sinal via bytecode_read_s2()/bytecode_read_s4())
 * e escrevendo-os no formato do `javap`.
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
        fprintf(out, "#%u", (unsigned)op[0]);
        print_cp_comment(out, class_file, op[0]);
        break;
    case OPK_CP_U2:
        index = (uint16_t)((op[0] << 8) | op[1]);
        fprintf(out, "#%u", (unsigned)index);
        print_cp_comment(out, class_file, index);
        break;
    case OPK_IINC:
        fprintf(out, "%u, %d", (unsigned)op[0],
                (int)(op[1] <= 0x7F ? op[1] : op[1] - 256));
        break;
    case OPK_BRANCH_S2:
        fprintf(out, "%" PRId64, (int64_t)pc + bytecode_read_s2(op));
        break;
    case OPK_BRANCH_S4:
        fprintf(out, "%" PRId64, (int64_t)pc + bytecode_read_s4(op));
        break;
    case OPK_NEWARRAY: {
        const char *type = opcode_newarray_type_name(op[0]);
        if (type != NULL) {
            fputs(type, out);
        } else {
            fprintf(out, "<tipo invalido %u>", (unsigned)op[0]);
        }
        break;
    }
    case OPK_INVOKEINTERFACE:
        index = (uint16_t)((op[0] << 8) | op[1]);
        fprintf(out, "#%u, %u", (unsigned)index, (unsigned)op[2]);
        print_cp_comment(out, class_file, index);
        break;
    case OPK_INVOKEDYNAMIC:
        index = (uint16_t)((op[0] << 8) | op[1]);
        fprintf(out, "#%u, 0", (unsigned)index);
        print_cp_comment(out, class_file, index);
        break;
    case OPK_MULTIANEWARRAY:
        index = (uint16_t)((op[0] << 8) | op[1]);
        fprintf(out, "#%u, %u", (unsigned)index, (unsigned)op[2]);
        print_cp_comment(out, class_file, index);
        break;
    case OPK_TABLESWITCH:
        print_switch(out, code, pc, true, indent);
        break;
    case OPK_LOOKUPSWITCH:
        print_switch(out, code, pc, false, indent);
        break;
    case OPK_WIDE: {
        unsigned local = (unsigned)((op[1] << 8) | op[2]);
        if (op[0] == OPCODE_IINC) {
            fprintf(out, "%u, %d", local, (int)bytecode_read_s2(op + 3));
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
            fprintf(out, "%5u: <instrucao invalida ou truncada: opcode 0x%02X>\n",
                    (unsigned)pc, (unsigned)code->code[pc]);
            return;
        }
        if (info->kind == OPK_NONE) {
            fprintf(out, "%5u: %s\n", (unsigned)pc, info->mnemonic);
        } else if (info->kind == OPK_WIDE) {
            /* como no javap: "wide iinc" aparece como "iinc_w" */
            char name[32];
            snprintf(name, sizeof name, "%s_w", opcode_info(code->code[pc + 1])->mnemonic);
            fprintf(out, "%5u: %-*s ", (unsigned)pc, MNEMONIC_WIDTH - 1, name);
            print_operands(out, class_file, code->code, pc, info, indent);
            fputc('\n', out);
        } else {
            fprintf(out, "%5u: %-*s ", (unsigned)pc, MNEMONIC_WIDTH - 1, info->mnemonic);
            print_operands(out, class_file, code->code, pc, info, indent);
            fputc('\n', out);
        }
        pc += length;
    }
}
