/**
 * @file test_vm.c
 * @brief Testes unitários das estruturas de execução: pilha de operandos
 *        (cat. 1 e cat. 2), locais, heap, carregador de classes e
 *        tabela de despacho (I-01, I-02, I-10).
 */
#include <string.h>

#include "test.h"
#include "vm/class_loader.h"
#include "vm/frame.h"
#include "vm/heap.h"
#include "vm/interpreter.h"

/** @brief Bytecode mínimo usado para criar frames de teste. */
static uint8_t dummy_code[1] = {0x00};

/**
 * @brief Cria um frame de teste sem classe, com pilha e locais dados.
 * @details **Algoritmo:** monta um `CodeAttribute` estático e chama frame_create().
 * @param[in] vm         Máquina virtual.
 * @param[in] max_stack  Capacidade da pilha.
 * @param[in] max_locals Quantidade de locais.
 * @return Frame (liberar com frame_free()).
 */
static Frame *test_frame(Vm *vm, uint16_t max_stack, uint16_t max_locals)
{
    static CodeAttribute code;
    memset(&code, 0, sizeof code);
    code.max_stack = max_stack;
    code.max_locals = max_locals;
    code.code_length = 1;
    code.code = dummy_code;
    return frame_create(vm, NULL, NULL, &code);
}

/** @brief Empilha e desempilha `int` e `float`, inclusive negativos e extremos. */
static void test_push_pop_cat1(void)
{
    Vm vm;
    Frame *f;
    vm_init(&vm, NULL, stdout);
    f = test_frame(&vm, 4, 0);
    frame_push_int(f, -1);
    frame_push_int(f, INT32_MIN);
    frame_push_float(f, -2.5f);
    ASSERT_EQ_INT(3, f->sp);
    ASSERT_TRUE(frame_pop_float(f) == -2.5f);
    ASSERT_EQ_INT(INT32_MIN, frame_pop_int(f));
    ASSERT_EQ_INT(-1, frame_pop_int(f));
    ASSERT_EQ_INT(0, f->sp);
    ASSERT_TRUE(!vm.fatal);
    frame_free(f);
    vm_free(&vm);
}

/** @brief `long` e `double` ocupam 2 slots (alto, baixo). */
static void test_push_pop_cat2(void)
{
    Vm vm;
    Frame *f;
    vm_init(&vm, NULL, stdout);
    f = test_frame(&vm, 4, 0);
    frame_push_long(f, -1234567890123LL);
    ASSERT_EQ_INT(2, f->sp);
    ASSERT_EQ_INT(-1234567890123LL, frame_pop_long(f));
    frame_push_long(f, INT64_MIN);
    ASSERT_TRUE(f->stack[0] == 0x80000000u && f->stack[1] == 0);
    ASSERT_TRUE(frame_pop_long(f) == INT64_MIN);
    frame_push_double(f, 0.1);
    frame_push_double(f, -1e300);
    ASSERT_EQ_INT(4, f->sp);
    ASSERT_TRUE(frame_pop_double(f) == -1e300);
    ASSERT_TRUE(frame_pop_double(f) == 0.1);
    frame_free(f);
    vm_free(&vm);
}

/** @brief Referências: ponteiro em 1 slot com etiqueta; int como referência é erro fatal. */
static void test_push_pop_ref(void)
{
    Vm vm;
    Frame *f;
    int dummy;
    vm_init(&vm, NULL, stdout);
    f = test_frame(&vm, 2, 0);
    frame_push_ref(f, (Object *)&dummy);
    frame_push_ref(f, NULL);
    ASSERT_EQ_INT(TAG_REF, f->stack_tags[1]);
    ASSERT_TRUE(frame_pop_ref(f) == NULL);
    ASSERT_TRUE(frame_pop_ref(f) == (Object *)&dummy);
    ASSERT_TRUE(!vm.fatal);
    frame_push_int(f, 12345);
    ASSERT_TRUE(frame_pop_ref(f) == NULL);
    ASSERT_TRUE(vm.fatal);
    frame_free(f);
    vm_free(&vm);
}

/** @brief Pilha cheia, pilha vazia e local fora de `max_locals` são erros fatais. */
static void test_stack_limits(void)
{
    Vm vm;
    Frame *f;
    vm_init(&vm, NULL, stdout);
    f = test_frame(&vm, 1, 1);
    frame_push_int(f, 1);
    frame_push_int(f, 2);
    ASSERT_TRUE(vm.fatal);
    ASSERT_EQ_INT(1, f->sp);
    vm.fatal = false;
    frame_pop_int(f);
    frame_pop_int(f);
    ASSERT_TRUE(vm.fatal);
    vm.fatal = false;
    frame_store(f, 0, 7, TAG_VALUE);
    ASSERT_EQ_INT(7, frame_load(f, 0, NULL));
    frame_store(f, 1, 7, TAG_VALUE);
    ASSERT_TRUE(vm.fatal);
    frame_free(f);
    vm_free(&vm);
}

/** @brief Objetos, arrays e strings do heap são liberados por vm_free() (ASan). */
static void test_heap(void)
{
    Vm vm;
    Object *array, *string;
    const Object *same;
    vm_init(&vm, NULL, stdout);
    array = heap_new_array(&vm, "[J", 3);
    ASSERT_TRUE(array != NULL);
    ASSERT_EQ_INT(3, array->as.array.length);
    ASSERT_EQ_INT(8, array->as.array.elem_size);
    ASSERT_EQ_INT(1, heap_new_array(&vm, "[Z", 1)->as.array.elem_size);
    ASSERT_EQ_INT(2, heap_new_array(&vm, "[C", 1)->as.array.elem_size);
    string = heap_intern(&vm, "ola");
    same = heap_intern(&vm, "ola");
    ASSERT_TRUE(string != NULL && string == same);
    ASSERT_EQ_INT(3, string->as.text.length);
    ASSERT_EQ_INT('o', string->as.text.chars[0]);
    ASSERT_TRUE(heap_new_array(&vm, "[I", HEAP_LIMIT) == NULL);
    ASSERT_TRUE(vm.fatal);
    vm_free(&vm);
}

/** @brief Carrega hierarquia de 3 níveis (Hierarchy → Base → Object) e consulta-a. */
static void test_class_loader(void)
{
    Vm vm;
    RuntimeClass *c;
    MethodRef ref;
    Object *object;
    vm_init(&vm, FIXTURES_DIR "/java8", stdout);
    c = class_load(&vm, "Hierarchy");
    ASSERT_TRUE(c != NULL);
    if (c == NULL) {
        vm_free(&vm);
        return;
    }
    ASSERT_EQ_STR("Base", c->super_class->name);
    ASSERT_EQ_STR("java/lang/Object", c->super_class->super_class->name);
    ASSERT_TRUE(c->super_class->super_class->super_class == NULL);
    ASSERT_TRUE(class_load(&vm, "Hierarchy") == c);
    ASSERT_TRUE(class_is_assignable(&vm, c, "Shape"));
    ASSERT_TRUE(class_is_assignable(&vm, c, "Base"));
    ASSERT_TRUE(!class_is_assignable(&vm, c, "java/lang/String"));
    ASSERT_TRUE(class_find_method(c, "run", "()V", &ref) && ref.owner == c);
    ASSERT_TRUE(class_find_method(c, "hashCode", "()I", &ref) && ref.native != NULL);
    ASSERT_TRUE(class_find_field(c, "id", "I") != NULL);
    ASSERT_TRUE(class_find_field(c, "nada", "I") == NULL);
    object = heap_new_object(&vm, c);
    ASSERT_EQ_INT(2, object->as.fields.count);
    ASSERT_TRUE(object_is_instance(&vm, object, "Named"));
    ASSERT_TRUE(class_load(&vm, "NaoExiste") == NULL);
    ASSERT_TRUE(vm.fatal);
    vm_free(&vm);
}

/**
 * @brief Nativos informam se são estáticos (regressão do fuzzing: `invokestatic`
 *        de `StringBuilder.append` passava o 1º argumento como `this`).
 */
static void test_native_static_flag(void)
{
    Vm vm;
    MethodRef ref;
    RuntimeClass *builder, *string;
    vm_init(&vm, NULL, stdout);
    builder = class_load(&vm, "java/lang/StringBuilder");
    string = class_load(&vm, "java/lang/String");
    ASSERT_TRUE(builder != NULL && string != NULL);
    ASSERT_TRUE(class_find_method(builder, "append", "(I)Ljava/lang/StringBuilder;", &ref));
    ASSERT_TRUE(ref.native != NULL && !ref.is_static);
    ASSERT_TRUE(class_find_method(string, "valueOf", "(I)Ljava/lang/String;", &ref));
    ASSERT_TRUE(ref.native != NULL && ref.is_static);
    vm_free(&vm);
}

/** @brief Todos os opcodes 0x00–0xC9 têm função na tabela; 0xCA–0xFF não (I-10). */
static void test_handler_table(void)
{
    int op;
    int missing = 0;
    for (op = 0x00; op <= 0xC9; op++) {
        if (interpreter_is_unimplemented(interpreter_handler((uint8_t)op))) {
            fprintf(stderr, "    opcode sem funcao: 0x%02X\n", (unsigned)op);
            missing++;
        }
    }
    ASSERT_EQ_INT(0, missing);
    for (op = 0xCA; op <= 0xFF; op++) {
        ASSERT_TRUE(interpreter_is_unimplemented(interpreter_handler((uint8_t)op)));
    }
}

/** @brief Executa os testes das estruturas de execução. */
void suite_vm(void)
{
    RUN_TEST(test_push_pop_cat1);
    RUN_TEST(test_push_pop_cat2);
    RUN_TEST(test_push_pop_ref);
    RUN_TEST(test_stack_limits);
    RUN_TEST(test_heap);
    RUN_TEST(test_class_loader);
    RUN_TEST(test_native_static_flag);
    RUN_TEST(test_handler_table);
}
