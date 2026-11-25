/**
 * @file test_vm_exec.c
 * @brief Testes unitários de instruções isoladas: todas as formas de
 *        `dup*`/`pop*`/`swap`, casos-limite da aritmética e das conversões,
 *        `jsr`/`ret` montado à mão e `wide` (I-03, I-04, I-05, D10).
 *
 * @details Cada teste monta um bytecode pequeno, executa-o com
 * interpreter_step() em um frame sem classe e confere a pilha.
 */
#include <string.h>

#include "test.h"
#include "vm/frame.h"
#include "vm/interpreter.h"

/** @brief Ambiente de execução de um teste: VM, classe e frame. */
typedef struct ExecEnv {
    Vm vm;               /**< Máquina virtual. */
    RuntimeClass cls;    /**< Classe fictícia dona do código. */
    CodeAttribute code;  /**< Atributo `Code` com o bytecode do teste. */
    Frame *frame;        /**< Frame em execução. */
} ExecEnv;

/**
 * @brief Prepara o ambiente e executa o bytecode até o fim, um erro ou exceção.
 *
 * @details **Algoritmo:** cria VM, classe fictícia e frame (pilha 8,
 * locais 4) e chama interpreter_step() enquanto `pc` estiver no código.
 *
 * @param[out] env    Ambiente.
 * @param[in]  code   Bytecode.
 * @param[in]  length Tamanho do bytecode.
 * @return `true` se executou até o fim sem exceção nem erro fatal.
 */
static bool run_code(ExecEnv *env, const uint8_t *code, uint32_t length)
{
    bool ok = true;
    memset(env, 0, sizeof *env);
    vm_init(&env->vm, NULL, stdout);
    env->cls.name = "Teste";
    env->code.max_stack = 8;
    env->code.max_locals = 4;
    env->code.code_length = length;
    env->code.code = (uint8_t *)code;
    env->frame = frame_create(&env->vm, &env->cls, NULL, &env->code);
    while (ok && env->frame->pc < length) {
        ok = interpreter_step(env->frame);
    }
    return ok;
}

/**
 * @brief Libera o ambiente de um teste.
 * @details **Algoritmo:** frame_free() e vm_free().
 * @param[in,out] env Ambiente.
 */
static void finish(ExecEnv *env)
{
    frame_free(env->frame);
    vm_free(&env->vm);
}

/**
 * @brief Executa `iconst_0 iconst_1 iconst_2 iconst_3 <op>` e confere a pilha.
 * @details **Algoritmo:** compara slot a slot com @p expected (de baixo para cima).
 * @param[in] opcode   Instrução de pilha testada.
 * @param[in] expected Valores esperados (string de dígitos).
 */
static void check_stack_op(uint8_t opcode, const char *expected)
{
    uint8_t code[] = {0x03, 0x04, 0x05, 0x06, 0x00};
    ExecEnv env;
    size_t i, n = strlen(expected);
    code[4] = opcode;
    ASSERT_TRUE(run_code(&env, code, sizeof code));
    ASSERT_EQ_INT(n, env.frame->sp);
    for (i = 0; i < n && i < env.frame->sp; i++) {
        ASSERT_EQ_INT(expected[i] - '0', env.frame->stack[i]);
    }
    finish(&env);
}

/** @brief Formas de categoria 1 de `pop`, `pop2`, `dup*` e `swap` (JVMS §6.5). */
static void test_stack_ops_cat1(void)
{
    check_stack_op(0x57, "012");     /* pop */
    check_stack_op(0x58, "01");      /* pop2 */
    check_stack_op(0x59, "01233");   /* dup */
    check_stack_op(0x5A, "01323");   /* dup_x1 */
    check_stack_op(0x5B, "03123");   /* dup_x2 */
    check_stack_op(0x5C, "012323");  /* dup2 */
    check_stack_op(0x5D, "023123");  /* dup2_x1 */
    check_stack_op(0x5E, "230123");  /* dup2_x2 */
    check_stack_op(0x5F, "0132");    /* swap */
}

/** @brief Formas de categoria 2: `dup2`, `pop2`, `dup2_x1` e `dup2_x2` com `long`. */
static void test_stack_ops_cat2(void)
{
    /* lconst_1 dup2 ladd -> 2 */
    static const uint8_t dup2[] = {0x0A, 0x5C, 0x61};
    /* iconst_5 lconst_1 dup2_x1 -> (long 1) 5 (long 1) */
    static const uint8_t dup2_x1[] = {0x08, 0x0A, 0x5D};
    /* lconst_0 lconst_1 dup2_x2 -> 1 0 1 */
    static const uint8_t dup2_x2[] = {0x09, 0x0A, 0x5E};
    /* lconst_1 iconst_2 dup_x2 -> 2 (long 1) 2 */
    static const uint8_t dup_x2[] = {0x0A, 0x05, 0x5B};
    ExecEnv env;
    ASSERT_TRUE(run_code(&env, dup2, sizeof dup2));
    ASSERT_EQ_INT(2, frame_pop_long(env.frame));
    finish(&env);
    ASSERT_TRUE(run_code(&env, dup2_x1, sizeof dup2_x1));
    ASSERT_EQ_INT(5, env.frame->sp);
    ASSERT_EQ_INT(1, frame_pop_long(env.frame));
    ASSERT_EQ_INT(5, frame_pop_int(env.frame));
    ASSERT_EQ_INT(1, frame_pop_long(env.frame));
    finish(&env);
    ASSERT_TRUE(run_code(&env, dup2_x2, sizeof dup2_x2));
    ASSERT_EQ_INT(1, frame_pop_long(env.frame));
    ASSERT_EQ_INT(0, frame_pop_long(env.frame));
    ASSERT_EQ_INT(1, frame_pop_long(env.frame));
    finish(&env);
    ASSERT_TRUE(run_code(&env, dup_x2, sizeof dup_x2));
    ASSERT_EQ_INT(2, frame_pop_int(env.frame));
    ASSERT_EQ_INT(1, frame_pop_long(env.frame));
    ASSERT_EQ_INT(2, frame_pop_int(env.frame));
    finish(&env);
}

/**
 * @brief Executa um bytecode que deixa um `int` na pilha e confere o valor.
 * @details **Algoritmo:** run_code() e frame_pop_int().
 * @param[in] code     Bytecode.
 * @param[in] length   Tamanho.
 * @param[in] expected Valor esperado.
 */
static void check_int(const uint8_t *code, uint32_t length, int32_t expected)
{
    ExecEnv env;
    ASSERT_TRUE(run_code(&env, code, length));
    ASSERT_EQ_INT(expected, frame_pop_int(env.frame));
    finish(&env);
}

/**
 * @brief Executa um bytecode que deixa um `long` na pilha e confere o valor.
 * @details **Algoritmo:** run_code() e frame_pop_long().
 * @param[in] code     Bytecode.
 * @param[in] length   Tamanho.
 * @param[in] expected Valor esperado.
 */
static void check_long(const uint8_t *code, uint32_t length, int64_t expected)
{
    ExecEnv env;
    ASSERT_TRUE(run_code(&env, code, length));
    ASSERT_TRUE(frame_pop_long(env.frame) == expected);
    finish(&env);
}

/** @brief Casos-limite de `int`: *overflow*, `MIN / -1`, `MIN % -1`, deslocamentos. */
static void test_int_limits(void)
{
    /* MIN = 1 << 31 */
    static const uint8_t div[] = {0x04, 0x10, 31, 0x78, 0x02, 0x6C};
    static const uint8_t rem[] = {0x04, 0x10, 31, 0x78, 0x02, 0x70};
    static const uint8_t overflow[] = {0x04, 0x10, 31, 0x78, 0x02, 0x60, 0x04, 0x60};
    static const uint8_t shl[] = {0x04, 0x10, 33, 0x78};
    static const uint8_t shr[] = {0x10, (uint8_t)-16, 0x05, 0x7A};
    static const uint8_t ushr[] = {0x10, (uint8_t)-16, 0x10, 28, 0x7C};
    static const uint8_t neg[] = {0x04, 0x10, 31, 0x78, 0x74};
    check_int(div, sizeof div, INT32_MIN);
    check_int(rem, sizeof rem, 0);
    check_int(overflow, sizeof overflow, INT32_MIN);
    check_int(shl, sizeof shl, 2);
    check_int(shr, sizeof shr, -4);
    check_int(ushr, sizeof ushr, 15);
    check_int(neg, sizeof neg, INT32_MIN);
}

/** @brief Casos-limite de `long`: `MIN / -1`, máscara 0x3F, deslocamento aritmético. */
static void test_long_limits(void)
{
    static const uint8_t div[] = {0x0A, 0x10, 63, 0x79, 0x02, 0x85, 0x6D};
    static const uint8_t rem[] = {0x0A, 0x10, 63, 0x79, 0x02, 0x85, 0x71};
    static const uint8_t shl[] = {0x0A, 0x10, 65, 0x79};
    static const uint8_t shr[] = {0x02, 0x85, 0x10, 70, 0x7B};
    static const uint8_t ushr[] = {0x02, 0x85, 0x10, 60, 0x7D};
    static const uint8_t cmp[] = {0x09, 0x0A, 0x94};
    check_long(div, sizeof div, INT64_MIN);
    check_long(rem, sizeof rem, 0);
    check_long(shl, sizeof shl, 2);
    check_long(shr, sizeof shr, -1);
    check_long(ushr, sizeof ushr, 15);
    check_int(cmp, sizeof cmp, -1);
}

/** @brief Divisão inteira por zero lança `ArithmeticException`. */
static void test_div_by_zero(void)
{
    static const uint8_t idiv[] = {0x04, 0x03, 0x6C};
    static const uint8_t lrem[] = {0x0A, 0x09, 0x71};
    ExecEnv env;
    ASSERT_TRUE(!run_code(&env, idiv, sizeof idiv));
    ASSERT_TRUE(env.vm.exception != NULL && !env.vm.fatal);
    ASSERT_EQ_STR("java/lang/ArithmeticException", env.vm.exception->class_ref->name);
    finish(&env);
    ASSERT_TRUE(!run_code(&env, lrem, sizeof lrem));
    ASSERT_TRUE(env.vm.exception != NULL);
    finish(&env);
}

/** @brief Conversões: NaN → 0, saturação, `i2b`/`i2c`/`i2s` e comparações com NaN. */
static void test_conversions(void)
{
    /* 0f/0f = NaN; 1f/0f = +inf; 1d/0d = +inf */
    static const uint8_t f2i_nan[] = {0x0B, 0x0B, 0x6E, 0x8B};
    static const uint8_t f2i_inf[] = {0x0C, 0x0B, 0x6E, 0x8B};
    static const uint8_t f2i_ninf[] = {0x0C, 0x0B, 0x6E, 0x76, 0x8B};
    static const uint8_t d2l_inf[] = {0x0F, 0x0E, 0x6F, 0x8F};
    static const uint8_t d2i_nan[] = {0x0E, 0x0E, 0x6F, 0x8E};
    static const uint8_t i2b[] = {0x11, 0x00, 200, 0x91};
    static const uint8_t i2c[] = {0x02, 0x92};
    static const uint8_t i2s[] = {0x11, 0x4E, 0x20, 0x05, 0x68, 0x93};
    static const uint8_t l2i[] = {0x0A, 0x10, 32, 0x79, 0x0A, 0x61, 0x88};
    static const uint8_t fcmpl[] = {0x0B, 0x0B, 0x6E, 0x0C, 0x95};
    static const uint8_t fcmpg[] = {0x0B, 0x0B, 0x6E, 0x0C, 0x96};
    static const uint8_t dcmpg[] = {0x0F, 0x0E, 0x98};
    check_int(f2i_nan, sizeof f2i_nan, 0);
    check_int(f2i_inf, sizeof f2i_inf, INT32_MAX);
    check_int(f2i_ninf, sizeof f2i_ninf, INT32_MIN);
    check_long(d2l_inf, sizeof d2l_inf, INT64_MAX);
    check_int(d2i_nan, sizeof d2i_nan, 0);
    check_int(i2b, sizeof i2b, -56);
    check_int(i2c, sizeof i2c, 65535);
    check_int(i2s, sizeof i2s, -25536);
    check_int(l2i, sizeof l2i, 1);
    check_int(fcmpl, sizeof fcmpl, -1);
    check_int(fcmpg, sizeof fcmpg, 1);
    check_int(dcmpg, sizeof dcmpg, 1);
}

/** @brief `jsr`/`ret` montado à mão (D10) e `wide iinc`/`wide iload`. */
static void test_jsr_ret_wide(void)
{
    /* 0: jsr +8 ; 3: iconst_2 ; 4: goto +7 ; 7: nop ; 8: astore_1 ; 9: ret 1 ; 11: nop */
    static const uint8_t jsr[] = {0xA8, 0x00, 0x08, 0x05, 0xA7, 0x00, 0x07,
                                  0x00, 0x4C, 0xA9, 0x01, 0x00};
    /* iconst_0 istore_0 ; wide iinc 0 1000 ; wide iload 0 */
    static const uint8_t wide[] = {0x03, 0x3B, 0xC4, 0x84, 0x00, 0x00, 0x03, 0xE8,
                                   0xC4, 0x15, 0x00, 0x00};
    ExecEnv env;
    ASSERT_TRUE(run_code(&env, jsr, sizeof jsr));
    ASSERT_EQ_INT(1, env.frame->sp);
    ASSERT_EQ_INT(2, frame_pop_int(env.frame));
    ASSERT_EQ_INT(TAG_RETADDR, env.frame->local_tags[1]);
    ASSERT_EQ_INT(3, env.frame->locals[1]);
    finish(&env);
    check_int(wide, sizeof wide, 1000);
}

/** @brief Desvio para fora do código e opcode inválido são erros fatais (I-10). */
static void test_invalid_code(void)
{
    static const uint8_t jump_out[] = {0xA7, 0x00, 0x10, 0x00};
    static const uint8_t bad_opcode[] = {0xCA};
    static const uint8_t aload_int[] = {0x03, 0x3B, 0x2A};
    ExecEnv env;
    ASSERT_TRUE(run_code(&env, jump_out, sizeof jump_out));
    ASSERT_TRUE(!interpreter_step(env.frame) && env.vm.fatal);
    finish(&env);
    ASSERT_TRUE(!run_code(&env, bad_opcode, sizeof bad_opcode) && env.vm.fatal);
    finish(&env);
    ASSERT_TRUE(!run_code(&env, aload_int, sizeof aload_int) && env.vm.fatal);
    finish(&env);
}

/** @brief Executa os testes de instruções. */
void suite_vm_exec(void)
{
    RUN_TEST(test_stack_ops_cat1);
    RUN_TEST(test_stack_ops_cat2);
    RUN_TEST(test_int_limits);
    RUN_TEST(test_long_limits);
    RUN_TEST(test_div_by_zero);
    RUN_TEST(test_conversions);
    RUN_TEST(test_jsr_ret_wide);
    RUN_TEST(test_invalid_code);
}
