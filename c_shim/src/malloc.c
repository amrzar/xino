/**
 * @file heap_alloc.c
 * @brief Minimal free-list memory allocator with alignment and realloc support.
 *
 * This file implements a simple best-fit free-list allocator that grows
 * on-demand using an external page allocator:
 *
 *   void *alloc_page(unsigned order);
 *   void  free_page(void *va, unsigned order);
 *
 * Each allocated "segment" consists of `2 ^ order` pages. The allocator places
 * a small segment header at the beginning of each segment, then creates an
 * initial free block that covers the rest of the segment.
 *
 * Freed blocks are coalesced only within their owning segment. If a segment
 * becomes entirely free again (i.e., one free block spanning the whole
 * segment payload), the segment is returned to the page allocator.
 *
 * The allocator supports `malloc`, `free`, `aligned_alloc`, and `realloc`.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <config.h> // for UKERNEL_PAGE_SIZE.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>    // for memcpy.
#include <sys/queue.h> // for LIST_ENTRY, LIST_HEAD, etc.

#define C_SHIM_PAGE_SIZE UKERNEL_PAGE_SIZE

void *alloc_page(unsigned order) __attribute__((weak));
void free_page(void *va, unsigned order) __attribute__((weak));

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

struct segment_header;

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
  bool is_aligned; /**< Whether the block was allocated via `aligned_alloc`. */
  struct segment_header *seg; /**< Owning segment. */
  union {
    char *end;  /**< End of the usable block memory. */
    char *orig; /**< Original pointer for aligned allocations. */
  };
};

/**
 * @struct segment_header
 * @brief Header stored at the start of each page-allocated segment.
 *
 * Lives inside the segment memory itself (no extra allocations).
 */
struct segment_header {
#define SEGMENT_HEADER_SIZE sizeof(struct segment_header)

  LIST_ENTRY(segment_header) node;

  unsigned order;
  char *base;
  char *end;

  struct block_header *first; /**< First block header in this segment. */
};

/**
 * @brief Segment list tracking available segments.
 */
static LIST_HEAD(segment_list_head, segment_header)
    heap_segments = LIST_HEAD_INITIALIZER(&heap_segments);

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

/* Page < -- > Bytes. */

static inline size_t order_to_bytes(unsigned order) {
  return C_SHIM_PAGE_SIZE << order;
}

static inline unsigned pages_for_bytes(size_t bytes) {
  return ((bytes + (C_SHIM_PAGE_SIZE - 1)) / C_SHIM_PAGE_SIZE);
}

static inline unsigned order_for_pages(unsigned pages) {
  unsigned order = 0;
  unsigned n = 1;

  while (n < pages) {
    n <<= 1;
    order++;
  }

  return order;
}

static inline unsigned order_for_bytes(size_t bytes) {
  return order_for_pages((bytes == 0) ? 1U : pages_for_bytes(bytes));
}

/* SEGMENTS. */

static struct block_header *insert_free_block_sorted(struct block_header *blk) {
  struct block_header *i, *prev = NULL;

  LIST_FOREACH(i, &free_block_list, node) {
    if (i > blk) {
      LIST_INSERT_BEFORE(i, blk, node);

      return prev;
    }
    prev = i;
  }

  if (prev)
    LIST_INSERT_AFTER(prev, blk, node);
  else
    LIST_INSERT_HEAD(&free_block_list, blk, node);

  return prev;
}

/**
 * @brief Create a new heap segment and expose it as a free block.
 *
 * @return Pointer to the initialised segment_header on success,
 *         or NULL on failure.
 */
static struct segment_header *create_segment(unsigned order) {
  char *base, *end, *blk_start;
  struct segment_header *seg;
  struct block_header *blk;
  void *va;

  va = alloc_page(order);
  if (!va)
    return NULL;

  base = (char *)va;
  end = base + order_to_bytes(order);

  /* Place the segment header at the base of the segment. */
  seg = (struct segment_header *)base;
  seg->order = order;
  seg->base = base;
  seg->end = end;
  seg->first = NULL;

  blk_start = (char *)ALIGN_PTR(seg + 1, BLOCK_HEADER_ALIGNMENT);
  if (blk_start + BLOCK_HEADER_SIZE > end) {
    /* Segment too small to hold metadata + at least a header. */
    free_page(va, order);

    return NULL;
  }

  blk = (struct block_header *)blk_start;
  blk->is_aligned = false;
  blk->seg = seg;
  blk->end = end;

  seg->first = blk;

  /* Track segment and expose its first free block. */
  LIST_INSERT_HEAD(&heap_segments, seg, node);

  insert_free_block_sorted(blk);

  return seg;
}

static void maybe_release_segment(struct block_header *blk) {
  struct segment_header *seg = blk->seg;

  /* Segment is fully free?! */
  if (blk == seg->first && blk->end == seg->end) {
    LIST_REMOVE(blk, node);
    LIST_REMOVE(seg, node);
    /* Return pages to the underlying page allocator. */
    free_page((void *)seg->base, seg->order);
  }
}

/* BLOCKS. */

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

retry_again:
  LIST_FOREACH(i, &free_block_list, node) {
    size_t avail = BLOCK_SIZE(i);

    if (avail >= size && (!best || avail < BLOCK_SIZE(best)))
      best = i;
  }

  if (!best) {
    const size_t need =
        SEGMENT_HEADER_SIZE + BLOCK_HEADER_SIZE + size + BLOCK_MIN_SPLIT_SIZE;
    // Re-fill.
    if (!create_segment(order_for_bytes(need)))
      return NULL;

    best = NULL;

    goto retry_again;
  }

  cptr = (char *)(best + 1);

  if (cptr + size + BLOCK_MIN_SPLIT_SIZE <= best->end) {
    char *t = cptr + size;

    // init the tail block
    tail = (struct block_header *)t;
    tail->is_aligned = false;
    tail->seg = best->seg;
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

  if (!align || (align & (align - 1)) || (size & (align - 1)))
    return NULL;

  if (align <= BLOCK_HEADER_ALIGNMENT)
    return malloc(size);

  cptr = malloc(size + align + BLOCK_HEADER_SIZE);
  if (!cptr)
    return NULL;

  aligned_cptr = ALIGN_PTR(cptr + BLOCK_HEADER_SIZE, align);
  fake = (struct block_header *)(aligned_cptr - BLOCK_HEADER_SIZE);
  fake->is_aligned = true;
  fake->seg = PTR_TO_BLOCK(cptr)->seg;
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
  struct block_header *blk, *prev = NULL, *next;

  if (!ptr)
    return;

  blk = PTR_TO_BLOCK(ptr);
  if (blk->is_aligned)
    blk = PTR_TO_BLOCK(blk->orig);

  prev = insert_free_block_sorted(blk);

  next = LIST_NEXT(blk, node);

  if (prev && prev->seg == blk->seg && prev->end == (char *)blk) {
    prev->end = blk->end;
    LIST_REMOVE(blk, node);
    blk = prev;
  }

  if (next && next->seg == blk->seg && blk->end == (char *)next) {
    blk->end = next->end;
    LIST_REMOVE(next, node);
  }

  maybe_release_segment(blk);
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
  size_t real_size;
  void *new_ptr;

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
    blk = PTR_TO_BLOCK(blk->orig);
    real_size = (size_t)(blk->end - (char *)ptr);
  } else {
    real_size = BLOCK_SIZE(blk);
  }

  memcpy(new_ptr, ptr, (real_size < size) ? real_size : size);
  free(ptr);

  return new_ptr;
}

/* Default allocator. */

void *alloc_page(unsigned order) { return NULL; }

void free_page(void *va, unsigned order) {}
