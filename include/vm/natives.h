/**
 * @file natives.h
 * @brief Classes e métodos nativos da biblioteca simulada em C (D7, D23):
 *        `Object`, `String`, `StringBuilder`/`StringBuffer`, `System.out`/
 *        `PrintStream`, `Throwable` e as exceções padrão.
 *
 * @details Nenhum `.class` da biblioteca é lido (R2). As classes nativas são
 * linhas de uma tabela `{nome, superclasse}` e os métodos, linhas de uma
 * tabela `{classe, método, descritor, função}` (natives.c).
 */
#ifndef JVM_NATIVES_H
#define JVM_NATIVES_H

#include "vm/runtime.h"

/** @brief Nome do campo de mensagem de `Throwable` (D23). */
#define THROWABLE_MESSAGE_FIELD "detailMessage"

/**
 * @brief Indica se um nome é de classe nativa e devolve a superclasse.
 *
 * @details **Algoritmo:** busca linear na tabela de classes nativas.
 *
 * @param[in]  name       Nome interno.
 * @param[out] super_name Nome da superclasse (NULL para `java/lang/Object`).
 * @return `true` se a classe é nativa.
 */
bool natives_class_super(const char *name, const char **super_name);

/**
 * @brief Prepara uma classe nativa recém-criada.
 *
 * @details **Algoritmo:** classes de texto recebem #OBJECT_TEXT;
 * `Throwable` recebe o campo de mensagem; `System` recebe o estático `out`
 * com um objeto `PrintStream`.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in,out] class_ref Classe nativa.
 * @return `true` em caso de sucesso.
 */
bool natives_setup_class(Vm *vm, RuntimeClass *class_ref);

/**
 * @brief Procura um método nativo.
 *
 * @details **Algoritmo:** busca linear por classe, nome e descritor.
 *
 * @param[in]  class_name Classe nativa.
 * @param[in]  name       Nome do método.
 * @param[in]  descriptor Descritor.
 * @param[out] is_static  Se o método é estático (só escrito quando encontrado).
 * @return Função; NULL se não existe.
 */
NativeMethod natives_find(const char *class_name, const char *name,
                          const char *descriptor, bool *is_static);

/**
 * @brief Converte um valor qualquer para `String` (como `String.valueOf`).
 *
 * @details
 * **Algoritmo:** conforme o tipo: primitivos são formatados como no Java
 * (`float`/`double` com number_format.c); `char` vira um caractere;
 * `boolean` vira `true`/`false`; `null` vira "null"; `char[]` vira o seu
 * texto; uma `String` é devolvida como está; os demais objetos chamam o
 * método virtual `toString()`.
 *
 * @param[in,out] vm    Máquina virtual.
 * @param[in]     type  Descritor do tipo do valor (ex.: `I`, `[C`, `Ljava/lang/Object;`).
 * @param[in]     value Valor (2 slots para `long`/`double`).
 * @return Objeto `String`; NULL com exceção pendente ou erro fatal.
 */
Object *natives_to_string(Vm *vm, const char *type, const Slot *value);

/**
 * @brief Devolve o texto de um objeto de texto, com verificação.
 *
 * @details **Algoritmo:** se o objeto não for #OBJECT_TEXT, erro fatal.
 *
 * @param[in,out] vm     Máquina virtual.
 * @param[in]     object Objeto (não NULL).
 * @return Texto; NULL em caso de erro fatal.
 */
Text *natives_text(Vm *vm, Object *object);

/**
 * @brief Acrescenta unidades UTF-16 ao fim de um texto.
 *
 * @details **Algoritmo:** realoca o vetor com o novo tamanho (sem
 * capacidade extra, D17) e copia as unidades.
 *
 * @param[in,out] vm     Máquina virtual.
 * @param[in,out] text   Texto de destino.
 * @param[in]     chars  Unidades a acrescentar.
 * @param[in]     length Quantidade de unidades.
 */
void natives_text_append(Vm *vm, Text *text, const uint16_t *chars, int32_t length);

/**
 * @brief Escreve um texto UTF-16 em UTF-8.
 *
 * @details **Algoritmo:** junta pares de *surrogates* e codifica cada
 * ponto de código em 1 a 4 bytes UTF-8.
 *
 * @param[in] out    Arquivo de saída.
 * @param[in] chars  Unidades UTF-16.
 * @param[in] length Quantidade de unidades.
 */
void natives_write_utf16(FILE *out, const uint16_t *chars, int32_t length);

/**
 * @brief Devolve o campo de mensagem (`detailMessage`) de um `Throwable`.
 *
 * @details **Algoritmo:** procura no objeto o campo copiado do modelo de
 * `java/lang/Throwable` (mesmo ponteiro de nome, ver object_field()).
 *
 * @param[in] exception Objeto que herda de `Throwable`.
 * @return Campo; NULL se o objeto não tem o campo.
 */
Field *natives_message_field(Object *exception);

/** @brief Linha da tabela de métodos nativos `{classe, método, descritor, função}` (D7). */
typedef struct NativeEntry {
    const char *class_name; /**< Classe nativa. */
    const char *name;       /**< Nome do método. */
    const char *descriptor; /**< Descritor do método. */
    NativeMethod function;  /**< Implementação em C. */
    bool is_static;         /**< Método estático (omitido = `false`, de instância). */
} NativeEntry;

/** @brief Nativos de `Object` e `Throwable` (native_object.c), terminada por `{NULL}`. */
extern const NativeEntry native_object_methods[];
/** @brief Nativos de `String`, `StringBuilder` e `StringBuffer` (native_string.c), terminada por `{NULL}`. */
extern const NativeEntry native_string_methods[];
/** @brief `valueOf(x)` e `append(x)` para todos os tipos (native_text.c), terminada por `{NULL}`. */
extern const NativeEntry native_text_methods[];
/** @brief Nativos de `PrintStream` (native_print.c), terminada por `{NULL}`. */
extern const NativeEntry native_print_methods[];

#endif /* JVM_NATIVES_H */
