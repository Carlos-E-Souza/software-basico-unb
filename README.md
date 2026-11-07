# software-basico-unb — JVM em C99

Repositório do projeto da disciplina de **Software Básico (UnB)**: uma **Máquina Virtual Java** escrita em C99, com fins didáticos. Ela tem dois modos:

- **Leitor/Exibidor:** lê um arquivo `.class` e mostra as estruturas montadas (pool de constantes, campos, métodos, atributos) e o bytecode com os mnemônicos.
- **Interpretador:** executa pequenos programas Java compilados com `javac` para **Java 2 (1.2), Java 6 ou Java 8**.


---

## Escopo

O interpretador suporta:

- instruções com tipos de **categoria 1** (`int`, `float`, referência) e **categoria 2** (`long`, `double`);
- **arrays** de todos os tipos, inclusive multidimensionais;
- **strings** (literais, concatenação, `StringBuilder`/`StringBuffer`);
- **métodos estáticos e de instância**;
- **atributos de classe e de instância**;
- **herança, polimorfismo e reescrita** de métodos;
- **objetos**;
- **tratamento de exceções** (`try/catch/finally`, exceções do usuário e da JVM);
- **impressão** simulada (`System.out.print`/`println`).

Fica **fora do escopo**: lambdas (`invokedynamic`), `Math`, classes *wrapper* (`Integer` etc.), `Scanner`/entrada de dados, threads, reflexão e coleta de lixo.

Pela regra da disciplina, **a única classe da API Java usada é `java/lang/Object`**. As demais classes de biblioteca necessárias (`String`, `System.out`, exceções padrão etc.) são simuladas em C.

---

## Regras do projeto

- Código compilado com **GCC** e **`-std=c99`**.
- **libFuzzer** como analisador dinâmico. O libFuzzer só existe no Clang, então o **Clang é usado apenas para compilar os alvos de fuzzing**; o executável do projeto é sempre compilado com GCC.
- Documentação gerada pelo **Doxygen**.

---

## Pré-requisitos

O desenvolvimento é feito em **Linux ou WSL (Ubuntu)**. O executável final (`jvm.exe`) é gerado no Linux/WSL e roda no **Windows 10/11**.

| Ferramenta | Uso | Versão mínima sugerida |
|------------|-----|------------------------|
| GCC + Make | Compilar o projeto (Linux) | GCC 9+ |
| MinGW-w64 (`x86_64-w64-mingw32-gcc`) | Gerar o `jvm.exe` para Windows | qualquer recente |
| Clang + LLVM | Compilar os alvos do libFuzzer | Clang 10+ |
| Doxygen | Gerar a documentação | 1.9+ |
| Graphviz (`dot`) | Diagramas de chamada e das estruturas | qualquer recente |
| JDK 8 | Gerar os `.class` de teste e servir de referência de saída | 8 |
| JDK antigo (1.3/1.4) | *(opcional)* Gerar uma segunda variante dos testes de Java 2 | — |

---

## Instalação (Ubuntu / WSL)

```bash
# Compilador, make e cross-compiler para Windows
sudo apt update
sudo apt install -y build-essential mingw-w64

# Clang + libFuzzer (apenas para fuzzing)
sudo apt install -y clang llvm libclang-rt-dev

# Documentação
sudo apt install -y doxygen graphviz

# Java 8 (para gerar os .class de teste)
sudo apt install -y openjdk-8-jdk
```

Se o pacote `openjdk-8-jdk` não existir na sua versão do Ubuntu, instale o JDK 8 pelo [SDKMAN!](https://sdkman.io/):

```bash
curl -s "https://get.sdkman.io" | bash
source "$HOME/.sdkman/bin/sdkman-init.sh"
sdk list java | grep -i "8\.0"   # escolha uma distribuição do Java 8 (ex.: Temurin)
sdk install java <versao-8-escolhida>
```

### Verificando o ambiente

```bash
gcc --version
x86_64-w64-mingw32-gcc --version
clang --version
doxygen --version
dot -V
javac -version   # deve mostrar 1.8.x
```

---

## Comandos comuns

| Comando | O que faz |
|---------|-----------|
| `make` | Compila o projeto com GCC (`build/jvm`) |
| `make debug` | Compila com símbolos de depuração e sanitizers (ASan/UBSan) |
| `make test` | Roda os testes unitários (compila antes as classes de `tests/fixtures` para Java 2, 6 e 8) |
| `make fixtures` | Apenas compila as classes de `tests/fixtures` em `build/fixtures/java{2,6,8}` |
| `make test-integration` | Roda os programas de `tests/java` na nossa JVM (Java 2, 6 e 8) e compara com a saída do `java` real |
| `make test-viewer` | Compara o conteúdo exibido (pool de constantes, bytecode e valores das constantes nos argumentos) com o `javap` em todos os fixtures |
| `make check` | `test` + `test-viewer` + `test-integration` |
| `make fuzz` | Compila e executa os alvos do libFuzzer com Clang (`FUZZ_TIME=60` segundos por alvo) |
| `make fuzz-checkpoint` | Sessão de fuzzing dos checkpoints: 10 minutos por alvo (`FUZZ_CHECKPOINT_TIME=600`) |
| `make fuzz-smoke` | Confirma que o libFuzzer funciona (precisa encontrar um bug proposital) |
| `make docs` | Gera a documentação Doxygen em `docs/html/index.html` |
| `make windows` | Gera `build/jvm.exe` para Windows (MinGW-w64) |
| `make test-windows` | Testes unitários (`unit_tests.exe`) e de integração (`jvm.exe`) executados no Windows a partir do WSL |
| `make check-env` | Verifica se todas as ferramentas estão instaladas e as versões de bytecode geradas |
| `make clean` | Remove os artefatos de build (`build/`) e a documentação gerada (`docs/html/`) |

### Gerando `.class` de teste

```bash
# Java 2 (major 46)
javac -source 1.2 -target 1.2 -d out/java2 Exemplo.java
# Java 6 (major 50)
javac -source 1.6 -target 1.6 -d out/java6 Exemplo.java
# Java 8 (major 52)
javac -source 1.8 -target 1.8 -d out/java8 Exemplo.java

# Conferir a versão gerada
javap -v out/java8/Exemplo.class | grep major
```

### Testes de integração

Cada teste é um arquivo `tests/java/<tema>/<Nome>.java` (classe principal `<Nome>`), com a saída esperada em `tests/expected/<tema>/<Nome>.txt` e o código de saída em `<Nome>.code`.

```bash
tests/run_tests.sh --update-expected      # gera as saídas esperadas com o java real
tests/run_tests.sh                        # roda todos os testes na nossa JVM
tests/run_tests.sh strings                # roda só os testes cujo caminho contém "strings"
JVM_CMD="java" tests/run_tests.sh         # valida os próprios testes contra o java real
```

No Windows, depois de compilar os testes no Linux/WSL (`tests/run_tests.sh --compile-only`):

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -Jvm build\jvm.exe
```

---

## Como utilizar

```
jvm -v [-cp <dir>] <Classe|arquivo.class> [-o <arquivo>]
jvm -i [-cp <dir>] <ClassePrincipal>
jvm --help
```

| Flag | Atalho | Descrição |
|------|--------|-----------|
| `--mode viewer` ou `--viewer` | `-v` | Modo leitor/exibidor: mostra as estruturas do `.class` e o bytecode |
| `--mode interpreter` ou `--interpreter` | `-i` | Modo interpretador: executa o método `main` da classe *(Etapa 4, ainda não disponível)* |
| `--classpath <dir>` | `-cp <dir>` | Diretório onde ficam os arquivos `.class` (padrão: diretório atual) |
| `--output <arquivo>` | `-o <arquivo>` | *(exibidor)* Grava a saída em arquivo em vez do terminal |
| `--help` | `-h` | Mostra a ajuda |
| `--version` | — | Mostra a versão do programa |

A classe pode ser informada pelo nome (`Exemplo`, ou `pacote.Exemplo`, procurada em `<classpath>/pacote/Exemplo.class`) ou pelo caminho do arquivo (`out/java8/Exemplo.class`).

Códigos de saída: `0` sucesso, `1` argumentos inválidos, `2` funcionalidade ainda não implementada, `3` erro ao ler o `.class` ou gravar a saída.

### Exemplos

```bash
# Exibir a estrutura e o bytecode de Exemplo.class
./build/jvm -v -cp out/java8 Exemplo

# O mesmo, informando o arquivo e gravando a saída em um arquivo texto
./build/jvm -v out/java8/Exemplo.class -o exemplo.txt

# Executar o programa Exemplo (Etapa 4)
./build/jvm -i -cp out/java8 Exemplo
```

No Windows, no PowerShell (que exige `.\` para executar programas da pasta atual):

```powershell
.\jvm.exe -v -cp out\java8 Exemplo
.\jvm.exe -i -cp out\java8 Exemplo
```

No `cmd` (Prompt de Comando), o `.\` é opcional: `jvm.exe -v -cp out\java8 Exemplo`.

### Saída do exibidor

A saída segue a organização do visualizador **jclasslib**: `General Information`, `Constant Pool`, `Interfaces`, `Fields`, `Methods` e `Attributes`, com os mesmos rótulos. Toda referência ao pool de constantes mostra o **índice do slot** e a **constante representada** (`cp_info #10 <java/lang/Object>`), e os argumentos do bytecode que apontam para o pool mostram o **valor da constante** (`invokespecial #1 <java/lang/Object.<init> : ()V>`). Trecho de exemplo:

```
=== General Information ===
  Magic:                   0xCAFEBABE
  Minor version:           0
  Major version:           52 [Java 8]
  Constant pool count:     60
  Access flags:            0x0021 [public super]
  This class:              cp_info #9 <Flow>
  Super class:             cp_info #10 <java/lang/Object>
...
=== Constant Pool (count = 60) ===
  [1] CONSTANT_Methodref_info
        Class name:              cp_info #10 <java/lang/Object>
        Name and type:           cp_info #48 <<init> : ()V>
  [2] CONSTANT_Class_info
        Class name:              cp_info #49 <Flow$Failure>
...
=== Methods (count = 5) ===
  [1] divide
        Name:                    cp_info #22 <divide>
        Descriptor:              cp_info #23 <(II)I>
        Access flags:            0x0008 [static]
        Attributes count:        2
        [0] Code
            Attribute name index:    cp_info #17 <Code>
            Attribute length:        87
            Maximum stack size:      3
            Maximum local variables: 2
            Code length:             18
            Bytecode:
                  0 iload_1
                  1 ifne 14 (+13)
                  4 new #2 <Flow$Failure>
                  7 dup
                  8 ldc #3 <divisao por zero>
                 10 invokespecial #4 <Flow$Failure.<init> : (Ljava/lang/String;)V>
                 13 athrow
```

No Windows, o programa configura o terminal para UTF-8 automaticamente, para que textos com acentos apareçam corretamente.

---

## Estrutura do repositório

```
include/   headers públicos
src/       código-fonte (cli, classfile, viewer, vm)
tests/     testes unitários (C) e programas Java de teste com saídas esperadas
fuzz/      alvos do libFuzzer e corpus inicial
build/     artefatos gerados (não versionado)
```

---

## Licença

Distribuído sob a licença descrita em [`LICENSE`](LICENSE).
