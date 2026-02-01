/**
 * @file io.hpp
 * @brief Minimal AArch64 MMIO accessors (raw / relaxed / ordered).
 *
 * **Raw access**: `xino::io::raw_read()`, `xino::io::raw_write()`
 *   - Exactly one volatile load/store.
 *   - No compiler barrier, no architectural barrier.
 *
 * **Relaxed access**: `xino::io::read_relaxed()`, `xino::io::write_relaxed()`
 *   - One volatile access plus compiler barrier(s)
 *     (via `xino::barrier::barrier()`).
 *   - No architectural barrier.
 *
 * **Ordered access**: `xino::io::read_ordered()`, `xino::io::write_ordered()`
 *   - Reads: barrier after the load (`xino::barrier::iormb()`).
 *   - Writes: barrier before the store (`xino::barrier::iowmb()`).
 *
 * - **Sized helpers**: `xino::io::read{b,w,l,q}`, `xino::io::write{b,w,l,q}`,
 *   `xino::io::read_relaxed{b,w,l,q}`, and `xino::io::write_relaxed{b,w,l,q}`
 *
 * @note This assumes MMIO mappings are Device-nGnRE/nGnRnE.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __IO_HPP__
#define __IO_HPP__

#include <barrier.hpp> // for barrier(), iormb(), iowmb()
#include <mm.hpp>      // for virt_addr
#include <type_traits> // for std::is_integral_v, std::is_same_v

namespace xino::io {
// mmio_addr_t is virtual address typically after mapping tough appropriate API.
using mmio_addr_t = xino::mm::virt_addr;

/**
 * @concept mmio_ok_v
 * @brief Constrains the supported MMIO access sizes and types.
 *
 * The API supports only integral types (excluding bool) of sizes:
 * 1, 2, 4, or 8 bytes.
 */
template <typename T>
concept mmio_ok_v =
    std::is_integral_v<T> && !std::is_same_v<T, bool> &&
    (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

/**
 * @brief Convert an MMIO address to a typed volatile pointer.
 *
 * @tparam T Access type (8/16/32/64-bit integer).
 * @param addr MMIO virtual address.
 * @return `volatile T *` pointing at @p addr.
 *
 * @warning This performs no mapping, validation, or alignment checks.
 *          The caller must ensure the address is valid and properly aligned
 *          for type @p T, and that the mapping attributes are correct.
 */
template <mmio_ok_v T>
[[nodiscard, gnu::always_inline]] inline volatile T *
ptr(mmio_addr_t addr) noexcept {
  const auto a = static_cast<mmio_addr_t::value_type>(addr);
  return reinterpret_cast<volatile T *>(a);
}

template <mmio_ok_v T>
[[nodiscard, gnu::always_inline]] inline T raw_read(mmio_addr_t addr) noexcept {
  return *ptr<T>(addr);
}

template <mmio_ok_v T>
[[gnu::always_inline]] inline void raw_write(mmio_addr_t addr,
                                             T value) noexcept {
  *ptr<T>(addr) = value;
}

/**
 * @brief Relaxed MMIO load with compiler ordering only.
 *
 * Performs one volatile MMIO load and surrounds it with a compiler barrier
 * `xino::barrier::barrier()` to prevent the compiler from hoisting/sinking
 * unrelated memory operations across the access.
 *
 * @tparam T Access type (8/16/32/64-bit integer).
 * @param addr MMIO virtual address.
 * @return Loaded value.
 */
template <mmio_ok_v T>
[[nodiscard, gnu::always_inline]] inline T
read_relaxed(mmio_addr_t addr) noexcept {
  xino::barrier::barrier();
  T v = raw_read<T>(addr);
  xino::barrier::barrier();
  return v;
}

/**
 * @brief Relaxed MMIO store with compiler ordering only.
 *
 * Performs one volatile MMIO store and surrounds it with a compiler barrier
 * `xino::barrier::barrier()` to prevent the compiler from hoisting/sinking
 * unrelated memory operations across the access.
 *
 * @tparam T Access type (8/16/32/64-bit integer).
 * @param addr MMIO virtual address.
 * @param value Value to store.
 */
template <mmio_ok_v T>
[[gnu::always_inline]] inline void write_relaxed(mmio_addr_t addr,
                                                 T value) noexcept {
  xino::barrier::barrier();
  raw_write<T>(addr, value);
  xino::barrier::barrier();
}

/**
 * @brief Ordered MMIO load.
 *
 * Executes a relaxed load (compiler barriers only), then an I/O read barrier
 * `xino::barrier::iormb()`.
 *
 * This prevents later memory operations from being reordered before the MMIO
 * read, which is the common "read barrier after read" convention.
 *
 * @tparam T Access type (8/16/32/64-bit integer).
 * @param addr MMIO virtual address.
 * @return Loaded value.
 */
template <mmio_ok_v T>
[[nodiscard, gnu::always_inline]] inline T
read_ordered(mmio_addr_t addr) noexcept {
  T v = read_relaxed<T>(addr);
  xino::barrier::iormb();
  return v;
}

/**
 * @brief Ordered MMIO store.
 *
 * Executes an I/O write barrier `xino::barrier::iowmb()` before the store,
 * then a relaxed write (compiler barriers only).
 *
 * This prevents earlier memory operations from being reordered after the MMIO
 * write, which is the common "write barrier before write" convention.
 *
 * @tparam T Access type (8/16/32/64-bit integer).
 * @param addr MMIO virtual address.
 * @param value Value to store.
 */
template <mmio_ok_v T>
[[gnu::always_inline]] inline void write_ordered(mmio_addr_t addr,
                                                 T value) noexcept {
  xino::barrier::iowmb(); // order: barrier then write
  write_relaxed<T>(addr, value);
}

/** @name Relaxed MMIO reads. */
///@{
// Ordered 8-bit MMIO read.
[[nodiscard, gnu::always_inline]] inline std::uint8_t
readb_relaxed(mmio_addr_t addr) noexcept {
  return read_relaxed<std::uint8_t>(addr);
}

// Ordered 16-bit MMIO read.
[[nodiscard, gnu::always_inline]] inline std::uint16_t
readw_relaxed(mmio_addr_t addr) noexcept {
  return read_relaxed<std::uint16_t>(addr);
}

// Ordered 32-bit MMIO read.
[[nodiscard, gnu::always_inline]] inline std::uint32_t
readl_relaxed(mmio_addr_t addr) noexcept {
  return read_relaxed<std::uint32_t>(addr);
}

// Ordered 64-bit MMIO read.
[[nodiscard, gnu::always_inline]] inline std::uint64_t
readq_relaxed(mmio_addr_t addr) noexcept {
  return read_relaxed<std::uint64_t>(addr);
}
///@}

/** @name Relaxed MMIO writes. */
///@{
// Ordered 8-bit MMIO write.
[[gnu::always_inline]] inline void writeb_relaxed(std::uint8_t value,
                                                  mmio_addr_t addr) noexcept {
  write_relaxed<std::uint8_t>(addr, value);
}

// Ordered 16-bit MMIO write.
[[gnu::always_inline]] inline void writew_relaxed(std::uint16_t value,
                                                  mmio_addr_t addr) noexcept {
  write_relaxed<std::uint16_t>(addr, value);
}

// Ordered 32-bit MMIO write.
[[gnu::always_inline]] inline void writel_relaxed(std::uint32_t value,
                                                  mmio_addr_t addr) noexcept {
  write_relaxed<std::uint32_t>(addr, value);
}

// Ordered 64-bit MMIO write.
[[gnu::always_inline]] inline void writeq_relaxed(std::uint64_t value,
                                                  mmio_addr_t addr) noexcept {
  write_relaxed<std::uint64_t>(addr, value);
}
///@}

/** @name Ordered MMIO reads. */
///@{
// Ordered 8-bit MMIO read.
[[nodiscard, gnu::always_inline]] inline std::uint8_t
readb(mmio_addr_t addr) noexcept {
  return read_ordered<std::uint8_t>(addr);
}

// Ordered 16-bit MMIO read.
[[nodiscard, gnu::always_inline]] inline std::uint16_t
readw(mmio_addr_t addr) noexcept {
  return read_ordered<std::uint16_t>(addr);
}

// Ordered 32-bit MMIO read.
[[nodiscard, gnu::always_inline]] inline std::uint32_t
readl(mmio_addr_t addr) noexcept {
  return read_ordered<std::uint32_t>(addr);
}

// Ordered 64-bit MMIO read.
[[nodiscard, gnu::always_inline]] inline std::uint64_t
readq(mmio_addr_t addr) noexcept {
  return read_ordered<std::uint64_t>(addr);
}
///@}

/** @name Ordered MMIO writes. */
///@{
// Ordered 8-bit MMIO write.
[[gnu::always_inline]] inline void writeb(std::uint8_t value,
                                          mmio_addr_t addr) noexcept {
  write_ordered<std::uint8_t>(addr, value);
}

// Ordered 16-bit MMIO write.
[[gnu::always_inline]] inline void writew(std::uint16_t value,
                                          mmio_addr_t addr) noexcept {
  write_ordered<std::uint16_t>(addr, value);
}

// Ordered 32-bit MMIO write.
[[gnu::always_inline]] inline void writel(std::uint32_t value,
                                          mmio_addr_t addr) noexcept {
  write_ordered<std::uint32_t>(addr, value);
}

// Ordered 64-bit MMIO write.
[[gnu::always_inline]] inline void writeq(std::uint64_t value,
                                          mmio_addr_t addr) noexcept {
  write_ordered<std::uint64_t>(addr, value);
}
///@}

} // namespace xino::io

#endif // __IO_HPP__
