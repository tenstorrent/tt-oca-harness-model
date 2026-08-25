// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SEP eFuse Integration Test
 *
 * Verifies that the fuse array preloaded at elaboration is reflected in the MMIO
 * shadow registers, that the lock registers accumulate write-one-to-set, and that
 * the program and read interfaces enforce their enable bit.
 *
 * Base address: 0x10930000  (sep_efuse_start_addr in Args.hpp)
 *
 * Expected values come from the image efuse_vp.ini names,
 * sep/peripherals/efuse/config/default_efuse.preload — the same file the RTL
 * testbench loads via +sep_preload_efuse — so this test checks the VP comes up as
 * the same part the RTL does:
 *   LC_STATE     0xF0        the differential encoding {~raw, raw} of raw 0, TEST_DEV
 *   LOCKS_LO/HI  0           no locks burned; ROM sets the ones it wants at runtime
 *   CHIPLET_UID  word 0 = 0xdeadbeef, remaining words 0
 *   SBOOT_DIS, SIP_DIS_*, SYS_DIS_*, STATUS_RPT  all erased
 */

#include <stdint.h>

extern int printf(const char *format, ...);

/* ------------------------------------------------------------------ */
/* Register map                                                         */
/* ------------------------------------------------------------------ */
#define EFUSE_BASE  0x10930000u

/* SEP_EFUSE_MAP */
#define EFUSE_LOCKS_LO      (EFUSE_BASE + 0x000u)
#define EFUSE_LOCKS_HI      (EFUSE_BASE + 0x004u)
#define EFUSE_LC_STATE      (EFUSE_BASE + 0x008u)
#define EFUSE_SBOOT_DIS     (EFUSE_BASE + 0x00Cu)
#define EFUSE_SIP_DIS_LO    (EFUSE_BASE + 0x014u)
#define EFUSE_SIP_DIS_HI    (EFUSE_BASE + 0x018u)
#define EFUSE_SYS_DIS_LO    (EFUSE_BASE + 0x01Cu)
#define EFUSE_SYS_DIS_HI    (EFUSE_BASE + 0x020u)
#define EFUSE_CHIPLET_UID_BASE (EFUSE_BASE + 0x0C8u)  /* [8] words */
#define EFUSE_STATUS_RPT    (EFUSE_BASE + 0x168u)

/* EFUSE_INTERFACE_CTRL */
#define EFUSE_STATUS          (EFUSE_BASE + 0x400u)
#define EFUSE_PROGRAM_CTRL      (EFUSE_BASE + 0x404u)
#define EFUSE_READ_CTRL       (EFUSE_BASE + 0x408u)
#define EFUSE_PROGRAM_RD_DATA (EFUSE_BASE + 0x40Cu)
#define EFUSE_READ_RD_DATA    (EFUSE_BASE + 0x410u)

/* EFUSE_PROGRAM_CTRL bits. Bits [15:0] are the bit address within the array. */
#define WC_PROGRAM_DATA     (1u << 16)
#define WC_PROGRAM_GO       (1u << 17)
#define WC_READ_BACK        (1u << 18)
#define WC_PROGRAM_BUSY     (1u << 24)
#define WC_PROGRAM_DONE     (1u << 25)
#define WC_PROGRAM_STATUS   (1u << 26)
#define WC_PROGRAM_ENABLE   (1u << 27)

/* EFUSE_READ_CTRL bits */
#define RC_READ_GO          (1u << 16)
#define RC_READ_BUSY        (1u << 24)
#define RC_READ_DONE        (1u << 25)
#define RC_READ_STATUS      (1u << 26)
#define RC_READ_ENABLE      (1u << 28)

/* Bit 0 of a word past every field the lock test touches, so the program and read
 * handshakes are unaffected by the locks that test 3 burns. */
#define SPARE_BIT_ADDR      2112u

#define REG_READ(addr)       (*((volatile uint32_t *)(uintptr_t)(addr)))
#define REG_WRITE(addr, val) (*((volatile uint32_t *)(uintptr_t)(addr)) = (val))

/* ------------------------------------------------------------------ */
/* Expected values — must match default_efuse.preload                  */
/* ------------------------------------------------------------------ */
/* The fuse holds the lifecycle state differentially encoded, so raw 0 (TEST_DEV)
 * appears as 0xF0. A single flipped bit therefore yields no legal state at all
 * rather than a quietly different one. */
#define EXP_LC_STATE    0xF0u
#define EXP_STATUS_RPT  0u

static const uint32_t exp_chiplet_uid[8] = {0xdeadbeefu, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

/* ------------------------------------------------------------------ */
/* Test infrastructure                                                  */
/* ------------------------------------------------------------------ */
static volatile uint32_t test_passed = 0;
static volatile uint32_t test_failed = 0;

static void check(const char *name, uint32_t got, uint32_t expected)
{
    if (got == expected) {
        printf("  [PASS] %s: 0x%08x\n", name, got);
        test_passed++;
    } else {
        printf("  [FAIL] %s: got=0x%08x  exp=0x%08x\n", name, got, expected);
        test_failed++;
    }
}

/* ------------------------------------------------------------------ */
/* Test 1: Scalar fuse registers reflect ini values                    */
/* ------------------------------------------------------------------ */
static void test_fuse_load_scalar(void)
{
    printf("\nTest 1: Scalar fuse registers (preloaded array)\n");

    check("LC_STATE",   REG_READ(EFUSE_LC_STATE),   EXP_LC_STATE);
    check("STATUS_RPT", REG_READ(EFUSE_STATUS_RPT), EXP_STATUS_RPT);
    check("SBOOT_DIS",  REG_READ(EFUSE_SBOOT_DIS),  0u);
    check("SIP_DIS_LO", REG_READ(EFUSE_SIP_DIS_LO), 0u);
    check("SIP_DIS_HI", REG_READ(EFUSE_SIP_DIS_HI), 0u);
    check("SYS_DIS_LO", REG_READ(EFUSE_SYS_DIS_LO), 0u);
    check("SYS_DIS_HI", REG_READ(EFUSE_SYS_DIS_HI), 0u);
}

/* ------------------------------------------------------------------ */
/* Test 2: CHIPLET_UID array reflects ini values                       */
/* ------------------------------------------------------------------ */
static void test_fuse_load_chiplet_uid(void)
{
    printf("\nTest 2: CHIPLET_UID array (preloaded array)\n");

    for (int i = 0; i < 8; i++) {
        uint32_t addr = EFUSE_CHIPLET_UID_BASE + (uint32_t)(i * 4);
        uint32_t val  = REG_READ(addr);
        if (val == exp_chiplet_uid[i]) {
            printf("  [PASS] CHIPLET_UID[%u]: 0x%08x\n", (uint32_t)i, val);
            test_passed++;
        } else {
            printf("  [FAIL] CHIPLET_UID[%u]: got=0x%08x  exp=0x%08x\n",
                   (uint32_t)i, val, exp_chiplet_uid[i]);
            test_failed++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Test 3: WOSET behaviour on LOCKS_LO                                 */
/* Bits can only be OR-accumulated; writes of 0 do nothing.            */
/* ------------------------------------------------------------------ */
static void test_woset_locks(void)
{
    printf("\nTest 3: WOSET accumulation (LOCKS_LO)\n");

    /* The image burns no locks, matching a fresh part: ROM sets the locks it wants
     * before handing off to BL1, so they are runtime state rather than fuse state. */
    check("initial LOCKS_LO", REG_READ(EFUSE_LOCKS_LO), 0u);

    /* Write 0x3 -> expect 0x3 */
    REG_WRITE(EFUSE_LOCKS_LO, 0x3u);
    check("after write 0x3", REG_READ(EFUSE_LOCKS_LO), 0x3u);

    /* Write 0xC -> expect 0xF (OR accumulation) */
    REG_WRITE(EFUSE_LOCKS_LO, 0xCu);
    check("after write 0xC", REG_READ(EFUSE_LOCKS_LO), 0xFu);

    /* Write 0x0 -> bits must stay (WOSET: no clear) */
    REG_WRITE(EFUSE_LOCKS_LO, 0x0u);
    check("after write 0x0", REG_READ(EFUSE_LOCKS_LO), 0xFu);
}

/* ------------------------------------------------------------------ */
/* Test 4: EFUSE_PROGRAM_CTRL — program handshake                        */
/*                                                                     */
/* A go pulse always completes in the same access: the interface never  */
/* stalls, it answers done and reports the outcome in status. A command */
/* issued without program_enable is refused, which is what makes a      */
/* stray write unable to burn a fuse.                                   */
/* ------------------------------------------------------------------ */
static void test_program_handshake(void)
{
    printf("\nTest 4: EFUSE_PROGRAM_CTRL program handshake\n");

    REG_WRITE(EFUSE_PROGRAM_CTRL, WC_PROGRAM_GO | WC_PROGRAM_DATA | SPARE_BIT_ADDR);
    uint32_t ctrl = REG_READ(EFUSE_PROGRAM_CTRL);
    printf("  after go without program_enable: 0x%08x\n", ctrl);

    check("program_go cleared",       (ctrl >> 17) & 1u, 0u);
    check("program_busy=0",           (ctrl >> 24) & 1u, 0u);
    check("program_done=1",           (ctrl >> 25) & 1u, 1u);
    check("program_status=1 (denied)", (ctrl >> 26) & 1u, 1u);

    /* Same command with program_enable set, plus read_back so the interface
     * returns the word it just burned. */
    REG_WRITE(EFUSE_PROGRAM_CTRL,
              WC_PROGRAM_GO | WC_PROGRAM_DATA | WC_READ_BACK | WC_PROGRAM_ENABLE |
              SPARE_BIT_ADDR);
    ctrl = REG_READ(EFUSE_PROGRAM_CTRL);
    printf("  after go with program_enable: 0x%08x\n", ctrl);

    check("program_done=1",            (ctrl >> 25) & 1u, 1u);
    check("program_status=0 (allowed)", (ctrl >> 26) & 1u, 0u);
    check("read_back shows burned bit",
          REG_READ(EFUSE_PROGRAM_RD_DATA) & 1u, 1u);
}

/* ------------------------------------------------------------------ */
/* Test 5: EFUSE_READ_CTRL — read handshake                            */
/* Reads the bit test 4 burned back out of the array.                  */
/* ------------------------------------------------------------------ */
static void test_read_handshake(void)
{
    printf("\nTest 5: EFUSE_READ_CTRL read handshake\n");

    REG_WRITE(EFUSE_READ_CTRL, RC_READ_GO | SPARE_BIT_ADDR);
    uint32_t ctrl = REG_READ(EFUSE_READ_CTRL);
    printf("  after go without read_enable: 0x%08x\n", ctrl);

    check("read_go cleared",        (ctrl >> 16) & 1u, 0u);
    check("read_busy=0",            (ctrl >> 24) & 1u, 0u);
    check("read_done=1",            (ctrl >> 25) & 1u, 1u);
    check("read_status=1 (denied)",  (ctrl >> 26) & 1u, 1u);
    /* A denied read returns zero rather than stale data, so a caller that ignores
     * the status bit cannot mistake a refusal for a cleared fuse. */
    check("denied read returns 0",  REG_READ(EFUSE_READ_RD_DATA), 0u);

    REG_WRITE(EFUSE_READ_CTRL, RC_READ_GO | RC_READ_ENABLE | SPARE_BIT_ADDR);
    ctrl = REG_READ(EFUSE_READ_CTRL);
    printf("  after go with read_enable: 0x%08x\n", ctrl);

    check("read_status=0 (allowed)", (ctrl >> 26) & 1u, 0u);
    check("reads back the burned bit", REG_READ(EFUSE_READ_RD_DATA) & 1u, 1u);
}

/* ------------------------------------------------------------------ */
/* Test 6: STATUS reports the refusals and clears write-1-to-clear     */
/* ------------------------------------------------------------------ */
static void test_status_sticky(void)
{
    printf("\nTest 6: STATUS sticky req_error\n");

    uint32_t status = REG_READ(EFUSE_STATUS);
    check("sense_done=1",           status & 1u, 1u);
    check("req_error latched",      (status >> 4) & 1u, 1u);

    /* The status bits themselves are read-only; each has its own clear control higher
     * up the word, so firmware acknowledges an error explicitly rather than by writing
     * back what it just read. */
    REG_WRITE(EFUSE_STATUS, 1u << 8);
    check("req_error cleared", (REG_READ(EFUSE_STATUS) >> 4) & 1u, 0u);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("\n=== SEP eFuse Integration Test ===\n");
    printf("Base: 0x%08x\n", EFUSE_BASE);

    test_fuse_load_scalar();
    test_fuse_load_chiplet_uid();
    test_woset_locks();
    test_program_handshake();
    test_read_handshake();
    test_status_sticky();

    printf("\n=== Summary ===\n");
    printf("Passed: %u\n", test_passed);
    printf("Failed: %u\n", test_failed);

    if (test_failed == 0)
        printf("\nAll tests PASSED!\n\n");
    else
        printf("\nSome tests FAILED!\n\n");

    return 0;
}
