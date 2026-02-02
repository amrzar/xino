
#ifndef __RUNTIME_HPP__
#define __RUNTIME_HPP__

#include <allocator.hpp>
#include <config.h> // UKERNEL_BOOT_HEAP_SIZE
#include <cstddef>

namespace xino::runtime {

constexpr std::size_t boot_allocator_order{
    xino::allocator::size_to_order(UKERNEL_BOOT_HEAP_SIZE)};

using boot_allocator_t = xino::allocator::buddy<boot_allocator_order>;

// Defined in allocator.cpp.
extern boot_allocator_t boot_allocator;

} // namespace xino::runtime

#endif // __RUNTIME_HPP__
