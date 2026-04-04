/*
 * wasm.h — Single-header WebAssembly runtime for C99
 *
 * USAGE:
 *   #define WASM_IMPL
 *   #include "wasm.h"
 *
 * CUSTOM ALLOCATORS:
 *   #define WASM_MALLOC(sz)      my_malloc(sz)
 *   #define WASM_REALLOC(p, sz)  my_realloc(p, sz)
 *   #define WASM_FREE(p)         my_free(p)
 *   #define WASM_CALLOC(n, sz)   my_calloc(n, sz)
 *   #define WASM_IMPL
 *   #include "wasm.h"
 *
 * QUICK START:
 *   wasm_runtime_t rt;
 *   wasm_init(&rt);
 *   wasm_module_t *mod = wasm_load(&rt, bytecode, bytecode_len);
 *   wasm_value_t args[1];
 *   args[0] = wasm_i32(42);
 *   wasm_value_t result;
 *   wasm_call(mod, "main", args, 1, &result, 1);
 *   printf("result = %d\n", result.of.i32);
 *
 * INTROSPECTION:
 *   uint32_t export_count = wasm_export_count(mod);
 *   for (uint32_t i = 0; i < export_count; i++) {
 *       printf("export %s kind=%u index=%u\n",
 *              wasm_export_name(mod, i),
 *              (unsigned)wasm_export_kind(mod, i),
 *              (unsigned)wasm_export_index(mod, i));
 *   }
 *   wasm_export_kind_t kind;
 *   uint32_t func_idx;
 *   if (wasm_find_export(mod, "main", &kind, &func_idx) && kind == WASM_EXPORT_FUNC) {
 *       printf("main takes %u params and returns %u values\n",
 *              (unsigned)wasm_func_param_count(mod, func_idx),
 *              (unsigned)wasm_func_result_count(mod, func_idx));
 *       wasm_call_index(mod, func_idx, args, 1, &result, 1);
 *   }
 *   wasm_free_module(mod);
 *   wasm_destroy(&rt);
 *
 * LICENSE: MIT
 *
 * COMPATIBILITY:
 *   - C99 or later (no compiler extensions required)
 *   - GCC, Clang, MSVC (2015+), TCC, Intel ICC, PGI/NVHPC
 *   - Windows, Linux, macOS, *BSD, bare-metal (with libc)
 *   - Little-endian and big-endian
 *   - 32-bit and 64-bit
 *
 * Targets: Wasm 1.0 plus selected post-MVP proposals
 *   - i32, i64, f32, f64, funcref, and externref values
 *   - Imports/exports, including imported and mutable globals
 *   - Linear memories with bounds checks, indexed memory access, and memory.grow
 *   - Tables, call_indirect, and reference-type table operations
 *   - Block/loop/if/br/br_if/br_table control flow with multi-value support
 *   - Bulk memory, sign-extension ops, trunc_sat conversions, extended const exprs, tail calls, exceptions, and SIMD
 *   - Load-time validation plus runtime-configurable feature gating
 *   - No threads or GC
 */

#ifndef WASM_H
#define WASM_H

/* ── Platform detection & compat shims ────────────────────────────── */

#if defined(_MSC_VER)
#if _MSC_VER < 1900
#error "wasm.h requires MSVC 2015 (v19.0) or later"
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#if !defined(__func__)
#define __func__ __FUNCTION__
#endif
#endif

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFFu
#endif

#ifndef INT32_MIN
#define INT32_MIN (-2147483647 - 1)
#endif

#ifndef INT64_MIN
#define INT64_MIN (-9223372036854775807LL - 1LL)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Inline portability ───────────────────────────────────────────── */
#if defined(_MSC_VER) && !defined(__cplusplus)
#define WASM_INLINE static __inline
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define WASM_INLINE static inline
#else
#define WASM_INLINE static
#endif

/* ── Error codes ──────────────────────────────────────────────────── */
typedef enum wasm_error_t {
    WASM_OK = 0,
    WASM_ERR_INVALID_MAGIC,
    WASM_ERR_INVALID_VERSION,
    WASM_ERR_MALFORMED,
    WASM_ERR_INVALID_SECTION,
    WASM_ERR_TYPE_MISMATCH,
    WASM_ERR_UNKNOWN_IMPORT,
    WASM_ERR_UNDEFINED_EXPORT,
    WASM_ERR_STACK_OVERFLOW,
    WASM_ERR_STACK_UNDERFLOW,
    WASM_ERR_OUT_OF_BOUNDS,
    WASM_ERR_UNREACHABLE,
    WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH,
    WASM_ERR_UNDEFINED_ELEMENT,
    WASM_ERR_UNINITIALIZED_TABLE,
    WASM_ERR_DIV_BY_ZERO,
    WASM_ERR_INT_OVERFLOW,
    WASM_ERR_INVALID_CONVERSION,
    WASM_ERR_CALL_STACK_EXHAUSTED,
    WASM_ERR_OOM,
    WASM_ERR_TRAP
} wasm_error_t;

/* ── Feature flags ───────────────────────────────────────────────── */
#define WASM_FEATURE_SIGN_EXT (1u << 0)
#define WASM_FEATURE_NONTRAPPING_FMA (1u << 1)
#define WASM_FEATURE_MULTI_VALUE (1u << 2)
#define WASM_FEATURE_MUTABLE_GLOBALS (1u << 3)
#define WASM_FEATURE_BULK_MEMORY (1u << 4)
#define WASM_FEATURE_REFERENCE_TYPES (1u << 5)
#define WASM_FEATURE_MULTI_MEMORY (1u << 6)
#define WASM_FEATURE_EXTENDED_CONST (1u << 7)
#define WASM_FEATURE_TAIL_CALL (1u << 8)
#define WASM_FEATURE_EXCEPTIONS (1u << 9)
#define WASM_FEATURE_SIMD (1u << 10)

/* ── Value types ──────────────────────────────────────────────────── */
typedef enum wasm_valtype_t {
    WASM_TYPE_EXTERNREF = 0x6F,
    WASM_TYPE_FUNCREF = 0x70,
    WASM_TYPE_V128 = 0x7B,
    WASM_TYPE_I32 = 0x7F,
    WASM_TYPE_I64 = 0x7E,
    WASM_TYPE_F32 = 0x7D,
    WASM_TYPE_F64 = 0x7C,
    WASM_TYPE_VOID = 0x40
} wasm_valtype_t;

typedef struct wasm_value_t {
    wasm_valtype_t type;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        uint8_t v128[16];
        uint32_t funcref;
        uintptr_t externref;
    } of;
} wasm_value_t;

WASM_INLINE wasm_value_t wasm_i32(int32_t v) {
    wasm_value_t r;
    r.type = WASM_TYPE_I32;
    r.of.i32 = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_i64(int64_t v) {
    wasm_value_t r;
    r.type = WASM_TYPE_I64;
    r.of.i64 = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_f32(float v) {
    wasm_value_t r;
    r.type = WASM_TYPE_F32;
    r.of.f32 = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_f64(double v) {
    wasm_value_t r;
    r.type = WASM_TYPE_F64;
    r.of.f64 = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_v128(const uint8_t bytes[16]) {
    wasm_value_t r;
    uint32_t i;

    r.type = WASM_TYPE_V128;
    for (i = 0; i < 16; i++) r.of.v128[i] = bytes ? bytes[i] : 0;
    return r;
}
WASM_INLINE wasm_value_t wasm_v128_zero(void) {
    return wasm_v128(NULL);
}
WASM_INLINE wasm_value_t wasm_funcref(uint32_t v) {
    wasm_value_t r;
    r.type = WASM_TYPE_FUNCREF;
    r.of.funcref = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_externref(uintptr_t v) {
    wasm_value_t r;
    r.type = WASM_TYPE_EXTERNREF;
    r.of.externref = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_ref_null(wasm_valtype_t type) {
    wasm_value_t r;
    r.type = type;
    if (type == WASM_TYPE_FUNCREF)
        r.of.funcref = UINT32_MAX;
    else
        r.of.externref = (uintptr_t)0;
    return r;
}

/* ── Forward declarations ─────────────────────────────────────────── */
typedef struct wasm_module_t wasm_module_t;
typedef struct wasm_runtime_t wasm_runtime_t;

/* ── Host function callback ───────────────────────────────────────── */
typedef wasm_error_t (*wasm_host_func_t)(
    wasm_runtime_t* rt,
    const wasm_value_t* args, uint32_t num_args,
    wasm_value_t* results, uint32_t num_results,
    void* userdata);

/* ── Function type signature ──────────────────────────────────────── */
typedef struct wasm_functype_t {
    uint32_t num_params;
    uint32_t num_results;
    wasm_valtype_t* params;
    wasm_valtype_t* results;
} wasm_functype_t;

/* ── Import binding ───────────────────────────────────────────────── */
typedef struct wasm_import_t {
    const char* module;
    const char* name;
    wasm_functype_t type;
    wasm_host_func_t func;
    void* userdata;
} wasm_import_t;

typedef struct wasm_global_import_t {
    const char* module;
    const char* name;
    wasm_valtype_t type;
    int is_mutable;
    wasm_value_t* value;
} wasm_global_import_t;

typedef struct wasm__control_target_t {
    uint32_t end_offset;
    uint32_t aux_offset;
} wasm__control_target_t;

/* ── Internal function representation ─────────────────────────────── */
typedef struct wasm_func_t {
    uint32_t type_idx;
    uint32_t num_locals;
    wasm_valtype_t* locals;
    const uint8_t* code;
    uint32_t code_len;
    wasm__control_target_t* control_targets;
    int is_import;
    wasm_host_func_t host_func;
    void* host_userdata;
} wasm_func_t;

/* ── Export entry ──────────────────────────────────────────────────── */
typedef enum wasm_export_kind_t {
    WASM_EXPORT_FUNC = 0x00,
    WASM_EXPORT_TABLE = 0x01,
    WASM_EXPORT_MEM = 0x02,
    WASM_EXPORT_GLOBAL = 0x03,
    WASM_EXPORT_TAG = 0x04
} wasm_export_kind_t;

typedef struct wasm_export_t {
    char name[128];
    wasm_export_kind_t kind;
    uint32_t index;
} wasm_export_t;

/* ── Global variable ──────────────────────────────────────────────── */
typedef struct wasm_global_t {
    wasm_valtype_t type;
    int is_mutable;
    int is_import;
    wasm_value_t* import_value;
    wasm_value_t value;
} wasm_global_t;

/* ── Table ────────────────────────────────────────────────────────── */
typedef struct wasm_table_t {
    wasm_valtype_t reftype;
    wasm_value_t* elems;
    uint32_t size;
    uint32_t max_size;
} wasm_table_t;

typedef struct wasm_data_segment_t {
    uint8_t* bytes;
    uint32_t size;
    uint32_t memory_index;
    uint32_t offset;
    int is_passive;
    int is_dropped;
} wasm_data_segment_t;

typedef struct wasm_elem_segment_t {
    wasm_valtype_t elem_type;
    wasm_value_t* elems;
    uint32_t num_elems;
    uint32_t table_index;
    uint32_t offset;
    int is_passive;
    int is_declarative;
    int is_dropped;
} wasm_elem_segment_t;

/* ── Linear memory ────────────────────────────────────────────────── */
#define WASM_PAGE_SIZE 65536u

typedef struct wasm_memory_t {
    uint8_t* data;
    uint32_t pages;
    uint32_t max_pages;
} wasm_memory_t;

typedef struct wasm_tag_t {
    uint32_t type_idx;
} wasm_tag_t;

typedef struct wasm__exception_state_t {
    uint32_t tag_index;
    wasm_value_t* values;
    uint32_t num_values;
    int is_pending;
} wasm__exception_state_t;

/* ── Module ───────────────────────────────────────────────────────── */
struct wasm_module_t {
    wasm_runtime_t* rt;
    wasm_functype_t* types;
    uint32_t num_types;
    wasm_func_t* funcs;
    uint32_t num_funcs;
    wasm_export_t* exports;
    uint32_t num_exports;
    wasm_global_t* globals;
    uint32_t num_globals;
    wasm_tag_t* tags;
    uint32_t num_tags;
    wasm_table_t* tables;
    uint32_t num_tables;
    wasm_memory_t* memories;
    wasm_data_segment_t* data_segments;
    uint32_t num_data_segments;
    wasm_elem_segment_t* elem_segments;
    uint32_t num_elem_segments;
    uint32_t num_func_imports;
    uint32_t num_global_imports;
    uint32_t num_memories;
    uint32_t num_code_bodies;
    uint32_t declared_data_count;
    uint32_t required_features;
    int has_data_count_section;
    int uses_data_count_section;
    uint32_t start_func;
    int startup_ran;
};

/* ── Runtime configuration ────────────────────────────────────────── */
#ifndef WASM_MAX_STACK
#define WASM_MAX_STACK 4096
#endif
#ifndef WASM_MAX_CALL_DEPTH
#define WASM_MAX_CALL_DEPTH 512
#endif

struct wasm_runtime_t {
    wasm_import_t* imports;
    uint32_t num_imports;
    uint32_t cap_imports;
    wasm_global_import_t* global_imports;
    uint32_t num_global_imports;
    uint32_t cap_global_imports;
    wasm_value_t stack[WASM_MAX_STACK];
    uint32_t sp;
    uint32_t enabled_features;
    wasm_error_t last_error;
    char error_msg[256];
    wasm__exception_state_t pending_exception;
};

/* ── Public API ───────────────────────────────────────────────────── */
wasm_error_t wasm_init(wasm_runtime_t* rt);
void wasm_destroy(wasm_runtime_t* rt);
wasm_error_t wasm_register_import(wasm_runtime_t* rt, const wasm_import_t* imp);
wasm_error_t wasm_register_global_import(wasm_runtime_t* rt, const wasm_global_import_t* imp);
void wasm_enable_feature(wasm_runtime_t* rt, uint32_t flag);
void wasm_disable_feature(wasm_runtime_t* rt, uint32_t flag);
void wasm_enable_all_features(wasm_runtime_t* rt);
wasm_module_t* wasm_load(wasm_runtime_t* rt, const uint8_t* bytes, size_t len);
void wasm_free_module(wasm_module_t* mod);
wasm_error_t wasm_call(wasm_module_t* mod, const char* func_name,
                       const wasm_value_t* args, uint32_t num_args,
                       wasm_value_t* results, uint32_t num_results);
wasm_error_t wasm_call_index(wasm_module_t* mod, uint32_t func_idx,
                             const wasm_value_t* args, uint32_t num_args,
                             wasm_value_t* results, uint32_t num_results);
uint32_t wasm_memory_count(wasm_module_t* mod);
uint8_t* wasm_memory_data_at(wasm_module_t* mod, uint32_t memory_index);
uint32_t wasm_memory_size_at(wasm_module_t* mod, uint32_t memory_index);
int32_t wasm_memory_grow_at(wasm_module_t* mod, uint32_t memory_index, uint32_t delta_pages);
uint8_t* wasm_memory_data(wasm_module_t* mod);
uint32_t wasm_memory_size(wasm_module_t* mod);
int32_t wasm_memory_grow(wasm_module_t* mod, uint32_t delta_pages);
uint32_t wasm_export_count(wasm_module_t* mod);
const char* wasm_export_name(wasm_module_t* mod, uint32_t index);
wasm_export_kind_t wasm_export_kind(wasm_module_t* mod, uint32_t index);
uint32_t wasm_export_index(wasm_module_t* mod, uint32_t export_index);
int wasm_find_export(wasm_module_t* mod, const char* name,
                     wasm_export_kind_t* out_kind, uint32_t* out_index);
uint32_t wasm_func_count(wasm_module_t* mod);
uint32_t wasm_func_param_count(wasm_module_t* mod, uint32_t func_idx);
uint32_t wasm_func_result_count(wasm_module_t* mod, uint32_t func_idx);
wasm_valtype_t wasm_func_param_type(wasm_module_t* mod, uint32_t func_idx,
                                    uint32_t param_idx);
wasm_valtype_t wasm_func_result_type(wasm_module_t* mod, uint32_t func_idx,
                                     uint32_t result_idx);
const char* wasm_error_string(wasm_error_t err);

#ifdef __cplusplus
}
#endif

/* ═══════════════════════════════════════════════════════════════════ */
#ifdef WASM_IMPL
/* ═══════════════════════════════════════════════════════════════════ */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WASM_MALLOC
#define WASM__HAS_CUSTOM_MALLOC 1
#endif

#ifndef WASM_MALLOC
#define WASM_MALLOC(sz) malloc(sz)
#endif
#ifndef WASM_REALLOC
#define WASM_REALLOC(p, sz) realloc(p, sz)
#endif
#ifndef WASM_FREE
#define WASM_FREE(p) free(p)
#endif
#ifndef WASM_CALLOC
#if defined(WASM__HAS_CUSTOM_MALLOC)
#define WASM__NEEDS_CALLOC_FALLBACK 1
#else
#define WASM_CALLOC(n, sz) calloc(n, sz)
#endif
#endif

#if defined(WASM__NEEDS_CALLOC_FALLBACK)
static void* wasm__calloc_fallback(size_t count, size_t size) {
    size_t total;
    void* ptr;

    if (size != 0 && count > ((size_t)-1) / size) return NULL;

    total = count * size;
    ptr = WASM_MALLOC(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}
#define WASM_CALLOC(n, sz) wasm__calloc_fallback((n), (sz))
#endif

#undef WASM__HAS_CUSTOM_MALLOC
#undef WASM__NEEDS_CALLOC_FALLBACK

/* ── Portable bit operations (pure C99, no builtins) ──────────────── */

static uint32_t wasm__clz32(uint32_t v) {
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

static uint32_t wasm__ctz32(uint32_t v) {
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

static uint32_t wasm__popcnt32(uint32_t v) {
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    return (((v + (v >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24;
}

static uint32_t wasm__clz64(uint64_t v) {
    uint32_t hi = (uint32_t)(v >> 32);
    return hi ? wasm__clz32(hi) : 32 + wasm__clz32((uint32_t)v);
}

static uint32_t wasm__ctz64(uint64_t v) {
    uint32_t lo = (uint32_t)v;
    return lo ? wasm__ctz32(lo) : 32 + wasm__ctz32((uint32_t)(v >> 32));
}

static uint32_t wasm__popcnt64(uint64_t v) {
    return wasm__popcnt32((uint32_t)v) + wasm__popcnt32((uint32_t)(v >> 32));
}

/* ── Endian-aware memory access ───────────────────────────────────── */

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

static uint16_t wasm__load_le16(const void* p) {
    uint16_t v;
    memcpy(&v, p, 2);
#if WASM__BIG_ENDIAN
    v = wasm__bswap16(v);
#endif
    return v;
}

static uint32_t wasm__load_le32(const void* p) {
    uint32_t v;
    memcpy(&v, p, 4);
#if WASM__BIG_ENDIAN
    v = wasm__bswap32(v);
#endif
    return v;
}

static uint64_t wasm__load_le64(const void* p) {
    uint64_t v;
    memcpy(&v, p, 8);
#if WASM__BIG_ENDIAN
    v = wasm__bswap64(v);
#endif
    return v;
}

static void wasm__store_le16(void* p, uint16_t v) {
#if WASM__BIG_ENDIAN
    v = wasm__bswap16(v);
#endif
    memcpy(p, &v, 2);
}

static void wasm__store_le32(void* p, uint32_t v) {
#if WASM__BIG_ENDIAN
    v = wasm__bswap32(v);
#endif
    memcpy(p, &v, 4);
}

static void wasm__store_le64(void* p, uint64_t v) {
#if WASM__BIG_ENDIAN
    v = wasm__bswap64(v);
#endif
    memcpy(p, &v, 8);
}

/* ── Portable snprintf ────────────────────────────────────────────── */
#if defined(_MSC_VER) && _MSC_VER < 1900
#define wasm__snprintf _snprintf
#else
#define wasm__snprintf snprintf
#endif

#define WASM__SET_ERR(rt, code, ...)                             \
    do {                                                         \
        (rt)->last_error = (code);                               \
        wasm__snprintf((rt)->error_msg, sizeof((rt)->error_msg), \
                       __VA_ARGS__);                             \
    } while (0)

#define WASM__IMPLEMENTED_FEATURES                                                     \
    (WASM_FEATURE_SIGN_EXT | WASM_FEATURE_NONTRAPPING_FMA | WASM_FEATURE_MULTI_VALUE | \
     WASM_FEATURE_MUTABLE_GLOBALS | WASM_FEATURE_BULK_MEMORY |                         \
     WASM_FEATURE_REFERENCE_TYPES | WASM_FEATURE_MULTI_MEMORY |                        \
     WASM_FEATURE_EXTENDED_CONST | WASM_FEATURE_TAIL_CALL | WASM_FEATURE_EXCEPTIONS |  \
     WASM_FEATURE_SIMD)

static const char* wasm__feature_name(uint32_t flag) {
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
        default:
            return "unknown feature";
    }
}

static wasm_error_t wasm__require_feature(wasm_module_t* mod, uint32_t flag) {
    mod->required_features |= flag;

    if ((flag & WASM__IMPLEMENTED_FEATURES) == 0) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "feature '%s' is not implemented", wasm__feature_name(flag));
        return WASM_ERR_MALFORMED;
    }

    if ((mod->rt->enabled_features & flag) == 0) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "feature '%s' is disabled", wasm__feature_name(flag));
        return WASM_ERR_MALFORMED;
    }

    return WASM_OK;
}

/* ── LEB128 decoding ─────────────────────────────────────────────── */
typedef struct wasm__reader_t {
    const uint8_t* ptr;
    const uint8_t* end;
    int malformed;
} wasm__reader_t;

static int wasm__has(wasm__reader_t* r, size_t n) {
    return !r->malformed && (size_t)(r->end - r->ptr) >= n;
}

static uint8_t wasm__read_u8(wasm__reader_t* r) {
    if (!wasm__has(r, 1)) {
        r->ptr = r->end;
        r->malformed = 1;
        return 0;
    }
    return *r->ptr++;
}

static uint32_t wasm__read_leb128_u32(wasm__reader_t* r) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint32_t i;

    for (i = 0; i < 5; i++) {
        uint8_t byte = wasm__read_u8(r);

        if (r->malformed) return 0;
        if (i == 4 && (byte & 0xF0u) != 0) {
            r->ptr = r->end;
            r->malformed = 1;
            return 0;
        }

        result |= (uint32_t)(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) return result;
        shift += 7;
    }

    r->ptr = r->end;
    r->malformed = 1;
    return 0;
}

static int32_t wasm__read_leb128_i32(wasm__reader_t* r) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint32_t i;

    for (i = 0; i < 5; i++) {
        uint8_t byte = wasm__read_u8(r);

        if (r->malformed) return 0;
        if (i == 4) {
            uint8_t payload = (uint8_t)(byte & 0x7Fu);
            uint8_t high_bits = (uint8_t)(payload & 0x70u);

            if (high_bits != 0x00u && high_bits != 0x70u) {
                r->ptr = r->end;
                r->malformed = 1;
                return 0;
            }
        }

        result |= (uint32_t)(byte & 0x7Fu) << shift;
        shift += 7;
        if ((byte & 0x80u) == 0) {
            if (shift < 32 && (byte & 0x40u)) result |= ~0u << shift;
            return (int32_t)result;
        }
    }

    r->ptr = r->end;
    r->malformed = 1;
    return 0;
}

static int64_t wasm__read_leb128_i64(wasm__reader_t* r) {
    uint64_t result = 0;
    uint32_t shift = 0;
    uint32_t i;

    for (i = 0; i < 10; i++) {
        uint8_t byte = wasm__read_u8(r);

        if (r->malformed) return 0;
        if (i == 9) {
            uint8_t payload = (uint8_t)(byte & 0x7Fu);

            if (payload != 0x00u && payload != 0x7Fu) {
                r->ptr = r->end;
                r->malformed = 1;
                return 0;
            }
        }

        result |= (uint64_t)(byte & 0x7Fu) << shift;
        shift += 7;
        if ((byte & 0x80u) == 0) {
            if (shift < 64 && (byte & 0x40u)) result |= ~(uint64_t)0 << shift;
            return (int64_t)result;
        }
    }

    r->ptr = r->end;
    r->malformed = 1;
    return 0;
}

static float wasm__read_f32(wasm__reader_t* r) {
    if (!wasm__has(r, 4)) {
        r->ptr = r->end;
        r->malformed = 1;
        return 0.0f;
    }
    uint32_t bits = wasm__load_le32(r->ptr);
    float v;
    memcpy(&v, &bits, 4);
    r->ptr += 4;
    return v;
}

static double wasm__read_f64(wasm__reader_t* r) {
    if (!wasm__has(r, 8)) {
        r->ptr = r->end;
        r->malformed = 1;
        return 0.0;
    }
    uint64_t bits = wasm__load_le64(r->ptr);
    double v;
    memcpy(&v, &bits, 8);
    r->ptr += 8;
    return v;
}

static void wasm__read_name(wasm__reader_t* r, char* buf, size_t bufsz) {
    uint32_t len = wasm__read_leb128_u32(r);
    uint32_t copy = len < (uint32_t)(bufsz - 1) ? len : (uint32_t)(bufsz - 1);

    if (r->malformed || !wasm__has(r, len)) {
        buf[0] = '\0';
        r->ptr = r->end;
        r->malformed = 1;
        return;
    }

    memcpy(buf, r->ptr, copy);
    buf[copy] = '\0';
    r->ptr += len;
}

static wasm_value_t wasm__u32_bits(uint32_t value) {
    wasm_value_t result;

    result.type = WASM_TYPE_I32;
    memcpy(&result.of.i32, &value, sizeof(value));
    return result;
}

static wasm_value_t wasm__u64_bits(uint64_t value) {
    wasm_value_t result;

    result.type = WASM_TYPE_I64;
    memcpy(&result.of.i64, &value, sizeof(value));
    return result;
}

static int wasm__is_ref_type(wasm_valtype_t type) {
    return type == WASM_TYPE_FUNCREF || type == WASM_TYPE_EXTERNREF;
}

static int wasm__is_vector_type(wasm_valtype_t type) {
    return type == WASM_TYPE_V128;
}

static int wasm__is_numeric_type(wasm_valtype_t type) {
    return type == WASM_TYPE_I32 || type == WASM_TYPE_I64 ||
           type == WASM_TYPE_F32 || type == WASM_TYPE_F64;
}

static int wasm__is_number_or_vector_type(wasm_valtype_t type) {
    return wasm__is_numeric_type(type) || wasm__is_vector_type(type);
}

static int wasm__is_value_type(wasm_valtype_t type) {
    return wasm__is_number_or_vector_type(type) || wasm__is_ref_type(type);
}

static int wasm__is_null_ref(const wasm_value_t* value) {
    if (value->type == WASM_TYPE_FUNCREF) return value->of.funcref == UINT32_MAX;
    if (value->type == WASM_TYPE_EXTERNREF) return value->of.externref == (uintptr_t)0;
    return 0;
}

static int wasm__value_matches_type(const wasm_value_t* value, wasm_valtype_t type) {
    return value->type == type;
}

static wasm_value_t wasm__default_value(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_I32:
            return wasm_i32(0);
        case WASM_TYPE_I64:
            return wasm_i64(0);
        case WASM_TYPE_F32:
            return wasm_f32(0.0f);
        case WASM_TYPE_F64:
            return wasm_f64(0.0);
        case WASM_TYPE_V128:
            return wasm_v128_zero();
        case WASM_TYPE_FUNCREF:
        case WASM_TYPE_EXTERNREF:
            return wasm_ref_null(type);
        default: {
            wasm_value_t result;
            memset(&result, 0, sizeof(result));
            result.type = type;
            return result;
        }
    }
}

static wasm_value_t wasm__trunc_sat_i32_s(double value) {
    if (isnan(value)) return wasm_i32(0);
    if (value <= -2147483648.0) return wasm_i32(INT32_MIN);
    if (value >= 2147483648.0) return wasm_i32(INT32_MAX);
    return wasm_i32((int32_t)value);
}

static wasm_value_t wasm__trunc_sat_i32_u(double value) {
    if (isnan(value) || value <= 0.0) return wasm__u32_bits(0);
    if (value >= 4294967296.0) return wasm__u32_bits(UINT32_MAX);
    return wasm__u32_bits((uint32_t)value);
}

static wasm_value_t wasm__trunc_sat_i64_s(double value) {
    if (isnan(value)) return wasm_i64(0);
    if (value <= -9223372036854775808.0) return wasm_i64(INT64_MIN);
    if (value >= 9223372036854775808.0) return wasm_i64(INT64_MAX);
    return wasm_i64((int64_t)value);
}

static wasm_value_t wasm__trunc_sat_i64_u(double value) {
    if (isnan(value) || value <= 0.0) return wasm__u64_bits(0);
    if (value >= 18446744073709551616.0) return wasm__u64_bits(UINT64_MAX);
    return wasm__u64_bits((uint64_t)value);
}

static wasm_error_t wasm__trunc_i32_s(double value, wasm_value_t* out_value) {
    double truncated;

    if (isnan(value))
        return WASM_ERR_INVALID_CONVERSION;
    truncated = trunc(value);
    if (truncated < -2147483648.0 || truncated >= 2147483648.0)
        return WASM_ERR_INVALID_CONVERSION;
    *out_value = wasm_i32((int32_t)truncated);
    return WASM_OK;
}

static wasm_error_t wasm__trunc_i32_u(double value, wasm_value_t* out_value) {
    double truncated;

    if (isnan(value))
        return WASM_ERR_INVALID_CONVERSION;
    truncated = trunc(value);
    if (truncated < 0.0 || truncated >= 4294967296.0)
        return WASM_ERR_INVALID_CONVERSION;
    *out_value = wasm__u32_bits((uint32_t)truncated);
    return WASM_OK;
}

static wasm_error_t wasm__trunc_i64_s(double value, wasm_value_t* out_value) {
    double truncated;

    if (isnan(value))
        return WASM_ERR_INVALID_CONVERSION;
    truncated = trunc(value);
    if (truncated < -9223372036854775808.0 || truncated >= 9223372036854775808.0)
        return WASM_ERR_INVALID_CONVERSION;
    *out_value = wasm_i64((int64_t)truncated);
    return WASM_OK;
}

static wasm_error_t wasm__trunc_i64_u(double value, wasm_value_t* out_value) {
    double truncated;

    if (isnan(value))
        return WASM_ERR_INVALID_CONVERSION;
    truncated = trunc(value);
    if (truncated < 0.0 || truncated >= 18446744073709551616.0)
        return WASM_ERR_INVALID_CONVERSION;
    *out_value = wasm__u64_bits((uint64_t)truncated);
    return WASM_OK;
}

static float wasm__f32_min(float lhs, float rhs) {
    if (isnan(lhs) || isnan(rhs)) return lhs + rhs;
    return fminf(lhs, rhs);
}

static float wasm__f32_max(float lhs, float rhs) {
    if (isnan(lhs) || isnan(rhs)) return lhs + rhs;
    return fmaxf(lhs, rhs);
}

static double wasm__f64_min(double lhs, double rhs) {
    if (isnan(lhs) || isnan(rhs)) return lhs + rhs;
    return fmin(lhs, rhs);
}

static double wasm__f64_max(double lhs, double rhs) {
    if (isnan(lhs) || isnan(rhs)) return lhs + rhs;
    return fmax(lhs, rhs);
}

static float wasm__nearest_f32(float value) {
    return nearbyintf(value);
}

static double wasm__nearest_f64(double value) {
    return nearbyint(value);
}

static wasm_error_t wasm__require_valtype_feature(wasm_module_t* mod, wasm_valtype_t type) {
    if (wasm__is_ref_type(type)) return wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
    if (wasm__is_vector_type(type)) return wasm__require_feature(mod, WASM_FEATURE_SIMD);
    return WASM_OK;
}

static int wasm__is_valtype_byte(uint8_t byte) {
    switch (byte) {
        case 0x6F:
        case 0x70:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
            return 1;
        default:
            return 0;
    }
}

static void wasm__free_functype(wasm_functype_t* ft) {
    WASM_FREE(ft->params);
    WASM_FREE(ft->results);
    ft->params = NULL;
    ft->results = NULL;
    ft->num_params = 0;
    ft->num_results = 0;
}

static wasm_error_t wasm__copy_functype(wasm_functype_t* dst, const wasm_functype_t* src) {
    memset(dst, 0, sizeof(*dst));
    dst->num_params = src->num_params;
    dst->num_results = src->num_results;

    if (src->num_params > 0) {
        dst->params = (wasm_valtype_t*)WASM_MALLOC(src->num_params * sizeof(wasm_valtype_t));
        if (!dst->params) {
            wasm__free_functype(dst);
            return WASM_ERR_OOM;
        }
        if (src->params)
            memcpy(dst->params, src->params, src->num_params * sizeof(wasm_valtype_t));
        else
            memset(dst->params, 0, src->num_params * sizeof(wasm_valtype_t));
    }

    if (src->num_results > 0) {
        dst->results = (wasm_valtype_t*)WASM_MALLOC(src->num_results * sizeof(wasm_valtype_t));
        if (!dst->results) {
            wasm__free_functype(dst);
            return WASM_ERR_OOM;
        }
        if (src->results)
            memcpy(dst->results, src->results, src->num_results * sizeof(wasm_valtype_t));
        else
            memset(dst->results, 0, src->num_results * sizeof(wasm_valtype_t));
    }

    return WASM_OK;
}

static int wasm__functype_equal(const wasm_functype_t* lhs, const wasm_functype_t* rhs) {
    uint32_t i;

    if (lhs->num_params != rhs->num_params || lhs->num_results != rhs->num_results)
        return 0;
    for (i = 0; i < lhs->num_params; i++) {
        if (lhs->params[i] != rhs->params[i]) return 0;
    }
    for (i = 0; i < lhs->num_results; i++) {
        if (lhs->results[i] != rhs->results[i]) return 0;
    }
    return 1;
}

static wasm_value_t wasm__global_get_value(const wasm_global_t* global) {
    return global->is_import ? *global->import_value : global->value;
}

static void wasm__global_set_value(wasm_global_t* global, wasm_value_t value) {
    value.type = global->type;
    if (global->is_import) {
        global->import_value->type = global->type;
        *global->import_value = value;
    } else {
        global->value = value;
    }
}

typedef struct wasm__init_expr_stack_t {
    wasm_value_t* values;
    uint32_t size;
    uint32_t cap;
} wasm__init_expr_stack_t;

static wasm_error_t wasm__init_expr_stack_push(wasm__init_expr_stack_t* stack, wasm_value_t value) {
    if (stack->size >= stack->cap) {
        uint32_t new_cap = stack->cap ? stack->cap * 2u : 8u;
        wasm_value_t* values = (wasm_value_t*)WASM_REALLOC(stack->values, new_cap * sizeof(wasm_value_t));
        if (!values) return WASM_ERR_OOM;
        stack->values = values;
        stack->cap = new_cap;
    }

    stack->values[stack->size++] = value;
    return WASM_OK;
}

static wasm_error_t wasm__init_expr_stack_pop_typed(wasm__init_expr_stack_t* stack,
                                                    wasm_valtype_t expected_type,
                                                    wasm_value_t* out_value) {
    wasm_value_t value;

    if (stack->size == 0) return WASM_ERR_MALFORMED;
    value = stack->values[--stack->size];
    if (value.type != expected_type) return WASM_ERR_TYPE_MISMATCH;
    *out_value = value;
    return WASM_OK;
}

static int32_t wasm__wrap_i32_add(int32_t lhs, int32_t rhs) {
    return (int32_t)((uint32_t)lhs + (uint32_t)rhs);
}

static int32_t wasm__wrap_i32_mul(int32_t lhs, int32_t rhs) {
    return (int32_t)((uint32_t)lhs * (uint32_t)rhs);
}

static int64_t wasm__wrap_i64_add(int64_t lhs, int64_t rhs) {
    return (int64_t)((uint64_t)lhs + (uint64_t)rhs);
}

static int64_t wasm__wrap_i64_mul(int64_t lhs, int64_t rhs) {
    return (int64_t)((uint64_t)lhs * (uint64_t)rhs);
}

static wasm_error_t wasm__append_funcs(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_funcs;
    wasm_func_t* funcs;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    funcs = (wasm_func_t*)WASM_REALLOC(mod->funcs, (base + count) * sizeof(wasm_func_t));
    if (!funcs) return WASM_ERR_OOM;

    mod->funcs = funcs;
    memset(mod->funcs + base, 0, count * sizeof(wasm_func_t));
    mod->num_funcs = base + count;
    return WASM_OK;
}

static wasm_error_t wasm__append_global_slots(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_globals;
    wasm_global_t* globals;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    globals = (wasm_global_t*)WASM_REALLOC(mod->globals, (base + count) * sizeof(wasm_global_t));
    if (!globals) return WASM_ERR_OOM;

    mod->globals = globals;
    memset(mod->globals + base, 0, count * sizeof(wasm_global_t));
    mod->num_globals = base + count;
    return WASM_OK;
}

static void wasm__free_exception_state(wasm__exception_state_t* ex) {
    WASM_FREE(ex->values);
    memset(ex, 0, sizeof(*ex));
}

static wasm_error_t wasm__set_exception_state(wasm__exception_state_t* ex,
                                              uint32_t tag_index,
                                              const wasm_value_t* values,
                                              uint32_t count) {
    wasm_value_t* copy = NULL;

    wasm__free_exception_state(ex);
    if (count > 0) {
        copy = (wasm_value_t*)WASM_MALLOC(count * sizeof(wasm_value_t));
        if (!copy) return WASM_ERR_OOM;
        memcpy(copy, values, count * sizeof(wasm_value_t));
    }

    ex->tag_index = tag_index;
    ex->values = copy;
    ex->num_values = count;
    ex->is_pending = 1;
    return WASM_OK;
}

static wasm_error_t wasm__append_tags(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_tags;
    wasm_tag_t* tags;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    tags = (wasm_tag_t*)WASM_REALLOC(mod->tags, (base + count) * sizeof(wasm_tag_t));
    if (!tags) return WASM_ERR_OOM;

    mod->tags = tags;
    memset(mod->tags + base, 0, count * sizeof(wasm_tag_t));
    mod->num_tags = base + count;
    return WASM_OK;
}

static void wasm__free_data_segment(wasm_data_segment_t* segment) {
    WASM_FREE(segment->bytes);
    memset(segment, 0, sizeof(*segment));
}

static void wasm__free_table(wasm_table_t* table) {
    WASM_FREE(table->elems);
    memset(table, 0, sizeof(*table));
}

static void wasm__free_memory(wasm_memory_t* memory) {
    WASM_FREE(memory->data);
    memset(memory, 0, sizeof(*memory));
}

static wasm_error_t wasm__append_tables(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_tables;
    wasm_table_t* tables;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    tables = (wasm_table_t*)WASM_REALLOC(mod->tables, (base + count) * sizeof(wasm_table_t));
    if (!tables) return WASM_ERR_OOM;

    mod->tables = tables;
    memset(mod->tables + base, 0, count * sizeof(wasm_table_t));
    mod->num_tables = base + count;
    return WASM_OK;
}

static wasm_error_t wasm__append_memories(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_memories;
    wasm_memory_t* memories;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    memories = (wasm_memory_t*)WASM_REALLOC(mod->memories, (base + count) * sizeof(wasm_memory_t));
    if (!memories) return WASM_ERR_OOM;

    mod->memories = memories;
    memset(mod->memories + base, 0, count * sizeof(wasm_memory_t));
    mod->num_memories = base + count;
    return WASM_OK;
}

static wasm_memory_t* wasm__memory_at(wasm_module_t* mod, uint32_t memory_index) {
    if (!mod || memory_index >= mod->num_memories) return NULL;
    return &mod->memories[memory_index];
}

static wasm_error_t wasm__init_memory_storage(wasm_memory_t* memory) {
    if (memory->pages == 0) return WASM_OK;
    if (memory->data) return WASM_OK;

    memory->data = (uint8_t*)WASM_CALLOC(memory->pages, WASM_PAGE_SIZE);
    if (!memory->data) return WASM_ERR_OOM;
    return WASM_OK;
}

static wasm_error_t wasm__init_table_storage(wasm_table_t* table) {
    uint32_t i;

    if (table->size == 0) return WASM_OK;
    if (table->reftype != WASM_TYPE_FUNCREF && table->reftype != WASM_TYPE_EXTERNREF) return WASM_ERR_MALFORMED;
    if (table->elems) return WASM_OK;

    table->elems = (wasm_value_t*)WASM_MALLOC(table->size * sizeof(wasm_value_t));
    if (!table->elems) return WASM_ERR_OOM;
    for (i = 0; i < table->size; i++) table->elems[i] = wasm_ref_null(table->reftype);
    return WASM_OK;
}

static wasm_error_t wasm__append_data_segments(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_data_segments;
    wasm_data_segment_t* segments;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    segments = (wasm_data_segment_t*)WASM_REALLOC(mod->data_segments, (base + count) * sizeof(wasm_data_segment_t));
    if (!segments) return WASM_ERR_OOM;

    mod->data_segments = segments;
    memset(mod->data_segments + base, 0, count * sizeof(wasm_data_segment_t));
    mod->num_data_segments = base + count;
    return WASM_OK;
}

static void wasm__free_elem_segment(wasm_elem_segment_t* segment) {
    WASM_FREE(segment->elems);
    memset(segment, 0, sizeof(*segment));
}

static wasm_error_t wasm__append_elem_segments(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_elem_segments;
    wasm_elem_segment_t* segments;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    segments = (wasm_elem_segment_t*)WASM_REALLOC(mod->elem_segments, (base + count) * sizeof(wasm_elem_segment_t));
    if (!segments) return WASM_ERR_OOM;

    mod->elem_segments = segments;
    memset(mod->elem_segments + base, 0, count * sizeof(wasm_elem_segment_t));
    mod->num_elem_segments = base + count;
    return WASM_OK;
}

static wasm_error_t wasm__read_elemkind(wasm__reader_t* r) {
    if (wasm__read_u8(r) != 0x00) return WASM_ERR_MALFORMED;
    return WASM_OK;
}

static wasm_error_t wasm__read_reftype(wasm__reader_t* r, wasm_valtype_t* out_type) {
    wasm_valtype_t type = (wasm_valtype_t)wasm__read_u8(r);

    if (!wasm__is_ref_type(type)) return WASM_ERR_MALFORMED;
    *out_type = type;
    return WASM_OK;
}

static wasm_error_t wasm__read_typed_select_immediate(wasm__reader_t* r, wasm_valtype_t* out_type) {
    uint32_t count = wasm__read_leb128_u32(r);
    wasm_valtype_t type;

    if (count != 1) return WASM_ERR_MALFORMED;
    type = (wasm_valtype_t)wasm__read_u8(r);
    if (!wasm__is_value_type(type)) return WASM_ERR_MALFORMED;
    if (out_type) *out_type = type;
    return WASM_OK;
}

static int wasm__range_in_bounds_u64(uint64_t offset, uint64_t size, uint64_t limit) {
    return offset <= limit && size <= (limit - offset);
}

typedef struct wasm__memarg_t {
    uint32_t align_log2;
    uint32_t memory_index;
    uint32_t offset;
} wasm__memarg_t;

static wasm_error_t wasm__read_memarg(wasm__reader_t* r, wasm__memarg_t* out_memarg) {
    uint32_t flags = wasm__read_leb128_u32(r);

    if ((flags & ~0x7Fu) != 0) return WASM_ERR_MALFORMED;

    out_memarg->align_log2 = flags & 0x3Fu;
    out_memarg->memory_index = 0;
    if ((flags & 0x40u) != 0) out_memarg->memory_index = wasm__read_leb128_u32(r);
    out_memarg->offset = wasm__read_leb128_u32(r);
    return WASM_OK;
}

/* ── SIMD helpers ────────────────────────────────────────────────── */

static int8_t wasm__v128_get_i8(const wasm_value_t* value, uint32_t lane) {
    return (int8_t)value->of.v128[lane];
}

static uint8_t wasm__v128_get_u8(const wasm_value_t* value, uint32_t lane) {
    return value->of.v128[lane];
}

static int16_t wasm__v128_get_i16(const wasm_value_t* value, uint32_t lane) {
    return (int16_t)wasm__load_le16(value->of.v128 + lane * 2u);
}

static uint16_t wasm__v128_get_u16(const wasm_value_t* value, uint32_t lane) {
    return wasm__load_le16(value->of.v128 + lane * 2u);
}

static int32_t wasm__v128_get_i32(const wasm_value_t* value, uint32_t lane) {
    return (int32_t)wasm__load_le32(value->of.v128 + lane * 4u);
}

static uint32_t wasm__v128_get_u32(const wasm_value_t* value, uint32_t lane) {
    return wasm__load_le32(value->of.v128 + lane * 4u);
}

static int64_t wasm__v128_get_i64(const wasm_value_t* value, uint32_t lane) {
    return (int64_t)wasm__load_le64(value->of.v128 + lane * 8u);
}

static uint64_t wasm__v128_get_u64(const wasm_value_t* value, uint32_t lane) {
    return wasm__load_le64(value->of.v128 + lane * 8u);
}

static float wasm__v128_get_f32(const wasm_value_t* value, uint32_t lane) {
    uint32_t bits = wasm__v128_get_u32(value, lane);
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static double wasm__v128_get_f64(const wasm_value_t* value, uint32_t lane) {
    uint64_t bits = wasm__v128_get_u64(value, lane);
    double result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static void wasm__v128_set_u8(wasm_value_t* value, uint32_t lane, uint32_t lane_value) {
    value->of.v128[lane] = (uint8_t)lane_value;
}

static void wasm__v128_set_i16(wasm_value_t* value, uint32_t lane, int32_t lane_value) {
    wasm__store_le16(value->of.v128 + lane * 2u, (uint16_t)lane_value);
}

static void wasm__v128_set_u16(wasm_value_t* value, uint32_t lane, uint32_t lane_value) {
    wasm__store_le16(value->of.v128 + lane * 2u, (uint16_t)lane_value);
}

static void wasm__v128_set_i32(wasm_value_t* value, uint32_t lane, int32_t lane_value) {
    wasm__store_le32(value->of.v128 + lane * 4u, (uint32_t)lane_value);
}

static void wasm__v128_set_u32(wasm_value_t* value, uint32_t lane, uint32_t lane_value) {
    wasm__store_le32(value->of.v128 + lane * 4u, lane_value);
}

static void wasm__v128_set_i64(wasm_value_t* value, uint32_t lane, int64_t lane_value) {
    wasm__store_le64(value->of.v128 + lane * 8u, (uint64_t)lane_value);
}

static void wasm__v128_set_u64(wasm_value_t* value, uint32_t lane, uint64_t lane_value) {
    wasm__store_le64(value->of.v128 + lane * 8u, lane_value);
}

static void wasm__v128_set_f32(wasm_value_t* value, uint32_t lane, float lane_value) {
    uint32_t bits;
    memcpy(&bits, &lane_value, sizeof(bits));
    wasm__store_le32(value->of.v128 + lane * 4u, bits);
}

static void wasm__v128_set_f64(wasm_value_t* value, uint32_t lane, double lane_value) {
    uint64_t bits;
    memcpy(&bits, &lane_value, sizeof(bits));
    wasm__store_le64(value->of.v128 + lane * 8u, bits);
}

static int8_t wasm__sat_i8(int32_t value) {
    if (value < -128) return (int8_t)-128;
    if (value > 127) return (int8_t)127;
    return (int8_t)value;
}

static uint8_t wasm__sat_u8(int32_t value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

static int16_t wasm__sat_i16(int32_t value) {
    if (value < -32768) return (int16_t)-32768;
    if (value > 32767) return (int16_t)32767;
    return (int16_t)value;
}

static uint16_t wasm__sat_u16(int32_t value) {
    if (value < 0) return 0;
    if (value > 65535) return 65535u;
    return (uint16_t)value;
}

static uint8_t wasm__popcount_u8(uint8_t value) {
    uint8_t count = 0;

    while (value != 0) {
        count = (uint8_t)(count + (uint8_t)(value & 1u));
        value = (uint8_t)(value >> 1);
    }

    return count;
}

static int wasm__simd_any_true(const wasm_value_t* value) {
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        if (value->of.v128[lane] != 0) return 1;
    }

    return 0;
}

static int32_t wasm__simd_bitmask_i8x16(const wasm_value_t* value) {
    uint32_t lane;
    int32_t mask = 0;

    for (lane = 0; lane < 16; lane++) {
        if ((value->of.v128[lane] & 0x80u) != 0) mask |= (int32_t)(1u << lane);
    }

    return mask;
}

static int32_t wasm__simd_bitmask_i16x8(const wasm_value_t* value) {
    uint32_t lane;
    int32_t mask = 0;

    for (lane = 0; lane < 8; lane++) {
        if (wasm__v128_get_i16(value, lane) < 0) mask |= (int32_t)(1u << lane);
    }

    return mask;
}

static int32_t wasm__simd_bitmask_i32x4(const wasm_value_t* value) {
    uint32_t lane;
    int32_t mask = 0;

    for (lane = 0; lane < 4; lane++) {
        if (wasm__v128_get_i32(value, lane) < 0) mask |= (int32_t)(1u << lane);
    }

    return mask;
}

static int32_t wasm__simd_bitmask_i64x2(const wasm_value_t* value) {
    uint32_t lane;
    int32_t mask = 0;

    for (lane = 0; lane < 2; lane++) {
        if (wasm__v128_get_i64(value, lane) < 0) mask |= (int32_t)(1u << lane);
    }

    return mask;
}

static int wasm__simd_all_true_i8x16(const wasm_value_t* value) {
    uint32_t lane;
    for (lane = 0; lane < 16; lane++) {
        if (wasm__v128_get_i8(value, lane) == 0) return 0;
    }
    return 1;
}

static int wasm__simd_all_true_i16x8(const wasm_value_t* value) {
    uint32_t lane;
    for (lane = 0; lane < 8; lane++) {
        if (wasm__v128_get_i16(value, lane) == 0) return 0;
    }
    return 1;
}

static int wasm__simd_all_true_i32x4(const wasm_value_t* value) {
    uint32_t lane;
    for (lane = 0; lane < 4; lane++) {
        if (wasm__v128_get_i32(value, lane) == 0) return 0;
    }
    return 1;
}

static int wasm__simd_all_true_i64x2(const wasm_value_t* value) {
    uint32_t lane;
    for (lane = 0; lane < 2; lane++) {
        if (wasm__v128_get_i64(value, lane) == 0) return 0;
    }
    return 1;
}

static wasm_value_t wasm__simd_bitwise_not(const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) result.of.v128[lane] = (uint8_t)~value->of.v128[lane];
    return result;
}

static wasm_value_t wasm__simd_bitwise_binary(uint32_t subop,
                                              const wasm_value_t* lhs,
                                              const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        switch (subop) {
            case 0x4E:
                result.of.v128[lane] = (uint8_t)(lhs->of.v128[lane] & rhs->of.v128[lane]);
                break;
            case 0x4F:
                result.of.v128[lane] = (uint8_t)(lhs->of.v128[lane] & (uint8_t)~rhs->of.v128[lane]);
                break;
            case 0x50:
                result.of.v128[lane] = (uint8_t)(lhs->of.v128[lane] | rhs->of.v128[lane]);
                break;
            default:
                result.of.v128[lane] = (uint8_t)(lhs->of.v128[lane] ^ rhs->of.v128[lane]);
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_bitselect(const wasm_value_t* lhs,
                                         const wasm_value_t* rhs,
                                         const wasm_value_t* mask) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        result.of.v128[lane] =
            (uint8_t)((lhs->of.v128[lane] & mask->of.v128[lane]) |
                      (rhs->of.v128[lane] & (uint8_t)~mask->of.v128[lane]));
    }

    return result;
}

static wasm_value_t wasm__simd_shuffle(const wasm_value_t* lhs,
                                       const wasm_value_t* rhs,
                                       const uint8_t lanes[16]) {
    wasm_value_t result = wasm_v128_zero();
    uint8_t combined[32];
    uint32_t lane;

    memcpy(combined, lhs->of.v128, 16);
    memcpy(combined + 16, rhs->of.v128, 16);
    for (lane = 0; lane < 16; lane++) result.of.v128[lane] = combined[lanes[lane]];
    return result;
}

static wasm_value_t wasm__simd_swizzle(const wasm_value_t* lhs, const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        uint8_t index = rhs->of.v128[lane];
        result.of.v128[lane] = (index < 16u) ? lhs->of.v128[index] : 0;
    }

    return result;
}

static uint32_t wasm__simd_memory_align_log2(uint32_t subop) {
    switch (subop) {
        case 0x00:
        case 0x0B:
            return 4;
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
            return 3;
        case 0x07:
        case 0x54:
        case 0x58:
            return 0;
        case 0x08:
        case 0x55:
        case 0x59:
            return 1;
        case 0x09:
        case 0x5C:
        case 0x56:
        case 0x5A:
            return 2;
        case 0x0A:
        case 0x5D:
        case 0x57:
        case 0x5B:
            return 3;
        default:
            return UINT32_MAX;
    }
}

static uint32_t wasm__simd_lane_limit(uint32_t subop) {
    switch (subop) {
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x54:
        case 0x58:
            return 16;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x55:
        case 0x59:
            return 8;
        case 0x1B:
        case 0x1C:
        case 0x1F:
        case 0x20:
        case 0x56:
        case 0x5A:
            return 4;
        default:
            return 2;
    }
}

static void wasm__skip_simd_immediates(wasm__reader_t* r) {
    uint32_t subop = wasm__read_leb128_u32(r);

    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D: {
            wasm__memarg_t memarg;
            if (wasm__read_memarg(r, &memarg) != WASM_OK) {
                r->ptr = r->end;
                return;
            }
            if (subop >= 0x54 && subop <= 0x5B) (void)wasm__read_u8(r);
            break;
        }
        case 0x0C:
            r->ptr += 16;
            if (r->ptr > r->end) {
                r->ptr = r->end;
                r->malformed = 1;
            }
            break;
        case 0x0D: {
            uint32_t lane;
            for (lane = 0; lane < 16 && r->ptr < r->end; lane++) (void)wasm__read_u8(r);
            break;
        }
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
        case 0x20:
        case 0x21:
        case 0x22:
            (void)wasm__read_u8(r);
            break;
        default:
            break;
    }
}

static wasm_value_t wasm__simd_cmp_i8x16(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        int8_t a = wasm__v128_get_i8(lhs, lane);
        int8_t b = wasm__v128_get_i8(rhs, lane);
        uint8_t ua = wasm__v128_get_u8(lhs, lane);
        uint8_t ub = wasm__v128_get_u8(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x23:
                truth = (ua == ub);
                break;
            case 0x24:
                truth = (ua != ub);
                break;
            case 0x25:
                truth = (a < b);
                break;
            case 0x26:
                truth = (ua < ub);
                break;
            case 0x27:
                truth = (a > b);
                break;
            case 0x28:
                truth = (ua > ub);
                break;
            case 0x29:
                truth = (a <= b);
                break;
            case 0x2A:
                truth = (ua <= ub);
                break;
            case 0x2B:
                truth = (a >= b);
                break;
            default:
                truth = (ua >= ub);
                break;
        }

        result.of.v128[lane] = truth ? 0xFFu : 0u;
    }

    return result;
}

static wasm_value_t wasm__simd_cmp_i16x8(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 8; lane++) {
        int16_t a = wasm__v128_get_i16(lhs, lane);
        int16_t b = wasm__v128_get_i16(rhs, lane);
        uint16_t ua = wasm__v128_get_u16(lhs, lane);
        uint16_t ub = wasm__v128_get_u16(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x2D:
                truth = (ua == ub);
                break;
            case 0x2E:
                truth = (ua != ub);
                break;
            case 0x2F:
                truth = (a < b);
                break;
            case 0x30:
                truth = (ua < ub);
                break;
            case 0x31:
                truth = (a > b);
                break;
            case 0x32:
                truth = (ua > ub);
                break;
            case 0x33:
                truth = (a <= b);
                break;
            case 0x34:
                truth = (ua <= ub);
                break;
            case 0x35:
                truth = (a >= b);
                break;
            default:
                truth = (ua >= ub);
                break;
        }

        wasm__v128_set_u16(&result, lane, truth ? 0xFFFFu : 0u);
    }

    return result;
}

static wasm_value_t wasm__simd_cmp_i32x4(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        int32_t a = wasm__v128_get_i32(lhs, lane);
        int32_t b = wasm__v128_get_i32(rhs, lane);
        uint32_t ua = wasm__v128_get_u32(lhs, lane);
        uint32_t ub = wasm__v128_get_u32(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x37:
                truth = (ua == ub);
                break;
            case 0x38:
                truth = (ua != ub);
                break;
            case 0x39:
                truth = (a < b);
                break;
            case 0x3A:
                truth = (ua < ub);
                break;
            case 0x3B:
                truth = (a > b);
                break;
            case 0x3C:
                truth = (ua > ub);
                break;
            case 0x3D:
                truth = (a <= b);
                break;
            case 0x3E:
                truth = (ua <= ub);
                break;
            case 0x3F:
                truth = (a >= b);
                break;
            default:
                truth = (ua >= ub);
                break;
        }

        wasm__v128_set_u32(&result, lane, truth ? UINT32_MAX : 0u);
    }

    return result;
}

static wasm_value_t wasm__simd_cmp_i64x2(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        int64_t a = wasm__v128_get_i64(lhs, lane);
        int64_t b = wasm__v128_get_i64(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0xD6:
                truth = (a == b);
                break;
            case 0xD7:
                truth = (a != b);
                break;
            case 0xD8:
                truth = (a < b);
                break;
            case 0xD9:
                truth = (a > b);
                break;
            case 0xDA:
                truth = (a <= b);
                break;
            default:
                truth = (a >= b);
                break;
        }

        wasm__v128_set_u64(&result, lane, truth ? UINT64_MAX : 0u);
    }

    return result;
}

static wasm_value_t wasm__simd_cmp_f32x4(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        float a = wasm__v128_get_f32(lhs, lane);
        float b = wasm__v128_get_f32(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x41:
                truth = (a == b);
                break;
            case 0x42:
                truth = (a != b);
                break;
            case 0x43:
                truth = (a < b);
                break;
            case 0x44:
                truth = (a > b);
                break;
            case 0x45:
                truth = (a <= b);
                break;
            default:
                truth = (a >= b);
                break;
        }

        wasm__v128_set_u32(&result, lane, truth ? UINT32_MAX : 0u);
    }

    return result;
}

static wasm_value_t wasm__simd_cmp_f64x2(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        double a = wasm__v128_get_f64(lhs, lane);
        double b = wasm__v128_get_f64(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x47:
                truth = (a == b);
                break;
            case 0x48:
                truth = (a != b);
                break;
            case 0x49:
                truth = (a < b);
                break;
            case 0x4A:
                truth = (a > b);
                break;
            case 0x4B:
                truth = (a <= b);
                break;
            default:
                truth = (a >= b);
                break;
        }

        wasm__v128_set_u64(&result, lane, truth ? UINT64_MAX : 0u);
    }

    return result;
}

static wasm_value_t wasm__simd_i8x16_unary(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        int8_t signed_value = wasm__v128_get_i8(value, lane);
        uint8_t unsigned_value = wasm__v128_get_u8(value, lane);

        switch (subop) {
            case 0x60:
                wasm__v128_set_u8(&result, lane,
                                  (uint32_t)(uint8_t)(signed_value < 0 ? -signed_value : signed_value));
                break;
            case 0x61:
                wasm__v128_set_u8(&result, lane, (uint32_t)(uint8_t)(-signed_value));
                break;
            default:
                wasm__v128_set_u8(&result, lane, wasm__popcount_u8(unsigned_value));
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i8x16_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    amount &= 7u;
    for (lane = 0; lane < 16; lane++) {
        int8_t signed_value = wasm__v128_get_i8(value, lane);
        uint8_t unsigned_value = wasm__v128_get_u8(value, lane);

        switch (subop) {
            case 0x6B:
                wasm__v128_set_u8(&result, lane, (uint8_t)(unsigned_value << amount));
                break;
            case 0x6C:
                wasm__v128_set_u8(&result, lane, (uint8_t)(signed_value >> amount));
                break;
            default:
                wasm__v128_set_u8(&result, lane, (uint8_t)(unsigned_value >> amount));
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i8x16_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    if (subop == 0x65 || subop == 0x66) {
        for (lane = 0; lane < 8; lane++) {
            int16_t lo = wasm__v128_get_i16(lhs, lane);
            int16_t hi = wasm__v128_get_i16(rhs, lane);

            if (subop == 0x65) {
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_i8((int32_t)lo));
                wasm__v128_set_u8(&result, lane + 8u, (uint8_t)wasm__sat_i8((int32_t)hi));
            } else {
                wasm__v128_set_u8(&result, lane, wasm__sat_u8((int32_t)lo));
                wasm__v128_set_u8(&result, lane + 8u, wasm__sat_u8((int32_t)hi));
            }
        }
        return result;
    }

    for (lane = 0; lane < 16; lane++) {
        int8_t a = wasm__v128_get_i8(lhs, lane);
        int8_t b = wasm__v128_get_i8(rhs, lane);
        uint8_t ua = wasm__v128_get_u8(lhs, lane);
        uint8_t ub = wasm__v128_get_u8(rhs, lane);

        switch (subop) {
            case 0x6E:
                wasm__v128_set_u8(&result, lane, (uint8_t)(ua + ub));
                break;
            case 0x6F:
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_i8((int32_t)a + (int32_t)b));
                break;
            case 0x70:
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_u8((int32_t)ua + (int32_t)ub));
                break;
            case 0x71:
                wasm__v128_set_u8(&result, lane, (uint8_t)(ua - ub));
                break;
            case 0x72:
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_i8((int32_t)a - (int32_t)b));
                break;
            case 0x73:
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_u8((int32_t)ua - (int32_t)ub));
                break;
            case 0x76:
                wasm__v128_set_u8(&result, lane, (uint8_t)((a < b) ? a : b));
                break;
            case 0x77:
                wasm__v128_set_u8(&result, lane, (ua < ub) ? ua : ub);
                break;
            case 0x78:
                wasm__v128_set_u8(&result, lane, (uint8_t)((a > b) ? a : b));
                break;
            case 0x79:
                wasm__v128_set_u8(&result, lane, (ua > ub) ? ua : ub);
                break;
            default:
                wasm__v128_set_u8(&result, lane, (uint8_t)(((uint32_t)ua + (uint32_t)ub + 1u) >> 1));
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i16x8_unary(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A: {
            uint32_t base = (subop == 0x88 || subop == 0x8A) ? 8u : 0u;
            for (lane = 0; lane < 8; lane++) {
                if (subop == 0x87 || subop == 0x88)
                    wasm__v128_set_i16(&result, lane, wasm__v128_get_i8(value, base + lane));
                else
                    wasm__v128_set_u16(&result, lane, wasm__v128_get_u8(value, base + lane));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 8; lane++) {
        int16_t signed_value = wasm__v128_get_i16(value, lane);

        switch (subop) {
            case 0x80:
                wasm__v128_set_u16(&result, lane,
                                   (uint16_t)(signed_value < 0 ? -signed_value : signed_value));
                break;
            default:
                wasm__v128_set_u16(&result, lane, (uint16_t)(-signed_value));
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i16x8_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    amount &= 15u;
    for (lane = 0; lane < 8; lane++) {
        int16_t signed_value = wasm__v128_get_i16(value, lane);
        uint16_t unsigned_value = wasm__v128_get_u16(value, lane);

        switch (subop) {
            case 0x8B:
                wasm__v128_set_u16(&result, lane, (uint16_t)(unsigned_value << amount));
                break;
            case 0x8C:
                wasm__v128_set_u16(&result, lane, (uint16_t)(signed_value >> amount));
                break;
            default:
                wasm__v128_set_u16(&result, lane, (uint16_t)(unsigned_value >> amount));
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i16x8_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0x82:
            for (lane = 0; lane < 8; lane++) {
                int32_t product = (int32_t)wasm__v128_get_i16(lhs, lane) * (int32_t)wasm__v128_get_i16(rhs, lane);
                int32_t rounded = (product + 0x4000) >> 15;
                wasm__v128_set_i16(&result, lane, wasm__sat_i16(rounded));
            }
            return result;
        case 0x85:
        case 0x86:
            for (lane = 0; lane < 4; lane++) {
                int32_t lo = wasm__v128_get_i32(lhs, lane);
                int32_t hi = wasm__v128_get_i32(rhs, lane);
                if (subop == 0x85) {
                    wasm__v128_set_i16(&result, lane, wasm__sat_i16(lo));
                    wasm__v128_set_i16(&result, lane + 4u, wasm__sat_i16(hi));
                } else {
                    wasm__v128_set_u16(&result, lane, wasm__sat_u16(lo));
                    wasm__v128_set_u16(&result, lane + 4u, wasm__sat_u16(hi));
                }
            }
            return result;
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F: {
            uint32_t base = (subop == 0x9D || subop == 0x9F) ? 8u : 0u;
            for (lane = 0; lane < 8; lane++) {
                uint32_t index = base + lane;
                if (subop == 0x9C || subop == 0x9D) {
                    int16_t product = (int16_t)((int32_t)wasm__v128_get_i8(lhs, index) *
                                                (int32_t)wasm__v128_get_i8(rhs, index));
                    wasm__v128_set_i16(&result, lane, product);
                } else {
                    uint16_t product = (uint16_t)((uint32_t)wasm__v128_get_u8(lhs, index) *
                                                  (uint32_t)wasm__v128_get_u8(rhs, index));
                    wasm__v128_set_u16(&result, lane, product);
                }
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 8; lane++) {
        int16_t a = wasm__v128_get_i16(lhs, lane);
        int16_t b = wasm__v128_get_i16(rhs, lane);
        uint16_t ua = wasm__v128_get_u16(lhs, lane);
        uint16_t ub = wasm__v128_get_u16(rhs, lane);

        switch (subop) {
            case 0x8E:
                wasm__v128_set_u16(&result, lane, (uint16_t)(ua + ub));
                break;
            case 0x8F:
                wasm__v128_set_i16(&result, lane, wasm__sat_i16((int32_t)a + (int32_t)b));
                break;
            case 0x90:
                wasm__v128_set_u16(&result, lane, wasm__sat_u16((int32_t)ua + (int32_t)ub));
                break;
            case 0x91:
                wasm__v128_set_u16(&result, lane, (uint16_t)(ua - ub));
                break;
            case 0x92:
                wasm__v128_set_i16(&result, lane, wasm__sat_i16((int32_t)a - (int32_t)b));
                break;
            case 0x93:
                wasm__v128_set_u16(&result, lane, wasm__sat_u16((int32_t)ua - (int32_t)ub));
                break;
            case 0x95:
                wasm__v128_set_u16(&result, lane, (uint16_t)(ua * ub));
                break;
            case 0x96:
                wasm__v128_set_i16(&result, lane, (a < b) ? a : b);
                break;
            case 0x97:
                wasm__v128_set_u16(&result, lane, (ua < ub) ? ua : ub);
                break;
            case 0x98:
                wasm__v128_set_i16(&result, lane, (a > b) ? a : b);
                break;
            case 0x99:
                wasm__v128_set_u16(&result, lane, (ua > ub) ? ua : ub);
                break;
            default:
                wasm__v128_set_u16(&result, lane, (uint16_t)(((uint32_t)ua + (uint32_t)ub + 1u) >> 1));
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i32x4_unary(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA: {
            uint32_t base = (subop == 0xA8 || subop == 0xAA) ? 4u : 0u;
            for (lane = 0; lane < 4; lane++) {
                if (subop == 0xA7 || subop == 0xA8)
                    wasm__v128_set_i32(&result, lane, wasm__v128_get_i16(value, base + lane));
                else
                    wasm__v128_set_u32(&result, lane, wasm__v128_get_u16(value, base + lane));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 4; lane++) {
        int32_t signed_value = wasm__v128_get_i32(value, lane);

        switch (subop) {
            case 0xA0:
                wasm__v128_set_u32(&result, lane,
                                   (uint32_t)(signed_value < 0 ? (uint32_t)(-(uint32_t)signed_value)
                                                               : (uint32_t)signed_value));
                break;
            default:
                wasm__v128_set_u32(&result, lane, (uint32_t)(-(uint32_t)signed_value));
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i32x4_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    amount &= 31u;
    for (lane = 0; lane < 4; lane++) {
        int32_t signed_value = wasm__v128_get_i32(value, lane);
        uint32_t unsigned_value = wasm__v128_get_u32(value, lane);

        switch (subop) {
            case 0xAB:
                wasm__v128_set_u32(&result, lane, unsigned_value << amount);
                break;
            case 0xAC:
                wasm__v128_set_u32(&result, lane, (uint32_t)(signed_value >> amount));
                break;
            default:
                wasm__v128_set_u32(&result, lane, unsigned_value >> amount);
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i32x4_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0xBA:
            for (lane = 0; lane < 4; lane++) {
                uint32_t base = lane * 2u;
                int32_t dot = (int32_t)wasm__v128_get_i16(lhs, base) * (int32_t)wasm__v128_get_i16(rhs, base) +
                              (int32_t)wasm__v128_get_i16(lhs, base + 1u) * (int32_t)wasm__v128_get_i16(rhs, base + 1u);
                wasm__v128_set_i32(&result, lane, dot);
            }
            return result;
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF: {
            uint32_t base = (subop == 0xBD || subop == 0xBF) ? 4u : 0u;
            for (lane = 0; lane < 4; lane++) {
                uint32_t index = base + lane;
                if (subop == 0xBC || subop == 0xBD)
                    wasm__v128_set_i32(&result, lane,
                                       (int32_t)wasm__v128_get_i16(lhs, index) *
                                           (int32_t)wasm__v128_get_i16(rhs, index));
                else
                    wasm__v128_set_u32(&result, lane,
                                       (uint32_t)wasm__v128_get_u16(lhs, index) *
                                           (uint32_t)wasm__v128_get_u16(rhs, index));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 4; lane++) {
        int32_t a = wasm__v128_get_i32(lhs, lane);
        int32_t b = wasm__v128_get_i32(rhs, lane);
        uint32_t ua = wasm__v128_get_u32(lhs, lane);
        uint32_t ub = wasm__v128_get_u32(rhs, lane);

        switch (subop) {
            case 0xAE:
                wasm__v128_set_u32(&result, lane, ua + ub);
                break;
            case 0xB1:
                wasm__v128_set_u32(&result, lane, ua - ub);
                break;
            case 0xB5:
                wasm__v128_set_u32(&result, lane, ua * ub);
                break;
            case 0xB6:
                wasm__v128_set_i32(&result, lane, (a < b) ? a : b);
                break;
            case 0xB7:
                wasm__v128_set_u32(&result, lane, (ua < ub) ? ua : ub);
                break;
            case 0xB8:
                wasm__v128_set_i32(&result, lane, (a > b) ? a : b);
                break;
            default:
                wasm__v128_set_u32(&result, lane, (ua > ub) ? ua : ub);
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i64x2_unary(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA: {
            uint32_t base = (subop == 0xC8 || subop == 0xCA) ? 2u : 0u;
            for (lane = 0; lane < 2; lane++) {
                if (subop == 0xC7 || subop == 0xC8)
                    wasm__v128_set_i64(&result, lane, wasm__v128_get_i32(value, base + lane));
                else
                    wasm__v128_set_u64(&result, lane, wasm__v128_get_u32(value, base + lane));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 2; lane++) {
        int64_t signed_value = wasm__v128_get_i64(value, lane);
        uint64_t unsigned_value = wasm__v128_get_u64(value, lane);

        switch (subop) {
            case 0xC0:
                if (signed_value < 0)
                    wasm__v128_set_u64(&result, lane, (uint64_t)(-unsigned_value));
                else
                    wasm__v128_set_u64(&result, lane, unsigned_value);
                break;
            default:
                wasm__v128_set_u64(&result, lane, (uint64_t)(-unsigned_value));
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i64x2_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    amount &= 63u;
    for (lane = 0; lane < 2; lane++) {
        int64_t signed_value = wasm__v128_get_i64(value, lane);
        uint64_t unsigned_value = wasm__v128_get_u64(value, lane);

        switch (subop) {
            case 0xCB:
                wasm__v128_set_u64(&result, lane, unsigned_value << amount);
                break;
            case 0xCC:
                wasm__v128_set_u64(&result, lane, (uint64_t)(signed_value >> amount));
                break;
            default:
                wasm__v128_set_u64(&result, lane, unsigned_value >> amount);
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_i64x2_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF: {
            uint32_t base = (subop == 0xDD || subop == 0xDF) ? 2u : 0u;
            for (lane = 0; lane < 2; lane++) {
                uint32_t index = base + lane;
                if (subop == 0xDC || subop == 0xDD)
                    wasm__v128_set_i64(&result, lane,
                                       (int64_t)wasm__v128_get_i32(lhs, index) *
                                           (int64_t)wasm__v128_get_i32(rhs, index));
                else
                    wasm__v128_set_u64(&result, lane,
                                       (uint64_t)wasm__v128_get_u32(lhs, index) *
                                           (uint64_t)wasm__v128_get_u32(rhs, index));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 2; lane++) {
        uint64_t ua = wasm__v128_get_u64(lhs, lane);
        uint64_t ub = wasm__v128_get_u64(rhs, lane);

        switch (subop) {
            case 0xCE:
                wasm__v128_set_u64(&result, lane, ua + ub);
                break;
            case 0xD1:
                wasm__v128_set_u64(&result, lane, ua - ub);
                break;
            default:
                wasm__v128_set_u64(&result, lane, ua * ub);
                break;
        }
    }

    return result;
}

static wasm_value_t wasm__simd_f32x4_unary(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        float current = wasm__v128_get_f32(value, lane);
        float out = current;

        switch (subop) {
            case 0x67:
                out = ceilf(current);
                break;
            case 0x68:
                out = floorf(current);
                break;
            case 0x69:
                out = truncf(current);
                break;
            case 0x6A:
                out = wasm__nearest_f32(current);
                break;
            case 0xE0:
                out = fabsf(current);
                break;
            case 0xE1:
                out = -current;
                break;
            default:
                out = sqrtf(current);
                break;
        }

        wasm__v128_set_f32(&result, lane, out);
    }

    return result;
}

static wasm_value_t wasm__simd_f32x4_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        float a = wasm__v128_get_f32(lhs, lane);
        float b = wasm__v128_get_f32(rhs, lane);
        float out;

        switch (subop) {
            case 0xE4:
                out = a + b;
                break;
            case 0xE5:
                out = a - b;
                break;
            case 0xE6:
                out = a * b;
                break;
            case 0xE7:
                out = a / b;
                break;
            case 0xE8:
                out = wasm__f32_min(a, b);
                break;
            case 0xE9:
                out = wasm__f32_max(a, b);
                break;
            case 0xEA:
                out = (b < a) ? b : a;
                break;
            default:
                out = (a < b) ? b : a;
                break;
        }

        wasm__v128_set_f32(&result, lane, out);
    }

    return result;
}

static wasm_value_t wasm__simd_f64x2_unary(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        double current = wasm__v128_get_f64(value, lane);
        double out = current;

        switch (subop) {
            case 0x74:
                out = ceil(current);
                break;
            case 0x75:
                out = floor(current);
                break;
            case 0x7A:
                out = trunc(current);
                break;
            case 0x94:
                out = wasm__nearest_f64(current);
                break;
            case 0xEC:
                out = fabs(current);
                break;
            case 0xED:
                out = -current;
                break;
            default:
                out = sqrt(current);
                break;
        }

        wasm__v128_set_f64(&result, lane, out);
    }

    return result;
}

static wasm_value_t wasm__simd_f64x2_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        double a = wasm__v128_get_f64(lhs, lane);
        double b = wasm__v128_get_f64(rhs, lane);
        double out;

        switch (subop) {
            case 0xF0:
                out = a + b;
                break;
            case 0xF1:
                out = a - b;
                break;
            case 0xF2:
                out = a * b;
                break;
            case 0xF3:
                out = a / b;
                break;
            case 0xF4:
                out = wasm__f64_min(a, b);
                break;
            case 0xF5:
                out = wasm__f64_max(a, b);
                break;
            case 0xF6:
                out = (b < a) ? b : a;
                break;
            default:
                out = (a < b) ? b : a;
                break;
        }

        wasm__v128_set_f64(&result, lane, out);
    }

    return result;
}

static wasm_value_t wasm__simd_demote_f64x2_zero(const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();

    wasm__v128_set_f32(&result, 0, (float)wasm__v128_get_f64(value, 0));
    wasm__v128_set_f32(&result, 1, (float)wasm__v128_get_f64(value, 1));
    wasm__v128_set_u32(&result, 2, 0u);
    wasm__v128_set_u32(&result, 3, 0u);
    return result;
}

static wasm_value_t wasm__simd_promote_low_f32x4(const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();

    wasm__v128_set_f64(&result, 0, (double)wasm__v128_get_f32(value, 0));
    wasm__v128_set_f64(&result, 1, (double)wasm__v128_get_f32(value, 1));
    return result;
}

static wasm_value_t wasm__simd_extadd_pairwise(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0x7C:
        case 0x7D:
            for (lane = 0; lane < 8; lane++) {
                uint32_t base = lane * 2u;
                if (subop == 0x7C)
                    wasm__v128_set_i16(&result, lane,
                                       (int32_t)wasm__v128_get_i8(value, base) +
                                           (int32_t)wasm__v128_get_i8(value, base + 1u));
                else
                    wasm__v128_set_u16(&result, lane,
                                       (uint32_t)wasm__v128_get_u8(value, base) +
                                           (uint32_t)wasm__v128_get_u8(value, base + 1u));
            }
            break;
        default:
            for (lane = 0; lane < 4; lane++) {
                uint32_t base = lane * 2u;
                if (subop == 0x7E)
                    wasm__v128_set_i32(&result, lane,
                                       (int32_t)wasm__v128_get_i16(value, base) +
                                           (int32_t)wasm__v128_get_i16(value, base + 1u));
                else
                    wasm__v128_set_u32(&result, lane,
                                       (uint32_t)wasm__v128_get_u16(value, base) +
                                           (uint32_t)wasm__v128_get_u16(value, base + 1u));
            }
            break;
    }

    return result;
}

static wasm_value_t wasm__simd_trunc_sat_f32x4(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        wasm_value_t converted =
            (subop == 0xF8) ? wasm__trunc_sat_i32_s((double)wasm__v128_get_f32(value, lane))
                            : wasm__trunc_sat_i32_u((double)wasm__v128_get_f32(value, lane));
        wasm__v128_set_i32(&result, lane, converted.of.i32);
    }

    return result;
}

static wasm_value_t wasm__simd_convert_i32x4(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        if (subop == 0xFA)
            wasm__v128_set_f32(&result, lane, (float)wasm__v128_get_i32(value, lane));
        else
            wasm__v128_set_f32(&result, lane, (float)(double)wasm__v128_get_u32(value, lane));
    }

    return result;
}

static wasm_value_t wasm__simd_trunc_sat_f64x2_zero(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        wasm_value_t converted =
            (subop == 0xFC) ? wasm__trunc_sat_i32_s(wasm__v128_get_f64(value, lane))
                            : wasm__trunc_sat_i32_u(wasm__v128_get_f64(value, lane));
        wasm__v128_set_i32(&result, lane, converted.of.i32);
    }
    wasm__v128_set_u32(&result, 2, 0u);
    wasm__v128_set_u32(&result, 3, 0u);
    return result;
}

static wasm_value_t wasm__simd_convert_low_i32x4_to_f64x2(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();

    if (subop == 0xFE) {
        wasm__v128_set_f64(&result, 0, (double)wasm__v128_get_i32(value, 0));
        wasm__v128_set_f64(&result, 1, (double)wasm__v128_get_i32(value, 1));
    } else {
        wasm__v128_set_f64(&result, 0, (double)wasm__v128_get_u32(value, 0));
        wasm__v128_set_f64(&result, 1, (double)wasm__v128_get_u32(value, 1));
    }

    return result;
}

static wasm_error_t wasm__apply_active_data_segments(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_data_segments; i++) {
        wasm_data_segment_t* segment = &mod->data_segments[i];
        wasm_memory_t* memory;
        uint64_t mem_size;

        if (segment->is_passive) continue;
        memory = wasm__memory_at(mod, segment->memory_index);
        if (!memory) return WASM_ERR_MALFORMED;
        mem_size = (uint64_t)memory->pages * WASM_PAGE_SIZE;
        if (!wasm__range_in_bounds_u64(segment->offset, segment->size, mem_size)) return WASM_ERR_OUT_OF_BOUNDS;
        if (segment->size > 0)
            memcpy(memory->data + segment->offset, segment->bytes, segment->size);
    }

    return WASM_OK;
}

static wasm_error_t wasm__apply_active_elem_segments(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_elem_segments; i++) {
        wasm_elem_segment_t* segment = &mod->elem_segments[i];
        wasm_table_t* table;
        uint32_t j;

        if (segment->is_passive || segment->is_declarative) continue;
        if (segment->table_index >= mod->num_tables) return WASM_ERR_MALFORMED;
        table = &mod->tables[segment->table_index];
        if (table->reftype != segment->elem_type) return WASM_ERR_TYPE_MISMATCH;
        if (!wasm__range_in_bounds_u64(segment->offset, segment->num_elems, table->size))
            return WASM_ERR_OUT_OF_BOUNDS;
        if (segment->num_elems > 0) {
            if (!table->elems) return WASM_ERR_OUT_OF_BOUNDS;
            for (j = 0; j < segment->num_elems; j++) table->elems[segment->offset + j] = segment->elems[j];
        }
    }

    return WASM_OK;
}

static wasm_error_t wasm__eval_init_expr(wasm_module_t* mod, wasm__reader_t* r,
                                         uint32_t max_global_index,
                                         wasm_value_t* out_value) {
    wasm__init_expr_stack_t stack;
    wasm_error_t err = WASM_OK;

    memset(&stack, 0, sizeof(stack));

    for (;;) {
        uint8_t op;

        if (!wasm__has(r, 1)) {
            err = WASM_ERR_MALFORMED;
            break;
        }

        op = wasm__read_u8(r);
        if (op == 0x0B) break;

        switch (op) {
            case 0x41:
                err = wasm__init_expr_stack_push(&stack, wasm_i32(wasm__read_leb128_i32(r)));
                break;
            case 0x42:
                err = wasm__init_expr_stack_push(&stack, wasm_i64(wasm__read_leb128_i64(r)));
                break;
            case 0x43: {
                wasm_value_t value = wasm_f32(wasm__read_f32(r));
                err = wasm__init_expr_stack_push(&stack, value);
                break;
            }
            case 0x44: {
                wasm_value_t value = wasm_f64(wasm__read_f64(r));
                err = wasm__init_expr_stack_push(&stack, value);
                break;
            }
            case 0x23: {
                uint32_t global_index = wasm__read_leb128_u32(r);
                const wasm_global_t* global;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                if (global_index >= max_global_index) {
                    err = WASM_ERR_MALFORMED;
                    break;
                }

                global = &mod->globals[global_index];
                if (!global->is_import && global->is_mutable) {
                    err = WASM_ERR_TYPE_MISMATCH;
                    break;
                }

                err = wasm__init_expr_stack_push(&stack, wasm__global_get_value(global));
                break;
            }
            case 0x6A: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i32(wasm__wrap_i32_add(lhs.of.i32, rhs.of.i32)));
                break;
            }
            case 0x6C: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i32(wasm__wrap_i32_mul(lhs.of.i32, rhs.of.i32)));
                break;
            }
            case 0x7C: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i64(wasm__wrap_i64_add(lhs.of.i64, rhs.of.i64)));
                break;
            }
            case 0x7E: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i64(wasm__wrap_i64_mul(lhs.of.i64, rhs.of.i64)));
                break;
            }
            case 0xD0: {
                wasm_valtype_t type;
                wasm_value_t value;

                err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) break;
                if (wasm__read_reftype(r, &type) != WASM_OK) {
                    err = WASM_ERR_MALFORMED;
                    break;
                }

                value = wasm_ref_null(type);
                err = wasm__init_expr_stack_push(&stack, value);
                break;
            }
            case 0xD2: {
                uint32_t func_index = wasm__read_leb128_u32(r);

                err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) break;
                if (func_index >= mod->num_funcs) {
                    err = WASM_ERR_MALFORMED;
                    break;
                }

                err = wasm__init_expr_stack_push(&stack, wasm_funcref(func_index));
                break;
            }
            default:
                err = WASM_ERR_MALFORMED;
                break;
        }

        if (err != WASM_OK) break;
    }

    if (err == WASM_OK) {
        if (stack.size != 1)
            err = WASM_ERR_MALFORMED;
        else
            *out_value = stack.values[0];
    }

    WASM_FREE(stack.values);
    return err;
}

static wasm_global_import_t* wasm__find_global_import(wasm_runtime_t* rt,
                                                      const char* module,
                                                      const char* name) {
    uint32_t i;

    for (i = 0; i < rt->num_global_imports; i++) {
        if (strcmp(rt->global_imports[i].module, module) == 0 &&
            strcmp(rt->global_imports[i].name, name) == 0)
            return &rt->global_imports[i];
    }

    return NULL;
}

/* ── Init / Destroy ───────────────────────────────────────────────── */

wasm_error_t wasm_init(wasm_runtime_t* rt) {
    memset(rt, 0, sizeof(*rt));
    rt->enabled_features = WASM__IMPLEMENTED_FEATURES;
    return WASM_OK;
}

void wasm_destroy(wasm_runtime_t* rt) {
    uint32_t i;

    for (i = 0; i < rt->num_imports; i++) wasm__free_functype(&rt->imports[i].type);
    WASM_FREE(rt->imports);
    WASM_FREE(rt->global_imports);
    memset(rt, 0, sizeof(*rt));
}

wasm_error_t wasm_register_import(wasm_runtime_t* rt, const wasm_import_t* imp) {
    wasm_import_t copy;
    wasm_error_t err;

    if (rt->num_imports >= rt->cap_imports) {
        uint32_t new_cap = rt->cap_imports ? rt->cap_imports * 2 : 16;
        wasm_import_t* a = (wasm_import_t*)WASM_REALLOC(rt->imports, new_cap * sizeof(wasm_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->imports = a;
        rt->cap_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = imp->module;
    copy.name = imp->name;
    copy.func = imp->func;
    copy.userdata = imp->userdata;
    err = wasm__copy_functype(&copy.type, &imp->type);
    if (err != WASM_OK) return err;

    rt->imports[rt->num_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_global_import(wasm_runtime_t* rt, const wasm_global_import_t* imp) {
    wasm_global_import_t copy;

    if (!imp || !imp->module || !imp->name || !imp->value) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid global import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_global_imports >= rt->cap_global_imports) {
        uint32_t new_cap = rt->cap_global_imports ? rt->cap_global_imports * 2 : 16;
        wasm_global_import_t* a =
            (wasm_global_import_t*)WASM_REALLOC(rt->global_imports, new_cap * sizeof(wasm_global_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->global_imports = a;
        rt->cap_global_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = imp->module;
    copy.name = imp->name;
    copy.type = imp->type;
    copy.is_mutable = imp->is_mutable;
    copy.value = imp->value;
    rt->global_imports[rt->num_global_imports++] = copy;
    return WASM_OK;
}

void wasm_enable_feature(wasm_runtime_t* rt, uint32_t flag) {
    rt->enabled_features |= flag;
}

void wasm_disable_feature(wasm_runtime_t* rt, uint32_t flag) {
    rt->enabled_features &= ~flag;
}

void wasm_enable_all_features(wasm_runtime_t* rt) {
    rt->enabled_features = WASM__IMPLEMENTED_FEATURES;
}

/* ── Section decoders ─────────────────────────────────────────────── */

static wasm_error_t wasm__decode_type_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);
    uint32_t i, p;
    if (count > 0) {
        mod->types = (wasm_functype_t*)WASM_CALLOC(count, sizeof(wasm_functype_t));
        if (!mod->types) return WASM_ERR_OOM;
    }
    mod->num_types = count;
    for (i = 0; i < count; i++) {
        wasm_functype_t* ft = &mod->types[i];
        if (wasm__read_u8(r) != 0x60) return WASM_ERR_MALFORMED;
        ft->num_params = wasm__read_leb128_u32(r);
        if (ft->num_params > 0) {
            ft->params = (wasm_valtype_t*)WASM_MALLOC(ft->num_params * sizeof(wasm_valtype_t));
            if (!ft->params) return WASM_ERR_OOM;
        }
        for (p = 0; p < ft->num_params; p++) {
            ft->params[p] = (wasm_valtype_t)wasm__read_u8(r);
            if (!wasm__is_value_type(ft->params[p])) return WASM_ERR_MALFORMED;
            {
                wasm_error_t err = wasm__require_valtype_feature(mod, ft->params[p]);
                if (err != WASM_OK) return err;
            }
        }
        ft->num_results = wasm__read_leb128_u32(r);
        if (ft->num_results > 1) {
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_MULTI_VALUE);
            if (err != WASM_OK) return err;
        }
        if (ft->num_results > 0) {
            ft->results = (wasm_valtype_t*)WASM_MALLOC(ft->num_results * sizeof(wasm_valtype_t));
            if (!ft->results) return WASM_ERR_OOM;
        }
        for (p = 0; p < ft->num_results; p++) {
            ft->results[p] = (wasm_valtype_t)wasm__read_u8(r);
            if (!wasm__is_value_type(ft->results[p])) return WASM_ERR_MALFORMED;
            {
                wasm_error_t err = wasm__require_valtype_feature(mod, ft->results[p]);
                if (err != WASM_OK) return err;
            }
        }
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_import_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);
    uint32_t i, j;

    for (i = 0; i < count; i++) {
        char mn[128], fn[128];
        uint8_t kind;
        wasm__read_name(r, mn, sizeof(mn));
        wasm__read_name(r, fn, sizeof(fn));
        kind = wasm__read_u8(r);
        if (kind == 0x00) {
            uint32_t ti = wasm__read_leb128_u32(r);
            uint32_t fi;

            if (wasm__append_funcs(mod, 1, &fi) != WASM_OK) return WASM_ERR_OOM;
            mod->funcs[fi].type_idx = ti;
            mod->funcs[fi].is_import = 1;
            mod->num_func_imports++;
            for (j = 0; j < mod->rt->num_imports; j++) {
                if (strcmp(mod->rt->imports[j].module, mn) == 0 && strcmp(mod->rt->imports[j].name, fn) == 0) {
                    mod->funcs[fi].host_func = mod->rt->imports[j].func;
                    mod->funcs[fi].host_userdata = mod->rt->imports[j].userdata;
                    break;
                }
            }
            if (!mod->funcs[fi].host_func) {
                WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", mn, fn);
                return WASM_ERR_UNKNOWN_IMPORT;
            }
        } else if (kind == 0x01) {
            uint32_t table_index;
            wasm_table_t* table;
            wasm_valtype_t reftype = (wasm_valtype_t)wasm__read_u8(r);
            uint8_t lf;
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
            if (err != WASM_OK) return err;
            if (!wasm__is_ref_type(reftype)) return WASM_ERR_MALFORMED;
            if (wasm__append_tables(mod, 1, &table_index) != WASM_OK) return WASM_ERR_OOM;
            table = &mod->tables[table_index];
            table->reftype = reftype;
            lf = wasm__read_u8(r);
            table->size = wasm__read_leb128_u32(r);
            table->max_size = (lf & 1) ? wasm__read_leb128_u32(r) : UINT32_MAX;
        } else if (kind == 0x02) {
            uint32_t memory_index;
            wasm_memory_t* memory;
            uint8_t lf = wasm__read_u8(r);

            if (wasm__append_memories(mod, 1, &memory_index) != WASM_OK) return WASM_ERR_OOM;
            if (mod->num_memories > 1) {
                wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_MULTI_MEMORY);
                if (err != WASM_OK) return err;
            }
            memory = &mod->memories[memory_index];
            memory->pages = wasm__read_leb128_u32(r);
            memory->max_pages = (lf & 1) ? wasm__read_leb128_u32(r) : 65536;
        } else if (kind == 0x03) {
            uint32_t gi;
            wasm_global_import_t* gimp;
            wasm_valtype_t type = (wasm_valtype_t)wasm__read_u8(r);
            uint8_t is_mutable = wasm__read_u8(r);

            if (!wasm__is_value_type(type)) return WASM_ERR_MALFORMED;
            {
                wasm_error_t err = wasm__require_valtype_feature(mod, type);
                if (err != WASM_OK) return err;
            }
            if (is_mutable) {
                wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_MUTABLE_GLOBALS);
                if (err != WASM_OK) return err;
            }

            gimp = wasm__find_global_import(mod->rt, mn, fn);
            if (!gimp) {
                WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", mn, fn);
                return WASM_ERR_UNKNOWN_IMPORT;
            }
            if (gimp->type != type || gimp->is_mutable != (int)is_mutable) {
                WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "global import type mismatch: %.64s.%.64s", mn, fn);
                return WASM_ERR_TYPE_MISMATCH;
            }
            if (!gimp->value) return WASM_ERR_MALFORMED;
            if (wasm__append_global_slots(mod, 1, &gi) != WASM_OK) return WASM_ERR_OOM;

            mod->globals[gi].type = type;
            mod->globals[gi].is_mutable = is_mutable;
            mod->globals[gi].is_import = 1;
            mod->globals[gi].import_value = gimp->value;
            mod->globals[gi].import_value->type = type;
            mod->num_global_imports++;
        } else if (kind == 0x04) {
            uint32_t tag_index;
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_EXCEPTIONS);

            if (err != WASM_OK) return err;
            if (wasm__read_u8(r) != 0x00) return WASM_ERR_MALFORMED;
            if (wasm__append_tags(mod, 1, &tag_index) != WASM_OK) return WASM_ERR_OOM;
            mod->tags[tag_index].type_idx = wasm__read_leb128_u32(r);
        } else {
            return WASM_ERR_MALFORMED;
        }
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_function_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);

    uint32_t base, i;

    if (wasm__append_funcs(mod, count, &base) != WASM_OK) return WASM_ERR_OOM;
    for (i = 0; i < count; i++) {
        uint32_t fi = base + i;
        mod->funcs[fi].type_idx = wasm__read_leb128_u32(r);
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_table_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i, base;

    if (count > 0) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
        if (err != WASM_OK) return err;
    }

    if (wasm__append_tables(mod, count, &base) != WASM_OK) return WASM_ERR_OOM;
    for (i = 0; i < count; i++) {
        wasm_table_t* table = &mod->tables[base + i];
        uint8_t lf;
        table->reftype = (wasm_valtype_t)wasm__read_u8(r);
        if (!wasm__is_ref_type(table->reftype)) return WASM_ERR_MALFORMED;
        lf = wasm__read_u8(r);
        table->size = wasm__read_leb128_u32(r);
        table->max_size = (lf & 1) ? wasm__read_leb128_u32(r) : UINT32_MAX;
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_memory_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i, base;

    if (wasm__append_memories(mod, count, &base) != WASM_OK) return WASM_ERR_OOM;
    if (mod->num_memories > 1) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_MULTI_MEMORY);
        if (err != WASM_OK) return err;
    }

    for (i = 0; i < count; i++) {
        wasm_memory_t* memory = &mod->memories[base + i];
        uint8_t lf = wasm__read_u8(r);

        memory->pages = wasm__read_leb128_u32(r);
        memory->max_pages = (lf & 1) ? wasm__read_leb128_u32(r) : 65536;
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_global_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i, base;
    wasm_error_t err;

    err = wasm__append_global_slots(mod, count, &base);
    if (err != WASM_OK) return err;

    for (i = 0; i < count; i++) {
        uint32_t global_index = base + i;
        wasm_value_t value;

        mod->globals[global_index].type = (wasm_valtype_t)wasm__read_u8(r);
        mod->globals[global_index].is_mutable = wasm__read_u8(r);
        if (!wasm__is_value_type(mod->globals[global_index].type)) return WASM_ERR_MALFORMED;
        err = wasm__require_valtype_feature(mod, mod->globals[global_index].type);
        if (err != WASM_OK) return err;
        if (mod->globals[global_index].is_mutable) {
            err = wasm__require_feature(mod, WASM_FEATURE_MUTABLE_GLOBALS);
            if (err != WASM_OK) return err;
        }
        err = wasm__eval_init_expr(mod, r, global_index, &value);
        if (err != WASM_OK) return err;
        if (value.type != mod->globals[global_index].type) return WASM_ERR_TYPE_MISMATCH;
        wasm__global_set_value(&mod->globals[global_index], value);
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_tag_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i, base;
    wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_EXCEPTIONS);

    if (err != WASM_OK) return err;
    err = wasm__append_tags(mod, count, &base);
    if (err != WASM_OK) return err;

    for (i = 0; i < count; i++) {
        if (wasm__read_u8(r) != 0x00) return WASM_ERR_MALFORMED;
        mod->tags[base + i].type_idx = wasm__read_leb128_u32(r);
    }

    return WASM_OK;
}

static wasm_error_t wasm__decode_export_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i;
    mod->exports = (wasm_export_t*)WASM_CALLOC(count, sizeof(wasm_export_t));
    if (!mod->exports) return WASM_ERR_OOM;
    mod->num_exports = count;
    for (i = 0; i < count; i++) {
        wasm__read_name(r, mod->exports[i].name, sizeof(mod->exports[i].name));
        mod->exports[i].kind = (wasm_export_kind_t)wasm__read_u8(r);
        mod->exports[i].index = wasm__read_leb128_u32(r);
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_start_section(wasm_module_t* mod, wasm__reader_t* r) {
    mod->start_func = wasm__read_leb128_u32(r);
    return WASM_OK;
}

static wasm_error_t wasm__decode_element_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i, base;
    wasm_error_t err;

    err = wasm__append_elem_segments(mod, count, &base);
    if (err != WASM_OK) return err;

    for (i = 0; i < count; i++) {
        wasm_elem_segment_t* segment = &mod->elem_segments[base + i];
        uint32_t flags = wasm__read_leb128_u32(r);
        int uses_expr = (flags & 0x04) != 0;
        uint32_t j;

        if (flags > 0x07) return WASM_ERR_MALFORMED;

        if (flags & 0x01) {
            err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
        }

        segment->elem_type = WASM_TYPE_FUNCREF;
        if (flags & 0x01) {
            if (flags & 0x02)
                segment->is_declarative = 1;
            else
                segment->is_passive = 1;
        }

        if (!(flags & 0x01)) {
            wasm_value_t offset_value;

            segment->table_index = (flags & 0x02) ? wasm__read_leb128_u32(r) : 0;
            err = wasm__eval_init_expr(mod, r, mod->num_globals, &offset_value);
            if (err != WASM_OK) return err;
            if (offset_value.type != WASM_TYPE_I32) return WASM_ERR_TYPE_MISMATCH;
            segment->offset = (uint32_t)offset_value.of.i32;
        }

        if (uses_expr) {
            err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
            if (err != WASM_OK) return err;
            err = wasm__read_reftype(r, &segment->elem_type);
            if (err != WASM_OK) return err;
        } else if ((flags & 0x03) != 0) {
            err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            err = wasm__read_elemkind(r);
            if (err != WASM_OK) return err;
        }

        segment->num_elems = wasm__read_leb128_u32(r);
        if (segment->num_elems == 0) continue;

        segment->elems = (wasm_value_t*)WASM_MALLOC(segment->num_elems * sizeof(wasm_value_t));
        if (!segment->elems) return WASM_ERR_OOM;

        for (j = 0; j < segment->num_elems; j++) {
            wasm_value_t value;

            if (uses_expr) {
                err = wasm__eval_init_expr(mod, r, mod->num_globals, &value);
                if (err != WASM_OK) return err;
                if (!wasm__value_matches_type(&value, segment->elem_type)) return WASM_ERR_TYPE_MISMATCH;
            } else {
                uint32_t func_index = wasm__read_leb128_u32(r);
                if (func_index >= mod->num_funcs) return WASM_ERR_MALFORMED;
                value = wasm_funcref(func_index);
            }
            segment->elems[j] = value;
        }
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_code_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i;
    mod->num_code_bodies = count;
    for (i = 0; i < count; i++) {
        uint32_t body_size = wasm__read_leb128_u32(r);
        const uint8_t* body_start = r->ptr;
        uint32_t fidx = mod->num_func_imports + i;
        uint32_t ndecls, total_locals = 0, all_locals, local_off, d, j, p;
        const uint8_t* ls;
        wasm_functype_t* ft;
        if (fidx >= mod->num_funcs) return WASM_ERR_MALFORMED;
        ft = &mod->types[mod->funcs[fidx].type_idx];
        ndecls = wasm__read_leb128_u32(r);
        ls = r->ptr;
        for (d = 0; d < ndecls; d++) {
            total_locals += wasm__read_leb128_u32(r);
            wasm__read_u8(r);
        }
        all_locals = ft->num_params + total_locals;
        mod->funcs[fidx].num_locals = all_locals;
        mod->funcs[fidx].locals = (wasm_valtype_t*)WASM_MALLOC(all_locals * sizeof(wasm_valtype_t));
        if (!mod->funcs[fidx].locals) return WASM_ERR_OOM;
        for (p = 0; p < ft->num_params; p++) mod->funcs[fidx].locals[p] = ft->params[p];
        r->ptr = ls;
        local_off = ft->num_params;
        for (d = 0; d < ndecls; d++) {
            uint32_t n = wasm__read_leb128_u32(r);
            wasm_valtype_t t = (wasm_valtype_t)wasm__read_u8(r);
            if (!wasm__is_value_type(t)) return WASM_ERR_MALFORMED;
            if (wasm__require_valtype_feature(mod, t) != WASM_OK) return WASM_ERR_MALFORMED;
            for (j = 0; j < n; j++) mod->funcs[fidx].locals[local_off++] = t;
        }
        mod->funcs[fidx].code = r->ptr;
        mod->funcs[fidx].code_len = (uint32_t)(body_size - (uint32_t)(r->ptr - body_start));
        r->ptr = body_start + body_size;
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_data_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i, base;
    wasm_error_t err;

    err = wasm__append_data_segments(mod, count, &base);
    if (err != WASM_OK) return err;

    for (i = 0; i < count; i++) {
        wasm_data_segment_t* segment = &mod->data_segments[base + i];
        uint32_t flags = wasm__read_leb128_u32(r);

        switch (flags) {
            case 0x00: {
                wasm_value_t offset_value;

                err = wasm__eval_init_expr(mod, r, mod->num_globals, &offset_value);
                if (err != WASM_OK) return err;
                if (offset_value.type != WASM_TYPE_I32) return WASM_ERR_TYPE_MISMATCH;
                segment->offset = (uint32_t)offset_value.of.i32;
                segment->memory_index = 0;
                break;
            }
            case 0x01:
                err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
                if (err != WASM_OK) return err;
                segment->is_passive = 1;
                break;
            case 0x02: {
                wasm_value_t offset_value;

                err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
                if (err != WASM_OK) return err;
                segment->memory_index = wasm__read_leb128_u32(r);
                err = wasm__eval_init_expr(mod, r, mod->num_globals, &offset_value);
                if (err != WASM_OK) return err;
                if (offset_value.type != WASM_TYPE_I32) return WASM_ERR_TYPE_MISMATCH;
                segment->offset = (uint32_t)offset_value.of.i32;
                break;
            }
            default:
                return WASM_ERR_MALFORMED;
        }

        segment->size = wasm__read_leb128_u32(r);
        if (!wasm__has(r, segment->size)) return WASM_ERR_MALFORMED;
        if (segment->size > 0) {
            segment->bytes = (uint8_t*)WASM_MALLOC(segment->size);
            if (!segment->bytes) return WASM_ERR_OOM;
            memcpy(segment->bytes, r->ptr, segment->size);
        }
        r->ptr += segment->size;
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_data_count_section(wasm_module_t* mod, wasm__reader_t* r) {
    wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
    if (err != WASM_OK) return err;
    mod->declared_data_count = wasm__read_leb128_u32(r);
    mod->has_data_count_section = 1;
    return WASM_OK;
}

static uint32_t wasm__find_exported_func(wasm_module_t* mod, const char* name) {
    uint32_t i;

    for (i = 0; i < mod->num_exports; i++) {
        if (mod->exports[i].kind == WASM_EXPORT_FUNC && strcmp(mod->exports[i].name, name) == 0)
            return mod->exports[i].index;
    }

    return UINT32_MAX;
}

static wasm_error_t wasm__run_startup(wasm_module_t* mod) {
    wasm_error_t err;

    if (mod->startup_ran) return WASM_OK;

    mod->startup_ran = 1;

    if (mod->start_func != UINT32_MAX) {
        err = wasm_call_index(mod, mod->start_func, NULL, 0, NULL, 0);
        if (err != WASM_OK) {
            mod->startup_ran = 0;
            return err;
        }
        return WASM_OK;
    }

    {
        uint32_t ctor_idx = wasm__find_exported_func(mod, "__wasm_call_ctors");
        if (ctor_idx != UINT32_MAX) {
            err = wasm_call_index(mod, ctor_idx, NULL, 0, NULL, 0);
            if (err != WASM_OK) {
                mod->startup_ran = 0;
                return err;
            }
        }
    }

    return WASM_OK;
}

static wasm_error_t wasm__set_validation_error(wasm_runtime_t* rt,
                                               uint32_t func_idx,
                                               uint32_t offset,
                                               const char* fmt,
                                               ...) {
    char detail[192];
    va_list args;

    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);

    rt->last_error = WASM_ERR_MALFORMED;
    if (func_idx == UINT32_MAX)
        wasm__snprintf(rt->error_msg, sizeof(rt->error_msg), "validation error: %s", detail);
    else
        wasm__snprintf(rt->error_msg, sizeof(rt->error_msg),
                       "validation error: func[%u] offset 0x%X: %s",
                       (unsigned)func_idx, (unsigned)offset, detail);
    return WASM_ERR_MALFORMED;
}

static wasm_error_t wasm__validate_structural(wasm_module_t* mod) {
    uint32_t i, j;

    if (mod->num_code_bodies != mod->num_funcs - mod->num_func_imports) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "code section count %u does not match %u defined functions",
                                          (unsigned)mod->num_code_bodies,
                                          (unsigned)(mod->num_funcs - mod->num_func_imports));
    }

    for (i = 0; i < mod->num_funcs; i++) {
        if (mod->funcs[i].type_idx >= mod->num_types) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "function %u type index %u out of range (have %u types)",
                                              (unsigned)i,
                                              (unsigned)mod->funcs[i].type_idx,
                                              (unsigned)mod->num_types);
        }
    }

    for (i = 0; i < mod->num_tags; i++) {
        wasm_functype_t* tag_type;

        if (mod->tags[i].type_idx >= mod->num_types) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "tag %u type index %u out of range (have %u types)",
                                              (unsigned)i,
                                              (unsigned)mod->tags[i].type_idx,
                                              (unsigned)mod->num_types);
        }

        tag_type = &mod->types[mod->tags[i].type_idx];
        if (tag_type->num_results != 0) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "tag %u type must not declare results",
                                              (unsigned)i);
        }
    }

    for (i = 0; i < mod->num_tables; i++) {
        wasm_table_t* table = &mod->tables[i];

        if (!wasm__is_ref_type(table->reftype)) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "table %u has invalid reftype 0x%X",
                                              (unsigned)i, (unsigned)table->reftype);
        }
        if (table->max_size != UINT32_MAX && table->size > table->max_size) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "table %u min %u exceeds max %u",
                                              (unsigned)i,
                                              (unsigned)table->size,
                                              (unsigned)table->max_size);
        }
    }

    for (i = 0; i < mod->num_memories; i++) {
        wasm_memory_t* memory = &mod->memories[i];

        if (memory->max_pages != 0 && memory->pages > memory->max_pages) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "memory %u min %u exceeds max %u",
                                              (unsigned)i,
                                              (unsigned)memory->pages,
                                              (unsigned)memory->max_pages);
        }
        if (memory->max_pages > 65536u) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "memory %u max %u exceeds 65536 pages",
                                              (unsigned)i,
                                              (unsigned)memory->max_pages);
        }
    }

    for (i = 0; i < mod->num_exports; i++) {
        uint32_t limit = 0;

        switch (mod->exports[i].kind) {
            case WASM_EXPORT_FUNC:
                limit = mod->num_funcs;
                break;
            case WASM_EXPORT_TABLE:
                limit = mod->num_tables;
                break;
            case WASM_EXPORT_MEM:
                limit = mod->num_memories;
                break;
            case WASM_EXPORT_GLOBAL:
                limit = mod->num_globals;
                break;
            case WASM_EXPORT_TAG:
                limit = mod->num_tags;
                break;
            default:
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "export '%s' has invalid kind %u",
                                                  mod->exports[i].name,
                                                  (unsigned)mod->exports[i].kind);
        }

        if (mod->exports[i].index >= limit) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "export '%s' index %u out of range",
                                              mod->exports[i].name,
                                              (unsigned)mod->exports[i].index);
        }

        for (j = i + 1; j < mod->num_exports; j++) {
            if (strcmp(mod->exports[i].name, mod->exports[j].name) == 0) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "duplicate export name '%s'",
                                                  mod->exports[i].name);
            }
        }
    }

    if (mod->start_func != UINT32_MAX) {
        wasm_functype_t* start_type;

        if (mod->start_func >= mod->num_funcs) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "start function index %u out of range",
                                              (unsigned)mod->start_func);
        }
        start_type = &mod->types[mod->funcs[mod->start_func].type_idx];
        if (start_type->num_params != 0 || start_type->num_results != 0) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "start function must have signature [] -> []");
        }
    }

    for (i = 0; i < mod->num_elem_segments; i++) {
        wasm_elem_segment_t* segment = &mod->elem_segments[i];

        if (segment->elem_type != WASM_TYPE_FUNCREF && segment->elem_type != WASM_TYPE_EXTERNREF) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "element segment %u has invalid reftype 0x%X",
                                              (unsigned)i,
                                              (unsigned)segment->elem_type);
        }
        if (!segment->is_passive && !segment->is_declarative) {
            wasm_table_t* table;

            if (segment->table_index >= mod->num_tables) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "element segment %u table index %u out of range",
                                                  (unsigned)i,
                                                  (unsigned)segment->table_index);
            }
            table = &mod->tables[segment->table_index];
            if (table->reftype != segment->elem_type) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "element segment %u type mismatch with table %u",
                                                  (unsigned)i,
                                                  (unsigned)segment->table_index);
            }
            if (!wasm__range_in_bounds_u64(segment->offset, segment->num_elems, table->size)) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "element segment %u does not fit declared table bounds",
                                                  (unsigned)i);
            }
        }
    }

    for (i = 0; i < mod->num_data_segments; i++) {
        wasm_data_segment_t* segment = &mod->data_segments[i];

        if (segment->memory_index >= mod->num_memories && !(segment->is_passive && mod->num_memories == 0)) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "data segment %u memory index %u out of range",
                                              (unsigned)i,
                                              (unsigned)segment->memory_index);
        }
        if (!segment->is_passive &&
            !wasm__range_in_bounds_u64(segment->offset,
                                       segment->size,
                                       (uint64_t)mod->memories[segment->memory_index].pages * WASM_PAGE_SIZE)) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "data segment %u does not fit declared memory bounds",
                                              (unsigned)i);
        }
    }

    if (mod->has_data_count_section && mod->declared_data_count != mod->num_data_segments) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "data count section declares %u segments but module has %u",
                                          (unsigned)mod->declared_data_count,
                                          (unsigned)mod->num_data_segments);
    }

    return WASM_OK;
}

#ifndef WASM__MAX_LABELS
#define WASM__MAX_LABELS 256
#endif

typedef struct wasm__blocktype_t {
    uint32_t param_arity;
    uint32_t result_arity;
    const wasm_valtype_t* params;
    const wasm_valtype_t* results;
    wasm_valtype_t inline_result;
} wasm__blocktype_t;
static wasm_error_t wasm__read_blocktype(wasm_module_t* mod, wasm__reader_t* r, wasm__blocktype_t* blocktype);

#define WASM__TYPE_BOT ((wasm_valtype_t)0xFF)

typedef struct wasm__val_frame_t {
    uint8_t opcode;
    uint32_t height;
    const wasm_valtype_t* param_types;
    uint32_t num_params;
    const wasm_valtype_t* result_types;
    uint32_t num_results;
    wasm_valtype_t inline_result;
    int unreachable;
    int seen_else;
    int in_catch;
    int seen_catch_all;
    int seen_delegate;
} wasm__val_frame_t;

typedef struct wasm__validator_t {
    wasm_module_t* mod;
    uint32_t func_idx;
    const uint8_t* body_start;
    wasm__reader_t r;
    wasm_valtype_t stack[WASM_MAX_STACK];
    uint32_t sp;
    wasm__val_frame_t frames[WASM__MAX_LABELS];
    uint32_t fp;
} wasm__validator_t;

static wasm_error_t wasm__validator_error(wasm__validator_t* v,
                                          const uint8_t* at,
                                          const char* fmt,
                                          ...) {
    char detail[192];
    va_list args;

    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);

    return wasm__set_validation_error(v->mod->rt,
                                      v->func_idx,
                                      (uint32_t)(at - v->body_start),
                                      "%s",
                                      detail);
}

static wasm_error_t wasm__validator_require_feature(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    uint32_t flag) {
    wasm_error_t err = wasm__require_feature(v->mod, flag);
    if (err != WASM_OK)
        return wasm__validator_error(v, at, "%s", v->mod->rt->error_msg);
    return WASM_OK;
}

static int wasm__validator_type_matches(wasm_valtype_t actual, wasm_valtype_t expected) {
    return actual == WASM__TYPE_BOT || expected == WASM__TYPE_BOT || actual == expected;
}

static wasm__val_frame_t* wasm__validator_frame(wasm__validator_t* v) {
    return &v->frames[v->fp - 1];
}

static wasm_error_t wasm__validator_push(wasm__validator_t* v,
                                         const uint8_t* at,
                                         wasm_valtype_t type) {
    if (v->sp >= WASM_MAX_STACK)
        return wasm__validator_error(v, at, "operand stack overflow");
    v->stack[v->sp++] = type;
    return WASM_OK;
}

static wasm_error_t wasm__validator_push_many(wasm__validator_t* v,
                                              const uint8_t* at,
                                              const wasm_valtype_t* types,
                                              uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        wasm_error_t err = wasm__validator_push(v, at, types[i]);
        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

static wasm_error_t wasm__validator_pop_any(wasm__validator_t* v,
                                            const uint8_t* at,
                                            wasm_valtype_t* out_type) {
    wasm__val_frame_t* frame = wasm__validator_frame(v);

    if (v->sp == frame->height) {
        if (!frame->unreachable)
            return wasm__validator_error(v, at, "stack underflow");
        *out_type = WASM__TYPE_BOT;
        return WASM_OK;
    }

    *out_type = v->stack[--v->sp];
    return WASM_OK;
}

static wasm_error_t wasm__validator_pop_expect(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t expected) {
    wasm_valtype_t actual;
    wasm_error_t err = wasm__validator_pop_any(v, at, &actual);
    if (err != WASM_OK) return err;
    if (!wasm__validator_type_matches(actual, expected)) {
        return wasm__validator_error(v, at,
                                     "type mismatch: expected 0x%X, got 0x%X",
                                     (unsigned)expected,
                                     (unsigned)actual);
    }
    return WASM_OK;
}

static wasm_error_t wasm__validator_check_types(wasm__validator_t* v,
                                                const uint8_t* at,
                                                const wasm_valtype_t* types,
                                                uint32_t count) {
    wasm__val_frame_t* frame = wasm__validator_frame(v);
    uint32_t sp = v->sp;
    uint32_t i;

    for (i = 0; i < count; i++) {
        wasm_valtype_t actual;
        wasm_valtype_t expected = types[count - 1 - i];

        if (sp == frame->height) {
            if (!frame->unreachable)
                return wasm__validator_error(v, at, "stack underflow");
            actual = WASM__TYPE_BOT;
        } else {
            actual = v->stack[--sp];
        }

        if (!wasm__validator_type_matches(actual, expected)) {
            return wasm__validator_error(v, at,
                                         "type mismatch: expected 0x%X, got 0x%X",
                                         (unsigned)expected,
                                         (unsigned)actual);
        }
    }

    return WASM_OK;
}

static void wasm__validator_mark_unreachable(wasm__validator_t* v) {
    wasm__val_frame_t* frame = wasm__validator_frame(v);
    v->sp = frame->height;
    frame->unreachable = 1;
}

static int wasm__validator_same_types(const wasm_valtype_t* a,
                                      uint32_t a_count,
                                      const wasm_valtype_t* b,
                                      uint32_t b_count) {
    uint32_t i;
    if (a_count != b_count) return 0;
    for (i = 0; i < a_count; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static void wasm__validator_branch_signature(const wasm__val_frame_t* frame,
                                             const wasm_valtype_t** out_types,
                                             uint32_t* out_count) {
    if (frame->opcode == 0x03) {
        *out_types = frame->param_types;
        *out_count = frame->num_params;
    } else {
        *out_types = frame->result_types;
        *out_count = frame->num_results;
    }
}

static wasm_error_t wasm__validator_check_tail_results(wasm__validator_t* v,
                                                       const uint8_t* at,
                                                       const wasm_functype_t* caller_type,
                                                       const wasm_functype_t* callee_type) {
    if (!wasm__validator_same_types(caller_type->results, caller_type->num_results,
                                    callee_type->results, callee_type->num_results)) {
        return wasm__validator_error(v, at,
                                     "tail call results must match the enclosing function results");
    }
    return WASM_OK;
}

static wasm_error_t wasm__validator_finish_try_arm(wasm__validator_t* v,
                                                   const uint8_t* at,
                                                   wasm__val_frame_t* frame) {
    wasm_error_t err;

    if (!frame->unreachable) {
        if (v->sp != frame->height + frame->num_results)
            return wasm__validator_error(v, at, "try arm leaves wrong stack height");
        err = wasm__validator_check_types(v, at, frame->result_types, frame->num_results);
        if (err != WASM_OK) return err;
    }

    v->sp = frame->height;
    frame->unreachable = 0;
    frame->in_catch = 1;
    return WASM_OK;
}

static wasm_error_t wasm__validator_find_rethrow_frame(wasm__validator_t* v,
                                                       uint32_t depth,
                                                       wasm__val_frame_t** out_frame) {
    uint32_t i;

    for (i = v->fp; i > 0; i--) {
        wasm__val_frame_t* frame = &v->frames[i - 1];

        if (frame->opcode == 0x06 && frame->in_catch) {
            if (depth == 0) {
                *out_frame = frame;
                return WASM_OK;
            }
            depth--;
        }
    }

    return WASM_ERR_MALFORMED;
}

static wasm_error_t wasm__validator_read_memarg(wasm__validator_t* v,
                                                const uint8_t* at,
                                                uint32_t max_align_log2) {
    wasm__memarg_t memarg;
    wasm_error_t err = wasm__read_memarg(&v->r, &memarg);

    if (err != WASM_OK)
        return wasm__validator_error(v, at, "malformed memarg");
    if (memarg.align_log2 > max_align_log2)
        return wasm__validator_error(v, at, "alignment %u exceeds natural alignment %u",
                                     (unsigned)memarg.align_log2,
                                     (unsigned)max_align_log2);
    if (memarg.memory_index != 0) {
        err = wasm__validator_require_feature(v, at, WASM_FEATURE_MULTI_MEMORY);
        if (err != WASM_OK) return err;
    }
    if (memarg.memory_index >= v->mod->num_memories)
        return wasm__validator_error(v, at, "memory %u out of range", (unsigned)memarg.memory_index);
    return WASM_OK;
}

static wasm_error_t wasm__validator_unary(wasm__validator_t* v,
                                          const uint8_t* at,
                                          wasm_valtype_t in_type,
                                          wasm_valtype_t out_type) {
    wasm_error_t err = wasm__validator_pop_expect(v, at, in_type);
    if (err != WASM_OK) return err;
    return wasm__validator_push(v, at, out_type);
}

static wasm_error_t wasm__validator_binary(wasm__validator_t* v,
                                           const uint8_t* at,
                                           wasm_valtype_t lhs_type,
                                           wasm_valtype_t rhs_type,
                                           wasm_valtype_t out_type) {
    wasm_error_t err = wasm__validator_pop_expect(v, at, rhs_type);
    if (err != WASM_OK) return err;
    err = wasm__validator_pop_expect(v, at, lhs_type);
    if (err != WASM_OK) return err;
    return wasm__validator_push(v, at, out_type);
}

static wasm_error_t wasm__validator_pop_three_i32(wasm__validator_t* v,
                                                  const uint8_t* at) {
    wasm_error_t err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
    if (err != WASM_OK) return err;
    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
    if (err != WASM_OK) return err;
    return wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
}

static wasm_error_t wasm__validator_read_typed_select(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      wasm_valtype_t* out_type) {
    wasm_error_t err = wasm__read_typed_select_immediate(&v->r, out_type);

    if (err != WASM_OK)
        return wasm__validator_error(v, at, "typed select requires exactly one value type");
    if (wasm__is_ref_type(*out_type)) {
        err = wasm__validator_require_feature(v, at, WASM_FEATURE_REFERENCE_TYPES);
        if (err != WASM_OK) return err;
    }
    if (wasm__is_vector_type(*out_type)) {
        err = wasm__validator_require_feature(v, at, WASM_FEATURE_SIMD);
        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

static wasm_error_t wasm__validator_read_simd_lane(wasm__validator_t* v,
                                                   const uint8_t* at,
                                                   uint32_t subop) {
    uint32_t lane_limit = wasm__simd_lane_limit(subop);
    uint32_t lane = wasm__read_u8(&v->r);

    if (lane >= lane_limit)
        return wasm__validator_error(v, at, "lane %u out of range", (unsigned)lane);
    return WASM_OK;
}

static wasm_error_t wasm__validator_validate_simd(wasm__validator_t* v, const uint8_t* at) {
    uint32_t subop = wasm__read_leb128_u32(&v->r);
    wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_SIMD);

    if (err != WASM_OK) return err;

    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x5C:
        case 0x5D: {
            uint32_t max_align = wasm__simd_memory_align_log2(subop);
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "SIMD memory load requires a memory");
            err = wasm__validator_read_memarg(v, at, max_align);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        }
        case 0x0B: {
            uint32_t max_align = wasm__simd_memory_align_log2(subop);
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "SIMD memory store requires a memory");
            err = wasm__validator_read_memarg(v, at, max_align);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
        }
        case 0x0C:
            v->r.ptr += 16;
            if (v->r.ptr > v->r.end) {
                v->r.ptr = v->r.end;
                v->r.malformed = 1;
            }
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x0D: {
            uint32_t lane;
            for (lane = 0; lane < 16; lane++) {
                uint32_t index = wasm__read_u8(&v->r);
                if (index >= 32u)
                    return wasm__validator_error(v, at, "shuffle lane %u out of range", (unsigned)index);
            }
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        }
        case 0x0E:
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x0F:
        case 0x10:
        case 0x11:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x12:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I64);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x13:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_F32);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x14:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_F64);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x15:
        case 0x16:
        case 0x17:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x17) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        case 0x18:
        case 0x19:
        case 0x1A:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x1A) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        case 0x1B:
        case 0x1C:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x1C) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        case 0x1D:
        case 0x1E:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x1E) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_I64);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I64);
        case 0x1F:
        case 0x20:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x20) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_F32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_F32);
        case 0x21:
        case 0x22:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x22) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_F64);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_F64);
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x4D:
            return wasm__validator_unary(v, at, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51:
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x52:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x53:
        case 0x63:
        case 0x64:
        case 0x83:
        case 0x84:
        case 0xA3:
        case 0xA4:
        case 0xC3:
        case 0xC4:
            return wasm__validator_unary(v, at, WASM_TYPE_V128, WASM_TYPE_I32);
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57: {
            uint32_t max_align = wasm__simd_memory_align_log2(subop);
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "SIMD lane load requires a memory");
            err = wasm__validator_read_memarg(v, at, max_align);
            if (err != WASM_OK) return err;
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        }
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B: {
            uint32_t max_align = wasm__simd_memory_align_log2(subop);
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "SIMD lane store requires a memory");
            err = wasm__validator_read_memarg(v, at, max_align);
            if (err != WASM_OK) return err;
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
        }
        case 0x5E:
        case 0x5F:
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x74:
        case 0x75:
        case 0x7A:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
        case 0x80:
        case 0x81:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0xA0:
        case 0xA1:
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA:
        case 0xC0:
        case 0xC1:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xE0:
        case 0xE1:
        case 0xE3:
        case 0xEC:
        case 0xED:
        case 0xEF:
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
            return wasm__validator_unary(v, at, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x65:
        case 0x66:
        case 0x6E:
        case 0x6F:
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x78:
        case 0x79:
        case 0x7B:
        case 0x82:
        case 0x85:
        case 0x86:
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x95:
        case 0x96:
        case 0x97:
        case 0x98:
        case 0x99:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        case 0xAE:
        case 0xB1:
        case 0xB5:
        case 0xB6:
        case 0xB7:
        case 0xB8:
        case 0xB9:
        case 0xBA:
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF:
        case 0xCE:
        case 0xD1:
        case 0xD5:
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF:
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0xF0:
        case 0xF1:
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0xAB:
        case 0xAC:
        case 0xAD:
        case 0xCB:
        case 0xCC:
        case 0xCD:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        default:
            return wasm__validator_error(v, at, "unknown prefixed opcode 0xFD 0x%X", (unsigned)subop);
    }
}

static wasm_error_t wasm__validator_validate_numeric(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     uint8_t op) {
    if (op >= 0x45 && op <= 0x4F) {
        if (op == 0x45) return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_I32);
        return wasm__validator_binary(v, at, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32);
    }
    if (op >= 0x50 && op <= 0x5A) {
        if (op == 0x50) return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_I32);
        return wasm__validator_binary(v, at, WASM_TYPE_I64, WASM_TYPE_I64, WASM_TYPE_I32);
    }
    if (op >= 0x5B && op <= 0x60)
        return wasm__validator_binary(v, at, WASM_TYPE_F32, WASM_TYPE_F32, WASM_TYPE_I32);
    if (op >= 0x61 && op <= 0x66)
        return wasm__validator_binary(v, at, WASM_TYPE_F64, WASM_TYPE_F64, WASM_TYPE_I32);
    if (op >= 0x67 && op <= 0x69)
        return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_I32);
    if (op >= 0x6A && op <= 0x78)
        return wasm__validator_binary(v, at, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32);
    if (op >= 0x79 && op <= 0x7B)
        return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_I64);
    if (op >= 0x7C && op <= 0x8A)
        return wasm__validator_binary(v, at, WASM_TYPE_I64, WASM_TYPE_I64, WASM_TYPE_I64);
    if (op >= 0x8B && op <= 0x91)
        return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_F32);
    if (op >= 0x92 && op <= 0x98)
        return wasm__validator_binary(v, at, WASM_TYPE_F32, WASM_TYPE_F32, WASM_TYPE_F32);
    if (op >= 0x99 && op <= 0x9F)
        return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_F64);
    if (op >= 0xA0 && op <= 0xA6)
        return wasm__validator_binary(v, at, WASM_TYPE_F64, WASM_TYPE_F64, WASM_TYPE_F64);

    switch (op) {
        case 0xA7:
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_I32);
        case 0xA8:
        case 0xA9:
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I32);
        case 0xAA:
        case 0xAB:
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I32);
        case 0xAC:
        case 0xAD:
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_I64);
        case 0xAE:
        case 0xAF:
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I64);
        case 0xB0:
        case 0xB1:
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I64);
        case 0xB2:
        case 0xB3:
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_F32);
        case 0xB4:
        case 0xB5:
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_F32);
        case 0xB6:
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_F32);
        case 0xB7:
        case 0xB8:
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_F64);
        case 0xB9:
        case 0xBA:
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_F64);
        case 0xBB:
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_F64);
        case 0xBC:
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I32);
        case 0xBD:
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I64);
        case 0xBE:
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_F32);
        case 0xBF:
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_F64);
        case 0xC0:
        case 0xC1: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_SIGN_EXT);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_I32);
        }
        case 0xC2:
        case 0xC3:
        case 0xC4: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_SIGN_EXT);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_I64);
        }
        default:
            return wasm__validator_error(v, at, "unknown opcode 0x%02X", op);
    }
}

static wasm_error_t wasm__validator_validate_prefixed(wasm__validator_t* v,
                                                      const uint8_t* at) {
    uint32_t subop = wasm__read_leb128_u32(&v->r);

    switch (subop) {
        case 0x00:
        case 0x01: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_NONTRAPPING_FMA);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I32);
        }
        case 0x02:
        case 0x03: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_NONTRAPPING_FMA);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I32);
        }
        case 0x04:
        case 0x05: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_NONTRAPPING_FMA);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I64);
        }
        case 0x06:
        case 0x07: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_NONTRAPPING_FMA);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I64);
        }
        case 0x08: {
            uint32_t data_idx = wasm__read_leb128_u32(&v->r);
            uint32_t mem_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            v->mod->uses_data_count_section = 1;
            if (data_idx >= v->mod->num_data_segments)
                return wasm__validator_error(v, at, "data segment %u out of range", (unsigned)data_idx);
            if (mem_idx != 0) {
                err = wasm__validator_require_feature(v, at, WASM_FEATURE_MULTI_MEMORY);
                if (err != WASM_OK) return err;
            }
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "memory.init requires a memory");
            if (mem_idx >= v->mod->num_memories)
                return wasm__validator_error(v, at, "memory %u out of range", (unsigned)mem_idx);
            return wasm__validator_pop_three_i32(v, at);
        }
        case 0x09: {
            uint32_t data_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            v->mod->uses_data_count_section = 1;
            if (data_idx >= v->mod->num_data_segments)
                return wasm__validator_error(v, at, "data segment %u out of range", (unsigned)data_idx);
            return WASM_OK;
        }
        case 0x0A: {
            uint32_t dst_mem_idx = wasm__read_leb128_u32(&v->r);
            uint32_t src_mem_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            if (dst_mem_idx != 0 || src_mem_idx != 0) {
                err = wasm__validator_require_feature(v, at, WASM_FEATURE_MULTI_MEMORY);
                if (err != WASM_OK) return err;
            }
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "memory.copy requires a memory");
            if (dst_mem_idx >= v->mod->num_memories || src_mem_idx >= v->mod->num_memories)
                return wasm__validator_error(v, at, "memory.copy memory index out of range");
            return wasm__validator_pop_three_i32(v, at);
        }
        case 0x0B: {
            uint32_t mem_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            if (mem_idx != 0) {
                err = wasm__validator_require_feature(v, at, WASM_FEATURE_MULTI_MEMORY);
                if (err != WASM_OK) return err;
            }
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "memory.fill requires a memory");
            if (mem_idx >= v->mod->num_memories)
                return wasm__validator_error(v, at, "memory %u out of range", (unsigned)mem_idx);
            return wasm__validator_pop_three_i32(v, at);
        }
        case 0x0C: {
            uint32_t elem_idx = wasm__read_leb128_u32(&v->r);
            uint32_t table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            wasm_elem_segment_t* segment;
            wasm_table_t* table;
            if (err != WASM_OK) return err;
            if (elem_idx >= v->mod->num_elem_segments || table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "table.init index out of range");
            segment = &v->mod->elem_segments[elem_idx];
            table = &v->mod->tables[table_idx];
            if (segment->elem_type != table->reftype)
                return wasm__validator_error(v, at, "table.init type mismatch");
            return wasm__validator_pop_three_i32(v, at);
        }
        case 0x0D: {
            uint32_t elem_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            if (elem_idx >= v->mod->num_elem_segments)
                return wasm__validator_error(v, at, "element segment %u out of range", (unsigned)elem_idx);
            return WASM_OK;
        }
        case 0x0E: {
            uint32_t dst_table_idx = wasm__read_leb128_u32(&v->r);
            uint32_t src_table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            if (dst_table_idx >= v->mod->num_tables || src_table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "table.copy index out of range");
            if (v->mod->tables[dst_table_idx].reftype != v->mod->tables[src_table_idx].reftype)
                return wasm__validator_error(v, at, "table.copy type mismatch");
            return wasm__validator_pop_three_i32(v, at);
        }
        case 0x0F: {
            uint32_t table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_REFERENCE_TYPES);
            wasm_table_t* table;
            if (err != WASM_OK) return err;
            if (table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "table %u out of range", (unsigned)table_idx);
            table = &v->mod->tables[table_idx];
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, table->reftype);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        }
        case 0x10: {
            uint32_t table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_REFERENCE_TYPES);
            if (err != WASM_OK) return err;
            if (table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "table %u out of range", (unsigned)table_idx);
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        }
        case 0x11: {
            uint32_t table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_REFERENCE_TYPES);
            wasm_table_t* table;
            if (err != WASM_OK) return err;
            if (table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "table %u out of range", (unsigned)table_idx);
            table = &v->mod->tables[table_idx];
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, table->reftype);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
        }
        default:
            return wasm__validator_error(v, at, "unknown prefixed opcode 0xFC 0x%X", (unsigned)subop);
    }
}

static wasm_error_t wasm__validate_function(wasm_module_t* mod, uint32_t func_idx) {
    wasm__validator_t v;
    wasm_func_t* func = &mod->funcs[func_idx];
    wasm_functype_t* ft = &mod->types[func->type_idx];

    memset(&v, 0, sizeof(v));
    v.mod = mod;
    v.func_idx = func_idx;
    v.body_start = func->code;
    v.r.ptr = func->code;
    v.r.end = func->code + func->code_len;
    v.r.malformed = 0;
    v.frames[0].opcode = 0xFF;
    v.frames[0].height = 0;
    v.frames[0].param_types = NULL;
    v.frames[0].num_params = 0;
    v.frames[0].result_types = ft->results;
    v.frames[0].num_results = ft->num_results;
    v.frames[0].in_catch = 0;
    v.frames[0].seen_catch_all = 0;
    v.frames[0].seen_delegate = 0;
    v.fp = 1;

    while (v.r.ptr < v.r.end) {
        const uint8_t* at = v.r.ptr;
        uint8_t op = wasm__read_u8(&v.r);
        wasm_error_t err = WASM_OK;

        switch (op) {
            case 0x00:
                wasm__validator_mark_unreachable(&v);
                break;
            case 0x01:
                break;
            case 0x02:
            case 0x03: {
                wasm__blocktype_t blocktype;

                err = wasm__read_blocktype(mod, &v.r, &blocktype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "%s", mod->rt->error_msg);
                err = wasm__validator_check_types(&v, at, blocktype.params, blocktype.param_arity);
                if (err != WASM_OK) return err;
                if (v.fp >= WASM__MAX_LABELS)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = v.sp - blocktype.param_arity;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                v.fp++;
                break;
            }
            case 0x04: {
                wasm__blocktype_t blocktype;

                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__read_blocktype(mod, &v.r, &blocktype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "%s", mod->rt->error_msg);
                err = wasm__validator_check_types(&v, at, blocktype.params, blocktype.param_arity);
                if (err != WASM_OK) return err;
                if (v.fp >= WASM__MAX_LABELS)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = v.sp - blocktype.param_arity;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                v.fp++;
                break;
            }
            case 0x05: {
                wasm__val_frame_t* frame;

                if (v.fp <= 1)
                    return wasm__validator_error(&v, at, "else without matching if");
                frame = wasm__validator_frame(&v);
                if (frame->opcode != 0x04 || frame->seen_else)
                    return wasm__validator_error(&v, at, "unexpected else");
                if (!frame->unreachable) {
                    if (v.sp != frame->height + frame->num_results)
                        return wasm__validator_error(&v, at, "then branch leaves wrong stack height");
                    err = wasm__validator_check_types(&v, at, frame->result_types, frame->num_results);
                    if (err != WASM_OK) return err;
                }
                v.sp = frame->height;
                err = wasm__validator_push_many(&v, at, frame->param_types, frame->num_params);
                if (err != WASM_OK) return err;
                frame->unreachable = 0;
                frame->seen_else = 1;
                break;
            }
            case 0x06: {
                wasm__blocktype_t blocktype;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                err = wasm__read_blocktype(mod, &v.r, &blocktype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "%s", mod->rt->error_msg);
                err = wasm__validator_check_types(&v, at, blocktype.params, blocktype.param_arity);
                if (err != WASM_OK) return err;
                if (v.fp >= WASM__MAX_LABELS)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = v.sp - blocktype.param_arity;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                v.fp++;
                break;
            }
            case 0x07: {
                uint32_t tag_index = wasm__read_leb128_u32(&v.r);
                wasm__val_frame_t* frame;
                wasm_functype_t* tag_type;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                if (v.fp <= 1)
                    return wasm__validator_error(&v, at, "catch without matching try");
                frame = wasm__validator_frame(&v);
                if (frame->opcode != 0x06 || frame->seen_catch_all || frame->seen_delegate)
                    return wasm__validator_error(&v, at, "unexpected catch");
                if (tag_index >= mod->num_tags)
                    return wasm__validator_error(&v, at, "tag %u out of range", (unsigned)tag_index);
                tag_type = &mod->types[mod->tags[tag_index].type_idx];
                err = wasm__validator_finish_try_arm(&v, at, frame);
                if (err != WASM_OK) return err;
                err = wasm__validator_push_many(&v, at, tag_type->params, tag_type->num_params);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x08: {
                uint32_t tag_index = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* tag_type;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                if (tag_index >= mod->num_tags)
                    return wasm__validator_error(&v, at, "tag %u out of range", (unsigned)tag_index);
                tag_type = &mod->types[mod->tags[tag_index].type_idx];
                err = wasm__validator_check_types(&v, at, tag_type->params, tag_type->num_params);
                if (err != WASM_OK) return err;
                v.sp -= tag_type->num_params;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x09: {
                uint32_t depth = wasm__read_leb128_u32(&v.r);
                wasm__val_frame_t* target_frame;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                err = wasm__validator_find_rethrow_frame(&v, depth, &target_frame);
                if (err != WASM_OK) {
                    return wasm__validator_error(&v, at,
                                                 "rethrow depth %u does not reference an active catch",
                                                 (unsigned)depth);
                }
                (void)target_frame;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x0B: {
                wasm__val_frame_t frame;
                if (v.fp == 0)
                    return wasm__validator_error(&v, at, "unexpected end");
                frame = v.frames[v.fp - 1];
                if (frame.opcode == 0x04 && !frame.seen_else &&
                    !wasm__validator_same_types(frame.param_types, frame.num_params,
                                                frame.result_types, frame.num_results)) {
                    return wasm__validator_error(&v, at,
                                                 "if without else requires param and result types to match");
                }
                if (!frame.unreachable) {
                    if (v.sp != frame.height + frame.num_results)
                        return wasm__validator_error(&v, at, "block leaves wrong stack height");
                    err = wasm__validator_check_types(&v, at, frame.result_types, frame.num_results);
                    if (err != WASM_OK) return err;
                }
                v.sp = frame.height;
                v.fp--;
                err = wasm__validator_push_many(&v, at, frame.result_types, frame.num_results);
                if (err != WASM_OK) return err;
                if (v.fp == 0) {
                    if (v.r.ptr != v.r.end)
                        return wasm__validator_error(&v, at, "instructions found after function end");
                    break;
                }
                break;
            }
            case 0x0C:
            case 0x0D: {
                uint32_t depth = wasm__read_leb128_u32(&v.r);
                const wasm__val_frame_t* target;
                const wasm_valtype_t* branch_types;
                uint32_t branch_count;

                if (op == 0x0D) {
                    err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                }
                if (depth >= v.fp)
                    return wasm__validator_error(&v, at, "branch depth %u out of range", (unsigned)depth);
                target = &v.frames[v.fp - 1 - depth];
                wasm__validator_branch_signature(target, &branch_types, &branch_count);
                err = wasm__validator_check_types(&v, at, branch_types, branch_count);
                if (err != WASM_OK) return err;
                if (op == 0x0C) wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x0E: {
                uint32_t count = wasm__read_leb128_u32(&v.r);
                uint32_t first_depth = UINT32_MAX;
                const wasm_valtype_t* branch_types = NULL;
                uint32_t branch_count = 0;
                uint32_t i;

                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                for (i = 0; i <= count; i++) {
                    uint32_t depth = wasm__read_leb128_u32(&v.r);
                    const wasm_valtype_t* candidate_types;
                    uint32_t candidate_count;

                    if (depth >= v.fp)
                        return wasm__validator_error(&v, at, "br_table depth %u out of range", (unsigned)depth);
                    wasm__validator_branch_signature(&v.frames[v.fp - 1 - depth], &candidate_types, &candidate_count);
                    if (i == 0) {
                        first_depth = depth;
                        branch_types = candidate_types;
                        branch_count = candidate_count;
                    } else if (!wasm__validator_same_types(branch_types, branch_count,
                                                           candidate_types, candidate_count)) {
                        return wasm__validator_error(&v, at,
                                                     "br_table targets do not share the same signature");
                    }
                }
                (void)first_depth;
                err = wasm__validator_check_types(&v, at, branch_types, branch_count);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x0F:
                err = wasm__validator_check_types(&v, at, ft->results, ft->num_results);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            case 0x10: {
                uint32_t callee = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;
                if (callee >= mod->num_funcs)
                    return wasm__validator_error(&v, at, "call target %u out of range", (unsigned)callee);
                callee_type = &mod->types[mod->funcs[callee].type_idx];
                err = wasm__validator_check_types(&v, at, callee_type->params, callee_type->num_params);
                if (err != WASM_OK) return err;
                v.sp -= callee_type->num_params;
                err = wasm__validator_push_many(&v, at, callee_type->results, callee_type->num_results);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x11: {
                uint32_t type_idx = wasm__read_leb128_u32(&v.r);
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;
                if (type_idx >= mod->num_types)
                    return wasm__validator_error(&v, at, "call_indirect type index %u out of range", (unsigned)type_idx);
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "call_indirect table index %u out of range", (unsigned)table_idx);
                if (table_idx != 0) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                    if (err != WASM_OK) return err;
                }
                if (mod->tables[table_idx].reftype != WASM_TYPE_FUNCREF)
                    return wasm__validator_error(&v, at, "call_indirect requires a funcref table");
                callee_type = &mod->types[type_idx];
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_check_types(&v, at, callee_type->params, callee_type->num_params);
                if (err != WASM_OK) return err;
                v.sp -= callee_type->num_params;
                err = wasm__validator_push_many(&v, at, callee_type->results, callee_type->num_results);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x12: {
                uint32_t callee = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_TAIL_CALL);
                if (err != WASM_OK) return err;
                if (callee >= mod->num_funcs)
                    return wasm__validator_error(&v, at, "call target %u out of range", (unsigned)callee);
                callee_type = &mod->types[mod->funcs[callee].type_idx];
                err = wasm__validator_check_types(&v, at, callee_type->params, callee_type->num_params);
                if (err != WASM_OK) return err;
                v.sp -= callee_type->num_params;
                err = wasm__validator_check_tail_results(&v, at, ft, callee_type);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x13: {
                uint32_t type_idx = wasm__read_leb128_u32(&v.r);
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_TAIL_CALL);
                if (err != WASM_OK) return err;
                if (type_idx >= mod->num_types)
                    return wasm__validator_error(&v, at, "call_indirect type index %u out of range", (unsigned)type_idx);
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "call_indirect table index %u out of range", (unsigned)table_idx);
                if (table_idx != 0) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                    if (err != WASM_OK) return err;
                }
                if (mod->tables[table_idx].reftype != WASM_TYPE_FUNCREF)
                    return wasm__validator_error(&v, at, "call_indirect requires a funcref table");
                callee_type = &mod->types[type_idx];
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_check_types(&v, at, callee_type->params, callee_type->num_params);
                if (err != WASM_OK) return err;
                v.sp -= callee_type->num_params;
                err = wasm__validator_check_tail_results(&v, at, ft, callee_type);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x18: {
                uint32_t depth = wasm__read_leb128_u32(&v.r);
                wasm__val_frame_t* frame;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                if (v.fp <= 1)
                    return wasm__validator_error(&v, at, "delegate without matching try");
                frame = wasm__validator_frame(&v);
                if (frame->opcode != 0x06 || frame->seen_delegate || frame->seen_catch_all)
                    return wasm__validator_error(&v, at, "unexpected delegate");
                if (depth >= v.fp - 1)
                    return wasm__validator_error(&v, at, "delegate depth %u out of range", (unsigned)depth);
                err = wasm__validator_finish_try_arm(&v, at, frame);
                if (err != WASM_OK) return err;
                frame->seen_delegate = 1;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x1A: {
                wasm_valtype_t ignored;
                err = wasm__validator_pop_any(&v, at, &ignored);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x19: {
                wasm__val_frame_t* frame;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                if (v.fp <= 1)
                    return wasm__validator_error(&v, at, "catch_all without matching try");
                frame = wasm__validator_frame(&v);
                if (frame->opcode != 0x06 || frame->seen_catch_all || frame->seen_delegate)
                    return wasm__validator_error(&v, at, "unexpected catch_all");
                err = wasm__validator_finish_try_arm(&v, at, frame);
                if (err != WASM_OK) return err;
                frame->seen_catch_all = 1;
                break;
            }
            case 0x1B: {
                wasm_valtype_t lhs, rhs;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_any(&v, at, &rhs);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_any(&v, at, &lhs);
                if (err != WASM_OK) return err;
                if (!wasm__validator_type_matches(lhs, rhs) ||
                    ((lhs != WASM__TYPE_BOT && !wasm__is_number_or_vector_type(lhs)) ||
                     (rhs != WASM__TYPE_BOT && !wasm__is_number_or_vector_type(rhs)))) {
                    return wasm__validator_error(&v, at, "select requires matching numeric or vector operand types");
                }
                err = wasm__validator_push(&v, at, lhs == WASM__TYPE_BOT ? rhs : lhs);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x1C: {
                wasm_valtype_t select_type;

                err = wasm__validator_read_typed_select(&v, at, &select_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, select_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, select_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_push(&v, at, select_type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x20: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                err = wasm__validator_push(&v, at, func->locals[index]);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x21: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                err = wasm__validator_pop_expect(&v, at, func->locals[index]);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x22: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                err = wasm__validator_check_types(&v, at, &func->locals[index], 1);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x23: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= mod->num_globals)
                    return wasm__validator_error(&v, at, "global index %u out of range", (unsigned)index);
                err = wasm__validator_push(&v, at, mod->globals[index].type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x24: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= mod->num_globals)
                    return wasm__validator_error(&v, at, "global index %u out of range", (unsigned)index);
                if (!mod->globals[index].is_mutable)
                    return wasm__validator_error(&v, at, "global %u is immutable", (unsigned)index);
                err = wasm__validator_pop_expect(&v, at, mod->globals[index].type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x25: {
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "table %u out of range", (unsigned)table_idx);
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_push(&v, at, mod->tables[table_idx].reftype);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x26: {
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "table %u out of range", (unsigned)table_idx);
                err = wasm__validator_pop_expect(&v, at, mod->tables[table_idx].reftype);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x28:
            case 0x29:
            case 0x2A:
            case 0x2B:
            case 0x2C:
            case 0x2D:
            case 0x2E:
            case 0x2F:
            case 0x30:
            case 0x31:
            case 0x32:
            case 0x33:
            case 0x34:
            case 0x35: {
                wasm_valtype_t result_type = WASM_TYPE_I32;
                uint32_t max_align = 0;
                if (mod->num_memories == 0)
                    return wasm__validator_error(&v, at, "memory load requires a memory");
                switch (op) {
                    case 0x28:
                        result_type = WASM_TYPE_I32;
                        max_align = 2;
                        break;
                    case 0x29:
                        result_type = WASM_TYPE_I64;
                        max_align = 3;
                        break;
                    case 0x2A:
                        result_type = WASM_TYPE_F32;
                        max_align = 2;
                        break;
                    case 0x2B:
                        result_type = WASM_TYPE_F64;
                        max_align = 3;
                        break;
                    case 0x2C:
                    case 0x2D:
                        max_align = 0;
                        break;
                    case 0x2E:
                    case 0x2F:
                        max_align = 1;
                        break;
                    case 0x30:
                    case 0x31:
                        result_type = WASM_TYPE_I64;
                        max_align = 0;
                        break;
                    case 0x32:
                    case 0x33:
                        result_type = WASM_TYPE_I64;
                        max_align = 1;
                        break;
                    case 0x34:
                    case 0x35:
                        result_type = WASM_TYPE_I64;
                        max_align = 2;
                        break;
                }
                err = wasm__validator_read_memarg(&v, at, max_align);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_push(&v, at, result_type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x36:
            case 0x37:
            case 0x38:
            case 0x39:
            case 0x3A:
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E: {
                wasm_valtype_t value_type = WASM_TYPE_I32;
                uint32_t max_align = 0;
                if (mod->num_memories == 0)
                    return wasm__validator_error(&v, at, "memory store requires a memory");
                switch (op) {
                    case 0x36:
                        value_type = WASM_TYPE_I32;
                        max_align = 2;
                        break;
                    case 0x37:
                        value_type = WASM_TYPE_I64;
                        max_align = 3;
                        break;
                    case 0x38:
                        value_type = WASM_TYPE_F32;
                        max_align = 2;
                        break;
                    case 0x39:
                        value_type = WASM_TYPE_F64;
                        max_align = 3;
                        break;
                    case 0x3A:
                        value_type = WASM_TYPE_I32;
                        max_align = 0;
                        break;
                    case 0x3B:
                        value_type = WASM_TYPE_I32;
                        max_align = 1;
                        break;
                    case 0x3C:
                        value_type = WASM_TYPE_I64;
                        max_align = 0;
                        break;
                    case 0x3D:
                        value_type = WASM_TYPE_I64;
                        max_align = 1;
                        break;
                    case 0x3E:
                        value_type = WASM_TYPE_I64;
                        max_align = 2;
                        break;
                }
                err = wasm__validator_read_memarg(&v, at, max_align);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, value_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x3F: {
                uint32_t mem_idx = wasm__read_leb128_u32(&v.r);
                if (mod->num_memories == 0)
                    return wasm__validator_error(&v, at, "memory.size requires a memory");
                if (mem_idx != 0) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_MULTI_MEMORY);
                    if (err != WASM_OK) return err;
                }
                if (mem_idx >= mod->num_memories)
                    return wasm__validator_error(&v, at, "memory %u out of range", (unsigned)mem_idx);
                err = wasm__validator_push(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x40: {
                uint32_t mem_idx = wasm__read_leb128_u32(&v.r);
                if (mod->num_memories == 0)
                    return wasm__validator_error(&v, at, "memory.grow requires a memory");
                if (mem_idx != 0) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_MULTI_MEMORY);
                    if (err != WASM_OK) return err;
                }
                if (mem_idx >= mod->num_memories)
                    return wasm__validator_error(&v, at, "memory %u out of range", (unsigned)mem_idx);
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_push(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x41:
                (void)wasm__read_leb128_i32(&v.r);
                err = wasm__validator_push(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            case 0x42:
                (void)wasm__read_leb128_i64(&v.r);
                err = wasm__validator_push(&v, at, WASM_TYPE_I64);
                if (err != WASM_OK) return err;
                break;
            case 0x43:
                (void)wasm__read_f32(&v.r);
                err = wasm__validator_push(&v, at, WASM_TYPE_F32);
                if (err != WASM_OK) return err;
                break;
            case 0x44:
                (void)wasm__read_f64(&v.r);
                err = wasm__validator_push(&v, at, WASM_TYPE_F64);
                if (err != WASM_OK) return err;
                break;
            case 0x45:
            case 0x46:
            case 0x47:
            case 0x48:
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
            case 0x4D:
            case 0x4E:
            case 0x4F:
            case 0x50:
            case 0x51:
            case 0x52:
            case 0x53:
            case 0x54:
            case 0x55:
            case 0x56:
            case 0x57:
            case 0x58:
            case 0x59:
            case 0x5A:
            case 0x5B:
            case 0x5C:
            case 0x5D:
            case 0x5E:
            case 0x5F:
            case 0x60:
            case 0x61:
            case 0x62:
            case 0x63:
            case 0x64:
            case 0x65:
            case 0x66:
            case 0x67:
            case 0x68:
            case 0x69:
            case 0x6A:
            case 0x6B:
            case 0x6C:
            case 0x6D:
            case 0x6E:
            case 0x6F:
            case 0x70:
            case 0x71:
            case 0x72:
            case 0x73:
            case 0x74:
            case 0x75:
            case 0x76:
            case 0x77:
            case 0x78:
            case 0x79:
            case 0x7A:
            case 0x7B:
            case 0x7C:
            case 0x7D:
            case 0x7E:
            case 0x7F:
            case 0x80:
            case 0x81:
            case 0x82:
            case 0x83:
            case 0x84:
            case 0x85:
            case 0x86:
            case 0x87:
            case 0x88:
            case 0x89:
            case 0x8A:
            case 0x8B:
            case 0x8C:
            case 0x8D:
            case 0x8E:
            case 0x8F:
            case 0x90:
            case 0x91:
            case 0x92:
            case 0x93:
            case 0x94:
            case 0x95:
            case 0x96:
            case 0x97:
            case 0x98:
            case 0x99:
            case 0x9A:
            case 0x9B:
            case 0x9C:
            case 0x9D:
            case 0x9E:
            case 0x9F:
            case 0xA0:
            case 0xA1:
            case 0xA2:
            case 0xA3:
            case 0xA4:
            case 0xA5:
            case 0xA6:
            case 0xA7:
            case 0xA8:
            case 0xA9:
            case 0xAA:
            case 0xAB:
            case 0xAC:
            case 0xAD:
            case 0xAE:
            case 0xAF:
            case 0xB0:
            case 0xB1:
            case 0xB2:
            case 0xB3:
            case 0xB4:
            case 0xB5:
            case 0xB6:
            case 0xB7:
            case 0xB8:
            case 0xB9:
            case 0xBA:
            case 0xBB:
            case 0xBC:
            case 0xBD:
            case 0xBE:
            case 0xBF:
            case 0xC0:
            case 0xC1:
            case 0xC2:
            case 0xC3:
            case 0xC4:
                err = wasm__validator_validate_numeric(&v, at, op);
                if (err != WASM_OK) return err;
                break;
            case 0xD0: {
                wasm_valtype_t reftype;
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                err = wasm__read_reftype(&v.r, &reftype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "invalid reftype");
                err = wasm__validator_push(&v, at, reftype);
                if (err != WASM_OK) return err;
                break;
            }
            case 0xD1: {
                wasm_valtype_t ref_type;
                err = wasm__validator_pop_any(&v, at, &ref_type);
                if (err != WASM_OK) return err;
                if (ref_type != WASM__TYPE_BOT && !wasm__is_ref_type(ref_type))
                    return wasm__validator_error(&v, at, "ref.is_null requires a reference operand");
                err = wasm__validator_push(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            }
            case 0xD2: {
                uint32_t func_ref = wasm__read_leb128_u32(&v.r);
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (func_ref >= mod->num_funcs)
                    return wasm__validator_error(&v, at, "ref.func index %u out of range", (unsigned)func_ref);
                err = wasm__validator_push(&v, at, WASM_TYPE_FUNCREF);
                if (err != WASM_OK) return err;
                break;
            }
            case 0xFC:
                err = wasm__validator_validate_prefixed(&v, at);
                if (err != WASM_OK) return err;
                break;
            case 0xFD:
                err = wasm__validator_validate_simd(&v, at);
                if (err != WASM_OK) return err;
                break;
            default:
                return wasm__validator_error(&v, at, "unknown opcode 0x%02X", op);
        }

        if (v.r.malformed)
            return wasm__validator_error(&v, at, "malformed immediate");
    }

    if (v.fp != 0)
        return wasm__validator_error(&v, v.r.end, "function body ended before all blocks closed");
    if (v.sp != ft->num_results)
        return wasm__validator_error(&v, v.r.end, "function leaves %u stack values, expected %u",
                                     (unsigned)v.sp,
                                     (unsigned)ft->num_results);
    return WASM_OK;
}

static wasm_error_t wasm__validate_code(wasm_module_t* mod) {
    uint32_t i;
    for (i = mod->num_func_imports; i < mod->num_funcs; i++) {
        wasm_error_t err = wasm__validate_function(mod, i);
        if (err != WASM_OK) return err;
    }
    if (mod->uses_data_count_section && !mod->has_data_count_section) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "data count section is required when memory.init or data.drop is used");
    }
    return WASM_OK;
}

static wasm_error_t wasm__build_module_control_targets(wasm_module_t* mod);

/* ── Module loader ────────────────────────────────────────────────── */

wasm_module_t* wasm_load(wasm_runtime_t* rt, const uint8_t* bytes, size_t len) {
    wasm__reader_t r;
    uint32_t version;
    wasm_module_t* mod;
    wasm_error_t err = WASM_OK;

    r.ptr = bytes;
    r.end = bytes + len;
    r.malformed = 0;
    if (!wasm__has(&r, 8)) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "too short");
        return NULL;
    }
    if (r.ptr[0] != 0x00 || r.ptr[1] != 0x61 || r.ptr[2] != 0x73 || r.ptr[3] != 0x6D) {
        WASM__SET_ERR(rt, WASM_ERR_INVALID_MAGIC, "bad magic");
        return NULL;
    }
    r.ptr += 4;
    version = (uint32_t)r.ptr[0] | ((uint32_t)r.ptr[1] << 8) | ((uint32_t)r.ptr[2] << 16) | ((uint32_t)r.ptr[3] << 24);
    r.ptr += 4;
    if (version != 1) {
        WASM__SET_ERR(rt, WASM_ERR_INVALID_VERSION, "version %u", version);
        return NULL;
    }

    mod = (wasm_module_t*)WASM_CALLOC(1, sizeof(wasm_module_t));
    if (!mod) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "module alloc");
        return NULL;
    }
    mod->rt = rt;
    mod->start_func = UINT32_MAX;

    while (r.ptr < r.end && err == WASM_OK) {
        uint8_t sid = wasm__read_u8(&r);
        uint32_t ssz = wasm__read_leb128_u32(&r);
        const uint8_t* se;
        wasm__reader_t sr;

        if (r.malformed) {
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "malformed section header");
            err = WASM_ERR_MALFORMED;
            break;
        }

        if (!wasm__has(&r, ssz)) {
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "section %u overruns module", (unsigned)sid);
            err = WASM_ERR_MALFORMED;
            break;
        }
        se = r.ptr + ssz;
        sr.ptr = r.ptr;
        sr.end = se;
        sr.malformed = 0;
        switch (sid) {
            case 0:
                sr.ptr = sr.end;
                break;
            case 1:
                err = wasm__decode_type_section(mod, &sr);
                break;
            case 2:
                err = wasm__decode_import_section(mod, &sr);
                break;
            case 3:
                err = wasm__decode_function_section(mod, &sr);
                break;
            case 4:
                err = wasm__decode_table_section(mod, &sr);
                break;
            case 5:
                err = wasm__decode_memory_section(mod, &sr);
                break;
            case 13:
                err = wasm__decode_tag_section(mod, &sr);
                break;
            case 6:
                err = wasm__decode_global_section(mod, &sr);
                break;
            case 7:
                err = wasm__decode_export_section(mod, &sr);
                break;
            case 8:
                err = wasm__decode_start_section(mod, &sr);
                break;
            case 9:
                err = wasm__decode_element_section(mod, &sr);
                break;
            case 10:
                err = wasm__decode_code_section(mod, &sr);
                break;
            case 11:
                err = wasm__decode_data_section(mod, &sr);
                break;
            case 12:
                err = wasm__decode_data_count_section(mod, &sr);
                break;
            default:
                WASM__SET_ERR(rt, WASM_ERR_INVALID_SECTION, "unknown section id %u", (unsigned)sid);
                err = WASM_ERR_INVALID_SECTION;
                break;
        }
        if (err == WASM_OK && (sr.malformed || sr.ptr != sr.end)) {
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "section %u has trailing bytes", (unsigned)sid);
            err = WASM_ERR_MALFORMED;
        }
        r.ptr = se;
    }
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    err = wasm__validate_structural(mod);
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    err = wasm__validate_code(mod);
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    err = wasm__build_module_control_targets(mod);
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    if (mod->num_memories > 0) {
        uint32_t mi;

        for (mi = 0; mi < mod->num_memories; mi++) {
            err = wasm__init_memory_storage(&mod->memories[mi]);
            if (err != WASM_OK) {
                wasm_free_module(mod);
                return NULL;
            }
        }
    }
    if (mod->num_tables > 0) {
        uint32_t ti;
        for (ti = 0; ti < mod->num_tables; ti++) {
            err = wasm__init_table_storage(&mod->tables[ti]);
            if (err != WASM_OK) {
                wasm_free_module(mod);
                return NULL;
            }
        }
    }

    err = wasm__apply_active_data_segments(mod);
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    err = wasm__apply_active_elem_segments(mod);
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    err = wasm__run_startup(mod);
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    return mod;
}

void wasm_free_module(wasm_module_t* mod) {
    uint32_t i;
    if (!mod) return;
    for (i = 0; i < mod->num_types; i++) wasm__free_functype(&mod->types[i]);
    WASM_FREE(mod->types);
    for (i = 0; i < mod->num_funcs; i++) {
        WASM_FREE(mod->funcs[i].locals);
        WASM_FREE(mod->funcs[i].control_targets);
    }
    WASM_FREE(mod->funcs);
    WASM_FREE(mod->exports);
    WASM_FREE(mod->globals);
    WASM_FREE(mod->tags);
    for (i = 0; i < mod->num_data_segments; i++) wasm__free_data_segment(&mod->data_segments[i]);
    WASM_FREE(mod->data_segments);
    for (i = 0; i < mod->num_elem_segments; i++) wasm__free_elem_segment(&mod->elem_segments[i]);
    WASM_FREE(mod->elem_segments);
    for (i = 0; i < mod->num_tables; i++) wasm__free_table(&mod->tables[i]);
    WASM_FREE(mod->tables);
    for (i = 0; i < mod->num_memories; i++) wasm__free_memory(&mod->memories[i]);
    WASM_FREE(mod->memories);
    WASM_FREE(mod);
}

/* ── Interpreter types ────────────────────────────────────────────── */

typedef struct wasm__label_t {
    const uint8_t* continuation;
    const uint8_t* handler_ptr;
    uint32_t sp_base;
    uint32_t param_arity;
    uint32_t arity;
    uint32_t caught_tag;
    uint32_t caught_count;
    wasm_value_t* caught_values;
    uint32_t delegate_depth;
    uint8_t opcode;
    uint8_t active_catch;
} wasm__label_t;

typedef struct wasm__try_clause_t {
    uint8_t opcode;
    uint32_t immediate;
    const uint8_t* body_start;
    const uint8_t* next_clause;
} wasm__try_clause_t;

static void wasm__clear_label(wasm__label_t* label) {
    WASM_FREE(label->caught_values);
    label->caught_values = NULL;
    label->caught_count = 0;
    label->active_catch = 0;
    label->delegate_depth = UINT32_MAX;
}

static void wasm__skip_blocktype(wasm__reader_t* r) {
    uint8_t lead = *r->ptr;

    if (lead == 0x40 || wasm__is_valtype_byte(lead)) {
        r->ptr++;
        return;
    }

    (void)wasm__read_leb128_i32(r);
}

static void wasm__skip_prefixed_immediates(wasm__reader_t* r) {
    uint32_t subop = wasm__read_leb128_u32(r);

    switch (subop) {
        case 0x08:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x09:
            wasm__read_leb128_u32(r);
            break;
        case 0x0A:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0B:
            wasm__read_leb128_u32(r);
            break;
        case 0x0C:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0D:
            wasm__read_leb128_u32(r);
            break;
        case 0x0E:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0F:
        case 0x10:
        case 0x11:
            wasm__read_leb128_u32(r);
            break;
        default:
            break;
    }
}

static wasm_error_t wasm__read_blocktype(wasm_module_t* mod, wasm__reader_t* r, wasm__blocktype_t* blocktype) {
    uint8_t lead = *r->ptr;
    wasm_error_t err;

    blocktype->param_arity = 0;
    blocktype->result_arity = 0;
    blocktype->params = NULL;
    blocktype->results = NULL;
    blocktype->inline_result = WASM_TYPE_VOID;

    if (lead == 0x40) {
        r->ptr++;
        return WASM_OK;
    }

    if (wasm__is_valtype_byte(lead)) {
        r->ptr++;
        err = wasm__require_valtype_feature(mod, (wasm_valtype_t)lead);
        if (err != WASM_OK) return err;
        blocktype->result_arity = 1;
        blocktype->inline_result = (wasm_valtype_t)lead;
        blocktype->results = &blocktype->inline_result;
        return WASM_OK;
    }

    {
        int32_t type_idx = wasm__read_leb128_i32(r);
        wasm_functype_t* ft;

        if (wasm__require_feature(mod, WASM_FEATURE_MULTI_VALUE) != WASM_OK) return WASM_ERR_MALFORMED;
        if (type_idx < 0 || (uint32_t)type_idx >= mod->num_types) return WASM_ERR_MALFORMED;
        ft = &mod->types[(uint32_t)type_idx];
        blocktype->param_arity = ft->num_params;
        blocktype->result_arity = ft->num_results;
        blocktype->params = ft->params;
        blocktype->results = ft->results;
    }

    return WASM_OK;
}

/* ── Block scanning ───────────────────────────────────────────────── */

static void wasm__skip_immediates(uint8_t op, wasm__reader_t* r) {
    switch (op) {
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x06:
            wasm__skip_blocktype(r);
            break;
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0C:
        case 0x0D:
        case 0x10:
        case 0x18:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x41:
        case 0x3F:
        case 0x40:
            wasm__read_leb128_u32(r);
            break;
        case 0x42:
            wasm__read_leb128_i64(r);
            break;
        case 0x43:
            r->ptr += 4;
            break;
        case 0x44:
            r->ptr += 8;
            break;
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E: {
            wasm__memarg_t memarg;
            if (wasm__read_memarg(r, &memarg) != WASM_OK) {
                r->ptr = r->end;
                return;
            }
        } break;
        case 0x0E: {
            uint32_t n = wasm__read_leb128_u32(r), j;
            for (j = 0; j <= n; j++) wasm__read_leb128_u32(r);
            break;
        }
        case 0x11:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x1C: {
            uint32_t count = wasm__read_leb128_u32(r);
            while (count-- > 0 && r->ptr < r->end) wasm__read_u8(r);
            break;
        }
        case 0xD0:
            wasm__read_u8(r);
            break;
        case 0xD2:
            wasm__read_leb128_u32(r);
            break;
        case 0xFC:
            wasm__skip_prefixed_immediates(r);
            break;
        case 0xFD:
            wasm__skip_simd_immediates(r);
            break;
        case 0x19:
        default:
            break;
    }
}

static const uint8_t* wasm__find_try_clause_boundary(const uint8_t* start, const uint8_t* code_end) {
    wasm__reader_t scan;
    int nest = 1;

    scan.ptr = start;
    scan.end = code_end;
    scan.malformed = 0;
    while (scan.ptr < scan.end) {
        const uint8_t* at = scan.ptr;
        uint8_t b = wasm__read_u8(&scan);

        if (scan.malformed) return code_end;

        if (b == 0x02 || b == 0x03 || b == 0x04 || b == 0x06) {
            nest++;
            wasm__skip_immediates(b, &scan);
        } else if ((b == 0x07 || b == 0x18 || b == 0x19) && nest == 1) {
            return at;
        } else if (b == 0x0B) {
            nest--;
            if (nest == 0) return at;
        } else {
            wasm__skip_immediates(b, &scan);
        }
    }

    return code_end;
}

typedef struct wasm__control_frame_t {
    uint8_t opcode;
    uint32_t offset;
} wasm__control_frame_t;

static wasm_error_t wasm__build_control_targets(wasm_func_t* func) {
    wasm__reader_t scan;
    wasm__control_target_t* targets;
    wasm__control_frame_t stack[WASM__MAX_LABELS];
    uint32_t depth = 0;
    uint32_t i;

    if (!func->code || func->code_len == 0) return WASM_OK;

    targets = (wasm__control_target_t*)WASM_MALLOC(func->code_len * sizeof(wasm__control_target_t));
    if (!targets) return WASM_ERR_OOM;

    for (i = 0; i < func->code_len; i++) {
        targets[i].end_offset = UINT32_MAX;
        targets[i].aux_offset = UINT32_MAX;
    }

    scan.ptr = func->code;
    scan.end = func->code + func->code_len;
    scan.malformed = 0;
    while (scan.ptr < scan.end) {
        const uint8_t* at = scan.ptr;
        uint8_t op = wasm__read_u8(&scan);

        if (scan.malformed) {
            WASM_FREE(targets);
            return WASM_ERR_MALFORMED;
        }

        switch (op) {
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x06:
                if (depth >= WASM__MAX_LABELS) {
                    WASM_FREE(targets);
                    return WASM_ERR_MALFORMED;
                }
                stack[depth].opcode = op;
                stack[depth].offset = (uint32_t)(at - func->code);
                wasm__skip_immediates(op, &scan);
                depth++;
                break;
            case 0x05:
                if (depth > 0 && stack[depth - 1].opcode == 0x04)
                    targets[stack[depth - 1].offset].aux_offset = (uint32_t)(scan.ptr - func->code);
                break;
            case 0x07:
            case 0x18:
            case 0x19:
                if (depth > 0 && stack[depth - 1].opcode == 0x06 &&
                    targets[stack[depth - 1].offset].aux_offset == UINT32_MAX)
                    targets[stack[depth - 1].offset].aux_offset = (uint32_t)(at - func->code);
                wasm__skip_immediates(op, &scan);
                break;
            case 0x0B:
                if (depth == 0) {
                    if (scan.ptr != scan.end) {
                        WASM_FREE(targets);
                        return WASM_ERR_MALFORMED;
                    }
                    break;
                }
                depth--;
                targets[stack[depth].offset].end_offset = (uint32_t)(scan.ptr - func->code);
                break;
            default:
                wasm__skip_immediates(op, &scan);
                break;
        }

        if (scan.malformed) {
            WASM_FREE(targets);
            return WASM_ERR_MALFORMED;
        }
    }

    if (depth != 0) {
        WASM_FREE(targets);
        return WASM_ERR_MALFORMED;
    }

    func->control_targets = targets;
    return WASM_OK;
}

static const wasm__control_target_t* wasm__control_target_at(const wasm_func_t* func, const uint8_t* pc) {
    uint32_t offset;

    if (!func || !func->control_targets || !func->code || pc < func->code) return NULL;
    offset = (uint32_t)(pc - func->code);
    if (offset >= func->code_len) return NULL;
    if (func->control_targets[offset].end_offset == UINT32_MAX) return NULL;
    return &func->control_targets[offset];
}

static wasm_error_t wasm__build_module_control_targets(wasm_module_t* mod) {
    uint32_t i;

    for (i = mod->num_func_imports; i < mod->num_funcs; i++) {
        wasm_error_t err = wasm__build_control_targets(&mod->funcs[i]);
        if (err != WASM_OK) return err;
    }

    return WASM_OK;
}

static wasm_error_t wasm__read_try_clause(const uint8_t* clause_ptr,
                                          const uint8_t* code_end,
                                          wasm__try_clause_t* out_clause) {
    wasm__reader_t r;

    if (!clause_ptr || clause_ptr >= code_end) return WASM_ERR_MALFORMED;

    r.ptr = clause_ptr;
    r.end = code_end;
    r.malformed = 0;
    out_clause->opcode = wasm__read_u8(&r);
    out_clause->immediate = UINT32_MAX;

    switch (out_clause->opcode) {
        case 0x07:
        case 0x18:
            out_clause->immediate = wasm__read_leb128_u32(&r);
            break;
        case 0x19:
            break;
        default:
            return WASM_ERR_MALFORMED;
    }

    if (r.malformed) return WASM_ERR_MALFORMED;

    out_clause->body_start = r.ptr;
    out_clause->next_clause = wasm__find_try_clause_boundary(out_clause->body_start, code_end);
    return WASM_OK;
}

/* ── Branch helper ────────────────────────────────────────────────── */

static wasm_error_t wasm__copy_exception_to_label(wasm__label_t* label,
                                                  const wasm__exception_state_t* ex) {
    wasm_value_t* copy = NULL;

    wasm__clear_label(label);
    if (ex->num_values > 0) {
        copy = (wasm_value_t*)WASM_MALLOC(ex->num_values * sizeof(wasm_value_t));
        if (!copy) return WASM_ERR_OOM;
        memcpy(copy, ex->values, ex->num_values * sizeof(wasm_value_t));
    }

    label->caught_tag = ex->tag_index;
    label->caught_values = copy;
    label->caught_count = ex->num_values;
    label->active_catch = 1;
    return WASM_OK;
}

static wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt) {
    WASM__SET_ERR(rt, WASM_ERR_TRAP, "uncaught exception tag %u", (unsigned)rt->pending_exception.tag_index);
    wasm__free_exception_state(&rt->pending_exception);
    return WASM_ERR_TRAP;
}

static int wasm__find_rethrow_label(const wasm__label_t* labels,
                                    uint32_t lsp,
                                    uint32_t depth,
                                    uint32_t* out_index) {
    uint32_t i;

    for (i = lsp; i > 0; i--) {
        const wasm__label_t* label = &labels[i - 1];

        if (label->opcode == 0x06 && label->active_catch) {
            if (depth == 0) {
                *out_index = i - 1;
                return 1;
            }
            depth--;
        }
    }

    return 0;
}

static wasm_error_t wasm__handle_exception(wasm_module_t* mod,
                                           wasm__label_t* labels,
                                           uint32_t* label_sp,
                                           wasm__reader_t* r,
                                           uint32_t sp_base) {
    wasm_runtime_t* rt = mod->rt;
    uint32_t idx = *label_sp;

    while (idx > 0) {
        wasm__label_t* label = &labels[idx - 1];
        int delegated = 0;

        if (label->opcode == 0x06 && label->handler_ptr) {
            const uint8_t* clause_ptr = label->handler_ptr;

            while (clause_ptr && clause_ptr < label->continuation) {
                wasm__try_clause_t clause;

                if (wasm__read_try_clause(clause_ptr, label->continuation, &clause) != WASM_OK)
                    return WASM_ERR_MALFORMED;

                if (clause.opcode == 0x07 && clause.immediate == rt->pending_exception.tag_index) {
                    wasm_error_t err;
                    uint32_t i;

                    while (*label_sp > idx) {
                        wasm__clear_label(&labels[*label_sp - 1]);
                        rt->sp = labels[*label_sp - 1].sp_base;
                        (*label_sp)--;
                    }
                    rt->sp = label->sp_base;
                    err = wasm__copy_exception_to_label(label, &rt->pending_exception);
                    if (err != WASM_OK) return err;
                    if (rt->sp + label->caught_count > WASM_MAX_STACK) return WASM_ERR_STACK_OVERFLOW;
                    for (i = 0; i < label->caught_count; i++) rt->stack[rt->sp++] = label->caught_values[i];
                    wasm__free_exception_state(&rt->pending_exception);
                    *label_sp = idx;
                    r->ptr = clause.body_start;
                    return WASM_OK;
                }

                if (clause.opcode == 0x19) {
                    wasm_error_t err;

                    while (*label_sp > idx) {
                        wasm__clear_label(&labels[*label_sp - 1]);
                        rt->sp = labels[*label_sp - 1].sp_base;
                        (*label_sp)--;
                    }
                    rt->sp = label->sp_base;
                    err = wasm__copy_exception_to_label(label, &rt->pending_exception);
                    if (err != WASM_OK) return err;
                    wasm__free_exception_state(&rt->pending_exception);
                    *label_sp = idx;
                    r->ptr = clause.body_start;
                    return WASM_OK;
                }

                if (clause.opcode == 0x18) {
                    uint32_t extra = clause.immediate;

                    wasm__clear_label(label);
                    rt->sp = label->sp_base;
                    idx--;
                    while (extra > 0 && idx > 0) {
                        wasm__clear_label(&labels[idx - 1]);
                        rt->sp = labels[idx - 1].sp_base;
                        idx--;
                        extra--;
                    }
                    delegated = 1;
                    break;
                }

                if (clause.next_clause >= label->continuation || *clause.next_clause == 0x0B) break;
                clause_ptr = clause.next_clause;
            }
        }

        if (delegated) continue;

        wasm__clear_label(label);
        rt->sp = label->sp_base;
        idx--;
    }

    *label_sp = 0;
    rt->sp = sp_base;
    return (wasm_error_t)-1;
}

static wasm_error_t wasm__do_branch(wasm_runtime_t* rt,
                                    wasm__label_t* labels,
                                    wasm__label_t* target,
                                    wasm__reader_t* r,
                                    uint32_t* label_sp,
                                    uint32_t depth_l) {
    wasm_value_t branch_values_buf[8];
    wasm_value_t* branch_values = branch_values_buf;
    uint32_t branch_arity = (target->opcode == 0x03) ? target->param_arity : target->arity;
    uint32_t pop_count = (target->opcode == 0x03) ? depth_l : depth_l + 1;
    uint32_t i;

    if (branch_arity > 8) {
        branch_values = (wasm_value_t*)WASM_MALLOC(branch_arity * sizeof(wasm_value_t));
        if (!branch_values) return WASM_ERR_OOM;
    }

    for (i = 0; i < branch_arity; i++) branch_values[i] = rt->stack[rt->sp - branch_arity + i];
    rt->sp = target->sp_base;
    for (i = 0; i < branch_arity; i++) rt->stack[rt->sp++] = branch_values[i];
    r->ptr = target->continuation;

    if (branch_values != branch_values_buf) WASM_FREE(branch_values);
    for (i = 0; i < pop_count; i++) wasm__clear_label(&labels[*label_sp - 1 - i]);
    *label_sp -= pop_count;

    return WASM_OK;
}

static wasm_error_t wasm__leave_label(wasm_runtime_t* rt, const wasm__label_t* label) {
    wasm_value_t result_values_buf[8];
    wasm_value_t* result_values = result_values_buf;
    uint32_t i;

    if (label->arity > 8) {
        result_values = (wasm_value_t*)WASM_MALLOC(label->arity * sizeof(wasm_value_t));
        if (!result_values) return WASM_ERR_OOM;
    }

    for (i = 0; i < label->arity; i++) result_values[i] = rt->stack[rt->sp - label->arity + i];
    rt->sp = label->sp_base;
    for (i = 0; i < label->arity; i++) rt->stack[rt->sp++] = result_values[i];

    if (result_values != result_values_buf) WASM_FREE(result_values);
    return WASM_OK;
}

/* ── Interpreter ──────────────────────────────────────────────────── */

#define WASM__ERR_EXCEPTION ((wasm_error_t) - 1)

#define WASM__PUSH(rt, val)                \
    do {                                   \
        if ((rt)->sp >= WASM_MAX_STACK) {  \
            err = WASM_ERR_STACK_OVERFLOW; \
            goto cleanup;                  \
        }                                  \
        (rt)->stack[(rt)->sp++] = (val);   \
    } while (0)
#define WASM__POP(rt) ((rt)->stack[--(rt)->sp])
#define WASM__PEEK(rt) ((rt)->stack[(rt)->sp - 1])
#define WASM__TRAP(code) \
    do {                 \
        err = (code);    \
        goto cleanup;    \
    } while (0)

static wasm_error_t wasm__exec(wasm_module_t* mod, uint32_t func_idx,
                               wasm_value_t* args, uint32_t num_args,
                               wasm_value_t* results, uint32_t num_results,
                               uint32_t depth);

static wasm_error_t wasm__exec(wasm_module_t* mod, uint32_t func_idx,
                               wasm_value_t* args, uint32_t num_args,
                               wasm_value_t* results, uint32_t num_results,
                               uint32_t depth) {
    wasm_runtime_t* rt = mod->rt;
    wasm_func_t* func = NULL;
    wasm_functype_t* ft = NULL;
    wasm_value_t locals_buf[256];
    wasm_value_t tail_args_buf[8];
    wasm_value_t* locals = NULL;
    wasm_value_t* current_args = args;
    uint32_t current_num_args = num_args;
    uint32_t current_func_idx = func_idx;
    uint32_t sp_base = 0, i;
    int current_args_owned = 0;
    int restart = 0;
    wasm__reader_t r;
    wasm__label_t labels[WASM__MAX_LABELS];
    uint32_t lsp = 0;
    wasm_error_t err = WASM_OK;

    for (;;) {
        restart = 0;
        if (depth >= WASM_MAX_CALL_DEPTH) {
            err = WASM_ERR_CALL_STACK_EXHAUSTED;
            goto cleanup;
        }
        if (current_func_idx >= mod->num_funcs) {
            err = WASM_ERR_MALFORMED;
            goto cleanup;
        }
        func = &mod->funcs[current_func_idx];
        ft = &mod->types[func->type_idx];

        if (func->is_import) {
            err = func->host_func(rt, current_args, current_num_args, results, num_results, func->host_userdata);
            goto cleanup;
        }

        locals = (func->num_locals <= 256) ? locals_buf : (wasm_value_t*)WASM_MALLOC(func->num_locals * sizeof(wasm_value_t));
        if (!locals) {
            err = WASM_ERR_OOM;
            goto cleanup;
        }
        for (i = 0; i < func->num_locals; i++) locals[i] = wasm__default_value(func->locals[i]);
        for (i = 0; i < ft->num_params && i < current_num_args; i++) {
            locals[i] = current_args[i];
            locals[i].type = ft->params[i];
        }
        if (current_args_owned) {
            WASM_FREE(current_args);
            current_args = NULL;
            current_args_owned = 0;
        }

        sp_base = rt->sp;
        r.ptr = func->code;
        r.end = func->code + func->code_len;
        r.malformed = 0;
        lsp = 0;

        /* Implicit function-level label */
        labels[lsp].continuation = r.end;
        labels[lsp].handler_ptr = NULL;
        labels[lsp].sp_base = sp_base;
        labels[lsp].param_arity = 0;
        labels[lsp].arity = ft->num_results;
        labels[lsp].caught_tag = UINT32_MAX;
        labels[lsp].caught_count = 0;
        labels[lsp].caught_values = NULL;
        labels[lsp].delegate_depth = UINT32_MAX;
        labels[lsp].opcode = 0x02;
        labels[lsp].active_catch = 0;
        lsp++;

        while (r.ptr < r.end) {
            const uint8_t* op_at = r.ptr;
            uint8_t op = wasm__read_u8(&r);
            switch (op) {
                case 0x00:
                    WASM__TRAP(WASM_ERR_UNREACHABLE);
                case 0x01:
                    break;

                case 0x02: { /* block */
                    wasm__blocktype_t blocktype;
                    err = wasm__read_blocktype(mod, &r, &blocktype);
                    if (err != WASM_OK) goto cleanup;
                    {
                        const wasm__control_target_t* target = wasm__control_target_at(func, op_at);
                        const uint8_t* be;

                        if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                        be = func->code + target->end_offset;
                        if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                        if (lsp >= WASM__MAX_LABELS) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                        labels[lsp].continuation = be;
                        labels[lsp].handler_ptr = NULL;
                        labels[lsp].sp_base = rt->sp - blocktype.param_arity;
                        labels[lsp].param_arity = blocktype.param_arity;
                        labels[lsp].arity = blocktype.result_arity;
                        labels[lsp].caught_tag = UINT32_MAX;
                        labels[lsp].caught_count = 0;
                        labels[lsp].caught_values = NULL;
                        labels[lsp].delegate_depth = UINT32_MAX;
                        labels[lsp].opcode = 0x02;
                        labels[lsp].active_catch = 0;
                        lsp++;
                    }
                    break;
                }
                case 0x03: { /* loop */
                    wasm__blocktype_t blocktype;
                    const uint8_t* ls;
                    err = wasm__read_blocktype(mod, &r, &blocktype);
                    if (err != WASM_OK) goto cleanup;
                    if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                    ls = r.ptr;
                    if (lsp >= WASM__MAX_LABELS) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                    labels[lsp].continuation = ls;
                    labels[lsp].handler_ptr = NULL;
                    labels[lsp].sp_base = rt->sp - blocktype.param_arity;
                    labels[lsp].param_arity = blocktype.param_arity;
                    labels[lsp].arity = blocktype.result_arity;
                    labels[lsp].caught_tag = UINT32_MAX;
                    labels[lsp].caught_count = 0;
                    labels[lsp].caught_values = NULL;
                    labels[lsp].delegate_depth = UINT32_MAX;
                    labels[lsp].opcode = 0x03;
                    labels[lsp].active_catch = 0;
                    lsp++;
                    break;
                }
                case 0x04: { /* if */
                    wasm__blocktype_t blocktype;
                    wasm_value_t cond = WASM__POP(rt);
                    err = wasm__read_blocktype(mod, &r, &blocktype);
                    if (err != WASM_OK) goto cleanup;
                    {
                        const wasm__control_target_t* target = wasm__control_target_at(func, op_at);
                        const uint8_t* ep = NULL;
                        const uint8_t* be;

                        if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                        be = func->code + target->end_offset;
                        if (target->aux_offset != UINT32_MAX)
                            ep = func->code + target->aux_offset;
                        if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                        if (!cond.of.i32) {
                            if (!ep) {
                                r.ptr = be;
                                break;
                            }
                            r.ptr = ep;
                        }
                        if (lsp >= WASM__MAX_LABELS) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                        labels[lsp].continuation = be;
                        labels[lsp].handler_ptr = NULL;
                        labels[lsp].sp_base = rt->sp - blocktype.param_arity;
                        labels[lsp].param_arity = blocktype.param_arity;
                        labels[lsp].arity = blocktype.result_arity;
                        labels[lsp].caught_tag = UINT32_MAX;
                        labels[lsp].caught_count = 0;
                        labels[lsp].caught_values = NULL;
                        labels[lsp].delegate_depth = UINT32_MAX;
                        labels[lsp].opcode = 0x04;
                        labels[lsp].active_catch = 0;
                        lsp++;
                    }
                    break;
                }
                case 0x06: { /* try */
                    wasm__blocktype_t blocktype;
                    const wasm__control_target_t* target;
                    const uint8_t* be;
                    const uint8_t* first_clause;

                    err = wasm__read_blocktype(mod, &r, &blocktype);
                    if (err != WASM_OK) goto cleanup;
                    target = wasm__control_target_at(func, op_at);
                    if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                    be = func->code + target->end_offset;
                    first_clause = (target->aux_offset == UINT32_MAX) ? NULL : (func->code + target->aux_offset);
                    if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                    if (lsp >= WASM__MAX_LABELS) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                    labels[lsp].continuation = be;
                    labels[lsp].handler_ptr = first_clause;
                    labels[lsp].sp_base = rt->sp - blocktype.param_arity;
                    labels[lsp].param_arity = blocktype.param_arity;
                    labels[lsp].arity = blocktype.result_arity;
                    labels[lsp].caught_tag = UINT32_MAX;
                    labels[lsp].caught_count = 0;
                    labels[lsp].caught_values = NULL;
                    labels[lsp].delegate_depth = UINT32_MAX;
                    labels[lsp].opcode = 0x06;
                    labels[lsp].active_catch = 0;
                    lsp++;
                    break;
                }
                case 0x05:
                    if (lsp > 0) {
                        err = wasm__leave_label(rt, &labels[lsp - 1]);
                        if (err != WASM_OK) goto cleanup;
                        wasm__clear_label(&labels[lsp - 1]);
                        r.ptr = labels[lsp - 1].continuation;
                        lsp--;
                    }
                    break;
                case 0x07:
                case 0x18:
                case 0x19:
                    if (lsp > 0 && labels[lsp - 1].opcode == 0x06) {
                        err = wasm__leave_label(rt, &labels[lsp - 1]);
                        if (err != WASM_OK) goto cleanup;
                        wasm__clear_label(&labels[lsp - 1]);
                        r.ptr = labels[lsp - 1].continuation;
                        lsp--;
                    } else {
                        err = WASM_ERR_MALFORMED;
                        goto cleanup;
                    }
                    break;
                case 0x08: { /* throw */
                    uint32_t tag_index = wasm__read_leb128_u32(&r);
                    wasm_functype_t* tag_type;
                    wasm_value_t payload_buf[8];
                    wasm_value_t* payload = payload_buf;
                    int j;

                    if (tag_index >= mod->num_tags) WASM__TRAP(WASM_ERR_MALFORMED);
                    tag_type = &mod->types[mod->tags[tag_index].type_idx];
                    if (tag_type->num_params > 8) {
                        payload = (wasm_value_t*)WASM_MALLOC(tag_type->num_params * sizeof(wasm_value_t));
                        if (!payload) WASM__TRAP(WASM_ERR_OOM);
                    }
                    for (j = (int)tag_type->num_params - 1; j >= 0; j--) payload[j] = WASM__POP(rt);
                    err = wasm__set_exception_state(&rt->pending_exception, tag_index, payload, tag_type->num_params);
                    if (payload != payload_buf) WASM_FREE(payload);
                    if (err != WASM_OK) goto cleanup;
                    err = wasm__handle_exception(mod, labels, &lsp, &r, sp_base);
                    if (err == WASM_OK) break;
                    if (err == WASM__ERR_EXCEPTION) goto cleanup_frame;
                    goto cleanup;
                }
                case 0x09: { /* rethrow */
                    uint32_t depth_index = wasm__read_leb128_u32(&r);
                    uint32_t target_index;

                    if (!wasm__find_rethrow_label(labels, lsp, depth_index, &target_index)) WASM__TRAP(WASM_ERR_MALFORMED);
                    err = wasm__set_exception_state(&rt->pending_exception,
                                                    labels[target_index].caught_tag,
                                                    labels[target_index].caught_values,
                                                    labels[target_index].caught_count);
                    if (err != WASM_OK) goto cleanup;
                    while (lsp > target_index) {
                        wasm__clear_label(&labels[lsp - 1]);
                        rt->sp = labels[lsp - 1].sp_base;
                        lsp--;
                    }
                    err = wasm__handle_exception(mod, labels, &lsp, &r, sp_base);
                    if (err == WASM_OK) break;
                    if (err == WASM__ERR_EXCEPTION) goto cleanup_frame;
                    goto cleanup;
                }
                case 0x0B:
                    if (lsp > 0) {
                        err = wasm__leave_label(rt, &labels[lsp - 1]);
                        if (err != WASM_OK) goto cleanup;
                        wasm__clear_label(&labels[lsp - 1]);
                        lsp--;
                    }
                    break;

                case 0x0C: {
                    uint32_t d = wasm__read_leb128_u32(&r);
                    if (d >= lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                    err = wasm__do_branch(rt, labels, &labels[lsp - 1 - d], &r, &lsp, d);
                    if (err != WASM_OK) goto cleanup;
                    break;
                }
                case 0x0D: {
                    uint32_t d = wasm__read_leb128_u32(&r);
                    wasm_value_t c = WASM__POP(rt);
                    if (c.of.i32) {
                        if (d >= lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                        err = wasm__do_branch(rt, labels, &labels[lsp - 1 - d], &r, &lsp, d);
                        if (err != WASM_OK) goto cleanup;
                    }
                    break;
                }
                case 0x0E: { /* br_table */
                    uint32_t nt = wasm__read_leb128_u32(&r), j, d;
                    uint32_t* tgts = (uint32_t*)WASM_MALLOC((nt + 1) * sizeof(uint32_t));
                    wasm_value_t iv;
                    if (!tgts) WASM__TRAP(WASM_ERR_OOM);
                    for (j = 0; j <= nt; j++) tgts[j] = wasm__read_leb128_u32(&r);
                    iv = WASM__POP(rt);
                    d = ((uint32_t)iv.of.i32 < nt) ? tgts[iv.of.i32] : tgts[nt];
                    WASM_FREE(tgts);
                    if (d >= lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                    err = wasm__do_branch(rt, labels, &labels[lsp - 1 - d], &r, &lsp, d);
                    if (err != WASM_OK) goto cleanup;
                    break;
                }
                case 0x0F: /* return */
                    for (i = 0; i < ft->num_results; i++) results[ft->num_results - 1 - i] = WASM__POP(rt);
                    rt->sp = sp_base;
                    err = WASM_OK;
                    goto cleanup_frame;

                case 0x10: { /* call */
                    uint32_t ci = wasm__read_leb128_u32(&r);
                    wasm_functype_t* cft = &mod->types[mod->funcs[ci].type_idx];
                    wasm_value_t call_args_buf[8], call_results_buf[8];
                    wasm_value_t* call_args = call_args_buf;
                    wasm_value_t* call_results = call_results_buf;
                    int j;

                    if (cft->num_params > 8) {
                        call_args = (wasm_value_t*)WASM_MALLOC(cft->num_params * sizeof(wasm_value_t));
                        if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                    }
                    if (cft->num_results > 8) {
                        call_results = (wasm_value_t*)WASM_MALLOC(cft->num_results * sizeof(wasm_value_t));
                        if (!call_results) {
                            if (call_args != call_args_buf) WASM_FREE(call_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                    for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);
                    err = wasm__exec(mod, ci, call_args, cft->num_params, call_results, cft->num_results, depth + 1);
                    if (call_args != call_args_buf) WASM_FREE(call_args);
                    if (err != WASM_OK) {
                        if (call_results != call_results_buf) WASM_FREE(call_results);
                        if (err == WASM__ERR_EXCEPTION) {
                            err = wasm__handle_exception(mod, labels, &lsp, &r, sp_base);
                            if (err == WASM_OK) break;
                        }
                        goto cleanup;
                    }
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                    if (call_results != call_results_buf) WASM_FREE(call_results);
                    break;
                }
                case 0x11: { /* call_indirect */
                    uint32_t ti = wasm__read_leb128_u32(&r), table_idx, tvi, ci;
                    wasm_functype_t* actual_type;
                    wasm_functype_t* cft;
                    wasm_table_t* table;
                    wasm_value_t call_args_buf[8], call_results_buf[8], iv;
                    wasm_value_t* call_args = call_args_buf;
                    wasm_value_t* call_results = call_results_buf;
                    int j;
                    table_idx = wasm__read_leb128_u32(&r);
                    iv = WASM__POP(rt);
                    tvi = (uint32_t)iv.of.i32;
                    if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                    table = &mod->tables[table_idx];
                    if (table->reftype != WASM_TYPE_FUNCREF) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                    if (tvi >= table->size || !table->elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                    if (wasm__is_null_ref(&table->elems[tvi])) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                    ci = table->elems[tvi].of.funcref;
                    if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                    actual_type = &mod->types[mod->funcs[ci].type_idx];
                    if (!wasm__functype_equal(actual_type, &mod->types[ti]))
                        WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                    cft = &mod->types[ti];
                    if (cft->num_params > 8) {
                        call_args = (wasm_value_t*)WASM_MALLOC(cft->num_params * sizeof(wasm_value_t));
                        if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                    }
                    if (cft->num_results > 8) {
                        call_results = (wasm_value_t*)WASM_MALLOC(cft->num_results * sizeof(wasm_value_t));
                        if (!call_results) {
                            if (call_args != call_args_buf) WASM_FREE(call_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                    for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);
                    err = wasm__exec(mod, ci, call_args, cft->num_params, call_results, cft->num_results, depth + 1);
                    if (call_args != call_args_buf) WASM_FREE(call_args);
                    if (err != WASM_OK) {
                        if (call_results != call_results_buf) WASM_FREE(call_results);
                        if (err == WASM__ERR_EXCEPTION) {
                            err = wasm__handle_exception(mod, labels, &lsp, &r, sp_base);
                            if (err == WASM_OK) break;
                        }
                        goto cleanup;
                    }
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                    if (call_results != call_results_buf) WASM_FREE(call_results);
                    break;
                }
                case 0x12: { /* return_call */
                    uint32_t ci = wasm__read_leb128_u32(&r);
                    wasm_functype_t* cft;
                    wasm_value_t* tail_args = tail_args_buf;
                    int j;

                    if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                    cft = &mod->types[mod->funcs[ci].type_idx];
                    if (cft->num_params > 8) {
                        tail_args = (wasm_value_t*)WASM_MALLOC(cft->num_params * sizeof(wasm_value_t));
                        if (!tail_args) WASM__TRAP(WASM_ERR_OOM);
                    }
                    for (j = (int)cft->num_params - 1; j >= 0; j--) tail_args[j] = WASM__POP(rt);

                    rt->sp = sp_base;
                    current_func_idx = ci;
                    current_args = tail_args;
                    current_num_args = cft->num_params;
                    current_args_owned = (tail_args != tail_args_buf);
                    restart = 1;
                    err = WASM_OK;
                    goto cleanup_frame;
                }
                case 0x13: { /* return_call_indirect */
                    uint32_t ti = wasm__read_leb128_u32(&r), table_idx, tvi, ci;
                    wasm_functype_t* actual_type;
                    wasm_functype_t* cft;
                    wasm_table_t* table;
                    wasm_value_t* tail_args = tail_args_buf;
                    wasm_value_t iv;
                    int j;

                    table_idx = wasm__read_leb128_u32(&r);
                    iv = WASM__POP(rt);
                    tvi = (uint32_t)iv.of.i32;
                    if (ti >= mod->num_types || table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                    table = &mod->tables[table_idx];
                    if (table->reftype != WASM_TYPE_FUNCREF) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                    if (tvi >= table->size || !table->elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                    if (wasm__is_null_ref(&table->elems[tvi])) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                    ci = table->elems[tvi].of.funcref;
                    if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                    actual_type = &mod->types[mod->funcs[ci].type_idx];
                    if (!wasm__functype_equal(actual_type, &mod->types[ti]))
                        WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);

                    cft = &mod->types[ti];
                    if (cft->num_params > 8) {
                        tail_args = (wasm_value_t*)WASM_MALLOC(cft->num_params * sizeof(wasm_value_t));
                        if (!tail_args) WASM__TRAP(WASM_ERR_OOM);
                    }
                    for (j = (int)cft->num_params - 1; j >= 0; j--) tail_args[j] = WASM__POP(rt);

                    rt->sp = sp_base;
                    current_func_idx = ci;
                    current_args = tail_args;
                    current_num_args = cft->num_params;
                    current_args_owned = (tail_args != tail_args_buf);
                    restart = 1;
                    err = WASM_OK;
                    goto cleanup_frame;
                }

                case 0xFC: {
                    uint32_t subop = wasm__read_leb128_u32(&r);
                    switch (subop) {
                        case 0x00: {
                            wasm_value_t a = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__trunc_sat_i32_s((double)a.of.f32));
                            break;
                        }
                        case 0x01: {
                            wasm_value_t a = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__trunc_sat_i32_u((double)a.of.f32));
                            break;
                        }
                        case 0x02: {
                            wasm_value_t a = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__trunc_sat_i32_s(a.of.f64));
                            break;
                        }
                        case 0x03: {
                            wasm_value_t a = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__trunc_sat_i32_u(a.of.f64));
                            break;
                        }
                        case 0x04: {
                            wasm_value_t a = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__trunc_sat_i64_s((double)a.of.f32));
                            break;
                        }
                        case 0x05: {
                            wasm_value_t a = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__trunc_sat_i64_u((double)a.of.f32));
                            break;
                        }
                        case 0x06: {
                            wasm_value_t a = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__trunc_sat_i64_s(a.of.f64));
                            break;
                        }
                        case 0x07: {
                            wasm_value_t a = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__trunc_sat_i64_u(a.of.f64));
                            break;
                        }
                        case 0x08: {
                            uint32_t data_idx = wasm__read_leb128_u32(&r);
                            uint32_t mem_idx = wasm__read_leb128_u32(&r);
                            wasm_value_t count = WASM__POP(rt);
                            wasm_value_t src = WASM__POP(rt);
                            wasm_value_t dst = WASM__POP(rt);
                            uint32_t len = (uint32_t)count.of.i32;
                            uint32_t src_offset = (uint32_t)src.of.i32;
                            uint32_t dst_offset = (uint32_t)dst.of.i32;
                            wasm_memory_t* memory;
                            uint64_t mem_size;
                            wasm_data_segment_t* segment;

                            if (data_idx >= mod->num_data_segments) {
                                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown data segment %u", (unsigned)data_idx);
                                err = WASM_ERR_MALFORMED;
                                goto cleanup;
                            }
                            memory = wasm__memory_at(mod, mem_idx);
                            if (!memory) {
                                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown memory %u", (unsigned)mem_idx);
                                err = WASM_ERR_MALFORMED;
                                goto cleanup;
                            }
                            mem_size = (uint64_t)memory->pages * WASM_PAGE_SIZE;

                            segment = &mod->data_segments[data_idx];
                            if (!segment->is_passive || segment->is_dropped) WASM__TRAP(WASM_ERR_TRAP);
                            if (!wasm__range_in_bounds_u64(src_offset, len, segment->size) ||
                                !wasm__range_in_bounds_u64(dst_offset, len, mem_size))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (len > 0)
                                memcpy(memory->data + dst_offset, segment->bytes + src_offset, len);
                            break;
                        }
                        case 0x09: {
                            uint32_t data_idx = wasm__read_leb128_u32(&r);

                            if (data_idx >= mod->num_data_segments) {
                                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown data segment %u", (unsigned)data_idx);
                                err = WASM_ERR_MALFORMED;
                                goto cleanup;
                            }

                            mod->data_segments[data_idx].is_dropped = 1;
                            break;
                        }
                        case 0x0A: {
                            uint32_t dst_mem_idx = wasm__read_leb128_u32(&r);
                            uint32_t src_mem_idx = wasm__read_leb128_u32(&r);
                            wasm_value_t count = WASM__POP(rt);
                            wasm_value_t src = WASM__POP(rt);
                            wasm_value_t dst = WASM__POP(rt);
                            uint32_t len = (uint32_t)count.of.i32;
                            uint32_t src_offset = (uint32_t)src.of.i32;
                            uint32_t dst_offset = (uint32_t)dst.of.i32;
                            wasm_memory_t* dst_memory = wasm__memory_at(mod, dst_mem_idx);
                            wasm_memory_t* src_memory = wasm__memory_at(mod, src_mem_idx);
                            uint64_t dst_mem_size;
                            uint64_t src_mem_size;

                            if (!dst_memory || !src_memory) {
                                WASM__SET_ERR(rt, WASM_ERR_MALFORMED,
                                              "unknown memory %u,%u",
                                              (unsigned)dst_mem_idx, (unsigned)src_mem_idx);
                                err = WASM_ERR_MALFORMED;
                                goto cleanup;
                            }
                            dst_mem_size = (uint64_t)dst_memory->pages * WASM_PAGE_SIZE;
                            src_mem_size = (uint64_t)src_memory->pages * WASM_PAGE_SIZE;
                            if (!wasm__range_in_bounds_u64(src_offset, len, src_mem_size) ||
                                !wasm__range_in_bounds_u64(dst_offset, len, dst_mem_size))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (len > 0)
                                memmove(dst_memory->data + dst_offset, src_memory->data + src_offset, len);
                            break;
                        }
                        case 0x0B: {
                            uint32_t mem_idx = wasm__read_leb128_u32(&r);
                            wasm_value_t count = WASM__POP(rt);
                            wasm_value_t value = WASM__POP(rt);
                            wasm_value_t dst = WASM__POP(rt);
                            uint32_t len = (uint32_t)count.of.i32;
                            uint32_t dst_offset = (uint32_t)dst.of.i32;
                            wasm_memory_t* memory = wasm__memory_at(mod, mem_idx);
                            uint64_t mem_size;

                            if (!memory) {
                                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown memory %u", (unsigned)mem_idx);
                                err = WASM_ERR_MALFORMED;
                                goto cleanup;
                            }
                            mem_size = (uint64_t)memory->pages * WASM_PAGE_SIZE;
                            if (!wasm__range_in_bounds_u64(dst_offset, len, mem_size)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (len > 0)
                                memset(memory->data + dst_offset, (unsigned char)(value.of.i32 & 0xFF), len);
                            break;
                        }
                        case 0x0C: {
                            uint32_t elem_idx = wasm__read_leb128_u32(&r);
                            uint32_t table_idx = wasm__read_leb128_u32(&r);
                            wasm_value_t count = WASM__POP(rt);
                            wasm_value_t src = WASM__POP(rt);
                            wasm_value_t dst = WASM__POP(rt);
                            uint32_t len = (uint32_t)count.of.i32;
                            uint32_t src_offset = (uint32_t)src.of.i32;
                            uint32_t dst_offset = (uint32_t)dst.of.i32;
                            wasm_elem_segment_t* segment;
                            wasm_table_t* table;
                            uint32_t j;

                            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                            if (elem_idx >= mod->num_elem_segments) {
                                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown elem segment %u", (unsigned)elem_idx);
                                err = WASM_ERR_MALFORMED;
                                goto cleanup;
                            }

                            table = &mod->tables[table_idx];
                            segment = &mod->elem_segments[elem_idx];
                            if (!segment->is_passive || segment->is_dropped) WASM__TRAP(WASM_ERR_TRAP);
                            if (table->reftype != segment->elem_type) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                            if (!wasm__range_in_bounds_u64(src_offset, len, segment->num_elems) ||
                                !wasm__range_in_bounds_u64(dst_offset, len, table->size))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (len > 0) {
                                if (!table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                for (j = 0; j < len; j++) table->elems[dst_offset + j] = segment->elems[src_offset + j];
                            }
                            break;
                        }
                        case 0x0D: {
                            uint32_t elem_idx = wasm__read_leb128_u32(&r);

                            if (elem_idx >= mod->num_elem_segments) {
                                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown elem segment %u", (unsigned)elem_idx);
                                err = WASM_ERR_MALFORMED;
                                goto cleanup;
                            }

                            mod->elem_segments[elem_idx].is_dropped = 1;
                            break;
                        }
                        case 0x0E: {
                            uint32_t dst_table_idx = wasm__read_leb128_u32(&r);
                            uint32_t src_table_idx = wasm__read_leb128_u32(&r);
                            wasm_value_t count = WASM__POP(rt);
                            wasm_value_t src = WASM__POP(rt);
                            wasm_value_t dst = WASM__POP(rt);
                            uint32_t len = (uint32_t)count.of.i32;
                            uint32_t src_offset = (uint32_t)src.of.i32;
                            uint32_t dst_offset = (uint32_t)dst.of.i32;
                            wasm_table_t* dst_table;
                            wasm_table_t* src_table;

                            if (dst_table_idx >= mod->num_tables || src_table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                            dst_table = &mod->tables[dst_table_idx];
                            src_table = &mod->tables[src_table_idx];
                            if (dst_table->reftype != src_table->reftype) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                            if (!wasm__range_in_bounds_u64(src_offset, len, src_table->size) ||
                                !wasm__range_in_bounds_u64(dst_offset, len, dst_table->size))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (len > 0) {
                                if (!dst_table->elems || !src_table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                memmove(dst_table->elems + dst_offset,
                                        src_table->elems + src_offset,
                                        len * sizeof(wasm_value_t));
                            }
                            break;
                        }
                        case 0x0F: {
                            uint32_t table_idx = wasm__read_leb128_u32(&r);
                            wasm_value_t delta_value = WASM__POP(rt);
                            wasm_value_t init_value = WASM__POP(rt);
                            wasm_table_t* table;
                            uint32_t old_size;
                            uint32_t delta;
                            uint32_t new_size;
                            wasm_value_t* new_elems;
                            uint32_t j;

                            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                            table = &mod->tables[table_idx];
                            if (!wasm__value_matches_type(&init_value, table->reftype)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                            old_size = table->size;
                            delta = (uint32_t)delta_value.of.i32;
                            if (delta == 0) {
                                WASM__PUSH(rt, wasm_i32((int32_t)old_size));
                                break;
                            }
                            if ((table->max_size != UINT32_MAX && delta > table->max_size - old_size) ||
                                delta > UINT32_MAX - old_size) {
                                WASM__PUSH(rt, wasm_i32(-1));
                                break;
                            }
                            new_size = old_size + delta;
                            new_elems = (wasm_value_t*)WASM_REALLOC(table->elems, new_size * sizeof(wasm_value_t));
                            if (!new_elems) {
                                WASM__PUSH(rt, wasm_i32(-1));
                                break;
                            }
                            table->elems = new_elems;
                            table->size = new_size;
                            for (j = old_size; j < new_size; j++) table->elems[j] = init_value;
                            WASM__PUSH(rt, wasm_i32((int32_t)old_size));
                            break;
                        }
                        case 0x10: {
                            uint32_t table_idx = wasm__read_leb128_u32(&r);
                            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                            WASM__PUSH(rt, wasm_i32((int32_t)mod->tables[table_idx].size));
                            break;
                        }
                        case 0x11: {
                            uint32_t table_idx = wasm__read_leb128_u32(&r);
                            wasm_value_t count = WASM__POP(rt);
                            wasm_value_t value = WASM__POP(rt);
                            wasm_value_t dst = WASM__POP(rt);
                            wasm_table_t* table;
                            uint32_t len = (uint32_t)count.of.i32;
                            uint32_t dst_offset = (uint32_t)dst.of.i32;
                            uint32_t j;

                            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                            table = &mod->tables[table_idx];
                            if (!wasm__value_matches_type(&value, table->reftype)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                            if (!wasm__range_in_bounds_u64(dst_offset, len, table->size)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (len > 0) {
                                if (!table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                for (j = 0; j < len; j++) table->elems[dst_offset + j] = value;
                            }
                            break;
                        }
                        default:
                            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown prefixed opcode 0xFC 0x%X", (unsigned)subop);
                            err = WASM_ERR_MALFORMED;
                            goto cleanup;
                    }
                    break;
                }

                case 0xFD: {
                    uint32_t subop = wasm__read_leb128_u32(&r);

                    switch (subop) {
                        case 0x00:
                        case 0x01:
                        case 0x02:
                        case 0x03:
                        case 0x04:
                        case 0x05:
                        case 0x06:
                        case 0x07:
                        case 0x08:
                        case 0x09:
                        case 0x0A:
                        case 0x5C:
                        case 0x5D: {
                            wasm__memarg_t memarg;
                            wasm_value_t base;
                            wasm_value_t vector = wasm_v128_zero();
                            wasm_memory_t* memory;
                            uint64_t addr;
                            uint64_t msz;
                            uint32_t lane;

                            err = wasm__read_memarg(&r, &memarg);
                            if (err != WASM_OK) goto cleanup;
                            base = WASM__POP(rt);
                            memory = wasm__memory_at(mod, memarg.memory_index);
                            if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                            addr = (uint64_t)(uint32_t)base.of.i32 + memarg.offset;
                            msz = (uint64_t)memory->pages * WASM_PAGE_SIZE;

                            switch (subop) {
                                case 0x00:
                                    if (addr + 16u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    memcpy(vector.of.v128, memory->data + addr, 16);
                                    break;
                                case 0x01:
                                case 0x02:
                                    if (addr + 8u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    for (lane = 0; lane < 8; lane++) {
                                        if (subop == 0x01)
                                            wasm__v128_set_i16(&vector, lane, (int8_t)memory->data[addr + lane]);
                                        else
                                            wasm__v128_set_u16(&vector, lane, memory->data[addr + lane]);
                                    }
                                    break;
                                case 0x03:
                                case 0x04:
                                    if (addr + 8u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    for (lane = 0; lane < 4; lane++) {
                                        uint16_t loaded = wasm__load_le16(memory->data + addr + lane * 2u);
                                        if (subop == 0x03)
                                            wasm__v128_set_i32(&vector, lane, (int16_t)loaded);
                                        else
                                            wasm__v128_set_u32(&vector, lane, loaded);
                                    }
                                    break;
                                case 0x05:
                                case 0x06:
                                    if (addr + 8u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    for (lane = 0; lane < 2; lane++) {
                                        uint32_t loaded = wasm__load_le32(memory->data + addr + lane * 4u);
                                        if (subop == 0x05)
                                            wasm__v128_set_i64(&vector, lane, (int32_t)loaded);
                                        else
                                            wasm__v128_set_u64(&vector, lane, loaded);
                                    }
                                    break;
                                case 0x07:
                                    if (addr + 1u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    for (lane = 0; lane < 16; lane++) vector.of.v128[lane] = memory->data[addr];
                                    break;
                                case 0x08: {
                                    uint16_t loaded;
                                    if (addr + 2u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    loaded = wasm__load_le16(memory->data + addr);
                                    for (lane = 0; lane < 8; lane++) wasm__v128_set_u16(&vector, lane, loaded);
                                    break;
                                }
                                case 0x09: {
                                    uint32_t loaded;
                                    if (addr + 4u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    loaded = wasm__load_le32(memory->data + addr);
                                    for (lane = 0; lane < 4; lane++) wasm__v128_set_u32(&vector, lane, loaded);
                                    break;
                                }
                                case 0x0A: {
                                    uint64_t loaded;
                                    if (addr + 8u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    loaded = wasm__load_le64(memory->data + addr);
                                    for (lane = 0; lane < 2; lane++) wasm__v128_set_u64(&vector, lane, loaded);
                                    break;
                                }
                                case 0x5C:
                                    if (addr + 4u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__v128_set_u32(&vector, 0, wasm__load_le32(memory->data + addr));
                                    break;
                                default:
                                    if (addr + 8u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__v128_set_u64(&vector, 0, wasm__load_le64(memory->data + addr));
                                    break;
                            }

                            WASM__PUSH(rt, vector);
                            break;
                        }
                        case 0x0B: {
                            wasm__memarg_t memarg;
                            wasm_value_t vector;
                            wasm_value_t base;
                            wasm_memory_t* memory;
                            uint64_t addr;
                            uint64_t msz;

                            err = wasm__read_memarg(&r, &memarg);
                            if (err != WASM_OK) goto cleanup;
                            vector = WASM__POP(rt);
                            base = WASM__POP(rt);
                            memory = wasm__memory_at(mod, memarg.memory_index);
                            if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                            addr = (uint64_t)(uint32_t)base.of.i32 + memarg.offset;
                            msz = (uint64_t)memory->pages * WASM_PAGE_SIZE;
                            if (addr + 16u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            memcpy(memory->data + addr, vector.of.v128, 16);
                            break;
                        }
                        case 0x0C: {
                            wasm_value_t vector = wasm_v128_zero();
                            if (!wasm__has(&r, 16)) WASM__TRAP(WASM_ERR_MALFORMED);
                            memcpy(vector.of.v128, r.ptr, 16);
                            r.ptr += 16;
                            WASM__PUSH(rt, vector);
                            break;
                        }
                        case 0x0D: {
                            wasm_value_t rhs;
                            wasm_value_t lhs;
                            wasm_value_t vector;
                            uint8_t lanes[16];
                            uint32_t lane;

                            for (lane = 0; lane < 16; lane++) lanes[lane] = wasm__read_u8(&r);
                            if (r.malformed) WASM__TRAP(WASM_ERR_MALFORMED);
                            rhs = WASM__POP(rt);
                            lhs = WASM__POP(rt);
                            vector = wasm__simd_shuffle(&lhs, &rhs, lanes);
                            WASM__PUSH(rt, vector);
                            break;
                        }
                        case 0x0E: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_swizzle(&lhs, &rhs));
                            break;
                        }
                        case 0x0F:
                        case 0x10:
                        case 0x11:
                        case 0x12:
                        case 0x13:
                        case 0x14: {
                            wasm_value_t scalar = WASM__POP(rt);
                            wasm_value_t vector = wasm_v128_zero();
                            uint32_t lane;

                            switch (subop) {
                                case 0x0F:
                                    for (lane = 0; lane < 16; lane++) wasm__v128_set_u8(&vector, lane, (uint8_t)scalar.of.i32);
                                    break;
                                case 0x10:
                                    for (lane = 0; lane < 8; lane++) wasm__v128_set_u16(&vector, lane, (uint16_t)scalar.of.i32);
                                    break;
                                case 0x11:
                                    for (lane = 0; lane < 4; lane++) wasm__v128_set_i32(&vector, lane, scalar.of.i32);
                                    break;
                                case 0x12:
                                    for (lane = 0; lane < 2; lane++) wasm__v128_set_i64(&vector, lane, scalar.of.i64);
                                    break;
                                case 0x13:
                                    for (lane = 0; lane < 4; lane++) wasm__v128_set_f32(&vector, lane, scalar.of.f32);
                                    break;
                                default:
                                    for (lane = 0; lane < 2; lane++) wasm__v128_set_f64(&vector, lane, scalar.of.f64);
                                    break;
                            }

                            WASM__PUSH(rt, vector);
                            break;
                        }
                        case 0x15:
                        case 0x16:
                        case 0x18:
                        case 0x19:
                        case 0x1B:
                        case 0x1D:
                        case 0x1F:
                        case 0x21: {
                            uint32_t lane = wasm__read_u8(&r);
                            wasm_value_t vector = WASM__POP(rt);
                            if (r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);

                            switch (subop) {
                                case 0x15:
                                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_i8(&vector, lane)));
                                    break;
                                case 0x16:
                                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_u8(&vector, lane)));
                                    break;
                                case 0x18:
                                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_i16(&vector, lane)));
                                    break;
                                case 0x19:
                                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_u16(&vector, lane)));
                                    break;
                                case 0x1B:
                                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_i32(&vector, lane)));
                                    break;
                                case 0x1D:
                                    WASM__PUSH(rt, wasm_i64(wasm__v128_get_i64(&vector, lane)));
                                    break;
                                case 0x1F:
                                    WASM__PUSH(rt, wasm_f32(wasm__v128_get_f32(&vector, lane)));
                                    break;
                                default:
                                    WASM__PUSH(rt, wasm_f64(wasm__v128_get_f64(&vector, lane)));
                                    break;
                            }
                            break;
                        }
                        case 0x17:
                        case 0x1A:
                        case 0x1C:
                        case 0x1E:
                        case 0x20:
                        case 0x22: {
                            uint32_t lane = wasm__read_u8(&r);
                            wasm_value_t scalar = WASM__POP(rt);
                            wasm_value_t vector = WASM__POP(rt);
                            if (r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);

                            switch (subop) {
                                case 0x17:
                                    wasm__v128_set_u8(&vector, lane, (uint8_t)scalar.of.i32);
                                    break;
                                case 0x1A:
                                    wasm__v128_set_u16(&vector, lane, (uint16_t)scalar.of.i32);
                                    break;
                                case 0x1C:
                                    wasm__v128_set_i32(&vector, lane, scalar.of.i32);
                                    break;
                                case 0x1E:
                                    wasm__v128_set_i64(&vector, lane, scalar.of.i64);
                                    break;
                                case 0x20:
                                    wasm__v128_set_f32(&vector, lane, scalar.of.f32);
                                    break;
                                default:
                                    wasm__v128_set_f64(&vector, lane, scalar.of.f64);
                                    break;
                            }

                            WASM__PUSH(rt, vector);
                            break;
                        }
                        case 0x23:
                        case 0x24:
                        case 0x25:
                        case 0x26:
                        case 0x27:
                        case 0x28:
                        case 0x29:
                        case 0x2A:
                        case 0x2B:
                        case 0x2C: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_cmp_i8x16(subop, &lhs, &rhs));
                            break;
                        }
                        case 0x2D:
                        case 0x2E:
                        case 0x2F:
                        case 0x30:
                        case 0x31:
                        case 0x32:
                        case 0x33:
                        case 0x34:
                        case 0x35:
                        case 0x36: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_cmp_i16x8(subop, &lhs, &rhs));
                            break;
                        }
                        case 0x37:
                        case 0x38:
                        case 0x39:
                        case 0x3A:
                        case 0x3B:
                        case 0x3C:
                        case 0x3D:
                        case 0x3E:
                        case 0x3F:
                        case 0x40: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_cmp_i32x4(subop, &lhs, &rhs));
                            break;
                        }
                        case 0x41:
                        case 0x42:
                        case 0x43:
                        case 0x44:
                        case 0x45:
                        case 0x46: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_cmp_f32x4(subop, &lhs, &rhs));
                            break;
                        }
                        case 0x47:
                        case 0x48:
                        case 0x49:
                        case 0x4A:
                        case 0x4B:
                        case 0x4C: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_cmp_f64x2(subop, &lhs, &rhs));
                            break;
                        }
                        case 0x4D: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_bitwise_not(&value));
                            break;
                        }
                        case 0x4E:
                        case 0x4F:
                        case 0x50:
                        case 0x51: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_bitwise_binary(subop, &lhs, &rhs));
                            break;
                        }
                        case 0x52: {
                            wasm_value_t mask = WASM__POP(rt);
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_bitselect(&lhs, &rhs, &mask));
                            break;
                        }
                        case 0x53: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_any_true(&value) ? 1 : 0));
                            break;
                        }
                        case 0x54:
                        case 0x55:
                        case 0x56:
                        case 0x57: {
                            wasm__memarg_t memarg;
                            uint32_t lane = 0;
                            wasm_value_t vector;
                            wasm_value_t base;
                            wasm_memory_t* memory;
                            uint64_t addr;
                            uint64_t msz;

                            err = wasm__read_memarg(&r, &memarg);
                            if (err != WASM_OK) goto cleanup;
                            lane = wasm__read_u8(&r);
                            vector = WASM__POP(rt);
                            base = WASM__POP(rt);
                            if (r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);
                            memory = wasm__memory_at(mod, memarg.memory_index);
                            if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                            addr = (uint64_t)(uint32_t)base.of.i32 + memarg.offset;
                            msz = (uint64_t)memory->pages * WASM_PAGE_SIZE;

                            switch (subop) {
                                case 0x54:
                                    if (addr + 1u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__v128_set_u8(&vector, lane, memory->data[addr]);
                                    break;
                                case 0x55:
                                    if (addr + 2u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__v128_set_u16(&vector, lane, wasm__load_le16(memory->data + addr));
                                    break;
                                case 0x56:
                                    if (addr + 4u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__v128_set_u32(&vector, lane, wasm__load_le32(memory->data + addr));
                                    break;
                                default:
                                    if (addr + 8u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__v128_set_u64(&vector, lane, wasm__load_le64(memory->data + addr));
                                    break;
                            }

                            WASM__PUSH(rt, vector);
                            break;
                        }
                        case 0x58:
                        case 0x59:
                        case 0x5A:
                        case 0x5B: {
                            wasm__memarg_t memarg;
                            uint32_t lane = 0;
                            wasm_value_t vector;
                            wasm_value_t base;
                            wasm_memory_t* memory;
                            uint64_t addr;
                            uint64_t msz;

                            err = wasm__read_memarg(&r, &memarg);
                            if (err != WASM_OK) goto cleanup;
                            lane = wasm__read_u8(&r);
                            vector = WASM__POP(rt);
                            base = WASM__POP(rt);
                            if (r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);
                            memory = wasm__memory_at(mod, memarg.memory_index);
                            if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                            addr = (uint64_t)(uint32_t)base.of.i32 + memarg.offset;
                            msz = (uint64_t)memory->pages * WASM_PAGE_SIZE;

                            switch (subop) {
                                case 0x58:
                                    if (addr + 1u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    memory->data[addr] = wasm__v128_get_u8(&vector, lane);
                                    break;
                                case 0x59:
                                    if (addr + 2u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__store_le16(memory->data + addr, wasm__v128_get_u16(&vector, lane));
                                    break;
                                case 0x5A:
                                    if (addr + 4u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__store_le32(memory->data + addr, wasm__v128_get_u32(&vector, lane));
                                    break;
                                default:
                                    if (addr + 8u > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                    wasm__store_le64(memory->data + addr, wasm__v128_get_u64(&vector, lane));
                                    break;
                            }

                            break;
                        }
                        case 0x5E:
                        case 0x5F:
                        case 0x7C:
                        case 0x7D:
                        case 0x7E:
                        case 0x7F:
                        case 0xF8:
                        case 0xF9:
                        case 0xFA:
                        case 0xFB:
                        case 0xFC:
                        case 0xFD:
                        case 0xFE:
                        case 0xFF: {
                            wasm_value_t value = WASM__POP(rt);
                            wasm_value_t vector;

                            switch (subop) {
                                case 0x5E:
                                    vector = wasm__simd_demote_f64x2_zero(&value);
                                    break;
                                case 0x5F:
                                    vector = wasm__simd_promote_low_f32x4(&value);
                                    break;
                                case 0x7C:
                                case 0x7D:
                                case 0x7E:
                                case 0x7F:
                                    vector = wasm__simd_extadd_pairwise(subop, &value);
                                    break;
                                case 0xF8:
                                case 0xF9:
                                    vector = wasm__simd_trunc_sat_f32x4(subop, &value);
                                    break;
                                case 0xFA:
                                case 0xFB:
                                    vector = wasm__simd_convert_i32x4(subop, &value);
                                    break;
                                case 0xFC:
                                case 0xFD:
                                    vector = wasm__simd_trunc_sat_f64x2_zero(subop, &value);
                                    break;
                                default:
                                    vector = wasm__simd_convert_low_i32x4_to_f64x2(subop, &value);
                                    break;
                            }

                            WASM__PUSH(rt, vector);
                            break;
                        }
                        case 0x60:
                        case 0x61:
                        case 0x62: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i8x16_unary(subop, &value));
                            break;
                        }
                        case 0x63: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_all_true_i8x16(&value) ? 1 : 0));
                            break;
                        }
                        case 0x64: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_bitmask_i8x16(&value)));
                            break;
                        }
                        case 0x65:
                        case 0x66:
                        case 0x6E:
                        case 0x6F:
                        case 0x70:
                        case 0x71:
                        case 0x72:
                        case 0x73:
                        case 0x78:
                        case 0x79:
                        case 0x7B: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i8x16_binary(subop, &lhs, &rhs));
                            break;
                        }
                        case 0x67:
                        case 0x68:
                        case 0x69:
                        case 0x6A:
                        case 0xE0:
                        case 0xE1:
                        case 0xE3: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_f32x4_unary(subop, &value));
                            break;
                        }
                        case 0x6B:
                        case 0x6C:
                        case 0x6D: {
                            wasm_value_t amount = WASM__POP(rt);
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i8x16_shift(subop, &value, (uint32_t)amount.of.i32));
                            break;
                        }
                        case 0x74:
                        case 0x75:
                        case 0x7A:
                        case 0x94:
                        case 0xEC:
                        case 0xED:
                        case 0xEF: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_f64x2_unary(subop, &value));
                            break;
                        }
                        case 0x80:
                        case 0x81:
                        case 0x87:
                        case 0x88:
                        case 0x89:
                        case 0x8A: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i16x8_unary(subop, &value));
                            break;
                        }
                        case 0x82:
                        case 0x85:
                        case 0x86:
                        case 0x8E:
                        case 0x8F:
                        case 0x90:
                        case 0x91:
                        case 0x92:
                        case 0x93:
                        case 0x95:
                        case 0x96:
                        case 0x97:
                        case 0x98:
                        case 0x99:
                        case 0x9B:
                        case 0x9C:
                        case 0x9D:
                        case 0x9E:
                        case 0x9F: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i16x8_binary(subop, &lhs, &rhs));
                            break;
                        }
                        case 0x83: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_all_true_i16x8(&value) ? 1 : 0));
                            break;
                        }
                        case 0x84: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_bitmask_i16x8(&value)));
                            break;
                        }
                        case 0x8B:
                        case 0x8C:
                        case 0x8D: {
                            wasm_value_t amount = WASM__POP(rt);
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i16x8_shift(subop, &value, (uint32_t)amount.of.i32));
                            break;
                        }
                        case 0xA0:
                        case 0xA1:
                        case 0xA7:
                        case 0xA8:
                        case 0xA9:
                        case 0xAA: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i32x4_unary(subop, &value));
                            break;
                        }
                        case 0xA3: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_all_true_i32x4(&value) ? 1 : 0));
                            break;
                        }
                        case 0xA4: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_bitmask_i32x4(&value)));
                            break;
                        }
                        case 0xAB:
                        case 0xAC:
                        case 0xAD: {
                            wasm_value_t amount = WASM__POP(rt);
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i32x4_shift(subop, &value, (uint32_t)amount.of.i32));
                            break;
                        }
                        case 0xAE:
                        case 0xB1:
                        case 0xB5:
                        case 0xB6:
                        case 0xB7:
                        case 0xB8:
                        case 0xB9:
                        case 0xBA:
                        case 0xBC:
                        case 0xBD:
                        case 0xBE:
                        case 0xBF: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i32x4_binary(subop, &lhs, &rhs));
                            break;
                        }
                        case 0xC0:
                        case 0xC1:
                        case 0xC7:
                        case 0xC8:
                        case 0xC9:
                        case 0xCA: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i64x2_unary(subop, &value));
                            break;
                        }
                        case 0xC3: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_all_true_i64x2(&value) ? 1 : 0));
                            break;
                        }
                        case 0xC4: {
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm_i32(wasm__simd_bitmask_i64x2(&value)));
                            break;
                        }
                        case 0xCB:
                        case 0xCC:
                        case 0xCD: {
                            wasm_value_t amount = WASM__POP(rt);
                            wasm_value_t value = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_i64x2_shift(subop, &value, (uint32_t)amount.of.i32));
                            break;
                        }
                        case 0xCE:
                        case 0xD1:
                        case 0xD5:
                        case 0xD6:
                        case 0xD7:
                        case 0xD8:
                        case 0xD9:
                        case 0xDA:
                        case 0xDB:
                        case 0xDC:
                        case 0xDD:
                        case 0xDE:
                        case 0xDF: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            if (subop >= 0xD6 && subop <= 0xDB)
                                WASM__PUSH(rt, wasm__simd_cmp_i64x2(subop, &lhs, &rhs));
                            else
                                WASM__PUSH(rt, wasm__simd_i64x2_binary(subop, &lhs, &rhs));
                            break;
                        }
                        case 0xE4:
                        case 0xE5:
                        case 0xE6:
                        case 0xE7:
                        case 0xE8:
                        case 0xE9:
                        case 0xEA:
                        case 0xEB: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_f32x4_binary(subop, &lhs, &rhs));
                            break;
                        }
                        case 0xF0:
                        case 0xF1:
                        case 0xF2:
                        case 0xF3:
                        case 0xF4:
                        case 0xF5:
                        case 0xF6:
                        case 0xF7: {
                            wasm_value_t rhs = WASM__POP(rt);
                            wasm_value_t lhs = WASM__POP(rt);
                            WASM__PUSH(rt, wasm__simd_f64x2_binary(subop, &lhs, &rhs));
                            break;
                        }
                        default:
                            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown prefixed opcode 0xFD 0x%X", (unsigned)subop);
                            err = WASM_ERR_MALFORMED;
                            goto cleanup;
                    }
                    break;
                }

                /* Parametric */
                case 0x1A: {
                    wasm_value_t tmp = WASM__POP(rt);
                    (void)tmp;
                    break;
                }
                case 0x1B: {
                    wasm_value_t c = WASM__POP(rt), v2 = WASM__POP(rt), v1 = WASM__POP(rt);
                    WASM__PUSH(rt, c.of.i32 ? v1 : v2);
                    break;
                }
                case 0x1C: {
                    wasm_valtype_t select_type;
                    wasm_value_t c, v2, v1;

                    err = wasm__read_typed_select_immediate(&r, &select_type);
                    if (err != WASM_OK) goto cleanup;
                    c = WASM__POP(rt);
                    v2 = WASM__POP(rt);
                    v1 = WASM__POP(rt);
                    if (!wasm__value_matches_type(&v1, select_type) || !wasm__value_matches_type(&v2, select_type))
                        WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                    WASM__PUSH(rt, c.of.i32 ? v1 : v2);
                    break;
                }

                /* Variables */
                case 0x20: {
                    uint32_t x = wasm__read_leb128_u32(&r);
                    WASM__PUSH(rt, locals[x]);
                    break;
                }
                case 0x21: {
                    uint32_t x = wasm__read_leb128_u32(&r);
                    locals[x] = WASM__POP(rt);
                    break;
                }
                case 0x22: {
                    uint32_t x = wasm__read_leb128_u32(&r);
                    locals[x] = WASM__PEEK(rt);
                    break;
                }
                case 0x23: {
                    uint32_t x = wasm__read_leb128_u32(&r);
                    WASM__PUSH(rt, wasm__global_get_value(&mod->globals[x]));
                    break;
                }
                case 0x24: {
                    uint32_t x = wasm__read_leb128_u32(&r);
                    if (!mod->globals[x].is_mutable) {
                        WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "global %u is immutable", (unsigned)x);
                        err = WASM_ERR_TYPE_MISMATCH;
                        goto cleanup;
                    }
                    wasm__global_set_value(&mod->globals[x], WASM__POP(rt));
                    break;
                }
                case 0x25: {
                    uint32_t table_idx = wasm__read_leb128_u32(&r);
                    wasm_table_t* table;
                    wasm_value_t index;
                    uint32_t element_index;

                    if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                    table = &mod->tables[table_idx];
                    index = WASM__POP(rt);
                    element_index = (uint32_t)index.of.i32;
                    if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    WASM__PUSH(rt, table->elems[element_index]);
                    break;
                }
                case 0x26: {
                    uint32_t table_idx = wasm__read_leb128_u32(&r);
                    wasm_table_t* table;
                    wasm_value_t value;
                    wasm_value_t index;
                    uint32_t element_index;

                    if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                    table = &mod->tables[table_idx];
                    value = WASM__POP(rt);
                    index = WASM__POP(rt);
                    element_index = (uint32_t)index.of.i32;
                    if (!wasm__value_matches_type(&value, table->reftype)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                    if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    table->elems[element_index] = value;
                    break;
                }

                /* Memory load */
                case 0x28:
                case 0x29:
                case 0x2A:
                case 0x2B:
                case 0x2C:
                case 0x2D:
                case 0x2E:
                case 0x2F:
                case 0x30:
                case 0x31:
                case 0x32:
                case 0x33:
                case 0x34:
                case 0x35: {
                    wasm__memarg_t memarg;
                    wasm_value_t base;
                    uint64_t addr, msz;
                    wasm_memory_t* memory;

                    err = wasm__read_memarg(&r, &memarg);
                    if (err != WASM_OK) goto cleanup;
                    base = WASM__POP(rt);
                    memory = wasm__memory_at(mod, memarg.memory_index);
                    if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                    addr = (uint64_t)(uint32_t)base.of.i32 + memarg.offset;
                    msz = (uint64_t)memory->pages * WASM_PAGE_SIZE;
                    switch (op) {
                        case 0x28: {
                            int32_t v;
                            if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            v = (int32_t)wasm__load_le32(memory->data + addr);
                            WASM__PUSH(rt, wasm_i32(v));
                            break;
                        }
                        case 0x29: {
                            int64_t v;
                            if (addr + 8 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            v = (int64_t)wasm__load_le64(memory->data + addr);
                            WASM__PUSH(rt, wasm_i64(v));
                            break;
                        }
                        case 0x2A: {
                            uint32_t b;
                            float v;
                            if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            b = wasm__load_le32(memory->data + addr);
                            memcpy(&v, &b, 4);
                            WASM__PUSH(rt, wasm_f32(v));
                            break;
                        }
                        case 0x2B: {
                            uint64_t b;
                            double v;
                            if (addr + 8 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            b = wasm__load_le64(memory->data + addr);
                            memcpy(&v, &b, 8);
                            WASM__PUSH(rt, wasm_f64(v));
                            break;
                        }
                        case 0x2C:
                            if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            WASM__PUSH(rt, wasm_i32((int32_t)(int8_t)memory->data[addr]));
                            break;
                        case 0x2D:
                            if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            WASM__PUSH(rt, wasm_i32((int32_t)memory->data[addr]));
                            break;
                        case 0x2E: {
                            int16_t v;
                            if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            v = (int16_t)wasm__load_le16(memory->data + addr);
                            WASM__PUSH(rt, wasm_i32((int32_t)v));
                            break;
                        }
                        case 0x2F: {
                            uint16_t v;
                            if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            v = wasm__load_le16(memory->data + addr);
                            WASM__PUSH(rt, wasm_i32((int32_t)v));
                            break;
                        }
                        case 0x30:
                            if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            WASM__PUSH(rt, wasm_i64((int64_t)(int8_t)memory->data[addr]));
                            break;
                        case 0x31:
                            if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            WASM__PUSH(rt, wasm_i64((int64_t)memory->data[addr]));
                            break;
                        case 0x32: {
                            int16_t v;
                            if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            v = (int16_t)wasm__load_le16(memory->data + addr);
                            WASM__PUSH(rt, wasm_i64((int64_t)v));
                            break;
                        }
                        case 0x33: {
                            uint16_t v;
                            if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            v = wasm__load_le16(memory->data + addr);
                            WASM__PUSH(rt, wasm_i64((int64_t)v));
                            break;
                        }
                        case 0x34: {
                            int32_t v;
                            if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            v = (int32_t)wasm__load_le32(memory->data + addr);
                            WASM__PUSH(rt, wasm_i64((int64_t)v));
                            break;
                        }
                        case 0x35: {
                            uint32_t v;
                            if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            v = wasm__load_le32(memory->data + addr);
                            WASM__PUSH(rt, wasm_i64((int64_t)v));
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }

                /* Memory store */
                case 0x36:
                case 0x37:
                case 0x38:
                case 0x39:
                case 0x3A:
                case 0x3B:
                case 0x3C:
                case 0x3D:
                case 0x3E: {
                    wasm__memarg_t memarg;
                    wasm_value_t val, base;
                    uint64_t addr, msz;
                    wasm_memory_t* memory;

                    err = wasm__read_memarg(&r, &memarg);
                    if (err != WASM_OK) goto cleanup;
                    val = WASM__POP(rt);
                    base = WASM__POP(rt);
                    memory = wasm__memory_at(mod, memarg.memory_index);
                    if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                    addr = (uint64_t)(uint32_t)base.of.i32 + memarg.offset;
                    msz = (uint64_t)memory->pages * WASM_PAGE_SIZE;
                    switch (op) {
                        case 0x36:
                            if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            wasm__store_le32(memory->data + addr, (uint32_t)val.of.i32);
                            break;
                        case 0x37:
                            if (addr + 8 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            wasm__store_le64(memory->data + addr, (uint64_t)val.of.i64);
                            break;
                        case 0x38: {
                            uint32_t b;
                            if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            memcpy(&b, &val.of.f32, 4);
                            wasm__store_le32(memory->data + addr, b);
                            break;
                        }
                        case 0x39: {
                            uint64_t b;
                            if (addr + 8 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            memcpy(&b, &val.of.f64, 8);
                            wasm__store_le64(memory->data + addr, b);
                            break;
                        }
                        case 0x3A:
                            if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            memory->data[addr] = (uint8_t)val.of.i32;
                            break;
                        case 0x3B:
                            if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            wasm__store_le16(memory->data + addr, (uint16_t)val.of.i32);
                            break;
                        case 0x3C:
                            if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            memory->data[addr] = (uint8_t)val.of.i64;
                            break;
                        case 0x3D:
                            if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            wasm__store_le16(memory->data + addr, (uint16_t)val.of.i64);
                            break;
                        case 0x3E:
                            if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            wasm__store_le32(memory->data + addr, (uint32_t)val.of.i64);
                            break;
                        default:
                            break;
                    }
                    break;
                }

                case 0x3F: {
                    uint32_t mem_idx = wasm__read_leb128_u32(&r);
                    wasm_memory_t* memory = wasm__memory_at(mod, mem_idx);

                    if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                    WASM__PUSH(rt, wasm_i32((int32_t)memory->pages));
                    break;
                }
                case 0x40: {
                    wasm_value_t d;
                    int32_t res;
                    uint32_t mem_idx = wasm__read_leb128_u32(&r);
                    d = WASM__POP(rt);
                    res = wasm_memory_grow_at(mod, mem_idx, (uint32_t)d.of.i32);
                    WASM__PUSH(rt, wasm_i32(res));
                    break;
                }

                /* Reference instructions */
                case 0xD0: {
                    wasm_valtype_t type;
                    err = wasm__read_reftype(&r, &type);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, wasm_ref_null(type));
                    break;
                }
                case 0xD1: {
                    wasm_value_t value = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(wasm__is_null_ref(&value) ? 1 : 0));
                    break;
                }
                case 0xD2: {
                    uint32_t func_index = wasm__read_leb128_u32(&r);
                    if (func_index >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                    WASM__PUSH(rt, wasm_funcref(func_index));
                    break;
                }

                /* Constants */
                case 0x41:
                    WASM__PUSH(rt, wasm_i32(wasm__read_leb128_i32(&r)));
                    break;
                case 0x42:
                    WASM__PUSH(rt, wasm_i64(wasm__read_leb128_i64(&r)));
                    break;
                case 0x43:
                    WASM__PUSH(rt, wasm_f32(wasm__read_f32(&r)));
                    break;
                case 0x44:
                    WASM__PUSH(rt, wasm_f64(wasm__read_f64(&r)));
                    break;

                /* i32 comparison */
                case 0x45: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 == 0 ? 1 : 0));
                    break;
                }
                case 0x46: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 == b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x47: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 != b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x48: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 < b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x49: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((uint32_t)a.of.i32 < (uint32_t)b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x4A: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 > b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x4B: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((uint32_t)a.of.i32 > (uint32_t)b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x4C: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 <= b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x4D: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((uint32_t)a.of.i32 <= (uint32_t)b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x4E: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 >= b.of.i32 ? 1 : 0));
                    break;
                }
                case 0x4F: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((uint32_t)a.of.i32 >= (uint32_t)b.of.i32 ? 1 : 0));
                    break;
                }

                /* i64 comparison */
                case 0x50: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i64 == 0 ? 1 : 0));
                    break;
                }
                case 0x51: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i64 == b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x52: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i64 != b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x53: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i64 < b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x54: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((uint64_t)a.of.i64 < (uint64_t)b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x55: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i64 > b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x56: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((uint64_t)a.of.i64 > (uint64_t)b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x57: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i64 <= b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x58: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((uint64_t)a.of.i64 <= (uint64_t)b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x59: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i64 >= b.of.i64 ? 1 : 0));
                    break;
                }
                case 0x5A: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((uint64_t)a.of.i64 >= (uint64_t)b.of.i64 ? 1 : 0));
                    break;
                }

                /* f32/f64 comparison */
                case 0x5B: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f32 == b.of.f32 ? 1 : 0));
                    break;
                }
                case 0x5C: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f32 != b.of.f32 ? 1 : 0));
                    break;
                }
                case 0x5D: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f32 < b.of.f32 ? 1 : 0));
                    break;
                }
                case 0x5E: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f32 > b.of.f32 ? 1 : 0));
                    break;
                }
                case 0x5F: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f32 <= b.of.f32 ? 1 : 0));
                    break;
                }
                case 0x60: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f32 >= b.of.f32 ? 1 : 0));
                    break;
                }
                case 0x61: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f64 == b.of.f64 ? 1 : 0));
                    break;
                }
                case 0x62: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f64 != b.of.f64 ? 1 : 0));
                    break;
                }
                case 0x63: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f64 < b.of.f64 ? 1 : 0));
                    break;
                }
                case 0x64: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f64 > b.of.f64 ? 1 : 0));
                    break;
                }
                case 0x65: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f64 <= b.of.f64 ? 1 : 0));
                    break;
                }
                case 0x66: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.f64 >= b.of.f64 ? 1 : 0));
                    break;
                }

                /* i32 arithmetic */
                case 0x67: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((int32_t)wasm__clz32((uint32_t)a.of.i32)));
                    break;
                }
                case 0x68: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((int32_t)wasm__ctz32((uint32_t)a.of.i32)));
                    break;
                }
                case 0x69: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((int32_t)wasm__popcnt32((uint32_t)a.of.i32)));
                    break;
                }
                case 0x6A: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 + b.of.i32));
                    break;
                }
                case 0x6B: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 - b.of.i32));
                    break;
                }
                case 0x6C: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 * b.of.i32));
                    break;
                }
                case 0x6D: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                    if (a.of.i32 == INT32_MIN && b.of.i32 == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 / b.of.i32));
                    break;
                }
                case 0x6E: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                    WASM__PUSH(rt, wasm_i32((int32_t)((uint32_t)a.of.i32 / (uint32_t)b.of.i32)));
                    break;
                }
                case 0x6F: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                    WASM__PUSH(rt, wasm_i32((a.of.i32 == INT32_MIN && b.of.i32 == -1) ? 0 : a.of.i32 % b.of.i32));
                    break;
                }
                case 0x70: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                    WASM__PUSH(rt, wasm_i32((int32_t)((uint32_t)a.of.i32 % (uint32_t)b.of.i32)));
                    break;
                }
                case 0x71: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 & b.of.i32));
                    break;
                }
                case 0x72: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 | b.of.i32));
                    break;
                }
                case 0x73: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 ^ b.of.i32));
                    break;
                }
                case 0x74: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 << (b.of.i32 & 31)));
                    break;
                }
                case 0x75: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32(a.of.i32 >> (b.of.i32 & 31)));
                    break;
                }
                case 0x76: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((int32_t)((uint32_t)a.of.i32 >> (b.of.i32 & 31))));
                    break;
                }
                case 0x77: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    uint32_t k = (uint32_t)b.of.i32 & 31u;
                    WASM__PUSH(rt,
                               wasm_i32((int32_t)(((uint32_t)a.of.i32 << k) |
                                                  ((uint32_t)a.of.i32 >> ((32u - k) & 31u)))));
                    break;
                }
                case 0x78: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    uint32_t k = (uint32_t)b.of.i32 & 31u;
                    WASM__PUSH(rt,
                               wasm_i32((int32_t)(((uint32_t)a.of.i32 >> k) |
                                                  ((uint32_t)a.of.i32 << ((32u - k) & 31u)))));
                    break;
                }

                /* i64 arithmetic */
                case 0x79: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64((int64_t)wasm__clz64((uint64_t)a.of.i64)));
                    break;
                }
                case 0x7A: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64((int64_t)wasm__ctz64((uint64_t)a.of.i64)));
                    break;
                }
                case 0x7B: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64((int64_t)wasm__popcnt64((uint64_t)a.of.i64)));
                    break;
                }
                case 0x7C: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 + b.of.i64));
                    break;
                }
                case 0x7D: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 - b.of.i64));
                    break;
                }
                case 0x7E: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 * b.of.i64));
                    break;
                }
                case 0x7F: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                    if (a.of.i64 == INT64_MIN && b.of.i64 == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 / b.of.i64));
                    break;
                }
                case 0x80: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                    WASM__PUSH(rt, wasm_i64((int64_t)((uint64_t)a.of.i64 / (uint64_t)b.of.i64)));
                    break;
                }
                case 0x81: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                    WASM__PUSH(rt, wasm_i64((a.of.i64 == INT64_MIN && b.of.i64 == -1) ? 0 : a.of.i64 % b.of.i64));
                    break;
                }
                case 0x82: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                    WASM__PUSH(rt, wasm_i64((int64_t)((uint64_t)a.of.i64 % (uint64_t)b.of.i64)));
                    break;
                }
                case 0x83: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 & b.of.i64));
                    break;
                }
                case 0x84: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 | b.of.i64));
                    break;
                }
                case 0x85: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 ^ b.of.i64));
                    break;
                }
                case 0x86: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 << (b.of.i64 & 63)));
                    break;
                }
                case 0x87: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64(a.of.i64 >> (b.of.i64 & 63)));
                    break;
                }
                case 0x88: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64((int64_t)((uint64_t)a.of.i64 >> (b.of.i64 & 63))));
                    break;
                }
                case 0x89: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    uint32_t k = (uint32_t)b.of.i64 & 63u;
                    WASM__PUSH(rt,
                               wasm_i64((int64_t)(((uint64_t)a.of.i64 << k) |
                                                  ((uint64_t)a.of.i64 >> ((64u - k) & 63u)))));
                    break;
                }
                case 0x8A: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    uint32_t k = (uint32_t)b.of.i64 & 63u;
                    WASM__PUSH(rt,
                               wasm_i64((int64_t)(((uint64_t)a.of.i64 >> k) |
                                                  ((uint64_t)a.of.i64 << ((64u - k) & 63u)))));
                    break;
                }

                /* f32 arithmetic */
                case 0x8B: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)fabs((double)a.of.f32)));
                    break;
                }
                case 0x8C: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32(-a.of.f32));
                    break;
                }
                case 0x8D: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)ceil((double)a.of.f32)));
                    break;
                }
                case 0x8E: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)floor((double)a.of.f32)));
                    break;
                }
                case 0x8F: {
                    wasm_value_t a = WASM__POP(rt);
                    double d = (double)a.of.f32;
                    WASM__PUSH(rt, wasm_f32((float)(d >= 0 ? floor(d) : ceil(d))));
                    break;
                }
                case 0x90: {
                    wasm_value_t a = WASM__POP(rt);
                    double d = (double)a.of.f32, rd = floor(d + 0.5);
                    if (d + 0.5 == rd && fmod(rd, 2.0) != 0.0) rd -= 1.0;
                    WASM__PUSH(rt, wasm_f32((float)rd));
                    break;
                }
                case 0x91: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)sqrt((double)a.of.f32)));
                    break;
                }
                case 0x92: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32(a.of.f32 + b.of.f32));
                    break;
                }
                case 0x93: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32(a.of.f32 - b.of.f32));
                    break;
                }
                case 0x94: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32(a.of.f32 * b.of.f32));
                    break;
                }
                case 0x95: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32(a.of.f32 / b.of.f32));
                    break;
                }
                case 0x96: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32(wasm__f32_min(a.of.f32, b.of.f32)));
                    break;
                }
                case 0x97: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32(wasm__f32_max(a.of.f32, b.of.f32)));
                    break;
                }
                case 0x98: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    uint32_t ab, bb, rb;
                    memcpy(&ab, &a.of.f32, 4);
                    memcpy(&bb, &b.of.f32, 4);
                    rb = (ab & 0x7FFFFFFFu) | (bb & 0x80000000u);
                    {
                        float fv;
                        memcpy(&fv, &rb, 4);
                        WASM__PUSH(rt, wasm_f32(fv));
                    }
                    break;
                }

                /* f64 arithmetic */
                case 0x99: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(fabs(a.of.f64)));
                    break;
                }
                case 0x9A: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(-a.of.f64));
                    break;
                }
                case 0x9B: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(ceil(a.of.f64)));
                    break;
                }
                case 0x9C: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(floor(a.of.f64)));
                    break;
                }
                case 0x9D: {
                    wasm_value_t a = WASM__POP(rt);
                    double d = a.of.f64;
                    WASM__PUSH(rt, wasm_f64(d >= 0 ? floor(d) : ceil(d)));
                    break;
                }
                case 0x9E: {
                    wasm_value_t a = WASM__POP(rt);
                    double d = a.of.f64, rd = floor(d + 0.5);
                    if (d + 0.5 == rd && fmod(rd, 2.0) != 0.0) rd -= 1.0;
                    WASM__PUSH(rt, wasm_f64(rd));
                    break;
                }
                case 0x9F: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(sqrt(a.of.f64)));
                    break;
                }
                case 0xA0: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(a.of.f64 + b.of.f64));
                    break;
                }
                case 0xA1: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(a.of.f64 - b.of.f64));
                    break;
                }
                case 0xA2: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(a.of.f64 * b.of.f64));
                    break;
                }
                case 0xA3: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(a.of.f64 / b.of.f64));
                    break;
                }
                case 0xA4: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(wasm__f64_min(a.of.f64, b.of.f64)));
                    break;
                }
                case 0xA5: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64(wasm__f64_max(a.of.f64, b.of.f64)));
                    break;
                }
                case 0xA6: {
                    wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                    uint64_t ab, bb, rb;
                    memcpy(&ab, &a.of.f64, 8);
                    memcpy(&bb, &b.of.f64, 8);
                    rb = (ab & (uint64_t)0x7FFFFFFFFFFFFFFFull) | (bb & (uint64_t)0x8000000000000000ull);
                    {
                        double dv;
                        memcpy(&dv, &rb, 8);
                        WASM__PUSH(rt, wasm_f64(dv));
                    }
                    break;
                }

                /* Conversions */
                case 0xA7: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i32((int32_t)a.of.i64));
                    break;
                }
                case 0xA8: {
                    wasm_value_t a = WASM__POP(rt);
                    wasm_value_t value;
                    err = wasm__trunc_i32_s((double)a.of.f32, &value);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, value);
                    break;
                }
                case 0xA9: {
                    wasm_value_t a = WASM__POP(rt);
                    wasm_value_t value;
                    err = wasm__trunc_i32_u((double)a.of.f32, &value);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, value);
                    break;
                }
                case 0xAA: {
                    wasm_value_t a = WASM__POP(rt);
                    wasm_value_t value;
                    err = wasm__trunc_i32_s(a.of.f64, &value);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, value);
                    break;
                }
                case 0xAB: {
                    wasm_value_t a = WASM__POP(rt);
                    wasm_value_t value;
                    err = wasm__trunc_i32_u(a.of.f64, &value);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, value);
                    break;
                }
                case 0xAC: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64((int64_t)a.of.i32));
                    break;
                }
                case 0xAD: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_i64((int64_t)(uint32_t)a.of.i32));
                    break;
                }
                case 0xAE: {
                    wasm_value_t a = WASM__POP(rt);
                    wasm_value_t value;
                    err = wasm__trunc_i64_s((double)a.of.f32, &value);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, value);
                    break;
                }
                case 0xAF: {
                    wasm_value_t a = WASM__POP(rt);
                    wasm_value_t value;
                    err = wasm__trunc_i64_u((double)a.of.f32, &value);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, value);
                    break;
                }
                case 0xB0: {
                    wasm_value_t a = WASM__POP(rt);
                    wasm_value_t value;
                    err = wasm__trunc_i64_s(a.of.f64, &value);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, value);
                    break;
                }
                case 0xB1: {
                    wasm_value_t a = WASM__POP(rt);
                    wasm_value_t value;
                    err = wasm__trunc_i64_u(a.of.f64, &value);
                    if (err != WASM_OK) goto cleanup;
                    WASM__PUSH(rt, value);
                    break;
                }
                case 0xB2: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)a.of.i32));
                    break;
                }
                case 0xB3: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)(uint32_t)a.of.i32));
                    break;
                }
                case 0xB4: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)a.of.i64));
                    break;
                }
                case 0xB5: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)(uint64_t)a.of.i64));
                    break;
                }
                case 0xB6: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f32((float)a.of.f64));
                    break;
                }
                case 0xB7: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64((double)a.of.i32));
                    break;
                }
                case 0xB8: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64((double)(uint32_t)a.of.i32));
                    break;
                }
                case 0xB9: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64((double)a.of.i64));
                    break;
                }
                case 0xBA: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64((double)(uint64_t)a.of.i64));
                    break;
                }
                case 0xBB: {
                    wasm_value_t a = WASM__POP(rt);
                    WASM__PUSH(rt, wasm_f64((double)a.of.f32));
                    break;
                }

                /* Reinterpretations */
                case 0xBC: {
                    wasm_value_t a = WASM__POP(rt);
                    int32_t v;
                    memcpy(&v, &a.of.f32, 4);
                    WASM__PUSH(rt, wasm_i32(v));
                    break;
                }
                case 0xBD: {
                    wasm_value_t a = WASM__POP(rt);
                    int64_t v;
                    memcpy(&v, &a.of.f64, 8);
                    WASM__PUSH(rt, wasm_i64(v));
                    break;
                }
                case 0xBE: {
                    wasm_value_t a = WASM__POP(rt);
                    float v;
                    memcpy(&v, &a.of.i32, 4);
                    WASM__PUSH(rt, wasm_f32(v));
                    break;
                }
                case 0xBF: {
                    wasm_value_t a = WASM__POP(rt);
                    double v;
                    memcpy(&v, &a.of.i64, 8);
                    WASM__PUSH(rt, wasm_f64(v));
                    break;
                }

                /* Sign extension */
                case 0xC0: {
                    wasm_value_t a = WASM__POP(rt);
                    uint32_t value = (uint32_t)a.of.i32 & 0xFFu;
                    if (value & 0x80u) value |= 0xFFFFFF00u;
                    WASM__PUSH(rt, wasm_i32((int32_t)value));
                    break;
                }
                case 0xC1: {
                    wasm_value_t a = WASM__POP(rt);
                    uint32_t value = (uint32_t)a.of.i32 & 0xFFFFu;
                    if (value & 0x8000u) value |= 0xFFFF0000u;
                    WASM__PUSH(rt, wasm_i32((int32_t)value));
                    break;
                }
                case 0xC2: {
                    wasm_value_t a = WASM__POP(rt);
                    uint64_t value = (uint64_t)a.of.i64 & 0xFFu;
                    if (value & 0x80u) value |= 0xFFFFFFFFFFFFFF00ull;
                    WASM__PUSH(rt, wasm_i64((int64_t)value));
                    break;
                }
                case 0xC3: {
                    wasm_value_t a = WASM__POP(rt);
                    uint64_t value = (uint64_t)a.of.i64 & 0xFFFFu;
                    if (value & 0x8000u) value |= 0xFFFFFFFFFFFF0000ull;
                    WASM__PUSH(rt, wasm_i64((int64_t)value));
                    break;
                }
                case 0xC4: {
                    wasm_value_t a = WASM__POP(rt);
                    uint64_t value = (uint64_t)a.of.i64 & 0xFFFFFFFFull;
                    if (value & 0x80000000ull) value |= 0xFFFFFFFF00000000ull;
                    WASM__PUSH(rt, wasm_i64((int64_t)value));
                    break;
                }

                default:
                    WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown opcode 0x%02X", op);
                    err = WASM_ERR_MALFORMED;
                    goto cleanup_frame;
            }

            if (r.malformed) WASM__TRAP(WASM_ERR_MALFORMED);
        }

        for (i = 0; i < ft->num_results; i++) results[ft->num_results - 1 - i] = WASM__POP(rt);
        rt->sp = sp_base;
        err = WASM_OK;

    cleanup_frame:
        while (lsp > 0) {
            wasm__clear_label(&labels[lsp - 1]);
            lsp--;
        }
        if (locals != locals_buf) WASM_FREE(locals);
        locals = NULL;
        if (restart) continue;
        goto cleanup;
    }

cleanup:
    while (lsp > 0) {
        wasm__clear_label(&labels[lsp - 1]);
        lsp--;
    }
    if (current_args_owned && current_args) WASM_FREE(current_args);
    return err;
}

/* ── Public call API ──────────────────────────────────────────────── */

wasm_error_t wasm_call(wasm_module_t* mod, const char* func_name,
                       const wasm_value_t* args, uint32_t num_args,
                       wasm_value_t* results, uint32_t num_results) {
    uint32_t idx;
    wasm_export_kind_t kind;

    if (!wasm_find_export(mod, func_name, &kind, &idx) || kind != WASM_EXPORT_FUNC) {
        WASM__SET_ERR(mod->rt, WASM_ERR_UNDEFINED_EXPORT, "export '%s' not found", func_name);
        return WASM_ERR_UNDEFINED_EXPORT;
    }
    return wasm_call_index(mod, idx, args, num_args, results, num_results);
}

wasm_error_t wasm_call_index(wasm_module_t* mod, uint32_t func_idx,
                             const wasm_value_t* args, uint32_t num_args,
                             wasm_value_t* results, uint32_t num_results) {
    wasm_functype_t* ft;

    if (func_idx >= mod->num_funcs) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "function index %u out of range", (unsigned)func_idx);
        return WASM_ERR_MALFORMED;
    }

    ft = &mod->types[mod->funcs[func_idx].type_idx];
    if (num_args != ft->num_params || num_results < ft->num_results ||
        (ft->num_params > 0 && !args) || (ft->num_results > 0 && !results)) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                      "call signature mismatch: expected %u args/%u results, got %u args/%u results",
                      (unsigned)ft->num_params, (unsigned)ft->num_results,
                      (unsigned)num_args, (unsigned)num_results);
        return WASM_ERR_TYPE_MISMATCH;
    }

    {
        wasm_error_t err = wasm__exec(mod, func_idx, (wasm_value_t*)args, num_args, results, num_results, 0);
        if (err == WASM__ERR_EXCEPTION) return wasm__report_uncaught_exception(mod->rt);
        return err;
    }
}

/* ── Memory helpers ───────────────────────────────────────────────── */

uint32_t wasm_memory_count(wasm_module_t* mod) {
    return mod ? mod->num_memories : 0;
}

uint8_t* wasm_memory_data_at(wasm_module_t* mod, uint32_t memory_index) {
    wasm_memory_t* memory = wasm__memory_at(mod, memory_index);
    return memory ? memory->data : NULL;
}

uint32_t wasm_memory_size_at(wasm_module_t* mod, uint32_t memory_index) {
    wasm_memory_t* memory = wasm__memory_at(mod, memory_index);
    return memory ? memory->pages * WASM_PAGE_SIZE : 0;
}

int32_t wasm_memory_grow_at(wasm_module_t* mod, uint32_t memory_index, uint32_t delta_pages) {
    wasm_memory_t* memory = wasm__memory_at(mod, memory_index);
    uint32_t old, np;
    uint8_t* nd;

    if (!memory) return -1;

    old = memory->pages;
    if (delta_pages > UINT32_MAX - old) return -1;
    np = old + delta_pages;
    if (memory->max_pages != UINT32_MAX && np > memory->max_pages) return -1;
    nd = (uint8_t*)WASM_REALLOC(memory->data, (size_t)np * WASM_PAGE_SIZE);
    if (!nd && np > 0) return -1;
    if (delta_pages > 0)
        memset(nd + (size_t)old * WASM_PAGE_SIZE, 0, (size_t)delta_pages * WASM_PAGE_SIZE);
    memory->data = nd;
    memory->pages = np;
    return (int32_t)old;
}

uint8_t* wasm_memory_data(wasm_module_t* mod) {
    return wasm_memory_data_at(mod, 0);
}
uint32_t wasm_memory_size(wasm_module_t* mod) {
    return wasm_memory_size_at(mod, 0);
}

int32_t wasm_memory_grow(wasm_module_t* mod, uint32_t delta_pages) {
    return wasm_memory_grow_at(mod, 0, delta_pages);
}

uint32_t wasm_export_count(wasm_module_t* mod) {
    return mod ? mod->num_exports : 0;
}

const char* wasm_export_name(wasm_module_t* mod, uint32_t index) {
    if (!mod || index >= mod->num_exports) return NULL;
    return mod->exports[index].name;
}

wasm_export_kind_t wasm_export_kind(wasm_module_t* mod, uint32_t index) {
    if (!mod || index >= mod->num_exports) return (wasm_export_kind_t)0;
    return mod->exports[index].kind;
}

uint32_t wasm_export_index(wasm_module_t* mod, uint32_t export_index) {
    if (!mod || export_index >= mod->num_exports) return 0;
    return mod->exports[export_index].index;
}

int wasm_find_export(wasm_module_t* mod, const char* name,
                     wasm_export_kind_t* out_kind, uint32_t* out_index) {
    uint32_t index;

    if (!mod || !name) return 0;

    for (index = 0; index < mod->num_exports; index++) {
        if (strcmp(mod->exports[index].name, name) == 0) {
            if (out_kind) *out_kind = mod->exports[index].kind;
            if (out_index) *out_index = mod->exports[index].index;
            return 1;
        }
    }

    return 0;
}

uint32_t wasm_func_count(wasm_module_t* mod) {
    return mod ? mod->num_funcs : 0;
}

uint32_t wasm_func_param_count(wasm_module_t* mod, uint32_t func_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return 0;
    ft = &mod->types[mod->funcs[func_idx].type_idx];
    return ft->num_params;
}

uint32_t wasm_func_result_count(wasm_module_t* mod, uint32_t func_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return 0;
    ft = &mod->types[mod->funcs[func_idx].type_idx];
    return ft->num_results;
}

wasm_valtype_t wasm_func_param_type(wasm_module_t* mod, uint32_t func_idx,
                                    uint32_t param_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return WASM_TYPE_VOID;
    ft = &mod->types[mod->funcs[func_idx].type_idx];
    if (param_idx >= ft->num_params) return WASM_TYPE_VOID;
    return ft->params[param_idx];
}

wasm_valtype_t wasm_func_result_type(wasm_module_t* mod, uint32_t func_idx,
                                     uint32_t result_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return WASM_TYPE_VOID;
    ft = &mod->types[mod->funcs[func_idx].type_idx];
    if (result_idx >= ft->num_results) return WASM_TYPE_VOID;
    return ft->results[result_idx];
}

const char* wasm_error_string(wasm_error_t err) {
    switch (err) {
        case WASM_OK:
            return "ok";
        case WASM_ERR_INVALID_MAGIC:
            return "invalid magic number";
        case WASM_ERR_INVALID_VERSION:
            return "invalid version";
        case WASM_ERR_MALFORMED:
            return "malformed module";
        case WASM_ERR_INVALID_SECTION:
            return "invalid section";
        case WASM_ERR_TYPE_MISMATCH:
            return "type mismatch";
        case WASM_ERR_UNKNOWN_IMPORT:
            return "unknown import";
        case WASM_ERR_UNDEFINED_EXPORT:
            return "undefined export";
        case WASM_ERR_STACK_OVERFLOW:
            return "stack overflow";
        case WASM_ERR_STACK_UNDERFLOW:
            return "stack underflow";
        case WASM_ERR_OUT_OF_BOUNDS:
            return "out of bounds memory access";
        case WASM_ERR_UNREACHABLE:
            return "unreachable executed";
        case WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH:
            return "indirect call type mismatch";
        case WASM_ERR_UNDEFINED_ELEMENT:
            return "undefined element";
        case WASM_ERR_UNINITIALIZED_TABLE:
            return "uninitialized table element";
        case WASM_ERR_DIV_BY_ZERO:
            return "integer divide by zero";
        case WASM_ERR_INT_OVERFLOW:
            return "integer overflow";
        case WASM_ERR_INVALID_CONVERSION:
            return "invalid conversion to integer";
        case WASM_ERR_CALL_STACK_EXHAUSTED:
            return "call stack exhausted";
        case WASM_ERR_OOM:
            return "out of memory";
        case WASM_ERR_TRAP:
            return "trap";
    }
    return "unknown error";
}

#endif /* WASM_IMPL */
#endif /* WASM_H */