
    .section .text.start, "ax"
    .align  4
    .global _start
    .type   _start, %function

    .extern __bss_start
    .extern __bss_end
    .extern __stack_top

    .extern ukernel_entry
    .extern ukernel_apply_relocations
    .extern uart_setup

_start:

    adrp    x0, __stack_top
    add     x0, x0, :lo12:__stack_top
    mov     sp, x0

    /* Reset .bss section. */
    adrp    x1, __bss_start
    add     x1, x1, :lo12:__bss_start
    adrp    x2, __bss_end
    add     x2, x2, :lo12:__bss_end
    mov     x3, xzr
1:
    cmp     x1, x2
    b.hs    2f
    str     x3, [x1], #8
    b       1b

2:

    bl      ukernel_apply_relocations
    bl      uart_setup
    bl      ukernel_entry

    /* SHOULD NOTE GET HERE. */

hang:
    wfe
    b       hang

    .size _start, . - _start
