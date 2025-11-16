/**
 * @file uart.hpp
 * @brief UART driver namespace with pluggable backends (PL011 and others).
 *
 * The `uart` namespace is an extensible container for **multiple UART device
 * backends**. Each backend exposes a tiny, static API:
 *
 *   - `init(base, use_fifo=false)` - bring-up; selects the device MMIO base.
 *   - `putc(char)` - blocking TX of a single character (CRLF on '\n').
 *   - `set_base(base)` - change the active MMIO base at runtime.
 *
 * A convenience alias `uart::driver` is selected at build time via `<config.h>`
 * (e.g., `using driver = PL011;`).
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __UART_HPP__
#define __UART_HPP__

#include <config.h> // for UKERNEL_UART_DRIVER and UKERNEL_UART_BASE.
#include <cstdint>

namespace xino::plat::uart {

/**
 * @class PL011
 * @brief PL011 backend (TX-only).
 *
 * This backend provides a tiny TX-only interface suitable for early boot.
 * It assumes platform/firmware has already configured baud rate and clock
 * (extend init() if required).
 */
class PL011 {
  /** @brief Active MMIO base; defined in uart.cpp. */
  static std::uintptr_t uart_base;

  static volatile std::uint32_t &reg_at(std::uintptr_t base,
                                        std::uintptr_t off) noexcept {
    return *reinterpret_cast<volatile std::uint32_t *>(base + off);
  }

  // Register offsets (see section 3.2).
  static constexpr std::uintptr_t UARTDR = 0x000;    /**< Data Register. */
  static constexpr std::uintptr_t UARTFR = 0x018;    /**< Flag Register. */
  static constexpr std::uintptr_t UARTLCR_H = 0x02C; /**< Line Control. */
  static constexpr std::uintptr_t UARTCR = 0x030;    /**< Control. */
  static constexpr std::uintptr_t UARTIMSC = 0x038;  /**< IRQ Mask Set/Clear. */
  static constexpr std::uintptr_t UARTICR = 0x044;   /**< IRQ Clear. */

  // Bitfields.
  static constexpr std::uint32_t TXFF = 1U << 5;   /**< UARTFR. TX FIFO full. */
  static constexpr std::uint32_t UARTEN = 1U << 0; /**< UARTCR. UART enable. */
  static constexpr std::uint32_t TXE = 1U << 8;    /**< UARTCR. TX enable. */
  static constexpr std::uint32_t WLEN_8 = 3U << 5; /**< UARTLCR_H. 8-bit. */
  static constexpr std::uint32_t FEN = 1U << 4; /**< UARTLCR_H. FIFO enable. */

  /** @brief Busy-wait until TX FIFO has space. */
  static void wait_tx_space_at(std::uintptr_t base) noexcept {
    while (reg_at(base, UARTFR) & TXFF) {
      /* Spin. */
    }
  }

public:
  PL011() = delete;

  static void init(std::uintptr_t new_base, bool use_fifo = false) noexcept;
  static void putc(char c) noexcept;
  /** @brief Change the base without reprogramming (for pure VA remap). */
  static void set_base(std::uintptr_t new_base) noexcept {
    uart_base = new_base;
  }
};

/* class xxx_uart { ... }; */

/* Build time selected UART backend alias. */
using driver = UKERNEL_UART_DRIVER;
} // namespace xino::plat::uart

#endif // __UART_HPP__
