/* SPDX-License-Identifier: Apache-2.0
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
 * sw/smu-vp-tests/smu-link-test/sep_main.c
 *
 * SEP side of the SMU on-die link test (runs on smu-vp).
 *
 * Drives path #1 (SEP -> SMC, dedicated sep_ext_to_smc_axi): writes a magic
 * word then a doorbell into the SMC global window at 0x4004_1000/0x4004_1004
 * (window-local offsets 0x4_1000/0x4_1004 -> SMC scratchpad 0xC004_1000/4).
 * The write tap / fallback store is bypassed because smu-vp presets
 * och_sep_ss1.smc_global.forward_en = true.
 *
 * Then waits on path #2 (SMC -> SEP, SMU crossbar): polls a mailbox cell in
 * SEP-local SRAM at 0x1000_8000 for the SMC's response, which the SMC writes
 * via the global aperture address 0x6000_8000.
 *
 * The mailbox address is clear of this image (~2 KiB at 0x1000_0000) and of
 * the stack (top of SRAM, 0x1004_0000, growing down).
 */
#include <stdint.h>

extern int printf(const char *format, ...);

/* SEP-local SRAM cell the SMC writes through the crossbar. */
#define SEP_SRAM_MAILBOX    0x10008000u

/* SMC global window: scratchpad message + doorbell.  Window offset 0x6_1000
 * lands at SMC-local 0xC006_1000 — the scratchpad RAM (the 0xC004_0000 alias
 * slot is the SMC boot ROM, not the scratchpad). */
#define SMC_SCRATCH_MSG     0x40061000u
#define SMC_SCRATCH_BELL    0x40061004u

#define MAGIC_SEP2SMC       0x5E92CA11u
#define MAGIC_DOORBELL      0xD00BEE11u
#define MAGIC_SMC2SEP       0x5C0DE077u

#define SPIN_LIMIT          50000000u

#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU link test (SEP side) ===\n");

    /* Arm the mailbox before ringing the SMC's doorbell. */
    REG32(SEP_SRAM_MAILBOX) = 0u;

    /* Path #1: magic first, doorbell second (TLM keeps the order). */
    REG32(SMC_SCRATCH_MSG)  = MAGIC_SEP2SMC;
    REG32(SMC_SCRATCH_BELL) = MAGIC_DOORBELL;
    printf("SEP->SMC dedicated path: magic + doorbell written via 0x%08x\n",
           SMC_SCRATCH_MSG);
    /* Read back through the same path: the value must come from the SMC
     * scratchpad (the fallback stub is bypassed in forward mode). */
    if (REG32(SMC_SCRATCH_MSG) != MAGIC_SEP2SMC ||
        REG32(SMC_SCRATCH_BELL) != MAGIC_DOORBELL) {
        printf("FAIL: SEP->SMC readback mismatch: msg=0x%08x bell=0x%08x\n",
               REG32(SMC_SCRATCH_MSG), REG32(SMC_SCRATCH_BELL));
        return 1;
    }
    printf("SEP->SMC dedicated path: readback verified\n");

    /* Path #2: wait for the SMC's crossbar response in local SRAM. */
    uint32_t spins = 0;
    while (REG32(SEP_SRAM_MAILBOX) != MAGIC_SMC2SEP) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SMC response at 0x%08x\n",
                   SEP_SRAM_MAILBOX);
            return 1;
        }
    }
    printf("SMC->SEP crossbar path: response 0x%08x received at 0x%08x\n",
           MAGIC_SMC2SEP, SEP_SRAM_MAILBOX);

    printf("PASS: SMU link test (SEP side)\n");
    return 0;
}
