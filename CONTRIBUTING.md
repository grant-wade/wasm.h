# Contributing

Contributions are welcome. By submitting a contribution, you agree that it may be distributed under the repository's MIT license.

## Development

1. Initialize submodules: `git submodule update --init --recursive`.
2. Configure: `cmake -S . -B build`.
3. Run the strict suite: `cmake --build build --target check`.
4. For runtime changes, also run a Release build and the sanitizer configuration described in `docs/BUILDING.md`.

Keep the runtime C99-compatible and warning-free. Add focused regression tests for behavior changes. Changes to validation or execution semantics should pass the applicable official WebAssembly specification tests.

Report vulnerabilities privately as described in `SECURITY.md`, not through a public issue or pull request.
