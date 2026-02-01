
#include <cpu.hpp>
#include <mm_va_layout.hpp>

namespace xino::mm::va_layout {

// UKERNEL_BASE should be in `[ukimage_va, ukimage_end]`.
// static_assert(ukimage_va <= ukernel_base && ukernel_base <= ukimage_end);

// UKERNEL_BASE must be aligned to the configured page granule.
static_assert((UKERNEL_BASE & (granule_size() - 1)) == 0);

extern "C" {
/* See linker.ldspp. */
extern char __image_start[];
extern char __image_end[];
}

/**
 * @brief Runtime VA base of the uKernel image mapping.
 *
 * If KASLR is available, the image may be relocated within the fixed
 * `[ukimage_va, ukimage_end]` window; this variable holds the chosen base.
 * If KASLR is not used, it may remain at the default value chosen by early
 * boot code (typically `UKERNEL_BASE`).
 *
 * The actual mapped image range is:
 *   `[ukimage_va_base, ukimage_va_base + ukimage_slot_size - 1]`.
 *
 * This variable is accessed during the boot stages, around the time the
 * uKernel applies PIE self-relocations (i.e., processing `R_AARCH64_RELATIVE`
 * entries in `.rela.dyn`) and before the MMU is on.
 *
 * `constinit` enforces **constant initialization** (static initialization
 * at compile time), guaranteeing the object is placed in `.bss`/`.data`
 * without generating any dynamic initialization code.
 *
 * Defining this as an `inline` header variable causes weak emission and
 * can lead to less predictable code generation and addressing modes across TUs.
 * A single strong definition in one TU keeps early-boot code paths stable.
 */
constinit xino::mm::virt_addr ukimage_va_base{};

/** @brief Runtime PA base of the uKernel image, see @ref ukimage_va_base.  */
constinit xino::mm::phys_addr ukimage_pa_base{};

/** @brief Size of the uKernel image, see @ref ukimage_va_base.  */
constinit std::size_t ukimage_size{};

/**
 * @brief Initialize uKernel VA-layout runtime bases while the MMU is off.
 *
 * This routine finalizes the runtime parameters of the uKernel VA layout that
 * are required for later address translation helpers (e.g. `virt_to_phys()`)
 * and for building the initial page tables.
 *
 * Safety / invariants checked:
 *  - @p va is aligned to the configured page granule.
 *  - @p va lies within the reserved image slot `[ukimage_va, ukimage_end]`.
 *  - The image size fits in the remaining area starting at @p va.
 *
 * @param va Requested runtime image VA base (for future KASLR support).
 *           Currently ignored; `UKERNEL_BASE` is used instead.
 */
extern "C" void ukernel_va_layout_init(std::uintptr_t va) noexcept {

  // With MMU off, runtime address of __image_start == physical load address.
  ukimage_pa_base = xino::mm::phys_addr{
      reinterpret_cast<xino::mm::phys_addr::value_type>(__image_start)};
  // uKernel range `[ukimage_va_base, ukimage_va_base + ukimage_size + 1]`.
  ukimage_va_base = xino::mm::virt_addr{UKERNEL_BASE};
  ukimage_size = static_cast<std::size_t>(__image_end - __image_start);

  // KASLR base must be aligned to the configured page granule.
  if (!ukimage_va_base.is_align(granule_size()))
    xino::cpu::panic();

  // // Check if `ukimage_va_base` in ukernel image slot.
  // if (ukimage_va_base < ukimage_va || ukimage_va_base > ukimage_end)
  //   xino::cpu::panic();

  // const std::ptrdiff_t avail{ukimage_end - ukimage_va_base + 1};
  // // Check there is enough space in ukernel image slot.
  // if (ukimage_size > avail)
  //   xino::cpu::panic();
}

} // namespace xino::mm::va_layout
