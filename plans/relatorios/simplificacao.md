# Relatório de simplificação — Etapa 4 (M-01 a M-03, checkpoint C3)

> Data: 2026-11-24. Referência "antes" = commit `a046195` (checkpoint C2, 64 bits).
> Contagem com `wc -l` sobre `src/**/*.c` e `include/**/*.h`.

## Resumo

| Item | Antes (C2) | Depois (C3) |
|------|-----------:|------------:|
| Arquivos em `src/` + `include/` | 45 | 42 |
| Linhas em `src/` + `include/` | 5731 | 5658 |
| Maior arquivo | `format.c` (306) | `class_file.c` (297) |
| Arquivos acima de ~300 linhas | 1 | 0 |
| Headers internos | 3 (`attr_parsers.h`, `printer_internal.h`, `format.h`) | 2 (`attr_parsers.h`, `printer_internal.h`) |
| Testes unitários | 50 | 49 (removido o teste de `jvm_version_string()`, que deixou de existir) |

Comportamento: a saída do exibidor (`jvm -v`) nos 29 fixtures de `build/fixtures`
(Java 2, 6 e 8) é **idêntica byte a byte** à do commit de referência, inclusive
o código de saída.

## M-01 — Migração para `-m32`

- `Makefile`: `ARCH := -m32 -msse2 -mfpmath=sse` em `CFLAGS`, `LDFLAGS`
  (release e debug), testes e `FUZZ_FLAGS`; `MINGW := i686-w64-mingw32-gcc`.
- `-msse2 -mfpmath=sse`: sem isso o GCC de 32 bits usa a FPU x87 (precisão
  estendida de 80 bits) e `float`/`double` deixam de seguir o IEEE 754 como no Java.
- `include/common/platform.h`: `typedef` com tamanho negativo se
  `sizeof(void *) != 4` (equivalente C99 de `_Static_assert`), incluído por
  `class_file.h` e, portanto, por todo código que usa o leitor.
- Revisão de suposições de 64 bits: os formatos `%lu` recebem valores
  `uint32_t` convertidos para `unsigned long` (32 bits é suficiente); os
  tamanhos alocados vêm de campos u2/u4 do `.class`; não há conversões
  ponteiro ↔ inteiro. Nada precisou ser alterado.
- Ambiente: `gcc-multilib` (GCC `-m32`, ASan/UBSan) e `g++-multilib`
  (`libstdc++` de 32 bits exigida pelo runtime do libFuzzer) — README,
  `tools/check_env.sh` e guia atualizados.

## M-02 — Reorganização por responsabilidade

| Antes | Depois | Motivo |
|-------|--------|--------|
| `attributes.c` (204) + `attr_common.c` (39) | `attributes.c` (246) | Mesma responsabilidade: leitura genérica de atributos e os utilitários usados por todos os leitores. |
| `attr_simple.c` (74) + `attr_tables.c` (181) | `attr_metadata.c` (248) | Mesma responsabilidade (atributos informativos); a divisão anterior era por formato, não por responsabilidade. |
| `format.c` (306) | `format.c` (180) + parte de `print_constant_pool.c` | `format.c` tinha duas responsabilidades; a representação das constantes (`print_constant`, `print_cp_ref`...) foi para `print_constant_pool.c`. |
| `printer.c` (162) | `printer.c` (122) | Os auxiliares de linha (`print_label`, `print_ref_line`, `print_number_line`, `print_flags_line`, `print_section`) foram para `format.c` (formatação geral). |
| `format.h` (121) + `printer_internal.h` (143) | `printer_internal.h` (233) | Um único header interno do exibidor, organizado por arquivo `.c`. |

## M-03 — Revisão de simplicidade

| Mudança | Motivo |
|---------|--------|
| `version.c` + `jvm_version_string()` + macros de *stringify* → macro `JVM_VERSION "0.1.0"` em `version.h` | Construção elaborada para produzir uma constante. |
| `constant_info_name()` removida; o exibidor escreve `CONSTANT_%s_info` com `cp_tag_name()` | Duplicava a tabela de nomes de tags do leitor. |
| `attribute_kind_from_name()` e `method_handle_kind_name()` viraram `static` | Usadas em um único arquivo; saíram da interface dos headers. |
| `members_free()` sem o teste de `NULL` | `free(NULL)` é válido e o laço não executa com `count == 0`. |

Mantidos por serem exigidos ou já diretos: decodificação campo a campo de
todos os atributos (inclusive `StackMapTable`, R9), `number_format.c`,
`mutf8.c`, `byte_reader.c`, `cli.c`, `cp_validate.c` (validação feita uma
única vez após a leitura do pool; as verificações do exibidor protegem os
índices vindos do bytecode, que não passam por `cp_validate`).

## Responsabilidade de cada arquivo (depois)

| Arquivo | Linhas | Responsabilidade |
|---------|-------:|------------------|
| `include/version.h` | 11 | Versão do programa (`--version`). |
| `include/common/platform.h` | 23 | Garantia de compilação em 32 bits. |
| `include/common/file_io.h` / `src/common/file_io.c` | 29 / 44 | Leitura de um arquivo inteiro para a memória. |
| `include/common/mutf8.h` / `src/common/mutf8.c` | 55 / 72 | Validação e decodificação de MUTF-8. |
| `include/common/number_format.h` / `src/common/number_format.c` | 51 / 165 | `float`/`double` em texto como `Float.toString`/`Double.toString`. |
| `include/cli/cli.h` / `src/cli/cli.c` | 97 / 190 | Argumentos de linha de comando e caminho do `.class`. |
| `src/main.c` | 132 | Ponto de entrada e despacho do modo. |
| `include/classfile/byte_reader.h` / `src/classfile/byte_reader.c` | 141 / 81 | Leitura big-endian com verificação de limites. |
| `include/classfile/cf_error.h` / `src/classfile/cf_error.c` | 68 / 40 | Códigos e registro de erros do leitor. |
| `include/classfile/class_file.h` / `src/classfile/class_file.c` | 179 / 297 | Estrutura raiz `ClassFile`: cabeçalho, interfaces, campos, métodos. |
| `include/classfile/cp_info.h` | 91 | Estruturas das entradas do pool. |
| `include/classfile/constant_pool.h` | 210 | Interface do pool (leitura, validação, consulta). |
| `src/classfile/constant_pool.c` | 188 | Leitura e liberação do pool. |
| `src/classfile/cp_validate.c` | 128 | Validação das referências internas do pool. |
| `src/classfile/cp_query.c` | 149 | Consultas seguras ao pool (`cp_get_*`). |
| `include/classfile/attribute_info.h` | 174 | Estruturas dos atributos. |
| `include/classfile/attributes.h` | 64 | Interface pública dos atributos. |
| `include/classfile/attr_parsers.h` | 259 | Header interno dos leitores de atributo. |
| `src/classfile/attributes.c` | 246 | Leitura genérica, despacho por tipo e utilitários. |
| `src/classfile/attr_metadata.c` | 248 | Atributos informativos e desconhecidos. |
| `src/classfile/attr_code.c` | 72 | Atributo `Code`. |
| `src/classfile/attr_stack_map.c` | 177 | Atributo `StackMapTable`. |
| `src/classfile/attr_free.c` | 74 | Liberação dos atributos. |
| `include/bytecode/opcodes.h` | 115 | Interface da tabela de opcodes. |
| `src/bytecode/opcodes.c` | 243 | Tabela dos 202 opcodes. |
| `src/bytecode/instruction.c` | 118 | Decodificação de operandos e tamanho das instruções. |
| `include/viewer/viewer.h` | 37 | Interface pública do exibidor. |
| `include/viewer/printer_internal.h` | 233 | Header interno do exibidor. |
| `include/viewer/disassembler.h` / `src/viewer/disassembler.c` | 45 / 208 | Bytecode com mnemônicos. |
| `src/viewer/printer.c` | 122 | `viewer_print_class()` e seções gerais, interfaces, campos e métodos. |
| `src/viewer/format.c` | 180 | Formatação geral: flags, indentação, MUTF-8, linhas `Rotulo: valor`. |
| `src/viewer/print_constant_pool.c` | 261 | Seção Constant Pool e representação das constantes. |
| `src/viewer/print_attributes.c` | 239 | Exibição dos atributos e do `Code`. |
| `src/viewer/print_stack_map.c` | 102 | Exibição do `StackMapTable`. |

## Tamanhos antes × depois

| Arquivo | Antes | Depois |
|---------|------:|-------:|
| `include/classfile/attr_parsers.h` | 262 | 259 |
| `include/classfile/attributes.h` | 74 | 64 |
| `include/classfile/class_file.h` | 177 | 179 |
| `include/common/platform.h` | — | 23 |
| `include/version.h` | 31 | 11 |
| `include/viewer/format.h` | 121 | — |
| `include/viewer/printer_internal.h` | 143 | 233 |
| `src/classfile/attr_common.c` | 39 | — |
| `src/classfile/attr_metadata.c` | — | 248 |
| `src/classfile/attr_simple.c` | 74 | — |
| `src/classfile/attr_tables.c` | 181 | — |
| `src/classfile/attributes.c` | 204 | 246 |
| `src/classfile/class_file.c` | 300 | 297 |
| `src/common/version.c` | 17 | — |
| `src/viewer/format.c` | 306 | 180 |
| `src/viewer/print_constant_pool.c` | 105 | 261 |
| `src/viewer/printer.c` | 162 | 122 |
| Demais arquivos | inalterados | inalterados |
