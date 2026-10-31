# =============================================================================
# Makefile da JVM didática (Software Básico - UnB)
#
# Alvos principais:
#   make               Compila o projeto com GCC (build/jvm)
#   make debug         Compila com -g e sanitizers ASan/UBSan (build/jvm-debug)
#   make test          Testes unitários
#   make test-integration  Testes de integração (programas Java em tests/java)
#   make check         test + test-integration
#   make windows       Gera build/jvm.exe com MinGW-w64 (GCC)
#   make test-windows  Testes de integração usando build/jvm.exe (via WSL)
#   make fuzz          Compila e roda os alvos do libFuzzer (Clang)
#   make fuzz-smoke    Verifica se o libFuzzer encontra o bug proposital
#   make docs          Gera a documentação Doxygen em build/docs/html
#   make check-env     Verifica as ferramentas instaladas
#   make clean         Remove build/
# =============================================================================

CC     := gcc
CLANG  := clang
MINGW  := x86_64-w64-mingw32-gcc

# Opções obrigatórias do projeto: C99 estrito e sem warnings.
STD      := -std=c99
WARN     := -Wall -Wextra -pedantic
WERROR   ?= -Werror
CPPFLAGS := -Iinclude
LDLIBS   := -lm

# Modo de compilação: release (padrão) ou debug.
MODE ?= release
ifeq ($(MODE),debug)
  BUILD    := build/debug
  BIN      := build/jvm-debug
  OPT      := -O0 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined
  LDFLAGS  := -fsanitize=address,undefined
else
  BUILD    := build/release
  BIN      := build/jvm
  OPT      := -O2
  LDFLAGS  :=
endif

CFLAGS := $(STD) $(WARN) $(WERROR) $(OPT)

# Fontes: tudo em src/; LIB_SRC exclui o main (usado em testes e fuzzing).
SRC     := $(shell find src -name '*.c' | sort)
LIB_SRC := $(filter-out src/main.c,$(SRC))
OBJ     := $(patsubst src/%.c,$(BUILD)/obj/%.o,$(SRC))
LIB_OBJ := $(patsubst src/%.c,$(BUILD)/obj/%.o,$(LIB_SRC))

TEST_SRC := $(wildcard tests/unit/*.c)
TEST_BIN := $(BUILD)/unit_tests

# Fuzzing (Clang + libFuzzer, também em C99).
FUZZ_FLAGS   := $(STD) -g -O1 -fsanitize=fuzzer,address,undefined $(CPPFLAGS)
FUZZ_TARGETS := $(filter-out fuzz_smoke,$(basename $(notdir $(wildcard fuzz/fuzz_*.c))))
FUZZ_TIME    ?= 60

.PHONY: all debug test test-integration check windows test-windows \
        fuzz fuzz-smoke docs check-env clean

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJ:.o=.d)

debug:
	$(MAKE) MODE=debug all

# ---------------------------------------------------------------- testes ----
$(TEST_BIN): $(TEST_SRC) $(LIB_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Itests/unit $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

test-integration: $(BIN)
	JVM_CMD="./$(BIN) -i" tests/run_tests.sh

check: test test-integration

# --------------------------------------------------------------- windows ----
windows: build/jvm.exe

build/jvm.exe: $(SRC) $(shell find include -name '*.h')
	@mkdir -p build
	$(MINGW) $(CPPFLAGS) $(STD) $(WARN) $(WERROR) -O2 -D__USE_MINGW_ANSI_STDIO=1 \
		$(SRC) -o $@ -static $(LDLIBS)

test-windows: build/jvm.exe
	JVM_CMD="./build/jvm.exe -i" tests/run_tests.sh

# ---------------------------------------------------------------- fuzzing ---
build/fuzz/%: fuzz/%.c $(LIB_SRC)
	@mkdir -p $(dir $@)
	$(CLANG) $(FUZZ_FLAGS) $< $(LIB_SRC) -o $@ $(LDLIBS)

fuzz: $(addprefix build/fuzz/,$(FUZZ_TARGETS))
	@if [ -z "$(FUZZ_TARGETS)" ]; then \
		echo "Nenhum alvo de fuzzing ainda (fuzz/fuzz_*.c). Use 'make fuzz-smoke'."; \
	fi
	@for t in $(FUZZ_TARGETS); do \
		echo "== Fuzzing $$t por $(FUZZ_TIME)s =="; \
		mkdir -p build/fuzz/corpus/$$t build/fuzz/artifacts/$$t fuzz/corpus/$$t; \
		./build/fuzz/$$t -max_total_time=$(FUZZ_TIME) \
			-artifact_prefix=build/fuzz/artifacts/$$t/ \
			build/fuzz/corpus/$$t fuzz/corpus/$$t || exit 1; \
	done

fuzz-smoke: build/fuzz/fuzz_smoke
	@mkdir -p build/fuzz/artifacts/fuzz_smoke
	@if ./build/fuzz/fuzz_smoke -max_total_time=120 \
		-artifact_prefix=build/fuzz/artifacts/fuzz_smoke/ \
		> build/fuzz/fuzz_smoke.log 2>&1; then \
		echo "ERRO: o libFuzzer NAO encontrou o bug proposital."; exit 1; \
	else \
		grep -m1 "ERROR: AddressSanitizer" build/fuzz/fuzz_smoke.log; \
		echo "OK: libFuzzer encontrou o bug proposital (log em build/fuzz/fuzz_smoke.log)."; \
	fi

# ------------------------------------------------------------ documentação --
docs:
	@mkdir -p build/docs
	doxygen Doxyfile
	@echo "Documentacao gerada em build/docs/html/index.html"

# ----------------------------------------------------------------- outros ---
check-env:
	@tools/check_env.sh

clean:
	rm -rf build
