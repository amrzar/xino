
#include <config.h> // UKERNEL_VA_BITS and UKERNEL_PAGE_nK
#include <cpu.hpp>
#include <mm_paging.hpp>

namespace xino::mm::paging {

struct paging_state {
  unsigned pa_bits;
  unsigned va_bits;
  unsigned ipa_bits;

  bool feat_vhe;
  bool feat_xnx;

  xino::cpu::mair_el2::reg_type mair_el2;
  xino::cpu::tcr_el2::reg_type tcr_el2;
  xino::cpu::vtcr_el2::reg_type vtcr_el2;
  xino::cpu::hcr_el2::reg_type hcr_el2;
};

static struct paging_state ps;

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
[[nodiscard]] static unsigned ps_for_bits(unsigned bits) {
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

[[nodiscard]] static bool gran4_s1_supported() noexcept {
  // xino::cpu::id_aa64mmfr0_el1::t_gran4::supported ||
  // xino::cpu::id_aa64mmfr0_el1::t_gran4::large_pa_52_bits
  return xino::cpu::id_aa64mmfr0_el1::read_t_gran4() !=
         xino::cpu::id_aa64mmfr0_el1::t_gran4::not_supported;
}

[[nodiscard]] static bool gran16_s1_supported() noexcept {
  // xino::cpu::id_aa64mmfr0_el1::t_gran16::supported ||
  // xino::cpu::id_aa64mmfr0_el1::t_gran16::large_pa_52_bits
  return xino::cpu::id_aa64mmfr0_el1::read_t_gran16() !=
         xino::cpu::id_aa64mmfr0_el1::t_gran16::not_supported;
}

[[nodiscard]] static bool gran4_s2_supported() noexcept {
  xino::cpu::id_aa64mmfr0_el1::reg_type gran4 =
      xino::cpu::id_aa64mmfr0_el1::read_t_gran4_2();

  if (gran4 == xino::cpu::id_aa64mmfr0_el1::t_gran4_2::t_gran4)
    return gran4_s1_supported(); // Check ID_AA64MMFR0_EL1.TGran4.
  // xino::cpu::id_aa64mmfr0_el1::t_gran4_2::supported ||
  // xino::cpu::id_aa64mmfr0_el1::t_gran4_2::large_pa_52_bits
  return gran4 != xino::cpu::id_aa64mmfr0_el1::t_gran4_2::not_supported;
}

[[nodiscard]] static bool gran16_s2_supported() noexcept {
  xino::cpu::id_aa64mmfr0_el1::reg_type gran16 =
      xino::cpu::id_aa64mmfr0_el1::read_t_gran16_2();

  if (gran16 == xino::cpu::id_aa64mmfr0_el1::t_gran16_2::t_gran16)
    return gran16_s1_supported(); // Check ID_AA64MMFR0_EL1.TGran16.
  // xino::cpu::id_aa64mmfr0_el1::t_gran16_2::supported ||
  // xino::cpu::id_aa64mmfr0_el1::t_gran16_2::large_pa_52_bits
  return gran16 != xino::cpu::id_aa64mmfr0_el1::t_gran16_2::not_supported;
}

[[nodiscard]] static xino::cpu::tcr_el2::reg_type tcr_tg0() {
#if defined(UKERNEL_PAGE_4K)
  return xino::cpu::tcr_el2::tg0::granule_4k;
#elif defined(UKERNEL_PAGE_16K)
  return xino::cpu::tcr_el2::tg0::granule_16k;
#endif
}

[[nodiscard]] static xino::cpu::tcr_el2::reg_type tcr_tg1() {
#if defined(UKERNEL_PAGE_4K)
  return xino::cpu::tcr_el2::tg1::granule_4k;
#elif defined(UKERNEL_PAGE_16K)
  return xino::cpu::tcr_el2::tg1::granule_16k;
#endif
}

[[nodiscard]] static xino::cpu::vtcr_el2::reg_type vtcr_sl0(unsigned ipa_bits) {
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

[[nodiscard]] static xino::cpu::vtcr_el2::reg_type vtcr_tg0() {
#if defined(UKERNEL_PAGE_4K)
  return xino::cpu::vtcr_el2::tg0::granule_4k;
#elif defined(UKERNEL_PAGE_16K)
  return xino::cpu::vtcr_el2::tg0::granule_16k;
#endif
}

// D24.2.122 MAIR_EL2, Memory Attribute Indirection Register.
[[nodiscard]] static xino::cpu::mair_el2::reg_type make_mair_el2() {
  using xino::cpu::mair_el2;
  mair_el2::reg_type attr_normal = 0xffU; // Attr0.
  mair_el2::reg_type attr_device = 0x00U; // Attr1, and others.
  return static_cast<mair_el2::reg_type>((attr_normal << 0) |
                                         (attr_device << 8));
}

// D24.2.183 TCR_EL2, Translation Control Register, When ELIsInHost.
[[nodiscard]] static xino::cpu::tcr_el2::reg_type
make_tcr_el2(unsigned pa_bits, unsigned va_bits) {
  using xino::cpu::tcr_el2;
  tcr_el2::reg_type tcr{0};

  // Setup TTBR0_EL2 attributes.
  tcr |= tcr_el2::t0sz::encode(64U - va_bits);
  tcr |= tcr_el2::irgn0::encode(tcr_el2::irgn0::wb_with_wa);
  tcr |= tcr_el2::orgn0::encode(tcr_el2::orgn0::wb_with_wa);
  tcr |= tcr_el2::sh0::encode(tcr_el2::sh0::inner_shareable);
  tcr |= tcr_el2::tg0::encode(tcr_tg0());

  // Setup TTBR1_EL2 attributes.
  tcr |= tcr_el2::t1sz::encode(64U - va_bits);
  tcr |= tcr_el2::irgn1::encode(tcr_el2::irgn1::wb_with_wa);
  tcr |= tcr_el2::orgn1::encode(tcr_el2::orgn1::wb_with_wa);
  tcr |= tcr_el2::sh1::encode(tcr_el2::sh1::inner_shareable);
  tcr |= tcr_el2::tg1::encode(tcr_tg1());

  tcr |= tcr_el2::ips::encode(ps_for_bits(pa_bits));

  return tcr;
}

// D24.2.210 VTCR_EL2, Virtualization Translation Control Register.
[[nodiscard]] static xino::cpu::vtcr_el2::reg_type
make_vtcr_el2(unsigned pa_bits, unsigned ipa_bits) {
  using xino::cpu::vtcr_el2;
  vtcr_el2::reg_type vtcr{0};

  // Setup VTTBR_EL2 attributes.
  vtcr |= vtcr_el2::t0sz::encode(64U - ipa_bits);
  vtcr |= vtcr_el2::irgn0::encode(vtcr_el2::irgn0::wb_with_wa);
  vtcr |= vtcr_el2::orgn0::encode(vtcr_el2::orgn0::wb_with_wa);
  vtcr |= vtcr_el2::sh0::encode(vtcr_el2::sh0::inner_shareable);
  vtcr |= vtcr_el2::tg0::encode(vtcr_tg0());
  vtcr |= vtcr_el2::sl0::encode(vtcr_sl0(ipa_bits));

  vtcr |= vtcr_el2::ps::encode(ps_for_bits(pa_bits));

  return vtcr;
}

void init_paging() {
  // Check assumptions:
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

  unsigned pa_bits = parange_bits(); // Currently clapped to 48.
  unsigned va_bits = static_cast<unsigned>(UKERNEL_VA_BITS);
  // Limit IPA width to the intersection of what we can address (VA) and what
  // the CPU can back (PA). This keeps stage-2 from becoming deeper than stage-1
  // (so page-table walkers can be reused) and avoids a needlessly large IPA
  // space on systems with smaller PARange, reducing stage-2 table
  // overhead where possible.
  unsigned pa_bits = va_bits < pa_bits ? va_bits : pa_bits;
}

} // namespace xino::mm::paging
