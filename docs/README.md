# wasm.h Documentation

This directory contains release-facing documentation for the current `wasm.h` runtime.

- [Embedding Guide](EMBEDDING.md) — integrate the header, configure a runtime, bind imports, load modules, call exports, and handle errors.
- [Building and Testing](BUILDING.md) — build the tools and examples, install the header, run sanitizers, and fuzz the loader.
- [Security Policy](../SECURITY.md) — supported versions, private reporting, and the host-capability model.
- [Changelog](../CHANGELOG.md) — release-facing changes and API-stability policy.

The root [README](../README.md) is the project overview, feature matrix, CLI guide, and `wasm2api` guide. The public declarations near the top of [`wasm.h`](../wasm.h) are the authoritative API surface.
