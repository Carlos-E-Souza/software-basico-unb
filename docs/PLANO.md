# Plano do Projeto — JVM didática em C99

> Disciplina: Software Básico (UnB)
> Objetivo: implementar, em C99, um **leitor/exibidor de arquivos `.class`** e um **interpretador de bytecode Java** capaz de executar pequenos programas compilados com `javac` para **Java 2 (1.2), Java 6 e Java 8**.

---

## 1. Resumo dos requisitos

### 1.1 Regras obrigatórias (hard rules)

| ID | Requisito | Como será garantido |
|----|-----------|---------------------|
| R1 | Compilar com **GCC** | `Makefile` usa `CC=gcc`; build de entrega no Windows via GCC (MinGW-w64) |
| R2 | Da API Java, só é permitida a classe **`java/lang/Object`** | Nenhum `rt.jar`/JDK é carregado pela JVM; `String`, `StringBuilder`, `System.out`, exceções da biblioteca etc. são **simulados nativamente em C** |
| R3 | Todo o código C com **`-std=c99`** | Flag fixa no `Makefile` + `-pedantic -Wall -Wextra` |
| R4 | **libFuzzer** como analisador dinâmico | Alvos de fuzzing para o leitor e para o interpretador (ver decisão D1) |
| R5 | **Flags de linha de comando** para escolher *leitor/exibidor* ou *interpretador* e o **diretório** dos `.class` | Parser de argumentos próprio (seção 3.3) |
| R6 | **Doxygen** gerando: página por função, diagrama de comunicação entre funções, descrição das principais estruturas; para cada função: nome, objetivo, parâmetros de entrada, parâmetros de saída, algoritmo, links para chamadoras e chamadas | `Doxyfile` configurado (seção 3.5) + modelo de comentário obrigatório (seção 3.4) |
| R7 | Comentários escritos **junto com o código**, em qualquer etapa | Definição de pronto de toda atividade exige comentários Doxygen completos |
| R8 | Comentários/documentação em **português do Brasil**; identificadores em **inglês** | Convenção de código (seção 3.4) + revisão em todo checkpoint |

### 1.2 O que será avaliado

1. **Leitor de `.class` e exibidor de bytecode** — exibir as estruturas montadas após a leitura e os mnemônicos associados aos bytecodes.
2. **Corretude da máquina virtual** — interpretar:
   - instruções de **categoria 1 e 2** (tipos computacionais da especificação: cat. 1 = `int`, `float`, `reference`, `returnAddress`; cat. 2 = `long`, `double`);
   - **arrays** (todos os tipos, inclusive multidimensionais);
   - **strings**;
   - **métodos estáticos e dinâmicos**;
   - **atributos de classe e de instância**;
   - **herança, polimorfismo e reescrita**;
   - **objetos**;
   - **tratamento de exceções**;
   - **simulação de impressão** (`System.out.print/println`).
3. **Documentação** contendo tudo o que foi descrito acima.

### 1.3 Ambiente de execução final

- O executável será rodado em **Windows 10/11**.
- Interpretará programas pequenos gerados com `javac`, com bytecode de **Java 2 (major 46), Java 6 (major 50) ou Java 8 (major 52)**.

---

## 2. Decisões técnicas

| ID | Decisão | Motivo |
|----|---------|--------|
| D1 | **GCC compila o projeto; o Clang é usado apenas para compilar os alvos de fuzzing** (`-fsanitize=fuzzer`), também em `-std=c99`. **(Confirmado.)** | O libFuzzer faz parte do LLVM e **não existe no GCC**. O código-fonte é o mesmo; só o *harness* de fuzz é compilado com Clang. |
| D2 | Desenvolvimento em Linux/WSL; o `jvm.exe` de entrega é gerado por **cross-compile** com MinGW-w64 (`x86_64-w64-mingw32-gcc`). Não é necessário compilar no próprio Windows, apenas **executar** lá. **(Confirmado.)** | Mantém o GCC como compilador e evita configurar toolchain no Windows. |
| D3 | Código **100% portátil em C99**: sem APIs POSIX (`dirent`, `mmap`, `unistd`), arquivos abertos com `"rb"`, separador de caminho tratado (`/` e `\`), tipos de tamanho fixo (`stdint.h`). | No Windows `long` tem 32 bits; `long`/`double` Java exigem `int64_t`. |
| D4 | Leitura **big-endian manual** (byte a byte), nunca `fread` direto em structs. | Independência de endianness e de padding. |
| D5 | **JDK 8** (OpenJDK 1.8.0_504) como compilador de referência, gerando os três formatos com `-source 1.2 -target 1.2`, `-source 1.6 -target 1.6` e `-source 1.8 -target 1.8` (verificado: major 46, 50 e 52). | Um único JDK cobre as três versões; o `java` real serve como **oráculo** de saída esperada. |
| D6 | Interpretador com **pilha de operandos e variáveis locais em slots de 32 bits**; `long`/`double` ocupam 2 slots (fiel à especificação). | Facilita acertar `dup2`, `pop2`, `dup_x2` etc. e a semântica cat. 1/cat. 2. |
| D7 | Classes da biblioteca (`String`, `StringBuilder`, `StringBuffer`, `System`, `PrintStream`, `Throwable` e exceções padrão, `Integer`/`Math` se necessário) são **classes sintéticas/nativas** registradas em C. `Object` pode ser carregado do `Object.class` ou também simulado. | Respeita R2. |
| D8 | Sem *garbage collector* (memória liberada ao final). GC é opcional/extra. | Programas de teste são pequenos. |
| D9 | **Escopo = somente o que está na lista de avaliação (seção 1.2).** Ficam fora: `invokedynamic`/lambdas, `Math`, `Integer`/wrappers, `Scanner`/entrada de dados, threads, reflexão, GC. O leitor/exibidor reconhece `invokedynamic`; o interpretador reporta "não suportado". Natives são implementados apenas na medida exigida por strings, impressão e exceções. **(Confirmado.)** | Concatenação de strings no Java 8 ainda usa `StringBuilder` (indy só no Java 9+), logo não é afetada. |
| D10 | `jsr`/`ret`/`jsr_w` **devem** ser implementados. | Verificado no setup: o `javac` 8 com `-target 1.2` **não** gera `jsr/ret` (inlina o `finally`), mas compiladores antigos (≤ 1.4.1) geram. Como não sabemos como os testes do professor são gerados (D13), a implementação é mantida. Diferença observada: com `-target 1.2` a concatenação usa `StringBuffer`; com 1.6/1.8 usa `StringBuilder`. |
| D11 | Framework de testes unitários **próprio e mínimo** em C (macros `ASSERT_*`), sem dependências. | Simples de compilar com GCC em qualquer SO. |
| D12 | **Formato de saída do exibidor é livre.** Usaremos um formato próprio, hierárquico e legível, inspirado no `javap -v`/jclasslib; a comparação com o `javap` é de **conteúdo**, não de formatação. **(Confirmado.)** | Liberdade de formato informada. |
| D13 | Como **não se sabe como os testes de "Java 2" serão gerados**, o leitor e o interpretador aceitam **qualquer major de 45 a 52**, e o corpus de testes de Java 2 é gerado de duas formas: `javac` 8 com `-target 1.2` e, se possível, um JDK antigo (1.3/1.4). | Cobre as diferenças de geração de código entre compiladores (ex.: `jsr/ret`, `StringBuffer`). |
| D14 | Sem cronograma com datas; a ordem é dada pelas dependências (seção 5) e pelos checkpoints. **(Confirmado.)** | Informado pelo usuário. |

---

## 3. Arquitetura proposta

### 3.1 Estrutura de diretórios

```
software-basico-unb/
├── Makefile                # build, debug, test, fuzz, docs, windows, clean
├── Doxyfile                # configuração do Doxygen
├── README.md               # como compilar/usar
├── docs/
│   ├── PLANO.md            # este documento
│   ├── mainpage.md         # página principal do Doxygen
│   └── estruturas.md       # página das principais estruturas de dados
├── include/                # headers públicos (.h)
├── src/
│   ├── main.c              # entrada + parsing de flags
│   ├── cli/                # argumentos de linha de comando
│   ├── common/             # tipos, erros, utilitários de memória/strings
│   ├── classfile/          # leitor: byte_reader, constant_pool, attributes, class_file
│   ├── viewer/             # exibidor: printer, opcodes (tabela de mnemônicos), disassembler
│   └── vm/                 # interpretador: class_loader, heap, frame, interpreter,
│                           #   instructions/*, natives/*, exceptions
├── tests/
│   ├── unit/               # testes unitários em C
│   ├── java/               # programas .java de teste, organizados por tema
│   ├── expected/           # saídas esperadas (geradas pelo java real)
│   └── run_tests.sh        # script de testes de integração (+ .bat/.ps1 para Windows)
├── fuzz/
│   ├── fuzz_reader.c       # alvo libFuzzer do leitor
│   ├── fuzz_interpreter.c  # alvo libFuzzer do interpretador
│   └── corpus/             # corpus inicial (.class válidos)
└── build/                  # artefatos (ignorado no git)
```

### 3.2 Módulos e fluxo

```
main ──► cli (flags) ──┬─► classfile (leitura) ──► viewer (exibição)           [modo leitor/exibidor]
                       └─► vm/class_loader ──► classfile ──► vm/interpreter   [modo interpretador]
                                                   │              ├─► heap (objetos/arrays/strings)
                                                   │              ├─► frame (pilha de operandos/locais)
                                                   │              ├─► natives (String, PrintStream, ...)
                                                   │              └─► exceptions (tabela de exceções/unwind)
```

### 3.3 Interface de linha de comando (proposta)

```
jvm --mode viewer      --classpath <dir> <Classe|arquivo.class> [--output <arquivo>]
jvm --mode interpreter --classpath <dir> <ClassePrincipal>
jvm --help
```

Atalhos: `-v` (viewer), `-i` (interpreter), `-cp <dir>`. Erros de argumento retornam código ≠ 0 e mostram a ajuda.

### 3.4 Convenções de código e de comentário

- Identificadores em inglês, `snake_case` para funções/variáveis, `PascalCase` para tipos (`ClassFile`, `Frame`), `UPPER_CASE` para constantes/macros.
- Todo arquivo começa com bloco `@file` + `@brief` em português.
- Toda função (pública **e** `static`) usa o modelo abaixo — é o que gera o "frame" exigido:

```c
/**
 * @brief  Lê um inteiro sem sinal de 16 bits em big-endian.        // OBJETIVO
 *
 * @details
 * **Algoritmo:**                                                    // DESCRIÇÃO DO ALGORITMO
 * 1. Verifica se há 2 bytes disponíveis no buffer;
 * 2. Combina os bytes como (b0 << 8) | b1;
 * 3. Avança o cursor em 2 posições.
 *
 * @param[in]     reader  Leitor de bytes posicionado no dado.      // PARÂMETROS DE ENTRADA
 * @param[out]    out     Valor lido.                                // PARÂMETROS DE SAÍDA
 * @return  `true` em caso de sucesso; `false` se o buffer acabou.   // SAÍDA
 */
bool read_u2(ByteReader *reader, uint16_t *out);
```

- Toda `struct`/`enum`/campo documentado com `/** ... */` ou `///<`.
- Links para funções chamadoras e chamadas são gerados automaticamente pelo Doxygen (*Referenced by* / *References* + grafos).

### 3.5 Configuração do Doxygen (essencial)

| Opção | Valor | Para quê |
|-------|-------|----------|
| `OUTPUT_LANGUAGE` | `Brazilian` | Interface em português |
| `OPTIMIZE_OUTPUT_FOR_C` | `YES` | Projeto em C |
| `EXTRACT_ALL` | `NO` | Proposital: com `YES` o Doxygen deixa de avisar sobre itens sem documentação. Como tudo é documentado, toda função ganha página mesmo assim |
| `EXTRACT_STATIC` | `YES` | Página também para funções `static` |
| `SEPARATE_MEMBER_PAGES` | `YES` | Uma página web por função |
| `HAVE_DOT` (Graphviz) | `YES` | Diagramas |
| `CALL_GRAPH`, `CALLER_GRAPH` | `YES` | **Diagrama de comunicação entre funções** |
| `REFERENCED_BY_RELATION`, `REFERENCES_RELATION`, `REFERENCES_LINK_SOURCE` | `YES` | **Links para chamadora e chamadas** |
| `SOURCE_BROWSER` | `YES` | Código navegável |
| `WARN_IF_UNDOCUMENTED`, `WARN_NO_PARAMDOC` | `YES` | Avisa se faltar documentação |
| `WARN_AS_ERROR` | `FAIL_ON_WARNINGS` | `make docs` falha se houver qualquer aviso |
| `INPUT` | `src include docs` | Inclui páginas `mainpage.md` e `estruturas.md` |
| `UML_LOOK`, `COLLABORATION_GRAPH` | `YES` | Diagramas das estruturas de dados |

---

## 4. Etapas, atividades e definições de pronto

Legenda de cada atividade: **Objetivo** · **Requisitos** (dependências) · **Tarefas** · **Pronto quando** (DoD).

> **DoD comum a TODA atividade de código** (vale sempre, além do DoD específico):
> compila com `gcc -std=c99 -Wall -Wextra -pedantic` **sem warnings**; comentários Doxygen completos em pt-BR seguindo o modelo 3.4; `make docs` sem warnings; testes da atividade passando; commit/PR revisado por outro membro.

---

### ETAPA 0 — Planejamento

#### P-01 · Plano do projeto
- **Objetivo:** consolidar requisitos, arquitetura, atividades e checkpoints.
- **Requisitos:** enunciado do trabalho.
- **Tarefas:** redigir este documento; esclarecer dúvidas abertas (seção 7).
- **Pronto quando:** plano aprovado pela equipe e dúvidas da seção 7 respondidas.

---

### ETAPA 1 — Setup do ambiente

#### S-01 · GCC e Make (Linux/WSL)
- **Objetivo:** ter o compilador oficial do projeto.
- **Requisitos:** P-01.
- **Tarefas:** instalar `build-essential`; registrar versões (`gcc --version`, `make --version`).
- **Situação:** ✅ concluída — GCC 15.2.0, GNU Make 4.4.1.
- **Pronto quando:** `gcc -std=c99 -pedantic` compila um `hello.c` sem warnings.

#### S-02 · Toolchain para Windows (MinGW-w64)
- **Objetivo:** gerar `jvm.exe` com GCC.
- **Requisitos:** S-01.
- **Tarefas:** instalar `mingw-w64` no WSL; criar alvo `make windows` (cross-compile).
- **Pronto quando:** `hello.exe` gerado com `x86_64-w64-mingw32-gcc -std=c99` executa no Windows 10/11 (testar via `cmd`/PowerShell).
- **Situação:** ✅ concluída — MinGW-w64 GCC 13; `build/jvm.exe` executa no Windows via interoperabilidade do WSL.

#### S-03 · Clang + libFuzzer
- **Objetivo:** viabilizar análise dinâmica com libFuzzer.
- **Requisitos:** S-01.
- **Tarefas:** instalar `clang` (e `llvm`); escrever `fuzz/fuzz_smoke.c` com `LLVMFuzzerTestOneInput` que tem um bug proposital; compilar com `clang -std=c99 -g -fsanitize=fuzzer,address,undefined`.
- **Pronto quando:** o fuzzer roda, **encontra o bug proposital** e gera o arquivo `crash-*`; alvo `make fuzz` funcionando.
- **Situação:** ✅ concluída — Clang 21.1.8 + `libclang-rt-dev`; `make fuzz-smoke` encontra o `stack-buffer-overflow` em < 1 s.

#### S-04 · Doxygen + Graphviz
- **Objetivo:** gerar documentação com diagramas.
- **Requisitos:** S-01.
- **Tarefas:** instalar `doxygen` e `graphviz`; criar `Doxyfile` com as opções da seção 3.5; documentar o `hello.c` com o modelo 3.4.
- **Pronto quando:** `make docs` gera `build/docs/html/index.html` em português, com página da função contendo objetivo, parâmetros, algoritmo, **grafo de chamadas e de chamadoras**.
- **Situação:** ✅ concluída — Doxygen 1.15.0 + Graphviz 14.1.2; função sem documentação faz `make docs` falhar.

#### S-05 · Java (JDK) e versões de bytecode
- **Objetivo:** garantir que conseguimos gerar `.class` nas três versões e ter um oráculo de saída.
- **Requisitos:** —
- **Tarefas:**
  1. instalar **JDK 8** (`openjdk-8-jdk` ou Temurin 8);
  2. compilar `Hello.java` com: `-source 1.2 -target 1.2` (ou `-source 1.3 -target 1.2`), `-source 1.6 -target 1.6` e `-source 1.8 -target 1.8`;
  3. verificar com `javap -v` que o *major version* é **46**, **50** e **52**;
  4. tentar obter também um JDK antigo (1.3/1.4) para gerar uma segunda variante do corpus Java 2 (D13); se o JDK 8 não aceitar o alvo 1.2, o JDK antigo passa a ser obrigatório.
- **Pronto quando:** os três `.class` existem com os *major versions* corretos e executam no `java` real.
- **Situação:** ✅ concluída — OpenJDK 1.8.0_504; `-source 1.2 -target 1.2` aceito (major 46). JDK antigo não obtido (opcional, ver D13).

#### S-06 · Esqueleto do repositório
- **Objetivo:** estrutura base do projeto.
- **Requisitos:** S-01…S-05.
- **Tarefas:** criar diretórios (3.1); `Makefile` com alvos `all`, `debug` (com `-g -fsanitize=address,undefined`), `test`, `fuzz`, `docs`, `windows`, `clean`; `main.c` mínimo documentado; `docs/mainpage.md`; atualizar `.gitignore` (`build/`, `*.class`, `crash-*`, artefatos do fuzzer); `README.md` com instruções.
- **Pronto quando:** `make && make test && make docs && make fuzz && make windows` executam sem erro em um clone limpo.
- **Situação:** ✅ concluída.

#### S-07 · Infraestrutura de testes
- **Objetivo:** testar automaticamente desde o início.
- **Requisitos:** S-05, S-06.
- **Tarefas:** mini-framework de testes unitários (`tests/unit/test.h`); script `tests/run_tests.sh` que: compila cada `tests/java/**/*.java` nas 3 versões → executa no `java` real para gerar `expected/` → executa na nossa JVM → compara com `diff`; versão `.bat`/`.ps1` para Windows.
- **Pronto quando:** um teste unitário trivial e um teste de integração trivial (que falha de propósito, depois passa) são reportados corretamente.
- **Situação:** ✅ concluída — `make test` (unitários); `make test-integration` falha com a nossa JVM (esperado, interpretador inexistente) e passa com `JVM_CMD=java`; `tests/run_tests.ps1` validado no PowerShell do Windows.

> ### ✅ CHECKPOINT C1 — Ambiente pronto (**atingido**)
> - [x] GCC (Linux e MinGW) compila em C99 sem warnings
> - [x] libFuzzer encontra o bug proposital
> - [x] Doxygen gera HTML em pt-BR com grafos de chamada/chamadora
> - [x] JDK gera `.class` major 46, 50 e 52
> - [x] `make` com todos os alvos funcionando; scripts de teste funcionando
> - [x] Executável "hello" roda no Windows

---

### ETAPA 2 — Leitor de `.class`

#### L-01 · Leitor de bytes (`ByteReader`)
- **Objetivo:** ler `u1`, `u2`, `u4` (big-endian) de um buffer com verificação de limites.
- **Requisitos:** C1.
- **Tarefas:** carregar arquivo inteiro em memória (`fopen "rb"`); funções `read_u1/u2/u4`, `read_bytes`; erro em leitura fora dos limites (nunca *crash*).
- **Pronto quando:** testes unitários cobrindo leitura normal, fim de buffer e arquivo inexistente.

#### L-02 · Estruturas de dados do ClassFile
- **Objetivo:** modelar em C todas as estruturas da especificação (JVMS §4).
- **Requisitos:** L-01.
- **Tarefas:** definir `ClassFile`, `CpInfo` (união por *tag*), `FieldInfo`, `MethodInfo`, `AttributeInfo` (união por tipo), `ExceptionTableEntry` etc.; documentar cada campo.
- **Pronto quando:** headers compilam; aparecem na página "Estruturas de dados" do Doxygen com descrição e diagrama.

#### L-03 · Cabeçalho e validação
- **Objetivo:** ler `magic`, `minor_version`, `major_version`.
- **Requisitos:** L-02.
- **Tarefas:** validar `0xCAFEBABE`; aceitar *major* 45–52 (mapear para "Java 1.1 … Java 8"); rejeitar demais com mensagem clara.
- **Pronto quando:** testes com arquivos válidos (46/50/52), magic inválido e versão não suportada.

#### L-04 · Pool de constantes
- **Objetivo:** ler o *constant pool* completo.
- **Requisitos:** L-03.
- **Tarefas:** tratar todas as *tags* até o Java 8: `Utf8`, `Integer`, `Float`, `Long`, `Double`, `Class`, `String`, `Fieldref`, `Methodref`, `InterfaceMethodref`, `NameAndType`, `MethodHandle`, `MethodType`, `InvokeDynamic`; `Long`/`Double` ocupam **duas entradas**; validar índices; decodificar **UTF-8 modificado** (MUTF-8); funções auxiliares para resolver nomes (`cp_get_utf8`, `cp_get_class_name`…).
- **Pronto quando:** teste com classe contendo todos os tipos de constante (incl. `long`/`double`) lida corretamente; índices inválidos geram erro, não *crash*.

#### L-05 · Flags, this/super, interfaces
- **Objetivo:** ler `access_flags`, `this_class`, `super_class`, `interfaces[]`.
- **Requisitos:** L-04.
- **Pronto quando:** testes com classe que estende outra e implementa 2 interfaces.

#### L-06 · Campos e métodos
- **Objetivo:** ler `fields[]` e `methods[]`.
- **Requisitos:** L-05.
- **Pronto quando:** testes com campos estáticos/instância e métodos estáticos/instância/construtores.

#### L-07 · Atributos
- **Objetivo:** interpretar os atributos relevantes.
- **Requisitos:** L-06.
- **Tarefas:** `Code` (com `exception_table` e atributos aninhados), `ConstantValue`, `Exceptions`, `SourceFile`, `LineNumberTable`, `LocalVariableTable`, `InnerClasses`, `Synthetic`, `Deprecated`, `Signature`, `StackMapTable`, `BootstrapMethods`, `EnclosingMethod`; atributos desconhecidos guardados como bytes brutos.
- **Pronto quando:** teste lê atributos de classes Java 2, 6 e 8 (incl. `StackMapTable` no Java 8); atributos desconhecidos não quebram a leitura.

#### L-08 · Liberação de memória e robustez
- **Objetivo:** leitor sem vazamentos e resistente a entradas inválidas.
- **Requisitos:** L-07.
- **Tarefas:** `class_file_free`; caminhos de erro liberam tudo; executar com ASan/UBSan (`make debug`).
- **Pronto quando:** ASan/LeakSanitizer sem erros em todo o corpus de testes.

#### L-09 · Fuzzing do leitor
- **Objetivo:** analisar dinamicamente o leitor com libFuzzer.
- **Requisitos:** L-08, S-03.
- **Tarefas:** `fuzz/fuzz_reader.c` (lê a partir de buffer em memória); corpus inicial com os `.class` de teste; rodar ≥ 30 min; corrigir *crashes* e adicionar cada um como teste de regressão.
- **Pronto quando:** sessão de fuzzing de 30 min sem *crash*/vazamento; relatório curto (cobertura, bugs encontrados) registrado em `docs/`.

---

### ETAPA 3 — Exibidor

#### E-01 · Tabela de opcodes
- **Objetivo:** mapear todos os opcodes (0x00–0xC9) para mnemônico e tamanho de operandos.
- **Requisitos:** L-02.
- **Tarefas:** tabela estática `{opcode, mnemonic, operand_bytes, operand_kind}`; casos especiais `tableswitch`/`lookupswitch` (alinhamento de 4 bytes), `wide`, `invokeinterface`, `multianewarray`.
- **Pronto quando:** teste unitário confere todos os 202 opcodes com a especificação.

#### E-02 · Informações gerais
- **Objetivo:** definir o formato próprio de saída (D12) e exibir minor/major (com nome da versão Java), tamanho do pool, flags decodificadas, this/super, contagens.
- **Requisitos:** L-07.
- **Pronto quando:** saída conferida manualmente com `javap -v` para 3 classes (uma de cada versão).

#### E-03 · Pool de constantes
- **Objetivo:** exibir cada entrada com índice, tipo e valor **resolvido** (ex.: `#7 = Methodref #2.#15 // java/io/PrintStream.println:(Ljava/lang/String;)V`).
- **Requisitos:** E-02.
- **Pronto quando:** comparação com `javap -v` sem divergências de conteúdo.

#### E-04 · Interfaces, campos e métodos
- **Objetivo:** exibir nome, descritor, flags e atributos de cada item.
- **Requisitos:** E-03.
- **Pronto quando:** idem E-03.

#### E-05 · Atributos e bytecode (disassembler)
- **Objetivo:** exibir atributos e o código com mnemônicos.
- **Requisitos:** E-01, E-04.
- **Tarefas:** para cada instrução: *offset*, mnemônico, operandos (índices do pool resolvidos, destinos de desvio em endereço absoluto, tabelas de `switch`, `wide`); `exception_table`, `LineNumberTable`, `LocalVariableTable`.
- **Pronto quando:** *disassembly* de todo o corpus confere com `javap -c -v` (script de comparação semiautomática).

#### E-06 · Integração com a CLI
- **Objetivo:** modo leitor/exibidor acessível pelas flags.
- **Requisitos:** E-05.
- **Tarefas:** `--mode viewer`, `--classpath`, `--output`; mensagens de erro claras.
- **Pronto quando:** funciona no Linux **e** no `jvm.exe` no Windows para os corpora Java 2, 6 e 8.

> ### ✅ CHECKPOINT C2 — Leitor/Exibidor (1ª entrega)
> - [ ] Lê corretamente `.class` de Java 2, 6 e 8
> - [ ] Exibe todas as estruturas e o bytecode com mnemônicos
> - [ ] Saída conferida contra `javap` em todo o corpus
> - [ ] Fuzzing do leitor sem *crashes*; ASan sem vazamentos
> - [ ] Funciona no Windows com as flags
> - [ ] Documentação Doxygen do leitor/exibidor completa e sem warnings
> - [ ] *Tag* git `v1.0-leitor-exibidor`

---

### ETAPA 4 — Interpretador (corretude da JVM)

#### I-01 · Estruturas de tempo de execução
- **Objetivo:** modelar área de métodos, heap, pilha de *frames*.
- **Requisitos:** C2.
- **Tarefas:** `Slot` (32 bits), `Frame` (variáveis locais, pilha de operandos, `pc`, referência ao método e à classe), `JvmThread`/pilha de frames, `RuntimeClass` (classe carregada + campos estáticos + vtable/lookup), `Object` (classe + campos), `Array` (tipo + tamanho + dados), referência = ponteiro/handle.
- **Pronto quando:** testes unitários de *push/pop* de cat. 1 e cat. 2 e criação/destruição de frames.

#### I-02 · Carregador de classes
- **Objetivo:** carregar classes sob demanda a partir do diretório (`--classpath`).
- **Requisitos:** I-01.
- **Tarefas:** busca por nome (`pkg/Classe` → `<dir>/pkg/Classe.class`); carga recursiva da superclasse e interfaces; registro das classes nativas/sintéticas (D7); cálculo do layout de campos de instância (incluindo herdados); inicialização de estáticos com `ConstantValue` e execução de `<clinit>` na primeira utilização ativa.
- **Pronto quando:** carrega hierarquia de 3 níveis; `<clinit>` executa uma única vez e na ordem correta.

#### I-03 · Laço de execução
- **Objetivo:** ciclo *fetch–decode–execute* com tabela de despacho (ponteiros de função por opcode).
- **Requisitos:** I-02.
- **Tarefas:** localizar `public static void main([Ljava/lang/String;)V`; criar frame inicial; laço até a pilha de frames esvaziar; opcode não implementado → erro claro com nome/offset.
- **Pronto quando:** um `main` vazio (`return`) executa e termina com código 0.

#### I-04 · Constantes, *loads* e *stores*
- **Tarefas:** `nop`, `aconst_null`, `iconst_*`, `lconst_*`, `fconst_*`, `dconst_*`, `bipush`, `sipush`, `ldc`, `ldc_w`, `ldc2_w`; `*load`, `*load_n`, `*store`, `*store_n` (i/l/f/d/a); `wide`.
- **Pronto quando:** testes Java com variáveis locais de todos os tipos primitivos passam nas 3 versões.

#### I-05 · Manipulação de pilha
- **Tarefas:** `pop`, `pop2`, `dup`, `dup_x1`, `dup_x2`, `dup2`, `dup2_x1`, `dup2_x2`, `swap` — respeitando as **formas de cat. 1 e cat. 2**.
- **Pronto quando:** testes unitários de cada forma + programa Java que gera `dup2_x1` com `long` (ex.: `arr[i] = x = longValue`).

#### I-06 · Aritmética, lógica e conversões
- **Tarefas:** `add/sub/mul/div/rem/neg` (i/l/f/d); `shl/shr/ushr` (máscara 0x1F/0x3F), `and/or/xor` (i/l); `iinc`; conversões `i2l…i2s` com regras do Java (NaN → 0, saturação em `f2i/d2i/f2l/d2l`); `lcmp`, `fcmpl/fcmpg`, `dcmpl/dcmpg`; `idiv/ldiv/irem/lrem` por zero → `ArithmeticException` (após I-12); overflow com aritmética *wrap-around* (sem UB em C: usar `uint32_t/uint64_t`); `fmod` para `frem/drem`.
- **Pronto quando:** programa de teste de limites (`Integer.MIN_VALUE / -1`, NaN, infinitos, overflow) produz saída idêntica ao `java` real.

#### I-07 · Controle de fluxo
- **Tarefas:** `if*`, `if_icmp*`, `if_acmp*`, `ifnull`, `ifnonnull`, `goto`, `goto_w`, `tableswitch`, `lookupswitch`, `jsr`, `ret`, `jsr_w`, `*return`.
- **Pronto quando:** testes com laços, `switch` denso/esparso e `try/finally` compilado em Java 2 (`jsr/ret`) passam.

#### I-08 · Métodos estáticos
- **Tarefas:** `invokestatic`; passagem de parâmetros a partir do descritor (cat. 2 ocupa 2 slots); valores de retorno; recursão.
- **Pronto quando:** testes de fatorial/fibonacci recursivos e métodos com parâmetros `long`/`double` mistos passam.

#### I-09 · Objetos e atributos
- **Tarefas:** `new`, `getfield`, `putfield`, `getstatic`, `putstatic`, `invokespecial` (construtores, `super.metodo()`, privados), `instanceof`, `checkcast`, valores padrão de campos.
- **Pronto quando:** testes de atributos de classe (contador estático compartilhado) e de instância passam.

#### I-10 · Herança, polimorfismo e reescrita
- **Tarefas:** `invokevirtual` com busca do método na hierarquia da classe real do objeto; `invokeinterface`; resolução de campos em superclasses; classes abstratas; `super` em cadeia.
- **Pronto quando:** testes com hierarquia `Animal → Cachorro/Gato`, array de `Animal[]` chamando método reescrito, interfaces e classe abstrata passam.

#### I-11 · Arrays
- **Tarefas:** `newarray` (todos os 8 tipos primitivos), `anewarray`, `multianewarray`, `arraylength`, `iaload/laload/faload/daload/aaload/baload/caload/saload` e respectivos `*astore`; verificação de limites e `null`.
- **Pronto quando:** testes com arrays de todos os tipos, matriz 3D, array de objetos e array de arrays irregular passam.

#### I-12 · Exceções
- **Tarefas:** `athrow`; busca na `exception_table` (faixa de `pc` + tipo compatível por herança, `catch_type = 0` para `finally`); desempilhamento de frames; exceções lançadas pela própria JVM (`ArithmeticException`, `NullPointerException`, `ArrayIndexOutOfBoundsException`, `NegativeArraySizeException`, `ClassCastException`) como classes sintéticas; exceções definidas pelo usuário (que estendem `Exception`/`RuntimeException`); `getMessage()`; exceção não capturada imprime mensagem no formato do `java` e termina com código ≠ 0.
- **Pronto quando:** testes com `try/catch/finally` aninhados, propagação entre métodos, exceção do usuário e exceções da JVM passam nas 3 versões.

#### I-13 · Strings
- **Tarefas:** `ldc` de `String` cria objeto string interno (com *intern*); natives de `String` usados nos testes (`length`, `charAt`, `equals`, `concat`, `substring`, `indexOf`, `toString`, `valueOf`…); `StringBuilder` (Java 6/8) e `StringBuffer` (Java 2) com `append` para todos os tipos e `toString`; concatenação com `+`.
- **Pronto quando:** testes de concatenação com todos os tipos primitivos, comparação e métodos de string passam nas 3 versões.

#### I-14 · Simulação de impressão
- **Tarefas:** `getstatic java/lang/System.out` retorna objeto `PrintStream` sintético; `print`/`println` para `int`, `long`, `float`, `double`, `char`, `boolean`, `String`, `char[]`, `Object` (`toString`); formatação de `float`/`double` **idêntica ao Java** (ex.: `1.0`, `1.0E10`, `NaN`, `Infinity`) — algoritmo de menor representação que faz *round-trip*.
- **Pronto quando:** teste de impressão de todos os tipos, incluindo valores especiais de ponto flutuante, idêntico ao `java` real.

#### I-15 · Instruções restantes e `Object`
- **Tarefas:** `monitorenter`/`monitorexit` (sem efeito — monothread); natives de `Object` (`<init>`, `hashCode`, `equals`, `toString` no formato `Classe@hash`); `invokedynamic` → erro "não suportado" (D9).
- **Pronto quando:** **todos os opcodes 0x00–0xC9** implementados ou explicitamente tratados; teste automatizado confirma que nenhum opcode cai no "não implementado" inesperadamente.

#### I-16 · Fuzzing e robustez do interpretador
- **Tarefas:** `fuzz/fuzz_interpreter.c` com limite de passos/memória; verificações defensivas (índices do pool, pilha estourada/vazia, `pc` fora do código); ASan/UBSan em toda a suíte.
- **Pronto quando:** 30 min de fuzzing sem *crash* não tratado; suíte limpa em ASan/UBSan.

> ### ✅ CHECKPOINT C3a — Núcleo (após I-08)
> - [ ] Programas só com `main` + métodos estáticos e todos os tipos primitivos executam corretamente nas 3 versões
>
> ### ✅ CHECKPOINT C3b — Orientação a objetos (após I-11)
> - [ ] Objetos, atributos, herança, polimorfismo, interfaces e arrays corretos
>
> ### ✅ CHECKPOINT C3c — Interpretador completo (após I-16)
> - [ ] Exceções, strings e impressão idênticos ao `java` real
> - [ ] 100% da suíte de integração passa em Java 2, 6 e 8, **no Linux e no Windows**
> - [ ] Fuzzing e sanitizers limpos
> - [ ] *Tag* git `v2.0-interpretador`

---

### ETAPA 5 — Validação final e documentação

#### F-01 · Bateria de programas de avaliação
- **Objetivo:** cobrir cada item avaliado com ao menos 2 programas.
- **Tarefas:** organizar `tests/java/` por tema: `cat1_cat2/`, `arrays/`, `strings/`, `static_methods/`, `instance_methods/`, `class_fields/`, `instance_fields/`, `inheritance/`, `polymorphism/`, `override/`, `objects/`, `exceptions/`, `print/`; rodar nas 3 versões.
- **Pronto quando:** matriz *tema × versão* 100% verde em Linux e Windows.

#### F-02 · Documentação Doxygen final
- **Tarefas:** `mainpage.md` (visão geral, arquitetura, como compilar/usar); `estruturas.md` (principais estruturas: `ClassFile`, `CpInfo`, `Frame`, `RuntimeClass`, `Object`, `Array`); revisar se cada função tem objetivo, parâmetros de entrada/saída, algoritmo; conferir grafos de chamada/chamadora.
- **Pronto quando:** `make docs` sem nenhum warning; revisão manual de amostra de 20 funções aprovada pela equipe; HTML gerado empacotado para entrega.

#### F-03 · Pacote de entrega
- **Tarefas:** `jvm.exe` compilado com GCC (MinGW), fontes, `Makefile`, documentação HTML, `README` com exemplos de uso das flags, relatório de fuzzing.
- **Pronto quando:** em uma máquina Windows limpa, seguindo só o `README`, é possível exibir e executar os programas de teste.

> ### ✅ CHECKPOINT C4 — Entrega final
> - [ ] Todos os itens da seção 1.2 demonstráveis
> - [ ] Requisitos R1–R8 verificados (checklist da seção 1.1)
> - [ ] *Tag* git `v3.0-final`

---

## 5. Ordem de execução e dependências

```
P-01 → S-01..S-07 → [C1] → L-01 → L-02 → L-03 → L-04 → L-05 → L-06 → L-07 → L-08 → L-09
                                    └→ E-01 ─────────────────────────────┐
                    L-07 → E-02 → E-03 → E-04 → E-05 → E-06 → [C2]
     [C2] → I-01 → I-02 → I-03 → I-04 → I-05 → I-06 → I-07 → I-08 → [C3a]
          → I-09 → I-10 → I-11 → [C3b] → I-12 → I-13 → I-14 → I-15 → I-16 → [C3c]
          → F-01 → F-02 → F-03 → [C4]
```

Paralelização possível em equipe: E-01 junto com L-03…L-07; I-13/I-14 (natives) em paralelo com I-09…I-11; F-02 contínuo (cada atividade já entrega comentários).

---

## 6. Riscos e mitigações

| Risco | Impacto | Mitigação |
|-------|---------|-----------|
| libFuzzer não existe no GCC | Conflito R1 × R4 | D1: Clang só para os *harnesses* de fuzz; produto final compilado com GCC |
| `javac` moderno não gera bytecode Java 2 / testes do professor gerados de forma desconhecida | Sem testes de Java 2 ou divergência na avaliação | D5/D13: testar cedo (S-05); aceitar major 45–52; corpus Java 2 gerado por mais de um compilador |
| Diferenças Linux × Windows (`long` 32 bits, `\r\n`, caminhos, `printf` de `%lld` no MinGW antigo) | Bugs só no Windows | D3; usar `PRId64` de `inttypes.h`; `__USE_MINGW_ANSI_STDIO=1`; testar no Windows em todo checkpoint |
| Formatação de `float`/`double` diferente do Java | Saída divergente | Implementar algoritmo de menor dígito com *round-trip*; testes dedicados (I-14) |
| UB em C na aritmética (overflow com sinal, shift ≥ largura, divisão `MIN/-1`) | Resultados errados/crash | Aritmética em tipos sem sinal; casos especiais explícitos; UBSan |
| `jsr/ret` e `finally` no Java 2 | Exceções falham no Java 2 | I-07 inclui testes específicos de `finally` em Java 2 |
| Documentação deixada para o fim | Perda de nota | R7/DoD comum: `make docs` com `WARN_AS_ERROR` |

---

## 7. Dúvidas respondidas

| # | Pergunta | Resposta | Reflexo no plano |
|---|----------|----------|------------------|
| 1 | Clang apenas para o fuzzing? | Sim | D1 |
| 2 | Compilar no Windows ou só executar? | Basta o `jvm.exe` executar no Windows | D2, S-02 |
| 3 | Formato do exibidor? | Livre | D12, E-02 |
| 4 | Como serão gerados os testes de Java 2? | Desconhecido | D13, S-05, riscos |
| 5 | `invokedynamic`, `Math`, `Integer`, `Scanner`? | Fora do escopo (só o que está na avaliação) | D9 |
| 6 | Equipe e prazos? | Não é necessário | D14 |
