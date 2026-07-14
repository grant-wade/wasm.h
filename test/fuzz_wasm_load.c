#include <stddef.h>
#include <stdint.h>

#define WASM_IMPL
#include "wasm.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    wasm_config_t config;
    wasm_runtime_t* runtime;
    wasm_module_t* module;

    wasm_config_default(&config);
    config.max_stack_values = 4096u;
    config.max_call_depth = 128u;
    config.max_labels = 1024u;
    config.initial_gc_heap_size = 256u * 1024u;
    config.frame_arena_size = 512u * 1024u;

    runtime = wasm_runtime_new(&config);
    if (!runtime || wasm_runtime_last_error(runtime) != WASM_OK) {
        wasm_runtime_free(runtime);
        return 0;
    }

    /* Bound start functions as well as calls made while instantiating a module. */
    wasm_set_fuel(runtime, UINT64_C(10000));
    module = wasm_load(runtime, data, size);
    wasm_free_module(module);
    wasm_runtime_free(runtime);
    return 0;
}
