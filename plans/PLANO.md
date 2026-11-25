# Plano do Projeto — JVM em C99

> Disciplina: Software Básico (UnB)
> Objetivo: implementar, em C99, um **leitor/exibidor de arquivos `.class`** e um **interpretador de bytecode Java** capaz de executar pequenos programas compilados com `javac` para **Java 2 (1.2), Java 6 e Java 8**.
>
> **Princípio do projeto:** é um trabalho **acadêmico**. Queremos uma JVM **funcional e o mais simples possível**: só o que está na lista de avaliação (seção 1.2), com a solução mais direta que a especificação permite. Tudo o que uma JVM real tem e a avaliação não pede (threads, JIT, *garbage collector*, verificador de bytecode etc. — ver D9) fica de fora. Na dúvida entre "mais completo" e "mais simples", escolhe-se o mais simples.

---

## 1. Resumo dos requisitos

### 1.1 Regras obrigatórias (hard rules)

| ID | Requisito | Como será garantido |
|----|-----------|---------------------|
| R1 | Compilar com **GCC** | `Makefile` usa `CC=gcc`; build de entrega no Windows via GCC (MinGW-w64) |
| R2 | Da API Java, só é permitida a classe **`java/lang/Object`** | Nenhum `rt.jar`/JDK é carregado pela JVM; `String`, `StringBuilder`, `System.out`, exceções da biblioteca etc. são **simulados nativamente em C** |
| R3 | Todo o código C com **`-std=c99`** | Flag fixa no `Makefile` + `-pedantic -Wall -Wextra` |
| R4 | **libFuzzer** como analisador dinâmico | Alvos de fuzzing para o leitor e para o interpretador (ver decisões D1 e D15) |
| R5 | **Flags de linha de comando** para escolher *leitor/exibidor* ou *interpretador* e o **diretório** dos `.class` | Parser de argumentos próprio (seção 3.3) |
| R6 | **Doxygen** gerando: página por função, diagrama de comunicação entre funções, descrição das principais estruturas; para cada função: nome, objetivo, parâmetros de entrada, parâmetros de saída, algoritmo, links para chamadoras e chamadas | `Doxyfile` configurado (seção 3.5) + modelo de comentário obrigatório (seção 3.4) |
| R7 | Comentários escritos **junto com o código**, em qualquer etapa | Definição de pronto de toda atividade exige comentários Doxygen completos |
| R8 | Comentários/documentação em **português do Brasil**; identificadores em **inglês** | Convenção de código (seção 3.4) + revisão em todo checkpoint |
| R9 | Exibidor **similar ao jclasslib**: apresentar as estruturas do `.class`; incluir os **índices dos slots do pool** e a **constante representada**; mnemônicos com seus parâmetros, e os parâmetros que apontam para o pool devem indicar o **valor da constante** | Saída organizada como o jclasslib (General Information, Constant Pool, Interfaces, Fields, Methods, Attributes); toda referência ao pool escrita como `cp_info #n <constante>`; bytecode como `invokevirtual #4 <java/io/PrintStream.println : (I)V>` (E-02 a E-05) |
| R10 | Compilar com **`-m32`** (código de 32 bits, como a especificação da JVM) | Flag `-m32` em **todos** os alvos do `Makefile` (build, debug, testes, fuzzing); `jvm.exe` gerado com o MinGW de 32 bits (`i686-w64-mingw32-gcc`); `_Static_assert`-equivalente em C99 garante ponteiro de 4 bytes (D16) |
| R11 | **cppcheck** como verificador estático de código | Alvo `make cppcheck` (D24) sem nenhum aviso em todo checkpoint e no DoD de toda atividade de código; instalado e conferido por `tools/check_env.sh` (S-09) |

### 1.2 O que será avaliado

1. **Leitor de `.class` e exibidor de bytecode** — exibir as estruturas montadas após a leitura e os mnemônicos associados aos bytecodes, de forma similar ao **jclasslib** (índices do pool com a constante representada; argumentos do bytecode que apontam para o pool com o valor da constante) — requisito R9.
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

- O executável será rodado em **Windows 10/11** (um `jvm.exe` de 32 bits roda normalmente no Windows de 64 bits).
- Interpretará programas pequenos gerados com `javac`, com bytecode de **Java 2 (major 46), Java 6 (major 50) ou Java 8 (major 52)**.

---

## 2. Decisões técnicas

| ID | Decisão | Motivo |
|----|---------|--------|
| D1 | **GCC compila o projeto; o Clang é usado apenas para compilar os alvos de fuzzing** (`-fsanitize=fuzzer`), também em `-std=c99` e `-m32`. **(Confirmado.)** | O libFuzzer faz parte do LLVM e **não existe no GCC**. O código-fonte é o mesmo; só o *harness* de fuzz é compilado com Clang. O Clang 21 instalado já traz o runtime `libclang_rt.fuzzer-i386.a`. |
| D2 | Desenvolvimento em Linux/WSL; o `jvm.exe` de entrega é gerado por **cross-compile** com MinGW-w64 de 32 bits (`i686-w64-mingw32-gcc`). Não é necessário compilar no próprio Windows, apenas **executar** lá. **(Confirmado; revisado por R10.)** | Mantém o GCC como compilador e evita configurar toolchain no Windows. |
| D3 | Código **100% portátil em C99**: sem APIs POSIX (`dirent`, `mmap`, `unistd`), arquivos abertos com `"rb"`, separador de caminho tratado (`/` e `\`), tipos de tamanho fixo (`stdint.h`). | Com `-m32`, `long`, `size_t` e ponteiros têm 32 bits no Linux **e** no Windows; `long`/`double` Java exigem `int64_t`. |
| D4 | Leitura **big-endian manual** (byte a byte), nunca `fread` direto em structs. | Independência de endianness e de padding. |
| D5 | **JDK 8** (OpenJDK 1.8.0_504) como compilador de referência, gerando os três formatos com `-source 1.2 -target 1.2`, `-source 1.6 -target 1.6` e `-source 1.8 -target 1.8` (verificado: major 46, 50 e 52). | Um único JDK cobre as três versões; o `java` real serve como **oráculo** de saída esperada. |
| D6 | Pilha de operandos e variáveis locais em **slots de 32 bits** (`typedef uint32_t Slot`, sem sinal porque a aritmética é feita em `uint32_t`, ver I-04; **revisada** em 2026-11-24: antes `int32_t`); `long`/`double` ocupam 2 slots (fiel à especificação). Com `-m32`, uma **referência cabe em um slot**: o slot guarda o próprio ponteiro do objeto (sem tabela de *handles*). | Facilita acertar `dup2`, `pop2`, `dup_x2` etc. e a semântica cat. 1/cat. 2; é a motivação do R10. |
| D7 | Classes da biblioteca (`Object`, `String`, `StringBuilder`, `StringBuffer`, `System`, `PrintStream`, `Throwable` e exceções padrão) são **classes nativas** registradas em C, com uma tabela única `{classe, método, descritor, função C}`. Nenhum `.class` da biblioteca é lido, nem o `Object.class`. | Respeita R2 e evita depender de um JDK em tempo de execução. |
| D8 | **Sem *garbage collector*.** Toda alocação do heap entra em uma lista e é liberada uma única vez ao final da execução. | Programas de teste são pequenos; GC não é avaliado. |
| D9 | **Escopo = somente o que está na lista de avaliação (seção 1.2).** Ficam **fora**: threads (`Thread`, `synchronized`/monitores com efeito), JIT/compilação, *garbage collector*, verificador de bytecode (confiamos no `javac`), *class loaders* customizados, `.jar`, `invokedynamic`/lambdas, métodos `default` de interface, reflexão, JNI, `Math`, `Integer`/*wrappers*/*autoboxing*, `Scanner`/`System.in`, `String.format`/`printf`, finalizadores, *hashCode* idêntico ao da JVM real. O leitor/exibidor reconhece `invokedynamic`; o interpretador reporta "não suportado". Natives existem apenas na medida exigida por strings, impressão e exceções. **(Confirmado.)** | Concatenação de strings no Java 8 ainda usa `StringBuilder` (indy só no Java 9+), logo não é afetada. |
| D10 | `jsr`/`ret`/`jsr_w` **devem** ser implementados (são ~10 linhas). | Verificado no setup: o `javac` 8 com `-target 1.2` **não** gera `jsr/ret` (inlina o `finally`), mas compiladores antigos (≤ 1.4.1) geram. Como não sabemos como os testes do professor são gerados (D13), a implementação é mantida. Diferença observada: com `-target 1.2` a concatenação usa `StringBuffer`; com 1.6/1.8 usa `StringBuilder`. |
| D11 | Framework de testes unitários **próprio e mínimo** em C (macros `ASSERT_*`), sem dependências. | Simples de compilar com GCC em qualquer SO. |
| D12 | **Formato de saída do exibidor similar ao jclasslib** (requisito R9): mesmas seções e rótulos, `cp_info #n <constante>` em toda referência ao pool, bytecode no formato `pc mnemônico #n <constante>`. O `javap` continua sendo a referência independente de **conteúdo** (`make test-viewer`). **(Revisada:** inicialmente o formato era livre e inspirado no `javap`.) | Critério de avaliação recebido depois da 1ª versão do exibidor. |
| D13 | Como **não se sabe como os testes de "Java 2" serão gerados**, o leitor e o interpretador aceitam **qualquer major de 45 a 52**, e o corpus de testes de Java 2 é gerado de duas formas: `javac` 8 com `-target 1.2` e, se possível, um JDK antigo (1.3/1.4). | Cobre as diferenças de geração de código entre compiladores (ex.: `jsr/ret`, `StringBuffer`). |
| D14 | Sem cronograma com datas; a ordem é dada pelas dependências (seção 5) e pelos checkpoints. **(Confirmado.)** | Informado pelo usuário. |
| D15 | **Fuzzing:** `make fuzz` (60 s) no dia a dia, depois de mudanças; sessões de **10 minutos** (`make fuzz-checkpoint`) em cada checkpoint, registradas em `plans/relatorios/`. **(Confirmado.)** | A cobertura praticamente estabiliza nos primeiros minutos; sessões mais longas trazem pouco ganho. |
| D16 | **Tudo é compilado com `-m32`** (GCC, Clang do fuzzing, testes) e o `jvm.exe` com `i686-w64-mingw32-gcc`. Um header comum verifica em tempo de compilação que `sizeof(void *) == 4`. | Requisito R10: a JVM é uma máquina de 32 bits (slot, referência e `returnAddress` de 32 bits). |
| D17 | **Simplicidade acima de completude** (princípio do topo do documento). Consequências: limite de ~300 linhas por arquivo; cada arquivo tem **uma responsabilidade** — arquivos com a mesma responsabilidade são agrupados (até ~300 linhas), mas um arquivo não é juntado a outro só por ser pequeno; menos headers internos; sem otimizações (caches, tabelas *hash*, *inline caching*) enquanto não houver necessidade medida. | Projeto acadêmico: o código precisa ser fácil de ler, explicar e corrigir pela equipe. |
| D18 | **Despacho do interpretador por tabela de ponteiros de função** indexada pelo opcode (`static const OpcodeHandler handlers[256]`, com `typedef void (*OpcodeHandler)(Frame *frame)`). Cada instrução é uma função curta (`op_iadd`, `op_getfield`...), agrupada em arquivos `exec_*.c` por família; as posições sem instrução apontam para `op_unimplemented` (erro com nome e *offset*). **(Decidido pela equipe.)** | O `switch` foi descartado por ser mais lento; a tabela faz o despacho em uma indexação, e cada função fica pequena, testável e documentada individualmente no Doxygen. |
| D19 | **Resolução simbólica sem cache:** a cada `getfield`/`invoke*`/`new`, o nome da classe, do campo ou do método é buscado no pool e procurado por comparação de *strings* (lista linear de classes carregadas; busca do método subindo a hierarquia). Sem *vtable*/*itable*. | Programas pequenos; elimina toda a fase de *linking* e a invalidação de caches. |
| D20 | **Ponto flutuante com SSE2 em 32 bits** (`-msse2 -mfpmath=sse` junto com `-m32`). | Em 32 bits o GCC usa a FPU x87 por padrão, que calcula com 80 bits e quebra o arredondamento IEEE 754 de `float`/`double` exigido pelo Java (observado na migração: `2.718281828` lido do pool diferente do literal). |
| D21 | **Campos por nome com 2 slots:** campos de instância e estáticos são `Field {nome, descritor, Slot valor[2]}`; categoria 1 usa `valor[0]`, `long`/`double` os 2 slots; busca por nome + descritor. **(Decidido pela equipe, 2026-11-24.)** | Elimina o cálculo de *layout*/*offsets* por classe; tudo continua em slots de 32 bits (D6); coerente com a busca linear de D19. Custo: 4 bytes a mais por campo de categoria 1. |
| D22 | **`String` com `uint16_t[]` direto** (`uint16_t *chars` + `length`), sem um `Array` interno. **(Decidido pela equipe.)** | Uma alocação e um nível de indireção a menos; `mutf8_to_utf16` já produz esse formato. |
| D23 | **Exceções nativas por tabela** `{nome, superclasse}`: mesmo layout (campo de mensagem) e mesmos natives (`<init>()`, `<init>(String)`, `getMessage()`) para todas. **(Decidido pela equipe.)** | Nenhum código específico por classe de exceção; adicionar uma exceção = adicionar uma linha. |
| D24 | **Análise estática com cppcheck:** `make cppcheck` roda `cppcheck --std=c99 --platform=unix32 --enable=warning,style,performance,portability --inline-suppr --suppress=missingIncludeSystem --error-exitcode=1 -Iinclude src fuzz` (e `tests/unit` com `-Itests/unit`). Supressões só *inline* (`// cppcheck-suppress <id>`) e com comentário justificando. | Exigido pelo enunciado (R11). `--platform=unix32` analisa com os tamanhos de tipo de 32 bits (ILP32, como `-m32`, R10); `--error-exitcode=1` faz o alvo falhar como o `-Werror`; complementa o `-Wall -Wextra` (estático) e os sanitizers/libFuzzer (dinâmicos). |
| D25 | **Etiqueta por slot** (`SlotTag`: valor, referência, `returnAddress`) na pilha e nas locais. Só um slot `TAG_REF` vira ponteiro; `ret` exige `TAG_RETADDR`; argumentos de tipo referência e retornos de `areturn` são conferidos com o descritor. **(Etapa 5, 2026-11-24.)** | É a verificação defensiva mínima de I-10: sem ela, um `.class` inválido (ou o fuzzer) transforma um `int` em ponteiro. Não é um verificador de bytecode (D9): não há análise de fluxo, só a etiqueta copiada junto com o valor. |
| D26 | **Uma chamada Java = uma chamada C** (`interpreter_call()` recursivo). Exceção não tratada faz a chamada retornar `false` e o chamador procura o seu tratador. Profundidade máxima de 1024 frames → `StackOverflowError`. Erro fatal (`vm_fatal`) para a execução inteira. **(Etapa 5.)** | Nativos (ex.: `println(Object)` → `toString()` reescrito) chamam métodos Java pela mesma função; não há pilha de frames separada para manter. |
| D27 | **Campo do objeto pela declaração:** a declaração é achada por nome + descritor subindo a hierarquia a partir da classe do `Fieldref` (D21); no objeto, o campo certo é o que tem o **mesmo ponteiro de nome** do modelo. **(Etapa 5.)** | Diferencia campos homônimos de superclasse e subclasse (`super.value`) sem calcular *offsets*. |
| D28 | **Códigos de saída do interpretador:** `3` classe não encontrada/`.class` inválido, `4` exceção não capturada (`Exception in thread "main" ...` em `stderr`), `5` erro fatal da VM. **(Etapa 5.)** | O `java` real devolve 1, mas `1` já é "argumentos inválidos"; os testes só comparam zero × não zero. |
| D29 | **Heap limitado a 256 MiB** (`HEAP_LIMIT`): passar disso é erro fatal "memória insuficiente". **(Etapa 5.)** | Sem GC (D8), um laço que aloca ou o fuzzer esgotariam a memória da máquina. |
| D30 | **(Aprovada pela equipe em 2026-11-24; ajusta D18.)** A tabela `handlers[256]` é `static` (não `const`) e montada na primeira execução pelas funções `exec_*_register()`; as funções `op_*` são `static` em cada `exec_*.c`; famílias em que o opcode codifica tipo/índice/operação (`iload_<n>`, `iconst_<i>`, `iadd`…`ixor`, `i2l`…`i2s`, `dup*`) são uma função cada. | Com `static const` todas as ~200 funções precisariam de protótipo documentado em um header (>300 linhas, D17); o despacho continua sendo uma indexação. O `switch` interno de algumas famílias (aritmética, conversões) custa um desvio, irrelevante para os programas avaliados; cada família documenta no `@details` os opcodes que cobre. Revisar se a avaliação exigir uma página Doxygen por instrução. |
| D31 | **(Aprovada pela equipe em 2026-11-24; ajusta D7.)** Os nativos ficam em 4 tabelas `{classe, método, descritor, função, estático}`, uma por arquivo (`native_object`, `native_string`, `native_text`, `native_print`), percorridas em sequência por `natives_find()`. | Mantém as funções nativas `static` e cada arquivo com ~300 linhas; uma tabela única exigiria exportar e documentar todas as funções em um header. |

---

## 3. Arquitetura proposta

### 3.1 Estrutura de diretórios

```
software-basico-unb/
├── Makefile                # build, debug, test, fuzz, docs, windows, clean (tudo com -m32)
├── Doxyfile                # configuração do Doxygen
├── README.md               # como compilar/usar
├── docs/                   # documentação Doxygen
│   ├── mainpage.md         # página principal (fonte)
│   ├── estruturas.md       # página das principais estruturas de dados (fonte)
│   ├── GUIA_DO_PROJETO.md  # guia de estudo (não entra no Doxygen)
│   └── html/               # documentação gerada por `make docs`
├── plans/
│   ├── PLANO.md            # este documento
│   └── relatorios/         # relatórios (ex.: sessões de fuzzing, simplificação)
├── include/                # headers públicos (.h)
├── src/
│   ├── main.c              # entrada + parsing de flags
│   ├── cli/                # argumentos de linha de comando
│   ├── common/             # tipos, utilitários de arquivos/strings/números
│   ├── classfile/          # leitor: byte_reader, class_file, constant_pool, attributes
│   ├── bytecode/           # opcodes (tabela de mnemônicos) e instruction (tamanho das instruções)
│   ├── viewer/             # exibidor: format, printer, disassembler
│   └── vm/                 # interpretador (seção 3.2.1)
├── tests/
│   ├── unit/               # testes unitários em C
│   ├── java/               # programas .java de teste, organizados por tema
│   ├── expected/           # saídas esperadas (geradas pelo java real)
│   └── run_tests.sh        # script de testes de integração (+ .ps1 para Windows)
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
                                                   │              ├─► natives (Object, String, PrintStream, ...)
                                                   │              └─► exceções (tabela de exceções/unwind)
```

#### 3.2.1 Interpretador — desenho mínimo

| Arquivo (`src/vm/`) | Conteúdo | Estruturas |
|---------------------|----------|------------|
| `runtime.h` (em `include/vm/`) | Tipos de execução | `Slot` (`uint32_t`), `Field`, `Frame`, `RuntimeClass`, `Object`, `Array`, `String` |
| `heap.c` | Alocação de objetos, arrays e strings; lista de alocações liberada no fim (D8) | — |
| `class_loader.c`, `class_hierarchy.c` | Busca `<dir>/pkg/Classe.class`, carrega superclasse, monta campos estáticos; `<clinit>` na 1ª utilização ativa, busca de métodos/campos e compatibilidade de tipos; lista linear de classes (D19) | `RuntimeClass`, `MethodRef` |
| `frame.c` | Criação de frame (locais + pilha com `max_locals`/`max_stack` do `Code`), *push*/*pop* cat. 1 e cat. 2 | `Frame` |
| `interpreter.c`, `vm.c`, `descriptor.c` | Laço *fetch–decode–execute* com a tabela de ponteiros de função (D18, D30); invocação de métodos (D26); desempilhamento de exceções; estado da VM, erros fatais e `main`; leitura de descritores | `OpcodeHandler`, `Vm` |
| `exec_*.c` | Funções `op_*`, um arquivo por família: `exec_load_store`, `exec_stack`, `exec_int`, `exec_float` (ponto flutuante, conversões, comparações), `exec_branch`, `exec_object`, `exec_invoke`, `exec_array` (D30) | — |
| `natives.c`, `native_*.c` | Tabela de classes nativas e busca de métodos (D7, D23, D31); `native_object` (`Object`, `Throwable`), `native_string` (`String`, `StringBuilder`/`StringBuffer`), `native_text` (conversão para texto, `valueOf`, `append`), `native_print` (`System.out`/`PrintStream`) | `NativeEntry` |

Regras do desenho:

- **Uma única pilha de frames** (monothread, D9); profundidade máxima fixa → erro "StackOverflow" simples.
- **`Object`**: ponteiro para a `RuntimeClass` + vetor de `Field` `{nome, descritor, Slot valor[2]}` com os campos da classe e das superclasses; categoria 1 usa `valor[0]`, `long`/`double` usam os 2 slots. Campo localizado por nome + descritor, sem cálculo de *offsets* (D21). Estáticos usam o mesmo `Field`, guardados na `RuntimeClass`.
- **`Array`**: tipo do elemento, tamanho e dados contíguos; array multidimensional = array de referências.
- **`String`**: objeto da classe nativa `java/lang/String` que guarda direto `uint16_t *chars` + `length` (UTF-16, via `mutf8_to_utf16`), sem `Array` interno (D22); literais do pool são internados em uma lista.
- **Exceção**: objeto comum de uma classe que herda de `java/lang/Throwable` (nativa) com campo de mensagem; as exceções nativas são linhas de uma tabela `{nome, superclasse}` com o mesmo layout e os mesmos natives (D23); a JVM lança as suas (`ArithmeticException`, `NullPointerException`...) criando esses objetos.

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
- Arquivos com **cerca de 300 linhas no máximo** (D17) e **uma responsabilidade por arquivo**. Arquivos com a mesma responsabilidade ficam juntos até esse limite; arquivos de responsabilidades diferentes ficam separados mesmo que sejam pequenos. Headers internos (`attr_parsers.h`, `printer_internal.h`) só existem se a divisão for inevitável. A tabela de arquivos por módulo está na página principal da documentação (`docs/mainpage.md`).
- Preferir a solução mais direta: sem macros elaboradas, sem otimizações prematuras, sem generalizações "para o futuro".
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
> compila com `gcc -m32 -std=c99 -Wall -Wextra -pedantic` **sem warnings**; `make cppcheck` sem avisos (R11); comentários Doxygen completos em pt-BR seguindo o modelo 3.4; `make docs` sem warnings; testes da atividade passando; solução revisada quanto à simplicidade (D17); commit/PR revisado por outro membro.

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
- **Situação:** ✅ concluída — MinGW-w64 GCC 13; `build/jvm.exe` executa no Windows via interoperabilidade do WSL. (A troca para 32 bits é feita em S-08/M-01.)

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
- **Pronto quando:** `make docs` gera `docs/html/index.html` em português, com página da função contendo objetivo, parâmetros, algoritmo, **grafo de chamadas e de chamadoras**.
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
- **Tarefas:** mini-framework de testes unitários (`tests/unit/test.h`); script `tests/run_tests.sh` que: compila cada `tests/java/**/*.java` nas 3 versões → executa no `java` real para gerar `expected/` → executa na nossa JVM → compara com `diff`; versão `.ps1` para Windows.
- **Pronto quando:** um teste unitário trivial e um teste de integração trivial (que falha de propósito, depois passa) são reportados corretamente.
- **Situação:** ✅ concluída — `make test` (unitários); `make test-integration` falha com a nossa JVM (esperado, interpretador inexistente) e passa com `JVM_CMD=java`; `tests/run_tests.ps1` validado no PowerShell do Windows.

#### S-08 · Toolchain de 32 bits (`-m32`)
- **Objetivo:** permitir compilar, testar e fuzzar o projeto em 32 bits (R10).
- **Requisitos:** S-01…S-04.
- **Tarefas:**
  1. instalar `gcc-multilib` (traz `libc6-dev-i386`, `lib32gcc` e `lib32asan`/`lib32ubsan` para o `make debug`) e `g++-multilib` (`libstdc++` de 32 bits, exigida pelo runtime do libFuzzer);
  2. conferir que o MinGW de 32 bits (`i686-w64-mingw32-gcc`, já incluso no pacote `mingw-w64`) gera um `hello.exe` que roda no Windows;
  3. conferir que `clang -m32 -fsanitize=fuzzer,address,undefined` liga (o runtime `libclang_rt.fuzzer-i386.a` já está instalado; faltam só as bibliotecas de 32 bits da libc);
  4. adicionar as ferramentas de 32 bits ao `tools/check_env.sh`.
- **Pronto quando:** um `hello.c` que imprime `sizeof(void *)` mostra `4` compilado com `gcc -m32`, com `gcc -m32 -fsanitize=address,undefined`, com `clang -m32 -fsanitize=fuzzer` e com `i686-w64-mingw32-gcc` (este último executado no Windows); `make fuzz-smoke` funciona em `-m32`.
- **Situação:** ⏳ parcial (2026-11-24) — `gcc-multilib` instalado: `gcc -m32` e `gcc -m32 -fsanitize=address,undefined` OK; `i686-w64-mingw32-gcc` OK. Falta `g++-multilib`: `clang -m32 -fsanitize=fuzzer` falha com `cannot find -lstdc++`.

#### S-09 · Verificador estático (cppcheck)
- **Objetivo:** disponibilizar o cppcheck (R11).
- **Requisitos:** S-01.
- **Tarefas:**
  1. instalar (`sudo apt install -y cppcheck`) e registrar no `README.md` (pré-requisitos e instalação);
  2. adicionar o cppcheck ao `tools/check_env.sh`;
  3. criar o alvo `make cppcheck` com as opções de D24 e citá-lo na tabela de comandos do `README.md`.
- **Pronto quando:** `make check-env` mostra o cppcheck; `make cppcheck` executa sobre `src/`, `fuzz/` e `tests/unit/`.
- **Situação:** ⏳ pendente — em 2026-11-24 o cppcheck não está instalado.

> ### ✅ CHECKPOINT C1 — Ambiente pronto (**atingido**; S-08 entra no C3)
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
- **Situação:** ✅ concluída — `classfile/byte_reader.c`, `common/file_io.c`; suíte `byte_reader`.

#### L-02 · Estruturas de dados do ClassFile
- **Objetivo:** modelar em C todas as estruturas da especificação (JVMS §4).
- **Requisitos:** L-01.
- **Tarefas:** definir `ClassFile`, `CpInfo` (união por *tag*), `FieldInfo`, `MethodInfo`, `AttributeInfo` (união por tipo), `ExceptionTableEntry` etc.; documentar cada campo.
- **Pronto quando:** headers compilam; aparecem na página "Estruturas de dados" do Doxygen com descrição e diagrama.
- **Situação:** ✅ concluída — `include/classfile/class_file.h` (+ `cp_info.h` e `attribute_info.h`); página `docs/estruturas.md` atualizada.

#### L-03 · Cabeçalho e validação
- **Objetivo:** ler `magic`, `minor_version`, `major_version`.
- **Requisitos:** L-02.
- **Tarefas:** validar `0xCAFEBABE`; aceitar *major* 45–52 (mapear para "Java 1.1 … Java 8"); rejeitar demais com mensagem clara.
- **Pronto quando:** testes com arquivos válidos (46/50/52), magic inválido e versão não suportada.
- **Situação:** ✅ concluída — majors 43–54 testados (45–52 aceitos).

#### L-04 · Pool de constantes
- **Objetivo:** ler o *constant pool* completo.
- **Requisitos:** L-03.
- **Tarefas:** tratar todas as *tags* até o Java 8: `Utf8`, `Integer`, `Float`, `Long`, `Double`, `Class`, `String`, `Fieldref`, `Methodref`, `InterfaceMethodref`, `NameAndType`, `MethodHandle`, `MethodType`, `InvokeDynamic`; `Long`/`Double` ocupam **duas entradas**; validar índices; decodificar **UTF-8 modificado** (MUTF-8); funções auxiliares para resolver nomes (`cp_get_utf8`, `cp_get_class_name`…).
- **Pronto quando:** teste com classe contendo todos os tipos de constante (incl. `long`/`double`) lida corretamente; índices inválidos geram erro, não *crash*.
- **Situação:** ✅ concluída — `classfile/constant_pool.c`, `cp_validate.c`, `cp_query.c` + `common/mutf8.c`; pool idêntico ao do `javap -v` nas 26 classes de fixture (Java 2, 6 e 8), incluindo NUL e *surrogates* em MUTF-8.

#### L-05 · Flags, this/super, interfaces
- **Objetivo:** ler `access_flags`, `this_class`, `super_class`, `interfaces[]`.
- **Requisitos:** L-04.
- **Pronto quando:** testes com classe que estende outra e implementa 2 interfaces.
- **Situação:** ✅ concluída — fixture `Hierarchy`.

#### L-06 · Campos e métodos
- **Objetivo:** ler `fields[]` e `methods[]`.
- **Requisitos:** L-05.
- **Pronto quando:** testes com campos estáticos/instância e métodos estáticos/instância/construtores.
- **Situação:** ✅ concluída — fixture `AllConstants`.

#### L-07 · Atributos
- **Objetivo:** interpretar os atributos relevantes.
- **Requisitos:** L-06.
- **Tarefas:** `Code` (com `exception_table` e atributos aninhados), `ConstantValue`, `Exceptions`, `SourceFile`, `LineNumberTable`, `LocalVariableTable`, `InnerClasses`, `Synthetic`, `Deprecated`, `Signature`, `StackMapTable`, `BootstrapMethods`, `EnclosingMethod`; atributos desconhecidos guardados como bytes brutos.
- **Pronto quando:** teste lê atributos de classes Java 2, 6 e 8 (incl. `StackMapTable` no Java 8); atributos desconhecidos não quebram a leitura.
- **Situação:** ✅ concluída — `classfile/attributes.c` + `attr_*.c`; todos os atributos listados, com validação de índices e de `attribute_length`; fixtures `Flow` e `Lambdas`. O exibidor mostra o `StackMapTable` campo a campo, como o jclasslib (confirmado pela equipe).

#### L-08 · Liberação de memória e robustez
- **Objetivo:** leitor sem vazamentos e resistente a entradas inválidas.
- **Requisitos:** L-07.
- **Tarefas:** `class_file_free`; caminhos de erro liberam tudo; executar com ASan/UBSan (`make debug`).
- **Pronto quando:** ASan/LeakSanitizer sem erros em todo o corpus de testes.
- **Situação:** ✅ concluída — `make MODE=debug test` limpo; todo prefixo de um `.class` real gera `CF_ERR_TRUNCATED` sem vazamento; testes também passam no Windows (`build/unit_tests.exe`).

#### L-09 · Fuzzing do leitor
- **Objetivo:** analisar dinamicamente o leitor com libFuzzer.
- **Requisitos:** L-08, S-03.
- **Tarefas:** `fuzz/fuzz_reader.c` (lê a partir de buffer em memória); corpus inicial com os `.class` de teste; rodar ≥ 10 min (`make fuzz-checkpoint`); corrigir *crashes* e adicionar cada um como teste de regressão.
- **Pronto quando:** sessão de fuzzing de 10 min sem *crash*/vazamento; relatório curto (cobertura, bugs encontrados) registrado em `plans/relatorios/`.
- **Situação:** ✅ concluída — 34,7 milhões de execuções em ~38 min, sem *crash*/vazamento/UB; relatório em `plans/relatorios/fuzzing-leitor-exibidor.md`.

---

### ETAPA 3 — Exibidor

#### E-01 · Tabela de opcodes
- **Objetivo:** mapear todos os opcodes (0x00–0xC9) para mnemônico e tamanho de operandos.
- **Requisitos:** L-02.
- **Tarefas:** tabela estática `{opcode, mnemonic, operand_bytes, operand_kind}`; casos especiais `tableswitch`/`lookupswitch` (alinhamento de 4 bytes), `wide`, `invokeinterface`, `multianewarray`.
- **Pronto quando:** teste unitário confere todos os 202 opcodes com a especificação.
- **Situação:** ✅ concluída — `bytecode/opcodes.c` + `bytecode/instruction.c` (compartilhados com o interpretador), incluindo `bytecode_instruction_length()` com alinhamento de `switch` e `wide`; suíte `opcodes`.

#### E-02 · Informações gerais
- **Objetivo:** exibir, como na aba *General Information* do jclasslib, minor/major (com nome da versão Java), tamanho do pool, flags decodificadas, this/super como `cp_info #n <constante>` e as contagens.
- **Requisitos:** L-07.
- **Pronto quando:** saída conferida com `javap -v` para 3 classes (uma de cada versão) e no formato do jclasslib (R9).
- **Situação:** ✅ concluída — `viewer/printer.c` (seção `General Information`).

#### E-03 · Pool de constantes
- **Objetivo:** exibir cada entrada como no jclasslib: índice do slot e estrutura (`[7] CONSTANT_Methodref_info`) e seus campos, cada referência como `cp_info #n <constante>` (ex.: `Class name: cp_info #2 <java/io/PrintStream>`, `Name and type: cp_info #15 <println : (I)V>`); Utf8 com tamanho em bytes e em caracteres; numéricos com bytes brutos e valor; segunda posição de Long/Double como `(large numeric continued)`.
- **Requisitos:** E-02.
- **Pronto quando:** comparação com `javap -v` sem divergências de conteúdo (índices, tipos, referências e valores).
- **Situação:** ✅ concluída — `viewer/printer.c` (+ `print_*.c`) + `viewer/format.c`; valores numéricos no formato do Java via `common/number_format.c`.

#### E-04 · Interfaces, campos e métodos
- **Objetivo:** exibir nome e descritor (`cp_info #n <constante>`), flags (`0x0009 [public static]`) e atributos de cada item; cada atributo com *Attribute name index*, *Attribute length* e o conteúdo específico, como no jclasslib.
- **Requisitos:** E-03.
- **Pronto quando:** idem E-03.
- **Situação:** ✅ concluída — flags decodificadas por contexto.

#### E-05 · Atributos e bytecode (disassembler)
- **Objetivo:** exibir atributos e o código com mnemônicos.
- **Requisitos:** E-01, E-04.
- **Tarefas:** para cada instrução, no formato do jclasslib: *offset*, mnemônico e argumentos — os que apontam para o pool como `#n <constante>` com o valor resolvido (ex.: `invokespecial #1 <java/lang/Object.<init> : ()V>`, `ldc #3 <texto>`, `new #2 <Classe>`); desvios como `alvo (+deslocamento)`; `iinc i by c`; `newarray 10 (int)`; casos de `switch`; `wide`. Tabela de exceções (`Nr. Start PC End PC Handler PC Catch Type`), `LineNumberTable`, `LocalVariableTable`, `StackMapTable` etc.
- **Pronto quando:** *disassembly* de todo o corpus confere com `javap -c -v` (pc, mnemônico, argumentos e **valor resolvido** de cada argumento do pool).
- **Situação:** ✅ concluída — `viewer/disassembler.c`; `make test-viewer` (`tools/compare_javap.sh`) confere pool, bytecode e valores resolvidos das 29 classes de fixture sem divergências; 167 opcodes distintos nos fixtures e os demais (`jsr`, `ret`, `goto_w`, `wide`...) em testes com bytecode montado à mão.

#### E-06 · Integração com a CLI
- **Objetivo:** modo leitor/exibidor acessível pelas flags.
- **Requisitos:** E-05.
- **Tarefas:** `--mode viewer`, `--classpath`, `--output`; mensagens de erro claras.
- **Pronto quando:** funciona no Linux **e** no `jvm.exe` no Windows para os corpora Java 2, 6 e 8.
- **Situação:** ✅ concluída — `cli/cli.c` + `main.c`; saída do `jvm.exe` idêntica à do Linux nas 29 classes; console do Windows configurado para UTF-8.

> ### ✅ CHECKPOINT C2 — Leitor/Exibidor (1ª entrega)
> - [x] Lê corretamente `.class` de Java 2, 6 e 8
> - [x] Exibe todas as estruturas e o bytecode com mnemônicos
> - [x] Saída similar ao jclasslib: índices do pool com a constante representada e argumentos do bytecode com o valor da constante (R9)
> - [x] Saída conferida contra `javap` em todo o corpus
> - [x] Fuzzing do leitor sem *crashes*; ASan sem vazamentos
> - [x] Funciona no Windows com as flags
> - [x] Documentação Doxygen do leitor/exibidor completa e sem warnings
> - [ ] *Tag* git `v1.0-leitor-exibidor`

---

### ETAPA 4 — Simplificação e migração para 32 bits

> Objetivo da etapa: deixar o que já existe **menor, mais simples e em 32 bits** antes de começar o interpretador, **sem mudar o comportamento** do leitor/exibidor. A rede de segurança é a suíte atual: `make check`, `make test-viewer`, `make test-windows`, `make fuzz` e `make docs` precisam continuar verdes depois de cada atividade.

#### M-01 · Migração para `-m32`
- **Objetivo:** cumprir R10 em todos os alvos.
- **Requisitos:** S-08, C2.
- **Tarefas:**
  1. `Makefile`: `-m32` em `CFLAGS` e `LDFLAGS` (release, debug e testes) e em `FUZZ_FLAGS`; `MINGW := i686-w64-mingw32-gcc`;
  2. header comum (ex.: `include/common/jvm_types.h`) com a verificação de ponteiro de 4 bytes em C99 (`typedef char check_ptr_size[sizeof(void *) == 4 ? 1 : -1];`) — incluído por `main.c` e, depois, pelo `vm`;
  3. revisar avisos e suposições de 64 bits (`size_t` de 32 bits nos cálculos de tamanho, formatos `%zu`/`PRIu32`, conversões ponteiro ↔ inteiro);
  4. atualizar `tools/check_env.sh`, `README.md` (pacotes `gcc-multilib`) e o `GUIA_DO_PROJETO.md`.
- **Pronto quando:** `file build/jvm` mostra *ELF 32-bit*; `file build/jvm.exe` mostra *PE32*; `make check`, `make MODE=debug test`, `make test-viewer`, `make test-windows` e `make fuzz` passam; saída do exibidor idêntica à de antes da migração em todos os fixtures.

#### M-02 · Reorganização por responsabilidade
- **Objetivo:** cada arquivo com uma única responsabilidade e no máximo ~300 linhas (D17). O critério é a **responsabilidade**, não o tamanho: arquivos só são agrupados quando cuidam da mesma coisa; um arquivo pequeno com responsabilidade própria continua separado.
- **Requisitos:** M-01.
- **Tarefas:**
  1. para cada arquivo de `src/` e `include/`, registrar em `plans/relatorios/simplificacao.md` a sua responsabilidade (1 frase) e o tamanho;
  2. agrupar os arquivos de mesma responsabilidade enquanto o resultado couber em ~300 linhas; dividir por responsabilidade os que passam do limite;
  3. eliminar headers internos que deixarem de ser necessários;
  4. atualizar a tabela de arquivos em `docs/mainpage.md` e o mapa do repositório no guia.
- **Análise inicial** (a confirmar na execução; tamanhos de 2026-11-10):

  | Arquivo(s) | Responsabilidade | Proposta |
  |------------|------------------|----------|
  | `attributes.c` (204) + `attr_common.c` (39) | Leitura genérica de atributos: cabeçalho, sub-leitor, despacho por tipo e as validações/alocações usadas por todos os leitores de atributo | **Agrupar** (~243) |
  | `attr_simple.c` (74) + `attr_tables.c` (181) | Leitura dos atributos informativos (`ConstantValue`, `Exceptions`, `SourceFile`, `Signature`, `EnclosingMethod`, `InnerClasses`, `LineNumberTable`, `LocalVariableTable`, `BootstrapMethods`, desconhecidos). A divisão atual é por formato (tamanho fixo × tabela), não por responsabilidade | **Agrupar** (~255) |
  | `attr_code.c` (72) | Leitura do atributo `Code` (corpo do método) | Manter |
  | `attr_stack_map.c` (177) | Leitura do `StackMapTable` | Manter |
  | `attr_free.c` (74) | Liberação de todos os atributos | Manter |
  | `cf_error.c` (40) | Registro e nomes dos erros do leitor | Manter |
  | `constant_pool.c` (188), `cp_validate.c` (128), `cp_query.c` (149) | Leitura/liberação, validação e consulta do pool — três responsabilidades | Manter |
  | `class_file.c` (300), `byte_reader.c` (81) | Estrutura raiz; leitura de bytes | Manter (`class_file.c` no limite) |
  | `format.c` (306) | **Duas** responsabilidades: formatação geral (flags, indentação, MUTF-8 na tela) e representação textual das constantes (`print_constant`, `print_cp_ref`, `constant_info_name`, ~150 linhas) | **Dividir**: a representação das constantes vai para `print_constant_pool.c` (pool + `<constante>` = mesma responsabilidade, ~255) |
  | `printer.c` (162) | Seções gerais do exibidor **e** auxiliares de linha (`print_label`, `print_ref_line`, `print_flags_line`, `print_section`, ~50 linhas) | Mover os auxiliares de linha para `format.c` (formatação geral, ~210) |
  | `print_attributes.c` (239), `print_stack_map.c` (102), `disassembler.c` (208) | Exibição dos atributos, do `StackMapTable` e do bytecode | Manter |
  | `opcodes.c` (243), `instruction.c` (118) | Tabela de opcodes; decodificação de instruções | Manter (juntos passariam de 300) |
  | `version.c` (17), `file_io.c` (44), `mutf8.c` (72), `number_format.c` (165), `cli.c` (190) | Responsabilidades próprias | Manter |
  | Headers de estruturas (`class_file.h`, `cp_info.h`, `attribute_info.h`) | Estruturas do `.class` | Manter (juntos passariam de 300) |

- **Pronto quando:** todo arquivo tem uma responsabilidade descrita no `@file`/`@brief`; nenhum arquivo acima de ~300 linhas; comportamento idêntico (mesma suíte verde); tabela de responsabilidades antes/depois registrada em `plans/relatorios/simplificacao.md`.

#### M-03 · Revisão de simplicidade do código existente
- **Objetivo:** remover complexidade que não paga o próprio custo.
- **Requisitos:** M-02.
- **Tarefas:** revisar cada módulo procurando: funções que só repassam chamadas, parâmetros nunca usados, enumerações/casos que nunca acontecem, validações duplicadas (ex.: o mesmo índice validado em `cp_validate` e de novo no uso), comentários desatualizados; substituir construções elaboradas pela forma direta. Ficam como estão, por serem exigidos: decodificação campo a campo de todos os atributos (inclusive `StackMapTable`, R9), `number_format.c` (imprimir `float`/`double` como o Java) e `mutf8.c` (`String`).
- **Pronto quando:** total de linhas de `src/` + `include/` menor que antes de M-01 (registrado em `plans/relatorios/simplificacao.md`), suíte inteira verde, `make docs` sem avisos.

#### M-04 · Documentação e fuzzing após a simplificação
- **Objetivo:** fechar a etapa com tudo coerente.
- **Requisitos:** M-01…M-03.
- **Tarefas:** atualizar `README.md`, `docs/mainpage.md`, `docs/estruturas.md` e `docs/GUIA_DO_PROJETO.md`; sessão `make fuzz-checkpoint` em 32 bits registrada em `plans/relatorios/`.
- **Pronto quando:** documentos sem referência a arquivos removidos, `make docs` sem avisos e fuzzing de 10 min sem *crash*.

#### M-05 · Análise estática do código existente
- **Objetivo:** zerar os avisos do cppcheck no leitor/exibidor antes do interpretador (R11).
- **Requisitos:** S-09, M-03.
- **Tarefas:** rodar `make cppcheck`; corrigir cada aviso (preferindo a correção à supressão); suprimir *inline* só falsos positivos, com justificativa; registrar avisos encontrados e o que foi feito em `plans/relatorios/cppcheck.md`.
- **Pronto quando:** `make cppcheck` sem avisos; suíte inteira verde; saída do exibidor idêntica nos fixtures.

> ### ✅ CHECKPOINT C3 — Base simplificada em 32 bits
> - [x] Todo o projeto compila com `-m32` (Linux) e `i686-w64-mingw32-gcc` (Windows), sem warnings
> - [x] Suíte completa verde (`make check`, `make MODE=debug test`, `make test-viewer`, `make test-windows`) — exceto `test-integration`, que depende do interpretador (Etapa 5) e já falhava no C2
> - [x] Um arquivo por responsabilidade (≤ ~300 linhas), menos linhas que no C2 (5731 → 5658), comportamento do exibidor inalterado (ver `plans/relatorios/simplificacao.md`)
> - [x] Fuzzing em 32 bits sem *crashes* (`make fuzz-checkpoint` em 2026-11-24, ver `plans/relatorios/fuzzing-interpretador.md`); documentação atualizada ✔
> - [x] `make cppcheck` sem avisos (S-09, M-05, R11; ver `plans/relatorios/cppcheck.md`)
> - [ ] *Tag* git `v1.1-simplificado`

---

### ETAPA 5 — Interpretador (corretude da JVM)

> Implementar **apenas** o que a seção 1.2 pede, com o desenho mínimo da seção 3.2.1 (D6, D7, D8, D9, D18, D19). Nada de threads, JIT, GC, verificador, caches ou *vtables*.

#### I-01 · Estruturas de execução e heap
- **Objetivo:** tipos de tempo de execução e alocação.
- **Requisitos:** C3.
- **Tarefas:** `include/vm/runtime.h` com `Slot` (`uint32_t`), `Field` (`{nome, descritor, Slot valor[2]}`, D21), `Frame` (locais, pilha de operandos, `pc`, método, classe), `RuntimeClass` (`ClassFile` + estáticos em `Field` + superclasse), `Object` (classe + vetor de `Field`), `Array` (tipo do elemento + tamanho + dados com o tamanho real de cada tipo), `String` (`uint16_t *` + tamanho, D22); `frame.c` com *push*/*pop* de cat. 1 e cat. 2 (`long`/`double` em 2 slots, referência = ponteiro de 32 bits); `heap.c` com a lista de alocações liberada no fim (D8).
- **Pronto quando:** testes unitários de *push*/*pop* de `int`, `float`, `long`, `double` e referência; criação/destruição de frames e objetos sem vazamento no ASan.

#### I-02 · Carregador de classes e classes nativas
- **Objetivo:** carregar classes do usuário sob demanda e registrar as nativas.
- **Requisitos:** I-01.
- **Tarefas:** `pkg/Classe` → `<dir>/pkg/Classe.class`; lista linear de classes carregadas (D19); carga recursiva da superclasse; lista dos campos de instância (`Field` por nome, da classe e das superclasses — D21); estáticos com `ConstantValue`; `<clinit>` na primeira utilização ativa (`new`, `getstatic`/`putstatic`, `invokestatic`); `natives.c` com a tabela `{classe, método, descritor, função}` e as classes nativas de D7 (inicialmente só `Object`).
- **Pronto quando:** carrega hierarquia de 3 níveis; `<clinit>` executa uma única vez e na ordem correta (superclasse antes).

#### I-03 · Laço de execução, constantes, loads/stores e pilha
- **Objetivo:** executar o primeiro `main`.
- **Requisitos:** I-02.
- **Tarefas:** localizar `public static void main([Ljava/lang/String;)V`; laço *fetch–decode–execute* com a tabela `handlers[256]` de ponteiros de função (D18); opcode sem função → `op_unimplemented` (erro com nome e *offset*); `nop`, `aconst_null`, `iconst_*`, `lconst_*`, `fconst_*`, `dconst_*`, `bipush`, `sipush`, `ldc`, `ldc_w`, `ldc2_w` (exceto `String`, que vem em I-08); `*load*`, `*store*`, `wide`; `pop`, `pop2`, `dup`, `dup_x1`, `dup_x2`, `dup2`, `dup2_x1`, `dup2_x2`, `swap` nas formas cat. 1 e cat. 2.
- **Pronto quando:** `main` vazio termina com código 0; testes unitários de cada forma de `dup*`/`pop2` passam.

#### I-04 · Aritmética, lógica, conversões e comparações
- **Requisitos:** I-03.
- **Tarefas:** `add/sub/mul/div/rem/neg` (i/l/f/d); `shl/shr/ushr` (máscara 0x1F/0x3F), `and/or/xor`; `iinc`; `i2l…i2s` com as regras do Java (NaN → 0, saturação em `f2i/d2i/f2l/d2l`); `lcmp`, `fcmpl/fcmpg`, `dcmpl/dcmpg`; *wrap-around* sem UB (operar em `uint32_t`/`uint64_t`); `MIN / -1`; `fmod` para `frem/drem`. Divisão inteira por zero lança `ArithmeticException` a partir de I-09 (antes disso, erro fatal).
- **Pronto quando:** testes unitários dos casos-limite (overflow, NaN, infinitos, saturação) passam.

#### I-05 · Controle de fluxo e métodos estáticos
- **Requisitos:** I-04.
- **Tarefas:** `if*`, `if_icmp*`, `if_acmp*`, `ifnull`, `ifnonnull`, `goto`, `goto_w`, `tableswitch`, `lookupswitch`, `jsr`, `ret`, `jsr_w` (D10), `*return`; `invokestatic` com parâmetros copiados do descritor (cat. 2 em 2 slots), valor de retorno e recursão. Para ver a saída dos testes, implementar já aqui o mínimo de `System.out.println(int/long/double...)` (completado em I-08).
- **Pronto quando:** programas Java com laços, `switch` denso/esparso, fatorial/fibonacci recursivos e parâmetros `long`/`double` mistos têm saída idêntica ao `java` real nas 3 versões.

> ### ✅ CHECKPOINT C4a — Núcleo
> - [x] Programas só com `main` + métodos estáticos e todos os tipos primitivos executam corretamente nas 3 versões

#### I-06 · Objetos, atributos, herança e polimorfismo
- **Requisitos:** I-05.
- **Tarefas:** `new`, `getfield`, `putfield`, `getstatic`, `putstatic` (campo procurado na hierarquia); `invokespecial` (construtores, `super.metodo()`, privados); `invokevirtual` e `invokeinterface` com a **mesma** busca: sobe a hierarquia a partir da classe real do objeto procurando nome + descritor (D19); `instanceof`, `checkcast` (subindo superclasses e interfaces); valores padrão de campos; classes abstratas.
- **Pronto quando:** testes de contador estático compartilhado, campos de instância, hierarquia `Animal → Cachorro/Gato` com método reescrito, interface e classe abstrata passam nas 3 versões.

#### I-07 · Arrays
- **Requisitos:** I-06.
- **Tarefas:** `newarray` (8 tipos primitivos), `anewarray`, `multianewarray` (array de arrays), `arraylength`, `*aload`/`*astore` de todos os tipos; verificação de limites e de `null` (exceções reais a partir de I-09).
- **Pronto quando:** testes com arrays de todos os tipos, matriz 3D, array de objetos (`Animal[]` com polimorfismo) e array irregular passam.

> ### ✅ CHECKPOINT C4b — Orientação a objetos
> - [x] Objetos, atributos, herança, polimorfismo, interfaces e arrays corretos

#### I-08 · Strings e impressão
- **Requisitos:** I-07.
- **Tarefas:** `ldc` de `String` cria objeto `String` nativo (literais internados em lista); natives de `String` usados nos testes (`length`, `charAt`, `equals`, `concat`, `substring`, `indexOf`, `toString`, `valueOf`); `StringBuilder` (Java 6/8) e `StringBuffer` (Java 2) com `<init>`, `append` para todos os tipos e `toString`; `System.out` como objeto `PrintStream` nativo; `print`/`println` para `int`, `long`, `float`, `double`, `char`, `boolean`, `String`, `char[]` e `Object` (via `toString`), com `float`/`double` formatados por `common/number_format.c`; natives de `Object` (`<init>`, `equals`, `hashCode`, `toString` no formato `Classe@hash`).
- **Pronto quando:** testes de concatenação com todos os tipos, métodos de string e impressão de todos os tipos (incl. `NaN`, `Infinity`, `1.0E10`) idênticos ao `java` real nas 3 versões.

#### I-09 · Exceções
- **Requisitos:** I-08.
- **Tarefas:** tabela `{nome, superclasse}` das exceções nativas (D23): `Throwable` → `Exception` → `RuntimeException` → `ArithmeticException`, `NullPointerException`, `ArrayIndexOutOfBoundsException`, `NegativeArraySizeException`, `ClassCastException` (e `Error` se necessário), todas com o mesmo campo de mensagem e os mesmos natives `<init>()`, `<init>(String)` e `getMessage()`; `athrow`; busca na `exception_table` (faixa de `pc` + tipo compatível por herança; `catch_type = 0` = `finally`); desempilhamento de frames; exceções do usuário (que estendem as nativas); exceção não capturada imprime `Exception in thread "main" Classe: mensagem` e termina com código ≠ 0.
- **Pronto quando:** testes com `try/catch/finally` aninhados, propagação entre métodos, exceção do usuário e exceções da JVM passam nas 3 versões.

#### I-10 · Instruções restantes e robustez
- **Requisitos:** I-09.
- **Tarefas:** `monitorenter`/`monitorexit` sem efeito (só desempilham; D9); `invokedynamic` → erro "não suportado"; teste que percorre 0x00–0xC9 e confirma que nenhuma posição da tabela `handlers` aponta para `op_unimplemented` por engano; verificações defensivas **mínimas** (índice do pool com tipo errado, pilha fora de `max_stack`, `pc` fora do código, profundidade máxima de frames) tratadas como erro fatal com mensagem; `fuzz/fuzz_interpreter.c` com limite de passos (o fuzzer roda só classes que o leitor aceita).
- **Pronto quando:** todos os opcodes implementados ou explicitamente tratados; 10 min de fuzzing (`make fuzz-checkpoint`) sem *crash* não tratado; suíte limpa em ASan/UBSan.

> ### ✅ CHECKPOINT C4c — Interpretador completo
> - [x] Exceções, strings e impressão idênticos ao `java` real (exceto `float` ≥ 10⁷ afetado pelo defeito do JDK 8, ver riscos)
> - [x] 100% da suíte de integração passa em Java 2, 6 e 8, **no Linux e no Windows** (30 programas × 3 versões após a F-01)
> - [x] Fuzzing e sanitizers limpos — ASan/UBSan em toda a suíte; `make fuzz-checkpoint` (10 min por alvo) achou 1 defeito (`invokestatic` de nativo de instância), corrigido com teste de regressão; nova sessão limpa (ver `plans/relatorios/fuzzing-interpretador.md`)
> - [x] `make cppcheck` sem avisos (R11)
> - [ ] *Tag* git `v2.0-interpretador`

---

### ETAPA 6 — Validação final e documentação

#### F-01 · Bateria de programas de avaliação
- **Objetivo:** cobrir cada item avaliado com ao menos 2 programas.
- **Tarefas:** organizar `tests/java/` por tema: `cat1_cat2/`, `arrays/`, `strings/`, `static_methods/`, `instance_methods/`, `class_fields/`, `instance_fields/`, `inheritance/`, `polymorphism/`, `override/`, `objects/`, `exceptions/`, `print/`; rodar nas 3 versões.
- **Pronto quando:** matriz *tema × versão* 100% verde em Linux e Windows.
- **Situação (2026-11-24):** ✅ 30 programas em 15 temas (os 13 acima, com ≥ 2 programas cada, + `control_flow/` e `basic/`) × Java 2/6/8 = 90 execuções verdes no Linux (`make test-integration`, também em ASan/UBSan), no Windows via WSL (`make test-windows`) e no PowerShell do Windows a partir do pacote (`tests/run_tests.ps1`).

#### F-02 · Documentação Doxygen final
- **Tarefas:** `mainpage.md` (visão geral, arquitetura, como compilar/usar); `estruturas.md` (principais estruturas: `ClassFile`, `CpInfo`, `Frame`, `RuntimeClass`, `Object`, `Array`); revisar se cada função tem objetivo, parâmetros de entrada/saída, algoritmo; conferir grafos de chamada/chamadora.
- **Pronto quando:** `make docs` sem nenhum warning; revisão manual de amostra de 20 funções aprovada pela equipe; HTML gerado empacotado para entrega.
- **Situação (2026-11-24):** `mainpage.md` com visão geral, como compilar, uso e arquitetura; `estruturas.md` com as estruturas do leitor e do interpretador; script conferiu que **todas** as funções de `src/` têm bloco Doxygen com **Algoritmo** (e `WARN_NO_PARAMDOC` + `FAIL_ON_WARNINGS` garantem os parâmetros); `make docs` sem avisos; grafos de chamada/chamadora gerados (Graphviz); HTML incluído no pacote. **Pendente:** revisão manual da amostra de 20 funções pela equipe.

#### F-03 · Pacote de entrega
- **Tarefas:** `jvm.exe` de 32 bits compilado com GCC (MinGW `i686`), fontes, `Makefile`, documentação HTML, `README` com exemplos de uso das flags, relatório de fuzzing e relatório do cppcheck (`make cppcheck` sem avisos).
- **Pronto quando:** em uma máquina Windows limpa, seguindo só o `README`, é possível exibir e executar os programas de teste.
- **Situação (2026-11-24):** `make dist` gera `build/entrega.zip` (`jvm.exe` PE32, fontes, `Makefile`, `docs/html`, relatórios de fuzzing e cppcheck, programas de teste compilados em `exemplos/`). O pacote extraído foi testado com o `jvm.exe` e o `tests/run_tests.ps1` no PowerShell do Windows: 90/90. **Pendente:** conferência em uma máquina Windows limpa (sem WSL).

> ### ✅ CHECKPOINT C5 — Entrega final
> - [x] Todos os itens da seção 1.2 demonstráveis (exibidor: `make test-viewer`; interpretador: um tema de `tests/java/` por item)
> - [x] Requisitos R1–R11 verificados (checklist da seção 1.1): R1 GCC/MinGW; R2 só nativos em C, nenhum `.class` da biblioteca; R3 `-std=c99 -pedantic -Werror`; R4 libFuzzer (`fuzz_reader`, `fuzz_interpreter`); R5 `-v`/`-i`/`-cp`; R6/R7 Doxygen sem avisos, toda função com algoritmo e grafos; R8 pt-BR/inglês; R9 formato jclasslib; R10 ELF 32-bit e PE32; R11 `make cppcheck` sem avisos
> - [ ] *Tag* git `v3.0-final`

---

## 5. Ordem de execução e dependências

```
P-01 → S-01..S-07 → [C1] → L-01 → L-02 → L-03 → L-04 → L-05 → L-06 → L-07 → L-08 → L-09
                                    └→ E-01 ─────────────────────────────┐
                    L-07 → E-02 → E-03 → E-04 → E-05 → E-06 → [C2]
     [C2] + S-08 + S-09 → M-01 → M-02 → M-03 → M-04 → M-05 → [C3]
     [C3] → I-01 → I-02 → I-03 → I-04 → I-05 → [C4a]
          → I-06 → I-07 → [C4b] → I-08 → I-09 → I-10 → [C4c]
          → F-01 → F-02 → F-03 → [C5]
```

Paralelização possível em equipe: M-03 em paralelo com M-02, desde que em módulos diferentes; I-08 (natives de `String`/impressão) em paralelo com I-06/I-07; F-02 contínuo (cada atividade já entrega comentários).

---

## 6. Riscos e mitigações

| Risco | Impacto | Mitigação |
|-------|---------|-----------|
| libFuzzer não existe no GCC | Conflito R1 × R4 | D1: Clang só para os *harnesses* de fuzz; produto final compilado com GCC |
| Bibliotecas de 32 bits ausentes (`gcc-multilib`, ASan/UBSan de 32 bits) | `-m32` não liga; sem sanitizers | S-08 valida todas as combinações antes de M-01; o runtime i386 do libFuzzer já está instalado |
| Suposições de 64 bits escondidas no código atual | Bugs só após `-m32` | M-01 roda a suíte inteira (incl. ASan/UBSan e fuzzing) e compara a saída do exibidor antes/depois |
| Simplificação quebrar o leitor/exibidor | Regressão na 1ª entrega | Etapa 4 só mexe com a suíte verde (`make check`, `test-viewer`, `test-windows`); uma atividade por commit |
| `javac` moderno não gera bytecode Java 2 / testes do professor gerados de forma desconhecida | Sem testes de Java 2 ou divergência na avaliação | D5/D13: testar cedo (S-05); aceitar major 45–52; corpus Java 2 gerado por mais de um compilador |
| Diferenças Linux × Windows (`\r\n`, caminhos, `printf` de `%lld` no MinGW antigo) | Bugs só no Windows | D3; usar `PRId64` de `inttypes.h`; `__USE_MINGW_ANSI_STDIO=1`; com `-m32` os dois lados são ILP32; testar no Windows em todo checkpoint |
| Formatação de `float`/`double` diferente do Java | Saída divergente | `common/number_format.c` já implementa as regras do `Double.toString` (menor representação, faixas decimal/científica, subnormais) e confere com o Java 8 em 97% de 20 mil valores aleatórios. As diferenças restantes vêm de um defeito do JDK 8 (nem sempre escolhe a menor representação para \|x\| ≥ 10⁷ em `float` e ≥ 10¹⁷ em `double`). Só tratar se algum teste precisar |
| UB em C na aritmética (overflow com sinal, shift ≥ largura, divisão `MIN/-1`) | Resultados errados/crash | Aritmética em tipos sem sinal; casos especiais explícitos; UBSan |
| Avisos do cppcheck acumulados ou falsos positivos | Retrabalho no fim; R11 não cumprido | `make cppcheck` no DoD de toda atividade (D24); falso positivo suprimido *inline* com justificativa, nunca globalmente |
| `jsr/ret` e `finally` no Java 2 | Exceções falham no Java 2 | I-05 implementa `jsr/ret` com teste de bytecode montado à mão; I-09 testa `finally` nas 3 versões |
| Busca linear por nome (D19) ficar lenta | Testes demorados | Programas de teste são pequenos; só otimizar se um teste real ficar lento |
| Documentação deixada para o fim | Perda de nota | R7/DoD comum: `make docs` com `WARN_AS_ERROR` |

---

## 7. Dúvidas respondidas

| # | Pergunta | Resposta | Reflexo no plano |
|---|----------|----------|------------------|
| 1 | Clang apenas para o fuzzing? | Sim | D1 |
| 2 | Compilar no Windows ou só executar? | Basta o `jvm.exe` executar no Windows | D2, S-02 |
| 3 | Formato do exibidor? | Livre (depois: similar ao jclasslib, ver R9) | D12, E-02 |
| 4 | Como serão gerados os testes de Java 2? | Desconhecido | D13, S-05, riscos |
| 5 | `invokedynamic`, `Math`, `Integer`, `Scanner`? | Fora do escopo (só o que está na avaliação) | D9 |
| 6 | Equipe e prazos? | Não é necessário | D14 |
| 7 | 32 ou 64 bits? | 32 bits: compilar com `-m32`, como a especificação da JVM | R10, D16, S-08, M-01 |
| 8 | Quanto da JVM real implementar? | O mínimo funcional: sem threads, JIT, GC, verificador etc.; projeto o mais simples possível | Princípio do topo, D9, D17–D19, Etapa 4 |
| 9 | O exibidor precisa decodificar o `StackMapTable` campo a campo? | Sim, como o jclasslib | L-07, M-03 |
| 10 | Limite de linhas por arquivo? | ~300 linhas; agrupar só arquivos de mesma responsabilidade | D17, 3.4, M-02 |
| 11 | `switch` ou tabela de ponteiros no interpretador? | Tabela de ponteiros de função (o `switch` é mais lento) | D18, I-03 |
