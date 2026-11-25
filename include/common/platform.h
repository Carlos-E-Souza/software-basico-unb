/**
 * @file platform.h
 * @brief Garantias de plataforma do projeto: código de 32 bits (`-m32`).
 *
 * @details A JVM é uma máquina de 32 bits: slots da pilha de operandos e das
 * variáveis locais, referências e `returnAddress` ocupam 32 bits. O projeto
 * é compilado com `-m32` (requisito R10) para que um ponteiro de C caiba em
 * um slot. Este header faz a compilação falhar se isso não for verdade, por
 * exemplo quando alguém compila sem `-m32`.
 */
#ifndef JVM_PLATFORM_H
#define JVM_PLATFORM_H

/**
 * @brief Verificação em tempo de compilação de que ponteiros têm 4 bytes.
 *
 * @details C99 não tem `_Static_assert`: um array com tamanho negativo é um
 * erro de compilação, então o `typedef` só compila se `sizeof(void *) == 4`.
 * Se este erro aparecer, compile com `-m32` (ou `i686-w64-mingw32-gcc`).
 */
typedef char platform_requires_32_bit_pointers[sizeof(void *) == 4 ? 1 : -1];

#endif /* JVM_PLATFORM_H */
