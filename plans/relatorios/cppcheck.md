# Relatório — análise estática com cppcheck (R11, D24)

- **Data:** 2026-11-24
- **Ferramenta:** Cppcheck 2.19.0
- **Comando:** `make cppcheck`, que executa

  ```
  cppcheck --std=c99 --platform=unix32 --enable=warning,style,performance,portability \
           --inline-suppr --suppress=missingIncludeSystem --error-exitcode=1 -q -Iinclude src fuzz
  cppcheck (mesmas opções) -Iinclude -Itests/unit tests/unit
  ```

- **Resultado final:** **0 avisos** (código de saída 0) em `src/`, `fuzz/` e `tests/unit/`.

## Avisos encontrados e o que foi feito

O alvo `make cppcheck` foi criado na Etapa 5 (antes só a decisão D24 existia).
A primeira execução sobre todo o código encontrou 41 avisos:

| Categoria (`id`) | Qtde | Onde | Correção |
|------------------|-----:|------|----------|
| `constVariablePointer` / `constParameterPointer` | 30 | `src/vm/*` (interpretador recém-escrito) | Ponteiros que só são lidos passaram a `const` (ex.: `frame_push_ref(Frame *, const Object *)`, `class_is_assignable(Vm *, const RuntimeClass *, ...)`, `object_is_instance(Vm *, const Object *, ...)`). |
| `variableScope` | 9 | `src/vm/*`, `fuzz/fuzz_reader.c`, `tests/unit/test_class_file.c`, `tests/unit/test_viewer.c` | Variável declarada no menor bloco em que é usada. |
| `duplicateExpression` | 1 | `tests/unit/test_number_format.c` (`zero / zero`) | **Supressão inline justificada:** a divisão 0/0 é proposital para gerar NaN em tempo de execução. |
| `arrayIndexOutOfBoundsCond` | 1 | `fuzz/fuzz_smoke.c` | **Supressão inline justificada:** o acesso fora dos limites é o bug proposital que o `make fuzz-smoke` usa para provar que o libFuzzer funciona. |

Nenhuma supressão global foi usada; as duas supressões estão na linha do
aviso, com o comentário `// cppcheck-suppress <id>` e a justificativa na
linha anterior.

## Como manter

`make cppcheck` faz parte do DoD de toda atividade de código (D24): uma
tarefa com aviso do cppcheck não está pronta. Falso positivo só com
supressão inline e justificativa.
