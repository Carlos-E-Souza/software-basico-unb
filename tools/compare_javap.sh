#!/usr/bin/env bash
# =============================================================================
# Compara o conteúdo exibido pelo nosso exibidor com o do javap (atividade E-05).
#
# Como os formatos são diferentes, compara apenas o CONTEÚDO, normalizado:
#   1. Pool de constantes: índice, tipo e referências brutas (ex.: "#1 = Methodref
#      #10.#48") e, para referências, também o valor resolvido do comentário;
#   2. Bytecode: cada instrução "pc: mnemonico operandos" (sem comentários),
#      na ordem em que aparece (métodos na ordem do arquivo).
#
# Uso: tools/compare_javap.sh [JVM] <arquivo.class>...
#   JVM: variável de ambiente com o executável (padrão: ./build/jvm)
# Retorna 0 se todos os arquivos conferem.
# =============================================================================
set -u

JVM="${JVM:-./build/jvm}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Normaliza linhas: remove espaços repetidos e espaços nas pontas.
squeeze() { sed -E 's/[[:space:]]+/ /g; s/^ //; s/ $//'; }

# Pool: "#n = Tag refs" e, para tipos de referência, " // comentário".
pool_lines() {
    grep -aE '^[[:space:]]*#[0-9]+ = [A-Za-z]+' | squeeze | awk '
        {
            tag = $3
            if (tag == "Utf8" || tag == "Integer" || tag == "Long" || tag == "Float" ||
                tag == "Double" || tag == "MethodHandle" || tag == "InvokeDynamic") {
                print $1, $2, $3          # valores: formatação pode diferir
            } else {
                print                     # refs + comentário resolvido
            }
        }'
}

# Bytecode: "pc: mnemonico operandos", sem comentários.
code_lines() {
    sed -E 's|//.*$||' | squeeze | grep -aE '^[0-9]+: [a-z]'
}

failures=0
for class in "$@"; do
    "$JVM" -v "$class" > "$TMP/ours.txt" 2> "$TMP/ours.err" || {
        echo "[ERRO]   $class: o exibidor falhou: $(head -n 1 "$TMP/ours.err")"
        failures=$((failures + 1)); continue
    }
    # O javap escreve o caractere NUL cru; o exibidor o escapa como \u0000.
    javap -v -p -c "$class" 2>/dev/null | sed 's/\x00/\\u0000/g' > "$TMP/javap.txt"
    pool_lines < "$TMP/ours.txt"  > "$TMP/ours.pool"
    pool_lines < "$TMP/javap.txt" > "$TMP/javap.pool"
    code_lines < "$TMP/ours.txt"  > "$TMP/ours.code"
    code_lines < "$TMP/javap.txt" > "$TMP/javap.code"
    if diff -q "$TMP/ours.pool" "$TMP/javap.pool" > /dev/null &&
       diff -q "$TMP/ours.code" "$TMP/javap.code" > /dev/null; then
        echo "[OK]     $class ($(wc -l < "$TMP/ours.pool") constantes, $(wc -l < "$TMP/ours.code") instrucoes)"
    else
        echo "[DIFERE] $class"
        diff "$TMP/ours.pool" "$TMP/javap.pool" | head -n 10 | sed 's/^/         pool: /'
        diff "$TMP/ours.code" "$TMP/javap.code" | head -n 10 | sed 's/^/         code: /'
        failures=$((failures + 1))
    fi
done

echo
echo "Resultado: $(( $# - failures )) ok, $failures diferenca(s)"
[ "$failures" -eq 0 ]
