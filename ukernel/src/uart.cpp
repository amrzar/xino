
#include <io_buffer.h>
#include <stddef.h>
#include <uart.hpp>

namespace xino::plat::uart {

/* PL011. */

/* Active MMIO base; zero means "not initialized". */
std::uintptr_t PL011::uart_base = 0;

void PL011::init(std::uintptr_t new_base, bool use_fifo) noexcept {
  uart_base = new_base;

  reg_at(uart_base, UARTCR) = 0;      // Disable UART.
  reg_at(uart_base, UARTIMSC) = 0;    // Mask interrupts
  reg_at(uart_base, UARTICR) = 0x7FF; // Clear pending interrupts.
  reg_at(uart_base, UARTLCR_H) = WLEN_8 | (use_fifo ? FEN : 0);
  reg_at(uart_base, UARTCR) = UARTEN | TXE;
}

void PL011::putc(char c) noexcept {
  if (!uart_base)
    return;

  if (c == '\n') {
    wait_tx_space_at(uart_base);
    reg_at(uart_base, UARTDR) =
        static_cast<std::uint32_t>(static_cast<std::uint8_t>('\r'));
  }

  wait_tx_space_at(uart_base);
  reg_at(uart_base, UARTDR) =
      static_cast<std::uint32_t>(static_cast<std::uint8_t>(c));
}

/* xxx_uart. */

} // namespace xino::plat::uart

extern "C" {
void uart_setup(void) {
  xino::plat::uart::driver::init(UKERNEL_UART_BASE, true);
}

void uart_set_base(std::uintptr_t base) {
  xino::plat::uart::driver::set_base(base);
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
