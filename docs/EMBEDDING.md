# Embedding wasm.h

Define `WASM_IMPL` in exactly one translation unit:

```c
#define WASM_IMPL
#include "wasm.h"
```

Elsewhere, include `wasm.h` without `WASM_IMPL`. For modular linking, see the [build guide](BUILDING.md#modular-library).

## Runtime and module lifetime

`wasm_runtime_t` and `wasm_module_t` are opaque. `wasm_runtime_new` takes an allocator (see [Custom allocators](#custom-allocators)) and a configuration, which may be NULL for the defaults, and returns NULL when allocation fails or either is invalid.

```c
wasm_runtime_t* rt = wasm_runtime_new(wasm_libc_allocator(), NULL);
wasm_module_t* mod;

if (!rt) return 1;

mod = wasm_load(rt, bytes, byte_count);
if (!mod) {
    fprintf(stderr, "module load: %s\n", wasm_runtime_error_message(rt));
    wasm_runtime_free(rt);
    return 1;
}

if (wasm_call(mod, "run", NULL, 0, NULL, 0) != WASM_OK)
    fprintf(stderr, "run: %s\n", wasm_runtime_error_message(rt));

wasm_module_free(mod);
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
wasm_module_free(instance);
wasm_module_free(compiled);
```

A compiled module supports the introspection functions, but calls, memory access, and global reads fail. `wasm_instantiate` resolves imports registered on the runtime at that point and does not validate function bodies again. Each instance has its own globals, memories, and tables, and is freed independently of the compiled module.

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
config.frame_arena_size = 256u * 1024u;
config.lazy_validation = 0;
config.max_function_locals = 50000;
config.max_memory_bytes = 256u * 1024u * 1024u;
config.max_total_memory_bytes = 1024u * 1024u * 1024u;
config.max_table_elements = 1000000;
config.max_gc_heap_bytes = 512u * 1024u * 1024u;

rt = wasm_runtime_new(wasm_libc_allocator(), &config);
```

With `lazy_validation` disabled, `wasm_load` validates all function bodies. Enabling it defers individual body validation until first execution. Call frames live in blocks of `frame_arena_size` bytes, added as calls nest deeper and released as they return.

The remaining fields bound what a module can make the host allocate; 0 removes a limit:

- `max_function_locals` (default 50,000) rejects functions that declare more locals, parameters included.
- `max_memory_bytes` (default 0, unlimited) caps each linear memory. A module whose memory starts larger fails to load with `WASM_ERR_OOM`; `memory.grow` and `wasm_memory_grow` past the cap return -1. A shared memory reserves address space only up to this cap.
- `max_total_memory_bytes` (default 0, unlimited) caps the linear memories that all of the runtime's instances define, together, in the same way. Memories the host creates and registers do not count.
- `max_table_elements` (default 10,000,000) caps each table the same way, at instantiation and for `table.grow`.
- `max_gc_heap_bytes` (default 0, unlimited) caps the GC heap, including `initial_gc_heap_size`. The heap grows when a collection leaves too little room, and an allocation that needs more than the cap fails with `WASM_ERR_OOM`.

Set `max_memory_bytes` when running untrusted modules: without it a module can grow each memory to the 4 GiB its type allows, or further with Memory64.

All implemented standardized proposals are enabled by default; proposals still in the standardization process (currently `WASM_FEATURE_WIDE_ARITHMETIC`, `WASM_FEATURE_CUSTOM_PAGE_SIZES`, `WASM_FEATURE_COMPACT_IMPORTS`, and `WASM_FEATURE_THREADS`) are enabled only by `wasm_runtime_enable_feature` or `wasm_runtime_enable_all_features`. Set feature flags before loading with `wasm_runtime_disable_feature`, `wasm_runtime_enable_feature`, or `wasm_runtime_set_enabled_features`.

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

Lower-level registration supports functions, globals, tables, memories, and tags. Create host memories and tables to register with `wasm_memory_new` and `wasm_table_new`, which take an allocator, and free them with `wasm_memory_free` and `wasm_table_free` after every module that imports them. Use `wasm_module_set_userdata` and `wasm_module_userdata` for per-module host state.

### Linking modules

`wasm_register_module` makes every export of an instance importable by modules loaded later in the same runtime:

```c
wasm_module_t* lib = wasm_load(rt, lib_bytes, lib_len);
wasm_register_module(rt, "lib", lib);
wasm_module_t* app = wasm_load(rt, app_bytes, app_len); /* may import "lib" "..." */
```

Importers share the exporting instance's globals, tables, memories, and tags, and call its functions, so free `lib` only after the modules that import from it. A call into a freed module's function traps.

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

Use `wasm_memory_write` for writes and `wasm_memory_get_string` for bounded, null-terminated string extraction. `wasm_memory_data`, `wasm_memory_size`, and `wasm_memory_grow` expose a memory's data, byte size, and growth. Reacquire direct data pointers after `wasm_memory_grow`, because growth may relocate storage.

Exported globals can be accessed by name with `wasm_global_get` and `wasm_global_set`, and any global by index with `wasm_global_get_index` and `wasm_global_set_index`; setters check mutability and type. Index-based type and mutability introspection is available through the `wasm_global_*` accessors.

Tables are read and changed with `wasm_table_size`, `wasm_table_get`, `wasm_table_set`, and `wasm_table_grow`. `wasm_call_ref` calls the function a funcref refers to, such as one read from a table.

## GC references

The collector runs when the GC heap fills, when enough `externref` conversions and caught `exnref` values accumulate, and on `wasm_gc_collect`. It keeps what globals, tables, element segments, and running frames reach, and moves GC objects. A host value that reaches Wasm as an `anyref`, directly or through `any.convert_extern`, comes back to the host with its original bits. A GC reference, an `externref` made by `extern.convert_any`, or an `exnref` held only by the host is valid until the next collection. Keep one longer with `wasm_ref_hold`, which returns a slot the collector keeps current, and read the reference from that slot when using it:

```c
wasm_value_t* held = wasm_ref_hold(rt, point);
/* ... calls that may collect ... */
wasm_struct_get_field(mod, *held, 0, &x);
wasm_ref_release(rt, held);
```

## Threads

With `WASM_FEATURE_THREADS` enabled, modules can declare shared memories and use the atomic instructions. A runtime and the modules loaded into it belong to one thread at a time; shared memories are what threads have in common. To run Wasm on several threads, give each thread its own runtime, load the module into each, and import the same shared memory everywhere:

```c
wasm_memory_t* memory = wasm_shared_memory_new(wasm_libc_allocator(), 1, 16, 0); /* pages, max pages, is_64 */
wasm_memory_import_t imp = { "env", "memory", memory };

/* On each thread: */
wasm_runtime_t* rt = wasm_runtime_new(wasm_libc_allocator(), NULL);
wasm_runtime_enable_feature(rt, WASM_FEATURE_THREADS);
wasm_register_memory_import(rt, &imp);
wasm_module_t* mod = wasm_load(rt, bytes, len);
```

A shared memory exported by one module can be imported the same way. Free it with `wasm_memory_free` (or free its owning module) only after every importing module is freed. Shared memories never move when they grow, so their data pointers stay valid. `wasm_memory_base` and `wasm_memory_length` return the data and byte size of a memory handle.

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

Most operations return `wasm_error_t`; loading returns `NULL` on failure. Loads and calls clear the previous error, and after a failure `wasm_runtime_last_error` returns its code and `wasm_runtime_error_message` describes it, such as `integer divide by zero` for a trap. `wasm_error_string` gives the generic error name.

A guest that calls WASI `proc_exit` stops at once: the call (or `wasm_load`, from a start function) fails with `WASM_ERR_EXIT`, and `wasm_runtime_exit_code` returns the status it passed.

Fuel metering is disabled until a budget is set:

```c
wasm_runtime_set_fuel(rt, 1000000);
err = wasm_call(mod, "run", NULL, 0, NULL, 0);
if (err == WASM_ERR_OUT_OF_FUEL) {
    fprintf(stderr, "instruction budget exhausted\n");
    wasm_runtime_cancel(rt);
}
```

Every instruction costs one unit. Bulk instructions also cost a unit per 64 bytes (`memory.fill`, `memory.copy`, `memory.init`, `memory.grow`) or per 8 elements (table and array bulk instructions, `table.grow`, and `array.new` and its variants) they touch. `wasm_runtime_disable_fuel` turns metering off again.

To stop a guest from another thread, call `wasm_runtime_interrupt`. The running call stops with `WASM_ERR_INTERRUPTED` at its next loop iteration, function entry, atomic wait, or WASI sleep; if no call is running, the next one fails before it starts.

After a trap, use `wasm_runtime_backtrace` or `wasm_runtime_call_frame` to inspect the captured Wasm stack. Frames name their module and function when the module has a `name` section, as in `#0 app!parse (func[12]) at offset 0x002A`; `wasm_func_name` and `wasm_module_name` return those names.

```c
char trace[2048];
wasm_runtime_backtrace(rt, trace, sizeof(trace));
fputs(trace, stderr);
```

### Resuming calls

A call that runs out of fuel, is interrupted, or reaches a host function that returns `WASM_YIELD` stays suspended with its frames intact. `wasm_runtime_suspended` returns the code it stopped with, `wasm_runtime_resume` continues it, and `wasm_runtime_cancel` discards it:

```c
wasm_runtime_set_fuel(rt, 10000);
err = wasm_call(mod, "run", NULL, 0, &result, 1);
while (err == WASM_ERR_OUT_OF_FUEL && wasm_runtime_suspended(rt)) {
    wasm_runtime_set_fuel(rt, 10000);
    err = wasm_runtime_resume(rt, NULL, 0, &result, 1);
}
```

A yielding host function's results are passed to `wasm_runtime_resume`, which checks them against the import's result types; after a fuel or interrupt stop it takes none. Its `results` buffer receives the call's results, also for a call started with `wasm_call_fmt`, whose result pointers are left unwritten. A resumed call may stop again.

- Fuel and interrupt stops fall between instructions; the call continues with the instruction it stopped before, and `memory.atomic.wait` waits again.
- `wasm_runtime_resume` clears a pending interrupt.
- Only the outermost call suspends. Calls made by host functions and start functions run by `wasm_load` or `wasm_instantiate` fail with the stop's code.
- An interrupt observed inside a host function, such as WASI sleep, fails the call.
- A host function that yields outside a suspendable call, including one called directly with `wasm_call_index`, fails it with `WASM_ERR_TRAP`.
- A runtime holds one suspended call; until it is resumed or cancelled, other calls on the runtime, including start functions, fail with `WASM_ERR_SUSPENDED`.
- `wasm_module_free` on a module the suspended call runs, and `wasm_runtime_free`, cancel it.
- References in suspended frames stay alive across collections, and `wasm_runtime_backtrace` and `wasm_runtime_call_frame` describe the suspended frames.

## Custom allocators

A runtime allocates all of its memory through the `wasm_allocator_t` passed to `wasm_runtime_new`. `wasm_libc_allocator()` returns one over `malloc`, `realloc`, and `free` that honors any alignment. The runtime copies the struct; `ctx` must outlive the runtime.

```c
static void* app_alloc(void* ctx, size_t size, size_t align);
static void* app_resize(void* ctx, void* p, size_t old_size, size_t new_size, size_t align);
static void app_free(void* ctx, void* p, size_t size, size_t align);

static const wasm_allocator_t app_allocator = { &app_heap, app_alloc, app_resize, app_free };
wasm_runtime_t* rt = wasm_runtime_new(&app_allocator, NULL);
```

`size` and `new_size` are never 0, `p` is never NULL, and `align` is a power of two. `free` and `resize` receive the size and alignment the block was allocated with. `resize` returns the block, possibly moved, or NULL and leaves the old block intact. `wasm_runtime_allocator` returns the runtime's copy.

`wasm_memory_new` and `wasm_table_new` take an allocator the same way, which also grows and frees the object. `wasm_shared_memory_new` reserves the memory's data from the operating system and uses its allocator for bookkeeping. An allocator used by runtimes on several threads must be thread-safe.
