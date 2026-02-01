
#include <cpu.hpp>

namespace xino::cpu {

[[noreturn]] void panic() {
  for (;;)
    wfe();

  __builtin_unreachable();
}

} // namespace xino::cpu
