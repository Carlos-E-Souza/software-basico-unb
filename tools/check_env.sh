#!/usr/bin/env bash
# =============================================================================
# Verifica se o ambiente de desenvolvimento está completo.
#
# Checa: GCC (+ bibliotecas de 32 bits), Make, MinGW-w64 i686, Clang (+ runtime
# do libFuzzer em 32 bits), Doxygen, Graphviz e JDK 8, e se o javac gera
# bytecode com major 46, 50 e 52.
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
check_tool i686-w64-mingw32-gcc   "i686-w64-mingw32-gcc --version"
check_tool clang                  "clang --version"
check_tool doxygen                "doxygen --version"
check_tool dot                    "dot -V"
check_tool cppcheck               "cppcheck --version"
check_tool python3                "python3 --version"
check_tool javac                  "javac -version"
check_tool java                   "java -version"

# Bibliotecas de 32 bits (-m32, requisito R10): compila e executa um programa
# mínimo com GCC (normal e com sanitizers) e liga um alvo do libFuzzer com Clang.
check_build() {
    local label="$1" hint="$2"; shift 2
    if "$@" > /dev/null 2>&1; then
        printf "  [OK]      %s\n" "$label"
        ok=$((ok + 1))
    else
        printf "  [AUSENTE] %s (%s)\n" "$label" "$hint"
        missing=$((missing + 1))
    fi
}

tmp="$(mktemp -d)"
printf '#include <stdio.h>\nint main(void){return sizeof(void *) == 4 ? 0 : 1;}\n' > "$tmp/m32.c"
printf '#include <stdint.h>\n#include <stddef.h>\nint LLVMFuzzerTestOneInput(const uint8_t *d, size_t s){(void)d;(void)s;return 0;}\n' > "$tmp/f.c"
if command -v gcc > /dev/null 2>&1; then
    check_build "gcc -m32" "instale gcc-multilib" \
        sh -c "gcc -m32 -std=c99 '$tmp/m32.c' -o '$tmp/m32' && '$tmp/m32'"
    check_build "gcc -m32 com ASan/UBSan" "instale gcc-multilib" \
        sh -c "gcc -m32 -std=c99 -fsanitize=address,undefined '$tmp/m32.c' -o '$tmp/m32s' && '$tmp/m32s'"
fi
if command -v clang > /dev/null 2>&1; then
    check_build "libFuzzer 32 bits (clang -m32 -fsanitize=fuzzer)" "instale libclang-rt-dev e g++-multilib" \
        clang -m32 -std=c99 -fsanitize=fuzzer "$tmp/f.c" -o "$tmp/f"
fi
rm -rf "$tmp"

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
