// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * TC_HMAC_009 (P1) - Key length configuration test
 *
 * Steps:
 *   1) For key_length values 0x01(128b), 0x02(256b), 0x04(384b), 0x08(512b):
 *      write CFG.key_length, readback verify
 *   2) Write a test key and HMAC-hash "test" with key_length=128 and key_length=256
 *   3) Verify the two digests differ (different effective key length => different HMAC)
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "test_completion.h"
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "sep_outbound_filter.h"

static inline uint32_t bswap32(uint32_t x) {
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8)  |
           ((x & 0x00FF0000u) >> 8)  |
           ((x & 0xFF000000u) >> 24);
}

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

static int feed_msg(const uint8_t *data, uint32_t len) {
    volatile uint8_t *fifo8 = (volatile uint8_t *)(uintptr_t)HMAC_MSG_FIFO_MEM_BASE_ADDR;
    for (uint32_t i = 0; i < len; i++) {
        int spins = 0;
        HMAC_STATUS_reg_u s = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        while (s.f.fifo_full) {
            if (spins++ > 10000) {
                printf("FIFO full timeout\n");
                return -1;
            }
            s.val = READ_REG(HMAC_STATUS_REG_ADDR);
        }
        *fifo8 = data[i];
    }
    return 0;
}

static int hmac_hash_with_key_length(uint32_t klen_val, uint32_t digest_out[8]) {
    /* Write a fixed test key to KEY_0..KEY_7 */
    for (int i = 0; i < 8; i++) {
        WRITE_REG(HMAC_KEY_0__REG_ADDR + (i * 4), 0xDEADBEEFu + i);
    }

    HMAC_INTR_ENABLE_reg_u intr_en = {.f.hmac_done = 1};
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, intr_en.val);

    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.hmac_en = 1;
    cfg.f.sha_en = 1;
    cfg.f.digest_size = 1;       /* SHA2_256 */
    cfg.f.key_length = klen_val;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CMD_reg_u cmd = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    const uint8_t msg[] = "test";
    if (feed_msg(msg, 4) != 0) return -1;

    HMAC_CMD_reg_u proc = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, proc.val);

    if (wait_for_done_or_idle() != 0) return -1;

    for (int i = 0; i < 8; i++) {
        digest_out[i] = READ_REG(HMAC_DIGEST_0__REG_ADDR + (i * 4));
    }

    /* Cleanup */
    HMAC_CFG_reg_u cfg_off = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg_off.f.sha_en = 0;
    cfg_off.f.hmac_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg_off.val);
    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xFFFFFFFFu);
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, 0);

    return 0;
}

int main(void) {
    sep_outbound_filter_init();

    printf("=== TC_HMAC_009: Key length configuration test ===\n");
    int pass = 1;

    /* Part 1: Readback verify key_length field for multiple values */
    uint32_t klen_vals[] = {0x01, 0x02, 0x04, 0x08};
    const char *klen_names[] = {"128b", "256b", "384b", "512b"};

    for (int t = 0; t < 4; t++) {
        HMAC_CFG_reg_u cfg = {.val = 0};
        cfg.f.sha_en = 1;
        cfg.f.digest_size = 1;
        cfg.f.key_length = klen_vals[t];
        WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

        HMAC_CFG_reg_u rb = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
        printf("key_length=%s: wrote=0x%02x readback=0x%02x %s\n",
               klen_names[t], klen_vals[t], rb.f.key_length,
               (rb.f.key_length == klen_vals[t]) ? "OK" : "MISMATCH");
        if (rb.f.key_length != klen_vals[t]) {
            pass = 0;
        }
    }

    /* Part 2: HMAC with key_length=128 vs key_length=256 must produce different digests */
    uint32_t digest_128[8];
    uint32_t digest_256[8];

    printf("HMAC with key_length=128b...\n");
    if (hmac_hash_with_key_length(0x01, digest_128) != 0) {
        printf("FAIL: HMAC with key_length=128 failed\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    printf("HMAC with key_length=256b...\n");
    if (hmac_hash_with_key_length(0x02, digest_256) != 0) {
        printf("FAIL: HMAC with key_length=256 failed\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    printf("Digest (128b key):");
    for (int i = 0; i < 8; i++) printf(" 0x%08x", digest_128[i]);
    printf("\n");

    printf("Digest (256b key):");
    for (int i = 0; i < 8; i++) printf(" 0x%08x", digest_256[i]);
    printf("\n");

    int differ = 0;
    for (int i = 0; i < 8; i++) {
        if (digest_128[i] != digest_256[i]) differ = 1;
    }
    if (!differ) {
        printf("FAIL: 128b and 256b key lengths produced identical digests\n");
        pass = 0;
    } else {
        printf("OK: Different key lengths produce different digests\n");
    }

    if (pass) {
        printf("=== TC_HMAC_009 PASSED ===\n");
        test_pass(0);
    } else {
        printf("FAIL: TC_HMAC_009 key length test\n");
        test_fail(1);
    }

    while (1) { __asm__("wfi"); }
}
