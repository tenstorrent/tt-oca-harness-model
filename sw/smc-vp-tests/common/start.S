/* SPDX-License-Identifier: Apache-2.0
 * sw/smc-vp-tests/common/start.S
 *
 * Bare-metal RV64 startup for smc-vp firmware tests.  Linked at 0x80000000
 * (cluster fast-mem window); the cluster's reset_pc is preset to the ELF
 * entry, which is _start.
 *
 * Sets mtvec, clears BSS, initializes UART0 (so printf works), calls main,
 * and parks on return.
 */

    .section .text.init, "ax"
    .global _start
_start:
    /* Park all harts except hart 0 (standard RISC-V SMP boot convention):
     * only hart 0 proceeds to main; harts 1..3 spin in a wfi loop so their
     * output doesn't interleave with the test. */
    csrr    t0, mhartid
    bnez    t0, _park

    /* Trap vector -> simple spin (tests that trap will hang here) */
    la      t0, _trap_spin
    csrw    mtvec, t0

    /* Stack pointer at top of fast-mem window (0x90000000) */
    li      sp, 0x90000000

    /* Clear BSS */
    la      t0, __bss_start
    la      t1, __bss_end
1:
    bgeu    t0, t1, 2f
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       1b
2:

    /* Initialize UART0 (8N1, divisor=1) so printf output works */
    call    uart_init

    /* Enter main */
    call    main

    /* Park on return */
_exit:
    j       _exit

_park:
    wfi
    j       _park

_trap_spin:
    j       _trap_spin

/* ----------------------------------------------------------------------------
 * uart_init: configure UART0 for 8N1, divisor=1, DLAB=0.
 * Mirrors the init sequence used by the historical smc/platform test firmware.
 * Constants inlined (assembler cannot parse smc_common.h).
 *   UART0 base = 0xC000A000
 *   THR/DLL = 0x00, IER/DLM = 0x04, LCR = 0x0C
 * ---------------------------------------------------------------------------- */
    .section .text.init, "ax"
uart_init:
    li      t0, 0xC000A000         /* SMC_UART0_BASE */
    li      t1, 0x80               /* DLAB=1 */
    sw      t1, 0x0C(t0)           /* LCR = 0x80 */
    li      t1, 0x01               /* DLL = 1 */
    sw      t1, 0x00(t0)           /* THR/DLL = 1 */
    sw      zero, 0x04(t0)         /* IER/DLM = 0  -> divisor = 1 */
    li      t1, 0x03               /* 8N1, DLAB=0 */
    sw      t1, 0x0C(t0)           /* LCR = 0x03 */
    ret
