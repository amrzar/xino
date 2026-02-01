
#include <allocator.hpp> // xino::allocator::boot_allocator
#include <cstdio>
#include <cstdlib> // for std::malloc and ste::free
#include <mm_va_layout.hpp>
#include <new>
#include <runtime.hpp>

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

static void register_eh_frames() {
  if (__register_frame)
    __register_frame(__eh_frame_start);
}

static void deregister_eh_frames() {
  if (__deregister_frame)
    __deregister_frame(__eh_frame_start);
}

void main();

extern "C" void ukernel_entry() {
  /* uKernel has been relocated, and the boot allocator is functional. */

  register_eh_frames();
  run_init_array();
  main();
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

/** @brief State of uKernel mapping: [true] established, [false]: identity. */
constinit bool use_mapping{};

/* malloc()-family allocator; see c_shim/src.malloc.c. */

/**
 * @brief Allocate a physically-contiguous pages and return a kernel VA.
 *
 * This is a C-ABI wrapper intended for a malloc()-family page allocator hook.
 * It requests `2^order` contiguous pages from the boot allocator and converts
 * the resulting physical address to a kernel virtual address using the current
 * global translation policy (`use_mapping`).
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
  xino::mm::virt_addr va{xino::mm::va_layout::phys_to_virt(pa, use_mapping)};

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
  std::optional<xino::mm::phys_addr> pa{
      xino::mm::va_layout::virt_to_phys(xino::mm::virt_addr{va}, use_mapping)};

  if (pa.has_value())
    xino::runtime::boot_allocator.free_pages(pa.value(), order);
}

} // namespace xino::runtime
