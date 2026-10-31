/**
 * @file test_number_format.c
 * @brief Testes da formatação de float/double no estilo Java.
 *
 * @details Os valores esperados foram obtidos executando `Double.toString`
 * e `Float.toString` no Java 8 (OpenJDK 1.8.0_504) para os mesmos bits.
 */
#include <stdint.h>
#include <string.h>

#include "common/number_format.h"
#include "test.h"

/** @brief Par (bits IEEE 754, texto esperado do Java) para double. */
typedef struct DoubleCase {
    uint64_t bits;        /**< Bits do double. */
    const char *expected; /**< Saída de Double.toString. */
} DoubleCase;

/** @brief Par (bits IEEE 754, texto esperado do Java) para float. */
typedef struct FloatCase {
    uint32_t bits;        /**< Bits do float. */
    const char *expected; /**< Saída de Float.toString. */
} FloatCase;

/** @brief Casos de double validados contra o Java. */
static const DoubleCase DOUBLES[] = {
    {0x0000000000000000ULL, "0.0"},
    {0x8000000000000000ULL, "-0.0"},
    {0x3FF0000000000000ULL, "1.0"},
    {0xBFF8000000000000ULL, "-1.5"},
    {0x4059000000000000ULL, "100.0"},
    {0x4132D68700000000ULL, "1234567.0"},
    {0x416312CFE0000000ULL, "9999999.0"},
    {0x416312D000000000ULL, "1.0E7"},
    {0x41678C29DCCCCCCDULL, "1.23456789E7"},
    {0x3F50624DD2F1A9FCULL, "0.001"},
    {0x3F5061E273273F09ULL, "9.999E-4"},
    {0x3FB999999999999AULL, "0.1"},
    {0x3FD3333333333334ULL, "0.30000000000000004"},
    {0x3FD5555555555555ULL, "0.3333333333333333"},
    {0x3FE5555555555555ULL, "0.6666666666666666"},
    {0x4202A05F20000000ULL, "1.0E10"},
    {0x3EEF75104D551D69ULL, "1.5E-5"},
    {0x405EDD2F1A9FBE77ULL, "123.456"},
    {0x400921FB54442D18ULL, "3.141592653589793"},
    {0x4005BF0A8B145769ULL, "2.718281828459045"},
    {0x7FEFFFFFFFFFFFFFULL, "1.7976931348623157E308"},
    {0x0000000000000001ULL, "4.9E-324"},
    {0x0000000000000001ULL, "4.9E-324"},
    {0x444B1AE4D6E2EF50ULL, "1.0E21"},
    {0x3E7AD7F29ABCAF48ULL, "1.0E-7"},
    {0x0000000000000001ULL, "4.9E-324"},
    {0x0010000000000000ULL, "2.2250738585072014E-308"},
    {0x4059200000000000ULL, "100.5"},
    {0x3FE0000000000000ULL, "0.5"},
    {0x54B249AD2594C37DULL, "1.0E100"},
    {0x42DC12218377DE40ULL, "1.23456789012345E14"},
};

/** @brief Casos de float validados contra o Java. */
static const FloatCase FLOATS[] = {
    {0x00000000UL, "0.0"},
    {0x80000000UL, "-0.0"},
    {0x3F800000UL, "1.0"},
    {0x3DCCCCCDUL, "0.1"},
    {0x40600000UL, "3.5"},
    {0x3EAAAAABUL, "0.33333334"},
    {0x42C80000UL, "100.0"},
    {0x4B189680UL, "1.0E7"},
    {0x501502F9UL, "1.0E10"},
    {0x377BA882UL, "1.5E-5"},
    {0x7F7FFFFFUL, "3.4028235E38"},
    {0x00000001UL, "1.4E-45"},
    {0x4B800000UL, "1.6777216E7"},
    {0x3E99999AUL, "0.3"},
    {0x3B23D70AUL, "0.0025"},
    {0x42F6E979UL, "123.456"},
    {0x40490FDBUL, "3.1415927"},
};

/** @brief Todos os doubles da tabela devem sair iguais ao Java. */
static void test_doubles_match_java(void)
{
    size_t i;
    char text[NUMBER_FORMAT_SIZE];
    for (i = 0; i < sizeof DOUBLES / sizeof DOUBLES[0]; i++) {
        double d;
        memcpy(&d, &DOUBLES[i].bits, sizeof d);
        java_format_double(text, sizeof text, d);
        ASSERT_EQ_STR(DOUBLES[i].expected, text);
    }
}

/** @brief Todos os floats da tabela devem sair iguais ao Java. */
static void test_floats_match_java(void)
{
    size_t i;
    char text[NUMBER_FORMAT_SIZE];
    for (i = 0; i < sizeof FLOATS / sizeof FLOATS[0]; i++) {
        float f;
        uint32_t bits = (uint32_t)FLOATS[i].bits;
        memcpy(&f, &bits, sizeof f);
        java_format_float(text, sizeof text, f);
        ASSERT_EQ_STR(FLOATS[i].expected, text);
    }
}

/** @brief Casos especiais: NaN e infinitos. */
static void test_special_values(void)
{
    char text[NUMBER_FORMAT_SIZE];
    double zero = 0.0;
    java_format_double(text, sizeof text, zero / zero);
    ASSERT_EQ_STR("NaN", text);
    java_format_double(text, sizeof text, 1.0 / zero);
    ASSERT_EQ_STR("Infinity", text);
    java_format_float(text, sizeof text, (float)(-1.0 / zero));
    ASSERT_EQ_STR("-Infinity", text);
}

/** @brief Registra os testes desta suíte. */
void suite_number_format(void)
{
    RUN_TEST(test_doubles_match_java);
    RUN_TEST(test_floats_match_java);
    RUN_TEST(test_special_values);
}
