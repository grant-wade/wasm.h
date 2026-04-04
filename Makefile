CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -DWL_ENABLE_PLATFORM=1
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -g
LDFLAGS ?= 

.PHONY: all test clean wasm-emcc wasm-emcc-build wasm-emcc-run wasm-emcc-run-strict

all: test

wl_test: wl_test.c wl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) wl_test.c -o $@ $(LDFLAGS)

wasm_test: wasm_test.c wasm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) wasm_test.c -o $@ $(LDFLAGS) -lm


test: wl_test wasm_test
	./wl_test
	./wasm_test

wasm-emcc:
	$(MAKE) -C wasm run

wasm-emcc-build:
	$(MAKE) -C wasm all

wasm-emcc-run:
	$(MAKE) -C wasm run

wasm-emcc-run-strict:
	$(MAKE) -C wasm run-strict

clean:
	rm -f wl_test wasm_test
	$(MAKE) -C wasm clean
