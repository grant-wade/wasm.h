# Third-party material

The runtime and tools have no required third-party runtime dependencies.

`test/spec/` is a pinned Git submodule of the [WebAssembly testsuite](https://github.com/WebAssembly/testsuite) at the commit recorded by Git. It is used only for testing and is licensed under Apache License 2.0; its license is included at `test/spec/LICENSE`.

Release archives should either include the initialized submodule with that license or omit the test submodule. No opaque third-party Wasm binaries are distributed in this repository.
