/**
 * @file class_file.c
 * @brief Leitura da estrutura principal `ClassFile`: cabeçalho, flags,
 *        interfaces, campos, métodos e atributos da classe.
 */
#include <stdlib.h>

#include "classfile/attributes.h"
#include "classfile/class_file.h"
#include "classfile/constant_pool.h"
#include "common/file_io.h"

const char *class_file_java_version_name(uint16_t major_version)
{
    switch (major_version) {
    case 45: return "Java 1.1";
    case 46: return "Java 1.2";
    case 47: return "Java 1.3";
    case 48: return "Java 1.4";
    case 49: return "Java 5";
    case 50: return "Java 6";
    case 51: return "Java 7";
    case 52: return "Java 8";
    default: return "desconhecida";
    }
}

/**
 * @brief Lê e valida `magic`, `minor_version` e `major_version`.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê `magic` (u4) e exige #CLASS_FILE_MAGIC;
 * 2. Lê `minor_version` e `major_version` (u2);
 * 3. Exige #CLASS_FILE_MIN_MAJOR ≤ major ≤ #CLASS_FILE_MAX_MAJOR.
 *
 * @param[in,out] reader     Leitor no início do arquivo.
 * @param[out]    class_file Estrutura a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_header(ByteReader *reader, ClassFile *class_file,
                             ClassFileError *err)
{
    if (!br_read_u4(reader, &class_file->magic)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "arquivo menor que o numero magico");
    }
    if (class_file->magic != CLASS_FILE_MAGIC) {
        return cf_fail(err, CF_ERR_BAD_MAGIC, 0,
                       "numero magico 0x%08lX (esperado 0xCAFEBABE)",
                       (unsigned long)class_file->magic);
    }
    if (!br_read_u2(reader, &class_file->minor_version) ||
        !br_read_u2(reader, &class_file->major_version)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "versao do arquivo truncada");
    }
    if (class_file->major_version < CLASS_FILE_MIN_MAJOR ||
        class_file->major_version > CLASS_FILE_MAX_MAJOR) {
        return cf_fail(err, CF_ERR_UNSUPPORTED_VERSION, 6,
                       "versao %u.%u nao suportada (aceitas: %u a %u, Java 1.1 a 8)",
                       (unsigned)class_file->major_version,
                       (unsigned)class_file->minor_version,
                       (unsigned)CLASS_FILE_MIN_MAJOR, (unsigned)CLASS_FILE_MAX_MAJOR);
    }
    return CF_OK;
}

/**
 * @brief Lê `access_flags`, `this_class`, `super_class` e as interfaces.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê os três campos u2 e valida `this_class` (Class) e `super_class`
 *    (0 ou Class);
 * 2. Lê `interfaces_count`, aloca o vetor e valida cada índice (Class).
 *
 * @param[in,out] reader     Leitor posicionado após o pool de constantes.
 * @param[in,out] class_file Estrutura a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_class_info(ByteReader *reader, ClassFile *class_file,
                                 ClassFileError *err)
{
    uint16_t n, i;

    if (!br_read_u2(reader, &class_file->access_flags) ||
        !br_read_u2(reader, &class_file->this_class) ||
        !br_read_u2(reader, &class_file->super_class)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "access_flags/this_class/super_class truncados");
    }
    if (!cp_check(class_file, class_file->this_class, CONSTANT_Class)) {
        return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(reader) - 4,
                       "this_class #%u nao e Class", (unsigned)class_file->this_class);
    }
    if (class_file->super_class != 0 &&
        !cp_check(class_file, class_file->super_class, CONSTANT_Class)) {
        return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(reader) - 2,
                       "super_class #%u nao e Class", (unsigned)class_file->super_class);
    }

    if (!br_read_u2(reader, &n)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "interfaces_count ausente");
    }
    if (n > 0) {
        class_file->interfaces = calloc(n, sizeof(uint16_t));
        if (class_file->interfaces == NULL) {
            return cf_fail(err, CF_ERR_MEMORY, br_offset(reader),
                           "sem memoria para interfaces");
        }
    }
    class_file->interfaces_count = n;
    for (i = 0; i < n; i++) {
        if (!br_read_u2(reader, &class_file->interfaces[i])) {
            return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                           "lista de interfaces truncada");
        }
        if (!cp_check(class_file, class_file->interfaces[i], CONSTANT_Class)) {
            return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(reader) - 2,
                           "interface #%u nao e Class",
                           (unsigned)class_file->interfaces[i]);
        }
    }
    return CF_OK;
}

/**
 * @brief Lê o vetor de campos ou de métodos (`field_info`/`method_info`).
 *
 * @details
 * **Algoritmo:**
 * 1. Lê a quantidade e aloca o vetor zerado (gravando-o em @p out/@p count
 *    para que seja liberado mesmo em caso de erro);
 * 2. Para cada membro, lê `access_flags`, `name_index` e `descriptor_index`
 *    (ambos devem ser Utf8) e os atributos (attributes_parse()).
 *
 * @param[in,out] reader     Leitor posicionado em `fields_count`/`methods_count`.
 * @param[in]     class_file Classe (pool já lido).
 * @param[in]     what       "campo" ou "metodo" (para mensagens).
 * @param[out]    count      Quantidade de membros.
 * @param[out]    out        Vetor alocado.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_members(ByteReader *reader, const ClassFile *class_file,
                              const char *what, uint16_t *count,
                              MemberInfo **out, ClassFileError *err)
{
    uint16_t n, i;

    if (!br_read_u2(reader, &n)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "quantidade de %ss ausente", what);
    }
    if (n > 0) {
        *out = calloc(n, sizeof(MemberInfo));
        if (*out == NULL) {
            return cf_fail(err, CF_ERR_MEMORY, br_offset(reader),
                           "sem memoria para %ss", what);
        }
    }
    *count = n;
    for (i = 0; i < n; i++) {
        MemberInfo *m = &(*out)[i];
        CfStatus s;
        if (!br_read_u2(reader, &m->access_flags) ||
            !br_read_u2(reader, &m->name_index) ||
            !br_read_u2(reader, &m->descriptor_index)) {
            return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                           "%s %u truncado", what, (unsigned)i);
        }
        if (!cp_check(class_file, m->name_index, CONSTANT_Utf8) ||
            !cp_check(class_file, m->descriptor_index, CONSTANT_Utf8)) {
            return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(reader) - 4,
                           "%s %u com nome/descritor que nao e Utf8", what,
                           (unsigned)i);
        }
        s = attributes_parse(reader, class_file, 0, &m->attributes_count,
                             &m->attributes, err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

/**
 * @brief Executa todas as etapas de leitura sobre uma estrutura já alocada.
 *
 * @details **Algoritmo:** chama, em ordem, parse_header(), cp_parse(),
 * cp_validate(), parse_class_info(), parse_members() (campos e métodos),
 * attributes_parse() e verifica que não sobraram bytes. Para no primeiro erro.
 *
 * @param[in,out] reader     Leitor sobre o arquivo inteiro.
 * @param[in,out] class_file Estrutura zerada a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_all(ByteReader *reader, ClassFile *class_file,
                          ClassFileError *err)
{
    CfStatus s;
    if ((s = parse_header(reader, class_file, err)) != CF_OK ||
        (s = cp_parse(reader, class_file, err)) != CF_OK ||
        (s = cp_validate(class_file, err)) != CF_OK ||
        (s = parse_class_info(reader, class_file, err)) != CF_OK ||
        (s = parse_members(reader, class_file, "campo", &class_file->fields_count,
                           &class_file->fields, err)) != CF_OK ||
        (s = parse_members(reader, class_file, "metodo", &class_file->methods_count,
                           &class_file->methods, err)) != CF_OK ||
        (s = attributes_parse(reader, class_file, 0, &class_file->attributes_count,
                              &class_file->attributes, err)) != CF_OK) {
        return s;
    }
    if (br_remaining(reader) != 0) {
        return cf_fail(err, CF_ERR_TRAILING_BYTES, br_offset(reader),
                       "%lu byte(s) extras apos o fim da classe",
                       (unsigned long)br_remaining(reader));
    }
    return CF_OK;
}

CfStatus class_file_parse(const uint8_t *data, size_t size, ClassFile **out,
                          ClassFileError *err)
{
    ByteReader reader;
    ClassFile *class_file;
    CfStatus s;

    *out = NULL;
    if (err != NULL) {
        err->status = CF_OK;
        err->offset = 0;
        err->message[0] = '\0';
    }
    class_file = calloc(1, sizeof *class_file);
    if (class_file == NULL) {
        return cf_fail(err, CF_ERR_MEMORY, 0, "sem memoria para ClassFile");
    }
    br_init(&reader, data, size);
    s = parse_all(&reader, class_file, err);
    if (s != CF_OK) {
        class_file_free(class_file);
        return s;
    }
    *out = class_file;
    return CF_OK;
}

CfStatus class_file_load(const char *path, ClassFile **out, ClassFileError *err)
{
    uint8_t *data;
    size_t size;
    CfStatus s;

    *out = NULL;
    if (!file_read_all(path, &data, &size)) {
        return cf_fail(err, CF_ERR_IO, 0, "nao foi possivel ler '%s'", path);
    }
    s = class_file_parse(data, size, out, err);
    free(data);
    return s;
}

/**
 * @brief Libera um vetor de campos ou métodos.
 *
 * @details **Algoritmo:** libera os atributos de cada membro e o vetor.
 *
 * @param[in] members Vetor de membros (pode ser NULL).
 * @param[in] count   Quantidade de membros.
 */
static void members_free(MemberInfo *members, uint16_t count)
{
    uint16_t i;
    if (members == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        attributes_free(members[i].attributes, members[i].attributes_count);
    }
    free(members);
}

void class_file_free(ClassFile *class_file)
{
    if (class_file == NULL) {
        return;
    }
    attributes_free(class_file->attributes, class_file->attributes_count);
    members_free(class_file->methods, class_file->methods_count);
    members_free(class_file->fields, class_file->fields_count);
    free(class_file->interfaces);
    cp_free(class_file);
    free(class_file);
}
