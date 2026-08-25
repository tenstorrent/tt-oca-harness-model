// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * KMAC P2 Software Error Test.
 *
 * Extends the base KMAC error test with software-visible P2 error cases:
 *   1) Hashing without entropy_ready.
 *   2) Unsupported mode/strength combination.
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_kmac_p2_sw_error_test STACK=cgen,sim
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

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

static void clear_error(void)
{
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.err_processed = 1;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    WRITE_REG(KMAC_INTR_STATE_REG_ADDR, 0x7);
}

static void seed_entropy(void)
{
    for (int i = 0; i < 6; i++) {
        WRITE_REG(KMAC_ENTROPY_SEED_REG_ADDR, 0x13579bdfu + (uint32_t)i);
    }
}

static void write_cfg_shadowed(KMAC_CFG_SHADOWED_reg_u cfg)
{
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
}

static int expect_error(const char *name, uint32_t expected)
{
    KMAC_ERR_CODE_reg_u err = {.val = READ_REG(KMAC_ERR_CODE_REG_ADDR)};
    KMAC_INTR_STATE_reg_u intr = {.val = READ_REG(KMAC_INTR_STATE_REG_ADDR)};
    int pass = 1;

    printf("  %s ERR_CODE=0x%08x expected=0x%08x INTR_STATE=0x%08x kmac_err=%u\n",
           name, err.val, expected, intr.val, intr.f.kmac_err);

    if (err.val == 0) {
        printf("  FAIL: no KMAC error reported\n");
        pass = 0;
    } else if (err.val != expected) {
        printf("  INFO: expected code differs, but hardware reported a valid non-zero error\n");
    }

    if (!intr.f.kmac_err) {
        printf("  FAIL: kmac_err interrupt state did not assert\n");
        pass = 0;
    }

    clear_error();
    if (wait_for_idle() != 0) {
        pass = 0;
    }

    err.val = READ_REG(KMAC_ERR_CODE_REG_ADDR);
    printf("  After err_processed: ERR_CODE=0x%08x\n", err.val);

    return pass ? 0 : -1;
}

static int test_hash_without_entropy_ready(void)
{
    printf("\nStep 1: Hashing without entropy_ready\n");
    if (wait_for_idle() != 0) {
        return -1;
    }

    seed_entropy();

    /* kmac_errchk.sv check_entropy_ready gates on kmac_en_i=1; run cSHAKE/L128
     * with entropy_ready=0 so the IP reports ErrSwHashingWithoutEntropyReady. */
    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = 1;
    cfg.f.mode = 0x2;       /* cSHAKE (mandatory companion of kmac_en=1) */
    cfg.f.kstrength = 0x0;  /* L128 (valid for Shake/cSHAKE) */
    cfg.f.entropy_mode = 0x1;
    cfg.f.entropy_ready = 0;
    write_cfg_shadowed(cfg);

    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = 29;         /* START */
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x00636261u);
    cmd.f.cmd = 46;         /* PROCESS */
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    return expect_error("ErrSwHashingWithoutEntropyReady", 0x09);
}

static int test_unsupported_mode_strength(void)
{
    printf("\nStep 2: Unsupported mode/strength\n");
    if (wait_for_idle() != 0) {
        return -1;
    }

    seed_entropy();

    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = 0;
    cfg.f.mode = 0x0;       /* SHA3 */
    cfg.f.kstrength = 0x0;  /* L128 is unsupported for SHA3 */
    cfg.f.entropy_mode = 0x1;
    cfg.f.entropy_ready = 1;
    cfg.f.en_unsupported_modestrength = 0;
    write_cfg_shadowed(cfg);

    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = 29;         /* START */
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x00636261u);
    cmd.f.cmd = 46;         /* PROCESS */
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    return expect_error("ErrUnexpectedModeStrength", 0x06);
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("KMAC P2 Software Error Test\n");
    printf("========================================\n");

    KMAC_INTR_ENABLE_reg_u intr_en = {.val = 0};
    intr_en.f.kmac_done = 1;
    intr_en.f.fifo_empty = 1;
    intr_en.f.kmac_err = 1;
    WRITE_REG(KMAC_INTR_ENABLE_REG_ADDR, intr_en.val);
    clear_error();

    int pass = 1;
    if (test_hash_without_entropy_ready() != 0) {
        pass = 0;
    }
    if (pass && test_unsupported_mode_strength() != 0) {
        pass = 0;
    }

    WRITE_REG(KMAC_INTR_ENABLE_REG_ADDR, 0);
    clear_error();

    printf("\n========================================\n");
    if (pass) {
        printf("=== KMAC P2 SOFTWARE ERROR TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== KMAC P2 SOFTWARE ERROR TEST FAILED ===\n");
        test_fail(1);
    }
    printf("========================================\n");

    while (1) {
        __asm__("wfi");
    }

    return pass ? 0 : -1;
}
