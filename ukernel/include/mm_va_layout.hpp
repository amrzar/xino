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
 *  - When KASLR is enabled, the image may be relocated within this window.
 *  - The chosen runtime base is stored in
 *    `xino::mm::va_layout::ukimage_va_base`.
 *
 * **Device mapping window**: `[devmap_va, devmap_end]`
 *  - Size: `UKERNEL_DEVMAP_SLOT_SIZE`
 *  - Intended for temporary or permanent device MMIO mappings.
 *
 * **Direct map window**: `[page_offset, page_end]`
 *  - Provides a linear mapping of physical memory (PA{0}) when the MMU is on.
 *  - Used by `xino::mm::va_layout::phys_to_virt()` and
 *    `xino::mm::va_layout::virt_to_phys()`.
 *
 * @author Amirreza Zarrabi
 * @date 2026
 */

#ifndef __MM_VA_LAYOUT_HPP__
#define __MM_VA_LAYOUT_HPP__

#include <config.h> // UKERNEL_VA_BITS, UKERNEL_PAGE_*, UKERNEL_BASE, UKERNEL_*_SLOT_SIZE
#include <cstdint>
#include <mm.hpp> // phys_addr, virt_addr
#include <optional>
#include <type_traits>

namespace xino::mm::va_layout {

/** @brief Log2 of the granule size of uKernel. */
[[nodiscard]] constexpr unsigned granule_shift() noexcept {
  return UKERNEL_PAGE_SHIFT;
}

/** @brief uKernel granule size in bytes. */
[[nodiscard]] constexpr std::size_t granule_size() noexcept {
  return static_cast<std::size_t>(UKERNEL_PAGE_SIZE);
}

constexpr unsigned va_bits{UKERNEL_VA_BITS};

/** @brief uKernel compile base address. */
constexpr xino::mm::virt_addr ukernel_base{UKERNEL_BASE};

/** @name uKernel VA space `[ukernel_va_start, ukernel_va_end]`. */
///@{
// Size (in bytes) of the uKernel virtual address space.
constexpr std::uintptr_t ukernel_va_size{(std::uintptr_t{1} << va_bits)};
constexpr xino::mm::virt_addr ukernel_va_end{~std::uintptr_t{0}};
constexpr xino::mm::virt_addr ukernel_va_start{~(ukernel_va_size - 1)};
///@}

/** @name uKernel image mapping window `[ukimage_va, ukimage_end]`. */
///@{
// Size (in bytes) reserved for the uKernel image mapping window.
constexpr std::uintptr_t ukimage_slot_size{UKERNEL_KIMAGE_SLOT_SIZE};
constexpr xino::mm::virt_addr ukimage_end{ukernel_va_end};
constexpr xino::mm::virt_addr ukimage_va{ukimage_end - ukimage_slot_size + 1};
///@}

/** @name Device mapping window `[devmap_va, devmap_end]`. */
///@{
// Size (in bytes) reserved for the device mapping window.
constexpr std::uintptr_t devmap_slot_size{UKERNEL_DEVMAP_SLOT_SIZE};
constexpr xino::mm::virt_addr devmap_end{ukimage_va - 1};
constexpr xino::mm::virt_addr devmap_va{devmap_end - devmap_slot_size + 1};
///@}

/** @name Direct map window `[page_offset, page_end]`. */
///@{
constexpr xino::mm::virt_addr page_offset{ukernel_va_start};
constexpr xino::mm::virt_addr page_end{devmap_va - 1};
///@}

// `ukimage_va_base` and `ukimage_pa_base` are initialized at boot.
// See mm_va_layout.cpp for details.
extern xino::mm::virt_addr ukimage_va_base;
extern xino::mm::phys_addr ukimage_pa_base;
// Size the uKernel (`__image_end - __image_start`).
extern std::size_t ukimage_size;

// Call only if MMU is on.
[[nodiscard]] inline bool is_ukimage(xino::mm::virt_addr va) noexcept {
  return ukimage_va_base <= va &&
         va <= xino::mm::virt_addr{ukimage_va_base + ukimage_size - 1};
}

// Call only if MMU is on.
[[nodiscard]] constexpr bool is_devmap(xino::mm::virt_addr va) noexcept {
  return devmap_va <= va && va <= devmap_end;
}

// Call only if MMU is on.
[[nodiscard]] constexpr bool is_direct_map(xino::mm::virt_addr va) noexcept {
  return page_offset <= va && va <= page_end;
}

/**
 * @brief Translate a PA to a **usable** VA.
 *
 * When the MMU is enabled, the uKernel exposes a direct-map window
 * `[page_offset, page_end]` that linearly maps physical memory starting at
 * `PA{0}`. This function returns the corresponding VA in that window.
 *
 * When the MMU is disabled (@p mmu_on is false), the function performs an
 * identity mapping (i.e. returns the same numerical address as a VA).
 *
 * @param pa Physical address to translate.
 * @param mmu_on Whether the MMU is enabled (defaults to true).
 *
 * @return Virtual address corresponding to @p pa, either in the direct-map
 *         window (MMU on) or identical to @p pa (MMU off).
 */
[[nodiscard, gnu::unused]] static constexpr xino::mm::virt_addr
phys_to_virt(xino::mm::phys_addr pa, bool mmu_on = true) noexcept {
  using av_t = xino::mm::phys_addr::value_type;

  if (!mmu_on) {
    // Expected to optimized out (O2/Os) for default mmu_on.
    return xino::mm::virt_addr{static_cast<av_t>(pa)};
  }

  return page_offset + static_cast<av_t>(pa);
}

/**
 * @brief Translate a VA to a PA in a recognized windows.
 *
 * When the MMU is enabled, this function recognizes the following windows:
 *  - Direct-map window: VAs in `[page_offset, page_end]` map linearly to
 *    physical memory starting at `PA{0}`.
 *  - uKernel image window: VAs in
 *   `[ukimage_va_base, ukimage_va_base + ukimage_size - 1]` map to
 *   `[ukimage_pa_base, ukimage_pa_base + ukimage_size - 1]`.
 *
 * When the MMU is disabled (@p mmu_on is false), the function performs an
 * identity mapping (i.e. returns the same numerical address as a PA).
 *
 * @param va Virtual address to translate.
 * @param mmu_on Whether the MMU is enabled (defaults to true).
 *
 * @return Physical address corresponding to @p va if the VA lies in a known
 *         window, otherwise `std::nullopt`.
 */
[[nodiscard, gnu::unused]] static std::optional<xino::mm::phys_addr>
virt_to_phys(xino::mm::virt_addr va, bool mmu_on = true) noexcept {
  using av_t = xino::mm::virt_addr::value_type;

  if (!mmu_on) {
    // Expected to optimized out (O2/Os) for default mmu_on.
    return xino::mm::phys_addr{static_cast<av_t>(va)};
  }

  if (is_direct_map(va)) {
    // VA e `[page_offset, page_end]`.
    return xino::mm::phys_addr{static_cast<av_t>(va) -
                               static_cast<av_t>(page_offset)};
  }

  if (is_ukimage(va)) {
    // VA e `[ukimage_va_base, ukimage_va_base + ukimage_size - 1]`.
    return ukimage_pa_base +
           (static_cast<av_t>(va) - static_cast<av_t>(ukimage_va_base));
  }

  // Devmap or unknown address.
  return std::nullopt;
}

} // namespace xino::mm::va_layout

#endif // __MM_VA_LAYOUT_HPP__
