/**
 * @file print_stack_map.c
 * @brief Exibição dos quadros do atributo StackMapTable.
 */
#include "viewer/printer_internal.h"

/**
 * @brief Escreve um `verification_type_info` (ex.: `int`, `Object cp_info #5 <[[I>`).
 *
 * @details **Algoritmo:** `switch` sobre a tag; ITEM_Object mostra a classe
 * como referência ao pool e ITEM_Uninitialized o offset do `new`.
 *
 * @param[in] out  Fluxo de saída.
 * @param[in] cf   Classe.
 * @param[in] info Item a escrever.
 */
static void print_verification_type(FILE *out, const ClassFile *cf,
                                    const VerificationTypeInfo *info)
{
    static const char *const names[] = {
        "top", "int", "float", "double", "long", "null", "uninitialized_this"
    };
    if (info->tag == ITEM_Object) {
        fputs("Object ", out);
        print_cp_ref(out, cf, info->value);
    } else if (info->tag == ITEM_Uninitialized) {
        fprintf(out, "uninitialized(offset %u)", (unsigned)info->value);
    } else {
        fputs(names[info->tag], out);
    }
}

/**
 * @brief Escreve uma lista de `verification_type_info` entre colchetes.
 *
 * @details **Algoritmo:** escreve os itens separados por vírgula.
 *
 * @param[in] out   Fluxo de saída.
 * @param[in] cf    Classe.
 * @param[in] items Itens.
 * @param[in] count Quantidade de itens.
 */
static void print_verification_list(FILE *out, const ClassFile *cf,
                                    const VerificationTypeInfo *items, uint16_t count)
{
    uint16_t i;
    fputs("[", out);
    for (i = 0; i < count; i++) {
        fputs(i > 0 ? ", " : "", out);
        print_verification_type(out, cf, &items[i]);
    }
    fputs("]", out);
}

/**
 * @brief Retorna o nome da forma de um quadro do StackMapTable.
 *
 * @details **Algoritmo:** classifica `frame_type` nas faixas da JVMS §4.7.4.
 *
 * @param[in] type `frame_type`.
 * @return Nome da forma (ex.: "same", "append").
 */
static const char *frame_type_name(uint8_t type)
{
    if (type <= 63) return "same";
    if (type <= 127) return "same_locals_1_stack_item";
    if (type == 247) return "same_locals_1_stack_item_extended";
    if (type >= 248 && type <= 250) return "chop";
    if (type == 251) return "same_frame_extended";
    if (type >= 252 && type <= 254) return "append";
    if (type == 255) return "full_frame";
    return "reservado";
}

void print_stack_map_table(FILE *out, const ClassFile *cf,
                           const AttributeInfo *attr, int indent)
{
    uint16_t i;
    uint32_t pc = 0;
    print_number_line(out, indent, "Number of entries",
                      attr->info.stack_map_table.number_of_entries);
    for (i = 0; i < attr->info.stack_map_table.number_of_entries; i++) {
        const StackMapFrame *f = &attr->info.stack_map_table.entries[i];
        pc = (i == 0) ? f->offset_delta : pc + f->offset_delta + 1u;
        print_indent(out, indent);
        fprintf(out, "[%u] pc %lu: %s (frame_type = %u, offset_delta = %u)\n",
                (unsigned)i, (unsigned long)pc, frame_type_name(f->frame_type),
                (unsigned)f->frame_type, (unsigned)f->offset_delta);
        if (f->number_of_locals > 0 || f->frame_type == 255) {
            print_indent(out, indent + 4);
            fputs("locals = ", out);
            print_verification_list(out, cf, f->locals, f->number_of_locals);
            fputc('\n', out);
        }
        if (f->number_of_stack_items > 0 || f->frame_type == 255) {
            print_indent(out, indent + 4);
            fputs("stack  = ", out);
            print_verification_list(out, cf, f->stack, f->number_of_stack_items);
            fputc('\n', out);
        }
    }
}
