#include "../internal.h"

#if WASM_ENABLE_PLATFORM
#else
wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt) {
    if (!rt) return WASM_ERR_MALFORMED;
    WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s",
                  "WASI stubs require WASM_ENABLE_PLATFORM=1");
    return WASM_ERR_MALFORMED;
}

wasm_error_t wasm_bind_emscripten_stubs(wasm_runtime_t* rt) {
    if (!rt) return WASM_ERR_MALFORMED;
    WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s",
                  "Emscripten stubs require WASM_ENABLE_PLATFORM=1");
    return WASM_ERR_MALFORMED;
}

#endif
