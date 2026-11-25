/**
 * @file class_loader.c
 * @brief Carregamento de classes do usuário (`<dir>/pkg/Classe.class`) e
 *        nativas, campos de instância e estáticos e liberação (I-02, D19, D21).
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/constant_pool.h"
#include "vm/class_loader.h"
#include "vm/descriptor.h"
#include "vm/heap.h"
#include "vm/interpreter.h"
#include "vm/natives.h"

/** @brief Tamanho máximo do caminho de um `.class`. */
#define CLASS_PATH_SIZE 4096
/** @brief Profundidade máxima de carregamento aninhado (evita hierarquia circular). */
#define MAX_LOAD_NESTING 64

/** @brief Carregamentos em andamento (detecta herança circular). */
static int load_nesting = 0;

/**
 * @brief Procura uma classe já carregada.
 *
 * @details **Algoritmo:** busca linear por nome na lista da VM (D19).
 *
 * @param[in] vm   Máquina virtual.
 * @param[in] name Nome interno.
 * @return Classe ou NULL.
 */
static RuntimeClass *find_loaded(const Vm *vm, const char *name)
{
    RuntimeClass *c;
    for (c = vm->classes; c != NULL; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            return c;
        }
    }
    return NULL;
}

/**
 * @brief Grava em um campo o valor do atributo `ConstantValue`.
 *
 * @details **Algoritmo:** procura o atributo entre os do campo e copia a
 * constante do pool conforme o tipo (`int`, `float`, `long`, `double` ou
 * literal `String` internado).
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in]     cf        Classe lida.
 * @param[in]     info      Campo no `.class`.
 * @param[out]    field     Campo de destino.
 * @return `true` em caso de sucesso.
 */
static bool apply_constant_value(Vm *vm, const ClassFile *cf, const FieldInfo *info,
                                 Field *field)
{
    uint16_t i;
    for (i = 0; i < info->attributes_count; i++) {
        uint16_t index;
        const CpInfo *entry;
        const Object *string;
        if (info->attributes[i].kind != ATTR_CONSTANT_VALUE) {
            continue;
        }
        index = info->attributes[i].info.constantvalue_index;
        if (index == 0 || index >= cf->constant_pool_count) {
            return false;
        }
        entry = &cf->constant_pool[index];
        switch (entry->tag) {
        case CONSTANT_Integer:
            field->value[0] = entry->info.integer_bytes;
            break;
        case CONSTANT_Float:
            field->value[0] = entry->info.float_bytes;
            break;
        case CONSTANT_Long:
        case CONSTANT_Double:
            field->value[0] = entry->info.wide.high_bytes;
            field->value[1] = entry->info.wide.low_bytes;
            break;
        case CONSTANT_String:
            string = heap_intern(vm, cp_get_string(cf, index));
            if (string == NULL) {
                return false;
            }
            field->value[0] = (Slot)(uintptr_t)string;
            break;
        default:
            return false;
        }
    }
    return true;
}

/**
 * @brief Monta os modelos de campos de instância e os campos estáticos.
 *
 * @details **Algoritmo:** conta os campos de cada tipo, aloca os dois
 * vetores e preenche nome e descritor (ponteiros para o pool); os
 * estáticos recebem o `ConstantValue`, se houver.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in,out] class_ref Classe com o `ClassFile` já lido.
 * @return `true` em caso de sucesso.
 */
static bool build_fields(Vm *vm, RuntimeClass *class_ref)
{
    const ClassFile *cf = class_ref->class_file;
    uint16_t i, n_static = 0, n_instance = 0;
    for (i = 0; i < cf->fields_count; i++) {
        if (cf->fields[i].access_flags & ACC_STATIC) {
            n_static++;
        } else {
            n_instance++;
        }
    }
    class_ref->statics = calloc((size_t)n_static + 1, sizeof(Field));
    class_ref->fields = calloc((size_t)n_instance + 1, sizeof(Field));
    if (class_ref->statics == NULL || class_ref->fields == NULL) {
        return false;
    }
    for (i = 0; i < cf->fields_count; i++) {
        const FieldInfo *info = &cf->fields[i];
        bool is_static = (info->access_flags & ACC_STATIC) != 0;
        Field *field = is_static ? &class_ref->statics[class_ref->static_count++]
                                 : &class_ref->fields[class_ref->field_count++];
        field->name = cp_get_utf8(cf, info->name_index);
        field->descriptor = cp_get_utf8(cf, info->descriptor_index);
        if (field->name == NULL || field->descriptor == NULL ||
            descriptor_skip_type(field->descriptor) == NULL) {
            return false;
        }
        if (is_static && !apply_constant_value(vm, cf, info, field)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Prepara uma classe do usuário cujo `ClassFile` já foi lido.
 *
 * @details **Algoritmo:** carrega a superclasse (todas as classes do
 * usuário têm uma; só `java/lang/Object`, nativa, não tem) e monta os
 * campos de instância e estáticos.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in,out] class_ref Classe com nome e `ClassFile` preenchidos.
 * @return `true` em caso de sucesso (senão já houve vm_fatal()).
 */
static bool prepare_user_class(Vm *vm, RuntimeClass *class_ref)
{
    const char *super_name = cp_get_class_name(class_ref->class_file,
                                               class_ref->class_file->super_class);
    if (super_name == NULL) {
        vm_fatal(vm, "classe '%s' sem superclasse", class_ref->name);
        return false;
    }
    class_ref->super_class = class_load(vm, super_name);
    if (class_ref->super_class == NULL) {
        return false;
    }
    if (!build_fields(vm, class_ref)) {
        if (!vm->fatal) {
            vm_fatal(vm, "campos invalidos na classe '%s'", class_ref->name);
        }
        return false;
    }
    return true;
}

/**
 * @brief Lê o `.class` de uma classe do usuário e a prepara.
 *
 * @details
 * **Algoritmo:**
 * 1. Monta o caminho `<classpath>/<nome>.class` e lê o arquivo;
 * 2. Confere se `this_class` tem o nome pedido;
 * 3. Chama prepare_user_class().
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in,out] class_ref Classe com o nome preenchido.
 * @return `true` em caso de sucesso (senão já houve vm_fatal()).
 */
static bool load_user_class(Vm *vm, RuntimeClass *class_ref)
{
    char path[CLASS_PATH_SIZE];
    ClassFileError err;
    const char *this_name;
    int n = snprintf(path, sizeof path, "%s/%s.class",
                     vm->classpath != NULL ? vm->classpath : ".", class_ref->name);
    if (n < 0 || (size_t)n >= sizeof path) {
        vm_fatal(vm, "caminho muito longo para a classe '%s'", class_ref->name);
        return false;
    }
    if (class_file_load(path, &class_ref->class_file, &err) != CF_OK) {
        vm_fatal(vm, "classe '%s' nao carregada: %s (%s)", class_ref->name,
                 err.message, path);
        return false;
    }
    this_name = cp_get_class_name(class_ref->class_file, class_ref->class_file->this_class);
    if (this_name == NULL || strcmp(this_name, class_ref->name) != 0) {
        vm_fatal(vm, "'%s' nao declara a classe '%s'", path, class_ref->name);
        return false;
    }
    return prepare_user_class(vm, class_ref);
}

/**
 * @brief Libera uma classe.
 *
 * @details **Algoritmo:** libera o `ClassFile`, os vetores de campos, o
 * nome e a estrutura (os nomes dos campos pertencem ao pool ou são literais).
 *
 * @param[in] class_ref Classe (pode ser NULL).
 */
static void free_class(RuntimeClass *class_ref)
{
    if (class_ref == NULL) {
        return;
    }
    class_file_free(class_ref->class_file);
    free(class_ref->fields);
    free(class_ref->statics);
    free(class_ref->name);
    free(class_ref);
}

RuntimeClass *class_load(Vm *vm, const char *name)
{
    RuntimeClass *class_ref = find_loaded(vm, name);
    const char *super_name = NULL;
    bool ok;
    if (class_ref != NULL || vm->fatal) {
        return class_ref;
    }
    if (name[0] == '[' || load_nesting >= MAX_LOAD_NESTING) {
        vm_fatal(vm, "classe '%s' invalida ou hierarquia circular", name);
        return NULL;
    }
    class_ref = calloc(1, sizeof *class_ref);
    if (class_ref == NULL || (class_ref->name = malloc(strlen(name) + 1)) == NULL) {
        free(class_ref);
        vm_fatal(vm, "memoria insuficiente");
        return NULL;
    }
    strcpy(class_ref->name, name);
    class_ref->instance_kind = OBJECT_INSTANCE;
    load_nesting++;
    if (natives_class_super(name, &super_name)) {
        ok = super_name == NULL || (class_ref->super_class = class_load(vm, super_name)) != NULL;
        if (ok) {
            /* Insere antes de preparar: o System cria um PrintStream. */
            class_ref->next = vm->classes;
            vm->classes = class_ref;
            load_nesting--;
            return natives_setup_class(vm, class_ref) ? class_ref : NULL;
        }
    } else {
        ok = load_user_class(vm, class_ref);
    }
    load_nesting--;
    if (!ok) {
        vm->exit_code = EXIT_CODE_CLASS_LOAD;
        free_class(class_ref);
        return NULL;
    }
    class_ref->next = vm->classes;
    vm->classes = class_ref;
    return class_ref;
}

RuntimeClass *class_define(Vm *vm, ClassFile *class_file)
{
    const char *name = cp_get_class_name(class_file, class_file->this_class);
    const char *super_name;
    RuntimeClass *class_ref;
    if (name == NULL || find_loaded(vm, name) != NULL || natives_class_super(name, &super_name)) {
        vm_fatal(vm, "classe invalida ou ja carregada");
        class_file_free(class_file);
        return NULL;
    }
    class_ref = calloc(1, sizeof *class_ref);
    if (class_ref == NULL || (class_ref->name = malloc(strlen(name) + 1)) == NULL) {
        free(class_ref);
        class_file_free(class_file);
        vm_fatal(vm, "memoria insuficiente");
        return NULL;
    }
    strcpy(class_ref->name, name);
    class_ref->class_file = class_file;
    class_ref->instance_kind = OBJECT_INSTANCE;
    if (!prepare_user_class(vm, class_ref)) {
        free_class(class_ref);
        return NULL;
    }
    class_ref->next = vm->classes;
    vm->classes = class_ref;
    return class_ref;
}

void class_free_all(Vm *vm)
{
    RuntimeClass *c = vm->classes;
    while (c != NULL) {
        RuntimeClass *next = c->next;
        free_class(c);
        c = next;
    }
    vm->classes = NULL;
    load_nesting = 0;
}
