#include <stdio.h>

void wl_case(void) {
    setbuf(stdout, NULL);
    fputs("stdout-fixture-ok", stdout);
}