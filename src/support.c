#include "internal.h"

#if defined(WASM__NEEDS_CALLOC_FALLBACK)
WASM_PRIVATE void* wasm__calloc_fallback(size_t count, size_t size) {
    size_t total;
    void* ptr;

    if (size != 0 && count > ((size_t)-1) / size) return NULL;

    total = count * size;
    ptr = WASM_MALLOC(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

#endif
#if defined(WASM__SIMD_USE_NEON)
WASM_PRIVATE uint8x16_t wasm__neon_load_u8x16(const wasm_value_t* value) {
    return vld1q_u8(value->of.v128);
}

WASM_PRIVATE int8x16_t wasm__neon_load_i8x16(const wasm_value_t* value) {
    return vreinterpretq_s8_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE uint16x8_t wasm__neon_load_u16x8(const wasm_value_t* value) {
    return vreinterpretq_u16_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE int16x8_t wasm__neon_load_i16x8(const wasm_value_t* value) {
    return vreinterpretq_s16_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE uint32x4_t wasm__neon_load_u32x4(const wasm_value_t* value) {
    return vreinterpretq_u32_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE int32x4_t wasm__neon_load_i32x4(const wasm_value_t* value) {
    return vreinterpretq_s32_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE uint64x2_t wasm__neon_load_u64x2(const wasm_value_t* value) {
    return vreinterpretq_u64_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE int64x2_t wasm__neon_load_i64x2(const wasm_value_t* value) {
    return vreinterpretq_s64_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE float32x4_t wasm__neon_load_f32x4(const wasm_value_t* value) {
    return vreinterpretq_f32_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE float64x2_t wasm__neon_load_f64x2(const wasm_value_t* value) {
    return vreinterpretq_f64_u8(wasm__neon_load_u8x16(value));
}

WASM_PRIVATE wasm_value_t wasm__neon_store_u8x16(uint8x16_t value) {
    wasm_value_t result = wasm_v128_zero();
    vst1q_u8(result.of.v128, value);
    return result;
}

WASM_PRIVATE wasm_value_t wasm__neon_store_u16x8(uint16x8_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_u16(value));
}

WASM_PRIVATE wasm_value_t wasm__neon_store_u32x4(uint32x4_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_u32(value));
}

WASM_PRIVATE wasm_value_t wasm__neon_store_u64x2(uint64x2_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_u64(value));
}

WASM_PRIVATE wasm_value_t wasm__neon_store_f32x4(float32x4_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_f32(value));
}

WASM_PRIVATE wasm_value_t wasm__neon_store_f64x2(float64x2_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_f64(value));
}

WASM_PRIVATE int wasm__neon_any_true_u8x16(uint8x16_t value) {
    uint64x2_t words = vreinterpretq_u64_u8(value);

    return (vgetq_lane_u64(words, 0) | vgetq_lane_u64(words, 1)) != 0u;
}

#endif
#if defined(WASM__SIMD_USE_SSE2)
WASM_PRIVATE __m128i wasm__sse_load_si128(const wasm_value_t* value) {
    return _mm_loadu_si128((const __m128i*)(const void*)value->of.v128);
}

WASM_PRIVATE __m128 wasm__sse_load_ps128(const wasm_value_t* value) {
    return _mm_castsi128_ps(wasm__sse_load_si128(value));
}

WASM_PRIVATE __m128d wasm__sse_load_pd128(const wasm_value_t* value) {
    return _mm_castsi128_pd(wasm__sse_load_si128(value));
}

WASM_PRIVATE wasm_value_t wasm__sse_store_si128(__m128i value) {
    wasm_value_t result = wasm_v128_zero();
    _mm_storeu_si128((__m128i*)(void*)result.of.v128, value);
    return result;
}

WASM_PRIVATE wasm_value_t wasm__sse_store_ps128(__m128 value) {
    return wasm__sse_store_si128(_mm_castps_si128(value));
}

WASM_PRIVATE wasm_value_t wasm__sse_store_pd128(__m128d value) {
    return wasm__sse_store_si128(_mm_castpd_si128(value));
}

WASM_PRIVATE __m128i wasm__sse_select_si128(__m128i mask, __m128i if_true, __m128i if_false) {
    return _mm_or_si128(_mm_and_si128(mask, if_true), _mm_andnot_si128(mask, if_false));
}

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu8(__m128i lhs, __m128i rhs) {
    __m128i bias = _mm_set1_epi8((char)-128);
    return _mm_cmpgt_epi8(_mm_xor_si128(lhs, bias), _mm_xor_si128(rhs, bias));
}

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu16(__m128i lhs, __m128i rhs) {
    __m128i bias = _mm_set1_epi16((short)INT16_MIN);
    return _mm_cmpgt_epi16(_mm_xor_si128(lhs, bias), _mm_xor_si128(rhs, bias));
}

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu32(__m128i lhs, __m128i rhs) {
    __m128i bias = _mm_set1_epi32(INT32_MIN);
    return _mm_cmpgt_epi32(_mm_xor_si128(lhs, bias), _mm_xor_si128(rhs, bias));
}

WASM_PRIVATE __m128i wasm__sse_cmpeq_epi64(__m128i lhs, __m128i rhs) {
    __m128i eq32 = _mm_cmpeq_epi32(lhs, rhs);
    __m128i swapped = _mm_shuffle_epi32(eq32, _MM_SHUFFLE(2, 3, 0, 1));
    return _mm_and_si128(eq32, swapped);
}

WASM_PRIVATE __m128 wasm__sse_abs_ps(__m128 value) {
    return _mm_andnot_ps(_mm_set1_ps(-0.0f), value);
}

WASM_PRIVATE __m128 wasm__sse_neg_ps(__m128 value) {
    return _mm_xor_ps(value, _mm_set1_ps(-0.0f));
}

WASM_PRIVATE __m128d wasm__sse_abs_pd(__m128d value) {
    return _mm_andnot_pd(_mm_set1_pd(-0.0), value);
}

WASM_PRIVATE __m128d wasm__sse_neg_pd(__m128d value) {
    return _mm_xor_pd(value, _mm_set1_pd(-0.0));
}

#endif
WASM_PRIVATE_INLINE int wasm__uses_funcref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_FUNCREF || type == WASM_TYPE_NOFUNC;
}

WASM_PRIVATE_INLINE int wasm__uses_externref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_EXTERNREF || type == WASM_TYPE_NOEXTERN;
}

WASM_PRIVATE_INLINE int wasm__uses_exnref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_EXNREF || type == WASM_TYPE_NOEXN;
}

WASM_PRIVATE_INLINE int wasm__uses_gc_ref_storage(wasm_valtype_t type) {
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

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE uint32_t wasm__platform_wasi_arg_count(void) {
    return (uint32_t)WASM_WASI_ARG_COUNT();
}

WASM_PRIVATE const char* wasm__platform_wasi_arg_at(uint32_t index) {
    (void)index;
    return WASM_WASI_ARG_AT(index);
}

WASM_PRIVATE uint32_t wasm__platform_wasi_env_count(void) {
    return (uint32_t)WASM_WASI_ENV_COUNT();
}

WASM_PRIVATE const char* wasm__platform_wasi_env_at(uint32_t index) {
    (void)index;
    return WASM_WASI_ENV_AT(index);
}

#endif
#if WASM_ENABLE_PLATFORM
#if WASM__PLATFORM_WINDOWS
WASM_PRIVATE uint64_t wasm__platform_windows_filetime_to_unix_ns(FILETIME ft) {
    ULARGE_INTEGER ticks;

    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    if (ticks.QuadPart < 116444736000000000ull) return 0u;
    return (ticks.QuadPart - 116444736000000000ull) * 100ull;
}

WASM_PRIVATE int wasm__platform_windows_qpc_time_ns(uint64_t* out_time) {
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

WASM_PRIVATE int wasm__platform_windows_process_time_ns(uint64_t* out_time) {
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

WASM_PRIVATE int wasm__platform_windows_thread_time_ns(uint64_t* out_time) {
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
#endif
#if WASM_ENABLE_PLATFORM
#if WASM__PLATFORM_POSIX
WASM_PRIVATE int wasm__platform_posix_clock_time_get(uint32_t clock_id, uint64_t* out_time) {
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
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#if WASM__PLATFORM_WINDOWS && defined(_MSC_VER)
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len) {
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

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#if WASM__PLATFORM_WINDOWS && defined(_MSC_VER)
#elif WASM__PLATFORM_POSIX
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    unsigned char* bytes = (unsigned char*)dst;
    FILE* random_file;
    size_t read_count;

    if (len > 0 && !dst) return 0;
    if (len == 0) return 1;

    random_file = fopen("/dev/urandom", "rb");
    if (!random_file) return 0;
    read_count = fread(bytes, 1, len, random_file);
    fclose(random_file);
    return read_count == len;
}

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#if WASM__PLATFORM_WINDOWS && defined(_MSC_VER)
#elif WASM__PLATFORM_POSIX
#else
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    (void)dst;
    return len == 0;
}

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#else
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    return WASM_WASI_FILL_RANDOM(dst, len);
}

#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#if WASM__PLATFORM_WINDOWS
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
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

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#if WASM__PLATFORM_WINDOWS
#elif WASM__PLATFORM_POSIX
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
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

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#if WASM__PLATFORM_WINDOWS
#elif WASM__PLATFORM_POSIX
#else
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
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
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#else
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    return WASM_WASI_CLOCK_TIME_GET(clock_id, precision, out_time);
}

#endif
#endif
