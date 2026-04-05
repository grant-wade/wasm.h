extern int imported_fn(int value);

int wl_case(void) {
    return imported_fn(4);
}