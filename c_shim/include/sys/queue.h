/**
 * @file queue.h
 * @brief Doubly-linked list implementation.
 *
 * PARTIALLY COPIED FROM GLIBC ''sys/queue.h''.
 *
 * '' A list is headed by a single forward pointer (or an array of forward
 *    pointers for a hash table header). The elements are doubly linked so
 *    that an arbitrary element can be removed without a need to traverse
 *    the list. New elements can be added to the list before or after an
 *    existing element or at the head of the list. A list may only be
 *    traversed in the forward direction. ''
 */

#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#define LIST_HEAD(name, type)                                                  \
  struct name {                                                                \
    struct type *lh_first; /**< Pointer to the first element. */               \
  }

#define LIST_HEAD_INITIALIZER(head) {NULL}

#define LIST_ENTRY(type)                                                       \
  struct {                                                                     \
    struct type *le_next;  /**< Next element in the list. */                   \
    struct type **le_prev; /**< Address of previous element's next pointer. */ \
  }

#define LIST_INIT(head)                                                        \
  do {                                                                         \
    (head)->lh_first = NULL;                                                   \
  } while (0)

#define LIST_INSERT_AFTER(listelm, elm, field)                                 \
  do {                                                                         \
    if (((elm)->field.le_next = (listelm)->field.le_next) != NULL)             \
      (listelm)->field.le_next->field.le_prev = &(elm)->field.le_next;         \
    (listelm)->field.le_next = (elm);                                          \
    (elm)->field.le_prev = &(listelm)->field.le_next;                          \
  } while (0)

#define LIST_INSERT_BEFORE(listelm, elm, field)                                \
  do {                                                                         \
    (elm)->field.le_prev = (listelm)->field.le_prev;                           \
    (elm)->field.le_next = (listelm);                                          \
    *(listelm)->field.le_prev = (elm);                                         \
    (listelm)->field.le_prev = &(elm)->field.le_next;                          \
  } while (0)

#define LIST_INSERT_HEAD(head, elm, field)                                     \
  do {                                                                         \
    if (((elm)->field.le_next = (head)->lh_first) != NULL)                     \
      (head)->lh_first->field.le_prev = &(elm)->field.le_next;                 \
    (head)->lh_first = (elm);                                                  \
    (elm)->field.le_prev = &(head)->lh_first;                                  \
  } while (0)

#define LIST_REMOVE(elm, field)                                                \
  do {                                                                         \
    if ((elm)->field.le_next != NULL)                                          \
      (elm)->field.le_next->field.le_prev = (elm)->field.le_prev;              \
    *(elm)->field.le_prev = (elm)->field.le_next;                              \
  } while (0)

#define LIST_FOREACH(var, head, field)                                         \
  for ((var) = ((head)->lh_first); (var); (var) = ((var)->field.le_next))

#define LIST_EMPTY(head) ((head)->lh_first == NULL)
#define LIST_FIRST(head) ((head)->lh_first)
#define LIST_NEXT(elm, field) ((elm)->field.le_next)

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */
