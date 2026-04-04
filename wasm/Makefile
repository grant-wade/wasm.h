CC ?= cc
EMCC ?= emcc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -g
LDFLAGS ?=

ROOT := ..
FIXTURE_DIR := fixtures
BUILD_DIR := build
OUT_DIR := out
RUNNER := $(BUILD_DIR)/wasm_runner
RUNNER_SRC := runner.c
RUNNER_DEPS := $(RUNNER_SRC) $(ROOT)/wasm.h

CASES := basic_add function_pointer bulk_memory

basic_add_EXPORT := wl_case
basic_add_RESULT := i32
basic_add_EXPECT := 10
basic_add_EMCCFLAGS := -O0

function_pointer_EXPORT := wl_case
function_pointer_RESULT := i32
function_pointer_EXPECT := 37
function_pointer_EMCCFLAGS := -O1

bulk_memory_EXPORT := wl_case
bulk_memory_RESULT := i32
bulk_memory_EXPECT := 228
bulk_memory_EMCCFLAGS := -O2 -mbulk-memory

ALL_WASM := $(addprefix $(OUT_DIR)/,$(addsuffix .wasm,$(CASES)))

.PHONY: all clean run run-strict list

all: $(RUNNER) $(ALL_WASM)

$(BUILD_DIR):
	mkdir -p $@

$(OUT_DIR):
	mkdir -p $@

$(RUNNER): $(RUNNER_DEPS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(ROOT) $(RUNNER_SRC) -o $@ $(LDFLAGS) -lm

define WASM_CASE_RULE
$(OUT_DIR)/$(1).wasm: $(FIXTURE_DIR)/$(1).c | $(OUT_DIR)
	$(EMCC) $$< -sSTANDALONE_WASM=1 -Wl,--no-entry -Wl,--export=$$($(1)_EXPORT) $$($(1)_EMCCFLAGS) -o $$@
endef

$(foreach case,$(CASES),$(eval $(call WASM_CASE_RULE,$(case))))

list:
	@printf 'fixtures:\n'; \
	$(foreach case,$(CASES),printf '  %-18s export=%s result=%s expect=%s\n' '$(case)' '$($(case)_EXPORT)' '$($(case)_RESULT)' '$($(case)_EXPECT)';)

run: all
	@passes=0; fails=0; \
	$(foreach case,$(CASES),printf '\n== %s ==\n' '$(case)'; \
	if ./$(RUNNER) $(OUT_DIR)/$(case).wasm $($(case)_EXPORT) $($(case)_RESULT) $($(case)_EXPECT); then \
		passes=$$((passes + 1)); \
	else \
		fails=$$((fails + 1)); \
	fi; ) \
	printf '\nsummary: %d passed, %d failed\n' $$passes $$fails

run-strict: all
	@passes=0; fails=0; \
	$(foreach case,$(CASES),printf '\n== %s ==\n' '$(case)'; \
	if ./$(RUNNER) $(OUT_DIR)/$(case).wasm $($(case)_EXPORT) $($(case)_RESULT) $($(case)_EXPECT); then \
		passes=$$((passes + 1)); \
	else \
		fails=$$((fails + 1)); \
	fi; ) \
	printf '\nsummary: %d passed, %d failed\n' $$passes $$fails; \
	test $$fails -eq 0

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)