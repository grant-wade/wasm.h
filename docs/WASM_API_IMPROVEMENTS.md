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

### Phase 3: GC Object Interop (Bridging the Managed World)
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

---

### Phase 4: Debugging & Stack Traces (Developer Experience)
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

---

