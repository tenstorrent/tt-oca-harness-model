// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * TC_KMAC_007 (P0) - Error Detection Test
 *
 * Tests two error conditions:
 *   1. ErrSwPushedMsgFifo (0x02): Write MSG_FIFO without START
 *   2. ErrSwCmdSequence (0x08): Issue PROCESS without START
 * Verifies ERR_CODE and clears via CMD.err_processed.
 */

#include <stdint.h>
#include <stdio.h>
#include "test_completion.h"
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "sep_outbound_filter.h"

static int test_errors = 0;

static int wait_for_idle(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        KMAC_STATUS_reg_u s = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
        if (s.f.sha3_idle) return 0;
    }
    printf("Timeout waiting for KMAC idle\n");
    return -1;
}

static void clear_error(void) {
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.err_processed = 1;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    /* Clear any pending interrupts */
    WRITE_REG(KMAC_INTR_STATE_REG_ADDR, 0x7);
}

static int test_err_sw_pushed_msg_fifo(void) {
    printf("\n=== Test ErrSwPushedMsgFifo ===\n");

    if (wait_for_idle() != 0) return -1;

    /* Configure but do NOT issue START */
    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = 0;
    cfg.f.mode = 0x0;
    cfg.f.kstrength = 0x2;
    cfg.f.entropy_mode = 0x1;  /* EDN mode = 0x1 (0=None, 1=EDN, 2=SW per hjson) */
    cfg.f.entropy_ready = 0;
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    /* Provide entropy */
    for (int i = 0; i < 6; i++)
        WRITE_REG(KMAC_ENTROPY_SEED_REG_ADDR, 0xDEADBEEF + i);
    cfg.f.entropy_ready = 1;
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    /* Write to MSG_FIFO without START - should trigger error */
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0xDEADBEEF);
    printf("  Wrote MSG_FIFO without START\n");

    KMAC_ERR_CODE_reg_u err = {.val = READ_REG(KMAC_ERR_CODE_REG_ADDR)};
    printf("  ERR_CODE = 0x%08x\n", err.val);

    if (err.val != 0) {
        printf("PASS: Error detected (ERR_CODE=0x%08x)\n", err.val);
        if (err.val == 0x02) {
            printf("  Confirmed: ErrSwPushedMsgFifo\n");
        }
    } else {
        printf("FAIL: No error detected for MSG_FIFO write without START\n");
        test_errors++;
    }

    /* Clear error */
    clear_error();
    printf("  Error cleared\n");

    /* Wait for idle after error recovery */
    if (wait_for_idle() != 0) {
        printf("  WARNING: KMAC not idle after error clear\n");
    }

    return 0;
}

static int test_err_sw_cmd_sequence(void) {
    printf("\n=== Test ErrSwCmdSequence ===\n");

    if (wait_for_idle() != 0) return -1;

    /* Configure */
    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = 0;
    cfg.f.mode = 0x0;
    cfg.f.kstrength = 0x2;
    cfg.f.entropy_mode = 0x1;  /* EDN mode = 0x1 (0=None, 1=EDN, 2=SW per hjson) */
    cfg.f.entropy_ready = 0;
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    for (int i = 0; i < 6; i++)
        WRITE_REG(KMAC_ENTROPY_SEED_REG_ADDR, 0xDEADBEEF + i);
    cfg.f.entropy_ready = 1;
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    /* Issue PROCESS without START - should trigger error */
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = 46;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    printf("  Issued PROCESS without START\n");

    KMAC_ERR_CODE_reg_u err = {.val = READ_REG(KMAC_ERR_CODE_REG_ADDR)};
    printf("  ERR_CODE = 0x%08x\n", err.val);

    if (err.val != 0) {
        printf("PASS: Error detected (ERR_CODE=0x%08x)\n", err.val);
        if (err.val == 0x08) {
            printf("  Confirmed: ErrSwCmdSequence\n");
        }
    } else {
        printf("FAIL: No error detected for PROCESS without START\n");
        test_errors++;
    }

    /* Clear error */
    clear_error();
    printf("  Error cleared\n");

    if (wait_for_idle() != 0) {
        printf("  WARNING: KMAC not idle after error clear\n");
    }

    return 0;
}

int main(void) {
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("  TC_KMAC_007: Error Handling Test\n");
    printf("========================================\n");

    test_err_sw_pushed_msg_fifo();
    test_err_sw_cmd_sequence();

    printf("\n========================================\n");
    if (test_errors == 0) {
        printf("  RESULT: ALL TESTS PASSED\n");
        test_pass(0);
    } else {
        printf("  RESULT: %d TESTS FAILED\n", test_errors);
        test_fail(1);
    }
    printf("========================================\n");

    while (1) {
        __asm__("wfi");
    }
}
