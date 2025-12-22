
#ifndef __ERRNO_HPP__
#define __ERRNO_HPP__

#include <cstdint>

namespace xino {
using error_t = std::int64_t;
} // namespace xino

namespace xino::error_nr {
constexpr xino::error_t ok{0};        // Ok.
constexpr xino::error_t nomem{-1};    // No memory.
constexpr xino::error_t invalid{-2};  // Invalid.
constexpr xino::error_t overflow{-3}; // Arithmetic overflow operation.
} // namespace xino::error_nr

#endif // __ERRNO_HPP__
