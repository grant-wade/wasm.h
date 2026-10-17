# Changelog

All notable changes are documented here. This project follows Semantic Versioning.

## [Unreleased]

### Changed

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
