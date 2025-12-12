/**
 * @file mm.hpp
 * @brief Core **abstract** mm types.
 *
 * The page-table builder and architecture-specific code translate these
 * abstract attributes into concrete AArch64 MMU bits.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __MM_HPP__
#define __MM_HPP__

#include <compare>
#include <cstddef>
#include <cstdint>

namespace xino::mm {

/* ADDRESS TAGS. */

/** @brief Tag for physical addresses. */
struct phys_tag {};
/** @brief Tag for bus/peripheral addresses. */
struct bus_tag {};
/** @brief Tag for virtual addresses. */
struct virt_tag {};

/**
 * @class address
 * @brief Opaque, strongly-typed address wrapper with byte arithmetic.
 *
 * @tparam Tag Empty tag type distinguishing the address space.
 */
template <typename Tag> class [[nodiscard]] address {
public:
  /** @brief Unsigned integer type used to store the address. */
  using value_type = std::uintptr_t;

  /** @brief Signed difference type (in bytes) between two addresses. */
  using difference_type = std::ptrdiff_t;

  /** @brief Construct a zero address. */
  constexpr address() noexcept = default;

  /** @brief Construct from raw integer value. */
  explicit constexpr address(value_type a) noexcept : addr{a} {}

  /**
   * @brief Align the address *up* to @p align.
   *
   * @param align Alignment in bytes (pre: `align != 0` and power-of-two).
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
   *
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

  /** @name Arithmetic ops. */
  ///@{
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
    a -= off;
    return a;
  }

  /**
   * @brief Difference in **bytes** between two addresses.
   *
   * @param lhs Left-hand side address.
   * @param rhs Right-hand side address.
   * @return Signed byte distance (`ptrdiff_t`).
   */
  [[nodiscard]] friend constexpr difference_type
  operator-(address lhs, address rhs) noexcept {
    return static_cast<difference_type>(lhs.addr - rhs.addr);
  }
  ///@}

  /** @brief Three-way comparison (defaulted). */
  [[nodiscard]] friend constexpr auto
  operator<=>(address lhs, address rhs) noexcept = default;

private:
  /** @brief Stored address value (bytes). */
  value_type addr{};
};

/* ADDRESS TYPE ALIASES. */

/** @brief Physical address type. */
using phys_addr = address<phys_tag>;
/** @brief Bus/peripheral address type. */
using bus_addr = address<bus_tag>;
/** @brief Virtual address type. */
using virt_addr = address<virt_tag>;

/* PROTECTION FLAGS. */

/**
 * @class prot
 * @brief Bitmask of abstract mapping protections/attributes.
 *
 * The page-table builder converts ::xino::mm::prot into architecture-
 * specific descriptor bits (e.g., AArch64 PTE fields).
 */
class prot {
public:
  /** @brief Underlying bitmask type. */
  using mask_t = std::uint16_t;

  /** @name Bit flags. */
  ///@{
  static constexpr mask_t NONE{0x0};
  static constexpr mask_t READ{0x1};
  static constexpr mask_t WRITE{0x2};
  static constexpr mask_t EXEC{0x4};
  static constexpr mask_t USER{0x8};
  static constexpr mask_t DEVICE{0x10};
  static constexpr mask_t SHARED{0x20};
  ///@}

  /** @brief Construct with no flags set. */
  constexpr prot() noexcept = default;

  /** @brief Return the raw bitmask. */
  constexpr prot(mask_t f) noexcept : flags{f} {}

  [[nodiscard]] constexpr mask_t raw() const noexcept { return flags; }

  /**
   * @brief Test if any bit in @p flag is set.
   *
   * @param flag Bitmask of flags to test.
   * @return `true` if any requested bit is set, otherwise `false`.
   */
  [[nodiscard]] constexpr bool has(mask_t flag) const noexcept {
    return (flags & flag) != 0;
  }

  /** @name Bitwise ops (closed over prot). */
  ///@{
  // Bitwise OR
  [[nodiscard]] friend constexpr prot operator|(prot lhs, prot rhs) noexcept {
    return prot{static_cast<mask_t>(lhs.flags | rhs.flags)};
  }

  // Bitwise AND
  [[nodiscard]] friend constexpr prot operator&(prot lhs, prot rhs) noexcept {
    return prot{static_cast<mask_t>(lhs.flags & rhs.flags)};
  }

  // Bitwise NOT
  [[nodiscard]] constexpr prot operator~() const noexcept {
    return prot{static_cast<mask_t>(~flags)};
  }

  // OR-assign
  constexpr prot &operator|=(prot other) noexcept {
    flags |= other.flags;
    return *this;
  }

  // AND-assign
  constexpr prot &operator&=(prot other) noexcept {
    flags &= other.flags;
    return *this;
  }
  ///@}

private:
  /** @brief Stored protection bits. */
  mask_t flags{};
};

} // namespace xino::mm

/* TLB MAINTENANCE. */

#endif // __MM_HPP__
