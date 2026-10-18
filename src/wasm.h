/*
 * wasm.h — Single-header WebAssembly runtime for C99
 *
 * USAGE:
 *   #define WASM_IMPL
 *   #include "wasm.h"
 *
 * PLATFORM BRIDGES:
 *   Define WASM_ENABLE_PLATFORM to 1 before WASM_IMPL to compile the
 *   platform-backed WASI and Emscripten compatibility helpers.
 *
 * CUSTOM ALLOCATORS:
 *   #define WASM_MALLOC(sz)      my_malloc(sz)
 *   #define WASM_REALLOC(p, sz)  my_realloc(p, sz)
 *   #define WASM_FREE(p)         my_free(p)
 *   #define WASM_CALLOC(n, sz)   my_calloc(n, sz)
 *   #define WASM_GC_ALLOC(sz)    my_gc_alloc(sz)
 *   #define WASM_GC_FREE(p)      my_gc_free(p)
 *   #define WASM_IMPL
 *   #include "wasm.h"
 *
 * QUICK START:
 *   wasm_runtime_t *rt;
 *   wasm_module_t *mod;
 *   wasm_value_t args[1];
 *   wasm_value_t result;
 *
 *   rt = wasm_runtime_new(NULL);
 *   if (!rt || wasm_runtime_last_error(rt) != WASM_OK) {
 *       wasm_runtime_free(rt);
 *       return 1;
 *   }
 *   mod = wasm_load(rt, bytecode, bytecode_len);
 *   if (mod == NULL) {
 *       wasm_runtime_free(rt);
 *       return 1;
 *   }
 *
 *   args[0] = wasm_i32(42);
 *   if (wasm_call(mod, "main", args, 1, &result, 1) == WASM_OK) {
 *       printf("result = %d\n", result.of.i32);
 *   }
 *
 *   wasm_free_module(mod);
 *   wasm_runtime_free(rt);
 *
 * INTROSPECTION:
 *   After a successful wasm_load() into mod:
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
 *
 * COMPATIBILITY:
 *   - C99 or later (no compiler extensions required)
 *   - GCC, Clang, MSVC (2015+), TCC
 *   - Windows, Linux, macOS, *BSD, bare-metal (with libc)
 *   - Little-endian and big-endian
 *   - 32-bit and 64-bit
 *
 * Targets: Wasm 1.0 plus selected post-MVP proposals
 *   - i32, i64, f32, f64, funcref, and externref values
 *   - Imports/exports, including imported and mutable globals
 *   - Multiple linear memories with bounds checks, indexed memory access, and memory.grow
 *   - Tables, call_indirect, and reference-type table operations
 *   - Block/loop/if/br/br_if/br_table control flow with multi-value support
 *   - Bulk memory, sign-extension ops, non-trapping float-to-int conversions, extended const exprs, tail calls, exceptions, SIMD, and GC
 *   - Optional WASI stub bindings for common wasi_snapshot_preview1 imports
 *   - Load-time validation plus runtime-configurable feature gating
 *   - No threads
 *
 * MIT License
 *
 * Copyright (c) 2026 Grant Wade <grant@wade.software>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WASM_H
#define WASM_H

#if defined(_MSC_VER)
/* Optimized intrinsic paths make the retained scalar SIMD fallbacks appear
 * unreachable after MSVC inlining. Keep that optimizer diagnostic local to
 * this header so consumers can continue to use /W4 /WX. */
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

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

#if defined(_WIN32) && !defined(_CRT_RAND_S)
#define _CRT_RAND_S
#endif

#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifndef WASM_ENABLE_PLATFORM
#define WASM_ENABLE_PLATFORM 0
#endif

#define WASM_H_VERSION_MAJOR 1
#define WASM_H_VERSION_MINOR 1
#define WASM_H_VERSION_PATCH 0
#define WASM_H_VERSION_STRING "1.1.0"

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
    WASM_ERR_OUT_OF_FUEL,
    WASM_ERR_TRAP,
    /* The guest requested termination, such as WASI proc_exit; see wasm_runtime_exit_code. */
    WASM_ERR_EXIT
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
#define WASM_FEATURE_GC (1u << 11)
#define WASM_FEATURE_MEMORY64 (1u << 12)
/* Proposals not yet standardized are implemented but disabled by default. */
#define WASM_FEATURE_WIDE_ARITHMETIC (1u << 13)
#define WASM_FEATURE_CUSTOM_PAGE_SIZES (1u << 14)
#define WASM_FEATURE_COMPACT_IMPORTS (1u << 15)
#define WASM_FEATURE_THREADS (1u << 16)

/* ── Value types ──────────────────────────────────────────────────── */
typedef enum wasm_valtype_t {
    WASM_TYPE_NOEXN = 0x10,
    WASM_TYPE_EXNREF = 0x11,
    WASM_TYPE_NOFUNC = 0x0D,
    WASM_TYPE_NOEXTERN = 0x0E,
    WASM_TYPE_NONE = 0x0F,
    WASM_TYPE_ANYREF = 0x12,
    WASM_TYPE_EQREF = 0x13,
    WASM_TYPE_I31REF = 0x14,
    WASM_TYPE_STRUCTREF = 0x15,
    WASM_TYPE_ARRAYREF = 0x16,
    WASM_TYPE_EXTERNREF = 0x6F,
    WASM_TYPE_FUNCREF = 0x70,
    WASM_TYPE_V128 = 0x7B,
    WASM_TYPE_I32 = 0x7F,
    WASM_TYPE_I64 = 0x7E,
    WASM_TYPE_F32 = 0x7D,
    WASM_TYPE_F64 = 0x7C,
    WASM_TYPE_VOID = 0x40
} wasm_valtype_t;

typedef struct wasm_module_t wasm_module_t;
typedef struct wasm_gc_header_t wasm_gc_header_t;
typedef struct wasm_runtime_t wasm_runtime_t;
typedef struct wasm__val_frame_t wasm__val_frame_t;
typedef struct wasm__externref_box_t wasm__externref_box_t;
typedef struct wasm_func_t wasm_func_t;
typedef struct wasm_func_ref_t wasm_func_ref_t;
typedef struct wasm__call_frame_t wasm__call_frame_t;
typedef struct wasm__exception_object_t wasm__exception_object_t;
typedef struct wasm__emscripten_state_t wasm__emscripten_state_t;
typedef struct wasm__wasi_state_t wasm__wasi_state_t;
struct wasm_table_t;
struct wasm_memory_t;

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
        uintptr_t gc_ref;
        wasm_gc_header_t* gc_obj;
    } of;
} wasm_value_t;

WASM_INLINE wasm_value_t wasm_i32(int32_t v) {
    wasm_value_t r = { 0 };
    r.type = WASM_TYPE_I32;
    r.of.i32 = v;
    return r;
}

WASM_INLINE wasm_value_t wasm_i64(int64_t v) {
    wasm_value_t r = { 0 };
    r.type = WASM_TYPE_I64;
    r.of.i64 = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_f32(float v) {
    wasm_value_t r = { 0 };
    r.type = WASM_TYPE_F32;
    r.of.f32 = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_f64(double v) {
    wasm_value_t r = { 0 };
    r.type = WASM_TYPE_F64;
    r.of.f64 = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_v128(const uint8_t bytes[16]) {
    wasm_value_t r = { 0 };
    uint32_t i;

    r.type = WASM_TYPE_V128;
    for (i = 0; i < 16; i++) r.of.v128[i] = bytes ? bytes[i] : 0;
    return r;
}
WASM_INLINE wasm_value_t wasm_v128_zero(void) {
    return wasm_v128(NULL);
}
WASM_INLINE wasm_value_t wasm_funcref(uint32_t v) {
    wasm_value_t r = { 0 };
    r.type = WASM_TYPE_FUNCREF;
    r.of.funcref = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_externref(uintptr_t v) {
    wasm_value_t r = { 0 };
    r.type = WASM_TYPE_EXTERNREF;
    r.of.externref = v;
    return r;
}
WASM_INLINE wasm_value_t wasm_i31ref(int32_t v) {
    wasm_value_t r = { 0 };
    r.type = WASM_TYPE_I31REF;
    r.of.gc_ref = ((((uintptr_t)((uint32_t)v & 0x7FFFFFFFu))) << 1) | (uintptr_t)1u;
    return r;
}
WASM_INLINE wasm_value_t wasm_ref_null(wasm_valtype_t type) {
    wasm_value_t r = { 0 };
    r.type = type;
    switch (type) {
        case WASM_TYPE_FUNCREF:
        case WASM_TYPE_NOFUNC:
            r.of.funcref = UINT32_MAX;
            break;
        case WASM_TYPE_EXTERNREF:
        case WASM_TYPE_NOEXTERN:
        case WASM_TYPE_EXNREF:
        case WASM_TYPE_NOEXN:
            r.of.externref = (uintptr_t)0;
            break;
        default:
            r.of.gc_ref = (uintptr_t)0;
            break;
    }
    return r;
}

typedef struct wasm_reftype_t {
    wasm_valtype_t type;
    uint32_t type_index;
    int has_type_index;
    int nullable;
} wasm_reftype_t;

/* ── Host function callback ───────────────────────────────────────── */
typedef wasm_error_t (*wasm_host_func_t)(
    wasm_module_t* mod,
    const wasm_value_t* args, uint32_t num_args,
    wasm_value_t* results, uint32_t num_results,
    void* userdata);

/* ── Function type signature ──────────────────────────────────────── */
typedef struct wasm_functype_t {
    uint32_t num_params;
    uint32_t num_results;
    wasm_valtype_t* params;
    wasm_valtype_t* results;
    wasm_reftype_t* param_reftypes;
    wasm_reftype_t* result_reftypes;
} wasm_functype_t;

typedef enum wasm_packedtype_t {
    WASM_PACKED_I16 = 0x77,
    WASM_PACKED_I8 = 0x78
} wasm_packedtype_t;

typedef enum wasm_storagetype_kind_t {
    WASM_STORAGE_VALTYPE = 0,
    WASM_STORAGE_PACKED = 1
} wasm_storagetype_kind_t;

typedef struct wasm_storagetype_t {
    wasm_storagetype_kind_t kind;
    union {
        wasm_valtype_t valtype;
        wasm_packedtype_t packed_type;
    } of;
    wasm_reftype_t ref_type;
} wasm_storagetype_t;

typedef struct wasm_fieldtype_t {
    wasm_storagetype_t storage;
    int is_mutable;
} wasm_fieldtype_t;

typedef struct wasm_structtype_t {
    uint32_t num_fields;
    wasm_fieldtype_t* fields;
} wasm_structtype_t;

typedef struct wasm_arraytype_t {
    wasm_fieldtype_t field;
} wasm_arraytype_t;

typedef enum wasm_comptype_kind_t {
    WASM_COMP_FUNC = 0,
    WASM_COMP_STRUCT = 1,
    WASM_COMP_ARRAY = 2,
    WASM_COMP_INVALID = 0xFF
} wasm_comptype_kind_t;

typedef struct wasm_comptype_t {
    wasm_comptype_kind_t kind;
    uint32_t num_supertypes;
    uint32_t* supertypes;
    uint32_t rec_group;
    uint32_t canonical_id;
    int is_final;
    union {
        wasm_functype_t func;
        wasm_structtype_t struct_;
        wasm_arraytype_t array;
    } of;
} wasm_comptype_t;

typedef struct wasm_recgroup_t {
    uint32_t first_type;
    uint32_t num_types;
} wasm_recgroup_t;

/* ── Import binding ───────────────────────────────────────────────── */
typedef struct wasm_import_t {
    const char* module;
    const char* name;
    wasm_functype_t type;
    wasm_module_t* type_module;
    uint32_t type_index;
    int has_type_context;
    wasm_host_func_t func;
    void* userdata;
} wasm_import_t;

typedef enum wasm_import_kind_t {
    WASM_IMPORT_FUNC = 0x00,
    WASM_IMPORT_TABLE = 0x01,
    WASM_IMPORT_MEM = 0x02,
    WASM_IMPORT_GLOBAL = 0x03,
    WASM_IMPORT_TAG = 0x04
} wasm_import_kind_t;

typedef struct wasm_import_info_t {
    char* module;
    char* name;
    wasm_import_kind_t kind;
    uint32_t index;
    uint32_t type_index;
    wasm_valtype_t type;
    wasm_reftype_t ref_type;
    int is_mutable;
} wasm_import_info_t;

typedef struct wasm_global_import_t {
    const char* module;
    const char* name;
    wasm_valtype_t type;
    wasm_reftype_t ref_type;
    int is_mutable;
    wasm_value_t* value;
} wasm_global_import_t;

/* ── Export entry ──────────────────────────────────────────────────── */
typedef enum wasm_export_kind_t {
    WASM_EXPORT_FUNC = 0x00,
    WASM_EXPORT_TABLE = 0x01,
    WASM_EXPORT_MEM = 0x02,
    WASM_EXPORT_GLOBAL = 0x03,
    WASM_EXPORT_TAG = 0x04
} wasm_export_kind_t;

typedef struct wasm_export_t {
    char* name;
    uint32_t name_len;
    wasm_export_kind_t kind;
    uint32_t index;
} wasm_export_t;

/* ── Global variable ──────────────────────────────────────────────── */
typedef struct wasm_global_t {
    wasm_valtype_t type;
    wasm_reftype_t ref_type;
    int is_mutable;
    int is_import;
    wasm_value_t* import_value;
    wasm_value_t value;
} wasm_global_t;

/* ── Table ────────────────────────────────────────────────────────── */
typedef struct wasm_table_t {
    wasm_valtype_t reftype;
    wasm_reftype_t reftype_info;
    wasm_value_t* elems;
    wasm_value_t default_value;
    uint32_t size;
    uint64_t max_size;
    int is_64;
    int is_import;
    struct wasm_table_t* import_table;
} wasm_table_t;

typedef struct wasm_data_segment_t {
    uint8_t* bytes;
    uint32_t size;
    uint32_t memory_index;
    uint64_t offset;
    int is_passive;
    int is_dropped;
} wasm_data_segment_t;

typedef struct wasm_elem_segment_t {
    wasm_valtype_t elem_type;
    wasm_reftype_t elem_ref_type;
    wasm_value_t* elems;
    uint32_t num_elems;
    uint32_t table_index;
    uint64_t offset;
    int uses_expr;
    int is_passive;
    int is_declarative;
    int is_dropped;
} wasm_elem_segment_t;

/* ── Linear memory ────────────────────────────────────────────────── */
#define WASM_PAGE_SIZE 65536u
#define WASM_MEMORY32_MAX_PAGES 65536u
#define WASM_MEMORY64_MAX_PAGES (UINT64_C(1) << 48)

typedef struct wasm_memory_t {
    uint8_t* data;
    uint64_t pages;
    uint64_t max_pages;
    int is_64;
    int is_import;
    struct wasm_memory_t* import_memory;
    uint32_t page_size; /* bytes per page, 1 or WASM_PAGE_SIZE; 0 means WASM_PAGE_SIZE */
    int is_shared;          /* shared memories never move and may be used by runtimes on other threads */
    uint64_t reserved_bytes; /* shared memories: size of the fixed reservation backing data */
    size_t shared_size;      /* shared memories: current size in bytes, read atomically while others grow it */
} wasm_memory_t;

typedef struct wasm_table_import_t {
    const char* module;
    const char* name;
    wasm_table_t* table;
} wasm_table_import_t;

typedef struct wasm_memory_import_t {
    const char* module;
    const char* name;
    wasm_memory_t* memory;
} wasm_memory_import_t;

typedef struct wasm_tag_import_t {
    const char* module;
    const char* name;
    wasm_functype_t type;
    wasm_module_t* type_module;
    uint32_t type_index;
    int has_type_context;
    uint64_t identity;
} wasm_tag_import_t;

typedef struct wasm_tag_t {
    uint32_t type_idx;
    uint64_t identity;
} wasm_tag_t;

typedef struct wasm_custom_section_t {
    char* name;
    const uint8_t* data;
    uint32_t size;
} wasm_custom_section_t;

typedef struct wasm__exception_state_t {
    uint32_t tag_index;
    uint64_t tag_identity;
    wasm_value_t* values;
    uint32_t num_values;
    int is_pending;
} wasm__exception_state_t;

/* ── Runtime configuration ────────────────────────────────────────── */
#ifndef WASM_MAX_STACK
#define WASM_MAX_STACK 4096
#endif
#ifndef WASM_MAX_CALL_DEPTH
#define WASM_MAX_CALL_DEPTH 512
#endif
#ifndef WASM_MAX_LABELS
#define WASM_MAX_LABELS 4096
#endif
#ifndef WASM_GC_HEAP_SIZE
#define WASM_GC_HEAP_SIZE (4u * 1024u * 1024u)
#endif
#ifndef WASM_FRAME_ARENA_SIZE
#define WASM_FRAME_ARENA_SIZE                                           \
        ((((size_t)WASM_MAX_CALL_DEPTH * (size_t)1024u) >                   \
            (size_t)(4u * 1024u * 1024u))                                     \
                 ? ((size_t)WASM_MAX_CALL_DEPTH * (size_t)1024u)                \
                 : (size_t)(4u * 1024u * 1024u))
#endif

typedef struct wasm_config_t {
    uint32_t max_stack_values;
    uint32_t max_call_depth;
    uint32_t max_labels;
    size_t initial_gc_heap_size;
    size_t frame_arena_size;
    int lazy_validation;
} wasm_config_t;

/* ── Public API ───────────────────────────────────────────────────── */
void wasm_config_default(wasm_config_t* config);
wasm_runtime_t* wasm_runtime_new(const wasm_config_t* config);
void wasm_runtime_free(wasm_runtime_t* rt);
wasm_error_t wasm_init(wasm_runtime_t* rt, const wasm_config_t* config);
void wasm_destroy(wasm_runtime_t* rt);
wasm_error_t wasm_runtime_last_error(const wasm_runtime_t* rt);
const char* wasm_runtime_error_message(const wasm_runtime_t* rt);
void wasm_runtime_clear_error(wasm_runtime_t* rt);
int32_t wasm_runtime_exit_code(const wasm_runtime_t* rt);
void wasm_runtime_set_error(wasm_runtime_t* rt, wasm_error_t err, const char* message);
void wasm_runtime_set_enabled_features(wasm_runtime_t* rt, uint32_t enabled_features);
uint32_t wasm_runtime_enabled_features(const wasm_runtime_t* rt);
wasm_error_t wasm_register_import(wasm_runtime_t* rt, const wasm_import_t* imp);
wasm_error_t wasm_register_global_import(wasm_runtime_t* rt, const wasm_global_import_t* imp);
wasm_error_t wasm_register_table_import(wasm_runtime_t* rt, const wasm_table_import_t* imp);
wasm_error_t wasm_register_memory_import(wasm_runtime_t* rt, const wasm_memory_import_t* imp);
wasm_error_t wasm_register_tag_import(wasm_runtime_t* rt, const wasm_tag_import_t* imp);
void wasm_enable_feature(wasm_runtime_t* rt, uint32_t flag);
void wasm_disable_feature(wasm_runtime_t* rt, uint32_t flag);
void wasm_enable_all_features(wasm_runtime_t* rt);
void wasm_gc_collect(wasm_runtime_t* rt);
wasm_module_t* wasm_load(wasm_runtime_t* rt, const uint8_t* bytes, size_t len);
void wasm_free_module(wasm_module_t* mod);
wasm_error_t wasm_call(wasm_module_t* mod, const char* func_name,
                       const wasm_value_t* args, uint32_t num_args,
                       wasm_value_t* results, uint32_t num_results);
wasm_error_t wasm_call_index(wasm_module_t* mod, uint32_t func_idx,
                             const wasm_value_t* args, uint32_t num_args,
                             wasm_value_t* results, uint32_t num_results);
wasm_error_t wasm_call_fmt(wasm_module_t* mod, const char* func_name, const char* fmt, ...);
wasm_error_t wasm_bind_host_func(wasm_runtime_t* rt, const char* module_name,
                                 const char* func_name, const char* fmt,
                                 wasm_host_func_t callback, void* userdata);
wasm_error_t wasm_bind_emscripten_stubs(wasm_runtime_t* rt);
wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt);
uint32_t wasm_memory_count(wasm_module_t* mod);
wasm_error_t wasm_memory_read(wasm_module_t* mod, uint32_t memory_index,
                              uint64_t offset, void* dst, size_t len);
wasm_error_t wasm_memory_write(wasm_module_t* mod, uint32_t memory_index,
                               uint64_t offset, const void* src, size_t len);
wasm_error_t wasm_memory_get_string(wasm_module_t* mod, uint32_t memory_index,
                                    uint64_t offset, char* dst, size_t max_len);
wasm_memory_t* wasm_memory_ref_at(wasm_module_t* mod, uint32_t memory_index);
uint8_t* wasm_memory_data_at(wasm_module_t* mod, uint32_t memory_index);
uint64_t wasm_memory_size_at(wasm_module_t* mod, uint32_t memory_index);
int64_t wasm_memory_grow_at(wasm_module_t* mod, uint32_t memory_index, uint64_t delta_pages);
uint8_t* wasm_memory_data(wasm_module_t* mod);
uint64_t wasm_memory_size(wasm_module_t* mod);
/* Creates a shared memory (threads proposal) for hosts to register as an import in several runtimes,
   each used by its own thread. It must outlive every module that imports it. */
wasm_memory_t* wasm_shared_memory_new(uint64_t pages, uint64_t max_pages, int is_64);
void wasm_shared_memory_free(wasm_memory_t* memory);
int64_t wasm_memory_grow(wasm_module_t* mod, uint64_t delta_pages);
uint32_t wasm_table_count(wasm_module_t* mod);
wasm_table_t* wasm_table_ref_at(wasm_module_t* mod, uint32_t table_idx);
wasm_valtype_t wasm_table_type(wasm_module_t* mod, uint32_t table_idx);
const wasm_reftype_t* wasm_table_reftype(wasm_module_t* mod, uint32_t table_idx);
uint32_t wasm_global_count(wasm_module_t* mod);
wasm_valtype_t wasm_global_type(wasm_module_t* mod, uint32_t global_idx);
const wasm_reftype_t* wasm_global_reftype(wasm_module_t* mod, uint32_t global_idx);
int wasm_global_is_mutable(wasm_module_t* mod, uint32_t global_idx);
wasm_value_t* wasm_global_value_ref_at(wasm_module_t* mod, uint32_t global_idx);
wasm_error_t wasm_global_get(wasm_module_t* mod, const char* name, wasm_value_t* out_val);
wasm_error_t wasm_global_set(wasm_module_t* mod, const char* name, wasm_value_t val);
uint32_t wasm_import_count(wasm_module_t* mod);
const wasm_import_info_t* wasm_import_info(wasm_module_t* mod, uint32_t index);
uint32_t wasm_export_count(wasm_module_t* mod);
const char* wasm_export_name(wasm_module_t* mod, uint32_t index);
const uint8_t* wasm_export_name_bytes(wasm_module_t* mod, uint32_t index, size_t* out_len);
wasm_export_kind_t wasm_export_kind(wasm_module_t* mod, uint32_t index);
uint32_t wasm_export_index(wasm_module_t* mod, uint32_t export_index);
int wasm_find_export(wasm_module_t* mod, const char* name,
                     wasm_export_kind_t* out_kind, uint32_t* out_index);
int wasm_find_export_bytes(wasm_module_t* mod, const uint8_t* name, size_t name_len,
                           wasm_export_kind_t* out_kind, uint32_t* out_index);
uint32_t wasm_custom_section_count(wasm_module_t* mod);
const char* wasm_custom_section_name(wasm_module_t* mod, uint32_t index);
const uint8_t* wasm_custom_section_data(wasm_module_t* mod, uint32_t index, size_t* out_len);
const uint8_t* wasm_custom_section_find(wasm_module_t* mod, const char* name, size_t* out_len);
uint32_t wasm_type_count(wasm_module_t* mod);
wasm_comptype_kind_t wasm_type_kind(wasm_module_t* mod, uint32_t type_idx);
const wasm_functype_t* wasm_type_functype(wasm_module_t* mod, uint32_t type_idx);
uint32_t wasm_struct_type_field_count(wasm_module_t* mod, uint32_t type_idx);
const wasm_fieldtype_t* wasm_struct_type_field(wasm_module_t* mod, uint32_t type_idx,
                                               uint32_t field_idx);
const wasm_fieldtype_t* wasm_array_type_field(wasm_module_t* mod, uint32_t type_idx);
uint32_t wasm_func_count(wasm_module_t* mod);
const wasm_functype_t* wasm_func_functype(wasm_module_t* mod, uint32_t func_idx);
uint32_t wasm_func_param_count(wasm_module_t* mod, uint32_t func_idx);
uint32_t wasm_func_result_count(wasm_module_t* mod, uint32_t func_idx);
wasm_valtype_t wasm_func_param_type(wasm_module_t* mod, uint32_t func_idx,
                                    uint32_t param_idx);
wasm_valtype_t wasm_func_result_type(wasm_module_t* mod, uint32_t func_idx,
                                     uint32_t result_idx);
uint32_t wasm_tag_count(wasm_module_t* mod);
const wasm_functype_t* wasm_tag_functype(wasm_module_t* mod, uint32_t tag_idx);
wasm_error_t wasm_struct_new(wasm_module_t* mod, uint32_t type_idx,
                             const wasm_value_t* field_values, uint32_t num_fields,
                             wasm_value_t* out_ref);
wasm_error_t wasm_array_new(wasm_module_t* mod, uint32_t type_idx,
                            const wasm_value_t* elem_values, uint32_t num_elems,
                            wasm_value_t* out_ref);
wasm_error_t wasm_struct_get_field(wasm_module_t* mod, wasm_value_t struct_ref,
                                   uint32_t field_idx, wasm_value_t* out_val);
wasm_error_t wasm_struct_set_field(wasm_module_t* mod, wasm_value_t struct_ref,
                                   uint32_t field_idx, wasm_value_t val);
wasm_error_t wasm_array_length(wasm_module_t* mod, wasm_value_t array_ref,
                               uint32_t* out_len);
wasm_error_t wasm_array_get_elem(wasm_module_t* mod, wasm_value_t array_ref,
                                 uint32_t index, wasm_value_t* out_val);
wasm_error_t wasm_array_set_elem(wasm_module_t* mod, wasm_value_t array_ref,
                                 uint32_t index, wasm_value_t val);
void wasm_dump_backtrace(wasm_runtime_t* rt, char* buffer, size_t buffer_size);
uint32_t wasm_get_call_stack_depth(wasm_runtime_t* rt);
wasm_error_t wasm_get_call_frame_info(wasm_runtime_t* rt, uint32_t depth,
                                      uint32_t* out_func_idx, uint32_t* out_offset);
void wasm_set_fuel(wasm_runtime_t* rt, uint64_t fuel);
uint64_t wasm_get_fuel(wasm_runtime_t* rt);
const wasm_runtime_t* wasm_module_runtime(const wasm_module_t* mod);
void wasm_module_set_userdata(wasm_module_t* mod, void* userdata);
void* wasm_module_get_userdata(wasm_module_t* mod);
const char* wasm_error_string(wasm_error_t err);

#ifdef __cplusplus
}
#endif


#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif /* WASM_H */
