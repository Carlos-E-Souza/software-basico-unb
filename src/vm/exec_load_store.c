/**
 * @file exec_load_store.c
 * @brief Instruções de constantes, loads, stores, `iinc` e `wide` (I-03, D18).
 *
 * @details As famílias em que o opcode codifica o tipo e o índice
 * (`iload_0` ... `aload_3`, `istore_0` ... `astore_3`, `iconst_m1` ...
 * `iconst_5`) são tratadas por uma função por família, que calcula tipo e
 * índice a partir do opcode.
 */
#include "classfile/constant_pool.h"
#include "vm/frame.h"
#include "vm/heap.h"
#include "vm/interpreter.h"

/** @brief Tipos das famílias de load/store, na ordem dos opcodes (JVMS §6.5). */
static const char LOAD_STORE_TYPES[] = "IJFDA";

/**
 * @brief Copia uma variável local para a pilha conforme o tipo.
 *
 * @details
 * **Algoritmo:**
 * - `I`/`F`: empilha 1 slot;
 * - `J`/`D`: empilha as locais `index` (alto) e `index + 1` (baixo);
 * - `A`: exige que a local guarde uma referência (#TAG_REF) e a empilha.
 *
 * @param[in,out] frame Frame.
 * @param[in]     type  Tipo (`I`, `J`, `F`, `D` ou `A`).
 * @param[in]     index Índice da local.
 */
static void load_local(Frame *frame, char type, uint32_t index)
{
    uint8_t tag;
    Slot value = frame_load(frame, index, &tag);
    if (type == 'A') {
        if (tag != TAG_REF) {
            vm_fatal(frame->vm, "aload de local %lu sem referencia", (unsigned long)index);
            return;
        }
        frame_push(frame, value, TAG_REF);
    } else if (type == 'J' || type == 'D') {
        frame_push(frame, value, TAG_VALUE);
        frame_push(frame, frame_load(frame, index + 1, NULL), TAG_VALUE);
    } else {
        frame_push(frame, value, TAG_VALUE);
    }
}

/**
 * @brief Copia o topo da pilha para uma variável local conforme o tipo.
 *
 * @details
 * **Algoritmo:**
 * - `I`/`F`: desempilha 1 slot;
 * - `J`/`D`: desempilha baixo e alto e grava em `index` (alto) e `index + 1`;
 * - `A`: aceita referência ou `returnAddress` (`astore` depois de `jsr`).
 *
 * @param[in,out] frame Frame.
 * @param[in]     type  Tipo (`I`, `J`, `F`, `D` ou `A`).
 * @param[in]     index Índice da local.
 */
static void store_local(Frame *frame, char type, uint32_t index)
{
    uint8_t tag;
    Slot value = frame_pop(frame, &tag);
    if (type == 'A') {
        if (tag != TAG_REF && tag != TAG_RETADDR) {
            vm_fatal(frame->vm, "astore sem referencia na pilha");
            return;
        }
        frame_store(frame, index, value, (SlotTag)tag);
    } else if (type == 'J' || type == 'D') {
        Slot high = frame_pop(frame, NULL);
        frame_store(frame, index, high, TAG_VALUE);
        frame_store(frame, index + 1, value, TAG_VALUE);
    } else {
        frame_store(frame, index, value, TAG_VALUE);
    }
}

/**
 * @brief `nop`: não faz nada.
 * @details **Algoritmo:** nenhum.
 * @param[in,out] frame Frame.
 */
static void op_nop(Frame *frame)
{
    (void)frame;
}

/**
 * @brief `aconst_null`: empilha a referência nula.
 * @details **Algoritmo:** frame_push_ref(NULL).
 * @param[in,out] frame Frame.
 */
static void op_aconst_null(Frame *frame)
{
    frame_push_ref(frame, NULL);
}

/**
 * @brief `iconst_m1` ... `iconst_5`: empilha um `int` de -1 a 5.
 * @details **Algoritmo:** o valor é `opcode - 0x03`.
 * @param[in,out] frame Frame.
 */
static void op_iconst(Frame *frame)
{
    frame_push_int(frame, (int32_t)frame_u1(frame, 0) - 0x03);
}

/**
 * @brief `lconst_0`/`lconst_1`: empilha o `long` 0 ou 1.
 * @details **Algoritmo:** o valor é `opcode - 0x09`.
 * @param[in,out] frame Frame.
 */
static void op_lconst(Frame *frame)
{
    frame_push_long(frame, (int64_t)frame_u1(frame, 0) - 0x09);
}

/**
 * @brief `fconst_0` ... `fconst_2`: empilha o `float` 0, 1 ou 2.
 * @details **Algoritmo:** o valor é `opcode - 0x0B`.
 * @param[in,out] frame Frame.
 */
static void op_fconst(Frame *frame)
{
    frame_push_float(frame, (float)(frame_u1(frame, 0) - 0x0B));
}

/**
 * @brief `dconst_0`/`dconst_1`: empilha o `double` 0 ou 1.
 * @details **Algoritmo:** o valor é `opcode - 0x0E`.
 * @param[in,out] frame Frame.
 */
static void op_dconst(Frame *frame)
{
    frame_push_double(frame, (double)(frame_u1(frame, 0) - 0x0E));
}

/**
 * @brief `bipush`/`sipush`: empilha o byte/short com sinal do operando.
 * @details **Algoritmo:** estende o sinal de 8 (`bipush`) ou 16 bits (`sipush`).
 * @param[in,out] frame Frame.
 */
static void op_push(Frame *frame)
{
    if (frame_u1(frame, 0) == 0x10) {
        frame_push_int(frame, (int32_t)(int8_t)frame_u1(frame, 1));
    } else {
        frame_push_int(frame, (int32_t)(int16_t)frame_u2(frame, 1));
    }
}

/**
 * @brief `ldc`, `ldc_w`, `ldc2_w`: empilha uma constante do pool.
 *
 * @details
 * **Algoritmo:** lê o índice (u1 em `ldc`, u2 nos demais) e, conforme a
 * tag: `Integer`/`Float` empilham os bits; `String` empilha o literal
 * internado (I-08); `Long`/`Double` (só em `ldc2_w`) empilham os 2 slots.
 * Qualquer outra tag é erro fatal.
 *
 * @param[in,out] frame Frame.
 */
static void op_ldc(Frame *frame)
{
    const ClassFile *cf = frame->class_ref->class_file;
    uint8_t opcode = frame_u1(frame, 0);
    uint16_t index = opcode == 0x12 ? frame_u1(frame, 1) : frame_u2(frame, 1);
    ConstantTag tag = cp_tag(cf, index);
    const CpInfo *entry = &cf->constant_pool[tag == CONSTANT_Unusable ? 0 : index];
    bool wide = opcode == 0x14;
    if (!wide && (tag == CONSTANT_Integer || tag == CONSTANT_Float)) {
        frame_push(frame, entry->info.integer_bytes, TAG_VALUE);
    } else if (!wide && tag == CONSTANT_String) {
        const Object *string = heap_intern(frame->vm, cp_get_string(cf, index));
        if (string != NULL) {
            frame_push_ref(frame, string);
        }
    } else if (wide && (tag == CONSTANT_Long || tag == CONSTANT_Double)) {
        frame_push(frame, entry->info.wide.high_bytes, TAG_VALUE);
        frame_push(frame, entry->info.wide.low_bytes, TAG_VALUE);
    } else {
        vm_fatal(frame->vm, "constante #%u (%s) nao suportada por ldc", (unsigned)index,
                 cp_tag_name(tag));
    }
}

/**
 * @brief `iload`, `lload`, `fload`, `dload`, `aload` com índice explícito.
 * @details **Algoritmo:** tipo = `LOAD_STORE_TYPES[opcode - 0x15]`; índice u1.
 * @param[in,out] frame Frame.
 */
static void op_load(Frame *frame)
{
    load_local(frame, LOAD_STORE_TYPES[frame_u1(frame, 0) - 0x15], frame_u1(frame, 1));
}

/**
 * @brief `iload_<n>` ... `aload_<n>`: load com índice 0 a 3 no opcode.
 * @details **Algoritmo:** `r = opcode - 0x1A`; tipo = `r / 4`; índice = `r % 4`.
 * @param[in,out] frame Frame.
 */
static void op_load_n(Frame *frame)
{
    unsigned r = frame_u1(frame, 0) - 0x1Au;
    load_local(frame, LOAD_STORE_TYPES[r / 4], r % 4);
}

/**
 * @brief `istore`, `lstore`, `fstore`, `dstore`, `astore` com índice explícito.
 * @details **Algoritmo:** tipo = `LOAD_STORE_TYPES[opcode - 0x36]`; índice u1.
 * @param[in,out] frame Frame.
 */
static void op_store(Frame *frame)
{
    store_local(frame, LOAD_STORE_TYPES[frame_u1(frame, 0) - 0x36], frame_u1(frame, 1));
}

/**
 * @brief `istore_<n>` ... `astore_<n>`: store com índice 0 a 3 no opcode.
 * @details **Algoritmo:** `r = opcode - 0x3B`; tipo = `r / 4`; índice = `r % 4`.
 * @param[in,out] frame Frame.
 */
static void op_store_n(Frame *frame)
{
    unsigned r = frame_u1(frame, 0) - 0x3Bu;
    store_local(frame, LOAD_STORE_TYPES[r / 4], r % 4);
}

/**
 * @brief Soma uma constante a uma local `int` (base de `iinc` e `wide iinc`).
 * @details **Algoritmo:** soma em `uint32_t` (*wrap-around* sem UB).
 * @param[in,out] frame Frame.
 * @param[in]     index Índice da local.
 * @param[in]     delta Constante com sinal.
 */
static void add_to_local(Frame *frame, uint32_t index, int32_t delta)
{
    Slot value = frame_load(frame, index, NULL);
    frame_store(frame, index, value + (Slot)delta, TAG_VALUE);
}

/**
 * @brief `iinc`: soma uma constante de 8 bits com sinal a uma local.
 * @details **Algoritmo:** índice u1, constante s1, add_to_local().
 * @param[in,out] frame Frame.
 */
static void op_iinc(Frame *frame)
{
    add_to_local(frame, frame_u1(frame, 1), (int8_t)frame_u1(frame, 2));
}

/**
 * @brief `wide`: versão de 16 bits do índice de load/store/`ret`/`iinc`.
 *
 * @details
 * **Algoritmo:** lê o opcode modificado e o índice u2; `iinc` lê também a
 * constante s2; loads e stores usam load_local()/store_local(); `ret`
 * desvia para o `returnAddress` da local.
 *
 * @param[in,out] frame Frame.
 */
static void op_wide(Frame *frame)
{
    uint8_t opcode = frame_u1(frame, 1);
    uint16_t index = frame_u2(frame, 2);
    uint8_t tag;
    if (opcode == 0x84) {
        add_to_local(frame, index, (int16_t)frame_u2(frame, 4));
    } else if (opcode >= 0x15 && opcode <= 0x19) {
        load_local(frame, LOAD_STORE_TYPES[opcode - 0x15], index);
    } else if (opcode >= 0x36 && opcode <= 0x3A) {
        store_local(frame, LOAD_STORE_TYPES[opcode - 0x36], index);
    } else {
        Slot target = frame_load(frame, index, &tag);
        if (tag != TAG_RETADDR) {
            vm_fatal(frame->vm, "ret sem returnAddress na local %u", (unsigned)index);
            return;
        }
        frame->next_pc = target;
    }
}

void exec_load_store_register(OpcodeHandler *table)
{
    int op;
    table[0x00] = op_nop;
    table[0x01] = op_aconst_null;
    for (op = 0x02; op <= 0x08; op++) table[op] = op_iconst;
    table[0x09] = table[0x0A] = op_lconst;
    table[0x0B] = table[0x0C] = table[0x0D] = op_fconst;
    table[0x0E] = table[0x0F] = op_dconst;
    table[0x10] = table[0x11] = op_push;
    table[0x12] = table[0x13] = table[0x14] = op_ldc;
    for (op = 0x15; op <= 0x19; op++) table[op] = op_load;
    for (op = 0x1A; op <= 0x2D; op++) table[op] = op_load_n;
    for (op = 0x36; op <= 0x3A; op++) table[op] = op_store;
    for (op = 0x3B; op <= 0x4E; op++) table[op] = op_store_n;
    table[0x84] = op_iinc;
    table[0xC4] = op_wide;
}
