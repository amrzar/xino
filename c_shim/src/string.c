/**
 * @file string.c
 * @brief Minimal implementations of common C standard library functions.
 *
 * This file provides a lightweight shim layer that implements basic
 * string and memory manipulation functions using compiler built-ins.
 *
 * @note All functions rely on `__builtin_*` intrinsics, which are
 *       typically provided by Clang and GCC.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <stddef.h>

size_t strlen(const char *s) { return __builtin_strlen(s); }

int strcmp(const char *a, const char *b) { return __builtin_strcmp(a, b); }

int strncmp(const char *a, const char *b, size_t n) {
  return __builtin_strncmp(a, b, n);
}

void *memchr(const void *s, int c, size_t n) {
  return __builtin_memchr(s, c, n);
}

void *memcpy(void *dst, const void *src, size_t n) {
  return __builtin_memcpy(dst, src, n);
}

void *memset(void *dst, int c, size_t n) { return __builtin_memset(dst, c, n); }

void *memmove(void *dst, const void *src, size_t n) {
  return __builtin_memmove(dst, src, n);
}

int memcmp(const void *a, const void *b, size_t n) {
  return __builtin_memcmp(a, b, n);
}
