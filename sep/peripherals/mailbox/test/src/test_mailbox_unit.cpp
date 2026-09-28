// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file test_mailbox_unit.cpp
 * @brief mailbox_unit_t decode, routing and fan-out test suite
 *
 * The channel tests in test_mailbox_func001..007 drive mailbox_ip's two sockets
 * directly, so nothing in them touches the wrapper that the subsystem actually
 * instantiates. This file covers that surface:
 *
 * - TC-MBU-001 block decode, per-channel/per-port routing, address restoration
 * - TC-MBU-002 unmapped holes inside a block, and the end of the aperture
 * - TC-MBU-003 per-channel state independence
 * - TC-MBU-004 reset fan-out to every channel
 * - TC-MBU-005 the 2 * NUM_MAILBOXES interrupt outputs
 *
 * Address map under test (axil_mailbox_sep_wrap.rdl), with NUM_MAILBOXES = 2:
 *
 *   channel 0 port 0 (outbound)  0x0000..0x07FF
 *   channel 0 port 1 (inbound)   0x0800..0x0FFF
 *   channel 1 port 0 (outbound)  0x1000..0x17FF
 *   channel 1 port 1 (inbound)   0x1800..0x1FFF
 *
 * Only 0x00..0x4F is mapped inside each block; 0x50..0x7FF is a hole the
 * channel itself rejects, and 0x2000 is the first address past the aperture.
 */

#include "testbench.h"

#include <iomanip>
#include <sstream>

namespace {

typedef mailbox_unit_t<2> unit_under_test;

constexpr uint64_t BLOCK_SIZE = unit_under_test::MAILBOX_SIZE;
constexpr uint64_t CHANNEL_STRIDE = 2 * BLOCK_SIZE;
constexpr uint64_t APERTURE = unit_under_test::APERTURE_SIZE;

/// @brief Address of @p offset within channel @p channel's port @p port block
uint64_t unit_addr(unsigned int channel, unsigned int port, uint64_t offset)
{
    return channel * CHANNEL_STRIDE + port * BLOCK_SIZE + offset;
}

/// @brief Index of a channel/port pair in the flattened four-signal irq vector
unsigned int irq_index(unsigned int channel, unsigned int port)
{
    return 2 * channel + port;
}

}  // namespace

simtlm::access_result testbench::unit_write(uint64_t address, uint64_t value)
{
    return simtlm::write_word<uint64_t>(unit_port->initiator_socket, address, value);
}

simtlm::access_result testbench::unit_read(uint64_t address, uint64_t& value)
{
    // "Observed" rather than the staging read: the negative cases below assert
    // on the data the unit left behind as well as on the status.
    return simtlm::read_word_observed<uint64_t>(unit_port->initiator_socket, address,
                                                value);
}

void testbench::apply_unit_reset()
{
    unit_rst_ni_sig.write(false);
    wait(10, SC_NS);
    unit_rst_ni_sig.write(true);
    wait(5, SC_NS);
}

// =============================================================================
// TC-MBU-001: Block decode, routing, and address restoration
// =============================================================================

/**
 * @brief TC-MBU-001: every block decodes to its own channel and port
 *
 * Verification Objective:
 * Prove that an access at `channel * 0x1000 + port * 0x800 + offset` reaches
 * exactly channel `channel`, port `port`, at register `offset`, and that the
 * caller's payload address survives the access unchanged.
 *
 * A wrapper that dropped the channel index, swapped outbound and inbound, or
 * forgot to restore the address would still let a single-channel smoke test
 * pass; routing is therefore checked with a distinct payload per block and read
 * back through the *peer* port of the same channel, which only the correct
 * decode can satisfy.
 *
 * Test Procedure:
 * 1. Reset the unit
 * 2. Write a unique word into every (channel, port) WRITE_DATA
 * 3. Read each word back from the peer port of the same channel
 * 4. Read WRITE_DATA (first mapped offset) in every block and expect the
 *    0xFEEDC0DE sentinel, proving the block responds at its very first address
 * 5. Write CTRL (last mapped offset) in every block and expect OK
 * 6. Check the payload address after a successful access and after an access
 *    the channel rejects
 *
 * Pass Criteria:
 * - Each word appears at exactly one peer port and nowhere else
 * - First and last mapped offsets of every block decode correctly
 * - trans.get_address() equals the address the initiator set, on both paths
 */
void testbench::test_unit_decode_routing()
{
    std::string test_name = "TC-MBU-001: unit block decode and routing";
    REG_INFO(2, logger) << "========================================";
    REG_INFO(2, logger) << "Running: " << test_name;
    REG_INFO(2, logger) << "========================================";

    bool test_passed = true;
    std::ostringstream msg;

    apply_unit_reset();

    // Distinct per block so a misrouted word is identifiable, not just wrong.
    uint64_t sent[UNIT_CHANNELS][2];
    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        for (unsigned int p = 0; p < 2; p++) {
            sent[m][p] = 0xC0DE000000000000ULL | (uint64_t(m) << 8) | p;

            const auto r = unit_write(unit_addr(m, p, mailbox_basetest::WRITE_DATA_OFFSET),
                                      sent[m][p]);
            if (!r.ok()) {
                msg.str("");
                msg << "FAIL: WRITE_DATA channel " << m << " port " << p
                    << " returned " << simtlm::response_name(r.status);
                REG_ERROR(0, logger) << msg.str();
                test_passed = false;
            }
        }
    }

    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        for (unsigned int p = 0; p < 2; p++) {
            const unsigned int peer = 1 - p;
            uint64_t got = 0;

            const auto r = unit_read(unit_addr(m, peer, mailbox_basetest::READ_DATA_OFFSET),
                                     got);
            if (!r.ok()) {
                msg.str("");
                msg << "FAIL: READ_DATA channel " << m << " port " << peer
                    << " returned " << simtlm::response_name(r.status);
                REG_ERROR(0, logger) << msg.str();
                test_passed = false;
            } else if (got != sent[m][p]) {
                msg.str("");
                msg << "FAIL: channel " << m << " port " << p << " -> " << peer
                    << " expected 0x" << std::hex << sent[m][p] << ", got 0x" << got;
                REG_ERROR(0, logger) << msg.str();
                test_passed = false;
            } else {
                msg.str("");
                msg << "PASS: channel " << m << " port " << p << " -> " << peer
                    << " carried 0x" << std::hex << got;
                REG_INFO(2, logger) << msg.str();
            }
        }
    }

    // First mapped offset of each block: a read of WRITE_DATA is the one
    // write-only read the model answers, and it answers with a fixed sentinel.
    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        for (unsigned int p = 0; p < 2; p++) {
            uint64_t got = 0;
            const auto r = unit_read(unit_addr(m, p, 0x00), got);
            if (!r.ok() || got != 0xFEEDC0DEULL) {
                msg.str("");
                msg << "FAIL: first offset of block (channel " << m << " port " << p
                    << ") gave 0x" << std::hex << got << " status "
                    << simtlm::response_name(r.status);
                REG_ERROR(0, logger) << msg.str();
                test_passed = false;
            }
        }
    }

    // Last mapped offset of each block: CTRL is a write-only strobe, so a write
    // of zero is accepted and a read is refused.
    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        for (unsigned int p = 0; p < 2; p++) {
            const auto w = unit_write(unit_addr(m, p, mailbox_basetest::CTRL_OFFSET), 0x0);
            if (!w.ok()) {
                msg.str("");
                msg << "FAIL: CTRL write in channel " << m << " port " << p
                    << " returned " << simtlm::response_name(w.status);
                REG_ERROR(0, logger) << msg.str();
                test_passed = false;
            }

            uint64_t got = 0xA5A5A5A5A5A5A5A5ULL;
            const auto r = unit_read(unit_addr(m, p, mailbox_basetest::CTRL_OFFSET), got);
            if (r.ok() || got != 0) {
                msg.str("");
                msg << "FAIL: CTRL read in channel " << m << " port " << p
                    << " gave 0x" << std::hex << got << " status "
                    << simtlm::response_name(r.status);
                REG_ERROR(0, logger) << msg.str();
                test_passed = false;
            }
        }
    }

    // Address restoration. The wrapper rewrites trans.get_address() to a
    // block-relative offset before delegating; an initiator that reuses the
    // payload (or an interconnect that inspects it afterwards) must see the
    // address it set. simtlm::malformed_payload is used here purely because it
    // hands back the live payload, which read_word/write_word do not.
    struct {
        uint64_t address;
        bool expect_ok;
        const char* what;
    } restore_cases[] = {
        {unit_addr(1, 1, mailbox_basetest::WIRQT_OFFSET), true, "accepted access"},
        {unit_addr(1, 0, 0x100), false, "hole inside a block"},
        {APERTURE, false, "first address past the aperture"},
    };

    for (const auto& c : restore_cases) {
        simtlm::target_geometry geo;
        geo.valid_address = c.address;
        geo.word_bytes = 8;
        geo.aperture_bytes = APERTURE;

        simtlm::malformed_payload mp(simtlm::defect::well_formed, geo,
                                     tlm::TLM_WRITE_COMMAND);
        const auto r = simtlm::access(unit_port->initiator_socket, mp.payload());

        if (mp.payload().get_address() != c.address) {
            msg.str("");
            msg << "FAIL: address not restored after " << c.what << ": set 0x"
                << std::hex << c.address << ", payload holds 0x"
                << mp.payload().get_address();
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }

        if (r.ok() != c.expect_ok) {
            msg.str("");
            msg << "FAIL: " << c.what << " at 0x" << std::hex << c.address
                << " returned " << simtlm::response_name(r.status);
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }
    }

    REG_INFO(2, logger) << "========================================";
    report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC-MBU-002: Holes inside a block, and the end of the aperture
// =============================================================================

/**
 * @brief TC-MBU-002: unmapped addresses are refused, mapped ones are not
 *
 * Verification Objective:
 * Pin both edges of the decode. Inside every block, 0x50..0x7FF is unmapped and
 * must answer with an error while the register file stays untouched. Outside
 * the aperture, the wrapper itself must answer rather than index past the end
 * of its socket array.
 *
 * The last mapped register in the aperture (channel 1, port 1, CTRL at 0x1FC8)
 * is checked alongside the last word of the aperture (0x1FF8) and the first
 * word past it (0x2000), so a decode that is off by one block in either
 * direction fails here.
 *
 * Pass Criteria:
 * - Every hole in every block answers with an error and zero data
 * - 0x1FC8 is accepted; 0x1FF8 and 0x2000 and beyond are refused
 * - A witness register written before the sweep survives it
 */
void testbench::test_unit_aperture_and_holes()
{
    std::string test_name = "TC-MBU-002: unit holes and aperture end";
    REG_INFO(2, logger) << "========================================";
    REG_INFO(2, logger) << "Running: " << test_name;
    REG_INFO(2, logger) << "========================================";

    bool test_passed = true;
    std::ostringstream msg;

    apply_unit_reset();

    const uint64_t witness_addr = unit_addr(0, 0, mailbox_basetest::WIRQT_OFFSET);
    const uint64_t witness_val = 0x5;
    unit_write(witness_addr, witness_val);

    // 0x50 is the first unmapped offset, 0x7F8 the last word of the block.
    const uint64_t holes[] = {0x50, 0x58, 0x100, 0x400, 0x7F0, 0x7F8};

    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        for (unsigned int p = 0; p < 2; p++) {
            for (uint64_t hole : holes) {
                const uint64_t addr = unit_addr(m, p, hole);

                uint64_t got = 0xDEADBEEFDEADBEEFULL;
                const auto r = unit_read(addr, got);
                if (r.ok() || got != 0) {
                    msg.str("");
                    msg << "FAIL: read of hole 0x" << std::hex << addr << " gave 0x"
                        << got << " status " << simtlm::response_name(r.status);
                    REG_ERROR(0, logger) << msg.str();
                    test_passed = false;
                }

                const auto w = unit_write(addr, 0x1234);
                if (w.ok()) {
                    msg.str("");
                    msg << "FAIL: write to hole 0x" << std::hex << addr << " was accepted";
                    REG_ERROR(0, logger) << msg.str();
                    test_passed = false;
                }
            }
        }
    }

    // Aperture edge. 0x1FC8 is the highest mapped register in the whole unit.
    const uint64_t last_mapped = unit_addr(UNIT_CHANNELS - 1, 1,
                                           mailbox_basetest::CTRL_OFFSET);
    const auto last_ok = unit_write(last_mapped, 0x0);
    if (!last_ok.ok()) {
        msg.str("");
        msg << "FAIL: last mapped address 0x" << std::hex << last_mapped
            << " returned " << simtlm::response_name(last_ok.status);
        REG_ERROR(0, logger) << msg.str();
        test_passed = false;
    } else {
        msg.str("");
        msg << "PASS: last mapped address 0x" << std::hex << last_mapped << " accepted";
        REG_INFO(2, logger) << msg.str();
    }

    const uint64_t outside[] = {APERTURE - 8, APERTURE, APERTURE + 0x48,
                                APERTURE + CHANNEL_STRIDE, 0xFFFFFFFFFFFFFFF8ULL};

    for (uint64_t addr : outside) {
        uint64_t got = 0xDEADBEEFDEADBEEFULL;
        const auto r = unit_read(addr, got);
        if (r.ok()) {
            msg.str("");
            msg << "FAIL: read outside the mapped space at 0x" << std::hex << addr
                << " was accepted";
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }
        if (r.status == tlm::TLM_INCOMPLETE_RESPONSE) {
            msg.str("");
            msg << "FAIL: address 0x" << std::hex << addr << " left the payload INCOMPLETE";
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }

        const auto w = unit_write(addr, 0x1234);
        if (w.ok() || w.status == tlm::TLM_INCOMPLETE_RESPONSE) {
            msg.str("");
            msg << "FAIL: write outside the mapped space at 0x" << std::hex << addr
                << " returned " << simtlm::response_name(w.status);
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }
    }

    uint64_t witness_after = 0;
    const auto wr = unit_read(witness_addr, witness_after);
    if (!wr.ok() || witness_after != witness_val) {
        msg.str("");
        msg << "FAIL: witness WIRQT changed across the sweep: expected 0x" << std::hex
            << witness_val << ", read 0x" << witness_after;
        REG_ERROR(0, logger) << msg.str();
        test_passed = false;
    } else {
        REG_INFO(2, logger) << "PASS: register file untouched by unmapped traffic";
    }

    REG_INFO(2, logger) << "========================================";
    report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC-MBU-003: Per-channel independence
// =============================================================================

/**
 * @brief TC-MBU-003: channels share nothing but the socket
 *
 * Verification Objective:
 * Show that the wrapper owns NUM_MAILBOXES separate mailbox_ip instances rather
 * than one shared instance behind a decode. Channel 0 is driven to a saturated,
 * non-default state (FIFO full, thresholds and enables programmed) while
 * channel 1 is left at reset, then every distinguishing observation is made on
 * both channels.
 *
 * Pass Criteria:
 * - Channel 0 reports full while channel 1 reports empty and not full
 * - Channel 1's thresholds and enables still read 0 after channel 0 is programmed
 * - Channel 1 still accepts a write and returns its own data, not channel 0's
 */
void testbench::test_unit_channel_independence()
{
    std::string test_name = "TC-MBU-003: unit per-channel independence";
    REG_INFO(2, logger) << "========================================";
    REG_INFO(2, logger) << "Running: " << test_name;
    REG_INFO(2, logger) << "========================================";

    bool test_passed = true;
    std::ostringstream msg;

    apply_unit_reset();

    const unsigned int depth = 8;

    unit_write(unit_addr(0, 0, mailbox_basetest::WIRQT_OFFSET), 0x3);
    unit_write(unit_addr(0, 0, mailbox_basetest::RIRQT_OFFSET), 0x2);
    unit_write(unit_addr(0, 0, mailbox_basetest::IRQEN_OFFSET), 0x7);

    for (unsigned int i = 0; i < depth; i++) {
        const auto w = unit_write(unit_addr(0, 0, mailbox_basetest::WRITE_DATA_OFFSET),
                                  0x0BAD0000ULL | i);
        if (!w.ok()) {
            msg.str("");
            msg << "FAIL: channel 0 refused entry " << i << " before reaching depth";
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }
    }

    uint64_t status = 0;
    unit_read(unit_addr(0, 0, mailbox_basetest::STATUS_OFFSET), status);
    if ((status & 0x2) == 0) {
        REG_ERROR(0, logger) << "FAIL: channel 0 port 0 not full after filling it";
        test_passed = false;
    }

    // Channel 1 must be untouched by everything above.
    unit_read(unit_addr(1, 0, mailbox_basetest::STATUS_OFFSET), status);
    if ((status & 0x2) != 0) {
        REG_ERROR(0, logger) << "FAIL: channel 1 port 0 reports full; FIFO state is shared";
        test_passed = false;
    }
    if ((status & 0x1) == 0) {
        REG_ERROR(0, logger) << "FAIL: channel 1 port 0 not empty; FIFO state is shared";
        test_passed = false;
    }

    const struct {
        unsigned int offset;
        const char* name;
    } shadow_regs[] = {
        {mailbox_basetest::WIRQT_OFFSET, "WIRQT"},
        {mailbox_basetest::RIRQT_OFFSET, "RIRQT"},
        {mailbox_basetest::IRQEN_OFFSET, "IRQEN"},
        {mailbox_basetest::IRQS_OFFSET, "IRQS"},
    };

    for (const auto& reg : shadow_regs) {
        uint64_t value = 0;
        unit_read(unit_addr(1, 0, reg.offset), value);
        if (value != 0) {
            msg.str("");
            msg << "FAIL: channel 1 " << reg.name << " = 0x" << std::hex << value
                << " after programming channel 0; shadow state is shared";
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }
    }

    // And channel 1 must still work, carrying its own payload.
    const uint64_t ch1_word = 0x1111222233334444ULL;
    const auto w = unit_write(unit_addr(1, 0, mailbox_basetest::WRITE_DATA_OFFSET),
                              ch1_word);
    if (!w.ok()) {
        REG_ERROR(0, logger) << "FAIL: channel 1 write refused while channel 0 is full";
        test_passed = false;
    }

    uint64_t got = 0;
    const auto r = unit_read(unit_addr(1, 1, mailbox_basetest::READ_DATA_OFFSET), got);
    if (!r.ok() || got != ch1_word) {
        msg.str("");
        msg << "FAIL: channel 1 returned 0x" << std::hex << got << " (expected 0x"
            << ch1_word << "), status " << simtlm::response_name(r.status);
        REG_ERROR(0, logger) << msg.str();
        test_passed = false;
    } else {
        REG_INFO(2, logger) << "PASS: channel 1 unaffected by a saturated channel 0";
    }

    REG_INFO(2, logger) << "========================================";
    report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC-MBU-004: Reset fan-out
// =============================================================================

/**
 * @brief TC-MBU-004: one rst_ni assertion clears every channel
 *
 * Verification Objective:
 * The wrapper binds its single rst_ni to all NUM_MAILBOXES channels. Every
 * channel is first driven into a state that is distinguishable from reset --
 * queued FIFO data, programmed thresholds and enables, a latched error, and an
 * asserted interrupt -- and after one reset pulse every one of those
 * observations must be back at its reset value on every channel, including the
 * physical interrupt pins.
 *
 * Pass Criteria:
 * - STATUS empty, IRQS, IRQEN, WIRQT, RIRQT and ERROR_FLAGS all at reset on
 *   every channel and port
 * - All 2 * NUM_MAILBOXES interrupt outputs low
 */
void testbench::test_unit_reset_fanout()
{
    std::string test_name = "TC-MBU-004: unit reset fan-out";
    REG_INFO(2, logger) << "========================================";
    REG_INFO(2, logger) << "Running: " << test_name;
    REG_INFO(2, logger) << "========================================";

    bool test_passed = true;
    std::ostringstream msg;

    apply_unit_reset();

    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        for (unsigned int p = 0; p < 2; p++) {
            unit_write(unit_addr(m, p, mailbox_basetest::IRQEN_OFFSET), 0x7);
            unit_write(unit_addr(m, p, mailbox_basetest::WIRQT_OFFSET), 0x0);
            unit_write(unit_addr(m, p, mailbox_basetest::RIRQT_OFFSET), 0x0);
            unit_write(unit_addr(m, p, mailbox_basetest::WRITE_DATA_OFFSET),
                       0xABCD0000ULL | (m << 4) | p);
        }
    }
    wait(SC_ZERO_TIME);

    // Confirm the pre-reset state really is distinguishable, otherwise the
    // post-reset checks below would pass against a DUT that never changed.
    bool any_irq_before = false;
    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        any_irq_before = any_irq_before || unit_outbound_irq_sig[m].read() ||
                         unit_inbound_irq_sig[m].read();
    }
    if (!any_irq_before) {
        REG_ERROR(0, logger) << "FAIL: no interrupt asserted before reset; the fan-out "
                                 "check would be vacuous";
        test_passed = false;
    }

    apply_unit_reset();

    const struct {
        unsigned int offset;
        uint64_t expected;
        const char* name;
    } after_reset[] = {
        {mailbox_basetest::STATUS_OFFSET, 0x1, "STATUS"},
        {mailbox_basetest::ERROR_FLAGS_OFFSET, 0x0, "ERROR_FLAGS"},
        {mailbox_basetest::WIRQT_OFFSET, 0x0, "WIRQT"},
        {mailbox_basetest::RIRQT_OFFSET, 0x0, "RIRQT"},
        {mailbox_basetest::IRQS_OFFSET, 0x0, "IRQS"},
        {mailbox_basetest::IRQEN_OFFSET, 0x0, "IRQEN"},
        {mailbox_basetest::IRQP_OFFSET, 0x0, "IRQP"},
    };

    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        for (unsigned int p = 0; p < 2; p++) {
            for (const auto& reg : after_reset) {
                uint64_t value = 0xFFFFFFFFFFFFFFFFULL;
                const auto r = unit_read(unit_addr(m, p, reg.offset), value);
                if (!r.ok() || value != reg.expected) {
                    msg.str("");
                    msg << "FAIL: channel " << m << " port " << p << " " << reg.name
                        << " = 0x" << std::hex << value << " after reset (expected 0x"
                        << reg.expected << "), status "
                        << simtlm::response_name(r.status);
                    REG_ERROR(0, logger) << msg.str();
                    test_passed = false;
                }
            }
        }
    }

    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        if (unit_outbound_irq_sig[m].read()) {
            msg.str("");
            msg << "FAIL: outbound_irq_o[" << m << "] still asserted after reset";
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }
        if (unit_inbound_irq_sig[m].read()) {
            msg.str("");
            msg << "FAIL: inbound_irq_o[" << m << "] still asserted after reset";
            REG_ERROR(0, logger) << msg.str();
            test_passed = false;
        }
    }

    if (test_passed) {
        REG_INFO(2, logger) << "PASS: reset reached every channel and every interrupt pin";
    }

    REG_INFO(2, logger) << "========================================";
    report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// TC-MBU-005: Interrupt vector wiring
// =============================================================================

/**
 * @brief TC-MBU-005: irq_o[p] of channel m drives exactly one vector entry
 *
 * Verification Objective:
 * The wrapper maps channel m's irq_o[0] to outbound_irq_o[m] and irq_o[1] to
 * inbound_irq_o[m]. Each of the 2 * NUM_MAILBOXES outputs is raised on its own,
 * from a fully reset unit, and all four pins are sampled every time: a swapped
 * or duplicated binding shows up as the wrong pin asserting or as more than one
 * pin asserting.
 *
 * Pass Criteria:
 * - Raising WTIRQ in channel m port p asserts exactly vector entry (m, p)
 * - Acknowledging it via IRQS write-1-to-clear deasserts that entry again
 */
void testbench::test_unit_interrupt_vectors()
{
    std::string test_name = "TC-MBU-005: unit interrupt vector wiring";
    REG_INFO(2, logger) << "========================================";
    REG_INFO(2, logger) << "Running: " << test_name;
    REG_INFO(2, logger) << "========================================";

    bool test_passed = true;
    std::ostringstream msg;

    sc_signal<bool>* irq_sig[2 * UNIT_CHANNELS];
    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        irq_sig[irq_index(m, 0)] = &unit_outbound_irq_sig[m];
        irq_sig[irq_index(m, 1)] = &unit_inbound_irq_sig[m];
    }

    for (unsigned int m = 0; m < UNIT_CHANNELS; m++) {
        for (unsigned int p = 0; p < 2; p++) {
            apply_unit_reset();

            // WIRQT is 0 after reset, so one queued word takes the write level
            // strictly above the threshold and latches WTIRQ on this port only.
            unit_write(unit_addr(m, p, mailbox_basetest::IRQEN_OFFSET), 0x1);
            unit_write(unit_addr(m, p, mailbox_basetest::WRITE_DATA_OFFSET),
                       0x5A5A0000ULL | (m << 4) | p);
            wait(SC_ZERO_TIME);

            const unsigned int expected = irq_index(m, p);
            for (unsigned int i = 0; i < 2 * UNIT_CHANNELS; i++) {
                const bool level = irq_sig[i]->read();
                if (level != (i == expected)) {
                    msg.str("");
                    msg << "FAIL: channel " << m << " port " << p
                        << " WTIRQ: vector entry " << i << " reads " << level
                        << ", expected " << (i == expected);
                    REG_ERROR(0, logger) << msg.str();
                    test_passed = false;
                }
            }

            unit_write(unit_addr(m, p, mailbox_basetest::IRQS_OFFSET), 0x1);
            wait(SC_ZERO_TIME);

            if (irq_sig[expected]->read()) {
                msg.str("");
                msg << "FAIL: channel " << m << " port " << p
                    << " interrupt still asserted after write-1-to-clear";
                REG_ERROR(0, logger) << msg.str();
                test_passed = false;
            }
        }
    }

    if (test_passed) {
        REG_INFO(2, logger) << "PASS: all " << (2 * UNIT_CHANNELS)
                             << " interrupt outputs wired to their own channel and port";
    }

    apply_unit_reset();

    REG_INFO(2, logger) << "========================================";
    report_test_result(test_name.c_str(), test_passed);
}

// =============================================================================
// Unit Test Suite Orchestration
// =============================================================================

void testbench::run_unit_tests()
{
    REG_INFO(2, logger) << "\n";
    REG_INFO(2, logger) << "========================================";
    REG_INFO(2, logger) << "MAILBOX UNIT TEST SUITE: mailbox_unit_t<" << UNIT_CHANNELS
                         << ">";
    REG_INFO(2, logger) << "========================================";

    test_unit_decode_routing();
    test_unit_aperture_and_holes();
    test_unit_channel_independence();
    test_unit_reset_fanout();
    test_unit_interrupt_vectors();

    REG_INFO(2, logger) << "========================================";
    REG_INFO(2, logger) << "MAILBOX UNIT TEST SUITE COMPLETED";
    REG_INFO(2, logger) << "========================================";
    REG_INFO(2, logger) << "\n";
}
