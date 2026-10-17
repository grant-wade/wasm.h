# Contributing

Contributions are welcome. By submitting a contribution, you agree that it may be distributed under the repository's MIT license.

## Development

1. Initialize submodules: `git submodule update --init --recursive`.
2. Bootstrap the build program: `cc -o cbuild build.c`.
3. Run the strict suite: `./cbuild --test`.
4. For runtime changes, also run `./cbuild --release --test` and the sanitizer configuration in [docs/BUILDING.md](docs/BUILDING.md).

Keep the runtime C99-compatible and warning-free. Add focused regression tests for behavior changes. Changes to validation or execution semantics should pass the applicable official WebAssembly specification tests.

Report vulnerabilities privately as described in `SECURITY.md`, not through a public issue or pull request.
