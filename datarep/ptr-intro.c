#include <stdio.h>

void g(int* x) {
  *x = 42;  // dereferencing a pointer
}

void f() {
    int local = 1;

    int* pointer = &local;

    g(pointer);

    printf("address of pointer: %p\n", &pointer);

    int** pointerpointer = &pointer;

    printf("value of local: %d\n", **pointerpointer);
}

int main() {
  f();
}
