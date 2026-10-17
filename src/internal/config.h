#ifndef WASM_INTERNAL_CONFIG_H
#define WASM_INTERNAL_CONFIG_H

#include "../wasm.h"

#ifdef WASM_AMALGAMATION
#define WASM_PRIVATE static
#define WASM_PRIVATE_INLINE WASM_INLINE
#else
#define WASM_PRIVATE extern
#define WASM_PRIVATE_INLINE extern
#endif

#if defined(_MSC_VER)
#define WASM__ALIGNOF(type) __alignof(type)
#else
#define WASM__ALIGNOF(type) offsetof(struct { char c; type value; }, value)
#endif

#define WASM__IMPLEMENTED_FEATURES                                                     \
    (WASM_FEATURE_SIGN_EXT | WASM_FEATURE_NONTRAPPING_FMA | WASM_FEATURE_MULTI_VALUE | \
     WASM_FEATURE_MUTABLE_GLOBALS | WASM_FEATURE_BULK_MEMORY |                         \
     WASM_FEATURE_REFERENCE_TYPES | WASM_FEATURE_MULTI_MEMORY |                        \
     WASM_FEATURE_EXTENDED_CONST | WASM_FEATURE_TAIL_CALL | WASM_FEATURE_EXCEPTIONS |  \
     WASM_FEATURE_SIMD | WASM_FEATURE_GC | WASM_FEATURE_MEMORY64)

#endif /* WASM_INTERNAL_CONFIG_H */
