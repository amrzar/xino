/**
 * @file sync.hpp
 * @brief Low-level synchronization and interrupt masking primitives.
 *
 * This header provides:
 *   - Helpers to mask/unmask IRQ/FIQ using the DAIF register.
 *   - A simple spin lock (`xino::sync::spin_lock`) that uses CAS for
 *     acquisition and event primitives to reduce contention while waiting.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __SYNC_HPP__
#define __SYNC_HPP__

#include <cpu.hpp>
#include <cstdint>
#include <regs.hpp>

namespace xino::sync {

using irq_flags_t = xino::cpu::daif::reg_type;

/**
 * @brief Save DAIF and mask IRQs (I-bit and F-bit).
 *
 * @return Previous DAIF value to be restored with irq_restore().
 */
[[nodiscard, gnu::always_inline]] inline irq_flags_t irq_save() noexcept {
  irq_flags_t f = xino::cpu::daif::read();
  xino::cpu::daifset::write<xino::cpu::daifset::flags::fiq |   // Mask IRQ.
                            xino::cpu::daifset::flags::irq>(); // Mask FRQ.
  return f;
}

/** @brief Restore DAIF exactly as saved. */
[[gnu::always_inline]] inline void irq_restore(irq_flags_t f) noexcept {
  xino::cpu::daif::write(f);
}

/**
 * @class spin_lock
 * @brief Spin lock with optional irqsave and WFE/SEV wait.
 *
 * - `state == 0` means unlocked, `state == 1` means locked.
 * - Acquire is done via CAS (0 -> 1) with `__ATOMIC_ACQUIRE`.
 * - Unlock is a store (0) with `__ATOMIC_RELEASE`, followed by `SEV`.
 */
class spin_lock {
public:
  constexpr spin_lock() noexcept = default;

  // No copy.
  spin_lock(const spin_lock &) = delete;

  // No assignment.
  spin_lock &operator=(const spin_lock &) = delete;

  /**
   * @brief Acquire the lock (spins until successful).
   *
   * Why `SEVL` is required
   *
   *  `WFE` is not a condition-variable; it waits on a **single-bit event
   *  latch**. That latch can be set by many things (e.g., `SEV`, interrupts)
   *  and it is **consumed** by `WFE` (a `WFE` that returns immediately clears
   *  the bit).
   *
   *  Without `SEVL`, a waiter can end up doing a blocking `WFE` after the
   *  unlock's `SEV` has already happened (or has been consumed by a previous
   *  `WFE`), while it still observes the lock held (due to re-acquire by
   *  another core or transiently stale observation). If interrupts are masked,
   *  there may be **no other event source** to wake it promptly, so
   *  it can "sleep too long".
   *
   *  Placing `SEVL` immediately before the `WFE` loop **primes the local event
   *  latch** so that the first `WFE` cannot block (it returns immediately and
   *  clears the bit). That guarantees at least one extra iteration of:
   *
   *    check(lock) -> WFE (non-blocking) -> re-check(lock)
   *
   *  before the CPU is allowed to truly sleep. This closes the classic "missed
   *  wake" style hazard in WFE-based wait loops, especially when IRQs are
   *  masked.
   */
  void lock() noexcept {
    if (try_lock())
      return;

    for (;;) {
      xino::cpu::sevl();

      while (__atomic_load_n(&state, __ATOMIC_RELAXED) != 0)
        xino::cpu::wfe();

      if (try_lock())
        return;
    }
  }

  /**
   * @brief Try to acquire the lock once.
   *
   * @return true on success, false if already held.
   */
  [[nodiscard]] bool try_lock() noexcept {
    std::uint32_t expected = 0;
    return __atomic_compare_exchange_n(&state, &expected, 1, false,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
  }

  /** @brief Release the lock. */
  void unlock() noexcept {
    __atomic_store_n(&state, 0, __ATOMIC_RELEASE);
    xino::cpu::sev();
  }

  /**
   * @brief Acquire the lock with IRQs and FIQs masked.
   *
   * @return Previous DAIF value to be restored with unlock_irqrestore().
   *
   * @par Example
   * @code
   * auto flags = lock.lock_irqsave();
   * // critical section (IRQs/FIQs masked on this core)
   * lock.unlock_irqrestore(flags);
   * @endcode
   */
  [[nodiscard]] irq_flags_t lock_irqsave() noexcept {
    irq_flags_t f = irq_save();
    lock();
    return f;
  }

  /** @brief Release the lock and restore IRQ flags from lock_irqsave(). */
  void unlock_irqrestore(irq_flags_t f) noexcept {
    unlock();
    irq_restore(f);
  }

private:
  alignas(4) std::uint32_t state{};
};

} // namespace xino::sync

#endif // __SYNC_HPP__
