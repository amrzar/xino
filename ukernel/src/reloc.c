/**
 * @file reloc.c
 * @brief Minimal AArch64 self-relocation (R_AARCH64_RELATIVE) for a PIE kernel.
 *
 * This module applies `R_AARCH64_RELATIVE` relocations recorded in the
 * `.rela.dyn` section at early boot, fixing up absolute addresses so they
 * point at their **final virtual addresses** in the range
 * `[UKERNEL_BASE, UKERNEL_BASE + __image_end)`.
 *
 * What it does
 *  - Computes the image load physical base via a linker-provided symbol.
 *  - Iterates `__rela_dyn_start .. __rela_dyn_end`.
 *  - For each `Elf64_Rela` with type `R_AARCH64_RELATIVE`, stores:
 *
 *   @code
 *   *(uint64_t *)(rela->r_offset + phys_base) = rela->r_addend + bias;
 *   @endcode
 *
 *  - For `R_AARCH64_RELATIVE`, `rela->r_addend` encodes (effectively) the
 *    link-time virtual address of the target object within the same image.
 *  - This adds the load bias (difference between load address and link-time
 *    base) to `rela->r_addend` to obtain the real runtime address.
 *  - Since ukernel is linked with `VMA = 0`, `rela->r_addend` is just an
 *    offset within the image, and the load `bias` is `UKERNEL_BASE`.
 *
 * This file is **PIC by nature**
 * - It is plain C with no global address constants stored in data,
 *   no TLS, and no C++ features (vtables/RTTI).
 * - References to `__image_start` / `__rela_dyn_*` are emitted as
 *   **PC-relative code relocations** (`ADRP` + `ADD :lo12:`), which are
 *   resolved **at link time** and therefore do **not** produce dynamic
 *   relocations in `.rela.dyn`.
 *
 * Requirements
 * - Compile all objects with `-fpie` (or libraries with `-fPIC`).
 * - Link with `-static --pie` and keep `.rela.dyn` in the image.
 * - Linker script must export `__image_start`, `__rela_dyn_start`, and
 *   `__rela_dyn_end`.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <config.h> // for UKERNEL_BASE.
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint64_t r_offset; /**< Location to apply the action. */
  uint64_t r_info;   /**< Index and type of relocation. */
  int64_t r_addend;  /**< Constant addend used to compute value. */
} Elf64_Rela;

#define R_AARCH64_RELATIVE 1027
#define ELF64_R_TYPE(info) ((uint32_t)((info) & 0xffffffffU))

extern char __image_start[];
extern Elf64_Rela __rela_dyn_start[], __rela_dyn_end[];

static inline uintptr_t load_phys_base() {
  return (uintptr_t)__image_start; /* Physical (or identity-mapped) base. */
}

static void apply_rela() {
  uintptr_t phys_base = load_phys_base();
  Elf64_Rela *rela;

  for (rela = __rela_dyn_start; rela < __rela_dyn_end; rela++) {
    if (ELF64_R_TYPE(rela->r_info) != R_AARCH64_RELATIVE) {
      // Unexpected relocation.
      for (;;)
        __asm__ __volatile__("wfe");
    }

    // Apply R_AARCH64_RELATIVE:
    *(uint64_t *)(rela->r_offset + phys_base) = rela->r_addend + UKERNEL_BASE;
  }
}

void ukernel_apply_relocations() { apply_rela(); }
