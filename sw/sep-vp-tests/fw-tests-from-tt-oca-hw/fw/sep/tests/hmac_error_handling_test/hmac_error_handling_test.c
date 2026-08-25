// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * HMAC Error Detection Test - TC_HMAC_006 (P0)
 *
 * Verifies ERR_CODE reporting for three error conditions:
 *   - Push MSG_FIFO when sha_en=0: generates SwPushMsgWhenDisallowed (ERR_CODE=0x5)
 *     Note: prim_sha2_pkg.sv marks SwPushMsgWhenShaDisabled(0x1) as "not used in this version";
 *     HW generates 0x5 (SwPushMsgWhenDisallowed) for this condition instead.
 *   - SwHashStartWhenShaDisabled (ERR_CODE=0x2)
 *   - SwHashStartWhenActive (ERR_CODE=0x4)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_hmac_error_handling_test STACK=sim
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

static int check_reg(const char *name, uint32_t actual, uint32_t expected)
{
    int ok = (actual == expected);
    printf("  %s: 0x%08x (expected 0x%08x) - %s\n",
           name, actual, expected, ok ? "PASS" : "FAIL");
    return ok;
}

static int wait_hmac_done(void)
{
    int timeout = 1000000;
    while (timeout-- > 0) {
        HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};
        HMAC_STATUS_reg_u sts = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        if (intr.f.hmac_done || sts.f.hmac_idle) {
            break;
        }
    }
    if (timeout <= 0) {
        printf("  Timeout waiting for HMAC completion\n");
        return -1;
    }
    HMAC_INTR_STATE_reg_u clear = {.f.hmac_done = 1};
    WRITE_REG(HMAC_INTR_STATE_REG_ADDR, clear.val);
    return 0;
}

static int hmac_reset_via_hash(void)
{
    HMAC_CFG_reg_u cfg = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    cfg.f.sha_en = 1;
    cfg.f.hmac_en = 0;
    cfg.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CMD_reg_u cmd = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    HMAC_CMD_reg_u cmd_proc = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd_proc.val);

    if (wait_hmac_done() != 0) return -1;

    HMAC_INTR_STATE_reg_u clr_all = {.val = 0};
    clr_all.f.hmac_done = 1;
    clr_all.f.fifo_empty = 1;
    clr_all.f.hmac_err = 1;
    WRITE_REG(HMAC_INTR_STATE_REG_ADDR, clr_all.val);

    return 0;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("HMAC Error Handling Test (TC_HMAC_006)\n");
    printf("========================================\n\n");

    int pass = 1;

    /* ---------------------------------------------------------- */
    /* Error 1: Push MSG_FIFO when sha_en=0 */
    /* prim_sha2_pkg.sv: SwPushMsgWhenShaDisabled(0x1) "not used in this version" */
    /* HW generates SwPushMsgWhenDisallowed(0x5) for this condition. */
    /* ---------------------------------------------------------- */
    printf("Step 1: Push MSG_FIFO when sha_en=0 (expect ERR_CODE=0x5 SwPushMsgWhenDisallowed)\n");

    HMAC_CFG_reg_u cfg = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    volatile uint8_t *fifo8 = (volatile uint8_t *)(uintptr_t)HMAC_MSG_FIFO_MEM_BASE_ADDR;
    *fifo8 = 0xAA;

    uint32_t err = READ_REG(HMAC_ERR_CODE_REG_ADDR);
    if (!check_reg("ERR_CODE (push when sha_en=0 -> SwPushMsgWhenDisallowed)", err, 0x5)) pass = 0;

    HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};
    printf("  INTR_STATE.hmac_err=%u (informational)\n", intr.f.hmac_err);

    printf("  Resetting HMAC block...\n");
    if (hmac_reset_via_hash() != 0) {
        printf("  Reset failed\n");
        pass = 0;
    }

    err = READ_REG(HMAC_ERR_CODE_REG_ADDR);
    printf("  ERR_CODE after reset: 0x%08x\n", err);

    /* ---------------------------------------------------------- */
    /* Error 2: SwHashStartWhenShaDisabled (ERR_CODE = 0x2) */
    /* ---------------------------------------------------------- */
    printf("\nStep 2: SwHashStartWhenShaDisabled (expect ERR_CODE=0x2)\n");

    cfg.val = READ_REG(HMAC_CFG_REG_ADDR);
    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CMD_reg_u cmd = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    err = READ_REG(HMAC_ERR_CODE_REG_ADDR);
    if (!check_reg("ERR_CODE (SwHashStartWhenShaDisabled)", err, 0x2)) pass = 0;

    intr.val = READ_REG(HMAC_INTR_STATE_REG_ADDR);
    printf("  INTR_STATE.hmac_err=%u (informational)\n", intr.f.hmac_err);

    printf("  Resetting HMAC block...\n");
    if (hmac_reset_via_hash() != 0) {
        printf("  Reset failed\n");
        pass = 0;
    }

    err = READ_REG(HMAC_ERR_CODE_REG_ADDR);
    printf("  ERR_CODE after reset: 0x%08x\n", err);

    /* ---------------------------------------------------------- */
    /* Error 3: SwHashStartWhenActive (ERR_CODE = 0x4) */
    /* ---------------------------------------------------------- */
    printf("\nStep 3: SwHashStartWhenActive (expect ERR_CODE=0x4)\n");

    cfg.val = 0;
    cfg.f.sha_en = 1;
    cfg.f.hmac_en = 0;
    cfg.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    cmd.val = 0;
    cmd.f.hash_start = 1;
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    cmd.val = 0;
    cmd.f.hash_start = 1;
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    err = READ_REG(HMAC_ERR_CODE_REG_ADDR);
    if (!check_reg("ERR_CODE (SwHashStartWhenActive)", err, 0x4)) pass = 0;

    intr.val = READ_REG(HMAC_INTR_STATE_REG_ADDR);
    printf("  INTR_STATE.hmac_err=%u (informational)\n", intr.f.hmac_err);

    printf("  Final cleanup: hash_process and wait...\n");
    HMAC_CMD_reg_u cmd_proc = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd_proc.val);
    if (wait_hmac_done() != 0) {
        printf("  Final cleanup wait failed\n");
        pass = 0;
    }

    cfg.val = READ_REG(HMAC_CFG_REG_ADDR);
    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);
    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xFFFFFFFFu);
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, 0);

    HMAC_INTR_STATE_reg_u clr_all = {.val = 0};
    clr_all.f.hmac_done = 1;
    clr_all.f.fifo_empty = 1;
    clr_all.f.hmac_err = 1;
    WRITE_REG(HMAC_INTR_STATE_REG_ADDR, clr_all.val);

    printf("\n========================================\n");
    if (pass) {
        printf("=== HMAC ERROR HANDLING TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== HMAC ERROR HANDLING TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
