#include <stddef.h>
#include <string.h>

#if defined(__clang__) || defined(__GNUC__)
#define WL_NOINLINE __attribute__((noinline))
#else
#define WL_NOINLINE
#endif

static volatile unsigned g_copy_len = 40;
static volatile unsigned g_move_len = 24;
static volatile unsigned g_fill_len = 6;

static WL_NOINLINE void bulk_copy(unsigned char* dst, const unsigned char* src, size_t len) {
    __builtin_memcpy(dst, src, len);
}

static WL_NOINLINE void bulk_move(unsigned char* dst, const unsigned char* src, size_t len) {
    __builtin_memmove(dst, src, len);
}

static WL_NOINLINE void bulk_fill(unsigned char* dst, int value, size_t len) {
    __builtin_memset(dst, value, len);
}

int wl_case(void) {
    unsigned char source[64];
    unsigned char target[96];
    size_t copy_len = (size_t)g_copy_len;
    size_t move_len = (size_t)g_move_len;
    size_t fill_len = (size_t)g_fill_len;
    int i;

    for (i = 0; i < (int)sizeof(source); i++) source[i] = (unsigned char)(30 + i);

    bulk_fill(target, 0, sizeof(target));
    bulk_copy(target + 8, source + 3, copy_len);
    bulk_move(target + 20, target + 8, move_len);
    bulk_fill(target + 4, 0x5A, fill_len);

    return (int)target[4] + (int)target[9] + (int)target[20] +
           (int)target[43] + (int)target[44] + (int)target[47];
}