# WASM Public API Improvements

### [DONE] Phase 1: Safe Memory & Global Access (The Foundation)
**Goal:** Allow the host C code to manipulate linear memory and global variables safely, without manual pointer arithmetic or bounds-checking.

**Proposed Public APIs:**
```c
// Memory Helpers
wasm_error_t wasm_memory_read(wasm_module_t* mod, uint32_t memory_idx, uint32_t offset, void* dst, size_t len);
wasm_error_t wasm_memory_write(wasm_module_t* mod, uint32_t memory_idx, uint32_t offset, const void* src, size_t len);

// String extraction (copies up to max_len, ensuring null-termination, safely stopping at memory bounds)
wasm_error_t wasm_memory_get_string(wasm_module_t* mod, uint32_t memory_idx, uint32_t offset, char* dst, size_t max_len);

// Global Helpers
wasm_error_t wasm_global_get(wasm_module_t* mod, const char* name, wasm_value_t* out_val);
wasm_error_t wasm_global_set(wasm_module_t* mod, const char* name, wasm_value_t val);
```
**Implementation Strategy:**
*   **Memory:** Wrap your existing `wasm__memory_at` logic. Just add `if (offset + len > memory->pages * WASM_PAGE_SIZE) return WASM_ERR_OUT_OF_BOUNDS;` before doing a `memcpy`.
*   **Globals:** Use your existing `wasm_find_export` to locate the global index, then read/write to `mod->globals[idx].value` (reusing your `wasm__global_get_value`/`set_value` internals).

---

### [DONE] Phase 2: The Format String API (The Ergonomic Core)
**Goal:** Eliminate the boilerplate of constructing `wasm_value_t` arrays and `wasm_functype_t` structs manually. 

**Format Mapping Concept:**
*   `i` = i32, `I` = i64
*   `f` = f32, `F` = f64
*   `r` = externref (passed as `void*`)
*   `v` = void (used only for returns)
*   Syntax: `"args(results)"` -> e.g., `"ii(i)"` (two i32s in, one i32 out), `"I(v)"` (one i64 in, void out).

**Proposed Public APIs:**
```c
// 1. Easy Calling
// Uses stdarg.h (va_list) to pack C types into a temporary wasm_value_t array, then calls the function.
wasm_error_t wasm_call_fmt(wasm_module_t* mod, const char* func_name, const char* fmt, ...);

// 2. Easy Import Registration
// Parses the format string, allocates the wasm_functype_t automatically, and registers the import.
wasm_error_t wasm_bind_host_func(wasm_runtime_t* rt, const char* module_name, const char* func_name, const char* fmt, wasm_host_func_t callback, void* userdata);
```
**Implementation Strategy:**
*   Write a tiny internal parser `wasm__parse_fmt_string(const char* fmt, wasm_valtype_t* params, wasm_valtype_t* results)`.
*   For `wasm_call_fmt`, parse and validate the format string against the exported function signature, then use `va_arg` to pack primitive C types into temporary `wasm_value_t` arrays before forwarding to `wasm_call_index`. Return values are written back through typed output pointers (`int32_t*`, `int64_t*`, `float*`, `double*`, `void**`).
*   For `wasm_bind_host_func`, use the parsed arrays to populate a `wasm_import_t` and call your existing `wasm_register_import`.

---

### [DONE] Phase 3: GC Object Interop (Bridging the Managed World)
**Goal:** If a Wasm function returns a `struct` or `array` reference, the host needs a way to read and write to its fields without causing memory corruption.

**Proposed Public APIs:**
```c
// Structs
wasm_error_t wasm_struct_get_field(wasm_module_t* mod, wasm_value_t struct_ref, uint32_t field_idx, wasm_value_t* out_val);
wasm_error_t wasm_struct_set_field(wasm_module_t* mod, wasm_value_t struct_ref, uint32_t field_idx, wasm_value_t val);

// Arrays
wasm_error_t wasm_array_length(wasm_module_t* mod, wasm_value_t array_ref, uint32_t* out_len);
wasm_error_t wasm_array_get_elem(wasm_module_t* mod, wasm_value_t array_ref, uint32_t index, wasm_value_t* out_val);
wasm_error_t wasm_array_set_elem(wasm_module_t* mod, wasm_value_t array_ref, uint32_t index, wasm_value_t val);
```
**Implementation Strategy:**
*   You already wrote `wasm__gc_struct_get_field` and `wasm__gc_array_get`. These public functions just act as safe wrappers.
*   They will verify that the passed `wasm_value_t` is actually a valid GC reference type, resolve it to the `wasm_gc_header_t*`, and then forward the work to your internal inline helpers.
*   For packed fields and elements, the public getters currently return a zero-extended `i32` so the host sees the raw stored bits without an implicit signedness choice.

---

### [DONE] Phase 4: Debugging & Stack Traces (Developer Experience)
**Goal:** When a Wasm module traps (e.g., divide by zero, unreachable, out of bounds), tell the user exactly where it happened.

**Proposed Public APIs:**
```c
// Fills a user-provided buffer with a human-readable stack trace.
// e.g., "#0 func[12] at offset 0x004B\n#1 func[4] at offset 0x011A"
void wasm_dump_backtrace(wasm_runtime_t* rt, char* buffer, size_t buffer_size);

// Alternative: Just get the raw frame data to let the host format it
uint32_t wasm_get_call_stack_depth(wasm_runtime_t* rt);
wasm_error_t wasm_get_call_frame_info(wasm_runtime_t* rt, uint32_t depth, uint32_t* out_func_idx, uint32_t* out_offset);
```
**Implementation Strategy:**
*   You already track the call stack perfectly inside `rt->call_frames` and `rt->call_frame_sp`. 
*   Iterate backwards from `rt->call_frame_sp - 1` down to `0`. 
*   The `func_idx` is sitting right there in `cf->func_idx`.
*   The instruction offset can be calculated dynamically: `cf->r.ptr - cf->func->code`. 
*   Print this to the provided buffer using `snprintf`.
*   In practice, the runtime now snapshots the active Wasm frames before unwind on traps so the host can query the last failing stack even after `wasm_call()` returns.

---

### [DONE] Phase 5: Runtime Configuration (Removing Hard Caps)
**Goal:** Allow the host to define the maximum stack, call depth, and label depth dynamically at runtime, *without* adding slow `realloc` checks to the hot interpreter loop.

**The Strategy: "Allocate Once, Bound Dynamically"**
Instead of `realloc`ing the stack dynamically every time it fills up (which adds a branching performance penalty to your `WASM__PUSH` macro), we shift the allocation to `wasm_init` based on a configuration struct.

#### Step 1: Introduce a Configuration Struct
Add a new struct that the host can pass to configure the runtime. Provide a "default" initializer so users don't have to guess good values.

```c
typedef struct wasm_config_t {
    uint32_t max_stack_values;    // e.g., 65536 (instead of 4096)
    uint32_t max_call_depth;      // e.g., 1024
    uint32_t max_labels;          // e.g., 1024
    size_t   initial_gc_heap_size;// e.g., 4MB
    size_t   frame_arena_size;    // e.g., 4MB
} wasm_config_t;

// Helper to get sensible defaults
void wasm_config_default(wasm_config_t* config);
```

#### Step 2: Update the `wasm_runtime_t` Struct
Change the inline arrays to pointers, and store the configured limits.

```c
struct wasm_runtime_t {
    // ... imports ...
    
    // Changed from: wasm_value_t stack[WASM_MAX_STACK];
    wasm_value_t* stack;         
    uint32_t max_stack;          // Stored from config
    uint32_t sp;
    
    // ... existing fields ...
    
    // Changed from: wasm__call_frame_t* call_frames + fixed WASM_MAX_CALL_DEPTH
    uint32_t max_call_frames;
};
```

#### Step 3: Update `wasm_init`
Modify `wasm_init` to take the configuration and perform the allocations.

```c
// Public API changes slightly:
wasm_error_t wasm_init(wasm_runtime_t* rt, const wasm_config_t* config);

// Implementation:
wasm_error_t wasm_init(wasm_runtime_t* rt, const wasm_config_t* config) {
    wasm_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        wasm_config_default(&cfg);
    }
    
    memset(rt, 0, sizeof(*rt));
    rt->max_stack = cfg.max_stack_values;
    rt->max_call_frames = cfg.max_call_depth;
    
    // Allocate the main stack on the heap instead of inline
    rt->stack = (wasm_value_t*)WASM_MALLOC(rt->max_stack * sizeof(wasm_value_t));
    if (!rt->stack) return WASM_ERR_OOM;
    
    // ... continue with your existing GC and Arena initialization ...
}

// Don't forget to WASM_FREE(rt->stack) in wasm_destroy!
```

#### Step 4: Update the Hot Macros
Because you pre-allocated the maximum size, your hot execution loop doesn't get slower. You just change the macro to check the dynamic variable `rt->max_stack` instead of the macro `WASM_MAX_STACK`.

```c
// BEFORE:
#define WASM__PUSH(rt, val)                \
    do {                                   \
        if ((rt)->sp >= WASM_MAX_STACK) {  \ // Macro check
            err = WASM_ERR_STACK_OVERFLOW; \
            goto cleanup;                  \
        }                                  \
        (rt)->stack[(rt)->sp++] = (val);   \
    } while (0)

// AFTER:
#define WASM__PUSH(rt, val)                \
    do {                                   \
        if ((rt)->sp >= (rt)->max_stack) { \ // Dynamic variable check
            err = WASM_ERR_STACK_OVERFLOW; \
            goto cleanup;                  \
        }                                  \
        (rt)->stack[(rt)->sp++] = (val);   \
    } while (0)
```

#### Step 5: Fix the Validator (`wasm__validator_t`)
The validator also has hardcoded arrays:
```c
wasm_valtype_t stack[WASM_MAX_STACK];
wasm_reftype_t stack_reftypes[WASM_MAX_STACK];
wasm__val_frame_t frames[WASM__MAX_LABELS];
```
Since the validator only runs *once* at load time, it's entirely safe to dynamically allocate these based on the runtime's configuration when `wasm__validate_function` is called, and `free` them when validation is done. (Or better yet, allocate them *once* inside `wasm_load` and reuse the buffers across all functions to avoid `malloc` overhead).

### Why this is the best approach:
1. **Zero Performance Hit:** Checking `rt->sp >= rt->max_stack` takes the exact same number of CPU cycles as checking `rt->sp >= 4096`. 
2. **No Dangling Pointers:** You avoid the complexity of dynamic array resizing (`realloc`), which would require you to carefully update pointers to the stack if the memory address changes.
3. **Total Host Control:** If a user is running on a microcontroller with 64KB of RAM, they can pass a config with `max_stack = 512`. If they are running on a desktop parsing a massive 50MB LLVM payload, they can pass `max_stack = 1000000`.


### [DONE] Phase 6: Gas Metering / Execution Limits (Security)
**The Problem:** WebAssembly is often used to run untrusted third-party code (plugins, mods, smart contracts). If a user uploads a Wasm file with an infinite loop `(loop (br 0))`, your host C application will freeze forever.
**The Solution:** Add "Gas Metering" (or an instruction counter). 

**Public API:**
```c
// Give the runtime a "budget" of instructions it is allowed to execute
void wasm_set_fuel(wasm_runtime_t* rt, uint64_t fuel);

// Get remaining fuel (useful to see how expensive a call was)
uint64_t wasm_get_fuel(wasm_runtime_t* rt);

// New Error Code
#define WASM_ERR_OUT_OF_FUEL -6
```

**Implementation Strategy:**
*   Add public `wasm_set_fuel()` / `wasm_get_fuel()` helpers on `wasm_runtime_t`, with metering disabled by default until the host sets a budget.
*   Inside `wasm__interp_loop`, check fuel once per opcode before dispatch. When fuel is enabled and reaches zero, trap with `WASM_ERR_OUT_OF_FUEL`.
*   This V1 implementation counts each dispatched opcode, including `end`, which keeps the runtime simple and makes remaining-fuel accounting predictable.

---

### [DONE] Phase 7: Built-in WASI Print Stubs (Compatibility)
**The Problem:** If someone writes a C, C++, Rust, or Zig program and types `printf("Hello World");`, the compiler targets `wasm32-wasi`. When they try to load that module into your runtime, it will crash with: `Unknown import: wasi_snapshot_preview1.fd_write`.
Forcing every user to manually read the WASI spec and implement `fd_write` just to get `printf` working is highly frustrating.

**The Solution:** Provide an optional, built-in WASI helper that hooks up `stdout`/`stderr` automatically.

**Public API:**
```c
// Automatically registers basic WASI imports (fd_write, fd_close, fd_seek,
// fd_fdstat_get, args_sizes_get, args_get, environ_sizes_get,
// environ_get, random_get, clock_time_get, proc_exit)
// so that standard C/Rust libraries don't crash on load.
wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt);
```

**Implementation Strategy:**
*   `wasm_bind_wasi_stubs()` now registers `wasi_snapshot_preview1.fd_write`, `fd_close`, `fd_seek`, `fd_fdstat_get`, `args_sizes_get`, `args_get`, `environ_sizes_get`, `environ_get`, `random_get`, `clock_time_get`, and `proc_exit`.
*   `fd_write` resolves the active calling module, validates guest iovecs against memory 0, writes to host `stdout`/`stderr`, and stores `nwritten` back into guest memory. `fd_fdstat_get` reports stdio descriptors as character devices.
*   Argument and environment stubs default to empty lists, while `random_get` and `clock_time_get` route through portable C99-friendly platform hooks near the top of the implementation section so embedders can override them without pulling POSIX-only code into the core runtime.

---

### [DONE] Phase 8: Module Userdata (Context Management)
**The Problem:** Host imports often need instance-local context. If a game engine has 50 monsters, each backed by a different `wasm_module_t`, the host callback needs an explicit way to know which module invoked it and to retrieve that module's opaque host state.

**The Solution:** Add an opaque `void* userdata` to the `wasm_module_t` itself.

**Public API:**
```c
void wasm_module_set_userdata(wasm_module_t* mod, void* userdata);
void* wasm_module_get_userdata(wasm_module_t* mod);

// Inside a host function callback, developers can now do:
// my_monster_t* monster = (my_monster_t*)wasm_module_get_userdata(mod);
// wasm_runtime_t* rt = mod->rt;
```

**Implementation Strategy:**
*   `wasm_module_t` now carries an opaque `void* host_userdata` slot with public `wasm_module_set_userdata()` / `wasm_module_get_userdata()` accessors.
*   `wasm_host_func_t` now receives the calling `wasm_module_t*` directly. Callbacks that need runtime services can reach them through `mod->rt`, while module-local context stays explicit through `wasm_module_get_userdata(mod)`.

---

### [DONE] Phase 9: Custom Sections Introspection
**The Problem:** Wasm files often contain "Custom Sections" (Section ID `0`). The standard `name` section contains the actual names of functions and variables (e.g., `calculate_physics` instead of `func[14]`). Game engines also use custom sections to bundle configuration data or assets directly inside the `.wasm` file. Your current loader (`wasm_load`) just skips Section 0.

**The Solution:** Parse and expose custom sections to the host.

**Public API:**
```c
uint32_t wasm_custom_section_count(wasm_module_t* mod);
const char* wasm_custom_section_name(wasm_module_t* mod, uint32_t index);
const uint8_t* wasm_custom_section_data(wasm_module_t* mod, uint32_t index, size_t* out_len);

// Helper to grab a specific section (e.g., "name" or "my_game_data")
const uint8_t* wasm_custom_section_find(wasm_module_t* mod, const char* name, size_t* out_len);
```

**Implementation Strategy:**
*   Add a `wasm_custom_section_t` array to `wasm_module_t`.
*   In `wasm_load`, when `sid == 0`:
    1. Read the name string (using your `wasm__read_name` helper).
    2. The rest of the section size is the payload.
    3. Because your `bytes` pointer (passed to `wasm_load`) must stay alive for the lifetime of the module anyway (since you don't copy the bytecode), you can just store a `const uint8_t* payload = r.ptr;` and `size_t len` directly into the struct without `malloc`ing new memory!
*   The runtime now owns a null-terminated copy of each section name for stable public access, while payload pointers continue to alias the original module byte buffer.
