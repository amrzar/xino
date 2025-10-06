
#include <cstdio>

void main(void) {
  unsigned long x;
  __asm__ volatile("mrs %0, CurrentEL" : "=r"(x));
  fprintf(stderr, "CurrentEL: %lu\n", x);
}
