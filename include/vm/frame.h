/**
 * @file frame.h
 * @brief Frames: criação, pilha de operandos (cat. 1 e cat. 2), variáveis
 *        locais e leitura dos operandos do bytecode (I-01, D6).
 *
 * @details Toda operação inválida (pilha cheia/vazia, local fora de
 * `max_locals`, referência esperada e não encontrada) é um erro fatal
 * (vm_fatal()), nunca um acesso fora da memória (I-10). Depois de um erro
 * fatal as funções devolvem 0/NULL e o laço do interpretador para.
 */
#ifndef JVM_FRAME_H
#define JVM_FRAME_H

#include "vm/runtime.h"

/**
 * @brief Cria o frame de um método com locais e pilha do atributo `Code`.
 *
 * @details **Algoritmo:** aloca a estrutura, as locais (`max_locals`) e a
 * pilha (`max_stack`), com as etiquetas zeradas (#TAG_VALUE), e posiciona
 * o `pc` em 0.
 *
 * @param[in] vm        Máquina virtual.
 * @param[in] class_ref Classe dona do método.
 * @param[in] method    Método.
 * @param[in] code      Atributo `Code` do método.
 * @return Frame alocado (liberar com frame_free()); NULL sem memória.
 */
Frame *frame_create(Vm *vm, RuntimeClass *class_ref, const MethodInfo *method,
                    const CodeAttribute *code);

/**
 * @brief Libera um frame.
 *
 * @details **Algoritmo:** libera locais, pilha, etiquetas e a estrutura.
 *
 * @param[in] frame Frame (pode ser NULL).
 */
void frame_free(Frame *frame);

/**
 * @brief Empilha um slot com a sua etiqueta.
 *
 * @details **Algoritmo:** se `sp == max_stack`, erro fatal; senão grava
 * valor e etiqueta no topo e incrementa `sp`.
 *
 * @param[in,out] frame Frame.
 * @param[in]     value Valor.
 * @param[in]     tag   Etiqueta (#SlotTag).
 */
void frame_push(Frame *frame, Slot value, SlotTag tag);

/**
 * @brief Desempilha um slot e a sua etiqueta.
 *
 * @details **Algoritmo:** se a pilha está vazia, erro fatal e devolve 0;
 * senão decrementa `sp` e devolve o topo.
 *
 * @param[in,out] frame Frame.
 * @param[out]    tag   Etiqueta do slot (pode ser NULL).
 * @return Valor do slot.
 */
Slot frame_pop(Frame *frame, uint8_t *tag);

/**
 * @brief Empilha um `int` (cat. 1).
 * @details **Algoritmo:** converte para os bits sem sinal e chama frame_push().
 * @param[in,out] frame Frame.
 * @param[in]     value Valor.
 */
void frame_push_int(Frame *frame, int32_t value);

/**
 * @brief Desempilha um `int` (cat. 1).
 * @details **Algoritmo:** frame_pop() e conversão com slot_to_int().
 * @param[in,out] frame Frame.
 * @return Valor.
 */
int32_t frame_pop_int(Frame *frame);

/**
 * @brief Empilha um `float` (cat. 1).
 * @details **Algoritmo:** copia os bits IEEE 754 para um slot.
 * @param[in,out] frame Frame.
 * @param[in]     value Valor.
 */
void frame_push_float(Frame *frame, float value);

/**
 * @brief Desempilha um `float` (cat. 1).
 * @details **Algoritmo:** desempilha um slot e copia os bits para um `float`.
 * @param[in,out] frame Frame.
 * @return Valor.
 */
float frame_pop_float(Frame *frame);

/**
 * @brief Empilha um `long` (cat. 2, dois slots: alto e depois baixo).
 * @details **Algoritmo:** empilha os 32 bits altos e depois os 32 baixos.
 * @param[in,out] frame Frame.
 * @param[in]     value Valor.
 */
void frame_push_long(Frame *frame, int64_t value);

/**
 * @brief Desempilha um `long` (cat. 2).
 * @details **Algoritmo:** desempilha os 32 bits baixos, depois os altos, e
 * junta-os com slots_to_long().
 * @param[in,out] frame Frame.
 * @return Valor.
 */
int64_t frame_pop_long(Frame *frame);

/**
 * @brief Empilha um `double` (cat. 2).
 * @details **Algoritmo:** copia os bits IEEE 754 e empilha como `long`.
 * @param[in,out] frame Frame.
 * @param[in]     value Valor.
 */
void frame_push_double(Frame *frame, double value);

/**
 * @brief Desempilha um `double` (cat. 2).
 * @details **Algoritmo:** desempilha como `long` e copia os bits.
 * @param[in,out] frame Frame.
 * @return Valor.
 */
double frame_pop_double(Frame *frame);

/**
 * @brief Empilha uma referência (ponteiro de 32 bits, D6).
 * @details **Algoritmo:** grava o ponteiro no slot com etiqueta #TAG_REF.
 * @param[in,out] frame Frame.
 * @param[in]     ref   Objeto ou NULL.
 */
void frame_push_ref(Frame *frame, const Object *ref);

/**
 * @brief Desempilha uma referência.
 *
 * @details **Algoritmo:** desempilha um slot; se a etiqueta não for
 * #TAG_REF, erro fatal (o slot não guarda um ponteiro válido) e devolve NULL.
 *
 * @param[in,out] frame Frame.
 * @return Objeto ou NULL.
 */
Object *frame_pop_ref(Frame *frame);

/**
 * @brief Lê uma variável local.
 *
 * @details **Algoritmo:** se `index >= max_locals`, erro fatal; senão
 * devolve valor e etiqueta.
 *
 * @param[in,out] frame Frame.
 * @param[in]     index Índice da local.
 * @param[out]    tag   Etiqueta (pode ser NULL).
 * @return Valor da local.
 */
Slot frame_load(Frame *frame, uint32_t index, uint8_t *tag);

/**
 * @brief Escreve uma variável local.
 *
 * @details **Algoritmo:** se `index >= max_locals`, erro fatal; senão grava
 * valor e etiqueta.
 *
 * @param[in,out] frame Frame.
 * @param[in]     index Índice da local.
 * @param[in]     value Valor.
 * @param[in]     tag   Etiqueta (#SlotTag).
 */
void frame_store(Frame *frame, uint32_t index, Slot value, SlotTag tag);

/**
 * @brief Lê um byte sem sinal do bytecode em `pc + offset`.
 * @details **Algoritmo:** acesso direto (o laço já validou o tamanho da instrução).
 * @param[in] frame  Frame.
 * @param[in] offset Deslocamento a partir do `pc`.
 * @return Byte lido.
 */
uint8_t frame_u1(const Frame *frame, uint32_t offset);

/**
 * @brief Lê um `u2` big-endian do bytecode em `pc + offset`.
 * @details **Algoritmo:** `(b0 << 8) | b1`.
 * @param[in] frame  Frame.
 * @param[in] offset Deslocamento a partir do `pc`.
 * @return Valor lido.
 */
uint16_t frame_u2(const Frame *frame, uint32_t offset);

/**
 * @brief Converte os bits de um slot em `int` sem comportamento indefinido.
 * @details **Algoritmo:** valores acima de `INT32_MAX` são mapeados para os
 * negativos por complemento de dois, sem depender da implementação.
 * @param[in] slot Slot.
 * @return Valor com sinal.
 */
int32_t slot_to_int(Slot slot);

/**
 * @brief Junta dois slots (alto, baixo) em um `long`.
 * @details **Algoritmo:** monta o `uint64_t` e converte para `int64_t` por
 * complemento de dois, como slot_to_int().
 * @param[in] high 32 bits altos.
 * @param[in] low  32 bits baixos.
 * @return Valor com sinal.
 */
int64_t slots_to_long(Slot high, Slot low);

#endif /* JVM_FRAME_H */
