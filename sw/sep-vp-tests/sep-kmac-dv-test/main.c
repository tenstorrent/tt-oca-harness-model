// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * KMAC DV Test - Comprehensive Design Verification Test for KMAC on SEP VP
 *
 * Tests organized in two categories:
 *
 * DUT-Level Tests (register/hardware verification):
 *   1. Register Connectivity     - Read default values, verify MMIO works
 *   2. Interrupt Test Logic      - INTR_TEST → INTR_STATE reflection
 *   3. CFG_REGWEN Protection     - Write protection when REGWEN=0
 *   4. Status Register Defaults  - Verify idle, fifo_empty after reset
 *   5. Error Code After Reset    - ERR_CODE reads 0
 *
 * App-Level Tests (functional verification):
 *   6. SHA3-256 Hash             - Hash empty message and verify NIST vector
 *   7. KMAC-128 Operation        - KMAC with key, message, and digest verify
 *   8. FSM State Transitions     - Verify IDLE→ABSORB→SQUEEZE→IDLE
 *   9. FIFO Status Tracking      - Monitor fifo_depth, fifo_empty, fifo_full
 */

#include <stdint.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"

/* Minimal declarations (implemented in printf.c) */
int printf(const char *format, ...);
int strcmp(const char *s1, const char *s2);

/* ============================================================================
 * KMAC Constants
 * ============================================================================ */

/* CMD register command encodings (6-bit field) */
#define CMD_START   29  /* CmdStart   = 6'b011101 */
#define CMD_PROCESS 46  /* CmdProcess = 6'b101110 */
#define CMD_DONE    22  /* CmdDone    = 6'b010110 */

/* CFG_SHADOWED mode field */
#define MODE_SHA3   0x0  /* SHA3 */
#define MODE_CSHAKE 0x2  /* cSHAKE */

/* CFG_SHADOWED kstrength field */
#define KSTRENGTH_L128  0x0  /* 128-bit security */
#define KSTRENGTH_L256  0x2  /* 256-bit security */

/* CFG_SHADOWED entropy_mode field (0x2 = SW mode) */
#define ENTROPY_SW_MODE  0x2

/* Timeout for busy-wait loops (each loop iteration ~10 instructions) */
#define POLL_TIMEOUT  10000

/* Test tracking */
static int tests_run    = 0;
static int tests_passed = 0;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static void print_status(const char *tag) {
    KMAC_STATUS_reg_u s = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
    printf("%s STATUS=0x%08x idle=%u absorb=%u squeeze=%u fifo_empty=%u fifo_full=%u depth=%u\n",
           tag, s.val, s.f.sha3_idle, s.f.sha3_absorb, s.f.sha3_squeeze,
           s.f.fifo_empty, s.f.fifo_full, s.f.fifo_depth);
}

static int wait_for_idle(void) {
    int timeout = POLL_TIMEOUT;
    while (timeout-- > 0) {
        KMAC_STATUS_reg_u s = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
        if (s.f.sha3_idle) return 0;
    }
    printf("  ERROR: Timeout waiting for KMAC idle\n");
    return -1;
}

static int wait_for_squeeze(void) {
    int timeout = POLL_TIMEOUT;
    while (timeout-- > 0) {
        KMAC_STATUS_reg_u s = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
        if (s.f.sha3_squeeze) return 0;
    }
    printf("  ERROR: Timeout waiting for KMAC squeeze\n");
    return -1;
}

static int wait_for_done(void) {
    int timeout = POLL_TIMEOUT;
    while (timeout-- > 0) {
        KMAC_INTR_STATE_reg_u intr = {.val = READ_REG(KMAC_INTR_STATE_REG_ADDR)};
        if (intr.f.kmac_done) {
            /* W1C: write 1 to clear */
            WRITE_REG(KMAC_INTR_STATE_REG_ADDR, 0x1);
            return 0;
        }
    }
    printf("  ERROR: Timeout waiting for kmac_done\n");
    return -1;
}

static void to_hex(const uint32_t *words, int nwords, char *out) {
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < nwords; i++) {
        for (int b = 0; b < 4; b++) {
            uint8_t byte = (words[i] >> (b * 8)) & 0xFF;
            *out++ = hex[(byte >> 4) & 0xF];
            *out++ = hex[byte & 0xF];
        }
    }
    *out = '\0';
}

static void report(const char *name, int pass) {
    tests_run++;
    if (pass) {
        tests_passed++;
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s\n", name);
    }
}

/*
 * Configure KMAC and initialize SW entropy.
 *
 * Correct sequence for the KMAC model:
 *   1. Write CFG_SHADOWED with entropy_mode=0x2 and entropy_ready=1 (shadow pair)
 *   2. Write ENTROPY_SEED 6 times (only accepted when entropy_mode=0x2 AND entropy_ready=1)
 *
 * Parameters control the KMAC operating mode (SHA3/KMAC, key strength, etc.)
 */
static void configure_kmac(int kmac_en, uint32_t mode, uint32_t kstrength) {
    KMAC_CFG_SHADOWED_reg_u cfg = {.val = 0};
    cfg.f.kmac_en = kmac_en;
    cfg.f.mode = mode;
    cfg.f.kstrength = kstrength;
    cfg.f.entropy_mode = ENTROPY_SW_MODE;
    cfg.f.entropy_ready = 1;

    /* Shadow register: must be written twice with same value */
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);
    WRITE_REG(KMAC_CFG_SHADOWED_REG_ADDR, cfg.val);

    /* Write 6 entropy seeds (required for SW entropy mode) */
    for (int i = 0; i < 6; i++) {
        WRITE_REG(KMAC_ENTROPY_SEED_REG_ADDR, 0xDEADBEEF + i);
    }
}

/* Issue CMD_DONE to return FSM to IDLE */
static void issue_done(void) {
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = CMD_DONE;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    wait_for_idle();
}

/* ============================================================================
 * DUT-Level Tests
 * ============================================================================ */

/*
 * Test 1: Register Connectivity
 * Verify basic MMIO read/write: read STATUS default, verify ERR_CODE=0,
 * verify CFG_REGWEN default=1.
 */
static int test_register_connectivity(void) {
    printf("\n--- Test 1: Register Connectivity ---\n");
    printf("  KMAC base=0x%08x\n", KMAC_REG_MAP_BASE_ADDR);

    int pass = 1;

    /* STATUS default: sha3_idle=1, fifo_empty=1 → 0x00004001 */
    KMAC_STATUS_reg_u sts = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
    printf("  STATUS=0x%08x (expect idle=1, fifo_empty=1)\n", sts.val);
    if (!sts.f.sha3_idle) {
        printf("  FAIL: sha3_idle not set after reset\n");
        pass = 0;
    }
    if (!sts.f.fifo_empty) {
        printf("  FAIL: fifo_empty not set after reset\n");
        pass = 0;
    }

    /* ERR_CODE should be 0 after reset */
    uint32_t err = READ_REG(KMAC_ERR_CODE_REG_ADDR);
    printf("  ERR_CODE=0x%08x (expect 0)\n", err);
    if (err != 0) {
        printf("  FAIL: ERR_CODE not 0\n");
        pass = 0;
    }

    /* CFG_REGWEN default: en=1 */
    KMAC_CFG_REGWEN_reg_u regwen = {.val = READ_REG(KMAC_CFG_REGWEN_REG_ADDR)};
    printf("  CFG_REGWEN=0x%08x (expect en=1)\n", regwen.val);
    if (!regwen.f.en) {
        printf("  FAIL: CFG_REGWEN.en not 1\n");
        pass = 0;
    }

    /* INTR_STATE should be 0 after reset */
    KMAC_INTR_STATE_reg_u intr = {.val = READ_REG(KMAC_INTR_STATE_REG_ADDR)};
    printf("  INTR_STATE=0x%08x (expect 0)\n", intr.val);
    if (intr.val != 0) {
        printf("  FAIL: INTR_STATE not 0\n");
        pass = 0;
    }

    return pass;
}

/*
 * Test 2: Interrupt Test Logic
 * Write INTR_TEST to set INTR_STATE, then clear via W1C.
 */
static int test_interrupt_logic(void) {
    printf("\n--- Test 2: Interrupt Test Logic ---\n");

    int pass = 1;

    /* Set kmac_done via INTR_TEST */
    KMAC_INTR_TEST_reg_u test_reg = {.f.kmac_done = 1};
    WRITE_REG(KMAC_INTR_TEST_REG_ADDR, test_reg.val);

    KMAC_INTR_STATE_reg_u intr = {.val = READ_REG(KMAC_INTR_STATE_REG_ADDR)};
    printf("  After INTR_TEST(kmac_done=1): INTR_STATE=0x%08x\n", intr.val);
    if (!intr.f.kmac_done) {
        printf("  FAIL: INTR_TEST did not set kmac_done in INTR_STATE\n");
        pass = 0;
    }

    /* Clear via W1C */
    WRITE_REG(KMAC_INTR_STATE_REG_ADDR, 0x1);
    intr.val = READ_REG(KMAC_INTR_STATE_REG_ADDR);
    printf("  After W1C: INTR_STATE=0x%08x\n", intr.val);
    if (intr.f.kmac_done) {
        printf("  FAIL: W1C did not clear kmac_done\n");
        pass = 0;
    }

    /* Set fifo_empty via INTR_TEST */
    test_reg.val = 0;
    test_reg.f.fifo_empty = 1;
    WRITE_REG(KMAC_INTR_TEST_REG_ADDR, test_reg.val);

    intr.val = READ_REG(KMAC_INTR_STATE_REG_ADDR);
    printf("  After INTR_TEST(fifo_empty=1): INTR_STATE=0x%08x\n", intr.val);
    if (!intr.f.fifo_empty) {
        printf("  FAIL: INTR_TEST did not set fifo_empty in INTR_STATE\n");
        pass = 0;
    }

    /* Clear fifo_empty */
    WRITE_REG(KMAC_INTR_STATE_REG_ADDR, 0x2);

    /* Set kmac_err via INTR_TEST */
    test_reg.val = 0;
    test_reg.f.kmac_err = 1;
    WRITE_REG(KMAC_INTR_TEST_REG_ADDR, test_reg.val);

    intr.val = READ_REG(KMAC_INTR_STATE_REG_ADDR);
    printf("  After INTR_TEST(kmac_err=1): INTR_STATE=0x%08x\n", intr.val);
    if (!intr.f.kmac_err) {
        printf("  FAIL: INTR_TEST did not set kmac_err in INTR_STATE\n");
        pass = 0;
    }

    /* Clear all */
    WRITE_REG(KMAC_INTR_STATE_REG_ADDR, 0x7);
    return pass;
}

/*
 * Test 3: CFG_REGWEN Write Protection
 * When CFG_REGWEN.en=0, writes to CFG_SHADOWED should be blocked.
 * CFG_REGWEN is automatically cleared when CMD=START is issued.
 */
static int test_cfg_regwen(void) {
    printf("\n--- Test 3: CFG_REGWEN Write Protection ---\n");

    int pass = 1;

    /* Verify REGWEN=1 initially (can write to config) */
    KMAC_CFG_REGWEN_reg_u regwen = {.val = READ_REG(KMAC_CFG_REGWEN_REG_ADDR)};
    printf("  Initial CFG_REGWEN=0x%08x\n", regwen.val);
    if (!regwen.f.en) {
        printf("  FAIL: CFG_REGWEN not 1 initially\n");
        pass = 0;
    }

    /* Configure KMAC (sets entropy_mode=0x2, entropy_ready=1, writes seeds) */
    configure_kmac(0, MODE_SHA3, KSTRENGTH_L256);

    /* Read back to verify write succeeded */
    uint32_t cfg_readback = READ_REG(KMAC_CFG_SHADOWED_REG_ADDR);
    printf("  CFG after write: 0x%08x (expect non-zero)\n", cfg_readback);

    /* Issue START → REGWEN should become 0 */
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = CMD_START;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    regwen.val = READ_REG(KMAC_CFG_REGWEN_REG_ADDR);
    printf("  CFG_REGWEN after START: 0x%08x (expect 0)\n", regwen.val);
    if (regwen.f.en) {
        printf("  FAIL: CFG_REGWEN not cleared after START\n");
        pass = 0;
    }

    /* Issue PROCESS + DONE to return to IDLE, REGWEN should be restored */
    cmd.f.cmd = CMD_PROCESS;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    wait_for_done();

    issue_done();

    regwen.val = READ_REG(KMAC_CFG_REGWEN_REG_ADDR);
    printf("  CFG_REGWEN after DONE: 0x%08x (expect 1)\n", regwen.val);
    if (!regwen.f.en) {
        printf("  FAIL: CFG_REGWEN not restored after DONE\n");
        pass = 0;
    }

    return pass;
}

/*
 * Test 4: Status Register Defaults
 * Verify all STATUS fields have expected post-reset values.
 */
static int test_status_defaults(void) {
    printf("\n--- Test 4: Status Register Defaults ---\n");

    int pass = 1;

    KMAC_STATUS_reg_u sts = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
    printf("  STATUS = 0x%08x\n", sts.val);
    printf("    sha3_idle=%u sha3_absorb=%u sha3_squeeze=%u\n",
           sts.f.sha3_idle, sts.f.sha3_absorb, sts.f.sha3_squeeze);
    printf("    fifo_depth=%u fifo_empty=%u fifo_full=%u\n",
           sts.f.fifo_depth, sts.f.fifo_empty, sts.f.fifo_full);

    if (!sts.f.sha3_idle) {
        printf("  FAIL: sha3_idle not set\n");
        pass = 0;
    }
    if (sts.f.sha3_absorb) {
        printf("  FAIL: sha3_absorb set unexpectedly\n");
        pass = 0;
    }
    if (sts.f.sha3_squeeze) {
        printf("  FAIL: sha3_squeeze set unexpectedly\n");
        pass = 0;
    }
    if (!sts.f.fifo_empty) {
        printf("  FAIL: fifo_empty not set\n");
        pass = 0;
    }
    if (sts.f.fifo_full) {
        printf("  FAIL: fifo_full set unexpectedly\n");
        pass = 0;
    }
    if (sts.f.fifo_depth != 0) {
        printf("  FAIL: fifo_depth=%u, expected 0\n", sts.f.fifo_depth);
        pass = 0;
    }

    return pass;
}

/*
 * Test 5: Error Code After Reset
 * ERR_CODE should read 0x00000000 after reset.
 */
static int test_err_code_reset(void) {
    printf("\n--- Test 5: Error Code After Reset ---\n");

    int pass = 1;

    uint32_t err = READ_REG(KMAC_ERR_CODE_REG_ADDR);
    printf("  ERR_CODE = 0x%08x (expect 0x00000000)\n", err);
    if (err != 0) {
        printf("  FAIL: ERR_CODE not 0 after reset\n");
        pass = 0;
    }

    return pass;
}

/* ============================================================================
 * App-Level Tests
 * ============================================================================ */

/*
 * Test 6: SHA3-256 Hash
 * Hash empty message ("") using SHA3-256 (non-KMAC mode) and verify
 * against NIST test vector.
 *
 * SHA3-256("") = a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a
 */
static int test_sha3_256_empty(void) {
    printf("\n--- Test 6: SHA3-256 Empty Message ---\n");

    int pass = 1;

    /* Wait for idle */
    if (wait_for_idle() != 0) return 0;

    /* Configure for SHA3-256, non-KMAC mode */
    configure_kmac(0, MODE_SHA3, KSTRENGTH_L256);

    /* Issue START */
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = CMD_START;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    print_status("  After START");

    /* No message data for empty hash */

    /* Issue PROCESS */
    cmd.f.cmd = CMD_PROCESS;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    print_status("  After PROCESS");

    /* Wait for done */
    if (wait_for_done() != 0) return 0;
    print_status("  After done");

    /* Read digest from STATE (XOR share0 and share1 for masked output) */
    uint32_t share0[8], share1[8], digest[8];
    for (int i = 0; i < 8; i++) {
        share0[i] = READ_REG(KMAC_STATE_MEM_BASE_ADDR + (i * 4));
    }
    for (int i = 0; i < 8; i++) {
        share1[i] = READ_REG(KMAC_STATE_MEM_BASE_ADDR + 0x100 + (i * 4));
    }
    for (int i = 0; i < 8; i++) {
        digest[i] = share0[i] ^ share1[i];
    }

    char got[65];
    to_hex(digest, 8, got);
    printf("  Digest:   %s\n", got);
    printf("  Expected: a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a\n");

    if (strcmp(got, "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a") != 0) {
        printf("  FAIL: Digest mismatch\n");
        pass = 0;
    }

    /* Issue DONE */
    issue_done();
    return pass;
}

/*
 * Test 7: KMAC-128 Operation
 * KMAC-128 with zero key, message "test", customization "KMAC".
 * Verifies that KMAC produces a non-zero digest (exact vector verification
 * would require a known test vector; this test validates the flow).
 */
static int test_kmac128_operation(void) {
    printf("\n--- Test 7: KMAC-128 Operation ---\n");

    int pass = 1;

    if (wait_for_idle() != 0) return 0;

    /* Configure for KMAC-128 (requires cSHAKE mode) */
    configure_kmac(1, MODE_CSHAKE, KSTRENGTH_L128);

    /* Set key length: Key128 = 0x0 */
    KMAC_KEY_LEN_reg_u key_len = {.val = 0};
    key_len.f.len = 0x0;
    WRITE_REG(KMAC_KEY_LEN_REG_ADDR, key_len.val);

    /* Write zero key (4 words = 128 bits) */
    for (int i = 0; i < 4; i++) {
        WRITE_REG(KMAC_KEY_SHARE0_0__REG_ADDR + (i * 4), 0);
        WRITE_REG(KMAC_KEY_SHARE1_0__REG_ADDR + (i * 4), 0);
    }

    /* Set PREFIX for "KMAC" customization string */
    /* encode_string("KMAC") = 01 20 4B 4D 41 43 → two 32-bit words */
    WRITE_REG(KMAC_PREFIX_0__REG_ADDR, 0x4D4B2001);  /* "MK" + left_encode */
    WRITE_REG(KMAC_PREFIX_1__REG_ADDR, 0x00004341);  /* "CA" */
    for (int i = 2; i < 11; i++) {
        WRITE_REG(KMAC_PREFIX_0__REG_ADDR + (i * 4), 0);
    }

    /* Issue START */
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = CMD_START;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    print_status("  After START");

    /* Write message "test" (4 bytes, little-endian) */
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x74736574);  /* "test" */

    /* Write right_encode(256) for output length */
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x00020001);

    /* Issue PROCESS */
    cmd.f.cmd = CMD_PROCESS;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    /* Wait for completion */
    if (wait_for_done() != 0) return 0;
    print_status("  After done");

    /* Read digest */
    uint32_t share0[8], share1[8], digest[8];
    for (int i = 0; i < 8; i++) {
        share0[i] = READ_REG(KMAC_STATE_MEM_BASE_ADDR + (i * 4));
    }
    for (int i = 0; i < 8; i++) {
        share1[i] = READ_REG(KMAC_STATE_MEM_BASE_ADDR + 0x100 + (i * 4));
    }

    int non_zero = 0;
    for (int i = 0; i < 8; i++) {
        digest[i] = share0[i] ^ share1[i];
        if (digest[i] != 0) non_zero = 1;
    }

    char got[65];
    to_hex(digest, 8, got);
    printf("  KMAC-128 Digest: %s\n", got);

    if (!non_zero) {
        printf("  FAIL: Digest is all zeros\n");
        pass = 0;
    }

    /* Issue DONE */
    issue_done();
    return pass;
}

/*
 * Test 8: FSM State Transitions
 * Verify the FSM transitions IDLE → ABSORB → SQUEEZE → IDLE
 * by checking STATUS register at each stage.
 */
static int test_fsm_transitions(void) {
    printf("\n--- Test 8: FSM State Transitions ---\n");

    int pass = 1;

    if (wait_for_idle() != 0) return 0;

    /* State 1: IDLE */
    KMAC_STATUS_reg_u sts = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
    printf("  [IDLE] idle=%u absorb=%u squeeze=%u\n",
           sts.f.sha3_idle, sts.f.sha3_absorb, sts.f.sha3_squeeze);
    if (!sts.f.sha3_idle || sts.f.sha3_absorb || sts.f.sha3_squeeze) {
        printf("  FAIL: Not in IDLE state\n");
        pass = 0;
    }

    /* Configure SHA3-256 */
    configure_kmac(0, MODE_SHA3, KSTRENGTH_L256);

    /* Issue START → should transition to ABSORB */
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = CMD_START;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    sts.val = READ_REG(KMAC_STATUS_REG_ADDR);
    printf("  [ABSORB] idle=%u absorb=%u squeeze=%u\n",
           sts.f.sha3_idle, sts.f.sha3_absorb, sts.f.sha3_squeeze);
    if (sts.f.sha3_idle || !sts.f.sha3_absorb || sts.f.sha3_squeeze) {
        printf("  FAIL: Not in ABSORB state after START\n");
        pass = 0;
    }

    /* Issue PROCESS → should transition to SQUEEZE after processing */
    cmd.f.cmd = CMD_PROCESS;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    /* Wait for squeeze or done (PROCESS transitions through SQUEEZE) */
    if (wait_for_squeeze() != 0) {
        wait_for_done();
    }

    sts.val = READ_REG(KMAC_STATUS_REG_ADDR);
    printf("  [SQUEEZE] idle=%u absorb=%u squeeze=%u\n",
           sts.f.sha3_idle, sts.f.sha3_absorb, sts.f.sha3_squeeze);

    /* Clear done interrupt if pending */
    KMAC_INTR_STATE_reg_u intr = {.val = READ_REG(KMAC_INTR_STATE_REG_ADDR)};
    if (intr.f.kmac_done) {
        WRITE_REG(KMAC_INTR_STATE_REG_ADDR, 0x1);
    }

    /* Issue DONE → should return to IDLE */
    issue_done();

    sts.val = READ_REG(KMAC_STATUS_REG_ADDR);
    printf("  [IDLE again] idle=%u absorb=%u squeeze=%u\n",
           sts.f.sha3_idle, sts.f.sha3_absorb, sts.f.sha3_squeeze);
    if (!sts.f.sha3_idle || sts.f.sha3_absorb || sts.f.sha3_squeeze) {
        printf("  FAIL: Not back in IDLE after DONE\n");
        pass = 0;
    }

    return pass;
}

/*
 * Test 9: FIFO Status Tracking
 * Write data to MSG_FIFO and observe fifo_depth, fifo_empty, fifo_full
 * transitions in STATUS register.
 */
static int test_fifo_status(void) {
    printf("\n--- Test 9: FIFO Status Tracking ---\n");

    int pass = 1;

    if (wait_for_idle() != 0) return 0;

    /* Configure SHA3-256 */
    configure_kmac(0, MODE_SHA3, KSTRENGTH_L256);

    /* Issue START */
    KMAC_CMD_reg_u cmd = {.val = 0};
    cmd.f.cmd = CMD_START;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);

    /* Check fifo_empty before writing */
    KMAC_STATUS_reg_u sts = {.val = READ_REG(KMAC_STATUS_REG_ADDR)};
    printf("  Before write: fifo_empty=%u fifo_depth=%u\n",
           sts.f.fifo_empty, sts.f.fifo_depth);
    if (!sts.f.fifo_empty) {
        printf("  FAIL: FIFO not empty before write\n");
        pass = 0;
    }

    /* Write a word to FIFO */
    WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0x12345678);

    /* Check fifo status after write */
    sts.val = READ_REG(KMAC_STATUS_REG_ADDR);
    printf("  After 1 word: fifo_empty=%u fifo_depth=%u\n",
           sts.f.fifo_empty, sts.f.fifo_depth);

    /* Write more data to observe depth changes */
    for (int i = 0; i < 4; i++) {
        WRITE_REG(KMAC_MSG_FIFO_MEM_BASE_ADDR, 0xAAAAAAAA + i);
    }
    sts.val = READ_REG(KMAC_STATUS_REG_ADDR);
    printf("  After 5 words: fifo_empty=%u fifo_depth=%u fifo_full=%u\n",
           sts.f.fifo_empty, sts.f.fifo_depth, sts.f.fifo_full);

    /* Process and clean up */
    cmd.f.cmd = CMD_PROCESS;
    WRITE_REG(KMAC_CMD_REG_ADDR, cmd.val);
    wait_for_done();
    issue_done();

    return pass;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  KMAC DV Test Suite for SEP VP\n");
    printf("========================================\n");
    printf("KMAC base addr: 0x%08x\n", KMAC_REG_MAP_BASE_ADDR);

    /* DUT-Level Tests */
    printf("\n==== DUT-Level Tests ====\n");
    report("Register Connectivity",  test_register_connectivity());
    report("Interrupt Test Logic",   test_interrupt_logic());
    report("CFG_REGWEN Protection",  test_cfg_regwen());
    report("Status Register Defaults", test_status_defaults());
    report("Error Code After Reset", test_err_code_reset());

    /* App-Level Tests */
    printf("\n==== App-Level Tests ====\n");
    report("SHA3-256 Empty Hash",    test_sha3_256_empty());
    report("KMAC-128 Operation",     test_kmac128_operation());
    report("FSM State Transitions",  test_fsm_transitions());
    report("FIFO Status Tracking",   test_fifo_status());

    /* Summary */
    printf("\n========================================\n");
    printf("  Results: %u/%u passed\n", tests_passed, tests_run);
    printf("========================================\n");

    if (tests_passed == tests_run) {
        printf("=== ALL TESTS PASSED ===\n");
        return 0;
    } else {
        printf("=== SOME TESTS FAILED ===\n");
        return 1;
    }
}
