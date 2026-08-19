/*
 * TC_HMAC_011 (P1) - Endian swap and digest swap test
 *
 * Steps:
 *   1) Hash "abc" with endian_swap=0, digest_swap=0, save raw digest words
 *   2) Hash "abc" with endian_swap=1, digest_swap=0, save raw digest words
 *   3) Verify digests from step 1 and 2 are different
 *   4) Hash "abc" with endian_swap=0, digest_swap=1, save raw digest words
 *   5) Verify digest from step 4 differs from step 1
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "test_completion.h"
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "sep_outbound_filter.h"

static int wait_for_done_or_idle(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};
        HMAC_STATUS_reg_u sts = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        if (intr.f.hmac_done || sts.f.hmac_idle) break;
    }
    if (timeout <= 0) {
        printf("Timeout waiting for HMAC completion\n");
        return -1;
    }
    HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};
    if (intr.f.hmac_done) {
        WRITE_REG(HMAC_INTR_STATE_REG_ADDR, intr.val);
    }
    return 0;
}

/* Write message as 32-bit words so endian_swap takes effect.
 * Per RDL: "A message written to MSG_FIFO one byte at a time will not be
 * affected by this setting." Word-granularity writes are required. */
static int feed_msg_words(const uint32_t *words, uint32_t count) {
    volatile uint32_t *fifo32 = (volatile uint32_t *)(uintptr_t)HMAC_MSG_FIFO_MEM_BASE_ADDR;
    for (uint32_t i = 0; i < count; i++) {
        int spins = 0;
        HMAC_STATUS_reg_u s = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        while (s.f.fifo_full) {
            if (spins++ > 10000) {
                printf("FIFO full timeout\n");
                return -1;
            }
            s.val = READ_REG(HMAC_STATUS_REG_ADDR);
        }
        *fifo32 = words[i];
    }
    return 0;
}

static int hash_abc_with_swap(uint32_t endian_swap, uint32_t digest_swap,
                              uint32_t digest_out[8]) {
    HMAC_INTR_ENABLE_reg_u intr_en = {.f.hmac_done = 1};
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, intr_en.val);

    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.sha_en = 1;
    cfg.f.digest_size = 1;
    cfg.f.endian_swap = endian_swap;
    cfg.f.digest_swap = digest_swap;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CMD_reg_u cmd = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    /* Write "abc\x00" as one 32-bit word (0x61626300).
     * endian_swap=1 byte-reverses within the word → SHA sees 0x00636261,
     * yielding a different digest than endian_swap=0. */
    const uint32_t msg_word[] = {0x61626300u};
    if (feed_msg_words(msg_word, 1) != 0) return -1;

    HMAC_CMD_reg_u proc = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, proc.val);

    if (wait_for_done_or_idle() != 0) return -1;

    for (int i = 0; i < 8; i++) {
        digest_out[i] = READ_REG(HMAC_DIGEST_0__REG_ADDR + (i * 4));
    }

    HMAC_CFG_reg_u cfg_off = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg_off.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg_off.val);
    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xFFFFFFFFu);
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, 0);

    return 0;
}

static void print_digest(const char *label, const uint32_t d[8]) {
    printf("%s:", label);
    for (int i = 0; i < 8; i++) printf(" 0x%08x", d[i]);
    printf("\n");
}

int main(void) {
    sep_outbound_filter_init();

    printf("=== TC_HMAC_011: Endian swap and digest swap test ===\n");
    int pass = 1;

    /* Case 1: endian_swap=0, digest_swap=0 (baseline) */
    uint32_t digest_base[8];
    printf("Hash abc: endian_swap=0, digest_swap=0\n");
    if (hash_abc_with_swap(0, 0, digest_base) != 0) {
        printf("FAIL: Baseline hash failed\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }
    print_digest("Baseline", digest_base);

    /* Case 2: endian_swap=1, digest_swap=0 */
    uint32_t digest_eswap[8];
    printf("Hash abc: endian_swap=1, digest_swap=0\n");
    if (hash_abc_with_swap(1, 0, digest_eswap) != 0) {
        printf("FAIL: Endian-swap hash failed\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }
    print_digest("Endian-swap", digest_eswap);

    /* Verify baseline vs endian-swap differ */
    int diff_e = 0;
    for (int i = 0; i < 8; i++) {
        if (digest_base[i] != digest_eswap[i]) diff_e = 1;
    }
    if (!diff_e) {
        printf("FAIL: endian_swap=1 produced same raw digest as endian_swap=0\n");
        pass = 0;
    } else {
        printf("OK: endian_swap produces different raw digest\n");
    }

    /* Case 3: endian_swap=0, digest_swap=1 */
    uint32_t digest_dswap[8];
    printf("Hash abc: endian_swap=0, digest_swap=1\n");
    if (hash_abc_with_swap(0, 1, digest_dswap) != 0) {
        printf("FAIL: Digest-swap hash failed\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }
    print_digest("Digest-swap", digest_dswap);

    /* Verify baseline vs digest-swap differ */
    int diff_d = 0;
    for (int i = 0; i < 8; i++) {
        if (digest_base[i] != digest_dswap[i]) diff_d = 1;
    }
    if (!diff_d) {
        printf("FAIL: digest_swap=1 produced same raw digest as digest_swap=0\n");
        pass = 0;
    } else {
        printf("OK: digest_swap produces different raw digest\n");
    }

    if (pass) {
        printf("=== TC_HMAC_011 PASSED ===\n");
        test_pass(0);
    } else {
        printf("FAIL: TC_HMAC_011 endian/digest swap test\n");
        test_fail(1);
    }

    while (1) { __asm__("wfi"); }
}
