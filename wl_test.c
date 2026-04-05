#include <stdio.h>
#include <string.h>
#if defined(WL_ENABLE_PLATFORM) && WL_ENABLE_PLATFORM
#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#define wl_test_getpid _getpid
#define wl_test_rmdir _rmdir
#else
#include <unistd.h>
#define wl_test_getpid getpid
#define wl_test_rmdir rmdir
#endif
#endif

#define WL_IMPL
#include "wl.h"

typedef struct test_alloc {
    wl_alloc_t base;
    usize alloc_calls;
    usize free_calls;
} test_alloc;

typedef struct log_capture {
    usize calls;
    wl_log_level level;
    int line;
    char file[128];
    char message[256];
} log_capture;

typedef struct assert_capture {
    usize calls;
    int line;
    char expr[128];
    char file[128];
    char message[128];
} assert_capture;

static log_capture g_log_capture;
static assert_capture g_assert_capture;

static void copy_text(char *dst, usize dst_size, const char *src) {
    if (dst == NULL || dst_size == 0u) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    (void)snprintf(dst, dst_size, "%s", src);
}

static void *test_alloc_alloc(void *ctx, u64 size) {
    test_alloc *alloc = (test_alloc *)ctx;
    ++alloc->alloc_calls;
    if (size == 0u) {
        size = 1u;
    }
    return malloc((size_t)size);
}

static void test_alloc_free(void *ctx, void *ptr) {
    test_alloc *alloc = (test_alloc *)ctx;
    ++alloc->free_calls;
    free(ptr);
}

static test_alloc test_alloc_make(void) {
    test_alloc alloc;
    alloc.base.alloc = test_alloc_alloc;
    alloc.base.realloc = NULL;
    alloc.base.free = test_alloc_free;
    alloc.alloc_calls = 0u;
    alloc.free_calls = 0u;
    return alloc;
}

static void capture_log(wl_log_level level, const char *file, int line, const char *fmt, va_list args) {
    va_list copy;
    ++g_log_capture.calls;
    g_log_capture.level = level;
    g_log_capture.line = line;
    copy_text(g_log_capture.file, sizeof(g_log_capture.file), file);
    va_copy(copy, args);
    (void)vsnprintf(g_log_capture.message, sizeof(g_log_capture.message), fmt, copy);
    va_end(copy);
}

static void capture_assert(const char *expr, const char *file, int line, const char *msg) {
    ++g_assert_capture.calls;
    g_assert_capture.line = line;
    copy_text(g_assert_capture.expr, sizeof(g_assert_capture.expr), expr);
    copy_text(g_assert_capture.file, sizeof(g_assert_capture.file), file);
    copy_text(g_assert_capture.message, sizeof(g_assert_capture.message), msg);
}

static int cmp_int_asc(const void *a, const void *b, void *ctx) {
    int lhs = *(const int *)a;
    int rhs = *(const int *)b;
    WL_UNUSED(ctx);
    return (lhs > rhs) - (lhs < rhs);
}

static int cmp_int_dir(const void *a, const void *b, void *ctx) {
    int dir = ctx != NULL ? *(const int *)ctx : 1;
    return dir * cmp_int_asc(a, b, NULL);
}

static u64 collision_hash(const void *key, usize key_size) {
    WL_UNUSED(key);
    WL_UNUSED(key_size);
    return 1u;
}

static bool int_eq(const void *lhs, const void *rhs, usize key_size) {
    WL_UNUSED(key_size);
    return *(const int *)lhs == *(const int *)rhs;
}

#if WL_ENABLE_PLATFORM
static void make_temp_name(char *buf, usize buf_size, const char *label) {
    u64 salt = (u64)wl_time_wall().ns ^ ((u64)(unsigned int)wl_test_getpid() << 16u);
    (void)snprintf(buf, buf_size, ".wl_test_%s_%llu_%u", label, (unsigned long long)salt, (unsigned int)wl_test_getpid());
}
#endif

WL_TEST(test_status_and_alloc) {
    test_alloc alloc = test_alloc_make();
    char *ptr;
    char *grown;

    WL_CHECK(t, strcmp(wl_status_str(WL_OK), "ok") == 0);
    WL_CHECK(t, strcmp(wl_status_str(WL_ERR_NOMEM), "out of memory") == 0);
    WL_CHECK(t, strcmp(wl_status_str((wl_status)12345), "unknown status") == 0);

    ptr = wl_alloc(NULL, 8u);
    WL_REQUIRE(t, ptr != NULL);
    memcpy(ptr, "abc", 4u);
    grown = wl_realloc(NULL, ptr, 16u);
    WL_REQUIRE(t, grown != NULL);
    WL_CHECK(t, strcmp(grown, "abc") == 0);
    wl_free(NULL, grown);

    ptr = wl_alloc(&alloc, 4u);
    WL_REQUIRE(t, ptr != NULL);
    memcpy(ptr, "hey", 4u);
    grown = wl_alloc_resize(&alloc.base, ptr, 4u, 9u);
    WL_REQUIRE(t, grown != NULL);
    WL_CHECK(t, memcmp(grown, "hey", 4u) == 0);
    wl_free(&alloc, grown);
    WL_CHECK(t, alloc.alloc_calls == 2u);
    WL_CHECK(t, alloc.free_calls == 2u);
}

WL_TEST(test_arena) {
    wl_arena arena;
    wl_arena_savepoint mark;
    void *first;
    void *scratch;
    void *scratch_again;
    void *aligned;
    uintptr_t aligned_value;
    wl_status status;

    WL_CHECK(t, wl_arena_init(NULL, 16u, NULL) == WL_ERR_INVALID);
    status = wl_arena_init(&arena, 128u, NULL);
    WL_REQUIRE(t, status == WL_OK);

    first = wl_arena_alloc(&arena, 1u);
    WL_REQUIRE(t, first != NULL);
    mark = wl_arena_save(&arena);
    scratch = wl_arena_alloc(&arena, 24u);
    WL_REQUIRE(t, scratch != NULL);
    wl_arena_restore(&arena, mark);
    scratch_again = wl_arena_alloc(&arena, 24u);
    WL_CHECK(t, scratch == scratch_again);

    wl_arena_reset(&arena);
    (void)wl_arena_alloc(&arena, 1u);
    aligned = wl_arena_alloc(&arena, sizeof(wl__max_align));
    WL_REQUIRE(t, aligned != NULL);
    aligned_value = (uintptr_t)aligned;
    WL_CHECK(t, aligned_value % WL_ALIGNOF(wl__max_align) == 0u);
    WL_CHECK(t, wl_arena_alloc(&arena, 1024u) == NULL);

    wl_arena_destroy(&arena);
    WL_CHECK(t, arena.buf == NULL);
    WL_CHECK(t, arena.cap == 0u);
    WL_CHECK(t, arena.pos == 0u);
}

WL_TEST(test_logging_and_assert) {
    wl_log_level old_level = wl_log_get_level();

    memset(&g_log_capture, 0, sizeof(g_log_capture));
    wl_log_set_callback(capture_log);
    wl_log_set_level(WL_LOG_WARN);
    wl_log_info("suppressed %d", 1);
    wl_log_error("captured %d", 7);
    WL_CHECK(t, g_log_capture.calls == 1u);
    WL_CHECK(t, g_log_capture.level == WL_LOG_ERROR);
    WL_CHECK(t, strstr(g_log_capture.message, "captured 7") != NULL);
    WL_CHECK(t, strstr(g_log_capture.file, "wl_test.c") != NULL);
    wl_log_set_callback(NULL);
    wl_log_set_level(old_level);

    memset(&g_assert_capture, 0, sizeof(g_assert_capture));
    wl_assert_set_handler(capture_assert);
#ifndef NDEBUG
    wl_assertm(2 + 2 == 5, "captured");
    WL_CHECK(t, g_assert_capture.calls == 1u);
    WL_CHECK(t, strstr(g_assert_capture.expr, "2 + 2 == 5") != NULL);
    WL_CHECK(t, strcmp(g_assert_capture.message, "captured") == 0);
#else
    wl_assert_fail("manual", __FILE__, __LINE__, "captured");
    WL_CHECK(t, g_assert_capture.calls == 1u);
    WL_CHECK(t, strcmp(g_assert_capture.expr, "manual") == 0);
#endif
    WL_CHECK(t, strstr(g_assert_capture.file, "wl_test.c") != NULL);
    wl_assert_set_handler(NULL);
}

WL_TEST(test_dynamic_array) {
    int *arr = NULL;
    wl_arena arena;
    int *arena_arr;
    wl_status status;
    usize old_cap;

    WL_CHECK(t, wl_arr_len(arr) == 0u);
    for (int i = 0; i < 100; ++i) {
        status = wl_arr_push(arr, i * 3);
        WL_REQUIRE_MSG(t, status == WL_OK, "push failed at %d", i);
    }
    WL_CHECK(t, wl_arr_len(arr) == 100u);
    WL_CHECK(t, wl_arr_last(arr) == 297);
    WL_CHECK(t, wl_arr_pop(arr) == 297);
    WL_CHECK(t, wl_arr_len(arr) == 99u);
    old_cap = wl_arr_cap(arr);
    wl_arr_clear(arr);
    WL_CHECK(t, wl_arr_len(arr) == 0u);
    WL_CHECK(t, wl_arr_cap(arr) == old_cap);
    wl_arr_free(arr);
    WL_CHECK(t, arr == NULL);

    status = wl_arena_init(&arena, WL_KB(8), NULL);
    WL_REQUIRE(t, status == WL_OK);
    arena_arr = wl_arr_init(int, &arena);
    WL_REQUIRE(t, arena_arr != NULL);
    for (int i = 0; i < 16; ++i) {
        WL_REQUIRE(t, wl_arr_push(arena_arr, i) == WL_OK);
    }
    WL_CHECK(t, wl_arr_len(arena_arr) == 16u);
    wl_arr_free(arena_arr);
    WL_CHECK(t, arena_arr == NULL);
    wl_arena_destroy(&arena);
}

WL_TEST(test_str_views) {
    wl_str empty = wl_str_from_cstr(NULL);
    wl_str haystack = wl_str_lit("bananas");

    WL_CHECK(t, empty.ptr == NULL);
    WL_CHECK(t, empty.len == 0u);
    WL_CHECK(t, wl_str_eq(wl_str_lit("abc"), wl_str_lit("abc")));
    WL_CHECK(t, !wl_str_eq(wl_str_lit("abc"), wl_str_lit("abd")));
    WL_CHECK(t, wl_str_starts_with(haystack, wl_str_lit("ban")));
    WL_CHECK(t, wl_str_ends_with(haystack, wl_str_lit("nas")));
    WL_CHECK(t, wl_str_search(haystack, wl_str_lit("ana")) == 1);
    WL_CHECK(t, wl_str_search(haystack, wl_str_lit("")) == 0);
    WL_CHECK(t, wl_str_search(haystack, wl_str_lit("s")) == 6);
    WL_CHECK(t, wl_str_search(haystack, wl_str_lit("banana split")) == -1);
    WL_CHECK(t, !wl_str_starts_with(haystack, wl_str_lit("nan")));
}

WL_TEST(test_owned_string) {
    wl_string s = wl_string_from("short", NULL);
    wl_string other;
    wl_arena arena;
    wl_string arena_string;
    wl_status status;

    WL_CHECK(t, s.data == NULL);
    status = wl_string_append_char(&s, '!');
    WL_REQUIRE(t, status == WL_OK);
    status = wl_string_append_cstr(&s, " this string should force heap growth");
    WL_REQUIRE(t, status == WL_OK);
    WL_CHECK(t, s.data != NULL);
    WL_CHECK(t, wl_string_len(&s) > WL_STRING_SSO_CAP);
    WL_CHECK(t, wl_string_find(&s, wl_str_lit("force")) >= 0);

    other = wl_string_from_str(wl_string_as_str(&s), NULL);
    WL_CHECK(t, wl_string_eq(&s, &other));
    WL_CHECK(t, wl_string_cmp(&s, &other) == 0);
    wl_string_clear(&other);
    WL_CHECK(t, wl_string_len(&other) == 0u);
    WL_CHECK(t, strcmp(wl_string_cstr(&other), "") == 0);
    WL_CHECK(t, wl_string_cmp(&other, &s) < 0);
    wl_string_destroy(&other);

    status = wl_arena_init(&arena, WL_KB(8), NULL);
    WL_REQUIRE(t, status == WL_OK);
    arena_string = wl_string_from("arena", &arena);
    WL_CHECK(t, wl_string_append_cstr(&arena_string, " string") == WL_OK);
    WL_CHECK(t, strcmp(wl_string_cstr(&arena_string), "arena string") == 0);
    wl_string_destroy(&arena_string);
    wl_arena_destroy(&arena);

    wl_string_destroy(&s);
    WL_CHECK(t, wl_string_len(&s) == 0u);
    WL_CHECK(t, strcmp(wl_string_cstr(&s), "") == 0);
}

WL_TEST(test_utf8) {
    static const u8 overlong[] = {0xc0u, 0xafu};
    static const u8 surrogate[] = {0xedu, 0xa0u, 0x80u};
    static const u8 truncated[] = {0xe2u, 0x82u};
    static const u8 globe[] = {0xf0u, 0x9fu, 0x8cu, 0x8du};
    wl_str utf = wl_str_lit("héllo 🌍");
    usize off = 0u;
    u32 cps[8];
    usize count = 0u;
    u8 encoded[4] = {0u, 0u, 0u, 0u};

    WL_CHECK(t, wl_utf8_valid(utf));
    WL_CHECK(t, wl_utf8_len(utf) == 7u);
    WL_CHECK(t, wl_utf8_decode(utf, utf.len) == WL_UTF8_INVALID);
    WL_CHECK(t, wl_utf8_next(utf, NULL) == WL_UTF8_INVALID);
    while (off < utf.len) {
        cps[count++] = wl_utf8_next(utf, &off);
    }
    WL_CHECK(t, count == 7u);
    WL_CHECK(t, cps[0] == 'h');
    WL_CHECK(t, cps[1] == 0x00e9u);
    WL_CHECK(t, cps[6] == 0x1f30du);
    WL_CHECK(t, wl_utf8_encode(encoded, 0x1f30du) == 4u);
    WL_CHECK(t, memcmp(encoded, globe, sizeof(globe)) == 0);
    WL_CHECK(t, wl_utf8_encode(encoded, 0x110000u) == 0u);
    WL_CHECK(t, !wl_utf8_valid(wl_str_make((const char *)overlong, sizeof(overlong))));
    WL_CHECK(t, !wl_utf8_valid(wl_str_make((const char *)surrogate, sizeof(surrogate))));
    WL_CHECK(t, !wl_utf8_valid(wl_str_make((const char *)truncated, sizeof(truncated))));
}

WL_TEST(test_hash_functions) {
    u64 h1 = wl_hash_bytes("abc", 3u, 0u);
    u64 h2 = wl_hash_bytes("abc", 3u, 0u);
    u64 h3 = wl_hash_bytes("abc", 3u, 123u);
    u64 hz = wl_hash_bytes(NULL, 0u, 0u);

    WL_CHECK(t, h1 == h2);
    WL_CHECK(t, h1 != 0u);
    WL_CHECK(t, h3 != h1);
    WL_CHECK(t, hz != 0u);
    WL_CHECK(t, wl_hash_str(wl_str_lit("abc")) == h1);
    WL_CHECK(t, wl_hash_u64(42u) == wl_hash_u64(42u));
    WL_CHECK(t, wl_hash_u64(42u) != wl_hash_u64(43u));
}

WL_TEST(test_hashmap_basic) {
    wl_hashmap map;
    wl_hashmap_iter iter;
    void *key_ptr;
    void *val_ptr;
    long long sum = 0;
    usize seen = 0u;

    WL_CHECK(t, wl_hashmap_init(NULL, sizeof(int), sizeof(int), NULL, NULL, NULL) == WL_ERR_INVALID);
    WL_REQUIRE(t, wl_hashmap_init(&map, sizeof(int), sizeof(int), NULL, NULL, NULL) == WL_OK);
    for (int i = 0; i < 128; ++i) {
        int value = i * 11;
        WL_REQUIRE_MSG(t, wl_hashmap_put(&map, &i, &value) == WL_OK, "put failed at %d", i);
    }
    for (int i = 0; i < 128; ++i) {
        int *value = wl_hashmap_get(&map, &i);
        WL_REQUIRE_MSG(t, value != NULL, "missing key %d", i);
        WL_CHECK(t, *value == i * 11);
    }
    {
        int key = 7;
        int value = 999;
        WL_CHECK(t, wl_hashmap_put(&map, &key, &value) == WL_OK);
        WL_CHECK(t, *(int *)wl_hashmap_get(&map, &key) == 999);
    }
    {
        int missing = 500;
        WL_CHECK(t, wl_hashmap_get(&map, &missing) == NULL);
        WL_CHECK(t, !wl_hashmap_remove(&map, &missing));
    }

    iter = wl_hashmap_iter_new(&map);
    while (wl_hashmap_iter_next(&iter, &key_ptr, &val_ptr)) {
        sum += *(int *)key_ptr;
        sum += *(int *)val_ptr;
        ++seen;
    }
    WL_CHECK(t, seen == 128u);
    WL_CHECK(t, sum > 0);

    wl_hashmap_clear(&map);
    WL_CHECK(t, wl_hashmap_count(&map) == 0u);
    {
        int key = 7;
        WL_CHECK(t, wl_hashmap_get(&map, &key) == NULL);
    }
    wl_hashmap_destroy(&map);
}

WL_TEST(test_hashmap_collisions_and_removal) {
    wl_hashmap map;

    WL_REQUIRE(t, wl_hashmap_init(&map, sizeof(int), sizeof(int), collision_hash, int_eq, NULL) == WL_OK);
    for (int i = 0; i < 64; ++i) {
        int value = -i;
        WL_REQUIRE(t, wl_hashmap_put(&map, &i, &value) == WL_OK);
    }
    for (int i = 0; i < 64; i += 2) {
        WL_CHECK(t, wl_hashmap_remove(&map, &i));
    }
    for (int i = 0; i < 64; ++i) {
        int *value = wl_hashmap_get(&map, &i);
        if ((i & 1) == 0) {
            WL_CHECK(t, value == NULL);
        } else {
            WL_REQUIRE(t, value != NULL);
            WL_CHECK(t, *value == -i);
        }
    }
    for (int i = 100; i < 116; ++i) {
        int value = i + 1;
        WL_REQUIRE(t, wl_hashmap_put(&map, &i, &value) == WL_OK);
    }
    WL_CHECK(t, wl_hashmap_count(&map) == 48u);
    wl_hashmap_destroy(&map);
}

WL_TEST(test_sort_and_search) {
    int small_values[] = {9, 1, 8, 2, 7, 3, 6, 4, 5, 0};
    int large_values[64];
    int dir = -1;
    int key = 9;
    int missing = 999;

    for (int i = 0; i < 64; ++i) {
        large_values[i] = i;
    }
    for (int i = 0; i < 64; ++i) {
        int j = (i * 17) & 63;
        int tmp = large_values[i];
        large_values[i] = large_values[j];
        large_values[j] = tmp;
    }

    wl_sort(small_values, WL_COUNTOF(small_values), sizeof(small_values[0]), cmp_int_asc, NULL);
    for (usize i = 1u; i < WL_COUNTOF(small_values); ++i) {
        WL_CHECK(t, small_values[i - 1u] <= small_values[i]);
    }

    wl_sort(large_values, WL_COUNTOF(large_values), sizeof(large_values[0]), cmp_int_dir, &dir);
    for (usize i = 1u; i < WL_COUNTOF(large_values); ++i) {
        WL_CHECK(t, large_values[i - 1u] >= large_values[i]);
    }
    WL_CHECK(t, wl_bsearch(small_values, WL_COUNTOF(small_values), sizeof(small_values[0]), &key, cmp_int_asc, NULL) == 9);
    WL_CHECK(t, wl_bsearch(small_values, WL_COUNTOF(small_values), sizeof(small_values[0]), &missing, cmp_int_asc, NULL) == -1);
}

WL_TEST(test_rng) {
    wl_rng a = wl_rng_seed(1234u);
    wl_rng b = wl_rng_seed(1234u);
    u8 fill_a[31];
    u8 fill_b[31];

    for (int i = 0; i < 16; ++i) {
        WL_CHECK(t, wl_rng_u32(&a) == wl_rng_u32(&b));
    }

    a = wl_rng_seed(55u);
    b = wl_rng_seed(55u);
    wl_rng_fill(&a, fill_a, sizeof(fill_a));
    wl_rng_fill(&b, fill_b, sizeof(fill_b));
    WL_CHECK(t, memcmp(fill_a, fill_b, sizeof(fill_a)) == 0);

    a = wl_rng_seed(42u);
    WL_CHECK(t, wl_rng_range(&a, 10u, 20u) >= 10u);
    WL_CHECK(t, wl_rng_range(&a, 10u, 20u) < 20u);
    WL_CHECK(t, wl_rng_range(&a, 7u, 7u) == 7u);
    {
        f64 value = wl_rng_f64(&a);
        WL_CHECK(t, value >= 0.0);
        WL_CHECK(t, value < 1.0);
    }
}

#if WL_ENABLE_PLATFORM
WL_TEST(test_time) {
    wl_time base = {1000ll};
    wl_duration delta = wl_ms(5);
    wl_time added = wl_time_add(base, delta);
    wl_duration diff = wl_time_diff(added, base);
    wl_time now_a = wl_time_now();
    wl_time now_b = wl_time_now();
    wl_time wall = wl_time_wall();

    WL_CHECK(t, wl_ns(12).ns == 12ll);
    WL_CHECK(t, wl_us(3).ns == 3000ll);
    WL_CHECK(t, wl_ms(5).ns == 5000000ll);
    WL_CHECK(t, wl_sec(2).ns == 2000000000ll);
    WL_CHECK(t, added.ns == 5001000ll);
    WL_CHECK(t, diff.ns == delta.ns);
    WL_CHECK(t, now_b.ns >= now_a.ns);
    WL_CHECK(t, wall.ns > 0ll);
    WL_CHECK(t, wl_sleep(wl_ms(1)) == WL_OK);
}

WL_TEST(test_fs_and_paths) {
    static const char payload[] = "abc123\nline2";
    char base[160];
    char nested[192];
    char file_path[224];
    char missing_path[224];
    char joined[64];
    test_alloc alloc = test_alloc_make();
    u8 *buf = NULL;
    usize len = 0u;
    wl_status status;

    make_temp_name(base, sizeof(base), "dir");
    WL_REQUIRE_MSG(t, wl_path_join(nested, sizeof(nested), wl_str_from_cstr(base), wl_str_lit("a/b")) == WL_OK, "join nested failed");
    WL_REQUIRE_MSG(t, wl_path_join(file_path, sizeof(file_path), wl_str_from_cstr(nested), wl_str_lit("file.txt")) == WL_OK, "join file failed");
    WL_REQUIRE_MSG(t, wl_path_join(missing_path, sizeof(missing_path), wl_str_from_cstr(base), wl_str_lit("missing.txt")) == WL_OK, "join missing failed");

    status = wl_fs_mkdir(nested, true);
    WL_REQUIRE(t, status == WL_OK);
    WL_CHECK(t, wl_fs_exists(base));
    WL_CHECK(t, wl_fs_exists(nested));
    WL_CHECK(t, wl_fs_mkdir(nested, true) == WL_OK);

    status = wl_fs_write_file(file_path, payload, sizeof(payload) - 1u);
    WL_REQUIRE(t, status == WL_OK);
    WL_CHECK(t, wl_fs_exists(file_path));

    status = wl_fs_read_file(file_path, &alloc, &buf, &len);
    WL_REQUIRE(t, status == WL_OK);
    WL_CHECK(t, len == sizeof(payload) - 1u);
    WL_CHECK(t, memcmp(buf, payload, len) == 0);
    wl_free(&alloc, buf);
    WL_CHECK(t, alloc.alloc_calls == 1u);
    WL_CHECK(t, alloc.free_calls == 1u);
    WL_CHECK(t, wl_fs_read_file(missing_path, NULL, &buf, &len) == WL_ERR_NOT_FOUND);

    WL_CHECK(t, wl_path_join(joined, sizeof(joined), wl_str_lit("tmp"), wl_str_lit("file.txt")) == WL_OK);
    WL_CHECK(t, strcmp(joined, "tmp/file.txt") == 0);
    WL_CHECK(t, wl_path_join(joined, sizeof(joined), wl_str_lit("tmp/"), wl_str_lit("file.txt")) == WL_OK);
    WL_CHECK(t, strcmp(joined, "tmp/file.txt") == 0);
    WL_CHECK(t, wl_path_join(joined, 4u, wl_str_lit("tmp"), wl_str_lit("file.txt")) == WL_ERR_OVERFLOW);
    WL_CHECK(t, wl_str_eq(wl_path_ext(wl_str_lit("tmp/file.txt")), wl_str_lit(".txt")));
    WL_CHECK(t, wl_str_eq(wl_path_stem(wl_str_lit("tmp/file.txt")), wl_str_lit("file")));
    WL_CHECK(t, wl_str_eq(wl_path_dir(wl_str_lit("tmp/file.txt")), wl_str_lit("tmp")));
    WL_CHECK(t, wl_path_ext(wl_str_lit("README")).len == 0u);
    WL_CHECK(t, wl_str_eq(wl_path_stem(wl_str_lit("README")), wl_str_lit("README")));
    WL_CHECK(t, wl_path_dir(wl_str_lit("README")).len == 0u);

    WL_CHECK(t, wl_fs_remove(file_path) == WL_OK);
    WL_CHECK(t, wl_fs_remove(file_path) == WL_ERR_NOT_FOUND);
    (void)wl_test_rmdir(nested);
    {
        char parent[192];
        WL_REQUIRE(t, wl_path_join(parent, sizeof(parent), wl_str_from_cstr(base), wl_str_lit("a")) == WL_OK);
        (void)wl_test_rmdir(parent);
    }
    (void)wl_test_rmdir(base);
}
#endif

int main(void) {
    static const wl_test_case cases[] = {
        WL_TEST_CASE(test_status_and_alloc),
        WL_TEST_CASE(test_arena),
        WL_TEST_CASE(test_logging_and_assert),
        WL_TEST_CASE(test_dynamic_array),
        WL_TEST_CASE(test_str_views),
        WL_TEST_CASE(test_owned_string),
        WL_TEST_CASE(test_utf8),
        WL_TEST_CASE(test_hash_functions),
        WL_TEST_CASE(test_hashmap_basic),
        WL_TEST_CASE(test_hashmap_collisions_and_removal),
        WL_TEST_CASE(test_sort_and_search),
        WL_TEST_CASE(test_rng),
#if WL_ENABLE_PLATFORM
        WL_TEST_CASE(test_time),
        WL_TEST_CASE(test_fs_and_paths),
#endif
    };

    return wl_test_run("wl", cases, WL_COUNTOF(cases));
}