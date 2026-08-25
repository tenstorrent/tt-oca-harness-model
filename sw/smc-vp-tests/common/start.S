/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
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

    /* Trap vector -> save registers, call weak trap_handler(), mret */
    la      t0, _trap_entry
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

/* ----------------------------------------------------------------------------
 * Trap entry: save caller-saved registers, call weak trap_handler(), then
 * mret back to the interrupted code.  Tests that need interrupts/exceptions
 * define their own non-weak trap_handler(); otherwise the default spins.
 * ---------------------------------------------------------------------------- */
    .section .text.init, "ax"
    .align 4
    .global _trap_entry
_trap_entry:
    addi    sp, sp, -128
    sd      ra, 0(sp)
    sd      t0, 8(sp)
    sd      t1, 16(sp)
    sd      t2, 24(sp)
    sd      a0, 32(sp)
    sd      a1, 40(sp)
    sd      a2, 48(sp)
    sd      a3, 56(sp)
    sd      a4, 64(sp)
    sd      a5, 72(sp)
    sd      a6, 80(sp)
    sd      a7, 88(sp)
    sd      t3, 96(sp)
    sd      t4, 104(sp)
    sd      t5, 112(sp)
    sd      t6, 120(sp)
    call    trap_handler
    ld      ra, 0(sp)
    ld      t0, 8(sp)
    ld      t1, 16(sp)
    ld      t2, 24(sp)
    ld      a0, 32(sp)
    ld      a1, 40(sp)
    ld      a2, 48(sp)
    ld      a3, 56(sp)
    ld      a4, 64(sp)
    ld      a5, 72(sp)
    ld      a6, 80(sp)
    ld      a7, 88(sp)
    ld      t3, 96(sp)
    ld      t4, 104(sp)
    ld      t5, 112(sp)
    ld      t6, 120(sp)
    addi    sp, sp, 128
    mret

    /* Weak default handler: spin forever.  Tests override this for real handling. */
    .weak   trap_handler
    .global trap_handler
trap_handler:
1:
    j       1b

/* ----------------------------------------------------------------------------
 * uart_init: configure UART0 for 8N1, divisor=1, DLAB=0.
 * Mirrors the init sequence used by the historical smc/platform test firmware.
 * Constants inlined (assembler cannot parse smc_common.h).
 *   UART0 base = 0xC0006100 (16550; D3=U1)
 *   THR/DLL = 0x00, IER/DLM = 0x04, LCR = 0x0C
 * ---------------------------------------------------------------------------- */
    .section .text.init, "ax"
uart_init:
    li      t0, 0xC0006100         /* SMC_UART0_BASE */
    li      t1, 0x80               /* DLAB=1 */
    sw      t1, 0x0C(t0)           /* LCR = 0x80 */
    li      t1, 0x01               /* DLL = 1 */
    sw      t1, 0x00(t0)           /* THR/DLL = 1 */
    sw      zero, 0x04(t0)         /* IER/DLM = 0  -> divisor = 1 */
    li      t1, 0x03               /* 8N1, DLAB=0 */
    sw      t1, 0x0C(t0)           /* LCR = 0x03 */
    ret
