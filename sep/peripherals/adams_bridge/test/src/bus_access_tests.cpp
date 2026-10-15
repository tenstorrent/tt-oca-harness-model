// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file bus_access_tests.cpp
 * @brief The ABR aperture answers bus accesses the way the SEP fabric does.
 *
 * Every word of a write is taken by its own strobe: a full strobe writes it, an
 * empty strobe skips it with OKAY, and a partial strobe skips it and answers SLVERR
 * (the AHB slave would otherwise zero-extend the write over the whole register). An
 * access wider than one 64-bit beat, or at or past the decoded extent 0xC018, is
 * answered DECERR by the crypto demux, which returns 0xBADCAB1E as read data. A
 * narrow read returns only the bytes it names. See hw/sys/sep/doc/adams_bridge.adoc
 * in tt-oca-harness.
 */

#include "abr_testbench.h"

#include <algorithm>
#include <iterator>
#include <string>

namespace {

constexpr uint32_t SCRATCH = abr::OFF_NOTIF_INTR_COUNT; // 32-bit RW, never masked

constexpr tlm::tlm_response_status OKAY   = tlm::TLM_OK_RESPONSE;
constexpr tlm::tlm_response_status SLVERR = tlm::TLM_GENERIC_ERROR_RESPONSE;
constexpr tlm::tlm_response_status DECERR = tlm::TLM_ADDRESS_ERROR_RESPONSE;

tlm::tlm_response_status write_be(abr_testbench &tb, uint32_t addr, uint32_t value,
                                  const unsigned char (&be)[4])
{
    unsigned char lanes[4];
    std::copy(std::begin(be), std::end(be), lanes);
    return tb.xact(tlm::TLM_WRITE_COMMAND, addr, reinterpret_cast<unsigned char *>(&value), 4u,
                   lanes, 4u);
}

} // namespace

void bus_access_tests(abr_testbench &tb)
{
    tb.section("Bus: per-word write strobes");

    tb.wr(SCRATCH, 0x11111111u);
    tb.check(write_be(tb, SCRATCH, 0x22222222u, {0xFF, 0xFF, 0xFF, 0xFF}) == OKAY,
             "a full-strobe write answers OKAY");
    tb.check_eq(tb.rd(SCRATCH), 0x22222222u, "a full-strobe write lands");

    tb.check(write_be(tb, SCRATCH, 0x33333333u, {0, 0, 0, 0}) == OKAY,
             "an empty-strobe write answers OKAY");
    tb.check_eq(tb.rd(SCRATCH), 0x22222222u, "an empty-strobe write writes nothing");

    tb.check(write_be(tb, SCRATCH, 0x44444444u, {0xFF, 0, 0, 0}) == SLVERR,
             "a partial-strobe write answers SLVERR");
    tb.check_eq(tb.rd(SCRATCH), 0x22222222u, "a partial-strobe write writes nothing");

    uint8_t byte = 0x55u;
    tb.check(tb.xact(tlm::TLM_WRITE_COMMAND, SCRATCH + 1u, &byte, 1u) == SLVERR,
             "a byte store answers SLVERR");
    uint16_t half = 0x6666u;
    tb.check(tb.xact(tlm::TLM_WRITE_COMMAND, SCRATCH + 2u,
                     reinterpret_cast<unsigned char *>(&half), 2u) == SLVERR,
             "a half-word store answers SLVERR");
    tb.check_eq(tb.rd(SCRATCH), 0x22222222u, "sub-word stores write nothing");

    tb.section("Bus: 64-bit beats");

    uint32_t two[2] = {0xAAAA0001u, 0xBBBB0002u};
    tb.check(tb.xact(tlm::TLM_WRITE_COMMAND, abr::OFF_ERROR_INTR_COUNT,
                     reinterpret_cast<unsigned char *>(two), 8u) == OKAY,
             "an aligned 64-bit write answers OKAY");
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTR_COUNT), 0xAAAA0001u, "the 64-bit write's low word lands");

    uint32_t beat[2] = {0x77777777u, 0x88888888u};
    unsigned char lo_only[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0};
    tb.check(tb.xact(tlm::TLM_WRITE_COMMAND, SCRATCH, reinterpret_cast<unsigned char *>(beat), 8u,
                     lo_only, 8u) == OKAY,
             "a 64-bit write strobing one word answers OKAY");
    tb.check_eq(tb.rd(SCRATCH), 0x77777777u, "the strobed word lands");

    unsigned char lo_partial[8] = {0xFF, 0, 0, 0, 0, 0, 0, 0};
    tb.check(tb.xact(tlm::TLM_WRITE_COMMAND, SCRATCH, reinterpret_cast<unsigned char *>(beat), 8u,
                     lo_partial, 8u) == SLVERR,
             "a 64-bit write with one partial word answers SLVERR");
    tb.check_eq(tb.rd(SCRATCH), 0x77777777u, "the partial word is not written");

    // 0x81FC ends a beat; the word after it, ERROR_INTR_COUNT, holds 0xAAAA0001.
    uint32_t straddle[2] = {0xEEEEEEEEu, 0xEEEEEEEEu};
    tb.check(tb.xact(tlm::TLM_READ_COMMAND, abr::OFF_ERROR_INTR_COUNT - 4u,
                     reinterpret_cast<unsigned char *>(straddle), 8u) == OKAY,
             "an 8-byte read across a beat boundary answers OKAY");
    tb.check_eq(straddle[1], 0u, "the bytes past the beat read 0, not the next register");

    tb.section("Bus: bursts and the decoded extent");

    uint32_t burst[4] = {1u, 2u, 3u, 4u};
    tb.check(tb.xact(tlm::TLM_WRITE_COMMAND, SCRATCH, reinterpret_cast<unsigned char *>(burst), 16u) ==
                 DECERR,
             "an access wider than one beat answers DECERR");
    tb.check_eq(tb.rd(SCRATCH), 0x77777777u, "a burst writes nothing");

    for (uint32_t off : {0xC018u, 0xC01Cu, 0xFFFCu, 0x10000u}) {
        uint32_t data = 0u;
        tb.check(tb.xact(tlm::TLM_READ_COMMAND, off, reinterpret_cast<unsigned char *>(&data), 4u) ==
                     DECERR,
                 "a read at offset " + std::to_string(off) + " answers DECERR");
        tb.check_eq(data, 0xBADCAB1Eu, "a DECERR read returns 0xBADCAB1E");
        uint32_t wdata = 0x12345678u;
        tb.check(tb.xact(tlm::TLM_WRITE_COMMAND, off, reinterpret_cast<unsigned char *>(&wdata), 4u) ==
                     DECERR,
                 "a write at offset " + std::to_string(off) + " answers DECERR");
    }
    uint32_t last = 0u;
    tb.check(tb.xact(tlm::TLM_READ_COMMAND, 0xC014u, reinterpret_cast<unsigned char *>(&last), 4u) ==
                 OKAY,
             "the last decoded word answers OKAY");

    tb.section("Bus: narrow reads");

    tb.wr(SCRATCH, 0xA1B2C3D4u);
    uint8_t b1 = 0u;
    tb.check(tb.xact(tlm::TLM_READ_COMMAND, SCRATCH + 1u, &b1, 1u) == OKAY, "a byte read answers OKAY");
    tb.check_eq(b1, 0xC3u, "a byte read returns only the byte it names");
    uint16_t h1 = 0u;
    tb.check(tb.xact(tlm::TLM_READ_COMMAND, SCRATCH + 2u, reinterpret_cast<unsigned char *>(&h1), 2u) ==
                 OKAY,
             "a half-word read answers OKAY");
    tb.check_eq(h1, 0xA1B2u, "a half-word read returns only the half it names");
    tb.wr(SCRATCH, 0u);
    tb.wr(abr::OFF_ERROR_INTR_COUNT, 0u);
}
