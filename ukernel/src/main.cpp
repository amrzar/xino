
#include <cstdio>

namespace xino::runtime {

void main() {
  unsigned long x;
  __asm__ volatile("mrs %0, CurrentEL" : "=r"(x));
  fprintf(stderr, "CurrentEL: %lu\n", x);
}

} // namespace xino::runtime
