/**
 * @file percpu.hpp
 * @brief Minimal per-CPU framework.
 *
 * xino implements per-CPU storage using a linker-defined *template* image that
 * is replicated once per CPU. TPIDR_EL2 holds the base of the calling CPU's
 * per-CPU slice.
 *
 * A per-CPU symbol is translated as:
 * @code
 * this_cpu_ptr = TPIDR_EL2 + (symbol_addr - __percpu_aligned_start)
 * @endcode
 *
 * ## Declaring per-CPU variables
 *
 * Standard C++ has no notion of custom linker sections, therefore per-CPU
 * variables must still be placed into the linker-collected input sections using
 * compiler extensions (e.g. `[[gnu::section(...)]]`).
 *
 * This header provides two aggregate wrappers:
 *  - `xino::percpu::var<T>` : normal per-CPU variable
 *  - `xino::percpu::hot<T>` : cache-line aligned per-CPU variable
 *
 * These wrappers are intentionally **aggregates** (no constructors, no member
 * initializers). This keeps requirements minimal and matches the runtime model
 * (memcpy-based replication).
 *
 * @note Payload type `T` must be trivially copyable and trivially destructible,
 *       because per-CPU replication is performed via `memcpy`, not by running
 *       constructors per CPU.
 *
 * @par Example (normal per-CPU)
 * @code
 * // In .cpp file:
 * [[gnu::used, gnu::section(".percpu")]]
 * constinit xino::percpu::var<std::uint64_t> vmexit_count{0};
 *
 * // In headers:
 * extern xino::percpu::var<std::uint64_t> vmexit_count;
 *
 * void on_vmexit() {
 *   xino::percpu::this_cpu(vmexit_count)++;
 * }
 * @endcode
 *
 * @par Example (hot per-CPU, cache-line aligned)
 * @code
 * [[gnu::used, gnu::section(".percpu_aligned")]]
 * constinit xino::percpu::hot<std::uint64_t> fast_counter{0};
 *
 * void tick() {
 *   xino::percpu::this_cpu(fast_counter)++;
 * }
 * @endcode
 *
 * ## Boot flow
 *
 * 1. Early boot (single CPU, before per-CPU replication):
 *    call `percpu_bootstrap_init()` to set TPIDR_EL2 to the template base.
 * 2. Once the number of CPUs is known and allocation is available:
 *    call `percpu_init(ncpu)` to allocate and replicate the template
 *    for all CPUs, then switch CPU0 to its final area.
 * 3. During secondary CPU bring-up:
 *    call `percpu_cpu_online(cpu_idx)` on each CPU to set its TPIDR_EL2 base.
 *
 * ## Design note
 *
 * Some C++ kernels build per-CPU state by allocating a per-CPU area and then
 * running per-CPU constructors during CPU bring-up.
 *
 * In xino we deliberately keep this simpler: the per-CPU template image in the
 * linker section is replicated for each CPU via `memcpy`. We enforce this model
 * with `static_assert` checks (trivially copyable/destructible) on per-CPU
 * payload types. If a user needs non-trivial per-CPU objects, they can
 * explicitly construct them in-place in their per-CPU storage during CPU
 * initialization.
 */

#ifndef __PERCPU_HPP__
#define __PERCPU_HPP__

#include <config.h> // for UKERNEL_CACHE_LINE
#include <cpu.hpp>  // for tpidr_el2 register accessor
#include <cstdint>
#include <errno.hpp>
#include <mm.hpp> // for virt_addr
#include <type_traits> // for std::is_trivially_copyable, std::is_trivially_destructible

extern "C" {
/* See linker.ldspp. */
extern char __percpu_aligned_start[];
extern char __percpu_start[];
extern char __percpu_end[];
}

namespace xino::percpu {

/**
 * @brief Normal per-CPU wrapper.
 *
 * @tparam T Payload type.
 *
 * @note `T` must be trivially copyable and trivially destructible, because the
 *       per-CPU template is replicated via `memcpy`.
 */
template <typename T> struct var {
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(std::is_trivially_destructible_v<T>);
  T value;
};

/**
 * @brief Hot per-CPU wrapper (cache-line aligned).
 *
 * Use for frequently accessed variables to reduce false sharing and improve
 * cache locality.
 *
 * @tparam T Payload type.
 *
 * @note `T` must be trivially copyable and trivially destructible, because the
 *       per-CPU template is replicated via `memcpy`.
 */
template <typename T> struct hot {
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(std::is_trivially_destructible_v<T>);
  alignas(UKERNEL_CACHE_LINE) T value;
};

/** @brief Translate a per-cpu symbol address to this CPU's copy. */
[[nodiscard]] inline xino::mm::virt_addr
this_cpu_addr(xino::mm::virt_addr sym) noexcept {
  const xino::cpu::tpidr_el2::reg_type tdidr{xino::cpu::tpidr_el2::read()};

  return tdidr + sym - reinterpret_cast<std::uintptr_t>(__percpu_aligned_start);
}

template <typename T> [[nodiscard]] inline T &this_cpu(var<T> &sym) noexcept {
  return this_cpu_addr(xino::mm::virt_addr{&sym}).ref<var<T>>().value;
}

template <typename T>
[[nodiscard]] inline const T &this_cpu(const var<T> &sym) noexcept {
  return this_cpu_addr(xino::mm::virt_addr{&sym}).ref<const var<T>>().value;
}

template <typename T> [[nodiscard]] inline T &this_cpu(hot<T> &sym) noexcept {
  return this_cpu_addr(xino::mm::virt_addr{&sym}).ref<hot<T>>().value;
}

template <typename T>
[[nodiscard]] inline const T &this_cpu(const hot<T> &sym) noexcept {
  return this_cpu_addr(xino::mm::virt_addr{&sym}).ref<const hot<T>>().value;
}

/* Initialization APIs. */

void percpu_bootstrap_init() noexcept;

void percpu_cpu_online(unsigned cpu_idx) noexcept;

xino::error_t percpu_init(unsigned ncpu) noexcept;

} // namespace xino::percpu

#endif // __PERCPU_HPP__
