/**
 * @file ctype.c
 * @brief Minimal <ctype.h>-like predicates for the ASCII-only "C" locale.
 *
 * This module implements the common `is*` classification functions using a
 * compact flag table generated on the fly by `__ctype_flags()`. Behavior
 * matches the ISO C "C" locale for 7-bit ASCII and treats bytes >= 0x80 as
 * non-classified (all predicates false). Control characters include
 * `'\t'`, `'\n'`, `'\v'`, `'\f'`, and `'\r'` which are also recognized as
 * whitespace.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <stdint.h>

/**
 * @name Classification flags
 * @brief Bit flags returned by `__ctype_flags` to describe character classes.
 * @{
 */
#define UPPER 0x01  /**< Uppercase letter 'A'..'Z'. */
#define LOWER 0x02  /**< Lowercase letter 'a'..'z'. */
#define DIGIT 0x04  /**< Decimal digit '0'..'9'. */
#define XDIGIT 0x08 /**< Hex digit '0'..'9','A'..'F','a'..'f'. */
#define SPACE 0x10  /**< Whitespace: ' ', '\t', '\n', '\v', '\f', '\r'. */
#define PRINT 0x20  /**< Printable (ASCII 0x20..0x7E). */
#define PUNCT 0x40  /**< Printable punctuation or symbols. */
#define CNTRL 0x80  /**< Control character (ASCII <= 0x1F or 0x7F). */
/** @} */

/* ASCII-only "C" locale. */
static inline uint8_t __ctype_flags(unsigned char c) {
  // ASCII control chars + DEL.
  if (c <= 0x1F || c == 0x7F) {
    // ASCII whitespace among controls (not printable).
    if (c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r')
      return CNTRL | SPACE;
    return CNTRL;
  }

  // Non-ASCII bytes: no classification in "C" locale.
  if (c >= 0x80)
    return 0;

  // Printable ASCII range 0x20..0x7E.
  uint8_t m = PRINT;

  // Space (printable).
  if (c == ' ')
    return m | SPACE;
  // Digits (also hex).
  if (c >= '0' && c <= '9')
    return m | DIGIT | XDIGIT;
  // Uppercase letters.
  if (c >= 'A' && c <= 'Z')
    return m | UPPER | (c <= 'F' ? XDIGIT : 0);
  // Lowercase letters.
  if (c >= 'a' && c <= 'z')
    return m | LOWER | (c <= 'f' ? XDIGIT : 0);
  // Remaining are punctuation or symbols.
  return m | PUNCT;
}

#define IS_OF_TYPE(c, type) (__ctype_flags((unsigned char)(c)) & (type))

int isupper(int c) { return IS_OF_TYPE(c, UPPER); }

int islower(int c) { return IS_OF_TYPE(c, LOWER); }

int isdigit(int c) { return IS_OF_TYPE(c, DIGIT); }

int isspace(int c) { return IS_OF_TYPE(c, SPACE); }

int isprint(int c) { return IS_OF_TYPE(c, PRINT); }

int ispunct(int c) { return IS_OF_TYPE(c, PUNCT); }

int iscntrl(int c) { return IS_OF_TYPE(c, CNTRL); }

int isalnum(int c) { return IS_OF_TYPE(c, UPPER | LOWER | DIGIT); }

int isalpha(int c) { return IS_OF_TYPE(c, UPPER | LOWER); }

int isgraph(int c) { return IS_OF_TYPE(c, UPPER | LOWER | DIGIT | PUNCT); }

int isxdigit(int c) { return IS_OF_TYPE(c, XDIGIT); }
