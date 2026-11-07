#!/usr/bin/env bash
# =============================================================================
# Testes de integração da JVM.
#
# Cada teste é um arquivo tests/java/<tema>/<Nome>.java cuja classe principal
# se chama <Nome>. O script:
#   1. compila o teste com o javac para Java 2, 6 e 8;
#   2. executa a classe na nossa JVM;
#   3. compara a saída padrão e o código de saída (zero/não zero) com
#      tests/expected/<tema>/<Nome>.txt e <Nome>.code.
#
# Uso:
#   tests/run_tests.sh                    roda todos os testes
#   tests/run_tests.sh <filtro>           roda apenas testes cujo caminho contém <filtro>
#   tests/run_tests.sh --update-expected  gera as saídas esperadas com o java real
#   tests/run_tests.sh --compile-only     apenas compila os testes
#
# Variáveis de ambiente:
#   JVM_CMD   comando da JVM a testar (padrão: "./build/jvm -i")
#   JAVAC     compilador Java (padrão: javac)
#   JAVA      JVM de referência para --update-expected (padrão: java)
#   VERSIONS  versões de bytecode (padrão: "java2 java6 java8")
# =============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

# Caminhos relativos: assim o jvm.exe (Windows) também os entende via WSL.
JVM_CMD="${JVM_CMD:-./build/jvm -i}"
JAVAC="${JAVAC:-javac}"
JAVA="${JAVA:-java}"
VERSIONS="${VERSIONS:-java2 java6 java8}"
CLASSES_DIR="build/tests/classes"
OUTPUT_DIR="build/tests/output"
EXPECTED_DIR="tests/expected"

MODE="run"
FILTER=""
for arg in "$@"; do
    case "$arg" in
        --update-expected) MODE="update" ;;
        --compile-only)    MODE="compile" ;;
        -h|--help)         sed -n '2,25p' "$0"; exit 0 ;;
        *)                 FILTER="$arg" ;;
    esac
done

# Opções do javac para cada versão de bytecode.
javac_flags() {
    case "$1" in
        java2) echo "-source 1.2 -target 1.2" ;;
        java6) echo "-source 1.6 -target 1.6" ;;
        java8) echo "-source 1.8 -target 1.8" ;;
        *)     echo "versao desconhecida: $1" >&2; return 1 ;;
    esac
}

# Lista os testes (caminho relativo a tests/java, sem extensão).
list_tests() {
    find tests/java -name '*.java' | sort | sed 's|^tests/java/||; s|\.java$||' |
        grep -F -- "$FILTER"
}

# Compila um teste para uma versão. Saída: build/tests/classes/<ver>/<tema>/<Nome>/
compile_test() {
    local version="$1" test="$2"
    local out="$CLASSES_DIR/$version/$test"
    mkdir -p "$out"
    # shellcheck disable=SC2046
    "$JAVAC" $(javac_flags "$version") -nowarn -encoding UTF-8 -d "$out" \
        "tests/java/$test.java" > "$out/javac.log" 2>&1
}

TESTS="$(list_tests)"
if [ -z "$TESTS" ]; then
    echo "Nenhum teste encontrado (filtro: '$FILTER')."
    exit 1
fi

pass=0
fail=0

for test in $TESTS; do
    name="$(basename "$test")"
    for version in $VERSIONS; do
        label="$test [$version]"
        if ! compile_test "$version" "$test"; then
            echo "[ERRO-COMPILACAO] $label (ver $CLASSES_DIR/$version/$test/javac.log)"
            fail=$((fail + 1))
            continue
        fi
        classpath="$CLASSES_DIR/$version/$test"

        case "$MODE" in
        compile)
            echo "[COMPILADO] $label"
            pass=$((pass + 1))
            ;;
        update)
            mkdir -p "$EXPECTED_DIR/$(dirname "$test")"
            tmp="$OUTPUT_DIR/$version/$test.ref"
            mkdir -p "$(dirname "$tmp")"
            "$JAVA" -cp "$classpath" "$name" > "$tmp" 2> /dev/null
            code=$?
            if [ "$version" = "java8" ] || [ ! -f "$EXPECTED_DIR/$test.txt" ]; then
                cp "$tmp" "$EXPECTED_DIR/$test.txt"
                echo "$code" > "$EXPECTED_DIR/$test.code"
            fi
            echo "[ATUALIZADO] $label (codigo $code)"
            pass=$((pass + 1))
            ;;
        run)
            expected_txt="$EXPECTED_DIR/$test.txt"
            expected_code_file="$EXPECTED_DIR/$test.code"
            if [ ! -f "$expected_txt" ] || [ ! -f "$expected_code_file" ]; then
                echo "[SEM-ESPERADO] $label (rode com --update-expected)"
                fail=$((fail + 1))
                continue
            fi
            actual="$OUTPUT_DIR/$version/$test.txt"
            mkdir -p "$(dirname "$actual")"
            # shellcheck disable=SC2086
            $JVM_CMD -cp "$classpath" "$name" 2> "$actual.err" | tr -d '\r' > "$actual"
            code=${PIPESTATUS[0]}
            expected_code="$(cat "$expected_code_file")"
            ok=1
            if ! diff -u "$expected_txt" "$actual" > "$actual.diff"; then ok=0; fi
            if { [ "$expected_code" -eq 0 ] && [ "$code" -ne 0 ]; } ||
               { [ "$expected_code" -ne 0 ] && [ "$code" -eq 0 ]; }; then ok=0; fi
            if [ $ok -eq 1 ]; then
                echo "[OK]     $label"
                pass=$((pass + 1))
            else
                echo "[FALHOU] $label (codigo esperado $expected_code, obtido $code)"
                head -n 20 "$actual.diff" | sed 's/^/         /'
                [ -s "$actual.err" ] && head -n 5 "$actual.err" | sed 's/^/  stderr:/'
                fail=$((fail + 1))
            fi
            ;;
        esac
    done
done

echo
echo "Resultado: $pass ok, $fail falha(s)"
[ $fail -eq 0 ]
