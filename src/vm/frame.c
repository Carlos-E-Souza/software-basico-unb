/**
 * @file frame.c
 * @brief Frames: criação, pilha de operandos (cat. 1 e cat. 2), variáveis
 *        locais e leitura dos operandos do bytecode (I-01, D6).
 */
#include <stdlib.h>
#include <string.h>

#include "vm/frame.h"
#include "vm/interpreter.h"

Frame *frame_create(Vm *vm, RuntimeClass *class_ref, const MethodInfo *method,
                    const CodeAttribute *code)
{
    Frame *frame = calloc(1, sizeof *frame);
    if (frame == NULL) {
        return NULL;
    }
    frame->vm = vm;
    frame->class_ref = class_ref;
    frame->method = method;
    frame->code_attr = code;
    frame->code = code->code;
    frame->code_length = code->code_length;
    frame->max_locals = code->max_locals;
    frame->max_stack = code->max_stack;
    /* +1: evita malloc(0) quando max_locals/max_stack valem 0. */
    frame->locals = calloc((size_t)code->max_locals + 1, sizeof(Slot));
    frame->local_tags = calloc((size_t)code->max_locals + 1, 1);
    frame->stack = calloc((size_t)code->max_stack + 1, sizeof(Slot));
    frame->stack_tags = calloc((size_t)code->max_stack + 1, 1);
    if (frame->locals == NULL || frame->local_tags == NULL ||
        frame->stack == NULL || frame->stack_tags == NULL) {
        frame_free(frame);
        return NULL;
    }
    return frame;
}

void frame_free(Frame *frame)
{
    if (frame == NULL) {
        return;
    }
    free(frame->locals);
    free(frame->local_tags);
    free(frame->stack);
    free(frame->stack_tags);
    free(frame);
}

void frame_push(Frame *frame, Slot value, SlotTag tag)
{
    if (frame->sp >= frame->max_stack) {
        vm_fatal(frame->vm, "pilha de operandos cheia (max_stack = %u)",
                 (unsigned)frame->max_stack);
        return;
    }
    frame->stack[frame->sp] = value;
    frame->stack_tags[frame->sp] = (uint8_t)tag;
    frame->sp++;
}

Slot frame_pop(Frame *frame, uint8_t *tag)
{
    if (frame->sp == 0) {
        vm_fatal(frame->vm, "pilha de operandos vazia");
        if (tag != NULL) {
            *tag = TAG_VALUE;
        }
        return 0;
    }
    frame->sp--;
    if (tag != NULL) {
        *tag = frame->stack_tags[frame->sp];
    }
    return frame->stack[frame->sp];
}

int32_t slot_to_int(Slot slot)
{
    if (slot <= (Slot)INT32_MAX) {
        return (int32_t)slot;
    }
    return -(int32_t)(~slot) - 1;
}

int64_t slots_to_long(Slot high, Slot low)
{
    uint64_t bits = ((uint64_t)high << 32) | low;
    if (bits <= (uint64_t)INT64_MAX) {
        return (int64_t)bits;
    }
    return -(int64_t)(~bits) - 1;
}

void frame_push_int(Frame *frame, int32_t value)
{
    frame_push(frame, (Slot)value, TAG_VALUE);
}

int32_t frame_pop_int(Frame *frame)
{
    return slot_to_int(frame_pop(frame, NULL));
}

void frame_push_float(Frame *frame, float value)
{
    Slot bits;
    memcpy(&bits, &value, sizeof bits);
    frame_push(frame, bits, TAG_VALUE);
}

float frame_pop_float(Frame *frame)
{
    Slot bits = frame_pop(frame, NULL);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

void frame_push_long(Frame *frame, int64_t value)
{
    uint64_t bits = (uint64_t)value;
    frame_push(frame, (Slot)(bits >> 32), TAG_VALUE);
    frame_push(frame, (Slot)bits, TAG_VALUE);
}

int64_t frame_pop_long(Frame *frame)
{
    Slot low = frame_pop(frame, NULL);
    Slot high = frame_pop(frame, NULL);
    return slots_to_long(high, low);
}

void frame_push_double(Frame *frame, double value)
{
    int64_t bits;
    memcpy(&bits, &value, sizeof bits);
    frame_push_long(frame, bits);
}

double frame_pop_double(Frame *frame)
{
    int64_t bits = frame_pop_long(frame);
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

void frame_push_ref(Frame *frame, const Object *ref)
{
    frame_push(frame, (Slot)(uintptr_t)ref, TAG_REF);
}

Object *frame_pop_ref(Frame *frame)
{
    uint8_t tag;
    Slot value = frame_pop(frame, &tag);
    if (tag != TAG_REF) {
        if (!frame->vm->fatal) {
            vm_fatal(frame->vm, "referencia esperada na pilha de operandos");
        }
        return NULL;
    }
    return (Object *)(uintptr_t)value;
}

Slot frame_load(Frame *frame, uint32_t index, uint8_t *tag)
{
    if (index >= frame->max_locals) {
        vm_fatal(frame->vm, "variavel local %lu fora de max_locals",
                 (unsigned long)index);
        if (tag != NULL) {
            *tag = TAG_VALUE;
        }
        return 0;
    }
    if (tag != NULL) {
        *tag = frame->local_tags[index];
    }
    return frame->locals[index];
}

void frame_store(Frame *frame, uint32_t index, Slot value, SlotTag tag)
{
    if (index >= frame->max_locals) {
        vm_fatal(frame->vm, "variavel local %lu fora de max_locals",
                 (unsigned long)index);
        return;
    }
    frame->locals[index] = value;
    frame->local_tags[index] = (uint8_t)tag;
}

uint8_t frame_u1(const Frame *frame, uint32_t offset)
{
    return frame->code[frame->pc + offset];
}

uint16_t frame_u2(const Frame *frame, uint32_t offset)
{
    return (uint16_t)((frame->code[frame->pc + offset] << 8) |
                      frame->code[frame->pc + offset + 1]);
}
