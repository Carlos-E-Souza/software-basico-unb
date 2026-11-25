/**
 * @file natives.c
 * @brief Tabela das classes nativas `{nome, superclasse}` (D7, D23),
 *        preparação dessas classes e busca de métodos nativos.
 *
 * @details Adicionar uma exceção da biblioteca = adicionar uma linha em
 * #NATIVE_CLASSES: todas herdam o campo de mensagem e os métodos de
 * `Throwable` (D23).
 */
#include <stdlib.h>
#include <string.h>

#include "vm/class_loader.h"
#include "vm/heap.h"
#include "vm/interpreter.h"
#include "vm/natives.h"

/** @brief Linha da tabela de classes nativas. */
typedef struct NativeClass {
    const char *name;       /**< Nome interno. */
    const char *super_name; /**< Superclasse (NULL só em `java/lang/Object`). */
} NativeClass;

/** @brief Classes da biblioteca simuladas em C (R2, D7, D23). */
static const NativeClass NATIVE_CLASSES[] = {
    {"java/lang/Object", NULL},
    {"java/lang/String", "java/lang/Object"},
    {"java/lang/StringBuilder", "java/lang/Object"},
    {"java/lang/StringBuffer", "java/lang/Object"},
    {"java/lang/System", "java/lang/Object"},
    {"java/io/PrintStream", "java/lang/Object"},
    {"java/lang/Throwable", "java/lang/Object"},
    {"java/lang/Exception", "java/lang/Throwable"},
    {"java/lang/Error", "java/lang/Throwable"},
    {"java/lang/StackOverflowError", "java/lang/Error"},
    {"java/lang/RuntimeException", "java/lang/Exception"},
    {"java/lang/ArithmeticException", "java/lang/RuntimeException"},
    {"java/lang/NullPointerException", "java/lang/RuntimeException"},
    {"java/lang/ClassCastException", "java/lang/RuntimeException"},
    {"java/lang/NegativeArraySizeException", "java/lang/RuntimeException"},
    {"java/lang/IllegalArgumentException", "java/lang/RuntimeException"},
    {"java/lang/IllegalStateException", "java/lang/RuntimeException"},
    {"java/lang/UnsupportedOperationException", "java/lang/RuntimeException"},
    {"java/lang/IndexOutOfBoundsException", "java/lang/RuntimeException"},
    {"java/lang/ArrayIndexOutOfBoundsException", "java/lang/IndexOutOfBoundsException"},
    {"java/lang/StringIndexOutOfBoundsException", "java/lang/IndexOutOfBoundsException"},
    {NULL, NULL}
};

/** @brief Modelo do campo de mensagem de `Throwable` (D23). */
static const Field THROWABLE_FIELD = {THROWABLE_MESSAGE_FIELD, "Ljava/lang/String;", {0, 0}};

bool natives_class_super(const char *name, const char **super_name)
{
    const NativeClass *c;
    for (c = NATIVE_CLASSES; c->name != NULL; c++) {
        if (strcmp(c->name, name) == 0) {
            *super_name = c->super_name;
            return true;
        }
    }
    return false;
}

bool natives_setup_class(Vm *vm, RuntimeClass *class_ref)
{
    const char *name = class_ref->name;
    class_ref->state = CLASS_INITIALIZED;
    if (strcmp(name, "java/lang/String") == 0 || strcmp(name, "java/lang/StringBuilder") == 0 ||
        strcmp(name, "java/lang/StringBuffer") == 0) {
        class_ref->instance_kind = OBJECT_TEXT;
    } else if (strcmp(name, "java/lang/Throwable") == 0) {
        class_ref->fields = malloc(sizeof(Field));
        if (class_ref->fields == NULL) {
            vm_fatal(vm, "memoria insuficiente");
            return false;
        }
        class_ref->fields[0] = THROWABLE_FIELD;
        class_ref->field_count = 1;
    } else if (strcmp(name, "java/lang/System") == 0) {
        RuntimeClass *print_stream = class_load(vm, "java/io/PrintStream");
        const Object *out = print_stream != NULL ? heap_new_object(vm, print_stream) : NULL;
        class_ref->statics = calloc(1, sizeof(Field));
        if (out == NULL || class_ref->statics == NULL) {
            vm_fatal(vm, "nao foi possivel criar System.out");
            return false;
        }
        class_ref->statics[0].name = "out";
        class_ref->statics[0].descriptor = "Ljava/io/PrintStream;";
        class_ref->statics[0].value[0] = (Slot)(uintptr_t)out;
        class_ref->static_count = 1;
    }
    return true;
}

/**
 * @brief Procura um método em uma tabela de nativos.
 *
 * @details **Algoritmo:** busca linear por classe, nome e descritor até a
 * linha terminadora `{NULL}`.
 *
 * @param[in]  table      Tabela.
 * @param[in]  class_name Classe.
 * @param[in]  name       Nome do método.
 * @param[in]  descriptor Descritor.
 * @param[out] is_static  Se o método é estático (só escrito quando encontrado).
 * @return Função; NULL se não existe.
 */
static NativeMethod find_in(const NativeEntry *table, const char *class_name,
                            const char *name, const char *descriptor, bool *is_static)
{
    const NativeEntry *e;
    for (e = table; e->class_name != NULL; e++) {
        if (strcmp(e->class_name, class_name) == 0 && strcmp(e->name, name) == 0 &&
            strcmp(e->descriptor, descriptor) == 0) {
            *is_static = e->is_static;
            return e->function;
        }
    }
    return NULL;
}

NativeMethod natives_find(const char *class_name, const char *name, const char *descriptor,
                          bool *is_static)
{
    NativeMethod found = find_in(native_object_methods, class_name, name, descriptor, is_static);
    if (found == NULL) {
        found = find_in(native_string_methods, class_name, name, descriptor, is_static);
    }
    if (found == NULL) {
        found = find_in(native_text_methods, class_name, name, descriptor, is_static);
    }
    if (found == NULL) {
        found = find_in(native_print_methods, class_name, name, descriptor, is_static);
    }
    return found;
}

Field *natives_message_field(Object *exception)
{
    uint16_t i;
    if (exception->kind != OBJECT_INSTANCE) {
        return NULL;
    }
    for (i = 0; i < exception->as.fields.count; i++) {
        if (exception->as.fields.items[i].name == THROWABLE_FIELD.name) {
            return &exception->as.fields.items[i];
        }
    }
    return NULL;
}
