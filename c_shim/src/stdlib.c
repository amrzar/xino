/**
 * @file stdlib.c
 * @brief Minimal environment and assertion support for freestanding
 * environments.
 *
 * This file provides a basic implementation of `getenv`, `abort`, and
 * `__assert_fail`, typically useful in freestanding or kernel-mode runtimes
 * where full libc is unavailable. Environment variables are simulated through
 * a static array, and `abort()` halts execution via a low-power wait loop.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <stddef.h>
#include <string.h> // for strlen and strncmp

/**
 * @brief Static environment variable list.
 *
 * A null-terminated array of strings representing the environment variables.
 * Add enviornment variable as needed.
 */
static const char *environ[] = {NULL};

/**
 * @brief Retrieves the value of an environment variable.
 *
 * Searches the simulated environment variable list for the given name and
 * returns a pointer to its value if found.
 *
 * @param name The name of the environment variable to search for.
 * @return Pointer to the value string, or NULL if the variable is not found.
 */
char *getenv(const char *name) {
  int len = strlen(name);
  char **p, *q;

  for (p = (char **)environ; (q = *p); p++) {
    if (!strncmp(name, q, len) && q[len] == '=') {
      return q + (len + 1);
    }
  }

  return NULL;
}

/**
 * @brief Halts the program indefinitely.
 *
 * Used as a last-resort failure mechanism. This implementation traps the
 * program in a low-power wait-for-event (`wfe`) instruction loop.
 *
 * @note This function never returns.
 */
__attribute__((noreturn)) void abort(void) {
  while (1)
    asm volatile("wfe");
}

/**
 * @brief Called when an assertion fails.
 *
 * This function is invoked by standard assert macros when an assertion
 * fails. It currently discards all debug information and simply calls
 * `abort()`.
 *
 * @param assertion The expression that failed.
 * @param file The file in which the assertion failed.
 * @param line The line number of the assertion.
 * @param function The function name containing the assertion.
 *
 * @note This function never returns.
 */
__attribute__((noreturn)) void __assert_fail(const char *assertion,
                                             const char *file,
                                             unsigned int line,
                                             const char *function) {
  (void)assertion;
  (void)file;
  (void)line;
  (void)function;

  abort();
}
