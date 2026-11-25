/**
 * @file class_hierarchy.c
 * @brief Consultas à hierarquia de classes: inicialização (`<clinit>`),
 *        busca de métodos e campos e compatibilidade de tipos
 *        (I-02, I-06, D19: busca linear, sem *cache* nem *vtable*).
 */
#include <string.h>

#include "classfile/constant_pool.h"
#include "vm/class_loader.h"
#include "vm/interpreter.h"
#include "vm/natives.h"

/** @brief Tamanho máximo do nome de uma classe extraído de um descritor. */
#define TYPE_NAME_SIZE 512
/** @brief Profundidade máxima de interfaces herdadas (protege contra ciclos). */
#define MAX_INTERFACE_DEPTH 32

/**
 * @brief Procura um método declarado em um `.class` (sem subir a hierarquia).
 *
 * @details **Algoritmo:** compara nome e descritor de cada método.
 *
 * @param[in] cf         Classe lida.
 * @param[in] name       Nome.
 * @param[in] descriptor Descritor.
 * @return Método ou NULL.
 */
static const MethodInfo *find_declared_method(const ClassFile *cf, const char *name,
                                              const char *descriptor)
{
    uint16_t i;
    for (i = 0; i < cf->methods_count; i++) {
        const char *m_name = cp_get_utf8(cf, cf->methods[i].name_index);
        const char *m_desc = cp_get_utf8(cf, cf->methods[i].descriptor_index);
        if (m_name != NULL && m_desc != NULL && strcmp(m_name, name) == 0 &&
            strcmp(m_desc, descriptor) == 0) {
            return &cf->methods[i];
        }
    }
    return NULL;
}

bool class_initialize(Vm *vm, RuntimeClass *class_ref)
{
    MethodRef ref;
    if (class_ref->state != CLASS_LOADED) {
        return true;
    }
    class_ref->state = CLASS_INITIALIZING;
    if (class_ref->super_class != NULL && !class_initialize(vm, class_ref->super_class)) {
        return false;
    }
    if (class_ref->class_file != NULL) {
        ref.owner = class_ref;
        ref.native = NULL;
        ref.is_static = true;
        ref.method = find_declared_method(class_ref->class_file, "<clinit>", "()V");
        if (ref.method != NULL) {
            Slot result[2];
            if (!interpreter_call(vm, &ref, "()V", NULL, NULL, 0, result)) {
                return false;
            }
        }
    }
    class_ref->state = CLASS_INITIALIZED;
    return true;
}

bool class_find_method(RuntimeClass *class_ref, const char *name,
                       const char *descriptor, MethodRef *out)
{
    RuntimeClass *c;
    for (c = class_ref; c != NULL; c = c->super_class) {
        out->owner = c;
        out->method = NULL;
        out->native = NULL;
        out->is_static = false;
        if (c->class_file != NULL) {
            out->method = find_declared_method(c->class_file, name, descriptor);
            out->is_static = out->method != NULL &&
                             (out->method->access_flags & ACC_STATIC) != 0;
        } else {
            out->native = natives_find(c->name, name, descriptor, &out->is_static);
        }
        if (out->method != NULL || out->native != NULL) {
            return true;
        }
    }
    return false;
}

const Field *class_find_field(const RuntimeClass *class_ref, const char *name,
                              const char *descriptor)
{
    const RuntimeClass *c;
    uint16_t i;
    for (c = class_ref; c != NULL; c = c->super_class) {
        for (i = 0; i < c->field_count; i++) {
            if (strcmp(c->fields[i].name, name) == 0 &&
                strcmp(c->fields[i].descriptor, descriptor) == 0) {
                return &c->fields[i];
            }
        }
    }
    return NULL;
}

/**
 * @brief Implementação de class_find_static() com limite de profundidade.
 *
 * @details **Algoritmo:** igual ao de class_find_static(); a recursão
 * pelas interfaces para em #MAX_INTERFACE_DEPTH níveis.
 *
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     class_ref  Classe inicial.
 * @param[in]     name       Nome do campo.
 * @param[in]     descriptor Descritor.
 * @param[out]    owner      Classe que declara o campo.
 * @param[in]     depth      Nível atual da recursão.
 * @return Campo; NULL se não existe.
 */
static Field *find_static(Vm *vm, RuntimeClass *class_ref, const char *name,
                          const char *descriptor, RuntimeClass **owner, int depth)
{
    RuntimeClass *c;
    uint16_t i;
    if (depth > MAX_INTERFACE_DEPTH) {
        return NULL;
    }
    for (c = class_ref; c != NULL; c = c->super_class) {
        const ClassFile *cf = c->class_file;
        for (i = 0; i < c->static_count; i++) {
            if (strcmp(c->statics[i].name, name) == 0 &&
                strcmp(c->statics[i].descriptor, descriptor) == 0) {
                *owner = c;
                return &c->statics[i];
            }
        }
        for (i = 0; cf != NULL && i < cf->interfaces_count; i++) {
            const char *iface_name = cp_get_class_name(cf, cf->interfaces[i]);
            RuntimeClass *iface = iface_name != NULL ? class_load(vm, iface_name) : NULL;
            Field *field = iface != NULL
                               ? find_static(vm, iface, name, descriptor, owner, depth + 1)
                               : NULL;
            if (field != NULL) {
                return field;
            }
        }
    }
    return NULL;
}

Field *class_find_static(Vm *vm, RuntimeClass *class_ref, const char *name,
                         const char *descriptor, RuntimeClass **owner)
{
    return find_static(vm, class_ref, name, descriptor, owner, 0);
}

/**
 * @brief Implementação de class_is_assignable() com limite de profundidade.
 *
 * @details **Algoritmo:** igual ao de class_is_assignable(); a recursão
 * pelas interfaces para em #MAX_INTERFACE_DEPTH níveis, o que protege
 * contra interfaces circulares em arquivos `.class` corrompidos.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in]     class_ref Classe.
 * @param[in]     target    Nome interno do alvo.
 * @param[in]     depth     Nível atual da recursão.
 * @return `true` se a atribuição é válida.
 */
static bool assignable(Vm *vm, const RuntimeClass *class_ref, const char *target, int depth)
{
    const RuntimeClass *c;
    uint16_t i;
    if (depth > MAX_INTERFACE_DEPTH) {
        return false;
    }
    for (c = class_ref; c != NULL; c = c->super_class) {
        const ClassFile *cf = c->class_file;
        if (strcmp(c->name, target) == 0) {
            return true;
        }
        for (i = 0; cf != NULL && i < cf->interfaces_count; i++) {
            const char *iface_name = cp_get_class_name(cf, cf->interfaces[i]);
            const RuntimeClass *iface = iface_name != NULL ? class_load(vm, iface_name) : NULL;
            if (iface != NULL && assignable(vm, iface, target, depth + 1)) {
                return true;
            }
        }
    }
    return false;
}

bool class_is_assignable(Vm *vm, const RuntimeClass *class_ref, const char *target)
{
    return assignable(vm, class_ref, target, 0);
}

/**
 * @brief Indica se um tipo de elemento/array é atribuível a outro.
 *
 * @details
 * **Algoritmo:** (tipos no formato de descritor)
 * 1. Destino `Ljava/lang/Object;`: qualquer referência é atribuível;
 * 2. Ambos arrays: compara os elementos recursivamente;
 * 3. Ambos classes: carrega a origem e usa class_is_assignable();
 * 4. Demais (primitivos): só se forem iguais.
 *
 * @param[in,out] vm     Máquina virtual.
 * @param[in]     source Tipo de origem.
 * @param[in]     target Tipo de destino.
 * @return `true` se a atribuição é válida.
 */
static bool type_is_assignable(Vm *vm, const char *source, const char *target)
{
    char name[TYPE_NAME_SIZE];
    const char *end;
    size_t length;
    const RuntimeClass *source_class;
    if (strcmp(target, "Ljava/lang/Object;") == 0 && (source[0] == 'L' || source[0] == '[')) {
        return true;
    }
    if (source[0] == '[' && target[0] == '[') {
        return type_is_assignable(vm, source + 1, target + 1);
    }
    if (source[0] != 'L' || target[0] != 'L') {
        return strcmp(source, target) == 0;
    }
    end = strchr(source, ';');
    length = end == NULL ? 0 : (size_t)(end - source - 1);
    if (length == 0 || length >= sizeof name) {
        return false;
    }
    memcpy(name, source + 1, length);
    name[length] = '\0';
    source_class = class_load(vm, name);
    if (source_class == NULL) {
        return false;
    }
    end = strchr(target, ';');
    length = end == NULL ? 0 : (size_t)(end - target - 1);
    if (length == 0 || length >= sizeof name) {
        return false;
    }
    memcpy(name, target + 1, length);
    name[length] = '\0';
    return class_is_assignable(vm, source_class, name);
}

bool object_is_instance(Vm *vm, const Object *object, const char *target)
{
    char wrapped[TYPE_NAME_SIZE];
    if (object->kind != OBJECT_ARRAY) {
        return class_is_assignable(vm, object->class_ref, target);
    }
    if (target[0] == '[') {
        return type_is_assignable(vm, object->as.array.type, target);
    }
    if (strlen(target) + 3 > sizeof wrapped) {
        return false;
    }
    sprintf(wrapped, "L%s;", target);
    return type_is_assignable(vm, object->as.array.type, wrapped);
}

const char *object_type_name(const Object *object)
{
    return object->kind == OBJECT_ARRAY ? object->as.array.type : object->class_ref->name;
}
