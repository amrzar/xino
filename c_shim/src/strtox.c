/**
 * @file strtox.c
 * @brief Buffered string-to-integer conversion for `_IO_BUFFER` streams.
 *
 * This file implements stream-based numeric parsing functions similar to
 * `strtoull()` and `strtoll()`, adapted to operate on `_IO_BUFFER` input.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <errno.h>
#include <io_buffer.h>
#include <limits.h>
#include <stdbool.h>

/**
 * @brief Converts a single ASCII character to its integer digit value.
 *
 * Supports decimal (0–9) and hexadecimal (a–f, A–F) digits.
 *
 * @param ch Input character.
 * @return Value from 0 to 15, or -1 if the character is not a valid digit.
 */
static int digit_value(int ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  return -1;
}

/**
 * @brief Reads and parses an unsigned long long integer from a buffered stream.
 *
 * Accepts decimal, octal, or hexadecimal based on prefix (0, 0x).
 * Detects and returns overflow errors via `-ERANGE`.
 *
 * @param io Pointer to input buffer stream.
 * @param base Numerical base (0 for auto-detect).
 * @param out Pointer to store the parsed result.
 * @return 0 on success, or -ERANGE if overflow occurs.
 */
static int __iob_strtoull(_IO_BUFFER io, int base, unsigned long long *out) {
  unsigned long long result = 0;
  char ch;

  // Auto-detect base if unspecified
  if (base == 0) {
    if (__iob_read(io, &ch, 1))
      goto done;

    if (ch == '0') {
      if (__iob_read(io, &ch, 1) && (ch == 'x' || ch == 'X')) {
        base = 16;
      } else {
        base = 8;
        if (ch)
          __iob_ungetc(io, ch);
      }
    } else {
      base = 10;
      __iob_ungetc(io, ch);
    }

  } else if (base == 16) {
    char prefix[2];
    size_t n = __iob_read(io, prefix, 2);

    if (n == 2 && prefix[0] == '0' && (prefix[1] == 'x' || prefix[1] == 'X')) {
      // Valid 0x prefix
    } else {
      // Push back if not matching
      while (n-- > 0)
        __iob_ungetc(io, prefix[n]);
    }
  }

  // Parse digit stream
  while (__iob_read(io, &ch, 1)) {
    int val = digit_value(ch);
    if (val < 0 || val >= base) {
      __iob_ungetc(io, ch);
      break;
    }

    // Detect overflow
    if (result > (ULLONG_MAX - val) / base) {
      __iob_ungetc(io, ch);
      return -ERANGE;
    }

    result = result * base + val;
  }

done:
  *out = result;
  return 0;
}

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
int iob_strtoull(_IO_BUFFER io, int base, unsigned long long *out) {
  return __iob_strtoull(io, base, out);
}

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
int iob_strtoll(_IO_BUFFER io, int base, long long *out) {
  unsigned long long ull_result;
  long long result = 0;
  char ch;
  int ret;

  if (!__iob_read(io, &ch, 1))
    goto done;

  if (ch == '-') {
    ret = iob_strtoull(io, base, &ull_result);
    if (ret < 0)
      return ret;

    if ((long long)(-ull_result) > 0)
      return -ERANGE;

    result = -ull_result;
  } else {
    if (ch != '+')
      __iob_ungetc(io, ch);

    ret = iob_strtoull(io, base, &ull_result);
    if (ret < 0)
      return ret;

    if ((long long)(ull_result) < 0)
      return -ERANGE;

    result = ull_result;
  }

done:
  *out = result;
  return 0;
}
