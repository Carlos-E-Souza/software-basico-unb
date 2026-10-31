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
