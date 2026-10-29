/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-rom-boot-test/rom.S
 *
 * Minimal SMC boot ROM image for the ROM-only boot tests. It is linked at the boot ROM base
 * and loaded with dut.bootrom.init_file, so it is the first code the SMC cores run when the
 * platform is given no ELF. Hart 0 prints the pass banner on UART0 and leaves a token in SMC
 * SRAM for the SEP; every hart then parks.
 */
    .section .text.init, "ax"
    .globl _start
_start:
    csrr    t0, mhartid
    bnez    t0, park

    li      t0, 0xC0006100          /* UART0 (16550): 8N1, divisor 1 */
    li      t1, 0x80
    sw      t1, 0x0C(t0)            /* LCR: DLAB */
    li      t1, 0x01
    sw      t1, 0x00(t0)            /* DLL */
    sw      zero, 0x04(t0)          /* DLM */
    li      t1, 0x03
    sw      t1, 0x0C(t0)            /* LCR: 8N1 */

    la      a0, banner
1:  lbu     t1, 0(a0)
    beqz    t1, 3f
2:  lw      t2, 0x14(t0)            /* LSR.THRE */
    andi    t2, t2, 0x20
    beqz    t2, 2b
    sw      t1, 0x00(t0)            /* THR */
    addi    a0, a0, 1
    j       1b

3:  li      t0, 0xC0061200          /* SMC SRAM; the SEP sees it at 0x4006_1200 */
    li      t1, 0x524F4D31          /* "ROM1" */
    sw      t1, 0(t0)

park:
    wfi
    j       park

banner:
    .asciz  "PASS: SMC ROM-only boot runs from the boot ROM base (SMC side)\n"
