# JVM Didática {#mainpage}

Máquina Virtual Java didática escrita em **C99** e compilada em **32 bits**
(`-m32`), desenvolvida para a disciplina de **Software Básico (UnB)**. O
objetivo é uma JVM funcional e o mais simples possível: sem threads, JIT,
coleta de lixo ou verificador de bytecode.

## Modos de operação

- **Leitor/Exibidor:** lê um arquivo `.class` e exibe as estruturas montadas
  (pool de constantes, campos, métodos, atributos) e o bytecode com os
  mnemônicos.
- **Interpretador:** executa programas Java compilados com `javac` para
  Java 2 (1.2), Java 6 ou Java 8.

## Como compilar

Linux/WSL com GCC multilib (`-m32`), Make e, para os testes, o JDK 8:

```
make               # build/jvm (ELF 32 bits)
make windows       # build/jvm.exe (PE32, MinGW i686), roda no Windows 10/11
make check         # testes unitários + comparação com javap + integração
make docs          # esta documentação (docs/html)
```

## Uso

```
jvm --mode viewer      --classpath <dir> <Classe|arquivo.class> [--output <arquivo>]
jvm --mode interpreter --classpath <dir> <ClassePrincipal>
```

Atalhos: `-v`, `-i`, `-cp`, `-o`. Exemplo: `jvm -i -cp out/java8 Exemplo`.
Códigos de saída: `0` sucesso, `1` argumentos inválidos, `3` erro no
`.class` ou classe não encontrada, `4` exceção não capturada, `5` erro fatal
do interpretador.

## Arquitetura

```
main ─► cli ─┬─► classfile (leitura) ─► viewer (exibição)                 [-v]
             └─► vm: class_loader ─► classfile ─► interpreter             [-i]
                               │                    ├─► heap (objetos, arrays, strings)
                               │                    ├─► frame (pilha de operandos e locais)
                               │                    ├─► exec_* (instruções, tabela handlers[256])
                               └────────────────────┴─► natives (Object, String, System.out, exceções)
```

- **Exibidor:** class_file_load() lê o `.class` byte a byte (big-endian) e
  valida tudo; viewer_print_class() escreve as seções no formato do
  jclasslib, com `cp_info #n <constante>` em toda referência ao pool.
- **Interpretador:** vm_run_main() carrega a classe principal (e, sob
  demanda, as demais), executa `<clinit>` e chama `main`. Cada método roda em
  interpreter_call(), cujo laço chama interpreter_step(): um índice na tabela
  `handlers[256]` por instrução. As classes da biblioteca (`Object`,
  `String`, `StringBuilder`, `System.out`, exceções) são simuladas em C.

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
`include/`. Cada arquivo tem **uma responsabilidade** e **cerca de 300 linhas no máximo**:
arquivos com a mesma responsabilidade ficam juntos até esse limite, e um
arquivo que passa dele é dividido por responsabilidade; as
funções compartilhadas entre as partes são declaradas em um header interno
(que não faz parte da interface pública do módulo).

| Módulo | Arquivos | Responsabilidade |
|--------|----------|------------------|
| `common` | `file_io`, `mutf8`, `number_format`, `platform.h` | Utilitários: leitura de arquivos, UTF-8 modificado, formatação de números no estilo do Java e verificação de 32 bits (`-m32`). A versão do programa é a macro `JVM_VERSION` de `version.h`. |
| `cli` | `cli` | Argumentos de linha de comando. |
| `classfile` | `byte_reader`, `cf_error` | Leitura de `u1`/`u2`/`u4` com verificação de limites e erros do leitor. |
| | `class_file` | Estrutura raiz, interfaces, campos e métodos. Estruturas em `class_file.h`, `cp_info.h` e `attribute_info.h`. |
| | `constant_pool`, `cp_validate`, `cp_query` | Pool de constantes: leitura/liberação, validação das referências internas e consultas `cp_get_*`. |
| | `attributes`, `attr_*` (header interno `attr_parsers.h`) | Atributos: leitura genérica, despacho e utilitários (`attributes.c`), atributos informativos e desconhecidos (`attr_metadata.c`), Code (`attr_code.c`), StackMapTable (`attr_stack_map.c`) e liberação (`attr_free.c`). |
| `bytecode` | `opcodes`, `instruction` | Tabela dos 202 opcodes e cálculo do tamanho de cada instrução. |
| `vm` | `runtime.h` | Tipos de execução: `Slot`, `Field`, `Object` (instância, array ou texto), `RuntimeClass`, `Frame`, `Vm`. |
| | `frame`, `heap`, `descriptor` | Pilha de operandos e locais (cat. 1/cat. 2, etiquetas de slot), heap sem GC e leitura de descritores. |
| | `class_loader`, `class_hierarchy` | Carregamento de classes do usuário e nativas, `<clinit>`, busca de métodos/campos e compatibilidade de tipos (busca linear, D19). |
| | `vm`, `interpreter` | Estado da VM, erros fatais, exceções, `main`, laço de execução com a tabela `handlers[256]` e chamada de métodos. |
| | `exec_*` | Instruções por família: constantes/loads/stores (`exec_load_store`), pilha (`exec_stack`), `int`/`long` (`exec_int`), ponto flutuante/conversões/comparações (`exec_float`), desvios/retornos (`exec_branch`), objetos/campos (`exec_object`), invocação (`exec_invoke`) e arrays (`exec_array`). |
| | `natives`, `native_*` | Biblioteca simulada em C (D7): tabela de classes nativas, `Object`/`Throwable` (`native_object`), `String`/`StringBuilder` (`native_string`), conversão para texto, `valueOf` e `append` (`native_text`) e `System.out` (`native_print`). |
| `viewer` | `printer`, `print_*`, `format`, `disassembler` (header interno `printer_internal.h`) | Exibidor: seções gerais, campos e métodos (`printer.c`), formatação geral — flags, MUTF-8, linhas `Rotulo: valor` (`format.c`), pool e representação das constantes (`print_constant_pool.c`), atributos (`print_attributes.c`), StackMapTable (`print_stack_map.c`) e desmontagem do bytecode (`disassembler.c`). |
