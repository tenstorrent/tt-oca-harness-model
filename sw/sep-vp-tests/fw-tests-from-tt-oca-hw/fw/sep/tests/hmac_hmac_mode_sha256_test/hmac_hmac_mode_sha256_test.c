// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * TC_HMAC_007 (P1) - HMAC-SHA256 with software key
 *
 * Uses RFC 4231 test case 2:
 *   Key  = "Jefe" (4 bytes, zero-padded to 256 bits)
 *   Data = "what do ya want for nothing?"
 *   Expected HMAC-SHA256 =
 *     5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843
 *
 * Steps:
 *   1) Write 256-bit key to KEY_0..KEY_7 (pad "Jefe" with zeros)
 *   2) Configure: hmac_en=1, sha_en=1, digest_size=SHA-256, key_length=0x02 (256b)
 *   3) hash_start, feed message, hash_process
 *   4) Wait done, read digest, bswap32, compare
 *   5) WIPE_SECRET cleanup
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

static void to_hex(const uint8_t *in, char *out, int len) {
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < len; i++) {
        out[2 * i + 0] = hex[(in[i] >> 4) & 0xF];
        out[2 * i + 1] = hex[(in[i] >> 0) & 0xF];
    }
    out[2 * len] = '\0';
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

int main(void) {
    sep_outbound_filter_init();

    printf("=== TC_HMAC_007: HMAC-SHA256 with software key ===\n");

    /* RFC 4231 test case 2 key: "Jefe" = 0x4a656665 */
    uint32_t key[8] = {0};
    key[0] = 0x4a656665;

    /* Write key to KEY_0..KEY_7 */
    for (int i = 0; i < 8; i++) {
        WRITE_REG(HMAC_KEY_0__REG_ADDR + (i * 4), key[i]);
    }

    /* Enable hmac_done interrupt */
    HMAC_INTR_ENABLE_reg_u intr_en = {.f.hmac_done = 1};
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, intr_en.val);

    /* Configure: HMAC mode, SHA-256, key_length=256b (0x02) */
    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.hmac_en = 1;
    cfg.f.sha_en = 1;
    cfg.f.endian_swap = 0;
    cfg.f.digest_swap = 0;
    cfg.f.digest_size = 1;     /* SHA2_256 */
    cfg.f.key_length = 0x02;   /* 256-bit key */
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    /* Start hash */
    HMAC_CMD_reg_u cmd = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    /* Feed message: "what do ya want for nothing?" */
    const uint8_t msg[] = "what do ya want for nothing?";
    uint32_t msg_len = 28;
    if (feed_msg(msg, msg_len) != 0) {
        printf("FAIL: FIFO feed error\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    /* Process */
    HMAC_CMD_reg_u proc = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, proc.val);

    if (wait_for_done_or_idle() != 0) {
        printf("FAIL: Timeout\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    /* Read digest and byte-swap */
    uint8_t digest[32];
    for (int i = 0; i < 8; i++) {
        uint32_t raw = READ_REG(HMAC_DIGEST_0__REG_ADDR + (i * 4));
        ((uint32_t *)digest)[i] = bswap32(raw);
    }

    char got[65];
    to_hex(digest, got, 32);
    const char *expected = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";

    printf("Digest:   %s\n", got);
    printf("Expected: %s\n", expected);

    /* Cleanup: disable and wipe */
    HMAC_CFG_reg_u cfg_off = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg_off.f.sha_en = 0;
    cfg_off.f.hmac_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg_off.val);
    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xFFFFFFFFu);
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, 0);

    if (strcmp(got, expected) == 0) {
        printf("=== TC_HMAC_007 PASSED ===\n");
        test_pass(0);
    } else {
        printf("FAIL: Digest mismatch\n");
        test_fail(1);
    }

    while (1) { __asm__("wfi"); }
}
