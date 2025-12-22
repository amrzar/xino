
#ifndef __ALLOCATOR_HPP__
#define __ALLOCATOR_HPP__

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mm.hpp>
#include <mm_va_layout.hpp>
#include <optional>
#include <stdexcept>

namespace xino {

struct nothrow_t {
  explicit constexpr nothrow_t() = default;
};

constexpr nothrow_t nothrow{};

/* ALLOCATORS. */

namespace allocator {

template <unsigned Order> class buddy {
public:
  // No empty allocator.
  buddy() = delete;

  // Non-throwing init (use is_ok() to check state.).
  explicit buddy(const xino::nothrow_t &, xino::mm::phys_addr pa,
                 std::size_t size) noexcept {
    buddy_init(pa, size);
  }

  // Throwing init.
  explicit buddy(xino::mm::phys_addr pa, std::size_t size) {
    if (!buddy_init(pa, size))
      throw std::invalid_argument{"buddy_init"};
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
   * @throws std::runtime_error If the underlying buddy allocator cannot satisfy
   *         the request. The exception message is @c "buddy_alloc_pages".
   */
  [[nodiscard]] xino::mm::phys_addr alloc_pages(unsigned order) {
    const auto pa = buddy_alloc_pages(order);
    if (pa.has_value())
      return pa.value();

    throw std::runtime_error{"buddy_alloc_pages"};
  }

  /**
   * @brief Allocate one or more physically-contiguous pages without throwing.
   *
   * @param nothrow Tag selecting the non-throwing overload.
   * @param order Allocation order (base-2 exponent).
   * @return Physical base address of the allocated page block on success;
   *         otherwise @c xino::mm::phys_addr{0}.
   */
  [[nodiscard]] xino::mm::phys_addr alloc_pages(const xino::nothrow_t &,
                                                unsigned order) noexcept {
    const auto pa = buddy_alloc_pages(order);
    if (pa.has_value())
      return pa.value();

    return xino::mm::phys_addr{0};
  }

  void free_pages(xino::mm::phys_addr pa, unsigned order) noexcept {
    buddy_free_pages(pa, order);
  }
  ///@}

  /**
   * @brief Convert a block size expressed in pages to an order (log2).
   *
   * @param pages Block size in pages.
   * @return The order corresponding to @p pages.
   *
   * @note If @p pages is not a power of two, the result is effectively
   *       `floor(log2(pages))`, not a rounding-up order.
   */
  [[nodiscard]] static constexpr unsigned
  pages_to_order(std::size_t pages) noexcept {
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
  [[nodiscard]] static constexpr std::size_t
  order_to_pages(unsigned order) noexcept {
    return std::size_t{1} << order;
  }

private:
  [[nodiscard]] bool buddy_init(xino::mm::phys_addr pa, std::size_t size) {
    xino::mm::phys_addr end{pa + size};
    // Check for overflow.
    if (end < pa)
      return false;

    end = end.align_down(page_size);

    xino::mm::phys_addr base{pa.align_up(page_size)};
    // Check if at least single page available.
    if (end <= base)
      return false;

    // Real pool size (page-aligned).
    size = static_cast<xino::mm::phys_addr::value_type>(end) -
           static_cast<xino::mm::phys_addr::value_type>(base);

    std::size_t pages = size / page_size;
    // The allocator represents at most `2^Order` pages.
    if (pages > order_to_pages(Order))
      return false;

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

    return true;
  }

  /**
   * @brief Allocate a buddy block of size @c 2^order pages.
   *
   * @param order Buddy order in pages (0 => 1 page, 1 => 2 pages, ...).
   * @return Physical address, or @c std::nullopt on failure.
   */
  [[nodiscard]] std::optional<xino::mm::phys_addr>
  buddy_alloc_pages(unsigned order) {
    if (!is_ok() || order > max_ord)
      return std::nullopt;

    // Find smallest order >= requested with any free node.
    unsigned o = order;
    std::size_t node = 0;
    for (; o <= max_ord; o++) {
      node = find_free_node_at_order(o);
      if (node != 0)
        break;
    }

    // No free block found.
    if (node == 0)
      return std::nullopt;

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

  void buddy_free_pages(xino::mm::phys_addr pa, unsigned order) {
    if (!is_ok() || order > max_ord)
      return;

    // Check if pa is valid buddy address.
    if (!pa.is_align(page_size))
      return;

    // Check if pa is in range [base_pa, end_pa).
    if (pa < base_pa || pa > end_pa - order_to_pages(order) * page_size)
      return;

    // Offset and page index from base.
    const std::uintptr_t off_bytes{
        static_cast<xino::mm::phys_addr::value_type>(pa) -
        static_cast<xino::mm::phys_addr::value_type>(base_pa)};
    const std::size_t page_idx{static_cast<std::size_t>(off_bytes / page_size)};

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
      const std::size_t bud = node ^ 1; // Node buddy.
      if (!test_bit(free_bits, bud))
        break;

      clear_bit(free_bits, node); // Free this node.
      clear_bit(free_bits, bud);  // Free node buddy.

      node >>= 1;                  // Ascend to parent.
      clear_bit(split_bits, node); // Parent no longer split
      set_bit(free_bits, node);    // Parent becomes free

      o++;
    }
  }

  // Allocator state (buddy allocator using binary tree).

  // Nodes in N-level (`N = Order + 1`) binary tree is `2^N - 1`.
  // `node_count = 2^N`, nodes are `[1 .. 2^N - 1]`, node 0 is unused.
  static constexpr std::size_t node_count{std::size_t{1} << (Order + 1)};
  static constexpr std::size_t word_bits{
      std::numeric_limits<std::uint64_t>::digits};
  // Number of words in a bitmap for `node_count` nodes, bit 0 is unused.
  static constexpr std::size_t word_count{(node_count + word_bits - 1) /
                                          word_bits};

  xino::mm::phys_addr base_pa{}; // Start physical address (inclusive).
  xino::mm::phys_addr end_pa{};  // End physical address (exclusive)
  std::size_t pool_pages{};      // Number of pages in the pool.
  std::size_t page_size{xino::mm::va_layout::granule_size()};
  unsigned max_ord{};

  // Binary tree state.
  // Let's avoid std::bitwise<N> to control exception.
  std::uint64_t free_bits[word_count]{};
  std::uint64_t split_bits[word_count]{};

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
   *     free   |   free   | Description
   *  ----------------------------------------------------------------------
   *      1          0       Node is a free block available for
   *                         allocation at that order.
   *      0          1       Node is split; children represent the state.
   *      0          0       Node is allocated at that order.
   *      1          1       Invalid.
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

  /* Static helpers (all noexcept). */

  // Bitwise ops.

  [[nodiscard]] static std::size_t word_of_bit(std::size_t bit) noexcept {
    return bit / word_bits;
  }

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
      return (st_word * word_bits) + std::countr_zero(v);
    }

    // First word (likely case).
    {
      const std::uint64_t v = bits[st_word] & st_mask;
      if (v != 0)
        return (st_word * word_bits) + std::countr_zero(v);
    }

    // Middle words (tight loop).
    for (std::size_t w{st_word + 1}; w < lst_word; ++w) {
      const std::uint64_t v = bits[w];
      if (v != 0)
        return (w * word_bits) + std::countr_zero(v);
    }

    // Last word.
    {
      const std::uint64_t v = bits[lst_word] & lst_mask;
      if (v != 0)
        return (lst_word * word_bits) + std::countr_zero(v);
    }

    return 0;
  }
};

/* Add other allocator, if required. */
/* class alloc { XXX }; */

} // namespace allocator

} // namespace xino

#endif // __ALLOCATOR_HPP__