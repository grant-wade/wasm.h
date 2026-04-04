CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -DWL_ENABLE_PLATFORM=1
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -g
LDFLAGS ?= 

.PHONY: all test clean

all: test

wl_test: wl_test.c wl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) wl_test.c -o $@ $(LDFLAGS)

wasm_test: wasm_test.c wasm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) wasm_test.c -o $@ $(LDFLAGS) -lm


test: wl_test wasm_test
	./wl_test
	./wasm_test

clean:
	rm -f wl_test wasm_test
