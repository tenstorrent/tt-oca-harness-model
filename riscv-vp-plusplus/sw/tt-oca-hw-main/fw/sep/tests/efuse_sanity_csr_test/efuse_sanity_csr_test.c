/*******************************************************************************
 * Efuse CSR Sanity Test
 *
 * This test verifies that the efuse CSRs are correctly implemented.
 *
 * Copyright 2025 Tenstorrent Inc.
 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"


int main(void)
{
    // Initialize outbound filter to allow testpass mailbox access
    sep_outbound_filter_init();

    printf("Efuse CSR Sanity Test\n");
    printf("====================================\n\n");

    /*
     * Step 1: Read the efuse CSRs and print the values
     */
    printf("Reading efuse read control register CSR...\n");
    uint32_t efuse_read_ctrl_reg_addr = READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR);
    printf("EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR value: 0x%08x\n", efuse_read_ctrl_reg_addr);

    /*
     * Step 2: Write to the efuse CSRs and print the values
     */
    printf("Writing 0x1234 to EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG CSR...\n");
    WRITE_REG(EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR, 0x1234);

    // Read back the value to verify the write
    printf("Reading efuse read control register CSR back...\n");
    uint32_t efuse_read_ctrl_reg_addr_back = READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR);
    printf("EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR value is 0x%08x\n", efuse_read_ctrl_reg_addr_back);

    if (efuse_read_ctrl_reg_addr_back != 0x1234) {
        printf("ERROR: EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR value is not 0x1234\n");
        test_fail(1);
    }

    /*
     * Step 3: Read efuse shim CSR and print the values
     */
    printf("Reading efuse shim CSR...\n");
    uint32_t efuse_shim_timing_ctrl_7 = READ_REG(EFUSE_SHIM_CTRL_EFUSE_TIMING_CTRL_7_REG_ADDR);
    printf("EFUSE_SHIM_CTRL_SAMSUNG_EFUSE_TIMING_CTRL_7_REG value: 0x%08x\n", efuse_shim_timing_ctrl_7);

    /*
     * Step 4: Write to the efuse shim CSR and print the values
     */
    printf("Writing 0xABCD to efuse interface CSR...\n");
    WRITE_REG(EFUSE_SHIM_CTRL_EFUSE_TIMING_CTRL_7_REG_ADDR, 0xABCD);

    // Read back the value to verify the write
    printf("Reading efuse interface CSR back...\n");
    uint32_t efuse_shim_timing_ctrl_7_back = READ_REG(EFUSE_SHIM_CTRL_EFUSE_TIMING_CTRL_7_REG_ADDR);
    printf("EFUSE_SHIM_CTRL_EFUSE_TIMING_CTRL_7_REG_ADDR value is 0x%08x\n", efuse_shim_timing_ctrl_7_back);

    if (efuse_shim_timing_ctrl_7_back != 0xABCD) {
        printf("ERROR: EFUSE_SHIM_CTRL_EFUSE_TIMING_CTRL_7_REG_ADDR value is not 0xABCD\n");
        test_fail(1);
    }


    printf("\n*** Efuse CSR Sanity Test PASSED ***\n");
    test_pass(0);
    // Keep CPU alive after signaling completion.
    while (1) {
        __asm__("wfi");
    }
}
