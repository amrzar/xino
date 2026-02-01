
#ifndef __RUNTIME_HPP__
#define __RUNTIME_HPP__

#include <allocator.hpp> // for buddy and helpers
#include <config.h>      // for UKERNEL_BOOT_HEAP_SIZE
#include <cpu.hpp>       // for cpu_state
#include <cstddef>

namespace xino::runtime {

extern bool use_mapping;

extern struct xino::cpu::cpu_state cpu_state;

constexpr std::size_t boot_allocator_order{
    xino::allocator::size_to_order(UKERNEL_BOOT_HEAP_SIZE)};

extern xino::allocator::buddy<boot_allocator_order> boot_allocator;

} // namespace xino::runtime

#endif // __RUNTIME_HPP__
