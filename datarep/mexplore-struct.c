#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hexdump.h"

struct ints_and_chars {
  int i1;
  int i2;
  int i3;
  char c1;
  char c2;
  char c3;
};

void f() {
  struct ints_and_chars x;
  x.i1 = 1;

  printf("whole struct:\n");
  hexdump(&x, sizeof(x));

  printf("\nmembers:\n");
  hexdump(&x.i1, sizeof(x.i1));
  hexdump(&x.i2, sizeof(x.i2));
  hexdump(&x.i3, sizeof(x.i3));
  hexdump(&x.c1, sizeof(x.c1));
  hexdump(&x.c2, sizeof(x.c2));
  hexdump(&x.c3, sizeof(x.c3));
}

int main() {
  f();
}
