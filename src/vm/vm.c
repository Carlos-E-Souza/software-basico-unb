/**
 * @file vm.c
 * @brief Estado da VM, erros fatais, lançamento de exceções nativas e
 *        execução do `main` (I-03, I-09).
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vm/class_loader.h"
#include "vm/heap.h"
#include "vm/interpreter.h"
#include "vm/natives.h"

/** @brief Tamanho máximo do nome interno da classe principal. */
#define MAIN_NAME_SIZE 1024

void vm_init(Vm *vm, const char *classpath, FILE *out)
{
    memset(vm, 0, sizeof *vm);
    vm->classpath = classpath;
    vm->out = out;
}

void vm_free(Vm *vm)
{
    heap_free_all(vm);
    class_free_all(vm);
    free(vm->interned);
    vm->interned = NULL;
    vm->interned_count = 0;
}

void vm_fatal(Vm *vm, const char *format, ...)
{
    va_list args;
    if (vm->fatal) {
        return;
    }
    vm->fatal = true;
    if (vm->exit_code == 0) {
        vm->exit_code = EXIT_CODE_VM_ERROR;
    }
    if (vm->quiet) {
        return;
    }
    fprintf(stderr, "Erro: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void vm_throw(Vm *vm, Object *exception)
{
    vm->exception = exception;
}

void vm_throw_new(Vm *vm, const char *class_name, const char *message)
{
    RuntimeClass *class_ref = class_load(vm, class_name);
    RuntimeClass *throwable = class_load(vm, "java/lang/Throwable");
    Object *exception;
    Field *field;
    if (class_ref == NULL || throwable == NULL) {
        return;
    }
    exception = heap_new_object(vm, class_ref);
    if (exception == NULL) {
        return;
    }
    field = object_field(exception, &throwable->fields[0]);
    if (message != NULL && field != NULL) {
        const Object *text = heap_new_string_utf8(vm, message);
        if (text == NULL) {
            return;
        }
        field->value[0] = (Slot)(uintptr_t)text;
    }
    vm_throw(vm, exception);
}

/**
 * @brief Imprime uma exceção não capturada como a JVM real.
 *
 * @details **Algoritmo:** escreve em `stderr`
 * `Exception in thread "main" <classe com pontos>[: <mensagem>]`.
 *
 * @param[in] exception Objeto da exceção.
 */
static void print_uncaught(Object *exception)
{
    const char *p;
    Field *field = natives_message_field(exception);
    const Object *message = field != NULL ? (Object *)(uintptr_t)field->value[0] : NULL;
    fprintf(stderr, "Exception in thread \"main\" ");
    for (p = exception->class_ref->name; *p != '\0'; p++) {
        fputc(*p == '/' ? '.' : *p, stderr);
    }
    if (message != NULL && message->kind == OBJECT_TEXT) {
        fprintf(stderr, ": ");
        natives_write_utf16(stderr, message->as.text.chars, message->as.text.length);
    }
    fprintf(stderr, "\n");
}

/**
 * @brief Converte o nome da classe da linha de comando para o nome interno.
 *
 * @details **Algoritmo:** remove o sufixo `.class` e troca `.` por `/`.
 *
 * @param[in]  class_name Nome informado (ex.: `pkg.Exemplo`).
 * @param[out] buffer     Destino.
 * @param[in]  size       Tamanho do destino.
 * @return `true` se coube no destino.
 */
static bool internal_name(const char *class_name, char *buffer, size_t size)
{
    size_t length = strlen(class_name);
    size_t i;
    if (length > 6 && strcmp(class_name + length - 6, ".class") == 0) {
        length -= 6;
    }
    if (length == 0 || length >= size) {
        return false;
    }
    for (i = 0; i < length; i++) {
        buffer[i] = class_name[i] == '.' ? '/' : class_name[i];
    }
    buffer[length] = '\0';
    return true;
}

void vm_call_main(Vm *vm, RuntimeClass *main_class)
{
    static const char main_desc[] = "([Ljava/lang/String;)V";
    MethodRef ref;
    const Object *args_array;
    Slot arg, result[2];
    uint8_t tag = TAG_REF;
    if (main_class == NULL) {
        return;
    }
    if (!class_find_method(main_class, "main", main_desc, &ref) || ref.method == NULL ||
        ref.owner != main_class || (ref.method->access_flags & ACC_STATIC) == 0) {
        vm_fatal(vm, "a classe '%s' nao tem 'public static void main(String[])'",
                 main_class->name);
        return;
    }
    if (!class_initialize(vm, main_class)) {
        return;
    }
    args_array = heap_new_array(vm, "[Ljava/lang/String;", 0);
    if (args_array == NULL) {
        return;
    }
    arg = (Slot)(uintptr_t)args_array;
    interpreter_call(vm, &ref, main_desc, &arg, &tag, 1, result);
}

int vm_run_main(const char *classpath, const char *class_name)
{
    char name[MAIN_NAME_SIZE];
    RuntimeClass *main_class;
    Vm vm;
    int code;
    vm_init(&vm, classpath, stdout);
    if (!internal_name(class_name, name, sizeof name)) {
        fprintf(stderr, "Erro: nome de classe invalido: '%s'\n", class_name);
        return EXIT_CODE_CLASS_LOAD;
    }
    main_class = class_load(&vm, name);
    if (main_class != NULL) {
        vm_call_main(&vm, main_class);
    }
    fflush(vm.out);
    if (vm.fatal) {
        code = vm.exit_code;
    } else if (vm.exception != NULL) {
        print_uncaught(vm.exception);
        code = EXIT_CODE_UNCAUGHT;
    } else {
        code = 0;
    }
    vm_free(&vm);
    return code;
}
