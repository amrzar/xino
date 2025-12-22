
#ifndef __CPU_HPP__
#define __CPU_HPP__

#include <regs.hpp>

namespace xino::cpu {

// IMPORT CPU registers:
using xino::autogen::regs::current_el;
using xino::autogen::regs::daif;
using xino::autogen::regs::daifset;
using xino::autogen::regs::hcr_el2;
using xino::autogen::regs::id_aa64mmfr0_el1;
using xino::autogen::regs::id_aa64mmfr1_el1;
using xino::autogen::regs::id_aa64mmfr2_el1;
using xino::autogen::regs::mair_el2;
using xino::autogen::regs::sctlr_el2;
using xino::autogen::regs::tcr_el2;
using xino::autogen::regs::ttbr0_el2;
using xino::autogen::regs::ttbr1_el2;
using xino::autogen::regs::vtcr_el2;
using xino::autogen::regs::vttbr_el2;

/**
 * @brief Wait for event.
 *
 * `WFE` checks a 1-bit local "event register"
 *   - If it is 1, `WFE` clears it to 0 and returns immediately.
 *   - If it is 0, `WFE` may sleep until an event arrives (`SEV`, interrupt,
 *     etc.).
 */
[[gnu::always_inline]] inline void wfe() noexcept {
  __asm__ __volatile__("wfe" ::: "memory");
}

/** @brief Set "event register" for all cores. */
[[gnu::always_inline]] inline void sev() noexcept {
  __asm__ __volatile__("sev" ::: "memory");
}

/**
 * @brief Set the local core "event register".
 *
 * `SEVL` sets the local event register to 1, guaranteeing the *next* `WFE`
 * cannot block. This is used to prevent subtle "sleep too long" hazards in
 * `WFE`-based wait loops (see spin_lock()).
 */
[[gnu::always_inline]] inline void sevl() noexcept {
  __asm__ __volatile__("sevl" ::: "memory");
}

[[noreturn, gnu::noinline]] void panic() {
  for (;;)
    wfe();
}

} // namespace xino::cpu

#endif // __CPU_HPP__