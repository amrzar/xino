/**
 * @file iob_snprintf.c
 * @brief `snprintf` implementation using the `_IO_BUFFER` formatting backend.
 *
 * This module adapts a custom `_IO_BUFFER`-based formatter (`iob_vsnprintf`)
 * to the standard `snprintf` interface. It provides a minimal I/O backend
 * that writes directly into a user-supplied character buffer while preserving
 * `snprintf` semantics.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <io_buffer.h>
#include <stdarg.h>
#include <stddef.h> // for offsetof.
#include <stdio.h>
#include <string.h> // for memcpy.

#define container_of(ptr, type, member)                                        \
  ((type *)((char *)(ptr) - offsetof(type, member)))

/* Assume a and b are size_t. */
#define min(a, b) (((a) < (b)) ? (a) : (b))

struct iob {
  struct io_buffer io;
  char *str;
  // Remaining space in destination.
  size_t size;
};

static size_t iob_write(struct io_buffer *io, const char *buf, size_t count) {
  struct iob *iob = container_of(io, struct iob, io);
  size_t n;

  if (iob->size == 0)
    return 0;

  // Leave 1 byte for NULL-termination (iob->size >= 1).
  n = min(count, iob->size - 1);
  if (n) {
    memcpy(iob->str, buf, n);
    iob->str += n;
    iob->size -= n;
  }

  // Bytes successfully written.
  return n;
}

static int iob_flush(struct io_buffer *io) {
  (void)io;
  // NOTHING TO DO.
  return 0;
}

static struct io_buffer_ops ops = {
    .write = iob_write,
    .flush = iob_flush,
};

/**
 * @brief Print formatted data to a string using the `_IO_BUFFER` formatter.
 *
 * Adapts `iob_vsnprintf()` to standard `snprintf` semantics:
 *
 * - Writes at most @p size bytes to @p str, including the terminating NULL.
 * - If @p size is greater than zero, the result is always NULL-terminated.
 * - Returns the number of characters that **would** have been written if
 *   enough space had been available (excluding the terminating NULL).
 *
 * This allows the caller to detect truncation when `return_value >= size`.
 *
 * @param str  Destination buffer.
 * @param size Size of @p str in bytes. May be zero.
 * @param fmt  `printf`-style format string.
 * @param ...  Variadic arguments consumed according to @p fmt.
 * @return The number of characters that would have been written, not counting
 *         the terminating NULL.
 *
 * @note When @p size is zero, no bytes are written and the buffer is not
 *       dereferenced; the return value still reflects the full length.
 */
int snprintf(char *str, size_t size, const char *fmt, ...) {
  struct iob iob = {0};
  int ret;

  iob.str = str;
  iob.size = size;
  iob.io.mode = _IONBF;
  // iob.io.buf_size == 0, bypass iob.io.buffer.
  iob.io.ops = &ops;
  // iob.io.io_unget_slop == 0, no ____iob_read() and __iob_ungetc().

  va_list ap;
  va_start(ap, fmt);
  // @ret should be "would-have-written" size.
  ret = iob_vsnprintf(&iob.io, fmt, ap);
  va_end(ap);

  if (size)
    *iob.str = '\0';

  return ret;
}
