/**
 * @file class_loader.h
 * @brief Carregador de classes (do usuário e nativas), inicialização
 *        (`<clinit>`) e consultas à hierarquia (I-02, I-06, D19).
 *
 * @details As classes ficam em uma lista ligada; toda busca é linear por
 * nome, sem *cache* nem *vtable* (D19). Métodos e campos são procurados
 * subindo a hierarquia a cada uso.
 */
#ifndef JVM_CLASS_LOADER_H
#define JVM_CLASS_LOADER_H

#include "vm/runtime.h"

/** @brief Código de saída: classe não encontrada ou `.class` inválido. */
#define EXIT_CODE_CLASS_LOAD 3

/**
 * @brief Devolve uma classe carregada, carregando-a se necessário.
 *
 * @details
 * **Algoritmo:**
 * 1. Procura o nome na lista de classes carregadas;
 * 2. Se for nativa (natives_class_super()), cria a classe sem `.class` e
 *    a prepara com natives_setup_class();
 * 3. Senão lê `<classpath>/<nome>.class` e confere o nome de `this_class`;
 * 4. Carrega a superclasse (recursivamente) e monta os modelos dos campos
 *    de instância e os campos estáticos (com `ConstantValue`);
 * 5. Insere a classe na lista. Qualquer falha é erro fatal.
 *
 * @param[in,out] vm   Máquina virtual.
 * @param[in]     name Nome interno (ex.: `pkg/Classe`).
 * @return Classe; NULL em caso de erro fatal.
 */
RuntimeClass *class_load(Vm *vm, const char *name);

/**
 * @brief Registra uma classe do usuário já lida (usado pelo fuzzing).
 *
 * @details **Algoritmo:** obtém o nome de `this_class` (não pode ser de
 * classe nativa nem já carregada), carrega a superclasse, monta os campos
 * e insere a classe na lista. A VM passa a ser dona de @p class_file
 * (liberado também em caso de erro).
 *
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     class_file Classe lida por class_file_parse().
 * @return Classe; NULL em caso de erro fatal.
 */
RuntimeClass *class_define(Vm *vm, ClassFile *class_file);

/**
 * @brief Inicializa a classe na primeira utilização ativa (JVMS §5.5).
 *
 * @details **Algoritmo:** se a classe já foi (ou está sendo) inicializada,
 * não faz nada; senão marca "inicializando", inicializa a superclasse e
 * executa `<clinit>`, se existir.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in]     class_ref Classe.
 * @return `true` em caso de sucesso; `false` com exceção ou erro fatal.
 */
bool class_initialize(Vm *vm, RuntimeClass *class_ref);

/**
 * @brief Libera todas as classes carregadas.
 *
 * @details **Algoritmo:** percorre a lista liberando `ClassFile`, nome,
 * modelos de campos e estáticos.
 *
 * @param[in,out] vm Máquina virtual.
 */
void class_free_all(Vm *vm);

/**
 * @brief Procura um método subindo a hierarquia (D19).
 *
 * @details **Algoritmo:** para cada classe, da inicial até `Object`:
 * procura nome + descritor nos métodos do `.class` ou, se a classe for
 * nativa, na tabela de nativos (natives_find()).
 *
 * @param[in]  class_ref  Classe inicial.
 * @param[in]  name       Nome do método.
 * @param[in]  descriptor Descritor.
 * @param[out] out        Método encontrado.
 * @return `true` se encontrou.
 */
bool class_find_method(RuntimeClass *class_ref, const char *name,
                       const char *descriptor, MethodRef *out);

/**
 * @brief Procura a declaração de um campo de instância na hierarquia.
 *
 * @details **Algoritmo:** sobe a partir de @p class_ref comparando nome e
 * descritor com os modelos de cada classe.
 *
 * @param[in] class_ref  Classe inicial.
 * @param[in] name       Nome do campo.
 * @param[in] descriptor Descritor.
 * @return Modelo do campo; NULL se não existe.
 */
const Field *class_find_field(const RuntimeClass *class_ref, const char *name,
                              const char *descriptor);

/**
 * @brief Procura um campo estático na classe, nas superinterfaces e nas
 *        superclasses (JVMS §5.4.3.2).
 *
 * @details **Algoritmo:** compara nome e descritor com os estáticos da
 * classe; depois procura nas interfaces (carregando-as) e na superclasse.
 *
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     class_ref  Classe inicial.
 * @param[in]     name       Nome do campo.
 * @param[in]     descriptor Descritor.
 * @param[out]    owner      Classe que declara o campo.
 * @return Campo; NULL se não existe.
 */
Field *class_find_static(Vm *vm, RuntimeClass *class_ref, const char *name,
                         const char *descriptor, RuntimeClass **owner);

/**
 * @brief Indica se uma classe é @p target, subclasse de @p target ou
 *        implementa a interface @p target (direta ou indiretamente).
 *
 * @details **Algoritmo:** sobe a hierarquia comparando nomes; em cada
 * classe com `.class`, verifica recursivamente as interfaces declaradas.
 *
 * @param[in,out] vm        Máquina virtual (carrega as interfaces).
 * @param[in]     class_ref Classe.
 * @param[in]     target    Nome interno da classe ou interface alvo.
 * @return `true` se a atribuição é válida.
 */
bool class_is_assignable(Vm *vm, const RuntimeClass *class_ref, const char *target);

/**
 * @brief Indica se um objeto é instância de um tipo (`instanceof`,
 *        `checkcast`, `catch`), incluindo arrays (JVMS §6.5 checkcast).
 *
 * @details **Algoritmo:** objetos usam class_is_assignable(); arrays são
 * atribuíveis a `java/lang/Object` e a arrays de mesmo tipo primitivo ou
 * de elementos atribuíveis.
 *
 * @param[in,out] vm     Máquina virtual.
 * @param[in]     object Objeto (não NULL).
 * @param[in]     target Nome interno ou descritor de array.
 * @return `true` se o objeto é instância de @p target.
 */
bool object_is_instance(Vm *vm, const Object *object, const char *target);

/**
 * @brief Devolve o nome do tipo de um objeto (classe ou descritor do array).
 * @details **Algoritmo:** arrays devolvem o descritor; os demais, o nome da classe.
 * @param[in] object Objeto (não NULL).
 * @return Nome interno.
 */
const char *object_type_name(const Object *object);

#endif /* JVM_CLASS_LOADER_H */
