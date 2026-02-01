/**
 * @file allocator.hpp
 * @brief Minimal early-boot-safe allocators (buddy).
 *
 * This header provides a small buddy allocator intended to work in both early
 * boot and normal runtime.
 *
 * The uKernel performs PIE self-relocation very early (processing
 * `R_AARCH64_RELATIVE` entries) and may execute code before any C++ dynamic
 * initialization (`.init_array`) has run. In that stage, it is critical that:
 *
 *  - No dynamic initialization code is required just to instantiate allocator
 *    objects (i.e. no hidden constructor calls via `.init_array`).
 *  - The allocator state is brought up explicitly, at a time chosen by the
 *    boot flow.
 *
 * Therefore, the buddy allocator is designed so that:
 *
 *  - It is safe to place in zero-initialized storage (`.bss`) at static
 *    storage duration (e.g. `constinit buddy<N> g{}`).
 *  - It performs all initialization explicitly in @ref buddy::init(), rather
 *    than relying on non-trivial constructors or member initializers.
 *
 * @author Amirreza Zarrabi
 * @date 2026
 */

#ifndef __ALLOCATOR_HPP__
#define __ALLOCATOR_HPP__

#include <cstddef>
#include <cstdint>
#include <errno.hpp>
#include <limits>
#include <mm.hpp>           // for phys_addr and phys_addr_range()
#include <mm_va_layout.hpp> // for granule_size()
#include <stdexcept>

namespace xino {

struct nothrow_t {
  explicit constexpr nothrow_t() = default;
};

constexpr nothrow_t nothrow{};

} // namespace xino

namespace xino::allocator {

/**
 * @brief Convert a block size expressed in pages to an order (log2).
 *
 * @param pages Block size in pages.
 * @return The order corresponding to @p pages.
 *
 * @note If @p pages is not a power of two, the result is effectively
 *       `floor(log2(pages))`, not a rounding-up order.
 */
[[nodiscard]] constexpr unsigned pages_to_order(std::size_t pages) noexcept {
  unsigned r = 0;
  while (pages > 1) {
    pages >>= 1;
    ++r;
  }
  return r;
}

/**
 * @brief Convert an order to a block size in pages.
 *
 * @param order Order to convert.
 * @return Block size in pages (`1U << order`).
 */
[[nodiscard]] constexpr std::size_t order_to_pages(unsigned order) noexcept {
  return std::size_t{1} << order;
}

/**
 * @brief Convert a size in bytes to an order.
 *
 * @param size Size in bytes.
 * @return The order corresponding to @p size.
 *
 * @note This conversion truncates: any remainder smaller than one page is
 *       discarded. If the resulting page count is not a power of two, the
 *       result is effectively `floor(log2(pages))`, not a rounding-up order.
 */
[[nodiscard]] constexpr unsigned size_to_order(std::size_t size) noexcept {
  return pages_to_order(size / xino::mm::va_layout::granule_size());
}

/* Binary tree buddy allocator.  */

template <unsigned Order> class buddy {
public:
  /**
   * @brief Initialize the buddy allocator over a physical memory interval.
   *
   * This function exists (instead of using a constructor) to keep the type
   * usable in early boot:
   *
   *  - In the uKernel boot flow, code may execute before any C++ dynamic
   *    initialization (`.init_array`) has run.
   *  - A non-trivial default constructor (or non-trivial member initializers)
   *    would force the compiler to emit dynamic initialization code, which is
   *    unsafe to rely on during early boot and PIE self-relocation.
   *
   * To avoid that, this class is intended to be *trivially default
   * constructible* (so a `constinit buddy<...> g{}` can live in `.bss` without
   * any generated init code), and all runtime setup is performed explicitly
   * here.
   *
   * What `init()` does:
   *  - Resets internal state to the "not initialized" state.
   *  - Clears internal bitmaps (`free_bits[]`/`split_bits[]`).
   *  - Calls the internal initializer to build the initial free structure.
   *
   * @param pa Physical start address of the candidate pool.
   * @param size Size (in bytes) of the candidate pool.
   *
   * @retval `xino::error_nr::ok` Success.
   * @retval `xino::error_nr::overflow` Address overflow.
   * @retval `xino::error_nr::invalid` The aligned region is empty or the pool
   *         contains more pages than the allocator can represent.
   *
   * @par Example boot allocator
   * @code
   * constinit xino::allocator::buddy<MAX_ORDER> boot_buddy{};
   * boot_buddy.init(pool_base, pool_size);
   * @endcode
   */
  [[nodiscard]] xino::error_t init(xino::mm::phys_addr pa,
                                   std::size_t size) noexcept {
    // Reset state to "not initialized".
    base_pa = xino::mm::phys_addr{0};
    end_pa = xino::mm::phys_addr{0};
    pool_pages = 0;
    max_ord = 0;

    // Init as (free = 0, split = 0), buddy_free_pages init bits in buddy_init.
    for (std::size_t i = 0; i < word_count; ++i) {
      free_bits[i] = 0;
      split_bits[i] = 0;
    }

    return buddy_init(pa, size);
  }

  [[nodiscard]] bool is_ok() const noexcept {
    // Check if there is at least single page in this allocator.
    return base_pa < end_pa;
  }

  /** @name Allocate and free. */
  ///@{
  /**
   * @brief Allocate one or more physically-contiguous pages.
   *
   * @param order Allocation order (base-2 exponent).
   * @return Physical base address of the allocated page block.
   * @throws `std::runtime_error` if the underlying buddy allocator failed.
   *         The exception message is `buddy_alloc_pages`.
   */
  [[nodiscard]] xino::mm::phys_addr alloc_pages(unsigned order) {
    const xino::mm::phys_addr pa{buddy_alloc_pages(order)};
    if (pa != xino::mm::phys_addr{0})
      return pa;

    throw std::runtime_error{"buddy_alloc_pages"};
  }

  /**
   * @brief Allocate one or more physically-contiguous pages without throwing.
   *
   * @param nothrow Tag selecting the non-throwing overload.
   * @param order Allocation order (base-2 exponent).
   * @return Physical base address of the allocated page block on success;
   *         otherwise `xino::mm::phys_addr{0}` (see @ref buddy_alloc_pages).
   */
  [[nodiscard]] xino::mm::phys_addr alloc_pages(const xino::nothrow_t &,
                                                unsigned order) noexcept {
    return buddy_alloc_pages(order);
  }

  void free_pages(xino::mm::phys_addr pa, unsigned order) noexcept {
    buddy_free_pages(pa, order);
  }
  ///@}

private:
  /**
   * @brief Initialize the buddy allocator pool over a physical address range.
   *
   * This function sets up the buddy allocator to manage a page-aligned subrange
   * of the provided physical memory interval. The effective managed range is
   * constructed as:
   *
   *   - `base = align_up(pa, page_size)`
   *   - `end  = align_down(pa + size, page_size)`
   *
   * and is treated as a half-open interval `[base, end)`.
   *
   * @param pa Starting physical address of the candidate pool.
   * @param size Size in bytes of the candidate pool.
   *
   * @retval `xino::error_nr::ok` Success.
   * @retval `xino::error_nr::overflow` Address overflow.
   * @retval `xino::error_nr::invalid` The aligned region is empty or the pool
   *         contains more pages than the allocator can represent.
   */
  [[nodiscard]] xino::error_t buddy_init(xino::mm::phys_addr pa,
                                         std::size_t size) {
    using av_t = xino::mm::phys_addr::value_type;

    // Check for overflow.
    if (pa + size < pa)
      return xino::error_nr::overflow;

    // Construct `[base, end)` as a page aligned range.
    xino::mm::phys_addr base{pa.align_up(page_size)};
    xino::mm::phys_addr end{(pa + size).align_down(page_size)};
    // Check if at least single page available.
    if (end <= base)
      return xino::error_nr::invalid;

    // Aligned pool size of `[base, end)`.
    size = static_cast<av_t>(end) - static_cast<av_t>(base);

    std::size_t pages = size / page_size;
    // The allocator represents at most `2^Order` pages.
    if (pages > order_to_pages(Order))
      return xino::error_nr::invalid;

    const unsigned odr = pages_to_order(pages);

    // Init the object:
    base_pa = base;
    end_pa = end;
    pool_pages = pages;
    max_ord = odr < Order ? odr : Order;

    // Free each page. The coalescing logic merges them into blocks, building
    // the initial free structure.
    for (xino::mm::phys_addr it :
         xino::mm::phys_addr_range(base, end, page_size))
      buddy_free_pages(it, 0);

    return xino::error_nr::ok;
  }

  /**
   * @brief Allocate a contiguous block of pages from the buddy allocator.
   *
   * @param order Buddy order of the block.
   * @return Physical address, or `xino::mm::phys_addr{0}` if the allocator is
   *         not initialized, @p order exceeds `max_ord`, or no suitable free
   *         block exists.
   */
  [[nodiscard]] xino::mm::phys_addr buddy_alloc_pages(unsigned order) {
    if (!is_ok() || order > max_ord)
      return xino::mm::phys_addr{0};

    // Find smallest order >= requested order with any free node.
    unsigned o = order;
    std::size_t node = 0;
    for (; o <= max_ord; o++) {
      node = find_free_node_at_order(o);
      if (node != 0)
        break;
    }

    // No free block found.
    if (node == 0)
      return xino::mm::phys_addr{0};

    // Consume that free node and split down.
    clear_bit(free_bits, node);
    while (o > order) {
      set_bit(split_bits, node);    // Parent becomes split.
      node <<= 1;                   // Descend to left child.
      set_bit(free_bits, node | 1); // Right child (`node | 1`) becomes free.
      o--;
    }

    // `o == order`, `node` represents the allocated block.
    return base_pa + (node_to_page_index(node, order) * page_size);
  }

  /**
   * @brief Free an allocated block of pages back to the buddy allocator.
   *
   * Frees contiguous pages starting at physical address @p pa and attempts to
   * coalesce the block with its buddy blocks while possible.
   *
   * @param pa Physical start address of the block to free.
   * @param order Buddy order of the block.
   */
  void buddy_free_pages(xino::mm::phys_addr pa, unsigned order) {
    using av_t = xino::mm::phys_addr::value_type;

    if (!is_ok() || order > max_ord)
      return;

    // Check if pa is valid buddy address.
    if (!pa.is_align(page_size))
      return;

    const std::size_t span = order_to_pages(order) * page_size;
    // Check if pa is in range `[base_pa, end_pa)`.
    if (pa < base_pa || pa > end_pa - span)
      return;

    // Page index from base.
    const std::uintptr_t off{static_cast<av_t>(pa) -
                             static_cast<av_t>(base_pa)};
    const std::size_t page_idx{static_cast<std::size_t>(off / page_size)};

    // page_index >> order is "block index".
    std::size_t node = node_number(order, page_idx >> order);

    // Split node can not freed.
    if (test_bit(split_bits, node))
      return;

    // Basic double-free guard.
    if (test_bit(free_bits, node))
      return;

    // Free the block.
    set_bit(free_bits, node);

    // Coalesce upward while buddy is free.
    unsigned o = order;
    while (o < max_ord) {
      const std::size_t node_bud = node ^ 1; // Node buddy.
      if (!test_bit(free_bits, node_bud))
        break;

      clear_bit(free_bits, node);     // Free this node.
      clear_bit(free_bits, node_bud); // Free node buddy.

      node >>= 1;                  // Ascend to parent.
      clear_bit(split_bits, node); // Parent no longer split
      set_bit(free_bits, node);    // Parent becomes free

      o++;
    }
  }

  /* Allocator state (buddy allocator using binary tree). */

  // Nodes in N-level (`N = Order + 1`) binary tree is `2^N - 1`.
  // `node_count = 2^N`, nodes are `[1 .. 2^N - 1]`, node 0 is unused.
  static constexpr std::size_t node_count{std::size_t{1} << (Order + 1)};
  static constexpr std::size_t word_bits{
      std::numeric_limits<std::uint64_t>::digits};
  // Number of words in a bitmap for `node_count` nodes, bit 0 is unused.
  static constexpr std::size_t word_count{(node_count + word_bits - 1) /
                                          word_bits};

  static constexpr std::size_t page_size{xino::mm::va_layout::granule_size()};

  // Pool state.
  xino::mm::phys_addr base_pa; // Start physical address (inclusive).
  xino::mm::phys_addr end_pa;  // End physical address (exclusive)
  std::size_t pool_pages;      // Number of pages in the pool.
  unsigned max_ord;            // Max order supported (maybe < `Order`).

  // Binary tree state.
  std::uint64_t free_bits[word_count];
  std::uint64_t split_bits[word_count];

  /**
   * Binary tree buddy description.
   *
   * A binary tree where each node corresponds to exactly one aligned block
   * in the pool. For instance for `Order = 3`, 8 pages, tree is like:
   *
   *                   node-1 (idx = 0)                      -- LVL 0, order 3
   *                /                    \
   *            n2(0)                     n3(1)              -- LVL 1, order 2
   *          /      \                  /       \
   *     n4(0)        n5(1)         n6(2)        n7(3)       -- LVL 2, order 1
   *    /    \       /    \        /    \       /    \
   *  n8(0) n9(1) n10(2) n11(3) n12(4) n13(5) n14(6) n15(7)  -- LVL 3, order 0
   *
   * Given an `order`, there are `2^level` blocks, where
   * `level = Order - order`, and blocks are indexed from `idx = 0`.
   *
   * Mapping between "the i-th block at order X" and "node number"
   *  - `node = 2^level + idx`
   *
   * Mapping between "the i-th block at order X" to "page index"
   *  - `page_idx = idx * 2^order`
   *
   * The binary tree is represented using two bitmaps `free_bits[word_count]`,
   * and `split_bits[word_count]`.
   *
   *  ----------------------------------------------------------------------
   *     free   |   split   |  Description
   *  ----------------------------------------------------------------------
   *      1           0        Node is a free block available for
   *                           allocation at that order.
   *      0           1        Node is split; children represent the state.
   *      0           0        Node is allocated at that order.
   *      1           1        Invalid.
   */

  // Block index -> Node number.
  // `level = Order - order`.
  // `node = 2^level + idx`.
  [[nodiscard]] std::size_t node_number(unsigned order,
                                        std::size_t idx) const noexcept {
    const unsigned level = Order - order;
    return (std::size_t{1} << level) + idx;
  }

  // `level = Order - order`.
  // `idx = node - 2^level`.
  // `page_idx = idx * 2^order`.
  [[nodiscard]] std::size_t node_to_page_index(std::size_t node,
                                               unsigned order) const noexcept {
    const unsigned level = Order - order;
    const std::size_t idx = node - (std::size_t{1} << level);
    return idx << order;
  }

  [[nodiscard]] std::size_t
  find_free_node_at_order(unsigned order) const noexcept {
    const unsigned level = Order - order;
    // `2^level` = first node number at `level`.
    const std::size_t first = (std::size_t{1} << level);
    // `2^(level + 1)` = first node number at next `level`.
    const std::size_t last = (std::size_t{1} << (level + 1));
    return find_set_in_range(free_bits, first, last);
  }

  /* Static bitops helpers (all noexcept). */

  // Index of the word in `std::uint64_t` array that contains a given bit.
  [[nodiscard]] static std::size_t word_of_bit(std::size_t bit) noexcept {
    return bit / word_bits;
  }

  // Position of a bit within its containing word in `std::uint64_t` array.
  [[nodiscard]] static unsigned bit_in_word(std::size_t bit) noexcept {
    return static_cast<unsigned>(bit & (word_bits - 1));
  }

  [[nodiscard]] static bool test_bit(const std::uint64_t *bits,
                                     std::size_t bit) noexcept {
    return !!(bits[word_of_bit(bit)] & (std::uint64_t{1} << bit_in_word(bit)));
  }

  static void set_bit(std::uint64_t *bits, std::size_t bit) noexcept {
    bits[word_of_bit(bit)] |= (std::uint64_t{1} << bit_in_word(bit));
  }

  static void clear_bit(std::uint64_t *bits, std::size_t bit) noexcept {
    bits[word_of_bit(bit)] &= ~(std::uint64_t{1} << bit_in_word(bit));
  }

  // Assume v != 0.
  // Use `__builtin_ctzll` instead of `counter_zero` to reduce dependency.
  [[nodiscard]] static unsigned ctz64(std::uint64_t v) noexcept {
    return static_cast<unsigned>(__builtin_ctzll(v));
  }

  /**
   * @brief Find the index of the first set bit within a half-open bit range.
   *
   * @param bits Pointer to an array of 64-bit words representing the bitset.
   * @param first First bit index in the range to search (inclusive).
   * @param last One past the last bit index in the range to search
   * (exclusive).
   *
   * @return The absolute bit index of the first set bit found in [`first`,
   * `last`), or 0 if no set bit exists in the range.
   */
  [[nodiscard]] static std::size_t
  find_set_in_range(const std::uint64_t *bits, std::size_t first,
                    std::size_t last) noexcept {
    std::size_t st_word, lst_word;

    // Check if range is empty.
    if (first >= last)
      return 0;

    st_word = word_of_bit(first);
    lst_word = word_of_bit(last - 1);

    // Masks for start and last words:
    //  - bits `[bit_in_word(first) .. 63]`.
    const std::uint64_t st_mask = ~std::uint64_t{0} << bit_in_word(first);
    //  - bits `[0 .. 63 - bit_in_word(last - 1)]`.
    const std::uint64_t lst_mask =
        ~std::uint64_t{0} >> (word_bits - 1 - bit_in_word(last - 1));

    // Single-word range (most likely case, Order <= 5, max 32 pages).
    if (st_word == lst_word) {
      const std::uint64_t v = bits[st_word] & st_mask & lst_mask;
      if (v == 0)
        return 0;
      return (st_word * word_bits) + ctz64(v);
    }

    // First word (likely case).
    {
      const std::uint64_t v = bits[st_word] & st_mask;
      if (v != 0)
        return (st_word * word_bits) + ctz64(v);
    }

    // Middle words (tight loop).
    for (std::size_t w{st_word + 1}; w < lst_word; ++w) {
      const std::uint64_t v = bits[w];
      if (v != 0)
        return (w * word_bits) + ctz64(v);
    }

    // Last word.
    {
      const std::uint64_t v = bits[lst_word] & lst_mask;
      if (v != 0)
        return (lst_word * word_bits) + ctz64(v);
    }

    return 0;
  }
};

} // namespace xino::allocator

#endif // __ALLOCATOR_HPP__
