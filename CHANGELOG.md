# Changelog

All notable changes are documented here. This project follows Semantic Versioning.

## [Unreleased]

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
