/* SPDX-License-Identifier: Apache-2.0
 * sw/smu-vp-tests/smu-link-test/smc_main.c
 *
 * SMC side of the SMU on-die link test (runs on smu-vp).
 *
 * Path under test #1 (inbound): SEP -> SMC over the dedicated
 * sep_ext_to_smc_axi path.  The SEP writes a magic word and a doorbell into
 * its SMC global window at 0x4004_1000/0x4004_1004, which the SMU platform
 * routes:  SEP bus -> smc_global (forward_en) -> sep_ext_to_smc_axi ->
 * sep2smc_remap (strip 0x4000_0000) -> SMC fabric sep_axi_in ->
 * to_local_addr (alias | 0xC000_0000) -> SMC scratchpad 0xC004_1000/1004.
 * This firmware polls the doorbell in the scratchpad, then verifies the
 * magic word.
 *
 * Path under test #2 (outbound): SMC -> SEP over the SMU crossbar.  This
 * firmware writes a response word to the SEP global aperture at
 * 0x6000_8000 (= sep_global_base 0x5000_0000 + SEP-local SRAM 0x1000_8000),
 * which routes:  cluster mmio -> fabric outbound -> output_axi -> smu_xbar
 * (sep aperture hit) -> sep_in -> SEP inbound remap (- 0x5000_0000) ->
 * SEP SRAM 0x1000_8000, where the SEP firmware polls it.
 *
 * Requires smc_smu_vp.ini settings: dut.cluster.mmio_lo = 0x40000000 and
 * smu_xbar.sep_region_size = 0x20000000 (aperture covers the SRAM alias).
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

/* SMC scratchpad cells written by the SEP.  NB: the fabric's coarse
 * FRONT_SPM range starts at 0xC004_0000, but the platform's front-port
 * decode places the boot ROM at 0xC004_0000..0xC006_0000 and the scratchpad
 * RAM at 0xC006_0000 — writes below 0xC006_0000 land in the ROM and vanish. */
#define SMC_SPM_BASE        0xC0060000ULL
#define SEP_MSG_OFF         0x1000u
#define SEP_DOORBELL_OFF    0x1004u

/* SEP SRAM mailbox, addressed through the SMU global aperture. */
#define SEP_SRAM_MAILBOX    0x60008000ULL

#define MAGIC_SEP2SMC       0x5E92CA11u
#define MAGIC_DOORBELL      0xD00BEE11u
#define MAGIC_SMC2SEP       0x5C0DE077u

#define SPIN_LIMIT          50000000u

#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU link test (SMC side) ===\n");

    /* Path #1: wait for the SEP's doorbell, then check the magic word. */
    uint32_t spins = 0;
    while (REG32(SMC_SPM_BASE + SEP_DOORBELL_OFF) != MAGIC_DOORBELL) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SEP doorbell at 0x%08x\n",
                   (unsigned)(SMC_SPM_BASE + SEP_DOORBELL_OFF));
            return 1;
        }
    }
    const uint32_t got = REG32(SMC_SPM_BASE + SEP_MSG_OFF);
    if (got != MAGIC_SEP2SMC) {
        printf("FAIL: SEP->SMC magic mismatch: got 0x%08x want 0x%08x\n",
               got, MAGIC_SEP2SMC);
        return 1;
    }
    printf("SEP->SMC dedicated path: doorbell + magic 0x%08x received\n", got);

    /* Path #2: answer through the crossbar into the SEP's SRAM mailbox. */
    REG32(SEP_SRAM_MAILBOX) = MAGIC_SMC2SEP;
    printf("SMC->SEP crossbar path: response 0x%08x written to 0x%08x\n",
           MAGIC_SMC2SEP, (unsigned)SEP_SRAM_MAILBOX);

    printf("PASS: SMU link test (SMC side)\n");
    return 0;
}
