static int global_value = 11;

__attribute__((constructor))
static void init_global_value(void) {
    global_value = 77;
}

int fixture_case(void) {
    return global_value;
}
