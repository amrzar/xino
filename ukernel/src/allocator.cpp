
#include <allocator.hpp>
#include <config.h>
#include <cpu.hpp>

namespace xino::allocator {

extern "C" {
extern char __boot_heap_start[];
extern char __boot_heap_end[];
}

/**
 * @brief Global buddy allocator used for early-boot allocations.
 *
 * This allocator services *physical* page allocations needed during the boot
 * stages (e.g., page-table pages, temporary boot buffers) before the main
 * allocator(s) are initialized.
 *
 * The allocator is configured and activated by @ref ukernel_boot_alloc_init(),
 * which binds it to the linker-reserved boot heap region
 * `[__boot_heap_start, __boot_heap_end)`.
 */
constinit boot_allocator_t boot_allocator{};

/**
 * @brief Initialize the uKernel boot-time allocator while the MMU is off.
 *
 * This routine brings up the allocator used during early boot to allocate
 * *physical* pages before the main memory subsystem is fully initialized.
 *
 * The allocator manages a dedicated boot-heap region reserved in the linker
 * script (e.g. a `.boot_heap (NOLOAD)` section). With the MMU disabled,
 * the runtime address of `__boot_heap_start`/`__boot_heap_end` equals the
 * physical load address of that region (VA == PA).
 *
 * The allocator is initialized to manage the physical interval:
 *   `[__boot_heap_start, __boot_heap_end)`.
 */
extern "C" void ukernel_boot_alloc_init() noexcept {
  using av_t = xino::mm::phys_addr::value_type;

  // Boot heap `[start, end]`.
  const xino::mm::phys_addr start{reinterpret_cast<av_t>(__boot_heap_start)};
  const xino::mm::phys_addr end{reinterpret_cast<av_t>(__boot_heap_end)};
  const std::size_t size = static_cast<std::size_t>(end - start);

  if (boot_allocator.init(start, size) != xino::error_nr::ok)
    xino::cpu::panic();
}

} // namespace xino::allocator
