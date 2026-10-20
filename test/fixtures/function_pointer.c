static int add_offset(int value) {
    return value + 5;
}

int fixture_case(void) {
    int (*fn)(int) = add_offset;

    return fn(32);
}
