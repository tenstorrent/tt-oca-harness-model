/*
 * Mailbox Test for SEP Platform
 *
 * Exercises the AXI-Lite mailbox unit (axil_mailbox_sep_wrap) the way the SEP
 * core can reach it on hardware. The SEP crossbar gives the core access to the
 * whole 0x10A0_0000 aperture, so both sides of every channel are addressable
 * and no external agent is needed to move data: writing one side's WRITE_DATA
 * makes the word appear at the other side's READ_DATA.
 *
 * Aperture layout (axil_mailbox_sep_wrap.rdl), 8 channels:
 *   channel n outbound block @ 0x10A0_0000 + n*0x1000
 *   channel n inbound  block @ 0x10A0_0000 + n*0x1000 + 0x800
 *
 * Register map within a block (all 64-bit wide):
 *   0x00  WRITE_DATA  WO  — push one FIFO entry
 *   0x08  READ_DATA   RO  — pop one FIFO entry
 *   0x10  STATUS      RO  — bit[0]=empty, bit[1]=full, bit[2]/[3]=above threshold
 *   0x18  ERROR_FLAGS RO/clear-on-read — bit[0]=read_error, bit[1]=write_error
 *   0x20  WIRQT       RW  — write interrupt threshold
 *   0x28  RIRQT       RW  — read interrupt threshold
 *   0x30  IRQS        RW/W1C
 *   0x38  IRQEN       RW
 *   0x40  IRQP        RO
 *   0x48  CTRL        WO  — bit[0]=wflush, bit[1]=rflush
 *
 * Direction convention, matching the RTL cross-connection:
 *   outbound WRITE_DATA → inbound READ_DATA   (SEP → SMC path)
 *   inbound  WRITE_DATA → outbound READ_DATA  (SMC → SEP path)
 *
 * MailboxBridge in the VP widens these 32-bit accesses to the 64-bit register
 * width, pushing or popping one entry per bus transaction.
 */

#include <stdint.h>

extern int printf(const char *format, ...);

/* =========================================================================
 * Mailbox address map
 * ========================================================================= */
#define MBOX_BASE            0x10A00000u
#define MBOX_NUM_CHANNELS    8u
#define MBOX_CHANNEL_STRIDE  0x1000u
#define MBOX_INBOUND_OFFSET  0x800u
#define MBOX_DEPTH           8u

#define MBOX_OUTBOUND(ch)    (MBOX_BASE + (ch) * MBOX_CHANNEL_STRIDE)
#define MBOX_INBOUND(ch)     (MBOX_OUTBOUND(ch) + MBOX_INBOUND_OFFSET)

/* Register offsets within a block */
#define MBOX_WRITE_DATA      0x00u
#define MBOX_READ_DATA       0x08u
#define MBOX_STATUS          0x10u
#define MBOX_ERROR_FLAGS     0x18u
#define MBOX_WIRQT           0x20u
#define MBOX_RIRQT           0x28u
#define MBOX_IRQS            0x30u
#define MBOX_IRQEN           0x38u
#define MBOX_IRQP            0x40u
#define MBOX_CTRL            0x48u

/* STATUS bits */
#define MBOX_STATUS_EMPTY    (1u << 0)  /* inbound FIFO of this side is empty  */
#define MBOX_STATUS_FULL     (1u << 1)  /* outbound FIFO of this side is full  */

/* ERROR_FLAGS bits */
#define MBOX_ERR_READ        (1u << 0)
#define MBOX_ERR_WRITE       (1u << 1)

/* CTRL bits */
#define MBOX_CTRL_WFLUSH     (1u << 0)
#define MBOX_CTRL_RFLUSH     (1u << 1)

/* Register helpers */
#define REG_READ(addr)       (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val) (*((volatile uint32_t *)(addr)) = (uint32_t)(val))

/* =========================================================================
 * Test state
 * ========================================================================= */
static volatile uint32_t test_passed = 0;
static volatile uint32_t test_failed = 0;

static void check(int condition, const char *what)
{
    if (condition) {
        printf("  %s [PASS]\n", what);
        test_passed++;
    } else {
        printf("  %s [FAIL]\n", what);
        test_failed++;
    }
}

/* =========================================================================
 * TC01 — Reset state of every channel
 * ========================================================================= */
static void tc01_reset_state(void)
{
    printf("TC01: Reset state of all %u channels\n", MBOX_NUM_CHANNELS);

    int all_empty = 1;
    int all_clear = 1;

    for (uint32_t ch = 0; ch < MBOX_NUM_CHANNELS; ch++) {
        uint32_t out_status = REG_READ(MBOX_OUTBOUND(ch) + MBOX_STATUS);
        uint32_t in_status  = REG_READ(MBOX_INBOUND(ch)  + MBOX_STATUS);

        if (!(out_status & MBOX_STATUS_EMPTY) || (out_status & MBOX_STATUS_FULL))
            all_empty = 0;
        if (!(in_status & MBOX_STATUS_EMPTY) || (in_status & MBOX_STATUS_FULL))
            all_empty = 0;

        /* Per the RDL both error flags reset to 0. */
        if (REG_READ(MBOX_OUTBOUND(ch) + MBOX_ERROR_FLAGS) != 0u)
            all_clear = 0;
    }

    check(all_empty, "All channels empty and not full at reset");
    check(all_clear, "ERROR_FLAGS reads 0 at reset on all channels");
}

/* =========================================================================
 * TC02 — SMC-to-SEP direction: inbound WRITE_DATA to outbound READ_DATA
 * ========================================================================= */
static void tc02_inbound_to_outbound(void)
{
    printf("TC02: inbound WRITE_DATA -> outbound READ_DATA (channel 0)\n");

    const uint32_t value = 0xDEADBEEFu;

    REG_WRITE(MBOX_INBOUND(0) + MBOX_WRITE_DATA, value);

    uint32_t out_status = REG_READ(MBOX_OUTBOUND(0) + MBOX_STATUS);
    check((out_status & MBOX_STATUS_EMPTY) == 0u,
          "Outbound side reports data available");

    uint32_t got = REG_READ(MBOX_OUTBOUND(0) + MBOX_READ_DATA);
    printf("  wrote 0x%08x, read 0x%08x\n", value, got);
    check(got == value, "Data crossed to the outbound side intact");

    out_status = REG_READ(MBOX_OUTBOUND(0) + MBOX_STATUS);
    check((out_status & MBOX_STATUS_EMPTY) != 0u,
          "Outbound side empty again after the pop");
}

/* =========================================================================
 * TC03 — SEP-to-SMC direction, several words in order
 * ========================================================================= */
static void tc03_outbound_to_inbound(void)
{
    printf("TC03: outbound WRITE_DATA -> inbound READ_DATA, 3 words in order\n");

    const uint32_t values[3] = { 0x11223344u, 0xAABBCCDDu, 0x00FF00FFu };

    for (int i = 0; i < 3; i++)
        REG_WRITE(MBOX_OUTBOUND(0) + MBOX_WRITE_DATA, values[i]);

    int in_order = 1;
    for (int i = 0; i < 3; i++) {
        uint32_t got = REG_READ(MBOX_INBOUND(0) + MBOX_READ_DATA);
        printf("  word %d: expected 0x%08x, read 0x%08x\n", i, values[i], got);
        if (got != values[i])
            in_order = 0;
    }

    check(in_order, "Words arrive on the inbound side in FIFO order");
}

/* =========================================================================
 * TC04 — Channels are independent
 * ========================================================================= */
static void tc04_channel_independence(void)
{
    printf("TC04: All %u channels are independent\n", MBOX_NUM_CHANNELS);

    /* Give every channel a distinct word on the SMC-to-SEP path. */
    for (uint32_t ch = 0; ch < MBOX_NUM_CHANNELS; ch++)
        REG_WRITE(MBOX_INBOUND(ch) + MBOX_WRITE_DATA, 0xC0DE0000u + ch);

    int all_match = 1;
    for (uint32_t ch = 0; ch < MBOX_NUM_CHANNELS; ch++) {
        uint32_t expected = 0xC0DE0000u + ch;
        uint32_t got = REG_READ(MBOX_OUTBOUND(ch) + MBOX_READ_DATA);
        printf("  channel %u: expected 0x%08x, read 0x%08x\n", ch, expected, got);
        if (got != expected)
            all_match = 0;
    }

    check(all_match, "Each channel returns its own word, no crosstalk");

    /* A word left on one channel must not make another channel non-empty. */
    REG_WRITE(MBOX_INBOUND(3) + MBOX_WRITE_DATA, 0x5A5A5A5Au);

    int others_empty = 1;
    for (uint32_t ch = 0; ch < MBOX_NUM_CHANNELS; ch++) {
        uint32_t status = REG_READ(MBOX_OUTBOUND(ch) + MBOX_STATUS);
        int empty = (status & MBOX_STATUS_EMPTY) != 0;
        if (ch == 3 ? empty : !empty)
            others_empty = 0;
    }
    check(others_empty, "Only channel 3 reports data pending");

    (void)REG_READ(MBOX_OUTBOUND(3) + MBOX_READ_DATA);
}

/* =========================================================================
 * TC05 — Threshold configuration with saturation
 * ========================================================================= */
static void tc05_threshold_config(void)
{
    printf("TC05: Threshold configuration and saturation\n");

    REG_WRITE(MBOX_OUTBOUND(0) + MBOX_WIRQT, 3u);
    REG_WRITE(MBOX_OUTBOUND(0) + MBOX_RIRQT, 2u);

    uint32_t wirqt = REG_READ(MBOX_OUTBOUND(0) + MBOX_WIRQT) & 0xFFu;
    uint32_t rirqt = REG_READ(MBOX_OUTBOUND(0) + MBOX_RIRQT) & 0xFFu;
    printf("  WIRQT = %u, RIRQT = %u\n", wirqt, rirqt);
    check(wirqt == 3u && rirqt == 2u, "Thresholds read back as written");

    /* A threshold at or above the FIFO depth saturates to depth-1. */
    REG_WRITE(MBOX_OUTBOUND(0) + MBOX_WIRQT, 0xFFu);
    uint32_t saturated = REG_READ(MBOX_OUTBOUND(0) + MBOX_WIRQT) & 0xFFu;
    printf("  WIRQT after writing 0xFF = %u\n", saturated);
    check(saturated == MBOX_DEPTH - 1u, "Oversized threshold saturates to depth-1");

    REG_WRITE(MBOX_OUTBOUND(0) + MBOX_WIRQT, 0u);
    REG_WRITE(MBOX_OUTBOUND(0) + MBOX_RIRQT, 0u);
}

/* =========================================================================
 * TC06 — Interrupt pending logic on the outbound side
 * ========================================================================= */
static void tc06_interrupt_pending(void)
{
    printf("TC06: IRQS/IRQEN/IRQP on the outbound side of channel 1\n");

    const uint32_t base = MBOX_OUTBOUND(1);

    /* RIRQT=0 means any word waiting to be read raises the read-threshold IRQ. */
    REG_WRITE(base + MBOX_RIRQT, 0u);
    REG_WRITE(base + MBOX_IRQS, 0x7u);   /* W1C: start from a clean slate */
    REG_WRITE(base + MBOX_IRQEN, 0x0u);  /* all sources masked */

    REG_WRITE(MBOX_INBOUND(1) + MBOX_WRITE_DATA, 0x12345678u);

    uint32_t irqs = REG_READ(base + MBOX_IRQS) & 0x7u;
    uint32_t irqp = REG_READ(base + MBOX_IRQP) & 0x7u;
    printf("  masked: IRQS = 0x%x, IRQP = 0x%x\n", irqs, irqp);
    check((irqs & 0x2u) != 0u, "Read-threshold status bit set");
    check(irqp == 0u, "IRQP stays clear while the source is masked");

    REG_WRITE(base + MBOX_IRQEN, 0x2u);
    irqp = REG_READ(base + MBOX_IRQP) & 0x7u;
    printf("  enabled: IRQP = 0x%x\n", irqp);
    check((irqp & 0x2u) != 0u, "IRQP follows IRQS & IRQEN once enabled");

    /* Drain and acknowledge. */
    (void)REG_READ(base + MBOX_READ_DATA);
    REG_WRITE(base + MBOX_IRQS, 0x7u);
    irqp = REG_READ(base + MBOX_IRQP) & 0x7u;
    check(irqp == 0u, "IRQP clears after write-1-to-clear acknowledge");

    REG_WRITE(base + MBOX_IRQEN, 0x0u);
}

/* =========================================================================
 * TC07 — Read from an empty FIFO reports an error
 *
 * The mailbox answers this access with RESP_SLVERR and drives 0xFEEDDEAD on
 * the read data. The sentinel is not observable from here: a load that takes a
 * bus error does not deliver data to the VeeR EL2 core, which reports it as an
 * imprecise load bus error instead. What firmware can check is the latched
 * error flag, so that is what this case asserts.
 * ========================================================================= */
static void tc07_read_empty(void)
{
    printf("TC07: Reading an empty FIFO\n");

    const uint32_t base = MBOX_OUTBOUND(2);

    /* Clear any latched flags first: ERROR_FLAGS is clear-on-read. */
    (void)REG_READ(base + MBOX_ERROR_FLAGS);

    uint32_t status = REG_READ(base + MBOX_STATUS);
    check((status & MBOX_STATUS_EMPTY) != 0u, "Channel 2 starts empty");

    (void)REG_READ(base + MBOX_READ_DATA);

    uint32_t flags = REG_READ(base + MBOX_ERROR_FLAGS);
    printf("  ERROR_FLAGS = 0x%08x\n", flags);
    check((flags & MBOX_ERR_READ) != 0u, "read_error flag latched");

    flags = REG_READ(base + MBOX_ERROR_FLAGS);
    check(flags == 0u, "ERROR_FLAGS cleared by the read");

    /* The failed read must not have popped anything. */
    status = REG_READ(base + MBOX_STATUS);
    check((status & MBOX_STATUS_EMPTY) != 0u, "FIFO still empty after the failed read");
}

/* =========================================================================
 * TC08 — Filling the FIFO and overflowing it
 * ========================================================================= */
static void tc08_fill_and_overflow(void)
{
    printf("TC08: Fill to depth %u then overflow\n", MBOX_DEPTH);

    const uint32_t out_base = MBOX_OUTBOUND(4);
    const uint32_t in_base  = MBOX_INBOUND(4);

    (void)REG_READ(out_base + MBOX_ERROR_FLAGS);

    for (uint32_t i = 0; i < MBOX_DEPTH; i++)
        REG_WRITE(out_base + MBOX_WRITE_DATA, 0x1000u + i);

    uint32_t status = REG_READ(out_base + MBOX_STATUS);
    printf("  STATUS after %u writes = 0x%08x\n", MBOX_DEPTH, status);
    check((status & MBOX_STATUS_FULL) != 0u, "STATUS.full set at depth");

    /* One more write must be discarded and flagged. */
    REG_WRITE(out_base + MBOX_WRITE_DATA, 0xBADBADu);

    uint32_t flags = REG_READ(out_base + MBOX_ERROR_FLAGS);
    printf("  ERROR_FLAGS after overflow = 0x%08x\n", flags);
    check((flags & MBOX_ERR_WRITE) != 0u, "write_error flag latched on overflow");

    /* The discarded word must not appear in the stream. */
    int in_order = 1;
    for (uint32_t i = 0; i < MBOX_DEPTH; i++) {
        uint32_t got = REG_READ(in_base + MBOX_READ_DATA);
        if (got != 0x1000u + i)
            in_order = 0;
    }
    check(in_order, "The 8 accepted words read back in order");

    status = REG_READ(in_base + MBOX_STATUS);
    check((status & MBOX_STATUS_EMPTY) != 0u, "FIFO empty after draining all 8");
}

/* =========================================================================
 * TC09 — CTRL flush
 * ========================================================================= */
static void tc09_ctrl_flush(void)
{
    printf("TC09: CTRL flush\n");

    const uint32_t out_base = MBOX_OUTBOUND(5);

    /* wflush drains the FIFO this side writes into. */
    REG_WRITE(out_base + MBOX_WRITE_DATA, 0x1u);
    REG_WRITE(out_base + MBOX_WRITE_DATA, 0x2u);

    uint32_t in_status = REG_READ(MBOX_INBOUND(5) + MBOX_STATUS);
    check((in_status & MBOX_STATUS_EMPTY) == 0u, "Peer sees the queued words");

    REG_WRITE(out_base + MBOX_CTRL, MBOX_CTRL_WFLUSH);

    in_status = REG_READ(MBOX_INBOUND(5) + MBOX_STATUS);
    printf("  peer STATUS after wflush = 0x%08x\n", in_status);
    check((in_status & MBOX_STATUS_EMPTY) != 0u, "wflush drained the outbound FIFO");

    /* rflush drains the FIFO this side reads from. */
    REG_WRITE(MBOX_INBOUND(5) + MBOX_WRITE_DATA, 0x3u);
    uint32_t out_status = REG_READ(out_base + MBOX_STATUS);
    check((out_status & MBOX_STATUS_EMPTY) == 0u, "Outbound side sees a pending word");

    REG_WRITE(out_base + MBOX_CTRL, MBOX_CTRL_RFLUSH);
    out_status = REG_READ(out_base + MBOX_STATUS);
    printf("  STATUS after rflush = 0x%08x\n", out_status);
    check((out_status & MBOX_STATUS_EMPTY) != 0u, "rflush drained the inbound FIFO");
}

/* =========================================================================
 * Main
 * ========================================================================= */
int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("Mailbox Test for SEP Platform\n");
    printf("========================================\n");
    printf("Aperture : 0x%08x, %u channels, depth %u\n",
           MBOX_BASE, MBOX_NUM_CHANNELS, MBOX_DEPTH);
    printf("Access   : 32-bit, widened by MailboxBridge\n");
    printf("\n");

    tc01_reset_state();
    printf("\n");
    tc02_inbound_to_outbound();
    printf("\n");
    tc03_outbound_to_inbound();
    printf("\n");
    tc04_channel_independence();
    printf("\n");
    tc05_threshold_config();
    printf("\n");
    tc06_interrupt_pending();
    printf("\n");
    tc07_read_empty();
    printf("\n");
    tc08_fill_and_overflow();
    printf("\n");
    tc09_ctrl_flush();

    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Passed: %u\n", test_passed);
    printf("Failed: %u\n", test_failed);

    if (test_failed == 0) {
        printf("\nAll tests PASSED!\n");
    } else {
        printf("\nSome tests FAILED!\n");
    }

    printf("\nTest complete. Press Ctrl-a, Ctrl-x to exit.\n");

    while (1) {
        __asm__("wfi");
    }

    return 0;
}
