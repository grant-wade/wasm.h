# Third-party material

The runtime and tools have no required third-party runtime dependencies.

`test/spec/` is a pinned Git submodule of the [WebAssembly testsuite](https://github.com/WebAssembly/testsuite) at the commit recorded by Git. It is used only for testing and is licensed under Apache License 2.0; its license is included at `test/spec/LICENSE`.

Release archives should either include the initialized submodule with that license or omit the test submodule. No opaque third-party Wasm binaries are present in the current source tree or release archives.

Earlier development history contains an `examples/sqlite3.wasm` artifact extracted from SQLite 3.51.3's official [`sqlite-wasm-3510300.zip`](https://sqlite.org/2026/sqlite-wasm-3510300.zip), published on the [SQLite download page](https://sqlite.org/download.html). The artifact was removed before the 1.0.0 release and is retained only as part of the project's development history.
