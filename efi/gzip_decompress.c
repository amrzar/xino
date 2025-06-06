/**
 * @file gzip_decompress.c
 * @brief GZIP decompression (DEFLATE algorithm) for freestanding environments.
 *
 * This file implements a minimal, position-independent DEFLATE decompressor
 * with GZIP header support based on:
 * [RFC 1951](https://www.rfc-editor.org/rfc/rfc1951.txt) and
 * [RFC 1952](https://www.rfc-editor.org/rfc/rfc1952.txt).
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @def MAX_BITS
 * @brief Maximum bit length of any Huffman code in the DEFLATE specification.
 *
 * This macro defines the size of arrays that track the number of symbols per
 * code length (e.g., `bl_count[MAX_BITS]`) or compute symbol offsets for
 * canonical Huffman decoding.
 */
#define MAX_BITS 16

/**
 * @def MAX_SYMBOLS
 * @brief Maximum number of literal/length symbols in the DEFLATE alphabet.
 *
 * This macro defines the capacity of the `sorted_symbols[]` array
 * used for building Huffman trees for the literal/length alphabet.
 *
 * @note Only the first 286 are ever emitted in a DEFLATE block; 286 and 287
 *       are “dummy” entries that only serve to complete the canonical Huffman
 *       construction, not to represent actual symbol.
 *
 * @see rfc1951.txt (Section 3.2.5)
 */
#define MAX_SYMBOLS 288

/**
 * @def MAX_DISTANCE
 * @brief Maximum number of distance codes supported in DEFLATE.
 *
 * In the DEFLATE format (RFC 1951), distance codes specify how far
 * back to copy data from in the decompressed output stream. The maximum
 * number of distance codes that can appear in a dynamic Huffman block is 32.
 *
 * @note This value is not the maximum distance offset (which can be 32 KiB),
 *       but rather the number of distinct Huffman codes used to represent
 *       distances.
 *
 * @see rfc1951.txt (Section 3.2.7)
 */
#define MAX_DISTANCE 32

typedef uint16_t symbol_t;

/**
 * @struct tree
 * @brief Represents a canonical Huffman tree used in DEFLATE compression.
 *
 * DEFLATE does not transmit the actual Huffman codes, only the code lengths.
 * Using the rules in Section 3.2.2, the codes can be reconstructed by:
 *
 *   1. Counting how many symbols exist for each length (`bl_count[]`).
 *   2. Sorting symbols by their code length (`sorted_symbols[]`).
 *   3. Generating codes in lexicographical order based on length and count.
 *
 * This structure supports decoding by traversing bit-by-bit until a code of
 * matching length and index is found in the `sorted_symbols[]` array.
 *
 * @note This representation is optimized for fast decoding and simplicity.
 *       It assumes that no code length exceeds @ref MAX_BITS, and that the
 *       total number of symbols does not exceed @ref MAX_SYMBOLS.
 */
struct tree {
  unsigned short bl_count[MAX_BITS];    /**< Count of symbols per bit length. */
  symbol_t sorted_symbols[MAX_SYMBOLS]; /**< Symbols sorted by code length. */
};

/**
 * @struct deflate
 * @brief Represents DEFLATE decompression state and stream context.
 *
 * This structure holds all necessary state to decode a single DEFLATE stream.
 * It manages the input and output buffers, a bit accumulator for reading
 * variable-length codes, and the decoded Huffman trees.
 *
 * It is designed to be used in a single-pass, freestanding context with no
 * dynamic memory allocation.
 */
struct deflate {
  const unsigned char *in_ptr; /**< Current input pointer. */
  const unsigned char *in_end; /**< End of input buffer. */
  unsigned char *dest;         /**< Output buffer pointer. */

#define ERR_NONE 0
#define ERR_OVERFLOW 1
#define ERR_SYMBOL 2
#define ERR_UNDEFINED 3
  int error;          /**< Error state. */
  uint32_t bit_accum; /**< Accumulator for next bits. */
  size_t nr_bits;     /**< Number of valid bits in accumulator. */
  size_t dest_size;   /**< Total bytes written to output. */

  struct tree ll;       /**< Literal/length Huffman tree. */
  struct tree distance; /**< Distance/backreference tree. */
};

typedef struct tree *tree_t;
typedef struct deflate *deflate_t;

/* For __arch64__ which is little-endian. */
typedef __attribute__((aligned(1))) uint16_t unaligned_u16;
typedef __attribute__((aligned(1))) uint32_t unaligned_u32;
/* Read uint16_t for an unaligned address. */
#define get_unaligned_u16(x) (*(const unaligned_u16 *)(x))
/* Read uint32_t for an unaligned address. */
#define get_unaligned_u32(x) (*(const unaligned_u32 *)(x))

/**
 * @brief Builds a canonical Huffman decoding tree from code lengths.
 *
 * This function constructs a canonical Huffman tree based on an array of
 * code lengths. It fills the `bl_count[]` array with the number of codes
 * for each bit length, then populates `sorted_symbols[]` with the symbols
 * sorted in order of increasing bit length.
 *
 * @param tree The tree to populate with bit-length counts and sorted symbols.
 * @param lengths Array of code lengths (in bits) for each symbol.
 * @param nr_sym Number of symbols in the `lengths[]` array.
 */
static void huffman_tree(tree_t tree, const unsigned char lengths[],
                         int nr_sym) {
  int i;
  unsigned short offs[MAX_BITS], sum = 0;

  for (i = 0; i < MAX_BITS; i++)
    tree->bl_count[i] = 0;

  for (i = 0; i < nr_sym; i++)
    tree->bl_count[lengths[i]]++;

  tree->bl_count[0] = 0;

  for (i = 0; i < MAX_BITS; i++) {
    offs[i] = sum;
    sum += tree->bl_count[i];
  }

  for (i = 0; i < nr_sym; i++) {
    if (lengths[i] != 0)
      tree->sorted_symbols[offs[lengths[i]]++] = i;
  }
}

/**
 * @brief Reads a specified number of bits from the input stream.
 *
 * This function extracts `nr_bits` from the DEFLATE input stream using a
 * little-endian bit buffer. It pulls in bytes as needed to ensure the
 * requested number of bits are available in the accumulator.
 *
 * If there is not enough data left in the stream to satisfy the request,
 * the function sets @p d error to `ERR_OVERFLOW` and returns 0.
 *
 * @param d Pointer to the deflate state.
 * @param nr_bits Number of bits to read (1–15 typically).
 * @return The extracted bits in LSB order, or 0 on error.
 */
static uint32_t get_bits(deflate_t d, size_t nr_bits) {
  uint32_t bits;

  if (d->error != ERR_NONE)
    return 0;

  while (d->nr_bits < nr_bits) {
    if (d->in_ptr == d->in_end) {
      d->error = ERR_OVERFLOW;

      return 0;
    }

    d->bit_accum |= ((uint32_t)(*d->in_ptr++) << d->nr_bits);
    d->nr_bits += 8;
  }

  bits = d->bit_accum & ((1 << nr_bits) - 1);
  d->bit_accum >>= nr_bits;
  d->nr_bits -= nr_bits;

  /* On exit the maximum number of bits left in bit_accum is 7. */

  return bits;
}

/* Sets @p d error to `ERR_OVERFLOW` on failure. */
static uint32_t get_extra_bits(deflate_t d, size_t nr_bits, uint32_t base) {
  uint32_t bits = nr_bits ? get_bits(d, nr_bits) : 0;

  return base + bits;
}

/**
 * @brief Decodes the next Huffman symbol from the bitstream.
 *
 * This function performs bit-by-bit traversal of a canonical Huffman tree as
 * used in DEFLATE, using the `bl_count[]` and `sorted_symbols[]` arrays from a
 * prebuilt @p tree.
 *
 * For each bit read, the function narrows down the search by computing
 * an offset into the range of symbols with the current code length.
 * Once a matching offset is found (i.e., a valid symbol), the function
 * returns the corresponding symbol from `sorted_symbols[]`.
 *
 * If no valid symbol is found after examining up to @ref MAX_BITS levels,
 * sets @p d error to `ERR_SYMBOL` and or if not enough data left in the stream
 * sets @p d error to `ERR_OVERFLOW` and returns 0.
 *
 * @param d Deflate context holding input stream and bit buffer.
 * @param tree Huffman tree to use for decoding.
 * @return The decoded symbol, or 0 on error (with @p d error set).
 */
static symbol_t decode_symbol(deflate_t d, tree_t tree) {
  int i;
  unsigned short sum = 0, offs = 0;

  /*
   * In Huffman tree, each level adds one bit to the code length, i.e. leaves
   * on the first level have code length of single bit, leaves on the second
   * level have code length of two bits, etc.
   *
   * Following the rules for Huffman code in DEFLATE format at each level `i`,
   * there are `bl_count[i]` leaves on the left and subtrees on the right
   * for the greater code length, i.e. `i + 1`.
   *
   * So, to convert the code to symbol, we go down the tree, one level
   * at a time looking only at the leaves.
   */

  for (i = 1; i < MAX_BITS; i++) {
    offs = 2 * offs + get_bits(d, 1);

    if (d->error != ERR_NONE)
      return 0;

    /*
     * Here, `offs` is an index to a code with length `i`, and `sum` is total
     * number of codes with length less than `i`, `sorted_symbols[sum + offs]`
     * is the current symbol.
     *
     * we assume `sum + offs` is in range, i.e. it is less than the index
     * in `sorted_symbols` of last symbols that participate in Huffman code
     * generation.
     */

    if (offs < tree->bl_count[i])
      return tree->sorted_symbols[sum + offs];

    sum += tree->bl_count[i];
    offs -= tree->bl_count[i];
  }

  d->error = ERR_SYMBOL;

  return 0;
}

/**
 * @brief Initializes fixed Huffman trees for literal/length and distance codes.
 *
 * This function sets up the standard "fixed Huffman codes" defined by the
 * DEFLATE specification. These trees are used when a DEFLATE block indicates
 * the use of fixed codes instead of dynamic ones.
 *
 * The fixed literal/length tree contains 288 symbols:
 *   - Symbols   0 - 143 use 8-bit codes.
 *   - Symbols 144 - 255 use 9-bit codes.
 *   - Symbols 256 - 279 use 7-bit codes.
 *   - Symbols 280 - 287 use 8-bit codes.
 *
 * The fixed distance tree contains 32 symbols, all using 5-bit codes.
 *
 * @param lt Tree to populate with fixed literal/length symbols.
 * @param dt Tree to populate with fixed distance symbols.
 *
 * @see rfc1951.txt (Section 3.2.6)
 */
static void huffman_fixed_tree(tree_t lt, tree_t dt) {
  int i;

  for (i = 0; i < MAX_BITS; i++)
    lt->bl_count[i] = 0;

  lt->bl_count[7] = 24;
  lt->bl_count[8] = 152;
  lt->bl_count[9] = 112;

  for (i = 0; i < 24; i++)
    lt->sorted_symbols[i] = 256 + i;

  for (i = 0; i < 144; i++)
    lt->sorted_symbols[24 + i] = i;

  for (i = 0; i < 8; i++)
    lt->sorted_symbols[24 + 144 + i] = 280 + i;

  for (i = 0; i < 112; i++)
    lt->sorted_symbols[24 + 144 + 8 + i] = 144 + i;

  for (i = 0; i < MAX_BITS; i++)
    dt->bl_count[i] = 0;

  dt->bl_count[5] = 32;

  for (i = 0; i < 32; i++)
    dt->sorted_symbols[i] = i;
}

/**
 * @brief Parses dynamic Huffman tree descriptions and builds decoding trees.
 *
 * This function implements the logic described in RFC 1951 for reading the
 * dynamic Huffman header in a DEFLATE block. It decodes a compressed
 * representation of code lengths for the literal/length and distance alphabets,
 * builds a temporary Huffman tree for the code length codes, then uses it to
 * reconstruct the final trees for literal/length and distance codes.
 *
 * Steps:
 * 1. Read HLIT (number of literal/length codes - 257).
 * 2. Read HDIST (number of distance codes - 1).
 * 3. Read HCLEN (number of code length codes - 4).
 * 4. Read HCLEN × 3-bit code length values and map them to the canonical order.
 * 5. Build a temporary Huffman tree for these code lengths.
 * 6. Use it to decode the HLIT + HDIST code lengths with run-length encoding.
 * 7. Build the final `lt` and `dt` Huffman trees.
 *
 * If an error occurs while decoding, it is stored in @p d error.
 *
 * @param d DEFLATE context containing input stream and bit state.
 * @param lt Tree to store the decoded literal/length tree.
 * @param dt Tree to store the decoded distance tree.
 *
 * @see rfc1951.txt (Section 3.2.7)
 */
static void huffman_dynamic_tree(deflate_t d, tree_t lt, tree_t dt) {
  int i, hlit, hdist, hclen;

  static unsigned char code_len_for_code_idx[19] = {
      16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
  };

  unsigned char lengths[MAX_SYMBOLS + MAX_DISTANCE] = {0};

  hlit = get_extra_bits(d, 5, 257); /* 5 bits for 'HLIT'. */
  hdist = get_extra_bits(d, 5, 1);  /* 5 bits for 'HDIST'. */
  hclen = get_extra_bits(d, 4, 4);  /* 4 bits for 'HCLEN'. */

  if (d->error != ERR_NONE)
    return;

  for (i = 0; i < hclen; i++) {
    lengths[code_len_for_code_idx[i]] = get_bits(d, 3);

    if (d->error != ERR_NONE)
      return;
  }

  /* Build the temporary Huffman tree for the code‐length alphabet */
  huffman_tree(lt, lengths, 19);

  i = 0;

  while (i < hlit + hdist) {
    enum {
      SYM_CL_COPY_3_6 = 16,
      SYM_CL_REPEAT_3_10 = 17,
      SYM_CL_REPEAT_11_138 = 18
    };
    symbol_t sym = decode_symbol(d, lt);
    int runlen;

    if (d->error != ERR_NONE)
      return;

    /* Copy previous code length ''3 .. 6'' times. */
    if (sym == SYM_CL_COPY_3_6) {
      if (i == 0) {
        d->error = ERR_SYMBOL;

        return;
      }

      sym = lengths[i - 1];
      runlen = get_extra_bits(d, 2, 3); /* Read 2 bits. */
      /* Repeat code length 0 for ''3 .. 10'' times. */
    } else if (sym == SYM_CL_REPEAT_3_10) {
      sym = 0;
      runlen = get_extra_bits(d, 3, 3); /* Read 3 bits. */
      /* Repeat code length 0 for ''11 .. 138'' times. */
    } else if (sym == SYM_CL_REPEAT_11_138) {
      sym = 0;
      runlen = get_extra_bits(d, 7, 11); /* Read 7 bits. */
    } else {
      runlen = 1;
    }

    if (d->error != ERR_NONE)
      return;

    while (runlen--)
      lengths[i++] = sym;
  }

  /* Finally build the real literal/length and distance trees. */
  huffman_tree(lt, lengths, hlit);
  huffman_tree(dt, lengths + hlit, hdist);
}

/**
 * @brief Decodes a single compressed DEFLATE block using Huffman coding.
 *
 * This function handles one compressed block of DEFLATE data using either
 * fixed or dynamically generated Huffman trees. The trees must already be
 * initialized in the deflate context (`d->ll` and `d->distance`).
 *
 * The block consists of a series of Huffman-encoded symbols, which are
 * interpreted as:
 *   - Literal byte values (0–255), copied directly to the output.
 *   - A length/distance pair (257–285), used for backreferences.
 *   - End-of-block marker (256), signaling the end of the block.
 *
 * For length/distance pairs:
 *   - The length symbol is decoded and extended with extra bits.
 *   - The distance symbol is decoded and also extended with extra bits.
 *   - `length` bytes are copied from `offset` bytes behind the current output.
 *
 * If an error occurs while decoding, it is stored in @p d error.
 *
 * @param d DEFLATE context with input/output state and Huffman trees.
 */
static void deflate_block(deflate_t d) {
  unsigned char *dest = d->dest;

  static unsigned char length_bits[30] = {
      0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 127,
  };

  static unsigned short length_base[30] = {
      3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23,  27,
      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0,
  };

  static unsigned char dist_bits[30] = {
      0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
  };

  static unsigned short dist_base[30] = {
      1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
      33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
      1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

  while (1) {
    symbol_t sym = decode_symbol(d, &d->ll);

    if (d->error != ERR_NONE)
      return;

    if (sym == 256) { /* EOB */
      d->dest_size += d->dest - dest;
      return;
    }

    /* (0 .. 255): Represent literal bytes. */
    if (sym < 256) {
      *d->dest++ = sym;

    } else {
      symbol_t dist_sym;
      int i, len, offset;

      /* 286 and 287 are “dummy” and should not appear in stream. */
      if (sym > 285) {
        d->error = ERR_SYMBOL;
        return;
      }

      /* Read ''[ len, backward offset ]'' pairs. */

      len = get_extra_bits(d, length_bits[sym - 257], length_base[sym - 257]);
      if (d->error != ERR_NONE)
        return;

      dist_sym = decode_symbol(d, &d->distance);
      if (d->error != ERR_NONE)
        return;

      if (dist_sym >= MAX_DISTANCE) {
        d->error = ERR_SYMBOL;
        return;
      }

      offset = get_extra_bits(d, dist_bits[dist_sym], dist_base[dist_sym]);
      if (d->error != ERR_NONE)
        return;

      /* Copy with overlap safety. */
      for (i = 0; i < len; i++)
        d->dest[i] = d->dest[i - offset];
      d->dest += len;
    }
  }
}

/**
 * @brief Decodes a single uncompressed DEFLATE block.
 *
 * This function handles raw (stored) DEFLATE blocks, which are byte-aligned
 * and not compressed. The block layout is:
 *
 *   - LEN (16bit) is the length of the uncompressed data.
 *   - NLEN (16bit) is the one's complement of LEN (used for error detection).
 *   - LEN bytes of literal data follow immediately.
 *
 * The function verifies that enough input is available, copies LEN bytes
 * into the destination, and resets the bit accumulator (i.e. any bits of input
 * up to the next byte boundary are ignored).
 *
 * If an error occurs while decoding, it is stored in @p d error.
 *
 * @param d DEFLATE context containing input/output state.
 *
 * @see rfc1951.txt (Section 3.2.4)
 */
static void deflate_uncompressed_block(deflate_t d) {
  uint16_t length;
  int i;

  if (d->in_end - d->in_ptr < 4) {
    d->error = ERR_OVERFLOW;
    return;
  }

  length = get_unaligned_u16(d->in_ptr);

  /* Skip 'LEN' and 'NLEN'. */
  d->in_ptr += 4;

  if (d->in_end - d->in_ptr < length) {
    d->error = ERR_OVERFLOW;
    return;
  }

  for (i = length; i > 0; i--)
    *d->dest++ = *d->in_ptr++;

  d->dest_size += length;
  d->bit_accum = 0;
  d->nr_bits = 0;
}

/**
 * @brief Decompresses a DEFLATE-compressed input buffer.
 *
 * This is the top-level routine for decoding raw DEFLATE streams. It processes
 * the stream block-by-block, reading the block header to determine the type:
 *
 *   - `00`: uncompressed (stored)
 *   - `01`: compressed with fixed Huffman codes
 *   - `10`: compressed with dynamic Huffman codes
 *   - `11`: reserved (error)
 *
 * The function maintains a persistent state across blocks, tracks the output
 * size, and updates @p dest_len on success.
 *
 * Block processing ends when the "final block" flag is set, or an error occurs.
 *
 * @param dest Output buffer where decompressed data will be written.
 * @param dest_len Pointer to variable updated with number of bytes written.
 * @param source Pointer to DEFLATE-compressed input buffer.
 * @param source_len Size of the compressed input buffer in bytes.
 *
 * @note The caller must ensure @p dest has sufficient capacity.
 *
 * @return 0 on success, or a non-zero error code (e.g. `ERR_OVERFLOW`,
 *         `ERR_SYMBOL`, `ERR_UNDEFINED`) on failure.
 */
static int deflate_buffer(unsigned char *dest, size_t *dest_len,
                          const unsigned char *source, size_t source_len) {
  struct deflate d;

  d.in_ptr = source;
  d.in_end = d.in_ptr + source_len;
  d.dest = dest;
  d.dest_size = 0;
  d.bit_accum = 0;
  d.nr_bits = 0;
  d.error = ERR_NONE;

  unsigned char block_type, block_final;

  do {
    block_final = get_bits(&d, 1);
    block_type = get_bits(&d, 2);

    if (d.error != ERR_NONE)
      break;

#define BTYPE_NO_COMPRESSION 0
#define BTYPE_COMPRESSED_FIXED 1
#define BTYPE_COMPRESSED_DYNAMIC 2

    if (block_type == BTYPE_NO_COMPRESSION) {
      deflate_uncompressed_block(&d);

    } else if (block_type == BTYPE_COMPRESSED_FIXED) {
      huffman_fixed_tree(&d.ll, &d.distance);
      deflate_block(&d);

    } else if (block_type == BTYPE_COMPRESSED_DYNAMIC) {
      huffman_dynamic_tree(&d, &d.ll, &d.distance);
      if (d.error == ERR_NONE)
        deflate_block(&d);

    } else {
      d.error = ERR_UNDEFINED;
    }

  } while ((block_final == 0) && (d.error == ERR_NONE));

  if (d.error == ERR_NONE)
    *dest_len = d.dest_size;

  return d.error;
}

/* ''GZIP''. */

static uint32_t update_crc32(const unsigned char *buffer, size_t size) {
  uint32_t crc = 0xFFFFFFFF;
  size_t n;

  static uint32_t crctab32[] = {0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
                                0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
                                0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
                                0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C};

  if (!size)
    return 0;

  for (n = 0; n < size; n++) {
    crc ^= buffer[n];
    crc = crctab32[crc & 0x0F] ^ (crc >> 4);
    crc = crctab32[crc & 0x0F] ^ (crc >> 4);
  }

  return (crc ^ 0xFFFFFFFF);
}

/**
 * @brief Decompresses a GZIP-compressed buffer using the DEFLATE algorithm.
 *
 * This function parses a GZIP header (RFC 1952), extracts the DEFLATE payload,
 * decompresses it using `deflate_buffer()`, and verifies the CRC32 and size
 * stored in the GZIP footer.
 *
 * @param dest Pointer to output buffer for decompressed data.
 * @param dest_len Pointer to variable updated with decompressed size.
 * @param source Pointer to input GZIP-compressed buffer.
 * @param source_len Size of input buffer in bytes.
 *
 * @note The caller must ensure @p dest has sufficient capacity.
 * @see rfc1952.txt
 *
 * @return 0 on success (valid GZIP), on failure -EINVAL (invalid header,
 *         CRC mismatch, unsupported flags) or -EIO (decompression failure).
 */
int decompress_gzip(unsigned char *dest, size_t *dest_len,
                    unsigned char *source, size_t source_len) {
  const unsigned char *start = source + 10;
  const unsigned char *end = source + source_len;

  if ((source_len < 18) ||   /* Size of empty compressed file.  */
      (source[0] != 0x1F) || /* Identification 1. */
      (source[1] != 0x8B) || /* Identification 2. */
      (source[2] != 0x8) ||  /* DEFLATE compression method. */
      (source[3] & 0xE0))    /* Reserved FLG bits must be zero. */
    return -EINVAL;

#define F_TEXT (1 << 0)
#define F_HCRC (1 << 1)
#define F_EXTRA (1 << 2)
#define F_NAME (1 << 3)
#define F_COMMENT (1 << 4)
  uint8_t flag = source[3];

  /* Skip 'F_EXTRA', 'F_NAME', and 'F_COMMENT'. */

  if (flag & F_EXTRA) {
    start += get_unaligned_u16(start) + 2;
    if (start > end)
      return -EINVAL;
  }

  if (flag & F_NAME) {
    do {
      if (start == end)
        return -EINVAL;
    } while (*start++);
  }

  if (flag & F_COMMENT) {
    do {
      if (start == end)
        return -EINVAL;
    } while (*start++);
  }

  if (flag & F_HCRC) {
    if (start + 2 > end)
      return -EINVAL;

    /* Check header CRC. */
    uint16_t crc16_expected = update_crc32(source, start - source) & 0xFFFF;
    if (crc16_expected != get_unaligned_u16(start))
      return -EINVAL;

    start += 2;
  }

  if (start + 8 > end)
    return -EINVAL;

  if (deflate_buffer(dest, dest_len, start, end - start - 8))
    return -EIO;

  /* Check ISIZE. */
  if (*dest_len != get_unaligned_u32(&source[source_len - 4]))
    return -EIO;

  /* Check CRC. */
  if (update_crc32(dest, *dest_len) !=
      get_unaligned_u32(&source[source_len - 8]))
    return -EIO;

  return 0;
}
