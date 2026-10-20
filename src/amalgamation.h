/* Assembly order for the single-header distribution. */
#ifndef WASM_AMALGAMATED_H
#define WASM_AMALGAMATED_H

#include "wasm.h"

#ifdef WASM_IMPL
#define WASM_AMALGAMATION
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include "support.c"
#include "handles.c"
#include "simd/native.c"
#include "platform/host.c"
#include "portable.c"
#include "atomics.c"
#include "platform/threads.c"
#include "reader.c"
#include "values.c"
#include "types/references.c"
#include "types/canonical.c"
#include "types/decode.c"
#include "module.c"
#include "types/cross_module.c"
#include "memory_internal.c"
#include "instructions.c"
#include "simd/lanes.c"
#include "simd/compare.c"
#include "simd/integer_small.c"
#include "simd/integer_wide.c"
#include "simd/float.c"
#include "init_expr.c"
#include "exec/stack.c"
#include "runtime.c"
#include "decode.c"
#include "validate/module.c"
#include "validate/stack.c"
#include "validate/gc.c"
#include "validate/simd.c"
#include "validate/numeric.c"
#include "validate/atomic.c"
#include "validate/cont.c"
#include "validate/function.c"
#include "load.c"
#include "exec/frames.c"
#include "exec/slots.c"
#include "gc.c"
#include "exec/calls.c"
#include "exec/cont.c"
#include "exec/translate.c"
#include "exec/exceptions.c"
#include "exec/gc.c"
#include "exec/bulk.c"
#include "exec/simd.c"
#include "exec/atomic.c"
#include "exec/interpreter.c"
#include "call.c"
#include "link.c"
#include "platform/wasi_state.c"
#include "platform/wasi_io.c"
#include "platform/wasi_fd.c"
#include "platform/wasi_path.c"
#include "platform/wasi_bind.c"
#include "platform/emscripten.c"
#include "platform/disabled.c"
#include "memory.c"
#include "inspect.c"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#undef WASM_PRIVATE
#undef WASM_PRIVATE_INLINE
#undef WASM_AMALGAMATION
#endif /* WASM_IMPL */
#endif /* WASM_AMALGAMATED_H */
