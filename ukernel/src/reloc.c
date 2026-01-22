/**
 * @file reloc.c
 * @brief Minimal AArch64 self-relocation for a PIE image.
 *
 * This module applies **only** `R_AARCH64_RELATIVE` relocations, recorded in
 * the `.rela.dyn` section at early boot, fixing up absolute addresses so they
 * point at their **final virtual addresses**.
 *
 * What it does
 *  - Computes the image load physical base via a linker-provided symbol.
 *  - Iterates `__rela_dyn_start .. __rela_dyn_end`.
 *  - For each `Elf64_Rela` with type `R_AARCH64_RELATIVE`, it does
 *
 *   @code
 *   *(uint64_t *)(rela->r_offset + phys_base) = rela->r_addend + bias;
 *   @endcode
 *
 *      where `bias` is the difference between
 *        the **runtime virtual address** and
 *        the **link-time virtual address**.
 *
 * This file is **PIC by nature**
 *  - It is plain C with no global address constants stored in data,
 *    no TLS, and no C++ features (vtables/RTTI).
 *  - References to `__image_start` / `__rela_dyn_*` are emitted as
 *    **PC-relative code relocations** (`ADRP` + `ADD :lo12:`), which are
 *    resolved **at link time** and therefore do **not** produce dynamic
 *    relocations in `.rela.dyn`.
 *
 * Requirements
 *  - Compile all objects with `-fpie` (or libraries with `-fPIC`).
 *  - Link with `-static --pie` and keep `.rela.dyn` in the image.
 *  - Linker script must export `__image_start`, `__rela_dyn_start`, and
 *    `__rela_dyn_end`.
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

/**
 * @brief Early-boot hazard: absolute VA dereference while MMU is off.
 *
 * Accessing linker-defined symbols (e.g., `__image_start`) during early boot is
 * only safe with MMU off if the compiler can prove at compile time that the
 * symbol is **non-preemptible** (hidden). This forces direct PC-relative code
 * generation (`adr/adrp` + `add/ldr`) instead of a GOT-based sequence.
 *
 * Safety condition (must hold):
 *  - Every TU that references such symbols must compile with hidden visibility
 *    in effect (e.g., global `-fvisibility=hidden`), so codegen avoids GOT.
 *  - Linker-script HIDDEN (e.g.`HIDDEN(__image_start = .))` is used to keep
 *    the final ELF symbol hidden.
 *
 * Recommended hardening:
 *  - `-fvisibility=hidden` for all C/C++ objects, especially early-boot code.
 *  - Fail the link if PLT/GOT machinery appears (ASSERTs or
 *    `--orphan-handling=error` for `.plt`, `.iplt`, `.rela.plt`, `.rela.iplt`,
 *    `.got.plt`).
 *
 * Regression check:
 *  - Validate relocations: `readelf -rW` should contain only the relocation
 *    types reloc.c handles (i.e. `R_AARCH64_RELATIVE`).
 */

extern char __image_start[];
extern Elf64_Rela __rela_dyn_start[];
extern Elf64_Rela __rela_dyn_end[];

static inline uintptr_t load_phys_base() {
  // Physical (or identity-mapped) base.
  return (uintptr_t)__image_start;
}

/**
 * @brief Apply `.rela.dyn` RELA relocations with a bias.
 *
 * The bias MUST be computed using **unsigned** arithmetic so that wraparound is
 * well-defined and negative deltas are represented correctly in
 * two's-complement form. Compute it as:
 *
 * @code
 *   uint64_t bias =
 *    (uint64_t)(uintptr_t)runtime_base_va - (uint64_t)(uintptr_t)link_base_va;
 * @endcode
 *
 * @param bias Unsigned relocation bias.
 */
static void apply_rela(uint64_t bias) {
  uintptr_t phys_base = load_phys_base();
  Elf64_Rela *rela;

  for (rela = __rela_dyn_start; rela < __rela_dyn_end; rela++) {
    if (ELF64_R_TYPE(rela->r_info) != R_AARCH64_RELATIVE) {
      // Unexpected relocation.
      for (;;)
        __asm__ __volatile__("wfe");
    }

    // Apply R_AARCH64_RELATIVE.
    *(uint64_t *)(phys_base + (uintptr_t)rela->r_offset) =
        (uint64_t)rela->r_addend + bias; // modulo 2^64.
  }
}

// NO SUPPORT FOR VA RANDOMIZATION.
// Use `UKERNEL_BASE` as `runtime_base_va`, `link_base_va` = 0.
// Call only if MMU is off.
void ukernel_apply_relocations(uintptr_t va) { apply_rela(UKERNEL_BASE); }
