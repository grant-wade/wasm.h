#ifndef WASM_INTERNAL_COMMON_H
#define WASM_INTERNAL_COMMON_H

#include "../wasm.h"

#ifdef WASM_AMALGAMATION
#define WASM_PRIVATE static
#define WASM_PRIVATE_INLINE WASM_INLINE
#else
#define WASM_PRIVATE extern
#define WASM_PRIVATE_INLINE extern
#endif

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#define WASM__SIMD_USE_NEON 1
#include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define WASM__SIMD_USE_SSE2 1
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

#if WASM_ENABLE_PLATFORM
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif
#endif

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

typedef struct wasm__control_target_t {
    uint32_t end_offset;
    uint32_t aux_offset;
} wasm__control_target_t;

struct wasm_func_t {
    uint32_t type_idx;
    uint32_t num_locals;
    wasm_valtype_t* locals;
    wasm_reftype_t* local_reftypes;
    const uint8_t* code;
    uint32_t code_len;
    wasm__control_target_t* control_targets;
    uint32_t max_label_depth;
    uint32_t funcref_id;
    int is_declared_ref;
    int is_import;
    wasm_host_func_t host_func;
    void* host_userdata;
};

struct wasm_func_ref_t {
    struct wasm_module_t* module;
    uint32_t func_idx;
};

struct wasm__exception_object_t {
    uint32_t tag_index;
    uint64_t tag_identity;
    wasm_value_t* values;
    uint32_t num_values;
    struct wasm__exception_object_t* next;
};

struct wasm__emscripten_state_t {
    wasm_memory_t memory;
};

struct wasm__externref_box_t {
    wasm_value_t value;
    wasm__externref_box_t* next;
};

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
    int preserve_on_failure;
    uint32_t start_func;
    int startup_ran;
    wasm_module_t* gc_prev;
    wasm_module_t* gc_next;
    wasm_module_t* failed_next;
};

struct wasm_runtime_t {
    wasm_import_t* imports;
    uint32_t num_imports;
    uint32_t cap_imports;
    wasm_func_ref_t* func_refs;
    uint32_t num_func_refs;
    uint32_t cap_func_refs;
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
    uint64_t next_tag_identity;
    wasm_value_t* stack;
    uint32_t max_stack;
    uint32_t sp;
    uint64_t fuel;
    int fuel_enabled;
    uint32_t enabled_features;
    int lazy_validation;
    wasm_error_t last_error;
    char error_msg[256];
    wasm__exception_state_t pending_exception;
    wasm__exception_object_t* exception_objects;
    uint8_t* gc_heap;
    size_t gc_heap_size;
    size_t gc_heap_offset;
    wasm_gc_header_t* gc_allocations;
    wasm_module_t* gc_modules;
    wasm_module_t* failed_modules;
    wasm__externref_box_t* externref_boxes;
    uint8_t* frame_arena;
    size_t frame_arena_size;
    size_t frame_arena_offset;
    wasm__call_frame_t* call_frames;
    uint32_t max_call_frames;
    uint32_t call_frame_sp;
    wasm_valtype_t* validator_stack;
    wasm_reftype_t* validator_stack_reftypes;
    wasm__val_frame_t* validator_frames;
    uint64_t* validator_local_init_bits;
    uint64_t* validator_local_init_entry_bits;
    size_t validator_local_init_capacity;
    uint32_t backtrace_depth;
    uint32_t max_labels;
    uint32_t* backtrace_func_indices;
    uint32_t* backtrace_offsets;
    wasm__wasi_state_t* wasi_state;
    int emscripten_stubs_enabled;
    wasm__emscripten_state_t* emscripten_state;
};

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
WASM_PRIVATE void* wasm__calloc_fallback(size_t count, size_t size);
#define WASM_CALLOC(n, sz) wasm__calloc_fallback((n), (sz))
#endif

#if defined(WASM__SIMD_USE_NEON)
WASM_PRIVATE uint8x16_t wasm__neon_load_u8x16(const wasm_value_t* value);

WASM_PRIVATE int8x16_t wasm__neon_load_i8x16(const wasm_value_t* value);

WASM_PRIVATE uint16x8_t wasm__neon_load_u16x8(const wasm_value_t* value);

WASM_PRIVATE int16x8_t wasm__neon_load_i16x8(const wasm_value_t* value);

WASM_PRIVATE uint32x4_t wasm__neon_load_u32x4(const wasm_value_t* value);

WASM_PRIVATE int32x4_t wasm__neon_load_i32x4(const wasm_value_t* value);

WASM_PRIVATE uint64x2_t wasm__neon_load_u64x2(const wasm_value_t* value);

WASM_PRIVATE int64x2_t wasm__neon_load_i64x2(const wasm_value_t* value);

WASM_PRIVATE float32x4_t wasm__neon_load_f32x4(const wasm_value_t* value);

WASM_PRIVATE float64x2_t wasm__neon_load_f64x2(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__neon_store_u8x16(uint8x16_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_u16x8(uint16x8_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_u32x4(uint32x4_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_u64x2(uint64x2_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_f32x4(float32x4_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_f64x2(float64x2_t value);

WASM_PRIVATE int wasm__neon_any_true_u8x16(uint8x16_t value);
#endif

#if defined(WASM__SIMD_USE_SSE2)
WASM_PRIVATE __m128i wasm__sse_load_si128(const wasm_value_t* value);

WASM_PRIVATE __m128 wasm__sse_load_ps128(const wasm_value_t* value);

WASM_PRIVATE __m128d wasm__sse_load_pd128(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__sse_store_si128(__m128i value);

WASM_PRIVATE wasm_value_t wasm__sse_store_ps128(__m128 value);

WASM_PRIVATE wasm_value_t wasm__sse_store_pd128(__m128d value);

WASM_PRIVATE __m128i wasm__sse_select_si128(__m128i mask, __m128i if_true, __m128i if_false);

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu8(__m128i lhs, __m128i rhs);

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu16(__m128i lhs, __m128i rhs);

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu32(__m128i lhs, __m128i rhs);

WASM_PRIVATE __m128i wasm__sse_cmpeq_epi64(__m128i lhs, __m128i rhs);

WASM_PRIVATE __m128 wasm__sse_abs_ps(__m128 value);

WASM_PRIVATE __m128 wasm__sse_neg_ps(__m128 value);

WASM_PRIVATE __m128d wasm__sse_abs_pd(__m128d value);

WASM_PRIVATE __m128d wasm__sse_neg_pd(__m128d value);
#endif

#if defined(_MSC_VER)
#define WASM__ALIGNOF(type) __alignof(type)
#else
#define WASM__ALIGNOF(type) offsetof(struct { char c; type value; }, value)
#endif

#if WASM_ENABLE_PLATFORM
/* ── WASI platform hooks ─────────────────────────────────────────── */

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
#endif /* WASM_ENABLE_PLATFORM */

/* ── Forward declarations ─────────────────────────────────────────── */
WASM_PRIVATE wasm_error_t wasm__resolve_module_reftypes(wasm_module_t* mod);
WASM_PRIVATE int wasm__resolve_func_ref(const wasm_runtime_t* rt,
                                  uint32_t ref_id,
                                  wasm_module_t** out_module,
                                  uint32_t* out_func_idx);
WASM_PRIVATE int wasm__is_heap_subtype(const wasm_module_t* mod,
                                 wasm_valtype_t subtype,
                                 wasm_valtype_t supertype);
WASM_PRIVATE_INLINE int wasm__is_subtype(const wasm_module_t* mod,
                                   uint32_t subtype_idx,
                                   uint32_t supertype_idx);
WASM_PRIVATE int wasm__is_valtype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype);
WASM_PRIVATE struct wasm_table_t* wasm__table_actual(struct wasm_table_t* table);
WASM_PRIVATE const struct wasm_table_t* wasm__table_actual_const(const struct wasm_table_t* table);
WASM_PRIVATE struct wasm_memory_t* wasm__memory_actual(struct wasm_memory_t* memory);
WASM_PRIVATE const struct wasm_memory_t* wasm__memory_actual_const(const struct wasm_memory_t* memory);

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE void wasm__wasi_state_destroy(wasm_runtime_t* rt);
#endif

WASM_PRIVATE_INLINE int wasm__uses_funcref_storage(wasm_valtype_t type);

WASM_PRIVATE_INLINE int wasm__uses_externref_storage(wasm_valtype_t type);

WASM_PRIVATE_INLINE int wasm__uses_exnref_storage(wasm_valtype_t type);

WASM_PRIVATE_INLINE int wasm__uses_gc_ref_storage(wasm_valtype_t type);

WASM_PRIVATE wasm_error_t wasm__register_import_internal(wasm_runtime_t* rt,
                                                   const wasm_import_t* imp,
                                                   int zeroed_type_is_unspecified);
#if WASM_ENABLE_PLATFORM
WASM_PRIVATE wasm_error_t wasm__ensure_emscripten_memory_import(wasm_runtime_t* rt,
                                                          uint64_t min_pages,
                                                          uint64_t max_pages,
                                                          int is_64);

WASM_PRIVATE uint32_t wasm__platform_wasi_arg_count(void);

WASM_PRIVATE const char* wasm__platform_wasi_arg_at(uint32_t index);

WASM_PRIVATE uint32_t wasm__platform_wasi_env_count(void);

WASM_PRIVATE const char* wasm__platform_wasi_env_at(uint32_t index);

#if WASM__PLATFORM_WINDOWS
WASM_PRIVATE uint64_t wasm__platform_windows_filetime_to_unix_ns(FILETIME ft);

WASM_PRIVATE int wasm__platform_windows_qpc_time_ns(uint64_t* out_time);

WASM_PRIVATE int wasm__platform_windows_process_time_ns(uint64_t* out_time);

WASM_PRIVATE int wasm__platform_windows_thread_time_ns(uint64_t* out_time);
#endif

#if WASM__PLATFORM_POSIX
WASM_PRIVATE int wasm__platform_posix_clock_time_get(uint32_t clock_id, uint64_t* out_time);
#endif

#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#if WASM__PLATFORM_WINDOWS && defined(_MSC_VER)
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len);
#elif WASM__PLATFORM_POSIX
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len);
#else
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len);
#endif
#else
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len);
#endif

#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#if WASM__PLATFORM_WINDOWS
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time);
#elif WASM__PLATFORM_POSIX
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time);
#else
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time);
#endif
#else
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time);
#endif

#endif /* WASM_ENABLE_PLATFORM */

/* ── Portable bit operations (pure C99, no builtins) ──────────────── */

WASM_PRIVATE uint32_t wasm__clz32(uint32_t v);

WASM_PRIVATE uint32_t wasm__ctz32(uint32_t v);

WASM_PRIVATE uint32_t wasm__popcnt32(uint32_t v);

WASM_PRIVATE uint32_t wasm__clz64(uint64_t v);

WASM_PRIVATE uint32_t wasm__ctz64(uint64_t v);

WASM_PRIVATE uint32_t wasm__popcnt64(uint64_t v);

WASM_PRIVATE uint32_t wasm__shr_s32_bits(uint32_t value, uint32_t shift);

WASM_PRIVATE uint64_t wasm__shr_s64_bits(uint64_t value, uint32_t shift);

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
WASM_PRIVATE uint16_t wasm__bswap16(uint16_t v);

WASM_PRIVATE uint32_t wasm__bswap32(uint32_t v);

WASM_PRIVATE uint64_t wasm__bswap64(uint64_t v);
#endif

WASM_PRIVATE uint16_t wasm__load_le16(const void* p);

WASM_PRIVATE uint32_t wasm__load_le32(const void* p);

WASM_PRIVATE uint64_t wasm__load_le64(const void* p);

WASM_PRIVATE void wasm__store_le16(void* p, uint16_t v);

WASM_PRIVATE void wasm__store_le32(void* p, uint32_t v);

WASM_PRIVATE void wasm__store_le64(void* p, uint64_t v);

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
     WASM_FEATURE_SIMD | WASM_FEATURE_GC | WASM_FEATURE_MEMORY64)

WASM_PRIVATE const char* wasm__feature_name(uint32_t flag);

WASM_PRIVATE wasm_error_t wasm__require_feature(wasm_module_t* mod, uint32_t flag);

WASM_PRIVATE wasm_error_t wasm__require_multi_memory(wasm_module_t* mod);


#endif /* WASM_INTERNAL_COMMON_H */
