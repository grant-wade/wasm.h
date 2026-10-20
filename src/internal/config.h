#ifndef WASM_INTERNAL_CONFIG_H
#define WASM_INTERNAL_CONFIG_H

#include "../wasm.h"
#include "types.h"

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

#if defined(__GNUC__) || defined(__clang__)
#define WASM__NOINLINE __attribute__((noinline))
#define WASM__COLD __attribute__((noinline, cold))
#else
#define WASM__NOINLINE
#define WASM__COLD
#endif

#endif /* WASM_INTERNAL_CONFIG_H */
