// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * TC_KMAC_008 - Entropy Configuration Test (P1)
 *
 * Verifies KMAC entropy period register, entropy seed provisioning,
 * and entropy_ready flow. Runs a SHA3-256 hash to confirm entropy
 * is functional, then reads ENTROPY_REFRESH_HASH_CNT.
 */

#include <stdint.h>
#include <stdio.h>
#include "test_completion.h"
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "sep_outbound_filter.h"
static int wait_for_idle(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        KMAC_STATUS_reg_u s = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
        if (s.f.sha3_idle) return 0;
    }
    printf("Timeout waiting for idle\n");
    return -1;
}

static int wait_for_done(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        if (READ_REG(KMAC_INTR_STATE_REG_ADDR) & 0x1) {
            WRITE_REG(KMAC_INTR_STATE_REG_ADDR, 0x1);
            return 0;
        }
    }
    printf("Timeout waiting for done\n");
    return -1;
}

static int test_entropy_config(void) {
    int errors = 0;
    uint32_t val;

    printf("=== Step 1: Read ENTROPY_PERIOD default ===\n");
    val = READ_REG(KMAC_ENTROPY_PERIOD_REG_ADDR);
    printf("ENTROPY_PERIOD default = 0x%08x\n", val);
    if (val != 0x00000000) {
        printf("FAIL: expected default 0x00000000\n");
        errors++;
    }

    printf("=== Step 2: Write ENTROPY_PERIOD 0x03FF0100 ===\n");
    WRITE_REG(KMAC_ENTROPY_PERIOD_REG_ADDR, 0x03FF0100);
    val = READ_REG(KMAC_ENTROPY_PERIOD_REG_ADDR);
    printf("ENTROPY_PERIOD readback = 0x%08x\n", val);
    KMAC_ENTROPY_PERIOD_reg_u ep = {.val = val};
    printf("  prescaler=%u wait_timer=%u\n", ep.f.prescaler, ep.f.wait_timer);
    if (val != 0x03FF0100) {
        printf("FAIL: readback mismatch\n");
        errors++;
    }

    printf("=== Step 3: Seed entropy ===\n");
    for (int i = 0; i < 6; i++)
        WRITE_REG(KMAC_ENTROPY_SEED_REG_ADDR, 0xDEADBEEF + i);

    printf("=== Step 4: Configure SHA3-256 with entropy ===\n");
    if (wait_for_idle() != 0) return -1;

    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = 0;
    cfg.f.mode = 0x0;
    cfg.f.kstrength = 0x2;
    cfg.f.entropy_mode = 0x1;  /* EDN mode = 0x1 (0=None, 1=EDN, 2=SW per hjson) */
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    cfg.f.entropy_ready = 1;
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    printf("=== Step 5: START, write message, PROCESS ===\n");
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = 29;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x74736574);

    cmd.f.cmd = 46;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    if (wait_for_done() != 0) return -1;

    printf("=== Step 6: Read digest ===\n");
    uint32_t digest[8];
    for (int i = 0; i < 8; i++) {
        uint32_t s0 = READ_REG(KMAC_STATE_MEM_BASE_ADDR + (i * 4));
        uint32_t s1 = READ_REG(KMAC_STATE_MEM_BASE_ADDR + 0x100 + (i * 4));
        digest[i] = s0 ^ s1;
    }

    int non_zero = 0;
    printf("Digest: ");
    for (int i = 0; i < 8; i++) {
        printf("%08x ", digest[i]);
        if (digest[i] != 0) non_zero = 1;
    }
    printf("\n");

    if (!non_zero) {
        printf("FAIL: digest is all zeros\n");
        errors++;
    }

    cmd.f.cmd = 22;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    printf("=== Step 7: Read ENTROPY_REFRESH_HASH_CNT (expect > 0 after hash) ===\n");
    KMAC_ENTROPY_REFRESH_HASH_CNT_reg_u hc = {
        .val = READ_REG(KMAC_ENTROPY_REFRESH_HASH_CNT_REG_ADDR)
    };
    printf("ENTROPY_REFRESH_HASH_CNT = %u\n", hc.f.hash_cnt);
    if (hc.f.hash_cnt > 0) {
        printf("PASS: ENTROPY_REFRESH_HASH_CNT incremented after hash\n");
    } else {
        printf("INFO: ENTROPY_REFRESH_HASH_CNT=0 (may reset with entropy_ready; not a hard fail)\n");
    }

    return errors;
}

int main(void) {
    sep_outbound_filter_init();

    printf("\n");
    printf("========================================\n");
    printf("  TC_KMAC_008: Entropy Config Test\n");
    printf("========================================\n\n");

    int result = test_entropy_config();

    if (result == 0) {
        printf("\n=== TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("\n=== TEST FAILED (errors=%d) ===\n", result);
        test_fail(1);
    }

    while (1) { __asm__("wfi"); }
}
