#include <stdint.h>

int wl_case(void) {
    volatile int32_t byte_value = 0x0000FF80;
    volatile int32_t short_value = 0x00008001;

    return (int)(int8_t)byte_value + (int)(int16_t)short_value;
}
