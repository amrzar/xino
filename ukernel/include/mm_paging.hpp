
#ifndef __MM_PAGING_HPP__
#define __MM_PAGING_HPP__

#include <config.h> // UKERNEL_VA_BITS and UKERNEL_PAGE_nK
#include <cstddef>
#include <cstdint>
#include <mm.hpp> // phys_addr, virt_addr, ipa_addr, and prot

// Chapter D8
// The AArch64 Virtual Memory System Architecture
namespace xino::mm::paging {

/** @brief Underlying storage type for page-table entries. */
using pte_t = std::uint64_t;

/**
 * @enum page_size
 * @brief Supported translation granule sizes.
 */
enum class page_size : std::uint8_t {
  PS_4K = 12,  /**< 4 KiB pages. */
  PS_16K = 14, /**< 16 KiB pages. */
};

/**
 * @enum stage
 * @brief Translation stage kind (stage-1 vs. stage-2).
 */
enum class stage : std::uint8_t {
  ST_1, /**< EL2 stage-1 (EL2/EL0 in VHE). */
  ST_2, /**< EL2 stage-2 (guest IPA -> PA). */
};

/** @brief Log2 of the granule size (e.g., 12 for 4K). */
constexpr unsigned granule_shift() noexcept {
#if defined(UKERNEL_PAGE_4K)
  // D8.2.8 VMSAv8-64 translation using the 4KB granule.
  return static_cast<unsigned>(page_size::PS_4K);
#elif defined(UKERNEL_PAGE_16K)
  // D8.2.9 VMSAv8-64 translation using the 16KB granule.
  return static_cast<unsigned>(page_size::PS_16K);
#else
#error "UKERNEL_PAGE_4K and UKERNEL_PAGE_16K are supported"
#endif
}

/** @brief Size of a single page (granule) in bytes. */
constexpr std::size_t granule_size() noexcept {
  return std::size_t{1} << granule_shift();
}

/** @brief Bits per page-table index (9 for 4K, 11 for 16K). */
constexpr unsigned index_stride() noexcept { return granule_shift() - 3; }

/** @brief Number of entries in a page-table page (512 for 4K, 2048 for 16K). */
constexpr unsigned entries_per_table() noexcept { return 1U << index_stride(); }

// Geometry calculation helpers.

/**
 * @brief Number of translation levels (HW and logical) for an address.
 *
 * Mirrors the Linux ARM64_HW_PGTABLE_LEVELS() logic:
 *   `DIV_ROUND_UP((addr_bits - granule_shift()), index_stride())`.
 *
 * @param addr_bits VA bits (stage-1) or IPA bits (stage-2).
 * @return Number of levels for @p addr_bits.
 * @note This is purely a *geometry* computation. Whether it is usable for
 *       stage-2 depends `on VTCR_EL2` constraints.
 */
constexpr unsigned levels_for_bits(unsigned addr_bits) noexcept {
  return static_cast<unsigned>((addr_bits - 4) / index_stride());
}

/**
 * @brief Hardware level number of the root table.
 *
 * @param addr_bits VA bits (stage-1) or IPA bits (stage-2).
 * @return Hardware level number of the root table.
 * @note This is purely a *geometry* computation. Whether it is usable for
 *       stage-2 depends `on VTCR_EL2` constraints.
 */
constexpr unsigned root_hw_level_for_bits(unsigned addr_bits) noexcept {
  return 4 - levels_for_bits(addr_bits);
}

/**
 * @brief Convert a logical level (0 = root) to a hardware level index.
 *
 * @param addr_bits VA bits (stage-1) or IPA bits (stage-2).
 * @param level Logical level `[0 .. levels_for_bits(addr_bits) - 1]`.
 * @return Hardware level for logical level.
 */
constexpr unsigned to_hw_level_for_bits(unsigned addr_bits,
                                        unsigned level) noexcept {
  return root_hw_level_for_bits(addr_bits) + level;
}

/**
 * @brief Address shift for a given hardware level.
 *
 * Mirrors the Linux ARM64_HW_PGTABLE_LEVEL_SHIFT() logic:
 *   `((4 - hw_level) - 1) * index_stride() + granule_shift()`.
 *
 * @param hw_level Hardware level.
 */
constexpr unsigned hw_level_shift(unsigned hw_level) noexcept {
  return index_stride() * (4 - hw_level) + 3;
}

/**
 * @brief Address shift for a given logical level.
 *
 * @param addr_bits VA bits (stage-1) or IPA bits (stage-2).
 * @param level Logical level `[0 .. levels_for_bits(addr_bits) - 1]`.
 */
constexpr unsigned level_shift_for_bits(unsigned addr_bits,
                                        unsigned level) noexcept {
  return hw_level_shift(to_hw_level_for_bits(addr_bits, level));
}

/**
 * @brief Mapping size in bytes of a leaf at a logical level.
 *
 * For the last level, this is the granule size. For upper levels it is
 * the block mapping size (e.g., 1G / 2M).
 *
 * @param addr_bits VA bits (stage-1) or IPA bits (stage-2).
 * @param level Logical level `[0 .. levels_for_bits(addr_bits) - 1]`.
 */
constexpr std::size_t level_size_for_bits(unsigned addr_bits,
                                          unsigned level) noexcept {
  return std::size_t{1} << level_shift_for_bits(addr_bits, level);
}

// Stage-1 wrappers.

/** @brief Number of translation levels for stage-1. */
constexpr unsigned s1_levels() noexcept {
  return levels_for_bits(static_cast<unsigned>(UKERNEL_VA_BITS));
}

/** @brief Hardware level number of the root table for stage-1. */
constexpr unsigned s1_root_hw_level() noexcept {
  return root_hw_level_for_bits(UKERNEL_VA_BITS);
}

/** @brief Mapping size in bytes of a leaf at a logical level for stage-1. */
constexpr std::size_t s1_level_size(unsigned level) noexcept {
  return level_size_for_bits(UKERNEL_VA_BITS, level);
}

/* DESCRIPTOR TYPES. */
/* D8.3.1 VMSAv8-64 descriptor formats. */

// Descriptor type, Bit 1 and Bit 0.
constexpr pte_t PTE_TYPE_MASK{0x3UL};
constexpr pte_t PTE_TYPE_FAULT{0x0UL};
constexpr pte_t PTE_TYPE_BLOCK{0x1UL};
constexpr pte_t PTE_TYPE_PAGE_OR_TABLE{0x3UL};
constexpr pte_t PTE_TYPE_PAGE{PTE_TYPE_PAGE_OR_TABLE};
constexpr pte_t PTE_TYPE_TABLE{PTE_TYPE_PAGE_OR_TABLE};

constexpr bool pte_is_fault(pte_t pte) noexcept {
  return (pte & PTE_TYPE_MASK) == PTE_TYPE_FAULT;
}

constexpr bool pte_is_block(pte_t pte) noexcept {
  return (pte & PTE_TYPE_MASK) == PTE_TYPE_BLOCK;
}

constexpr bool pte_is_table_or_page(pte_t pte) noexcept {
  return (pte & PTE_TYPE_MASK) == PTE_TYPE_PAGE_OR_TABLE;
}

/* D8.3.1.1 VMSAv8-64 Table descriptor format. */

// For 4KB granule size 12 LSB bits are ignored except descriptor type bits.
// For 16KB granule size 14 LSB bits are ignored except descriptor type bits.

/* D8.3.1.2 VMSAv8-64 Block descriptor and Page descriptor formats. */

// Figure D8-16
// Stage 1 attribute fields in VMSAv8-64 Block and Page descriptors.
constexpr std::uint64_t PTE_ATTRINDX_SHIFT{2}; // 3-bits.
constexpr std::uint64_t PTE_AP_SHIFT{6};       // 2-bits.
constexpr std::uint64_t PTE_SH_SHIFT{8};       // 2-bits.
constexpr std::uint64_t PTE_AF_SHIFT{10};      // 1-bit.
constexpr std::uint64_t PTE_PXN_SHIFT{53};     // 1-bit.
constexpr std::uint64_t PTE_UXN_SHIFT{54};     // 1-bit.

// D8.6.1 Stage 1 memory type and Cacheability attributes.
constexpr pte_t PTE_ATTRINDX_MASK{0x7UL << PTE_ATTRINDX_SHIFT};
constexpr pte_t PTE_ATTRINDX(std::uint64_t idx) {
  return (idx & 0x7UL) << PTE_ATTRINDX_SHIFT;
}

// Attr index conventions (must match MAIR_EL2 programmed by cpu_setup).
// See xino::mm::paging::make_mair_el2().
constexpr std::uint64_t MAIR_IDX_NORMAL{0}; // Normal, WBWA
constexpr std::uint64_t MAIR_IDX_DEVICE{1}; // Device, nGnRnE

// D8.4.1.2.1 Stage 1 data accesses using Direct permissions (Table D8-63).
constexpr pte_t PTE_AP_MASK{0x3UL << PTE_AP_SHIFT};
constexpr pte_t PTE_AP_RW_EL2{0UL << PTE_AP_SHIFT};
constexpr pte_t PTE_AP_RW_EL0_EL2{1UL << PTE_AP_SHIFT};
constexpr pte_t PTE_AP_RO_EL2{2UL << PTE_AP_SHIFT};
constexpr pte_t PTE_AP_RO_EL0_EL2{3UL << PTE_AP_SHIFT};

// D8.6.2 Stage 1 Shareability attributes (Table D8-95).
constexpr pte_t PTE_SH_MASK{0x3UL << PTE_SH_SHIFT};
constexpr pte_t PTE_SH_NON_SHAREABLE{0UL << PTE_SH_SHIFT};
constexpr pte_t PTE_SH_OUTER_SHAREABLE{2UL << PTE_SH_SHIFT};
constexpr pte_t PTE_SH_INNER_SHAREABLE{3UL << PTE_SH_SHIFT};

// D8.5.1 The Access flag.
constexpr pte_t PTE_AF{1UL << PTE_AF_SHIFT};

// D8.4.1.2.3 Stage 1 instruction execution using Direct permissions.
constexpr pte_t PTE_PXN{1UL << PTE_PXN_SHIFT};
constexpr pte_t PTE_UXN{1UL << PTE_UXN_SHIFT};

// Figure D8-17
// Stage 2 attribute fields in VMSAv8-64 Block and Page descriptors.

constexpr std::uint64_t PTE_S2_MEMATTR_SHIFT{2}; // 4-bits.
constexpr std::uint64_t PTE_S2_AP_SHIFT{6};      // 2-bits.
constexpr std::uint64_t PTE_S2_SH_SHIFT{8};      // 2-bits.
constexpr std::uint64_t PTE_S2_AF_SHIFT{10};     // 1-bit.
constexpr std::uint64_t PTE_S2_XN_SHIFT{53};     // 2-bits.

// D8.6.5 Stage 2 memory type and Cacheability attributes when FWB is disabled.
constexpr pte_t PTE_S2_MEMATTR_MASK{0xfUL << PTE_S2_MEMATTR_SHIFT};
constexpr pte_t PTE_S2_MEMATTR(std::uint64_t attr) {
  return (attr & 0xfUL) << PTE_S2_MEMATTR_SHIFT;
}

constexpr std::uint64_t S2_MEMATTR_DEVICE_nGnRnE{0x0}; // Device, nGnRnE
constexpr std::uint64_t S2_MEMATTR_NORMAL_WB{0xF};     // Normal, outer+inner WB

// D8.4.2.1.1 Stage 2 data accesses using Direct permissions (Table D8-76).
constexpr pte_t PTE_S2_AP_MASK{0x3UL << PTE_S2_AP_SHIFT};
constexpr pte_t PTE_S2_AP_RDONLY{1UL << PTE_S2_AP_SHIFT};
constexpr pte_t PTE_S2_AP_RDWR{3UL << PTE_S2_AP_SHIFT};

// D8.6.7 Stage 2 Shareability attributes (Table D8-102).
constexpr pte_t PTE_S2_SH_MASK{0x3UL << PTE_S2_SH_SHIFT};
constexpr pte_t PTE_S2_SH_NON_SHAREABLE{0UL << PTE_S2_SH_SHIFT};
constexpr pte_t PTE_S2_SH_OUTER_SHAREABLE{2UL << PTE_S2_SH_SHIFT};
constexpr pte_t PTE_S2_SH_INNER_SHAREABLE{3UL << PTE_S2_SH_SHIFT};

// D8.5.1 The Access flag.
constexpr pte_t PTE_S2_AF{1UL << PTE_S2_AF_SHIFT};

/* PTE ENCODERS. */

template <typename Derived> struct pte_encoder_base {
  using addr_t = xino::mm::phys_addr;
  using prot_t = xino::mm::prot;

  /** @brief Encode a physical address into the PTE address field. */
  [[nodiscard]] static pte_t phys_to_pte(addr_t pa) noexcept {
    const auto raw = static_cast<typename addr_t::value_type>(pa);
    return static_cast<uint64_t>(raw) & pte_phys_field_mask();
  }

  /** @brief Extract the physical address from a PTE. */
  [[nodiscard]] static addr_t pte_to_phys(pte_t pte) noexcept {
    const std::uint64_t raw = pte & pte_phys_field_mask();
    return addr_t{static_cast<typename addr_t::value_type>(raw)};
  }

  /** @brief Make a table descriptor. */
  [[nodiscard]] static pte_t make_table(addr_t pa) noexcept {
    return PTE_TYPE_TABLE | phys_to_pte(pa);
  }

  /** @brief Make a page leaf descriptor. */
  [[nodiscard]] static pte_t make_leaf_page(addr_t pa, prot_t p,
                                            bool device) noexcept {
    return PTE_TYPE_PAGE | Derived::encode_attrs(p, device) | phys_to_pte(pa);
  }

  /** @brief Make a block leaf descriptor. */
  [[nodiscard]] static pte_t make_leaf_block(addr_t pa, prot_t p,
                                             bool device) noexcept {
    return PTE_TYPE_BLOCK | Derived::encode_attrs(p, device) | phys_to_pte(pa);
  }
};

template <stage Stage> struct pte_encoder;

/* CRTP. */

template <>
struct pte_encoder<stage::ST_1>
    : public pte_encoder_base<pte_encoder<stage::ST_1>> {
  /** @brief Helper to compute stage-1 attribute bits. */
  [[nodiscard]] static pte_t encode_attrs(prot_t prot, bool device) noexcept {
    pte_t pte = 0;

    pte |= PTE_ATTRINDX(device ? MAIR_IDX_DEVICE : MAIR_IDX_NORMAL);
    pte |= PTE_AF;
    pte |= prot.has(prot_t::SHARED) ? PTE_SH_INNER_SHAREABLE
                                    : PTE_SH_NON_SHAREABLE;

    // D8.4.1.2.6 Summary of Direct permissions for stage 1 translations.
    if (prot.has(prot_t::USER))
      pte |= prot.has(prot_t::WRITE) ? PTE_AP_RW_EL0_EL2 : PTE_AP_RO_EL0_EL2;
    else
      pte |= prot.has(prot_t::WRITE) ? PTE_AP_RW_EL2 : PTE_AP_RO_EL2;

    if (!prot.has(prot_t::EXEC))
      pte |= (PTE_PXN | PTE_UXN);

    return pte;
  }
};

template <>
struct pte_encoder<stage::ST_2>
    : public pte_encoder_base<pte_encoder<stage::ST_2>> {
  /** @brief Helper to compute stage-2 attribute bits. */
  [[nodiscard]] static pte_t encode_attrs(prot_t p, bool device) noexcept {
    pte_t pte = 0;

    pte |= PTE_S2_MEMATTR(device ? S2_MEMATTR_DEVICE_nGnRnE
                                 : S2_MEMATTR_NORMAL_WB);
    pte |= PTE_AF;

    const bool rd = p.has(prot_t::READ);
    const bool wr = p.has(prot_t::WRITE);

    if (rd && wr) {
      pte |= PTE_S2_AP_RDWR;
    } else if (rd && !wr) {
      pte |= PTE_S2_AP_RDONLY;
    } else {
      // 00 => no access
    }

    if (!p.has(prot_t::EXEC)) {
      pte |= PTE_UXN; // XN (conservative)
    }

    return pte;
  }
};

/** @brief Invalidate all EL2 stage-1 translations. */
void invalidate_all_stage1();

/** @brief Invalidate stage-1 translations for VA range. */
void invalidate_va_range(virt_addr va, std::size_t size);

/** @brief Invalidate all stage-2 translations. */
void invalidate_all_stage2();

/** @brief Invalidate stage-2 translations for IPA range. */
void invalidate_ipa_range(ipa_addr ipa, std::size_t size);

} // namespace xino::mm::paging

#endif // __MM_PAGING_HPP__
