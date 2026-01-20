/**
 * @file barrier.hpp
 * @brief AArch64 barrier API.
 *
 * This header provides a small set of barrier *families* (Linux-style) plus
 * template-form `dmb<>()` and `dsb<>()` helpers to select the barrier domain.
 *
 * Families provided (`xino::barrier::`):
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

/** @brief Barrier option selector for DMB and DSB. */
enum class opt {
  sy,    /**< Full system. */
  st,    /**< Stores only. */
  ld,    /**< Loads only. */
  ish,   /**< Inner-shareable. */
  ishst, /**< Inner-shareable stores. */
  ishld, /**< Inner-shareable loads. */
  osh,   /**< Outer-shareable. */
  oshst, /**< Outer-shareable stores. */
  oshld, /**< Outer-shareable loads. */
};

/**
 * @brief Data Memory Barrier (DMB).
 *
 * Emits `dmb <O>` with a `"memory"` clobber. Use this for ordering of memory
 * accesses (as observed by other observers in the selected domain), without
 * the stronger completion semantics of DSB.
 *
 * @tparam O Barrier option.
 */
template <opt O> [[gnu::always_inline]] inline void dmb() noexcept {
#define DMB(opt) __asm__ __volatile__("dmb " #opt : : : "memory")
  switch (O) {
  case opt::sy:
    DMB(sy);
    break;
  case opt::st:
    DMB(st);
    break;
  case opt::ld:
    DMB(ld);
    break;
  case opt::ish:
    DMB(ish);
    break;
  case opt::ishst:
    DMB(ishst);
    break;
  case opt::ishld:
    DMB(ishld);
    break;
  case opt::osh:
    DMB(osh);
    break;
  case opt::oshst:
    DMB(oshst);
    break;
  case opt::oshld:
    DMB(oshld);
    break;
  }
#undef DMB
}

/**
 * @brief Data Synchronization Barrier (DSB).
 *
 * Emits `dsb <O>` with a `"memory"` clobber. DSB is stronger than DMB; it
 * provides completion semantics in addition to ordering.
 *
 * @tparam O Barrier option.
 */
template <opt O> [[gnu::always_inline]] inline void dsb() noexcept {
#define DSB(opt) __asm__ __volatile__("dsb " #opt : : : "memory")
  switch (O) {
  case opt::sy:
    DSB(sy);
    break;
  case opt::st:
    DSB(st);
    break;
  case opt::ld:
    DSB(ld);
    break;
  case opt::ish:
    DSB(ish);
    break;
  case opt::ishst:
    DSB(ishst);
    break;
  case opt::ishld:
    DSB(ishld);
    break;
  case opt::osh:
    DSB(osh);
    break;
  case opt::oshst:
    DSB(oshst);
    break;
  case opt::oshld:
    DSB(oshld);
    break;
  }
#undef DSB
}

/** @name Strong system barriers. */
///@{
[[gnu::always_inline]] inline void mb() noexcept { dsb<opt::sy>(); }
[[gnu::always_inline]] inline void rmb() noexcept { dsb<opt::ld>(); }
[[gnu::always_inline]] inline void wmb() noexcept { dsb<opt::st>(); }
///@}

/** @name DMA barriers. */
///@{
[[gnu::always_inline]] inline void dma_mb() noexcept { dmb<opt::osh>(); }
[[gnu::always_inline]] inline void dma_rmb() noexcept { dmb<opt::oshld>(); }
[[gnu::always_inline]] inline void dma_wmb() noexcept { dmb<opt::oshst>(); }
///@}

/** @name IO barriers (aliases for dma_*). */
///@{
[[gnu::always_inline]] inline void iomb() noexcept { dma_mb(); }
[[gnu::always_inline]] inline void iormb() noexcept { dma_rmb(); }
[[gnu::always_inline]] inline void iowmb() noexcept { dma_wmb(); }
///@}

/** @name SMP barriers. */
///@{
#ifdef UKERNEL_SMP
[[gnu::always_inline]] inline void smp_mb() noexcept { dmb<opt::ish>(); }
[[gnu::always_inline]] inline void smp_rmb() noexcept { dmb<opt::ishld>(); }
[[gnu::always_inline]] inline void smp_wmb() noexcept { dmb<opt::ishst>(); }
#else
[[gnu::always_inline]] inline void smp_mb() noexcept { barrier(); }
[[gnu::always_inline]] inline void smp_rmb() noexcept { barrier(); }
[[gnu::always_inline]] inline void smp_wmb() noexcept { barrier(); }
#endif
///@}

/**
 * @brief Instruction Synchronization Barrier (ISB).
 *
 * Ensures that subsequent instructions are fetched and executed only after
 * effects of prior system register writes or context changes are visible.
 */
[[gnu::always_inline]] inline void isb() noexcept {
  __asm__ __volatile__("isb" ::: "memory");
}

} // namespace xino::barrier

#endif // __BARRIER_HPP__
