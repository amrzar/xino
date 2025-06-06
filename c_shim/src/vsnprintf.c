/**
 * @file iob_vsnprintf.c
 * @brief Minimal `vsnprintf`-like formatter for `_IO_BUFFER` streams.
 *
 * This file implements a subset of the `printf` family (formatting of integers,
 * strings, characters, and pointers) targeting `_IO_BUFFER`, a buffered I/O
 * abstraction.
 *
 * It supports flags (`-+ #0`), field width and precision (including `*`),
 * signed and unsigned formatting, character and string output, pointer
 * formatting, and `hh`, `h`, `l`, `ll` length modifiers.
 *
 * @note Floating point formatting is not supported.
 * @note All output is written to a user-defined `_IO_BUFFER` via `__iob_write`.
 * @note It is not thread-safe.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <io_buffer.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

/**
 * @defgroup FormatFlags Format Flags
 * @brief Flags used to modify the behavior of format specifiers.
 * @{
 */

/**
 * @def FL_ZERO
 * @brief Pad numeric values with leading zeros.
 */
#define FL_ZERO (1 << 0)

/**
 * @def FL_MINUS
 * @brief Left-align output within the field width.
 */
#define FL_MINUS (1 << 1)

/**
 * @def FL_PLUS
 * @brief Always print the sign for signed values.
 */
#define FL_PLUS (1 << 2)

/**
 * @def FL_SPACE
 * @brief Insert a space before positive signed values.
 */
#define FL_SPACE (1 << 3)

/**
 * @def FL_HASH
 * @brief Use an alternate form (e.g., `0x`, `0`).
 */
#define FL_HASH (1 << 4)

/**
 * @def FL_SIGNED
 * @brief Interpret number as signed.
 */
#define FL_SIGNED (1 << 5)

/**
 * @def FL_UPPER
 * @brief Use uppercase letters for hex output.
 */
#define FL_UPPER (1 << 6)

/** @} */ // end of FormatFlags

enum { RANK_CHAR = -2, RANK_SHORT, RANK_INT, RANK_LONG, RANK_LLONG };

/**
 * @brief Running count of bytes written by `iob_vsnprintf`.
 */
static int iob_count;

/**
 * @brief Writes a single character to the buffer.
 */
static void put_char(_IO_BUFFER io, char c) {
  __iob_write(io, &c, 1);
  iob_count++;
}

/**
 * @brief Pads the output with a specific character.
 *
 * @param io Destination I/O buffer.
 * @param left If true, pad to the left (right-align).
 * @param c Padding character (typically ' ' or '0').
 * @param width Desired total field width.
 * @param len Actual length of printed content.
 */
static void pad(_IO_BUFFER io, bool left, char c, int width, int len) {
  int i;
  if (!left && width > len) {
    for (i = width - len; i > 0; i--)
      put_char(io, c);
  }
}

static const char *digit_table(unsigned flags) {
  return (flags & FL_UPPER) ? "0123456789ABCDEF" : "0123456789abcdef";
}

/**
 * @brief Formats an integer and writes it to the buffer.
 */
static void format_integer(_IO_BUFFER io, unsigned long long value,
                           unsigned flags, int base, int width, int precision) {
  char buf[65];
  int pos = 0;
  bool negative = false;

  // Convert signed if needed
  if (flags & FL_SIGNED) {
    long long sv = (long long)value;
    if (sv < 0) {
      negative = true;
      value = (unsigned long long)(-sv);
    }
  }

  // Convert digits in reverse order
  do {
    buf[pos++] = digit_table(flags)[value % base];
    value /= base;
  } while (value);

  while (pos < precision)
    buf[pos++] = '0';

  int prefix_len = 0;
  char prefix[3] = {0};

  if (negative)
    prefix[prefix_len++] = '-';
  else if (flags & FL_PLUS)
    prefix[prefix_len++] = '+';
  else if (flags & FL_SPACE)
    prefix[prefix_len++] = ' ';

  if (flags & FL_HASH) {
    if (base == 16) {
      prefix[prefix_len++] = '0';
      prefix[prefix_len++] = (flags & FL_UPPER) ? 'X' : 'x';
    } else if (base == 8) {
      prefix[prefix_len++] = '0';
    }
  }

  int total_len = prefix_len + pos;

  pad(io, !(flags & FL_MINUS), (flags & FL_ZERO) ? '0' : ' ', width, total_len);

  for (int i = 0; i < prefix_len; i++)
    put_char(io, prefix[i]);

  while (pos--)
    put_char(io, buf[pos]);

  pad(io, (flags & FL_MINUS), ' ', width, total_len);
}

/**
 * @brief Extracts a signed or unsigned integer argument based on rank and
 * flags.
 */
static void get_arg_int(va_list *ap, int rank, unsigned flags,
                        unsigned long long *out) {
  if (flags & FL_SIGNED) {
    switch (rank) {
    case RANK_CHAR:
      *out = (signed char)va_arg(*ap, int);
      break;
    case RANK_SHORT:
      *out = (short)va_arg(*ap, int);
      break;
    case RANK_INT:
      *out = va_arg(*ap, int);
      break;
    case RANK_LONG:
      *out = va_arg(*ap, long);
      break;
    default:
      *out = va_arg(*ap, long long);
      break;
    }
  } else {
    switch (rank) {
    case RANK_CHAR:
      *out = (unsigned char)va_arg(*ap, unsigned int);
      break;
    case RANK_SHORT:
      *out = (unsigned short)va_arg(*ap, unsigned int);
      break;
    case RANK_INT:
      *out = va_arg(*ap, unsigned int);
      break;
    case RANK_LONG:
      *out = va_arg(*ap, unsigned long);
      break;
    default:
      *out = va_arg(*ap, unsigned long long);
      break;
    }
  }
}

/**
 * @brief Formats a string and writes it to the buffer.
 *
 * Handles width, alignment, and precision.
 */
static void format_string(_IO_BUFFER io, const char *s, unsigned flags,
                          int width, int precision) {
  int len = s ? strlen(s) : 6;

  if (!s)
    s = "(null)";

  if (precision >= 0 && len > precision)
    len = precision;

  pad(io, !(flags & FL_MINUS), ' ', width, len);
  __iob_write(io, s, len);
  iob_count += len;
  pad(io, (flags & FL_MINUS), ' ', width, len);
}

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
int iob_vsnprintf(_IO_BUFFER io, const char *fmt, va_list ap_src) {
  va_list ap;
  iob_count = 0;
  va_copy(ap, ap_src);

  unsigned flags = 0;
  int width = 0, precision = -1, rank = RANK_INT;
  const char *p = fmt;

  while (*p) {
    if (*p != '%') {
      put_char(io, *p++);
      continue;
    }

    p++; // Skip '%'

    // Reset specifiers
    flags = 0;
    width = 0;
    precision = -1;
    rank = RANK_INT;

    // Parse flags
    while (*p == '-' || *p == '+' || *p == ' ' || *p == '0' || *p == '#') {
      if (*p == '-')
        flags |= FL_MINUS;
      if (*p == '+')
        flags |= FL_PLUS;
      if (*p == ' ')
        flags |= FL_SPACE;
      if (*p == '0')
        flags |= FL_ZERO;
      if (*p == '#')
        flags |= FL_HASH;
      p++;
    }

    // Parse width
    if (*p == '*') {
      width = va_arg(ap, int);
      if (width < 0) {
        width = -width;
        flags |= FL_MINUS;
      }
      p++;
    } else {
      while (*p >= '0' && *p <= '9')
        width = width * 10 + (*p++ - '0');
    }

    // Parse precision
    if (*p == '.') {
      p++;
      precision = 0;
      if (*p == '*') {
        precision = va_arg(ap, int);
        if (precision < 0)
          precision = -1;
        p++;
      } else {
        while (*p >= '0' && *p <= '9')
          precision = precision * 10 + (*p++ - '0');
      }
    }

    // Parse length
    while (*p == 'h' || *p == 'l') {
      if (*p == 'h' && rank > RANK_CHAR)
        rank--;
      if (*p == 'l' && rank < RANK_LLONG)
        rank++;
      p++;
    }

    // Parse type
    switch (*p++) {
    case 'd':
    case 'i':
      flags |= FL_SIGNED;
    case 'u': {
      unsigned long long val;
      get_arg_int(&ap, rank, flags, &val);
      format_integer(io, val, flags, 10, width, precision);
      break;
    }
    case 'o': {
      unsigned long long val;
      get_arg_int(&ap, rank, flags, &val);
      format_integer(io, val, flags, 8, width, precision);
      break;
    }
    case 'X':
      flags |= FL_UPPER;
    case 'x': {
      unsigned long long val;
      get_arg_int(&ap, rank, flags, &val);
      format_integer(io, val, flags, 16, width, precision);
      break;
    }
    case 'P':
      flags |= FL_UPPER;
    case 'p':
      flags |= FL_HASH;
      format_integer(io, (unsigned long long)va_arg(ap, void *), flags, 16,
                     width, sizeof(void *) * 2);
      break;
    case 'c': {
      char c = (char)va_arg(ap, int);
      format_string(io, &c, flags, width, 1);
      break;
    }
    case 's':
      format_string(io, va_arg(ap, const char *), flags, width, precision);
      break;
    case '%':
      put_char(io, '%');
      break;
    default:
      // Unsupported format specifier
      return -1;
    }
  }

  va_end(ap);

  return iob_count;
}
