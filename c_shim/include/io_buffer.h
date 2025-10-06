/**
 * @file io_buffer.h
 * @brief Buffered I/O abstraction for low-level or embedded systems.
 *
 * This header defines the `_IO_BUFFER` interface, a simplified replacement
 * for FILE-based buffered I/O using a custom memory buffer and user-defined
 * read, write, and flush operations.
 *
 * It also provides functions to perform formatted output, stream parsing,
 * and `ungetc`-like behavior on a custom I/O buffer.
 *
 * This interface is intended for freestanding environments such as kernels,
 * bootloaders, or other low-level systems without standard I/O libraries.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef IO_BUFFER_H
#define IO_BUFFER_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct io_buffer;

/**
 * @struct io_buffer_ops
 * @brief Defines the I/O backend for an `_IO_BUFFER` stream.
 *
 * The user must implement these callbacks to handle physical I/O,
 * such as reading from a file, serial port, or in-memory stream.
 */
struct io_buffer_ops {
  /**
   * @brief Read function called when input is required.
   * @param io Pointer to the I/O buffer.
   * @param buf Destination buffer.
   * @param count Maximum number of bytes to read.
   * @return Number of bytes read, or 0 on EOF/error.
   */
  size_t (*read)(struct io_buffer *io, char *buf, size_t count);

  /**
   * @brief Write function called when output is flushed or bypassed.
   * @param io Pointer to the I/O buffer.
   * @param buf Source buffer.
   * @param count Number of bytes to write.
   * @return Number of bytes written, or 0 on error.
   */
  size_t (*write)(struct io_buffer *io, const char *buf, size_t count);

  /**
   * @brief Flushes the I/O buffer (input or output).
   * @param io Pointer to the I/O buffer.
   * @return 0 on success, or EOF on error.
   */
  int (*flush)(struct io_buffer *io);
};

/**
 * @struct io_buffer
 * @brief Represents a buffered I/O stream with custom read/write behavior.
 *
 * This structure is used to manage input and output buffering for low-level
 * I/O operations. It supports flushing, ungetc, and different buffering modes
 * (full, line, none). The actual I/O backend is defined via user-provided
 * callbacks in `struct io_buffer_ops`.
 *
 * @note Only one of @c in or @c out is non-zero at any time, indicating whether
 *       the buffer is currently used for input or output.
 */
struct io_buffer {
  int mode;             /**< Buffering mode: _IOFBF, _IOLBF, or _IONBF. */
  char *buffer;         /**< Pointer to buffer area (including head slop). */
  size_t buf_size;      /**< Size of usable buffer (excluding head slop). */
  size_t io_unget_slop; /**< Size of head slop area for __iob_ungetc(). */
  //
  //            +-----------------+----------------------------------+
  //  buffer -> |  io_unget_slop  |             buf_size             |
  //            +-----------------+----------------------------------+
  //                              ^
  //                              inptr (if io_buffer is input)
  //
  struct io_buffer_ops *ops; /**< Backend I/O operations (read/write/flush). */

  // INTERNAL USE:
  size_t in;   /**< Number of unread bytes in input buffer. */
  size_t out;  /**< Number of un-flushed bytes in output buffer. */
  char *inptr; /**< Current read position in buffer. */
};

/**
 * @typedef _IO_BUFFER
 * @brief Typedef shorthand for pointer to `struct io_buffer`.
 */
typedef struct io_buffer *_IO_BUFFER;

/**
 * @def _IO_UNGET_SLOP_FULL
 * @brief Checks whether the slop buffer for ungetc is full.
 *
 * @param io The I/O buffer to check.
 */
#define _IO_UNGET_SLOP_FULL(io) ((io)->inptr == (io)->buffer)

size_t __iob_write(_IO_BUFFER io, const char *buffer, size_t count);
size_t __iob_read(_IO_BUFFER io, char *buffer, size_t count);
int __iob_ungetc(_IO_BUFFER io, char c);

int iob_strtoull(_IO_BUFFER io, int base, unsigned long long *result_ret);
int iob_strtoll(_IO_BUFFER io, int base, long long *result_ret);
int iob_vsnprintf(_IO_BUFFER io, const char *format, va_list _ap);

#ifdef __cplusplus
}
#endif

#endif /* IO_BUFFER_H */
