
#include <cpu.hpp>

namespace xino::cpu {

// Shared CPU states (CPU state intersection).
constinit struct cpu_state cpu_state{};

[[noreturn]] void panic() {
  for (;;)
    wfe();

  __builtin_unreachable();
}

} // namespace xino::cpu
