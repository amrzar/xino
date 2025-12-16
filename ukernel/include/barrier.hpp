/**
 * @file barrier.hpp
 * @brief AArch64 barrier API.
 *
 * Families provided:
 *   - `mb()`, `rmb()`, and `wmb`(): strong system barriers (DSB).
 *   - `smp_mb()`, `smp_rmb()`, and `smp_wmb()`: SMP barriers for normal
 *      cacheable memory (DMB ISH*).
 *   - `dma_mb()`, `dma_rmb()`, and `dma_wmb()`: Device-visible barriers
 *     (DMB OSH*).
 *   - `iomb()`, `iormb()`, and `iowmb()`: aliases for dma_* (IO ordering).
 *
 * AArch64 conventions, Linux-like intent:
 *   - OSH* is typically used for ordering relative to devices and DMA.
 *   - ISH* is typically used for inter-CPU ordering on normal memory.
 *   - DSB implies stronger ordering/completion than DMB.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __BARRIER_HPP__
#define __BARRIER_HPP__

namespace xino::barrier {

// Compiler barrier.
[[gnu::always_inline]] inline void barrier() noexcept {
  __asm__ __volatile__("" ::: "memory");
}

/** @name Strong system barriers. */
///@{
[[gnu::always_inline]] inline void mb() noexcept {
  __asm__ __volatile__("dsb sy" ::: "memory");
}

[[gnu::always_inline]] inline void rmb() noexcept {
  __asm__ __volatile__("dsb ld" ::: "memory");
}

[[gnu::always_inline]] inline void wmb() noexcept {
  __asm__ __volatile__("dsb st" ::: "memory");
}
///@}

/** @name DMA and IO barriers. */
///@{
[[gnu::always_inline]] inline void dma_mb() noexcept {
  __asm__ __volatile__("dmb osh" ::: "memory");
}

[[gnu::always_inline]] inline void dma_rmb() noexcept {
  __asm__ __volatile__("dmb oshld" ::: "memory");
}

[[gnu::always_inline]] inline void dma_wmb() noexcept {
  __asm__ __volatile__("dmb oshst" ::: "memory");
}

[[gnu::always_inline]] inline void iomb() noexcept { dma_mb(); }
[[gnu::always_inline]] inline void iormb() noexcept { dma_rmb(); }
[[gnu::always_inline]] inline void iowmb() noexcept { dma_wmb(); }
///@}

/** @name SMP barriers. */
///@{
#ifdef UKERNEL_SMP
[[gnu::always_inline]] inline void smp_mb() noexcept {
  __asm__ __volatile__("dmb ish" ::: "memory");
}

[[gnu::always_inline]] inline void smp_rmb() noexcept {
  __asm__ __volatile__("dmb ishld" ::: "memory");
}

[[gnu::always_inline]] inline void smp_wmb() noexcept {
  __asm__ __volatile__("dmb ishst" ::: "memory");
}
#else
[[gnu::always_inline]] inline void smp_mb() noexcept { barrier(); }
[[gnu::always_inline]] inline void smp_rmb() noexcept { barrier(); }
[[gnu::always_inline]] inline void smp_wmb() noexcept { barrier(); }
#endif
///@}

// Instruction sync barrier.
[[gnu::always_inline]] inline void isb() noexcept {
  __asm__ __volatile__("isb" ::: "memory");
}

} // namespace xino::barrier

#endif // __BARRIER_HPP__
