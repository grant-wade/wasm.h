# wasm emcc Harness

This directory holds a small harness for compiling real C fixtures with `emcc` and running the emitted `.wasm` files through [../wasm.h](../wasm.h).

The intent is not to require green results today. It is a regression and compatibility probe for the interpreter as feature support grows.

## Layout

- `fixtures/*.c` — source files compiled by `emcc`
- `runner.c` — native host runner that loads a `.wasm` file with `wasm.h`, prints exports, and optionally calls one export
- `Makefile` — builds the runner, compiles the fixtures, and runs the suite

The runner binds the built-in WASI stubs automatically, so fixtures that call `printf`/`puts` through `wasi_snapshot_preview1.fd_write` can run without extra setup.

## Usage

From the repository root:

```sh
make wasm-emcc-build
make wasm-emcc-run
make wasm-emcc-run-strict
```

Or directly in this directory:

```sh
make
make run
make run-strict
```

`run` always completes and prints a pass/fail summary even if the current interpreter cannot load or execute some modules.

`run-strict` returns a failing exit code if any fixture fails.

## Adding A Fixture

1. Add a new `fixtures/<name>.c` file.
2. Add `<name>` to `CASES` in [Makefile](Makefile).
3. Define `<name>_RUNNER_ARGS` and `<name>_EMCCFLAGS` in [Makefile](Makefile).

Current fixtures assume a zero-argument export named `wl_case`.

For `emcc` constructor-style modules, the harness can also export `__wasm_call_ctors`. The runtime now runs the real start function when present, or an exported `__wasm_call_ctors` once at load time when there is no start section.

Supported runner modes are:

- `load-ok`
- `load-fail`
- `call-void wl_case`
- `call-i32 wl_case <expected>`
- `call-i64 wl_case <expected>`
- `call-stdout wl_case <expected_stdout>`
- `call-fail wl_case`

That makes it easy to keep deliberately unsupported-feature probes in the suite without having to remove them when they fail today.
