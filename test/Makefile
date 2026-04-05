CMAKE ?= cmake
ROOT ?= ..
BUILD_DIR ?= $(ROOT)/build
CONFIG ?=
CMAKE_ARGS ?=
BUILD_ARGS ?=

ifeq ($(strip $(CONFIG)),)
BUILD_CONFIG_ARGS :=
else
BUILD_CONFIG_ARGS := --config $(CONFIG)
endif

.PHONY: all configure run run-strict list clean

all: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wasm-emcc-build $(BUILD_ARGS)

configure:
	$(CMAKE) -S $(ROOT) -B $(BUILD_DIR) $(CMAKE_ARGS)

run: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wasm-emcc-run $(BUILD_ARGS)

run-strict: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wasm-emcc-run-strict $(BUILD_ARGS)

list: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target wasm-emcc-list $(BUILD_ARGS)

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)