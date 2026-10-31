#!/usr/bin/env bash
# =============================================================================
# Verifica se o ambiente de desenvolvimento está completo.
#
# Checa: GCC, Make, MinGW-w64, Clang (+ runtime do libFuzzer), Doxygen,
# Graphviz e JDK 8, e se o javac gera bytecode com major 46, 50 e 52.
# =============================================================================
set -u

ok=0
missing=0

check_tool() {
    local tool="$1" version_cmd="$2"
    if command -v "$tool" > /dev/null 2>&1; then
        printf "  [OK]      %-24s %s\n" "$tool" "$($version_cmd 2>&1 | head -n 1)"
        ok=$((ok + 1))
    else
        printf "  [AUSENTE] %s\n" "$tool"
        missing=$((missing + 1))
    fi
}

echo "Ferramentas:"
check_tool gcc                    "gcc --version"
check_tool make                   "make --version"
check_tool x86_64-w64-mingw32-gcc "x86_64-w64-mingw32-gcc --version"
check_tool clang                  "clang --version"
check_tool doxygen                "doxygen --version"
check_tool dot                    "dot -V"
check_tool javac                  "javac -version"
check_tool java                   "java -version"

# Runtime do libFuzzer: tenta compilar e linkar um alvo mínimo.
if command -v clang > /dev/null 2>&1; then
    tmp="$(mktemp -d)"
    printf '#include <stdint.h>\n#include <stddef.h>\nint LLVMFuzzerTestOneInput(const uint8_t *d, size_t s){(void)d;(void)s;return 0;}\n' > "$tmp/f.c"
    if clang -std=c99 -fsanitize=fuzzer "$tmp/f.c" -o "$tmp/f" > /dev/null 2>&1; then
        echo "  [OK]      libFuzzer (clang -fsanitize=fuzzer)"
        ok=$((ok + 1))
    else
        echo "  [AUSENTE] runtime do libFuzzer (instale libclang-rt-dev)"
        missing=$((missing + 1))
    fi
    rm -rf "$tmp"
fi

# Versões de bytecode: compila uma classe mínima para cada alvo e lê o major.
if command -v javac > /dev/null 2>&1; then
    echo
    echo "Versoes de bytecode (javac):"
    tmp="$(mktemp -d)"
    printf 'public class EnvCheck { public static void main(String[] a) { } }\n' > "$tmp/EnvCheck.java"
    for spec in "Java2:46:-source 1.2 -target 1.2" "Java6:50:-source 1.6 -target 1.6" "Java8:52:-source 1.8 -target 1.8"; do
        name="${spec%%:*}"; rest="${spec#*:}"; want="${rest%%:*}"; flags="${rest#*:}"
        mkdir -p "$tmp/$name"
        # shellcheck disable=SC2086
        if javac $flags -nowarn -d "$tmp/$name" "$tmp/EnvCheck.java" > "$tmp/$name.log" 2>&1; then
            major=$(od -An -t u1 -j 6 -N 2 "$tmp/$name/EnvCheck.class" | awk '{print $1 * 256 + $2}')
            if [ "$major" = "$want" ]; then
                printf "  [OK]      %-6s major %s (%s)\n" "$name" "$major" "$flags"
                ok=$((ok + 1))
            else
                printf "  [ERRO]    %-6s major %s, esperado %s\n" "$name" "$major" "$want"
                missing=$((missing + 1))
            fi
        else
            printf "  [ERRO]    %-6s javac falhou: %s\n" "$name" "$(head -n 1 "$tmp/$name.log")"
            missing=$((missing + 1))
        fi
    done
    rm -rf "$tmp"
fi

echo
echo "Resultado: $ok ok, $missing problema(s)"
[ $missing -eq 0 ]
