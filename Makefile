# =============================================================================
# Makefile da JVM (Software Básico - UnB)
#
# Alvos principais:
#   make               Compila o projeto com GCC (build/jvm)
#   make debug         Compila com -g e sanitizers ASan/UBSan (build/jvm-debug)
#   make test          Testes unitários
#   make test-integration  Testes de integração (programas Java em tests/java)
#   make test-viewer   Compara o exibidor com o javap em todos os fixtures
#   make check         test + test-viewer + test-integration
#   make windows       Gera build/jvm.exe com MinGW-w64 (GCC)
#   make test-windows  Testes de integração usando build/jvm.exe (via WSL)
#   make fuzz          Compila e roda os alvos do libFuzzer (Clang), FUZZ_TIME=60 s
#   make fuzz-checkpoint  Sessão de fuzzing dos checkpoints (10 min por alvo)
#   make fuzz-smoke    Verifica se o libFuzzer encontra o bug proposital
#   make docs          Gera a documentação Doxygen em docs/html
#   make check-env     Verifica as ferramentas instaladas
#   make clean         Remove build/ e a documentação gerada (docs/html)
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

# Fixtures: classes Java compiladas para Java 2, 6 e 8 (usadas nos testes e
# como corpus inicial do fuzzer). As de tests/fixtures/java8 só existem no 8.
JAVAC          := javac
JAVAC_FLAGS    := -g -nowarn -Xlint:-options -encoding UTF-8
FIXTURE_COMMON := $(wildcard tests/fixtures/common/*.java)
FIXTURE_JAVA8  := $(wildcard tests/fixtures/java8/*.java)
FIXTURES_DIR   := build/fixtures
FIXTURES_STAMP := $(FIXTURES_DIR)/.stamp

# Fuzzing (Clang + libFuzzer, também em C99).
FUZZ_FLAGS   := $(STD) -g -O1 -fsanitize=fuzzer,address,undefined $(CPPFLAGS)
FUZZ_TARGETS := $(filter-out fuzz_smoke,$(basename $(notdir $(wildcard fuzz/fuzz_*.c))))
FUZZ_TIME    ?= 60
# Duração padrão das sessões de fuzzing dos checkpoints (10 minutos).
FUZZ_CHECKPOINT_TIME ?= 600

.PHONY: all debug test test-integration check windows test-windows \
        fuzz fuzz-checkpoint fuzz-smoke docs check-env clean fixtures test-viewer

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
fixtures: $(FIXTURES_STAMP)

$(FIXTURES_STAMP): $(FIXTURE_COMMON) $(FIXTURE_JAVA8)
	rm -rf $(FIXTURES_DIR)
	mkdir -p $(FIXTURES_DIR)/java2 $(FIXTURES_DIR)/java6 $(FIXTURES_DIR)/java8
	$(JAVAC) $(JAVAC_FLAGS) -source 1.2 -target 1.2 -d $(FIXTURES_DIR)/java2 $(FIXTURE_COMMON)
	$(JAVAC) $(JAVAC_FLAGS) -source 1.6 -target 1.6 -d $(FIXTURES_DIR)/java6 $(FIXTURE_COMMON)
	$(JAVAC) $(JAVAC_FLAGS) -source 1.8 -target 1.8 -d $(FIXTURES_DIR)/java8 $(FIXTURE_COMMON) $(FIXTURE_JAVA8)
	@touch $@

$(TEST_BIN): $(TEST_SRC) $(wildcard tests/unit/*.h) $(LIB_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Itests/unit -DFIXTURES_DIR=\"$(FIXTURES_DIR)\" $(CFLAGS) $(LDFLAGS) \
		$(TEST_SRC) $(LIB_OBJ) -o $@ $(LDLIBS)

test: $(TEST_BIN) $(FIXTURES_STAMP)
	./$(TEST_BIN)

test-integration: $(BIN)
	JVM_CMD="./$(BIN) -i" tests/run_tests.sh

test-viewer: $(BIN) $(FIXTURES_STAMP)
	JVM=./$(BIN) tools/compare_javap.sh $$(find $(FIXTURES_DIR) -name '*.class' | sort)

check: test test-viewer test-integration

# --------------------------------------------------------------- windows ----
windows: build/jvm.exe

build/jvm.exe: $(SRC) $(shell find include -name '*.h')
	@mkdir -p build
	$(MINGW) $(CPPFLAGS) $(STD) $(WARN) $(WERROR) -O2 -D__USE_MINGW_ANSI_STDIO=1 \
		$(SRC) -o $@ -static $(LDLIBS)

build/unit_tests.exe: $(TEST_SRC) $(wildcard tests/unit/*.h) $(LIB_SRC) $(shell find include -name '*.h')
	@mkdir -p build
	$(MINGW) $(CPPFLAGS) -Itests/unit -DFIXTURES_DIR=\"$(FIXTURES_DIR)\" $(STD) $(WARN) $(WERROR) \
		-O2 -D__USE_MINGW_ANSI_STDIO=1 $(TEST_SRC) $(LIB_SRC) -o $@ -static $(LDLIBS)

# Testes unitários e de integração executados no Windows (via WSL).
test-windows: build/jvm.exe build/unit_tests.exe $(FIXTURES_STAMP)
	./build/unit_tests.exe
	JVM_CMD="./build/jvm.exe -i" tests/run_tests.sh

# ---------------------------------------------------------------- fuzzing ---
build/fuzz/%: fuzz/%.c $(LIB_SRC)
	@mkdir -p $(dir $@)
	$(CLANG) $(FUZZ_FLAGS) $< $(LIB_SRC) -o $@ $(LDLIBS)

fuzz: $(addprefix build/fuzz/,$(FUZZ_TARGETS)) $(FIXTURES_STAMP)
	@if [ -z "$(FUZZ_TARGETS)" ]; then \
		echo "Nenhum alvo de fuzzing ainda (fuzz/fuzz_*.c). Use 'make fuzz-smoke'."; \
	fi
	@for t in $(FUZZ_TARGETS); do \
		echo "== Fuzzing $$t por $(FUZZ_TIME)s =="; \
		mkdir -p build/fuzz/corpus/$$t build/fuzz/artifacts/$$t fuzz/corpus/$$t; \
		dict=""; [ -f fuzz/$$t.dict ] && dict="-dict=fuzz/$$t.dict"; \
		./build/fuzz/$$t -max_total_time=$(FUZZ_TIME) $$dict \
			-artifact_prefix=build/fuzz/artifacts/$$t/ \
			build/fuzz/corpus/$$t fuzz/corpus/$$t $(FIXTURES_DIR) || exit 1; \
	done

fuzz-checkpoint:
	$(MAKE) fuzz FUZZ_TIME=$(FUZZ_CHECKPOINT_TIME)

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
	doxygen Doxyfile
	@echo "Documentacao gerada em docs/html/index.html"

# ----------------------------------------------------------------- outros ---
check-env:
	@tools/check_env.sh

clean:
	rm -rf build docs/html
