
#ifndef __MM_PAGING_HPP__
#define __MM_PAGING_HPP__

#include <allocator.hpp>
#include <cstddef>
#include <cstdint>
#include <errno.hpp>
#include <mm.hpp> // phys_addr, virt_addr, ipa_addr, and prot
#include <mm_va_layout.hpp>

/**
 * @anchor chapter_d8
 * Chapter D8
 * The AArch64 Virtual Memory System Architecture.
 *
 * Figure D8-3 52-bit IA resolved using 4KB translation granule.
 *
 * 51      48 47       39 38       30 29       21 20       12 11              0
 * +---------+-----------+-----------+-----------+-----------+----------------+
 * |         |           |           |           |           |                |
 * +---------+-----------+-----------+-----------+-----------+----------------+
 * IA[51:48] IA[47:39]   IA[38:30]   IA[29:21]   IA[20:12]   IA[11:0]
 * hw-L(-1)  hw-L(0)     hw-L(1)     hw-L(2)     hw-L(3)
 *           512GB       1GB         2MB         4KB
 *
 *
 * Figure D8-6 52-bit IA resolved using 16KB translation granule.
 *
 * 51      47 46          36 35         25 24         14 13                   0
 * +---------+--------------+-------------+-------------+---------------------+
 * |         |              |             |             |                     |
 * +---------+--------------+-------------+-------------+---------------------+
 * IA[51:47] IA[46:36]      IA[35:25]     IA[24:14]     IA[13:0]
 * hw-L(0)   hw-L(1)        hw-L(2)       hw-L(3)
 *           64GB           32MB          16KB
 *
 * This file supports
 *
 *  - 4KB granule, IA 39-bit, root hw level (1).
 *  - 16KB granule, IA 36-bit, root hw level (2).
 *  - OA is capped to 48-bit, see `parange_bits()`.
 */

namespace xino::mm::paging {

/** @brief Underlying storage type for page-table entries. */
using pte_t = std::uint64_t;

/**
 * @enum stage
 * @brief Translation stage kind (stage-1 vs. stage-2).
 */
enum class stage : std::uint8_t {
  ST_1, /**< EL2 stage-1 (EL2/EL0 in VHE). */
  ST_2, /**< EL2 stage-2 (guest IPA -> PA). */
};

/** @brief Bits per page-table index (9 for 4K, 11 for 16K). */
constexpr unsigned index_stride() noexcept {
  return xino::mm::va_layout::granule_shift() - 3;
}

/** @brief Number of entries in a page-table page (512 for 4K, 2048 for 16K). */
constexpr unsigned entries_per_table() noexcept { return 1U << index_stride(); }

/* Geometry calculation helpers. */

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
 * @note `level_size_for_bits()` assumes all levels can represent a region.
 *        Currently the lowest hw level for 4KB granule is 0 and for
 *        16KB granule is 1; see @ref chapter_d8 "above".
 */
constexpr std::size_t level_size_for_bits(unsigned addr_bits,
                                          unsigned level) noexcept {
  return std::size_t{1} << level_shift_for_bits(addr_bits, level);
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

/**
 * @anchor d8_3_1_1
 * D8.3.1.1 VMSAv8-64 Table descriptor format.
 *
 * Figure D8-12 VMSAv8-64 Table descriptor formats.
 * 4KB and 16KB, 48-bit OA
 *
 * 63   59 58 51 50  48 47                      #m m-1 12 11             2 1 0
 * +------+-----+------+--------------------------+------+----------------+-+-+
 * | Attr | IGN | Res0 |    NLT address[47:m]     | RES0 |      IGN       |1|1|
 * +------+-----+------+--------------------------+------+----------------+-+-+
 *   # With the 4KB granule size m is 12, with the 16KB granule size m is 14.
 *   # When m is 12, the RES0 field shown for bits[(m-1):12] is absent.
 */

/**
 * @anchor d8_3_1_2
 * D8.3.1.2 VMSAv8-64 Block descriptor and Page descriptor formats.
 *
 * Figure D8-14 VMSAv8-64 Block descriptor formats.
 * 4KB and 16KB, 48-bit OA
 *
 * 63         50 49  48 47                      #n n-1 12 11             2 1 0
 * +------------+------+--------------------------+------+----------------+-+-+
 * | Upper Attr | Res0 |   Output address[47:n]   | RES0 |   Lower Attr   |0|1|
 * +------------+------+--------------------------+------+----------------+-+-+
 *   # For the 4KB granule size, the hw-level 1 descriptor n is 30, and the
 *     hw-level 2 descriptor n is 21.
 *   # For the 16KB granule size, the level 2 descriptor n is 25.
 *
 *
 * Figure D8-15 VMSAv8-64 Page descriptor formats.
 * 4KB granule 48-bit OA
 *
 * 63         50 49  48 47                             12 11             2 1 0
 * +------------+------+---------------------------------+----------------+-+-+
 * | Upper Attr | Res0 |      Output address[47:12]      |   Lower Attr   |1|1|
 * +------------+------+---------------------------------+----------------+-+-+
 *
 * 16KB granule 48-bit OA
 *
 * 63         50 49  48 47                      14 13 12 11             2 1 0
 * +------------+------+--------------------------+------+----------------+-+-+
 * | Upper Attr | Res0 |   Output address[47:14]  | RES0 |   Lower Attr   |1|1|
 * +------------+------+--------------------------+------+----------------+-+-+
 */

// Figure D8-16
// Stage 1 attribute fields in VMSAv8-64 Block and Page descriptors.
constexpr std::uint64_t PTE_ATTRINDX_SHIFT{2}; // 3-bits.
constexpr std::uint64_t PTE_AP_SHIFT{6};       // 2-bits.
constexpr std::uint64_t PTE_SH_SHIFT{8};       // 2-bits.
constexpr std::uint64_t PTE_AF_SHIFT{10};      // 1-bit.
constexpr std::uint64_t PTE_nG_SHIFT{11};      // 1-bit.
constexpr std::uint64_t PTE_PXN_SHIFT{53};     // 1-bit.
constexpr std::uint64_t PTE_UXN_SHIFT{54};     // 1-bit.

// D8.6.1 Stage 1 memory type and Cacheability attributes.
constexpr pte_t PTE_ATTRINDX_MASK{pte_t{0b111} << PTE_ATTRINDX_SHIFT};
constexpr pte_t PTE_ATTRINDX(std::uint64_t idx) {
  return (idx & 0x7UL) << PTE_ATTRINDX_SHIFT;
}

// Attr index conventions (must match MAIR_EL2 programmed by cpu_setup).
// See xino::mm::paging::make_mair_el2().
constexpr std::uint64_t MAIR_IDX_NORMAL{0}; // Normal, WBWA
constexpr std::uint64_t MAIR_IDX_DEVICE{1}; // Device, nGnRnE

// D8.4.1.2.1 Stage 1 data accesses using Direct permissions (Table D8-63).
constexpr pte_t PTE_AP_MASK{pte_t{0b11} << PTE_AP_SHIFT};
constexpr pte_t PTE_AP_RW_EL2{pte_t{0} << PTE_AP_SHIFT};
constexpr pte_t PTE_AP_RW_EL0_EL2{pte_t{1} << PTE_AP_SHIFT};
constexpr pte_t PTE_AP_RO_EL2{pte_t{2} << PTE_AP_SHIFT};
constexpr pte_t PTE_AP_RO_EL0_EL2{pte_t{3} << PTE_AP_SHIFT};

// D8.6.2 Stage 1 Shareability attributes (Table D8-95).
constexpr pte_t PTE_SH_MASK{pte_t{0b11} << PTE_SH_SHIFT};
constexpr pte_t PTE_SH_NON_SHAREABLE{pte_t{0} << PTE_SH_SHIFT};
constexpr pte_t PTE_SH_OUTER_SHAREABLE{pte_t{2} << PTE_SH_SHIFT};
constexpr pte_t PTE_SH_INNER_SHAREABLE{pte_t{3} << PTE_SH_SHIFT};

// D8.5.1 The Access flag.
constexpr pte_t PTE_AF{pte_t{1} << PTE_AF_SHIFT};
// D8.16.3.1 Global and process-specific translation table entries.
constexpr pte_t PTE_nG{pte_t{1} << PTE_nG_SHIFT};
// D8.4.1.2.3 Stage 1 instruction execution using Direct permissions.
constexpr pte_t PTE_PXN{pte_t{1} << PTE_PXN_SHIFT};
constexpr pte_t PTE_UXN{pte_t{1} << PTE_UXN_SHIFT};

// Figure D8-17
// Stage 2 attribute fields in VMSAv8-64 Block and Page descriptors.
constexpr std::uint64_t PTE_S2_MEMATTR_SHIFT{2}; // 4-bits.
constexpr std::uint64_t PTE_S2_AP_SHIFT{6};      // 2-bits.
constexpr std::uint64_t PTE_S2_SH_SHIFT{8};      // 2-bits.
constexpr std::uint64_t PTE_S2_AF_SHIFT{10};     // 1-bit.
constexpr std::uint64_t PTE_S2_XN_SHIFT{53};     // 2-bits.

// D8.6.5 Stage 2 memory type and Cacheability attributes when FWB is disabled.
constexpr pte_t PTE_S2_MEMATTR_MASK{pte_t{0b1111} << PTE_S2_MEMATTR_SHIFT};
constexpr pte_t PTE_S2_MEMATTR(std::uint64_t attr) {
  return (attr & 0xfUL) << PTE_S2_MEMATTR_SHIFT;
}

constexpr std::uint64_t S2_MEMATTR_DEVICE_nGnRnE{0x0}; // Device, nGnRnE
constexpr std::uint64_t S2_MEMATTR_NORMAL_WB{0xF};     // Normal, outer+inner WB

// D8.4.2.1.1 Stage 2 data accesses using Direct permissions (Table D8-76).
constexpr pte_t PTE_S2_AP_MASK{pte_t{0b11} << PTE_S2_AP_SHIFT};
constexpr pte_t PTE_S2_AP_RDONLY{pte_t{1} << PTE_S2_AP_SHIFT};
constexpr pte_t PTE_S2_AP_RDWR{pte_t{3} << PTE_S2_AP_SHIFT};

// D8.6.7 Stage 2 Shareability attributes (Table D8-102).
constexpr pte_t PTE_S2_SH_MASK{pte_t{0b11} << PTE_S2_SH_SHIFT};
constexpr pte_t PTE_S2_SH_NON_SHAREABLE{pte_t{0} << PTE_S2_SH_SHIFT};
constexpr pte_t PTE_S2_SH_OUTER_SHAREABLE{pte_t{2} << PTE_S2_SH_SHIFT};
constexpr pte_t PTE_S2_SH_INNER_SHAREABLE{pte_t{3} << PTE_S2_SH_SHIFT};

// D8.5.1 The Access flag.
constexpr pte_t PTE_S2_AF{pte_t{1} << PTE_S2_AF_SHIFT};

/* PTE ENCODERS. */

inline pte_t pte_phys_field_mask() {
  const std::uint64_t mask{(std::uint64_t{1} << xino::cpu::state.pa_bits) - 1};
  const std::uint64_t granule_mask{xino::mm::va_layout::granule_size() - 1};
  // e.g. 0x0000'FFFF'FFFF'F000UL for 4KB granule.
  return static_cast<pte_t>(mask & ~granule_mask);
}

inline pte_t pte_attr_field_mask() {
  // e.g. 0xFFFF'0000'0000'0FFFUL for 4KB granule.
  return static_cast<pte_t>(~pte_phys_field_mask() & ~PTE_TYPE_MASK);
}

/**
 * @brief CRTP helper for encoding/decoding page-table entries (PTEs).
 *
 * This base provides common utilities to:
 *  - encode a physical address into the PTE address field,
 *  - extract the physical address from an existing PTE,
 *  - construct table descriptors and leaf descriptors (page/block).
 *
 * The concrete encoder is supplied via CRTP (`Derived`) and must provide:
 * @code
 * static pte_t encode_attrs(xino::mm::prot p, bool device) noexcept;
 * @endcode
 *
 * @tparam Derived CRTP-derived encoder type.
 */
template <typename Derived> struct pte_encoder_base {

  /** @brief Encode a physical address into the PTE address field. */
  [[nodiscard]] static pte_t phys_to_pte(xino::mm::phys_addr pa) noexcept {
    return static_cast<pte_t>(pa) & pte_phys_field_mask();
  }

  /** @brief Extract the physical address from a PTE. */
  [[nodiscard]] static xino::mm::phys_addr pte_to_phys(pte_t pte) noexcept {
    // Mask address bits in PTE.
    pte &= pte_phys_field_mask();
    return phys_addr{static_cast<xino::mm::phys_addr::value_type>(pte)};
  }

  /** @brief Make a table descriptor (no attribute). */
  [[nodiscard]] static pte_t make_table(xino::mm::phys_addr pa) noexcept {
    return PTE_TYPE_TABLE | phys_to_pte(pa);
  }

  /**
   * @brief Make a **page** leaf descriptor.
   *
   * @param pa Physical address to map (encoded into the PTE).
   * @param attr Attribute to apply.
   * @return The constructed page leaf descriptor.
   */
  [[nodiscard]] static pte_t make_leaf_page_attr(xino::mm::phys_addr pa,
                                                 pte_t attr) noexcept {
    return PTE_TYPE_PAGE | attr | phys_to_pte(pa);
  }

  // Make a page leaf descriptor.
  [[nodiscard]] static pte_t make_leaf_page(xino::mm::phys_addr pa, prot p,
                                            bool device) noexcept {
    return make_leaf_page_attr(pa, Derived::encode_attrs(p, device));
  }

  /**
   * @brief Make a **block** leaf descriptor.
   *
   * @param pa Physical address to map (encoded into the PTE).
   * @param attr Attribute to apply.
   * @return The constructed block leaf descriptor.
   */
  [[nodiscard]] static pte_t make_leaf_block_attr(xino::mm::phys_addr pa,
                                                  pte_t attr) noexcept {
    return PTE_TYPE_BLOCK | attr | phys_to_pte(pa);
  }

  // Make a block leaf descriptor.
  [[nodiscard]] static pte_t make_leaf_block(xino::mm::phys_addr pa, prot p,
                                             bool device) noexcept {
    return make_leaf_block_attr(pa, Derived::encode_attrs(p, device));
  }
};

template <stage Stage> struct pte_encoder;

/* CRTP. */

// Stage-1 encoder.
template <>
struct pte_encoder<stage::ST_1>
    : public pte_encoder_base<pte_encoder<stage::ST_1>> {
  /** @brief Helper to compute stage-1 attribute bits. */
  [[nodiscard]] static pte_t encode_attrs(xino::mm::prot p,
                                          bool device) noexcept {
    pte_t pte{PTE_TYPE_FAULT};

    pte |= PTE_ATTRINDX(device ? MAIR_IDX_DEVICE : MAIR_IDX_NORMAL);
    pte |= PTE_AF;
    pte |= p & xino::mm::prot::SHARED ? PTE_SH_INNER_SHAREABLE
                                      : PTE_SH_NON_SHAREABLE;

    // D8.4.1.2.1 Stage 1 data accesses using Direct permissions.
    if (p & xino::mm::prot::KERNEL) {
      pte |= p & xino::mm::prot::WRITE ? PTE_AP_RW_EL2 : PTE_AP_RO_EL2;
    } else {
      pte |= p & xino::mm::prot::WRITE ? PTE_AP_RW_EL0_EL2 : PTE_AP_RO_EL0_EL2;
      pte |= PTE_nG; // User page.
    }

    if (!(p & xino::mm::prot::EXEC))
      pte |= (PTE_PXN | PTE_UXN);

    return pte;
  }
};

// Stage-2 encoder.
template <>
struct pte_encoder<stage::ST_2>
    : public pte_encoder_base<pte_encoder<stage::ST_2>> {
  /** @brief Helper to compute stage-2 attribute bits. */
  [[nodiscard]] static pte_t encode_attrs(xino::mm::prot p,
                                          bool device) noexcept {
    pte_t pte{PTE_TYPE_FAULT};

    pte |= PTE_S2_MEMATTR(device ? S2_MEMATTR_DEVICE_nGnRnE
                                 : S2_MEMATTR_NORMAL_WB);
    pte |= PTE_AF;

    const bool rd{static_cast<bool>(p & xino::mm::prot::READ)};
    const bool wr{static_cast<bool>(p & xino::mm::prot::WRITE)};

    if (rd && wr) {
      pte |= PTE_S2_AP_RDWR;
    } else if (rd && !wr) {
      pte |= PTE_S2_AP_RDONLY;
    } else {
      // 00 => no access
    }

    if (!(p & xino::mm::prot::EXEC)) {
      pte |= PTE_UXN; // XN (conservative)
    }

    return pte;
  }
};

/** @brief Invalidate all EL2 stage-1 translations. */
void invalidate_all_stage1() noexcept;

/** @brief Invalidate stage-1 translations for VA range. */
void invalidate_va_range(xino::mm::virt_addr va, std::size_t size,
                         std::uint16_t asid) noexcept;

/** @brief Invalidate all stage-2 translations. */
void invalidate_all_stage2() noexcept;

/** @brief Invalidate stage-2 translations for IPA range. */
void invalidate_ipa_range(xino::mm::ipa_addr ipa, std::size_t size) noexcept;

/* "PAGE TABLE" */

/** @brief Stage-specific address used by page-table operations. */
template <stage Stage> struct addr_for;

/** @brief Stage-1 input address (VA + ASID). */
template <> struct addr_for<stage::ST_1> {
  using addr_type = xino::mm::virt_addr;

  addr_type addr{};
  std::uint16_t asid{};
};

/** @brief Stage-2 input address (IPA). */
template <> struct addr_for<stage::ST_2> {
  using addr_type = xino::mm::ipa_addr;

  addr_type addr{};
};

template <stage Stage, typename Allocator> class page_table {
public:
  /** @brief Stage-specific address type (VA + ASID or IPA). */
  using addr_t = addr_for<Stage>;

  page_table() = delete;

  // No copy.
  page_table(const page_table &) = delete;

  // No assignment.
  page_table &operator=(const page_table &) = delete;

  explicit constexpr page_table(Allocator &a) noexcept : allocator{a} {
    for (unsigned i{0}; i < entries_per_table(); i++)
      pt_root[i] = PTE_TYPE_FAULT;
  }

  // MAP, UNMAP, and PROTECT.

  /**
   * @brief Map a contiguous address range to a contiguous physical range.
   *
   * Maps `[a.addr, a.addr + size)` to `[pa, pa + size)` using protections @p p.
   * The implementation attempts to use the largest feasible leaf level (block
   * vs page) based on alignment and remaining size. If @p size is smaller than
   * a page size, a single page is mapped.
   *
   * @param a Start address (VA + ASID for stage-1, IPA for stage-2).
   * @param pa Start physical address.
   * @param size Size in bytes. A size of 0 is a no-op.
   * @param p Protection/attribute flags.
   *
   * @retval `xino::error_nr::ok` Success (or `size == 0`).
   * @retval `xino::error_nr::overflow` Address overflow.
   * @retval `xino::error_nr::invalid` Overlaps an existing valid mapping or
   *          @p a or @p pa is not page aligned.
   * @retval `xino::error_nr::nomem` Failed to allocate a page-table page.
   */
  [[nodiscard]] xino::error_t map_range(addr_t a, xino::mm::phys_addr pa,
                                        std::size_t size,
                                        xino::mm::prot p) noexcept {
    if (size == 0)
      return xino::error_nr::ok;

    const std::size_t gs = xino::mm::va_layout::granule_size();
    // At least `a` and `pa` should be page aligned.
    if (!a.addr.is_align(gs) || !pa.is_align(gs))
      return xino::error_nr::invalid;

    // Check for overflow.
    if (a.addr + size < a.addr || pa + size < pa)
      return xino::error_nr::overflow;

    while (size) {
      // Pick a most suitable level to map.
      const unsigned leaf = choose_leaf_level(a, pa, size);

      const std::size_t map_sz = level_size(leaf);

      if (auto ret = map_one(a, pa, p, leaf); ret != xino::error_nr::ok)
        return ret;

      a.addr += map_sz;
      pa += map_sz;
      size = (size > map_sz) ? (size - map_sz) : 0;
    }

    return xino::error_nr::ok;
  }

  /**
   * @brief Update protections for an address range.
   *
   * Updates permissions/attributes for mappings in `[a.addr, a.addr + size)`.
   * This routine operates at the smallest granularity, and will split larger
   * blocks as needed. If @p size is smaller than a page size, a single page
   * permissions/attributes is updated.
   *
   * @param a Start address (VA + ASID for stage-1, IPA for stage-2).
   * @param size Size in bytes. A size of 0 is a no-op.
   * @param p New protection/attribute flags.
   *
   * @retval `xino::error_nr::ok` Success (or `size == 0`).
   * @retval `xino::error_nr::overflow` Address overflow.
   * @retval `xino::error_nr::invalid` A target entry is unmapped or not a leaf
   *          or @p a is not page aligned.
   * @retval `xino::error_nr::nomem` Failed to allocate a page-table page.
   */
  [[nodiscard]] xino::error_t protect_range(addr_t a, std::size_t size,
                                            xino::mm::prot p) noexcept {
    if (size == 0)
      return xino::error_nr::ok;

    const std::size_t gs = xino::mm::va_layout::granule_size();
    // At least `a` should be page aligned.
    if (!a.addr.is_align(gs))
      return xino::error_nr::invalid;

    // Check for overflow.
    if (a.addr + size < a.addr)
      return xino::error_nr::overflow;

    // Pick page granule to enforce protect; it is the safest.
    const unsigned leaf = levels() - 1;
    const std::size_t chunk = level_size(leaf);

    while (size) {
      if (auto ret = protect_one(a, p, leaf); ret != xino::error_nr::ok)
        return ret;

      a.addr += chunk;
      size = (size > chunk) ? (size - chunk) : 0;
    }

    return xino::error_nr::ok;
  }

  /**
   * @brief Unmap an address range.
   *
   * Unmaps `[a.addr, a.addr + size)` at page granularity. Unmapping an already
   * unmapped region is treated as a no-op for the corresponding pages.
   *
   * @param a Start address (VA + ASID for stage-1, IPA for stage-2).
   * @param size Size in bytes. A size of 0 is a no-op.
   *
   * @retval `xino::error_nr::ok` Success (or `size == 0`).
   * @retval `xino::error_nr::overflow` Address overflow.
   * @retval `xino::error_nr::invalid` @p a is not page aligned.
   * @retval `xino::error_nr::nomem` Failed to allocate a page-table page.
   */
  [[nodiscard]] xino::error_t unmap_range(addr_t a, std::size_t size) noexcept {
    if (size == 0)
      return xino::error_nr::ok;

    const std::size_t gs = xino::mm::va_layout::granule_size();
    // At least `a` should be page aligned.
    if (!a.addr.is_align(gs))
      return xino::error_nr::invalid;

    // Check for overflow.
    if (a.addr + size < a.addr)
      return xino::error_nr::overflow;

    // Pick page granule to enforce unmap; it is the safest.
    const unsigned leaf = levels() - 1;
    const std::size_t chunk = level_size(leaf);

    while (size) {
      if (auto ret = unmap_one(a, leaf); ret != xino::error_nr::ok)
        return ret;

      a.addr += chunk;
      size = (size > chunk) ? (size - chunk) : 0;
    }

    return xino::error_nr::ok;
  }

private:
  /** @brief Page-table entry update kind. */
  enum class kind : std::uint8_t {
    INSTALL, /**< Install a new valid descriptor into a FAULT slot. */
    REMOVE,  /**< Remove an existing mapping (set the slot to FAULT). */
    UPDATE   /**< Replace an existing mapping/attributes. */
  };

  /**
   * @brief Allocate and initialize a single page-table page.
   *
   * @return Physical address of the newly allocated page-table page, or
   *         `xino::mm::phys_addr{0}` on failure.
   */
  [[nodiscard]] xino::mm::phys_addr alloc_single_pt() noexcept {
    using namespace xino::barrier;

    xino::mm::phys_addr pa{allocator.alloc_pages(xino::nothrow, 0)};
    if (pa == xino::mm::phys_addr{0})
      return pa;

    pte_t *table{xino::mm::va_layout::phys_to_virt(pa).ptr<pte_t>()};
    // Init PTEs.
    for (unsigned i{0}; i < entries_per_table(); i++)
      table[i] = PTE_TYPE_FAULT;
    // Make sure the table is in FAULT state.
    dmb<opt::ishst>();

    return pa;
  }

  // Geometry APIs.

  [[nodiscard]] static unsigned ia_bits() noexcept {
    if constexpr (Stage == stage::ST_1) {
      return xino::mm::va_layout::va_bits;
    } else { // Stage == stage::ST_2.
      return xino::cpu::state.ipa_bits;
    }
  }

  [[nodiscard]] static unsigned levels() noexcept {
    return levels_for_bits(ia_bits());
  }

  [[nodiscard]] static unsigned level_shift(unsigned level) noexcept {
    return level_shift_for_bits(ia_bits(), level);
  }

  [[nodiscard]] static std::size_t level_size(unsigned level) noexcept {
    return level_size_for_bits(ia_bits(), level);
  }

  // Other helper APIs.

  [[nodiscard]] static bool aligned_for_level(const addr_t &a,
                                              xino::mm::phys_addr pa,
                                              unsigned level) noexcept {
    using av_t = typename addr_t::addr_type::value_type;

    const std::size_t size{level_size(level)};
    // Check if address and pa are both aligned to page or block at a level.
    return ((static_cast<av_t>(a.addr) |
             static_cast<xino::mm::phys_addr::value_type>(pa)) &
            (size - 1)) == 0;
  }

  [[nodiscard]] static unsigned choose_leaf_level(const addr_t &a,
                                                  xino::mm::phys_addr pa,
                                                  std::size_t size) noexcept {
    const unsigned lvls{levels()};

    for (unsigned level{0}; level < lvls; level++) {
      if (size >= level_size(level) && aligned_for_level(a, pa, level))
        return level;
    }

    // Select lowest granule.
    return lvls - 1;
  }

  [[nodiscard]] static unsigned index_for(const addr_t &a,
                                          unsigned level) noexcept {
    using av_t = typename addr_t::addr_type::value_type;

    const unsigned shift{level_shift(level)};
    // Index to the table at a level.
    return static_cast<unsigned>((static_cast<av_t>(a.addr) >> shift) &
                                 (entries_per_table() - 1));
  }

  // Return address that suitable to map at specified level.
  [[nodiscard]] static addr_t block_base(addr_t a, unsigned level) noexcept {
    // Update address, keep ASID untouched..
    a.addr = a.addr.align_down(level_size(level));

    return a;
  }

  // PTE APIs.

  [[nodiscard]] static bool entry_is_table(unsigned level, pte_t pte) noexcept {
    if (!pte_is_table_or_page(pte))
      return false;
    // Or level < levels() - 1; lets not subtract with unsigned.
    return (level + 1) < levels();
  }

  [[nodiscard]] static bool entry_is_block(pte_t pte) noexcept {
    return pte_is_block(pte);
  }

  [[nodiscard]] static bool entry_is_valid(pte_t pte) noexcept {
    return !pte_is_fault(pte);
  }

  static void invalidate_range(const addr_t &a, std::size_t size) noexcept {
    // Assuming `invalidate_*_range()` handles alignment.
    if constexpr (Stage == stage::ST_1) {
      invalidate_va_range(a.addr, size, a.asid);
    } else { // Stage == stage::ST_2.
      invalidate_ipa_range(a.addr, size);
    }
  }

  /**
   * @brief Update a PTE slot and perform required TLB maintenance.
   *
   * Writes @p value into the PTE @p slot. When the MMU is enabled, this helper
   * enforces Arm's break-before-make requirements for descriptor replacement
   * (`UPDATE` and `REMOVE`) and performs the corresponding TLB invalidation
   * for the affected translation range, followed by the required barriers.
   *
   * - If MMU is on and @p k is `UPDATE` or `REMOVE`:
   *     1) Write FAULT to @p slot (break).
   *     2) `dsb(ishst)` to make the store visible.
   *     3) Invalidate the affected range.
   *     4) `dsb(ish)` + `isb()` to complete invalidation and sync the pipeline.
   *
   * - Write the final descriptor (@p value) to @p slot (install).
   *
   * - If MMU is on:
   *     1) `dsb(ishst)`
   *     2) Invalidate the affected range (ensures new mapping is observed).
   *     3) `dsb(ish)` + `isb()`
   *
   * If the MMU is off, this function performs only the descriptor store(s)
   * without barriers or invalidation.
   *
   * @param k Update kind (install/remove/update).
   * @param a Address identifying the translation to invalidate.
   *          It should be **aligned** for @p size.
   * @param size Size in bytes of the affected translation range.
   * @param slot Reference to the PTE slot being updated.
   * @param value Descriptor value to write.
   */
  static void write_pte_and_sync(kind k, const addr_t &a, std::size_t size,
                                 pte_t &slot, pte_t value) noexcept {
    using namespace xino::barrier;

    if (xino::mm::mmu_on) [[likely]] {
      if (k == kind::UPDATE || k == kind::REMOVE) {
        slot = PTE_TYPE_FAULT;
        dsb<opt::ishst>();
        invalidate_range(a, size);
        dsb<opt::ish>();
        isb();
      }
    }

    // Install descriptor.
    slot = value;

    if (xino::mm::mmu_on) [[likely]] {
      dsb<opt::ishst>();
      invalidate_range(a, size);
      dsb<opt::ish>();
      isb();
    }
  }

  /**
   * @brief Ensure a child page-table exists at the given entry and return it.
   *
   * For a non-leaf level, the current table entry must either already reference
   * a child table or be empty (FAULT).
   *
   * - If @p entry already points to a table, the child table physical address
   *   is returned in @p child.
   * - If @p entry is FAULT, a new page-table page is allocated, initialized to
   *   FAULT entries, and @p entry is set to a "table descriptor" that points to
   *   the new child. The new child physical address is returned in @p child.
   *
   * @param[in,out] entry Table entry at the current level to examine/modify.
   * @param[in] level Current level of @p entry within the walk.
   * @param[out] child Receives the physical address of the child table.
   *
   * @retval `xino::error_nr::ok` Child table is available and returned.
   * @retval `xino::error_nr::invalid` @p entry was valid but not a table.
   * @retval `xino::error_nr::nomem` Allocation of a new child table failed.
   */
  [[nodiscard]] xino::error_t
  ensure_next_table(pte_t &entry, unsigned level,
                    xino::mm::phys_addr &child) noexcept {
    // If PTE is already a table return it.
    if (entry_is_table(level, entry)) {
      child = pte_encoder<Stage>::pte_to_phys(entry);

      return xino::error_nr::ok;
    }

    // If PTE is valid but not a table, i.e. already mapped.
    if (!pte_is_fault(entry))
      return xino::error_nr::invalid;

    xino::mm::phys_addr pa{alloc_single_pt()};
    if (pa == xino::mm::phys_addr{0})
      return xino::error_nr::nomem;

    // Install the table; FAULT to VALID, so no sync.
    // `alloc_single_pt()` already does `dmb<opt::ishst>()`.
    entry = pte_encoder<Stage>::make_table(pa);

    child = pa;

    return xino::error_nr::ok;
  }

  /**
   * @brief Split a block mapping into a child table of smaller mappings.
   *
   * If @p entry at @p level is a block descriptor, this function allocates a
   * new page-table and populates it with entries that cover the same address
   * range as the original block, preserving the original block attributes.
   *
   * After building the child table, the original block entry is replaced with a
   * table descriptor that points to the new child. When the MMU is enabled, the
   * replacement is performed via @ref write_pte_and_sync to satisfy
   * break-before-make rules and to invalidate the affected translations.
   *
   * If @p entry is not a block, this is a no-op and returns success.
   *
   * @param[in] a Address that identifies this translation for TLB maintenance.
   *              It should be **aligned** for the block at @p level.
   * @param[in,out] entry The PTE entry to potentially split.
   * @param[in] level Level of @p entry in the page-table walk.
   *
   * @retval `xino::error_nr::ok` Success (including the no-op case).
   * @retval `xino::error_nr::invalid` @p a is not aligned for a block.
   * @retval `xino::error_nr::nomem` Failed to allocate the new child table.
   */
  [[nodiscard]] xino::error_t split_block(const addr_t &a, pte_t &entry,
                                          unsigned level) noexcept {
    using namespace xino::barrier;

    // It is not a block to split.
    if (!entry_is_block(entry))
      return xino::error_nr::ok;

    const std::size_t ls = level_size(level);
    // `a` should be page aligned for the block at level.
    if (!a.addr.is_align(ls))
      return xino::error_nr::invalid;

    xino::mm::phys_addr pa{alloc_single_pt()};
    if (pa == xino::mm::phys_addr{0})
      return xino::error_nr::nomem;

    pte_t *table{xino::mm::va_layout::phys_to_virt(pa).ptr<pte_t>()};

    // Extracts the physical address and attributes from the PTE.
    const xino::mm::phys_addr pte_pa{pte_encoder<Stage>::pte_to_phys(entry)};
    const pte_t pte_attr{entry & pte_attr_field_mask()};

    // Size of block for the next availabe level.
    const std::size_t sub_sz{level_size(level + 1)};

    for (unsigned i{0}; i < entries_per_table(); i++) {
      const xino::mm::phys_addr next{pte_pa + (sub_sz * i)};

      const pte_t leaf =
          (level + 2 < levels())
              ? pte_encoder<Stage>::make_leaf_block_attr(next, pte_attr)
              : pte_encoder<Stage>::make_leaf_page_attr(next, pte_attr);

      table[i] = leaf;
    }

    // Make sure updates to table are visible.
    // `write_pte_and_sync()` does not do any barrier if MMU is off.
    dmb<opt::ishst>();

    // Replacing a block with a table.
    write_pte_and_sync(kind::UPDATE, a, level_size(level), entry,
                       pte_encoder<Stage>::make_table(pa));

    return xino::error_nr::ok;
  }

  // PTE operations: MAP, UNMAP, and PROTECT.

  [[nodiscard]] xino::error_t map_one(const addr_t &a, xino::mm::phys_addr pa,
                                      xino::mm::prot p,
                                      unsigned leaf_level) noexcept {
    pte_t *t{pt_root};

    for (unsigned level{0}; level < leaf_level; level++) {
      const unsigned idx = index_for(a, level);

      pte_t &entry = t[idx];

      // If entry is a block, allocate a table and break the block.
      if (auto ret = split_block(block_base(a, level), entry, level);
          ret != xino::error_nr::ok)
        return ret;

      xino::mm::phys_addr child{};
      // At level < leaf_level, make sure entry is a table.
      if (auto ret = ensure_next_table(entry, level, child);
          ret != xino::error_nr::ok)
        return ret;

      t = xino::mm::va_layout::phys_to_virt(child).ptr<pte_t>();
    }

    // At leaf_level, t should be updated.

    const unsigned idx = index_for(a, leaf_level);
    // Make sure entry is FAULT.
    if (entry_is_valid(t[idx]))
      return xino::error_nr::invalid;

    const bool device = static_cast<bool>(p & xino::mm::prot::DEVICE);

    const pte_t pte = (leaf_level + 1) < levels()
                          ? pte_encoder<Stage>::make_leaf_block(pa, p, device)
                          : pte_encoder<Stage>::make_leaf_page(pa, p, device);

    write_pte_and_sync(kind::INSTALL, block_base(a, leaf_level),
                       level_size(leaf_level), t[idx], pte);

    return xino::error_nr::ok;
  }

  [[nodiscard]] xino::error_t unmap_one(const addr_t &a,
                                        unsigned leaf_level) noexcept {
    pte_t *t{pt_root};

    for (unsigned level{0}; level < leaf_level; level++) {
      const unsigned idx = index_for(a, level);

      pte_t &entry = t[idx];

      // If entry is not valid, there is nothing to unmap.
      if (!entry_is_valid(entry))
        return xino::error_nr::ok;

      if (entry_is_block(entry)) {
        // If entry is a block, allocate a table and break the block.
        if (auto ret = split_block(block_base(a, level), entry, level);
            ret != xino::error_nr::ok)
          return ret;
      }

      // After splitting, must be a table to continue (check only to be safe).
      if (!entry_is_table(level, entry)) [[unlikely]]
        return xino::error_nr::ok;

      const xino::mm::phys_addr child = pte_encoder<Stage>::pte_to_phys(entry);
      // t is a page table.
      t = xino::mm::va_layout::phys_to_virt(child).ptr<pte_t>();
    }

    // At leaf_level, t should be updated.

    const unsigned idx = index_for(a, leaf_level);
    // Make sure entry is not FAULT.
    if (!entry_is_valid(t[idx]))
      return xino::error_nr::ok;

    write_pte_and_sync(kind::REMOVE, block_base(a, leaf_level),
                       level_size(leaf_level), t[idx], PTE_TYPE_FAULT);

    return xino::error_nr::ok;
  }

  [[nodiscard]] xino::error_t protect_one(const addr_t &a, xino::mm::prot p,
                                          unsigned leaf_level) noexcept {
    pte_t *t{pt_root};

    for (unsigned level = 0; level < leaf_level; level++) {
      const unsigned idx = index_for(a, level);

      pte_t &entry = t[idx];

      // If entry is not valid, unable to update permission.
      if (!entry_is_valid(entry))
        return xino::error_nr::invalid;

      if (entry_is_block(entry)) {
        // If entry is a block, allocate a table and break the block.
        if (auto ret = split_block(block_base(a, level), entry, level);
            ret != xino::error_nr::ok)
          return ret;
      }

      // After splitting, must be a table to continue (check only to be safe).
      if (!entry_is_table(level, entry)) [[unlikely]]
        return xino::error_nr::invalid;

      const xino::mm::phys_addr child = pte_encoder<Stage>::pte_to_phys(entry);
      // t is a page table.
      t = xino::mm::va_layout::phys_to_virt(child).ptr<pte_t>();
    }

    // At leaf_level, t should be updated.

    const unsigned idx = index_for(a, leaf_level);
    // Make sure entry is not FAULT, and is a page or block.
    if (!entry_is_valid(t[idx]) || entry_is_table(leaf_level, t[idx]))
      return xino::error_nr::invalid;

    // Extracts the physical address form PTE.
    const xino::mm::phys_addr pa = pte_encoder<Stage>::pte_to_phys(t[idx]);

    const bool device = static_cast<bool>(p & xino::mm::prot::DEVICE);

    const pte_t pte = (leaf_level + 1) < levels()
                          ? pte_encoder<Stage>::make_leaf_block(pa, p, device)
                          : pte_encoder<Stage>::make_leaf_page(pa, p, device);

    write_pte_and_sync(kind::UPDATE, block_base(a, leaf_level),
                       level_size(leaf_level), t[idx], pte);

    return xino::error_nr::ok;
  }

  Allocator &allocator;
  // PT root.
  pte_t pt_root[entries_per_table()];
};

} // namespace xino::mm::paging

#endif // __MM_PAGING_HPP__
