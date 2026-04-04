# wasm emcc Harness

This directory holds a small harness for compiling real C fixtures with `emcc` and running the emitted `.wasm` files through [../wasm.h](../wasm.h).

The intent is not to require green results today. It is a regression and compatibility probe for the interpreter as feature support grows.

## Layout

- `fixtures/*.c` — source files compiled by `emcc`
- `runner.c` — native host runner that loads a `.wasm` file with `wasm.h`, prints exports, and optionally calls one export
- `Makefile` — builds the runner, compiles the fixtures, and runs the suite

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
3. Define `<name>_EXPORT`, `<name>_RESULT`, `<name>_EXPECT`, and `<name>_EMCCFLAGS` in [Makefile](Makefile).

Current fixtures assume a zero-argument export named `wl_case` that returns an `i32`. The runner itself can also exercise `void` exports, so that can be extended later without changing the basic flow.
