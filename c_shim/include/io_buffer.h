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
 * @struct io_buffer_operations
 * @brief Defines the I/O backend for an `_IO_BUFFER` stream.
 *
 * The user must implement these callbacks to handle physical I/O,
 * such as reading from a file, serial port, or in-memory stream.
 */
struct io_buffer_operations {
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
 * callbacks in `struct io_buffer_operations`.
 *
 * @note Only one of @c in or @c out is non-zero at any time, indicating whether
 *       the buffer is currently used for input or output.
 */
struct io_buffer {
  int mode;             /**< Buffering mode: _IOFBF, _IOLBF, or _IONBF. */
  size_t in;            /**< Number of unread bytes in input buffer. */
  size_t out;           /**< Number of unflushed bytes in output buffer. */
  char *buffer;         /**< Pointer to buffer area (including slop). */
  size_t buf_size;      /**< Size of usable buffer, excluding slop. */
  char *inptr;          /**< Current read position in buffer. */
  size_t io_unget_slop; /**< Size of slop area before buffer for ungetc. */

  struct io_buffer_operations
      *ops; /**< Backend I/O operations (read/write/flush). */
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
size_t __iob_write(_IO_BUFFER io, const char *buffer, size_t count);

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
size_t __iob_read(_IO_BUFFER io, char *buffer, size_t count);

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
int __iob_ungetc(_IO_BUFFER io, char c);

/**
 * @brief Parses an unsigned long long integer from the input stream.
 *
 * Supports optional base prefixes like `0x` for hex.
 *
 * @param io Input stream.
 * @param base Numerical base (0 = auto-detect).
 * @param result_ret Pointer to store the parsed value.
 * @return 0 on success, or -ERANGE on overflow.
 */
int iob_strtoull(_IO_BUFFER io, int base, unsigned long long *result_ret);

/**
 * @brief Parses a signed long long integer from a buffered stream.
 *
 * Handles optional '+' or '-' sign. Checks for signed overflow or underflow.
 *
 * @param io Pointer to input stream.
 * @param base Numerical base (0 = auto-detect).
 * @param out Pointer to store result.
 * @return 0 on success, or -ERANGE on overflow/underflow.
 */
int iob_strtoll(_IO_BUFFER io, int base, long long *result_ret);

/**
 * @brief Minimal implementation of `vsnprintf`-like formatting.
 *
 * Parses a format string and variadic argument list, writing output to the
 * `_IO_BUFFER`. Supports a wide subset of `printf` format specifiers and flags.
 *
 * @param io Output stream buffer.
 * @param fmt Format string.
 * @param ap_src `va_list` containing arguments to format.
 * @return Number of bytes written to @p io, -1 on failure (e.g., unsupported
 * format).
 */
int iob_vsnprintf(_IO_BUFFER io, const char *format, va_list _ap);

#ifdef __cplusplus
}
#endif

#endif /* IO_BUFFER_H */
