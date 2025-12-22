
#ifndef __CPU_HPP__
#define __CPU_HPP__

#include <mm.hpp>
#include <regs.hpp>

namespace xino::cpu {

/** @name CPU registers. */
///@{
namespace R = xino::autogen::regs;
// Import used cpu registers.
using current_el = R::CurrentEL;
using id_aa64mmfr0_el1 = R::ID_AA64MMFR0_EL1;
using id_aa64mmfr1_el1 = R::ID_AA64MMFR1_EL1;
using id_aa64mmfr2_el1 = R::ID_AA64MMFR2_EL1;
using mair_el2 = R::MAIR_EL2;
using sctlr_el2 = R::SCTLR_EL2;
using tcr_el2 = R::TCR_EL2;
using ttbr0_el2 = R::TTBR0_EL2;
using ttbr1_el2 = R::TTBR1_EL2;
using hcr_el2 = R::HCR_EL2;
using vtcr_el2 = R::VTCR_EL2;
using vttbr_el2 = R::VTTBR_EL2;
using daif = R::DAIF;
using daifset = R::DAIFSet;
using daifclr = R::DAIFClr;
///@}

struct cpu_state {
  unsigned pa_bits;
  unsigned ipa_bits;

  bool feat_vhe;

  mair_el2::reg_type mair_el2;
  tcr_el2::reg_type tcr_el2;
  vtcr_el2::reg_type vtcr_el2;
};

// Shared CPU states.
inline struct cpu_state state;

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

/**
 * @brief Invalidate all Stage-1 EL2 TLB entries, inner-shareable.
 * See C5.5.5 `TLBI ALLE2IS`, `TLBI ALLE2ISNXS`, TLB Invalidate All, EL2,
 * Inner Shareable.
 */
[[gnu::always_inline]] inline void tlbi_alle2is() noexcept {
  __asm__ __volatile__("tlbi alle2is" ::: "memory");
}

/**
 * @brief Invalidate all Stage-1 and 2 EL1 TLB entries for all EL1&0 contexts
 *        via EL2, inner-shareable.
 * See C5.5.81 `TLBI VMALLS12E1IS`, `TLBI VMALLS12E1ISNXS`,
 * TLB Invalidate by VMID, All at Stage 1 and 2, EL1, Inner Shareable.
 */
[[gnu::always_inline]] inline void tlbi_vmalls12e1is() noexcept {
  __asm__ __volatile__("tlbi vmalls12e1is" ::: "memory");
}

/**
 * @brief Invalidate a Stage-1 EL2 TLB entry by virtual address,
 * inner-shareable.
 *
 * See C5.5.63 `TLBI VAE2IS`, `TLBI VAE2ISNXS`, TLB Invalidate by VA, EL2,
 * Inner Shareable.
 *
 * @param va VA to invalidate. Low 12 bits are ignored.
 * @param asid ASID to match.
 * @param ttl_hint Optional 4-bit TTL hint (Default: "no hint").
 */
[[gnu::always_inline]] inline void
tlbi_vae2is(xino::mm::virt_addr va, std::uint16_t asid,
            std::uint8_t ttl_hint = 0) noexcept {
  constexpr std::uint64_t mask{(std::uint64_t{1} << 44) - 1};
  // arg[43:0]  = va[55:12].
  // arg[47:44] = 0b00xx (TTL hint, Any level).
  // arg[63:48] = ASID.
  std::uint64_t arg{0};
  arg |= (static_cast<xino::mm::virt_addr::value_type>(va) >> 12) & mask;
  arg |= (static_cast<std::uint64_t>(ttl_hint) & 0xfUL) << 44;
  arg |= static_cast<std::uint64_t>(asid) << 48;
  __asm__ __volatile__("tlbi vae2is, %0" ::"r"(arg) : "memory");
}

/**
 * @brief Invalidate Stage-2 TLB entries by IPA at EL1 via EL2, inner-shareable.
 *
 * See C5.5.14 `TLBI IPAS2E1IS`, `TLBI IPAS2E1ISNXS`, TLB Invalidate by
 * Intermediate Physical Address, Stage 2, EL1, Inner Shareable.
 *
 * @param ipa IPA to invalidate. Low 12 bits are ignored.
 * @param ttl_hint Optional 4-bit TTL hint (Default: "no hint").
 */
[[gnu::always_inline]] inline void
tlbi_ipas2e1is(xino::mm::ipa_addr ipa, std::uint8_t ttl_hint = 0) noexcept {
  constexpr std::uint64_t mask{(std::uint64_t{1} << 44) - 1};
  // arg[43:0]  = ipa[55:12].
  // arg[47:44] = 0b00xx (TTL hint, Any level).
  std::uint64_t arg{0};
  arg |= (static_cast<xino::mm::ipa_addr::value_type>(ipa) >> 12) & mask;
  arg |= (static_cast<std::uint64_t>(ttl_hint) & 0xfUL) << 44;
  __asm__ __volatile__("tlbi ipas2e1is, %0" ::"r"(arg) : "memory");
}

[[noreturn]] void panic();

} // namespace xino::cpu

#endif // __CPU_HPP__