#include "u-lib.hh"

unsigned f_helper(unsigned i);

unsigned f(unsigned i) {
    if (i == 0) {
        return i;
    } else {
        return f_helper(i) + i;
    }
}

unsigned f_helper(unsigned i) {
    return f(i - 1);
}


void process_main() {
    console_printf(0x0E00, "Hello from p-recurse process %d\n", sys_getpid());
    for (unsigned i = 0; i < 10; ++i) {
        console_printf(0x0E00, "f(%u) == %u\n", i, f(i));
    }
    console_printf(0x0E00, "Goodbye now!\n");

    while (true) {
        sys_yield();
    }
}
