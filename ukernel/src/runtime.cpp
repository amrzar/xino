
#include <allocator.hpp> // xino::allocator::boot_allocator
#include <barrier.hpp>
#include <cpu.hpp>
#include <cstdint> // for std::uint16_t
#include <cstdlib> // for std::malloc and ste::free
#include <mm_paging.hpp>
#include <mm_va_layout.hpp>
#include <new>
#include <runtime.hpp>

void main();

namespace xino::runtime {

extern "C" {
typedef void (*ctor_t)();
/* See linker.ldspp. */
extern ctor_t __init_array_start[], __init_array_end[];
extern ctor_t __fini_array_start[], __fini_array_end[];
}

static void run_init_array() {
  for (auto p{__init_array_start}; p != __init_array_end; ++p)
    if (*p)
      (*p)();
}

static void run_fini_array() {
  for (auto p{__fini_array_start}; p != __fini_array_end; ++p)
    if (*p)
      (*p)();
}

extern "C" {
extern char __eh_frame_start[], __eh_frame_end[];
}

extern "C" void __register_frame(void *) __attribute__((weak));
extern "C" void __deregister_frame(void *) __attribute__((weak));

extern "C" {
extern char __image_start[];
extern char __rodata_start[];
extern char __data_start[];
extern char __rela_dyn_start[], __rela_dyn_end[];
}

static void register_eh_frames() {
  if (__register_frame)
    __register_frame(__eh_frame_start);
}

static void deregister_eh_frames() {
  if (__deregister_frame)
    __deregister_frame(__eh_frame_start);
}

// Temporary identity mapping.
constinit ukernel_pt_t identity_pt{};
// uKernel mapping (direct + ukimage).
constinit ukernel_pt_t ukernel_pt{};
constexpr std::uint16_t ukernel_asid{0};

/**
 * @brief Map a contiguous uKernel image segment into the ukernel VA window.
 *
 * Builds a VA/PA pair based on the relocation delta between the current image
 * layout (`__image_start`) and the requested segment range, then programs the
 * final kernel page tables with the provided protection flags.
 *
 * @param begin Inclusive start of the segment in the loaded image.
 * @param end Exclusive end of the segment in the loaded image.
 * @param prot Protection mask to apply to the mapped range.
 *
 * @retval xino::error_nr::ok Segment was empty or mapped successfully.
 * @retval Other `xino::error_nr` Mapping failed (propagated from
 *         `ukernel_pt_t::map_range`).
 */
[[nodiscard]] static xino::error_t
map_image_segment(const char *begin, const char *end, xino::mm::prot prot) {
  const auto begin_u{reinterpret_cast<std::uintptr_t>(begin)};
  const auto end_u{reinterpret_cast<std::uintptr_t>(end)};

  if (end_u <= begin_u)
    return xino::error_nr::ok;

  const auto offset{static_cast<std::size_t>(
      begin_u - reinterpret_cast<std::uintptr_t>(__image_start))};
  const auto size{static_cast<std::size_t>(end_u - begin_u)};

  ukernel_pt_t::addr_t seg_va{};
  seg_va.addr = xino::mm::va_layout::ukimage_va_base + offset;
  seg_va.asid = ukernel_asid;
  // `linker.ldspp` aligns each referenced section boundary to
  // `UKERNEL_PAGE_SIZE`, so both `offset` and `size` are granule multiples and
  // satisfy `map_range()` alignment requirements.
  return ukernel_pt.map_range(
      seg_va, xino::mm::va_layout::ukimage_pa_base + offset, size, prot);
}

/**
 * @brief Create a temporary identity mapping for the ukernel image.
 *
 * Allocates a dedicated stage-1 page table, installs RWX mappings that cover
 * `[ukimage_pa_base, ukimage_pa_base + ukimage_size)`, and programs TTBR0 so
 * the currently executing code keeps running once the MMU is turned on.
 *
 * @return `xino::error_nr::ok` on success, otherwise the status returned by
 *         `ukernel_pt_t::init()` or `map_range()`.
 */
[[nodiscard]] static xino::error_t setup_identity_mapping() {
  if (auto ret{identity_pt.init(boot_allocator)}; ret)
    return ret;

  const auto pa{static_cast<xino::mm::phys_addr::value_type>(
      xino::mm::va_layout::ukimage_pa_base)};

  ukernel_pt_t::addr_t ident_va_range{};
  ident_va_range.addr = xino::mm::virt_addr{pa};
  ident_va_range.asid = ukernel_asid;
  // Identity map the ukernel range:
  // `[ukimage_pa_base, ukimage_pa_base + ukimage_size)`.
  if (auto ret{identity_pt.map_range(ident_va_range,
                                     xino::mm::va_layout::ukimage_pa_base,
                                     xino::mm::va_layout::ukimage_size,
                                     xino::mm::prot{mm::prot::KERNEL_RWX})};
      ret) {
    // `map_range()` is not atomic.
    identity_pt.deinit();

    return ret;
  }

  xino::mm::paging::install_user_ttbr(identity_pt.root(), ukernel_asid);

  return xino::error_nr::ok;
}

/**
 * @brief Build the final uKernel mappings (direct map + ukimage window).
 *
 * Initializes the long-lived stage-1 tables, maps the full direct-map window
 * with RW permissions, applies per-section permissions for the relocated
 * ukernel image inside `[ukimage_va_base, ukimage_va_base + ukimage_size)`,
 * and finally installs the table in TTBR1.
 *
 * @return `xino::error_nr::ok` on success, otherwise the status returned by
 *         `ukernel_pt_t::init()` or `map_range()`.
 */
[[nodiscard]] static xino::error_t setup_ukernel_mapping() {
  if (auto ret{ukernel_pt.init(boot_allocator)}; ret)
    return ret;

  ukernel_pt_t::addr_t direct_va_range{};
  direct_va_range.addr = xino::mm::va_layout::page_offset;
  direct_va_range.asid = ukernel_asid;
  // Map the entire direct-map window `[page_offset, page_end]` (inclusive) to
  // `[0x0, page_end - page_offset + 1)`. For instance `[0x1000, 0x1fff]`
  // becomes `[0x0, 0x1000)` calling
  // `ukernel_pt.map_range(0x1000, 0x0, 0xfff + 1, ...)`.
  if (auto ret{ukernel_pt.map_range(direct_va_range, mm::phys_addr{0},
                                    xino::mm::va_layout::page_end -
                                        xino::mm::va_layout::page_offset + 1,
                                    xino::mm::prot{mm::prot::KERNEL_RW})};
      ret) {
    // `map_range()` is not atomic.
    ukernel_pt.deinit();

    return ret;
  }

  const struct {
    const char *begin;
    const char *end;
    xino::mm::prot prot;
  } image_segments[] = {
      // .text :
      {__image_start, __rodata_start, xino::mm::prot{mm::prot::KERNEL_RX}},
      // .rodata, .eh_frame_hdr, .eh_frame, .gcc_except_table, .init_array,
      // .fini_array, .got :
      {__rodata_start, __data_start, xino::mm::prot{mm::prot::KERNEL_R}},
      // .data, .percpu, .bss, .boot_heap :
      {__data_start, __rela_dyn_start, xino::mm::prot{mm::prot::KERNEL_RW}},
      // .rela.dyn (ignored, already consumed in `reloc.c` during boot):
      // {__rela_dyn_start, __rela_dyn_end, xino::mm::prot{mm::prot::KERNEL_R}},
  };

  for (const auto &seg : image_segments) {
    if (auto ret{map_image_segment(seg.begin, seg.end, seg.prot)}; ret) {
      // `map_range()` is not atomic.
      ukernel_pt.deinit();

      return ret;
    }
  }

  xino::mm::paging::install_kernel_ttbr(ukernel_pt.root(), ukernel_asid);

  return xino::error_nr::ok;
}

extern "C" void ukernel_entry() {
  /* uKernel has been relocated, and the boot allocator is functional. */

  /* -- Begin of Pre-C++ runtime boot -- */

  xino::mm::paging::init_paging();

  cpu::mair_el2::write(xino::mm::paging::make_mair_el2());
  cpu::tcr_el2::write(xino::mm::paging::make_tcr_el2(
      xino::cpu::cpu_feats.pa_bits, xino::mm::va_layout::va_bits));

  if (setup_identity_mapping())
    xino::cpu::panic();

  if (setup_ukernel_mapping())
    xino::cpu::panic();

  mm::va_layout::va_layout_enabled = true;

  barrier::isb();
  mm::paging::enable_mmu();
  barrier::isb();

  /* -- Begin of C++ runtime boot -- */

  register_eh_frames();
  run_init_array();
  ::main();
  run_fini_array();
  deregister_eh_frames();
}

} // namespace xino::runtime

/* NEW and DELETE operators; Keep them here. */

void *operator new(std::size_t sz) {
  void *ptr = std::malloc(sz);
  if (ptr)
    return ptr;
  throw std::bad_alloc{};
}

void *operator new[](std::size_t sz) {
  void *ptr = std::malloc(sz);
  if (ptr)
    return ptr;
  throw std::bad_alloc{};
}

void *operator new(std::size_t sz, std::align_val_t align) {
  void *ptr = std::aligned_alloc(static_cast<std::size_t>(align), sz);
  if (ptr)
    return ptr;
  throw std::bad_alloc{};
}

void *operator new[](std::size_t sz, std::align_val_t align) {
  void *ptr = std::aligned_alloc(static_cast<std::size_t>(align), sz);
  if (ptr)
    return ptr;
  throw std::bad_alloc{};
}

void *operator new(std::size_t sz, const std::nothrow_t &) noexcept {
  return malloc(sz);
}

void *operator new[](std::size_t sz, const std::nothrow_t &) noexcept {
  return malloc(sz);
}

void *operator new(std::size_t sz, std::align_val_t align,
                   const std::nothrow_t &) noexcept {
  return aligned_alloc(static_cast<std::size_t>(align), sz);
}

void *operator new[](std::size_t sz, std::align_val_t align,
                     const std::nothrow_t &) noexcept {
  return aligned_alloc(static_cast<std::size_t>(align), sz);
}

void operator delete(void *ptr) noexcept { std::free(ptr); }

void operator delete[](void *ptr) noexcept { std::free(ptr); }

void operator delete(void *ptr, std::align_val_t align) noexcept { free(ptr); }

void operator delete[](void *ptr, std::align_val_t align) noexcept {
  free(ptr);
}

void operator delete(void *ptr, std::size_t sz) noexcept { std::free(ptr); }

void operator delete[](void *ptr, std::size_t sz) noexcept { std::free(ptr); }

void operator delete(void *ptr, std::size_t sz,
                     std::align_val_t align) noexcept {
  free(ptr);
}

void operator delete[](void *ptr, std::size_t sz,
                       std::align_val_t align) noexcept {
  free(ptr);
}

namespace xino::runtime {

/* malloc()-family allocator; see c_shim/src/malloc.c. */

/**
 * @brief Allocate a physically-contiguous pages and return a kernel VA.
 *
 * This is a C-ABI wrapper intended for a malloc()-family page allocator hook.
 * It requests `2^order` contiguous pages from the boot allocator and converts
 * the resulting physical address to a kernel virtual address using the current
 * global translation policy (`xino::mm::va_layout::va_layout_enabled`).
 *
 * @param order Allocation order in pages: allocates `2^order` contiguous pages.
 * @return Pointer to the base kernel virtual address of the allocation, or
 *         NULL on failure.
 */
extern "C" void *alloc_page(unsigned order) {
  xino::mm::phys_addr pa{
      xino::runtime::boot_allocator.alloc_pages(xino::nothrow, order)};

  if (pa == xino::mm::phys_addr{0})
    return NULL;
  // Get VA for the PA of the page allocated.
  xino::mm::virt_addr va{xino::mm::va_layout::phys_to_virt(
      pa, xino::mm::va_layout::va_layout_enabled)};

  return static_cast<void *>(va);
}

/**
 * @brief Free pages previously allocated by alloc_page().
 *
 * If the VA cannot be translated (e.g., not representable under the current
 * policy), this function does nothing.
 *
 * Callers are expected to pass the same `order` used for allocation and a
 * page-aligned base address. Mismatched order or invalid VA may lead to leaks
 * or allocator corruption (depending on allocator invariants).
 *
 * @param va Base kernel virtual address previously returned by alloc_page().
 * @param order Allocation order originally used: frees `2^order` pages.
 */
extern "C" void free_page(void *va, unsigned order) {
  std::optional<xino::mm::phys_addr> pa{xino::mm::va_layout::virt_to_phys(
      xino::mm::virt_addr{va}, xino::mm::va_layout::va_layout_enabled)};

  if (pa.has_value())
    xino::runtime::boot_allocator.free_pages(pa.value(), order);
}

} // namespace xino::runtime
