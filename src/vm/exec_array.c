/**
 * @file exec_array.c
 * @brief Arrays: `newarray`, `anewarray`, `multianewarray`, `arraylength`,
 *        `*aload` e `*astore` de todos os tipos (I-07).
 *
 * @details Cada array guarda os elementos com o tamanho real do tipo
 * (`byte`/`boolean` = 1 byte, `char`/`short` = 2, `int`/`float`/referência
 * = 4, `long`/`double` = 8). O acesso confere `null`, os limites e se a
 * instrução combina com o tipo do array (I-10).
 */
#include <stdio.h>
#include <string.h>

#include "classfile/constant_pool.h"
#include "vm/frame.h"
#include "vm/heap.h"
#include "vm/interpreter.h"

/** @brief Tamanho máximo do descritor de um array criado por `anewarray`. */
#define ARRAY_TYPE_SIZE 512
/** @brief Tipos dos elementos das famílias `*aload`/`*astore`, na ordem dos opcodes. */
static const char ACCESS_TYPES[] = "IJFDABCS";

/**
 * @brief Lança `NegativeArraySizeException` se o tamanho for negativo.
 * @details **Algoritmo:** sem mensagem, como a JVM do Java 8.
 * @param[in,out] frame  Frame.
 * @param[in]     length Tamanho pedido.
 * @return `true` se o tamanho é válido.
 */
static bool check_length(Frame *frame, int32_t length)
{
    if (length >= 0) {
        return true;
    }
    vm_throw_new(frame->vm, "java/lang/NegativeArraySizeException", NULL);
    return false;
}

/**
 * @brief `newarray`: array de tipo primitivo.
 * @details **Algoritmo:** converte o código `atype` (4 a 11) no descritor
 * (`[Z`, `[C`, `[F`, `[D`, `[B`, `[S`, `[I`, `[J`) e cria o array.
 * @param[in,out] frame Frame.
 */
static void op_newarray(Frame *frame)
{
    static const char *const TYPES[] = {"[Z", "[C", "[F", "[D", "[B", "[S", "[I", "[J"};
    uint8_t atype = frame_u1(frame, 1);
    int32_t length = frame_pop_int(frame);
    const Object *array;
    if (atype < 4 || atype > 11) {
        vm_fatal(frame->vm, "newarray com tipo invalido %u", (unsigned)atype);
        return;
    }
    if (!check_length(frame, length)) {
        return;
    }
    array = heap_new_array(frame->vm, TYPES[atype - 4], length);
    if (array != NULL) {
        frame_push_ref(frame, array);
    }
}

/**
 * @brief `anewarray`: array de referências.
 * @details **Algoritmo:** o tipo é `[` + descritor do elemento (`[L<classe>;`
 * ou `[` + o descritor, se o elemento já for array).
 * @param[in,out] frame Frame.
 */
static void op_anewarray(Frame *frame)
{
    const char *name = cp_get_class_name(frame->class_ref->class_file, frame_u2(frame, 1));
    char type[ARRAY_TYPE_SIZE];
    int32_t length = frame_pop_int(frame);
    const Object *array;
    int n;
    if (name == NULL) {
        vm_fatal(frame->vm, "anewarray com classe invalida");
        return;
    }
    n = name[0] == '[' ? snprintf(type, sizeof type, "[%s", name)
                       : snprintf(type, sizeof type, "[L%s;", name);
    if (n < 0 || (size_t)n >= sizeof type) {
        vm_fatal(frame->vm, "tipo de array muito longo");
        return;
    }
    if (!check_length(frame, length)) {
        return;
    }
    array = heap_new_array(frame->vm, type, length);
    if (array != NULL) {
        frame_push_ref(frame, array);
    }
}

/**
 * @brief Cria recursivamente um array multidimensional (array de arrays).
 *
 * @details **Algoritmo:** cria o array externo com `lengths[0]`; se ainda
 * há dimensões, cria cada elemento com o tipo sem o primeiro `[`.
 *
 * @param[in,out] frame   Frame.
 * @param[in]     type    Descritor do array desta dimensão.
 * @param[in]     lengths Tamanhos das dimensões restantes.
 * @param[in]     dims    Quantidade de dimensões restantes (>= 1).
 * @return Array; NULL em caso de erro fatal.
 */
static Object *new_multi(Frame *frame, const char *type, const int32_t *lengths, int dims)
{
    Object *array = heap_new_array(frame->vm, type, lengths[0]);
    int32_t i;
    if (array == NULL || dims == 1) {
        return array;
    }
    for (i = 0; i < lengths[0]; i++) {
        Object *element = new_multi(frame, type + 1, lengths + 1, dims - 1);
        if (element == NULL) {
            return NULL;
        }
        ((Object **)array->as.array.data)[i] = element;
    }
    return array;
}

/**
 * @brief `multianewarray`: array com várias dimensões.
 *
 * @details **Algoritmo:** desempilha os tamanhos (o último está no topo);
 * qualquer negativo lança `NegativeArraySizeException`; o número de
 * dimensões não pode passar das do descritor.
 *
 * @param[in,out] frame Frame.
 */
static void op_multianewarray(Frame *frame)
{
    const char *type = cp_get_class_name(frame->class_ref->class_file, frame_u2(frame, 1));
    int dims = frame_u1(frame, 3);
    int32_t lengths[255];
    const Object *array;
    int i;
    if (type == NULL || dims == 0 || (int)strspn(type, "[") < dims) {
        vm_fatal(frame->vm, "multianewarray invalido");
        return;
    }
    for (i = dims - 1; i >= 0; i--) {
        lengths[i] = frame_pop_int(frame);
    }
    for (i = 0; i < dims; i++) {
        if (!check_length(frame, lengths[i])) {
            return;
        }
    }
    array = new_multi(frame, type, lengths, dims);
    if (array != NULL) {
        frame_push_ref(frame, array);
    }
}

/**
 * @brief `arraylength`: tamanho do array do topo.
 * @details **Algoritmo:** `null` → `NullPointerException`; não-array → erro fatal.
 * @param[in,out] frame Frame.
 */
static void op_arraylength(Frame *frame)
{
    const Object *array = frame_pop_ref(frame);
    if (frame->vm->fatal) {
        return;
    }
    if (array == NULL) {
        vm_throw_new(frame->vm, "java/lang/NullPointerException", NULL);
    } else if (array->kind != OBJECT_ARRAY) {
        vm_fatal(frame->vm, "arraylength de objeto que nao e array");
    } else {
        frame_push_int(frame, array->as.array.length);
    }
}

/**
 * @brief Valida referência, tipo e índice de um acesso a array.
 *
 * @details **Algoritmo:** `null` → `NullPointerException`; tipo do array
 * diferente do da instrução → erro fatal (`A` aceita `L` e `[`, `B` aceita
 * `Z`); índice fora de `[0, length)` → `ArrayIndexOutOfBoundsException`
 * com o índice como mensagem.
 *
 * @param[in,out] frame Frame.
 * @param[in]     array Array desempilhado.
 * @param[in]     index Índice.
 * @param[in]     type  Tipo da instrução (`I`, `J`, `F`, `D`, `A`, `B`, `C`, `S`).
 * @return Endereço do elemento; NULL se houve exceção ou erro.
 */
static uint8_t *element_address(Frame *frame, Object *array, int32_t index, char type)
{
    char elem;
    if (frame->vm->fatal) {
        return NULL;
    }
    if (array == NULL) {
        vm_throw_new(frame->vm, "java/lang/NullPointerException", NULL);
        return NULL;
    }
    elem = array->kind == OBJECT_ARRAY ? array->as.array.type[1] : '?';
    if (!(elem == type || (type == 'A' && (elem == 'L' || elem == '[')) ||
          (type == 'B' && elem == 'Z'))) {
        vm_fatal(frame->vm, "instrucao de array incompativel com o tipo do array");
        return NULL;
    }
    if (index < 0 || index >= array->as.array.length) {
        char message[16];
        sprintf(message, "%ld", (long)index);
        vm_throw_new(frame->vm, "java/lang/ArrayIndexOutOfBoundsException", message);
        return NULL;
    }
    return (uint8_t *)array->as.array.data + (size_t)index * array->as.array.elem_size;
}

/**
 * @brief `iaload`, `laload`, `faload`, `daload`, `aaload`, `baload`, `caload`, `saload`.
 *
 * @details **Algoritmo:** desempilha índice e array, valida (element_address())
 * e empilha o elemento: `byte`/`short` com extensão de sinal, `char` sem
 * sinal, `long`/`double` em 2 slots, referência com #TAG_REF.
 *
 * @param[in,out] frame Frame.
 */
static void op_aload(Frame *frame)
{
    char type = ACCESS_TYPES[frame_u1(frame, 0) - 0x2E];
    int32_t index = frame_pop_int(frame);
    const uint8_t *p = element_address(frame, frame_pop_ref(frame), index, type);
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    if (p == NULL) {
        return;
    }
    switch (type) {
    case 'B': frame_push_int(frame, (int32_t)(p[0] ^ 0x80u) - 0x80); break;
    case 'C':
    case 'S':
        memcpy(&u16, p, 2);
        frame_push_int(frame, type == 'C' ? (int32_t)u16 : (int32_t)(u16 ^ 0x8000u) - 0x8000);
        break;
    case 'J':
    case 'D':
        memcpy(&u64, p, 8);
        frame_push(frame, (Slot)(u64 >> 32), TAG_VALUE);
        frame_push(frame, (Slot)u64, TAG_VALUE);
        break;
    default:
        memcpy(&u32, p, 4);
        frame_push(frame, u32, type == 'A' ? TAG_REF : TAG_VALUE);
        break;
    }
}

/**
 * @brief `iastore`, `lastore`, `fastore`, `dastore`, `aastore`, `bastore`, `castore`, `sastore`.
 *
 * @details **Algoritmo:** desempilha o valor (2 slots em `long`/`double`;
 * referência verificada em `aastore`), o índice e o array; valida e grava
 * o valor truncado para o tamanho do elemento.
 *
 * @param[in,out] frame Frame.
 */
static void op_astore(Frame *frame)
{
    char type = ACCESS_TYPES[frame_u1(frame, 0) - 0x4F];
    uint64_t value;
    int32_t index;
    uint8_t *p;
    uint16_t u16;
    uint32_t u32;
    if (type == 'J' || type == 'D') {
        Slot low = frame_pop(frame, NULL);
        value = ((uint64_t)frame_pop(frame, NULL) << 32) | low;
    } else if (type == 'A') {
        value = (uintptr_t)frame_pop_ref(frame);
    } else {
        value = frame_pop(frame, NULL);
    }
    index = frame_pop_int(frame);
    p = element_address(frame, frame_pop_ref(frame), index, type);
    if (p == NULL) {
        return;
    }
    switch (type) {
    case 'B': p[0] = (uint8_t)value; break;
    case 'C':
    case 'S': u16 = (uint16_t)value; memcpy(p, &u16, 2); break;
    case 'J':
    case 'D': memcpy(p, &value, 8); break;
    default:  u32 = (uint32_t)value; memcpy(p, &u32, 4); break;
    }
}

void exec_array_register(OpcodeHandler *table)
{
    int op;
    for (op = 0x2E; op <= 0x35; op++) table[op] = op_aload;
    for (op = 0x4F; op <= 0x56; op++) table[op] = op_astore;
    table[0xBC] = op_newarray;
    table[0xBD] = op_anewarray;
    table[0xBE] = op_arraylength;
    table[0xC5] = op_multianewarray;
}
