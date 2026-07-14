# Embedding wasm.h

`wasm.h` is a C99 single-header runtime. Include it normally wherever declarations are needed and define `WASM_IMPL` in exactly one translation unit.

```c
/* wasm_runtime.c */
#define WASM_IMPL
#include "wasm.h"
```

```c
/* application.c */
#include "wasm.h"
```

## Runtime and module lifetime

The allocation-based lifecycle is the recommended interface because `wasm_runtime_t` is opaque to declaration-only users.

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

/* Use mod here. */

wasm_free_module(mod);
wasm_runtime_free(rt);
```

`wasm_load` copies the input binary, so the caller may release its input buffer after the call returns. Free every module before freeing its runtime.

For custom storage, `wasm_init` and `wasm_destroy` provide the non-allocating lifecycle when the implementation's concrete runtime definition is available. Most embedders should use `wasm_runtime_new` and `wasm_runtime_free` instead.

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

All implemented proposal features are enabled by default. Use `wasm_disable_feature`, `wasm_enable_feature`, or `wasm_runtime_set_enabled_features` before loading a module when the embedder needs a narrower accepted feature set.

## Calling exports

### Value-array API

Construct `wasm_value_t` values with the provided helpers and inspect the matching union member in each result.

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

Use `wasm_call_index` after resolving and caching a function index when repeated calls should avoid export-name lookup.

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

Lower-level registration functions are available for functions, globals, tables, memories, and tags when format strings are insufficient. `wasm_module_set_userdata` associates instance-specific host state with a loaded module; callbacks can retrieve it with `wasm_module_get_userdata`.

Optional compatibility helpers are available for common imports. Their OS-specific code is excluded from the portable core by default; enable it in the implementation translation unit when needed:

```c
#define WASM_ENABLE_PLATFORM 1
#define WASM_IMPL
#include "wasm.h"
```

The helpers can then be bound before loading a module:

```c
wasm_bind_wasi_stubs(rt);
/* Bind only for trusted toolchain-produced modules. */
wasm_bind_emscripten_stubs(rt);
```

These are convenience bindings, not complete implementations of either environment. The WASI helper does not preopen a host directory: it exposes standard I/O, but path operations have no ambient filesystem capability and return `BADF`. The Emscripten helper is more permissive and forwards some guest-requested filesystem operations to the host, so it must only be enabled for trusted modules. Strict C99 builds on POSIX systems may also need `_POSIX_C_SOURCE=200809L`; the repository's CMake build supplies it automatically.

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

The public API exposes imports, exports, function signatures, custom sections, memories, tables, globals, tags, and GC composite types. A typical export walk is:

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

Most operations return `wasm_error_t`; module loading returns `NULL` on failure. Read `wasm_runtime_error_message` immediately for the runtime's detailed diagnostic. `wasm_error_string` provides the generic name for an error code.

Fuel metering is disabled until a budget is set:

```c
wasm_set_fuel(rt, 1000000);
err = wasm_call(mod, "run", NULL, 0, NULL, 0);
if (err == WASM_ERR_OUT_OF_FUEL) {
    /* The guest exhausted its instruction budget. */
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

The declarations-only include does not need these definitions.
