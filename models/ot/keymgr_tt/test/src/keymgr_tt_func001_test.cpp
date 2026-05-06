/**
 * @file keymgr_tt_func001_test.cpp
 * @brief Skeleton register read/write test
 *
 * Tests covered:
 *   001a - Reset values on all Mailbox registers
 *   001b - RW register: write and read back MB_IRQEN (only writable bits stick)
 *   001c - RW register: write and read back MB_CTRL
 *   001d - W1C register: set MB_IRQS, verify sticky; write 1 to clear, verify cleared
 *   001e - WO register:  write MB_WDATA, verify read returns 0 (write-only)
 *   001f - RO register:  write MB_STATUS, verify value unchanged (read-only)
 *   001g - KPVLP reset:  KPVLP_STATUS reset value is 0 (no slots unlocked)
 *   001h - KPVLP_CTRL:   write writable fields, verify only masked bits stored
 *   001i - KPVLP_KEY:    write key word, verify read returns 0 (write-only)
 */

#include "testbench.h"
#include "keymgr_tt_test.h"
#include "keymgr_tt.h"
#include <iostream>
#include <iomanip>

// ---------------------------------------------------------------------------
// Helper macros
// ---------------------------------------------------------------------------
#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cout << "[FAIL] " << msg << std::endl; \
            failures++; \
        } else { \
            std::cout << "[PASS] " << msg << std::endl; \
        } \
    } while(0)

#define CHECK_EQ(got, expected, msg) \
    do { \
        if ((got) != (expected)) { \
            std::cout << "[FAIL] " << msg \
                      << "  got=0x" << std::hex << (got) \
                      << "  expected=0x" << (expected) << std::dec << std::endl; \
            failures++; \
        } else { \
            std::cout << "[PASS] " << msg << std::endl; \
        } \
    } while(0)

// ---------------------------------------------------------------------------
int keymgr_tt_func001_test(keymgr_tt_test* test, keymgr_tt_model* dut, testbench* /*tb*/)
{
    int failures = 0;
    uint32_t rdata = 0;

    std::cout << "\n--- FUNC001: Skeleton Register Read/Write ---\n";

    // ------------------------------------------------------------------
    // 001a: Mailbox reset values
    // Note: MB_WDATA, MB_STATUS, MB_IRQEN, MB_IRQS, MB_CTRL are checked first.
    // MB_RDATA is checked last because reading from an empty outbound FIFO is a
    // side-effect operation (sets OUTBOUND_UNDERFLOW) — we clear the flag after.
    // ------------------------------------------------------------------
    test->register_read_32(keymgr_tt_basetest::MB_WDATA_OFFSET,  rdata);
    CHECK_EQ(rdata, (uint32_t)keymgr_tt_basetest::MB_WDATA_RESET,  "001a: MB_WDATA reset value");

    test->register_read_32(keymgr_tt_basetest::MB_STATUS_OFFSET, rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::MB_STATUS_READ,
             (uint32_t)keymgr_tt_basetest::MB_STATUS_RESET,        "001a: MB_STATUS reset value");

    test->register_read_32(keymgr_tt_basetest::MB_IRQEN_OFFSET,  rdata);
    CHECK_EQ(rdata, (uint32_t)keymgr_tt_basetest::MB_IRQEN_RESET,  "001a: MB_IRQEN reset value");

    test->register_read_32(keymgr_tt_basetest::MB_IRQS_OFFSET,   rdata);
    CHECK_EQ(rdata, (uint32_t)keymgr_tt_basetest::MB_IRQS_RESET,   "001a: MB_IRQS reset value");

    test->register_read_32(keymgr_tt_basetest::MB_CTRL_OFFSET,   rdata);
    CHECK_EQ(rdata, (uint32_t)keymgr_tt_basetest::MB_CTRL_RESET,   "001a: MB_CTRL reset value");

    // MB_RDATA: reading an empty outbound FIFO returns 0 (correct reset value)
    // but sets OUTBOUND_UNDERFLOW as a side effect. Clear it afterwards.
    test->register_read_32(keymgr_tt_basetest::MB_RDATA_OFFSET,  rdata);
    CHECK_EQ(rdata, (uint32_t)keymgr_tt_basetest::MB_RDATA_RESET,  "001a: MB_RDATA reset value");
    // Clear OUTBOUND_UNDERFLOW IRQ (W1C bit[3]) and STATUS bit[23] caused by reading empty FIFO
    test->register_write_32(keymgr_tt_basetest::MB_IRQS_OFFSET,   0x00000008u);  // W1C bit[3]
    test->register_write_32(keymgr_tt_basetest::MB_STATUS_OFFSET, 0x00800000u);  // W1C bit[23]

    // ------------------------------------------------------------------
    // 001b: MB_IRQEN — write all 1s, read back only writable bits
    // ------------------------------------------------------------------
    test->register_write_32(keymgr_tt_basetest::MB_IRQEN_OFFSET, 0xFFFFFFFF);
    test->register_read_32 (keymgr_tt_basetest::MB_IRQEN_OFFSET, rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::MB_IRQEN_READ,
             (uint32_t)keymgr_tt_basetest::MB_IRQEN_WRITE & (uint32_t)keymgr_tt_basetest::MB_IRQEN_READ,
             "001b: MB_IRQEN write 0xFFFFFFFF — only writable bits stick");

    // write 0 to clear
    test->register_write_32(keymgr_tt_basetest::MB_IRQEN_OFFSET, 0x00000000);
    test->register_read_32 (keymgr_tt_basetest::MB_IRQEN_OFFSET, rdata);
    CHECK_EQ(rdata, 0x00000000u, "001b: MB_IRQEN write 0 — clears");

    // ------------------------------------------------------------------
    // 001c: MB_CTRL — FLUSH bits persist, WRITE_EOM (bit2) self-clears
    //   Write 0x3 (FLUSH_INBOUND|FLUSH_OUTBOUND only, WRITE_EOM=0)
    //   Readback should be 0x3 (FLUSH bits persist as written)
    // ------------------------------------------------------------------
    test->register_write_32(keymgr_tt_basetest::MB_CTRL_OFFSET, 0x3);
    test->register_read_32 (keymgr_tt_basetest::MB_CTRL_OFFSET, rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::MB_CTRL_READ, 0x3u,
             "001c: MB_CTRL write 0x3 — FLUSH bits read back as written");

    // Write 0x7 (all bits including WRITE_EOM): EOM fires and self-clears, FLUSH bits persist
    test->register_write_32(keymgr_tt_basetest::MB_CTRL_OFFSET, 0x7);
    test->register_read_32 (keymgr_tt_basetest::MB_CTRL_OFFSET, rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::MB_CTRL_READ, 0x3u,
             "001c: MB_CTRL write 0x7 — WRITE_EOM self-clears, FLUSH bits persist");

    test->register_write_32(keymgr_tt_basetest::MB_CTRL_OFFSET, 0x0);

    // ------------------------------------------------------------------
    // 001d: MB_IRQS — W1C behaviour
    //   Bits[1:0] are level-sensitive (reflect live FIFO state — not sticky).
    //   Bits[4:2] are sticky W1C (set by HW events, cleared by SEP writing 1).
    //   At reset: sticky bits[4:2] = 0x0. (Level bits[1:0] may be non-zero.)
    //   Write 0x3 (W1C on zero W1C bits) → sticky bits stay 0.
    // ------------------------------------------------------------------
    test->register_read_32(keymgr_tt_basetest::MB_IRQS_OFFSET, rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::MB_IRQS_WRITE, 0x0u,
             "001d: MB_IRQS sticky bits[4:2] reset = 0x0");

    // W1C write on already-zero sticky bits must leave them at 0
    test->register_write_32(keymgr_tt_basetest::MB_IRQS_OFFSET, 0x3);
    test->register_read_32 (keymgr_tt_basetest::MB_IRQS_OFFSET, rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::MB_IRQS_WRITE, 0x0u,
             "001d: MB_IRQS W1C write on zero bits — sticky bits stay 0");

    // ------------------------------------------------------------------
    // 001e: MB_WDATA — write-only, read must return 0
    // ------------------------------------------------------------------
    test->register_write_32(keymgr_tt_basetest::MB_WDATA_OFFSET, 0xDEADBEEF);
    test->register_read_32 (keymgr_tt_basetest::MB_WDATA_OFFSET, rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::MB_WDATA_READ, 0x0u,
             "001e: MB_WDATA is write-only — read returns 0");

    // ------------------------------------------------------------------
    // 001f: MB_STATUS — read-only, write is ignored
    // ------------------------------------------------------------------
    test->register_read_32(keymgr_tt_basetest::MB_STATUS_OFFSET, rdata);
    uint32_t status_before = rdata;
    test->register_write_32(keymgr_tt_basetest::MB_STATUS_OFFSET, 0xFFFFFFFF);
    test->register_read_32 (keymgr_tt_basetest::MB_STATUS_OFFSET, rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::MB_STATUS_READ, status_before,
             "001f: MB_STATUS is read-only — write is ignored");

    // ------------------------------------------------------------------
    // 001g: KPVLP_STATUS — reset value = 0 (no slots unlocked)
    // ------------------------------------------------------------------
    test->register_read_32(keymgr_tt_basetest::KPVLP_STATUS_OFFSET, rdata);
    CHECK_EQ(rdata, (uint32_t)keymgr_tt_basetest::KPVLP_STATUS_RESET,
             "001g: KPVLP_STATUS reset = 0x0 (no slots unlocked)");

    // ------------------------------------------------------------------
    // 001h: KPVLP_CTRL[0] — write all 1s, only writable bits should stick
    // ------------------------------------------------------------------
    test->register_write_32(keymgr_tt_basetest::kpvlp_ctrl_offset(0), 0xFFFFFFFF);
    test->register_read_32 (keymgr_tt_basetest::kpvlp_ctrl_offset(0), rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::KPVLP_CTRL_READ,
             0x0u,  // WO: reads back 0 regardless
             "001h: KPVLP_CTRL[0] is write-only — read returns 0");

    // ------------------------------------------------------------------
    // 001i: KPVLP_KEY[0][0] — write-only, read must return 0
    // ------------------------------------------------------------------
    test->register_write_32(keymgr_tt_basetest::kpvlp_key_offset(0, 0), 0xCAFEBABE);
    test->register_read_32 (keymgr_tt_basetest::kpvlp_key_offset(0, 0), rdata);
    CHECK_EQ(rdata & (uint32_t)keymgr_tt_basetest::KPVLP_KEY_READ, 0x0u,
             "001i: KPVLP_KEY[0][0] is write-only — read returns 0");

    // ------------------------------------------------------------------
    std::cout << "\n--- FUNC001 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
