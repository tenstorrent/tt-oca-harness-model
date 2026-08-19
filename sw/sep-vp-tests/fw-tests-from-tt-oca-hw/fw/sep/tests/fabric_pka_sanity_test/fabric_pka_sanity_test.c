/*
 * Fabric PKA Sanity Test - TC_FABRIC_011
 *
 * Verifies the SEP fabric path to cpu_ctrl CSRs.
 * NOTE: CLOCK_GATE_CTRL is a reserved placeholder pending implementation.
 * This test verifies fabric accessibility by reading CLOCK_GATE_CTRL and
 * checking its reset-default value matches the expected default.
 */

#include <stdint.h>
#include <stdio.h>

#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

int main(void)
{
    int pass = 1;

    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("Fabric PKA Sanity Test (TC_FABRIC_011)\n");
    printf("========================================\n\n");

    SEP_CPU_CTRL_CLOCK_GATE_CTRL_reg_u cg = {
        .val = READ_REG(SEP_CPU_CTRL_CLOCK_GATE_CTRL_REG_ADDR)
    };
    printf("CLOCK_GATE_CTRL = 0x%08x (expected 0x%08x)\n",
           (uint32_t)cg.val, SEP_CPU_CTRL_CLOCK_GATE_CTRL_REG_DEFAULT);

    if ((uint32_t)cg.val != SEP_CPU_CTRL_CLOCK_GATE_CTRL_REG_DEFAULT) {
        pass = 0;
        printf("CLOCK_GATE_CTRL reset default mismatch!\n");
    }

    if (pass) {
        printf("=== FABRIC PKA SANITY TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== FABRIC PKA SANITY TEST FAILED ===\n");
        test_fail(0);
    }

    while (1) {
        __asm__("wfi");
    }

    return pass ? 0 : -1;
}
