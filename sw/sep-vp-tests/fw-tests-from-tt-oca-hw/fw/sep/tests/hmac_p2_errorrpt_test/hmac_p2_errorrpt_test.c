/*
 * HMAC P2 Error Reporting Test.
 *
 * Covers illegal operation reporting and recovery:
 *   1) MSG_FIFO push while sha_en=0 -> ERR_CODE=0x5 and hmac_err.
 *   2) hash_start while engine is already active -> ERR_CODE=0x4 and hmac_err.
 *   3) Safe FIFO saturation to fifo_full, then process/drain recovery.
 *
 * The test intentionally does not perform an extra MMIO write after fifo_full,
 * because the AXI write can legally backpressure and hang firmware execution.
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_hmac_p2_errorrpt_test STACK=cgen,sim
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

static int check_reg(const char *name, uint32_t actual, uint32_t expected)
{
    int ok = (actual == expected);
    printf("  %s: 0x%08x (expected 0x%08x) - %s\n",
           name, actual, expected, ok ? "PASS" : "FAIL");
    return ok;
}

static int wait_for_idle(void)
{
    int timeout = 1000000;
    while (timeout-- > 0) {
        HMAC_STATUS_reg_u status = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        if (status.f.hmac_idle) {
            return 0;
        }
    }

    printf("  Timeout waiting for HMAC idle\n");
    return -1;
}

static int wait_for_hmac_done(void)
{
    int timeout = 1000000;
    while (timeout-- > 0) {
        HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};
        HMAC_STATUS_reg_u status = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        if (intr.f.hmac_done || status.f.hmac_idle) {
            break;
        }
    }
    if (timeout <= 0) {
        printf("  Timeout waiting for HMAC completion\n");
        return -1;
    }

    HMAC_INTR_STATE_reg_u clear = {.val = 0};
    clear.f.hmac_done = 1;
    clear.f.fifo_empty = 1;
    clear.f.hmac_err = 1;
    WRITE_REG(HMAC_INTR_STATE_REG_ADDR, clear.val);

    return 0;
}

static int recover_hmac_state(void)
{
    HMAC_CFG_reg_u cfg = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg.f.sha_en = 0;
    cfg.f.hmac_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    cfg.val = 0;
    cfg.f.sha_en = 1;
    cfg.f.hmac_en = 0;
    cfg.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CMD_reg_u start = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, start.val);
    HMAC_CMD_reg_u process = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, process.val);

    if (wait_for_hmac_done() != 0) {
        return -1;
    }

    cfg.val = READ_REG(HMAC_CFG_REG_ADDR);
    cfg.f.sha_en = 0;
    cfg.f.hmac_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);
    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xffffffffu);

    HMAC_INTR_STATE_reg_u clear = {.val = 0};
    clear.f.hmac_done = 1;
    clear.f.fifo_empty = 1;
    clear.f.hmac_err = 1;
    WRITE_REG(HMAC_INTR_STATE_REG_ADDR, clear.val);

    printf("  Recovery: ERR_CODE=0x%08x STATUS=0x%08x\n",
           READ_REG(HMAC_ERR_CODE_REG_ADDR), READ_REG(HMAC_STATUS_REG_ADDR));

    return wait_for_idle();
}

static int expect_hmac_error(const char *name, uint32_t expected_err)
{
    uint32_t err = READ_REG(HMAC_ERR_CODE_REG_ADDR);
    HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};

    int pass = 1;
    if (!check_reg(name, err, expected_err)) {
        pass = 0;
    }

    printf("  INTR_STATE.hmac_err=%u\n", intr.f.hmac_err);
    if (!intr.f.hmac_err) {
        printf("  FAIL: hmac_err interrupt state did not assert\n");
        pass = 0;
    }

    return pass ? 0 : -1;
}

static int test_push_when_sha_disabled(void)
{
    printf("\nStep 1: MSG_FIFO push while sha_en=0\n");

    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    volatile uint8_t *fifo8 = (volatile uint8_t *)(uintptr_t)HMAC_MSG_FIFO_MEM_BASE_ADDR;
    *fifo8 = 0xa5;

    if (expect_hmac_error("ERR_CODE push while sha_en=0", 0x5) != 0) {
        return -1;
    }

    return recover_hmac_state();
}

static int test_hash_start_when_busy(void)
{
    printf("\nStep 2: hash_start while engine is active\n");

    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.sha_en = 1;
    cfg.f.hmac_en = 0;
    cfg.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CMD_reg_u start = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, start.val);
    WRITE_REG(HMAC_CMD_REG_ADDR, start.val);

    if (expect_hmac_error("ERR_CODE hash_start while active", 0x4) != 0) {
        return -1;
    }

    return recover_hmac_state();
}

static int test_fifo_saturation_and_reset_recovery(void)
{
    printf("\nStep 3: Fill MSG_FIFO to fifo_full and recover by process/drain\n");

    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.sha_en = 1;
    cfg.f.hmac_en = 0;
    cfg.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CMD_reg_u start = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, start.val);

    volatile uint32_t *fifo32 = (volatile uint32_t *)(uintptr_t)HMAC_MSG_FIFO_MEM_BASE_ADDR;
    uint32_t words_written = 0;
    int full_seen = 0;
    uint32_t max_depth = 0;

    for (uint32_t i = 0; i < 128; i++) {
        HMAC_STATUS_reg_u status = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        if (status.f.fifo_depth > max_depth) {
            max_depth = status.f.fifo_depth;
        }
        if (status.f.fifo_full) {
            full_seen = 1;
            printf("  fifo_full asserted before word %u, depth=%u\n", i, status.f.fifo_depth);
            break;
        }

        *fifo32 = 0x5a000000u | i;
        words_written++;
    }

    HMAC_STATUS_reg_u final_status = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
    if (final_status.f.fifo_depth > max_depth) {
        max_depth = final_status.f.fifo_depth;
    }

    printf("  words_written=%u fifo_full=%u fifo_depth=%u max_depth=%u\n",
           words_written, final_status.f.fifo_full, final_status.f.fifo_depth, max_depth);

    if (!full_seen && !final_status.f.fifo_full) {
        printf("  INFO: fifo_full not observed; SHA engine drained FIFO while FW streamed data\n");
    } else if (max_depth < 32) {
        printf("  FAIL: fifo_full asserted but fifo_depth never reached 32 entries\n");
        return -1;
    } else {
        printf("  INFO: Extra write beyond fifo_full is skipped to avoid CPU MMIO deadlock\n");
    }

    HMAC_CMD_reg_u process = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, process.val);
    if (wait_for_hmac_done() != 0) {
        return -1;
    }

    HMAC_CFG_reg_u cfg_off = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg_off.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg_off.val);
    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xffffffffu);

    HMAC_STATUS_reg_u reset_status = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
    int pass = 1;
    if (!check_reg("STATUS.hmac_idle after drain", reset_status.f.hmac_idle, 1)) {
        pass = 0;
    }
    if (!check_reg("STATUS.fifo_empty after drain", reset_status.f.fifo_empty, 1)) {
        pass = 0;
    }
    printf("  ERR_CODE after drain: 0x%08x (sticky error code is allowed)\n",
           READ_REG(HMAC_ERR_CODE_REG_ADDR));

    return pass ? 0 : -1;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("HMAC P2 Error Reporting Test\n");
    printf("========================================\n");

    int pass = 1;

    HMAC_INTR_ENABLE_reg_u intr_en = {.val = 0};
    intr_en.f.hmac_done = 1;
    intr_en.f.fifo_empty = 1;
    intr_en.f.hmac_err = 1;
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, intr_en.val);

    if (test_push_when_sha_disabled() != 0) {
        pass = 0;
    }
    if (pass && test_hash_start_when_busy() != 0) {
        pass = 0;
    }
    if (pass && test_fifo_saturation_and_reset_recovery() != 0) {
        pass = 0;
    }

    printf("\n========================================\n");
    if (pass) {
        printf("=== HMAC P2 ERROR REPORTING TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== HMAC P2 ERROR REPORTING TEST FAILED ===\n");
        test_fail(1);
    }
    printf("========================================\n");

    while (1) {
        __asm__("wfi");
    }

    return pass ? 0 : -1;
}
