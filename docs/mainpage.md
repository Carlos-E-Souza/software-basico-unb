# JVM Didática {#mainpage}

Máquina Virtual Java didática escrita em **C99**, desenvolvida para a disciplina
de **Software Básico (UnB)**.

## Modos de operação

- **Leitor/Exibidor:** lê um arquivo `.class` e exibe as estruturas montadas
  (pool de constantes, campos, métodos, atributos) e o bytecode com os
  mnemônicos.
- **Interpretador:** executa programas Java compilados com `javac` para
  Java 2 (1.2), Java 6 ou Java 8.

## Uso

```
jvm --mode viewer      --classpath <dir> <Classe|arquivo.class> [--output <arquivo>]
jvm --mode interpreter --classpath <dir> <ClassePrincipal>
```

## Como navegar nesta documentação

- **Arquivos:** cada arquivo-fonte com suas funções, grafos de inclusão e
  código navegável.
- **Funções:** cada função tem página própria com objetivo, parâmetros de
  entrada e de saída, descrição do algoritmo, links para as funções chamadas
  (*Referências*) e chamadoras (*Referenciado por*), além dos grafos de
  chamada e de chamadoras (diagrama de comunicação entre funções).
- **Estruturas de dados:** veja a página @ref estruturas.
- **Organização do código:** veja a seção abaixo.

## Organização do código

Cada módulo fica em um diretório de `src/` com os headers correspondentes em
`include/`. Os arquivos são mantidos com **cerca de 250 linhas no máximo**;
quando um arquivo cresce além disso, ele é dividido por responsabilidade e as
funções compartilhadas entre as partes são declaradas em um header interno
(que não faz parte da interface pública do módulo).

| Módulo | Arquivos | Responsabilidade |
|--------|----------|------------------|
| `common` | `file_io`, `mutf8`, `number_format`, `version` | Utilitários: leitura de arquivos, UTF-8 modificado, formatação de números no estilo do Java. |
| `cli` | `cli` | Argumentos de linha de comando. |
| `classfile` | `byte_reader`, `cf_error` | Leitura de `u1`/`u2`/`u4` com verificação de limites e erros do leitor. |
| | `class_file` | Estrutura raiz, interfaces, campos e métodos. Estruturas em `class_file.h`, `cp_info.h` e `attribute_info.h`. |
| | `constant_pool`, `cp_validate`, `cp_query` | Pool de constantes: leitura/liberação, validação das referências internas e consultas `cp_get_*`. |
| | `attributes`, `attr_*` (header interno `attr_parsers.h`) | Atributos: leitura genérica e despacho (`attributes.c`), utilitários (`attr_common.c`), atributos de tamanho fixo (`attr_simple.c`), Code (`attr_code.c`), StackMapTable (`attr_stack_map.c`), tabelas (`attr_tables.c`) e liberação (`attr_free.c`). |
| `bytecode` | `opcodes`, `instruction` | Tabela dos 202 opcodes e cálculo do tamanho de cada instrução. |
| `viewer` | `format`, `disassembler` | Formatação de flags, textos e constantes; desmontagem do bytecode. |
| | `printer`, `print_*` (header interno `printer_internal.h`) | Exibidor: seções gerais, campos e métodos (`printer.c`), pool (`print_constant_pool.c`), atributos (`print_attributes.c`) e StackMapTable (`print_stack_map.c`). |
