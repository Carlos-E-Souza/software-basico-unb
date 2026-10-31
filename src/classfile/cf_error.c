/**
 * @file cf_error.c
 * @brief Implementação do registro de erros da leitura de `.class`.
 */
#include <stdarg.h>
#include <stdio.h>

#include "classfile/cf_error.h"

CfStatus cf_fail(ClassFileError *err, CfStatus status, size_t offset,
                 const char *format, ...)
{
    va_list args;
    if (err == NULL) {
        return status;
    }
    err->status = status;
    err->offset = offset;
    va_start(args, format);
    vsnprintf(err->message, sizeof err->message, format, args);
    va_end(args);
    return status;
}

const char *cf_status_name(CfStatus status)
{
    switch (status) {
    case CF_OK:                      return "sucesso";
    case CF_ERR_IO:                  return "erro de leitura do arquivo";
    case CF_ERR_MEMORY:              return "memoria insuficiente";
    case CF_ERR_TRUNCATED:           return "arquivo truncado";
    case CF_ERR_BAD_MAGIC:           return "numero magico invalido";
    case CF_ERR_UNSUPPORTED_VERSION: return "versao nao suportada";
    case CF_ERR_BAD_CONSTANT:        return "constante invalida";
    case CF_ERR_BAD_INDEX:           return "indice invalido no pool de constantes";
    case CF_ERR_BAD_ATTRIBUTE:       return "atributo invalido";
    case CF_ERR_TRAILING_BYTES:      return "bytes extras no fim do arquivo";
    }
    return "erro desconhecido";
}
