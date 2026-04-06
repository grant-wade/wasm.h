CMAKE ?= cmake
BUILD_DIR ?= build
CONFIG ?=
CMAKE_ARGS ?=
BUILD_ARGS ?=

ifeq ($(strip $(CONFIG)),)
BUILD_CONFIG_ARGS :=
else
BUILD_CONFIG_ARGS := --config $(CONFIG)
endif

NATIVE_TARGETS := wl_test wasm_test wasm2api wasm basic_add_demo

.PHONY: all configure build test check clean wabt-tools wasm-emcc wasm-emcc-build wasm-emcc-run wasm-emcc-run-strict wasm-emcc-list $(NATIVE_TARGETS)

all: test

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) $(BUILD_ARGS)

test: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target check $(BUILD_ARGS)

check: test

$(NATIVE_TARGETS): configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target $@ $(BUILD_ARGS)

wabt-tools: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wabt-tools $(BUILD_ARGS)

wasm-emcc: wasm-emcc-run

wasm-emcc-build: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wasm-emcc-build $(BUILD_ARGS)

wasm-emcc-run: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wasm-emcc-run $(BUILD_ARGS)

wasm-emcc-run-strict: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wasm-emcc-run-strict $(BUILD_ARGS)

wasm-emcc-list: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wasm-emcc-list $(BUILD_ARGS)

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)
