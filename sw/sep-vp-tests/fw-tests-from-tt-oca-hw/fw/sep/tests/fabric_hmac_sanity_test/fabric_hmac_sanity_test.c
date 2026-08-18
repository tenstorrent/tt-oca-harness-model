/*
 * Fabric HMAC Sanity Test - TC_FABRIC_012
 *
 * Lightweight firmware check for the SEP fabric path to HMAC registers.
 */

#include <stdint.h>
#include <stdio.h>

#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

static int check_bit(const char *name, uint32_t value)
{
    printf("%s: %u - %s\n", name, value, value ? "PASS" : "FAIL");
    return value ? 1 : 0;
}

int main(void)
{
    int pass = 1;

    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("Fabric HMAC Sanity Test (TC_FABRIC_012)\n");
    printf("========================================\n\n");

    HMAC_CFG_reg_u cfg = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    HMAC_STATUS_reg_u status = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
    uint32_t err_code = READ_REG(HMAC_ERR_CODE_REG_ADDR);

    printf("HMAC_CFG    = 0x%08x\n", cfg.val);
    printf("HMAC_STATUS = 0x%08x idle=%u empty=%u full=%u depth=%u\n",
           status.val, status.f.hmac_idle, status.f.fifo_empty,
           status.f.fifo_full, status.f.fifo_depth);
    printf("HMAC_ERR_CODE = 0x%08x\n", err_code);

    if (!check_bit("HMAC idle", status.f.hmac_idle)) {
        pass = 0;
    }
    if (!check_bit("HMAC FIFO empty", status.f.fifo_empty)) {
        pass = 0;
    }
    if (err_code != HMAC_ERR_CODE_REG_DEFAULT) {
        printf("Unexpected HMAC_ERR_CODE default\n");
        pass = 0;
    }

    HMAC_INTR_TEST_reg_u intr_test = {.f.hmac_done = 1};
    WRITE_REG(HMAC_INTR_TEST_REG_ADDR, intr_test.val);

    HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};
    if (!check_bit("HMAC INTR_TEST hmac_done", intr.f.hmac_done)) {
        pass = 0;
    }

    HMAC_INTR_STATE_reg_u clear = {.f.hmac_done = 1};
    WRITE_REG(HMAC_INTR_STATE_REG_ADDR, clear.val);

    intr.val = READ_REG(HMAC_INTR_STATE_REG_ADDR);
    if (intr.f.hmac_done != 0) {
        printf("HMAC hmac_done interrupt did not clear\n");
        pass = 0;
    }

    if (pass) {
        printf("=== FABRIC HMAC SANITY TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== FABRIC HMAC SANITY TEST FAILED ===\n");
        test_fail(0);
    }

    while (1) {
        __asm__("wfi");
    }

    return pass ? 0 : -1;
}
