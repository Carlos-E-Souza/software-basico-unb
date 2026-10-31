/**
 * @file test_viewer.c
 * @brief Testes do exibidor: formatação auxiliar, desmontagem e saída completa.
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/class_file.h"
#include "test.h"
#include "test_util.h"
#include "viewer/format.h"
#include "viewer/viewer.h"

/** @brief Tamanho máximo da saída capturada nos testes. */
#define CAPTURE_SIZE (256 * 1024)

/** @brief Saída capturada do último viewer_print_class(). */
static char captured[CAPTURE_SIZE];

/**
 * @brief Abre um arquivo temporário de leitura e escrita dentro de build/.
 *
 * @details Não usa `tmpfile()` porque, no Windows, ele tenta criar o arquivo
 * na raiz do disco, o que exige permissão de administrador.
 */
static FILE *open_temp(void)
{
    return fopen(FIXTURES_DIR "/../viewer_test.tmp", "w+b");
}

/** @brief Executa o exibidor sobre uma classe e captura a saída em #captured. */
static void capture(const ClassFile *cf)
{
    FILE *tmp = open_temp();
    size_t n;
    captured[0] = '\0';
    if (tmp == NULL) {
        TEST_FAIL_("%s", "nao foi possivel criar o arquivo temporario");
        return;
    }
    viewer_print_class(tmp, cf, "teste");
    rewind(tmp);
    n = fread(captured, 1, CAPTURE_SIZE - 1, tmp);
    captured[n] = '\0';
    fclose(tmp);
}

/** @brief Verifica se a saída capturada contém um trecho. */
#define ASSERT_OUTPUT_HAS(text)                                               \
    do {                                                                      \
        if (strstr(captured, (text)) == NULL)                                 \
            TEST_FAIL_("saida nao contem \"%s\"", (text));                    \
    } while (0)

/** @brief Flags são decodificadas conforme o contexto. */
static void test_access_flags(void)
{
    char text[FLAGS_BUFFER_SIZE];
    format_access_flags(text, sizeof text, 0x0021, FLAGS_CLASS);
    ASSERT_EQ_STR("public super", text);
    format_access_flags(text, sizeof text, 0x0021, FLAGS_METHOD);
    ASSERT_EQ_STR("public synchronized", text);
    format_access_flags(text, sizeof text, 0x00C8, FLAGS_FIELD);
    ASSERT_EQ_STR("static volatile transient", text);
    format_access_flags(text, sizeof text, 0x0000, FLAGS_FIELD);
    ASSERT_EQ_STR("", text);
    format_access_flags(text, 8, 0x0009, FLAGS_METHOD); /* buffer pequeno: não estoura */
    ASSERT_TRUE(strlen(text) < 8);
}

/** @brief Texto MUTF-8 vira UTF-8, com controles e NUL escapados. */
static void test_print_mutf8(void)
{
    static const char text[] = "a\n\xC0\x80\xC3\xA9\xED\xA0\xBD\xED\xB8\x80";
    FILE *tmp = open_temp();
    char out[64];
    size_t n;
    if (tmp == NULL) {
        TEST_FAIL_("%s", "nao foi possivel criar o arquivo temporario");
        return;
    }
    print_mutf8(tmp, text, sizeof text - 1);
    rewind(tmp);
    n = fread(out, 1, sizeof out - 1, tmp);
    out[n] = '\0';
    fclose(tmp);
    ASSERT_EQ_STR("a\\n\\u0000\xC3\xA9\xF0\x9F\x98\x80", out);
}

/** @brief jsr/ret, goto_w, wide e opcode inválido em bytecode montado à mão. */
static void test_disassemble_rare_instructions(void)
{
    static const uint8_t code[] = {
        0xA8, 0x00, 0x0C,             /*  0: jsr 12 */
        0xC8, 0x00, 0x00, 0x00, 0x0A, /*  3: goto_w 13 */
        0xC4, 0x15, 0x01, 0x2C,       /*  8: iload_w 300 (wide) */
        0x4C,                         /* 12: astore_1 (sub-rotina: guarda o endereço de retorno) */
        0xA9, 0x01,                   /* 13: ret 1 */
        0xC4, 0x84, 0x01, 0x2C, 0xFF, 0xFB, /* 15: iinc_w 300, -5 */
        0xCA                          /* 21: breakpoint (reservado) */
    };
    ByteBuf b;
    ClassFile *cf = NULL;
    bb_class_with_code(&b, code, sizeof code);
    ASSERT_EQ_INT(CF_OK, class_file_parse(b.data, b.size, &cf, NULL));
    if (cf == NULL) {
        return;
    }
    capture(cf);
    ASSERT_OUTPUT_HAS("    0 jsr 12 (+12)\n");
    ASSERT_OUTPUT_HAS("    3 goto_w 13 (+10)\n");
    ASSERT_OUTPUT_HAS("    8 wide iload 300\n");
    ASSERT_OUTPUT_HAS("   13 ret 1\n");
    ASSERT_OUTPUT_HAS("   15 wide iinc 300 by -5\n");
    ASSERT_OUTPUT_HAS("   21 <instrucao invalida ou truncada: opcode 0xCA>");
    class_file_free(cf);
}

/** @brief Saída completa de um fixture: seções, rótulos e referências ao pool no estilo do jclasslib. */
static void test_full_output_of_fixture(void)
{
    ClassFile *cf = NULL;
    ClassFileError err;
    if (class_file_load(FIXTURES_DIR "/java8/Flow.class", &cf, &err) != CF_OK) {
        TEST_FAIL_("falha ao ler Flow.class: %s", err.message);
        return;
    }
    capture(cf);
    /* informações gerais */
    ASSERT_OUTPUT_HAS("=== General Information ===");
    ASSERT_OUTPUT_HAS("Major version:           52 [Java 8]");
    ASSERT_OUTPUT_HAS("Access flags:            0x0021 [public super]");
    ASSERT_OUTPUT_HAS("This class:              cp_info #9 <Flow>");
    ASSERT_OUTPUT_HAS("Super class:             cp_info #10 <java/lang/Object>");
    /* pool: índice do slot, tipo e campos com a constante representada */
    ASSERT_OUTPUT_HAS("=== Constant Pool (count = 60) ===");
    ASSERT_OUTPUT_HAS("  [1] CONSTANT_Methodref_info\n"
                      "        Class name:              cp_info #10 <java/lang/Object>\n"
                      "        Name and type:           cp_info #48 <<init> : ()V>\n");
    ASSERT_OUTPUT_HAS("  [3] CONSTANT_String_info\n"
                      "        String:                  cp_info #50 <divisao por zero>\n");
    ASSERT_OUTPUT_HAS("  [15] CONSTANT_Utf8_info\n"
                      "        Length of byte array:    6\n"
                      "        Length of string:        6\n"
                      "        String:                  <init>\n");
    /* membros e atributos */
    ASSERT_OUTPUT_HAS("=== Methods (count = 5) ===");
    ASSERT_OUTPUT_HAS("Name:                    cp_info #22 <divide>");
    ASSERT_OUTPUT_HAS("Descriptor:              cp_info #23 <(II)I>");
    ASSERT_OUTPUT_HAS("Access flags:            0x0008 [static]");
    ASSERT_OUTPUT_HAS("Attribute name index:    cp_info #17 <Code>");
    ASSERT_OUTPUT_HAS("Maximum stack size:      3");
    /* bytecode: mnemônicos com os argumentos do pool resolvidos */
    ASSERT_OUTPUT_HAS("    1 invokespecial #1 <java/lang/Object.<init> : ()V>\n");
    ASSERT_OUTPUT_HAS("    4 new #2 <Flow$Failure>\n");
    ASSERT_OUTPUT_HAS("    8 ldc #3 <divisao por zero>\n");
    ASSERT_OUTPUT_HAS("   24 invokestatic #6 <Flow.divide : (II)I>\n");
    ASSERT_OUTPUT_HAS("    4 multianewarray #5 <[[I> dim 2\n");
    ASSERT_OUTPUT_HAS("   10 newarray 11 (long)\n");
    ASSERT_OUTPUT_HAS("    1 ifne 14 (+13)\n");
    ASSERT_OUTPUT_HAS("   57 goto 15 (-42)\n");
    ASSERT_OUTPUT_HAS("   37 iinc 0 by -1\n");
    ASSERT_OUTPUT_HAS("    1 tableswitch 1 to 3\n");
    ASSERT_OUTPUT_HAS("    1 lookupswitch 3\n");
    ASSERT_OUTPUT_HAS("default:  37 (+36)");
    /* tabelas e atributos */
    ASSERT_OUTPUT_HAS("cp_info #2 <Flow$Failure>");
    ASSERT_OUTPUT_HAS("cp_info #0 <any>");
    ASSERT_OUTPUT_HAS("full_frame (frame_type = 255");
    ASSERT_OUTPUT_HAS("=== Attributes (count = 2) ===");
    ASSERT_OUTPUT_HAS("Source file name index:  cp_info #47 <Flow.java>");
    class_file_free(cf);
}

/** @brief Constantes numéricas e de texto: bytes brutos e valores no formato do Java. */
static void test_constant_values(void)
{
    ClassFile *cf = NULL;
    if (class_file_load(FIXTURES_DIR "/java2/AllConstants.class", &cf, NULL) != CF_OK) {
        TEST_FAIL_("%s", "falha ao ler AllConstants.class");
        return;
    }
    capture(cf);
    ASSERT_OUTPUT_HAS("Major version:           46 [Java 1.2]");
    ASSERT_OUTPUT_HAS("High bytes:              0x0000011F\n"
                      "        Low bytes:               0x71FB04CB\n"
                      "        Long:                    1234567890123\n");
    ASSERT_OUTPUT_HAS("(large numeric continued)");
    ASSERT_OUTPUT_HAS("Bytes:                   0x40600000\n"
                      "        Float:                   3.5\n");
    ASSERT_OUTPUT_HAS("Double:                  2.718281828\n");
    ASSERT_OUTPUT_HAS("Integer:                 123456789\n");
    /* 18 bytes MUTF-8 (é = 2, 中 = 3, NUL = 2, emoji = 6) e 10 caracteres UTF-16
     * (o emoji é um par de surrogates: 2 caracteres) */
    ASSERT_OUTPUT_HAS("Length of byte array:    18\n"
                      "        Length of string:        10\n"
                      "        String:                  "
                      "Ola \xC3\xA9\xE4\xB8\xAD\\u0000\xF0\x9F\x98\x80!\n");
    ASSERT_OUTPUT_HAS("Constant value index:    cp_info #");
    class_file_free(cf);
}

/** @brief Referência a cp_info #0 (nenhum) é escrita sem constante. */
static void test_cp_ref_zero(void)
{
    ClassFile *cf = NULL;
    FILE *tmp;
    char out[64];
    size_t n;
    if (class_file_load(FIXTURES_DIR "/java8/Flow.class", &cf, NULL) != CF_OK) {
        TEST_FAIL_("%s", "falha ao ler Flow.class");
        return;
    }
    tmp = open_temp();
    if (tmp != NULL) {
        print_cp_ref(tmp, cf, 0);
        fputc('|', tmp);
        print_cp_ref(tmp, cf, 9);
        rewind(tmp);
        n = fread(out, 1, sizeof out - 1, tmp);
        out[n] = '\0';
        fclose(tmp);
        ASSERT_EQ_STR("cp_info #0|cp_info #9 <Flow>", out);
    }
    class_file_free(cf);
}

/** @brief Registra os testes desta suíte. */
void suite_viewer(void)
{
    RUN_TEST(test_access_flags);
    RUN_TEST(test_print_mutf8);
    RUN_TEST(test_disassemble_rare_instructions);
    RUN_TEST(test_full_output_of_fixture);
    RUN_TEST(test_constant_values);
    RUN_TEST(test_cp_ref_zero);
}
