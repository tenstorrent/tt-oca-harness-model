// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * HMAC P2 Sensitive Register Access Test.
 *
 * Verifies security-sensitive register access behavior exposed by the current
 * HMAC register map:
 *   1) KEY registers are write-only: reads must return zero.
 *   2) DIGEST registers are HW-driven outside context restore: writes must not echo.
 *   3) CFG_REGWEN is not present in the current HMAC map; record this as N/A.
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_hmac_p2_sensreg_access_test STACK=cgen,sim
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

static int check_true(const char *name, int condition)
{
    printf("  %s - %s\n", name, condition ? "PASS" : "FAIL");
    return condition;
}

static int test_key_read_protection(void)
{
    printf("\nStep 1: KEY register read-as-zero protection\n");

    int pass = 1;
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t addr = HMAC_KEY_0__REG_ADDR + i * 4u;
        uint32_t pattern = 0xa5a50000u | (i * 0x1111u) | i;

        WRITE_REG(addr, pattern);
        uint32_t rb = READ_REG(addr);
        printf("  KEY_%u wrote=0x%08x read=0x%08x\n", i, pattern, rb);

        if (rb != 0) {
            printf("  FAIL: KEY_%u read-as-zero violation: got 0x%08x\n", i, rb);
            pass = 0;
        }
    }

    return pass ? 0 : -1;
}

static int test_digest_write_non_echo(void)
{
    printf("\nStep 2: DIGEST write outside context restore must not echo\n");

    int pass = 1;
    for (uint32_t i = 0; i < 8; i++) {
        uint32_t addr = HMAC_DIGEST_0__REG_ADDR + i * 4u;
        uint32_t before = READ_REG(addr);
        uint32_t pattern = 0x5a5a0000u | (i * 0x0101u) | i;

        WRITE_REG(addr, pattern);
        uint32_t after = READ_REG(addr);
        printf("  DIGEST_%u before=0x%08x wrote=0x%08x after=0x%08x\n",
               i, before, pattern, after);

        if (after == pattern) {
            printf("  FAIL: DIGEST_%u echoed SW write outside context restore\n", i);
            pass = 0;
        }
    }

    return pass ? 0 : -1;
}

static int test_cfg_regwen_absent(void)
{
    printf("\nStep 3: CFG_REGWEN lock behavior\n");
    printf("  INFO: HMAC_CFG_REGWEN is not present in och_sep_top_reg.h; step is N/A\n");

    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.sha_en = 1;
    cfg.f.hmac_en = 0;
    cfg.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CFG_reg_u rb = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    printf("  CFG write/readback without regwen: wrote=0x%08x read=0x%08x\n", cfg.val, rb.val);

    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    return check_true("CFG remains writable because no regwen register exists", rb.val == cfg.val);
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("HMAC P2 Sensitive Register Access Test\n");
    printf("========================================\n");

    int pass = 1;

    if (test_key_read_protection() != 0) {
        pass = 0;
    }
    if (pass && test_digest_write_non_echo() != 0) {
        pass = 0;
    }
    if (pass && test_cfg_regwen_absent() != 0) {
        pass = 0;
    }

    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xffffffffu);
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, 0);

    printf("\n========================================\n");
    if (pass) {
        printf("=== HMAC P2 SENSITIVE REGISTER ACCESS TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== HMAC P2 SENSITIVE REGISTER ACCESS TEST FAILED ===\n");
        test_fail(1);
    }
    printf("========================================\n");

    while (1) {
        __asm__("wfi");
    }

    return pass ? 0 : -1;
}
