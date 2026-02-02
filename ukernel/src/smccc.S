
    .text
    .align  4

    .global smccc_smc
    .type   smccc_smc, %function

    /* void smccc_smc(const regs *in, regs *out) { ... } */
smccc_smc:

    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    /* Save X19, X20 on stack, use as scratch for argument. */
    stp     x19, x20, [sp, #-16]!
    mov     x19, x0     /* x19 = in  */
    mov     x20, x1     /* x20 = out */

    /* Load X0..X17 from [x19]. */
    ldp     x0,  x1,  [x19, #0]
    ldp     x2,  x3,  [x19, #16]
    ldp     x4,  x5,  [x19, #32]
    ldp     x6,  x7,  [x19, #48]
    ldp     x8,  x9,  [x19, #64]
    ldp     x10, x11, [x19, #80]
    ldp     x12, x13, [x19, #96]
    ldp     x14, x15, [x19, #112]
    ldp     x16, x17, [x19, #128]

    smc     #0

    /* Store X0..X17 to [x20]. */
    stp     x0,  x1,  [x20, #0]
    stp     x2,  x3,  [x20, #16]
    stp     x4,  x5,  [x20, #32]
    stp     x6,  x7,  [x20, #48]
    stp     x8,  x9,  [x20, #64]
    stp     x10, x11, [x20, #80]
    stp     x12, x13, [x20, #96]
    stp     x14, x15, [x20, #112]
    stp     x16, x17, [x20, #128]

    /* Restore X19 and X20. */
    ldp     x19, x20, [sp], #16

    ldp     x29, x30, [sp], #16

    ret

    .size   smccc_smc, .-smccc_smc
    