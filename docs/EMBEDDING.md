# Embedding wasm.h

Define `WASM_IMPL` in exactly one translation unit:

```c
#define WASM_IMPL
#include "wasm.h"
```

Elsewhere, include `wasm.h` without `WASM_IMPL`. For modular linking, see the [build guide](BUILDING.md#modular-library).

## Runtime and module lifetime

`wasm_runtime_t` is opaque to declaration-only users; allocate it with `wasm_runtime_new`.

```c
wasm_runtime_t* rt = wasm_runtime_new(NULL);
wasm_module_t* mod;

if (!rt || wasm_runtime_last_error(rt) != WASM_OK) {
    fprintf(stderr, "runtime init: %s\n",
            rt ? wasm_runtime_error_message(rt) : "out of memory");
    wasm_runtime_free(rt);
    return 1;
}

mod = wasm_load(rt, bytes, byte_count);
if (!mod) {
    fprintf(stderr, "module load: %s\n", wasm_runtime_error_message(rt));
    wasm_runtime_free(rt);
    return 1;
}

if (wasm_call(mod, "run", NULL, 0, NULL, 0) != WASM_OK)
    fprintf(stderr, "run: %s\n", wasm_runtime_error_message(rt));

wasm_free_module(mod);
wasm_runtime_free(rt);
```

`wasm_load` copies the input binary, so the caller may release its input buffer after the call returns. Free every module before freeing its runtime.

`wasm_init` and `wasm_destroy` support custom storage when the concrete runtime definition is available.

## Configuration

Start from `wasm_config_default` so new fields receive supported defaults.

```c
wasm_config_t config;
wasm_runtime_t* rt;

wasm_config_default(&config);
config.max_stack_values = 65536;
config.max_call_depth = 1024;
config.max_labels = 4096;
config.initial_gc_heap_size = 2u * 1024u * 1024u;
config.frame_arena_size = 4u * 1024u * 1024u;
config.lazy_validation = 0;

rt = wasm_runtime_new(&config);
```

With `lazy_validation` disabled, `wasm_load` validates all function bodies. Enabling it defers individual body validation until first execution.

All implemented standardized proposals are enabled by default; proposals still in the standardization process (currently `WASM_FEATURE_WIDE_ARITHMETIC`, `WASM_FEATURE_CUSTOM_PAGE_SIZES`, and `WASM_FEATURE_COMPACT_IMPORTS`) are enabled only by `wasm_enable_feature` or `wasm_enable_all_features`. Set feature flags before loading with `wasm_disable_feature`, `wasm_enable_feature`, or `wasm_runtime_set_enabled_features`.

## Calling exports

### Value-array API

```c
wasm_value_t args[2];
wasm_value_t results[1];
wasm_error_t err;

args[0] = wasm_i32(2);
args[1] = wasm_i32(40);
err = wasm_call(mod, "add", args, 2, results, 1);
if (err != WASM_OK) {
    fprintf(stderr, "add: %s\n", wasm_runtime_error_message(rt));
    return 1;
}
printf("%d\n", results[0].of.i32);
```

Use `wasm_call_index` with a cached function index to avoid repeated name lookups.

### Format-string API

`wasm_call_fmt` uses `params(results)` syntax:

| Character | Wasm type | C argument | Result destination |
| --- | --- | --- | --- |
| `i` | `i32` | `int32_t` | `int32_t*` |
| `I` | `i64` | `int64_t` | `int64_t*` |
| `f` | `f32` | `double` through varargs | `float*` |
| `F` | `f64` | `double` | `double*` |
| `r` | `externref` | `void*` | `void**` |
| `v` | no results | — | — |

```c
int32_t result;
wasm_error_t err = wasm_call_fmt(mod, "add", "ii(i)",
                                 (int32_t)2, (int32_t)40, &result);
```

The format must exactly match the exported function signature. Use the value-array API for types not represented by the format language, including `v128`, typed references, and GC references.

## Host function imports

Bind imports before calling `wasm_load`. A callback receives the calling module, typed value arrays, and the binding's userdata.

```c
static wasm_error_t host_log(wasm_module_t* caller,
                             const wasm_value_t* args, uint32_t num_args,
                             wasm_value_t* results, uint32_t num_results,
                             void* userdata) {
    (void)caller;
    (void)results;
    (void)num_results;
    (void)userdata;

    if (num_args != 1 || args[0].type != WASM_TYPE_I32)
        return WASM_ERR_TYPE_MISMATCH;

    printf("guest: %d\n", args[0].of.i32);
    return WASM_OK;
}

wasm_error_t err = wasm_bind_host_func(rt, "env", "log", "i(v)",
                                       host_log, NULL);
```

Lower-level registration supports functions, globals, tables, memories, and tags. Use `wasm_module_set_userdata` and `wasm_module_get_userdata` for per-module host state.

Enable platform compatibility helpers in the implementation translation unit:

```c
#define WASM_ENABLE_PLATFORM 1
#define WASM_IMPL
#include "wasm.h"
```

Bind helpers before loading:

```c
wasm_bind_wasi_stubs(rt);
wasm_bind_emscripten_stubs(rt);
```

These bindings are partial. WASI exposes standard I/O but preopens no directories; path operations requiring directory capabilities return `BADF`. Emscripten forwards some filesystem operations to the host: bind it only for trusted modules.

Strict C99 POSIX builds may need `_POSIX_C_SOURCE=200809L`; the repository build supplies it automatically.

## Linear memory and globals

Bounds-checked memory helpers take an explicit memory index and 64-bit guest offset.

```c
uint8_t data[32];
wasm_error_t err = wasm_memory_read(mod, 0, guest_offset,
                                    data, sizeof(data));
```

Use `wasm_memory_write` for writes and `wasm_memory_get_string` for bounded, null-terminated string extraction. The `*_at` accessors expose direct memory data, size, and growth for a selected memory. Reacquire direct data pointers after `wasm_memory_grow_at`, because growth may relocate storage.

Exported globals can be accessed by name with `wasm_global_get` and `wasm_global_set`. Index-based type and mutability introspection is available through the `wasm_global_*` accessors.

## Introspection

Inspect imports, exports, signatures, custom sections, memories, tables, globals, tags, and GC types through the public accessors. For example:

```c
uint32_t i;
for (i = 0; i < wasm_export_count(mod); i++) {
    printf("%s kind=%u index=%u\n",
           wasm_export_name(mod, i),
           (unsigned)wasm_export_kind(mod, i),
           (unsigned)wasm_export_index(mod, i));
}
```

Names may contain arbitrary bytes. Use `wasm_export_name_bytes` and `wasm_find_export_bytes` when a C string is not sufficient.

## Errors, traps, and limits

Most operations return `wasm_error_t`; loading returns `NULL` on failure. Read `wasm_runtime_error_message` immediately for details, or `wasm_error_string` for the generic error name.

Fuel metering is disabled until a budget is set:

```c
wasm_set_fuel(rt, 1000000);
err = wasm_call(mod, "run", NULL, 0, NULL, 0);
if (err == WASM_ERR_OUT_OF_FUEL) {
    fprintf(stderr, "instruction budget exhausted\n");
}
```

After a trap, use `wasm_dump_backtrace` or the raw call-frame accessors to inspect the captured Wasm stack.

```c
char trace[2048];
wasm_dump_backtrace(rt, trace, sizeof(trace));
fputs(trace, stderr);
```

## Custom allocators

Define allocator overrides before the implementation include:

```c
#define WASM_MALLOC(sz)      app_malloc(sz)
#define WASM_REALLOC(p, sz)  app_realloc((p), (sz))
#define WASM_FREE(p)         app_free(p)
#define WASM_CALLOC(n, sz)   app_calloc((n), (sz))
#define WASM_GC_ALLOC(sz)    app_gc_alloc(sz)
#define WASM_GC_FREE(p)      app_gc_free(p)
#define WASM_IMPL
#include "wasm.h"
```
