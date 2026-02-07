
#include <cpu.hpp>

namespace xino::cpu {

// System-wide "safe" feature snapshot.
constinit struct cpu_feats cpu_feats{};

[[noreturn]] void panic() {
  for (;;)
    wfe();

  __builtin_unreachable();
}

} // namespace xino::cpu
