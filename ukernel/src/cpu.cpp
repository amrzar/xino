
#include <cpu.hpp>

namespace xino::cpu {

[[noreturn]] void panic() {
  for (;;)
    wfe();
}

} // namespace xino::cpu
