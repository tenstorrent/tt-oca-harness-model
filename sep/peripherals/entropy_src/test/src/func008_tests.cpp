// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/******************************************************************************
 * @file func008_tests.cpp
 * @brief Basic Register Access Tests for New Registers added from RDL Update
 *
 * Implements Read/Write and Reset default value tests for the 22 new registers.
 *
 ******************************************************************************/

#include "testbench.h"

#include <iomanip>
#include <sstream>

#define FUNC008_CHECK(cond, msg_stream)           \
    do {                                           \
        if (!(cond))                               \
        {                                          \
            CSML_ERROR(0, logger) << msg_stream;   \
            ok = false;                            \
        }                                          \
    } while (false)

// =============================================================================
// tc_f001_new_regs_reset_values — Verify reset values of 22 new registers
// =============================================================================
bool testbench::tc_f001_new_regs_reset_values()
{
    bool ok = true;
    uint32_t read_val;

    struct RegCheck {
        unsigned int offset;
        uint32_t expected;
        const char* name;
    };

    RegCheck regs[] = {
        { entropy_src_basetest::SHA256_STATUS_OFFSET, entropy_src_basetest::SHA256_STATUS_RESET, "SHA256_STATUS" },
        { entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_RESET, "HEALTH_TEST_WINDOW_SIZE" },
        { entropy_src_basetest::HT_WATERMARK_NUM_OFFSET, entropy_src_basetest::HT_WATERMARK_NUM_RESET, "HT_WATERMARK_NUM" },
        { entropy_src_basetest::HT_WATERMARK_OFFSET, entropy_src_basetest::HT_WATERMARK_RESET, "HT_WATERMARK" },
        { entropy_src_basetest::REPCNT_TOTAL_FAILS_OFFSET, entropy_src_basetest::REPCNT_TOTAL_FAILS_RESET, "REPCNT_TOTAL_FAILS" },
        { entropy_src_basetest::APT_HI_TOTAL_FAILS_OFFSET, entropy_src_basetest::APT_HI_TOTAL_FAILS_RESET, "APT_HI_TOTAL_FAILS" },
        { entropy_src_basetest::APT_LO_TOTAL_FAILS_OFFSET, entropy_src_basetest::APT_LO_TOTAL_FAILS_RESET, "APT_LO_TOTAL_FAILS" },
        { entropy_src_basetest::MARKOV_HI_TOTAL_FAILS_OFFSET, entropy_src_basetest::MARKOV_HI_TOTAL_FAILS_RESET, "MARKOV_HI_TOTAL_FAILS" },
        { entropy_src_basetest::MARKOV_LO_TOTAL_FAILS_OFFSET, entropy_src_basetest::MARKOV_LO_TOTAL_FAILS_RESET, "MARKOV_LO_TOTAL_FAILS" },
        { entropy_src_basetest::ALERT_SUMMARY_FAIL_COUNTS_OFFSET, entropy_src_basetest::ALERT_SUMMARY_FAIL_COUNTS_RESET, "ALERT_SUMMARY_FAIL_COUNTS" },
        { entropy_src_basetest::ALERT_FAIL_COUNTS_OFFSET, entropy_src_basetest::ALERT_FAIL_COUNTS_RESET, "ALERT_FAIL_COUNTS" }
    };

    for (const auto& r : regs) {
        test->register_read_32(r.offset, read_val);
        FUNC008_CHECK(read_val == r.expected,
            "Reset Value Check " << r.name << ": expected 0x" << std::hex << r.expected
            << " got 0x" << read_val);
    }

    // Generator Sample Clock Config registers
    for (int i = 0; i < 12; ++i) {
        unsigned int offset = entropy_src_basetest::GENERATOR_0_SAMPLE_CLK_CONFIG_OFFSET + (i * 4);
        uint32_t expected;
        switch(i) {
            case 0: case 1: case 6: case 7: expected = 0; break;
            case 2: case 3: case 8: case 9: expected = 1; break;
            case 4: case 5: case 10: case 11: expected = 2; break;
            default: expected = 0; break;
        }
        test->register_read_32(offset, read_val);
        FUNC008_CHECK(read_val == expected,
            "Reset Value Check GENERATOR_" << std::dec << i << "_SAMPLE_CLK_CONFIG: expected 0x" 
            << std::hex << expected << " got 0x" << read_val);
    }

    return ok;
}

// =============================================================================
// tc_f001_new_regs_rw_access — Verify RW access for RW registers
// =============================================================================
bool testbench::tc_f001_new_regs_rw_access()
{
    bool ok = true;
    uint32_t read_val;

    // 1. HEALTH_TEST_WINDOW_SIZE (RW mask 0x0000FFFF)
    test->register_write_32(entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, 0xFFFFFFFF);
    test->register_read_32(entropy_src_basetest::HEALTH_TEST_WINDOW_SIZE_OFFSET, read_val);
    FUNC008_CHECK(read_val == 0x0000FFFF,
        "RW Access HEALTH_TEST_WINDOW_SIZE: expected 0x0000FFFF got 0x" << std::hex << read_val);

    // 2. HT_WATERMARK_NUM (RW mask 0x0000000F)
    test->register_write_32(entropy_src_basetest::HT_WATERMARK_NUM_OFFSET, 0xFFFFFFFF);
    test->register_read_32(entropy_src_basetest::HT_WATERMARK_NUM_OFFSET, read_val);
    FUNC008_CHECK(read_val == 0x0000000F,
        "RW Access HT_WATERMARK_NUM: expected 0x0000000F got 0x" << std::hex << read_val);

    // 3. GENERATOR_x_SAMPLE_CLK_CONFIG (RW mask 0x0000001F)
    for (int i = 0; i < 12; ++i) {
        unsigned int offset = entropy_src_basetest::GENERATOR_0_SAMPLE_CLK_CONFIG_OFFSET + (i * 4);
        test->register_write_32(offset, 0xFFFFFFFF);
        test->register_read_32(offset, read_val);
        FUNC008_CHECK(read_val == 0x0000001F,
            "RW Access GENERATOR_" << std::dec << i << "_SAMPLE_CLK_CONFIG: expected 0x0000001F got 0x" 
            << std::hex << read_val);
    }

    return ok;
}

// =============================================================================
// tc_f001_new_regs_ro_enforcement — Verify RO enforcement for new RO registers
// =============================================================================
bool testbench::tc_f001_new_regs_ro_enforcement()
{
    bool ok = true;
    uint32_t read_val;

    struct RegCheck {
        unsigned int offset;
        uint32_t reset_val;
        const char* name;
    };

    RegCheck regs[] = {
        { entropy_src_basetest::SHA256_STATUS_OFFSET, entropy_src_basetest::SHA256_STATUS_RESET, "SHA256_STATUS" },
        { entropy_src_basetest::HT_WATERMARK_OFFSET, entropy_src_basetest::HT_WATERMARK_RESET, "HT_WATERMARK" },
        { entropy_src_basetest::REPCNT_TOTAL_FAILS_OFFSET, entropy_src_basetest::REPCNT_TOTAL_FAILS_RESET, "REPCNT_TOTAL_FAILS" },
        { entropy_src_basetest::APT_HI_TOTAL_FAILS_OFFSET, entropy_src_basetest::APT_HI_TOTAL_FAILS_RESET, "APT_HI_TOTAL_FAILS" },
        { entropy_src_basetest::APT_LO_TOTAL_FAILS_OFFSET, entropy_src_basetest::APT_LO_TOTAL_FAILS_RESET, "APT_LO_TOTAL_FAILS" },
        { entropy_src_basetest::MARKOV_HI_TOTAL_FAILS_OFFSET, entropy_src_basetest::MARKOV_HI_TOTAL_FAILS_RESET, "MARKOV_HI_TOTAL_FAILS" },
        { entropy_src_basetest::MARKOV_LO_TOTAL_FAILS_OFFSET, entropy_src_basetest::MARKOV_LO_TOTAL_FAILS_RESET, "MARKOV_LO_TOTAL_FAILS" },
        { entropy_src_basetest::ALERT_SUMMARY_FAIL_COUNTS_OFFSET, entropy_src_basetest::ALERT_SUMMARY_FAIL_COUNTS_RESET, "ALERT_SUMMARY_FAIL_COUNTS" },
        { entropy_src_basetest::ALERT_FAIL_COUNTS_OFFSET, entropy_src_basetest::ALERT_FAIL_COUNTS_RESET, "ALERT_FAIL_COUNTS" }
    };

    for (const auto& r : regs) {
        test->register_write_32(r.offset, 0xFFFFFFFF);
        test->register_read_32(r.offset, read_val);
        FUNC008_CHECK(read_val == r.reset_val,
            "RO Enforcement " << r.name << ": expected 0x" << std::hex << r.reset_val
            << " got 0x" << read_val << " (write should have no effect)");
    }

    return ok;
}
