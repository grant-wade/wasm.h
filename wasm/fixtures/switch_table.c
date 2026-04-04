int wl_case(void) {
    volatile int selector = 3;

    switch (selector) {
        case 0:
            return 10;
        case 1:
            return 20;
        case 2:
            return 30;
        case 3:
            return 40;
        default:
            return 50;
    }
}
