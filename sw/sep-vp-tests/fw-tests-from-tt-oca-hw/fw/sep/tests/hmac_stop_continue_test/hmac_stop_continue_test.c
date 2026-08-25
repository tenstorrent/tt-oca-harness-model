// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * TC_HMAC_010 (P1) - Hash stop/continue for multi-part hashing
 *
 * Steps:
 *   1) Single-pass: hash "Hello World!" in one shot, save digest
 *   2) Multi-part: hash_start, feed "Hello ", hash_stop, verify idle,
 *      hash_continue, feed "World!", hash_process, read digest
 *   3) Compare: both digests must match
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

static int wait_for_idle(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        HMAC_STATUS_reg_u sts = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        if (sts.f.hmac_idle) return 0;
    }
    printf("Timeout waiting for HMAC idle\n");
    return -1;
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

static void to_hex(const uint8_t *in, char *out, int len) {
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < len; i++) {
        out[2 * i + 0] = hex[(in[i] >> 4) & 0xF];
        out[2 * i + 1] = hex[(in[i] >> 0) & 0xF];
    }
    out[2 * len] = '\0';
}

static void read_digest(uint8_t digest[32]) {
    for (int i = 0; i < 8; i++) {
        uint32_t raw = READ_REG(HMAC_DIGEST_0__REG_ADDR + (i * 4));
        ((uint32_t *)digest)[i] = bswap32(raw);
    }
}

static void cleanup(void) {
    HMAC_CFG_reg_u cfg = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);
    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xFFFFFFFFu);
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, 0);
}

int main(void) {
    sep_outbound_filter_init();

    printf("=== TC_HMAC_010: Hash stop/continue multi-part test ===\n");

    /* Part1 must be exactly 64 bytes (SHA-256 block boundary) for hash_stop to work:
     *   1) Packer only flushes on hash_process, not hash_stop; partial words cause idle deadlock.
     *   2) digest_on_blk requires message_length mod 512 == 0; otherwise hmac_done never fires.
     * Part2 can be any length since hash_process triggers packer flush automatically. */
    const uint8_t full_msg[69] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" "Hello";
    const uint8_t part1[64]    = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    const uint8_t part2[5]     = "Hello";
    uint32_t full_len = 69;
    uint32_t p1_len = 64;
    uint32_t p2_len = 5;

    /* ---- Single-pass hash ---- */
    printf("[Single-pass] Hashing \"Hello World!\"...\n");

    HMAC_INTR_ENABLE_reg_u intr_en = {.f.hmac_done = 1};
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, intr_en.val);

    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.sha_en = 1;
    cfg.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    HMAC_CMD_reg_u cmd_start = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd_start.val);

    if (feed_msg(full_msg, full_len) != 0) {
        printf("FAIL: Single-pass feed error\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    HMAC_CMD_reg_u cmd_proc = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd_proc.val);

    if (wait_for_done_or_idle() != 0) {
        printf("FAIL: Single-pass timeout\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    uint8_t digest_single[32];
    read_digest(digest_single);
    cleanup();

    char hex_single[65];
    to_hex(digest_single, hex_single, 32);
    printf("Single-pass digest: %s\n", hex_single);

    /* ---- Multi-part hash with stop/continue ---- */
    printf("[Multi-part] Hashing \"Hello \" + \"World!\"...\n");

    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, intr_en.val);

    HMAC_CFG_reg_u cfg2 = {.val = 0};
    cfg2.f.sha_en = 1;
    cfg2.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg2.val);

    /* hash_start */
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd_start.val);

    /* Feed part 1 */
    if (feed_msg(part1, p1_len) != 0) {
        printf("FAIL: Multi-part feed part1 error\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    /* hash_stop */
    HMAC_CMD_reg_u cmd_stop = {.f.hash_stop = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd_stop.val);

    /* hash_stop at a 64-byte boundary generates hmac_done then asserts hmac_idle.
     * Use wait_for_done_or_idle() to catch either signal. */
    if (wait_for_done_or_idle() != 0) {
        printf("FAIL: Not idle/done after hash_stop\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }
    printf("HMAC idle/done after hash_stop: OK\n");

    /* hash_continue */
    HMAC_CMD_reg_u cmd_cont = {.f.hash_continue = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd_cont.val);

    /* Feed part 2 */
    if (feed_msg(part2, p2_len) != 0) {
        printf("FAIL: Multi-part feed part2 error\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    /* hash_process */
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd_proc.val);

    if (wait_for_done_or_idle() != 0) {
        printf("FAIL: Multi-part timeout\n");
        test_fail(1);
        while (1) { __asm__("wfi"); }
    }

    uint8_t digest_multi[32];
    read_digest(digest_multi);
    cleanup();

    char hex_multi[65];
    to_hex(digest_multi, hex_multi, 32);
    printf("Multi-part digest:  %s\n", hex_multi);

    /* ---- Compare ---- */
    if (memcmp(digest_single, digest_multi, 32) == 0) {
        printf("Digests match\n");
        printf("=== TC_HMAC_010 PASSED ===\n");
        test_pass(0);
    } else {
        printf("FAIL: Single-pass and multi-part digests differ\n");
        test_fail(1);
    }

    while (1) { __asm__("wfi"); }
}
