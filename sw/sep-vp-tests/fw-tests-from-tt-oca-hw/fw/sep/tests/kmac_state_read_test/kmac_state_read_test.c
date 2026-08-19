/*
 * TC_KMAC_013 - STATE Share Verification Test (P1)
 *
 * Runs SHA3-256 of "abc", reads both STATE shares (share0 and share1),
 * verifies that both are non-zero, they differ (masking is active),
 * and their XOR produces a non-zero digest.
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

static void setup_entropy(void) {
    for (int i = 0; i < 6; i++)
        WRITE_REG(KMAC_ENTROPY_SEED_REG_ADDR, 0xDEADBEEF + i);
}

static int test_state_read(void) {
    int errors = 0;

    printf("=== Step 1: Configure SHA3-256 ===\n");
    if (wait_for_idle() != 0) return -1;

    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = 0;
    cfg.f.mode = 0x0;
    cfg.f.kstrength = 0x2;
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

    printf("=== Step 3: Write 'abc' to MSG_FIFO ===\n");
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x00636261);

    printf("=== Step 4: PROCESS ===\n");
    cmd.f.cmd = 46;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    if (wait_for_done() != 0) return -1;

    printf("=== Step 5: Read STATE shares ===\n");
    uint32_t share0[8], share1[8], digest[8];

    for (int i = 0; i < 8; i++)
        share0[i] = READ_REG(KMAC_STATE_MEM_BASE_ADDR + (i * 4));

    for (int i = 0; i < 8; i++)
        share1[i] = READ_REG(KMAC_STATE_MEM_BASE_ADDR + 0x100 + (i * 4));

    for (int i = 0; i < 8; i++)
        digest[i] = share0[i] ^ share1[i];

    printf("Share0: ");
    for (int i = 0; i < 8; i++) printf("%08x ", share0[i]);
    printf("\n");

    printf("Share1: ");
    for (int i = 0; i < 8; i++) printf("%08x ", share1[i]);
    printf("\n");

    printf("Digest: ");
    for (int i = 0; i < 8; i++) printf("%08x ", digest[i]);
    printf("\n");

    printf("=== Step 6: Verify share properties ===\n");

    int s0_nz = 0, s1_nz = 0, d_nz = 0;
    for (int i = 0; i < 8; i++) {
        if (share0[i] != 0) s0_nz = 1;
        if (share1[i] != 0) s1_nz = 1;
        if (digest[i] != 0) d_nz = 1;
    }

    if (!s0_nz) {
        printf("FAIL: share0 is all zeros\n");
        errors++;
    } else {
        printf("PASS: share0 is non-zero\n");
    }

    if (!s1_nz) {
        printf("FAIL: share1 is all zeros\n");
        errors++;
    } else {
        printf("PASS: share1 is non-zero\n");
    }

    int shares_same = 1;
    for (int i = 0; i < 8; i++) {
        if (share0[i] != share1[i]) { shares_same = 0; break; }
    }
    if (shares_same) {
        printf("FAIL: share0 == share1 (masking not active)\n");
        errors++;
    } else {
        printf("PASS: share0 != share1 (masking active)\n");
    }

    if (!d_nz) {
        printf("FAIL: digest (XOR) is all zeros\n");
        errors++;
    } else {
        printf("PASS: digest is non-zero\n");
    }

    printf("=== Step 7: DONE ===\n");
    cmd.f.cmd = 22;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    return errors;
}

int main(void) {
    sep_outbound_filter_init();

    printf("\n");
    printf("========================================\n");
    printf("  TC_KMAC_013: STATE Share Verify Test\n");
    printf("========================================\n\n");

    int result = test_state_read();

    if (result == 0) {
        printf("\n=== TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("\n=== TEST FAILED (errors=%d) ===\n", result);
        test_fail(1);
    }

    while (1) { __asm__("wfi"); }
}
