#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "hexdump.h"

void f() {
    char local_st[] = "We <3 systems";

    hexdump(local_st, 13);
}

int main() {
    f();
}
