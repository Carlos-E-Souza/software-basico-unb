/**
 * @file native_print.c
 * @brief Simulação de `System.out.print`/`println` (`java/io/PrintStream`)
 *        para todos os tipos (I-05, I-08).
 *
 * @details O texto de cada valor vem de natives_to_string() (as mesmas
 * regras de `String.valueOf`), e é escrito em UTF-8 em `vm->out`.
 */
#include "vm/interpreter.h"
#include "vm/natives.h"

/**
 * @brief Escreve o valor do parâmetro (se houver) e, opcionalmente, `\n`.
 *
 * @details **Algoritmo:** se o descritor tem parâmetro, converte-o com
 * natives_to_string() e escreve o texto; depois escreve a quebra de linha
 * se @p newline for verdadeiro.
 *
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor (`()V` ou `(x)V`).
 * @param[in]     args       `this` (o `PrintStream`) e o valor.
 * @param[in]     newline    `true` em `println`.
 */
static void print_value(Vm *vm, const char *descriptor, const Slot *args, bool newline)
{
    if (descriptor[1] != ')') {
        Object *text = natives_to_string(vm, descriptor + 1, args + 1);
        if (text == NULL || natives_text(vm, text) == NULL) {
            return;
        }
        natives_write_utf16(vm->out, text->as.text.chars, text->as.text.length);
    }
    if (newline) {
        fputc('\n', vm->out);
    }
}

/**
 * @brief `PrintStream.print(x)V`.
 * @details **Algoritmo:** print_value() sem quebra de linha.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this` e o valor.
 * @param[out]    result     Sem retorno.
 */
static void print(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    (void)result;
    print_value(vm, descriptor, args, false);
}

/**
 * @brief `PrintStream.println()V` e `PrintStream.println(x)V`.
 * @details **Algoritmo:** print_value() com quebra de linha.
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     descriptor Descritor.
 * @param[in]     args       `this` e o valor.
 * @param[out]    result     Sem retorno.
 */
static void println(Vm *vm, const char *descriptor, const Slot *args, Slot *result)
{
    (void)result;
    print_value(vm, descriptor, args, true);
}

/** @brief Gera as linhas `print(x)` e `println(x)` para um tipo de parâmetro. */
#define PRINT_ENTRIES(type)                                     \
    {"java/io/PrintStream", "print", "(" type ")V", print, false},     \
    {"java/io/PrintStream", "println", "(" type ")V", println, false}

const NativeEntry native_print_methods[] = {
    {"java/io/PrintStream", "println", "()V", println, false},
    PRINT_ENTRIES("I"),
    PRINT_ENTRIES("J"),
    PRINT_ENTRIES("F"),
    PRINT_ENTRIES("D"),
    PRINT_ENTRIES("C"),
    PRINT_ENTRIES("Z"),
    PRINT_ENTRIES("[C"),
    PRINT_ENTRIES("Ljava/lang/String;"),
    PRINT_ENTRIES("Ljava/lang/Object;"),
    {NULL, NULL, NULL, NULL, false}
};
