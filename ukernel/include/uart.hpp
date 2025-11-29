/**
 * @file uart.hpp
 * @brief UART driver namespace with pluggable backends (PL011 and others).
 *
 * The `uart` namespace is an extensible container for **multiple UART device
 * backends**. Each backend exposes a tiny, static API:
 *
 *   - `init(base, fifo=false)` - bring-up; selects the device MMIO base.
 *   - `putc(char)` - blocking TX of a single character (CRLF on '\n').
 *   - `set_base(base)` - change the active MMIO base at runtime.
 *
 * A convenience alias `xino::plat::uart::driver` is selected at build time
 * via `<config.h>` (e.g., `using driver = PL011;`).
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __UART_HPP__
#define __UART_HPP__

#include <addr_types.hpp> // phys::addr, virt::addr.
#include <config.h>       // for UKERNEL_UART_DRIVER and UKERNEL_UART_BASE.
#include <cstdint>

namespace xino::plat::uart {

/**
 * @class PL011
 * @brief PL011 backend (TX-only).
 *
 * This backend provides a tiny TX-only interface suitable for early boot.
 * It assumes platform/firmware has already configured baud rate and clock
 * (extend init() if required).
 *
 * See
 * https://documentation-service.arm.com/static/5e8e36c2fd977155116a90b5?token=.
 */
class PL011 {
private:
  /** @brief Active MMIO base; zero means "not initialized". */
  inline static xino::virt::addr uart_base{};

  static volatile std::uint32_t &reg_at(std::uintptr_t off) noexcept {
    return *uart_base.as_ptr<volatile std::uint32_t>(off);
  }

  // Register offsets (see section 3.2).
  static constexpr std::uintptr_t UARTDR{0x000};    /**< Data Register. */
  static constexpr std::uintptr_t UARTFR{0x018};    /**< Flag Register. */
  static constexpr std::uintptr_t UARTLCR_H{0x02c}; /**< Line Control. */
  static constexpr std::uintptr_t UARTCR{0x030};    /**< Control. */
  static constexpr std::uintptr_t UARTIMSC{0x038};  /**< IRQ Mask Set/Clear. */
  static constexpr std::uintptr_t UARTICR{0x044};   /**< IRQ Clear. */

  // Bitfields.
  static constexpr std::uint32_t UARTFR_TXFF{1U << 5};   /**< TX FIFO full. */
  static constexpr std::uint32_t UARTCR_UARTEN{1U << 0}; /**< UART enable. */
  static constexpr std::uint32_t UARTCR_TXE{1U << 8};    /**< TX enable. */
  static constexpr std::uint32_t UARTLCR_H_WLEN_8{3U << 5}; /**< 8-bit. */
  static constexpr std::uint32_t UARTLCR_H_FEN{1U << 4};    /**< FIFO enable. */

  /** @brief Busy-wait until TX FIFO has space. */
  static void wait_tx_space_at() noexcept {
    while (reg_at(UARTFR) & UARTFR_TXFF) {
      /* Spin. */
    }
  }

public:
  PL011() = delete;

  static void init(const xino::virt::addr &new_base,
                   bool fifo = false) noexcept;
  static void putc(char c) noexcept;
  /** @brief Change the base without reprogramming (for pure VA remap). */
  static void set_base(const xino::virt::addr &new_base) noexcept {
    uart_base = new_base;
  }
};

/**
 * @class DW_APB
 * @brief Synopsys DesignWare APB (NS16550-like) UART backend (TX-only).
 *
 * See https://github.com/amrzar/RK3588, Chapter 19.
 */
class DW_APB {
private:
  /** @brief Active MMIO base; zero means "not initialized". */
  inline static xino::virt::addr uart_base{};

  static volatile std::uint32_t &reg_at(std::uintptr_t off) noexcept {
    return *uart_base.as_ptr<volatile std::uint32_t>(off);
  }

  // Register offsets (see section 19.4).
  static constexpr std::uintptr_t THR{0x0000}; /**< Transmit Buffer. */
  static constexpr std::uintptr_t IER{0x0004}; /**< nterrupt Enable. */
  static constexpr std::uintptr_t FCR{0x0008}; /**< FFIFO Enable. */
  static constexpr std::uintptr_t LCR{0x000c}; /**< Line Control. */
  static constexpr std::uintptr_t MCR{0x0010}; /**< Modem Control. */
  static constexpr std::uintptr_t LSR{0x0014}; /**< Line Status. */

  // Bitfields.
  static constexpr std::uint32_t LCR_WLEN8{3U};       /**< 8-bit. */
  static constexpr std::uint32_t FCR_FIFOE{1U << 0};  /**< FIFO enable. */
  static constexpr std::uint32_t FCR_RFIFOR{1U << 1}; /**< RCVR FIFO Reset. */
  static constexpr std::uint32_t FCR_XFIFOR{1U << 2}; /**< XMIT FIFO Reset. */
  static constexpr std::uint32_t LSR_THRE{
      1U << 5}; /**< Transmit Holding Register Empty. */
  static constexpr std::uint32_t LSR_TEMT{1U << 6}; /**< Transmitter empty. */

  static void wait_tx_space_at() noexcept {
    while (!(reg_at(LSR) & LSR_THRE)) {
      /* spin */
    }
  }

public:
  DW_APB() = delete;

  static void init(const xino::virt::addr &new_base,
                   bool fifo = false) noexcept;
  static void putc(char c) noexcept;
  /** @brief Change the base without reprogramming (for pure VA remap). */
  static void set_base(const xino::virt::addr &new_base) noexcept {
    uart_base = new_base;
  }
};

/* class xxx_uart { ... }; */

/* Build time selected UART backend alias. */
using driver = UKERNEL_UART_DRIVER;
} // namespace xino::plat::uart

#endif // __UART_HPP__
