# Building and Testing

The build is [`build.c`](../build.c), a C program written against the vendored [`cbuild.h`](../cbuild.h). It needs only a C99 compiler. Copying `wasm.h` into an application needs no build system at all.

## Build and test

```sh
git submodule update --init --recursive
cc -o cbuild build.c        # once; Windows: cl /nologo /Fe:cbuild.exe build.c
./cbuild                    # runtime, CLI, wrapper generator, and test programs
./cbuild --test             # build what the tests need, then run them
```

Compile the build program once; it rebuilds itself when `build.c` or `cbuild.h` changes. On Windows, use a Developer Command Prompt with `cl` on `PATH`. On FreeBSD, use the base system's Clang `cc`.

`--test` runs the native tests and every available specification test against both the single header and the modular library, in parallel, and exits non-zero if any fail. The output of a failed test is printed, and every test's output is kept in `build/<config>/tests/<name>.log`. Optional tools are detected each run:

- `wasm-tools` with `json-from-wast` and `parse`: specification tests.
- `emcc` (on `PATH`, or under `EMSDK` or `EMSCRIPTEN`): Emscripten fixtures and `--regenerate-examples`.
- `zig` 0.17 on `PATH`: the `zig` test, which runs `zig build test` with matching `--release`, `--no-platform`, and `--no-werror` settings. See the [Zig guide](ZIG.md).

Each `test/unit/<path>_test.c` runs as `wasm_test.<suite>` and `wasm_modular_test.<suite>`, where `<suite>` is `<path>` with `/` replaced by `.`; `build/<config>/wasm_test <suite>...` runs suites directly. Suites under `test/spec/proposals/` and `test/proposals/` run as `spectest.proposal.<proposal>.<file>` and also carry the `proposal` label. A proposal directory in both fails the build. Proposals listed in `implemented_proposals` in `build.c` run by default; the rest are reported as skipped unless `--spec-proposals` is given. Suites under `test/wast/<suite>/` run as `spectest.wast.<suite>.<file>` with the features `wast_suites` in `build.c` gives them. Suites under `test/spec/legacy/` and `test/spec/custom/` run as `spectest.legacy.<file>` and `spectest.custom.<file>`.

Pick tests with `--test-label` (`unit`, `modular`, `spectest`, `proposal`, `emcc`, `zig`), `--test-filter`, and `--test-exclude`, which take wildcards on test names. `--list-tests` shows what is selected. `-j` sets compile and test parallelism; `--test-timeout=SECONDS` limits each test.

```sh
./cbuild --test --test-label=unit
./cbuild --test --test-filter='spectest.*simd*'
./cbuild --test --test-label=spectest --test-exclude='spectest.modular.*'
```

## Options

The build has no cache: options that shape it are read on every run, so pass the same ones each time. Each configuration builds into its own directory by default.

| Option | Default | Purpose |
| --- | --- | --- |
| `--release` | off | Optimized build (`-O3 -DNDEBUG`, or `/O2`) in `build/release` instead of a debug build in `build/debug`. |
| `--build-dir=DIR` | `build/debug`, `build/release`, `build/gc-stress`, or `build/stack-stress` | Output directory. |
| `--cc=COMPILER` | `$CC`, else `cc` or `cl` | C compiler; GCC, Clang, and MSVC are supported. |
| `--cflags=FLAGS`, `--ldflags=FLAGS` | `$CFLAGS`, `$LDFLAGS` | Extra flags, applied after all built-in ones. |
| `--no-platform` | off | Build without platform-backed runtime and test helpers. |
| `--no-werror` | off | Do not treat compiler warnings as errors. |
| `--no-emcc` | off | Skip the Emscripten fixture tests even when `emcc` is available. |
| `--no-zig` | off | Skip the Zig package tests even when `zig` 0.17 is available. |
| `--gc-stress` | off | Collect garbage before every GC allocation (`-DWASM_GC_STRESS`) in `build/gc-stress`. |
| `--stack-stress` | off | Start execution stacks at one slot and one call frame, and move them to new, poisoned memory on every push that could grow them (`-DWASM_STACK_STRESS`), in `build/stack-stress` (`build/gc-stack-stress` with `--gc-stress`). |
| `--fuzzer` | off | Also build the Clang/libFuzzer loader and execution harnesses. |
| `--regenerate-examples` | off | Add `session_math_regenerate`, which rewrites the committed example module and wrappers with `emcc`. |
| `--spec-proposals` | off | Run specification proposal suites the runtime does not implement instead of skipping them. |

`./cbuild --help` lists these with cbuild's own options. For example, the CI sanitizer configuration is:

```sh
CC=gcc ./cbuild --test --no-werror --build-dir=build/asan \
  --cflags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all' \
  --ldflags='-fsanitize=address,undefined'
```

## Targets

Build individual targets with `./cbuild -t <target>`; `./cbuild --list` shows them all.

| Target | Purpose |
| --- | --- |
| `wasm`, `wasm2api` | CLI and wrapper generator. |
| `wasm_bench` | Interpreter micro-benchmarks from `bench/kernels.wat`; run a `--release` build. |
| `wasm_runtime` | Modular static library. |
| `wasm-header` | Regenerate the root `wasm.h` from `src/`. |
| `wasm_test`, `wasm_modular_test` | Native regression executables. |
| `session_math_demo` | Wrapper example. |
| `session_math_regenerate` | Rebuild `examples/session_math.wasm` and its wrappers; requires `emcc` and `--regenerate-examples`. |
| `wasm_load_fuzzer`, `wasm_exec_fuzzer` | libFuzzer harnesses; require `--fuzzer`. |
| `wasm_exec_fuzz_replay` | Execution harness over `test/fuzz/corpus`, run as a unit test; requires `wasm-tools`. |

`./cbuild --clean` removes the current configuration's build directory. `./cbuild --compile-commands` writes `compile_commands.json` there for clangd and similar tools. See [test/README.md](../test/README.md) for fixture development and specification-test skips.

## Runtime sources

Edit `src/`; its `.c` files compile independently. `src/wasm.h` contains the public declarations. Every build regenerates the root `wasm.h` when `src/` changes; to do only that:

```sh
./cbuild -t wasm-header
```

Commit the regenerated `wasm.h` with the source change. Add new implementation files to both `src/amalgamation.h` and `wasm_runtime_sources` in `build.c`.

The generator, [`tools/amalgamate.c`](../tools/amalgamate.c), also builds on its own. To check freshness without modifying the header:

```sh
cc -o amalgamate tools/amalgamate.c
./amalgamate --check
```

To check staged sources and header before commits:

```sh
git config core.hooksPath .githooks
```

If using a hook manager, call `.githooks/pre-commit` from the existing hook instead.

## Modular library

`./cbuild -t wasm_runtime` builds `build/<config>/libwasm_runtime.a` (`wasm_runtime.lib` with MSVC). Link it, add `src/` to the application's include path, link the math library where it is separate (`-lm`), and include `wasm.h` without defining `WASM_IMPL`.

To build it elsewhere, compile every `.c` file listed in `build.c` and add `src/` to the include path. Apply platform definitions consistently to every runtime translation unit. For platform-enabled POSIX builds, compile and link with `-pthread`.

## Standalone header

Compile one application translation unit with `WASM_IMPL` defined before including `wasm.h`. For `WASM_ENABLE_PLATFORM=1` on POSIX systems, add `-D_POSIX_C_SOURCE=200809L -pthread`:

```sh
cc -std=c99 -O2 application.c -lm -o application
```

32-bit x86 GCC/Clang builds require `-msse2 -mfpmath=sse` to preserve Wasm floating-point rounding. `build.c` detects such targets and adds these flags automatically.

To install, copy `wasm.h` into the application or an include directory.

## CI and fuzzing

[ci.yml](../.github/workflows/ci.yml) defines CI for Linux, macOS, Windows, and self-hosted FreeBSD, plus header freshness checks, ASan/LSan/UBSan runs, libFuzzer smoke tests, and Zig tests and cross-compilation. Emscripten fixtures run locally.

To fuzz the module loader with Clang and ASan/UBSan:

```sh
./cbuild --fuzzer --cc=clang -t wasm_load_fuzzer
./build/debug/wasm_load_fuzzer -runs=10000 -max_len=65536
```

`wasm_exec_fuzzer` goes further: it compiles each input with every implemented proposal except threads enabled, stubs all its imports, instantiates it, and calls each exported function with zero arguments under a fuel budget and memory and table limits. A second instance repeats each call in small fuel steps, resuming after every stop and yielding from every stubbed import, and the fuzzer aborts if its results or memory differ. Seed it with the modules `./cbuild --test` builds from `test/fuzz/corpus` into `build/debug/fuzz-corpus`:

```sh
./cbuild --fuzzer --cc=clang -t wasm_exec_fuzzer
./build/debug/wasm_exec_fuzzer -max_len=65536 -timeout=30 build/debug/fuzz-corpus
```
