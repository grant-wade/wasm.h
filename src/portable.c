#include "portable.h"
#include "module.h"
#include "runtime.h"
#include "internal/config.h"
#include <string.h>
#include <stdio.h>

WASM_PRIVATE uint32_t wasm__clz32(uint32_t v) {
    uint32_t n = 0;
    if (v == 0) return 32;
    if ((v & 0xFFFF0000u) == 0) {
        n += 16;
        v <<= 16;
    }
    if ((v & 0xFF000000u) == 0) {
        n += 8;
        v <<= 8;
    }
    if ((v & 0xF0000000u) == 0) {
        n += 4;
        v <<= 4;
    }
    if ((v & 0xC0000000u) == 0) {
        n += 2;
        v <<= 2;
    }
    if ((v & 0x80000000u) == 0) {
        n += 1;
    }
    return n;
}

WASM_PRIVATE uint32_t wasm__ctz32(uint32_t v) {
    uint32_t n = 0;
    if (v == 0) return 32;
    if ((v & 0x0000FFFFu) == 0) {
        n += 16;
        v >>= 16;
    }
    if ((v & 0x000000FFu) == 0) {
        n += 8;
        v >>= 8;
    }
    if ((v & 0x0000000Fu) == 0) {
        n += 4;
        v >>= 4;
    }
    if ((v & 0x00000003u) == 0) {
        n += 2;
        v >>= 2;
    }
    if ((v & 0x00000001u) == 0) {
        n += 1;
    }
    return n;
}

WASM_PRIVATE uint32_t wasm__popcnt32(uint32_t v) {
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    return (((v + (v >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24;
}

WASM_PRIVATE uint32_t wasm__clz64(uint64_t v) {
    uint32_t hi = (uint32_t)(v >> 32);
    return hi ? wasm__clz32(hi) : 32 + wasm__clz32((uint32_t)v);
}

WASM_PRIVATE uint32_t wasm__ctz64(uint64_t v) {
    uint32_t lo = (uint32_t)v;
    return lo ? wasm__ctz32(lo) : 32 + wasm__ctz32((uint32_t)(v >> 32));
}

WASM_PRIVATE uint32_t wasm__popcnt64(uint64_t v) {
    return wasm__popcnt32((uint32_t)v) + wasm__popcnt32((uint32_t)(v >> 32));
}

WASM_PRIVATE uint32_t wasm__shr_s32_bits(uint32_t value, uint32_t shift) {
    shift &= 31u;
    if (shift == 0u) return value;
    return (value >> shift) |
           ((value & 0x80000000u) ? (UINT32_MAX << (32u - shift)) : 0u);
}

WASM_PRIVATE uint64_t wasm__shr_s64_bits(uint64_t value, uint32_t shift) {
    shift &= 63u;
    if (shift == 0u) return value;
    return (value >> shift) |
           ((value & UINT64_C(0x8000000000000000)) ? (UINT64_MAX << (64u - shift)) : 0u);
}

#if WASM__BIG_ENDIAN
static uint16_t wasm__bswap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static uint32_t wasm__bswap32(uint32_t v) {
    return ((v >> 24) & 0x000000FFu) | ((v >> 8) & 0x0000FF00u) |
           ((v << 8) & 0x00FF0000u) | ((v << 24) & 0xFF000000u);
}

static uint64_t wasm__bswap64(uint64_t v) {
    uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
    return ((uint64_t)wasm__bswap32(lo) << 32) | (uint64_t)wasm__bswap32(hi);
}

#endif
WASM_PRIVATE uint16_t wasm__load_le16(const void* p) {
    uint16_t v;
    memcpy(&v, p, 2);
#if WASM__BIG_ENDIAN
    v = wasm__bswap16(v);
#endif
    return v;
}

WASM_PRIVATE uint32_t wasm__load_le32(const void* p) {
    uint32_t v;
    memcpy(&v, p, 4);
#if WASM__BIG_ENDIAN
    v = wasm__bswap32(v);
#endif
    return v;
}

WASM_PRIVATE uint64_t wasm__load_le64(const void* p) {
    uint64_t v;
    memcpy(&v, p, 8);
#if WASM__BIG_ENDIAN
    v = wasm__bswap64(v);
#endif
    return v;
}

WASM_PRIVATE void wasm__store_le16(void* p, uint16_t v) {
#if WASM__BIG_ENDIAN
    v = wasm__bswap16(v);
#endif
    memcpy(p, &v, 2);
}

WASM_PRIVATE void wasm__store_le32(void* p, uint32_t v) {
#if WASM__BIG_ENDIAN
    v = wasm__bswap32(v);
#endif
    memcpy(p, &v, 4);
}

WASM_PRIVATE void wasm__store_le64(void* p, uint64_t v) {
#if WASM__BIG_ENDIAN
    v = wasm__bswap64(v);
#endif
    memcpy(p, &v, 8);
}

static const char* wasm__feature_name(uint64_t flag) {
    switch (flag) {
        case WASM_FEATURE_SIGN_EXT:
            return "sign-extension operators";
        case WASM_FEATURE_NONTRAPPING_FMA:
            return "non-trapping float-to-int conversions";
        case WASM_FEATURE_MULTI_VALUE:
            return "multi-value";
        case WASM_FEATURE_MUTABLE_GLOBALS:
            return "mutable globals";
        case WASM_FEATURE_BULK_MEMORY:
            return "bulk memory";
        case WASM_FEATURE_REFERENCE_TYPES:
            return "reference types";
        case WASM_FEATURE_MULTI_MEMORY:
            return "multi-memory";
        case WASM_FEATURE_EXTENDED_CONST:
            return "extended const expressions";
        case WASM_FEATURE_TAIL_CALL:
            return "tail calls";
        case WASM_FEATURE_EXCEPTIONS:
            return "exceptions";
        case WASM_FEATURE_SIMD:
            return "SIMD";
        case WASM_FEATURE_GC:
            return "garbage collection";
        case WASM_FEATURE_MEMORY64:
            return "Memory64";
        case WASM_FEATURE_WIDE_ARITHMETIC:
            return "wide arithmetic";
        case WASM_FEATURE_CUSTOM_PAGE_SIZES:
            return "custom page sizes";
        case WASM_FEATURE_COMPACT_IMPORTS:
            return "compact imports";
        case WASM_FEATURE_THREADS:
            return "threads";
        case WASM_FEATURE_STACK_SWITCHING:
            return "stack switching";
        case WASM_FEATURE_CUSTOM_DESCRIPTORS:
            return "custom descriptors";
        default:
            return "unknown feature";
    }
}

WASM_PRIVATE wasm_error_t wasm__require_feature(wasm_module_t* mod, uint64_t flag) {
    mod->required_features |= flag;

    if ((mod->rt->features & flag) == 0) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "feature '%s' is disabled", wasm__feature_name(flag));
        return WASM_ERR_MALFORMED;
    }

    if ((flag & WASM_FEATURE_GC) != 0) {
        mod->required_features |= WASM_FEATURE_REFERENCE_TYPES;
        if ((mod->rt->features & WASM_FEATURE_REFERENCE_TYPES) == 0) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "feature '%s' is disabled",
                          wasm__feature_name(WASM_FEATURE_REFERENCE_TYPES));
            return WASM_ERR_MALFORMED;
        }
    }

    if ((flag & WASM_FEATURE_STACK_SWITCHING) != 0) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);

        if (err != WASM_OK) return err;
        return wasm__require_feature(mod, WASM_FEATURE_EXCEPTIONS);
    }
    if ((flag & WASM_FEATURE_CUSTOM_DESCRIPTORS) != 0) return wasm__require_feature(mod, WASM_FEATURE_GC);

    return WASM_OK;
}

WASM_PRIVATE int wasm__allow_tag_results(wasm_module_t* mod) {
    if ((mod->rt->features & WASM_FEATURE_STACK_SWITCHING) == 0) return 0;
    return wasm__require_feature(mod, WASM_FEATURE_STACK_SWITCHING) == WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__require_multi_table(wasm_module_t* mod) {
    if ((mod->rt->features & WASM_FEATURE_REFERENCE_TYPES) == 0) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "multiple tables");
        return WASM_ERR_MALFORMED;
    }

    mod->required_features |= WASM_FEATURE_REFERENCE_TYPES;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__require_multi_memory(wasm_module_t* mod) {
    if ((mod->rt->features & WASM_FEATURE_MULTI_MEMORY) == 0) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "multiple memories");
        return WASM_ERR_MALFORMED;
    }

    mod->required_features |= WASM_FEATURE_MULTI_MEMORY;
    return WASM_OK;
}
