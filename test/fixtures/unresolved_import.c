extern int imported_fn(int value);

int fixture_case(void) {
    return imported_fn(4);
}