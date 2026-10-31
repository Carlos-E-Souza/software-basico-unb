#!/usr/bin/env bash
# =============================================================================
# Compara o conteúdo exibido pelo nosso exibidor (formato jclasslib) com o do
# javap, que serve de referência independente.
#
# Como os formatos são diferentes, cada lado é reduzido ao mesmo conteúdo:
#   1. Pool de constantes, uma linha por entrada:
#        "índice Tipo refs..."  para entradas com referências (ex.: "1 Methodref 10 48")
#        "índice Tipo valor"    para Utf8, Integer, Float, Long e Double
#   2. Bytecode, uma linha por instrução: "pc mnemônico argumentos-numéricos"
#      (ex.: "24 invokestatic 6", "17 if_icmpge 60", "4 multianewarray 5 2"),
#      mais uma linha por caso de tableswitch/lookupswitch ("1: 28", "default: 37");
#   3. Valores das constantes apontadas pelos argumentos do bytecode, uma linha
#      por instrução: "pc valor" (ex.: "1 java/lang/Object.<init>:()V"),
#      comparando o "<...>" do exibidor com o comentário "// ..." do javap.
#
# Uso: tools/compare_javap.sh <arquivo.class>...
#   JVM: variável de ambiente com o executável (padrão: ./build/jvm)
# Retorna 0 se todos os arquivos conferem.
# =============================================================================
set -u

JVM="${JVM:-./build/jvm}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Remove espaços repetidos e espaços nas pontas.
squeeze() { sed -E 's/[[:space:]]+/ /g; s/^ //; s/ $//'; }

# Pool do nosso exibidor ("[n] CONSTANT_Xxx_info" seguido dos campos).
ours_pool() {
    squeeze | awk '
        function flush() { if (idx != "") print idx, tag, (tag ~ /^(Utf8|Integer|Float|Long|Double)$/ ? value : refs) }
        /^\[[0-9]+\] CONSTANT_[A-Za-z0-9]+_info$/ {
            flush()
            idx = substr($1, 2, length($1) - 2)
            tag = $2; sub(/^CONSTANT_/, "", tag); sub(/_info$/, "", tag)
            refs = ""; value = ""; next
        }
        /^\[[0-9]+\] \(large numeric continued\)$/ { flush(); idx = ""; next }
        idx == "" { next }
        /^(Integer|Float|Long|Double): / { value = substr($0, index($0, ": ") + 2); next }
        /^String: / && tag == "Utf8" { value = substr($0, index($0, ": ") + 2); next }
        /^(Reference kind|Bootstrap method): / { refs = refs (refs == "" ? "" : " ") $3; next }
        /cp_info #[0-9]+/ {
            match($0, /cp_info #[0-9]+/)
            refs = refs (refs == "" ? "" : " ") substr($0, RSTART + 9, RLENGTH - 9)
            next
        }
        /^=== / { flush(); idx = "" }
        END { flush() }'
}

# Pool do javap ("#n = Tipo resto // comentário").
javap_pool() {
    grep -aE '^[[:space:]]*#[0-9]+ = [A-Za-z]+' | squeeze | awk '
        {
            idx = substr($1, 2); tag = $3
            rest = $0; sub(/^#[0-9]+ = [A-Za-z0-9]+ ?/, "", rest)
            if (tag ~ /^(Integer|Float|Long|Double)$/) {
                sub(/[lfd]$/, "", rest); print idx, tag, rest
            } else if (tag == "Utf8") {
                print idx, tag, rest
            } else {
                sub(/ *\/\/.*$/, "", rest); gsub(/[^0-9]+/, " ", rest)
                gsub(/^ +| +$/, "", rest); print idx, tag, rest
            }
        }'
}

# Bytecode do nosso exibidor ("pc mnemonico argumentos").
ours_code() {
    squeeze | awk '
        /^Bytecode:$/ { in_code = 1; next }
        /^Exception table/ { in_code = 0; next }
        !in_code { next }
        /^[0-9]+ [a-z][a-z0-9_]*( |$)/ {
            pc = $1; op = $2; rest = $0; sub(/^[0-9]+ [a-z0-9_]+ ?/, "", rest)
            if (op == "wide") { op = $3 "_w"; sub(/^[a-z0-9_]+ ?/, "", rest) }
            if (op == "newarray") { match(rest, /\([a-z]+\)/); print pc, op, substr(rest, RSTART + 1, RLENGTH - 2); next }
            if (op == "tableswitch" || op == "lookupswitch") { print pc, op; next }
            sub(/<.*>/, "", rest); gsub(/\([^)]*\)/, "", rest)
            gsub(/[^-0-9]+/, " ", rest); gsub(/^ +| +$/, "", rest)
            print pc, op (rest == "" ? "" : " " rest); next
        }
        /^(-?[0-9]+|default): +-?[0-9]+/ { gsub(/\([^)]*\)/, ""); gsub(/ +/, " "); sub(/ $/, ""); print }'
}

# Valores resolvidos dos argumentos do pool no nosso exibidor: "pc valor".
# Aspas e espaços em volta de ":" são removidos para comparar com o javap, e o
# prefixo "EstaClasse." de campos/métodos da própria classe é retirado, porque
# o javap o omite (o exibidor sempre mostra o nome completo, como o jclasslib).
ours_values() {
    squeeze | awk '
        /^This class: cp_info #[0-9]+ </ { this = $0; sub(/^[^<]*</, "", this); sub(/>$/, "", this); next }
        /^Bytecode:$/ { in_code = 1; next }
        /^Exception table/ { in_code = 0; next }
        in_code && /^[0-9]+ [a-z][a-z0-9_]* #[0-9]+ </ && $2 != "invokedynamic" {
            v = $0; sub(/^[^<]*</, "", v); sub(/>[^>]*$/, "", v)
            gsub(/ : /, ":", v); gsub(/"/, "", v)
            if (this != "" && index(v, this ".") == 1 && index(v, ":") > 0) { v = substr(v, length(this) + 2) }
            print $1, v
        }'
}

# Valores resolvidos no comentário do javap ("// Method a.b:()V" -> "a.b:()V").
javap_values() {
    squeeze | awk '
        /^[0-9]+: [a-z][a-z0-9_]* #[0-9]+.*\/\/ / && $2 != "invokedynamic" {
            pc = $1; sub(/:$/, "", pc)
            v = $0; sub(/^[^\/]*\/\/ [A-Za-z]+ /, "", v); gsub(/"/, "", v)
            if ($0 ~ /\/\/ (long|float|double) /) { sub(/[lfd]$/, "", v) }
            print pc, v
        }'
}

# Bytecode do javap ("pc: mnemonico argumentos // comentário").
javap_code() {
    sed -E 's|//.*$||' | squeeze | awk '
        /^[0-9]+: [a-z][a-z0-9_]*( |$)/ {
            pc = $1; sub(/:$/, "", pc); op = $2; rest = $0; sub(/^[0-9]+: [a-z0-9_]+ ?/, "", rest)
            if (op == "newarray") { print pc, op, rest; next }
            if (op == "tableswitch" || op == "lookupswitch") { print pc, op; next }
            gsub(/[^-0-9]+/, " ", rest); gsub(/^ +| +$/, "", rest)
            if (op == "invokedynamic") { sub(/ 0$/, "", rest) }
            print pc, op (rest == "" ? "" : " " rest); next
        }
        /^(-?[0-9]+|default): -?[0-9]+$/ { print }'
}

failures=0
for class in "$@"; do
    "$JVM" -v "$class" > "$TMP/ours.txt" 2> "$TMP/ours.err" || {
        echo "[ERRO]   $class: o exibidor falhou: $(head -n 1 "$TMP/ours.err")"
        failures=$((failures + 1)); continue
    }
    # O javap escreve o caractere NUL cru; o exibidor o escapa como \u0000.
    javap -v -p -c "$class" 2>/dev/null | sed 's/\x00/\\u0000/g' > "$TMP/javap.txt"
    ours_pool  < "$TMP/ours.txt"  > "$TMP/ours.pool"
    javap_pool < "$TMP/javap.txt" > "$TMP/javap.pool"
    ours_code  < "$TMP/ours.txt"  > "$TMP/ours.code"
    javap_code < "$TMP/javap.txt" > "$TMP/javap.code"
    ours_values  < "$TMP/ours.txt"  > "$TMP/ours.values"
    javap_values < "$TMP/javap.txt" > "$TMP/javap.values"
    if diff -q "$TMP/ours.pool" "$TMP/javap.pool" > /dev/null &&
       diff -q "$TMP/ours.code" "$TMP/javap.code" > /dev/null &&
       diff -q "$TMP/ours.values" "$TMP/javap.values" > /dev/null; then
        echo "[OK]     $class ($(wc -l < "$TMP/ours.pool") constantes, $(wc -l < "$TMP/ours.code") linhas de bytecode, $(wc -l < "$TMP/ours.values") argumentos resolvidos)"
    else
        echo "[DIFERE] $class"
        diff "$TMP/ours.pool" "$TMP/javap.pool" | head -n 10 | sed 's/^/         pool: /'
        diff "$TMP/ours.code" "$TMP/javap.code" | head -n 10 | sed 's/^/         code: /'
        diff "$TMP/ours.values" "$TMP/javap.values" | head -n 10 | sed 's/^/         valor: /'
        failures=$((failures + 1))
    fi
done

echo
echo "Resultado: $(( $# - failures )) ok, $failures diferenca(s)"
[ "$failures" -eq 0 ]
