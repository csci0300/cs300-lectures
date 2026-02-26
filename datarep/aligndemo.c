#include <stdlib.h>
#include <string.h>
#include "hexdump.h"

void f() {
    void* data = malloc(19 + 4);
    memset(data, 0xaa, 19);
    int num = 0x12345678;
    //int* p = (int*)(data + 19);
    

    //*p = 0x12345678;
    char* p = (char*)(data + 19);
    memcpy(p, (void *)(&num), 4);

    printf("Address occupies addresses[%p, %p]\n", data, data + 19 + 4 - 1);
    hexdump(data, 19+4);
}

int main() {
  f();
}
