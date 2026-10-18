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

### Compiling and instantiating separately

`wasm_load` decodes, validates, and instantiates in one step: it resolves imports, allocates memories and tables, applies data and element segments, and runs the start function. `wasm_compile` does only the first part. Its result needs no imports and runs no code, so it can inspect a module of unknown origin, and it can be instantiated any number of times:

```c
wasm_module_t* compiled = wasm_compile(rt, bytes, byte_count);
wasm_module_t* instance;

if (!compiled) { /* malformed or invalid: wasm_runtime_error_message(rt) */ }
for (uint32_t i = 0; i < wasm_import_count(compiled); i++) {
    const wasm_import_info_t* info = wasm_import_info(compiled, i);
    /* register a binding for info->module / info->name */
}
instance = wasm_instantiate(compiled);
/* ... */
wasm_free_module(instance);
wasm_free_module(compiled);
```

A compiled module supports the introspection functions, but calls, memory access, and global reads fail. `wasm_instantiate` resolves imports registered on the runtime at that point and does not validate function bodies again. Each instance has its own globals, memories, and tables, and is freed independently of the compiled module.

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
config.max_function_locals = 50000;
config.max_memory_bytes = 256u * 1024u * 1024u;
config.max_table_elements = 1000000;

rt = wasm_runtime_new(&config);
```

With `lazy_validation` disabled, `wasm_load` validates all function bodies. Enabling it defers individual body validation until first execution.

The remaining fields bound what a module can make the host allocate; 0 removes a limit:

- `max_function_locals` (default 50,000) rejects functions that declare more locals, parameters included.
- `max_memory_bytes` (default 0, unlimited) caps each linear memory. A module whose memory starts larger fails to load with `WASM_ERR_OOM`; `memory.grow` and `wasm_memory_grow_at` past the cap return -1.
- `max_table_elements` (default 10,000,000) caps each table the same way, at instantiation and for `table.grow`.

Set `max_memory_bytes` when running untrusted modules: without it a module can grow each memory to the 4 GiB its type allows, or further with Memory64.

All implemented standardized proposals are enabled by default; proposals still in the standardization process (currently `WASM_FEATURE_WIDE_ARITHMETIC`, `WASM_FEATURE_CUSTOM_PAGE_SIZES`, `WASM_FEATURE_COMPACT_IMPORTS`, and `WASM_FEATURE_THREADS`) are enabled only by `wasm_enable_feature` or `wasm_enable_all_features`. Set feature flags before loading with `wasm_disable_feature`, `wasm_enable_feature`, or `wasm_runtime_set_enabled_features`.

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

Each result starts as the zero or null value of its declared type, so a callback may set only the payload, such as `results[0].of.i32`. Results whose type does not match the import's signature fail the call with `WASM_ERR_TYPE_MISMATCH`.

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

Configure WASI per runtime, before or after binding. The guest can open files only beneath preopened directories:

```c
static const char* const args[] = { "app", "input.txt" };
static const char* const env[] = { "LANG=C" };

wasm_wasi_set_args(rt, 2, args);
wasm_wasi_set_env(rt, 1, env);
wasm_wasi_preopen_dir(rt, "./data", "/data");
```

These bindings are partial. Without a preopen, path operations return `BADF`. `poll_oneoff` sleeps for clock subscriptions and reports file descriptor subscriptions ready at once. `proc_raise`, the socket calls, and the timestamp setters return `NOSYS`. Arguments and environment default to the `WASM_WASI_ARG_COUNT`, `WASM_WASI_ARG_AT`, `WASM_WASI_ENV_COUNT`, and `WASM_WASI_ENV_AT` macros, which return none unless defined. Emscripten forwards some filesystem operations to the host: bind it only for trusted modules.

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

## GC references

The collector runs when the GC heap fills, when enough `externref` conversions and caught `exnref` values accumulate, and on `wasm_gc_collect`. It keeps what globals, tables, element segments, and running frames reach, and moves GC objects. A GC reference, converted `externref`, or `exnref` held only by the host is valid until the next collection: store values the host keeps in a global or table.

## Threads

With `WASM_FEATURE_THREADS` enabled, modules can declare shared memories and use the atomic instructions. A runtime and the modules loaded into it belong to one thread at a time; shared memories are what threads have in common. To run Wasm on several threads, give each thread its own runtime, load the module into each, and import the same shared memory everywhere:

```c
wasm_memory_t* memory = wasm_shared_memory_new(1, 16, 0); /* pages, max pages, is_64 */
wasm_memory_import_t imp = { "env", "memory", memory };

/* On each thread: */
wasm_runtime_t* rt = wasm_runtime_new(NULL);
wasm_enable_feature(rt, WASM_FEATURE_THREADS);
wasm_register_memory_import(rt, &imp);
wasm_module_t* mod = wasm_load(rt, bytes, len);
```

A shared memory exported by one module can be imported the same way. Free it with `wasm_shared_memory_free` (or free its owning module) only after every importing module is freed. Shared memories never move when they grow, so their data pointers stay valid.

`memory.atomic.wait32`/`wait64` block and `memory.atomic.notify` wakes threads when the platform layer is compiled in (`WASM_ENABLE_PLATFORM`), using pthreads or Win32; link with `-pthread` on toolchains that need it. Without it, a wait that would block traps, because no other thread could wake it.

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

A guest that calls WASI `proc_exit` stops at once: the call (or `wasm_load`, from a start function) fails with `WASM_ERR_EXIT`, and `wasm_runtime_exit_code` returns the status it passed.

Fuel metering is disabled until a budget is set:

```c
wasm_set_fuel(rt, 1000000);
err = wasm_call(mod, "run", NULL, 0, NULL, 0);
if (err == WASM_ERR_OUT_OF_FUEL) {
    fprintf(stderr, "instruction budget exhausted\n");
}
```

`wasm_disable_fuel` turns metering off again.

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
