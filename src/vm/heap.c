/**
 * @file heap.c
 * @brief Heap da VM: criação de objetos, arrays e strings; lista de
 *        alocações liberada no fim da execução (D8, sem GC).
 */
#include <stdlib.h>
#include <string.h>

#include "common/mutf8.h"
#include "vm/class_loader.h"
#include "vm/heap.h"
#include "vm/interpreter.h"

void *heap_alloc(Vm *vm, size_t size)
{
    void *memory;
    if (size > HEAP_LIMIT || vm->heap_bytes > HEAP_LIMIT - size) {
        vm_fatal(vm, "memoria insuficiente (limite do heap: %lu bytes)",
                 (unsigned long)HEAP_LIMIT);
        return NULL;
    }
    memory = calloc(1, size == 0 ? 1 : size);
    if (memory == NULL) {
        vm_fatal(vm, "memoria insuficiente");
        return NULL;
    }
    vm->heap_bytes += size;
    return memory;
}

/**
 * @brief Cria um objeto vazio e o insere na lista do heap.
 *
 * @details **Algoritmo:** aloca a estrutura com heap_alloc(), define o tipo
 * e a classe e a coloca no início da lista `vm->heap`.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in]     kind      Tipo do objeto.
 * @param[in]     class_ref Classe (NULL para arrays).
 * @return Objeto; NULL em caso de erro fatal.
 */
static Object *new_header(Vm *vm, ObjectKind kind, RuntimeClass *class_ref)
{
    Object *object = heap_alloc(vm, sizeof *object);
    if (object == NULL) {
        return NULL;
    }
    object->kind = kind;
    object->class_ref = class_ref;
    object->next = vm->heap;
    vm->heap = object;
    return object;
}

/**
 * @brief Copia os modelos de campos de uma classe e das superclasses.
 *
 * @details **Algoritmo:** recursão até a raiz; depois copia os modelos da
 * classe a partir de @p next, de modo que os campos da superclasse ficam
 * antes dos da subclasse.
 *
 * @param[in]  class_ref Classe atual (pode ser NULL).
 * @param[out] items     Vetor de destino.
 * @param[in]  next      Próxima posição livre.
 * @return Próxima posição livre depois da cópia.
 */
static uint16_t copy_fields(const RuntimeClass *class_ref, Field *items, uint16_t next)
{
    if (class_ref == NULL) {
        return next;
    }
    next = copy_fields(class_ref->super_class, items, next);
    if (class_ref->field_count > 0) {
        memcpy(items + next, class_ref->fields, class_ref->field_count * sizeof(Field));
    }
    return (uint16_t)(next + class_ref->field_count);
}

Object *heap_new_object(Vm *vm, RuntimeClass *class_ref)
{
    const RuntimeClass *c;
    uint32_t count = 0;
    Object *object = new_header(vm, class_ref->instance_kind, class_ref);
    if (object == NULL || object->kind == OBJECT_TEXT) {
        return object;
    }
    for (c = class_ref; c != NULL; c = c->super_class) {
        count += c->field_count;
    }
    if (count > UINT16_MAX) {
        vm_fatal(vm, "classe '%s' com campos demais", class_ref->name);
        return NULL;
    }
    object->as.fields.items = heap_alloc(vm, count * sizeof(Field));
    if (object->as.fields.items == NULL) {
        return NULL;
    }
    object->as.fields.count = copy_fields(class_ref, object->as.fields.items, 0);
    return object;
}

/**
 * @brief Tamanho real de um elemento de array conforme o seu tipo.
 *
 * @details **Algoritmo:** `Z`/`B` = 1, `C`/`S` = 2, `J`/`D` = 8; `I`, `F`
 * e referências (`L`, `[`) = 4.
 *
 * @param[in] elem Primeiro caractere do tipo do elemento.
 * @return Tamanho em bytes.
 */
static uint32_t element_size(char elem)
{
    switch (elem) {
    case 'Z': case 'B': return 1;
    case 'C': case 'S': return 2;
    case 'J': case 'D': return 8;
    default:            return 4;
    }
}

Object *heap_new_array(Vm *vm, const char *type, int32_t length)
{
    size_t type_len = strlen(type);
    uint32_t size = element_size(type[1]);
    Object *object;
    if (length < 0 || (size_t)length > HEAP_LIMIT / size) {
        vm_fatal(vm, "memoria insuficiente para array de %ld elementos", (long)length);
        return NULL;
    }
    object = new_header(vm, OBJECT_ARRAY, NULL);
    if (object == NULL) {
        return NULL;
    }
    object->as.array.type = heap_alloc(vm, type_len + 1);
    object->as.array.data = heap_alloc(vm, (size_t)length * size);
    if (object->as.array.type == NULL || object->as.array.data == NULL) {
        return NULL;
    }
    memcpy(object->as.array.type, type, type_len + 1);
    object->as.array.length = length;
    object->as.array.elem_size = size;
    return object;
}

Object *heap_new_string(Vm *vm, const uint16_t *chars, int32_t length)
{
    RuntimeClass *string_class = class_load(vm, "java/lang/String");
    Object *object;
    if (string_class == NULL) {
        return NULL;
    }
    object = heap_new_object(vm, string_class);
    if (object == NULL) {
        return NULL;
    }
    object->as.text.chars = heap_alloc(vm, (size_t)length * sizeof(uint16_t));
    if (object->as.text.chars == NULL) {
        return NULL;
    }
    if (length > 0) {
        memcpy(object->as.text.chars, chars, (size_t)length * sizeof(uint16_t));
    }
    object->as.text.length = length;
    return object;
}

Object *heap_new_string_utf8(Vm *vm, const char *text)
{
    size_t bytes = strlen(text);
    size_t count = mutf8_to_utf16((const uint8_t *)text, bytes, NULL);
    uint16_t *chars = malloc((count + 1) * sizeof(uint16_t));
    Object *object;
    if (chars == NULL) {
        vm_fatal(vm, "memoria insuficiente");
        return NULL;
    }
    mutf8_to_utf16((const uint8_t *)text, bytes, chars);
    object = heap_new_string(vm, chars, (int32_t)count);
    free(chars);
    return object;
}

Object *heap_intern(Vm *vm, const char *text)
{
    Object *created;
    Object **grown;
    size_t i;
    if (text == NULL) {
        vm_fatal(vm, "literal String invalido no pool de constantes");
        return NULL;
    }
    created = heap_new_string_utf8(vm, text);
    if (created == NULL) {
        return NULL;
    }
    for (i = 0; i < vm->interned_count; i++) {
        const Text *a = &vm->interned[i]->as.text;
        const Text *b = &created->as.text;
        if (a->length == b->length &&
            (a->length == 0 ||
             memcmp(a->chars, b->chars, (size_t)a->length * sizeof(uint16_t)) == 0)) {
            return vm->interned[i];
        }
    }
    grown = realloc(vm->interned, (vm->interned_count + 1) * sizeof *grown);
    if (grown == NULL) {
        vm_fatal(vm, "memoria insuficiente");
        return NULL;
    }
    vm->interned = grown;
    vm->interned[vm->interned_count++] = created;
    return created;
}

Field *object_field(Object *object, const Field *decl)
{
    uint16_t i;
    if (object->kind != OBJECT_INSTANCE) {
        return NULL;
    }
    for (i = 0; i < object->as.fields.count; i++) {
        if (object->as.fields.items[i].name == decl->name) {
            return &object->as.fields.items[i];
        }
    }
    return NULL;
}

void heap_free_all(Vm *vm)
{
    Object *object = vm->heap;
    while (object != NULL) {
        Object *next = object->next;
        switch (object->kind) {
        case OBJECT_INSTANCE:
            free(object->as.fields.items);
            break;
        case OBJECT_ARRAY:
            free(object->as.array.type);
            free(object->as.array.data);
            break;
        case OBJECT_TEXT:
            free(object->as.text.chars);
            break;
        }
        free(object);
        object = next;
    }
    vm->heap = NULL;
    vm->heap_bytes = 0;
}
