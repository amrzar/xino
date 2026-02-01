

#include <new>
#include <percpu.hpp>
#include <string.h>

namespace xino::percpu {

static xino::mm::virt_addr base; // CPU0 area base
static std::size_t unit;         // bytes per CPU
static std::size_t nr_cpus;

static xino::mm::virt_addr cpu_base(unsigned cpu_idx) {
  return base + (unit * cpu_idx);
}

/** @brief Size of template `[__percpu_aligned_start, __percpu_end)`. */
static std::size_t percpu_size() noexcept {
  return static_cast<std::size_t>(__percpu_end - __percpu_aligned_start);
}

/**
 * @brief Initialize per-CPU addressing during early boot.
 *
 * This function enables the per-CPU accessors (e.g. `this_cpu()` /
 * `this_cpu_addr()`) before the SMP per-CPU area is allocated.
 *
 * It programs the EL2 per-CPU base register (TPIDR_EL2) to point at the
 * linker-provided per-CPU template start, `__percpu_aligned_start`.
 * In this bootstrap phase there is effectively a single per-CPU instance
 * shared by the boot CPU, backed directly by the in-image template.
 *
 * @note This must be called on the boot CPU before any `this_cpu()` access.
 * @note After `percpu_init()` allocates and replicates the per-CPU template,
 *       CPU0 is switched to its final per-CPU area and this bootstrap base
 *       is no longer used.
 */
void percpu_bootstrap_init() noexcept {
  xino::cpu::tpidr_el2::write(
      reinterpret_cast<xino::cpu::tpidr_el2::reg_type>(__percpu_aligned_start));
}

/**
 * @brief Install the per-CPU base for the calling CPU after it comes online.
 *
 * Programs TPIDR_EL2 to the base address of the calling CPU's per-CPU area
 * within the replicated per-CPU area allocated by `percpu_init()`.
 *
 * After this is called on a given CPU, all `this_cpu()` / `this_cpu_addr()`
 * translations on that CPU resolve to that CPU's own per-CPU instance
 * (i.e. `cpu_base(cpu_idx) + offset(sym)`).
 *
 * @param cpu_idx Logical CPU index in the range `[0, nr_cpus)`.
 */
void percpu_cpu_online(unsigned cpu_idx) noexcept {
  xino::cpu::tpidr_el2::write(
      static_cast<xino::cpu::tpidr_el2::reg_type>(cpu_base(cpu_idx)));
}

xino::error_t percpu_init(unsigned ncpu) noexcept {
  // Check if percpu area is empty.
  // `unit` is cache-line aligned (see linker.ldspp).
  unit = percpu_size();
  if (unit == 0)
    return xino::error_nr::ok;

  nr_cpus = ncpu;
  if (nr_cpus == 0)
    return xino::error_nr::invalid;

  // Check overflow.
  const std::size_t bytes = unit * nr_cpus;
  if ((bytes / unit) != nr_cpus)
    return xino::error_nr::overflow;

  // Allocate memory for all available CPUs.
  base = xino::mm::virt_addr{new (std::align_val_t{UKERNEL_CACHE_LINE},
                                  std::nothrow) unsigned char[bytes]};
  if (base == xino::mm::virt_addr{})
    return xino::error_nr::nomem;

  for (unsigned cpu = 0; cpu < nr_cpus; cpu++) {
    xino::mm::virt_addr a{cpu_base(cpu)};
    // Copy the template to the cpu area.
    memcpy(a.ptr<void>(), __percpu_aligned_start, unit);
  }

  // Switch from bootstrap area to the final area.
  xino::cpu::tpidr_el2::write(
      static_cast<xino::cpu::tpidr_el2::reg_type>(cpu_base(0)));

  return xino::error_nr::ok;
}

} // namespace xino::percpu
