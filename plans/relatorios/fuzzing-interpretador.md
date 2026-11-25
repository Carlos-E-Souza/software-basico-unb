# Relatório de fuzzing — interpretador (I-10, checkpoints C3 e C4c)

Sessões de 10 minutos por alvo (`make fuzz-checkpoint`, D15), em 32 bits
(`clang -m32 -msse2 -mfpmath=sse -std=c99 -g -O1 -fsanitize=fuzzer,address,undefined`).

## Alvos

| Alvo | O que exercita |
|------|----------------|
| `fuzz/fuzz_interpreter.c` | Cada entrada é o `.class` da classe principal: se o leitor a aceita, ela é registrada (`class_define()`) e o `main` é executado com limite de 100 mil instruções (`vm.max_steps`), saída descartada e mensagens silenciadas (`vm.quiet`). Classes nativas funcionam; outras classes do usuário não existem (erro fatal tratado). |
| `fuzz/fuzz_reader.c` | Leitor e exibidor (como no C2), agora compilados em 32 bits. |

Corpus inicial do interpretador: 132 `.class` dos testes de integração
(`build/tests/classes`, Java 2, 6 e 8) + `build/fixtures`.

## Sessão 1 — 2026-11-24 (encontrou 1 defeito)

| Item | Valor |
|------|-------|
| Alvo | `fuzz_interpreter` |
| Resultado | *Crash* após poucos minutos: `SEGV` em `natives_text()` (`src/vm/native_text.c:19`), chamado por `builder_append()` |
| Causa | Um `invokestatic` apontando para um método **de instância nativo** (`StringBuilder.append`). `find_target()` só conferia `ACC_STATIC` dos métodos do `.class`; nos nativos o 1º argumento (`null`) era tratado como `this`. |
| Correção | `NativeEntry` e `MethodRef` ganharam `is_static`; `find_target()` passa a exigir que ser estático combine com o tipo de invocação para **todos** os métodos (erro fatal "ACC_STATIC incompativel"). |
| Regressão | Teste unitário `test_native_static_flag` (falhou antes da correção, passa depois) e o artefato guardado em `fuzz/corpus/fuzz_interpreter/regressao-invokestatic-nativo`, reexecutado em toda sessão. |

## Sessão 2 — 2026-11-24 (depois da correção)

| Alvo | Duração | Execuções | Cobertura (arestas) | *Features* | Corpus final | Resultado |
|------|--------:|----------:|--------------------:|-----------:|-------------:|-----------|
| `fuzz_interpreter` | 601 s | 2.208.240 (~3,7 mil/s) | 3641 → 3686 | 15817 → 17340 | 813 entradas (1029 KB) | **Nenhum** *crash*, vazamento ou UB |
| `fuzz_reader` | 601 s | 5.940.407 (~9,9 mil/s) | 2052 → 2094 | 7541 → 8520 | 627 entradas (625 KB) | **Nenhum** *crash*, vazamento ou UB |

Log completo: `build/fuzz/checkpoint-c4c.log` (não versionado).

## Antes do libFuzzer em 32 bits

Enquanto o `g++-multilib` (runtime C++ de 32 bits exigido pelo libFuzzer)
não estava instalado, o mesmo `fuzz_interpreter.c` foi compilado com
GCC + ASan/UBSan e executado sobre 23 mil `.class` mutados aleatoriamente
(1 a 4 bytes alterados) a partir dos testes de integração: nenhuma falha.
Esse método não tem a cobertura guiada do libFuzzer e por isso não achou o
defeito da sessão 1.

## Proteções que o fuzzing confirma

- Etiquetas de slot (D25): um inteiro nunca é usado como ponteiro.
- Tamanho de cada instrução validado antes de executá-la; desvio para fora
  do código é erro fatal.
- Limites de pilha/locais (`max_stack`, `max_locals`), profundidade de frames
  (1024, D26), heap (256 MiB, D29) e instruções (só no fuzzing).
