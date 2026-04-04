#include <string.h>

int wl_case(void) {
    static const char source[] = "wasm";
    char target[8];

    memset(target, 0, sizeof(target));
    memcpy(target, source, sizeof(source));
    return (int)target[0] + (int)target[3];
}