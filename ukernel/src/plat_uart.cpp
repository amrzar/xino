
#include <io_buffer.h>
#include <plat_uart.hpp>
#include <stddef.h>

namespace xino::plat::uart {

/* PL011. */

void PL011::init(const xino::mm::virt_addr &new_base, bool fifo) noexcept {
  using namespace xino::io;

  uart_base = new_base;

  writel(0x0, reg(UARTCR));    // Disable UART.
  writel(0x0, reg(UARTIMSC));  // Mask interrupts.
  writel(0x7FF, reg(UARTICR)); // Clear pending interrupts.
  writel(UARTLCR_H_WLEN_8 | (fifo ? UARTLCR_H_FEN : 0), reg(UARTLCR_H));
  writel(UARTCR_UARTEN | UARTCR_TXE, reg(UARTCR));
}

void PL011::putc(char c) noexcept {
  using namespace xino::io;

  if (uart_base == xino::mm::virt_addr{0})
    return;

  if (c == '\n') {
    wait_tx_space_at();
    writel(static_cast<std::uint32_t>(static_cast<std::uint8_t>('\r')),
           reg(UARTDR));
  }

  wait_tx_space_at();
  writel(static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)), reg(UARTDR));
}

/* DW_APB. */

void DW_APB::init(const xino::mm::virt_addr &new_base, bool fifo) noexcept {
  using namespace xino::io;

  uart_base = new_base;

  writel(0x0, reg(IER));       // Disable Interrupts.
  writel(LCR_WLEN8, reg(LCR)); // 8N1, disable parity, normal access access.
  writel(fifo ? FCR_FIFOE | FCR_RFIFOR | FCR_XFIFOR : 0x0, reg(FCR));
  writel(0x0, reg(MCR)); // No modem ctrl.
}

void DW_APB::putc(char c) noexcept {
  using namespace xino::io;

  if (uart_base == xino::mm::virt_addr{0})
    return;

  if (c == '\n') {
    wait_tx_space_at();
    writel(static_cast<std::uint32_t>(static_cast<std::uint8_t>('\r')),
           reg(THR));
  }

  wait_tx_space_at();
  writel(static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)), reg(THR));
}

/* xxx_uart. */

} // namespace xino::plat::uart

extern "C" {
void uart_setup() {
  xino::plat::uart::driver::init(xino::mm::virt_addr{UKERNEL_UART_BASE}, true);
}

void uart_set_base(uintptr_t base) {
  xino::plat::uart::driver::set_base(xino::mm::virt_addr{base});
}

/* Override stdio.h weak writers. */

size_t iob_write_stdout(struct io_buffer *io, const char *buf, size_t count) {
  (void)io;
  for (int i{0}; i < count; i++)
    xino::plat::uart::driver::putc(buf[i]);
  return count;
}

size_t iob_write_stderr(struct io_buffer *io, const char *buf, size_t count) {
  (void)io;
  for (int i{0}; i < count; i++)
    xino::plat::uart::driver::putc(buf[i]);
  return count;
}
}
