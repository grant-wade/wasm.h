#ifndef WASM_PORTABLE_H
#define WASM_PORTABLE_H

#include "internal/config.h"

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WASM__BIG_ENDIAN 1
#else
#define WASM__BIG_ENDIAN 0
#endif
#elif defined(_BIG_ENDIAN) || defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || defined(__MIPSEB__)
#define WASM__BIG_ENDIAN 1
#else
#define WASM__BIG_ENDIAN 0
#endif



WASM_PRIVATE uint32_t wasm__clz32(uint32_t v);

WASM_PRIVATE uint32_t wasm__ctz32(uint32_t v);

WASM_PRIVATE uint32_t wasm__popcnt32(uint32_t v);

WASM_PRIVATE uint32_t wasm__clz64(uint64_t v);

WASM_PRIVATE uint32_t wasm__ctz64(uint64_t v);

WASM_PRIVATE uint32_t wasm__popcnt64(uint64_t v);

WASM_PRIVATE uint32_t wasm__shr_s32_bits(uint32_t value, uint32_t shift);

WASM_PRIVATE uint64_t wasm__shr_s64_bits(uint64_t value, uint32_t shift);

WASM_PRIVATE uint16_t wasm__load_le16(const void* p);

WASM_PRIVATE uint32_t wasm__load_le32(const void* p);

WASM_PRIVATE uint64_t wasm__load_le64(const void* p);

WASM_PRIVATE void wasm__store_le16(void* p, uint16_t v);

WASM_PRIVATE void wasm__store_le32(void* p, uint32_t v);

WASM_PRIVATE void wasm__store_le64(void* p, uint64_t v);

WASM_PRIVATE wasm_error_t wasm__require_feature(wasm_module_t* mod, uint64_t flag);

/* Returns whether tag types may declare results, recording the requirement when they may. */
WASM_PRIVATE int wasm__allow_tag_results(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__require_multi_memory(wasm_module_t* mod);

/* A single funcref table is MVP; further tables need reference types. */
WASM_PRIVATE wasm_error_t wasm__require_multi_table(wasm_module_t* mod);

#endif /* WASM_PORTABLE_H */
