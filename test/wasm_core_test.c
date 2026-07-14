#define WASM_IMPL
#include "wasm.h"

static const uint8_t wasm_core_test_bytes[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x08, 0x01, 0x04, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00,
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x0b
};

int main(void) {
    wasm_runtime_t* rt = wasm_runtime_new(NULL);
    wasm_module_t* mod;
    int32_t result = 0;
    int failed = 0;

    if (!rt || wasm_runtime_last_error(rt) != WASM_OK) {
        wasm_runtime_free(rt);
        return 1;
    }

    mod = wasm_load(rt, wasm_core_test_bytes, sizeof(wasm_core_test_bytes));
    if (!mod) {
        wasm_runtime_free(rt);
        return 1;
    }

    if (wasm_call_fmt(mod, "main", "i(i)", (int32_t)42, &result) != WASM_OK || result != 43)
        failed = 1;

    wasm_free_module(mod);
    wasm_runtime_free(rt);
    return failed;
}
