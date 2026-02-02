/**
 * @file mm.hpp
 * @brief Core mm *abstract* types.
 *
 * The page-table builder code translate abstract attributes into concrete
 * AARCH64 MMU bits and operations.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __MM_HPP__
#define __MM_HPP__

#include <compare> // for defaulted operator<=> support
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
/** @brief Tag for guest intermediate physical addresses (IPA). */
struct ipa_tag {};

/**
 * @class address_base
 * @brief Opaque, strongly-typed address wrapper with byte arithmetic.
 *
 * @tparam Derived The concrete address type inheriting from this base.
 * @tparam Tag Empty tag type distinguishing the address space.
 */
template <typename Derived, typename Tag> class [[nodiscard]] address_base {
public:
  /** @brief Unsigned integer type used to store the address. */
  using value_type = std::uintptr_t;

  /**
   * @brief Align the address *up* to @p align.
   *
   * @param align Alignment in bytes.
   * @pre `align != 0` and power-of-two.
   * @return A new `address` rounded up to a multiple of @p align.
   *
   * @note This uses the common `(x + a - 1) & ~(a - 1)` idiom.
   */
  [[nodiscard]] constexpr Derived align_up(std::size_t align) const noexcept {
    const value_type mask{static_cast<value_type>(align) - 1};
    return Derived{static_cast<value_type>((addr + mask) & ~mask)};
  }

  /**
   * @brief Align the address *down* to @p align.
   *
   * @param align Alignment in bytes.
   * @pre `align != 0` and power-of-two.
   * @return A new `address` rounded down to a multiple of @p align.
   *
   * @note This uses the common `x & ~(a - 1)` idiom.
   */
  [[nodiscard]] constexpr Derived align_down(std::size_t align) const noexcept {
    const value_type mask{static_cast<value_type>(align) - 1};
    return Derived{static_cast<value_type>(addr & ~mask)};
  }

  /**
   * @brief Test whether the address is aligned to @p align.
   *
   * @param align Alignment in bytes.
   * @pre `align != 0` and power-of-two.
   * @return `true` if the address is aligned to @p align, otherwise `false`.
   */
  [[nodiscard]] constexpr bool is_align(std::size_t align) const noexcept {
    const value_type mask{static_cast<value_type>(align) - 1};
    return (addr & mask) == 0;
  }

  /**
   * @brief Convert this strongly-typed address to its integer value.
   *
   * This explicit conversion (e.g. `static_cast<std::uintptr_t>()`) returns
   * the underlying address value.
   *
   * @return The underlying address value.
   *
   * @note It is marked `explicit` to avoid accidental mixing of different
   * address spaces in arithmetic (e.g., `phys_addr{A} + virt_addr{B}`) or
   * comparisons (e.g., `phys_addr{A} < virt_addr{B}`) that would silently
   * discard the type tag.
   */
  [[nodiscard]] explicit constexpr operator value_type() const noexcept {
    return addr;
  }

  /** @name Arithmetic ops. */
  ///@{
  constexpr Derived &operator+=(std::uintptr_t off) noexcept {
    addr += off;
    // Return the derived object vs. `address_base<Derived,Tag>&`.
    // CRTP pattern, `this` is guaranteed to be `Derived`.
    return static_cast<Derived &>(*this);
  }

  constexpr Derived &operator-=(std::uintptr_t off) noexcept {
    addr -= off;
    // Return the derived object vs. `address_base<Derived,Tag>&`.
    // CRTP pattern, `this` is guaranteed to be `Derived`.
    return static_cast<Derived &>(*this);
  }

  // Return `a + off`.
  [[nodiscard]] friend constexpr Derived
  operator+(Derived a, std::uintptr_t off) noexcept {
    a += off;
    return a;
  }

  // Return `off + a`.
  [[nodiscard]] friend constexpr Derived operator+(std::uintptr_t off,
                                                   Derived a) noexcept {
    a += off;
    return a;
  }

  // Return `a - off`.
  [[nodiscard]] friend constexpr Derived
  operator-(Derived a, std::uintptr_t off) noexcept {
    a -= off;
    return a;
  }

  // Return `a - b`.
  [[nodiscard]] friend constexpr std::ptrdiff_t
  operator-(const Derived &a, const Derived &b) noexcept {
    return static_cast<std::ptrdiff_t>(a.addr - b.addr);
  }
  ///@}

  /** @name Comparison ops. */
  ///@{
  // Not-defaulted as argument may be incomplete at instantiation time.
  [[nodiscard]] friend constexpr auto operator<=>(const Derived &lhs,
                                                  const Derived &rhs) noexcept {
    return lhs.addr <=> rhs.addr;
  }

  // Not-defaulted three way, compiler will not implicitly generate `==`.
  [[nodiscard]] friend constexpr bool operator==(const Derived &lhs,
                                                 const Derived &rhs) noexcept {
    return lhs.addr == rhs.addr;
  }
  ///@}

protected:
  /** @brief Construct a zero address. */
  constexpr address_base() noexcept : addr{0} {};

  /** @brief Construct from raw integer value. */
  explicit constexpr address_base(value_type a) noexcept : addr{a} {}

  /** @brief Stored address value (bytes). */
  value_type addr;
};

/** @name Address types aliases. */
///@{

/**
 * @class phys_addr
 * @brief Physical address type.
 */
class [[nodiscard]] phys_addr : public address_base<phys_addr, phys_tag> {
public:
  using value_type = address_base<phys_addr, phys_tag>::value_type;

  /** @brief Construct a zero address. */
  constexpr phys_addr() noexcept = default;

  /** @brief Construct from raw integer value. */
  explicit constexpr phys_addr(value_type a) noexcept
      : address_base<phys_addr, phys_tag>(a) {}
};

/**
 * @class bus_addr
 * @brief Bus/peripheral address type.
 */
class [[nodiscard]] bus_addr : public address_base<bus_addr, bus_tag> {
public:
  using value_type = address_base<bus_addr, bus_tag>::value_type;

  /** @brief Construct a zero address. */
  constexpr bus_addr() noexcept = default;

  /** @brief Construct from raw integer value. */
  explicit constexpr bus_addr(value_type a) noexcept
      : address_base<bus_addr, bus_tag>(a) {}
};

/**
 * @class virt_addr
 * @brief Virtual address type.
 */
class [[nodiscard]] virt_addr : public address_base<virt_addr, virt_tag> {
public:
  using value_type = address_base<virt_addr, virt_tag>::value_type;

  /** @brief Construct a zero address. */
  constexpr virt_addr() noexcept = default;

  /** @brief Construct from raw integer value. */
  explicit constexpr virt_addr(value_type a) noexcept
      : address_base<virt_addr, virt_tag>(a) {}

  /** @brief Construct from an object pointer (non-const). */
  explicit virt_addr(void const *ptr) noexcept
      : address_base<virt_addr, virt_tag>(reinterpret_cast<value_type>(ptr)) {}

  /** @brief Convert to an untyped pointer. */
  explicit constexpr operator void *() noexcept { return ptr<void>(); }

  /** @brief Convert to an untyped pointer (const-qualified). */
  explicit constexpr operator const void *() const noexcept {
    return ptr<const void>();
  }

  /**
   * @brief View this virtual address as a typed pointer.
   *
   * Forms a `T*` by reinterpreting the underlying address value.
   *
   * @tparam T Pointer type.
   * @return A pointer to `T` at this virtual address.
   */
  template <typename T> [[nodiscard]] constexpr T *ptr() noexcept {
    return reinterpret_cast<T *>(addr);
  }

  /**
   * @brief View this virtual address as a typed pointer (const-qualified).
   *
   * Forms a `const T*` by reinterpreting the underlying address value. This
   * overload is selected when the `virt_addr` object itself is `const`.
   *
   * @tparam T Pointer type.
   * @return A const pointer to `T` at this virtual address.
   */
  template <typename T> [[nodiscard]] constexpr const T *ptr() const noexcept {
    return reinterpret_cast<const T *>(addr);
  }

  // See `ptr()` docs.
  template <typename T> [[nodiscard]] constexpr T &ref() noexcept {
    return *ptr<T>();
  }

  // See `ptr()` docs.
  template <typename T> [[nodiscard]] constexpr const T &ref() const noexcept {
    return *ptr<T>();
  }
};

/**
 * @class ipa_addr
 * @brief Intermediate address type.
 */
class [[nodiscard]] ipa_addr : public address_base<ipa_addr, ipa_tag> {
public:
  using value_type = address_base<ipa_addr, ipa_tag>::value_type;

  /** @brief Construct a zero address. */
  constexpr ipa_addr() noexcept = default;

  /** @brief Construct from raw integer value. */
  explicit constexpr ipa_addr(value_type a) noexcept
      : address_base<ipa_addr, ipa_tag>(a) {}
};

///@}

/* Minimal address iterator (range-for). */

/**
 * @class address_stride_iterator
 * @brief Minimal forward iterator that advances an address by a fixed stride.
 *
 * This is intentionally small and designed primarily for range-for loops.
 * It yields addresses (e.g. `virt_addr`) **by value** (the wrapper is tiny).
 *
 * @tparam Addr Address type (e.g. `virt_addr`).
 */
template <typename Addr> class address_iterator {
public:
  using addr_t = Addr;

  address_iterator() = delete;

  /**
   * @brief Construct an iterator at address @p a with stride @p s bytes.
   * @param a Current address.
   * @param s Stride in bytes.
   * @pre @p s must be non-zero.
   */
  constexpr address_iterator(addr_t a, std::size_t s) noexcept
      : cur{a}, step{s} {}

  constexpr addr_t operator*() const noexcept { return cur; }

  constexpr address_iterator &operator++() noexcept {
    cur += static_cast<std::uintptr_t>(step);
    return *this;
  }

  friend constexpr bool operator==(const address_iterator &it,
                                   const address_iterator &end) noexcept {
    // So loop stops safely, even if `last - start` in not multiple of `step`.
    return it.cur >= end.cur;
  }

private:
  addr_t cur;       // current address in the iterator.
  std::size_t step; // Step to increment current address.
};

/**
 * @class address_range
 * @brief Half-open strided range of addresses: `[first, last)`.
 *
 * Use this to iterate addresses by a fixed step (e.g. page/granule size).
 *
 * @tparam Addr Address type (e.g. `virt_addr`).
 *
 * @par Example
 * @code
 * using xino::virt_addr;
 *
 * const std::size_t g = xino::mm::paging::granule_size();
 *
 * virt_addr va0{0xFFFF'0000'0000'0000UL};
 * virt_addr va1{0xFFFF'0000'0001'0000UL};
 *
 * // Iterate each granule-aligned VA in [va0, va1).
 * for (virt_addr va : xino::mm::address_range<virt_addr>(va0, va1, g)) {
 *   // do something ...
 * }
 * @endcode
 */
template <typename Addr> class address_range {
public:
  using addr_t = Addr;
  using iterator = address_iterator<Addr>;

  address_range() = delete;

  /**
   * @brief Construct a range `[first, last)` with stride @p s bytes.
   *
   * @param a Range start (included).
   * @param b Range end (excluded).
   * @param s Stride in bytes.
   * @pre @p s must be non-zero.
   */
  constexpr address_range(addr_t a, addr_t b, std::size_t s = 1) noexcept
      : first{a}, last{b}, step{s} {}

  // Begin iterator.
  [[nodiscard]] constexpr iterator begin() const noexcept {
    return iterator{first, step};
  }

  // End iterator.
  [[nodiscard]] constexpr iterator end() const noexcept {
    return iterator{last, step};
  }

private:
  addr_t first;     // First address in the range.
  addr_t last;      // Last address in the range.
  std::size_t step; // Step to iterate from start to last.
};

/* ADDRESS RANGE ALIASES. */

/* `phys_addr_range(start, end, stride)`. */
using phys_addr_range = address_range<phys_addr>;
/* `bus_addr_range(start, end, stride)`. */
using bus_addr_range = address_range<bus_addr>;
/* `virt_addr_range(start, end, stride)`. */
using virt_addr_range = address_range<virt_addr>;
/* `ipa_addr_range(start, end, stride)`. */
using ipa_addr_range = address_range<ipa_addr>;

/* PROTECTION FLAGS. */

/**
 * @class prot
 * @brief Bitmask of abstract mapping protections/attributes.
 *
 * The page-table builder converts `xino::mm::prot` into architecture-
 * specific descriptor bits (e.g., AArch64 PTE fields).
 */
class prot {
public:
  /** @brief Underlying bitmask type. */
  using mask_t = std::uint16_t;

  /** @name Bit flags. */
  ///@{
  static constexpr mask_t NONE{0x0};
  static constexpr mask_t READ{0x1};         /**< Readable. */
  static constexpr mask_t WRITE{0x2};        /**< Writable. */
  static constexpr mask_t EXECUTE{0x4};      /**< Executable. */
  static constexpr mask_t KERNEL{0x8};       /**< Kernel page.*/
  static constexpr mask_t DEVICE{0x10};      /**< Device. */
  static constexpr mask_t SHARED{0x20};      /**< Inner-Sharable page. */
  static constexpr mask_t RW{READ | WRITE};  /**< Readable and writable. */
  static constexpr mask_t RWE{RW | EXECUTE}; /**< RW, and executable. */
  static constexpr mask_t ALL_BITS{RWE | KERNEL | DEVICE | SHARED};
  // Combined flags.
  static constexpr mask_t KERNEL_RW{RW | KERNEL | SHARED};
  static constexpr mask_t KERNEL_RWX{RWE | KERNEL | SHARED};
  ///@}

  /** @brief Construct with no flags set (i.e. NONE). */
  constexpr prot() noexcept : flags{NONE} {};

  /** @brief Construct with flags set (mask out unsupported flags). */
  constexpr prot(mask_t f) noexcept
      : flags{static_cast<mask_t>(f & ALL_BITS)} {}

  [[nodiscard]] explicit constexpr operator bool() const noexcept {
    return flags != NONE;
  }

  /** @name Bitwise ops (closed over prot). */
  ///@{
  // Bitwise OR
  [[nodiscard]] constexpr prot operator|(prot other) const noexcept {
    return prot{static_cast<mask_t>(flags | other.flags)};
  }

  // Bitwise AND
  [[nodiscard]] constexpr prot operator&(prot other) const noexcept {
    return prot{static_cast<mask_t>(flags & other.flags)};
  }

  // Bitwise NOT
  [[nodiscard]] constexpr prot operator~() const noexcept {
    return prot{static_cast<mask_t>((~flags) & ALL_BITS)};
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
  mask_t flags;
};

} // namespace xino::mm

#endif // __MM_HPP__
