/**
 * @file interpreter.h
 * @brief Máquina virtual e interpretador: estado da VM, erros fatais,
 *        exceções, laço *fetch–decode–execute* (D18) e chamada de métodos.
 *
 * @details Fluxo: vm_run_main() → class_load() → class_initialize() →
 * interpreter_call() → laço com a tabela `handlers[256]`. As instruções
 * `invoke*` chamam interpreter_call() recursivamente (um frame em C por
 * frame Java); uma exceção não tratada faz interpreter_call() retornar
 * `false` e o chamador procura o seu tratador.
 */
#ifndef JVM_INTERPRETER_H
#define JVM_INTERPRETER_H

#include "vm/runtime.h"

/** @brief Profundidade máxima de frames (acima disso: `StackOverflowError`). */
#define VM_MAX_DEPTH 1024u

/** @brief Código de saída: exceção não capturada. */
#define EXIT_CODE_UNCAUGHT 4
/** @brief Código de saída: erro fatal da VM (instrução inválida, sem suporte...). */
#define EXIT_CODE_VM_ERROR 5

/**
 * @brief Inicializa o estado da VM.
 *
 * @details **Algoritmo:** zera a estrutura e guarda o *classpath* e a saída.
 *
 * @param[out] vm        Estrutura a inicializar.
 * @param[in]  classpath Diretório dos `.class` (NULL = diretório atual).
 * @param[in]  out       Saída de `System.out`.
 */
void vm_init(Vm *vm, const char *classpath, FILE *out);

/**
 * @brief Libera tudo o que a VM alocou (heap e classes, D8).
 *
 * @details **Algoritmo:** heap_free_all(), class_free_all() e o vetor de
 * literais internados.
 *
 * @param[in,out] vm Máquina virtual.
 */
void vm_free(Vm *vm);

/**
 * @brief Registra um erro fatal: imprime a mensagem e para a execução.
 *
 * @details **Algoritmo:** só a primeira mensagem é impressa (em `stderr`,
 * prefixada por "Erro: "; nada é impresso se `vm->quiet`); marca `vm->fatal` e o código
 * #EXIT_CODE_VM_ERROR (se nenhum outro foi definido).
 *
 * @param[in,out] vm     Máquina virtual.
 * @param[in]     format Formato no estilo `printf`.
 * @param[in]     ...    Argumentos do formato.
 */
void vm_fatal(Vm *vm, const char *format, ...);

/**
 * @brief Lança uma exceção já criada.
 *
 * @details **Algoritmo:** guarda o objeto em `vm->exception`; o laço do
 * interpretador procura o tratador depois da instrução atual.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in]     exception Objeto da exceção (não NULL).
 */
void vm_throw(Vm *vm, Object *exception);

/**
 * @brief Cria e lança uma exceção nativa (D23) com mensagem opcional.
 *
 * @details **Algoritmo:** carrega a classe, cria o objeto, grava a
 * mensagem no campo de `Throwable` e chama vm_throw().
 *
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     class_name Nome interno (ex.: `java/lang/ArithmeticException`).
 * @param[in]     message    Mensagem (ASCII) ou NULL.
 */
void vm_throw_new(Vm *vm, const char *class_name, const char *message);

/**
 * @brief Chama o `main` de uma classe já carregada.
 *
 * @details **Algoritmo:** procura `main([Ljava/lang/String;)V` estático
 * declarado na classe, inicializa a classe (`<clinit>`) e chama o método
 * com um `String[]` vazio. Erros ficam em `vm->fatal`/`vm->exception`.
 *
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     main_class Classe principal.
 */
void vm_call_main(Vm *vm, RuntimeClass *main_class);

/**
 * @brief Executa `public static void main(String[])` de uma classe.
 *
 * @details
 * **Algoritmo:**
 * 1. Inicializa a VM e carrega a classe (com superclasses);
 * 2. Localiza `main([Ljava/lang/String;)V` estático;
 * 3. Inicializa a classe (`<clinit>`) e chama `main` com um `String[0]`;
 * 4. Exceção não capturada: imprime `Exception in thread "main" ...` em
 *    `stderr`;
 * 5. Libera tudo (D8) e devolve o código de saída.
 *
 * @param[in] classpath  Diretório dos `.class` (NULL = diretório atual).
 * @param[in] class_name Classe principal (ex.: `Exemplo` ou `pkg.Exemplo`).
 * @return 0, #EXIT_CODE_UNCAUGHT, #EXIT_CODE_VM_ERROR ou 3 (erro no `.class`).
 */
int vm_run_main(const char *classpath, const char *class_name);

/**
 * @brief Chama um método (do `.class` ou nativo) com os argumentos dados.
 *
 * @details
 * **Algoritmo:**
 * 1. Nativo: chama a função C e retorna;
 * 2. Método abstrato ou sem `Code`: erro fatal;
 * 3. Cria o frame, copia @p arg_count slots (e etiquetas) para as locais;
 * 4. Executa o laço até um `*return` ou até uma exceção não tratada;
 * 5. Copia o valor de retorno para @p result e libera o frame.
 *
 * @param[in,out] vm        Máquina virtual.
 * @param[in]     ref       Método resolvido.
 * @param[in]     descriptor Descritor do método.
 * @param[in]     args      Argumentos (`this` primeiro, se houver).
 * @param[in]     tags      Etiquetas dos argumentos.
 * @param[in]     arg_count Quantidade de slots de argumentos.
 * @param[out]    result    Valor de retorno (2 slots).
 * @return `true` se terminou normalmente; `false` com exceção pendente ou erro fatal.
 */
bool interpreter_call(Vm *vm, const MethodRef *ref, const char *descriptor,
                      const Slot *args, const uint8_t *tags, uint16_t arg_count,
                      Slot *result);

/**
 * @brief Chama um método virtual sem argumentos além de `this` (usado
 *        pelos nativos: `toString()`, `hashCode()`).
 *
 * @details **Algoritmo:** procura o método a partir da classe real do
 * objeto (arrays usam `java/lang/Object`) e chama interpreter_call().
 *
 * @param[in,out] vm         Máquina virtual.
 * @param[in]     object     Objeto (não NULL).
 * @param[in]     name       Nome do método.
 * @param[in]     descriptor Descritor (ex.: `()Ljava/lang/String;`).
 * @param[out]    result     Valor de retorno.
 * @return `true` se terminou normalmente.
 */
bool interpreter_call_virtual(Vm *vm, Object *object, const char *name,
                              const char *descriptor, Slot *result);

/**
 * @brief Executa uma única instrução do frame (usado pelo laço e pelos testes).
 *
 * @details
 * **Algoritmo:**
 * 1. Valida o tamanho da instrução em `pc` (bytecode_instruction_length());
 * 2. Define `next_pc`, chama `handlers[opcode]` e avança `pc`;
 * 3. Com exceção pendente, procura o tratador na `exception_table`: se
 *    achar, limpa a pilha, empilha a exceção e desvia para o tratador.
 *
 * @param[in,out] frame Frame em execução.
 * @return `true` se a execução do frame pode continuar; `false` com
 *         exceção não tratada neste frame ou erro fatal.
 */
bool interpreter_step(Frame *frame);

/**
 * @brief Preenche as posições da tabela de despacho de constantes,
 *        loads, stores e `wide` (exec_load_store.c).
 * @details **Algoritmo:** atribui cada `op_*` ao seu opcode.
 * @param[out] table Tabela `handlers[256]`.
 */
void exec_load_store_register(OpcodeHandler *table);

/**
 * @brief Preenche a tabela com as instruções de pilha (exec_stack.c).
 * @details **Algoritmo:** atribui cada `op_*` ao seu opcode.
 * @param[out] table Tabela `handlers[256]`.
 */
void exec_stack_register(OpcodeHandler *table);

/**
 * @brief Preenche a tabela com a aritmética de `int`/`long` (exec_int.c).
 * @details **Algoritmo:** atribui cada `op_*` ao seu opcode.
 * @param[out] table Tabela `handlers[256]`.
 */
void exec_int_register(OpcodeHandler *table);

/**
 * @brief Preenche a tabela com a aritmética de ponto flutuante, conversões
 *        e comparações (exec_float.c).
 * @details **Algoritmo:** atribui cada `op_*` ao seu opcode.
 * @param[out] table Tabela `handlers[256]`.
 */
void exec_float_register(OpcodeHandler *table);

/**
 * @brief Preenche a tabela com desvios, `switch`, `jsr`/`ret` e retornos
 *        (exec_branch.c).
 * @details **Algoritmo:** atribui cada `op_*` ao seu opcode.
 * @param[out] table Tabela `handlers[256]`.
 */
void exec_branch_register(OpcodeHandler *table);

/**
 * @brief Preenche a tabela com objetos, campos, tipos, `athrow` e
 *        monitores (exec_object.c).
 * @details **Algoritmo:** atribui cada `op_*` ao seu opcode.
 * @param[out] table Tabela `handlers[256]`.
 */
void exec_object_register(OpcodeHandler *table);

/**
 * @brief Preenche a tabela com as instruções `invoke*` (exec_invoke.c).
 * @details **Algoritmo:** atribui cada `op_*` ao seu opcode.
 * @param[out] table Tabela `handlers[256]`.
 */
void exec_invoke_register(OpcodeHandler *table);

/**
 * @brief Preenche a tabela com as instruções de arrays (exec_array.c).
 * @details **Algoritmo:** atribui cada `op_*` ao seu opcode.
 * @param[out] table Tabela `handlers[256]`.
 */
void exec_array_register(OpcodeHandler *table);

/**
 * @brief Devolve a função que executa um opcode (usada pelos testes, I-10).
 * @details **Algoritmo:** monta a tabela na primeira chamada e a indexa.
 * @param[in] opcode Opcode.
 * @return Função da tabela (`op_unimplemented` se não houver instrução).
 */
OpcodeHandler interpreter_handler(uint8_t opcode);

/**
 * @brief Indica se uma função da tabela é a de "opcode não implementado".
 * @details **Algoritmo:** compara com o endereço de `op_unimplemented`.
 * @param[in] handler Função da tabela.
 * @return `true` se @p handler é `op_unimplemented`.
 */
bool interpreter_is_unimplemented(OpcodeHandler handler);

#endif /* JVM_INTERPRETER_H */
