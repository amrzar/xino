
#include <cstdlib> // for std::malloc and ste::free.
#include <new>

extern "C" {
typedef void (*ctor_t)();
extern ctor_t __init_array_start[], __init_array_end[];
extern ctor_t __fini_array_start[], __fini_array_end[];
}

static void run_init_array() {
  for (auto p = __init_array_start; p != __init_array_end; ++p)
    if (*p)
      (*p)();
}

static void run_fini_array() {
  for (auto p = __fini_array_start; p != __fini_array_end; ++p)
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
  register_eh_frames();
  run_init_array();
  main();
  run_fini_array();
  deregister_eh_frames();
}

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

void *operator new(std::size_t sz, const std::nothrow_t &) noexcept {
  return malloc(sz);
}

void *operator new[](std::size_t sz, const std::nothrow_t &) noexcept {
  return malloc(sz);
}

void operator delete(void *ptr) noexcept { std::free(ptr); }

void operator delete[](void *ptr) noexcept { std::free(ptr); }

void operator delete(void *ptr, std::size_t sz) noexcept { std::free(ptr); }

void operator delete[](void *ptr, std::size_t sz) noexcept { std::free(ptr); }
