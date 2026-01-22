
#include <barrier.hpp>
#include <cpu.hpp>
#include <mm_paging.hpp>

namespace xino::mm::paging {

[[nodiscard]] static unsigned parange_bits() noexcept {
  switch (xino::cpu::id_aa64mmfr0_el1::read_pa_range()) {
  case xino::cpu::id_aa64mmfr0_el1::pa_range::pa_32_bits:
    return 32;
  case xino::cpu::id_aa64mmfr0_el1::pa_range::pa_36_bits:
    return 36;
  case xino::cpu::id_aa64mmfr0_el1::pa_range::pa_40_bits:
    return 40;
  case xino::cpu::id_aa64mmfr0_el1::pa_range::pa_42_bits:
    return 42;
  case xino::cpu::id_aa64mmfr0_el1::pa_range::pa_44_bits:
    return 44;
  case xino::cpu::id_aa64mmfr0_el1::pa_range::pa_48_bits:
    return 48;
  // case xino::cpu::id_aa64mmfr0_el1::pa_range::pa_52_bits:
  // case xino::cpu::id_aa64mmfr0_el1::pa_range::pa_56_bits:
  default:
    return 48;
  }
}

// Use for TCR_EL2.IPS and VTCR_EL2.PS.
[[nodiscard]] static unsigned ps_for_bits(unsigned bits) noexcept {
  if (bits <= 32) {
    // xino::cpu::tcr_el2::ips::pa_32_bits;
    // xino::cpu::vtcr_el2::ps::pa_32_bits;
    return 0b000U;
  }

  if (bits <= 36) {
    // xino::cpu::tcr_el2::ips::pa_36_bits;
    // xino::cpu::vtcr_el2::ps::pa_36_bits;
    return 0b001U;
  }

  if (bits <= 40) {
    // xino::cpu::tcr_el2::ips::pa_40_bits;
    // xino::cpu::vtcr_el2::ps::pa_40_bits;
    return 0b010U;
  }

  if (bits <= 42) {
    // xino::cpu::tcr_el2::ips::pa_42_bits;
    // xino::cpu::vtcr_el2::ps::pa_42_bits;
    return 0b011U;
  }

  if (bits <= 44) {
    // xino::cpu::tcr_el2::ips::pa_44_bits;
    // xino::cpu::vtcr_el2::ps::pa_44_bits;
    return 0b100U;
  }

  // Default:
  // xino::cpu::tcr_el2::ips::pa_48_bits;
  // xino::cpu::vtcr_el2::ps::pa_48_bits;
  return 0b101U;
}

[[nodiscard, gnu::unused]] static bool gran4_s1_supported() noexcept {
  // xino::cpu::id_aa64mmfr0_el1::t_gran4::supported ||
  // xino::cpu::id_aa64mmfr0_el1::t_gran4::large_pa_52_bits
  return xino::cpu::id_aa64mmfr0_el1::read_t_gran4() !=
         xino::cpu::id_aa64mmfr0_el1::t_gran4::not_supported;
}

[[nodiscard, gnu::unused]] static bool gran16_s1_supported() noexcept {
  // xino::cpu::id_aa64mmfr0_el1::t_gran16::supported ||
  // xino::cpu::id_aa64mmfr0_el1::t_gran16::large_pa_52_bits
  return xino::cpu::id_aa64mmfr0_el1::read_t_gran16() !=
         xino::cpu::id_aa64mmfr0_el1::t_gran16::not_supported;
}

[[nodiscard, gnu::unused]] static bool gran4_s2_supported() noexcept {
  xino::cpu::id_aa64mmfr0_el1::reg_type gran4 =
      xino::cpu::id_aa64mmfr0_el1::read_t_gran4_2();

  if (gran4 == xino::cpu::id_aa64mmfr0_el1::t_gran4_2::t_gran4)
    return gran4_s1_supported(); // Check ID_AA64MMFR0_EL1.TGran4.
  // xino::cpu::id_aa64mmfr0_el1::t_gran4_2::supported ||
  // xino::cpu::id_aa64mmfr0_el1::t_gran4_2::large_pa_52_bits
  return gran4 != xino::cpu::id_aa64mmfr0_el1::t_gran4_2::not_supported;
}

[[nodiscard, gnu::unused]] static bool gran16_s2_supported() noexcept {
  xino::cpu::id_aa64mmfr0_el1::reg_type gran16 =
      xino::cpu::id_aa64mmfr0_el1::read_t_gran16_2();

  if (gran16 == xino::cpu::id_aa64mmfr0_el1::t_gran16_2::t_gran16)
    return gran16_s1_supported(); // Check ID_AA64MMFR0_EL1.TGran16.
  // xino::cpu::id_aa64mmfr0_el1::t_gran16_2::supported ||
  // xino::cpu::id_aa64mmfr0_el1::t_gran16_2::large_pa_52_bits
  return gran16 != xino::cpu::id_aa64mmfr0_el1::t_gran16_2::not_supported;
}

[[nodiscard]] static xino::cpu::tcr_el2::reg_type tcr_tg0() noexcept {
#if defined(UKERNEL_PAGE_4K)
  return xino::cpu::tcr_el2::tg0::granule_4k;
#elif defined(UKERNEL_PAGE_16K)
  return xino::cpu::tcr_el2::tg0::granule_16k;
#endif
}

[[nodiscard]] static xino::cpu::tcr_el2::reg_type tcr_tg1() noexcept {
#if defined(UKERNEL_PAGE_4K)
  return xino::cpu::tcr_el2::tg1::granule_4k;
#elif defined(UKERNEL_PAGE_16K)
  return xino::cpu::tcr_el2::tg1::granule_16k;
#endif
}

[[nodiscard]] static xino::cpu::vtcr_el2::reg_type
vtcr_sl0(unsigned ipa_bits) noexcept {
  unsigned root = root_hw_level_for_bits(ipa_bits);

#if defined(UKERNEL_PAGE_4K)
  switch (root) {
  case 0:
    return 0b10UL;
  case 1:
    return 0b01UL;
  case 2:
    return 0b00UL;
  case 3:
    if (xino::cpu::id_aa64mmfr2_el1::read_st())
      return 0b11UL;
  default:
    break;
  }
#elif defined(UKERNEL_PAGE_16K)
  switch (root) {
  case 1:
    return 0b10UL;
  case 2:
    return 0b01UL;
  case 3:
    return 0b00UL;
  // case 0:
  default:
    break;
  }
#endif

  xino::cpu::panic();
}

[[nodiscard]] static xino::cpu::vtcr_el2::reg_type vtcr_tg0() noexcept {
#if defined(UKERNEL_PAGE_4K)
  return xino::cpu::vtcr_el2::tg0::granule_4k;
#elif defined(UKERNEL_PAGE_16K)
  return xino::cpu::vtcr_el2::tg0::granule_16k;
#endif
}

// D24.2.122 MAIR_EL2, Memory Attribute Indirection Register.
[[nodiscard]] static xino::cpu::mair_el2::reg_type make_mair_el2() noexcept {
  using xino::cpu::mair_el2;

  mair_el2::reg_type attr_normal = 0xffU; // Attr0.
  mair_el2::reg_type attr_device = 0x00U; // Attr1, and others.

  return static_cast<mair_el2::reg_type>((attr_normal << 0) |
                                         (attr_device << 8));
}

// D24.2.183 TCR_EL2, Translation Control Register, When ELIsInHost.
[[nodiscard]] static xino::cpu::tcr_el2::reg_type
make_tcr_el2(unsigned pa_bits, unsigned va_bits) noexcept {
  using xino::cpu::tcr_el2;

  tcr_el2::reg_type tcr{0};

  // Setup TTBR0_EL2 attributes.
  tcr |= tcr_el2::t0sz::encode(64 - va_bits);
  tcr |= tcr_el2::irgn0::encode(tcr_el2::irgn0::wb_with_wa);
  tcr |= tcr_el2::orgn0::encode(tcr_el2::orgn0::wb_with_wa);
  tcr |= tcr_el2::sh0::encode(tcr_el2::sh0::inner_shareable);
  tcr |= tcr_el2::tg0::encode(tcr_tg0());

  // Setup TTBR1_EL2 attributes.
  tcr |= tcr_el2::t1sz::encode(64 - va_bits);
  tcr |= tcr_el2::irgn1::encode(tcr_el2::irgn1::wb_with_wa);
  tcr |= tcr_el2::orgn1::encode(tcr_el2::orgn1::wb_with_wa);
  tcr |= tcr_el2::sh1::encode(tcr_el2::sh1::inner_shareable);
  tcr |= tcr_el2::tg1::encode(tcr_tg1());

  tcr |= tcr_el2::ips::encode(ps_for_bits(pa_bits));

  return tcr;
}

// D24.2.210 VTCR_EL2, Virtualization Translation Control Register.
[[nodiscard]] static xino::cpu::vtcr_el2::reg_type
make_vtcr_el2(unsigned pa_bits, unsigned ipa_bits) noexcept {
  using xino::cpu::vtcr_el2;

  vtcr_el2::reg_type vtcr{0};

  // Setup VTTBR_EL2 attributes.
  vtcr |= vtcr_el2::t0sz::encode(64 - ipa_bits);
  vtcr |= vtcr_el2::irgn0::encode(vtcr_el2::irgn0::wb_with_wa);
  vtcr |= vtcr_el2::orgn0::encode(vtcr_el2::orgn0::wb_with_wa);
  vtcr |= vtcr_el2::sh0::encode(vtcr_el2::sh0::inner_shareable);
  vtcr |= vtcr_el2::tg0::encode(vtcr_tg0());
  vtcr |= vtcr_el2::sl0::encode(vtcr_sl0(ipa_bits));

  vtcr |= vtcr_el2::ps::encode(ps_for_bits(pa_bits));

  return vtcr;
}

// This assume MMU is off.
void init_paging() noexcept {
  if (xino::cpu::current_el::read_el() != 2)
    xino::cpu::panic();

#if defined(UKERNEL_PAGE_4K)
  if (!gran4_s1_supported() || !gran4_s2_supported())
    xino::cpu::panic();
#elif defined(UKERNEL_PAGE_16K)
  if (!gran16_s1_supported() || !gran16_s2_supported())
    xino::cpu::panic();
#endif

  if (!xino::cpu::id_aa64mmfr1_el1::read_vh())
    xino::cpu::panic();

  const unsigned pa_bits = parange_bits(); // Currently clapped to 48.
  const unsigned va_bits = xino::mm::va_layout::va_bits;
  // Limit IPA width to the intersection of what we can address (VA) and what
  // the CPU can back (PA). This keeps stage-2 from becoming deeper than stage-1
  // and avoids a needlessly large IPA space on systems with smaller PARange,
  // reducing stage-2 table overhead where possible.
  const unsigned ipa_bits = va_bits < pa_bits ? va_bits : pa_bits;

  // Calculate intersection (unified_state):
  // Check if it is the first cpu running init_paging().
  if (xino::cpu::state.pa_bits == 0U) {
    xino::cpu::state.pa_bits = pa_bits;
    xino::cpu::state.ipa_bits = ipa_bits;
    xino::cpu::state.feat_vhe = true;
    xino::cpu::state.mair_el2 = make_mair_el2();
    xino::cpu::state.tcr_el2 = make_tcr_el2(pa_bits, va_bits);
    xino::cpu::state.vtcr_el2 = make_vtcr_el2(pa_bits, ipa_bits);
  } else {
    if (pa_bits < xino::cpu::state.pa_bits) {
      xino::cpu::state.pa_bits = pa_bits;

      if (ipa_bits < xino::cpu::state.ipa_bits)
        xino::cpu::state.ipa_bits = ipa_bits;

      // Recalculate TCT_EL2 and VTCR_EL2.
      xino::cpu::state.tcr_el2 = make_tcr_el2(pa_bits, va_bits);
      xino::cpu::state.vtcr_el2 = make_vtcr_el2(pa_bits, ipa_bits);
    }
  }
}

/**
 * @brief Install a translation-table base register (TTBR) value.
 *
 * Builds a TTBR register value from the provided physical base address and
 * ASID, then writes it via the register accessor type.
 *
 * @tparam TTBRReg Register accessor type for the target TTBR.
 * @param ttbr_pa Physical address of the translation table base. *
 * @param asid Address Space Identifier (ASID) to associate with this TTBR.
 */
template <typename TTBRReg>
static void install_ttbr(xino::mm::phys_addr ttbr_pa,
                         std::uint16_t asid) noexcept {
  typename TTBRReg::reg_type ttbr{0};

  ttbr |= TTBRReg::asid::encode(asid);
  ttbr |= TTBRReg::base_addr::encode(
      static_cast<xino::mm::phys_addr::value_type>(ttbr_pa));

  TTBRReg::write(ttbr);
}

// TTBR0_EL2.
void install_user_ttbr(xino::mm::phys_addr ttbr0_pa,
                       std::uint16_t asid) noexcept {
  install_ttbr<xino::cpu::ttbr0_el2>(ttbr0_pa, asid);
}

// TTBR1_EL2.
void install_kernel_ttbr(xino::mm::phys_addr ttbr1_pa,
                         std::uint16_t asid = 0) noexcept {
  install_ttbr<xino::cpu::ttbr1_el2>(ttbr1_pa, asid);
}

/* Boot. */

void enable_mmu() noexcept {
  using xino::cpu::sctlr_el2;

  sctlr_el2::reg_type mask{0};
  // Enable cache and MMU.
  mask |= sctlr_el2::c::mask;
  mask |= sctlr_el2::i::mask;
  mask |= sctlr_el2::m::mask;

  sctlr_el2::write_bits(mask);
}

/* TLB Ops. */

/** @brief Invalidate all EL2 stage-1 translations. */
void invalidate_all_stage1() noexcept {
  using namespace xino::barrier;

  dsb<opt::ishst>();
  xino::cpu::tlbi_alle2is();
  dsb<opt::ish>();
  isb();
}

/** @brief Invalidate stage-1 translations for VA range. */
void invalidate_va_range(xino::mm::virt_addr va, std::size_t size,
                         std::uint16_t asid) noexcept {
  using namespace xino::barrier;

  const std::size_t g = xino::mm::va_layout::granule_size();
  xino::mm::virt_addr start = va.align_down(g);
  xino::mm::virt_addr end = (va + size).align_up(g);

  dsb<opt::ishst>();
  for (xino::mm::virt_addr it : xino::mm::virt_addr_range(start, end, g))
    xino::cpu::tlbi_vae2is(it, asid);
  dsb<opt::ish>();
  isb();
}

/** @brief Invalidate all stage-2 translations. */
void invalidate_all_stage2() noexcept {
  using namespace xino::barrier;

  dsb<opt::ishst>();
  xino::cpu::tlbi_vmalls12e1is();
  dsb<opt::ish>();
  isb();
}

/** @brief Invalidate stage-2 translations for IPA range. */
void invalidate_ipa_range(xino::mm::ipa_addr ipa, std::size_t size) noexcept {
  using namespace xino::barrier;

  const std::size_t g = xino::mm::va_layout::granule_size();
  xino::mm::ipa_addr start = ipa.align_down(g);
  xino::mm::ipa_addr end = (ipa + size).align_up(g);

  dsb<opt::ishst>();
  for (xino::mm::ipa_addr it : xino::mm::ipa_addr_range(start, end, g))
    xino::cpu::tlbi_ipas2e1is(it);
  dsb<opt::ish>();
  isb();
}

} // namespace xino::mm::paging
