
#ifndef __RUNTIME_HPP__
#define __RUNTIME_HPP__

#include <allocator.hpp>
#include <config.h> // UKERNEL_BOOT_HEAP_SIZE
#include <cstddef>
#include <mm_paging.hpp>

namespace xino::runtime {

constexpr std::size_t boot_allocator_order{
    xino::allocator::size_to_order(UKERNEL_BOOT_HEAP_SIZE)};

using boot_allocator_t = xino::allocator::buddy<boot_allocator_order>;

// Defined in allocator.cpp.
extern boot_allocator_t boot_allocator;

// Page table type used for ST1, uKernel. It uses boot allocator.
using ukernel_pt_t = xino::mm::paging::page_table<xino::mm::paging::stage::ST_1,
                                                  boot_allocator_t>;

} // namespace xino::runtime

#endif // __RUNTIME_HPP__
