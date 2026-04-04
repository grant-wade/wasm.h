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

CASES := basic_add function_pointer bulk_memory sign_extension switch_table start_constructor i64_return trunc_sat unresolved_import

basic_add_RUNNER_ARGS := call-i32 wl_case 10
basic_add_EMCCFLAGS := -O0

function_pointer_RUNNER_ARGS := call-i32 wl_case 37
function_pointer_EMCCFLAGS := -O1

bulk_memory_RUNNER_ARGS := call-i32 wl_case 228
bulk_memory_EMCCFLAGS := -O2 -mbulk-memory

sign_extension_RUNNER_ARGS := call-i32 wl_case -32895
sign_extension_EMCCFLAGS := -O2

switch_table_RUNNER_ARGS := call-i32 wl_case 40
switch_table_EMCCFLAGS := -O2

start_constructor_RUNNER_ARGS := call-i32 wl_case 77
start_constructor_EMCCFLAGS := -O0 -Wl,--export=__wasm_call_ctors

i64_return_RUNNER_ARGS := call-i64 wl_case 1099511627785
i64_return_EMCCFLAGS := -O1

trunc_sat_RUNNER_ARGS := call-i32 wl_case 255
trunc_sat_EMCCFLAGS := -O2

unresolved_import_RUNNER_ARGS := load-fail
unresolved_import_EMCCFLAGS := -O0 -Wl,--allow-undefined

ALL_WASM := $(addprefix $(OUT_DIR)/,$(addsuffix .wasm,$(CASES)))

.PHONY: all clean run run-strict list

all: $(RUNNER) $(ALL_WASM)

$(BUILD_DIR):
	mkdir -p $@

$(OUT_DIR):
	mkdir -p $@

$(RUNNER): $(RUNNER_DEPS) Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(ROOT) $(RUNNER_SRC) -o $@ $(LDFLAGS) -lm

define WASM_CASE_RULE
$(OUT_DIR)/$(1).wasm: $(FIXTURE_DIR)/$(1).c Makefile | $(OUT_DIR)
	$(EMCC) $$< -sSTANDALONE_WASM=1 -Wl,--no-entry -Wl,--export=wl_case $$($(1)_EMCCFLAGS) -o $$@
endef

$(foreach case,$(CASES),$(eval $(call WASM_CASE_RULE,$(case))))

list:
	@printf 'fixtures:\n'; \
	$(foreach case,$(CASES),printf '  %-20s %s\n' '$(case)' '$($(case)_RUNNER_ARGS)';)

run: all
	@passes=0; fails=0; \
	$(foreach case,$(CASES),printf '\n== %s ==\n' '$(case)'; \
	if ./$(RUNNER) $(OUT_DIR)/$(case).wasm $($(case)_RUNNER_ARGS); then \
		passes=$$((passes + 1)); \
	else \
		fails=$$((fails + 1)); \
	fi; ) \
	printf '\nsummary: %d passed, %d failed\n' $$passes $$fails

run-strict: all
	@passes=0; fails=0; \
	$(foreach case,$(CASES),printf '\n== %s ==\n' '$(case)'; \
	if ./$(RUNNER) $(OUT_DIR)/$(case).wasm $($(case)_RUNNER_ARGS); then \
		passes=$$((passes + 1)); \
	else \
		fails=$$((fails + 1)); \
	fi; ) \
	printf '\nsummary: %d passed, %d failed\n' $$passes $$fails; \
	test $$fails -eq 0

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)