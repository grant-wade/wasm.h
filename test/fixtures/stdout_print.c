#include <stdio.h>

void fixture_case(void) {
    setbuf(stdout, NULL);
    fputs("stdout-fixture-ok", stdout);
}