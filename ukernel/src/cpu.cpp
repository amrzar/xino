
#include <cpu.hpp>

namespace xino::cpu {

[[noreturn]] void panic() {
  for (;;)
    wfe();

  __builtin_unreachable();
}

} // namespace xino::cpu

namespace xino::runtime {

// Shared CPU states (CPU state intersection).
constinit struct xino::cpu::cpu_state cpu_state{};

} // namespace xino::runtime
