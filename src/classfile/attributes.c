/**
 * @file attributes.c
 * @brief Implementação da leitura e liberação de atributos.
 *
 * @details Cada tipo de atributo tem sua função `parse_*`, que lê o conteúdo
 * a partir de um sub-leitor limitado a `attribute_length` bytes. Se o
 * conteúdo for menor que o declarado, a leitura falha no sub-leitor e o erro
 * é #CF_ERR_BAD_ATTRIBUTE; se sobrarem bytes, também.
 */
#include <stdlib.h>
#include <string.h>

#include "classfile/attributes.h"
#include "classfile/constant_pool.h"

/** @brief Associação entre nome e tipo de atributo. */
typedef struct AttributeName {
    const char *name;   /**< Nome na especificação. */
    AttributeKind kind; /**< Tipo correspondente. */
} AttributeName;

/** @brief Tabela dos atributos reconhecidos. */
static const AttributeName ATTRIBUTE_NAMES[] = {
    {"ConstantValue", ATTR_CONSTANT_VALUE},
    {"Code", ATTR_CODE},
    {"StackMapTable", ATTR_STACK_MAP_TABLE},
    {"Exceptions", ATTR_EXCEPTIONS},
    {"InnerClasses", ATTR_INNER_CLASSES},
    {"EnclosingMethod", ATTR_ENCLOSING_METHOD},
    {"Synthetic", ATTR_SYNTHETIC},
    {"Signature", ATTR_SIGNATURE},
    {"SourceFile", ATTR_SOURCE_FILE},
    {"LineNumberTable", ATTR_LINE_NUMBER_TABLE},
    {"LocalVariableTable", ATTR_LOCAL_VARIABLE_TABLE},
    {"LocalVariableTypeTable", ATTR_LOCAL_VARIABLE_TYPE_TABLE},
    {"Deprecated", ATTR_DEPRECATED},
    {"BootstrapMethods", ATTR_BOOTSTRAP_METHODS},
};

AttributeKind attribute_kind_from_name(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof ATTRIBUTE_NAMES / sizeof ATTRIBUTE_NAMES[0]; i++) {
        if (strcmp(ATTRIBUTE_NAMES[i].name, name) == 0) {
            return ATTRIBUTE_NAMES[i].kind;
        }
    }
    return ATTR_UNKNOWN;
}

const AttributeInfo *attribute_find(const AttributeInfo *attributes,
                                    uint16_t count, AttributeKind kind)
{
    uint16_t i;
    for (i = 0; i < count; i++) {
        if (attributes[i].kind == kind) {
            return &attributes[i];
        }
    }
    return NULL;
}

/**
 * @brief Gera o erro padrão para atributo cujo conteúdo acabou antes do esperado.
 *
 * @details **Algoritmo:** chama cf_fail() com #CF_ERR_BAD_ATTRIBUTE e a
 * posição atual do sub-leitor.
 *
 * @param[in]  reader Sub-leitor do atributo.
 * @param[in]  what   Nome do atributo ou do campo que faltou.
 * @param[out] err    Detalhes do erro (pode ser NULL).
 * @return #CF_ERR_BAD_ATTRIBUTE.
 */
static CfStatus too_short(const ByteReader *reader, const char *what,
                          ClassFileError *err)
{
    return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(reader),
                   "atributo %s menor que attribute_length indica", what);
}

/**
 * @brief Verifica um índice do pool usado por um atributo.
 *
 * @details **Algoritmo:** se @p allow_zero e o índice é 0, aceita; senão
 * exige cp_check() com a tag dada.
 *
 * @param[in]  class_file Classe.
 * @param[in]  reader     Sub-leitor (para a posição do erro).
 * @param[in]  index      Índice a verificar.
 * @param[in]  tag        Tag esperada.
 * @param[in]  allow_zero Se o valor 0 ("nenhum") é permitido.
 * @param[in]  what       Descrição do campo (para a mensagem).
 * @param[out] err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou #CF_ERR_BAD_INDEX.
 */
static CfStatus check_index(const ClassFile *class_file, const ByteReader *reader,
                            uint16_t index, ConstantTag tag, bool allow_zero,
                            const char *what, ClassFileError *err)
{
    if ((allow_zero && index == 0) || cp_check(class_file, index, tag)) {
        return CF_OK;
    }
    return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(reader),
                   "%s: indice #%u deveria ser %s", what, (unsigned)index,
                   cp_tag_name(tag));
}

/**
 * @brief Aloca um vetor zerado de @p count elementos (NULL quando @p count é 0).
 *
 * @details **Algoritmo:** chama `calloc` somente se @p count > 0 e informa
 * falha de memória por meio de @p ok.
 *
 * @param[in]  count Quantidade de elementos.
 * @param[in]  size  Tamanho de cada elemento.
 * @param[out] ok    `false` se a alocação falhou.
 * @return Ponteiro para o vetor, ou NULL.
 */
static void *alloc_array(size_t count, size_t size, bool *ok)
{
    void *p;
    if (count == 0) {
        *ok = true;
        return NULL;
    }
    p = calloc(count, size);
    *ok = (p != NULL);
    return p;
}

/**
 * @brief Interpreta o atributo ConstantValue.
 *
 * @details **Algoritmo:** lê `constantvalue_index` e exige que aponte para
 * Integer, Float, Long, Double ou String.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_constant_value(ByteReader *r, const ClassFile *class_file,
                                     AttributeInfo *attr, ClassFileError *err)
{
    ConstantTag tag;
    if (!br_read_u2(r, &attr->info.constantvalue_index)) {
        return too_short(r, "ConstantValue", err);
    }
    tag = cp_tag(class_file, attr->info.constantvalue_index);
    if (tag != CONSTANT_Integer && tag != CONSTANT_Float && tag != CONSTANT_Long &&
        tag != CONSTANT_Double && tag != CONSTANT_String) {
        return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(r),
                       "ConstantValue aponta para #%u (%s), que nao e constante",
                       (unsigned)attr->info.constantvalue_index, cp_tag_name(tag));
    }
    return CF_OK;
}

/**
 * @brief Interpreta o atributo Code.
 *
 * @details
 * **Algoritmo:**
 * 1. Lê `max_stack`, `max_locals` e `code_length` (1 a 65535) e copia o bytecode;
 * 2. Lê a tabela de exceções validando `start_pc < end_pc <= code_length`,
 *    `handler_pc < code_length` e `catch_type` (0 ou Class);
 * 3. Lê os atributos aninhados com profundidade @p depth + 1.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     depth      Profundidade atual.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_code(ByteReader *r, const ClassFile *class_file,
                           unsigned depth, AttributeInfo *attr,
                           ClassFileError *err)
{
    CodeAttribute *code;
    const uint8_t *bytes;
    uint16_t length;
    uint16_t i;
    bool ok;

    code = calloc(1, sizeof *code);
    if (code == NULL) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para Code");
    }
    attr->info.code = code;

    if (!br_read_u2(r, &code->max_stack) || !br_read_u2(r, &code->max_locals) ||
        !br_read_u4(r, &code->code_length)) {
        return too_short(r, "Code", err);
    }
    if (code->code_length == 0 || code->code_length > 65535u) {
        return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(r) - 4,
                       "code_length invalido (%lu)", (unsigned long)code->code_length);
    }
    if (!br_read_bytes(r, code->code_length, &bytes)) {
        return too_short(r, "Code (bytecode)", err);
    }
    code->code = malloc(code->code_length);
    if (code->code == NULL) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para bytecode");
    }
    memcpy(code->code, bytes, code->code_length);

    if (!br_read_u2(r, &length)) {
        return too_short(r, "Code (exception_table_length)", err);
    }
    code->exception_table = alloc_array(length, sizeof(ExceptionTableEntry), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para exception_table");
    }
    code->exception_table_length = length;
    for (i = 0; i < length; i++) {
        ExceptionTableEntry *e = &code->exception_table[i];
        CfStatus s;
        if (!br_read_u2(r, &e->start_pc) || !br_read_u2(r, &e->end_pc) ||
            !br_read_u2(r, &e->handler_pc) || !br_read_u2(r, &e->catch_type)) {
            return too_short(r, "Code (exception_table)", err);
        }
        if (e->start_pc >= e->end_pc || e->end_pc > code->code_length ||
            e->handler_pc >= code->code_length) {
            return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(r) - 8,
                           "entrada %u da exception_table fora do codigo", (unsigned)i);
        }
        s = check_index(class_file, r, e->catch_type, CONSTANT_Class, true,
                        "exception_table.catch_type", err);
        if (s != CF_OK) {
            return s;
        }
    }
    return attributes_parse(r, class_file, depth + 1, &code->attributes_count,
                            &code->attributes, err);
}

/**
 * @brief Lê um `verification_type_info` do StackMapTable.
 *
 * @details **Algoritmo:** lê a tag (0 a 8); para ITEM_Object lê e valida o
 * `cpool_index` (Class); para ITEM_Uninitialized lê o `offset`.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    info       Estrutura a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_verification_type(ByteReader *r, const ClassFile *class_file,
                                        VerificationTypeInfo *info,
                                        ClassFileError *err)
{
    if (!br_read_u1(r, &info->tag)) {
        return too_short(r, "StackMapTable", err);
    }
    if (info->tag > ITEM_Uninitialized) {
        return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(r) - 1,
                       "verification_type_info com tag invalida (%u)",
                       (unsigned)info->tag);
    }
    if (info->tag == ITEM_Object || info->tag == ITEM_Uninitialized) {
        if (!br_read_u2(r, &info->value)) {
            return too_short(r, "StackMapTable", err);
        }
        if (info->tag == ITEM_Object) {
            return check_index(class_file, r, info->value, CONSTANT_Class, false,
                               "StackMapTable Object", err);
        }
    }
    return CF_OK;
}

/**
 * @brief Lê um vetor de `verification_type_info`.
 *
 * @details **Algoritmo:** aloca @p count posições, grava o vetor em @p out
 * (para que seja liberado mesmo em caso de erro) e lê cada item com
 * parse_verification_type().
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     count      Quantidade de itens.
 * @param[out]    out        Vetor alocado.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_verification_list(ByteReader *r, const ClassFile *class_file,
                                        uint16_t count, VerificationTypeInfo **out,
                                        ClassFileError *err)
{
    uint16_t i;
    bool ok;
    *out = alloc_array(count, sizeof(VerificationTypeInfo), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para StackMapTable");
    }
    for (i = 0; i < count; i++) {
        CfStatus s = parse_verification_type(r, class_file, &(*out)[i], err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

/**
 * @brief Lê um quadro (`stack_map_frame`) e o normaliza em #StackMapFrame.
 *
 * @details
 * **Algoritmo (JVMS 4.7.4)**, conforme `frame_type`:
 * - 0–63 *same*: `offset_delta = frame_type`;
 * - 64–127 *same_locals_1_stack_item*: `offset_delta = frame_type - 64`, 1 item de pilha;
 * - 128–246: reservados (erro);
 * - 247 *same_locals_1_stack_item_extended*: `offset_delta` (u2) + 1 item de pilha;
 * - 248–250 *chop* e 251 *same_frame_extended*: apenas `offset_delta` (u2);
 * - 252–254 *append*: `offset_delta` + (`frame_type - 251`) locais;
 * - 255 *full_frame*: `offset_delta`, locais e pilha completos.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    frame      Quadro a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_stack_map_frame(ByteReader *r, const ClassFile *class_file,
                                      StackMapFrame *frame, ClassFileError *err)
{
    uint8_t type;
    uint16_t n;
    CfStatus s;

    if (!br_read_u1(r, &type)) {
        return too_short(r, "StackMapTable", err);
    }
    frame->frame_type = type;

    if (type <= 63) {
        frame->offset_delta = type;
        return CF_OK;
    }
    if (type <= 127) {
        frame->offset_delta = (uint16_t)(type - 64);
        frame->number_of_stack_items = 1;
        return parse_verification_list(r, class_file, 1, &frame->stack, err);
    }
    if (type <= 246) {
        return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(r) - 1,
                       "frame_type reservado (%u) no StackMapTable", (unsigned)type);
    }
    if (!br_read_u2(r, &frame->offset_delta)) {
        return too_short(r, "StackMapTable", err);
    }
    if (type == 247) {
        frame->number_of_stack_items = 1;
        return parse_verification_list(r, class_file, 1, &frame->stack, err);
    }
    if (type <= 251) {
        return CF_OK; /* chop ou same_frame_extended */
    }
    if (type <= 254) {
        frame->number_of_locals = (uint16_t)(type - 251);
        return parse_verification_list(r, class_file, frame->number_of_locals,
                                       &frame->locals, err);
    }
    /* 255: full_frame */
    if (!br_read_u2(r, &n)) {
        return too_short(r, "StackMapTable", err);
    }
    s = parse_verification_list(r, class_file, n, &frame->locals, err);
    frame->number_of_locals = (frame->locals != NULL) ? n : 0;
    if (s != CF_OK) {
        return s;
    }
    if (!br_read_u2(r, &n)) {
        return too_short(r, "StackMapTable", err);
    }
    s = parse_verification_list(r, class_file, n, &frame->stack, err);
    frame->number_of_stack_items = (frame->stack != NULL) ? n : 0;
    return s;
}

/**
 * @brief Interpreta o atributo StackMapTable.
 *
 * @details **Algoritmo:** lê `number_of_entries`, aloca os quadros e lê cada
 * um com parse_stack_map_frame().
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_stack_map_table(ByteReader *r, const ClassFile *class_file,
                                      AttributeInfo *attr, ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return too_short(r, "StackMapTable", err);
    }
    attr->info.stack_map_table.entries = alloc_array(n, sizeof(StackMapFrame), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para StackMapTable");
    }
    attr->info.stack_map_table.number_of_entries = n;
    for (i = 0; i < n; i++) {
        CfStatus s = parse_stack_map_frame(r, class_file,
                                           &attr->info.stack_map_table.entries[i], err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

/**
 * @brief Lê um vetor de índices u2 do pool, validando a tag de cada um.
 *
 * @details **Algoritmo:** lê a quantidade, aloca o vetor, grava em @p out e
 * @p count e lê/valida cada índice.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     tag        Tag exigida para cada índice.
 * @param[in]     what       Nome do atributo (para mensagens).
 * @param[out]    count      Quantidade lida.
 * @param[out]    out        Vetor alocado.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_index_list(ByteReader *r, const ClassFile *class_file,
                                 ConstantTag tag, const char *what,
                                 uint16_t *count, uint16_t **out,
                                 ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return too_short(r, what, err);
    }
    *out = alloc_array(n, sizeof(uint16_t), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para %s", what);
    }
    *count = n;
    for (i = 0; i < n; i++) {
        CfStatus s;
        if (!br_read_u2(r, &(*out)[i])) {
            return too_short(r, what, err);
        }
        s = check_index(class_file, r, (*out)[i], tag, false, what, err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

/**
 * @brief Interpreta o atributo InnerClasses.
 *
 * @details **Algoritmo:** lê `number_of_classes` e, para cada entrada, os
 * quatro campos u2, validando: classe interna (Class), classe externa
 * (0 ou Class) e nome (0 ou Utf8).
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_inner_classes(ByteReader *r, const ClassFile *class_file,
                                    AttributeInfo *attr, ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return too_short(r, "InnerClasses", err);
    }
    attr->info.inner_classes.classes = alloc_array(n, sizeof(InnerClassEntry), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para InnerClasses");
    }
    attr->info.inner_classes.number_of_classes = n;
    for (i = 0; i < n; i++) {
        InnerClassEntry *e = &attr->info.inner_classes.classes[i];
        CfStatus s;
        if (!br_read_u2(r, &e->inner_class_info_index) ||
            !br_read_u2(r, &e->outer_class_info_index) ||
            !br_read_u2(r, &e->inner_name_index) ||
            !br_read_u2(r, &e->inner_class_access_flags)) {
            return too_short(r, "InnerClasses", err);
        }
        s = check_index(class_file, r, e->inner_class_info_index, CONSTANT_Class,
                        false, "InnerClasses.inner_class", err);
        if (s == CF_OK) {
            s = check_index(class_file, r, e->outer_class_info_index, CONSTANT_Class,
                            true, "InnerClasses.outer_class", err);
        }
        if (s == CF_OK) {
            s = check_index(class_file, r, e->inner_name_index, CONSTANT_Utf8, true,
                            "InnerClasses.inner_name", err);
        }
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

/**
 * @brief Interpreta o atributo LineNumberTable.
 *
 * @details **Algoritmo:** lê o tamanho, aloca e lê os pares
 * (`start_pc`, `line_number`).
 *
 * @param[in,out] r    Sub-leitor do atributo.
 * @param[out]    attr Atributo a preencher.
 * @param[out]    err  Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_line_numbers(ByteReader *r, AttributeInfo *attr,
                                   ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return too_short(r, "LineNumberTable", err);
    }
    attr->info.line_numbers.line_number_table =
        alloc_array(n, sizeof(LineNumberEntry), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para LineNumberTable");
    }
    attr->info.line_numbers.line_number_table_length = n;
    for (i = 0; i < n; i++) {
        LineNumberEntry *e = &attr->info.line_numbers.line_number_table[i];
        if (!br_read_u2(r, &e->start_pc) || !br_read_u2(r, &e->line_number)) {
            return too_short(r, "LineNumberTable", err);
        }
    }
    return CF_OK;
}

/**
 * @brief Interpreta LocalVariableTable ou LocalVariableTypeTable (mesmo formato).
 *
 * @details **Algoritmo:** lê o tamanho e as entradas de 5 campos u2,
 * validando que nome e descritor/assinatura são Utf8.
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_local_variables(ByteReader *r, const ClassFile *class_file,
                                      AttributeInfo *attr, ClassFileError *err)
{
    uint16_t n, i;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return too_short(r, "LocalVariableTable", err);
    }
    attr->info.local_variables.local_variable_table =
        alloc_array(n, sizeof(LocalVariableEntry), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r),
                       "sem memoria para LocalVariableTable");
    }
    attr->info.local_variables.local_variable_table_length = n;
    for (i = 0; i < n; i++) {
        LocalVariableEntry *e = &attr->info.local_variables.local_variable_table[i];
        CfStatus s;
        if (!br_read_u2(r, &e->start_pc) || !br_read_u2(r, &e->length) ||
            !br_read_u2(r, &e->name_index) || !br_read_u2(r, &e->descriptor_index) ||
            !br_read_u2(r, &e->index)) {
            return too_short(r, "LocalVariableTable", err);
        }
        s = check_index(class_file, r, e->name_index, CONSTANT_Utf8, false,
                        "LocalVariableTable.name", err);
        if (s == CF_OK) {
            s = check_index(class_file, r, e->descriptor_index, CONSTANT_Utf8, false,
                            "LocalVariableTable.descriptor", err);
        }
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

/**
 * @brief Interpreta o atributo BootstrapMethods.
 *
 * @details **Algoritmo:** lê a quantidade de métodos; para cada um, lê e
 * valida `bootstrap_method_ref` (MethodHandle) e a lista de argumentos
 * (cada um deve ser um índice utilizável do pool).
 *
 * @param[in,out] r          Sub-leitor do atributo.
 * @param[in]     class_file Classe.
 * @param[out]    attr       Atributo a preencher.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_bootstrap_methods(ByteReader *r, const ClassFile *class_file,
                                        AttributeInfo *attr, ClassFileError *err)
{
    uint16_t n, i, j;
    bool ok;
    if (!br_read_u2(r, &n)) {
        return too_short(r, "BootstrapMethods", err);
    }
    attr->info.bootstrap.bootstrap_methods = alloc_array(n, sizeof(BootstrapMethod), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para BootstrapMethods");
    }
    attr->info.bootstrap.num_bootstrap_methods = n;
    for (i = 0; i < n; i++) {
        BootstrapMethod *m = &attr->info.bootstrap.bootstrap_methods[i];
        uint16_t argc;
        CfStatus s;
        if (!br_read_u2(r, &m->bootstrap_method_ref) || !br_read_u2(r, &argc)) {
            return too_short(r, "BootstrapMethods", err);
        }
        s = check_index(class_file, r, m->bootstrap_method_ref, CONSTANT_MethodHandle,
                        false, "BootstrapMethods.bootstrap_method_ref", err);
        if (s != CF_OK) {
            return s;
        }
        m->bootstrap_arguments = alloc_array(argc, sizeof(uint16_t), &ok);
        if (!ok) {
            return cf_fail(err, CF_ERR_MEMORY, br_offset(r),
                           "sem memoria para BootstrapMethods");
        }
        m->num_bootstrap_arguments = argc;
        for (j = 0; j < argc; j++) {
            if (!br_read_u2(r, &m->bootstrap_arguments[j])) {
                return too_short(r, "BootstrapMethods", err);
            }
            if (cp_tag(class_file, m->bootstrap_arguments[j]) == CONSTANT_Unusable) {
                return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(r) - 2,
                               "argumento de bootstrap #%u invalido",
                               (unsigned)m->bootstrap_arguments[j]);
            }
        }
    }
    return CF_OK;
}

/**
 * @brief Interpreta o conteúdo de um atributo conforme seu tipo.
 *
 * @details **Algoritmo:** `switch` sobre @c attr->kind, delegando para a
 * função `parse_*` do tipo; atributos simples (um único u2) são lidos aqui;
 * Synthetic e Deprecated não têm conteúdo; desconhecidos são copiados.
 *
 * @param[in,out] r          Sub-leitor limitado a `attribute_length`.
 * @param[in]     class_file Classe.
 * @param[in]     depth      Profundidade de aninhamento.
 * @param[in,out] attr       Atributo com nome, tamanho e tipo já preenchidos.
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_attribute_body(ByteReader *r, const ClassFile *class_file,
                                     unsigned depth, AttributeInfo *attr,
                                     ClassFileError *err)
{
    const uint8_t *bytes;
    switch (attr->kind) {
    case ATTR_CONSTANT_VALUE:
        return parse_constant_value(r, class_file, attr, err);
    case ATTR_CODE:
        return parse_code(r, class_file, depth, attr, err);
    case ATTR_STACK_MAP_TABLE:
        return parse_stack_map_table(r, class_file, attr, err);
    case ATTR_EXCEPTIONS:
        return parse_index_list(r, class_file, CONSTANT_Class, "Exceptions",
                                &attr->info.exceptions.number_of_exceptions,
                                &attr->info.exceptions.exception_index_table, err);
    case ATTR_INNER_CLASSES:
        return parse_inner_classes(r, class_file, attr, err);
    case ATTR_ENCLOSING_METHOD:
        if (!br_read_u2(r, &attr->info.enclosing_method.class_index) ||
            !br_read_u2(r, &attr->info.enclosing_method.method_index)) {
            return too_short(r, "EnclosingMethod", err);
        }
        if (check_index(class_file, r, attr->info.enclosing_method.class_index,
                        CONSTANT_Class, false, "EnclosingMethod.class", err) != CF_OK) {
            return CF_ERR_BAD_INDEX;
        }
        return check_index(class_file, r, attr->info.enclosing_method.method_index,
                           CONSTANT_NameAndType, true, "EnclosingMethod.method", err);
    case ATTR_SIGNATURE:
        if (!br_read_u2(r, &attr->info.signature_index)) {
            return too_short(r, "Signature", err);
        }
        return check_index(class_file, r, attr->info.signature_index, CONSTANT_Utf8,
                           false, "Signature", err);
    case ATTR_SOURCE_FILE:
        if (!br_read_u2(r, &attr->info.sourcefile_index)) {
            return too_short(r, "SourceFile", err);
        }
        return check_index(class_file, r, attr->info.sourcefile_index, CONSTANT_Utf8,
                           false, "SourceFile", err);
    case ATTR_LINE_NUMBER_TABLE:
        return parse_line_numbers(r, attr, err);
    case ATTR_LOCAL_VARIABLE_TABLE:
    case ATTR_LOCAL_VARIABLE_TYPE_TABLE:
        return parse_local_variables(r, class_file, attr, err);
    case ATTR_BOOTSTRAP_METHODS:
        return parse_bootstrap_methods(r, class_file, attr, err);
    case ATTR_SYNTHETIC:
    case ATTR_DEPRECATED:
        return CF_OK;
    case ATTR_UNKNOWN:
        break;
    }
    /* Desconhecido: copia os bytes brutos. */
    if (attr->attribute_length > 0) {
        if (!br_read_bytes(r, attr->attribute_length, &bytes)) {
            return too_short(r, "desconhecido", err);
        }
        attr->info.raw = malloc(attr->attribute_length);
        if (attr->info.raw == NULL) {
            return cf_fail(err, CF_ERR_MEMORY, br_offset(r), "sem memoria para atributo");
        }
        memcpy(attr->info.raw, bytes, attr->attribute_length);
    }
    return CF_OK;
}

/**
 * @brief Lê um atributo completo (cabeçalho + conteúdo).
 *
 * @details
 * **Algoritmo:**
 * 1. Lê `attribute_name_index` (Utf8) e `attribute_length`;
 * 2. Cria um sub-leitor com `attribute_length` bytes (erro de truncamento se
 *    o arquivo for menor);
 * 3. Identifica o tipo pelo nome (Code aninhado vira desconhecido);
 * 4. Interpreta o conteúdo e exige que todos os bytes tenham sido consumidos.
 *
 * @param[in,out] reader     Leitor posicionado no início do atributo.
 * @param[in]     class_file Classe.
 * @param[in]     depth      Profundidade de aninhamento.
 * @param[out]    attr       Atributo a preencher (zerado).
 * @param[out]    err        Detalhes do erro (pode ser NULL).
 * @return #CF_OK ou código de erro.
 */
static CfStatus parse_attribute(ByteReader *reader, const ClassFile *class_file,
                                unsigned depth, AttributeInfo *attr,
                                ClassFileError *err)
{
    ByteReader sub;
    const char *name;
    CfStatus s;

    if (!br_read_u2(reader, &attr->attribute_name_index) ||
        !br_read_u4(reader, &attr->attribute_length)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "cabecalho de atributo truncado");
    }
    name = cp_get_utf8(class_file, attr->attribute_name_index);
    if (name == NULL) {
        return cf_fail(err, CF_ERR_BAD_INDEX, br_offset(reader) - 6,
                       "attribute_name_index #%u nao e Utf8",
                       (unsigned)attr->attribute_name_index);
    }
    if (!br_sub_reader(reader, attr->attribute_length, &sub)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "atributo %s truncado (%lu bytes declarados, %lu disponiveis)",
                       name, (unsigned long)attr->attribute_length,
                       (unsigned long)br_remaining(reader));
    }
    attr->kind = attribute_kind_from_name(name);
    if (attr->kind == ATTR_CODE && depth > 0) {
        attr->kind = ATTR_UNKNOWN; /* Code dentro de Code não é válido */
    }
    s = parse_attribute_body(&sub, class_file, depth, attr, err);
    if (s != CF_OK) {
        return s;
    }
    if (br_remaining(&sub) != 0) {
        return cf_fail(err, CF_ERR_BAD_ATTRIBUTE, br_offset(&sub),
                       "atributo %s tem %lu byte(s) alem do conteudo", name,
                       (unsigned long)br_remaining(&sub));
    }
    return CF_OK;
}

CfStatus attributes_parse(ByteReader *reader, const ClassFile *class_file,
                          unsigned depth, uint16_t *count,
                          AttributeInfo **attributes, ClassFileError *err)
{
    uint16_t n, i;
    bool ok;

    *count = 0;
    *attributes = NULL;
    if (!br_read_u2(reader, &n)) {
        return cf_fail(err, CF_ERR_TRUNCATED, br_offset(reader),
                       "attributes_count ausente");
    }
    *attributes = alloc_array(n, sizeof(AttributeInfo), &ok);
    if (!ok) {
        return cf_fail(err, CF_ERR_MEMORY, br_offset(reader),
                       "sem memoria para atributos");
    }
    *count = n;
    for (i = 0; i < n; i++) {
        CfStatus s = parse_attribute(reader, class_file, depth, &(*attributes)[i], err);
        if (s != CF_OK) {
            return s;
        }
    }
    return CF_OK;
}

/**
 * @brief Libera o conteúdo de um atributo Code.
 *
 * @details **Algoritmo:** libera o bytecode, a tabela de exceções, os
 * atributos aninhados (recursivamente) e a estrutura.
 *
 * @param[in] code Atributo Code (pode ser NULL).
 */
static void code_free(CodeAttribute *code)
{
    if (code == NULL) {
        return;
    }
    free(code->code);
    free(code->exception_table);
    attributes_free(code->attributes, code->attributes_count);
    free(code);
}

void attributes_free(AttributeInfo *attributes, uint16_t count)
{
    uint16_t i, j;
    if (attributes == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        AttributeInfo *a = &attributes[i];
        switch (a->kind) {
        case ATTR_CODE:
            code_free(a->info.code);
            break;
        case ATTR_STACK_MAP_TABLE:
            for (j = 0; j < a->info.stack_map_table.number_of_entries; j++) {
                free(a->info.stack_map_table.entries[j].locals);
                free(a->info.stack_map_table.entries[j].stack);
            }
            free(a->info.stack_map_table.entries);
            break;
        case ATTR_EXCEPTIONS:
            free(a->info.exceptions.exception_index_table);
            break;
        case ATTR_INNER_CLASSES:
            free(a->info.inner_classes.classes);
            break;
        case ATTR_LINE_NUMBER_TABLE:
            free(a->info.line_numbers.line_number_table);
            break;
        case ATTR_LOCAL_VARIABLE_TABLE:
        case ATTR_LOCAL_VARIABLE_TYPE_TABLE:
            free(a->info.local_variables.local_variable_table);
            break;
        case ATTR_BOOTSTRAP_METHODS:
            for (j = 0; j < a->info.bootstrap.num_bootstrap_methods; j++) {
                free(a->info.bootstrap.bootstrap_methods[j].bootstrap_arguments);
            }
            free(a->info.bootstrap.bootstrap_methods);
            break;
        case ATTR_UNKNOWN:
            free(a->info.raw);
            break;
        default:
            break; /* tipos sem memória própria */
        }
    }
    free(attributes);
}
