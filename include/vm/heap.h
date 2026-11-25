/**
 * @file heap.h
 * @brief Heap da VM: criação de objetos, arrays e strings; lista de
 *        alocações liberada no fim da execução (D8, sem GC).
 *
 * @details Toda alocação passa por heap_alloc(), que soma os bytes e recusa
 * passar de #HEAP_LIMIT (erro fatal "sem memória"), evitando que um
 * programa (ou o fuzzer) esgote a memória da máquina.
 */
#ifndef JVM_HEAP_H
#define JVM_HEAP_H

#include "vm/runtime.h"

/** @brief Limite total de bytes alocados no heap (256 MiB). */
#define HEAP_LIMIT (256u * 1024u * 1024u)

/**
 * @brief Aloca memória zerada contabilizada no heap.
 *
 * @details **Algoritmo:** recusa (erro fatal) se `vm->heap_bytes + size`
 * passa de #HEAP_LIMIT; senão chama `calloc` e soma o tamanho.
 *
 * @param[in,out] vm   Máquina virtual.
 * @param[in]     size Bytes.
 * @return Memória zerada (o dono a libera); NULL em caso de erro fatal.
 */
void *heap_alloc(Vm *vm, size_t size);

/**
 * @brief Cria uma instância de uma classe com os campos zerados (D21).
 *
 * @details
 * **Algoritmo:**
 * 1. Classes de texto (`String`, `StringBuilder`, `StringBuffer`): objeto
 *    #OBJECT_TEXT vazio;
 * 2. Demais: conta os campos declarados na classe e nas superclasses,
 *    aloca o vetor e copia os modelos, da raiz para a classe concreta;
 * 3. Insere o objeto na lista do heap.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in]     class_ref Classe.
 * @return Objeto; NULL em caso de erro fatal.
 */
Object *heap_new_object(Vm *vm, RuntimeClass *class_ref);

/**
 * @brief Cria um array com elementos zerados.
 *
 * @details **Algoritmo:** o tamanho do elemento vem do tipo após `[`
 * (`Z`/`B` = 1, `C`/`S` = 2, `I`/`F`/referências = 4, `J`/`D` = 8); copia o
 * descritor e aloca os dados contíguos.
 *
 * @param[in,out] vm     Máquina virtual.
 * @param[in]     type   Descritor do array (ex.: `[I`, `[[D`, `[Ljava/lang/String;`).
 * @param[in]     length Quantidade de elementos (>= 0).
 * @return Objeto array; NULL em caso de erro fatal.
 */
Object *heap_new_array(Vm *vm, const char *type, int32_t length);

/**
 * @brief Cria uma `String` com uma cópia das unidades UTF-16 (D22).
 *
 * @details **Algoritmo:** heap_new_object() da classe `java/lang/String` e
 * cópia de @p length unidades.
 *
 * @param[in,out] vm     Máquina virtual.
 * @param[in]     chars  Unidades UTF-16 (pode ser NULL se @p length = 0).
 * @param[in]     length Quantidade de unidades.
 * @return Objeto `String`; NULL em caso de erro fatal.
 */
Object *heap_new_string(Vm *vm, const uint16_t *chars, int32_t length);

/**
 * @brief Cria uma `String` a partir de texto MUTF-8/ASCII.
 *
 * @details **Algoritmo:** decodifica com mutf8_to_utf16() e chama
 * heap_new_string().
 *
 * @param[in,out] vm   Máquina virtual.
 * @param[in]     text Texto terminado em '\\0'.
 * @return Objeto `String`; NULL em caso de erro fatal.
 */
Object *heap_new_string_utf8(Vm *vm, const char *text);

/**
 * @brief Devolve o objeto `String` internado de um literal (`ldc`).
 *
 * @details **Algoritmo:** cria a `String`; procura na lista de internados
 * uma com o mesmo conteúdo (busca linear); se achar devolve a existente
 * (a nova fica no heap e é liberada no fim), senão acrescenta a nova.
 *
 * @param[in,out] vm   Máquina virtual.
 * @param[in]     text Literal MUTF-8 do pool.
 * @return Objeto `String` internado; NULL em caso de erro fatal.
 */
Object *heap_intern(Vm *vm, const char *text);

/**
 * @brief Procura em um objeto o campo correspondente a uma declaração.
 *
 * @details **Algoritmo:** os campos do objeto foram copiados dos modelos
 * da classe; o campo certo é o que tem o mesmo ponteiro de nome do modelo
 * @p decl (isso diferencia campos homônimos de superclasse e subclasse).
 *
 * @param[in] object Instância (#OBJECT_INSTANCE).
 * @param[in] decl   Modelo do campo (de RuntimeClass::fields).
 * @return Campo do objeto ou NULL.
 */
Field *object_field(Object *object, const Field *decl);

/**
 * @brief Libera todos os objetos do heap (D8).
 *
 * @details **Algoritmo:** percorre a lista liberando campos, dados de
 * arrays, textos e o próprio objeto.
 *
 * @param[in,out] vm Máquina virtual.
 */
void heap_free_all(Vm *vm);

#endif /* JVM_HEAP_H */
