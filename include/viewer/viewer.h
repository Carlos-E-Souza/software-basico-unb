/**
 * @file viewer.h
 * @brief Exibidor: escreve todas as estruturas de um `.class` lido.
 */
#ifndef JVM_VIEWER_H
#define JVM_VIEWER_H

#include <stdio.h>

#include "classfile/class_file.h"

/**
 * @brief Escreve, em formato legível, todas as estruturas de uma classe.
 *
 * @details
 * Seções escritas, na ordem do arquivo:
 * 1. **Informações gerais:** magic, versões (com o nome da versão do Java),
 *    tamanho do pool, flags de acesso decodificadas, classe, superclasse e
 *    contagens;
 * 2. **Pool de constantes:** cada entrada com índice, tipo, referências
 *    brutas e o valor resolvido;
 * 3. **Interfaces**;
 * 4. **Campos** e **métodos**: nome, descritor, flags e atributos; o
 *    atributo Code inclui o bytecode desmontado (disassemble_code()), a
 *    tabela de exceções e os atributos aninhados;
 * 5. **Atributos da classe**.
 *
 * **Algoritmo:** chama, em sequência, as funções de exibição de cada seção.
 *
 * @param[in] out         Fluxo de saída (terminal ou arquivo).
 * @param[in] class_file  Classe lida por class_file_load().
 * @param[in] source_name Caminho do arquivo, exibido no cabeçalho (pode ser NULL).
 */
void viewer_print_class(FILE *out, const ClassFile *class_file,
                        const char *source_name);

#endif /* JVM_VIEWER_H */
