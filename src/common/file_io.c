/**
 * @file file_io.c
 * @brief Implementação da leitura de arquivos inteiros para a memória.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common/file_io.h"

bool file_read_all(const char *path, uint8_t **data, size_t *size)
{
    FILE *file;
    long length;
    uint8_t *buffer;

    *data = NULL;
    *size = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    buffer = malloc(length > 0 ? (size_t)length : 1u);
    if (buffer == NULL) {
        fclose(file);
        return false;
    }
    if (length > 0 && fread(buffer, 1, (size_t)length, file) != (size_t)length) {
        free(buffer);
        fclose(file);
        return false;
    }
    fclose(file);

    *data = buffer;
    *size = (size_t)length;
    return true;
}
