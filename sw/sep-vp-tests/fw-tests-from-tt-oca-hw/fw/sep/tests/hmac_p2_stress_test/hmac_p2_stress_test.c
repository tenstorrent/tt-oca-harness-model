/*
 * HMAC P2 Stress Test - continuous SHA-256 operations without IP reset.
 *
 * Verifies repeated hash_start/hash_process cycles can run back-to-back while
 * sha_en remains asserted. Each iteration clears hmac_done and checks that the
 * next operation retriggers completion and produces an independent digest.
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_hmac_p2_stress_test STACK=cgen,sim
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

typedef struct {
    const char *name;
    const uint8_t *data;
    uint32_t len;
    const char *expected_hex;
} hmac_stress_case_t;

static const uint8_t msg_empty[] = "";
static const uint8_t msg_abc[] = "abc";
static const uint8_t msg_hello[] = "Hello OTBN.";
static const uint8_t msg_iter3[] = "continuous-hmac-stress-iteration-3";
static const uint8_t msg_64[64] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
};
static uint8_t msg_96[96];

static inline uint32_t bswap32(uint32_t x)
{
    return ((x & 0x000000ffu) << 24) |
           ((x & 0x0000ff00u) << 8)  |
           ((x & 0x00ff0000u) >> 8)  |
           ((x & 0xff000000u) >> 24);
}

static void init_msg_96(void)
{
    for (uint32_t i = 0; i < sizeof(msg_96); i++) {
        msg_96[i] = (uint8_t)((i * 7u + 3u) & 0xffu);
    }
}

static void clear_hmac_done(void)
{
    HMAC_INTR_STATE_reg_u clear = {.val = 0};
    clear.f.hmac_done = 1;
    WRITE_REG(HMAC_INTR_STATE_REG_ADDR, clear.val);
}

static int wait_for_hmac_done(void)
{
    int timeout = 1000000;
    while (timeout-- > 0) {
        HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};
        if (intr.f.hmac_done) {
            return 0;
        }
    }

    printf("  Timeout waiting for hmac_done\n");
    return -1;
}

static int feed_msg(const uint8_t *data, uint32_t len)
{
    volatile uint8_t *fifo8 = (volatile uint8_t *)(uintptr_t)HMAC_MSG_FIFO_MEM_BASE_ADDR;

    for (uint32_t i = 0; i < len; i++) {
        int spins = 0;
        HMAC_STATUS_reg_u status = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
        while (status.f.fifo_full) {
            if (spins++ > 10000) {
                printf("  FIFO full timeout at byte %u\n", i);
                return -1;
            }
            status.val = READ_REG(HMAC_STATUS_REG_ADDR);
        }
        *fifo8 = data[i];
    }

    return 0;
}

static void read_digest_hex(char *hex_out)
{
    static const char hex_chars[] = "0123456789abcdef";

    for (int word = 0; word < 8; word++) {
        uint32_t raw = READ_REG(HMAC_DIGEST_0__REG_ADDR + (word * 4));
        uint32_t digest_word = bswap32(raw);
        for (int byte = 0; byte < 4; byte++) {
            uint8_t value = (uint8_t)(digest_word >> (byte * 8));
            int idx = word * 8 + byte * 2;
            hex_out[idx + 0] = hex_chars[(value >> 4) & 0xf];
            hex_out[idx + 1] = hex_chars[value & 0xf];
        }
    }
    hex_out[64] = '\0';
}

static void cleanup_hmac(void)
{
    HMAC_CFG_reg_u cfg = {.val = READ_REG(HMAC_CFG_REG_ADDR)};
    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);
    WRITE_REG(HMAC_WIPE_SECRET_REG_ADDR, 0xffffffffu);
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, 0);
    clear_hmac_done();
}

static int run_stress_case(const hmac_stress_case_t *test_case, uint32_t iter)
{
    printf("\n[Iteration %u] %s (%u bytes)\n", iter, test_case->name, test_case->len);

    clear_hmac_done();
    HMAC_INTR_STATE_reg_u intr = {.val = READ_REG(HMAC_INTR_STATE_REG_ADDR)};
    if (intr.f.hmac_done) {
        printf("  FAIL: hmac_done did not clear before iteration\n");
        return -1;
    }

    HMAC_CMD_reg_u start = {.f.hash_start = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, start.val);

    if (feed_msg(test_case->data, test_case->len) != 0) {
        return -1;
    }

    uint32_t msg_len = READ_REG(HMAC_MSG_LENGTH_LOWER_REG_ADDR);
    uint32_t expected_bits = test_case->len * 8u;
    printf("  MSG_LENGTH_LOWER=%u expected=%u\n", msg_len, expected_bits);
    if (msg_len != expected_bits) {
        printf("  FAIL: message length was polluted by a previous operation\n");
        return -1;
    }

    HMAC_CMD_reg_u process = {.f.hash_process = 1};
    WRITE_REG(HMAC_CMD_REG_ADDR, process.val);

    if (wait_for_hmac_done() != 0) {
        return -1;
    }

    HMAC_STATUS_reg_u status = {.val = READ_REG(HMAC_STATUS_REG_ADDR)};
    printf("  STATUS=0x%08x idle=%u empty=%u full=%u depth=%u\n",
           status.val, status.f.hmac_idle, status.f.fifo_empty,
           status.f.fifo_full, status.f.fifo_depth);
    if (!status.f.hmac_idle) {
        printf("  FAIL: HMAC did not return to idle after completion\n");
        return -1;
    }

    char got_hex[65];
    read_digest_hex(got_hex);
    printf("  Digest:   %s\n", got_hex);
    printf("  Expected: %s\n", test_case->expected_hex);

    if (strcmp(got_hex, test_case->expected_hex) != 0) {
        printf("  FAIL: digest mismatch\n");
        return -1;
    }

    clear_hmac_done();
    intr.val = READ_REG(HMAC_INTR_STATE_REG_ADDR);
    if (intr.f.hmac_done) {
        printf("  FAIL: hmac_done did not clear after iteration\n");
        return -1;
    }

    return 0;
}

int main(void)
{
    sep_outbound_filter_init();
    init_msg_96();

    printf("\n========================================\n");
    printf("HMAC P2 Stress Test\n");
    printf("========================================\n");

    HMAC_INTR_ENABLE_reg_u intr_en = {.val = 0};
    intr_en.f.hmac_done = 1;
    WRITE_REG(HMAC_INTR_ENABLE_REG_ADDR, intr_en.val);

    HMAC_CFG_reg_u cfg = {.val = 0};
    cfg.f.sha_en = 1;
    cfg.f.hmac_en = 0;
    cfg.f.digest_size = 1;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    static const hmac_stress_case_t cases[] = {
        {
            "empty",
            msg_empty,
            0,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        },
        {
            "abc",
            msg_abc,
            3,
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        },
        {
            "Hello OTBN.",
            msg_hello,
            11,
            "2e8bd199adc454937f7f8c68e3366d8a30b486d35359fbbb83625a716eaf2403",
        },
        {
            "iteration-3 string",
            msg_iter3,
            34,
            "5c247ab3f4980307c838b26c90dc880b1216e1bdd366134c90510470f84d76c9",
        },
        {
            "64-byte ramp",
            msg_64,
            sizeof(msg_64),
            "fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108",
        },
        {
            "96-byte generated pattern",
            msg_96,
            sizeof(msg_96),
            "c9f1a5f79d7bea01a54f4edb41673722f627ee2e82dda324946b63cf4b9b16af",
        },
    };

    int pass = 1;
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (run_stress_case(&cases[i], i) != 0) {
            pass = 0;
            break;
        }
    }

    cleanup_hmac();

    printf("\n========================================\n");
    if (pass) {
        printf("=== HMAC P2 STRESS TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== HMAC P2 STRESS TEST FAILED ===\n");
        test_fail(1);
    }
    printf("========================================\n");

    while (1) {
        __asm__("wfi");
    }

    return pass ? 0 : -1;
}
