// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * KMAC P2 Emergency Wipe Test.
 *
 * KMAC does not expose a dedicated WIPE_SECRET register in this SEP map. Use
 * the SEP reset controller KMAC software reset as the emergency wipe path:
 *   1) Generate non-zero KMAC state from a SHA3 operation.
 *   2) Assert and release KMAC SW reset.
 *   3) Verify reset defaults and functional recovery.
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_kmac_p2_emergency_wipe_test STACK=cgen,sim
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

#define RST_KMAC  (1u << 4)

static int wait_for_idle(void)
{
    int timeout = 1000000;
    while (timeout-- > 0) {
        KMAC_STATUS_reg_u status = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
        if (status.f.sha3_idle) {
            return 0;
        }
    }

    printf("  Timeout waiting for KMAC idle\n");
    return -1;
}

static int wait_for_done(void)
{
    int timeout = 1000000;
    while (timeout-- > 0) {
        KMAC_INTR_STATE_reg_u intr = {.val = READ_REG(KMAC_INTR_STATE_REG_ADDR)};
        if (intr.f.kmac_done) {
            WRITE_REG(KMAC_INTR_STATE_REG_ADDR, intr.val);
            return 0;
        }
    }

    printf("  Timeout waiting for KMAC done\n");
    return -1;
}

static void seed_entropy(void)
{
    for (int i = 0; i < 6; i++) {
        WRITE_REG(KMAC_ENTROPY_SEED_REG_ADDR, 0xdeadbeefu + (uint32_t)i);
    }
}

static void write_cfg_shadowed(KMAC_CFG_SHADOWED_reg_u cfg)
{
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
}

static int run_sha3(uint32_t *digest0_out)
{
    if (wait_for_idle() != 0) {
        return -1;
    }

    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = 0;
    cfg.f.mode = 0x0;
    cfg.f.kstrength = 0x2;
    cfg.f.entropy_mode = 0x1;
    write_cfg_shadowed(cfg);

    seed_entropy();
    cfg.f.entropy_ready = 1;
    write_cfg_shadowed(cfg);

    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = 29;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x74736574u);
    cmd.f.cmd = 46;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    if (wait_for_done() != 0) {
        return -1;
    }

    uint32_t digest0 = READ_REG(KMAC_STATE_MEM_BASE_ADDR)
                     ^ READ_REG(KMAC_STATE_MEM_BASE_ADDR + 0x100);
    printf("  digest0=0x%08x\n", digest0);
    *digest0_out = digest0;

    cmd.f.cmd = 22;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    return (digest0 != 0) ? 0 : -1;
}

static void pulse_kmac_reset(void)
{
    uint32_t sw_reset_n = READ_REG(SEP_RESET_CTRL_SW_RESET_N_REG_ADDR);

    WRITE_REG(SEP_RESET_CTRL_SW_RESET_N_REG_ADDR, sw_reset_n & ~RST_KMAC);
    for (volatile int i = 0; i < 64; i++) {
        __asm__ volatile("nop");
    }
    WRITE_REG(SEP_RESET_CTRL_SW_RESET_N_REG_ADDR, sw_reset_n | RST_KMAC);
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("KMAC P2 Emergency Wipe Test\n");
    printf("========================================\n");

    int pass = 1;
    uint32_t digest_before = 0;
    uint32_t digest_after = 0;

    printf("\nStep 1: Produce non-zero KMAC state\n");
    if (run_sha3(&digest_before) != 0) {
        printf("  FAIL: initial SHA3 operation failed\n");
        pass = 0;
    }

    printf("\nStep 2: Assert/release KMAC SW reset as emergency wipe\n");
    pulse_kmac_reset();
    if (wait_for_idle() != 0) {
        pass = 0;
    }

    KMAC_STATUS_reg_u status = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
    KMAC_CFG_REGWEN_reg_u regwen = {.val = READ_REG(KMAC_CFG_REGWEN_REG_ADDR)};
    printf("  STATUS=0x%08x idle=%u empty=%u CFG_REGWEN=%u\n",
           status.val, status.f.sha3_idle, status.f.fifo_empty, regwen.f.en);
    if (!status.f.sha3_idle || !status.f.fifo_empty || !regwen.f.en) {
        printf("  FAIL: KMAC did not return to reset defaults after emergency wipe\n");
        pass = 0;
    }

    printf("\nStep 3: Functional recovery after wipe\n");
    if (pass && run_sha3(&digest_after) != 0) {
        printf("  FAIL: post-wipe SHA3 operation failed\n");
        pass = 0;
    }

    printf("  digest_before=0x%08x digest_after=0x%08x\n", digest_before, digest_after);

    printf("\n========================================\n");
    if (pass) {
        printf("=== KMAC P2 EMERGENCY WIPE TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== KMAC P2 EMERGENCY WIPE TEST FAILED ===\n");
        test_fail(1);
    }
    printf("========================================\n");

    while (1) {
        __asm__("wfi");
    }

    return pass ? 0 : -1;
}
