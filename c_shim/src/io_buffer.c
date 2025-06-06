/**
 * @file iob_ops.c
 * @brief Buffered I/O operations for the `_IO_BUFFER` abstraction.
 *
 * This file implements low-level read and write logic for buffered I/O streams.
 * It supports full, line, and unbuffered modes, with flushing and minimal
 * buffering. Functions include reading, writing, and ungetting characters
 * with handling for internal buffer states.
 *
 * @note This implementation depends on the `_IO_BUFFER` abstraction defined
 *       in `<io_buffer.h>`, which must provide `read`, `write`, and `flush`
 *       callbacks.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <errno.h>
#include <io_buffer.h>
#include <stdbool.h>
#include <stdio.h>  // For _IOFBF, _IOLBF, _IONBF
#include <string.h> // for memcpy

/* Assume a and b are size_t. */
#define min(a, b) (((a) < (b)) ? (a) : (b))

/**
 * @brief Writes data to a buffered stream without triggering a flush.
 *
 * This internal helper function copies data into the output buffer or bypasses
 * it for large writes. If the output buffer is full, it attempts to flush it
 * first.
 *
 * @param io Pointer to the I/O buffer object.
 * @param buffer Pointer to the data to write.
 * @param count Number of bytes to write.
 * @return Number of bytes written, which may be less than @p count if writing
 * fails.
 */
static size_t wbuffer_no_flush(_IO_BUFFER io, const char *buffer,
                               size_t count) {
  size_t n, written = 0;

  // Flush input buffer if there's pending input
  if (io->in) {
    if (io->ops->flush(io))
      return 0;
  }

  while (count) {
    // If buffer is full, flush it
    if (io->out == io->buf_size) {
      if (io->ops->flush(io))
        break;
    }

    if (io->out == 0 && io->buf_size <= count) {
      // Bypass buffer for large writes
      n = io->ops->write(io, buffer, count);
      if (!n)
        break;
    } else {
      n = min(count, io->buf_size - io->out);
      memcpy(&io->buffer[io->out], buffer, n);
      io->out += n;
    }

    buffer += n;
    written += n;
    count -= n;
  }

  return written;
}

/**
 * @brief Writes data to a buffered stream with respect to buffering mode.
 *
 * This is the main public write entry point. It handles:
 * - Fully buffered mode (`_IOFBF`)
 * - Line buffered mode (`_IOLBF`)
 * - Unbuffered mode (`_IONBF`)
 *
 * In line-buffered mode, it flushes up to the last newline.
 *
 * @param io Pointer to the I/O buffer object.
 * @param buffer Pointer to the data to write.
 * @param count Number of bytes to write.
 * @return Total number of bytes written, which may be less than @p count.
 */
size_t __iob_write(_IO_BUFFER io, const char *buffer, size_t count) {
  size_t written = 0;
  size_t flush_len, buffer_len;

  switch (io->mode) {
  case _IOFBF:
    flush_len = 0;
    buffer_len = count;
    break;
  case _IOLBF:
    flush_len = count;
    buffer_len = 0;
    while ((flush_len != 0) && (buffer[flush_len - 1] != '\n')) {
      flush_len--;
      buffer_len++;
    }
    break;
  case _IONBF:
  default:
    flush_len = count;
    buffer_len = 0;
  }

  if (flush_len) {
    written = wbuffer_no_flush(io, buffer, flush_len);
    if (written != flush_len)
      return written;

    if (io->ops->flush(io))
      return written;

    buffer += written;
  }

  if (buffer_len)
    written += wbuffer_no_flush(io, buffer, buffer_len);

  return written;
}

/**
 * @brief Reads data from a buffered stream.
 *
 * Performs read operations into the internal buffer (if buffering is possible),
 * or directly into the user buffer (for large reads or unbuffered mode).
 *
 * @param io Pointer to the I/O buffer object.
 * @param buffer Pointer to the destination buffer.
 * @param count Maximum number of bytes to read.
 * @return Number of bytes actually read, which may be less than @p count.
 */
size_t __iob_read(_IO_BUFFER io, char *buffer, size_t count) {
  size_t n, read = 0;
  char *buf_ptr;

  // Flush pending output first
  if (io->out) {
    if (io->ops->flush(io))
      return 0;
  }

  while (count) {
    if (!io->in) {
      bool buffered = (count < io->buf_size);
      if (buffered) {
        buf_ptr = io->buffer + io->io_unget_slop;
        n = io->buf_size;
      } else {
        buf_ptr = buffer;
        n = count;
      }

      n = io->ops->read(io, buf_ptr, n);
      if (!n)
        return read;

      if (buffered) {
        io->inptr = buf_ptr;
        io->in = n;
      } else {
        buffer += n;
        read += n;
        count -= n;
      }

      continue;
    }

    n = min(count, io->in);
    memcpy(buffer, io->inptr, n);

    buffer += n;
    read += n;
    count -= n;

    io->inptr += n;
    io->in -= n;
  }

  return read;
}

/**
 * @brief Pushes a character back into the input stream.
 *
 * Used to implement `ungetc` functionality. The character is stored in a
 * reserved "slop" area at the beginning of the input buffer.
 *
 * @param io Pointer to the I/O buffer object.
 * @param c Character to be pushed back.
 * @return The character value (as an unsigned char) on success, or a negative
 * errno code.
 */
int __iob_ungetc(_IO_BUFFER io, char c) {
  if (io->out != 0)
    return -EINVAL;

  if (_IO_UNGET_SLOP_FULL(io))
    return -ENOSPC;

  io->inptr--;
  io->inptr[0] = c;
  io->in++;

  return (int)(unsigned char)c;
}
