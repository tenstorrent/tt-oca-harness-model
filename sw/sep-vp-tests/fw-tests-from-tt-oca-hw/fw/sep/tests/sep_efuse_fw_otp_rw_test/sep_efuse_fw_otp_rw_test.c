// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*******************************************************************************
 * SEP eFuse FW OTP read/write test.
 ******************************************************************************/

#include <stdint.h>
#include <stdio.h>

#include "efuse_fw_test_common.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

int main(void)
{
    const uint32_t positive_bits[] = {
        EFUSE_FW_CLASS_KEY_BIT0,
        EFUSE_FW_CLASS_KEY_BIT0 + 37u,
        EFUSE_FW_CHIPLET_UID_BIT0,
        EFUSE_FW_CHIPLET_UID_BIT0 + 64u,
        EFUSE_FW_CHIPLET_UID_BIT0 + 127u,
        EFUSE_FW_BL1_VERSION_BIT0,
    };
    uint32_t bit_value = 0;
    efuse_fw_read_result_t locked_read;

    sep_outbound_filter_init();

    printf("SEP eFuse FW OTP RW Test\n");

    if (efuse_wait_sense_done() != 0) {
        test_fail(1);
    }

    efuse_clear_req_error();

    if (efuse_config_program_clock(12) != 0) {
        test_fail(1);
    }

    for (unsigned int i = 0; i < sizeof(positive_bits) / sizeof(positive_bits[0]); i++) {
        printf("Programming OTP bit %u\n", positive_bits[i]);
        if (efuse_program_bit(positive_bits[i]) != 0) {
            test_fail(1);
        }

        if (efuse_read_bit(positive_bits[i], &bit_value) != 0) {
            test_fail(1);
        }

        printf("Read back OTP bit %u = %u\n", positive_bits[i], bit_value);
        if (bit_value != 1u) {
            printf("ERROR: expected programmed OTP bit %u to read back as 1\n",
                   positive_bits[i]);
            test_fail(1);
        }
    }

    printf("Setting CHIPLET_UID OTP write lock\n");
    if (efuse_set_shadow_lock_bit(EFUSE_FW_WRITE_LOCK_BIT(EFUSE_FW_FIELD_CHIPLET_UID)) != 0) {
        test_fail(1);
    }

    if (efuse_program_bit_expect(EFUSE_FW_CHIPLET_UID_BIT0 + 191u, 0) != 0) {
        test_fail(1);
    }

    printf("Setting CHIPLET_UID OTP read lock\n");
    if (efuse_set_shadow_lock_bit(EFUSE_FW_READ_LOCK_BIT(EFUSE_FW_FIELD_CHIPLET_UID)) != 0) {
        test_fail(1);
    }

    efuse_clear_req_error();
    if (efuse_read_word_raw(EFUSE_FW_CHIPLET_UID_BIT0, &locked_read) != 0) {
        test_fail(1);
    }

    if (locked_read.completed == 0 || locked_read.req_error == 0 ||
        locked_read.data_word != 0) {
        printf("ERROR: locked OTP read expected req_error=1/data=0, got "
               "completed=%u read_status=%u req_error=%u data=0x%08x\n",
               locked_read.completed, locked_read.read_status,
               locked_read.req_error, locked_read.data_word);
        test_fail(1);
    }
    efuse_clear_req_error();

    printf("*** SEP eFuse FW OTP RW Test PASSED ***\n");
    test_pass(0);
    while (1) {
        __asm__("wfi");
    }
}
