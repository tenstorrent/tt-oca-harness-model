/*
 * TC_KMAC_012 - SHAKE128 XOF Test (P1)
 *
 * Verifies SHAKE128 eXtendable Output Function operation:
 * performs first squeeze, issues MANUAL_RUN for second squeeze,
 * and confirms the two squeeze outputs differ.
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

static int wait_for_squeeze(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        KMAC_STATUS_reg_u s = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
        if (s.f.sha3_squeeze) return 0;
    }
    printf("Timeout waiting for squeeze\n");
    return -1;
}

static void setup_entropy(void) {
    for (int i = 0; i < 6; i++)
        WRITE_REG(KMAC_ENTROPY_SEED_REG_ADDR, 0xDEADBEEF + i);
}

static void read_state(uint32_t *out, int words) {
    for (int i = 0; i < words; i++) {
        uint32_t s0 = READ_REG(KMAC_STATE_MEM_BASE_ADDR + (i * 4));
        uint32_t s1 = READ_REG(KMAC_STATE_MEM_BASE_ADDR + 0x100 + (i * 4));
        out[i] = s0 ^ s1;
    }
}

static int test_shake128_xof(void) {
    int errors = 0;

    printf("=== Step 1: Configure SHAKE128 ===\n");
    if (wait_for_idle() != 0) return -1;

    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = 0;
    cfg.f.mode = 0x2;   // SHAKE = value 2 per hjson (sha3_mode_e::Shake = 2'b10)
    cfg.f.kstrength = 0x0;
    cfg.f.entropy_mode = 0x1;  /* EDN mode = 0x1 (0=None, 1=EDN, 2=SW per hjson) */
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    setup_entropy();

    cfg.f.entropy_ready = 1;
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    printf("=== Step 2: START ===\n");
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = 29;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    printf("=== Step 3: Write message 'test' ===\n");
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x74736574);

    printf("=== Step 4: PROCESS ===\n");
    cmd.f.cmd = 46;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    if (wait_for_squeeze() != 0) return -1;

    printf("=== Step 5: First squeeze - read STATE ===\n");
    uint32_t first_squeeze[8];
    read_state(first_squeeze, 8);

    printf("First squeeze: ");
    for (int i = 0; i < 8; i++) printf("%08x ", first_squeeze[i]);
    printf("\n");

    int nz_first = 0;
    for (int i = 0; i < 8; i++) {
        if (first_squeeze[i] != 0) nz_first = 1;
    }
    if (!nz_first) {
        printf("FAIL: first squeeze all zeros\n");
        errors++;
    }

    printf("=== Step 6: Issue MANUAL_RUN for second squeeze ===\n");
    cmd.f.cmd = 49;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    if (wait_for_squeeze() != 0) {
        printf("FAIL: timeout waiting for second squeeze\n");
        return errors + 1;
    }

    printf("=== Step 7: Second squeeze - read STATE ===\n");
    uint32_t second_squeeze[8];
    read_state(second_squeeze, 8);

    printf("Second squeeze: ");
    for (int i = 0; i < 8; i++) printf("%08x ", second_squeeze[i]);
    printf("\n");

    printf("=== Step 8: Compare squeezes ===\n");
    int same = 1;
    for (int i = 0; i < 8; i++) {
        if (first_squeeze[i] != second_squeeze[i]) { same = 0; break; }
    }
    if (same) {
        printf("FAIL: first and second squeezes are identical\n");
        errors++;
    } else {
        printf("PASS: squeezes differ as expected\n");
    }

    printf("=== Step 9: DONE ===\n");
    cmd.f.cmd = 22;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    return errors;
}

int main(void) {
    sep_outbound_filter_init();

    printf("\n");
    printf("========================================\n");
    printf("  TC_KMAC_012: SHAKE128 XOF Test\n");
    printf("========================================\n\n");

    int result = test_shake128_xof();

    if (result == 0) {
        printf("\n=== TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("\n=== TEST FAILED (errors=%d) ===\n", result);
        test_fail(1);
    }

    while (1) { __asm__("wfi"); }
}
