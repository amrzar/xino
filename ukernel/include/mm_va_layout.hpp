/**
 * @file mm_va_layout.hpp
 * @brief uKernel virtual address (VA) layout definitions.
 *
 * This header defines the **virtual address space layout** used by the uKernel
 * when the MMU is enabled, and provides small helper routines to translate
 * between physical addresses (PA) and **usable** virtual addresses (VA).
 *
 * The uKernel VA space spans is `[ukernel_va_start, ukernel_va_end]`.
 *
 * Within this space, the following windows are reserved (high to low):
 *
 * **uKernel image mapping window**: `[ukimage_va, ukimage_end]`
 *  - Size: `UKERNEL_KIMAGE_SLOT_SIZE`
 *  - When KASLR is enabled, the image may be relocated within this slot.
 *  - The chosen runtime base is stored in
 *    `xino::mm::va_layout::ukimage_va_base`.
 *
 * **Device mapping window**: `[devmap_va, devmap_end]`
 *  - Size: `UKERNEL_DEVMAP_SLOT_SIZE`
 *  - Intended for temporary or permanent device MMIO mappings.
 *
 * **Direct map window**: `[page_offset, page_end]`
 *  - Provides a linear mapping of physical memory when the MMU is on.
 *  - Used by `xino::mm::va_layout::phys_to_virt()` and
 *    `xino::mm::va_layout::virt_to_phys()`.
 *
 * @author Amirreza Zarrabi
 * @date 2026
 */

#ifndef __MM_VA_LAYOUT_HPP__
#define __MM_VA_LAYOUT_HPP__

#include <config.h> // UKERNEL_VA_BITS, UKERNEL_PAGE_nK
#include <cstdint>
#include <mm.hpp> // phys_addr, virt_addr
#include <optional>

namespace xino::mm::va_layout {

/** @brief Log2 of the granule size of uKernel. */
[[nodiscard]] constexpr unsigned granule_shift() noexcept {
#if defined(UKERNEL_PAGE_4K)
  return 12; // 4KB pages bits.
#elif defined(UKERNEL_PAGE_16K)
  return 14; // 16KB pages bits.
#else
#error "UKERNEL_PAGE_4K and UKERNEL_PAGE_16K are supported"
#endif
}

/** @brief uKernel granule size in bytes. */
[[nodiscard]] constexpr std::size_t granule_size() noexcept {
  return std::size_t{1} << granule_shift();
}

constexpr unsigned va_bits{UKERNEL_VA_BITS};

/** @name uKernel VA space `[ukernel_va_start, ukernel_va_end]`. */
///@{
// Size (in bytes) of the uKernel virtual address space.
constexpr std::uintptr_t ukernel_va_size{(std::uintptr_t{1} << va_bits)};
constexpr xino::mm::virt_addr ukernel_va_end{~std::uintptr_t{0}};
constexpr xino::mm::virt_addr ukernel_va_start{~(ukernel_va_size - 1)};
///@}

/** @name uKernel image mapping window `[ukimage_va, ukimage_end]`. */
///@{
// Size (in bytes) reserved for the uKernel image mapping slot.
constexpr std::uintptr_t ukimage_slot_size{UKERNEL_KIMAGE_SLOT_SIZE};
constexpr xino::mm::virt_addr ukimage_end{ukernel_va_end};
constexpr xino::mm::virt_addr ukimage_va{ukimage_end - ukimage_slot_size + 1};
///@}

/** @name Device mapping window `[devmap_va, devmap_end]`. */
///@{
// Size (in bytes) reserved for the device mapping slot.
constexpr std::uintptr_t devmap_slot_size{UKERNEL_DEVMAP_SLOT_SIZE};
constexpr xino::mm::virt_addr devmap_end{ukimage_va - 1};
constexpr xino::mm::virt_addr devmap_va{devmap_end - devmap_slot_size + 1};
///@}

/** @name Direct map window `[page_offset, page_end]`. */
///@{
constexpr xino::mm::virt_addr page_offset{ukernel_va_start};
constexpr xino::mm::virt_addr page_end{devmap_va - 1};
///@}

/**
 * @brief Runtime VA base of the uKernel image mapping.
 *
 * If KASLR is available, the image may be relocated within the fixed
 * `[ukimage_va, ukimage_end]` slot; this variable holds the chosen base. If
 * KASLR is not used, it defaults to `ukimage_va`.
 *
 * The actual mapped image range is
 *   `[ukimage_va_base, ukimage_va_base + ukimage_slot_size - 1]`.
 */
inline xino::mm::virt_addr ukimage_va_base{ukimage_va};

/** @brief Runtime PA base of the uKernel image.  */
inline xino::mm::phys_addr ukimage_pa_base{};

[[nodiscard]] inline bool is_ukimage(xino::mm::virt_addr &va) noexcept {
  if (!xino::mm::mmu_on) [[unlikely]]
    return false;
  return ukimage_va_base <= va &&
         va <= xino::mm::virt_addr{ukimage_va_base + ukimage_slot_size - 1};
}

[[nodiscard]] inline bool is_devmap(xino::mm::virt_addr &va) noexcept {
  if (!xino::mm::mmu_on) [[unlikely]]
    return false;
  return devmap_va <= va && va <= devmap_end;
}

[[nodiscard]] inline bool is_direct_map(xino::mm::virt_addr &va) noexcept {
  if (!xino::mm::mmu_on) [[unlikely]]
    return false;
  return page_offset <= va && va <= page_end;
}

/**
 * @brief Convert a physical address to a **usable** virtual address.
 *
 * @param pa Physical address to convert.
 * @return Virtual address corresponding to @p pa (identity if MMU is off,
 *         direct-map if MMU is on).
 */
[[nodiscard]] inline xino::mm::virt_addr
phys_to_virt(xino::mm::phys_addr pa) noexcept {
  const auto value{static_cast<xino::mm::phys_addr::value_type>(pa)};

  if (!xino::mm::mmu_on) [[unlikely]]
    return xino::mm::virt_addr{value};
  // Direct map.
  return page_offset + value;
}

/**
 * @brief Convert a virtual address to its corresponding physical address.
 *
 * @param va Virtual address to convert.
 * @return The physical address if @p va belongs to a recognized region;
 *         @c std::nullopt otherwise.
 */
[[nodiscard]] inline std::optional<xino::mm::phys_addr>
virt_to_phys(xino::mm::virt_addr va) noexcept {
  const auto value{static_cast<xino::mm::virt_addr::value_type>(va)};

  if (!xino::mm::mmu_on) [[unlikely]]
    return xino::mm::phys_addr{value};

  if (is_direct_map(va))
    return xino::mm::phys_addr{
        value - static_cast<xino::mm::virt_addr::value_type>(page_offset)};

  if (is_ukimage(va)) {
    return ukimage_pa_base +
           (value -
            static_cast<xino::mm::virt_addr::value_type>(ukimage_va_base));
  }

  // Devmap or unknown address.
  return std::nullopt;
}

} // namespace xino::mm::va_layout

#endif // __MM_VA_LAYOUT_HPP__
