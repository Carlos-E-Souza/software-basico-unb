# Principais estruturas de dados {#estruturas}

Esta página descreve as principais estruturas de dados da JVM didática.
Ela é atualizada a cada etapa do projeto, conforme as estruturas são
implementadas.

## Leitor de `.class` (Etapa 2)

O leitor transforma os bytes de um arquivo `.class` em uma árvore de
estruturas C que segue fielmente o capítulo 4 da especificação da JVM. Os
nomes dos campos são os mesmos da especificação.

```
ClassFile
├── magic, minor_version, major_version
├── constant_pool[1..count-1] ── CpInfo { tag, union info }
├── access_flags, this_class, super_class
├── interfaces[]                 (índices de CONSTANT_Class)
├── fields[]   ── MemberInfo { access_flags, name_index, descriptor_index, attributes[] }
├── methods[]  ── MemberInfo ── attributes[] ── AttributeInfo { kind = ATTR_CODE }
│                                               └── CodeAttribute { code[], exception_table[], attributes[] }
└── attributes[]                 (SourceFile, InnerClasses, BootstrapMethods, ...)
```

| Estrutura | Arquivo | Descrição |
|-----------|---------|-----------|
| ::ByteReader | `classfile/byte_reader.h` | Cursor sobre um buffer; lê `u1`, `u2`, `u4` em big-endian com verificação de limites. Sub-leitores restringem a leitura ao tamanho declarado de cada atributo. |
| ::ClassFile | `classfile/class_file.h` | Raiz da árvore: versão, pool, flags, classe/superclasse, interfaces, campos, métodos e atributos. É dona de toda a memória e é liberada por class_file_free(). O header inclui `cp_info.h` e `attribute_info.h`, então basta incluí-lo para ter todas as estruturas. |
| ::CpInfo | `classfile/cp_info.h` | Uma entrada do pool. O campo `tag` (::ConstantTag) indica qual membro da união `info` é válido. `Long` e `Double` ocupam duas posições; a segunda tem tag `CONSTANT_Unusable`. |
| ::CpUtf8 | `classfile/cp_info.h` | Texto em UTF-8 modificado. Como o MUTF-8 nunca contém byte zero, os bytes + `'\0'` formam uma string C. |
| ::MemberInfo | `classfile/class_file.h` | Campo (::FieldInfo) ou método (::MethodInfo): flags, nome, descritor e atributos. |
| ::AttributeInfo | `classfile/attribute_info.h` | Um atributo já interpretado; `kind` (::AttributeKind) indica o membro válido da união `info`. Atributos desconhecidos guardam os bytes brutos. |
| ::CodeAttribute | `classfile/attribute_info.h` | Bytecode de um método, `max_stack`, `max_locals`, tabela de exceções (::ExceptionTableEntry) e atributos aninhados (LineNumberTable, LocalVariableTable, StackMapTable). |
| ::StackMapFrame | `classfile/attribute_info.h` | Quadro do StackMapTable em forma normalizada (todas as formas — *same*, *chop*, *append*, *full* — usam a mesma estrutura). |
| ::ClassFileError | `classfile/cf_error.h` | Código (::CfStatus), posição no arquivo e mensagem de um erro de leitura. |

### Garantias do leitor

- Nenhuma leitura ultrapassa o buffer: arquivos truncados geram `CF_ERR_TRUNCATED`.
- Todos os índices para o pool são validados (tipo e limites) antes do uso;
  as funções `cp_get_*` retornam `NULL`/`false` para índices inválidos.
- Cada atributo deve consumir exatamente `attribute_length` bytes.
- Versões aceitas: *major* 45 a 52 (Java 1.1 a Java 8).

## Exibidor (Etapa 3)

| Estrutura | Arquivo | Descrição |
|-----------|---------|-----------|
| ::OpcodeInfo | `bytecode/opcodes.h` | Mnemônico e formato dos operandos (::OperandKind) de cada um dos 256 valores de opcode; os 202 opcodes definidos (0x00–0xC9) são válidos. Usada pelo desmontador e, depois, pelo interpretador. |
| ::CliOptions | `cli/cli.h` | Modo (::CliMode), classpath, classe alvo e arquivo de saída obtidos da linha de comando. |
| ::FlagContext | `viewer/printer_internal.h` | Contexto usado para decodificar flags de acesso (o mesmo bit significa coisas diferentes em classes, campos e métodos, ex.: 0x0020 é `ACC_SUPER` ou `ACC_SYNCHRONIZED`). |

Fluxo do exibidor: `main` → cli_parse() → cli_resolve_class_path() →
class_file_load() → viewer_print_class() → disassemble_code() para cada
atributo Code.

## Interpretador (Etapa 5)

Definidas em `include/vm/runtime.h` (desenho mínimo da seção 3.2.1 do plano):

| Estrutura | Conteúdo | Observações |
|-----------|----------|-------------|
| `Slot` | `uint32_t` | Unidade da pilha de operandos e das locais (D6). `long`/`double` ocupam 2 slots (alto, baixo); uma referência ocupa 1 slot com o próprio ponteiro (`-m32`, D16). |
| `SlotTag` | `TAG_VALUE`, `TAG_REF`, `TAG_RETADDR` | Etiqueta de cada slot da pilha e das locais. Só slots `TAG_REF` são usados como ponteiro; `ret` exige `TAG_RETADDR`. É a verificação mínima que impede um bytecode inválido de transformar um inteiro em ponteiro (D25). |
| `Field` | nome, descritor, `Slot value[2]` | Campo de instância ou estático (D21). Os modelos ficam na `RuntimeClass`; cada objeto copia os modelos da classe e das superclasses. |
| `Object` | `kind`, classe, próximo da lista do heap, união | `OBJECT_INSTANCE` (vetor de `Field`), `OBJECT_ARRAY` (`Array`) ou `OBJECT_TEXT` (`Text`: `String`, `StringBuilder`, `StringBuffer`). |
| `Array` | descritor (`[I`, `[[Ljava/lang/String;`...), tamanho, tamanho do elemento, dados | Elementos com o tamanho real do tipo (1, 2, 4 ou 8 bytes); multidimensional = array de referências. |
| `Text` | `uint16_t *chars`, `length` | Texto UTF-16 sem `Array` interno (D22). |
| `RuntimeClass` | nome, `ClassFile` (NULL nas nativas), superclasse, modelos de campos, estáticos, estado do `<clinit>` | Lista ligada na `Vm`; busca linear por nome (D19). |
| `Frame` | classe, método, bytecode, `pc`/`next_pc`, locais, pilha, etiquetas, valor de retorno | Criado por `interpreter_call()`; uma chamada Java = uma chamada C (D26). |
| `Vm` | *classpath*, saída, classes, heap, literais internados, exceção pendente, erro fatal, profundidade | Estado global monothread (D9). |
| `MethodRef` | classe dona, `MethodInfo` ou função nativa | Resultado da busca de método na hierarquia. |

### Execução

- **Despacho:** `interpreter_step()` valida o tamanho da instrução, define
  `next_pc` e chama `handlers[opcode]` (D18). Desvios só alteram `next_pc`.
- **Exceções:** a instrução que lança grava `vm->exception`; o laço procura
  na `exception_table` do método (faixa de `pc` + `catch_type` compatível
  por herança). Sem tratador, `interpreter_call()` retorna `false` e o
  chamador repete a busca no seu método.
- **Nativos:** linhas `{classe, método, descritor, função}`; a busca de
  método sobe a hierarquia e, nas classes nativas, consulta essas tabelas.

