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
 *   #define WASM_GC_ALLOC(sz)    my_gc_alloc(sz)
 *   #define WASM_GC_FREE(p)      my_gc_free(p)
 *   #define WASM_IMPL
 *   #include "wasm.h"
 *
 * QUICK START:
 *   wasm_runtime_t rt;
 *   wasm_module_t *mod;
 *   wasm_value_t args[1];
 *   wasm_value_t result;
 *
 *   if (wasm_init(&rt, NULL) != WASM_OK) return 1;
 *   mod = wasm_load(&rt, bytecode, bytecode_len);
 *   if (mod == NULL) {
 *       wasm_destroy(&rt);
 *       return 1;
 *   }
 *
 *   args[0] = wasm_i32(42);
 *   if (wasm_call(mod, "main", args, 1, &result, 1) == WASM_OK) {
 *       printf("result = %d\n", result.of.i32);
 *   }
 *
 *   wasm_free_module(mod);
 *   wasm_destroy(&rt);
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
 * LICENSE: MIT
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
 */

#ifndef WASM_H
#define WASM_H

/* ── Platform detection & compat shims ────────────────────────────── */

#if defined(_MSC_VER)
#if _MSC_VER < 1900
#error "wasm.h requires MSVC 2015 (v19.0) or later"
#endif
#ifndef _CRT_RAND_S
#define _CRT_RAND_S
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
#include <stdarg.h>
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
    WASM_ERR_OUT_OF_FUEL,
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
#define WASM_FEATURE_GC (1u << 11)

/* ── Value types ──────────────────────────────────────────────────── */
typedef enum wasm_valtype_t {
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
typedef struct wasm__val_frame_t wasm__val_frame_t;
struct wasm_table_t;
struct wasm_memory_t;

WASM_INLINE int wasm__uses_funcref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_FUNCREF || type == WASM_TYPE_NOFUNC;
}

WASM_INLINE int wasm__uses_externref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_EXTERNREF || type == WASM_TYPE_NOEXTERN;
}

WASM_INLINE int wasm__uses_gc_ref_storage(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_NONE:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return 1;
        default:
            return 0;
    }
}

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
WASM_INLINE wasm_value_t wasm_i31ref(int32_t v) {
    wasm_value_t r;
    r.type = WASM_TYPE_I31REF;
    r.of.gc_ref = ((((uintptr_t)((uint32_t)v & 0x7FFFFFFFu))) << 1) | (uintptr_t)1u;
    return r;
}
WASM_INLINE wasm_value_t wasm_ref_null(wasm_valtype_t type) {
    wasm_value_t r;
    r.type = type;
    if (wasm__uses_funcref_storage(type))
        r.of.funcref = UINT32_MAX;
    else if (wasm__uses_externref_storage(type))
        r.of.externref = (uintptr_t)0;
    else
        r.of.gc_ref = (uintptr_t)0;
    return r;
}

typedef enum wasm_gc_object_kind_t {
    WASM_GC_OBJ_STRUCT = 1,
    WASM_GC_OBJ_ARRAY = 2
} wasm_gc_object_kind_t;

struct wasm_gc_header_t {
    wasm_gc_header_t* next_alloc;
    size_t object_size;
    wasm_module_t* module;
    uint32_t type_index;
    uint8_t kind;
    uint8_t marked;
    uint16_t reserved;
};

typedef struct wasm_gc_struct_t {
    wasm_gc_header_t header;
    uint8_t payload[1];
} wasm_gc_struct_t;

typedef struct wasm_gc_array_t {
    wasm_gc_header_t header;
    uint32_t length;
    uint8_t payload[1];
} wasm_gc_array_t;

#define WASM__GC_STRUCT_HEADER_SIZE offsetof(wasm_gc_struct_t, payload)
#define WASM__GC_ARRAY_HEADER_SIZE offsetof(wasm_gc_array_t, payload)

/* ── Forward declarations ─────────────────────────────────────────── */
typedef struct wasm_runtime_t wasm_runtime_t;
#ifdef WASM_IMPL
static wasm_error_t wasm__resolve_module_reftypes(wasm_module_t* mod);
static int wasm__is_heap_subtype(const wasm_module_t* mod,
                                 wasm_valtype_t subtype,
                                 wasm_valtype_t supertype);
static inline int wasm__is_subtype(const wasm_module_t* mod,
                                   uint32_t subtype_idx,
                                   uint32_t supertype_idx);
static int wasm__is_valtype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype);
static struct wasm_table_t* wasm__table_actual(struct wasm_table_t* table);
static const struct wasm_table_t* wasm__table_actual_const(const struct wasm_table_t* table);
static struct wasm_memory_t* wasm__memory_actual(struct wasm_memory_t* memory);
static const struct wasm_memory_t* wasm__memory_actual_const(const struct wasm_memory_t* memory);
#endif

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

typedef struct wasm__call_frame_t wasm__call_frame_t;

typedef struct wasm__control_target_t {
    uint32_t end_offset;
    uint32_t aux_offset;
} wasm__control_target_t;

/* ── Internal function representation ─────────────────────────────── */
typedef struct wasm_func_t {
    uint32_t type_idx;
    uint32_t num_locals;
    wasm_valtype_t* locals;
    wasm_reftype_t* local_reftypes;
    const uint8_t* code;
    uint32_t code_len;
    wasm__control_target_t* control_targets;
    uint32_t max_label_depth;
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
    uint32_t size;
    uint32_t max_size;
    int is_import;
    struct wasm_table_t* import_table;
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
    wasm_reftype_t elem_ref_type;
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
    int is_import;
    struct wasm_memory_t* import_memory;
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
} wasm_tag_import_t;

typedef struct wasm_tag_t {
    uint32_t type_idx;
} wasm_tag_t;

typedef struct wasm_custom_section_t {
    char* name;
    const uint8_t* data;
    uint32_t size;
} wasm_custom_section_t;

typedef struct wasm__exception_state_t {
    uint32_t tag_index;
    wasm_value_t* values;
    uint32_t num_values;
    int is_pending;
} wasm__exception_state_t;

/* ── Module ───────────────────────────────────────────────────────── */
struct wasm_module_t {
    wasm_runtime_t* rt;
    void* host_userdata;
    uint8_t* module_bytes;
    size_t module_size;
    wasm_comptype_t* types;
    uint32_t num_types;
    wasm_recgroup_t* rec_groups;
    uint32_t num_rec_groups;
    wasm_func_t* funcs;
    uint32_t num_funcs;
    wasm_import_info_t* imports;
    uint32_t num_imports;
    wasm_export_t* exports;
    uint32_t num_exports;
    wasm_global_t* globals;
    uint32_t num_globals;
    wasm_tag_t* tags;
    uint32_t num_tags;
    wasm_table_t* tables;
    uint32_t num_tables;
    wasm_memory_t* memories;
    wasm_custom_section_t* custom_sections;
    uint32_t num_custom_sections;
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
    wasm_module_t* gc_prev;
    wasm_module_t* gc_next;
};

/* ── Runtime configuration ────────────────────────────────────────── */
#ifndef WASM_MAX_STACK
#define WASM_MAX_STACK 4096
#endif
#ifndef WASM_MAX_CALL_DEPTH
#define WASM_MAX_CALL_DEPTH 512
#endif
#ifndef WASM_MAX_LABELS
#define WASM_MAX_LABELS 256
#endif
#ifndef WASM__FRAME_ARENA_ESTIMATED_FRAME_SIZE
#define WASM__FRAME_ARENA_ESTIMATED_FRAME_SIZE 1024u
#endif
#ifndef WASM__FRAME_ARENA_MIN_SIZE
#define WASM__FRAME_ARENA_MIN_SIZE (4u * 1024u * 1024u)
#endif
#ifndef WASM_GC_HEAP_SIZE
#define WASM_GC_HEAP_SIZE (4u * 1024u * 1024u)
#endif
#ifndef WASM_FRAME_ARENA_SIZE
#define WASM_FRAME_ARENA_SIZE                                                             \
    ((((size_t)WASM_MAX_CALL_DEPTH * (size_t)WASM__FRAME_ARENA_ESTIMATED_FRAME_SIZE) >    \
      (size_t)WASM__FRAME_ARENA_MIN_SIZE)                                                 \
         ? ((size_t)WASM_MAX_CALL_DEPTH * (size_t)WASM__FRAME_ARENA_ESTIMATED_FRAME_SIZE) \
         : (size_t)WASM__FRAME_ARENA_MIN_SIZE)
#endif

typedef struct wasm_config_t {
    uint32_t max_stack_values;
    uint32_t max_call_depth;
    uint32_t max_labels;
    size_t initial_gc_heap_size;
    size_t frame_arena_size;
} wasm_config_t;

struct wasm_runtime_t {
    wasm_import_t* imports;
    uint32_t num_imports;
    uint32_t cap_imports;
    wasm_global_import_t* global_imports;
    uint32_t num_global_imports;
    uint32_t cap_global_imports;
    wasm_table_import_t* table_imports;
    uint32_t num_table_imports;
    uint32_t cap_table_imports;
    wasm_memory_import_t* memory_imports;
    uint32_t num_memory_imports;
    uint32_t cap_memory_imports;
    wasm_tag_import_t* tag_imports;
    uint32_t num_tag_imports;
    uint32_t cap_tag_imports;
    wasm_value_t* stack;
    uint32_t max_stack;
    uint32_t sp;
    uint64_t fuel;
    int fuel_enabled;
    uint32_t enabled_features;
    wasm_error_t last_error;
    char error_msg[256];
    wasm__exception_state_t pending_exception;
    uint8_t* gc_heap;
    size_t gc_heap_size;
    size_t gc_heap_offset;
    wasm_gc_header_t* gc_allocations;
    wasm_module_t* gc_modules;
    uint8_t* frame_arena;
    size_t frame_arena_size;
    size_t frame_arena_offset;
    wasm__call_frame_t* call_frames;
    uint32_t max_call_frames;
    uint32_t call_frame_sp;
    wasm_valtype_t* validator_stack;
    wasm_reftype_t* validator_stack_reftypes;
    wasm__val_frame_t* validator_frames;
    uint32_t backtrace_depth;
    uint32_t max_labels;
    uint32_t* backtrace_func_indices;
    uint32_t* backtrace_offsets;
};

/* ── Public API ───────────────────────────────────────────────────── */
void wasm_config_default(wasm_config_t* config);
wasm_error_t wasm_init(wasm_runtime_t* rt, const wasm_config_t* config);
void wasm_destroy(wasm_runtime_t* rt);
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
wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt);
uint32_t wasm_memory_count(wasm_module_t* mod);
wasm_error_t wasm_memory_read(wasm_module_t* mod, uint32_t memory_index,
                              uint32_t offset, void* dst, size_t len);
wasm_error_t wasm_memory_write(wasm_module_t* mod, uint32_t memory_index,
                               uint32_t offset, const void* src, size_t len);
wasm_error_t wasm_memory_get_string(wasm_module_t* mod, uint32_t memory_index,
                                    uint32_t offset, char* dst, size_t max_len);
wasm_memory_t* wasm_memory_ref_at(wasm_module_t* mod, uint32_t memory_index);
uint8_t* wasm_memory_data_at(wasm_module_t* mod, uint32_t memory_index);
uint32_t wasm_memory_size_at(wasm_module_t* mod, uint32_t memory_index);
int32_t wasm_memory_grow_at(wasm_module_t* mod, uint32_t memory_index, uint32_t delta_pages);
uint8_t* wasm_memory_data(wasm_module_t* mod);
uint32_t wasm_memory_size(wasm_module_t* mod);
int32_t wasm_memory_grow(wasm_module_t* mod, uint32_t delta_pages);
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
wasm_export_kind_t wasm_export_kind(wasm_module_t* mod, uint32_t index);
uint32_t wasm_export_index(wasm_module_t* mod, uint32_t export_index);
int wasm_find_export(wasm_module_t* mod, const char* name,
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
void wasm_module_set_userdata(wasm_module_t* mod, void* userdata);
void* wasm_module_get_userdata(wasm_module_t* mod);
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
#include <time.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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
#ifndef WASM_GC_ALLOC
#define WASM_GC_ALLOC(sz) WASM_MALLOC(sz)
#endif
#ifndef WASM_GC_FREE
#define WASM_GC_FREE(p) WASM_FREE(p)
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

#define WASM__ALIGNOF(type) offsetof(struct { char c; type value; }, value)

/* ── WASI platform hooks (portable C99 defaults) ─────────────────── */

#if defined(_WIN32)
#define WASM__PLATFORM_WINDOWS 1
#define WASM__PLATFORM_POSIX 0
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
#define WASM__PLATFORM_WINDOWS 0
#define WASM__PLATFORM_POSIX 1
#else
#define WASM__PLATFORM_WINDOWS 0
#define WASM__PLATFORM_POSIX 0
#endif

#ifndef WASM_WASI_ARG_COUNT
#define WASM_WASI_ARG_COUNT() 0u
#endif

#ifndef WASM_WASI_ARG_AT
#define WASM_WASI_ARG_AT(index) ((const char*)0)
#endif

#ifndef WASM_WASI_ENV_COUNT
#define WASM_WASI_ENV_COUNT() 0u
#endif

#ifndef WASM_WASI_ENV_AT
#define WASM_WASI_ENV_AT(index) ((const char*)0)
#endif

#ifndef WASM_WASI_FILL_RANDOM
#define WASM__WASI_NEEDS_RANDOM_FALLBACK 1
#endif

#ifndef WASM_WASI_CLOCK_TIME_GET
#define WASM__WASI_NEEDS_CLOCK_FALLBACK 1
#endif

static uint32_t wasm__platform_wasi_arg_count(void) {
    return (uint32_t)WASM_WASI_ARG_COUNT();
}

static const char* wasm__platform_wasi_arg_at(uint32_t index) {
    (void)index;
    return WASM_WASI_ARG_AT(index);
}

static uint32_t wasm__platform_wasi_env_count(void) {
    return (uint32_t)WASM_WASI_ENV_COUNT();
}

static const char* wasm__platform_wasi_env_at(uint32_t index) {
    (void)index;
    return WASM_WASI_ENV_AT(index);
}

#if WASM__PLATFORM_WINDOWS
static uint64_t wasm__platform_windows_filetime_to_unix_ns(FILETIME ft) {
    ULARGE_INTEGER ticks;

    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    if (ticks.QuadPart < 116444736000000000ull) return 0u;
    return (ticks.QuadPart - 116444736000000000ull) * 100ull;
}

static int wasm__platform_windows_qpc_time_ns(uint64_t* out_time) {
    static LARGE_INTEGER freq = { 0 };
    LARGE_INTEGER counter;

    if (!out_time) return 0;
    if (freq.QuadPart == 0) {
        if (QueryPerformanceFrequency(&freq) == 0 || freq.QuadPart <= 0) return 0;
    }
    if (QueryPerformanceCounter(&counter) == 0) return 0;

    *out_time = (uint64_t)((counter.QuadPart / freq.QuadPart) * 1000000000ull +
                           ((counter.QuadPart % freq.QuadPart) * 1000000000ull) / freq.QuadPart);
    return 1;
}

static int wasm__platform_windows_process_time_ns(uint64_t* out_time) {
    FILETIME create_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    ULARGE_INTEGER kernel_ticks;
    ULARGE_INTEGER user_ticks;

    if (!out_time) return 0;
    if (GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time) == 0) return 0;

    kernel_ticks.LowPart = kernel_time.dwLowDateTime;
    kernel_ticks.HighPart = kernel_time.dwHighDateTime;
    user_ticks.LowPart = user_time.dwLowDateTime;
    user_ticks.HighPart = user_time.dwHighDateTime;
    *out_time = (kernel_ticks.QuadPart + user_ticks.QuadPart) * 100ull;
    return 1;
}

static int wasm__platform_windows_thread_time_ns(uint64_t* out_time) {
    FILETIME create_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    ULARGE_INTEGER kernel_ticks;
    ULARGE_INTEGER user_ticks;

    if (!out_time) return 0;
    if (GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_time, &user_time) == 0) return 0;

    kernel_ticks.LowPart = kernel_time.dwLowDateTime;
    kernel_ticks.HighPart = kernel_time.dwHighDateTime;
    user_ticks.LowPart = user_time.dwLowDateTime;
    user_ticks.HighPart = user_time.dwHighDateTime;
    *out_time = (kernel_ticks.QuadPart + user_ticks.QuadPart) * 100ull;
    return 1;
}
#endif

#if WASM__PLATFORM_POSIX
static int wasm__platform_posix_clock_time_get(uint32_t clock_id, uint64_t* out_time) {
#if defined(CLOCK_REALTIME)
    struct timespec ts;
    int native_clock = -1;

    if (!out_time) return 0;

    switch (clock_id) {
        case 0:
            native_clock = CLOCK_REALTIME;
            break;
#if defined(CLOCK_MONOTONIC)
        case 1:
            native_clock = CLOCK_MONOTONIC;
            break;
#endif
#if defined(CLOCK_PROCESS_CPUTIME_ID)
        case 2:
            native_clock = CLOCK_PROCESS_CPUTIME_ID;
            break;
#endif
#if defined(CLOCK_THREAD_CPUTIME_ID)
        case 3:
            native_clock = CLOCK_THREAD_CPUTIME_ID;
            break;
#endif
        default:
            break;
    }

    if (native_clock >= 0 && clock_gettime((clockid_t)native_clock, &ts) == 0) {
        *out_time = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
        return 1;
    }
#else
    (void)clock_id;
    (void)out_time;
#endif
    return 0;
}
#endif

#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#if WASM__PLATFORM_WINDOWS && defined(_MSC_VER)
static int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    unsigned char* bytes = (unsigned char*)dst;
    size_t i;

    if (len > 0 && !dst) return 0;

    for (i = 0; i < len;) {
        unsigned int value;
        size_t chunk = len - i;

        if (rand_s(&value) != 0) return 0;
        if (chunk > sizeof(value)) chunk = sizeof(value);
        memcpy(bytes + i, &value, chunk);
        i += chunk;
    }
    return 1;
}
#elif WASM__PLATFORM_POSIX
static int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    unsigned char* bytes = (unsigned char*)dst;
    size_t i;
    static int seeded = 0;

    if (len > 0 && !dst) return 0;

    if (len > 0) {
        FILE* random_file = fopen("/dev/urandom", "rb");

        if (random_file) {
            size_t read_count = fread(bytes, 1, len, random_file);
            fclose(random_file);
            if (read_count == len) return 1;
        }
    } else {
        return 1;
    }

    if (!seeded) {
        unsigned int seed = (unsigned int)time(NULL);
        clock_t ticks = clock();

        if (ticks != (clock_t)-1) seed ^= (unsigned int)ticks;
        srand(seed);
        seeded = 1;
    }

    for (i = 0; i < len; i++) bytes[i] = (unsigned char)(rand() & 0xFF);
    return 1;
}
#else
static int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    unsigned char* bytes = (unsigned char*)dst;
    size_t i;
    static int seeded = 0;

    if (len > 0 && !dst) return 0;

    if (!seeded) {
        unsigned int seed = (unsigned int)time(NULL);
        clock_t ticks = clock();

        if (ticks != (clock_t)-1) seed ^= (unsigned int)ticks;
        srand(seed);
        seeded = 1;
    }

    for (i = 0; i < len; i++) bytes[i] = (unsigned char)(rand() & 0xFF);
    return 1;
}
#endif
#else
static int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    return WASM_WASI_FILL_RANDOM(dst, len);
}
#endif

#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#if WASM__PLATFORM_WINDOWS
static int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    (void)precision;

    if (!out_time) return 0;

    switch (clock_id) {
        case 0: {
            FILETIME ft;

            GetSystemTimeAsFileTime(&ft);
            *out_time = wasm__platform_windows_filetime_to_unix_ns(ft);
            return 1;
        }
        case 1:
            return wasm__platform_windows_qpc_time_ns(out_time);
        case 2:
            return wasm__platform_windows_process_time_ns(out_time);
        case 3:
            return wasm__platform_windows_thread_time_ns(out_time);
        default:
            return 0;
    }
}
#elif WASM__PLATFORM_POSIX
static int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    (void)precision;

    if (!out_time) return 0;
    if (wasm__platform_posix_clock_time_get(clock_id, out_time)) return 1;

    switch (clock_id) {
        case 0: {
            time_t now = time(NULL);

            if (now == (time_t)-1) return 0;
            *out_time = (uint64_t)(now > (time_t)0 ? now : (time_t)0) * 1000000000ull;
            return 1;
        }
        case 1:
        case 2:
        case 3: {
            clock_t ticks = clock();

            if (ticks == (clock_t)-1 || CLOCKS_PER_SEC <= 0) return 0;
            *out_time = ((uint64_t)ticks * 1000000000ull) / (uint64_t)CLOCKS_PER_SEC;
            return 1;
        }
        default:
            return 0;
    }
}
#else
static int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    (void)precision;

    if (!out_time) return 0;

    switch (clock_id) {
        case 0: {
            time_t now = time(NULL);

            if (now == (time_t)-1) return 0;
            *out_time = (uint64_t)(now > (time_t)0 ? now : (time_t)0) * 1000000000ull;
            return 1;
        }
        case 1:
        case 2:
        case 3: {
            clock_t ticks = clock();

            if (ticks == (clock_t)-1 || CLOCKS_PER_SEC <= 0) return 0;
            *out_time = ((uint64_t)ticks * 1000000000ull) / (uint64_t)CLOCKS_PER_SEC;
            return 1;
        }
        default:
            return 0;
    }
}
#endif
#else
static int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    return WASM_WASI_CLOCK_TIME_GET(clock_id, precision, out_time);
}
#endif

#undef WASM__WASI_NEEDS_RANDOM_FALLBACK
#undef WASM__WASI_NEEDS_CLOCK_FALLBACK
#undef WASM__PLATFORM_WINDOWS
#undef WASM__PLATFORM_POSIX

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
     WASM_FEATURE_SIMD | WASM_FEATURE_GC)

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
        case WASM_FEATURE_GC:
            return "garbage collection";
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

    if ((flag & WASM_FEATURE_GC) != 0) {
        mod->required_features |= WASM_FEATURE_REFERENCE_TYPES;
        if ((WASM__IMPLEMENTED_FEATURES & WASM_FEATURE_REFERENCE_TYPES) == 0) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "feature '%s' is not implemented",
                          wasm__feature_name(WASM_FEATURE_REFERENCE_TYPES));
            return WASM_ERR_MALFORMED;
        }
        if ((mod->rt->enabled_features & WASM_FEATURE_REFERENCE_TYPES) == 0) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "feature '%s' is disabled",
                          wasm__feature_name(WASM_FEATURE_REFERENCE_TYPES));
            return WASM_ERR_MALFORMED;
        }
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

static wasm_error_t wasm__read_name_owned(wasm__reader_t* r, char** out_name) {
    uint32_t len = wasm__read_leb128_u32(r);
    char* name;

    if (!out_name) return WASM_ERR_MALFORMED;
    *out_name = NULL;

    if (r->malformed || !wasm__has(r, len)) {
        r->ptr = r->end;
        return WASM_ERR_MALFORMED;
    }

    name = (char*)WASM_MALLOC((size_t)len + 1u);
    if (!name) return WASM_ERR_OOM;

    if (len > 0) memcpy(name, r->ptr, len);
    name[len] = '\0';
    r->ptr += len;
    *out_name = name;
    return WASM_OK;
}

static char* wasm__strdup(const char* text) {
    size_t len;
    char* copy;

    if (!text) return NULL;
    len = strlen(text);
    copy = (char*)WASM_MALLOC(len + 1u);
    if (!copy) return NULL;
    if (len > 0u) memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
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

#define WASM__GC_I31_TAG ((uintptr_t)1u)

static uintptr_t wasm__gc_i31_encode(int32_t value) {
    return ((((uintptr_t)((uint32_t)value & 0x7FFFFFFFu))) << 1) | WASM__GC_I31_TAG;
}

static int32_t wasm__gc_i31_get_s(uintptr_t bits) {
    uint32_t raw = (uint32_t)(bits >> 1) & 0x7FFFFFFFu;

    if ((raw & 0x40000000u) != 0) raw |= 0x80000000u;
    return (int32_t)raw;
}

static uint32_t wasm__gc_i31_get_u(uintptr_t bits) {
    return (uint32_t)(bits >> 1) & 0x7FFFFFFFu;
}

static int wasm__runtime_value_matches_type(const wasm_module_t* mod,
                                            const wasm_value_t* value,
                                            wasm_valtype_t expected_type,
                                            const wasm_reftype_t* expected_ref);
static wasm_value_t wasm__typed_ref_null(wasm_valtype_t type,
                                         const wasm_reftype_t* reftype);
static int wasm__get_effective_reftype(const wasm_module_t* mod,
                                       wasm_valtype_t type,
                                       const wasm_reftype_t* ref_type,
                                       wasm_reftype_t* out_reftype);
static int wasm__typed_valtype_is_subtype(const wasm_module_t* mod,
                                          wasm_valtype_t subtype,
                                          const wasm_reftype_t* subtype_ref,
                                          wasm_valtype_t supertype,
                                          const wasm_reftype_t* supertype_ref);
static int wasm__typed_valtype_equal(const wasm_module_t* mod,
                                     wasm_valtype_t lhs,
                                     const wasm_reftype_t* lhs_ref,
                                     wasm_valtype_t rhs,
                                     const wasm_reftype_t* rhs_ref);
static int wasm__functype_is_subtype(const wasm_module_t* mod,
                                     const wasm_functype_t* subtype,
                                     const wasm_functype_t* supertype);
static int wasm__globaltype_matches_import(const wasm_module_t* mod,
                                           wasm_valtype_t actual_type,
                                           const wasm_reftype_t* actual_ref,
                                           int actual_mutable,
                                           wasm_valtype_t expected_type,
                                           const wasm_reftype_t* expected_ref,
                                           int expected_mutable);

static size_t wasm__align_up_size(size_t value, size_t alignment) {
    size_t remainder;

    if (alignment <= 1) return value;
    remainder = value % alignment;
    if (remainder == 0) return value;
    if (value > (size_t)-1 - (alignment - remainder)) return (size_t)-1;
    return value + (alignment - remainder);
}

static size_t wasm__gc_field_storage_alignment(const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) {
        return field->storage.of.packed_type == WASM_PACKED_I8 ? 1u : 2u;
    }
    return (size_t)WASM__ALIGNOF(wasm_value_t);
}

static size_t wasm__gc_field_storage_size(const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) {
        return field->storage.of.packed_type == WASM_PACKED_I8 ? 1u : 2u;
    }
    return sizeof(wasm_value_t);
}

static size_t wasm__gc_struct_field_offset(const wasm_structtype_t* type, uint32_t field_index) {
    size_t offset = WASM__GC_STRUCT_HEADER_SIZE;
    uint32_t i;

    for (i = 0; i < type->num_fields && i < field_index; i++) {
        size_t alignment = wasm__gc_field_storage_alignment(&type->fields[i]);
        size_t size = wasm__gc_field_storage_size(&type->fields[i]);

        offset = wasm__align_up_size(offset, alignment);
        if (offset == (size_t)-1 || size > (size_t)-1 - offset) return (size_t)-1;
        offset += size;
    }

    if (field_index < type->num_fields) {
        size_t alignment = wasm__gc_field_storage_alignment(&type->fields[field_index]);
        offset = wasm__align_up_size(offset, alignment);
    }

    return offset;
}

static int wasm__gc_struct_size(const wasm_structtype_t* type, size_t* out_size) {
    size_t offset = WASM__GC_STRUCT_HEADER_SIZE;
    uint32_t i;

    for (i = 0; i < type->num_fields; i++) {
        size_t alignment = wasm__gc_field_storage_alignment(&type->fields[i]);
        size_t size = wasm__gc_field_storage_size(&type->fields[i]);

        offset = wasm__align_up_size(offset, alignment);
        if (offset == (size_t)-1 || size > (size_t)-1 - offset) return 0;
        offset += size;
    }

    offset = wasm__align_up_size(offset, (size_t)WASM__ALIGNOF(wasm_value_t));
    if (offset == (size_t)-1) return 0;
    *out_size = offset;
    return 1;
}

static size_t wasm__gc_array_data_offset(const wasm_arraytype_t* type) {
    return wasm__align_up_size(WASM__GC_ARRAY_HEADER_SIZE, wasm__gc_field_storage_alignment(&type->field));
}

static int wasm__gc_array_total_size(const wasm_arraytype_t* type,
                                     uint32_t length,
                                     size_t* out_size) {
    size_t offset = wasm__gc_array_data_offset(type);
    size_t elem_size = wasm__gc_field_storage_size(&type->field);
    size_t payload_size;

    if (offset == (size_t)-1) return 0;
    if (length != 0 && elem_size > ((size_t)-1 - offset) / (size_t)length) return 0;
    payload_size = elem_size * (size_t)length;
    *out_size = offset + payload_size;
    return 1;
}

static wasm_value_t wasm__gc_field_default_value(const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) return wasm_i32(0);

    switch (field->storage.of.valtype) {
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
        default:
            return wasm__typed_ref_null(field->storage.of.valtype, &field->storage.ref_type);
    }
}

static wasm_error_t wasm__gc_load_field_bytes(const wasm_fieldtype_t* field,
                                              const void* src,
                                              int sign_extend,
                                              wasm_value_t* out_value) {
    if (!field || !src || !out_value) return WASM_ERR_MALFORMED;

    if (field->storage.kind == WASM_STORAGE_PACKED) {
        if (field->storage.of.packed_type == WASM_PACKED_I8) {
            uint8_t raw = *(const uint8_t*)src;

            *out_value = wasm_i32(sign_extend ? (int32_t)(int8_t)raw : (int32_t)raw);
        } else {
            uint16_t raw = wasm__load_le16(src);

            *out_value = wasm_i32(sign_extend ? (int32_t)(int16_t)raw : (int32_t)raw);
        }
        return WASM_OK;
    }

    *out_value = *(const wasm_value_t*)src;
    return WASM_OK;
}

static wasm_error_t wasm__gc_store_field_bytes(const wasm_module_t* mod,
                                               const wasm_fieldtype_t* field,
                                               void* dst,
                                               wasm_value_t value) {
    if (field->storage.kind == WASM_STORAGE_PACKED) {
        if (value.type != WASM_TYPE_I32) return WASM_ERR_TYPE_MISMATCH;
        if (field->storage.of.packed_type == WASM_PACKED_I8) {
            *(uint8_t*)dst = (uint8_t)value.of.i32;
        } else {
            wasm__store_le16(dst, (uint16_t)value.of.i32);
        }
        return WASM_OK;
    }

    if (!wasm__runtime_value_matches_type(mod,
                                          &value,
                                          field->storage.of.valtype,
                                          &field->storage.ref_type))
        return WASM_ERR_TYPE_MISMATCH;
    value.type = field->storage.of.valtype;
    *(wasm_value_t*)dst = value;
    return WASM_OK;
}

static void* wasm__gc_struct_field_ptr(wasm_gc_struct_t* object,
                                       const wasm_structtype_t* type,
                                       uint32_t field_index) {
    size_t offset;

    if (!object || !type || field_index >= type->num_fields) return NULL;
    offset = wasm__gc_struct_field_offset(type, field_index);
    if (offset == (size_t)-1) return NULL;
    return ((uint8_t*)object) + offset;
}

static const void* wasm__gc_struct_field_const_ptr(const wasm_gc_struct_t* object,
                                                   const wasm_structtype_t* type,
                                                   uint32_t field_index) {
    size_t offset;

    if (!object || !type || field_index >= type->num_fields) return NULL;
    offset = wasm__gc_struct_field_offset(type, field_index);
    if (offset == (size_t)-1) return NULL;
    return ((const uint8_t*)object) + offset;
}

WASM_INLINE wasm_error_t wasm__gc_struct_get_field(const wasm_gc_struct_t* object,
                                                   const wasm_structtype_t* type,
                                                   uint32_t field_index,
                                                   int sign_extend,
                                                   wasm_value_t* out_value) {
    const void* src;

    if (!object) return WASM_ERR_TRAP;
    if (!type || field_index >= type->num_fields) return WASM_ERR_MALFORMED;
    src = wasm__gc_struct_field_const_ptr(object, type, field_index);
    if (!src) return WASM_ERR_OOM;
    return wasm__gc_load_field_bytes(&type->fields[field_index], src, sign_extend, out_value);
}

static wasm_error_t wasm__gc_struct_set_field(wasm_gc_struct_t* object,
                                              const wasm_structtype_t* type,
                                              uint32_t field_index,
                                              wasm_value_t value) {
    void* dst;

    if (!object) return WASM_ERR_TRAP;
    if (!type || field_index >= type->num_fields) return WASM_ERR_MALFORMED;
    dst = wasm__gc_struct_field_ptr(object, type, field_index);
    if (!dst) return WASM_ERR_OOM;
    return wasm__gc_store_field_bytes(object->header.module, &type->fields[field_index], dst, value);
}

static wasm_error_t wasm__gc_struct_store_field(wasm_gc_struct_t* object,
                                                const wasm_structtype_t* type,
                                                uint32_t field_index,
                                                wasm_value_t value) {
    return wasm__gc_struct_set_field(object, type, field_index, value);
}

static const void* wasm__gc_array_element_const_ptr(const wasm_gc_array_t* object,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t index) {
    size_t data_offset = wasm__gc_array_data_offset(type);
    size_t elem_size = wasm__gc_field_storage_size(&type->field);

    if (!object || !type || index >= object->length || data_offset == (size_t)-1) return NULL;
    return ((const uint8_t*)object) + data_offset + ((size_t)index * elem_size);
}

static void* wasm__gc_array_element_ptr(wasm_gc_array_t* object,
                                        const wasm_arraytype_t* type,
                                        uint32_t index) {
    size_t data_offset = wasm__gc_array_data_offset(type);
    size_t elem_size = wasm__gc_field_storage_size(&type->field);

    if (!object || !type || index >= object->length || data_offset == (size_t)-1) return NULL;
    return ((uint8_t*)object) + data_offset + ((size_t)index * elem_size);
}

WASM_INLINE wasm_error_t wasm__gc_array_get(const wasm_gc_array_t* object,
                                            const wasm_arraytype_t* type,
                                            uint32_t index,
                                            int sign_extend,
                                            wasm_value_t* out_value) {
    const void* src;

    if (!object) return WASM_ERR_TRAP;
    if (!type) return WASM_ERR_MALFORMED;
    if (index >= object->length) return WASM_ERR_OUT_OF_BOUNDS;
    src = wasm__gc_array_element_const_ptr(object, type, index);
    if (!src) return WASM_ERR_OOM;
    return wasm__gc_load_field_bytes(&type->field, src, sign_extend, out_value);
}

static wasm_error_t wasm__gc_array_set(wasm_gc_array_t* object,
                                       const wasm_arraytype_t* type,
                                       uint32_t index,
                                       wasm_value_t value) {
    void* dst;

    if (!object) return WASM_ERR_TRAP;
    if (!type) return WASM_ERR_MALFORMED;
    if (index >= object->length) return WASM_ERR_OUT_OF_BOUNDS;
    dst = wasm__gc_array_element_ptr(object, type, index);
    if (!dst) return WASM_ERR_OOM;
    return wasm__gc_store_field_bytes(object->header.module, &type->field, dst, value);
}

WASM_INLINE uint32_t wasm__gc_array_len(const wasm_gc_array_t* object) {
    return object ? object->length : 0u;
}

static wasm_error_t wasm__gc_array_store_element(wasm_gc_array_t* object,
                                                 const wasm_arraytype_t* type,
                                                 uint32_t index,
                                                 wasm_value_t value) {
    return wasm__gc_array_set(object, type, index, value);
}

static wasm_value_t wasm__gc_type_ref_value(const wasm_module_t* mod,
                                            uint32_t type_index,
                                            wasm_gc_header_t* object) {
    wasm_value_t result;

    result.type = WASM_TYPE_ANYREF;
    result.of.gc_ref = (uintptr_t)object;
    if (mod && type_index < mod->num_types) {
        switch (mod->types[type_index].kind) {
            case WASM_COMP_STRUCT:
                result.type = WASM_TYPE_STRUCTREF;
                break;
            case WASM_COMP_ARRAY:
                result.type = WASM_TYPE_ARRAYREF;
                break;
            default:
                break;
        }
    }
    return result;
}

static int wasm__gc_array_data_element_size(const wasm_arraytype_t* type, size_t* out_size) {
    if (!type || !out_size) return 0;
    if (type->field.storage.kind == WASM_STORAGE_PACKED) {
        *out_size = type->field.storage.of.packed_type == WASM_PACKED_I8 ? 1u : 2u;
        return 1;
    }

    switch (type->field.storage.of.valtype) {
        case WASM_TYPE_I32:
        case WASM_TYPE_F32:
            *out_size = 4u;
            return 1;
        case WASM_TYPE_I64:
        case WASM_TYPE_F64:
            *out_size = 8u;
            return 1;
        case WASM_TYPE_V128:
            *out_size = 16u;
            return 1;
        default:
            return 0;
    }
}

static wasm_error_t wasm__gc_array_read_data_value(const wasm_arraytype_t* type,
                                                   const uint8_t* src,
                                                   wasm_value_t* out_value) {
    if (!type || !src || !out_value) return WASM_ERR_MALFORMED;
    if (type->field.storage.kind == WASM_STORAGE_PACKED)
        return wasm__gc_load_field_bytes(&type->field, src, 0, out_value);

    switch (type->field.storage.of.valtype) {
        case WASM_TYPE_I32:
            *out_value = wasm_i32((int32_t)wasm__load_le32(src));
            return WASM_OK;
        case WASM_TYPE_I64:
            *out_value = wasm_i64((int64_t)wasm__load_le64(src));
            return WASM_OK;
        case WASM_TYPE_F32: {
            uint32_t bits = wasm__load_le32(src);
            float value;

            memcpy(&value, &bits, sizeof(value));
            *out_value = wasm_f32(value);
            return WASM_OK;
        }
        case WASM_TYPE_F64: {
            uint64_t bits = wasm__load_le64(src);
            double value;

            memcpy(&value, &bits, sizeof(value));
            *out_value = wasm_f64(value);
            return WASM_OK;
        }
        case WASM_TYPE_V128:
            *out_value = wasm_v128_zero();
            memcpy(out_value->of.v128, src, 16u);
            return WASM_OK;
        default:
            return WASM_ERR_TYPE_MISMATCH;
    }
}

static int wasm__is_gc_ref_type(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_NOFUNC:
        case WASM_TYPE_NOEXTERN:
        case WASM_TYPE_NONE:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return 1;
        default:
            return 0;
    }
}

static int wasm__is_ref_type(wasm_valtype_t type) {
    return type == WASM_TYPE_FUNCREF || type == WASM_TYPE_EXTERNREF ||
           wasm__is_gc_ref_type(type);
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
    if (wasm__uses_funcref_storage(value->type)) return value->of.funcref == UINT32_MAX;
    if (wasm__uses_externref_storage(value->type)) return value->of.externref == (uintptr_t)0;
    if (wasm__uses_gc_ref_storage(value->type)) return value->of.gc_ref == (uintptr_t)0;
    return 0;
}

static int wasm__is_heap_gc_ref_value(const wasm_value_t* value) {
    if (!value || !wasm__uses_gc_ref_storage(value->type)) return 0;
    if (value->of.gc_ref == (uintptr_t)0) return 0;
    return (value->of.gc_ref & WASM__GC_I31_TAG) == 0;
}

static int wasm__value_matches_type(const wasm_value_t* value, wasm_valtype_t type) {
    return wasm__is_valtype_subtype(NULL, value->type, type);
}

static wasm_value_t wasm__typed_ref_null(wasm_valtype_t type,
                                         const wasm_reftype_t* reftype) {
    wasm_valtype_t null_type = type;

    if (reftype && reftype->has_type_index) {
        wasm_valtype_t concrete_type = reftype->type ? reftype->type : type;

        switch (concrete_type) {
            case WASM_TYPE_FUNCREF:
                null_type = WASM_TYPE_NOFUNC;
                break;
            case WASM_TYPE_EXTERNREF:
                null_type = WASM_TYPE_NOEXTERN;
                break;
            default:
                null_type = WASM_TYPE_NONE;
                break;
        }
    }

    if (null_type == WASM_TYPE_VOID) null_type = WASM_TYPE_ANYREF;
    return wasm_ref_null(null_type);
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
        case WASM_TYPE_NOFUNC:
        case WASM_TYPE_NOEXTERN:
        case WASM_TYPE_NONE:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
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
    if (lhs == rhs) {
        if (lhs == 0.0f) return (signbit(lhs) || signbit(rhs)) ? -0.0f : 0.0f;
        return lhs;
    }
    return lhs < rhs ? lhs : rhs;
}

static float wasm__f32_max(float lhs, float rhs) {
    if (isnan(lhs) || isnan(rhs)) return lhs + rhs;
    if (lhs == rhs) {
        if (lhs == 0.0f) return (signbit(lhs) && signbit(rhs)) ? -0.0f : 0.0f;
        return lhs;
    }
    return lhs > rhs ? lhs : rhs;
}

static double wasm__f64_min(double lhs, double rhs) {
    if (isnan(lhs) || isnan(rhs)) return lhs + rhs;
    if (lhs == rhs) {
        if (lhs == 0.0) return (signbit(lhs) || signbit(rhs)) ? -0.0 : 0.0;
        return lhs;
    }
    return lhs < rhs ? lhs : rhs;
}

static double wasm__f64_max(double lhs, double rhs) {
    if (isnan(lhs) || isnan(rhs)) return lhs + rhs;
    if (lhs == rhs) {
        if (lhs == 0.0) return (signbit(lhs) && signbit(rhs)) ? -0.0 : 0.0;
        return lhs;
    }
    return lhs > rhs ? lhs : rhs;
}

static float wasm__nearest_f32(float value) {
    return nearbyintf(value);
}

static double wasm__nearest_f64(double value) {
    return nearbyint(value);
}

static wasm_error_t wasm__require_valtype_feature(wasm_module_t* mod, wasm_valtype_t type) {
    if (!mod) return WASM_OK;
    if (type == WASM_TYPE_FUNCREF || type == WASM_TYPE_EXTERNREF)
        return wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
    if (wasm__is_gc_ref_type(type)) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
        if (err != WASM_OK) return err;
        return wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
    }
    if (wasm__is_vector_type(type)) return wasm__require_feature(mod, WASM_FEATURE_SIMD);
    return WASM_OK;
}

static void wasm__clear_reftype(wasm_reftype_t* reftype) {
    if (!reftype) return;
    memset(reftype, 0, sizeof(*reftype));
}

static int wasm__is_absheaptype_byte(uint8_t byte) {
    return byte >= 0x6A && byte <= 0x73;
}

static int wasm__is_reftype_lead_byte(uint8_t byte) {
    return byte == 0x63 || byte == 0x64 || wasm__is_absheaptype_byte(byte);
}

static wasm_valtype_t wasm__abstract_heap_byte_to_type(uint8_t byte) {
    switch (byte) {
        case 0x73:
            return WASM_TYPE_NOFUNC;
        case 0x72:
            return WASM_TYPE_NOEXTERN;
        case 0x71:
            return WASM_TYPE_NONE;
        case 0x70:
            return WASM_TYPE_FUNCREF;
        case 0x6F:
            return WASM_TYPE_EXTERNREF;
        case 0x6E:
            return WASM_TYPE_ANYREF;
        case 0x6D:
            return WASM_TYPE_EQREF;
        case 0x6C:
            return WASM_TYPE_I31REF;
        case 0x6B:
            return WASM_TYPE_STRUCTREF;
        case 0x6A:
            return WASM_TYPE_ARRAYREF;
        default:
            return WASM_TYPE_VOID;
    }
}

static wasm_error_t wasm__resolve_reftype(wasm_module_t* mod, wasm_reftype_t* reftype) {
    if (!reftype || !reftype->has_type_index) return WASM_OK;
    if (!mod || reftype->type_index >= mod->num_types) return WASM_ERR_MALFORMED;

    switch (mod->types[reftype->type_index].kind) {
        case WASM_COMP_FUNC:
            reftype->type = WASM_TYPE_FUNCREF;
            return WASM_OK;
        case WASM_COMP_STRUCT:
            reftype->type = WASM_TYPE_STRUCTREF;
            return WASM_OK;
        case WASM_COMP_ARRAY:
            reftype->type = WASM_TYPE_ARRAYREF;
            return WASM_OK;
        default:
            return WASM_ERR_MALFORMED;
    }
}

static wasm_error_t wasm__resolve_valtype_reftype(wasm_module_t* mod,
                                                  wasm_valtype_t* type,
                                                  wasm_reftype_t* reftype) {
    wasm_error_t err;

    if (!reftype) return WASM_OK;
    err = wasm__resolve_reftype(mod, reftype);
    if (err != WASM_OK) return err;
    if (reftype->nullable || reftype->has_type_index || wasm__is_ref_type(reftype->type)) {
        if (type) *type = reftype->type;
    }
    return WASM_OK;
}

static int wasm__has_reftype_info(const wasm_reftype_t* reftype) {
    return reftype && (reftype->type != 0 || reftype->has_type_index || reftype->nullable);
}

static int wasm__reftype_equal(const wasm_reftype_t* lhs, const wasm_reftype_t* rhs) {
    if (!wasm__has_reftype_info(lhs) || !wasm__has_reftype_info(rhs)) return 0;
    return lhs->type == rhs->type && lhs->type_index == rhs->type_index &&
           lhs->has_type_index == rhs->has_type_index && lhs->nullable == rhs->nullable;
}

static int wasm__reftype_is_subtype(const wasm_module_t* mod,
                                    const wasm_reftype_t* subtype,
                                    const wasm_reftype_t* supertype) {
    if (!wasm__has_reftype_info(subtype) || !wasm__has_reftype_info(supertype)) return 0;
    if (subtype->nullable && !supertype->nullable) return 0;

    if (subtype->has_type_index && supertype->has_type_index)
        return wasm__is_subtype(mod, subtype->type_index, supertype->type_index);

    if (subtype->has_type_index)
        return wasm__is_heap_subtype(mod, subtype->type, supertype->type);

    if (supertype->has_type_index) {
        switch (subtype->type) {
            case WASM_TYPE_NOFUNC:
                return supertype->type == WASM_TYPE_FUNCREF && supertype->nullable;
            case WASM_TYPE_NONE:
                return (supertype->type == WASM_TYPE_STRUCTREF ||
                        supertype->type == WASM_TYPE_ARRAYREF) &&
                       supertype->nullable;
            default:
                return 0;
        }
    }

    return wasm__is_heap_subtype(mod, subtype->type, supertype->type);
}

static const wasm_gc_header_t* wasm__runtime_get_gc_object(const wasm_module_t* mod,
                                                           const wasm_value_t* value) {
    const wasm_runtime_t* rt;
    const uint8_t* addr;

    if (!mod || !value || !wasm__uses_gc_ref_storage(value->type)) return NULL;
    if (value->of.gc_ref == (uintptr_t)0 || (value->of.gc_ref & WASM__GC_I31_TAG) != 0) return NULL;

    rt = mod->rt;
    if (!rt || !rt->gc_heap) return NULL;
    addr = (const uint8_t*)value->of.gc_ref;
    if (addr < rt->gc_heap || addr >= rt->gc_heap + rt->gc_heap_size) return NULL;
    return (const wasm_gc_header_t*)value->of.gc_ref;
}

static int wasm__runtime_value_reftype(const wasm_module_t* mod,
                                       const wasm_value_t* value,
                                       wasm_reftype_t* out_reftype) {
    const wasm_gc_header_t* object;

    if (!value || !out_reftype || !wasm__is_ref_type(value->type)) {
        wasm__clear_reftype(out_reftype);
        return 0;
    }

    wasm__clear_reftype(out_reftype);
    out_reftype->type = value->type;
    out_reftype->nullable = wasm__is_null_ref(value);
    if (out_reftype->nullable || wasm__uses_funcref_storage(value->type) ||
        wasm__uses_externref_storage(value->type))
        return 1;

    if ((value->of.gc_ref & WASM__GC_I31_TAG) != 0) {
        out_reftype->type = WASM_TYPE_I31REF;
        out_reftype->nullable = 0;
        return 1;
    }

    object = wasm__runtime_get_gc_object(mod, value);
    if (!object) {
        if (value->type == WASM_TYPE_ANYREF)
            out_reftype->type = WASM_TYPE_ANYREF;
        else if (value->type == WASM_TYPE_EQREF)
            out_reftype->type = WASM_TYPE_EQREF;
        return 1;
    }

    out_reftype->nullable = 0;
    switch (object->kind) {
        case WASM_GC_OBJ_STRUCT:
            out_reftype->type = WASM_TYPE_STRUCTREF;
            break;
        case WASM_GC_OBJ_ARRAY:
            out_reftype->type = WASM_TYPE_ARRAYREF;
            break;
        default:
            out_reftype->type = value->type;
            return 1;
    }

    if (object->module == mod && mod && object->type_index < mod->num_types) {
        out_reftype->has_type_index = 1;
        out_reftype->type_index = object->type_index;
    }
    return 1;
}

static int wasm__runtime_value_matches_type(const wasm_module_t* mod,
                                            const wasm_value_t* value,
                                            wasm_valtype_t expected_type,
                                            const wasm_reftype_t* expected_ref) {
    wasm_reftype_t actual_ref;
    wasm_reftype_t effective_expected;

    if (!value) return 0;
    if (!wasm__is_ref_type(expected_type))
        return wasm__is_valtype_subtype(mod, value->type, expected_type);
    if (!wasm__runtime_value_reftype(mod, value, &actual_ref)) return 0;

    if (expected_ref && wasm__has_reftype_info(expected_ref))
        effective_expected = *expected_ref;
    else {
        wasm__clear_reftype(&effective_expected);
        effective_expected.type = expected_type;
        effective_expected.nullable = 1;
    }
    if (effective_expected.type == WASM_TYPE_VOID) effective_expected.type = expected_type;

    if (!wasm__is_valtype_subtype(mod, actual_ref.type, expected_type)) return 0;
    return wasm__reftype_is_subtype(mod, &actual_ref, &effective_expected);
}

static int wasm__get_effective_reftype(const wasm_module_t* mod,
                                       wasm_valtype_t type,
                                       const wasm_reftype_t* ref_type,
                                       wasm_reftype_t* out_reftype) {
    if (!wasm__is_ref_type(type)) {
        wasm__clear_reftype(out_reftype);
        return 0;
    }

    if (ref_type && wasm__has_reftype_info(ref_type)) {
        *out_reftype = *ref_type;
        if (out_reftype->has_type_index && mod != NULL) {
            wasm_reftype_t resolved = *out_reftype;

            if (wasm__resolve_reftype((wasm_module_t*)mod, &resolved) == WASM_OK)
                *out_reftype = resolved;
        }
        if (out_reftype->type == WASM_TYPE_VOID) out_reftype->type = type;
        return 1;
    }

    wasm__clear_reftype(out_reftype);
    out_reftype->type = type;
    out_reftype->nullable = 1;
    return 1;
}

static int wasm__typed_valtype_is_subtype(const wasm_module_t* mod,
                                          wasm_valtype_t subtype,
                                          const wasm_reftype_t* subtype_ref,
                                          wasm_valtype_t supertype,
                                          const wasm_reftype_t* supertype_ref) {
    wasm_reftype_t effective_subtype;
    wasm_reftype_t effective_supertype;

    if (!wasm__is_valtype_subtype(mod, subtype, supertype)) return 0;
    if (!wasm__is_ref_type(subtype) || !wasm__is_ref_type(supertype)) return 1;
    if (!wasm__get_effective_reftype(mod, subtype, subtype_ref, &effective_subtype) ||
        !wasm__get_effective_reftype(mod, supertype, supertype_ref, &effective_supertype))
        return 1;
    return wasm__reftype_is_subtype(mod, &effective_subtype, &effective_supertype);
}

static int wasm__typed_valtype_equal(const wasm_module_t* mod,
                                     wasm_valtype_t lhs,
                                     const wasm_reftype_t* lhs_ref,
                                     wasm_valtype_t rhs,
                                     const wasm_reftype_t* rhs_ref) {
    return wasm__typed_valtype_is_subtype(mod, lhs, lhs_ref, rhs, rhs_ref) &&
           wasm__typed_valtype_is_subtype(mod, rhs, rhs_ref, lhs, lhs_ref);
}

static int wasm__functype_is_subtype(const wasm_module_t* mod,
                                     const wasm_functype_t* subtype,
                                     const wasm_functype_t* supertype) {
    uint32_t i;

    if (!subtype || !supertype) return 0;
    if (subtype->num_params != supertype->num_params || subtype->num_results != supertype->num_results)
        return 0;

    for (i = 0; i < subtype->num_params; i++) {
        const wasm_reftype_t* subtype_ref = subtype->param_reftypes ? &subtype->param_reftypes[i] : NULL;
        const wasm_reftype_t* supertype_ref = supertype->param_reftypes ? &supertype->param_reftypes[i] : NULL;

        if (!wasm__typed_valtype_is_subtype(mod,
                                            supertype->params[i],
                                            supertype_ref,
                                            subtype->params[i],
                                            subtype_ref))
            return 0;
    }

    for (i = 0; i < subtype->num_results; i++) {
        const wasm_reftype_t* subtype_ref = subtype->result_reftypes ? &subtype->result_reftypes[i] : NULL;
        const wasm_reftype_t* supertype_ref = supertype->result_reftypes ? &supertype->result_reftypes[i] : NULL;

        if (!wasm__typed_valtype_is_subtype(mod,
                                            subtype->results[i],
                                            subtype_ref,
                                            supertype->results[i],
                                            supertype_ref))
            return 0;
    }

    return 1;
}

static int wasm__globaltype_matches_import(const wasm_module_t* mod,
                                           wasm_valtype_t actual_type,
                                           const wasm_reftype_t* actual_ref,
                                           int actual_mutable,
                                           wasm_valtype_t expected_type,
                                           const wasm_reftype_t* expected_ref,
                                           int expected_mutable) {
    if (actual_mutable != expected_mutable) return 0;
    if (expected_mutable)
        return wasm__typed_valtype_equal(mod, actual_type, actual_ref, expected_type, expected_ref);
    return wasm__typed_valtype_is_subtype(mod, actual_type, actual_ref, expected_type, expected_ref);
}

static int wasm__limits_match_import(uint32_t actual_min,
                                     uint32_t actual_max,
                                     uint32_t expected_min,
                                     uint32_t expected_max) {
    if (actual_min < expected_min) return 0;
    if (expected_max == UINT32_MAX) return 1;
    return actual_max <= expected_max;
}

static int wasm__tabletype_matches_import(const wasm_module_t* mod,
                                          const wasm_table_t* actual_table,
                                          wasm_valtype_t expected_type,
                                          const wasm_reftype_t* expected_ref,
                                          uint32_t expected_min,
                                          uint32_t expected_max) {
    actual_table = wasm__table_actual_const(actual_table);
    if (!actual_table) return 0;
    if (!wasm__typed_valtype_is_subtype(mod,
                                        actual_table->reftype,
                                        &actual_table->reftype_info,
                                        expected_type,
                                        expected_ref)) {
        return 0;
    }
    return wasm__limits_match_import(actual_table->size,
                                     actual_table->max_size,
                                     expected_min,
                                     expected_max);
}

static int wasm__memorytype_matches_import(const wasm_memory_t* actual_memory,
                                           uint32_t expected_min,
                                           uint32_t expected_max) {
    actual_memory = wasm__memory_actual_const(actual_memory);
    if (!actual_memory) return 0;
    return wasm__limits_match_import(actual_memory->pages,
                                     actual_memory->max_pages,
                                     expected_min,
                                     expected_max);
}

static wasm_error_t wasm__read_heaptype(wasm_module_t* mod,
                                        wasm__reader_t* r,
                                        wasm_reftype_t* out_reftype) {
    int64_t type_index;
    int nullable = out_reftype ? out_reftype->nullable : 0;

    wasm__clear_reftype(out_reftype);
    if (out_reftype) out_reftype->nullable = nullable;

    if (r->ptr >= r->end) return WASM_ERR_MALFORMED;
    if (wasm__is_absheaptype_byte(*r->ptr)) {
        out_reftype->type = wasm__abstract_heap_byte_to_type(wasm__read_u8(r));
        return wasm__require_valtype_feature(mod, out_reftype->type);
    }

    type_index = wasm__read_leb128_i64(r);
    if (type_index < 0 || type_index > (int64_t)UINT32_MAX) return WASM_ERR_MALFORMED;
    out_reftype->has_type_index = 1;
    out_reftype->type_index = (uint32_t)type_index;
    if (mod && out_reftype->type_index >= mod->num_types) return WASM_ERR_MALFORMED;

    if (mod) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
        if (err != WASM_OK) return err;
        return wasm__resolve_reftype(mod, out_reftype);
    }

    return WASM_OK;
}

static wasm_error_t wasm__runtime_make_typeidx_reftype(wasm_module_t* mod,
                                                       uint32_t type_idx,
                                                       int nullable,
                                                       wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    if (!out_reftype || !mod || type_idx >= mod->num_types) return WASM_ERR_MALFORMED;
    wasm__clear_reftype(out_reftype);
    out_reftype->has_type_index = 1;
    out_reftype->type_index = type_idx;
    out_reftype->nullable = nullable;
    err = wasm__resolve_reftype(mod, out_reftype);
    if (err != WASM_OK) return WASM_ERR_MALFORMED;
    return WASM_OK;
}

static wasm_error_t wasm__runtime_read_heap_reftype(wasm_module_t* mod,
                                                    wasm__reader_t* r,
                                                    int nullable,
                                                    wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    if (!out_reftype) return WASM_ERR_MALFORMED;
    wasm__clear_reftype(out_reftype);
    out_reftype->nullable = nullable;
    err = wasm__read_heaptype(mod, r, out_reftype);
    if (err != WASM_OK) return err;
    if (out_reftype->type == WASM_TYPE_VOID) out_reftype->type = WASM_TYPE_ANYREF;
    return WASM_OK;
}

static wasm_value_t wasm__runtime_cast_success_value(const wasm_reftype_t* target,
                                                     wasm_value_t value) {
    value.type = target && target->type ? target->type : WASM_TYPE_ANYREF;
    return value;
}

static wasm_value_t wasm__runtime_cast_difference_value(const wasm_reftype_t* source,
                                                        const wasm_reftype_t* target,
                                                        wasm_value_t value) {
    wasm_valtype_t diff_type = source && source->type ? source->type : WASM_TYPE_ANYREF;

    if (source && target && source->nullable && !target->nullable &&
        source->has_type_index == target->has_type_index &&
        source->type_index == target->type_index &&
        source->type == target->type) {
        switch (source->type) {
            case WASM_TYPE_FUNCREF:
                diff_type = WASM_TYPE_NOFUNC;
                break;
            case WASM_TYPE_EXTERNREF:
                diff_type = WASM_TYPE_NOEXTERN;
                break;
            default:
                diff_type = WASM_TYPE_NONE;
                break;
        }
        return wasm_ref_null(diff_type);
    }

    value.type = diff_type;
    return value;
}

static wasm_error_t wasm__read_reftype(wasm_module_t* mod,
                                       wasm__reader_t* r,
                                       wasm_valtype_t* out_type,
                                       wasm_reftype_t* out_reftype) {
    wasm_reftype_t reftype;
    uint8_t lead;
    wasm_error_t err;

    if (r->ptr >= r->end) return WASM_ERR_MALFORMED;
    lead = *r->ptr;
    wasm__clear_reftype(&reftype);

    if (wasm__is_absheaptype_byte(lead)) {
        reftype.nullable = 1;
        err = wasm__read_heaptype(mod, r, &reftype);
    } else if (lead == 0x63 || lead == 0x64) {
        r->ptr++;
        reftype.nullable = (lead == 0x63);
        err = wasm__read_heaptype(mod, r, &reftype);
    } else {
        return WASM_ERR_MALFORMED;
    }
    if (err != WASM_OK) return err;

    if (out_reftype) *out_reftype = reftype;
    if (out_type) *out_type = reftype.type ? reftype.type : WASM_TYPE_ANYREF;
    return WASM_OK;
}

static wasm_error_t wasm__read_valtype(wasm_module_t* mod,
                                       wasm__reader_t* r,
                                       wasm_valtype_t* out_type,
                                       wasm_reftype_t* out_reftype) {
    wasm_valtype_t type;

    if (r->ptr >= r->end) return WASM_ERR_MALFORMED;
    if (wasm__is_reftype_lead_byte(*r->ptr)) return wasm__read_reftype(mod, r, out_type, out_reftype);

    type = (wasm_valtype_t)wasm__read_u8(r);
    if (!wasm__is_value_type(type)) return WASM_ERR_MALFORMED;
    if (out_type) *out_type = type;
    if (out_reftype) wasm__clear_reftype(out_reftype);
    return wasm__require_valtype_feature(mod, type);
}

static wasm_error_t wasm__read_unresolved_valtype(wasm_module_t* mod,
                                                  wasm__reader_t* r,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    if (r->ptr >= r->end) return WASM_ERR_MALFORMED;
    if (!wasm__is_reftype_lead_byte(*r->ptr)) return wasm__read_valtype(mod, r, out_type, out_reftype);

    err = wasm__read_reftype(NULL, r, out_type, out_reftype);
    if (err != WASM_OK) return err;
    if (out_reftype && out_reftype->has_type_index) return wasm__require_feature(mod, WASM_FEATURE_GC);
    return wasm__require_valtype_feature(mod, *out_type);
}

static void wasm__skip_heaptype(wasm__reader_t* r) {
    if (r->ptr >= r->end) return;
    if (wasm__is_absheaptype_byte(*r->ptr)) {
        r->ptr++;
        return;
    }
    (void)wasm__read_leb128_i64(r);
}

static void wasm__skip_reftype(wasm__reader_t* r) {
    if (r->ptr >= r->end) return;
    if (wasm__is_absheaptype_byte(*r->ptr)) {
        r->ptr++;
        return;
    }
    if (*r->ptr == 0x63 || *r->ptr == 0x64) {
        r->ptr++;
        wasm__skip_heaptype(r);
        return;
    }
    r->ptr = r->end;
}

static void wasm__skip_valtype(wasm__reader_t* r) {
    if (r->ptr >= r->end) return;
    if (wasm__is_reftype_lead_byte(*r->ptr)) {
        wasm__skip_reftype(r);
        return;
    }
    r->ptr++;
}

static int wasm__is_packedtype_byte(uint8_t byte) {
    return byte == 0x77 || byte == 0x78;
}

static int wasm__is_valtype_byte(uint8_t byte) {
    switch (byte) {
        case 0x63:
        case 0x64:
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
    WASM_FREE(ft->param_reftypes);
    WASM_FREE(ft->result_reftypes);
    ft->params = NULL;
    ft->results = NULL;
    ft->param_reftypes = NULL;
    ft->result_reftypes = NULL;
    ft->num_params = 0;
    ft->num_results = 0;
}

static void wasm__free_structtype(wasm_structtype_t* st) {
    WASM_FREE(st->fields);
    st->fields = NULL;
    st->num_fields = 0;
}

static void wasm__free_comptype(wasm_comptype_t* type) {
    WASM_FREE(type->supertypes);
    type->supertypes = NULL;
    type->num_supertypes = 0;

    switch (type->kind) {
        case WASM_COMP_FUNC:
            wasm__free_functype(&type->of.func);
            break;
        case WASM_COMP_STRUCT:
            wasm__free_structtype(&type->of.struct_);
            break;
        case WASM_COMP_ARRAY:
            break;
        default:
            break;
    }

    type->kind = WASM_COMP_FUNC;
    type->rec_group = 0;
    type->canonical_id = 0;
    type->is_final = 0;
}

static wasm_error_t wasm__copy_functype(wasm_functype_t* dst, const wasm_functype_t* src) {
    memset(dst, 0, sizeof(*dst));
    dst->num_params = src->num_params;
    dst->num_results = src->num_results;

    if (src->num_params > 0) {
        dst->params = (wasm_valtype_t*)WASM_MALLOC(src->num_params * sizeof(wasm_valtype_t));
        dst->param_reftypes = (wasm_reftype_t*)WASM_CALLOC(src->num_params, sizeof(wasm_reftype_t));
        if (!dst->params || !dst->param_reftypes) {
            wasm__free_functype(dst);
            return WASM_ERR_OOM;
        }
        if (src->params)
            memcpy(dst->params, src->params, src->num_params * sizeof(wasm_valtype_t));
        else
            memset(dst->params, 0, src->num_params * sizeof(wasm_valtype_t));
        if (src->param_reftypes)
            memcpy(dst->param_reftypes, src->param_reftypes, src->num_params * sizeof(wasm_reftype_t));
    }

    if (src->num_results > 0) {
        dst->results = (wasm_valtype_t*)WASM_MALLOC(src->num_results * sizeof(wasm_valtype_t));
        dst->result_reftypes = (wasm_reftype_t*)WASM_CALLOC(src->num_results, sizeof(wasm_reftype_t));
        if (!dst->results || !dst->result_reftypes) {
            wasm__free_functype(dst);
            return WASM_ERR_OOM;
        }
        if (src->results)
            memcpy(dst->results, src->results, src->num_results * sizeof(wasm_valtype_t));
        else
            memset(dst->results, 0, src->num_results * sizeof(wasm_valtype_t));
        if (src->result_reftypes)
            memcpy(dst->result_reftypes, src->result_reftypes, src->num_results * sizeof(wasm_reftype_t));
    }

    return WASM_OK;
}

static int wasm__functype_equal(const wasm_functype_t* lhs, const wasm_functype_t* rhs) {
    uint32_t i;

    if (lhs->num_params != rhs->num_params || lhs->num_results != rhs->num_results)
        return 0;
    for (i = 0; i < lhs->num_params; i++) {
        const wasm_reftype_t* lhs_ref = lhs->param_reftypes ? lhs->param_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->param_reftypes ? rhs->param_reftypes + i : NULL;
        if (lhs->params[i] != rhs->params[i]) return 0;
        if (wasm__is_ref_type(lhs->params[i]) || wasm__is_ref_type(rhs->params[i])) {
            if (wasm__has_reftype_info(lhs_ref) || wasm__has_reftype_info(rhs_ref)) {
                if (!wasm__reftype_equal(lhs_ref, rhs_ref)) return 0;
            }
        }
    }
    for (i = 0; i < lhs->num_results; i++) {
        const wasm_reftype_t* lhs_ref = lhs->result_reftypes ? lhs->result_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->result_reftypes ? rhs->result_reftypes + i : NULL;
        if (lhs->results[i] != rhs->results[i]) return 0;
        if (wasm__is_ref_type(lhs->results[i]) || wasm__is_ref_type(rhs->results[i])) {
            if (wasm__has_reftype_info(lhs_ref) || wasm__has_reftype_info(rhs_ref)) {
                if (!wasm__reftype_equal(lhs_ref, rhs_ref)) return 0;
            }
        }
    }
    return 1;
}

static int wasm__storagetype_equal(const wasm_storagetype_t* lhs, const wasm_storagetype_t* rhs) {
    if (lhs->kind != rhs->kind) return 0;
    if (lhs->kind == WASM_STORAGE_PACKED) return lhs->of.packed_type == rhs->of.packed_type;
    if (lhs->of.valtype != rhs->of.valtype) return 0;
    if (!wasm__is_ref_type(lhs->of.valtype) && !wasm__is_ref_type(rhs->of.valtype)) return 1;
    if (wasm__has_reftype_info(&lhs->ref_type) || wasm__has_reftype_info(&rhs->ref_type))
        return wasm__reftype_equal(&lhs->ref_type, &rhs->ref_type);
    return 1;
}

static int wasm__fieldtype_equal(const wasm_fieldtype_t* lhs, const wasm_fieldtype_t* rhs) {
    return lhs->is_mutable == rhs->is_mutable &&
           wasm__storagetype_equal(&lhs->storage, &rhs->storage);
}

static int wasm__comptype_body_equal(const wasm_comptype_t* lhs, const wasm_comptype_t* rhs) {
    uint32_t i;

    if (lhs->kind != rhs->kind) return 0;

    switch (lhs->kind) {
        case WASM_COMP_FUNC:
            return wasm__functype_equal(&lhs->of.func, &rhs->of.func);
        case WASM_COMP_STRUCT:
            if (lhs->of.struct_.num_fields != rhs->of.struct_.num_fields) return 0;
            for (i = 0; i < lhs->of.struct_.num_fields; i++) {
                if (!wasm__fieldtype_equal(&lhs->of.struct_.fields[i], &rhs->of.struct_.fields[i]))
                    return 0;
            }
            return 1;
        case WASM_COMP_ARRAY:
            return wasm__fieldtype_equal(&lhs->of.array.field, &rhs->of.array.field);
        default:
            return 0;
    }
}

static wasm_error_t wasm__canonicalize_types(wasm_module_t* mod) {
    uint32_t i;
    uint32_t next_id = 1;

    if (!mod) return WASM_OK;

    for (i = 0; i < mod->num_types; i++) mod->types[i].canonical_id = 0;

    for (i = 0; i < mod->num_types; i++) {
        uint32_t j;

        for (j = 0; j < i; j++) {
            if (wasm__comptype_body_equal(&mod->types[i], &mod->types[j])) {
                mod->types[i].canonical_id = mod->types[j].canonical_id;
                break;
            }
        }

        if (j == i) {
            if (next_id == 0) return WASM_ERR_OOM;
            mod->types[i].canonical_id = next_id++;
        }
    }

    return WASM_OK;
}

static int wasm__type_equal(const wasm_module_t* mod, uint32_t lhs_idx, uint32_t rhs_idx) {
    const wasm_comptype_t* lhs;
    const wasm_comptype_t* rhs;

    if (!mod || lhs_idx >= mod->num_types || rhs_idx >= mod->num_types) return 0;
    if (lhs_idx == rhs_idx) return 1;

    lhs = &mod->types[lhs_idx];
    rhs = &mod->types[rhs_idx];
    if (lhs->canonical_id != 0 && rhs->canonical_id != 0) return lhs->canonical_id == rhs->canonical_id;
    return wasm__comptype_body_equal(lhs, rhs);
}

static int wasm__is_heap_subtype(const wasm_module_t* mod,
                                 wasm_valtype_t subtype,
                                 wasm_valtype_t supertype) {
    (void)mod;

    if (subtype == supertype) return 1;

    switch (subtype) {
        case WASM_TYPE_NONE:
            return supertype == WASM_TYPE_I31REF || supertype == WASM_TYPE_STRUCTREF ||
                   supertype == WASM_TYPE_ARRAYREF || supertype == WASM_TYPE_EQREF ||
                   supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return supertype == WASM_TYPE_EQREF || supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_EQREF:
            return supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_NOFUNC:
            return supertype == WASM_TYPE_FUNCREF;
        case WASM_TYPE_NOEXTERN:
            return supertype == WASM_TYPE_EXTERNREF;
        default:
            return 0;
    }
}

static int wasm__is_reftype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype) {
    if (!wasm__is_ref_type(subtype) || !wasm__is_ref_type(supertype)) return 0;
    return wasm__is_heap_subtype(mod, subtype, supertype);
}

static int wasm__is_valtype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype) {
    if (subtype == supertype) return 1;
    if (wasm__is_ref_type(subtype) && wasm__is_ref_type(supertype))
        return wasm__is_reftype_subtype(mod, subtype, supertype);
    return 0;
}

static int wasm__is_storagetype_subtype(const wasm_module_t* mod,
                                        const wasm_storagetype_t* subtype,
                                        const wasm_storagetype_t* supertype) {
    if (subtype->kind != supertype->kind) return 0;
    if (subtype->kind == WASM_STORAGE_PACKED)
        return subtype->of.packed_type == supertype->of.packed_type;
    if ((wasm__has_reftype_info(&subtype->ref_type) || wasm__has_reftype_info(&supertype->ref_type)) &&
        wasm__is_ref_type(subtype->of.valtype) && wasm__is_ref_type(supertype->of.valtype))
        return wasm__reftype_is_subtype(mod, &subtype->ref_type, &supertype->ref_type);
    return wasm__is_valtype_subtype(mod, subtype->of.valtype, supertype->of.valtype);
}

static int wasm__is_fieldtype_subtype(const wasm_module_t* mod,
                                      const wasm_fieldtype_t* subtype,
                                      const wasm_fieldtype_t* supertype) {
    if (subtype->is_mutable != supertype->is_mutable) return 0;
    if (subtype->is_mutable)
        return wasm__storagetype_equal(&subtype->storage, &supertype->storage);
    return wasm__is_storagetype_subtype(mod, &subtype->storage, &supertype->storage);
}

static int wasm__is_comptype_immediate_subtype(const wasm_module_t* mod,
                                               const wasm_comptype_t* subtype,
                                               const wasm_comptype_t* supertype) {
    uint32_t i;

    if (subtype->kind != supertype->kind) return 0;

    switch (subtype->kind) {
        case WASM_COMP_FUNC:
            if (subtype->of.func.num_params != supertype->of.func.num_params ||
                subtype->of.func.num_results != supertype->of.func.num_results)
                return 0;
            for (i = 0; i < subtype->of.func.num_params; i++) {
                if (!wasm__is_valtype_subtype(mod,
                                              supertype->of.func.params[i],
                                              subtype->of.func.params[i]))
                    return 0;
            }
            for (i = 0; i < subtype->of.func.num_results; i++) {
                if (!wasm__is_valtype_subtype(mod,
                                              subtype->of.func.results[i],
                                              supertype->of.func.results[i]))
                    return 0;
            }
            return 1;
        case WASM_COMP_STRUCT:
            if (subtype->of.struct_.num_fields < supertype->of.struct_.num_fields) return 0;
            for (i = 0; i < supertype->of.struct_.num_fields; i++) {
                if (!wasm__is_fieldtype_subtype(mod,
                                                &subtype->of.struct_.fields[i],
                                                &supertype->of.struct_.fields[i]))
                    return 0;
            }
            return 1;
        case WASM_COMP_ARRAY:
            return wasm__is_fieldtype_subtype(mod,
                                              &subtype->of.array.field,
                                              &supertype->of.array.field);
        default:
            return 0;
    }
}

static inline int wasm__is_subtype(const wasm_module_t* mod,
                                   uint32_t subtype_idx,
                                   uint32_t supertype_idx) {
    const wasm_comptype_t* subtype;
    uint32_t i;

    if (!mod || subtype_idx >= mod->num_types || supertype_idx >= mod->num_types) return 0;
    if (wasm__type_equal(mod, subtype_idx, supertype_idx)) return 1;

    subtype = &mod->types[subtype_idx];
    for (i = 0; i < subtype->num_supertypes; i++) {
        uint32_t parent_idx = subtype->supertypes[i];

        if (parent_idx >= mod->num_types) continue;
        if (wasm__is_subtype(mod, parent_idx, supertype_idx)) return 1;
    }

    return 0;
}

static int wasm__functype_is_unspecified(const wasm_functype_t* ft) {
    return ft && ft->num_params == 0 && ft->num_results == 0 &&
           ft->params == NULL && ft->results == NULL;
}

static wasm_functype_t* wasm__type_as_functype(wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_FUNC) return NULL;
    return &type->of.func;
}

static const wasm_functype_t* wasm__type_as_const_functype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_FUNC) return NULL;
    return &type->of.func;
}

static wasm_functype_t* wasm__module_functype(wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_functype(&mod->types[type_idx]);
}

static const wasm_functype_t* wasm__module_const_functype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_functype(&mod->types[type_idx]);
}

static const wasm_structtype_t* wasm__type_as_const_structtype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_STRUCT) return NULL;
    return &type->of.struct_;
}

static const wasm_arraytype_t* wasm__type_as_const_arraytype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_ARRAY) return NULL;
    return &type->of.array;
}

static const wasm_structtype_t* wasm__module_const_structtype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_structtype(&mod->types[type_idx]);
}

static const wasm_arraytype_t* wasm__module_const_arraytype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_arraytype(&mod->types[type_idx]);
}

static wasm_gc_struct_t* wasm__gc_alloc_struct_object(wasm_runtime_t* rt,
                                                      const wasm_module_t* mod,
                                                      uint32_t type_index,
                                                      const wasm_structtype_t* type);
static wasm_gc_array_t* wasm__gc_alloc_array_object(wasm_runtime_t* rt,
                                                    const wasm_module_t* mod,
                                                    uint32_t type_index,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t length);

static wasm_error_t wasm__append_types(wasm_module_t* mod, uint32_t count, uint32_t* out_base) {
    uint32_t base;
    size_t new_count;
    wasm_comptype_t* types;

    if (count == 0) {
        if (out_base) *out_base = mod->num_types;
        return WASM_OK;
    }
    if (count > UINT32_MAX - mod->num_types) return WASM_ERR_OOM;

    base = mod->num_types;
    new_count = (size_t)base + (size_t)count;
    types = (wasm_comptype_t*)WASM_REALLOC(mod->types, new_count * sizeof(*types));
    if (!types) return WASM_ERR_OOM;

    mod->types = types;
    memset(mod->types + base, 0, count * sizeof(*mod->types));
    mod->num_types = base + count;
    if (out_base) *out_base = base;
    return WASM_OK;
}

static wasm_error_t wasm__append_rec_groups(wasm_module_t* mod, uint32_t count, uint32_t* out_base) {
    uint32_t base;
    size_t new_count;
    wasm_recgroup_t* groups;

    if (count == 0) {
        if (out_base) *out_base = mod->num_rec_groups;
        return WASM_OK;
    }
    if (count > UINT32_MAX - mod->num_rec_groups) return WASM_ERR_OOM;

    base = mod->num_rec_groups;
    new_count = (size_t)base + (size_t)count;
    groups = (wasm_recgroup_t*)WASM_REALLOC(mod->rec_groups, new_count * sizeof(*groups));
    if (!groups) return WASM_ERR_OOM;

    mod->rec_groups = groups;
    memset(mod->rec_groups + base, 0, count * sizeof(*mod->rec_groups));
    mod->num_rec_groups = base + count;
    if (out_base) *out_base = base;
    return WASM_OK;
}

static wasm_error_t wasm__decode_functype(wasm_module_t* mod, wasm__reader_t* r, wasm_functype_t* ft) {
    uint32_t p;

    ft->num_params = wasm__read_leb128_u32(r);
    if (ft->num_params > 0) {
        ft->params = (wasm_valtype_t*)WASM_MALLOC(ft->num_params * sizeof(wasm_valtype_t));
        ft->param_reftypes = (wasm_reftype_t*)WASM_CALLOC(ft->num_params, sizeof(wasm_reftype_t));
        if (!ft->params || !ft->param_reftypes) return WASM_ERR_OOM;
    }
    for (p = 0; p < ft->num_params; p++) {
        wasm_error_t err = wasm__read_unresolved_valtype(mod, r, &ft->params[p], &ft->param_reftypes[p]);
        if (err != WASM_OK) return err;
    }

    ft->num_results = wasm__read_leb128_u32(r);
    if (ft->num_results > 1) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_MULTI_VALUE);
        if (err != WASM_OK) return err;
    }
    if (ft->num_results > 0) {
        ft->results = (wasm_valtype_t*)WASM_MALLOC(ft->num_results * sizeof(wasm_valtype_t));
        ft->result_reftypes = (wasm_reftype_t*)WASM_CALLOC(ft->num_results, sizeof(wasm_reftype_t));
        if (!ft->results || !ft->result_reftypes) return WASM_ERR_OOM;
    }
    for (p = 0; p < ft->num_results; p++) {
        wasm_error_t err = wasm__read_unresolved_valtype(mod, r, &ft->results[p], &ft->result_reftypes[p]);
        if (err != WASM_OK) return err;
    }

    return WASM_OK;
}

static wasm_error_t wasm__read_storage_type(wasm_module_t* mod,
                                            wasm__reader_t* r,
                                            wasm_storagetype_t* out_type) {
    uint8_t byte = wasm__read_u8(r);

    wasm__clear_reftype(&out_type->ref_type);

    if (wasm__is_packedtype_byte(byte)) {
        out_type->kind = WASM_STORAGE_PACKED;
        out_type->of.packed_type = (wasm_packedtype_t)byte;
        return WASM_OK;
    }

    r->ptr--;
    out_type->kind = WASM_STORAGE_VALTYPE;
    return wasm__read_unresolved_valtype(mod, r, &out_type->of.valtype, &out_type->ref_type);
}

static wasm_error_t wasm__decode_fieldtype(wasm_module_t* mod,
                                           wasm__reader_t* r,
                                           wasm_fieldtype_t* field) {
    uint8_t mutability;
    wasm_error_t err = wasm__read_storage_type(mod, r, &field->storage);
    if (err != WASM_OK) return err;

    mutability = wasm__read_u8(r);
    if (mutability > 1) return WASM_ERR_MALFORMED;
    field->is_mutable = (int)mutability;
    return WASM_OK;
}

static wasm_error_t wasm__decode_comptype_body(wasm_module_t* mod,
                                               wasm__reader_t* r,
                                               uint8_t opcode,
                                               wasm_comptype_t* type) {
    switch (opcode) {
        case 0x60:
            type->kind = WASM_COMP_FUNC;
            return wasm__decode_functype(mod, r, &type->of.func);
        case 0x5F: {
            uint32_t i;
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
            if (err != WASM_OK) return err;

            type->kind = WASM_COMP_STRUCT;
            type->of.struct_.num_fields = wasm__read_leb128_u32(r);
            if (type->of.struct_.num_fields > 0) {
                type->of.struct_.fields = (wasm_fieldtype_t*)WASM_CALLOC(
                    type->of.struct_.num_fields, sizeof(wasm_fieldtype_t));
                if (!type->of.struct_.fields) return WASM_ERR_OOM;
            }
            for (i = 0; i < type->of.struct_.num_fields; i++) {
                err = wasm__decode_fieldtype(mod, r, &type->of.struct_.fields[i]);
                if (err != WASM_OK) return err;
            }
            return WASM_OK;
        }
        case 0x5E: {
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
            if (err != WASM_OK) return err;

            type->kind = WASM_COMP_ARRAY;
            return wasm__decode_fieldtype(mod, r, &type->of.array.field);
        }
        default:
            return WASM_ERR_MALFORMED;
    }
}

static wasm_error_t wasm__decode_type_entry(wasm_module_t* mod,
                                            wasm__reader_t* r,
                                            uint8_t opcode,
                                            uint32_t rec_group_idx,
                                            wasm_comptype_t* type) {
    memset(type, 0, sizeof(*type));
    type->rec_group = rec_group_idx;
    type->is_final = 1;

    if (opcode == 0x50 || opcode == 0x4F) {
        uint32_t i;
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
        if (err != WASM_OK) return err;

        type->is_final = (opcode == 0x4F);
        type->num_supertypes = wasm__read_leb128_u32(r);
        if (type->num_supertypes > 0) {
            type->supertypes = (uint32_t*)WASM_MALLOC(type->num_supertypes * sizeof(uint32_t));
            if (!type->supertypes) return WASM_ERR_OOM;
        }
        for (i = 0; i < type->num_supertypes; i++)
            type->supertypes[i] = wasm__read_leb128_u32(r);
        opcode = wasm__read_u8(r);
    }

    return wasm__decode_comptype_body(mod, r, opcode, type);
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

static wasm_error_t wasm__init_expr_stack_pop_expect(wasm_module_t* mod,
                                                     wasm__init_expr_stack_t* stack,
                                                     wasm_valtype_t expected_type,
                                                     const wasm_reftype_t* expected_ref,
                                                     wasm_value_t* out_value) {
    wasm_value_t value;

    if (stack->size == 0) return WASM_ERR_MALFORMED;
    value = stack->values[--stack->size];
    if (!wasm__runtime_value_matches_type(mod, &value, expected_type, expected_ref))
        return WASM_ERR_TYPE_MISMATCH;
    *out_value = value;
    return WASM_OK;
}

static wasm_error_t wasm__init_expr_stack_pop_typed(wasm__init_expr_stack_t* stack,
                                                    wasm_valtype_t expected_type,
                                                    wasm_value_t* out_value) {
    return wasm__init_expr_stack_pop_expect(NULL, stack, expected_type, NULL, out_value);
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

static void wasm__free_import_info(wasm_import_info_t* info) {
    if (!info) return;
    WASM_FREE(info->module);
    WASM_FREE(info->name);
    wasm__clear_reftype(&info->ref_type);
    memset(info, 0, sizeof(*info));
}

static wasm_error_t wasm__append_import_infos(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_imports;
    wasm_import_info_t* infos;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    infos = (wasm_import_info_t*)WASM_REALLOC(mod->imports, (base + count) * sizeof(wasm_import_info_t));
    if (!infos) return WASM_ERR_OOM;

    mod->imports = infos;
    memset(mod->imports + base, 0, count * sizeof(wasm_import_info_t));
    mod->num_imports = base + count;
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

static wasm_table_t* wasm__table_actual(wasm_table_t* table) {
    while (table && table->is_import && table->import_table && table->import_table != table)
        table = table->import_table;
    return table;
}

static const wasm_table_t* wasm__table_actual_const(const wasm_table_t* table) {
    while (table && table->is_import && table->import_table && table->import_table != table)
        table = table->import_table;
    return table;
}

static wasm_memory_t* wasm__memory_actual(wasm_memory_t* memory) {
    while (memory && memory->is_import && memory->import_memory && memory->import_memory != memory)
        memory = memory->import_memory;
    return memory;
}

static const wasm_memory_t* wasm__memory_actual_const(const wasm_memory_t* memory) {
    while (memory && memory->is_import && memory->import_memory && memory->import_memory != memory)
        memory = memory->import_memory;
    return memory;
}

static void wasm__free_table(wasm_table_t* table) {
    if (!table->is_import) WASM_FREE(table->elems);
    memset(table, 0, sizeof(*table));
}

static void wasm__free_memory(wasm_memory_t* memory) {
    if (!memory->is_import) WASM_FREE(memory->data);
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

static wasm_table_t* wasm__table_at(wasm_module_t* mod, uint32_t table_index) {
    if (!mod || table_index >= mod->num_tables) return NULL;
    return wasm__table_actual(&mod->tables[table_index]);
}

static wasm_memory_t* wasm__memory_at(wasm_module_t* mod, uint32_t memory_index) {
    if (!mod || memory_index >= mod->num_memories) return NULL;
    return wasm__memory_actual(&mod->memories[memory_index]);
}

static wasm_error_t wasm__init_memory_storage(wasm_memory_t* memory) {
    memory = wasm__memory_actual(memory);
    if (!memory) return WASM_ERR_MALFORMED;
    if (memory->pages == 0) return WASM_OK;
    if (memory->data) return WASM_OK;

    memory->data = (uint8_t*)WASM_CALLOC(memory->pages, WASM_PAGE_SIZE);
    if (!memory->data) return WASM_ERR_OOM;
    return WASM_OK;
}

static wasm_error_t wasm__init_table_storage(wasm_table_t* table) {
    uint32_t i;

    table = wasm__table_actual(table);
    if (!table) return WASM_ERR_MALFORMED;
    if (table->size == 0) return WASM_OK;
    if (!wasm__is_ref_type(table->reftype)) return WASM_ERR_MALFORMED;
    if (table->elems) return WASM_OK;

    table->elems = (wasm_value_t*)WASM_MALLOC(table->size * sizeof(wasm_value_t));
    if (!table->elems) return WASM_ERR_OOM;
    for (i = 0; i < table->size; i++)
        table->elems[i] = wasm__typed_ref_null(table->reftype, &table->reftype_info);
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

static void wasm__free_custom_section(wasm_custom_section_t* section) {
    WASM_FREE(section->name);
    memset(section, 0, sizeof(*section));
}

static wasm_error_t wasm__append_custom_sections(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_custom_sections;
    wasm_custom_section_t* sections;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    sections = (wasm_custom_section_t*)WASM_REALLOC(mod->custom_sections,
                                                    (base + count) * sizeof(wasm_custom_section_t));
    if (!sections) return WASM_ERR_OOM;

    mod->custom_sections = sections;
    memset(mod->custom_sections + base, 0, count * sizeof(wasm_custom_section_t));
    mod->num_custom_sections = base + count;
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

static wasm_error_t wasm__read_typed_select_immediate(wasm_module_t* mod,
                                                      wasm__reader_t* r,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype) {
    uint32_t count = wasm__read_leb128_u32(r);

    if (count != 1) return WASM_ERR_MALFORMED;
    return wasm__read_valtype(mod, r, out_type, out_reftype);
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
        uint32_t wrapped_value = (uint32_t)signed_value;

        switch (subop) {
            case 0xA0:
                wasm__v128_set_u32(&result, lane, signed_value < 0 ? (uint32_t)(0u - wrapped_value) : wrapped_value);
                break;
            default:
                wasm__v128_set_u32(&result, lane, (uint32_t)(0u - wrapped_value));
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
                    wasm__v128_set_u64(&result, lane, (uint64_t)(0ull - unsigned_value));
                else
                    wasm__v128_set_u64(&result, lane, unsigned_value);
                break;
            default:
                wasm__v128_set_u64(&result, lane, (uint64_t)(0ull - unsigned_value));
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
        table = wasm__table_at(mod, segment->table_index);
        if (!table) return WASM_ERR_MALFORMED;
        if (!wasm__typed_valtype_is_subtype(mod,
                                            segment->elem_type,
                                            &segment->elem_ref_type,
                                            table->reftype,
                                            &table->reftype_info))
            return WASM_ERR_TYPE_MISMATCH;
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
                wasm_reftype_t reftype = { 0 };

                err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) break;
                err = wasm__read_heaptype(mod, r, &reftype);
                if (err != WASM_OK) break;
                reftype.nullable = 1;
                type = reftype.type ? reftype.type : WASM_TYPE_ANYREF;

                value = wasm__typed_ref_null(type, &reftype);
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
            case 0xFB: {
                uint32_t subop = wasm__read_leb128_u32(r);

                err = wasm__require_feature(mod, WASM_FEATURE_GC);
                if (err != WASM_OK) break;

                switch (subop) {
                    case 0x00:
                    case 0x01: {
                        uint32_t type_idx = wasm__read_leb128_u32(r);
                        const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
                        wasm_gc_struct_t* object;
                        uint32_t i;

                        if (!st) {
                            err = WASM_ERR_MALFORMED;
                            break;
                        }
                        object = wasm__gc_alloc_struct_object(mod->rt, mod, type_idx, st);
                        if (!object) {
                            err = WASM_ERR_OOM;
                            break;
                        }
                        if (subop == 0x00) {
                            for (i = st->num_fields; i > 0; i--) {
                                wasm_value_t field_value;
                                const wasm_fieldtype_t* field = &st->fields[i - 1];
                                wasm_valtype_t expected_type =
                                    (field->storage.kind == WASM_STORAGE_PACKED) ? WASM_TYPE_I32
                                                                                 : field->storage.of.valtype;

                                err = wasm__init_expr_stack_pop_expect(mod,
                                                                       &stack,
                                                                       expected_type,
                                                                       &field->storage.ref_type,
                                                                       &field_value);
                                if (err != WASM_OK) break;
                                err = wasm__gc_struct_store_field(object, st, i - 1, field_value);
                                if (err != WASM_OK) break;
                            }
                            if (err != WASM_OK) break;
                        } else {
                            for (i = 0; i < st->num_fields; i++) {
                                err = wasm__gc_struct_store_field(object, st, i,
                                                                  wasm__gc_field_default_value(&st->fields[i]));
                                if (err != WASM_OK) break;
                            }
                            if (err != WASM_OK) break;
                        }

                        err = wasm__init_expr_stack_push(&stack,
                                                         wasm__gc_type_ref_value(mod, type_idx, &object->header));
                        break;
                    }
                    case 0x06:
                    case 0x07:
                    case 0x08: {
                        uint32_t type_idx = wasm__read_leb128_u32(r);
                        const wasm_arraytype_t* atype = wasm__module_const_arraytype(mod, type_idx);
                        wasm_gc_array_t* object;
                        uint32_t count = 0;
                        uint32_t i;

                        if (!atype) {
                            err = WASM_ERR_MALFORMED;
                            break;
                        }

                        if (subop == 0x08) {
                            count = wasm__read_leb128_u32(r);
                        } else {
                            wasm_value_t length_value;

                            if (subop == 0x06) {
                                wasm_value_t init_value;
                                wasm_valtype_t expected_type =
                                    (atype->field.storage.kind == WASM_STORAGE_PACKED) ? WASM_TYPE_I32
                                                                                       : atype->field.storage.of.valtype;

                                err = wasm__init_expr_stack_pop_expect(mod,
                                                                       &stack,
                                                                       expected_type,
                                                                       &atype->field.storage.ref_type,
                                                                       &init_value);
                                if (err != WASM_OK) break;
                                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &length_value);
                                if (err != WASM_OK) break;
                                if (length_value.of.i32 < 0) {
                                    err = WASM_ERR_OUT_OF_BOUNDS;
                                    break;
                                }
                                count = (uint32_t)length_value.of.i32;
                                object = wasm__gc_alloc_array_object(mod->rt, mod, type_idx, atype, count);
                                if (!object) {
                                    err = WASM_ERR_OOM;
                                    break;
                                }
                                for (i = 0; i < count; i++) {
                                    err = wasm__gc_array_store_element(object, atype, i, init_value);
                                    if (err != WASM_OK) break;
                                }
                                if (err != WASM_OK) break;
                                err = wasm__init_expr_stack_push(&stack,
                                                                 wasm__gc_type_ref_value(mod, type_idx, &object->header));
                                break;
                            }

                            err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &length_value);
                            if (err != WASM_OK) break;
                            if (length_value.of.i32 < 0) {
                                err = WASM_ERR_OUT_OF_BOUNDS;
                                break;
                            }
                            count = (uint32_t)length_value.of.i32;
                        }

                        object = wasm__gc_alloc_array_object(mod->rt, mod, type_idx, atype, count);
                        if (!object) {
                            err = WASM_ERR_OOM;
                            break;
                        }

                        if (subop == 0x07) {
                            wasm_value_t default_value = wasm__gc_field_default_value(&atype->field);
                            for (i = 0; i < count; i++) {
                                err = wasm__gc_array_store_element(object, atype, i, default_value);
                                if (err != WASM_OK) break;
                            }
                            if (err != WASM_OK) break;
                        } else {
                            for (i = count; i > 0; i--) {
                                wasm_value_t elem_value;
                                wasm_valtype_t expected_type =
                                    (atype->field.storage.kind == WASM_STORAGE_PACKED) ? WASM_TYPE_I32
                                                                                       : atype->field.storage.of.valtype;

                                err = wasm__init_expr_stack_pop_expect(mod,
                                                                       &stack,
                                                                       expected_type,
                                                                       &atype->field.storage.ref_type,
                                                                       &elem_value);
                                if (err != WASM_OK) break;
                                err = wasm__gc_array_store_element(object, atype, i - 1, elem_value);
                                if (err != WASM_OK) break;
                            }
                            if (err != WASM_OK) break;
                        }

                        err = wasm__init_expr_stack_push(&stack,
                                                         wasm__gc_type_ref_value(mod, type_idx, &object->header));
                        break;
                    }
                    case 0x1A: {
                        wasm_value_t value;

                        err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_EXTERNREF, &value);
                        if (err != WASM_OK) break;
                        value.type = WASM_TYPE_ANYREF;
                        err = wasm__init_expr_stack_push(&stack, value);
                        break;
                    }
                    case 0x1B: {
                        wasm_value_t value;

                        err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_ANYREF, &value);
                        if (err != WASM_OK) break;
                        value.type = WASM_TYPE_EXTERNREF;
                        err = wasm__init_expr_stack_push(&stack, value);
                        break;
                    }
                    case 0x1C: {
                        wasm_value_t value;

                        err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &value);
                        if (err != WASM_OK) break;
                        value.type = WASM_TYPE_I31REF;
                        value.of.gc_ref = wasm__gc_i31_encode(value.of.i32);
                        err = wasm__init_expr_stack_push(&stack, value);
                        break;
                    }
                    default:
                        err = WASM_ERR_MALFORMED;
                        break;
                }
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
    uint32_t i = rt ? rt->num_global_imports : 0u;

    while (i > 0u) {
        i--;
        if (strcmp(rt->global_imports[i].module, module) == 0 &&
            strcmp(rt->global_imports[i].name, name) == 0)
            return &rt->global_imports[i];
    }

    return NULL;
}

static wasm_table_import_t* wasm__find_table_import(wasm_runtime_t* rt,
                                                    const char* module,
                                                    const char* name) {
    uint32_t i = rt ? rt->num_table_imports : 0u;

    while (i > 0u) {
        i--;
        if (strcmp(rt->table_imports[i].module, module) == 0 &&
            strcmp(rt->table_imports[i].name, name) == 0)
            return &rt->table_imports[i];
    }

    return NULL;
}

static wasm_memory_import_t* wasm__find_memory_import(wasm_runtime_t* rt,
                                                      const char* module,
                                                      const char* name) {
    uint32_t i = rt ? rt->num_memory_imports : 0u;

    while (i > 0u) {
        i--;
        if (strcmp(rt->memory_imports[i].module, module) == 0 &&
            strcmp(rt->memory_imports[i].name, name) == 0)
            return &rt->memory_imports[i];
    }

    return NULL;
}

static wasm_tag_import_t* wasm__find_tag_import(wasm_runtime_t* rt,
                                                const char* module,
                                                const char* name) {
    uint32_t i = rt ? rt->num_tag_imports : 0u;

    while (i > 0u) {
        i--;
        if (strcmp(rt->tag_imports[i].module, module) == 0 &&
            strcmp(rt->tag_imports[i].name, name) == 0)
            return &rt->tag_imports[i];
    }

    return NULL;
}

/* ── Init / Destroy ───────────────────────────────────────────────── */

static void wasm__gc_heap_destroy(wasm_runtime_t* rt) {
    WASM_GC_FREE(rt->gc_heap);
    rt->gc_heap = NULL;
    rt->gc_heap_size = 0;
    rt->gc_heap_offset = 0;
    rt->gc_allocations = NULL;
}

static void wasm__gc_register_module(wasm_runtime_t* rt, wasm_module_t* mod) {
    if (!rt || !mod) return;
    if (mod->gc_prev || mod->gc_next || rt->gc_modules == mod) return;

    mod->gc_prev = NULL;
    mod->gc_next = rt->gc_modules;
    if (rt->gc_modules) rt->gc_modules->gc_prev = mod;
    rt->gc_modules = mod;
}

static void wasm__gc_unregister_module(wasm_runtime_t* rt, wasm_module_t* mod) {
    if (!rt || !mod) return;

    if (mod->gc_prev)
        mod->gc_prev->gc_next = mod->gc_next;
    else if (rt->gc_modules == mod)
        rt->gc_modules = mod->gc_next;

    if (mod->gc_next) mod->gc_next->gc_prev = mod->gc_prev;
    mod->gc_prev = NULL;
    mod->gc_next = NULL;
}

static int wasm__gc_ptr_in_heap(const wasm_runtime_t* rt, const void* ptr) {
    const uint8_t* addr = (const uint8_t*)ptr;

    if (!rt || !rt->gc_heap || !ptr) return 0;
    return addr >= rt->gc_heap && addr < rt->gc_heap + rt->gc_heap_size;
}

static wasm_error_t wasm__gc_heap_init(wasm_runtime_t* rt, size_t size) {
    wasm__gc_heap_destroy(rt);
    if (size == 0) return WASM_OK;

    rt->gc_heap = (uint8_t*)WASM_GC_ALLOC(size);
    if (!rt->gc_heap) return WASM_ERR_OOM;

    rt->gc_heap_size = size;
    rt->gc_heap_offset = 0;
    rt->gc_allocations = NULL;
    return WASM_OK;
}

static void* wasm__gc_bump_alloc(wasm_runtime_t* rt, size_t size, size_t alignment) {
    size_t aligned_offset = wasm__align_up_size(rt->gc_heap_offset, alignment);

    if (!rt->gc_heap) return NULL;
    if (aligned_offset == (size_t)-1) return NULL;
    if (aligned_offset > rt->gc_heap_size || size > rt->gc_heap_size - aligned_offset) return NULL;

    rt->gc_heap_offset = aligned_offset + size;
    return rt->gc_heap + aligned_offset;
}

static wasm_gc_header_t* wasm__gc_alloc_object(wasm_runtime_t* rt,
                                               const wasm_module_t* mod,
                                               uint32_t type_index,
                                               wasm_gc_object_kind_t kind,
                                               size_t object_size) {
    wasm_gc_header_t* object;

    object = (wasm_gc_header_t*)wasm__gc_bump_alloc(rt, object_size, (size_t)WASM__ALIGNOF(wasm_value_t));
    if (!object) {
        wasm_gc_collect(rt);
        object = (wasm_gc_header_t*)wasm__gc_bump_alloc(rt, object_size, (size_t)WASM__ALIGNOF(wasm_value_t));
        if (!object) return NULL;
    }

    memset(object, 0, object_size);
    object->next_alloc = rt->gc_allocations;
    object->object_size = object_size;
    object->module = (wasm_module_t*)mod;
    object->type_index = type_index;
    object->kind = (uint8_t)kind;
    object->marked = 0;
    rt->gc_allocations = object;
    return object;
}

static wasm_gc_struct_t* wasm__gc_alloc_struct_object(wasm_runtime_t* rt,
                                                      const wasm_module_t* mod,
                                                      uint32_t type_index,
                                                      const wasm_structtype_t* type) {
    size_t object_size;

    if (!wasm__gc_struct_size(type, &object_size)) return NULL;
    return (wasm_gc_struct_t*)wasm__gc_alloc_object(rt, mod, type_index, WASM_GC_OBJ_STRUCT, object_size);
}

static wasm_gc_array_t* wasm__gc_alloc_array_object(wasm_runtime_t* rt,
                                                    const wasm_module_t* mod,
                                                    uint32_t type_index,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t length) {
    size_t object_size;
    wasm_gc_array_t* object;

    if (!wasm__gc_array_total_size(type, length, &object_size)) return NULL;
    object = (wasm_gc_array_t*)wasm__gc_alloc_object(rt, mod, type_index, WASM_GC_OBJ_ARRAY, object_size);
    if (!object) return NULL;
    object->length = length;
    return object;
}

static void wasm__arena_destroy(wasm_runtime_t* rt) {
    WASM_FREE(rt->frame_arena);
    rt->frame_arena = NULL;
    rt->frame_arena_size = 0;
    rt->frame_arena_offset = 0;
}

static wasm_error_t wasm__arena_init(wasm_runtime_t* rt, size_t size) {
    wasm__arena_destroy(rt);
    if (size == 0) return WASM_OK;

    rt->frame_arena = (uint8_t*)WASM_MALLOC(size);
    if (!rt->frame_arena) return WASM_ERR_OOM;

    rt->frame_arena_size = size;
    rt->frame_arena_offset = 0;
    return WASM_OK;
}

static void* wasm__arena_alloc(wasm_runtime_t* rt, size_t size) {
    size_t alignment = sizeof(void*);
    size_t aligned_offset = rt->frame_arena_offset;

    if (!rt->frame_arena) return NULL;
    if (alignment > 1) {
        size_t remainder = aligned_offset % alignment;
        if (remainder != 0) aligned_offset += alignment - remainder;
    }
    if (aligned_offset > rt->frame_arena_size || size > rt->frame_arena_size - aligned_offset)
        return NULL;

    rt->frame_arena_offset = aligned_offset + size;
    return rt->frame_arena + aligned_offset;
}

static void wasm__arena_reset(wasm_runtime_t* rt, size_t saved_offset) {
    if (saved_offset <= rt->frame_arena_size) rt->frame_arena_offset = saved_offset;
}

void wasm_config_default(wasm_config_t* config) {
    if (!config) return;

    config->max_stack_values = WASM_MAX_STACK;
    config->max_call_depth = WASM_MAX_CALL_DEPTH;
    config->max_labels = WASM_MAX_LABELS;
    config->initial_gc_heap_size = (size_t)WASM_GC_HEAP_SIZE;
    config->frame_arena_size = (size_t)WASM_FRAME_ARENA_SIZE;
}

static wasm_error_t wasm__stack_init(wasm_runtime_t* rt, uint32_t max_stack) {
    if (max_stack == 0) return WASM_ERR_MALFORMED;

    rt->stack = (wasm_value_t*)WASM_MALLOC((size_t)max_stack * sizeof(*rt->stack));
    if (!rt->stack) return WASM_ERR_OOM;

    rt->max_stack = max_stack;
    rt->sp = 0;
    return WASM_OK;
}

static void wasm__stack_destroy(wasm_runtime_t* rt) {
    WASM_FREE(rt->stack);
    rt->stack = NULL;
    rt->max_stack = 0;
    rt->sp = 0;
}

static wasm_error_t wasm__backtrace_init(wasm_runtime_t* rt, uint32_t max_depth) {
    if (max_depth == 0) return WASM_ERR_MALFORMED;

    rt->backtrace_func_indices = (uint32_t*)WASM_MALLOC((size_t)max_depth * sizeof(*rt->backtrace_func_indices));
    if (!rt->backtrace_func_indices) return WASM_ERR_OOM;

    rt->backtrace_offsets = (uint32_t*)WASM_MALLOC((size_t)max_depth * sizeof(*rt->backtrace_offsets));
    if (!rt->backtrace_offsets) {
        WASM_FREE(rt->backtrace_func_indices);
        rt->backtrace_func_indices = NULL;
        return WASM_ERR_OOM;
    }

    rt->backtrace_depth = 0;
    return WASM_OK;
}

static void wasm__backtrace_destroy(wasm_runtime_t* rt) {
    WASM_FREE(rt->backtrace_func_indices);
    WASM_FREE(rt->backtrace_offsets);
    rt->backtrace_func_indices = NULL;
    rt->backtrace_offsets = NULL;
    rt->backtrace_depth = 0;
}

static wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt);
static void wasm__call_frame_stack_destroy(wasm_runtime_t* rt);
static wasm_error_t wasm__ensure_module_frame_arena(wasm_module_t* mod);

wasm_error_t wasm_init(wasm_runtime_t* rt, const wasm_config_t* config) {
    wasm_config_t cfg;
    wasm_error_t err;

    if (!rt) return WASM_ERR_MALFORMED;

    memset(rt, 0, sizeof(*rt));

    if (config)
        cfg = *config;
    else
        wasm_config_default(&cfg);

    if (cfg.max_stack_values == 0 || cfg.max_call_depth == 0 || cfg.max_labels == 0) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "invalid runtime config");
        return WASM_ERR_MALFORMED;
    }

    rt->enabled_features = WASM__IMPLEMENTED_FEATURES;
    rt->max_call_frames = cfg.max_call_depth;
    rt->max_labels = cfg.max_labels;

    err = wasm__stack_init(rt, cfg.max_stack_values);
    if (err != WASM_OK) {
        WASM__SET_ERR(rt, err, "%s", err == WASM_ERR_OOM ? "failed to allocate value stack" : "invalid runtime config");
        return err;
    }

    if (wasm__gc_heap_init(rt, cfg.initial_gc_heap_size) != WASM_OK) {
        wasm__stack_destroy(rt);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate GC heap");
        return WASM_ERR_OOM;
    }

    if (wasm__arena_init(rt, cfg.frame_arena_size) != WASM_OK) {
        wasm__gc_heap_destroy(rt);
        wasm__stack_destroy(rt);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate frame arena");
        return WASM_ERR_OOM;
    }

    if (wasm__call_frame_stack_init(rt) != WASM_OK) {
        wasm__arena_destroy(rt);
        wasm__gc_heap_destroy(rt);
        wasm__stack_destroy(rt);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate call frame stack");
        return WASM_ERR_OOM;
    }

    if (wasm__backtrace_init(rt, cfg.max_call_depth) != WASM_OK) {
        wasm__call_frame_stack_destroy(rt);
        wasm__arena_destroy(rt);
        wasm__gc_heap_destroy(rt);
        wasm__stack_destroy(rt);
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "failed to allocate backtrace buffers");
        return WASM_ERR_OOM;
    }

    return WASM_OK;
}

void wasm_destroy(wasm_runtime_t* rt) {
    uint32_t i;

    for (i = 0; i < rt->num_imports; i++) {
        WASM_FREE((void*)rt->imports[i].module);
        WASM_FREE((void*)rt->imports[i].name);
        wasm__free_functype(&rt->imports[i].type);
    }
    for (i = 0; i < rt->num_global_imports; i++) {
        WASM_FREE((void*)rt->global_imports[i].module);
        WASM_FREE((void*)rt->global_imports[i].name);
    }
    for (i = 0; i < rt->num_table_imports; i++) {
        WASM_FREE((void*)rt->table_imports[i].module);
        WASM_FREE((void*)rt->table_imports[i].name);
    }
    for (i = 0; i < rt->num_memory_imports; i++) {
        WASM_FREE((void*)rt->memory_imports[i].module);
        WASM_FREE((void*)rt->memory_imports[i].name);
    }
    for (i = 0; i < rt->num_tag_imports; i++) {
        WASM_FREE((void*)rt->tag_imports[i].module);
        WASM_FREE((void*)rt->tag_imports[i].name);
        wasm__free_functype(&rt->tag_imports[i].type);
    }
    WASM_FREE(rt->imports);
    WASM_FREE(rt->global_imports);
    WASM_FREE(rt->table_imports);
    WASM_FREE(rt->memory_imports);
    WASM_FREE(rt->tag_imports);
    WASM_FREE(rt->validator_stack);
    WASM_FREE(rt->validator_stack_reftypes);
    WASM_FREE(rt->validator_frames);
    wasm__free_exception_state(&rt->pending_exception);
    wasm__backtrace_destroy(rt);
    wasm__call_frame_stack_destroy(rt);
    wasm__arena_destroy(rt);
    rt->gc_modules = NULL;
    wasm__gc_heap_destroy(rt);
    wasm__stack_destroy(rt);
    memset(rt, 0, sizeof(*rt));
}

wasm_error_t wasm_register_import(wasm_runtime_t* rt, const wasm_import_t* imp) {
    wasm_import_t copy;
    wasm_error_t err;

    if (!rt || !imp || !imp->module || !imp->name || !imp->func) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid function import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_imports >= rt->cap_imports) {
        uint32_t new_cap = rt->cap_imports ? rt->cap_imports * 2 : 16;
        wasm_import_t* a = (wasm_import_t*)WASM_REALLOC(rt->imports, new_cap * sizeof(wasm_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->imports = a;
        rt->cap_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.func = imp->func;
    copy.userdata = imp->userdata;
    err = wasm__copy_functype(&copy.type, &imp->type);
    if (err != WASM_OK) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return err;
    }

    rt->imports[rt->num_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_global_import(wasm_runtime_t* rt, const wasm_global_import_t* imp) {
    wasm_global_import_t copy;

    if (!rt || !imp || !imp->module || !imp->name || !imp->value) {
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
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.type = imp->type;
    copy.ref_type = imp->ref_type;
    copy.is_mutable = imp->is_mutable;
    copy.value = imp->value;
    rt->global_imports[rt->num_global_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_table_import(wasm_runtime_t* rt, const wasm_table_import_t* imp) {
    wasm_table_import_t copy;

    if (!rt || !imp || !imp->module || !imp->name || !imp->table) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid table import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_table_imports >= rt->cap_table_imports) {
        uint32_t new_cap = rt->cap_table_imports ? rt->cap_table_imports * 2 : 16;
        wasm_table_import_t* a =
            (wasm_table_import_t*)WASM_REALLOC(rt->table_imports, new_cap * sizeof(wasm_table_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->table_imports = a;
        rt->cap_table_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.table = wasm__table_actual(imp->table);
    if (!copy.table) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid table import");
        return WASM_ERR_MALFORMED;
    }

    rt->table_imports[rt->num_table_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_memory_import(wasm_runtime_t* rt, const wasm_memory_import_t* imp) {
    wasm_memory_import_t copy;

    if (!rt || !imp || !imp->module || !imp->name || !imp->memory) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid memory import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_memory_imports >= rt->cap_memory_imports) {
        uint32_t new_cap = rt->cap_memory_imports ? rt->cap_memory_imports * 2 : 16;
        wasm_memory_import_t* a =
            (wasm_memory_import_t*)WASM_REALLOC(rt->memory_imports, new_cap * sizeof(wasm_memory_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->memory_imports = a;
        rt->cap_memory_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }
    copy.memory = wasm__memory_actual(imp->memory);
    if (!copy.memory) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid memory import");
        return WASM_ERR_MALFORMED;
    }

    rt->memory_imports[rt->num_memory_imports++] = copy;
    return WASM_OK;
}

wasm_error_t wasm_register_tag_import(wasm_runtime_t* rt, const wasm_tag_import_t* imp) {
    wasm_tag_import_t copy;
    wasm_error_t err;

    if (!rt || !imp || !imp->module || !imp->name) {
        if (rt) WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "invalid tag import");
        return WASM_ERR_MALFORMED;
    }

    if (rt->num_tag_imports >= rt->cap_tag_imports) {
        uint32_t new_cap = rt->cap_tag_imports ? rt->cap_tag_imports * 2 : 16;
        wasm_tag_import_t* a =
            (wasm_tag_import_t*)WASM_REALLOC(rt->tag_imports, new_cap * sizeof(wasm_tag_import_t));
        if (!a) return WASM_ERR_OOM;
        rt->tag_imports = a;
        rt->cap_tag_imports = new_cap;
    }

    memset(&copy, 0, sizeof(copy));
    copy.module = wasm__strdup(imp->module);
    copy.name = wasm__strdup(imp->name);
    if (!copy.module || !copy.name) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return WASM_ERR_OOM;
    }

    err = wasm__copy_functype(&copy.type, &imp->type);
    if (err != WASM_OK) {
        WASM_FREE((void*)copy.module);
        WASM_FREE((void*)copy.name);
        return err;
    }

    rt->tag_imports[rt->num_tag_imports++] = copy;
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
    uint32_t entry_count = wasm__read_leb128_u32(r);
    uint32_t entry_index;

    for (entry_index = 0; entry_index < entry_count; entry_index++) {
        uint8_t opcode = wasm__read_u8(r);
        uint32_t group_index;
        uint32_t first_type;
        uint32_t group_size = 1;
        uint32_t i;
        wasm_error_t err;

        if (opcode == 0x4E) {
            err = wasm__require_feature(mod, WASM_FEATURE_GC);
            if (err != WASM_OK) return err;
            group_size = wasm__read_leb128_u32(r);
            if (group_size == 0) return WASM_ERR_MALFORMED;
        }

        err = wasm__append_rec_groups(mod, 1, &group_index);
        if (err != WASM_OK) return err;
        err = wasm__append_types(mod, group_size, &first_type);
        if (err != WASM_OK) return err;

        mod->rec_groups[group_index].first_type = first_type;
        mod->rec_groups[group_index].num_types = group_size;

        for (i = 0; i < group_size; i++) {
            wasm_comptype_t* type = &mod->types[first_type + i];
            uint8_t entry_opcode = (opcode == 0x4E) ? wasm__read_u8(r) : opcode;

            err = wasm__decode_type_entry(mod, r, entry_opcode, group_index, type);
            if (err != WASM_OK) return err;
        }
    }

    return WASM_OK;
}

static wasm_error_t wasm__decode_custom_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t section_index;
    wasm_custom_section_t* section;
    wasm_error_t err;

    err = wasm__append_custom_sections(mod, 1, &section_index);
    if (err != WASM_OK) return err;

    section = &mod->custom_sections[section_index];
    err = wasm__read_name_owned(r, &section->name);
    if (err != WASM_OK) return err;

    section->data = r->ptr;
    section->size = (uint32_t)(r->end - r->ptr);
    r->ptr = r->end;
    return WASM_OK;
}

static wasm_error_t wasm__decode_import_desc(wasm_module_t* mod,
                                            wasm__reader_t* r,
                                            wasm_import_info_t* info,
                                            uint8_t kind) {
    if (kind == 0x00) {
        uint32_t ti = wasm__read_leb128_u32(r);
        uint32_t fi;
        const wasm_functype_t* import_type = wasm__module_const_functype(mod, ti);
        uint32_t j;

        if (!import_type) return WASM_ERR_MALFORMED;
        if (wasm__append_funcs(mod, 1, &fi) != WASM_OK) return WASM_ERR_OOM;
        mod->funcs[fi].type_idx = ti;
        mod->funcs[fi].is_import = 1;
        mod->num_func_imports++;
        info->index = fi;
        info->type_index = ti;
        for (j = mod->rt->num_imports; j > 0u; j--) {
            uint32_t binding_index = j - 1u;

            if (strcmp(mod->rt->imports[binding_index].module, info->module) == 0 &&
                strcmp(mod->rt->imports[binding_index].name, info->name) == 0) {
                if (!wasm__functype_is_unspecified(&mod->rt->imports[binding_index].type) &&
                    !wasm__functype_is_subtype(mod, &mod->rt->imports[binding_index].type, import_type)) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                                  "function import type mismatch: %.64s.%.64s", info->module, info->name);
                    return WASM_ERR_TYPE_MISMATCH;
                }
                mod->funcs[fi].host_func = mod->rt->imports[binding_index].func;
                mod->funcs[fi].host_userdata = mod->rt->imports[binding_index].userdata;
                break;
            }
        }
        if (!mod->funcs[fi].host_func) {
            WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_UNKNOWN_IMPORT;
        }
    } else if (kind == 0x01) {
        uint32_t table_index;
        wasm_table_t* table;
        wasm_table_import_t* timp;
        wasm_table_t* bound_table;
        uint8_t lf;
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
        if (err != WASM_OK) return err;
        if (wasm__append_tables(mod, 1, &table_index) != WASM_OK) return WASM_ERR_OOM;
        table = &mod->tables[table_index];
        err = wasm__read_reftype(mod, r, &table->reftype, &table->reftype_info);
        if (err != WASM_OK) return err;
        lf = wasm__read_u8(r);
        table->size = wasm__read_leb128_u32(r);
        table->max_size = (lf & 1) ? wasm__read_leb128_u32(r) : UINT32_MAX;
        timp = wasm__find_table_import(mod->rt, info->module, info->name);
        if (!timp) {
            WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_UNKNOWN_IMPORT;
        }
        bound_table = wasm__table_actual(timp->table);
        if (!bound_table) return WASM_ERR_MALFORMED;
        if (!wasm__tabletype_matches_import(mod,
                                            bound_table,
                                            table->reftype,
                                            &table->reftype_info,
                                            table->size,
                                            table->max_size)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "table import type mismatch: %.64s.%.64s",
                          info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        table->is_import = 1;
        table->import_table = bound_table;
        table->size = bound_table->size;
        table->max_size = bound_table->max_size;
        table->elems = bound_table->elems;
        info->index = table_index;
        info->type = table->reftype;
        info->ref_type = table->reftype_info;
    } else if (kind == 0x02) {
        uint32_t memory_index;
        wasm_memory_t* memory;
        wasm_memory_import_t* mimp;
        wasm_memory_t* bound_memory;
        uint8_t lf = wasm__read_u8(r);

        if (wasm__append_memories(mod, 1, &memory_index) != WASM_OK) return WASM_ERR_OOM;
        if (mod->num_memories > 1) {
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_MULTI_MEMORY);
            if (err != WASM_OK) return err;
        }
        memory = &mod->memories[memory_index];
        memory->pages = wasm__read_leb128_u32(r);
        memory->max_pages = (lf & 1) ? wasm__read_leb128_u32(r) : 65536;
        mimp = wasm__find_memory_import(mod->rt, info->module, info->name);
        if (!mimp) {
            WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_UNKNOWN_IMPORT;
        }
        bound_memory = wasm__memory_actual(mimp->memory);
        if (!bound_memory) return WASM_ERR_MALFORMED;
        if (!wasm__memorytype_matches_import(bound_memory, memory->pages, memory->max_pages)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "memory import type mismatch: %.64s.%.64s",
                          info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        memory->is_import = 1;
        memory->import_memory = bound_memory;
        memory->pages = bound_memory->pages;
        memory->max_pages = bound_memory->max_pages;
        memory->data = bound_memory->data;
        info->index = memory_index;
    } else if (kind == 0x03) {
        uint32_t gi;
        wasm_global_import_t* gimp;
        wasm_valtype_t type;
        wasm_reftype_t ref_type;
        uint8_t is_mutable;
        wasm_error_t err;

        err = wasm__read_valtype(mod, r, &type, &ref_type);
        if (err != WASM_OK) return err;
        is_mutable = wasm__read_u8(r);
        if (is_mutable) {
            wasm_error_t feature_err = wasm__require_feature(mod, WASM_FEATURE_MUTABLE_GLOBALS);
            if (feature_err != WASM_OK) return feature_err;
        }

        gimp = wasm__find_global_import(mod->rt, info->module, info->name);
        if (!gimp) {
            WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_UNKNOWN_IMPORT;
        }
        if (!wasm__globaltype_matches_import(mod,
                                             gimp->type,
                                             &gimp->ref_type,
                                             gimp->is_mutable,
                                             type,
                                             &ref_type,
                                             (int)is_mutable)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "global import type mismatch: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        if (!gimp->value) return WASM_ERR_MALFORMED;
        if (!wasm__runtime_value_matches_type(mod, gimp->value, gimp->type, &gimp->ref_type)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "global import value mismatch: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        if (wasm__append_global_slots(mod, 1, &gi) != WASM_OK) return WASM_ERR_OOM;

        mod->globals[gi].type = type;
        mod->globals[gi].ref_type = ref_type;
        mod->globals[gi].is_mutable = is_mutable;
        mod->globals[gi].is_import = 1;
        mod->globals[gi].import_value = gimp->value;
        mod->num_global_imports++;
        info->index = gi;
        info->type = type;
        info->ref_type = ref_type;
        info->is_mutable = (int)is_mutable;
    } else if (kind == 0x04) {
        uint32_t tag_index;
        uint32_t type_index;
        wasm_tag_import_t* timp;
        const wasm_functype_t* import_type;
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_EXCEPTIONS);

        if (err != WASM_OK) return err;
        if (wasm__read_u8(r) != 0x00) return WASM_ERR_MALFORMED;
        type_index = wasm__read_leb128_u32(r);
        import_type = wasm__module_const_functype(mod, type_index);
        if (!import_type) return WASM_ERR_MALFORMED;
        timp = wasm__find_tag_import(mod->rt, info->module, info->name);
        if (!timp) {
            WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_UNKNOWN_IMPORT;
        }
        if (!wasm__functype_equal(&timp->type, import_type)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "tag import type mismatch: %.64s.%.64s",
                          info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        if (wasm__append_tags(mod, 1, &tag_index) != WASM_OK) return WASM_ERR_OOM;
        mod->tags[tag_index].type_idx = type_index;
        info->index = tag_index;
        info->type_index = mod->tags[tag_index].type_idx;
    } else {
        return WASM_ERR_MALFORMED;
    }

    return WASM_OK;
}

static wasm_error_t wasm__decode_import_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_leb128_u32(r);
    uint32_t i = 0;

    while (i < count) {
        char* module_name = NULL;
        char* field_name = NULL;
        wasm_error_t err;
        uint8_t kind;

        err = wasm__read_name_owned(r, &module_name);
        if (err != WASM_OK) return err;
        err = wasm__read_name_owned(r, &field_name);
        if (err != WASM_OK) {
            WASM_FREE(module_name);
            return err;
        }
        kind = wasm__read_u8(r);

        if (field_name[0] == '\0' && (kind == 0x7F || kind == 0x7E)) {
            uint32_t compact_count;
            uint8_t compact_kind = kind;
            uint32_t j;

            WASM_FREE(field_name);
            field_name = NULL;
            if (compact_kind == 0x7E) compact_kind = wasm__read_u8(r);
            compact_count = wasm__read_leb128_u32(r);
            if (compact_count == 0 || compact_count > count - i) {
                WASM_FREE(module_name);
                return WASM_ERR_MALFORMED;
            }

            for (j = 0; j < compact_count; j++, i++) {
                uint32_t import_index;
                wasm_import_info_t* info;
                uint8_t entry_kind = compact_kind;
                wasm_error_t append_err = wasm__append_import_infos(mod, 1, &import_index);

                if (append_err != WASM_OK) {
                    WASM_FREE(module_name);
                    return append_err;
                }
                info = &mod->imports[import_index];
                info->type_index = UINT32_MAX;
                info->module = wasm__strdup(module_name);
                if (!info->module) {
                    WASM_FREE(module_name);
                    return WASM_ERR_OOM;
                }
                err = wasm__read_name_owned(r, &info->name);
                if (err != WASM_OK) {
                    WASM_FREE(module_name);
                    return err;
                }
                if (kind == 0x7F) entry_kind = wasm__read_u8(r);
                info->kind = (wasm_import_kind_t)entry_kind;
                err = wasm__decode_import_desc(mod, r, info, entry_kind);
                if (err != WASM_OK) {
                    WASM_FREE(module_name);
                    return err;
                }
            }
            WASM_FREE(module_name);
            continue;
        }

        {
            uint32_t import_index;
            wasm_import_info_t* info;
            wasm_error_t append_err = wasm__append_import_infos(mod, 1, &import_index);

            if (append_err != WASM_OK) {
                WASM_FREE(module_name);
                WASM_FREE(field_name);
                return append_err;
            }

            info = &mod->imports[import_index];
            info->type_index = UINT32_MAX;
            info->module = module_name;
            info->name = field_name;
            info->kind = (wasm_import_kind_t)kind;
            err = wasm__decode_import_desc(mod, r, info, kind);
            if (err != WASM_OK) return err;
            i++;
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
        wasm_error_t err = wasm__read_reftype(mod, r, &table->reftype, &table->reftype_info);
        if (err != WASM_OK) return err;
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

        err = wasm__read_valtype(mod,
                                 r,
                                 &mod->globals[global_index].type,
                                 &mod->globals[global_index].ref_type);
        if (err != WASM_OK) return err;
        mod->globals[global_index].is_mutable = wasm__read_u8(r);
        if (mod->globals[global_index].is_mutable) {
            err = wasm__require_feature(mod, WASM_FEATURE_MUTABLE_GLOBALS);
            if (err != WASM_OK) return err;
        }
        err = wasm__eval_init_expr(mod, r, global_index, &value);
        if (err != WASM_OK) return err;
        if (!wasm__runtime_value_matches_type(mod,
                                              &value,
                                              mod->globals[global_index].type,
                                              &mod->globals[global_index].ref_type))
            return WASM_ERR_TYPE_MISMATCH;
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
        segment->elem_ref_type.type = WASM_TYPE_FUNCREF;
        segment->elem_ref_type.nullable = 1;
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

            /* Flag 4 uses expression elements with an implicit funcref type. */
            if (flags != 0x04) {
                err = wasm__read_reftype(mod, r, &segment->elem_type, &segment->elem_ref_type);
                if (err != WASM_OK) return err;
            }
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
                if (!wasm__runtime_value_matches_type(mod,
                                                      &value,
                                                      segment->elem_type,
                                                      &segment->elem_ref_type))
                    return WASM_ERR_TYPE_MISMATCH;
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
        ft = wasm__module_functype(mod, mod->funcs[fidx].type_idx);
        if (!ft) return WASM_ERR_MALFORMED;
        ndecls = wasm__read_leb128_u32(r);
        ls = r->ptr;
        for (d = 0; d < ndecls; d++) {
            wasm_valtype_t local_type;
            wasm_reftype_t local_ref_type;

            total_locals += wasm__read_leb128_u32(r);
            if (wasm__read_valtype(mod, r, &local_type, &local_ref_type) != WASM_OK)
                return WASM_ERR_MALFORMED;
        }
        all_locals = ft->num_params + total_locals;
        mod->funcs[fidx].num_locals = all_locals;
        mod->funcs[fidx].locals = (wasm_valtype_t*)WASM_MALLOC(all_locals * sizeof(wasm_valtype_t));
        mod->funcs[fidx].local_reftypes = (wasm_reftype_t*)WASM_CALLOC(all_locals, sizeof(wasm_reftype_t));
        if (!mod->funcs[fidx].locals || !mod->funcs[fidx].local_reftypes) return WASM_ERR_OOM;
        for (p = 0; p < ft->num_params; p++) {
            mod->funcs[fidx].locals[p] = ft->params[p];
            if (ft->param_reftypes) mod->funcs[fidx].local_reftypes[p] = ft->param_reftypes[p];
        }
        r->ptr = ls;
        local_off = ft->num_params;
        for (d = 0; d < ndecls; d++) {
            uint32_t n = wasm__read_leb128_u32(r);
            wasm_valtype_t t;
            wasm_reftype_t ref_type;
            wasm_error_t local_err = wasm__read_valtype(mod, r, &t, &ref_type);
            if (local_err != WASM_OK) return local_err;
            for (j = 0; j < n; j++) {
                mod->funcs[fidx].locals[local_off] = t;
                mod->funcs[fidx].local_reftypes[local_off] = ref_type;
                local_off++;
            }
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

static wasm_error_t wasm__validate_types(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_rec_groups; i++) {
        wasm_recgroup_t* group = &mod->rec_groups[i];

        if (group->first_type > mod->num_types ||
            group->num_types > mod->num_types - group->first_type) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "rec group %u is out of range",
                                              (unsigned)i);
        }
    }

    for (i = 0; i < mod->num_types; i++) {
        wasm_comptype_t* type = &mod->types[i];
        uint32_t j;

        if (type->rec_group >= mod->num_rec_groups) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "type %u refers to missing rec group %u",
                                              (unsigned)i,
                                              (unsigned)type->rec_group);
        }

        {
            wasm_recgroup_t* group = &mod->rec_groups[type->rec_group];

            if (i < group->first_type || i >= group->first_type + group->num_types) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u is outside rec group %u bounds",
                                                  (unsigned)i,
                                                  (unsigned)type->rec_group);
            }
        }

        for (j = 0; j < type->num_supertypes; j++) {
            uint32_t super_idx = type->supertypes[j];
            wasm_comptype_t* supertype;

            if (super_idx >= mod->num_types) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u supertype %u out of range",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }
            if (super_idx >= i) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u supertype %u must have a lower index",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }

            supertype = &mod->types[super_idx];
            if (supertype->is_final) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u cannot extend final type %u",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }
            if (type->kind != supertype->kind) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u kind does not match supertype %u",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }
            if (!wasm__is_comptype_immediate_subtype(mod, type, supertype)) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u is not a valid subtype of %u",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }
        }
    }

    return WASM_OK;
}

static wasm_error_t wasm__resolve_functype_reftypes(wasm_module_t* mod, wasm_functype_t* ft) {
    uint32_t i;

    for (i = 0; i < ft->num_params; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(
            mod, &ft->params[i], ft->param_reftypes ? &ft->param_reftypes[i] : NULL);
        if (err != WASM_OK) return err;
    }
    for (i = 0; i < ft->num_results; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(
            mod, &ft->results[i], ft->result_reftypes ? &ft->result_reftypes[i] : NULL);
        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

static wasm_error_t wasm__resolve_storagetype_reftype(wasm_module_t* mod, wasm_storagetype_t* storage) {
    if (storage->kind != WASM_STORAGE_VALTYPE) return WASM_OK;
    return wasm__resolve_valtype_reftype(mod, &storage->of.valtype, &storage->ref_type);
}

static wasm_error_t wasm__resolve_module_reftypes(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_types; i++) {
        wasm_comptype_t* type = &mod->types[i];
        wasm_error_t err;
        uint32_t j;

        switch (type->kind) {
            case WASM_COMP_FUNC:
                err = wasm__resolve_functype_reftypes(mod, &type->of.func);
                if (err != WASM_OK) return err;
                break;
            case WASM_COMP_STRUCT:
                for (j = 0; j < type->of.struct_.num_fields; j++) {
                    err = wasm__resolve_storagetype_reftype(mod, &type->of.struct_.fields[j].storage);
                    if (err != WASM_OK) return err;
                }
                break;
            case WASM_COMP_ARRAY:
                err = wasm__resolve_storagetype_reftype(mod, &type->of.array.field.storage);
                if (err != WASM_OK) return err;
                break;
            default:
                return WASM_ERR_MALFORMED;
        }
    }

    for (i = 0; i < mod->num_tables; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(mod, &mod->tables[i].reftype, &mod->tables[i].reftype_info);
        if (err != WASM_OK) return err;
    }

    for (i = 0; i < mod->num_globals; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(mod, &mod->globals[i].type, &mod->globals[i].ref_type);
        if (err != WASM_OK) return err;
    }

    for (i = 0; i < mod->num_elem_segments; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(
            mod, &mod->elem_segments[i].elem_type, &mod->elem_segments[i].elem_ref_type);
        if (err != WASM_OK) return err;
    }

    for (i = 0; i < mod->num_funcs; i++) {
        wasm_func_t* func = &mod->funcs[i];
        uint32_t j;

        for (j = 0; j < func->num_locals; j++) {
            wasm_error_t err = wasm__resolve_valtype_reftype(
                mod, &func->locals[j], func->local_reftypes ? &func->local_reftypes[j] : NULL);
            if (err != WASM_OK) return err;
        }
    }

    return WASM_OK;
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

        tag_type = wasm__module_functype(mod, mod->tags[i].type_idx);
        if (!tag_type) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "tag %u type must reference a function type",
                                              (unsigned)i);
        }
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
        start_type = wasm__module_functype(mod, mod->funcs[mod->start_func].type_idx);
        if (!start_type) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "start function must reference a function type");
        }
        if (start_type->num_params != 0 || start_type->num_results != 0) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "start function must have signature [] -> []");
        }
    }

    for (i = 0; i < mod->num_elem_segments; i++) {
        wasm_elem_segment_t* segment = &mod->elem_segments[i];

        if (!wasm__is_ref_type(segment->elem_type)) {
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
            table = wasm__table_at(mod, segment->table_index);
            if (!table) return WASM_ERR_MALFORMED;
            if (!wasm__typed_valtype_is_subtype(mod,
                                                segment->elem_type,
                                                &segment->elem_ref_type,
                                                table->reftype,
                                                &table->reftype_info)) {
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
typedef struct wasm__blocktype_t {
    uint32_t param_arity;
    uint32_t result_arity;
    const wasm_valtype_t* params;
    const wasm_valtype_t* results;
    const wasm_reftype_t* param_reftypes;
    const wasm_reftype_t* result_reftypes;
    wasm_valtype_t inline_result;
    wasm_reftype_t inline_result_reftype;
} wasm__blocktype_t;
static wasm_error_t wasm__read_blocktype(wasm_module_t* mod, wasm__reader_t* r, wasm__blocktype_t* blocktype);

#define WASM__TYPE_BOT ((wasm_valtype_t)0xFF)

struct wasm__val_frame_t {
    uint8_t opcode;
    uint32_t height;
    const wasm_valtype_t* param_types;
    const wasm_reftype_t* param_reftypes;
    uint32_t num_params;
    const wasm_valtype_t* result_types;
    const wasm_reftype_t* result_reftypes;
    uint32_t num_results;
    wasm_valtype_t inline_result;
    wasm_reftype_t inline_result_reftype;
    int unreachable;
    int seen_else;
    int in_catch;
    int seen_catch_all;
    int seen_delegate;
};

typedef struct wasm__validator_t {
    wasm_module_t* mod;
    uint32_t func_idx;
    const uint8_t* body_start;
    wasm__reader_t r;
    wasm_valtype_t* stack;
    wasm_reftype_t* stack_reftypes;
    uint32_t stack_capacity;
    uint32_t sp;
    wasm__val_frame_t* frames;
    uint32_t frame_capacity;
    uint32_t fp;
    uint32_t max_fp;
} wasm__validator_t;

static void wasm__validator_destroy(wasm__validator_t* v) {
    if (!v) return;
    v->stack = NULL;
    v->stack_reftypes = NULL;
    v->frames = NULL;
    v->stack_capacity = 0;
    v->frame_capacity = 0;
}

static wasm_error_t wasm__validator_init(wasm__validator_t* v, wasm_module_t* mod, uint32_t func_idx) {
    wasm_runtime_t* rt;
    uint32_t stack_capacity;
    uint32_t frame_capacity;

    if (!v || !mod || !mod->rt) return WASM_ERR_MALFORMED;

    memset(v, 0, sizeof(*v));
    v->mod = mod;
    v->func_idx = func_idx;
    rt = mod->rt;
    stack_capacity = rt->max_stack ? rt->max_stack : 1u;
    frame_capacity = rt->max_labels ? rt->max_labels : 1u;

    if (!rt->validator_stack) {
        rt->validator_stack = (wasm_valtype_t*)WASM_MALLOC((size_t)stack_capacity * sizeof(*rt->validator_stack));
        if (!rt->validator_stack) return WASM_ERR_OOM;
    }
    if (!rt->validator_stack_reftypes) {
        rt->validator_stack_reftypes =
            (wasm_reftype_t*)WASM_MALLOC((size_t)stack_capacity * sizeof(*rt->validator_stack_reftypes));
        if (!rt->validator_stack_reftypes) return WASM_ERR_OOM;
    }
    if (!rt->validator_frames) {
        rt->validator_frames = (wasm__val_frame_t*)WASM_CALLOC(frame_capacity, sizeof(*rt->validator_frames));
        if (!rt->validator_frames) return WASM_ERR_OOM;
    } else {
        memset(rt->validator_frames, 0, (size_t)frame_capacity * sizeof(*rt->validator_frames));
    }

    v->stack = rt->validator_stack;
    v->stack_reftypes = rt->validator_stack_reftypes;
    v->frames = rt->validator_frames;
    v->stack_capacity = stack_capacity;
    v->frame_capacity = frame_capacity;
    return WASM_OK;
}

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

static int wasm__validator_get_effective_reftype(wasm_valtype_t type,
                                                 const wasm_reftype_t* ref_type,
                                                 wasm_reftype_t* out) {
    return wasm__get_effective_reftype(NULL, type, ref_type, out);
}

static int wasm__validator_type_matches(const wasm__validator_t* v,
                                        wasm_valtype_t actual,
                                        const wasm_reftype_t* actual_ref,
                                        wasm_valtype_t expected,
                                        const wasm_reftype_t* expected_ref) {
    wasm_reftype_t actual_effective;
    wasm_reftype_t expected_effective;

    if (actual == WASM__TYPE_BOT || expected == WASM__TYPE_BOT) return 1;
    if (!wasm__is_valtype_subtype(v->mod, actual, expected)) return 0;
    if (!wasm__is_ref_type(actual) || !wasm__is_ref_type(expected)) return 1;
    if (!wasm__validator_get_effective_reftype(actual, actual_ref, &actual_effective) ||
        !wasm__validator_get_effective_reftype(expected, expected_ref, &expected_effective)) {
        return 1;
    }
    return wasm__reftype_is_subtype(v->mod, &actual_effective, &expected_effective);
}

static wasm__val_frame_t* wasm__validator_frame(wasm__validator_t* v) {
    return &v->frames[v->fp - 1];
}

static wasm_error_t wasm__validator_push_typed(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t type,
                                               const wasm_reftype_t* ref_type);
static wasm_error_t wasm__validator_push_many_typed(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    const wasm_valtype_t* types,
                                                    const wasm_reftype_t* reftypes,
                                                    uint32_t count);
static wasm_error_t wasm__validator_pop_any_typed(wasm__validator_t* v,
                                                  const uint8_t* at,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_ref_type);
static wasm_error_t wasm__validator_pop_expect_typed(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     wasm_valtype_t expected,
                                                     const wasm_reftype_t* expected_ref);
static wasm_error_t wasm__validator_check_types_typed(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      const wasm_valtype_t* types,
                                                      const wasm_reftype_t* reftypes,
                                                      uint32_t count);

static wasm_error_t wasm__validator_push(wasm__validator_t* v,
                                         const uint8_t* at,
                                         wasm_valtype_t type) {
    return wasm__validator_push_typed(v, at, type, NULL);
}

static wasm_error_t wasm__validator_push_typed(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t type,
                                               const wasm_reftype_t* ref_type) {
    if (v->sp >= v->stack_capacity)
        return wasm__validator_error(v, at, "operand stack overflow");
    v->stack[v->sp++] = type;
    if (ref_type && wasm__has_reftype_info(ref_type))
        v->stack_reftypes[v->sp - 1] = *ref_type;
    else
        wasm__clear_reftype(&v->stack_reftypes[v->sp - 1]);
    return WASM_OK;
}

static wasm_error_t wasm__validator_push_many_typed(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    const wasm_valtype_t* types,
                                                    const wasm_reftype_t* reftypes,
                                                    uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        const wasm_reftype_t* ref_type = reftypes ? &reftypes[i] : NULL;
        wasm_error_t err = wasm__validator_push_typed(v, at, types[i], ref_type);
        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

static wasm_error_t wasm__validator_pop_any(wasm__validator_t* v,
                                            const uint8_t* at,
                                            wasm_valtype_t* out_type) {
    return wasm__validator_pop_any_typed(v, at, out_type, NULL);
}

static wasm_error_t wasm__validator_pop_any_typed(wasm__validator_t* v,
                                                  const uint8_t* at,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_ref_type) {
    wasm__val_frame_t* frame = wasm__validator_frame(v);

    if (v->sp == frame->height) {
        if (!frame->unreachable)
            return wasm__validator_error(v, at, "stack underflow");
        *out_type = WASM__TYPE_BOT;
        wasm__clear_reftype(out_ref_type);
        return WASM_OK;
    }

    *out_type = v->stack[--v->sp];
    if (out_ref_type) *out_ref_type = v->stack_reftypes[v->sp];
    return WASM_OK;
}

static wasm_error_t wasm__validator_pop_expect(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t expected) {
    return wasm__validator_pop_expect_typed(v, at, expected, NULL);
}

static wasm_error_t wasm__validator_pop_expect_typed(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     wasm_valtype_t expected,
                                                     const wasm_reftype_t* expected_ref) {
    wasm_valtype_t actual;
    wasm_reftype_t actual_ref;
    wasm_error_t err = wasm__validator_pop_any_typed(v, at, &actual, &actual_ref);
    if (err != WASM_OK) return err;
    if (!wasm__validator_type_matches(v, actual, &actual_ref, expected, expected_ref)) {
        return wasm__validator_error(v, at,
                                     "type mismatch: expected 0x%X, got 0x%X",
                                     (unsigned)expected,
                                     (unsigned)actual);
    }
    return WASM_OK;
}

static wasm_error_t wasm__validator_check_types_typed(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      const wasm_valtype_t* types,
                                                      const wasm_reftype_t* reftypes,
                                                      uint32_t count) {
    wasm__val_frame_t* frame = wasm__validator_frame(v);
    uint32_t sp = v->sp;
    uint32_t i;

    for (i = 0; i < count; i++) {
        wasm_valtype_t actual;
        wasm_reftype_t actual_ref;
        wasm_valtype_t expected = types[count - 1 - i];
        const wasm_reftype_t* expected_ref = reftypes ? &reftypes[count - 1 - i] : NULL;

        if (sp == frame->height) {
            if (!frame->unreachable)
                return wasm__validator_error(v, at, "stack underflow");
            actual = WASM__TYPE_BOT;
            wasm__clear_reftype(&actual_ref);
        } else {
            actual = v->stack[--sp];
            actual_ref = v->stack_reftypes[sp];
        }

        if (!wasm__validator_type_matches(v, actual, &actual_ref, expected, expected_ref)) {
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
                                      const wasm_reftype_t* a_refs,
                                      uint32_t a_count,
                                      const wasm_valtype_t* b,
                                      const wasm_reftype_t* b_refs,
                                      uint32_t b_count) {
    wasm_reftype_t a_effective;
    wasm_reftype_t b_effective;
    uint32_t i;
    if (a_count != b_count) return 0;
    for (i = 0; i < a_count; i++) {
        if (a[i] != b[i]) return 0;
        if (wasm__is_ref_type(a[i]) || wasm__is_ref_type(b[i])) {
            wasm__validator_get_effective_reftype(a[i], a_refs ? &a_refs[i] : NULL, &a_effective);
            wasm__validator_get_effective_reftype(b[i], b_refs ? &b_refs[i] : NULL, &b_effective);
            if (!wasm__reftype_equal(&a_effective, &b_effective)) return 0;
        }
    }
    return 1;
}

static void wasm__validator_branch_signature(const wasm__val_frame_t* frame,
                                             const wasm_valtype_t** out_types,
                                             const wasm_reftype_t** out_reftypes,
                                             uint32_t* out_count) {
    if (frame->opcode == 0x03) {
        *out_types = frame->param_types;
        *out_reftypes = frame->param_reftypes;
        *out_count = frame->num_params;
    } else {
        *out_types = frame->result_types;
        *out_reftypes = frame->result_reftypes;
        *out_count = frame->num_results;
    }
}

static wasm_error_t wasm__validator_check_tail_results(wasm__validator_t* v,
                                                       const uint8_t* at,
                                                       const wasm_functype_t* caller_type,
                                                       const wasm_functype_t* callee_type) {
    if (!wasm__validator_same_types(caller_type->results, caller_type->result_reftypes,
                                    caller_type->num_results,
                                    callee_type->results, callee_type->result_reftypes,
                                    callee_type->num_results)) {
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
        err = wasm__validator_check_types_typed(v, at,
                                                frame->result_types,
                                                frame->result_reftypes,
                                                frame->num_results);
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
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype) {
    wasm_error_t err = wasm__read_typed_select_immediate(v->mod, &v->r, out_type, out_reftype);

    if (err != WASM_OK)
        return wasm__validator_error(v, at, "typed select requires exactly one value type");
    return WASM_OK;
}

static wasm_valtype_t wasm__field_stack_type(const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) return WASM_TYPE_I32;
    return field->storage.of.valtype;
}

static const wasm_reftype_t* wasm__field_stack_reftype(const wasm_fieldtype_t* field) {
    if (field->storage.kind != WASM_STORAGE_VALTYPE) return NULL;
    if (!wasm__is_ref_type(field->storage.of.valtype)) return NULL;
    return &field->storage.ref_type;
}

static int wasm__field_is_reference_type(const wasm_fieldtype_t* field) {
    return field->storage.kind == WASM_STORAGE_VALTYPE &&
           wasm__is_ref_type(field->storage.of.valtype);
}

static wasm_error_t wasm__validator_make_typeidx_reftype(wasm__validator_t* v,
                                                         const uint8_t* at,
                                                         uint32_t type_idx,
                                                         int nullable,
                                                         wasm_valtype_t* out_type,
                                                         wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    if (type_idx >= v->mod->num_types)
        return wasm__validator_error(v, at, "type index %u out of range", (unsigned)type_idx);

    wasm__clear_reftype(out_reftype);
    out_reftype->has_type_index = 1;
    out_reftype->type_index = type_idx;
    out_reftype->nullable = nullable;
    err = wasm__resolve_reftype(v->mod, out_reftype);
    if (err != WASM_OK)
        return wasm__validator_error(v, at, "type index %u does not name a reference type",
                                     (unsigned)type_idx);
    if (out_type) *out_type = out_reftype->type;
    return WASM_OK;
}

static wasm_error_t wasm__validator_read_heap_reftype(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      int nullable,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    wasm__clear_reftype(out_reftype);
    out_reftype->nullable = nullable;
    err = wasm__read_heaptype(v->mod, &v->r, out_reftype);
    if (err != WASM_OK) return wasm__validator_error(v, at, "invalid heaptype");
    if (out_type) *out_type = out_reftype->type ? out_reftype->type : WASM_TYPE_ANYREF;
    return WASM_OK;
}

static wasm_error_t wasm__validator_read_cast_flag_reftype(wasm__validator_t* v,
                                                           const uint8_t* at,
                                                           uint8_t flags,
                                                           uint8_t bit,
                                                           wasm_valtype_t* out_type,
                                                           wasm_reftype_t* out_reftype) {
    return wasm__validator_read_heap_reftype(v, at, (flags & (uint8_t)(1u << bit)) != 0,
                                             out_type, out_reftype);
}

static wasm_error_t wasm__validator_pop_ref_supertype(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      wasm_valtype_t expected_type,
                                                      const wasm_reftype_t* expected_ref,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_ref) {
    wasm_reftype_t actual_effective;
    wasm_error_t err = wasm__validator_pop_any_typed(v, at, out_type, out_ref);

    if (err != WASM_OK) return err;
    if (*out_type == WASM__TYPE_BOT) return WASM_OK;
    if (!wasm__is_ref_type(*out_type))
        return wasm__validator_error(v, at, "instruction requires a reference operand");
    if (!wasm__validator_get_effective_reftype(*out_type, out_ref, &actual_effective) ||
        !wasm__reftype_is_subtype(v->mod, expected_ref, &actual_effective) ||
        !wasm__is_valtype_subtype(v->mod, expected_type, *out_type)) {
        return wasm__validator_error(v, at,
                                     "type mismatch: expected 0x%X, got 0x%X",
                                     (unsigned)expected_type,
                                     (unsigned)*out_type);
    }
    return WASM_OK;
}

static void wasm__validator_reference_difference(const wasm_reftype_t* source,
                                                 const wasm_reftype_t* target,
                                                 wasm_valtype_t* out_type,
                                                 wasm_reftype_t* out_ref) {
    wasm_reftype_t diff = *source;

    if (source->nullable && !target->nullable &&
        source->has_type_index == target->has_type_index &&
        source->type_index == target->type_index &&
        source->type == target->type) {
        diff.has_type_index = 0;
        diff.nullable = 1;
        switch (source->type) {
            case WASM_TYPE_FUNCREF:
                diff.type = WASM_TYPE_NOFUNC;
                break;
            case WASM_TYPE_EXTERNREF:
                diff.type = WASM_TYPE_NOEXTERN;
                break;
            default:
                diff.type = WASM_TYPE_NONE;
                break;
        }
    }

    if (out_type) *out_type = diff.type;
    if (out_ref) *out_ref = diff;
}

static wasm_error_t wasm__validator_validate_gc(wasm__validator_t* v, const uint8_t* at) {
    uint32_t subop = wasm__read_leb128_u32(&v->r);
    wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_GC);

    if (err != WASM_OK) return err;

    switch (subop) {
        case 0x00:
        case 0x01: {
            uint32_t type_idx = wasm__read_leb128_u32(&v->r);
            const wasm_structtype_t* st = wasm__module_const_structtype(v->mod, type_idx);
            wasm_valtype_t result_type;
            wasm_reftype_t result_ref;
            uint32_t i;

            if (!st)
                return wasm__validator_error(v, at, "struct instruction type index %u is not a struct type",
                                             (unsigned)type_idx);
            if (subop == 0x00) {
                for (i = st->num_fields; i > 0; i--) {
                    err = wasm__validator_pop_expect_typed(v, at,
                                                           wasm__field_stack_type(&st->fields[i - 1]),
                                                           wasm__field_stack_reftype(&st->fields[i - 1]));
                    if (err != WASM_OK) return err;
                }
            }
            err = wasm__validator_make_typeidx_reftype(v, at, type_idx, 0, &result_type, &result_ref);
            if (err != WASM_OK) return err;
            return wasm__validator_push_typed(v, at, result_type, &result_ref);
        }
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05: {
            uint32_t type_idx = wasm__read_leb128_u32(&v->r);
            uint32_t field_idx = wasm__read_leb128_u32(&v->r);
            const wasm_structtype_t* st = wasm__module_const_structtype(v->mod, type_idx);
            const wasm_fieldtype_t* field;
            wasm_valtype_t struct_ref_type;
            wasm_reftype_t struct_ref;

            if (!st)
                return wasm__validator_error(v, at, "struct instruction type index %u is not a struct type",
                                             (unsigned)type_idx);
            if (field_idx >= st->num_fields)
                return wasm__validator_error(v, at, "struct field index %u out of range", (unsigned)field_idx);
            err = wasm__validator_make_typeidx_reftype(v, at, type_idx, 1, &struct_ref_type, &struct_ref);
            if (err != WASM_OK) return err;

            field = &st->fields[field_idx];
            if (subop == 0x03 || subop == 0x04) {
                if (field->storage.kind != WASM_STORAGE_PACKED)
                    return wasm__validator_error(v, at, "struct.get_s/u requires a packed field");
            } else if (subop == 0x02 && field->storage.kind == WASM_STORAGE_PACKED) {
                return wasm__validator_error(v, at, "struct.get requires an unpacked field");
            }

            if (subop == 0x05) {
                if (!field->is_mutable)
                    return wasm__validator_error(v, at, "struct.set requires a mutable field");
                err = wasm__validator_pop_expect_typed(v, at,
                                                       wasm__field_stack_type(field),
                                                       wasm__field_stack_reftype(field));
                if (err != WASM_OK) return err;
                return wasm__validator_pop_expect_typed(v, at, struct_ref_type, &struct_ref);
            }

            err = wasm__validator_pop_expect_typed(v, at, struct_ref_type, &struct_ref);
            if (err != WASM_OK) return err;
            return wasm__validator_push_typed(v, at,
                                              wasm__field_stack_type(field),
                                              wasm__field_stack_reftype(field));
        }
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13: {
            uint32_t type_idx = wasm__read_leb128_u32(&v->r);
            const wasm_arraytype_t* atype = wasm__module_const_arraytype(v->mod, type_idx);
            wasm_valtype_t elem_type;
            const wasm_reftype_t* elem_ref_type;
            wasm_valtype_t array_ref_type;
            wasm_reftype_t array_ref;

            if (!atype)
                return wasm__validator_error(v, at, "array instruction type index %u is not an array type",
                                             (unsigned)type_idx);
            err = wasm__validator_make_typeidx_reftype(v, at, type_idx, 1, &array_ref_type, &array_ref);
            if (err != WASM_OK) return err;

            elem_type = wasm__field_stack_type(&atype->field);
            elem_ref_type = wasm__field_stack_reftype(&atype->field);
            switch (subop) {
                case 0x06:
                    err = wasm__validator_pop_expect_typed(v, at, elem_type, elem_ref_type);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                case 0x07:
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                case 0x08: {
                    uint32_t count = wasm__read_leb128_u32(&v->r);
                    uint32_t i;

                    for (i = count; i > 0; i--) {
                        err = wasm__validator_pop_expect_typed(v, at, elem_type, elem_ref_type);
                        if (err != WASM_OK) return err;
                    }
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x09: {
                    uint32_t data_idx = wasm__read_leb128_u32(&v->r);

                    v->mod->uses_data_count_section = 1;
                    if (data_idx >= v->mod->num_data_segments)
                        return wasm__validator_error(v, at, "data segment %u out of range", (unsigned)data_idx);
                    if (wasm__field_is_reference_type(&atype->field))
                        return wasm__validator_error(v, at, "array.new_data requires a non-reference element type");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x0A: {
                    uint32_t elem_idx = wasm__read_leb128_u32(&v->r);

                    if (elem_idx >= v->mod->num_elem_segments)
                        return wasm__validator_error(v, at, "element segment %u out of range", (unsigned)elem_idx);
                    if (!wasm__field_is_reference_type(&atype->field))
                        return wasm__validator_error(v, at, "array.new_elem requires a reference element type");
                    if (!wasm__validator_type_matches(v,
                                                      v->mod->elem_segments[elem_idx].elem_type,
                                                      &v->mod->elem_segments[elem_idx].elem_ref_type,
                                                      atype->field.storage.of.valtype,
                                                      &atype->field.storage.ref_type)) {
                        return wasm__validator_error(v, at, "array.new_elem type mismatch");
                    }
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x0B:
                case 0x0C:
                case 0x0D:
                    if (subop != 0x0B && atype->field.storage.kind != WASM_STORAGE_PACKED)
                        return wasm__validator_error(v, at, "array.get_s/u requires a packed element type");
                    if (subop == 0x0B && atype->field.storage.kind == WASM_STORAGE_PACKED)
                        return wasm__validator_error(v, at, "array.get requires an unpacked element type");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                    if (err != WASM_OK) return err;
                    return wasm__validator_push_typed(v, at, elem_type, elem_ref_type);
                case 0x0E:
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.set requires a mutable element type");
                    err = wasm__validator_pop_expect_typed(v, at, elem_type, elem_ref_type);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                case 0x10:
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.fill requires a mutable element type");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect_typed(v, at, elem_type, elem_ref_type);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                case 0x11: {
                    uint32_t src_type_idx = wasm__read_leb128_u32(&v->r);
                    const wasm_arraytype_t* src_type = wasm__module_const_arraytype(v->mod, src_type_idx);
                    wasm_valtype_t src_array_ref_type;
                    wasm_reftype_t src_array_ref;

                    if (!src_type)
                        return wasm__validator_error(v, at, "array.copy source type index %u is not an array type",
                                                     (unsigned)src_type_idx);
                    err = wasm__validator_make_typeidx_reftype(v, at, src_type_idx, 1,
                                                               &src_array_ref_type, &src_array_ref);
                    if (err != WASM_OK) return err;
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.copy destination must be mutable");
                    if (!wasm__is_fieldtype_subtype(v->mod, &src_type->field, &atype->field))
                        return wasm__validator_error(v, at, "array.copy type mismatch");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect_typed(v, at, src_array_ref_type, &src_array_ref);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x12: {
                    uint32_t data_idx = wasm__read_leb128_u32(&v->r);

                    v->mod->uses_data_count_section = 1;
                    if (data_idx >= v->mod->num_data_segments)
                        return wasm__validator_error(v, at, "data segment %u out of range", (unsigned)data_idx);
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.init_data requires a mutable element type");
                    if (wasm__field_is_reference_type(&atype->field))
                        return wasm__validator_error(v, at, "array.init_data requires a non-reference element type");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x13: {
                    uint32_t elem_idx = wasm__read_leb128_u32(&v->r);

                    if (elem_idx >= v->mod->num_elem_segments)
                        return wasm__validator_error(v, at, "element segment %u out of range", (unsigned)elem_idx);
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.init_elem requires a mutable element type");
                    if (!wasm__field_is_reference_type(&atype->field))
                        return wasm__validator_error(v, at, "array.init_elem requires a reference element type");
                    if (!wasm__validator_type_matches(v,
                                                      v->mod->elem_segments[elem_idx].elem_type,
                                                      &v->mod->elem_segments[elem_idx].elem_ref_type,
                                                      atype->field.storage.of.valtype,
                                                      &atype->field.storage.ref_type)) {
                        return wasm__validator_error(v, at, "array.init_elem type mismatch");
                    }
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                }
                default:
                    return wasm__validator_error(v, at, "unsupported array GC opcode 0xFB 0x%X", (unsigned)subop);
            }
        }
        case 0x0F: {
            wasm_reftype_t array_ref;
            wasm__clear_reftype(&array_ref);
            array_ref.type = WASM_TYPE_ARRAYREF;
            array_ref.nullable = 1;
            err = wasm__validator_pop_expect_typed(v, at, WASM_TYPE_ARRAYREF, &array_ref);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        }
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17: {
            wasm_valtype_t target_type;
            wasm_reftype_t target_ref;
            wasm_valtype_t actual_type;
            wasm_reftype_t actual_ref;

            err = wasm__validator_read_heap_reftype(v, at, (subop == 0x15 || subop == 0x17),
                                                    &target_type, &target_ref);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_ref_supertype(v, at,
                                                    target_type, &target_ref,
                                                    &actual_type, &actual_ref);
            if (err != WASM_OK) return err;
            if (subop == 0x14 || subop == 0x15)
                return wasm__validator_push(v, at, WASM_TYPE_I32);
            return wasm__validator_push_typed(v, at, target_type, &target_ref);
        }
        case 0x18:
        case 0x19: {
            uint8_t flags = wasm__read_u8(&v->r);
            uint32_t depth = wasm__read_leb128_u32(&v->r);
            const wasm__val_frame_t* target_frame;
            const wasm_valtype_t* branch_types;
            const wasm_reftype_t* branch_reftypes;
            uint32_t branch_count;
            wasm_valtype_t source_type;
            wasm_reftype_t source_ref;
            wasm_valtype_t target_type;
            wasm_reftype_t target_ref;
            wasm_valtype_t diff_type;
            wasm_reftype_t diff_ref;
            wasm_valtype_t actual_type;
            wasm_reftype_t actual_ref;

            if ((flags & ~0x03u) != 0)
                return wasm__validator_error(v, at, "invalid br_on_cast flags 0x%X", (unsigned)flags);
            if (depth >= v->fp)
                return wasm__validator_error(v, at, "branch depth %u out of range", (unsigned)depth);
            err = wasm__validator_read_cast_flag_reftype(v, at, flags, 0, &source_type, &source_ref);
            if (err != WASM_OK) return err;
            err = wasm__validator_read_cast_flag_reftype(v, at, flags, 1, &target_type, &target_ref);
            if (err != WASM_OK) return err;
            if (!wasm__reftype_is_subtype(v->mod, &target_ref, &source_ref))
                return wasm__validator_error(v, at, "br_on_cast target type must be a subtype of source type");
            err = wasm__validator_pop_ref_supertype(v, at,
                                                    source_type, &source_ref,
                                                    &actual_type, &actual_ref);
            if (err != WASM_OK) return err;

            target_frame = &v->frames[v->fp - 1 - depth];
            wasm__validator_branch_signature(target_frame, &branch_types, &branch_reftypes, &branch_count);
            if (branch_count == 0)
                return wasm__validator_error(v, at, "br_on_cast target must accept the cast operand");
            err = wasm__validator_check_types_typed(v, at,
                                                    branch_types,
                                                    branch_reftypes,
                                                    branch_count - 1);
            if (err != WASM_OK) return err;

            wasm__validator_reference_difference(&source_ref, &target_ref, &diff_type, &diff_ref);
            if (subop == 0x18) {
                if (!wasm__validator_type_matches(v,
                                                  target_type, &target_ref,
                                                  branch_types[branch_count - 1],
                                                  branch_reftypes ? &branch_reftypes[branch_count - 1] : NULL)) {
                    return wasm__validator_error(v, at, "br_on_cast target label type mismatch");
                }
                return wasm__validator_push_typed(v, at, diff_type, &diff_ref);
            }

            if (!wasm__validator_type_matches(v,
                                              diff_type, &diff_ref,
                                              branch_types[branch_count - 1],
                                              branch_reftypes ? &branch_reftypes[branch_count - 1] : NULL)) {
                return wasm__validator_error(v, at, "br_on_cast_fail target label type mismatch");
            }
            return wasm__validator_push_typed(v, at, target_type, &target_ref);
        }
        case 0x1A: {
            wasm_valtype_t actual_type;
            wasm_reftype_t actual_ref;
            wasm_reftype_t expected_ref;
            wasm_reftype_t pushed_ref;

            expected_ref.type = WASM_TYPE_EXTERNREF;
            expected_ref.nullable = 1;
            err = wasm__validator_pop_ref_supertype(v, at,
                                                    WASM_TYPE_EXTERNREF, &expected_ref,
                                                    &actual_type, &actual_ref);
            if (err != WASM_OK) return err;
            wasm__clear_reftype(&pushed_ref);
            if (actual_type != WASM__TYPE_BOT) {
                wasm__validator_get_effective_reftype(actual_type, &actual_ref, &pushed_ref);
                pushed_ref.type = WASM_TYPE_ANYREF;
            }
            return wasm__validator_push_typed(v, at, WASM_TYPE_ANYREF, &pushed_ref);
        }
        case 0x1B: {
            wasm_valtype_t actual_type;
            wasm_reftype_t actual_ref;
            wasm_reftype_t expected_ref;
            wasm_reftype_t pushed_ref;

            expected_ref.type = WASM_TYPE_ANYREF;
            expected_ref.nullable = 1;
            err = wasm__validator_pop_ref_supertype(v, at,
                                                    WASM_TYPE_ANYREF, &expected_ref,
                                                    &actual_type, &actual_ref);
            if (err != WASM_OK) return err;
            wasm__clear_reftype(&pushed_ref);
            if (actual_type != WASM__TYPE_BOT) {
                wasm__validator_get_effective_reftype(actual_type, &actual_ref, &pushed_ref);
                pushed_ref.type = WASM_TYPE_EXTERNREF;
            }
            return wasm__validator_push_typed(v, at, WASM_TYPE_EXTERNREF, &pushed_ref);
        }
        case 0x1C: {
            wasm_reftype_t i31_ref;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            i31_ref.type = WASM_TYPE_I31REF;
            i31_ref.nullable = 0;
            i31_ref.has_type_index = 0;
            i31_ref.type_index = 0;
            return wasm__validator_push_typed(v, at, WASM_TYPE_I31REF, &i31_ref);
        }
        case 0x1D:
        case 0x1E: {
            wasm_reftype_t i31_ref;
            i31_ref.type = WASM_TYPE_I31REF;
            i31_ref.nullable = 1;
            i31_ref.has_type_index = 0;
            i31_ref.type_index = 0;
            err = wasm__validator_pop_expect_typed(v, at, WASM_TYPE_I31REF, &i31_ref);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        }
        default:
            return wasm__validator_error(v, at, "unsupported GC opcode 0xFB 0x%X", (unsigned)subop);
    }
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
            if (!wasm__validator_type_matches(v,
                                              segment->elem_type,
                                              &segment->elem_ref_type,
                                              table->reftype,
                                              &table->reftype_info))
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
            if (!wasm__validator_type_matches(v,
                                              v->mod->tables[src_table_idx].reftype,
                                              &v->mod->tables[src_table_idx].reftype_info,
                                              v->mod->tables[dst_table_idx].reftype,
                                              &v->mod->tables[dst_table_idx].reftype_info))
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
            err = wasm__validator_pop_expect_typed(v, at, table->reftype, &table->reftype_info);
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
            err = wasm__validator_pop_expect_typed(v, at, table->reftype, &table->reftype_info);
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
    wasm_functype_t* ft = wasm__module_functype(mod, func->type_idx);
    wasm_error_t err;

    if (!ft) {
        return wasm__set_validation_error(mod->rt, func_idx, 0,
                                          "function %u type index %u is not a function type",
                                          (unsigned)func_idx,
                                          (unsigned)func->type_idx);
    }

    err = wasm__validator_init(&v, mod, func_idx);
    if (err != WASM_OK) {
        return wasm__set_validation_error(mod->rt, func_idx, 0,
                                          "failed to allocate validator state");
    }

    v.body_start = func->code;
    v.r.ptr = func->code;
    v.r.end = func->code + func->code_len;
    v.r.malformed = 0;
    v.frames[0].opcode = 0xFF;
    v.frames[0].height = 0;
    v.frames[0].param_types = NULL;
    v.frames[0].param_reftypes = NULL;
    v.frames[0].num_params = 0;
    v.frames[0].result_types = ft->results;
    v.frames[0].result_reftypes = ft->result_reftypes;
    v.frames[0].num_results = ft->num_results;
    wasm__clear_reftype(&v.frames[0].inline_result_reftype);
    v.frames[0].in_catch = 0;
    v.frames[0].seen_catch_all = 0;
    v.frames[0].seen_delegate = 0;
    v.fp = 1;
    v.max_fp = 1;

    while (v.r.ptr < v.r.end) {
        const uint8_t* at = v.r.ptr;
        uint8_t op = wasm__read_u8(&v.r);

        err = WASM_OK;

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
                err = wasm__validator_check_types_typed(&v, at,
                                                        blocktype.params,
                                                        blocktype.param_reftypes,
                                                        blocktype.param_arity);
                if (err != WASM_OK) return err;
                if (v.fp >= v.frame_capacity)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = v.sp - blocktype.param_arity;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].param_reftypes = blocktype.param_reftypes;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].inline_result_reftype = blocktype.inline_result_reftype;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].result_reftypes =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result_reftype
                        : blocktype.result_reftypes;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                v.fp++;
                if (v.fp > v.max_fp) v.max_fp = v.fp;
                break;
            }
            case 0x04: {
                wasm__blocktype_t blocktype;

                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__read_blocktype(mod, &v.r, &blocktype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "%s", mod->rt->error_msg);
                err = wasm__validator_check_types_typed(&v, at,
                                                        blocktype.params,
                                                        blocktype.param_reftypes,
                                                        blocktype.param_arity);
                if (err != WASM_OK) return err;
                if (v.fp >= v.frame_capacity)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = v.sp - blocktype.param_arity;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].param_reftypes = blocktype.param_reftypes;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].inline_result_reftype = blocktype.inline_result_reftype;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].result_reftypes =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result_reftype
                        : blocktype.result_reftypes;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                v.fp++;
                if (v.fp > v.max_fp) v.max_fp = v.fp;
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
                    err = wasm__validator_check_types_typed(&v, at,
                                                            frame->result_types,
                                                            frame->result_reftypes,
                                                            frame->num_results);
                    if (err != WASM_OK) return err;
                }
                v.sp = frame->height;
                err = wasm__validator_push_many_typed(&v, at,
                                                      frame->param_types,
                                                      frame->param_reftypes,
                                                      frame->num_params);
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
                err = wasm__validator_check_types_typed(&v, at,
                                                        blocktype.params,
                                                        blocktype.param_reftypes,
                                                        blocktype.param_arity);
                if (err != WASM_OK) return err;
                if (v.fp >= v.frame_capacity)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = v.sp - blocktype.param_arity;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].param_reftypes = blocktype.param_reftypes;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].inline_result_reftype = blocktype.inline_result_reftype;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].result_reftypes =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result_reftype
                        : blocktype.result_reftypes;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                v.fp++;
                if (v.fp > v.max_fp) v.max_fp = v.fp;
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
                tag_type = wasm__module_functype(mod, mod->tags[tag_index].type_idx);
                if (!tag_type)
                    return wasm__validator_error(&v, at, "tag %u does not reference a function type",
                                                 (unsigned)tag_index);
                err = wasm__validator_finish_try_arm(&v, at, frame);
                if (err != WASM_OK) return err;
                err = wasm__validator_push_many_typed(&v, at,
                                                      tag_type->params,
                                                      tag_type->param_reftypes,
                                                      tag_type->num_params);
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
                tag_type = wasm__module_functype(mod, mod->tags[tag_index].type_idx);
                if (!tag_type)
                    return wasm__validator_error(&v, at, "tag %u does not reference a function type",
                                                 (unsigned)tag_index);
                err = wasm__validator_check_types_typed(&v, at,
                                                        tag_type->params,
                                                        tag_type->param_reftypes,
                                                        tag_type->num_params);
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
                    !wasm__validator_same_types(frame.param_types, frame.param_reftypes,
                                                frame.num_params,
                                                frame.result_types, frame.result_reftypes,
                                                frame.num_results)) {
                    return wasm__validator_error(&v, at,
                                                 "if without else requires param and result types to match");
                }
                if (!frame.unreachable) {
                    if (v.sp != frame.height + frame.num_results)
                        return wasm__validator_error(&v, at, "block leaves wrong stack height");
                    err = wasm__validator_check_types_typed(&v, at,
                                                            frame.result_types,
                                                            frame.result_reftypes,
                                                            frame.num_results);
                    if (err != WASM_OK) return err;
                }
                v.sp = frame.height;
                v.fp--;
                err = wasm__validator_push_many_typed(&v, at,
                                                      frame.result_types,
                                                      frame.result_reftypes,
                                                      frame.num_results);
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
                const wasm_reftype_t* branch_reftypes;
                uint32_t branch_count;

                if (op == 0x0D) {
                    err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                }
                if (depth >= v.fp)
                    return wasm__validator_error(&v, at, "branch depth %u out of range", (unsigned)depth);
                target = &v.frames[v.fp - 1 - depth];
                wasm__validator_branch_signature(target, &branch_types, &branch_reftypes, &branch_count);
                err = wasm__validator_check_types_typed(&v, at, branch_types, branch_reftypes, branch_count);
                if (err != WASM_OK) return err;
                if (op == 0x0C) wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x0E: {
                uint32_t count = wasm__read_leb128_u32(&v.r);
                uint32_t first_depth = UINT32_MAX;
                const wasm_valtype_t* branch_types = NULL;
                const wasm_reftype_t* branch_reftypes = NULL;
                uint32_t branch_count = 0;
                uint32_t i;

                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                for (i = 0; i <= count; i++) {
                    uint32_t depth = wasm__read_leb128_u32(&v.r);
                    const wasm_valtype_t* candidate_types;
                    const wasm_reftype_t* candidate_reftypes;
                    uint32_t candidate_count;

                    if (depth >= v.fp)
                        return wasm__validator_error(&v, at, "br_table depth %u out of range", (unsigned)depth);
                    wasm__validator_branch_signature(&v.frames[v.fp - 1 - depth],
                                                     &candidate_types,
                                                     &candidate_reftypes,
                                                     &candidate_count);
                    if (i == 0) {
                        first_depth = depth;
                        branch_types = candidate_types;
                        branch_reftypes = candidate_reftypes;
                        branch_count = candidate_count;
                    } else if (!wasm__validator_same_types(branch_types, branch_reftypes, branch_count,
                                                           candidate_types, candidate_reftypes,
                                                           candidate_count)) {
                        return wasm__validator_error(&v, at,
                                                     "br_table targets do not share the same signature");
                    }
                }
                (void)first_depth;
                err = wasm__validator_check_types_typed(&v, at,
                                                        branch_types,
                                                        branch_reftypes,
                                                        branch_count);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x0F:
                err = wasm__validator_check_types_typed(&v, at,
                                                        ft->results,
                                                        ft->result_reftypes,
                                                        ft->num_results);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            case 0x10: {
                uint32_t callee = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;
                if (callee >= mod->num_funcs)
                    return wasm__validator_error(&v, at, "call target %u out of range", (unsigned)callee);
                callee_type = wasm__module_functype(mod, mod->funcs[callee].type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call target %u does not use a function type",
                                                 (unsigned)callee);
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
                if (err != WASM_OK) return err;
                v.sp -= callee_type->num_params;
                err = wasm__validator_push_many_typed(&v, at,
                                                      callee_type->results,
                                                      callee_type->result_reftypes,
                                                      callee_type->num_results);
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
                callee_type = wasm__module_functype(mod, type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call_indirect type index %u is not a function type",
                                                 (unsigned)type_idx);
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
                if (err != WASM_OK) return err;
                v.sp -= callee_type->num_params;
                err = wasm__validator_push_many_typed(&v, at,
                                                      callee_type->results,
                                                      callee_type->result_reftypes,
                                                      callee_type->num_results);
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
                callee_type = wasm__module_functype(mod, mod->funcs[callee].type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call target %u does not use a function type",
                                                 (unsigned)callee);
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
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
                callee_type = wasm__module_functype(mod, type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call_indirect type index %u is not a function type",
                                                 (unsigned)type_idx);
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
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
                wasm_reftype_t lhs_ref, rhs_ref;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_any_typed(&v, at, &rhs, &rhs_ref);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_any_typed(&v, at, &lhs, &lhs_ref);
                if (err != WASM_OK) return err;
                if (!wasm__validator_type_matches(&v, lhs, &lhs_ref, rhs, &rhs_ref) ||
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
                wasm_reftype_t select_ref_type;

                err = wasm__validator_read_typed_select(&v, at, &select_type, &select_ref_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect_typed(&v, at, select_type, &select_ref_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect_typed(&v, at, select_type, &select_ref_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_push_typed(&v, at, select_type, &select_ref_type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x20: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                err = wasm__validator_push_typed(&v, at,
                                                 func->locals[index],
                                                 func->local_reftypes ? &func->local_reftypes[index] : NULL);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x21: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                err = wasm__validator_pop_expect_typed(&v, at,
                                                       func->locals[index],
                                                       func->local_reftypes ? &func->local_reftypes[index] : NULL);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x22: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                err = wasm__validator_check_types_typed(&v, at,
                                                        &func->locals[index],
                                                        func->local_reftypes ? &func->local_reftypes[index] : NULL,
                                                        1);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x23: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= mod->num_globals)
                    return wasm__validator_error(&v, at, "global index %u out of range", (unsigned)index);
                err = wasm__validator_push_typed(&v, at,
                                                 mod->globals[index].type,
                                                 &mod->globals[index].ref_type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x24: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= mod->num_globals)
                    return wasm__validator_error(&v, at, "global index %u out of range", (unsigned)index);
                if (!mod->globals[index].is_mutable)
                    return wasm__validator_error(&v, at, "global %u is immutable", (unsigned)index);
                err = wasm__validator_pop_expect_typed(&v, at,
                                                       mod->globals[index].type,
                                                       &mod->globals[index].ref_type);
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
                err = wasm__validator_push_typed(&v, at,
                                                 mod->tables[table_idx].reftype,
                                                 &mod->tables[table_idx].reftype_info);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x26: {
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "table %u out of range", (unsigned)table_idx);
                err = wasm__validator_pop_expect_typed(&v, at,
                                                       mod->tables[table_idx].reftype,
                                                       &mod->tables[table_idx].reftype_info);
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
                wasm_reftype_t type_info = { 0 };
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                err = wasm__read_heaptype(mod, &v.r, &type_info);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "invalid heaptype");
                type_info.nullable = 1;
                reftype = type_info.type ? type_info.type : WASM_TYPE_ANYREF;
                err = wasm__validator_push_typed(&v, at, reftype, &type_info);
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
                {
                    wasm_reftype_t ref_type;
                    wasm__clear_reftype(&ref_type);
                    ref_type.type = WASM_TYPE_FUNCREF;
                    ref_type.nullable = 0;
                    err = wasm__validator_push_typed(&v, at, WASM_TYPE_FUNCREF, &ref_type);
                }
                if (err != WASM_OK) return err;
                break;
            }
            case 0xD3:
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_EQREF);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_EQREF);
                if (err != WASM_OK) return err;
                err = wasm__validator_push(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            case 0xFB:
                err = wasm__validator_validate_gc(&v, at);
                if (err != WASM_OK) return err;
                break;
            case 0xFC:
                err = wasm__validator_validate_prefixed(&v, at);
                if (err != WASM_OK) return err;
                break;
            case 0xFD:
                err = wasm__validator_validate_simd(&v, at);
                if (err != WASM_OK) return err;
                break;
            default:
                err = wasm__validator_error(&v, at, "unknown opcode 0x%02X", op);
                goto done;
        }

        if (v.r.malformed) {
            err = wasm__validator_error(&v, at, "malformed immediate");
            goto done;
        }
    }

    if (v.fp != 0) {
        err = wasm__validator_error(&v, v.r.end, "function body ended before all blocks closed");
        goto done;
    }
    if (v.sp != ft->num_results) {
        err = wasm__validator_error(&v, v.r.end, "function leaves %u stack values, expected %u",
                                    (unsigned)v.sp,
                                    (unsigned)ft->num_results);
        goto done;
    }
    func->max_label_depth = v.max_fp;
    err = WASM_OK;

done:
    wasm__validator_destroy(&v);
    return err;
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
    if (len > 0) {
        mod->module_bytes = (uint8_t*)WASM_MALLOC(len);
        if (!mod->module_bytes) {
            WASM__SET_ERR(rt, WASM_ERR_OOM, "module bytes alloc");
            WASM_FREE(mod);
            return NULL;
        }
        memcpy(mod->module_bytes, bytes, len);
        mod->module_size = len;
        r.ptr = mod->module_bytes + 8;
        r.end = mod->module_bytes + len;
        r.malformed = 0;
    }
    wasm__gc_register_module(rt, mod);

#define WASM__LOAD_FAIL(stage_literal)                                               \
    do {                                                                             \
        if (rt->last_error == WASM_OK)                                               \
            WASM__SET_ERR(rt, err, "%s: %s", stage_literal, wasm_error_string(err)); \
        wasm_free_module(mod);                                                       \
        return NULL;                                                                 \
    } while (0)

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
                err = wasm__decode_custom_section(mod, &sr);
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
            case 13:
                err = wasm__decode_tag_section(mod, &sr);
                break;
            default:
                WASM__SET_ERR(rt, WASM_ERR_INVALID_SECTION, "unknown section id %u", (unsigned)sid);
                err = WASM_ERR_INVALID_SECTION;
                break;
        }
        if (err != WASM_OK && rt->last_error == WASM_OK) {
            WASM__SET_ERR(rt, err, "section %u decode failed: %s",
                          (unsigned)sid, wasm_error_string(err));
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

    err = wasm__resolve_module_reftypes(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("resolve_module_reftypes");
    }

    err = wasm__canonicalize_types(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("canonicalize_types");
    }

    err = wasm__validate_types(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("validate_types");
    }

    err = wasm__validate_structural(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("validate_structural");
    }

    err = wasm__validate_code(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("validate_code");
    }

    err = wasm__build_module_control_targets(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("build_control_targets");
    }

    if (mod->num_memories > 0) {
        uint32_t mi;

        for (mi = 0; mi < mod->num_memories; mi++) {
            err = wasm__init_memory_storage(&mod->memories[mi]);
            if (err != WASM_OK) {
                WASM__LOAD_FAIL("init_memory_storage");
            }
        }
    }
    if (mod->num_tables > 0) {
        uint32_t ti;
        for (ti = 0; ti < mod->num_tables; ti++) {
            err = wasm__init_table_storage(&mod->tables[ti]);
            if (err != WASM_OK) {
                WASM__LOAD_FAIL("init_table_storage");
            }
        }
    }

    err = wasm__apply_active_data_segments(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("apply_active_data_segments");
    }

    err = wasm__apply_active_elem_segments(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("apply_active_elem_segments");
    }

    err = wasm__run_startup(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("run_startup");
    }

#undef WASM__LOAD_FAIL

    return mod;
}

void wasm_free_module(wasm_module_t* mod) {
    wasm_gc_header_t* object;
    uint32_t i;
    if (!mod) return;
    if (mod->rt) {
        for (object = mod->rt->gc_allocations; object; object = object->next_alloc) {
            if (object->module == mod) object->module = NULL;
        }
        wasm__gc_unregister_module(mod->rt, mod);
    }
    for (i = 0; i < mod->num_types; i++) wasm__free_comptype(&mod->types[i]);
    WASM_FREE(mod->types);
    WASM_FREE(mod->rec_groups);
    for (i = 0; i < mod->num_funcs; i++) {
        WASM_FREE(mod->funcs[i].locals);
        WASM_FREE(mod->funcs[i].local_reftypes);
        WASM_FREE(mod->funcs[i].control_targets);
    }
    WASM_FREE(mod->funcs);
    for (i = 0; i < mod->num_imports; i++) wasm__free_import_info(&mod->imports[i]);
    WASM_FREE(mod->imports);
    WASM_FREE(mod->exports);
    WASM_FREE(mod->globals);
    WASM_FREE(mod->tags);
    for (i = 0; i < mod->num_custom_sections; i++) wasm__free_custom_section(&mod->custom_sections[i]);
    WASM_FREE(mod->custom_sections);
    for (i = 0; i < mod->num_data_segments; i++) wasm__free_data_segment(&mod->data_segments[i]);
    WASM_FREE(mod->data_segments);
    for (i = 0; i < mod->num_elem_segments; i++) wasm__free_elem_segment(&mod->elem_segments[i]);
    WASM_FREE(mod->elem_segments);
    for (i = 0; i < mod->num_tables; i++) wasm__free_table(&mod->tables[i]);
    WASM_FREE(mod->tables);
    for (i = 0; i < mod->num_memories; i++) wasm__free_memory(&mod->memories[i]);
    WASM_FREE(mod->memories);
    WASM_FREE(mod->module_bytes);
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

static int wasm__size_mul(size_t a, size_t b, size_t* out) {
    if (a != 0 && b > ((size_t)-1) / a) return 0;
    *out = a * b;
    return 1;
}

static int wasm__size_add(size_t a, size_t b, size_t* out) {
    if (a > ((size_t)-1) - b) return 0;
    *out = a + b;
    return 1;
}

static int wasm__size_align(size_t value, size_t alignment, size_t* out) {
    size_t remainder;

    if (alignment <= 1) {
        *out = value;
        return 1;
    }

    remainder = value % alignment;
    if (remainder == 0) {
        *out = value;
        return 1;
    }

    return wasm__size_add(value, alignment - remainder, out);
}

static int wasm__frame_storage_size(uint32_t num_locals,
                                    uint32_t max_labels,
                                    size_t* out) {
    size_t locals_bytes = 0;
    size_t labels_bytes;
    size_t total_bytes;

    if (num_locals > 0) {
        if (!wasm__size_mul((size_t)num_locals, sizeof(wasm_value_t), &locals_bytes)) return 0;
        if (!wasm__size_align(locals_bytes, sizeof(void*), &locals_bytes)) return 0;
    }

    if (max_labels == 0) max_labels = 1;
    if (!wasm__size_mul((size_t)max_labels, sizeof(wasm__label_t), &labels_bytes)) return 0;
    if (!wasm__size_align(labels_bytes, sizeof(void*), &labels_bytes)) return 0;
    if (!wasm__size_add(locals_bytes, labels_bytes, &total_bytes)) return 0;

    *out = total_bytes;
    return 1;
}

static int wasm__module_frame_arena_requirement(wasm_module_t* mod, size_t* out) {
    size_t max_frame_bytes = 0;
    size_t total_bytes;
    size_t minimum_size;
    uint32_t i;

    if (!mod || !out) return 0;

    for (i = mod->num_func_imports; i < mod->num_funcs; i++) {
        wasm_func_t* func = &mod->funcs[i];
        size_t frame_bytes;
        uint32_t max_labels = func->max_label_depth ? func->max_label_depth : 1u;

        if (!wasm__frame_storage_size(func->num_locals, max_labels, &frame_bytes)) return 0;
        if (frame_bytes > max_frame_bytes) max_frame_bytes = frame_bytes;
    }

    minimum_size = mod->rt ? mod->rt->frame_arena_size : (size_t)WASM_FRAME_ARENA_SIZE;
    if (max_frame_bytes == 0) {
        *out = minimum_size;
        return 1;
    }

    if (!wasm__size_mul(max_frame_bytes,
                        (size_t)((mod->rt && mod->rt->max_call_frames) ? mod->rt->max_call_frames : WASM_MAX_CALL_DEPTH),
                        &total_bytes)) {
        return 0;
    }
    if (total_bytes < minimum_size) total_bytes = minimum_size;

    *out = total_bytes;
    return 1;
}

static wasm_error_t wasm__reserve_frame_arena(wasm_runtime_t* rt, size_t size) {
    uint8_t* arena;

    if (size <= rt->frame_arena_size) return WASM_OK;
    if (rt->call_frame_sp != 0) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "cannot grow frame arena while %u call frames are active",
                      (unsigned)rt->call_frame_sp);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }

    arena = (uint8_t*)WASM_REALLOC(rt->frame_arena, size);
    if (!arena) {
        WASM__SET_ERR(rt, WASM_ERR_OOM,
                      "failed to reserve %zu-byte frame arena",
                      size);
        return WASM_ERR_OOM;
    }

    rt->frame_arena = arena;
    rt->frame_arena_size = size;
    return WASM_OK;
}

static wasm_error_t wasm__ensure_module_frame_arena(wasm_module_t* mod) {
    size_t required_size;

    if (!mod || !mod->rt) return WASM_OK;
    if (!wasm__module_frame_arena_requirement(mod, &required_size)) {
        WASM__SET_ERR(mod->rt, WASM_ERR_OOM, "%s", "frame arena size overflow");
        return WASM_ERR_OOM;
    }

    return wasm__reserve_frame_arena(mod->rt, required_size);
}

struct wasm__call_frame_t {
    uint32_t func_idx;
    uint32_t sp_base;
    wasm_value_t* locals;
    wasm__label_t* labels;
    uint32_t lsp;
    uint32_t max_labels;
    wasm__reader_t r;
    wasm_func_t* func;
    wasm_functype_t* ft;
    wasm_value_t* results_dst;
    uint32_t num_results;
    size_t arena_saved;
    uint32_t current_offset;
};

static wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt) {
    rt->call_frames = (wasm__call_frame_t*)WASM_CALLOC(rt->max_call_frames, sizeof(*rt->call_frames));
    if (!rt->call_frames) return WASM_ERR_OOM;
    rt->call_frame_sp = 0;
    return WASM_OK;
}

static void wasm__call_frame_stack_destroy(wasm_runtime_t* rt) {
    WASM_FREE(rt->call_frames);
    rt->call_frames = NULL;
    rt->max_call_frames = 0;
    rt->call_frame_sp = 0;
}

static void wasm__clear_backtrace(wasm_runtime_t* rt) {
    if (!rt) return;
    rt->backtrace_depth = 0;
}

static uint32_t wasm__call_frame_offset(const wasm__call_frame_t* cf) {
    ptrdiff_t offset;

    if (!cf || !cf->func || !cf->func->code) return 0;
    if (cf->current_offset <= cf->func->code_len) return cf->current_offset;
    if (!cf->r.ptr || cf->r.ptr < cf->func->code) return 0;
    offset = cf->r.ptr - cf->func->code;
    if (offset < 0) return 0;
    if ((uint32_t)offset > cf->func->code_len) return cf->func->code_len;
    return (uint32_t)offset;
}

static void wasm__capture_backtrace(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    uint32_t depth;
    uint32_t i;

    if (!rt) return;
    if (rt->call_frame_sp <= base_frame_sp) {
        rt->backtrace_depth = 0;
        return;
    }

    depth = rt->call_frame_sp - base_frame_sp;
    if (depth > rt->max_call_frames) depth = rt->max_call_frames;
    rt->backtrace_depth = depth;
    for (i = 0; i < depth; i++) {
        const wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1u - i];

        rt->backtrace_func_indices[i] = cf->func_idx;
        rt->backtrace_offsets[i] = wasm__call_frame_offset(cf);
    }
}

static uint32_t wasm__visible_call_stack_depth(const wasm_runtime_t* rt) {
    if (!rt) return 0;
    if (rt->call_frame_sp > 0) return rt->call_frame_sp;
    return rt->backtrace_depth;
}

static int wasm__visible_call_frame_info(const wasm_runtime_t* rt,
                                         uint32_t depth,
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset) {
    if (!rt) return 0;

    if (rt->call_frame_sp > 0) {
        const wasm__call_frame_t* cf;

        if (depth >= rt->call_frame_sp) return 0;
        cf = &rt->call_frames[rt->call_frame_sp - 1u - depth];
        if (out_func_idx) *out_func_idx = cf->func_idx;
        if (out_offset) *out_offset = wasm__call_frame_offset(cf);
        return 1;
    }

    if (depth >= rt->backtrace_depth) return 0;
    if (out_func_idx) *out_func_idx = rt->backtrace_func_indices[depth];
    if (out_offset) *out_offset = rt->backtrace_offsets[depth];
    return 1;
}

static void wasm__gc_mark_object(wasm_gc_header_t* object);

static void wasm__gc_mark_value(const wasm_value_t* value) {
    if (!wasm__is_heap_gc_ref_value(value)) return;
    wasm__gc_mark_object((wasm_gc_header_t*)value->of.gc_obj);
}

static void wasm__gc_mark_values(const wasm_value_t* values, uint32_t count) {
    uint32_t i;

    if (!values) return;
    for (i = 0; i < count; i++) wasm__gc_mark_value(&values[i]);
}

static void wasm__gc_mark_struct_fields(wasm_gc_header_t* object,
                                        const wasm_structtype_t* type) {
    uint32_t i;

    for (i = 0; i < type->num_fields; i++) {
        size_t offset;

        if (type->fields[i].storage.kind == WASM_STORAGE_PACKED) continue;
        offset = wasm__gc_struct_field_offset(type, i);
        if (offset == (size_t)-1) continue;
        wasm__gc_mark_value((const wasm_value_t*)(((const uint8_t*)object) + offset));
    }
}

static void wasm__gc_mark_array_elements(wasm_gc_header_t* object,
                                         const wasm_arraytype_t* type) {
    wasm_gc_array_t* array_object;
    size_t data_offset;
    wasm_value_t* elems;
    uint32_t i;

    if (type->field.storage.kind == WASM_STORAGE_PACKED) return;
    data_offset = wasm__gc_array_data_offset(type);
    if (data_offset == (size_t)-1) return;

    array_object = (wasm_gc_array_t*)object;
    elems = (wasm_value_t*)(((uint8_t*)array_object) + data_offset);
    for (i = 0; i < array_object->length; i++) wasm__gc_mark_value(&elems[i]);
}

static void wasm__gc_mark_object(wasm_gc_header_t* object) {
    const wasm_module_t* mod;

    if (!object || object->marked) return;
    object->marked = 1;

    mod = object->module;
    if (!mod || object->type_index >= mod->num_types) return;

    switch (object->kind) {
        case WASM_GC_OBJ_STRUCT: {
            const wasm_structtype_t* type = wasm__module_const_structtype(mod, object->type_index);

            if (type) wasm__gc_mark_struct_fields(object, type);
            break;
        }
        case WASM_GC_OBJ_ARRAY: {
            const wasm_arraytype_t* type = wasm__module_const_arraytype(mod, object->type_index);

            if (type) wasm__gc_mark_array_elements(object, type);
            break;
        }
        default:
            break;
    }
}

static void wasm__gc_mark_runtime_roots(wasm_runtime_t* rt) {
    wasm_module_t* mod;
    uint32_t i;

    if (!rt) return;

    wasm__gc_mark_values(rt->stack, rt->sp);
    wasm__gc_mark_values(rt->pending_exception.values, rt->pending_exception.num_values);
    for (i = 0; i < rt->num_global_imports; i++) {
        if (rt->global_imports[i].value) wasm__gc_mark_value(rt->global_imports[i].value);
    }
    for (i = 0; i < rt->call_frame_sp; i++) {
        wasm__call_frame_t* cf = &rt->call_frames[i];
        uint32_t j;

        if (cf->func && cf->locals) wasm__gc_mark_values(cf->locals, cf->func->num_locals);
        for (j = 0; j < cf->lsp; j++) {
            wasm__gc_mark_values(cf->labels[j].caught_values, cf->labels[j].caught_count);
        }
    }
    for (mod = rt->gc_modules; mod; mod = mod->gc_next) {
        for (i = 0; i < mod->num_globals; i++) {
            if (mod->globals[i].is_import && mod->globals[i].import_value)
                wasm__gc_mark_value(mod->globals[i].import_value);
            else
                wasm__gc_mark_value(&mod->globals[i].value);
        }
        for (i = 0; i < mod->num_tables; i++) {
            wasm_table_t* table = wasm__table_at(mod, i);
            if (table && table->elems) wasm__gc_mark_values(table->elems, table->size);
        }
        for (i = 0; i < mod->num_elem_segments; i++) {
            wasm_elem_segment_t* segment = &mod->elem_segments[i];

            if (!segment->is_passive || segment->is_declarative || segment->is_dropped) continue;
            wasm__gc_mark_values(segment->elems, segment->num_elems);
        }
    }
}

static void wasm__gc_forward_value(wasm_runtime_t* rt, wasm_value_t* value) {
    wasm_gc_header_t* old_object;

    if (!wasm__is_heap_gc_ref_value(value)) return;
    old_object = value->of.gc_obj;
    if (!wasm__gc_ptr_in_heap(rt, old_object) || !old_object->next_alloc) return;

    value->of.gc_obj = old_object->next_alloc;
    value->of.gc_ref = (uintptr_t)old_object->next_alloc;
}

static void wasm__gc_forward_values(wasm_runtime_t* rt, wasm_value_t* values, uint32_t count) {
    uint32_t i;

    if (!values) return;
    for (i = 0; i < count; i++) wasm__gc_forward_value(rt, &values[i]);
}

static void wasm__gc_forward_object_refs(wasm_runtime_t* rt, wasm_gc_header_t* object) {
    const wasm_module_t* mod;

    if (!object) return;
    mod = object->module;
    if (!mod || object->type_index >= mod->num_types) return;

    switch (object->kind) {
        case WASM_GC_OBJ_STRUCT: {
            const wasm_structtype_t* type = wasm__module_const_structtype(mod, object->type_index);
            uint32_t i;

            if (!type) return;
            for (i = 0; i < type->num_fields; i++) {
                size_t offset;

                if (type->fields[i].storage.kind == WASM_STORAGE_PACKED) continue;
                offset = wasm__gc_struct_field_offset(type, i);
                if (offset == (size_t)-1) continue;
                wasm__gc_forward_value(rt, (wasm_value_t*)(((uint8_t*)object) + offset));
            }
            break;
        }
        case WASM_GC_OBJ_ARRAY: {
            const wasm_arraytype_t* type = wasm__module_const_arraytype(mod, object->type_index);
            size_t data_offset;

            if (!type || type->field.storage.kind == WASM_STORAGE_PACKED) return;
            data_offset = wasm__gc_array_data_offset(type);
            if (data_offset == (size_t)-1) return;
            wasm__gc_forward_values(rt,
                                    (wasm_value_t*)(((uint8_t*)object) + data_offset),
                                    ((wasm_gc_array_t*)object)->length);
            break;
        }
        default:
            break;
    }
}

static void wasm__gc_forward_runtime_roots(wasm_runtime_t* rt) {
    wasm_module_t* mod;
    uint32_t i;

    if (!rt) return;

    wasm__gc_forward_values(rt, rt->stack, rt->sp);
    wasm__gc_forward_values(rt, rt->pending_exception.values, rt->pending_exception.num_values);
    for (i = 0; i < rt->num_global_imports; i++) {
        if (rt->global_imports[i].value) wasm__gc_forward_value(rt, rt->global_imports[i].value);
    }
    for (i = 0; i < rt->call_frame_sp; i++) {
        wasm__call_frame_t* cf = &rt->call_frames[i];
        uint32_t j;

        if (cf->func && cf->locals) wasm__gc_forward_values(rt, cf->locals, cf->func->num_locals);
        for (j = 0; j < cf->lsp; j++) {
            wasm__gc_forward_values(rt, cf->labels[j].caught_values, cf->labels[j].caught_count);
        }
    }
    for (mod = rt->gc_modules; mod; mod = mod->gc_next) {
        for (i = 0; i < mod->num_globals; i++) {
            if (mod->globals[i].is_import && mod->globals[i].import_value)
                wasm__gc_forward_value(rt, mod->globals[i].import_value);
            else
                wasm__gc_forward_value(rt, &mod->globals[i].value);
        }
        for (i = 0; i < mod->num_tables; i++) {
            wasm_table_t* table = wasm__table_at(mod, i);
            if (table && table->elems)
                wasm__gc_forward_values(rt, table->elems, table->size);
        }
        for (i = 0; i < mod->num_elem_segments; i++) {
            wasm_elem_segment_t* segment = &mod->elem_segments[i];

            if (!segment->is_passive || segment->is_declarative || segment->is_dropped) continue;
            wasm__gc_forward_values(rt, segment->elems, segment->num_elems);
        }
    }
}

void wasm_gc_collect(wasm_runtime_t* rt) {
    wasm_gc_header_t* object;
    wasm_gc_header_t* new_allocations = NULL;
    wasm_gc_header_t* new_tail = NULL;
    uint8_t* new_heap = NULL;
    size_t new_offset = 0;
    int has_live = 0;

    if (!rt || !rt->gc_heap) return;

    for (object = rt->gc_allocations; object; object = object->next_alloc) object->marked = 0;

    wasm__gc_mark_runtime_roots(rt);
    for (object = rt->gc_allocations; object; object = object->next_alloc) {
        if (object->marked) {
            has_live = 1;
            break;
        }
    }

    if (!has_live) {
        rt->gc_allocations = NULL;
        rt->gc_heap_offset = 0;
        return;
    }

    new_heap = (uint8_t*)WASM_GC_ALLOC(rt->gc_heap_size);
    if (!new_heap) return;

    for (object = rt->gc_allocations; object;) {
        wasm_gc_header_t* next = object->next_alloc;

        if (object->marked) {
            size_t aligned_offset = wasm__align_up_size(new_offset, (size_t)WASM__ALIGNOF(wasm_value_t));
            wasm_gc_header_t* moved;

            if (aligned_offset == (size_t)-1 || aligned_offset > rt->gc_heap_size ||
                object->object_size > rt->gc_heap_size - aligned_offset) {
                WASM_GC_FREE(new_heap);
                return;
            }

            moved = (wasm_gc_header_t*)(new_heap + aligned_offset);
            memcpy(moved, object, object->object_size);
            moved->marked = 0;
            moved->next_alloc = NULL;

            if (!new_allocations)
                new_allocations = moved;
            else
                new_tail->next_alloc = moved;
            new_tail = moved;

            object->next_alloc = moved;
            new_offset = aligned_offset + object->object_size;
        }

        object = next;
    }

    wasm__gc_forward_runtime_roots(rt);
    for (object = new_allocations; object; object = object->next_alloc) {
        wasm__gc_forward_object_refs(rt, object);
    }

    WASM_GC_FREE(rt->gc_heap);
    rt->gc_heap = new_heap;
    rt->gc_heap_offset = new_offset;
    rt->gc_allocations = new_allocations;
}

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

#define WASM__ERR_EXCEPTION ((wasm_error_t) - 1)
#define WASM__ERR_CALL_FRAME ((wasm_error_t) - 2)
#define WASM__ERR_RETURN_FRAME ((wasm_error_t) - 3)
#define WASM__ERR_RESUME_FRAME ((wasm_error_t) - 4)
#define WASM__ERR_TAIL_CALL_FRAME ((wasm_error_t) - 5)

static void wasm__init_call_frame(wasm__call_frame_t* cf,
                                  wasm_runtime_t* rt,
                                  uint32_t func_idx,
                                  wasm_func_t* func,
                                  wasm_functype_t* ft,
                                  wasm_value_t* locals,
                                  wasm__label_t* labels,
                                  wasm_value_t* results_dst,
                                  uint32_t num_results);

static wasm_error_t wasm__handle_exception_in_frame(wasm_module_t* mod,
                                                    wasm__label_t* labels,
                                                    uint32_t* label_sp,
                                                    wasm__reader_t* r,
                                                    uint32_t sp_base);
static wasm_error_t wasm__handle_exception_frames(wasm_module_t* mod, uint32_t base_frame_sp);

static void wasm__clear_call_frame(wasm__call_frame_t* cf) {
    if (!cf) return;

    while (cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }
}

static wasm_error_t wasm__push_call_frame(wasm_module_t* mod,
                                          uint32_t func_idx,
                                          const wasm_value_t* args,
                                          uint32_t num_args,
                                          wasm_value_t* results_dst,
                                          uint32_t num_results) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err;
    wasm_func_t* func;
    wasm_functype_t* ft;
    wasm__call_frame_t* cf;
    wasm_value_t* locals = NULL;
    wasm__label_t* labels;
    uint32_t max_labels;
    size_t arena_saved;
    uint32_t i;

    if (rt->call_frame_sp == 0) {
        err = wasm__ensure_module_frame_arena(mod);
        if (err != WASM_OK) return err;
    }

    if (rt->call_frame_sp >= rt->max_call_frames) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "call frame depth exceeded limit %u",
                      (unsigned)rt->max_call_frames);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import) return WASM_ERR_MALFORMED;
    ft = wasm__module_functype(mod, func->type_idx);
    if (!ft) return WASM_ERR_MALFORMED;
    max_labels = func->max_label_depth ? func->max_label_depth : 1;
    if (max_labels > rt->max_labels) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "function label depth %u exceeds limit %u",
                      (unsigned)max_labels,
                      (unsigned)rt->max_labels);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    arena_saved = rt->frame_arena_offset;

    if (func->num_locals > 0) {
        locals = (wasm_value_t*)wasm__arena_alloc(rt, func->num_locals * sizeof(wasm_value_t));
        if (!locals) {
            wasm__arena_reset(rt, arena_saved);
            WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED, "%s", "frame arena exhausted");
            return WASM_ERR_CALL_STACK_EXHAUSTED;
        }
    }

    labels = (wasm__label_t*)wasm__arena_alloc(rt, max_labels * sizeof(wasm__label_t));
    if (!labels) {
        wasm__arena_reset(rt, arena_saved);
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED, "%s", "frame arena exhausted");
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }

    cf = &rt->call_frames[rt->call_frame_sp++];
    wasm__init_call_frame(cf, rt, func_idx, func, ft, locals, labels, results_dst, num_results);
    cf->arena_saved = arena_saved;

    for (i = 0; i < func->num_locals; i++) cf->locals[i] = wasm__default_value(func->locals[i]);
    for (i = 0; i < ft->num_params && i < num_args; i++) {
        cf->locals[i] = args[i];
        cf->locals[i].type = ft->params[i];
    }

    return WASM_OK;
}

static wasm_error_t wasm__finish_call_frame(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    wasm__call_frame_t* cf;
    wasm_value_t result_values_buf[8];
    wasm_value_t* result_values = result_values_buf;
    wasm_value_t* results_dst;
    uint32_t result_count;
    uint32_t i;
    int is_root;

    if (rt->call_frame_sp <= base_frame_sp) return WASM_ERR_MALFORMED;

    cf = &rt->call_frames[rt->call_frame_sp - 1];
    results_dst = cf->results_dst;
    result_count = cf->ft->num_results;
    is_root = (rt->call_frame_sp == base_frame_sp + 1u);

    if (result_count > 8) {
        result_values = (wasm_value_t*)WASM_MALLOC(result_count * sizeof(wasm_value_t));
        if (!result_values) return WASM_ERR_OOM;
    }

    for (i = 0; i < result_count; i++)
        result_values[i] = rt->stack[rt->sp - result_count + i];

    rt->sp = cf->sp_base;
    wasm__arena_reset(rt, cf->arena_saved);
    rt->call_frame_sp--;
    memset(cf, 0, sizeof(*cf));

    if (is_root) {
        for (i = 0; i < result_count; i++) results_dst[i] = result_values[i];
    } else {
        for (i = 0; i < result_count; i++) rt->stack[rt->sp++] = result_values[i];
    }

    if (result_values != result_values_buf) WASM_FREE(result_values);
    return WASM_OK;
}

static wasm_error_t wasm__handle_exception_frames(wasm_module_t* mod, uint32_t base_frame_sp) {
    wasm_runtime_t* rt = mod->rt;

    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm_error_t err;

        err = wasm__handle_exception_in_frame(mod, cf->labels, &cf->lsp, &cf->r, cf->sp_base);
        if (err == WASM_OK) return WASM_OK;
        if (err != WASM__ERR_EXCEPTION) return err;

        wasm__clear_call_frame(cf);
        rt->sp = cf->sp_base;
        wasm__arena_reset(rt, cf->arena_saved);
        memset(cf, 0, sizeof(*cf));
        rt->call_frame_sp--;
    }

    return WASM__ERR_EXCEPTION;
}

static void wasm__skip_blocktype(wasm__reader_t* r) {
    uint8_t lead = *r->ptr;

    if (lead == 0x40 || wasm__is_valtype_byte(lead)) {
        if (lead == 0x40)
            r->ptr++;
        else
            wasm__skip_valtype(r);
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

static void wasm__skip_gc_immediates(wasm__reader_t* r) {
    uint32_t subop = wasm__read_leb128_u32(r);

    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x06:
        case 0x07:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x10:
        case 0x12:
        case 0x13:
            wasm__read_leb128_u32(r);
            break;
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x09:
        case 0x0A:
        case 0x11:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x08:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            wasm__skip_heaptype(r);
            break;
        case 0x18:
        case 0x19:
            if (r->ptr < r->end) r->ptr++;
            wasm__read_leb128_u32(r);
            wasm__skip_heaptype(r);
            wasm__skip_heaptype(r);
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
    blocktype->param_reftypes = NULL;
    blocktype->result_reftypes = NULL;
    blocktype->inline_result = WASM_TYPE_VOID;
    wasm__clear_reftype(&blocktype->inline_result_reftype);

    if (lead == 0x40) {
        r->ptr++;
        return WASM_OK;
    }

    if (wasm__is_valtype_byte(lead)) {
        err = wasm__read_valtype(mod, r, &blocktype->inline_result, &blocktype->inline_result_reftype);
        if (err != WASM_OK) return err;
        blocktype->result_arity = 1;
        blocktype->results = &blocktype->inline_result;
        blocktype->result_reftypes = &blocktype->inline_result_reftype;
        return WASM_OK;
    }

    {
        int32_t type_idx = wasm__read_leb128_i32(r);
        wasm_functype_t* ft;

        if (wasm__require_feature(mod, WASM_FEATURE_MULTI_VALUE) != WASM_OK) return WASM_ERR_MALFORMED;
        if (type_idx < 0 || (uint32_t)type_idx >= mod->num_types) return WASM_ERR_MALFORMED;
        ft = wasm__module_functype(mod, (uint32_t)type_idx);
        if (!ft) return WASM_ERR_MALFORMED;
        blocktype->param_arity = ft->num_params;
        blocktype->result_arity = ft->num_results;
        blocktype->params = ft->params;
        blocktype->results = ft->results;
        blocktype->param_reftypes = ft->param_reftypes;
        blocktype->result_reftypes = ft->result_reftypes;
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
        case 0x3F:
        case 0x40:
            wasm__read_leb128_u32(r);
            break;
        case 0x41:
            wasm__read_leb128_i32(r);
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
            while (count-- > 0 && r->ptr < r->end) wasm__skip_valtype(r);
            break;
        }
        case 0xD0:
            wasm__skip_heaptype(r);
            break;
        case 0xD2:
            wasm__read_leb128_u32(r);
            break;
        case 0xFC:
            wasm__skip_prefixed_immediates(r);
            break;
        case 0xFB:
            wasm__skip_gc_immediates(r);
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
    wasm__control_frame_t* stack;
    uint32_t stack_capacity;
    uint32_t depth = 0;
    uint32_t i;

    if (!func->code || func->code_len == 0) return WASM_OK;

    targets = (wasm__control_target_t*)WASM_MALLOC(func->code_len * sizeof(wasm__control_target_t));
    if (!targets) return WASM_ERR_OOM;

    stack_capacity = func->max_label_depth ? func->max_label_depth : 1u;
    stack = (wasm__control_frame_t*)WASM_MALLOC((size_t)stack_capacity * sizeof(*stack));
    if (!stack) {
        WASM_FREE(targets);
        return WASM_ERR_OOM;
    }

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
            WASM_FREE(stack);
            WASM_FREE(targets);
            return WASM_ERR_MALFORMED;
        }

        switch (op) {
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x06:
                if (depth >= stack_capacity) {
                    WASM_FREE(stack);
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
                        WASM_FREE(stack);
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
            WASM_FREE(stack);
            WASM_FREE(targets);
            return WASM_ERR_MALFORMED;
        }
    }

    if (depth != 0) {
        WASM_FREE(stack);
        WASM_FREE(targets);
        return WASM_ERR_MALFORMED;
    }

    WASM_FREE(stack);
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

static wasm_error_t wasm__handle_exception_in_frame(wasm_module_t* mod,
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
                    if (rt->sp + label->caught_count > rt->max_stack) return WASM_ERR_STACK_OVERFLOW;
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

#define WASM__PUSH(rt, val)                \
    do {                                   \
        if ((rt)->sp >= (rt)->max_stack) { \
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

static wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results);

static wasm_error_t wasm__invoke_host_func(wasm_module_t* mod,
                                           wasm_host_func_t host_func,
                                           const wasm_value_t* args,
                                           uint32_t num_args,
                                           wasm_value_t* results,
                                           uint32_t num_results,
                                           void* userdata) {
    if (!mod || !mod->rt || !host_func) return WASM_ERR_MALFORMED;
    return host_func(mod, args, num_args, results, num_results, userdata);
}

static void wasm__init_call_frame(wasm__call_frame_t* cf,
                                  wasm_runtime_t* rt,
                                  uint32_t func_idx,
                                  wasm_func_t* func,
                                  wasm_functype_t* ft,
                                  wasm_value_t* locals,
                                  wasm__label_t* labels,
                                  wasm_value_t* results_dst,
                                  uint32_t num_results) {
    memset(cf, 0, sizeof(*cf));
    cf->func_idx = func_idx;
    cf->sp_base = rt->sp;
    cf->locals = locals;
    cf->labels = labels;
    cf->lsp = 0;
    cf->max_labels = func->max_label_depth ? func->max_label_depth : 1;
    cf->r.ptr = func->code;
    cf->r.end = func->code + func->code_len;
    cf->r.malformed = 0;
    cf->func = func;
    cf->ft = ft;
    cf->results_dst = results_dst;
    cf->num_results = num_results;
    cf->arena_saved = rt->frame_arena_offset;
    cf->current_offset = 0;

    labels[cf->lsp].continuation = cf->r.end;
    labels[cf->lsp].handler_ptr = NULL;
    labels[cf->lsp].sp_base = cf->sp_base;
    labels[cf->lsp].param_arity = 0;
    labels[cf->lsp].arity = ft->num_results;
    labels[cf->lsp].caught_tag = UINT32_MAX;
    labels[cf->lsp].caught_count = 0;
    labels[cf->lsp].caught_values = NULL;
    labels[cf->lsp].delegate_depth = UINT32_MAX;
    labels[cf->lsp].opcode = 0x02;
    labels[cf->lsp].active_catch = 0;
    cf->lsp++;
}

static wasm_error_t wasm__interp_loop(wasm_module_t* mod,
                                      wasm__call_frame_t* cf,
                                      uint32_t* next_func_idx,
                                      wasm_value_t** next_args,
                                      uint32_t* next_num_args,
                                      int* next_args_owned,
                                      wasm_value_t* tail_args_buf,
                                      uint32_t base_frame_sp);

static wasm_error_t wasm__interp_loop(wasm_module_t* mod,
                                      wasm__call_frame_t* cf,
                                      uint32_t* next_func_idx,
                                      wasm_value_t** next_args,
                                      uint32_t* next_num_args,
                                      int* next_args_owned,
                                      wasm_value_t* tail_args_buf,
                                      uint32_t base_frame_sp) {
    wasm_runtime_t* rt = mod->rt;
    wasm_func_t* func;
    uint32_t sp_base;
    uint32_t i;
    wasm_error_t err = WASM_OK;

    if (!cf) return WASM_ERR_MALFORMED;
    func = cf->func;
    sp_base = cf->sp_base;

    while (cf->r.ptr < cf->r.end) {
        if (rt->fuel_enabled) {
            if (rt->fuel == 0) WASM__TRAP(WASM_ERR_OUT_OF_FUEL);
            rt->fuel--;
        }

        const uint8_t* op_at = cf->r.ptr;
        uint8_t op = wasm__read_u8(&cf->r);

        cf->current_offset = (uint32_t)(op_at - func->code);
        switch (op) {
            case 0x00:
                WASM__TRAP(WASM_ERR_UNREACHABLE);
            case 0x01:
                break;

            case 0x02: { /* block */
                wasm__blocktype_t blocktype;
                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                {
                    const wasm__control_target_t* target = wasm__control_target_at(func, op_at);
                    const uint8_t* be;

                    if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                    be = func->code + target->end_offset;
                    if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                    if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                    cf->labels[cf->lsp].continuation = be;
                    cf->labels[cf->lsp].handler_ptr = NULL;
                    cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                    cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                    cf->labels[cf->lsp].arity = blocktype.result_arity;
                    cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                    cf->labels[cf->lsp].caught_count = 0;
                    cf->labels[cf->lsp].caught_values = NULL;
                    cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                    cf->labels[cf->lsp].opcode = 0x02;
                    cf->labels[cf->lsp].active_catch = 0;
                    cf->lsp++;
                }
                break;
            }
            case 0x03: { /* loop */
                wasm__blocktype_t blocktype;
                const uint8_t* ls;
                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                ls = cf->r.ptr;
                if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                cf->labels[cf->lsp].continuation = ls;
                cf->labels[cf->lsp].handler_ptr = NULL;
                cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                cf->labels[cf->lsp].arity = blocktype.result_arity;
                cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                cf->labels[cf->lsp].caught_count = 0;
                cf->labels[cf->lsp].caught_values = NULL;
                cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                cf->labels[cf->lsp].opcode = 0x03;
                cf->labels[cf->lsp].active_catch = 0;
                cf->lsp++;
                break;
            }
            case 0x04: { /* if */
                wasm__blocktype_t blocktype;
                wasm_value_t cond = WASM__POP(rt);
                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
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
                            cf->r.ptr = be;
                            break;
                        }
                        cf->r.ptr = ep;
                    }
                    if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                    cf->labels[cf->lsp].continuation = be;
                    cf->labels[cf->lsp].handler_ptr = NULL;
                    cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                    cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                    cf->labels[cf->lsp].arity = blocktype.result_arity;
                    cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                    cf->labels[cf->lsp].caught_count = 0;
                    cf->labels[cf->lsp].caught_values = NULL;
                    cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                    cf->labels[cf->lsp].opcode = 0x04;
                    cf->labels[cf->lsp].active_catch = 0;
                    cf->lsp++;
                }
                break;
            }
            case 0x06: { /* try */
                wasm__blocktype_t blocktype;
                const wasm__control_target_t* target;
                const uint8_t* be;
                const uint8_t* first_clause;

                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                target = wasm__control_target_at(func, op_at);
                if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                be = func->code + target->end_offset;
                first_clause = (target->aux_offset == UINT32_MAX) ? NULL : (func->code + target->aux_offset);
                if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                cf->labels[cf->lsp].continuation = be;
                cf->labels[cf->lsp].handler_ptr = first_clause;
                cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                cf->labels[cf->lsp].arity = blocktype.result_arity;
                cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                cf->labels[cf->lsp].caught_count = 0;
                cf->labels[cf->lsp].caught_values = NULL;
                cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                cf->labels[cf->lsp].opcode = 0x06;
                cf->labels[cf->lsp].active_catch = 0;
                cf->lsp++;
                break;
            }
            case 0x05:
                if (cf->lsp > 0) {
                    err = wasm__leave_label(rt, &cf->labels[cf->lsp - 1]);
                    if (err != WASM_OK) goto cleanup;
                    wasm__clear_label(&cf->labels[cf->lsp - 1]);
                    cf->r.ptr = cf->labels[cf->lsp - 1].continuation;
                    cf->lsp--;
                }
                break;
            case 0x07:
            case 0x18:
            case 0x19:
                if (cf->lsp > 0 && cf->labels[cf->lsp - 1].opcode == 0x06) {
                    err = wasm__leave_label(rt, &cf->labels[cf->lsp - 1]);
                    if (err != WASM_OK) goto cleanup;
                    wasm__clear_label(&cf->labels[cf->lsp - 1]);
                    cf->r.ptr = cf->labels[cf->lsp - 1].continuation;
                    cf->lsp--;
                } else {
                    err = WASM_ERR_MALFORMED;
                    goto cleanup;
                }
                break;
            case 0x08: { /* throw */
                uint32_t tag_index = wasm__read_leb128_u32(&cf->r);
                wasm_functype_t* tag_type;
                wasm_value_t payload_buf[8];
                wasm_value_t* payload = payload_buf;
                int j;

                if (tag_index >= mod->num_tags) WASM__TRAP(WASM_ERR_MALFORMED);
                tag_type = wasm__module_functype(mod, mod->tags[tag_index].type_idx);
                if (!tag_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if (tag_type->num_params > 8) {
                    payload = (wasm_value_t*)WASM_MALLOC(tag_type->num_params * sizeof(wasm_value_t));
                    if (!payload) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)tag_type->num_params - 1; j >= 0; j--) payload[j] = WASM__POP(rt);
                err = wasm__set_exception_state(&rt->pending_exception, tag_index, payload, tag_type->num_params);
                if (payload != payload_buf) WASM_FREE(payload);
                if (err != WASM_OK) goto cleanup;
                err = wasm__handle_exception_frames(mod, base_frame_sp);
                if (err == WASM_OK) return WASM__ERR_RESUME_FRAME;
                if (err == WASM__ERR_EXCEPTION) return err;
                goto cleanup;
            }
            case 0x09: { /* rethrow */
                uint32_t depth_index = wasm__read_leb128_u32(&cf->r);
                uint32_t target_index;

                if (!wasm__find_rethrow_label(cf->labels, cf->lsp, depth_index, &target_index)) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__set_exception_state(&rt->pending_exception,
                                                cf->labels[target_index].caught_tag,
                                                cf->labels[target_index].caught_values,
                                                cf->labels[target_index].caught_count);
                if (err != WASM_OK) goto cleanup;
                while (cf->lsp > target_index) {
                    wasm__clear_label(&cf->labels[cf->lsp - 1]);
                    rt->sp = cf->labels[cf->lsp - 1].sp_base;
                    cf->lsp--;
                }
                err = wasm__handle_exception_frames(mod, base_frame_sp);
                if (err == WASM_OK) return WASM__ERR_RESUME_FRAME;
                if (err == WASM__ERR_EXCEPTION) return err;
                goto cleanup;
            }
            case 0x0B:
                if (cf->lsp > 0) {
                    err = wasm__leave_label(rt, &cf->labels[cf->lsp - 1]);
                    if (err != WASM_OK) goto cleanup;
                    wasm__clear_label(&cf->labels[cf->lsp - 1]);
                    cf->lsp--;
                }
                break;

            case 0x0C: {
                uint32_t d = wasm__read_leb128_u32(&cf->r);
                if (d >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__do_branch(rt, cf->labels, &cf->labels[cf->lsp - 1 - d], &cf->r, &cf->lsp, d);
                if (err != WASM_OK) goto cleanup;
                break;
            }
            case 0x0D: {
                uint32_t d = wasm__read_leb128_u32(&cf->r);
                wasm_value_t c = WASM__POP(rt);
                if (c.of.i32) {
                    if (d >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                    err = wasm__do_branch(rt, cf->labels, &cf->labels[cf->lsp - 1 - d], &cf->r, &cf->lsp, d);
                    if (err != WASM_OK) goto cleanup;
                }
                break;
            }
            case 0x0E: { /* br_table */
                uint32_t nt = wasm__read_leb128_u32(&cf->r), j, d;
                uint32_t* tgts = (uint32_t*)WASM_MALLOC((nt + 1) * sizeof(uint32_t));
                wasm_value_t iv;
                if (!tgts) WASM__TRAP(WASM_ERR_OOM);
                for (j = 0; j <= nt; j++) tgts[j] = wasm__read_leb128_u32(&cf->r);
                iv = WASM__POP(rt);
                d = ((uint32_t)iv.of.i32 < nt) ? tgts[iv.of.i32] : tgts[nt];
                WASM_FREE(tgts);
                if (d >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__do_branch(rt, cf->labels, &cf->labels[cf->lsp - 1 - d], &cf->r, &cf->lsp, d);
                if (err != WASM_OK) goto cleanup;
                break;
            }
            case 0x0F: /* return */
                err = WASM__ERR_RETURN_FRAME;
                goto cleanup_frame;

            case 0x10: { /* call */
                uint32_t ci = wasm__read_leb128_u32(&cf->r);
                wasm_functype_t* cft;
                wasm_value_t* call_args = tail_args_buf;
                wasm_value_t call_results_buf[8];
                wasm_value_t* call_results = call_results_buf;
                wasm_func_t* callee;
                int j;

                if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &mod->funcs[ci];
                cft = wasm__module_functype(mod, mod->funcs[ci].type_idx);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);
                if (cft->num_params > 8) {
                    call_args = (wasm_value_t*)WASM_MALLOC(cft->num_params * sizeof(wasm_value_t));
                    if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        call_results = (wasm_value_t*)WASM_MALLOC(cft->num_results * sizeof(wasm_value_t));
                        if (!call_results) {
                            if (call_args != tail_args_buf) WASM_FREE(call_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                    err = wasm__invoke_host_func(mod, callee->host_func,
                                                 call_args, cft->num_params,
                                                 call_results, cft->num_results,
                                                 callee->host_userdata);
                    if (call_args != tail_args_buf) WASM_FREE(call_args);
                    if (err != WASM_OK) {
                        if (call_results != call_results_buf) WASM_FREE(call_results);
                        goto cleanup;
                    }
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                    if (call_results != call_results_buf) WASM_FREE(call_results);
                    break;
                }

                *next_func_idx = ci;
                *next_args = call_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (call_args != tail_args_buf);
                return WASM__ERR_CALL_FRAME;
                break;
            }
            case 0x11: { /* call_indirect */
                uint32_t ti = wasm__read_leb128_u32(&cf->r), table_idx, tvi, ci;
                wasm_functype_t* cft;
                wasm_func_t* callee;
                wasm_table_t* table;
                wasm_value_t call_results_buf[8], iv;
                wasm_value_t* call_args = tail_args_buf;
                wasm_value_t* call_results = call_results_buf;
                int j;
                table_idx = wasm__read_leb128_u32(&cf->r);
                iv = WASM__POP(rt);
                tvi = (uint32_t)iv.of.i32;
                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                if (table->reftype != WASM_TYPE_FUNCREF) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (tvi >= table->size || !table->elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                if (wasm__is_null_ref(&table->elems[tvi])) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                ci = table->elems[tvi].of.funcref;
                if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &mod->funcs[ci];
                cft = wasm__module_functype(mod, ti);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);
                if (!wasm__type_equal(mod, callee->type_idx, ti))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (cft->num_params > 8) {
                    call_args = (wasm_value_t*)WASM_MALLOC(cft->num_params * sizeof(wasm_value_t));
                    if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        call_results = (wasm_value_t*)WASM_MALLOC(cft->num_results * sizeof(wasm_value_t));
                        if (!call_results) {
                            if (call_args != tail_args_buf) WASM_FREE(call_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                    err = wasm__invoke_host_func(mod, callee->host_func,
                                                 call_args, cft->num_params,
                                                 call_results, cft->num_results,
                                                 callee->host_userdata);
                    if (call_args != tail_args_buf) WASM_FREE(call_args);
                    if (err != WASM_OK) {
                        if (call_results != call_results_buf) WASM_FREE(call_results);
                        goto cleanup;
                    }
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                    if (call_results != call_results_buf) WASM_FREE(call_results);
                    break;
                }

                *next_func_idx = ci;
                *next_args = call_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (call_args != tail_args_buf);
                return WASM__ERR_CALL_FRAME;
                break;
            }
            case 0x12: { /* return_call */
                uint32_t ci = wasm__read_leb128_u32(&cf->r);
                wasm_functype_t* cft;
                wasm_value_t* tail_args = tail_args_buf;
                wasm_value_t tail_results_buf[8];
                wasm_value_t* tail_results = tail_results_buf;
                wasm_func_t* callee;
                int j;

                if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &mod->funcs[ci];
                cft = wasm__module_functype(mod, mod->funcs[ci].type_idx);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);
                if (cft->num_params > 8) {
                    tail_args = (wasm_value_t*)WASM_MALLOC(cft->num_params * sizeof(wasm_value_t));
                    if (!tail_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) tail_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        tail_results = (wasm_value_t*)WASM_MALLOC(cft->num_results * sizeof(wasm_value_t));
                        if (!tail_results) {
                            if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(tail_results, 0, cft->num_results * sizeof(wasm_value_t));
                    err = wasm__invoke_host_func(mod, callee->host_func,
                                                 tail_args, cft->num_params,
                                                 tail_results, cft->num_results,
                                                 callee->host_userdata);
                    if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                    if (err != WASM_OK) {
                        if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                        goto cleanup;
                    }

                    rt->sp = sp_base;
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, tail_results[i]);
                    if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                    err = WASM__ERR_RETURN_FRAME;
                    goto cleanup_frame;
                }

                rt->sp = sp_base;
                *next_func_idx = ci;
                *next_args = tail_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (tail_args != tail_args_buf);
                err = WASM__ERR_TAIL_CALL_FRAME;
                goto cleanup_frame;
            }
            case 0x13: { /* return_call_indirect */
                uint32_t ti = wasm__read_leb128_u32(&cf->r), table_idx, tvi, ci;
                wasm_functype_t* cft;
                wasm_table_t* table;
                wasm_value_t* tail_args = tail_args_buf;
                wasm_value_t tail_results_buf[8];
                wasm_value_t* tail_results = tail_results_buf;
                wasm_value_t iv;
                wasm_func_t* callee;
                int j;

                table_idx = wasm__read_leb128_u32(&cf->r);
                iv = WASM__POP(rt);
                tvi = (uint32_t)iv.of.i32;
                if (ti >= mod->num_types || table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                if (table->reftype != WASM_TYPE_FUNCREF) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (tvi >= table->size || !table->elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                if (wasm__is_null_ref(&table->elems[tvi])) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                ci = table->elems[tvi].of.funcref;
                if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &mod->funcs[ci];
                cft = wasm__module_functype(mod, ti);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);
                if (!wasm__type_equal(mod, callee->type_idx, ti))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);

                if (cft->num_params > 8) {
                    tail_args = (wasm_value_t*)WASM_MALLOC(cft->num_params * sizeof(wasm_value_t));
                    if (!tail_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) tail_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        tail_results = (wasm_value_t*)WASM_MALLOC(cft->num_results * sizeof(wasm_value_t));
                        if (!tail_results) {
                            if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(tail_results, 0, cft->num_results * sizeof(wasm_value_t));
                    err = wasm__invoke_host_func(mod, callee->host_func,
                                                 tail_args, cft->num_params,
                                                 tail_results, cft->num_results,
                                                 callee->host_userdata);
                    if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                    if (err != WASM_OK) {
                        if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                        goto cleanup;
                    }

                    rt->sp = sp_base;
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, tail_results[i]);
                    if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                    err = WASM__ERR_RETURN_FRAME;
                    goto cleanup_frame;
                }

                rt->sp = sp_base;
                *next_func_idx = ci;
                *next_args = tail_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (tail_args != tail_args_buf);
                err = WASM__ERR_TAIL_CALL_FRAME;
                goto cleanup_frame;
            }

            case 0xFB: {
                uint32_t subop = wasm__read_leb128_u32(&cf->r);

                switch (subop) {
                    case 0x00:
                    case 0x01: {
                        uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
                        const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
                        wasm_gc_struct_t* object;

                        if (!st) WASM__TRAP(WASM_ERR_MALFORMED);
                        object = wasm__gc_alloc_struct_object(rt, mod, type_idx, st);
                        if (!object) WASM__TRAP(WASM_ERR_OOM);

                        if (subop == 0x00) {
                            for (i = st->num_fields; i > 0; i--) {
                                err = wasm__gc_struct_set_field(object, st, i - 1, WASM__POP(rt));
                                if (err != WASM_OK) goto cleanup;
                            }
                        } else {
                            for (i = 0; i < st->num_fields; i++) {
                                err = wasm__gc_struct_set_field(object, st, i,
                                                                wasm__gc_field_default_value(&st->fields[i]));
                                if (err != WASM_OK) goto cleanup;
                            }
                        }

                        WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                        break;
                    }
                    case 0x02:
                    case 0x03:
                    case 0x04:
                    case 0x05: {
                        uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
                        uint32_t field_idx = wasm__read_leb128_u32(&cf->r);
                        const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
                        wasm_reftype_t struct_ref;
                        wasm_value_t ref_value;
                        const wasm_gc_header_t* header;
                        wasm_gc_struct_t* object;

                        if (!st || field_idx >= st->num_fields) WASM__TRAP(WASM_ERR_MALFORMED);
                        err = wasm__runtime_make_typeidx_reftype(mod, type_idx, 1, &struct_ref);
                        if (err != WASM_OK) WASM__TRAP(WASM_ERR_MALFORMED);

                        if (subop == 0x05) {
                            wasm_value_t stored = WASM__POP(rt);

                            ref_value = WASM__POP(rt);
                            if (!wasm__runtime_value_matches_type(mod, &ref_value, struct_ref.type, &struct_ref))
                                WASM__TRAP(WASM_ERR_TRAP);
                            header = wasm__runtime_get_gc_object(mod, &ref_value);
                            if (!header || header->kind != WASM_GC_OBJ_STRUCT) WASM__TRAP(WASM_ERR_TRAP);
                            object = (wasm_gc_struct_t*)header;
                            err = wasm__gc_struct_set_field(object, st, field_idx, stored);
                            if (err != WASM_OK) goto cleanup;
                            break;
                        }

                        ref_value = WASM__POP(rt);
                        if (!wasm__runtime_value_matches_type(mod, &ref_value, struct_ref.type, &struct_ref))
                            WASM__TRAP(WASM_ERR_TRAP);
                        header = wasm__runtime_get_gc_object(mod, &ref_value);
                        if (!header || header->kind != WASM_GC_OBJ_STRUCT) WASM__TRAP(WASM_ERR_TRAP);
                        object = (wasm_gc_struct_t*)header;
                        err = wasm__gc_struct_get_field(object,
                                                        st,
                                                        field_idx,
                                                        subop == 0x03,
                                                        &ref_value);
                        if (err != WASM_OK) goto cleanup;
                        WASM__PUSH(rt, ref_value);
                        break;
                    }
                    case 0x06:
                    case 0x07:
                    case 0x08:
                    case 0x09:
                    case 0x0A: {
                        uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
                        const wasm_arraytype_t* atype = wasm__module_const_arraytype(mod, type_idx);
                        wasm_gc_array_t* object;
                        uint32_t count;

                        if (!atype) WASM__TRAP(WASM_ERR_MALFORMED);

                        if (subop == 0x08) {
                            count = wasm__read_leb128_u32(&cf->r);
                            object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                            if (!object) WASM__TRAP(WASM_ERR_OOM);
                            for (i = count; i > 0; i--) {
                                err = wasm__gc_array_set(object, atype, i - 1, WASM__POP(rt));
                                if (err != WASM_OK) goto cleanup;
                            }
                            WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                            break;
                        }

                        if (subop == 0x06 || subop == 0x07) {
                            wasm_value_t length_value;

                            if (subop == 0x06) {
                                wasm_value_t init_value = WASM__POP(rt);
                                length_value = WASM__POP(rt);
                                if (length_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                count = (uint32_t)length_value.of.i32;
                                object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                                if (!object) WASM__TRAP(WASM_ERR_OOM);
                                for (i = 0; i < count; i++) {
                                    err = wasm__gc_array_set(object, atype, i, init_value);
                                    if (err != WASM_OK) goto cleanup;
                                }
                            } else {
                                length_value = WASM__POP(rt);
                                if (length_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                                count = (uint32_t)length_value.of.i32;
                                object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                                if (!object) WASM__TRAP(WASM_ERR_OOM);
                                for (i = 0; i < count; i++) {
                                    err = wasm__gc_array_set(object, atype, i,
                                                             wasm__gc_field_default_value(&atype->field));
                                    if (err != WASM_OK) goto cleanup;
                                }
                            }

                            WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                            break;
                        }

                        if (subop == 0x09) {
                            uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
                            wasm_value_t size_value = WASM__POP(rt);
                            wasm_value_t offset_value = WASM__POP(rt);
                            wasm_data_segment_t* segment;
                            size_t elem_size;
                            uint64_t byte_count;
                            uint64_t src_offset;

                            if (data_idx >= mod->num_data_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                            if (size_value.of.i32 < 0 || offset_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (!wasm__gc_array_data_element_size(atype, &elem_size)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                            count = (uint32_t)size_value.of.i32;
                            segment = &mod->data_segments[data_idx];
                            if (!segment->is_passive || segment->is_dropped) WASM__TRAP(WASM_ERR_TRAP);
                            if (count != 0 && elem_size > UINT64_MAX / count) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            byte_count = (uint64_t)elem_size * (uint64_t)count;
                            src_offset = (uint64_t)(uint32_t)offset_value.of.i32;
                            if (!wasm__range_in_bounds_u64(src_offset, byte_count, segment->size))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                            object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                            if (!object) WASM__TRAP(WASM_ERR_OOM);
                            for (i = 0; i < count; i++) {
                                wasm_value_t elem_value;

                                err = wasm__gc_array_read_data_value(
                                    atype,
                                    segment->bytes + src_offset + ((uint64_t)i * (uint64_t)elem_size),
                                    &elem_value);
                                if (err != WASM_OK) goto cleanup;
                                err = wasm__gc_array_set(object, atype, i, elem_value);
                                if (err != WASM_OK) goto cleanup;
                            }

                            WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                            break;
                        }

                        {
                            uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
                            wasm_value_t size_value = WASM__POP(rt);
                            wasm_value_t offset_value = WASM__POP(rt);
                            wasm_elem_segment_t* segment;
                            uint32_t src_offset;

                            if (elem_idx >= mod->num_elem_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                            if (size_value.of.i32 < 0 || offset_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            count = (uint32_t)size_value.of.i32;
                            src_offset = (uint32_t)offset_value.of.i32;
                            segment = &mod->elem_segments[elem_idx];
                            if (!segment->is_passive || segment->is_dropped) WASM__TRAP(WASM_ERR_TRAP);
                            if (!wasm__range_in_bounds_u64(src_offset, count, segment->num_elems))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                            object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                            if (!object) WASM__TRAP(WASM_ERR_OOM);
                            for (i = 0; i < count; i++) {
                                err = wasm__gc_array_set(object, atype, i, segment->elems[src_offset + i]);
                                if (err != WASM_OK) goto cleanup;
                            }

                            WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                            break;
                        }
                    }
                    case 0x0B:
                    case 0x0C:
                    case 0x0D:
                    case 0x0E:
                    case 0x0F:
                    case 0x10:
                    case 0x11:
                    case 0x12:
                    case 0x13: {
                        uint32_t type_idx = subop == 0x0F ? UINT32_MAX : wasm__read_leb128_u32(&cf->r);
                        const wasm_arraytype_t* atype = subop == 0x0F ? NULL : wasm__module_const_arraytype(mod, type_idx);
                        wasm_reftype_t array_ref;
                        wasm_value_t ref_value;
                        const wasm_gc_header_t* header;
                        wasm_gc_array_t* object;

                        if (subop != 0x0F && !atype) WASM__TRAP(WASM_ERR_MALFORMED);
                        wasm__clear_reftype(&array_ref);
                        array_ref.type = WASM_TYPE_ARRAYREF;
                        array_ref.nullable = 1;
                        if (subop != 0x0F) {
                            err = wasm__runtime_make_typeidx_reftype(mod, type_idx, 1, &array_ref);
                            if (err != WASM_OK) WASM__TRAP(WASM_ERR_MALFORMED);
                        }

                        if (subop == 0x0B || subop == 0x0C || subop == 0x0D) {
                            wasm_value_t index_value = WASM__POP(rt);

                            ref_value = WASM__POP(rt);
                            if (index_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                                WASM__TRAP(WASM_ERR_TRAP);
                            header = wasm__runtime_get_gc_object(mod, &ref_value);
                            if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                            object = (wasm_gc_array_t*)header;
                            err = wasm__gc_array_get(object,
                                                     atype,
                                                     (uint32_t)index_value.of.i32,
                                                     subop == 0x0C,
                                                     &ref_value);
                            if (err != WASM_OK) goto cleanup;
                            WASM__PUSH(rt, ref_value);
                            break;
                        }

                        if (subop == 0x0E) {
                            wasm_value_t stored = WASM__POP(rt);
                            wasm_value_t index_value = WASM__POP(rt);

                            ref_value = WASM__POP(rt);
                            if (index_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                                WASM__TRAP(WASM_ERR_TRAP);
                            header = wasm__runtime_get_gc_object(mod, &ref_value);
                            if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                            object = (wasm_gc_array_t*)header;
                            err = wasm__gc_array_set(object, atype, (uint32_t)index_value.of.i32, stored);
                            if (err != WASM_OK) goto cleanup;
                            break;
                        }

                        if (subop == 0x0F) {
                            ref_value = WASM__POP(rt);
                            if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                                WASM__TRAP(WASM_ERR_TRAP);
                            header = wasm__runtime_get_gc_object(mod, &ref_value);
                            if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                            WASM__PUSH(rt, wasm_i32((int32_t)wasm__gc_array_len((const wasm_gc_array_t*)header)));
                            break;
                        }

                        if (subop == 0x10) {
                            wasm_value_t count_value = WASM__POP(rt);
                            wasm_value_t fill_value = WASM__POP(rt);
                            wasm_value_t dst_value = WASM__POP(rt);
                            uint32_t count;
                            uint32_t dst_index;

                            ref_value = WASM__POP(rt);
                            if (count_value.of.i32 < 0 || dst_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                                WASM__TRAP(WASM_ERR_TRAP);
                            header = wasm__runtime_get_gc_object(mod, &ref_value);
                            if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                            object = (wasm_gc_array_t*)header;
                            count = (uint32_t)count_value.of.i32;
                            dst_index = (uint32_t)dst_value.of.i32;
                            if (!wasm__range_in_bounds_u64(dst_index, count, object->length))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            for (i = 0; i < count; i++) {
                                err = wasm__gc_array_set(object, atype, dst_index + i, fill_value);
                                if (err != WASM_OK) goto cleanup;
                            }
                            break;
                        }

                        if (subop == 0x11) {
                            uint32_t src_type_idx = wasm__read_leb128_u32(&cf->r);
                            const wasm_arraytype_t* src_type = wasm__module_const_arraytype(mod, src_type_idx);
                            wasm_reftype_t src_ref;
                            wasm_value_t count_value = WASM__POP(rt);
                            wasm_value_t src_offset_value = WASM__POP(rt);
                            wasm_value_t src_value = WASM__POP(rt);
                            wasm_value_t dst_offset_value = WASM__POP(rt);
                            uint32_t count;
                            uint32_t src_index;
                            uint32_t dst_index;
                            const wasm_gc_header_t* src_header;
                            const wasm_gc_array_t* src_object;

                            ref_value = WASM__POP(rt);
                            if (!src_type) WASM__TRAP(WASM_ERR_MALFORMED);
                            err = wasm__runtime_make_typeidx_reftype(mod, src_type_idx, 1, &src_ref);
                            if (err != WASM_OK) WASM__TRAP(WASM_ERR_MALFORMED);
                            if (count_value.of.i32 < 0 || src_offset_value.of.i32 < 0 || dst_offset_value.of.i32 < 0)
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref) ||
                                !wasm__runtime_value_matches_type(mod, &src_value, src_ref.type, &src_ref))
                                WASM__TRAP(WASM_ERR_TRAP);
                            header = wasm__runtime_get_gc_object(mod, &ref_value);
                            src_header = wasm__runtime_get_gc_object(mod, &src_value);
                            if (!header || header->kind != WASM_GC_OBJ_ARRAY ||
                                !src_header || src_header->kind != WASM_GC_OBJ_ARRAY)
                                WASM__TRAP(WASM_ERR_TRAP);

                            object = (wasm_gc_array_t*)header;
                            src_object = (const wasm_gc_array_t*)src_header;
                            count = (uint32_t)count_value.of.i32;
                            src_index = (uint32_t)src_offset_value.of.i32;
                            dst_index = (uint32_t)dst_offset_value.of.i32;
                            if (!wasm__range_in_bounds_u64(src_index, count, src_object->length) ||
                                !wasm__range_in_bounds_u64(dst_index, count, object->length))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                            if (count > 0 && object == src_object && dst_index > src_index) {
                                for (i = count; i > 0; i--) {
                                    wasm_value_t copied;

                                    err = wasm__gc_array_get(src_object, src_type, src_index + i - 1, 0, &copied);
                                    if (err != WASM_OK) goto cleanup;
                                    err = wasm__gc_array_set(object, atype, dst_index + i - 1, copied);
                                    if (err != WASM_OK) goto cleanup;
                                }
                            } else {
                                for (i = 0; i < count; i++) {
                                    wasm_value_t copied;

                                    err = wasm__gc_array_get(src_object, src_type, src_index + i, 0, &copied);
                                    if (err != WASM_OK) goto cleanup;
                                    err = wasm__gc_array_set(object, atype, dst_index + i, copied);
                                    if (err != WASM_OK) goto cleanup;
                                }
                            }
                            break;
                        }

                        if (subop == 0x12) {
                            uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
                            wasm_value_t count_value = WASM__POP(rt);
                            wasm_value_t src_offset_value = WASM__POP(rt);
                            wasm_value_t dst_offset_value = WASM__POP(rt);
                            wasm_data_segment_t* segment;
                            size_t elem_size;
                            uint64_t byte_count;
                            uint64_t src_offset;
                            uint32_t dst_index;
                            uint32_t count;

                            ref_value = WASM__POP(rt);
                            if (data_idx >= mod->num_data_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                            if (count_value.of.i32 < 0 || src_offset_value.of.i32 < 0 || dst_offset_value.of.i32 < 0)
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                                WASM__TRAP(WASM_ERR_TRAP);
                            header = wasm__runtime_get_gc_object(mod, &ref_value);
                            if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                            object = (wasm_gc_array_t*)header;
                            segment = &mod->data_segments[data_idx];
                            if (!segment->is_passive || segment->is_dropped) WASM__TRAP(WASM_ERR_TRAP);
                            if (!wasm__gc_array_data_element_size(atype, &elem_size)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);

                            count = (uint32_t)count_value.of.i32;
                            src_offset = (uint64_t)(uint32_t)src_offset_value.of.i32;
                            dst_index = (uint32_t)dst_offset_value.of.i32;
                            if (count != 0 && elem_size > UINT64_MAX / count) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            byte_count = (uint64_t)elem_size * (uint64_t)count;
                            if (!wasm__range_in_bounds_u64(src_offset, byte_count, segment->size) ||
                                !wasm__range_in_bounds_u64(dst_index, count, object->length))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                            for (i = 0; i < count; i++) {
                                wasm_value_t elem_value;

                                err = wasm__gc_array_read_data_value(
                                    atype,
                                    segment->bytes + src_offset + ((uint64_t)i * (uint64_t)elem_size),
                                    &elem_value);
                                if (err != WASM_OK) goto cleanup;
                                err = wasm__gc_array_set(object, atype, dst_index + i, elem_value);
                                if (err != WASM_OK) goto cleanup;
                            }
                            break;
                        }

                        {
                            uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
                            wasm_value_t count_value = WASM__POP(rt);
                            wasm_value_t src_offset_value = WASM__POP(rt);
                            wasm_value_t dst_offset_value = WASM__POP(rt);
                            wasm_elem_segment_t* segment;
                            uint32_t count;
                            uint32_t src_index;
                            uint32_t dst_index;

                            ref_value = WASM__POP(rt);
                            if (elem_idx >= mod->num_elem_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                            if (count_value.of.i32 < 0 || src_offset_value.of.i32 < 0 || dst_offset_value.of.i32 < 0)
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                            if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                                WASM__TRAP(WASM_ERR_TRAP);
                            header = wasm__runtime_get_gc_object(mod, &ref_value);
                            if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                            object = (wasm_gc_array_t*)header;
                            segment = &mod->elem_segments[elem_idx];
                            if (!segment->is_passive || segment->is_dropped) WASM__TRAP(WASM_ERR_TRAP);
                            count = (uint32_t)count_value.of.i32;
                            src_index = (uint32_t)src_offset_value.of.i32;
                            dst_index = (uint32_t)dst_offset_value.of.i32;
                            if (!wasm__range_in_bounds_u64(src_index, count, segment->num_elems) ||
                                !wasm__range_in_bounds_u64(dst_index, count, object->length))
                                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                            for (i = 0; i < count; i++) {
                                err = wasm__gc_array_set(object, atype, dst_index + i, segment->elems[src_index + i]);
                                if (err != WASM_OK) goto cleanup;
                            }
                            break;
                        }
                    }
                    case 0x14:
                    case 0x15:
                    case 0x16:
                    case 0x17: {
                        wasm_reftype_t target_ref;
                        wasm_value_t value = WASM__POP(rt);
                        int matches;

                        err = wasm__runtime_read_heap_reftype(mod,
                                                              &cf->r,
                                                              subop == 0x15 || subop == 0x17,
                                                              &target_ref);
                        if (err != WASM_OK) goto cleanup;
                        matches = wasm__runtime_value_matches_type(mod, &value, target_ref.type, &target_ref);
                        if (subop == 0x14 || subop == 0x15) {
                            WASM__PUSH(rt, wasm_i32(matches ? 1 : 0));
                            break;
                        }
                        if (!matches) WASM__TRAP(WASM_ERR_TRAP);
                        WASM__PUSH(rt, wasm__runtime_cast_success_value(&target_ref, value));
                        break;
                    }
                    case 0x18:
                    case 0x19: {
                        uint8_t flags = wasm__read_u8(&cf->r);
                        uint32_t depth = wasm__read_leb128_u32(&cf->r);
                        wasm_reftype_t source_ref;
                        wasm_reftype_t target_ref;
                        wasm_value_t value;
                        int matches;

                        if ((flags & ~0x03u) != 0 || depth >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                        err = wasm__runtime_read_heap_reftype(mod, &cf->r, (flags & 0x01u) != 0, &source_ref);
                        if (err != WASM_OK) goto cleanup;
                        err = wasm__runtime_read_heap_reftype(mod, &cf->r, (flags & 0x02u) != 0, &target_ref);
                        if (err != WASM_OK) goto cleanup;

                        value = WASM__PEEK(rt);
                        matches = wasm__runtime_value_matches_type(mod, &value, target_ref.type, &target_ref);
                        if (matches) {
                            rt->stack[rt->sp - 1] = wasm__runtime_cast_success_value(&target_ref, value);
                            if (subop == 0x18) {
                                err = wasm__do_branch(rt,
                                                      cf->labels,
                                                      &cf->labels[cf->lsp - 1 - depth],
                                                      &cf->r,
                                                      &cf->lsp,
                                                      depth);
                                if (err != WASM_OK) goto cleanup;
                            }
                        } else {
                            rt->stack[rt->sp - 1] = wasm__runtime_cast_difference_value(&source_ref,
                                                                                        &target_ref,
                                                                                        value);
                            if (subop == 0x19) {
                                err = wasm__do_branch(rt,
                                                      cf->labels,
                                                      &cf->labels[cf->lsp - 1 - depth],
                                                      &cf->r,
                                                      &cf->lsp,
                                                      depth);
                                if (err != WASM_OK) goto cleanup;
                            }
                        }
                        break;
                    }
                    case 0x1A:
                    case 0x1B: {
                        wasm_value_t value = WASM__POP(rt);

                        value.type = subop == 0x1A ? WASM_TYPE_ANYREF : WASM_TYPE_EXTERNREF;
                        WASM__PUSH(rt, value);
                        break;
                    }
                    case 0x1C: {
                        wasm_value_t value = WASM__POP(rt);

                        WASM__PUSH(rt, wasm_i31ref(value.of.i32));
                        break;
                    }
                    case 0x1D:
                    case 0x1E: {
                        wasm_value_t value = WASM__POP(rt);

                        if (wasm__is_null_ref(&value)) WASM__TRAP(WASM_ERR_TRAP);
                        if ((value.of.gc_ref & WASM__GC_I31_TAG) == 0) WASM__TRAP(WASM_ERR_TRAP);
                        WASM__PUSH(rt, subop == 0x1D ? wasm_i32(wasm__gc_i31_get_s(value.of.gc_ref))
                                                     : wasm_i32((int32_t)wasm__gc_i31_get_u(value.of.gc_ref)));
                        break;
                    }
                    default:
                        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown GC opcode 0xFB 0x%X", (unsigned)subop);
                        err = WASM_ERR_MALFORMED;
                        goto cleanup;
                }
                break;
            }

            case 0xFC: {
                uint32_t subop = wasm__read_leb128_u32(&cf->r);
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
                        uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
                        uint32_t mem_idx = wasm__read_leb128_u32(&cf->r);
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
                        uint32_t data_idx = wasm__read_leb128_u32(&cf->r);

                        if (data_idx >= mod->num_data_segments) {
                            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown data segment %u", (unsigned)data_idx);
                            err = WASM_ERR_MALFORMED;
                            goto cleanup;
                        }

                        mod->data_segments[data_idx].is_dropped = 1;
                        break;
                    }
                    case 0x0A: {
                        uint32_t dst_mem_idx = wasm__read_leb128_u32(&cf->r);
                        uint32_t src_mem_idx = wasm__read_leb128_u32(&cf->r);
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
                        uint32_t mem_idx = wasm__read_leb128_u32(&cf->r);
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
                        uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
                        uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
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

                        table = wasm__table_at(mod, table_idx);
                        if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                        segment = &mod->elem_segments[elem_idx];
                        if (!segment->is_passive || segment->is_dropped) WASM__TRAP(WASM_ERR_TRAP);
                        if (!wasm__typed_valtype_is_subtype(mod,
                                                            segment->elem_type,
                                                            &segment->elem_ref_type,
                                                            table->reftype,
                                                            &table->reftype_info))
                            WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
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
                        uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);

                        if (elem_idx >= mod->num_elem_segments) {
                            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown elem segment %u", (unsigned)elem_idx);
                            err = WASM_ERR_MALFORMED;
                            goto cleanup;
                        }

                        mod->elem_segments[elem_idx].is_dropped = 1;
                        break;
                    }
                    case 0x0E: {
                        uint32_t dst_table_idx = wasm__read_leb128_u32(&cf->r);
                        uint32_t src_table_idx = wasm__read_leb128_u32(&cf->r);
                        wasm_value_t count = WASM__POP(rt);
                        wasm_value_t src = WASM__POP(rt);
                        wasm_value_t dst = WASM__POP(rt);
                        uint32_t len = (uint32_t)count.of.i32;
                        uint32_t src_offset = (uint32_t)src.of.i32;
                        uint32_t dst_offset = (uint32_t)dst.of.i32;
                        wasm_table_t* dst_table;
                        wasm_table_t* src_table;

                        if (dst_table_idx >= mod->num_tables || src_table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                        dst_table = wasm__table_at(mod, dst_table_idx);
                        src_table = wasm__table_at(mod, src_table_idx);
                        if (!dst_table || !src_table) WASM__TRAP(WASM_ERR_MALFORMED);
                        if (!wasm__typed_valtype_is_subtype(mod,
                                                            src_table->reftype,
                                                            &src_table->reftype_info,
                                                            dst_table->reftype,
                                                            &dst_table->reftype_info))
                            WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
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
                        uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
                        wasm_value_t delta_value = WASM__POP(rt);
                        wasm_value_t init_value = WASM__POP(rt);
                        wasm_table_t* table;
                        uint32_t old_size;
                        uint32_t delta;
                        uint32_t new_size;
                        wasm_value_t* new_elems;
                        uint32_t j;

                        if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                        table = wasm__table_at(mod, table_idx);
                        if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                        if (!wasm__runtime_value_matches_type(mod,
                                                              &init_value,
                                                              table->reftype,
                                                              &table->reftype_info))
                            WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
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
                        uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
                        wasm_table_t* table;

                        if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                        table = wasm__table_at(mod, table_idx);
                        if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                        WASM__PUSH(rt, wasm_i32((int32_t)table->size));
                        break;
                    }
                    case 0x11: {
                        uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
                        wasm_value_t count = WASM__POP(rt);
                        wasm_value_t value = WASM__POP(rt);
                        wasm_value_t dst = WASM__POP(rt);
                        wasm_table_t* table;
                        uint32_t len = (uint32_t)count.of.i32;
                        uint32_t dst_offset = (uint32_t)dst.of.i32;
                        uint32_t j;

                        if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                        table = wasm__table_at(mod, table_idx);
                        if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                        if (!wasm__runtime_value_matches_type(mod,
                                                              &value,
                                                              table->reftype,
                                                              &table->reftype_info))
                            WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
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
                uint32_t subop = wasm__read_leb128_u32(&cf->r);

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

                        err = wasm__read_memarg(&cf->r, &memarg);
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

                        err = wasm__read_memarg(&cf->r, &memarg);
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
                        if (!wasm__has(&cf->r, 16)) WASM__TRAP(WASM_ERR_MALFORMED);
                        memcpy(vector.of.v128, cf->r.ptr, 16);
                        cf->r.ptr += 16;
                        WASM__PUSH(rt, vector);
                        break;
                    }
                    case 0x0D: {
                        wasm_value_t rhs;
                        wasm_value_t lhs;
                        wasm_value_t vector;
                        uint8_t lanes[16];
                        uint32_t lane;

                        for (lane = 0; lane < 16; lane++) lanes[lane] = wasm__read_u8(&cf->r);
                        if (cf->r.malformed) WASM__TRAP(WASM_ERR_MALFORMED);
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
                        uint32_t lane = wasm__read_u8(&cf->r);
                        wasm_value_t vector = WASM__POP(rt);
                        if (cf->r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);

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
                        uint32_t lane = wasm__read_u8(&cf->r);
                        wasm_value_t scalar = WASM__POP(rt);
                        wasm_value_t vector = WASM__POP(rt);
                        if (cf->r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);

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

                        err = wasm__read_memarg(&cf->r, &memarg);
                        if (err != WASM_OK) goto cleanup;
                        lane = wasm__read_u8(&cf->r);
                        vector = WASM__POP(rt);
                        base = WASM__POP(rt);
                        if (cf->r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);
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

                        err = wasm__read_memarg(&cf->r, &memarg);
                        if (err != WASM_OK) goto cleanup;
                        lane = wasm__read_u8(&cf->r);
                        vector = WASM__POP(rt);
                        base = WASM__POP(rt);
                        if (cf->r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);
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

                err = wasm__read_typed_select_immediate(mod, &cf->r, &select_type, NULL);
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
                uint32_t x = wasm__read_leb128_u32(&cf->r);
                WASM__PUSH(rt, cf->locals[x]);
                break;
            }
            case 0x21: {
                uint32_t x = wasm__read_leb128_u32(&cf->r);
                cf->locals[x] = WASM__POP(rt);
                break;
            }
            case 0x22: {
                uint32_t x = wasm__read_leb128_u32(&cf->r);
                cf->locals[x] = WASM__PEEK(rt);
                break;
            }
            case 0x23: {
                uint32_t x = wasm__read_leb128_u32(&cf->r);
                WASM__PUSH(rt, wasm__global_get_value(&mod->globals[x]));
                break;
            }
            case 0x24: {
                uint32_t x = wasm__read_leb128_u32(&cf->r);
                wasm_value_t value;

                if (!mod->globals[x].is_mutable) {
                    WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "global %u is immutable", (unsigned)x);
                    err = WASM_ERR_TYPE_MISMATCH;
                    goto cleanup;
                }
                value = WASM__POP(rt);
                if (!wasm__runtime_value_matches_type(mod,
                                                      &value,
                                                      mod->globals[x].type,
                                                      &mod->globals[x].ref_type))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                wasm__global_set_value(&mod->globals[x], value);
                break;
            }
            case 0x25: {
                uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
                wasm_table_t* table;
                wasm_value_t index;
                uint32_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                index = WASM__POP(rt);
                element_index = (uint32_t)index.of.i32;
                if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__PUSH(rt, table->elems[element_index]);
                break;
            }
            case 0x26: {
                uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
                wasm_table_t* table;
                wasm_value_t value;
                wasm_value_t index;
                uint32_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                value = WASM__POP(rt);
                index = WASM__POP(rt);
                element_index = (uint32_t)index.of.i32;
                if (!wasm__runtime_value_matches_type(mod,
                                                      &value,
                                                      table->reftype,
                                                      &table->reftype_info))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
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

                err = wasm__read_memarg(&cf->r, &memarg);
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

                err = wasm__read_memarg(&cf->r, &memarg);
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
                uint32_t mem_idx = wasm__read_leb128_u32(&cf->r);
                wasm_memory_t* memory = wasm__memory_at(mod, mem_idx);

                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                WASM__PUSH(rt, wasm_i32((int32_t)memory->pages));
                break;
            }
            case 0x40: {
                wasm_value_t d;
                int32_t res;
                uint32_t mem_idx = wasm__read_leb128_u32(&cf->r);
                d = WASM__POP(rt);
                res = wasm_memory_grow_at(mod, mem_idx, (uint32_t)d.of.i32);
                WASM__PUSH(rt, wasm_i32(res));
                break;
            }

            /* Reference instructions */
            case 0xD0: {
                wasm_valtype_t type;
                wasm_reftype_t reftype = { 0 };
                err = wasm__read_heaptype(mod, &cf->r, &reftype);
                if (err != WASM_OK) goto cleanup;
                type = reftype.type ? reftype.type : WASM_TYPE_ANYREF;
                WASM__PUSH(rt, wasm__typed_ref_null(type, &reftype));
                break;
            }
            case 0xD1: {
                wasm_value_t value = WASM__POP(rt);
                WASM__PUSH(rt, wasm_i32(wasm__is_null_ref(&value) ? 1 : 0));
                break;
            }
            case 0xD2: {
                uint32_t func_index = wasm__read_leb128_u32(&cf->r);
                if (func_index >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                WASM__PUSH(rt, wasm_funcref(func_index));
                break;
            }
            case 0xD3: {
                wasm_value_t rhs = WASM__POP(rt);
                wasm_value_t lhs = WASM__POP(rt);
                int equal;

                if (!wasm__is_ref_type(lhs.type) || !wasm__is_ref_type(rhs.type)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (wasm__uses_funcref_storage(lhs.type) || wasm__uses_funcref_storage(rhs.type))
                    equal = wasm__uses_funcref_storage(lhs.type) && wasm__uses_funcref_storage(rhs.type) &&
                            lhs.of.funcref == rhs.of.funcref;
                else if (wasm__uses_externref_storage(lhs.type) || wasm__uses_externref_storage(rhs.type))
                    equal = wasm__uses_externref_storage(lhs.type) && wasm__uses_externref_storage(rhs.type) &&
                            lhs.of.externref == rhs.of.externref;
                else
                    equal = lhs.of.gc_ref == rhs.of.gc_ref;
                WASM__PUSH(rt, wasm_i32(equal ? 1 : 0));
                break;
            }

            /* Constants */
            case 0x41:
                WASM__PUSH(rt, wasm_i32(wasm__read_leb128_i32(&cf->r)));
                break;
            case 0x42:
                WASM__PUSH(rt, wasm_i64(wasm__read_leb128_i64(&cf->r)));
                break;
            case 0x43:
                WASM__PUSH(rt, wasm_f32(wasm__read_f32(&cf->r)));
                break;
            case 0x44:
                WASM__PUSH(rt, wasm_f64(wasm__read_f64(&cf->r)));
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

        if (cf->r.malformed) WASM__TRAP(WASM_ERR_MALFORMED);
    }

    err = WASM__ERR_RETURN_FRAME;

cleanup:
cleanup_frame:
    while (cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }

    return err;
}

static wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results) {
    wasm_runtime_t* rt = mod->rt;
    wasm_func_t* func;
    uint32_t base_frame_sp = rt->call_frame_sp;
    wasm_value_t request_args_buf[8];
    wasm_value_t* next_args = NULL;
    uint32_t next_func_idx = 0;
    uint32_t next_num_args = 0;
    int next_args_owned = 0;
    wasm_error_t err = WASM_OK;

    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import)
        return wasm__invoke_host_func(mod, func->host_func,
                                      args, num_args, results, num_results,
                                      func->host_userdata);

    err = wasm__push_call_frame(mod, func_idx, args, num_args, results, num_results);
    if (err != WASM_OK) goto cleanup;

    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];

        next_args = NULL;
        next_num_args = 0;
        next_args_owned = 0;
        next_func_idx = 0;

        err = wasm__interp_loop(mod, cf,
                                &next_func_idx, &next_args, &next_num_args,
                                &next_args_owned, request_args_buf, base_frame_sp);

        if (err == WASM__ERR_CALL_FRAME) {
            err = wasm__push_call_frame(mod, next_func_idx, next_args, next_num_args,
                                        NULL, 0);
            if (next_args_owned && next_args) {
                WASM_FREE(next_args);
                next_args = NULL;
                next_args_owned = 0;
            }
            if (err != WASM_OK) goto cleanup;
            continue;
        }

        if (err == WASM__ERR_TAIL_CALL_FRAME) {
            wasm_value_t* next_results_dst = cf->results_dst;
            uint32_t next_result_count = cf->num_results;

            wasm__arena_reset(rt, cf->arena_saved);
            memset(cf, 0, sizeof(*cf));
            rt->call_frame_sp--;

            err = wasm__push_call_frame(mod, next_func_idx, next_args, next_num_args,
                                        next_results_dst, next_result_count);
            if (next_args_owned && next_args) {
                WASM_FREE(next_args);
                next_args = NULL;
                next_args_owned = 0;
            }
            if (err != WASM_OK) goto cleanup;
            continue;
        }

        if (next_args_owned && next_args) {
            WASM_FREE(next_args);
            next_args = NULL;
            next_args_owned = 0;
        }

        if (err == WASM__ERR_RETURN_FRAME) {
            err = wasm__finish_call_frame(rt, base_frame_sp);
            if (err != WASM_OK) goto cleanup;
            continue;
        }

        if (err == WASM__ERR_RESUME_FRAME) continue;

        if (err == WASM__ERR_EXCEPTION) goto cleanup;

        if (err != WASM_OK) goto cleanup;
    }

cleanup:
    if (err != WASM_OK && rt->call_frame_sp > base_frame_sp)
        wasm__capture_backtrace(rt, base_frame_sp);
    if (next_args_owned && next_args) WASM_FREE(next_args);
    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm__clear_call_frame(cf);
        wasm__arena_reset(rt, cf->arena_saved);
        memset(cf, 0, sizeof(*cf));
        rt->call_frame_sp--;
    }
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

    if (mod && mod->rt) wasm__clear_backtrace(mod->rt);

    if (func_idx >= mod->num_funcs) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "function index %u out of range", (unsigned)func_idx);
        return WASM_ERR_MALFORMED;
    }

    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                      "function %u does not reference a function type",
                      (unsigned)func_idx);
        return WASM_ERR_MALFORMED;
    }
    if (num_args != ft->num_params || num_results < ft->num_results ||
        (ft->num_params > 0 && !args) || (ft->num_results > 0 && !results)) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                      "call signature mismatch: expected %u args/%u results, got %u args/%u results",
                      (unsigned)ft->num_params, (unsigned)ft->num_results,
                      (unsigned)num_args, (unsigned)num_results);
        return WASM_ERR_TYPE_MISMATCH;
    }

    {
        wasm_error_t err = wasm__interp(mod, func_idx, (wasm_value_t*)args, num_args, results, num_results);
        if (err == WASM__ERR_EXCEPTION) return wasm__report_uncaught_exception(mod->rt);
        return err;
    }
}

static wasm_error_t wasm__resolve_exported_func(wasm_module_t* mod,
                                                const char* name,
                                                uint32_t* out_index,
                                                const wasm_functype_t** out_type) {
    wasm_export_kind_t kind;
    uint32_t index;
    const wasm_functype_t* type;

    if (out_index) *out_index = 0;
    if (out_type) *out_type = NULL;

    if (!mod || !name) return WASM_ERR_MALFORMED;

    if (!wasm_find_export(mod, name, &kind, &index) || kind != WASM_EXPORT_FUNC) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_UNDEFINED_EXPORT,
                          "function export '%s' not found", name);
        return WASM_ERR_UNDEFINED_EXPORT;
    }
    if (index >= mod->num_funcs) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "function export '%s' index %u out of range",
                          name, (unsigned)index);
        return WASM_ERR_MALFORMED;
    }

    type = wasm__module_const_functype(mod, mod->funcs[index].type_idx);
    if (!type) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "function export '%s' has no valid function type", name);
        return WASM_ERR_MALFORMED;
    }

    if (out_index) *out_index = index;
    if (out_type) *out_type = type;
    return WASM_OK;
}

static wasm_error_t wasm__resolve_exported_global(wasm_module_t* mod,
                                                  const char* name,
                                                  wasm_global_t** out_global,
                                                  uint32_t* out_index) {
    wasm_export_kind_t kind;
    uint32_t index;

    if (out_global) *out_global = NULL;
    if (out_index) *out_index = 0;

    if (!mod || !name) return WASM_ERR_MALFORMED;

    if (!wasm_find_export(mod, name, &kind, &index) || kind != WASM_EXPORT_GLOBAL) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_UNDEFINED_EXPORT,
                          "global export '%s' not found", name);
        return WASM_ERR_UNDEFINED_EXPORT;
    }
    if (index >= mod->num_globals) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "global export '%s' index %u out of range",
                          name, (unsigned)index);
        return WASM_ERR_MALFORMED;
    }

    if (out_global) *out_global = &mod->globals[index];
    if (out_index) *out_index = index;
    return WASM_OK;
}

static wasm_error_t wasm__fmt_char_to_type(char ch,
                                           int in_results,
                                           int* out_is_void,
                                           wasm_valtype_t* out_type,
                                           wasm_reftype_t* out_reftype) {
    if (out_is_void) *out_is_void = 0;
    if (out_reftype) wasm__clear_reftype(out_reftype);

    switch (ch) {
        case 'i':
            if (out_type) *out_type = WASM_TYPE_I32;
            return WASM_OK;
        case 'I':
            if (out_type) *out_type = WASM_TYPE_I64;
            return WASM_OK;
        case 'f':
            if (out_type) *out_type = WASM_TYPE_F32;
            return WASM_OK;
        case 'F':
            if (out_type) *out_type = WASM_TYPE_F64;
            return WASM_OK;
        case 'r':
            if (out_type) *out_type = WASM_TYPE_EXTERNREF;
            if (out_reftype) {
                out_reftype->type = WASM_TYPE_EXTERNREF;
                out_reftype->nullable = 1;
            }
            return WASM_OK;
        case 'v':
            if (!in_results) return WASM_ERR_MALFORMED;
            if (out_is_void) *out_is_void = 1;
            return WASM_OK;
        default:
            return WASM_ERR_MALFORMED;
    }
}

static wasm_error_t wasm__parse_fmt_string(const char* fmt,
                                           wasm_functype_t* out_type,
                                           const char** out_error) {
    const char* p;
    uint32_t num_params = 0;
    uint32_t num_results = 0;
    uint32_t param_index = 0;
    uint32_t result_index = 0;
    int in_results = 0;
    int saw_open = 0;
    int saw_close = 0;

    if (out_error) *out_error = "invalid format string";
    if (!fmt || !out_type) return WASM_ERR_MALFORMED;

    memset(out_type, 0, sizeof(*out_type));

    for (p = fmt; *p; p++) {
        int is_void = 0;
        wasm_error_t err;

        if (*p == '(') {
            if (saw_open || saw_close) {
                if (out_error) *out_error = "format string must contain a single result list";
                return WASM_ERR_MALFORMED;
            }
            saw_open = 1;
            in_results = 1;
            continue;
        }
        if (*p == ')') {
            if (!saw_open || saw_close) {
                if (out_error) *out_error = "format string has mismatched parentheses";
                return WASM_ERR_MALFORMED;
            }
            saw_close = 1;
            if (p[1] != '\0') {
                if (out_error) *out_error = "format string has trailing characters";
                return WASM_ERR_MALFORMED;
            }
            break;
        }

        err = wasm__fmt_char_to_type(*p, in_results, &is_void, NULL, NULL);
        if (err != WASM_OK) {
            if (out_error) *out_error = in_results ? "invalid result format specifier" : "invalid parameter format specifier";
            return err;
        }
        if (!in_results)
            num_params++;
        else if (is_void) {
            if (num_results != 0 || p[1] != ')') {
                if (out_error) *out_error = "'v' must be the only result specifier";
                return WASM_ERR_MALFORMED;
            }
        } else {
            num_results++;
        }
    }

    if (!saw_open || !saw_close) {
        if (out_error) *out_error = "format string must use args(results) syntax";
        return WASM_ERR_MALFORMED;
    }

    out_type->num_params = num_params;
    out_type->num_results = num_results;
    if (num_params > 0) {
        out_type->params = (wasm_valtype_t*)WASM_MALLOC(num_params * sizeof(wasm_valtype_t));
        out_type->param_reftypes = (wasm_reftype_t*)WASM_CALLOC(num_params, sizeof(wasm_reftype_t));
        if (!out_type->params || !out_type->param_reftypes) {
            if (out_error) *out_error = "out of memory while parsing parameter types";
            wasm__free_functype(out_type);
            return WASM_ERR_OOM;
        }
    }
    if (num_results > 0) {
        out_type->results = (wasm_valtype_t*)WASM_MALLOC(num_results * sizeof(wasm_valtype_t));
        out_type->result_reftypes = (wasm_reftype_t*)WASM_CALLOC(num_results, sizeof(wasm_reftype_t));
        if (!out_type->results || !out_type->result_reftypes) {
            if (out_error) *out_error = "out of memory while parsing result types";
            wasm__free_functype(out_type);
            return WASM_ERR_OOM;
        }
    }

    in_results = 0;
    for (p = fmt; *p; p++) {
        int is_void = 0;
        wasm_error_t err;
        wasm_valtype_t type;
        wasm_reftype_t ref_type;

        if (*p == '(') {
            in_results = 1;
            continue;
        }
        if (*p == ')') break;

        err = wasm__fmt_char_to_type(*p, in_results, &is_void, &type, &ref_type);
        if (err != WASM_OK) {
            wasm__free_functype(out_type);
            if (out_error) *out_error = "invalid format string";
            return err;
        }
        if (!in_results) {
            out_type->params[param_index] = type;
            out_type->param_reftypes[param_index++] = ref_type;
        } else if (!is_void) {
            out_type->results[result_index] = type;
            out_type->result_reftypes[result_index++] = ref_type;
        }
    }

    return WASM_OK;
}

static wasm_error_t wasm__set_fmt_error(wasm_runtime_t* rt,
                                        wasm_error_t err,
                                        const char* prefix,
                                        const char* detail) {
    if (rt && detail)
        WASM__SET_ERR(rt, err, "%s: %s", prefix, detail);
    else if (rt)
        WASM__SET_ERR(rt, err, "%s", prefix);
    return err;
}

static wasm_error_t wasm__call_fmt_va(wasm_module_t* mod,
                                      const char* func_name,
                                      const char* fmt,
                                      va_list ap) {
    const wasm_functype_t* actual_type;
    wasm_functype_t parsed_type;
    wasm_value_t* args = NULL;
    wasm_value_t* results = NULL;
    void** result_ptrs = NULL;
    const char* parse_error = NULL;
    uint32_t func_index;
    uint32_t i;
    wasm_error_t err;

    memset(&parsed_type, 0, sizeof(parsed_type));
    if (!mod || !func_name || !fmt)
        return wasm__set_fmt_error(mod ? mod->rt : NULL, WASM_ERR_MALFORMED,
                                   "invalid format call arguments", NULL);

    err = wasm__parse_fmt_string(fmt, &parsed_type, &parse_error);
    if (err != WASM_OK)
        return wasm__set_fmt_error(mod->rt, err, "invalid format string", parse_error);

    err = wasm__resolve_exported_func(mod, func_name, &func_index, &actual_type);
    if (err != WASM_OK) goto cleanup;

    if (!wasm__functype_equal(&parsed_type, actual_type)) {
        err = wasm__set_fmt_error(mod->rt, WASM_ERR_TYPE_MISMATCH,
                                  "format signature mismatch",
                                  "format string does not match exported function signature");
        goto cleanup;
    }

    if (parsed_type.num_params > 0) {
        args = (wasm_value_t*)WASM_MALLOC(parsed_type.num_params * sizeof(wasm_value_t));
        if (!args) {
            err = wasm__set_fmt_error(mod->rt, WASM_ERR_OOM, "call_fmt allocation failed", "args");
            goto cleanup;
        }
    }
    if (parsed_type.num_results > 0) {
        results = (wasm_value_t*)WASM_MALLOC(parsed_type.num_results * sizeof(wasm_value_t));
        result_ptrs = (void**)WASM_MALLOC(parsed_type.num_results * sizeof(void*));
        if (!results || !result_ptrs) {
            err = wasm__set_fmt_error(mod->rt, WASM_ERR_OOM, "call_fmt allocation failed", "results");
            goto cleanup;
        }
    }

    for (i = 0; i < parsed_type.num_params; i++) {
        switch (parsed_type.params[i]) {
            case WASM_TYPE_I32:
                args[i] = wasm_i32(va_arg(ap, int32_t));
                break;
            case WASM_TYPE_I64:
                args[i] = wasm_i64(va_arg(ap, int64_t));
                break;
            case WASM_TYPE_F32:
                args[i] = wasm_f32((float)va_arg(ap, double));
                break;
            case WASM_TYPE_F64:
                args[i] = wasm_f64(va_arg(ap, double));
                break;
            case WASM_TYPE_EXTERNREF:
                args[i] = wasm_externref((uintptr_t)va_arg(ap, void*));
                break;
            default:
                err = wasm__set_fmt_error(mod->rt, WASM_ERR_MALFORMED,
                                          "unsupported format argument type", NULL);
                goto cleanup;
        }
    }

    for (i = 0; i < parsed_type.num_results; i++) {
        switch (parsed_type.results[i]) {
            case WASM_TYPE_I32:
                result_ptrs[i] = va_arg(ap, int32_t*);
                break;
            case WASM_TYPE_I64:
                result_ptrs[i] = va_arg(ap, int64_t*);
                break;
            case WASM_TYPE_F32:
                result_ptrs[i] = va_arg(ap, float*);
                break;
            case WASM_TYPE_F64:
                result_ptrs[i] = va_arg(ap, double*);
                break;
            case WASM_TYPE_EXTERNREF:
                result_ptrs[i] = va_arg(ap, void**);
                break;
            default:
                result_ptrs[i] = NULL;
                break;
        }
        if (!result_ptrs[i]) {
            err = wasm__set_fmt_error(mod->rt, WASM_ERR_MALFORMED,
                                      "invalid format result destination", NULL);
            goto cleanup;
        }
    }

    err = wasm_call_index(mod, func_index, args, parsed_type.num_params, results, parsed_type.num_results);
    if (err != WASM_OK) goto cleanup;

    for (i = 0; i < parsed_type.num_results; i++) {
        switch (parsed_type.results[i]) {
            case WASM_TYPE_I32:
                *(int32_t*)result_ptrs[i] = results[i].of.i32;
                break;
            case WASM_TYPE_I64:
                *(int64_t*)result_ptrs[i] = results[i].of.i64;
                break;
            case WASM_TYPE_F32:
                *(float*)result_ptrs[i] = results[i].of.f32;
                break;
            case WASM_TYPE_F64:
                *(double*)result_ptrs[i] = results[i].of.f64;
                break;
            case WASM_TYPE_EXTERNREF:
                *(void**)result_ptrs[i] = (void*)results[i].of.externref;
                break;
            default:
                err = wasm__set_fmt_error(mod->rt, WASM_ERR_MALFORMED,
                                          "unsupported format result type", NULL);
                goto cleanup;
        }
    }

cleanup:
    WASM_FREE(args);
    WASM_FREE(results);
    WASM_FREE(result_ptrs);
    wasm__free_functype(&parsed_type);
    return err;
}

wasm_error_t wasm_call_fmt(wasm_module_t* mod, const char* func_name, const char* fmt, ...) {
    va_list ap;
    wasm_error_t err;

    va_start(ap, fmt);
    err = wasm__call_fmt_va(mod, func_name, fmt, ap);
    va_end(ap);
    return err;
}

wasm_error_t wasm_bind_host_func(wasm_runtime_t* rt, const char* module_name,
                                 const char* func_name, const char* fmt,
                                 wasm_host_func_t callback, void* userdata) {
    wasm_import_t imp;
    wasm_functype_t type;
    const char* parse_error = NULL;
    wasm_error_t err;

    if (!rt || !module_name || !func_name || !fmt || !callback)
        return wasm__set_fmt_error(rt, WASM_ERR_MALFORMED,
                                   "invalid host function binding arguments", NULL);

    memset(&type, 0, sizeof(type));
    err = wasm__parse_fmt_string(fmt, &type, &parse_error);
    if (err != WASM_OK)
        return wasm__set_fmt_error(rt, err, "invalid format string", parse_error);

    memset(&imp, 0, sizeof(imp));
    imp.module = module_name;
    imp.name = func_name;
    imp.type = type;
    imp.func = callback;
    imp.userdata = userdata;

    err = wasm_register_import(rt, &imp);
    wasm__free_functype(&type);
    return err;
}

#define WASM__WASI_MODULE_NAME "wasi_snapshot_preview1"
#define WASM__WASI_FD_STDIN 0u
#define WASM__WASI_FD_STDOUT 1u
#define WASM__WASI_FD_STDERR 2u
#define WASM__WASI_FILETYPE_CHARACTER_DEVICE 2u
#define WASM__WASI_WHENCE_SET 0u
#define WASM__WASI_WHENCE_CUR 1u
#define WASM__WASI_WHENCE_END 2u
#define WASM__WASI_CLOCKID_REALTIME 0u
#define WASM__WASI_CLOCKID_MONOTONIC 1u
#define WASM__WASI_CLOCKID_PROCESS_CPUTIME_ID 2u
#define WASM__WASI_CLOCKID_THREAD_CPUTIME_ID 3u
#define WASM__WASI_ERRNO_SUCCESS 0u
#define WASM__WASI_ERRNO_BADF 8u
#define WASM__WASI_ERRNO_INVAL 28u
#define WASM__WASI_ERRNO_IO 29u
#define WASM__WASI_ERRNO_FAULT 21u

typedef uint32_t (*wasm__wasi_string_count_fn)(void);
typedef const char* (*wasm__wasi_string_at_fn)(uint32_t index);

static void wasm__wasi_set_errno(wasm_value_t* results, uint32_t num_results, uint32_t errno_code) {
    if (results && num_results > 0) results[0] = wasm_i32((int32_t)errno_code);
}

static wasm_error_t wasm__wasi_get_memory0(wasm_module_t* mod,
                                           const char* op_name,
                                           wasm_memory_t** out_memory,
                                           uint64_t* out_mem_size) {
    wasm_runtime_t* rt;
    wasm_memory_t* memory;

    if (!mod || !mod->rt || !op_name || !out_memory || !out_mem_size) return WASM_ERR_MALFORMED;

    rt = mod->rt;

    memory = wasm__memory_at(mod, 0);
    if (!memory) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "wasi %s requires memory 0", op_name);
        return WASM_ERR_MALFORMED;
    }

    *out_memory = memory;
    *out_mem_size = (uint64_t)memory->pages * WASM_PAGE_SIZE;
    return WASM_OK;
}

static uint64_t wasm__wasi_string_list_size(wasm__wasi_string_count_fn count_fn,
                                            wasm__wasi_string_at_fn at_fn) {
    uint32_t count;
    uint32_t i;
    uint64_t total = 0;

    if (!count_fn || !at_fn) return UINT64_MAX;

    count = count_fn();
    for (i = 0; i < count; i++) {
        const char* value = at_fn(i);
        size_t len = value ? strlen(value) : 0u;

        if ((uint64_t)len >= UINT64_MAX - total) return UINT64_MAX;
        total += (uint64_t)len + 1u;
    }

    return total;
}

static uint32_t wasm__wasi_string_list_count_args(void) {
    return wasm__platform_wasi_arg_count();
}

static const char* wasm__wasi_string_list_at_args(uint32_t index) {
    return wasm__platform_wasi_arg_at(index);
}

static uint32_t wasm__wasi_string_list_count_env(void) {
    return wasm__platform_wasi_env_count();
}

static const char* wasm__wasi_string_list_at_env(uint32_t index) {
    return wasm__platform_wasi_env_at(index);
}

static uint32_t wasm__wasi_string_sizes_get(wasm_module_t* mod,
                                            wasm__wasi_string_count_fn count_fn,
                                            wasm__wasi_string_at_fn at_fn,
                                            uint32_t count_ptr,
                                            uint32_t size_ptr,
                                            wasm_value_t* results,
                                            uint32_t num_results) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint64_t total_size;
    uint32_t count;
    wasm_error_t err;

    err = wasm__wasi_get_memory0(mod, "string sizes_get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    count = count_fn ? count_fn() : 0u;
    total_size = wasm__wasi_string_list_size(count_fn, at_fn);
    if (total_size == UINT64_MAX || total_size > UINT32_MAX) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)count_ptr, 4u, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)size_ptr, 4u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    wasm__store_le32(memory->data + count_ptr, count);
    wasm__store_le32(memory->data + size_ptr, (uint32_t)total_size);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

static uint32_t wasm__wasi_string_list_get(wasm_module_t* mod,
                                           wasm__wasi_string_count_fn count_fn,
                                           wasm__wasi_string_at_fn at_fn,
                                           uint32_t entries_ptr,
                                           uint32_t data_ptr,
                                           wasm_value_t* results,
                                           uint32_t num_results) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint64_t strings_size;
    uint32_t count;
    uint32_t i;
    uint32_t cursor;
    wasm_error_t err;

    err = wasm__wasi_get_memory0(mod, "string get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    count = count_fn ? count_fn() : 0u;
    strings_size = wasm__wasi_string_list_size(count_fn, at_fn);
    if (strings_size == UINT64_MAX || strings_size > UINT32_MAX) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)entries_ptr, (uint64_t)count * 4u, mem_size) ||
        !wasm__range_in_bounds_u64((uint64_t)data_ptr, strings_size, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    cursor = data_ptr;
    for (i = 0; i < count; i++) {
        const char* value = at_fn(i);
        size_t len = value ? strlen(value) : 0u;

        wasm__store_le32(memory->data + entries_ptr + (size_t)i * 4u, cursor);
        if (len > 0) memcpy(memory->data + cursor, value, len);
        memory->data[cursor + len] = 0;
        cursor += (uint32_t)len + 1u;
    }

    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

static int wasm__wasi_is_stdio_fd(uint32_t fd) {
    return fd == WASM__WASI_FD_STDIN || fd == WASM__WASI_FD_STDOUT || fd == WASM__WASI_FD_STDERR;
}

static wasm_error_t wasm__wasi_args_sizes_get(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    return wasm__wasi_string_sizes_get(mod,
                                       wasm__wasi_string_list_count_args,
                                       wasm__wasi_string_list_at_args,
                                       (uint32_t)args[0].of.i32,
                                       (uint32_t)args[1].of.i32,
                                       results,
                                       num_results);
}

static wasm_error_t wasm__wasi_args_get(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata) {
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    return wasm__wasi_string_list_get(mod,
                                      wasm__wasi_string_list_count_args,
                                      wasm__wasi_string_list_at_args,
                                      (uint32_t)args[0].of.i32,
                                      (uint32_t)args[1].of.i32,
                                      results,
                                      num_results);
}

static wasm_error_t wasm__wasi_environ_sizes_get(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata) {
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    return wasm__wasi_string_sizes_get(mod,
                                       wasm__wasi_string_list_count_env,
                                       wasm__wasi_string_list_at_env,
                                       (uint32_t)args[0].of.i32,
                                       (uint32_t)args[1].of.i32,
                                       results,
                                       num_results);
}

static wasm_error_t wasm__wasi_fd_write(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata) {
    wasm_memory_t* memory;
    FILE* stream;
    uint64_t mem_size;
    uint64_t total_written = 0;
    uint32_t fd;
    uint32_t iovs_ptr;
    uint32_t iovs_len;
    uint32_t nwritten_ptr;
    uint32_t i;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;

    err = wasm__wasi_get_memory0(mod, "fd_write", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[1].of.i32 < 0 || args[2].of.i32 < 0 || args[3].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    iovs_ptr = (uint32_t)args[1].of.i32;
    iovs_len = (uint32_t)args[2].of.i32;
    nwritten_ptr = (uint32_t)args[3].of.i32;

    if (fd == WASM__WASI_FD_STDOUT)
        stream = stdout;
    else if (fd == WASM__WASI_FD_STDERR)
        stream = stderr;
    else {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }

    if (!wasm__range_in_bounds_u64((uint64_t)nwritten_ptr, 4u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)iovs_ptr, (uint64_t)iovs_len * 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    for (i = 0; i < iovs_len; i++) {
        const uint8_t* iov_base = memory->data + iovs_ptr + (size_t)i * 8u;
        uint32_t buf_ptr = wasm__load_le32(iov_base);
        uint32_t buf_len = wasm__load_le32(iov_base + 4u);
        size_t written;

        if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size)) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
            return WASM_OK;
        }
        if (buf_len == 0) continue;

        written = fwrite(memory->data + buf_ptr, 1u, buf_len, stream);
        if (written != buf_len) {
            clearerr(stream);
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_IO);
            return WASM_OK;
        }
        total_written += written;
        if (total_written > UINT32_MAX) {
            wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
            return WASM_OK;
        }
    }

    fflush(stream);
    wasm__store_le32(memory->data + nwritten_ptr, (uint32_t)total_written);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

static wasm_error_t wasm__wasi_proc_exit(wasm_module_t* mod,
                                         const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results,
                                         void* userdata) {
    (void)mod;
    (void)args;
    (void)results;
    (void)userdata;

    if (num_args != 1 || num_results != 0) return WASM_ERR_TYPE_MISMATCH;
    return WASM_OK;
}

static wasm_error_t wasm__wasi_fd_close(wasm_module_t* mod,
                                        const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results,
                                        void* userdata) {
    uint32_t fd;

    (void)mod;
    (void)userdata;

    if (!results || num_args != 1 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    if (fd == WASM__WASI_FD_STDIN || fd == WASM__WASI_FD_STDOUT || fd == WASM__WASI_FD_STDERR)
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    else
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
    return WASM_OK;
}

static wasm_error_t wasm__wasi_fd_seek(wasm_module_t* mod,
                                       const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results,
                                       void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint32_t fd;
    uint32_t whence;
    uint32_t newoffset_ptr;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 4 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;

    err = wasm__wasi_get_memory0(mod, "fd_seek", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[0].of.i32 < 0 || args[2].of.i32 < 0 || args[3].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    whence = (uint32_t)args[2].of.i32;
    newoffset_ptr = (uint32_t)args[3].of.i32;

    if (!wasm__wasi_is_stdio_fd(fd)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (whence != WASM__WASI_WHENCE_SET && whence != WASM__WASI_WHENCE_CUR && whence != WASM__WASI_WHENCE_END) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)newoffset_ptr, 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    wasm__store_le64(memory->data + newoffset_ptr, 0u);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

static wasm_error_t wasm__wasi_environ_get(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    (void)userdata;

    if (!results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    return wasm__wasi_string_list_get(mod,
                                      wasm__wasi_string_list_count_env,
                                      wasm__wasi_string_list_at_env,
                                      (uint32_t)args[0].of.i32,
                                      (uint32_t)args[1].of.i32,
                                      results,
                                      num_results);
}

static wasm_error_t wasm__wasi_fd_fdstat_get(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint32_t fd;
    uint32_t stat_ptr;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    err = wasm__wasi_get_memory0(mod, "fd_fdstat_get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    fd = (uint32_t)args[0].of.i32;
    stat_ptr = (uint32_t)args[1].of.i32;
    if (!wasm__wasi_is_stdio_fd(fd)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_BADF);
        return WASM_OK;
    }
    if (!wasm__range_in_bounds_u64((uint64_t)stat_ptr, 24u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }

    memory->data[stat_ptr + 0u] = WASM__WASI_FILETYPE_CHARACTER_DEVICE;
    memory->data[stat_ptr + 1u] = 0u;
    wasm__store_le16(memory->data + stat_ptr + 2u, 0u);
    memset(memory->data + stat_ptr + 4u, 0, 4u);
    wasm__store_le64(memory->data + stat_ptr + 8u, UINT64_MAX);
    wasm__store_le64(memory->data + stat_ptr + 16u, UINT64_MAX);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

static wasm_error_t wasm__wasi_random_get(wasm_module_t* mod,
                                          const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results,
                                          void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint32_t buf_ptr;
    uint32_t buf_len;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    err = wasm__wasi_get_memory0(mod, "random_get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[0].of.i32 < 0 || args[1].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    buf_ptr = (uint32_t)args[0].of.i32;
    buf_len = (uint32_t)args[1].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)buf_ptr, (uint64_t)buf_len, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    if (!wasm__platform_wasi_fill_random(memory->data + buf_ptr, buf_len)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_IO);
        return WASM_OK;
    }

    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

static wasm_error_t wasm__wasi_clock_time_get(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint64_t timestamp;
    uint32_t clock_id;
    uint32_t out_ptr;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 3 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    err = wasm__wasi_get_memory0(mod, "clock_time_get", &memory, &mem_size);
    if (err != WASM_OK) return err;

    if (args[0].of.i32 < 0 || args[2].of.i32 < 0) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }

    clock_id = (uint32_t)args[0].of.i32;
    out_ptr = (uint32_t)args[2].of.i32;
    if (!wasm__range_in_bounds_u64((uint64_t)out_ptr, 8u, mem_size)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_FAULT);
        return WASM_OK;
    }
    if (clock_id != WASM__WASI_CLOCKID_REALTIME &&
        clock_id != WASM__WASI_CLOCKID_MONOTONIC &&
        clock_id != WASM__WASI_CLOCKID_PROCESS_CPUTIME_ID &&
        clock_id != WASM__WASI_CLOCKID_THREAD_CPUTIME_ID) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_INVAL);
        return WASM_OK;
    }
    if (!wasm__platform_wasi_clock_time_get(clock_id, (uint64_t)args[1].of.i64, &timestamp)) {
        wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_IO);
        return WASM_OK;
    }

    wasm__store_le64(memory->data + out_ptr, timestamp);
    wasm__wasi_set_errno(results, num_results, WASM__WASI_ERRNO_SUCCESS);
    return WASM_OK;
}

wasm_error_t wasm_bind_wasi_stubs(wasm_runtime_t* rt) {
    wasm_error_t err;

    if (!rt) return WASM_ERR_MALFORMED;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "fd_write", "iiii(i)",
                              wasm__wasi_fd_write, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "args_sizes_get", "ii(i)",
                              wasm__wasi_args_sizes_get, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "args_get", "ii(i)",
                              wasm__wasi_args_get, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "environ_sizes_get", "ii(i)",
                              wasm__wasi_environ_sizes_get, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "proc_exit", "i(v)",
                              wasm__wasi_proc_exit, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "fd_close", "i(i)",
                              wasm__wasi_fd_close, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "fd_seek", "iIii(i)",
                              wasm__wasi_fd_seek, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "fd_fdstat_get", "ii(i)",
                              wasm__wasi_fd_fdstat_get, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "environ_get", "ii(i)",
                              wasm__wasi_environ_get, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "random_get", "ii(i)",
                              wasm__wasi_random_get, NULL);
    if (err != WASM_OK) return err;

    err = wasm_bind_host_func(rt, WASM__WASI_MODULE_NAME, "clock_time_get", "iIi(i)",
                              wasm__wasi_clock_time_get, NULL);
    if (err != WASM_OK) return err;

    return WASM_OK;
}

/* ── Memory helpers ───────────────────────────────────────────────── */

uint32_t wasm_memory_count(wasm_module_t* mod) {
    return mod ? mod->num_memories : 0;
}

wasm_error_t wasm_memory_read(wasm_module_t* mod, uint32_t memory_index,
                              uint32_t offset, void* dst, size_t len) {
    wasm_memory_t* memory;
    uint64_t mem_size;

    if (!mod || (len > 0 && !dst)) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid memory read arguments");
        return WASM_ERR_MALFORMED;
    }

    memory = wasm__memory_at(mod, memory_index);
    if (!memory) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "unknown memory %u", (unsigned)memory_index);
        return WASM_ERR_MALFORMED;
    }

    mem_size = (uint64_t)memory->pages * WASM_PAGE_SIZE;
    if (!wasm__range_in_bounds_u64((uint64_t)offset, (uint64_t)len, mem_size)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_OUT_OF_BOUNDS,
                          "memory %u read out of bounds: offset=%u len=%zu size=%llu",
                          (unsigned)memory_index, (unsigned)offset, len,
                          (unsigned long long)mem_size);
        return WASM_ERR_OUT_OF_BOUNDS;
    }

    if (len > 0) memcpy(dst, memory->data + offset, len);
    return WASM_OK;
}

wasm_error_t wasm_memory_write(wasm_module_t* mod, uint32_t memory_index,
                               uint32_t offset, const void* src, size_t len) {
    wasm_memory_t* memory;
    uint64_t mem_size;

    if (!mod || (len > 0 && !src)) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid memory write arguments");
        return WASM_ERR_MALFORMED;
    }

    memory = wasm__memory_at(mod, memory_index);
    if (!memory) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "unknown memory %u", (unsigned)memory_index);
        return WASM_ERR_MALFORMED;
    }

    mem_size = (uint64_t)memory->pages * WASM_PAGE_SIZE;
    if (!wasm__range_in_bounds_u64((uint64_t)offset, (uint64_t)len, mem_size)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_OUT_OF_BOUNDS,
                          "memory %u write out of bounds: offset=%u len=%zu size=%llu",
                          (unsigned)memory_index, (unsigned)offset, len,
                          (unsigned long long)mem_size);
        return WASM_ERR_OUT_OF_BOUNDS;
    }

    if (len > 0) memcpy(memory->data + offset, src, len);
    return WASM_OK;
}

wasm_error_t wasm_memory_get_string(wasm_module_t* mod, uint32_t memory_index,
                                    uint32_t offset, char* dst, size_t max_len) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint64_t available_u64;
    size_t max_copy;
    size_t actual_len = 0;
    const uint8_t* src;

    if (!mod || !dst || max_len == 0) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid string extraction arguments");
        return WASM_ERR_MALFORMED;
    }

    memory = wasm__memory_at(mod, memory_index);
    if (!memory) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "unknown memory %u", (unsigned)memory_index);
        return WASM_ERR_MALFORMED;
    }

    mem_size = (uint64_t)memory->pages * WASM_PAGE_SIZE;
    if ((uint64_t)offset > mem_size) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_OUT_OF_BOUNDS,
                          "memory %u string offset out of bounds: offset=%u size=%llu",
                          (unsigned)memory_index, (unsigned)offset,
                          (unsigned long long)mem_size);
        return WASM_ERR_OUT_OF_BOUNDS;
    }

    available_u64 = mem_size - (uint64_t)offset;
    max_copy = max_len - 1u;
    if ((uint64_t)max_copy > available_u64) max_copy = (size_t)available_u64;

    src = memory->data ? memory->data + offset : NULL;
    if (src) {
        while (actual_len < max_copy && src[actual_len] != 0) actual_len++;
        if (actual_len > 0) memcpy(dst, src, actual_len);
    }

    dst[actual_len] = '\0';
    return WASM_OK;
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

wasm_memory_t* wasm_memory_ref_at(wasm_module_t* mod, uint32_t memory_index) {
    return wasm__memory_at(mod, memory_index);
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

uint32_t wasm_table_count(wasm_module_t* mod) {
    return mod ? mod->num_tables : 0;
}

wasm_table_t* wasm_table_ref_at(wasm_module_t* mod, uint32_t table_idx) {
    return wasm__table_at(mod, table_idx);
}

wasm_valtype_t wasm_table_type(wasm_module_t* mod, uint32_t table_idx) {
    wasm_table_t* table = wasm__table_at(mod, table_idx);
    return table ? table->reftype : WASM_TYPE_VOID;
}

const wasm_reftype_t* wasm_table_reftype(wasm_module_t* mod, uint32_t table_idx) {
    wasm_table_t* table = wasm__table_at(mod, table_idx);
    return table ? &table->reftype_info : NULL;
}

uint32_t wasm_global_count(wasm_module_t* mod) {
    return mod ? mod->num_globals : 0;
}

wasm_valtype_t wasm_global_type(wasm_module_t* mod, uint32_t global_idx) {
    if (!mod || global_idx >= mod->num_globals) return WASM_TYPE_VOID;
    return mod->globals[global_idx].type;
}

const wasm_reftype_t* wasm_global_reftype(wasm_module_t* mod, uint32_t global_idx) {
    if (!mod || global_idx >= mod->num_globals) return NULL;
    return &mod->globals[global_idx].ref_type;
}

int wasm_global_is_mutable(wasm_module_t* mod, uint32_t global_idx) {
    if (!mod || global_idx >= mod->num_globals) return 0;
    return mod->globals[global_idx].is_mutable;
}

wasm_value_t* wasm_global_value_ref_at(wasm_module_t* mod, uint32_t global_idx) {
    wasm_global_t* global;

    if (!mod || global_idx >= mod->num_globals) return NULL;
    global = &mod->globals[global_idx];
    return global->is_import ? global->import_value : &global->value;
}

wasm_error_t wasm_global_get(wasm_module_t* mod, const char* name, wasm_value_t* out_val) {
    wasm_global_t* global;
    wasm_error_t err;

    if (!mod || !name || !out_val) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid global get arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__resolve_exported_global(mod, name, &global, NULL);
    if (err != WASM_OK) return err;

    *out_val = wasm__global_get_value(global);
    return WASM_OK;
}

wasm_error_t wasm_global_set(wasm_module_t* mod, const char* name, wasm_value_t val) {
    wasm_global_t* global;
    wasm_error_t err;

    if (!mod || !name) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid global set arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__resolve_exported_global(mod, name, &global, NULL);
    if (err != WASM_OK) return err;

    if (!global->is_mutable) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "global '%s' is immutable", name);
        return WASM_ERR_TYPE_MISMATCH;
    }
    if (!wasm__runtime_value_matches_type(mod, &val, global->type, &global->ref_type)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "global '%s' type mismatch", name);
        return WASM_ERR_TYPE_MISMATCH;
    }

    wasm__global_set_value(global, val);
    return WASM_OK;
}

uint32_t wasm_export_count(wasm_module_t* mod) {
    return mod ? mod->num_exports : 0;
}

uint32_t wasm_import_count(wasm_module_t* mod) {
    return mod ? mod->num_imports : 0;
}

const wasm_import_info_t* wasm_import_info(wasm_module_t* mod, uint32_t index) {
    if (!mod || index >= mod->num_imports) return NULL;
    return &mod->imports[index];
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

uint32_t wasm_custom_section_count(wasm_module_t* mod) {
    return mod ? mod->num_custom_sections : 0;
}

const char* wasm_custom_section_name(wasm_module_t* mod, uint32_t index) {
    if (!mod || index >= mod->num_custom_sections) return NULL;
    return mod->custom_sections[index].name;
}

const uint8_t* wasm_custom_section_data(wasm_module_t* mod, uint32_t index, size_t* out_len) {
    if (out_len) *out_len = 0;
    if (!mod || index >= mod->num_custom_sections) return NULL;
    if (out_len) *out_len = (size_t)mod->custom_sections[index].size;
    return mod->custom_sections[index].data;
}

const uint8_t* wasm_custom_section_find(wasm_module_t* mod, const char* name, size_t* out_len) {
    uint32_t index;

    if (out_len) *out_len = 0;
    if (!mod || !name) return NULL;

    for (index = 0; index < mod->num_custom_sections; index++) {
        if (strcmp(mod->custom_sections[index].name, name) == 0) {
            if (out_len) *out_len = (size_t)mod->custom_sections[index].size;
            return mod->custom_sections[index].data;
        }
    }

    return NULL;
}

uint32_t wasm_type_count(wasm_module_t* mod) {
    return mod ? mod->num_types : 0;
}

wasm_comptype_kind_t wasm_type_kind(wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return WASM_COMP_INVALID;
    return mod->types[type_idx].kind;
}

const wasm_functype_t* wasm_type_functype(wasm_module_t* mod, uint32_t type_idx) {
    return wasm__module_const_functype(mod, type_idx);
}

uint32_t wasm_struct_type_field_count(wasm_module_t* mod, uint32_t type_idx) {
    const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
    return st ? st->num_fields : 0;
}

const wasm_fieldtype_t* wasm_struct_type_field(wasm_module_t* mod, uint32_t type_idx,
                                               uint32_t field_idx) {
    const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);

    if (!st || field_idx >= st->num_fields) return NULL;
    return &st->fields[field_idx];
}

const wasm_fieldtype_t* wasm_array_type_field(wasm_module_t* mod, uint32_t type_idx) {
    const wasm_arraytype_t* at = wasm__module_const_arraytype(mod, type_idx);

    if (!at) return NULL;
    return &at->field;
}

uint32_t wasm_func_count(wasm_module_t* mod) {
    return mod ? mod->num_funcs : 0;
}

const wasm_functype_t* wasm_func_functype(wasm_module_t* mod, uint32_t func_idx) {
    if (!mod || func_idx >= mod->num_funcs) return NULL;
    return wasm__module_const_functype(mod, mod->funcs[func_idx].type_idx);
}

uint32_t wasm_func_param_count(wasm_module_t* mod, uint32_t func_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return 0;
    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) return 0;
    return ft->num_params;
}

uint32_t wasm_func_result_count(wasm_module_t* mod, uint32_t func_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return 0;
    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) return 0;
    return ft->num_results;
}

wasm_valtype_t wasm_func_param_type(wasm_module_t* mod, uint32_t func_idx,
                                    uint32_t param_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return WASM_TYPE_VOID;
    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) return WASM_TYPE_VOID;
    if (param_idx >= ft->num_params) return WASM_TYPE_VOID;
    return ft->params[param_idx];
}

wasm_valtype_t wasm_func_result_type(wasm_module_t* mod, uint32_t func_idx,
                                     uint32_t result_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return WASM_TYPE_VOID;
    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) return WASM_TYPE_VOID;
    if (result_idx >= ft->num_results) return WASM_TYPE_VOID;
    return ft->results[result_idx];
}

uint32_t wasm_tag_count(wasm_module_t* mod) {
    return mod ? mod->num_tags : 0;
}

const wasm_functype_t* wasm_tag_functype(wasm_module_t* mod, uint32_t tag_idx) {
    if (!mod || tag_idx >= mod->num_tags) return NULL;
    return wasm__module_const_functype(mod, mod->tags[tag_idx].type_idx);
}

static const char* wasm__gc_kind_name(wasm_gc_object_kind_t kind) {
    switch (kind) {
        case WASM_GC_OBJ_STRUCT:
            return "struct";
        case WASM_GC_OBJ_ARRAY:
            return "array";
        default:
            return "gc";
    }
}

static wasm_error_t wasm__public_resolve_gc_object(wasm_module_t* mod,
                                                   const wasm_value_t* ref,
                                                   wasm_gc_object_kind_t expected_kind,
                                                   const wasm_gc_header_t** out_object) {
    const wasm_gc_header_t* object;

    if (!mod || !ref || !out_object) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid GC access arguments");
        return WASM_ERR_MALFORMED;
    }

    if (!wasm__uses_gc_ref_storage(ref->type)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "expected %s reference, got value type 0x%X",
                          wasm__gc_kind_name(expected_kind),
                          (unsigned)ref->type);
        return WASM_ERR_TYPE_MISMATCH;
    }

    if (wasm__is_null_ref(ref)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TRAP,
                          "null %s reference", wasm__gc_kind_name(expected_kind));
        return WASM_ERR_TRAP;
    }

    if ((ref->of.gc_ref & WASM__GC_I31_TAG) != 0) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "expected %s heap object reference",
                          wasm__gc_kind_name(expected_kind));
        return WASM_ERR_TYPE_MISMATCH;
    }

    object = wasm__runtime_get_gc_object(mod, ref);
    if (!object) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TRAP,
                          "invalid %s reference", wasm__gc_kind_name(expected_kind));
        return WASM_ERR_TRAP;
    }

    if (object->kind != expected_kind) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "expected %s reference", wasm__gc_kind_name(expected_kind));
        return WASM_ERR_TYPE_MISMATCH;
    }

    if (!object->module || object->type_index >= object->module->num_types) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "invalid %s type metadata", wasm__gc_kind_name(expected_kind));
        return WASM_ERR_MALFORMED;
    }

    *out_object = object;
    return WASM_OK;
}

wasm_error_t wasm_struct_get_field(wasm_module_t* mod, wasm_value_t struct_ref,
                                   uint32_t field_idx, wasm_value_t* out_val) {
    const wasm_gc_header_t* header;
    const wasm_structtype_t* type;
    wasm_error_t err;

    if (!out_val) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid struct get arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__public_resolve_gc_object(mod, &struct_ref, WASM_GC_OBJ_STRUCT, &header);
    if (err != WASM_OK) return err;

    type = wasm__module_const_structtype(header->module, header->type_index);
    if (!type) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid struct type metadata");
        return WASM_ERR_MALFORMED;
    }

    return wasm__gc_struct_get_field((const wasm_gc_struct_t*)header, type, field_idx, 0, out_val);
}

wasm_error_t wasm_struct_set_field(wasm_module_t* mod, wasm_value_t struct_ref,
                                   uint32_t field_idx, wasm_value_t val) {
    const wasm_gc_header_t* header;
    const wasm_structtype_t* type;
    wasm_error_t err;

    err = wasm__public_resolve_gc_object(mod, &struct_ref, WASM_GC_OBJ_STRUCT, &header);
    if (err != WASM_OK) return err;

    type = wasm__module_const_structtype(header->module, header->type_index);
    if (!type) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid struct type metadata");
        return WASM_ERR_MALFORMED;
    }
    if (field_idx >= type->num_fields) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "struct field %u out of range", (unsigned)field_idx);
        return WASM_ERR_MALFORMED;
    }
    if (!type->fields[field_idx].is_mutable) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "struct field %u is immutable", (unsigned)field_idx);
        return WASM_ERR_TYPE_MISMATCH;
    }

    return wasm__gc_struct_set_field((wasm_gc_struct_t*)header, type, field_idx, val);
}

wasm_error_t wasm_array_length(wasm_module_t* mod, wasm_value_t array_ref,
                               uint32_t* out_len) {
    const wasm_gc_header_t* header;
    wasm_error_t err;

    if (!out_len) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid array length arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__public_resolve_gc_object(mod, &array_ref, WASM_GC_OBJ_ARRAY, &header);
    if (err != WASM_OK) return err;

    *out_len = ((const wasm_gc_array_t*)header)->length;
    return WASM_OK;
}

wasm_error_t wasm_array_get_elem(wasm_module_t* mod, wasm_value_t array_ref,
                                 uint32_t index, wasm_value_t* out_val) {
    const wasm_gc_header_t* header;
    const wasm_arraytype_t* type;
    wasm_error_t err;

    if (!out_val) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid array get arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__public_resolve_gc_object(mod, &array_ref, WASM_GC_OBJ_ARRAY, &header);
    if (err != WASM_OK) return err;

    type = wasm__module_const_arraytype(header->module, header->type_index);
    if (!type) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid array type metadata");
        return WASM_ERR_MALFORMED;
    }

    return wasm__gc_array_get((const wasm_gc_array_t*)header, type, index, 0, out_val);
}

wasm_error_t wasm_array_set_elem(wasm_module_t* mod, wasm_value_t array_ref,
                                 uint32_t index, wasm_value_t val) {
    const wasm_gc_header_t* header;
    const wasm_arraytype_t* type;
    wasm_error_t err;

    err = wasm__public_resolve_gc_object(mod, &array_ref, WASM_GC_OBJ_ARRAY, &header);
    if (err != WASM_OK) return err;

    type = wasm__module_const_arraytype(header->module, header->type_index);
    if (!type) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid array type metadata");
        return WASM_ERR_MALFORMED;
    }
    if (!type->field.is_mutable) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "%s", "array element is immutable");
        return WASM_ERR_TYPE_MISMATCH;
    }

    return wasm__gc_array_set((wasm_gc_array_t*)header, type, index, val);
}

void wasm_dump_backtrace(wasm_runtime_t* rt, char* buffer, size_t buffer_size) {
    uint32_t depth;
    size_t used = 0;
    uint32_t i;

    if (!buffer || buffer_size == 0) return;

    buffer[0] = '\0';
    if (!rt) return;

    depth = wasm__visible_call_stack_depth(rt);
    for (i = 0; i < depth; i++) {
        uint32_t func_idx;
        uint32_t offset;
        int written;

        if (!wasm__visible_call_frame_info(rt, i, &func_idx, &offset)) break;
        written = wasm__snprintf(buffer + used, buffer_size - used,
                                 "#%u func[%u] at offset 0x%04X%s",
                                 (unsigned)i,
                                 (unsigned)func_idx,
                                 (unsigned)offset,
                                 (i + 1u < depth) ? "\n" : "");
        if (written < 0) break;
        if ((size_t)written >= buffer_size - used) {
            used = buffer_size - 1u;
            break;
        }
        used += (size_t)written;
    }

    buffer[used] = '\0';
}

uint32_t wasm_get_call_stack_depth(wasm_runtime_t* rt) {
    return wasm__visible_call_stack_depth(rt);
}

wasm_error_t wasm_get_call_frame_info(wasm_runtime_t* rt, uint32_t depth,
                                      uint32_t* out_func_idx, uint32_t* out_offset) {
    uint32_t stack_depth;

    if (!rt || !out_func_idx || !out_offset) {
        if (rt)
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "invalid call frame query arguments");
        return WASM_ERR_MALFORMED;
    }

    stack_depth = wasm__visible_call_stack_depth(rt);
    if (!wasm__visible_call_frame_info(rt, depth, out_func_idx, out_offset)) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED,
                      "call frame depth %u out of range (depth=%u)",
                      (unsigned)depth, (unsigned)stack_depth);
        return WASM_ERR_MALFORMED;
    }

    return WASM_OK;
}

void wasm_set_fuel(wasm_runtime_t* rt, uint64_t fuel) {
    if (!rt) return;

    rt->fuel = fuel;
    rt->fuel_enabled = 1;
}

uint64_t wasm_get_fuel(wasm_runtime_t* rt) {
    return rt ? rt->fuel : 0;
}

void wasm_module_set_userdata(wasm_module_t* mod, void* userdata) {
    if (!mod) return;
    mod->host_userdata = userdata;
}

void* wasm_module_get_userdata(wasm_module_t* mod) {
    return mod ? mod->host_userdata : NULL;
}

wasm_error_t wasm_struct_new(wasm_module_t* mod, uint32_t type_idx,
                             const wasm_value_t* field_values, uint32_t num_fields,
                             wasm_value_t* out_ref) {
    const wasm_structtype_t* st;
    wasm_gc_struct_t* object;
    uint32_t i;

    if (!mod || !out_ref) return WASM_ERR_MALFORMED;

    st = wasm__module_const_structtype(mod, type_idx);
    if (!st) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "type %u is not a struct type", (unsigned)type_idx);
        return WASM_ERR_TYPE_MISMATCH;
    }
    if (num_fields != st->num_fields || (num_fields > 0 && !field_values)) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                      "struct type %u expects %u fields, got %u",
                      (unsigned)type_idx,
                      (unsigned)st->num_fields,
                      (unsigned)num_fields);
        return WASM_ERR_TYPE_MISMATCH;
    }

    object = wasm__gc_alloc_struct_object(mod->rt, mod, type_idx, st);
    if (!object) return WASM_ERR_OOM;

    for (i = 0; i < st->num_fields; i++) {
        wasm_error_t err = wasm__gc_struct_store_field(object, st, i, field_values[i]);
        if (err != WASM_OK) return err;
    }

    *out_ref = wasm__gc_type_ref_value(mod, type_idx, &object->header);
    return WASM_OK;
}

wasm_error_t wasm_array_new(wasm_module_t* mod, uint32_t type_idx,
                            const wasm_value_t* elem_values, uint32_t num_elems,
                            wasm_value_t* out_ref) {
    const wasm_arraytype_t* at;
    wasm_gc_array_t* object;
    uint32_t i;

    if (!mod || !out_ref) return WASM_ERR_MALFORMED;

    at = wasm__module_const_arraytype(mod, type_idx);
    if (!at) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "type %u is not an array type", (unsigned)type_idx);
        return WASM_ERR_TYPE_MISMATCH;
    }
    if (num_elems > 0 && !elem_values) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                      "array values are required for %u elements",
                      (unsigned)num_elems);
        return WASM_ERR_MALFORMED;
    }

    object = wasm__gc_alloc_array_object(mod->rt, mod, type_idx, at, num_elems);
    if (!object) return WASM_ERR_OOM;

    for (i = 0; i < num_elems; i++) {
        wasm_error_t err = wasm__gc_array_store_element(object, at, i, elem_values[i]);
        if (err != WASM_OK) return err;
    }

    *out_ref = wasm__gc_type_ref_value(mod, type_idx, &object->header);
    return WASM_OK;
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
        case WASM_ERR_OUT_OF_FUEL:
            return "out of fuel";
        case WASM_ERR_TRAP:
            return "trap";
    }
    return "unknown error";
}

#endif /* WASM_IMPL */
#endif /* WASM_H */