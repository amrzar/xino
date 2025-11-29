/**
 * @file addr_types.hpp
 * @brief Strongly-typed address wrappers for phys/bus/virt spaces.
 *
 * This header defines a small, constexpr-friendly `address<Tag>` wrapper
 * around `std::uintptr_t` with byte-based arithmetic, alignment helpers,
 * and comparisons. Three empty tag types (`phys_tag`, `bus_tag`, `virt_tag`)
 * are provided and re-exported as short aliases:
 *
 *   - `xino::phys::addr` : physical address
 *   - `xino::bus::addr`  : bus/peripheral address
 *   - `xino::virt::addr` : virtual address
 *
 * The type system prevents accidental mixing of address spaces at compile time.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __ADDR_TYPES_HPP__
#define __ADDR_TYPES_HPP__

#include <compare>
#include <cstddef>
#include <cstdint>

namespace xino {

/** @brief Tag for physical addresses. */
struct phys_tag {};
/** @brief Tag for bus/peripheral addresses. */
struct bus_tag {};
/** @brief Tag for virtual addresses. */
struct virt_tag {};

/**
 * @tparam Tag  Empty tag type distinguishing the address space.
 *
 * @class address
 * @brief Opaque, strongly-typed address wrapper with byte arithmetic.
 */
template <typename Tag> class [[nodiscard]] address {
public:
  /** @brief Unsigned integer type used to store the address. */
  using value_type = std::uintptr_t;

  /** @brief Signed difference type (in bytes) between two addresses. */
  using difference_type = std::ptrdiff_t;

  constexpr address() = default;

  explicit constexpr address(value_type a) noexcept : addr{a} {}

  /**
   * @brief Align the address *up* to @p align.
   * @param align Alignment in bytes (pre: `align != 0`, power-of-two).
   * @return A new `address` rounded up to a multiple of @p align.
   *
   * @note This uses the common `(x + a - 1) & ~(a - 1)` idiom.
   */
  [[nodiscard]] constexpr address align(std::size_t align) const noexcept {
    const auto mask = static_cast<value_type>(align - 1);
    return address{static_cast<value_type>((addr + mask) & ~mask)};
  }

  [[nodiscard]] explicit constexpr operator value_type() const noexcept {
    return addr;
  }

  /**
   * @brief View the address (optionally offset) as a pointer to `U`.
   * @tparam U Pointee type.
   * @param off Optional byte offset added to the base address.
   * @return `U *` obtained via `reinterpret_cast` from `addr + off`.
   *
   * @warning This does not validate mapping or alignment. Use with care.
   */
  template <typename U>
  [[nodiscard]] constexpr U *as_ptr(std::uintptr_t off = 0) const noexcept {
    return reinterpret_cast<U *>(addr + off);
  }

  constexpr address &operator+=(std::uintptr_t off) noexcept {
    addr += off;
    return *this;
  }

  constexpr address &operator-=(std::uintptr_t off) noexcept {
    addr -= off;
    return *this;
  }

  // Return `a + off` (bytes).
  [[nodiscard]] friend constexpr address
  operator+(address a, std::uintptr_t off) noexcept {
    a += off;
    return a;
  }

  // Return `off + a` (bytes).
  [[nodiscard]] friend constexpr address operator+(std::uintptr_t off,
                                                   address a) noexcept {
    a += off;
    return a;
  }

  // Return `a - off` (bytes).
  [[nodiscard]] friend constexpr address
  operator-(address a, std::uintptr_t off) noexcept {
    a += off;
    return a;
  }

  /**
   * @brief Difference in **bytes** between two addresses.
   * @return Signed byte distance (`ptrdiff_t`).
   */
  [[nodiscard]] friend constexpr difference_type
  operator-(address lhs, address rhs) noexcept {
    return static_cast<difference_type>(lhs.addr - rhs.addr);
  }

  [[nodiscard]] friend constexpr auto operator<=>(address,
                                                  address) noexcept = default;

private:
  /** @brief Stored address value (bytes). */
  value_type addr{};
};

namespace phys {
using addr = address<phys_tag>;
} // namespace phys

namespace bus {
using addr = address<bus_tag>;
} // namespace bus

namespace virt {
using addr = address<virt_tag>;
} // namespace virt

} // namespace xino

#endif // __ADDR_TYPES_HPP__
