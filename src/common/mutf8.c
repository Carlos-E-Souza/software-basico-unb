/**
 * @file mutf8.c
 * @brief Implementação da validação e decodificação de MUTF-8.
 */
#include "common/mutf8.h"

/**
 * @brief Indica se um byte é de continuação (`10xxxxxx`).
 *
 * @details **Algoritmo:** testa se os dois bits mais altos valem `10`.
 *
 * @param[in] byte Byte a testar.
 * @return `true` se for byte de continuação.
 */
static bool is_continuation(uint8_t byte)
{
    return (byte & 0xC0u) == 0x80u;
}

bool mutf8_is_valid(const uint8_t *bytes, size_t length)
{
    size_t i = 0;
    while (i < length) {
        uint8_t b = bytes[i];
        if (b >= 0x01u && b <= 0x7Fu) {
            i += 1;
        } else if ((b & 0xE0u) == 0xC0u) {
            if (i + 1 >= length || !is_continuation(bytes[i + 1])) {
                return false;
            }
            i += 2;
        } else if ((b & 0xF0u) == 0xE0u) {
            if (i + 2 >= length || !is_continuation(bytes[i + 1]) ||
                !is_continuation(bytes[i + 2])) {
                return false;
            }
            i += 3;
        } else {
            return false; /* 0x00, continuação solta ou 0xF0-0xFF */
        }
    }
    return true;
}

size_t mutf8_to_utf16(const uint8_t *bytes, size_t length, uint16_t *out)
{
    size_t i = 0;
    size_t count = 0;
    while (i < length) {
        uint8_t b = bytes[i];
        uint16_t unit;
        if (b < 0x80u) {
            unit = b;
            i += 1;
        } else if ((b & 0xE0u) == 0xC0u && i + 1 < length) {
            unit = (uint16_t)(((b & 0x1Fu) << 6) | (bytes[i + 1] & 0x3Fu));
            i += 2;
        } else if (i + 2 < length) {
            unit = (uint16_t)(((b & 0x0Fu) << 12) |
                              ((bytes[i + 1] & 0x3Fu) << 6) |
                              (bytes[i + 2] & 0x3Fu));
            i += 3;
        } else {
            break; /* entrada inválida: interrompe com segurança */
        }
        if (out != NULL) {
            out[count] = unit;
        }
        count++;
    }
    return count;
}
