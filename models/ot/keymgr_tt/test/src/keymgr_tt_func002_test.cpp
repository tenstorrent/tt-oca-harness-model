/**
 * @file keymgr_tt_func002_test.cpp
 * @brief Mailbox framing and IRQ behaviour
 *
 * Tests covered:
 *   002a - Send CMD_HW_VER (no payload), receive valid RESP_CMD with RET_SUCCESS
 *   002b - Verify ret_arg encodes hardware version 1.0.0
 *   002c - Enable IRQEN[0]; send command; verify irq_i asserted before reading response
 *   002d - Verify irq_i de-asserted after all outbound data is consumed
 *   002e - Read from empty outbound FIFO sets OUTBOUND_UNDERFLOW sticky IRQ bit
 *   002f - W1C clears OUTBOUND_UNDERFLOW sticky bit
 *   002g - MB_WSEP self-clears after tagging one word (not sticky for next word)
 */

#include "testbench.h"
#include "km_firmware_handler.h"
#include <iostream>

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { std::cout << "[FAIL] " << msg << std::endl; failures++; } \
        else          { std::cout << "[PASS] " << msg << std::endl; } \
    } while(0)

#define CHECK_EQ(got, expected, msg) \
    do { \
        if ((got) != (expected)) { \
            std::cout << "[FAIL] " << msg \
                      << "  got=0x" << std::hex << (uint32_t)(got) \
                      << "  expected=0x" << (uint32_t)(expected) << std::dec << std::endl; \
            failures++; \
        } else { std::cout << "[PASS] " << msg << std::endl; } \
    } while(0)

int keymgr_tt_func002_test(keymgr_tt_test* test, keymgr_tt_model* /*dut*/, testbench* /*tb*/)
{
    int failures = 0;
    std::cout << "\n--- FUNC002: Mailbox Framing & IRQ ---\n";

    // Fresh state for this test group
    test->trigger_reset();

    uint8_t  resp_id = 0, src_seq = 0, echoed_cmd = 0;
    int32_t  ret_code = 0;
    uint32_t ret_arg  = 0;
    std::vector<uint32_t> frame;

    // ------------------------------------------------------------------
    // 002a: Send CMD_HW_VER (no payload) — verify RESP_CMD / RET_SUCCESS
    // ------------------------------------------------------------------
    test->mb_send_command(0, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    bool got = test->mb_receive_frame(frame);
    CHECK(got, "002a: response received within timeout");
    bool parsed = keymgr_tt_test::parse_resp_cmd(frame, resp_id, src_seq, echoed_cmd,
                                                  ret_code, ret_arg);
    CHECK(parsed,  "002a: frame parses as RESP_CMD");
    CHECK_EQ(ret_code, 0, "002a: ret_code = RET_SUCCESS");
    CHECK_EQ(src_seq,  0, "002a: echoed seq = 0");
    CHECK_EQ(echoed_cmd, (uint8_t)keymgr_tt::km_firmware_handler::CMD_HW_VER,
             "002a: echoed cmd_id = CMD_HW_VER");

    // ------------------------------------------------------------------
    // 002b: Verify ret_arg encodes HW version 1.0.0 = 0x00010000
    //   [23:16]=HW_MAJOR(1) | [15:8]=HW_MINOR(0) | [7:0]=HW_PATCH(0)
    // ------------------------------------------------------------------
    constexpr uint32_t HW_VER_EXPECTED = (1u << 16) | (0u << 8) | (0u);
    CHECK_EQ(ret_arg, HW_VER_EXPECTED, "002b: ret_arg = HW version 1.0.0 (0x00010000)");

    // ------------------------------------------------------------------
    // 002c: Enable IRQEN[0] (OUTBOUND_DATA_AVAIL); send CMD; verify irq_i
    //       goes high BEFORE we read the response.
    // ------------------------------------------------------------------
    // Enable bit[0] of MB_IRQEN
    test->register_write_32(keymgr_tt_basetest::MB_IRQEN_OFFSET, 0x01u);

    // Send CMD_HW_VER with seq=1 (seq=0 was consumed in 002a)
    test->mb_send_command(1, keymgr_tt::km_firmware_handler::CMD_HW_VER, {});
    wait(1, SC_NS);  // let fw_thread process and irq_update_process fire

    bool irq_high = test->irq_i.read();
    CHECK(irq_high, "002c: irq_i asserted after response queued (IRQEN[0]=1)");

    // ------------------------------------------------------------------
    // 002d: Read the response; IRQ must de-assert once outbound FIFO is empty
    // ------------------------------------------------------------------
    frame.clear();
    got = test->mb_receive_frame(frame);
    CHECK(got, "002d: response received");
    wait(1, SC_NS);  // let irq_update_process re-evaluate

    bool irq_low = !test->irq_i.read();
    CHECK(irq_low, "002d: irq_i de-asserted after outbound FIFO emptied");

    // Disable IRQEN to avoid interfering with later tests
    test->register_write_32(keymgr_tt_basetest::MB_IRQEN_OFFSET, 0x00u);

    // ------------------------------------------------------------------
    // 002e: Read from empty outbound FIFO — OUTBOUND_UNDERFLOW sticky set
    //   Consuming from an empty outbound FIFO sets MB_IRQS bit[3].
    // ------------------------------------------------------------------
    uint32_t rdata = 0;
    test->register_read_32(keymgr_tt_basetest::MB_RDATA_OFFSET, rdata);  // underflow trigger

    uint32_t irqs = 0;
    test->register_read_32(keymgr_tt_basetest::MB_IRQS_OFFSET, irqs);
    // Bit[3] = OUTBOUND_UNDERFLOW sticky
    CHECK((irqs >> 3) & 0x1u, "002e: OUTBOUND_UNDERFLOW sticky bit set after empty read");

    // ------------------------------------------------------------------
    // 002f: W1C clears OUTBOUND_UNDERFLOW (write 1 to bit[3])
    // ------------------------------------------------------------------
    test->register_write_32(keymgr_tt_basetest::MB_IRQS_OFFSET, 0x08u);  // W1C bit[3]
    test->register_read_32 (keymgr_tt_basetest::MB_IRQS_OFFSET, irqs);
    CHECK(!((irqs >> 3) & 0x1u), "002f: OUTBOUND_UNDERFLOW cleared by W1C");

    // ------------------------------------------------------------------
    // 002g: MB_WSEP self-clears after tagging one word
    //   Write MB_WSEP=1, write one MB_WDATA word → WSEP consumed.
    //   Read MB_WSEP back — it must be 0 (cleared by hardware after use).
    //   The WDATA write above inserted a separator-tagged garbage word into
    //   the inbound FIFO.  We need to flush it before the next FUNC test.
    // ------------------------------------------------------------------
    test->register_write_32(keymgr_tt_basetest::MB_WSEP_OFFSET, 1u);
    uint32_t wsep_before = 0;
    test->register_read_32(keymgr_tt_basetest::MB_WSEP_OFFSET, wsep_before);
    // Write a dummy header word — this consumes the pending WSEP
    // Use seq=2 but deliberately skip CRC → doesn't matter, we just check WSEP clearing
    test->register_write_32(keymgr_tt_basetest::MB_WDATA_OFFSET, 0xDEADBEEFu);  // sep fires here

    uint32_t wsep_after = 0;
    test->register_read_32(keymgr_tt_basetest::MB_WSEP_OFFSET, wsep_after);
    CHECK_EQ(wsep_after & 0x1u, 0x0u, "002g: MB_WSEP self-clears after tagging one word");

    // Flush any leftover inbound garbage so subsequent tests start clean
    test->register_write_32(keymgr_tt_basetest::MB_CTRL_OFFSET, 0x01u);  // FLUSH_INBOUND
    test->register_write_32(keymgr_tt_basetest::MB_STATUS_OFFSET, 0x00F00000u);  // clear overflow/underflow

    std::cout << "\n--- FUNC002 complete: " << failures << " failure(s) ---\n\n";
    return failures;
}
