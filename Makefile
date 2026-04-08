CMAKE ?= cmake
BUILD_DIR ?= build
CONFIG ?=
CMAKE_ARGS ?=
BUILD_ARGS ?=
TARGET ?=

ifeq ($(strip $(CONFIG)),)
BUILD_CONFIG_ARGS :=
else
BUILD_CONFIG_ARGS := --config $(CONFIG)
endif

COMMON_CMAKE_TARGETS := \
	wl_test \
	wasm_test \
	wasm2api \
	wasm \
	wasm_runner \
	spectest_runner \
	session_math_wasm \
	session_math_generate \
	session_math_demo \
	spectest-support-build \
	wasm-spectest-build \
	wasm-spectest-run \
	wasm-spectest-run-strict \
	wasm-spectest \
	wasm-emcc-build \
	wasm-emcc-run \
	wasm-emcc-run-strict \
	wasm-emcc \
	wasm-emcc-list


.PHONY: all help configure build target list-targets targets test check clean $(COMMON_CMAKE_TARGETS)

all: build

help:
	@printf '%s\n' \
		'make all                           Build the default CMake target graph' \
		'make configure                     Configure CMake into $(BUILD_DIR)' \
		'make build                         Build the default CMake target' \
		'make test | make check            Build and run the full check target' \
		'make <common-target>              Build a mirrored CMake target exposed by this Makefile' \
		'make spectest_case_<name>         Build one generated spectest case target when available' \
		'make target TARGET=<cmake-target> Build any configured CMake target by name' \
		'make list-targets                 Show the configured CMake target list' \
		'' \
		'Conditional targets depend on the current CMake configuration and available tools.'

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_ARGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) $(BUILD_ARGS)

target: configure
	@if [ -z "$(strip $(TARGET))" ]; then \
		echo 'TARGET is required, e.g. make target TARGET=wasm-spectest-run' >&2; \
		exit 2; \
	fi
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target $(TARGET) $(BUILD_ARGS)

list-targets: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target help

targets: list-targets

test: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target check $(BUILD_ARGS)

check: test


$(COMMON_CMAKE_TARGETS): configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target $@ $(BUILD_ARGS)

spectest_case_%: configure
	$(CMAKE) --build $(BUILD_DIR) $(BUILD_CONFIG_ARGS) --target $@ $(BUILD_ARGS)

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)
