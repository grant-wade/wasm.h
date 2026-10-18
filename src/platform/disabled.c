#include "../runtime.h"


#if WASM_ENABLE_PLATFORM
#else
wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt) {
    if (!rt) return WASM_ERR_MALFORMED;
    WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s",
                  "WASI stubs require WASM_ENABLE_PLATFORM=1");
    return WASM_ERR_MALFORMED;
}

static wasm_error_t wasm__wasi_requires_platform(wasm_runtime_t* rt) {
    if (!rt) return WASM_ERR_MALFORMED;
    WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s",
                  "WASI configuration requires WASM_ENABLE_PLATFORM=1");
    return WASM_ERR_MALFORMED;
}

wasm_error_t wasm_wasi_set_args(wasm_runtime_t* rt, uint32_t argc, const char* const* argv) {
    (void)argc;
    (void)argv;
    return wasm__wasi_requires_platform(rt);
}

wasm_error_t wasm_wasi_set_env(wasm_runtime_t* rt, uint32_t count, const char* const* env) {
    (void)count;
    (void)env;
    return wasm__wasi_requires_platform(rt);
}

wasm_error_t wasm_wasi_preopen_dir(wasm_runtime_t* rt, const char* host_path, const char* guest_path) {
    (void)host_path;
    (void)guest_path;
    return wasm__wasi_requires_platform(rt);
}

wasm_error_t wasm_bind_emscripten_stubs(wasm_runtime_t* rt) {
    if (!rt) return WASM_ERR_MALFORMED;
    WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s",
                  "Emscripten stubs require WASM_ENABLE_PLATFORM=1");
    return WASM_ERR_MALFORMED;
}

#endif
