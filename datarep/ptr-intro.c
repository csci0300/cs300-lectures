#include <stdio.h>

void f() {
    int local = 1;

    printf("value of local: %d\n", local);
    printf("address of local: %p\n", &local);
}

int main() {
  f();
}
