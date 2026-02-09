#include <stdio.h>
#include <stdlib.h>
#include "hexdump.h"

typedef struct {
} list_t;

int main() {
  list_t* l = (list_t*)malloc(sizeof(list_t));

  printf("sizeof(list_t): %lu\n", sizeof(list_t));

  free(l);
}
