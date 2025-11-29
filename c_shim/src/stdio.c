/**
 * @file stdio.c
 * @brief Minimal implementation of stdio-style APIs for use with `_IO_BUFFER`.
 *
 * This file provides a thin wrapper over the `_IO_BUFFER` interface to
 * implement standard C stdio-like functions (`fprintf`, `fflush`, `fputc`,
 * `fwrite`, etc.) in environments where the standard `FILE` stream is not
 * available or desirable.
 *
 * The implementation redefines `FILE` to use `struct io_buffer`, and forwards
 * calls to internal `_iob_*` functions.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <io_buffer.h>
#include <stdarg.h>

#define __DEFINED_struct__IO_FILE
#define __DEFINED_FILE

/**
 * @typedef FILE
 * @brief Redefines FILE to point to @ref io_buffer for custom stream handling.
 *
 * Using `__DEFINED_struct__IO_FILE` and `__DEFINED_FILE` to override the
 * default definition of FILE.
 */
typedef struct io_buffer FILE;
/* It should be after `typedef`. */
#include <stdio.h>

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
  _IO_BUFFER io = stream;

  return iob_vsnprintf(io, fmt, ap);
}

int fprintf(FILE *stream, const char *fmt, ...) {
  int ret;

  va_list ap;
  va_start(ap, fmt);
  ret = vfprintf(stream, fmt, ap);
  va_end(ap);

  return ret;
}

int fflush(FILE *stream) {
  _IO_BUFFER io = stream;

  return io->ops->flush(io);
}

int fputc(int c, FILE *stream) {
  char ch = (char)c;

  _IO_BUFFER io = stream;
  if (!__iob_write(io, &ch, 1))
    return EOF;
  return c;
}

size_t fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream) {
  _IO_BUFFER io = stream;

  return __iob_write(io, ptr, size * nitems);
}

/* stderr and stdout. */

size_t iob_write_stdout(struct io_buffer *io, const char *buf, size_t count)
    __attribute__((weak));

static int iob_flush_stdout(struct io_buffer *io) {
  (void)io;
  // NOTHING TO DO.
  return 0;
}

static struct io_buffer_ops stdout_ops = {
    .write = iob_write_stdout,
    .flush = iob_flush_stdout,
};

static struct io_buffer __stdout = {
    .mode = _IONBF,
    // Bypass iob.io.buffer.
    .buf_size = 0,
    // No __iob_read() and __iob_ungetc().
    .io_unget_slop = 0,
    .ops = &stdout_ops,
};

FILE *const stdout = &__stdout;

size_t iob_write_stderr(struct io_buffer *io, const char *buf, size_t count)
    __attribute__((weak));

static int iob_flush_stderr(struct io_buffer *io) {
  (void)io;
  // NOTHING TO DO.
  return 0;
}

static struct io_buffer_ops stderr_ops = {
    .write = iob_write_stderr,
    .flush = iob_flush_stderr,
};

static struct io_buffer __stderr = {
    .mode = _IONBF,
    // Bypass iob.io.buffer.
    .buf_size = 0,
    // No __iob_read() and __iob_ungetc().
    .io_unget_slop = 0,
    .ops = &stderr_ops,
};

FILE *const stderr = &__stderr;

/* Default writers. */

size_t iob_write_stdout(struct io_buffer *io, const char *buf, size_t count) {
  return count;
}

size_t iob_write_stderr(struct io_buffer *io, const char *buf, size_t count) {
  return count;
}
