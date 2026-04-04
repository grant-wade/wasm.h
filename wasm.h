/*
 * wasm.h — Single-header WebAssembly runtime for C99
 *
 * USAGE:
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
 * Targets: Wasm MVP spec (1.0)
 *   - i32, i64, f32, f64 value types
 *   - Linear memory with bounds checking
 *   - Tables (funcref)
 *   - Imports/exports
 *   - Block/loop/br/br_if/br_table control flow
 *   - No SIMD, threads, GC, multi-memory, tail calls
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

/* ── Value types ──────────────────────────────────────────────────── */
typedef enum wasm_valtype_t {
    WASM_TYPE_EXTERNREF = 0x6F,
    WASM_TYPE_FUNCREF = 0x70,
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

/* ── Internal function representation ─────────────────────────────── */
typedef struct wasm_func_t {
    uint32_t type_idx;
    uint32_t num_locals;
    wasm_valtype_t* locals;
    const uint8_t* code;
    uint32_t code_len;
    int is_import;
    wasm_host_func_t host_func;
    void* host_userdata;
} wasm_func_t;

/* ── Export entry ──────────────────────────────────────────────────── */
typedef enum wasm_export_kind_t {
    WASM_EXPORT_FUNC = 0x00,
    WASM_EXPORT_TABLE = 0x01,
    WASM_EXPORT_MEM = 0x02,
    WASM_EXPORT_GLOBAL = 0x03
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
    wasm_value_t value;
} wasm_global_t;

/* ── Table ────────────────────────────────────────────────────────── */
typedef struct wasm_table_t {
    uint32_t* elems;
    uint32_t size;
    uint32_t max_size;
} wasm_table_t;

/* ── Linear memory ────────────────────────────────────────────────── */
#define WASM_PAGE_SIZE 65536u

typedef struct wasm_memory_t {
    uint8_t* data;
    uint32_t pages;
    uint32_t max_pages;
} wasm_memory_t;

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
    wasm_table_t table;
    wasm_memory_t memory;
    uint32_t num_func_imports;
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
    wasm_value_t stack[WASM_MAX_STACK];
    uint32_t sp;
    wasm_error_t last_error;
    char error_msg[256];
};

/* ── Public API ───────────────────────────────────────────────────── */
wasm_error_t wasm_init(wasm_runtime_t* rt);
void wasm_destroy(wasm_runtime_t* rt);
wasm_error_t wasm_register_import(wasm_runtime_t* rt, const wasm_import_t* imp);
wasm_module_t* wasm_load(wasm_runtime_t* rt, const uint8_t* bytes, size_t len);
void wasm_free_module(wasm_module_t* mod);
wasm_error_t wasm_call(wasm_module_t* mod, const char* func_name,
                       const wasm_value_t* args, uint32_t num_args,
                       wasm_value_t* results, uint32_t num_results);
wasm_error_t wasm_call_index(wasm_module_t* mod, uint32_t func_idx,
                             const wasm_value_t* args, uint32_t num_args,
                             wasm_value_t* results, uint32_t num_results);
uint8_t* wasm_memory_data(wasm_module_t* mod);
uint32_t wasm_memory_size(wasm_module_t* mod);
int32_t wasm_memory_grow(wasm_module_t* mod, uint32_t delta_pages);
const char* wasm_error_string(wasm_error_t err);

#ifdef __cplusplus
}
#endif

/* ═══════════════════════════════════════════════════════════════════ */
#ifdef WASM_IMPL
/* ═══════════════════════════════════════════════════════════════════ */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ── LEB128 decoding ─────────────────────────────────────────────── */
typedef struct wasm__reader_t {
    const uint8_t* ptr;
    const uint8_t* end;
} wasm__reader_t;

static int wasm__has(wasm__reader_t* r, size_t n) {
    return (size_t)(r->end - r->ptr) >= n;
}

static uint8_t wasm__read_u8(wasm__reader_t* r) {
    return *r->ptr++;
}

static uint32_t wasm__read_leb128_u32(wasm__reader_t* r) {
    uint32_t result = 0, shift = 0;
    uint8_t byte;
    do {
        byte = wasm__read_u8(r);
        result |= (uint32_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return result;
}

static int32_t wasm__read_leb128_i32(wasm__reader_t* r) {
    int32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    do {
        byte = wasm__read_u8(r);
        result |= (int32_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    if (shift < 32 && (byte & 0x40)) result |= -(1 << shift);
    return result;
}

static int64_t wasm__read_leb128_i64(wasm__reader_t* r) {
    int64_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    do {
        byte = wasm__read_u8(r);
        result |= (int64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    if (shift < 64 && (byte & 0x40)) result |= -(((int64_t)1) << shift);
    return result;
}

static float wasm__read_f32(wasm__reader_t* r) {
    uint32_t bits = wasm__load_le32(r->ptr);
    float v;
    memcpy(&v, &bits, 4);
    r->ptr += 4;
    return v;
}

static double wasm__read_f64(wasm__reader_t* r) {
    uint64_t bits = wasm__load_le64(r->ptr);
    double v;
    memcpy(&v, &bits, 8);
    r->ptr += 8;
    return v;
}

static void wasm__read_name(wasm__reader_t* r, char* buf, size_t bufsz) {
    uint32_t len = wasm__read_leb128_u32(r);
    uint32_t copy = len < (uint32_t)(bufsz - 1) ? len : (uint32_t)(bufsz - 1);
    memcpy(buf, r->ptr, copy);
    buf[copy] = '\0';
    r->ptr += len;
}

static int wasm__is_valtype_byte(uint8_t byte) {
    switch (byte) {
        case 0x6F:
        case 0x70:
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
    free(ft->params);
    free(ft->results);
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
        dst->params = (wasm_valtype_t*)malloc(src->num_params * sizeof(wasm_valtype_t));
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
        dst->results = (wasm_valtype_t*)malloc(src->num_results * sizeof(wasm_valtype_t));
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

/* ── Init / Destroy ───────────────────────────────────────────────── */

wasm_error_t wasm_init(wasm_runtime_t* rt) {
    memset(rt, 0, sizeof(*rt));
    return WASM_OK;
}

void wasm_destroy(wasm_runtime_t* rt) {
    uint32_t i;

    for (i = 0; i < rt->num_imports; i++) wasm__free_functype(&rt->imports[i].type);
    free(rt->imports);
    memset(rt, 0, sizeof(*rt));
}

wasm_error_t wasm_register_import(wasm_runtime_t* rt, const wasm_import_t* imp) {
    wasm_import_t copy;
    wasm_error_t err;

    if (rt->num_imports >= rt->cap_imports) {
        uint32_t new_cap = rt->cap_imports ? rt->cap_imports * 2 : 16;
        wasm_import_t* a = (wasm_import_t*)realloc(rt->imports, new_cap * sizeof(wasm_import_t));
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

/* ── Section decoders ─────────────────────────────────────────────── */

static wasm_error_t wasm__decode_type_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);
    uint32_t i, p;
    if (count > 0) {
        mod->types = (wasm_functype_t*)calloc(count, sizeof(wasm_functype_t));
        if (!mod->types) return WASM_ERR_OOM;
    }
    mod->num_types = count;
    for (i = 0; i < count; i++) {
        wasm_functype_t* ft = &mod->types[i];
        if (wasm__read_u8(r) != 0x60) return WASM_ERR_MALFORMED;
        ft->num_params = wasm__read_leb128_u32(r);
        if (ft->num_params > 0) {
            ft->params = (wasm_valtype_t*)malloc(ft->num_params * sizeof(wasm_valtype_t));
            if (!ft->params) return WASM_ERR_OOM;
        }
        for (p = 0; p < ft->num_params; p++) ft->params[p] = (wasm_valtype_t)wasm__read_u8(r);
        ft->num_results = wasm__read_leb128_u32(r);
        if (ft->num_results > 0) {
            ft->results = (wasm_valtype_t*)malloc(ft->num_results * sizeof(wasm_valtype_t));
            if (!ft->results) return WASM_ERR_OOM;
        }
        for (p = 0; p < ft->num_results; p++) ft->results[p] = (wasm_valtype_t)wasm__read_u8(r);
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_import_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);
    uint32_t i, j;
    mod->funcs = (wasm_func_t*)calloc(count + 256, sizeof(wasm_func_t));
    if (!mod->funcs) return WASM_ERR_OOM;
    for (i = 0; i < count; i++) {
        char mn[128], fn[128];
        uint8_t kind;
        wasm__read_name(r, mn, sizeof(mn));
        wasm__read_name(r, fn, sizeof(fn));
        kind = wasm__read_u8(r);
        if (kind == 0x00) {
            uint32_t ti = wasm__read_leb128_u32(r);
            uint32_t fi = mod->num_funcs++;
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
            uint8_t lf;
            wasm__read_u8(r);
            lf = wasm__read_u8(r);
            mod->table.size = wasm__read_leb128_u32(r);
            mod->table.max_size = (lf & 1) ? wasm__read_leb128_u32(r) : 0;
        } else if (kind == 0x02) {
            uint8_t lf = wasm__read_u8(r);
            mod->memory.pages = wasm__read_leb128_u32(r);
            mod->memory.max_pages = (lf & 1) ? wasm__read_leb128_u32(r) : 65536;
        } else if (kind == 0x03) {
            wasm__read_u8(r);
            wasm__read_u8(r);
        }
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_function_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);
    uint32_t total = mod->num_funcs + count, i;
    wasm_func_t* nf = (wasm_func_t*)realloc(mod->funcs, total * sizeof(wasm_func_t));
    if (!nf) return WASM_ERR_OOM;
    mod->funcs = nf;
    for (i = 0; i < count; i++) {
        uint32_t fi = mod->num_funcs++;
        memset(&mod->funcs[fi], 0, sizeof(wasm_func_t));
        mod->funcs[fi].type_idx = wasm__read_leb128_u32(r);
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_table_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);
    if (count > 0) {
        uint8_t lf;
        wasm__read_u8(r);
        lf = wasm__read_u8(r);
        mod->table.size = wasm__read_leb128_u32(r);
        mod->table.max_size = (lf & 1) ? wasm__read_leb128_u32(r) : 0;
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_memory_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);
    if (count > 0) {
        uint8_t lf = wasm__read_u8(r);
        mod->memory.pages = wasm__read_leb128_u32(r);
        mod->memory.max_pages = (lf & 1) ? wasm__read_leb128_u32(r) : 65536;
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_global_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i;
    mod->globals = (wasm_global_t*)calloc(count, sizeof(wasm_global_t));
    if (!mod->globals && count) return WASM_ERR_OOM;
    mod->num_globals = count;
    for (i = 0; i < count; i++) {
        uint8_t op;
        mod->globals[i].type = (wasm_valtype_t)wasm__read_u8(r);
        mod->globals[i].is_mutable = wasm__read_u8(r);
        op = wasm__read_u8(r);
        switch (op) {
            case 0x41:
                mod->globals[i].value.of.i32 = wasm__read_leb128_i32(r);
                break;
            case 0x42:
                mod->globals[i].value.of.i64 = wasm__read_leb128_i64(r);
                break;
            case 0x43:
                mod->globals[i].value.of.f32 = wasm__read_f32(r);
                break;
            case 0x44:
                mod->globals[i].value.of.f64 = wasm__read_f64(r);
                break;
            default:
                break;
        }
        wasm__read_u8(r);
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_export_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i;
    mod->exports = (wasm_export_t*)calloc(count, sizeof(wasm_export_t));
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
    uint32_t count = wasm__read_leb128_u32(r), i, e;
    for (i = 0; i < count; i++) {
        uint32_t num_elems, offset = 0;
        uint8_t op;
        wasm__read_leb128_u32(r); /* table_idx */
        op = wasm__read_u8(r);
        if (op == 0x41)
            offset = (uint32_t)wasm__read_leb128_i32(r);
        else if (op == 0x23)
            offset = wasm__read_leb128_u32(r);
        wasm__read_u8(r);
        num_elems = wasm__read_leb128_u32(r);
        if (!mod->table.elems && mod->table.size > 0) {
            uint32_t te;
            mod->table.elems = (uint32_t*)malloc(mod->table.size * sizeof(uint32_t));
            if (!mod->table.elems) return WASM_ERR_OOM;
            for (te = 0; te < mod->table.size; te++) mod->table.elems[te] = UINT32_MAX;
        }
        for (e = 0; e < num_elems; e++) {
            uint32_t fi = wasm__read_leb128_u32(r);
            if (mod->table.elems && (offset + e) < mod->table.size) mod->table.elems[offset + e] = fi;
        }
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_code_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i;
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
        mod->funcs[fidx].locals = (wasm_valtype_t*)malloc(all_locals * sizeof(wasm_valtype_t));
        if (!mod->funcs[fidx].locals) return WASM_ERR_OOM;
        for (p = 0; p < ft->num_params; p++) mod->funcs[fidx].locals[p] = ft->params[p];
        r->ptr = ls;
        local_off = ft->num_params;
        for (d = 0; d < ndecls; d++) {
            uint32_t n = wasm__read_leb128_u32(r);
            wasm_valtype_t t = (wasm_valtype_t)wasm__read_u8(r);
            for (j = 0; j < n; j++) mod->funcs[fidx].locals[local_off++] = t;
        }
        mod->funcs[fidx].code = r->ptr;
        mod->funcs[fidx].code_len = (uint32_t)(body_size - (uint32_t)(r->ptr - body_start));
        r->ptr = body_start + body_size;
    }
    return WASM_OK;
}

static wasm_error_t wasm__decode_data_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r), i;
    for (i = 0; i < count; i++) {
        uint32_t offset = 0, size;
        uint8_t op;
        wasm__read_leb128_u32(r);
        op = wasm__read_u8(r);
        if (op == 0x41)
            offset = (uint32_t)wasm__read_leb128_i32(r);
        else if (op == 0x23) {
            uint32_t gi = wasm__read_leb128_u32(r);
            if (gi < mod->num_globals) offset = (uint32_t)mod->globals[gi].value.of.i32;
        }
        wasm__read_u8(r);
        size = wasm__read_leb128_u32(r);
        if (mod->memory.data && offset + size <= mod->memory.pages * WASM_PAGE_SIZE)
            memcpy(mod->memory.data + offset, r->ptr, size);
        r->ptr += size;
    }
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

/* ── Module loader ────────────────────────────────────────────────── */

wasm_module_t* wasm_load(wasm_runtime_t* rt, const uint8_t* bytes, size_t len) {
    wasm__reader_t r;
    uint32_t version;
    wasm_module_t* mod;
    wasm_error_t err = WASM_OK;

    r.ptr = bytes;
    r.end = bytes + len;
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

    mod = (wasm_module_t*)calloc(1, sizeof(wasm_module_t));
    if (!mod) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "module alloc");
        return NULL;
    }
    mod->rt = rt;
    mod->start_func = UINT32_MAX;

    while (r.ptr < r.end && err == WASM_OK) {
        uint8_t sid = wasm__read_u8(&r);
        uint32_t ssz = wasm__read_leb128_u32(&r);
        const uint8_t* se = r.ptr + ssz;
        wasm__reader_t sr;
        sr.ptr = r.ptr;
        sr.end = se;
        switch (sid) {
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
            default:
                break;
        }
        r.ptr = se;
    }
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    if (mod->memory.pages > 0) {
        mod->memory.data = (uint8_t*)calloc(mod->memory.pages, WASM_PAGE_SIZE);
        if (!mod->memory.data) {
            wasm_free_module(mod);
            return NULL;
        }
    }
    if (mod->table.size > 0 && !mod->table.elems) {
        uint32_t ti;
        mod->table.elems = (uint32_t*)malloc(mod->table.size * sizeof(uint32_t));
        if (!mod->table.elems) {
            wasm_free_module(mod);
            return NULL;
        }
        for (ti = 0; ti < mod->table.size; ti++) mod->table.elems[ti] = UINT32_MAX;
    }

    /* Second pass: data + element sections need allocated memory */
    r.ptr = bytes + 8;
    while (r.ptr < r.end) {
        uint8_t sid = wasm__read_u8(&r);
        uint32_t ssz = wasm__read_leb128_u32(&r);
        const uint8_t* se = r.ptr + ssz;
        wasm__reader_t sr;
        sr.ptr = r.ptr;
        sr.end = se;
        if (sid == 9) wasm__decode_element_section(mod, &sr);
        if (sid == 11) wasm__decode_data_section(mod, &sr);
        r.ptr = se;
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
    free(mod->types);
    for (i = 0; i < mod->num_funcs; i++) free(mod->funcs[i].locals);
    free(mod->funcs);
    free(mod->exports);
    free(mod->globals);
    free(mod->table.elems);
    free(mod->memory.data);
    free(mod);
}

/* ── Interpreter types ────────────────────────────────────────────── */

typedef struct wasm__label_t {
    const uint8_t* continuation;
    uint32_t sp_base;
    uint32_t param_arity;
    uint32_t arity;
    uint8_t opcode;
} wasm__label_t;

typedef struct wasm__blocktype_t {
    uint32_t param_arity;
    uint32_t result_arity;
} wasm__blocktype_t;

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

    blocktype->param_arity = 0;
    blocktype->result_arity = 0;

    if (lead == 0x40) {
        r->ptr++;
        return WASM_OK;
    }

    if (wasm__is_valtype_byte(lead)) {
        r->ptr++;
        blocktype->result_arity = 1;
        return WASM_OK;
    }

    {
        int32_t type_idx = wasm__read_leb128_i32(r);
        wasm_functype_t* ft;

        if (type_idx < 0 || (uint32_t)type_idx >= mod->num_types) return WASM_ERR_MALFORMED;
        ft = &mod->types[(uint32_t)type_idx];
        blocktype->param_arity = ft->num_params;
        blocktype->result_arity = ft->num_results;
    }

    return WASM_OK;
}

/* ── Block scanning ───────────────────────────────────────────────── */

static void wasm__skip_immediates(uint8_t op, wasm__reader_t* r) {
    switch (op) {
        case 0x02:
        case 0x03:
        case 0x04:
            wasm__skip_blocktype(r);
            break;
        case 0x0C:
        case 0x0D:
        case 0x10:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
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
        case 0x3E:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0E: {
            uint32_t n = wasm__read_leb128_u32(r), j;
            for (j = 0; j <= n; j++) wasm__read_leb128_u32(r);
            break;
        }
        case 0x11:
            wasm__read_leb128_u32(r);
            wasm__read_u8(r);
            break;
        case 0xFC:
            wasm__skip_prefixed_immediates(r);
            break;
        default:
            break;
    }
}

static const uint8_t* wasm__find_block_end(const uint8_t* start, const uint8_t* code_end, const uint8_t** else_ptr) {
    wasm__reader_t scan;
    int nest = 1;
    scan.ptr = start;
    scan.end = code_end;
    if (else_ptr) *else_ptr = NULL;
    while (nest > 0 && scan.ptr < scan.end) {
        uint8_t b = wasm__read_u8(&scan);
        if (b == 0x02 || b == 0x03 || b == 0x04) {
            nest++;
            wasm__skip_immediates(b, &scan);
        } else if (b == 0x05 && nest == 1) {
            if (else_ptr) *else_ptr = scan.ptr;
        } else if (b == 0x0B) {
            nest--;
        } else {
            wasm__skip_immediates(b, &scan);
        }
    }
    return scan.ptr;
}

/* ── Branch helper ────────────────────────────────────────────────── */

static wasm_error_t wasm__do_branch(wasm_runtime_t* rt, wasm__label_t* target,
                                    wasm__reader_t* r, uint32_t* label_sp, uint32_t depth_l) {
    wasm_value_t branch_values_buf[8];
    wasm_value_t* branch_values = branch_values_buf;
    uint32_t branch_arity = (target->opcode == 0x03) ? target->param_arity : target->arity;
    uint32_t i;

    if (branch_arity > 8) {
        branch_values = (wasm_value_t*)malloc(branch_arity * sizeof(wasm_value_t));
        if (!branch_values) return WASM_ERR_OOM;
    }

    for (i = 0; i < branch_arity; i++) branch_values[i] = rt->stack[rt->sp - branch_arity + i];
    rt->sp = target->sp_base;
    for (i = 0; i < branch_arity; i++) rt->stack[rt->sp++] = branch_values[i];
    r->ptr = target->continuation;

    if (branch_values != branch_values_buf) free(branch_values);
    if (target->opcode == 0x03)
        *label_sp -= depth_l;
    else
        *label_sp -= depth_l + 1;

    return WASM_OK;
}

/* ── Interpreter ──────────────────────────────────────────────────── */

#define WASM__MAX_LABELS 256
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
    wasm_func_t* func;
    wasm_functype_t* ft;
    wasm_value_t locals_buf[256];
    wasm_value_t* locals;
    uint32_t sp_base, i;
    wasm__reader_t r;
    wasm__label_t labels[WASM__MAX_LABELS];
    uint32_t lsp = 0;
    wasm_error_t err = WASM_OK;

    if (depth >= WASM_MAX_CALL_DEPTH) return WASM_ERR_CALL_STACK_EXHAUSTED;
    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;
    func = &mod->funcs[func_idx];
    ft = &mod->types[func->type_idx];

    if (func->is_import)
        return func->host_func(rt, args, num_args, results, num_results, func->host_userdata);

    locals = (func->num_locals <= 256) ? locals_buf : (wasm_value_t*)calloc(func->num_locals, sizeof(wasm_value_t));
    if (!locals) return WASM_ERR_OOM;
    memset(locals, 0, func->num_locals * sizeof(wasm_value_t));
    for (i = 0; i < ft->num_params && i < num_args; i++) {
        locals[i] = args[i];
        locals[i].type = ft->params[i];
    }
    for (i = ft->num_params; i < func->num_locals; i++) locals[i].type = func->locals[i];

    sp_base = rt->sp;
    r.ptr = func->code;
    r.end = func->code + func->code_len;

    /* Implicit function-level label */
    labels[lsp].continuation = r.end;
    labels[lsp].sp_base = sp_base;
    labels[lsp].param_arity = 0;
    labels[lsp].arity = ft->num_results;
    labels[lsp].opcode = 0x02;
    lsp++;

    while (r.ptr < r.end) {
        uint8_t op = wasm__read_u8(&r);
        switch (op) {
            case 0x00:
                WASM__TRAP(WASM_ERR_UNREACHABLE);
            case 0x01:
                break;

            case 0x02: { /* block */
                wasm__blocktype_t blocktype;
                const uint8_t* be = wasm__find_block_end(r.ptr, r.end, NULL);
                err = wasm__read_blocktype(mod, &r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (lsp >= WASM__MAX_LABELS) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                labels[lsp].continuation = be;
                labels[lsp].sp_base = rt->sp - blocktype.param_arity;
                labels[lsp].param_arity = blocktype.param_arity;
                labels[lsp].arity = blocktype.result_arity;
                labels[lsp].opcode = 0x02;
                lsp++;
                break;
            }
            case 0x03: { /* loop */
                wasm__blocktype_t blocktype;
                const uint8_t* ls;
                err = wasm__read_blocktype(mod, &r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                ls = r.ptr;
                wasm__find_block_end(r.ptr, r.end, NULL); /* skip past to register, but don't move r */
                if (lsp >= WASM__MAX_LABELS) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                labels[lsp].continuation = ls;
                labels[lsp].sp_base = rt->sp - blocktype.param_arity;
                labels[lsp].param_arity = blocktype.param_arity;
                labels[lsp].arity = blocktype.result_arity;
                labels[lsp].opcode = 0x03;
                lsp++;
                break;
            }
            case 0x04: { /* if */
                wasm__blocktype_t blocktype;
                const uint8_t* ep = NULL;
                const uint8_t* be = wasm__find_block_end(r.ptr, r.end, &ep);
                wasm_value_t cond = WASM__POP(rt);
                err = wasm__read_blocktype(mod, &r, &blocktype);
                if (err != WASM_OK) goto cleanup;
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
                labels[lsp].sp_base = rt->sp - blocktype.param_arity;
                labels[lsp].param_arity = blocktype.param_arity;
                labels[lsp].arity = blocktype.result_arity;
                labels[lsp].opcode = 0x04;
                lsp++;
                break;
            }
            case 0x05:
                if (lsp > 0) {
                    r.ptr = labels[lsp - 1].continuation;
                    lsp--;
                }
                break;
            case 0x0B:
                if (lsp > 0) lsp--;
                break;

            case 0x0C: {
                uint32_t d = wasm__read_leb128_u32(&r);
                if (d >= lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__do_branch(rt, &labels[lsp - 1 - d], &r, &lsp, d);
                if (err != WASM_OK) goto cleanup;
                break;
            }
            case 0x0D: {
                uint32_t d = wasm__read_leb128_u32(&r);
                wasm_value_t c = WASM__POP(rt);
                if (c.of.i32) {
                    if (d >= lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                    err = wasm__do_branch(rt, &labels[lsp - 1 - d], &r, &lsp, d);
                    if (err != WASM_OK) goto cleanup;
                }
                break;
            }
            case 0x0E: { /* br_table */
                uint32_t nt = wasm__read_leb128_u32(&r), j, d;
                uint32_t* tgts = (uint32_t*)malloc((nt + 1) * sizeof(uint32_t));
                wasm_value_t iv;
                if (!tgts) WASM__TRAP(WASM_ERR_OOM);
                for (j = 0; j <= nt; j++) tgts[j] = wasm__read_leb128_u32(&r);
                iv = WASM__POP(rt);
                d = ((uint32_t)iv.of.i32 < nt) ? tgts[iv.of.i32] : tgts[nt];
                free(tgts);
                if (d >= lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__do_branch(rt, &labels[lsp - 1 - d], &r, &lsp, d);
                if (err != WASM_OK) goto cleanup;
                break;
            }
            case 0x0F: /* return */
                for (i = 0; i < ft->num_results; i++) results[ft->num_results - 1 - i] = WASM__POP(rt);
                rt->sp = sp_base;
                goto cleanup;

            case 0x10: { /* call */
                uint32_t ci = wasm__read_leb128_u32(&r);
                wasm_functype_t* cft = &mod->types[mod->funcs[ci].type_idx];
                wasm_value_t call_args_buf[8], call_results_buf[8];
                wasm_value_t* call_args = call_args_buf;
                wasm_value_t* call_results = call_results_buf;
                int j;

                if (cft->num_params > 8) {
                    call_args = (wasm_value_t*)malloc(cft->num_params * sizeof(wasm_value_t));
                    if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                }
                if (cft->num_results > 8) {
                    call_results = (wasm_value_t*)malloc(cft->num_results * sizeof(wasm_value_t));
                    if (!call_results) {
                        if (call_args != call_args_buf) free(call_args);
                        WASM__TRAP(WASM_ERR_OOM);
                    }
                }
                if (cft->num_results > 0)
                    memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);
                err = wasm__exec(mod, ci, call_args, cft->num_params, call_results, cft->num_results, depth + 1);
                if (call_args != call_args_buf) free(call_args);
                if (err != WASM_OK) {
                    if (call_results != call_results_buf) free(call_results);
                    goto cleanup;
                }
                for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                if (call_results != call_results_buf) free(call_results);
                break;
            }
            case 0x11: { /* call_indirect */
                uint32_t ti = wasm__read_leb128_u32(&r), tvi, ci;
                wasm_functype_t* cft;
                wasm_value_t call_args_buf[8], call_results_buf[8], iv;
                wasm_value_t* call_args = call_args_buf;
                wasm_value_t* call_results = call_results_buf;
                int j;
                wasm__read_u8(&r);
                iv = WASM__POP(rt);
                tvi = (uint32_t)iv.of.i32;
                if (tvi >= mod->table.size || !mod->table.elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                ci = mod->table.elems[tvi];
                if (ci == UINT32_MAX) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                if (mod->funcs[ci].type_idx != ti) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                cft = &mod->types[ti];
                if (cft->num_params > 8) {
                    call_args = (wasm_value_t*)malloc(cft->num_params * sizeof(wasm_value_t));
                    if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                }
                if (cft->num_results > 8) {
                    call_results = (wasm_value_t*)malloc(cft->num_results * sizeof(wasm_value_t));
                    if (!call_results) {
                        if (call_args != call_args_buf) free(call_args);
                        WASM__TRAP(WASM_ERR_OOM);
                    }
                }
                if (cft->num_results > 0)
                    memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);
                err = wasm__exec(mod, ci, call_args, cft->num_params, call_results, cft->num_results, depth + 1);
                if (call_args != call_args_buf) free(call_args);
                if (err != WASM_OK) {
                    if (call_results != call_results_buf) free(call_results);
                    goto cleanup;
                }
                for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                if (call_results != call_results_buf) free(call_results);
                break;
            }

            case 0xFC: {
                uint32_t subop = wasm__read_leb128_u32(&r);
                switch (subop) {
                    default:
                        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown prefixed opcode 0xFC 0x%X", (unsigned)subop);
                        err = WASM_ERR_MALFORMED;
                        goto cleanup;
                }
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
                WASM__PUSH(rt, mod->globals[x].value);
                break;
            }
            case 0x24: {
                uint32_t x = wasm__read_leb128_u32(&r);
                mod->globals[x].value = WASM__POP(rt);
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
                uint32_t al, off;
                wasm_value_t base;
                uint64_t addr, msz;
                al = wasm__read_leb128_u32(&r);
                (void)al;
                off = wasm__read_leb128_u32(&r);
                base = WASM__POP(rt);
                addr = (uint64_t)(uint32_t)base.of.i32 + off;
                msz = (uint64_t)mod->memory.pages * WASM_PAGE_SIZE;
                switch (op) {
                    case 0x28: {
                        int32_t v;
                        if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int32_t)wasm__load_le32(mod->memory.data + addr);
                        WASM__PUSH(rt, wasm_i32(v));
                        break;
                    }
                    case 0x29: {
                        int64_t v;
                        if (addr + 8 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int64_t)wasm__load_le64(mod->memory.data + addr);
                        WASM__PUSH(rt, wasm_i64(v));
                        break;
                    }
                    case 0x2A: {
                        uint32_t b;
                        float v;
                        if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        b = wasm__load_le32(mod->memory.data + addr);
                        memcpy(&v, &b, 4);
                        WASM__PUSH(rt, wasm_f32(v));
                        break;
                    }
                    case 0x2B: {
                        uint64_t b;
                        double v;
                        if (addr + 8 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        b = wasm__load_le64(mod->memory.data + addr);
                        memcpy(&v, &b, 8);
                        WASM__PUSH(rt, wasm_f64(v));
                        break;
                    }
                    case 0x2C:
                        if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__PUSH(rt, wasm_i32((int32_t)(int8_t)mod->memory.data[addr]));
                        break;
                    case 0x2D:
                        if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__PUSH(rt, wasm_i32((int32_t)mod->memory.data[addr]));
                        break;
                    case 0x2E: {
                        int16_t v;
                        if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int16_t)wasm__load_le16(mod->memory.data + addr);
                        WASM__PUSH(rt, wasm_i32((int32_t)v));
                        break;
                    }
                    case 0x2F: {
                        uint16_t v;
                        if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le16(mod->memory.data + addr);
                        WASM__PUSH(rt, wasm_i32((int32_t)v));
                        break;
                    }
                    case 0x30:
                        if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__PUSH(rt, wasm_i64((int64_t)(int8_t)mod->memory.data[addr]));
                        break;
                    case 0x31:
                        if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__PUSH(rt, wasm_i64((int64_t)mod->memory.data[addr]));
                        break;
                    case 0x32: {
                        int16_t v;
                        if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int16_t)wasm__load_le16(mod->memory.data + addr);
                        WASM__PUSH(rt, wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x33: {
                        uint16_t v;
                        if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le16(mod->memory.data + addr);
                        WASM__PUSH(rt, wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x34: {
                        int32_t v;
                        if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int32_t)wasm__load_le32(mod->memory.data + addr);
                        WASM__PUSH(rt, wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x35: {
                        uint32_t v;
                        if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le32(mod->memory.data + addr);
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
                uint32_t al, off;
                wasm_value_t val, base;
                uint64_t addr, msz;
                al = wasm__read_leb128_u32(&r);
                (void)al;
                off = wasm__read_leb128_u32(&r);
                val = WASM__POP(rt);
                base = WASM__POP(rt);
                addr = (uint64_t)(uint32_t)base.of.i32 + off;
                msz = (uint64_t)mod->memory.pages * WASM_PAGE_SIZE;
                switch (op) {
                    case 0x36:
                        if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le32(mod->memory.data + addr, (uint32_t)val.of.i32);
                        break;
                    case 0x37:
                        if (addr + 8 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le64(mod->memory.data + addr, (uint64_t)val.of.i64);
                        break;
                    case 0x38: {
                        uint32_t b;
                        if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        memcpy(&b, &val.of.f32, 4);
                        wasm__store_le32(mod->memory.data + addr, b);
                        break;
                    }
                    case 0x39: {
                        uint64_t b;
                        if (addr + 8 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        memcpy(&b, &val.of.f64, 8);
                        wasm__store_le64(mod->memory.data + addr, b);
                        break;
                    }
                    case 0x3A:
                        if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        mod->memory.data[addr] = (uint8_t)val.of.i32;
                        break;
                    case 0x3B:
                        if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le16(mod->memory.data + addr, (uint16_t)val.of.i32);
                        break;
                    case 0x3C:
                        if (addr + 1 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        mod->memory.data[addr] = (uint8_t)val.of.i64;
                        break;
                    case 0x3D:
                        if (addr + 2 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le16(mod->memory.data + addr, (uint16_t)val.of.i64);
                        break;
                    case 0x3E:
                        if (addr + 4 > msz) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le32(mod->memory.data + addr, (uint32_t)val.of.i64);
                        break;
                    default:
                        break;
                }
                break;
            }

            case 0x3F:
                wasm__read_u8(&r);
                WASM__PUSH(rt, wasm_i32((int32_t)mod->memory.pages));
                break;
            case 0x40: {
                wasm_value_t d;
                int32_t res;
                wasm__read_u8(&r);
                d = WASM__POP(rt);
                res = wasm_memory_grow(mod, (uint32_t)d.of.i32);
                WASM__PUSH(rt, wasm_i32(res));
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
                WASM__PUSH(rt, wasm_i32((int32_t)(((uint32_t)a.of.i32 << k) | ((uint32_t)a.of.i32 >> (32u - k)))));
                break;
            }
            case 0x78: {
                wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                uint32_t k = (uint32_t)b.of.i32 & 31u;
                WASM__PUSH(rt, wasm_i32((int32_t)(((uint32_t)a.of.i32 >> k) | ((uint32_t)a.of.i32 << (32u - k)))));
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
                uint64_t k = (uint64_t)b.of.i64 & 63u;
                WASM__PUSH(rt, wasm_i64((int64_t)(((uint64_t)a.of.i64 << k) | ((uint64_t)a.of.i64 >> (64u - k)))));
                break;
            }
            case 0x8A: {
                wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                uint64_t k = (uint64_t)b.of.i64 & 63u;
                WASM__PUSH(rt, wasm_i64((int64_t)(((uint64_t)a.of.i64 >> k) | ((uint64_t)a.of.i64 << (64u - k)))));
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
                WASM__PUSH(rt, wasm_f32(a.of.f32 < b.of.f32 ? a.of.f32 : b.of.f32));
                break;
            }
            case 0x97: {
                wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                WASM__PUSH(rt, wasm_f32(a.of.f32 > b.of.f32 ? a.of.f32 : b.of.f32));
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
                WASM__PUSH(rt, wasm_f64(a.of.f64 < b.of.f64 ? a.of.f64 : b.of.f64));
                break;
            }
            case 0xA5: {
                wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
                WASM__PUSH(rt, wasm_f64(a.of.f64 > b.of.f64 ? a.of.f64 : b.of.f64));
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
                WASM__PUSH(rt, wasm_i32((int32_t)a.of.f32));
                break;
            }
            case 0xA9: {
                wasm_value_t a = WASM__POP(rt);
                WASM__PUSH(rt, wasm_i32((int32_t)(uint32_t)a.of.f32));
                break;
            }
            case 0xAA: {
                wasm_value_t a = WASM__POP(rt);
                WASM__PUSH(rt, wasm_i32((int32_t)a.of.f64));
                break;
            }
            case 0xAB: {
                wasm_value_t a = WASM__POP(rt);
                WASM__PUSH(rt, wasm_i32((int32_t)(uint32_t)a.of.f64));
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
                WASM__PUSH(rt, wasm_i64((int64_t)a.of.f32));
                break;
            }
            case 0xAF: {
                wasm_value_t a = WASM__POP(rt);
                WASM__PUSH(rt, wasm_i64((int64_t)(uint64_t)a.of.f32));
                break;
            }
            case 0xB0: {
                wasm_value_t a = WASM__POP(rt);
                WASM__PUSH(rt, wasm_i64((int64_t)a.of.f64));
                break;
            }
            case 0xB1: {
                wasm_value_t a = WASM__POP(rt);
                WASM__PUSH(rt, wasm_i64((int64_t)(uint64_t)a.of.f64));
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
                goto cleanup;
        }
    }

    for (i = 0; i < ft->num_results; i++) results[ft->num_results - 1 - i] = WASM__POP(rt);
    rt->sp = sp_base;

cleanup:
    if (locals != locals_buf) free(locals);
    return err;
}

/* ── Public call API ──────────────────────────────────────────────── */

wasm_error_t wasm_call(wasm_module_t* mod, const char* func_name,
                       const wasm_value_t* args, uint32_t num_args,
                       wasm_value_t* results, uint32_t num_results) {
    uint32_t i;
    for (i = 0; i < mod->num_exports; i++) {
        if (mod->exports[i].kind == WASM_EXPORT_FUNC &&
            strcmp(mod->exports[i].name, func_name) == 0)
            return wasm_call_index(mod, mod->exports[i].index, args, num_args, results, num_results);
    }
    WASM__SET_ERR(mod->rt, WASM_ERR_UNDEFINED_EXPORT, "export '%s' not found", func_name);
    return WASM_ERR_UNDEFINED_EXPORT;
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

    return wasm__exec(mod, func_idx, (wasm_value_t*)args, num_args, results, num_results, 0);
}

/* ── Memory helpers ───────────────────────────────────────────────── */

uint8_t* wasm_memory_data(wasm_module_t* mod) {
    return mod->memory.data;
}
uint32_t wasm_memory_size(wasm_module_t* mod) {
    return mod->memory.pages * WASM_PAGE_SIZE;
}

int32_t wasm_memory_grow(wasm_module_t* mod, uint32_t delta_pages) {
    uint32_t old = mod->memory.pages, np = old + delta_pages;
    uint8_t* nd;
    if (mod->memory.max_pages && np > mod->memory.max_pages) return -1;
    nd = (uint8_t*)realloc(mod->memory.data, (size_t)np * WASM_PAGE_SIZE);
    if (!nd) return -1;
    memset(nd + (size_t)old * WASM_PAGE_SIZE, 0, (size_t)delta_pages * WASM_PAGE_SIZE);
    mod->memory.data = nd;
    mod->memory.pages = np;
    return (int32_t)old;
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