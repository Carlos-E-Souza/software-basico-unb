# Relatório de fuzzing — leitor de `.class` (L-09)

| Item | Valor |
|------|-------|
| Data | 2026-10-31 |
| Alvo | `fuzz/fuzz_reader.c` (`class_file_parse` + todas as funções `cp_get_*` + `class_file_free`) |
| Compilação | `clang -std=c99 -g -O1 -fsanitize=fuzzer,address,undefined` (Clang 21.1.8) |
| Comando | `make fuzz FUZZ_TIME=1800` |
| Corpus inicial | 345 arquivos: 26 classes de `build/fixtures` (Java 2, 6 e 8) + corpus de uma rodada curta anterior |
| Dicionário | `fuzz/fuzz_reader.dict` (número mágico, versões e nomes de atributos) |
| Duração | 2265 s (~38 min, incluindo uma pausa da máquina) |
| Execuções | 34.728.924 (~19 mil/s) |
| Cobertura | 1204 → 1224 arestas; 4354 → 4788 *features* |
| Corpus final | 418 entradas (236 KB) |

## Resultado

- **Nenhum *crash*, vazamento de memória ou comportamento indefinido** detectado
  (AddressSanitizer, LeakSanitizer e UndefinedBehaviorSanitizer ativos).
- Um artefato `slow-unit` foi gravado (69 s). Ao reexecutar a mesma entrada, ela
  é processada em ~36 ms: a lentidão foi uma pausa do ambiente (WSL), não um
  problema do leitor.

## Antes da sessão longa

Os testes unitários já cobriam os casos que o fuzzer costuma encontrar
primeiro (truncamento em todas as posições de um `.class` real, índices
inválidos, tags desconhecidas, MUTF-8 inválido, atributos com tamanho
incoerente), o que explica a ausência de falhas.

## Como reproduzir

```bash
make fuzz FUZZ_TIME=1800
```
