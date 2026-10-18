# Changelog

All notable changes are documented here. This project follows Semantic Versioning.

## [Unreleased]

### Breaking

- `wasm_init` and `wasm_destroy` are removed, and `wasm_runtime_new` returns NULL when the configuration is invalid or allocation fails.
- Runtime state functions are `wasm_runtime_set_fuel`, `wasm_runtime_fuel`, `wasm_runtime_enable_feature`, `wasm_runtime_disable_feature`, `wasm_runtime_enable_all_features`, `wasm_runtime_call_depth`, `wasm_runtime_call_frame`, and `wasm_runtime_backtrace`, replacing `wasm_set_fuel`, `wasm_get_fuel`, `wasm_enable_feature`, `wasm_disable_feature`, `wasm_enable_all_features`, `wasm_get_call_stack_depth`, `wasm_get_call_frame_info`, and `wasm_dump_backtrace`.
- `wasm_runtime_call_frame` takes a `wasm_module_t** out_module` after `depth`, and any of its outputs may be NULL. In Zig, `CallFrame` has a `module` field.
- `wasm_free_module` is `wasm_module_free`, and `wasm_module_get_userdata` is `wasm_module_userdata`.
- `wasm_memory_data`, `wasm_memory_size`, and `wasm_memory_grow` take a memory index and replace their `_at` variants.
- `wasm_memory_ref_at`, `wasm_table_ref_at`, and `wasm_global_value_ref_at` are `wasm_module_memory`, `wasm_module_table`, and `wasm_module_global`.
- `wasm_memory_t`, `wasm_table_t`, `wasm_global_t`, `wasm_comptype_t`, `wasm_export_t`, and the segment, tag, custom section, and exception state structs are opaque.
- Module and runtime query functions take `const` pointers, `wasm_module_runtime` returns a non-`const` runtime, and `wasm_import_info_t` names are `const char*`.
- `wasm_shared_memory_free` is now `wasm_memory_free`, which also frees memories from the new `wasm_memory_new`.
- `frame_arena_size` (and `WASM_FRAME_ARENA_SIZE`, now 256 KiB) sets the size of each block of call frame storage. The runtime allocated the largest frame times the call depth limit up front, up to about 750 MB for a module with large frames, and a nested call needing more failed with `WASM_ERR_CALL_STACK_EXHAUSTED`. Blocks are now added as calls nest and released as they return, and running out of memory for a frame fails with `WASM_ERR_OOM`.
- `wasm_error_t` has new last members, `WASM_ERR_INTERRUPTED` and `WASM_YIELD`.
- A call stopped by fuel or `wasm_runtime_interrupt` stays suspended, and other calls on its runtime fail until `wasm_runtime_resume` or `wasm_runtime_cancel`.
- `wasm_runtime_new` takes a required `wasm_allocator_t` before the configuration; `wasm_libc_allocator()` returns one over `malloc`.
- `WASM_MALLOC`, `WASM_REALLOC`, `WASM_FREE`, `WASM_CALLOC`, `WASM_GC_ALLOC`, and `WASM_GC_FREE` are removed.
- `wasm_memory_new`, `wasm_shared_memory_new`, and `wasm_table_new` take an allocator as their first argument.
- Generated `wasm2api` options have an `allocator` field, defaulting to `wasm_libc_allocator()`.
- In Zig, `Runtime.init` and `SharedMemory.init` take a `std.mem.Allocator` first.

### Security

- WASI `proc_raise` returns `NOSYS`. It raised the requested signal in the host process, so a guest could stop or kill its embedder with `SIGKILL` or `SIGSEGV`.
- Values a host function returns must match its import's result types; a mismatch fails the call with `WASM_ERR_TYPE_MISMATCH`. The interpreter pushed them unchecked, so a host returning an `i32` for an `anyref` result handed the guest an arbitrary pointer. Results now start as the zero or null value of their type, so callbacks that set only the payload keep working.

### Added

- `wasm_runtime_allocator` returns a runtime's copy of its allocator.
- `wasm_runtime_interrupt` stops a running call from any thread with `WASM_ERR_INTERRUPTED`, at the next loop iteration, function entry, `memory.atomic.wait`, or WASI sleep. In Zig, `Runtime.interrupt` and `error.Interrupted`.
- `wasm_ref_hold` and `wasm_ref_release` keep a GC reference, converted `externref`, or `exnref` alive across collections in a slot the collector updates. Collections move objects, so references held only by the host went stale. In Zig, `Runtime.hold` and `release`.
- `wasm_register_module` makes an instance's exports importable by later modules in the same runtime. The spec test harness now links modules through it.
- `wasm_table_size`, `wasm_table_get`, `wasm_table_set`, and `wasm_table_grow`; `wasm_global_get_index` and `wasm_global_set_index`, which check mutability and type; and `wasm_call_ref`, which calls a funcref.
- `wasm_memory_new`, `wasm_table_new`, and `wasm_table_free` create and free host memories and tables to register as imports, and `wasm_memory_base` and `wasm_memory_length` read a memory handle's data and size.
- `wasm_config_t.max_total_memory_bytes` caps the linear memories of all of a runtime's instances together. In Zig, `Config.max_total_memory_bytes`.
- `wasm_config_t.max_gc_heap_bytes` caps the GC heap. In Zig, `Config.max_gc_heap_bytes`.
- Backtraces name each frame's module and function from the `name` section, as in `#0 app!parse (func[12]) at offset 0x002A`, and `wasm_func_name` and `wasm_module_name` return those names.
- `wasm_wasi_set_args`, `wasm_wasi_set_env`, and `wasm_wasi_preopen_dir` configure a runtime's WASI arguments, environment, and accessible directories. Since 1.2.0 required a preopen for every path operation, no embedder could give a guest file access. In Zig, `Runtime.setWasiArgs`, `setWasiEnv`, and `preopenDir`.
- WASI `poll_oneoff`, which sleeps until the earliest clock subscription and reports file descriptor subscriptions ready at once, so `sleep` and `nanosleep` work in wasi-libc programs.
- `wasm_runtime_disable_fuel` turns fuel metering off again; in Zig, `Runtime.disableFuel`.
- `wasm_runtime_resume` continues a call suspended by running out of fuel, an interrupt, or a host function returning `WASM_YIELD`, and `wasm_runtime_suspended` and `wasm_runtime_cancel` report and discard it. In Zig, `Runtime.resumeCall`, `TypedFunc.resumeCall`, `Runtime.suspended`, `Runtime.cancel`, and `error.Yield`.
- The CLI `run` command runs a WASI command's `_start` with the remaining arguments as its argv, and `--dir HOST[::GUEST]` and `--env NAME=VALUE` configure its WASI environment. With `--wasi`, `call` passes the module path as `argv[0]`.

### Changed

- Bulk instructions cost fuel for their size: a unit per 64 bytes or 8 elements they touch, on top of the unit per instruction. `memory.fill` over 4 GiB cost one unit, so fuel did not bound running time.
- `memory.grow` and `table.grow` within their maximum run out of fuel before growing when the budget cannot cover them.
- A shared memory a module defines reserves address space only up to `max_memory_bytes`. It reserved its declared maximum, up to 4 GiB, regardless of the limit.
- Validated function bodies are translated to pre-decoded instructions, and `wasm_bench` runs about 2.6x faster.
- A call fails with `WASM_ERR_STACK_OVERFLOW` on entry when the operand stack cannot hold the most values its body can need.
- Interpreter instructions read and write operand stack slots in place; `wasm_bench` takes about 1.3 s, down from 3.4 s.

### Fixed

- An exception thrown inside a legacy `catch` or `catch_all` body was caught by a later clause of the same `try`.
- `array.new` popped its initial value before allocating the array, so a collection during that allocation missed the value, and the array was filled with a reference into the freed heap.
- The GC frees `externref` boxes made by `extern.convert_any` and the `exnref` objects made by `catch_ref` and `catch_all_ref` once nothing reaches them. Both lived until the runtime was freed and kept what they referred to alive, so a guest loop grew host memory without bound, and `any.convert_extern` slowed down as boxes accumulated.
- Calls that trap set `wasm_runtime_last_error` and `wasm_runtime_error_message`, which reported `ok` after a trap.
- The GC heap grows to hold live objects and allocations larger than its initial size, which failed with `WASM_ERR_OOM` once about 4 MiB was live.
- A collection skips anyref values made by `any.convert_extern` from host `externref` values, which it dereferenced as GC objects and crashed on.
- Collections during instantiation keep the GC objects that constant expressions are building; globals, tables, and element segments could end up referring to freed or reused objects.
- `wasm_struct_new` and `wasm_array_new` keep the references they are given alive while allocating, which a collection during that allocation left pointing at freed objects.
- `extern.convert_any` returns exactly the host `externref` that `any.convert_extern` received; odd values such as 1 came back as a different pointer and tested as `i31ref` in between.

## [1.2.0] - 2026-10-17

### Security

- WASI path operations accept only directories reached from a preopen. Standard I/O descriptors were treated as directory capabilities, so a guest could open, create, rename, or delete any host file through `path_open(0, "/abs/path")` and the other `path_*` calls. Guest paths, and symlink targets, must now be relative and must not climb above their directory with `..`; violations return `NOTCAPABLE`.

### Added

- `wasm_config_t.max_function_locals` (default 50,000, `WASM_MAX_FUNCTION_LOCALS`) limits the locals a function may declare; 0 removes the limit. A 30-byte module could declare hundreds of millions of locals and make loading allocate gigabytes.
- `wasm_config_t.max_memory_bytes` (default unlimited) and `max_table_elements` (default 10,000,000) cap each memory and table at instantiation and on growth. A module could declare a table of 2³² elements and make loading write every one.
- `wasm_compile` decodes and validates a module without resolving imports or running code, and `wasm_instantiate` creates instances of it without validating function bodies again. Zig exposes them as `Runtime.compile` and `Module.instantiate`.
- The CLI `imports` command. The inspection commands compile instead of instantiating, so they no longer fail on modules with unbound imports or run start functions.
- `wasm_bench`, interpreter micro-benchmarks for loops, memory access, calls, `br_table`, `call_indirect`, and floating point.
- `wasm_exec_fuzzer`, a libFuzzer harness that instantiates each input with stubbed imports and calls its exports, and `wasm_exec_fuzz_replay`, which runs that harness over the seed corpus in `test/fuzz/corpus` as part of `./cbuild --test`.
- `WASM_ERR_EXIT` and `wasm_runtime_exit_code`, which report a guest's requested termination and its status. In Zig, `error.Exit` and `Runtime.exitCode`.
- Zig `Config` fields for `max_function_locals`, `max_memory_bytes`, and `max_table_elements`.

### Changed

- The interpreter is about 25% faster on `wasm_bench` (release build). Single-byte immediates and the common memory-argument form decode inline, numeric locals, parameters, and results skip reference retagging, `br_table` no longer allocates, branches move results without a scratch buffer, block exits skip `free(NULL)`, frames record their position only when execution leaves them, and `call_indirect` and `call_ref` skip the subtype check when the types are identical.

### Fixed

- Modules whose sections repeat or appear out of the binary format's order are malformed. A second export section replaced the first and leaked it; the execution fuzzer found it.
- wasm2api inspects modules with `wasm_compile`. It bound the Emscripten stubs, which reach the host filesystem, and ran the module's start function while generating code, and it failed on modules that import memories or tables.
- `wasm_load` clears the previous error first, so a failure no longer reports a stale message from an earlier one.
- WASI `proc_exit` stops the guest. It used to return to the caller, so programs kept running past `exit()` and the status was lost. The CLI exits with the guest's status.
- Exceptions thrown through an imported function reach Wasm handlers after `call_indirect`, `call_ref`, and the tail calls, not only after `call`. A tail call no longer leaves its own function's handlers in place.
- Freeing a module invalidates the funcrefs that point into it. A funcref kept by the host or stored in a table that outlives the module was resolved through freed memory; using it now traps or fails the type check.
- Public functions given a NULL runtime or module return an error instead of crashing; `wasm_call`, `wasm_call_index`, the `wasm_register_*_import` functions, and the feature toggles dereferenced it.
- Type canonicalization hashes each rec group and compares it only with earlier groups of the same hash. It compared every pair of types and allocated a types² table for each comparison, so a 20 KB module with 2,000 types took over 30 seconds to load.
- Validating functions with non-defaultable locals no longer clears local-initialization state for every possible block depth.
- GC marking uses an explicit worklist instead of recursion, so collecting long object chains no longer overflows the native stack.
- WASI `path_open` derives the host access mode from the requested rights, so directories and read-only files can be opened on POSIX hosts.

## [1.1.0] - 2026-10-17

### Added

- Opt-in support for wide arithmetic, custom page sizes, compact imports, and threads, including shared memories usable from runtimes on separate threads.
- A Zig 0.17 package: `build.zig` compiles the runtime from `src/` with Zig's C compiler, and the `wasm` module wraps the C API with checked typed calls, host functions generated from Zig functions, a Zig error set, and memory, global, introspection, GC, and shared-memory accessors. `zig build test` runs the wrapper tests and the modular C regression tests; `./cbuild --test` runs it as the `zig` test when Zig 0.17 is available. See `docs/ZIG.md`.

### Fixed

- Calls reject arguments that do not match the function parameter types with `WASM_ERR_TYPE_MISMATCH` before execution.
- Bound decoded vector counts and checked array allocation sizes; fixed relaxed SIMD i16 dot-product wrapping.
- Restored warning-clean MSVC builds and fixed 32-bit x86 builds, including 64-bit atomic alignment and handling of allocation failures.
- The WASI `fd_readdir` binding reads entry serial numbers from `d_fileno` on the BSDs, whose `dirent` declares `d_ino` only outside strict POSIX mode; platform builds failed on FreeBSD.

### Changed

- Split the runtime into independently compiled sources under `src/`, with a generated single-header distribution and modular-library tests.
- Updated the WebAssembly testsuite to 2026-10-14 and added proposal-suite coverage.
- The build moved from CMake to [cbuild](https://github.com/grant-wade/cbuild): `build.c` describes the build and test graph and needs only a C compiler. Bootstrap with `cc -o cbuild build.c`; `./cbuild --test` replaces the `check` target. See `docs/BUILDING.md` for the option and target mapping.
- `tools/amalgamate.c` replaces `cmake/Amalgamate.cmake` as the `wasm.h` generator and freshness check, including in the pre-commit hook. Its output is unchanged apart from the regeneration command in the header comment.

### Removed

- The CMake build, its installable package metadata (`find_package(wasm_h)`), and the Makefile wrappers. Install by copying `wasm.h`.
- The non-failing `wasm-spectest-run` and `wasm-emcc-run` variants; `./cbuild --test --test-label=spectest` (or `emcc`) runs a suite and reports failures.

## [1.0.0] - 2026-07-14

Initial public release.

### Security

- Removed the implicit WASI preopen of the host working directory.
- Made WASI bindings opt-in in the CLI.
- Documented that Emscripten syscall shims are for trusted modules only.

### Fixed

- Made Wasm i32/i64 wrapping arithmetic independent of signed C overflow.
- Preserved required quiet-NaN behavior in optimized rounding operations.
- Fixed optimized platform-build warnings and spectest harness leaks.

### Changed

- Removed the opaque prebuilt SQLite Wasm demo; examples are now source-reproducible.

### Added

- Release, sanitizer, platform, and compiler CI jobs.
- A libFuzzer module-loader harness.
- Version macros and installable CMake package metadata.
