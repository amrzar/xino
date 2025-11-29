
#include <io_buffer.h>
#include <stddef.h>
#include <uart.hpp>

namespace xino::plat::uart {

/* PL011. */

void PL011::init(const xino::virt::addr &new_base, bool fifo) noexcept {
  uart_base = new_base;

  reg_at(UARTCR) = 0;      // Disable UART.
  reg_at(UARTIMSC) = 0;    // Mask interrupts
  reg_at(UARTICR) = 0x7FF; // Clear pending interrupts.
  reg_at(UARTLCR_H) = UARTLCR_H_WLEN_8 | (fifo ? UARTLCR_H_FEN : 0);
  reg_at(UARTCR) = UARTCR_UARTEN | UARTCR_TXE;
}

void PL011::putc(char c) noexcept {
  if (uart_base == xino::virt::addr{})
    return;

  if (c == '\n') {
    wait_tx_space_at();
    reg_at(UARTDR) =
        static_cast<std::uint32_t>(static_cast<std::uint8_t>('\r'));
  }

  wait_tx_space_at();
  reg_at(UARTDR) = static_cast<std::uint32_t>(static_cast<std::uint8_t>(c));
}

/* DW_APB. */

void DW_APB::init(const xino::virt::addr &new_base, bool fifo) noexcept {
  uart_base = new_base;

  reg_at(IER) = 0x00;      // Disable Interrupts.
  reg_at(LCR) = LCR_WLEN8; // 8N1, disable parity, normal access access.
  reg_at(FCR) = fifo ? FCR_FIFOE | FCR_RFIFOR | FCR_XFIFOR : 0x00;
  reg_at(MCR) = 0x00; // No modem ctrl.
}

void DW_APB::putc(char c) noexcept {
  if (uart_base == xino::virt::addr{})
    return;

  if (c == '\n') {
    wait_tx_space_at();
    reg_at(THR) = static_cast<std::uint32_t>(static_cast<std::uint8_t>('\r'));
  }

  wait_tx_space_at();
  reg_at(THR) = static_cast<std::uint32_t>(static_cast<std::uint8_t>(c));
}

/* xxx_uart. */

} // namespace xino::plat::uart

extern "C" {
void uart_setup() {
  xino::plat::uart::driver::init(xino::virt::addr{UKERNEL_UART_BASE}, true);
}

void uart_set_base(uintptr_t base) {
  xino::plat::uart::driver::set_base(xino::virt::addr{base});
}

/* Override stdio.h weak writers. */

size_t iob_write_stdout(struct io_buffer *io, const char *buf, size_t count) {
  (void)io;
  for (int i = 0; i < count; i++)
    xino::plat::uart::driver::putc(buf[i]);
  return count;
}

size_t iob_write_stderr(struct io_buffer *io, const char *buf, size_t count) {
  (void)io;
  for (int i = 0; i < count; i++)
    xino::plat::uart::driver::putc(buf[i]);
  return count;
}
}
