# Changelog

All notable changes are documented here. This project follows Semantic Versioning.

## [Unreleased]

### Changed

- The interpreter is about 25% faster on `wasm_bench` (release build). Single-byte immediates and the common memory-argument form decode inline, numeric locals, parameters, and results skip reference retagging, `br_table` no longer allocates, branches move results without a scratch buffer, block exits skip `free(NULL)`, frames record their position only when execution leaves them, and `call_indirect` and `call_ref` skip the subtype check when the types are identical.

### Security

- WASI path operations accept only directories reached from a preopen. Standard I/O descriptors were treated as directory capabilities, so a guest could open, create, rename, or delete any host file through `path_open(0, "/abs/path")` and the other `path_*` calls. Guest paths, and symlink targets, must now be relative and must not climb above their directory with `..`; violations return `NOTCAPABLE`.

### Added

- `wasm_config_t.max_function_locals` (default 50,000, `WASM_MAX_FUNCTION_LOCALS`) limits the locals a function may declare; 0 removes the limit. A 30-byte module could declare hundreds of millions of locals and make loading allocate gigabytes.
- `wasm_config_t.max_memory_bytes` (default unlimited) and `max_table_elements` (default 10,000,000) cap each memory and table at instantiation and on growth. A module could declare a table of 2³² elements and make loading write every one.
- `wasm_compile` decodes and validates a module without resolving imports or running code, and `wasm_instantiate` creates instances of it without validating function bodies again. Zig exposes them as `Runtime.compile` and `Module.instantiate`.
- The CLI `imports` command, and the inspection commands compile instead of instantiating: they no longer fail on modules with unbound imports or run start functions.
- `wasm_bench`, interpreter micro-benchmarks for loops, memory access, calls, `br_table`, `call_indirect`, and floating point.
- `wasm_exec_fuzzer`, a libFuzzer harness that instantiates each input with stubbed imports and calls its exports, and `wasm_exec_fuzz_replay`, which runs that harness over the seed corpus in `test/fuzz/corpus` as part of `./cbuild --test`.
- `WASM_ERR_EXIT` and `wasm_runtime_exit_code`, which report a guest's requested termination and its status. Zig maps it to `error.Exit` with `Runtime.exitCode`.

### Fixed

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
