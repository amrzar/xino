/**
 * @file heap_alloc.c
 * @brief Minimal free-list memory allocator with alignment and realloc support.
 *
 * This file implements a simple best-fit free-list allocator for a statically
 * defined heap. It supports `malloc`, `free`, `aligned_alloc`, and `realloc`.
 * Memory blocks are managed via `struct block_header` and tracked in a
 * doubly-linked list using BSD-style `sys/queue.h` macros.
 *
 * The allocator performs block splitting and coalescing to reduce
 * fragmentation. Alignment-aware allocations are supported with metadata stored
 * in padded headers.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <config.h> // for C_SHIM_HEAP_SIZE.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>    // for memcpy.
#include <sys/queue.h> // for LIST_ENTRY, LIST_HEAD, etc.

#define ALIGN_UP_MASK(x, mask) (((x) + (mask)) & ~(mask))

/**
 * @def ALIGN_UP
 * @brief Aligns @p x to the nearest multiple of @p a.
 */
#define ALIGN_UP(x, a) ALIGN_UP_MASK(x, (__typeof__(x))(a) - 1)

/**
 * @def ALIGN_PTR
 * @brief Aligns a pointer @p p to the specified alignment @p a.
 */
#define ALIGN_PTR(p, a) ((__typeof__(p))ALIGN_UP((uintptr_t)(p), (a)))

/**
 * @struct block_header
 * @brief Represents a metadata header for each memory allocation block.
 *
 * Each allocated or free block is preceded by a `block_header`, storing
 * its end address and alignment information.
 */
struct block_header {
#define BLOCK_HEADER_SIZE sizeof(struct block_header)
#define BLOCK_HEADER_ALIGNMENT __alignof__(struct block_header)

/**
 * @def BLOCK_SIZE
 * @brief Calculates usable size of a memory block.
 */
#define BLOCK_SIZE(b) (size_t)((b)->end - ((char *)((b) + 1)))

  LIST_ENTRY(block_header) node; /**< Linked list node for the free list. */
  bool is_aligned; /**< Whether the block was allocated via aligned_alloc. */
  union {
    char *end;  /**< End of the usable block memory. */
    char *orig; /**< Original pointer for aligned allocations. */
  };
};

/**
 * @brief Static memory region serving as the heap.
 */
static char xino_heap[C_SHIM_HEAP_SIZE]
    __attribute__((aligned(BLOCK_HEADER_ALIGNMENT)))
    __attribute__((section(".bss")));

/**
 * @brief Free block list tracking available memory regions.
 */
static LIST_HEAD(block_list_head, block_header)
    free_block_list = LIST_HEAD_INITIALIZER(&free_block_list);

/**
 * @def BLOCK_MIN_SPLIT_SIZE
 * @brief Minimum size needed to split a block into two.
 */
#define BLOCK_MIN_SPLIT_SIZE (BLOCK_HEADER_SIZE + 32U)

/**
 * @def PTR_TO_BLOCK
 * @brief Converts a pointer returned to the user back to its block header.
 */
#define PTR_TO_BLOCK(p)                                                        \
  ((struct block_header *)((uintptr_t)(p) - BLOCK_HEADER_SIZE))

/**
 * @brief Initializes the allocator with a single free block.
 *
 * Called automatically via the constructor attribute.
 */
__attribute__((constructor)) static void init_heap() {
  struct block_header *xh_b = (struct block_header *)xino_heap;

  LIST_INSERT_HEAD(&free_block_list, xh_b, node);
  xh_b->end = xino_heap + C_SHIM_HEAP_SIZE;
  xh_b->is_aligned = false;
}

/**
 * @brief Allocates memory of the given size.
 *
 * Searches the free list for a best-fit block, optionally splitting it.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure.
 */
void *malloc(size_t size) {
  struct block_header *i, *best = NULL, *tail;
  char *cptr;

  if (!size)
    return NULL;

  size = ALIGN_UP(size, BLOCK_HEADER_ALIGNMENT);
  if (size > C_SHIM_HEAP_SIZE)
    return NULL;

  LIST_FOREACH(i, &free_block_list, node) {
    size_t avail = BLOCK_SIZE(i);

    if (avail >= size && (!best || avail < BLOCK_SIZE(best)))
      best = i;
  }

  if (!best)
    return NULL;

  cptr = (char *)(best + 1);

  if (cptr + size + BLOCK_MIN_SPLIT_SIZE <= best->end) {
    char *t = cptr + size;

    // init the tail block
    tail = (struct block_header *)t;
    tail->is_aligned = false;
    tail->end = best->end;
    LIST_INSERT_AFTER(best, tail, node);

    best->end = t;
  }

  LIST_REMOVE(best, node);

  return cptr;
}

/**
 * @brief Allocates aligned memory.
 *
 * Returns a pointer aligned to @p align. Over-allocates memory and adjusts
 * the return pointer to the required alignment, storing original pointer
 * metadata.
 *
 * @param align Desired alignment (must be a power of two).
 * @param size Number of bytes to allocate (must be a multiple of align).
 * @return Aligned memory pointer or NULL on failure.
 */
void *aligned_alloc(size_t align, size_t size) {
  char *aligned_cptr, *cptr;
  struct block_header *fake;

  if ((align & (align - 1)) || (size & (align - 1)))
    return NULL;

  if (align <= BLOCK_HEADER_ALIGNMENT)
    return malloc(size);

  cptr = malloc(size + align + BLOCK_HEADER_SIZE);
  if (!cptr)
    return NULL;

  aligned_cptr = ALIGN_PTR(cptr + BLOCK_HEADER_SIZE, align);
  fake = (struct block_header *)(aligned_cptr - BLOCK_HEADER_SIZE);
  fake->is_aligned = true;
  fake->orig = cptr;

  return aligned_cptr;
}

/**
 * @brief Frees memory previously allocated via malloc or aligned_alloc.
 *
 * Reinserts the block into the free list and attempts to coalesce
 * adjacent blocks to reduce fragmentation.
 *
 * @param ptr Pointer to memory to free.
 */
void free(void *ptr) {
  struct block_header *i, *blk, *prev = NULL, *next;

  if (!ptr)
    return;

  blk = PTR_TO_BLOCK(ptr);
  if (blk->is_aligned) {
    ptr = blk->orig;
    blk = PTR_TO_BLOCK(ptr);
  }

  LIST_FOREACH(i, &free_block_list, node) {
    if (i > blk) {
      LIST_INSERT_BEFORE(i, blk, node);
      goto do_coalesce;
    }
    prev = i;
  }

  if (prev)
    LIST_INSERT_AFTER(prev, blk, node);
  else
    LIST_INSERT_HEAD(&free_block_list, blk, node);

do_coalesce:
  next = LIST_NEXT(blk, node);

  if (prev && prev->end == (char *)blk) {
    prev->end = blk->end;
    LIST_REMOVE(blk, node);
    blk = prev;
  }

  if (next && blk->end == (char *)next) {
    blk->end = next->end;
    LIST_REMOVE(next, node);
  }
}

/**
 * @brief Reallocates a memory block to a new size.
 *
 * Allocates new memory, copies the contents from the old block,
 * and then frees the old block. Alignment metadata is respected but the
 * return memory may not be aligned if originally allocated via aligned_alloc.
 *
 * @param ptr Pointer to the previously allocated memory.
 * @param size New size in bytes.
 * @return Pointer to newly allocated memory, or NULL on failure.
 */
void *realloc(void *ptr, size_t size) {
  struct block_header *blk;
  void *orig_ptr, *new_ptr;
  size_t real_size;

  if (!ptr)
    return malloc(size);

  if (!size) {
    free(ptr);
    return NULL;
  }

  new_ptr = malloc(size);
  if (!new_ptr)
    return NULL;

  blk = PTR_TO_BLOCK(ptr);
  if (blk->is_aligned) {
    orig_ptr = blk->orig;
    blk = PTR_TO_BLOCK(orig_ptr);
    real_size = (size_t)(blk->end - (uintptr_t)ptr);
  } else {
    orig_ptr = ptr;
    real_size = BLOCK_SIZE(blk);
  }

  memcpy(new_ptr, ptr, (real_size < size) ? real_size : size);
  free(ptr);

  return new_ptr;
}
