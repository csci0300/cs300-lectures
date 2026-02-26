#include <stdlib.h>
#include <string.h>
#include "hexdump.h"

void f() {
    void* data = malloc(19);
    memset(data, 0xaa, 19);

    printf("Address occupies addresses[%p, %p]\n", data, data + 19 - 1);
    hexdump(data, 19);
}

int main() {
  f();
}
