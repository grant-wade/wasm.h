# Security Policy

## Supported versions

`wasm.h` is pre-1.0 software. Security fixes are applied to the latest release on the `main` branch. Older pre-1.0 releases are not maintained unless a release announcement says otherwise.

## Reporting a vulnerability

Please report suspected vulnerabilities privately to **grant@wade.software**. Include a reproducer, affected commit or version, impact, and any suggested mitigation. Please do not open a public issue until a fix or coordinated disclosure date is available.

An initial response should arrive within seven days. Confirmed issues will be tracked privately, fixed on supported branches, and disclosed with credit unless the reporter requests otherwise.

## Security model

The binary decoder, validator, and portable interpreter are intended to process untrusted Wasm, subject to configured memory, stack, call-depth, and fuel limits. This intent is not a claim that the pre-1.0 runtime has completed an independent security audit.

Host imports define the guest's capabilities:

- No host imports are bound automatically by the embedding API.
- The `wasm` CLI also binds no host imports by default.
- `wasm_bind_wasi_stubs()` exposes standard I/O and metadata helpers but does not preopen a host directory. Path operations therefore have no ambient filesystem capability.
- `wasm_bind_emscripten_stubs()` forwards some filesystem operations to the host. It is intended only for trusted, toolchain-produced modules.
- Application-defined host callbacks must validate guest pointers and lengths before accessing linear memory.

Fuel bounds interpreted instructions, but it does not impose a total host memory limit. Embedders handling adversarial input should combine conservative runtime configuration with operating-system resource limits.
