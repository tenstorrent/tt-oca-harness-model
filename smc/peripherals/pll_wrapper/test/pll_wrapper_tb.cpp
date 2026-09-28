// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// pll_wrapper_tb.cpp -- self-checking test bench for the composed SMC PLL
// wrapper (pll_cntl @0x000, cgm_0 @0x100, cgm_1 @0x200, awm_0 @0x400,
// awm_1 @0xA00).
//
// The bench is split into two kinds of case, and the split is deliberate:
//
//   ARCHITECTURAL cases drive the model only through its TLM sockets and check
//   it against the independent register manifest below.  These are the cases
//   that can be claimed as evidence the model matches the hardware contract.
//
//   STRUCTURAL cases exercise the model's own back doors (peek/poke, callback
//   replacement, dump_state).  They keep the debug infrastructure working and
//   they raise coverage, but they prove nothing about PLL architecture and
//   must not be cited as functional evidence.
//
// The manifest (kExp*) is a second, independent transcription of the register
// map: offset, reset, read mask, write mask and self-clearing mask for every
// register.  It is deliberately NOT derived from the model's own tables in
// src/*.cpp, so a transcription error on either side shows up as a failure.
// When the RDL sources become available in-tree this table should be generated
// from them instead of hand-maintained.
//
// Prints "ALL TESTS PASSED" on success; non-zero exit on failure.

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "pll_wrapper.h"
#include "smc_axi_extension.h"

using sc_core::sc_module_name;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_ZERO_TIME;

namespace {

unsigned g_failures = 0;

void fail_at(const char* file, int line, const std::string& what)
{
    std::cerr << "FAIL " << file << ":" << line << "  " << what << "\n";
    ++g_failures;
}

#define EXPECT_EQ(expected, actual)                                            \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::ostringstream _o;                                             \
            _o << std::hex << "expected=0x" << (uint64_t)_e << " actual=0x"    \
               << (uint64_t)_a << std::dec << "  (" #expected " == " #actual   \
               << ")";                                                         \
            fail_at(__FILE__, __LINE__, _o.str());                             \
        }                                                                      \
    } while (0)

// Same as EXPECT_EQ but carries a caller-supplied context string (register
// name / offset / access shape), so a failure inside a sweep is diagnosable.
#define EXPECT_EQ_CTX(expected, actual, ctx)                                   \
    do {                                                                       \
        const auto _e = (expected);                                            \
        const auto _a = (actual);                                              \
        if (!(_e == _a)) {                                                     \
            std::ostringstream _o;                                             \
            _o << (ctx) << ": " << std::hex << "expected=0x" << (uint64_t)_e   \
               << " actual=0x" << (uint64_t)_a << std::dec;                    \
            fail_at(__FILE__, __LINE__, _o.str());                             \
        }                                                                      \
    } while (0)

#define EXPECT_STREQ(expected, actual)                                         \
    do {                                                                       \
        const std::string _e = (expected);                                     \
        const std::string _a = (actual);                                       \
        if (_e != _a) {                                                        \
            fail_at(__FILE__, __LINE__,                                        \
                    "expected=\"" + _e + "\" actual=\"" + _a + "\"");          \
        }                                                                      \
    } while (0)

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fail_at(__FILE__, __LINE__, "expected TRUE: " #cond);              \
        }                                                                      \
    } while (0)

#define EXPECT_TIME_EQ_CTX(expected, actual, ctx)                              \
    do {                                                                       \
        const sc_time _e = (expected);                                         \
        const sc_time _a = (actual);                                           \
        if (_e != _a) {                                                        \
            fail_at(__FILE__, __LINE__,                                        \
                    std::string(ctx) + ": expected=" + _e.to_string() +        \
                        " actual=" + _a.to_string());                          \
        }                                                                      \
    } while (0)

// ===========================================================================
// Independent register manifest (see file header)
// ===========================================================================

/// A reserved address between cgm_1's end and awm_0's base: the representative
/// decode gap used wherever a case needs "an address that routes nowhere".
constexpr uint64_t kDecodeGap = 0x300;

struct expect_reg {
    uint32_t    offset;
    const char* name;
    uint32_t    reset;
    uint32_t    rmask;   ///< software-readable bits (0 elsewhere => RAZ)
    uint32_t    wmask;   ///< software-writable bits (0 => read-only)
    uint32_t    sc;      ///< bits forced back to 0 after any write
};

// -- pll_cntl.rdl -----------------------------------------------------------
// AWM_x_CTRL    : freq_sel_one_hot clk0[2:0]/clk1[5:3]/clk2[8:6] + droop[18:16]
// AWM_x_STATUS  : lock_detect[2:0], debug_status[7:4], droop_interrupt[12:8] RO
// AG_MUX_SELECT : cgm_clkmux[7:0], cgm_ag[15:8], awm_clkmux[21:16], awm_ag[29:24]
// GPIO_CLK_OBS  : gpio_mux_sel[2:0], postdiv_divider[11:4], use_postdiv[12],
//                 postdiv_update_div[16] (singlepulse), clk_obs_en[20]
// CLOCK_COUNTER : cgm_0[3:0], cgm_1[7:4], awm_0[10:8], awm_1[14:12]
constexpr expect_reg kExpPllCntl[] = {
    {0x00, "CGM_0_STATUS",            0x00000000u, 0x000000F1u, 0x00000000u, 0x00000000u},
    {0x04, "CGM_1_STATUS",            0x00000000u, 0x000000F1u, 0x00000000u, 0x00000000u},
    {0x10, "AWM_0_CTRL",              0x00000111u, 0x000701FFu, 0x000701FFu, 0x00000000u},
    {0x14, "AWM_0_STATUS",            0x00000000u, 0x00001FF7u, 0x00000000u, 0x00000000u},
    {0x18, "AWM_1_CTRL",              0x00000111u, 0x000701FFu, 0x000701FFu, 0x00000000u},
    {0x1C, "AWM_1_STATUS",            0x00000000u, 0x00001FF7u, 0x00000000u, 0x00000000u},
    {0x20, "AG_MUX_SELECT",           0x00000000u, 0x3F3FFFFFu, 0x3F3FFFFFu, 0x00000000u},
    {0x24, "GPIO_CLK_OBS_CTRL",       0x00001500u, 0x00111FF7u, 0x00111FF7u, 0x00010000u},
    {0x30, "CLOCK_COUNTER_CTRL",      0x00000000u, 0x000077FFu, 0x000077FFu, 0x00000000u},
    {0x34, "CLOCK_COUNTER_STATUS",    0x00000000u, 0x000077FFu, 0x00000000u, 0x00000000u},
    {0x38, "REF_CLK_COUNT_PERIOD_LO", 0x00000000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u},
    {0x3C, "REF_CLK_COUNT_PERIOD_HI", 0x00000000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u},
    {0x40, "CGM_0_CLOCK_0_COUNT_LO",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x44, "CGM_0_CLOCK_0_COUNT_HI",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x50, "CGM_1_CLOCK_0_COUNT_LO",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x54, "CGM_1_CLOCK_0_COUNT_HI",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x60, "AWM_0_CLOCK_0_COUNT_LO",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x64, "AWM_0_CLOCK_0_COUNT_HI",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x70, "AWM_0_CLOCK_2_COUNT_LO",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x74, "AWM_0_CLOCK_2_COUNT_HI",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x80, "AWM_1_CLOCK_0_COUNT_LO",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
    {0x84, "AWM_1_CLOCK_0_COUNT_HI",  0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u},
};

// -- cgm.rdl (16-bit registers on 4-byte strides) ---------------------------
constexpr expect_reg kExpCgm[] = {
    {0x00, "ENABLES",             0x0004u, 0x0007u, 0x0007u, 0x0000u},
    {0x04, "FCW_INT",             0x0010u, 0x00FFu, 0x00FFu, 0x0000u},
    {0x08, "FCW_FRAC",            0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x0C, "PREDIV",              0x0000u, 0x0003u, 0x0003u, 0x0000u},
    {0x10, "POSTDIV_ARRAY_0",     0x0000u, 0x0FFFu, 0x0FFFu, 0x0000u},
    {0x18, "POSTDIV_CONFIG",      0x0000u, 0x00FFu, 0x00FFu, 0x0000u},
    {0x1C, "LOCK_CONFIG",         0x0000u, 0x007Fu, 0x007Fu, 0x0000u},
    {0x20, "REG_UPDATE",          0x0000u, 0x0001u, 0x0001u, 0x0001u},
    {0x38, "OPEN_LOOP",           0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x3C, "LOCK_MONITOR",        0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x40, "SAMPLE_STROBE",       0x0000u, 0x0001u, 0x0001u, 0x0001u},
    {0x44, "DM0_READ",            0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x48, "DM1_READ",            0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x4C, "CGM_STATUS",          0x0000u, 0x0007u, 0x0000u, 0x0000u},
    {0x50, "DM2_READ",            0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x54, "DM3_READ",            0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x58, "DCO_CODE_INST_READ",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x5C, "MONITOR_CONFIG",      0x0000u, 0x0001u, 0x0001u, 0x0000u},
    {0x60, "FREQ_MONITOR_CONFIG", 0x0000u, 0x007Fu, 0x007Fu, 0x0000u},
    {0x64, "FREQ_MONITOR_COUNT",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x6C, "DM0_CONFIG",          0x0000u, 0x007Fu, 0x007Fu, 0x0000u},
    {0x8C, "FCW_INC",             0x0000u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x90, "SSC_HALF_PERIOD",     0x0000u, 0xFFFFu, 0xFFFFu, 0x0000u},
};

// -- awm.rdl GLOBAL sub-block @0x000 ---------------------------------------
constexpr expect_reg kExpAwmGlobal[] = {
    {0x00, "RESOURCE_CONFIGURATION_ENABLES", 0x0300u, 0x1FFFu, 0x1FFFu, 0x0000u},
    {0x04, "DYNAMIC_SCHEME_0",      0x0000u, 0x0007u, 0x0007u, 0x0000u},
    {0x08, "DYNAMIC_SCHEME_1",      0x000Fu, 0x00FFu, 0x00FFu, 0x0000u},
    {0x0C, "DYNAMIC_SCHEME_2",      0xFFFFu, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x10, "DYNAMIC_SCHEME_3",      0xFFFFu, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x14, "DYNAMIC_SCHEME_4",      0xFFFFu, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x18, "DYNAMIC_SCHEME_5",      0x0000u, 0x01FFu, 0x01FFu, 0x0000u},
    {0x1C, "FCW_INT_BOUND",         0x1428u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x20, "FCW_FRAC_UPPERBOUND",   0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x24, "FCW_FRAC_LOWERBOUND",   0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x28, "REG_UPDATE",            0x0000u, 0x0000u, 0x0003u, 0x0003u},
    {0x34, "MEAS_CONFIG",           0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x38, "MEAS_DURATION0",        0x0000u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x3C, "MEAS_DURATION1",        0x0000u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x40, "MEAS_STATUS",           0x0000u, 0x001Fu, 0x0000u, 0x0000u},
    {0x44, "EXT_FREQ",              0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x48, "EXT_REF",               0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x4C, "EXT_DROOP_DUR0",        0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x50, "EXT_DROOP_DUR1",        0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x54, "EXT_DROOP_DUR2",        0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x58, "EXT_DROOP_DUR3",        0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x5C, "EXT_DROOP_TRAN01",      0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x60, "EXT_DROOP_TRAN23",      0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x64, "EXT_FLOOR_LOOPS",       0x0000u, 0x00FFu, 0x0000u, 0x0000u},
    {0x68, "EXT_FLOOR1_DURATION0",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x6C, "EXT_FLOOR2_DURATION0",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x70, "EXT_FLOOR3_DURATION0",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x74, "EXT_FLOOR1_DURATION1",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x78, "EXT_FLOOR2_DURATION1",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x7C, "EXT_FLOOR3_DURATION1",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x80, "EXT_FLOOR1_DURATION2",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x84, "EXT_FLOOR2_DURATION2",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x88, "EXT_FLOOR3_DURATION2",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x8C, "EXT_SAFETY_DURATION0",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x90, "EXT_SAFETY_DURATION1",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x94, "EXT_SAFETY_DURATION2",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x98, "LOCK_STATUS",           0x0000u, 0x07FFu, 0x0007u, 0x0000u},
    {0x9C, "LOCK_MONITOR_0",        0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0xA0, "LOCK_MONITOR_1",        0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0xA4, "LOCK_MONITOR_2",        0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0xA8, "DROOP_PAUSE",           0x0000u, 0x0007u, 0x0007u, 0x0000u},
};

// -- awm.rdl FREQUENCYn sub-block (x6, stride 0x40 from 0x100) --------------
constexpr expect_reg kExpAwmFreq[] = {
    {0x00, "FCW_INT0",          0x1010u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x04, "FCW_INT1",          0x1010u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x08, "FCW_FRAC",          0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x0C, "FCW_FRAC1",         0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x10, "FCW_FRAC2",         0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x14, "FCW_FRAC3",         0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x18, "PREDIV",            0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x1C, "FREQ_ACQ_ENABLES",  0x0001u, 0x0001u, 0x0001u, 0x0000u},
};

// -- awm.rdl CGMn sub-block (x3 @0x280 / 0x380 / 0x480) --------------------
constexpr expect_reg kExpAwmCgm[] = {
    {0x00, "ENABLES",             0x0000u, 0x0001u, 0x0001u, 0x0000u},
    {0x04, "OPEN_LOOP",           0x0000u, 0x3FFEu, 0x3FFEu, 0x0000u},
    {0x20, "DM0_READ",            0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x24, "DM1_READ",            0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x28, "DM2_READ",            0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x2C, "DM3_READ",            0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x30, "CGM_STATUS",          0x0000u, 0x0003u, 0x0000u, 0x0000u},
    {0x34, "DM0_CONFIG",          0x0000u, 0x007Fu, 0x007Fu, 0x0000u},
    {0x54, "FREQ_MONITOR_CONFIG", 0x0000u, 0x047Fu, 0x047Fu, 0x0000u},
    {0x58, "FREQ_MONITOR_COUNT",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x60, "DCO_CODE_INST_READ",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
};

template <std::size_t N>
constexpr std::size_t count(const expect_reg (&)[N]) { return N; }

// Tile the awm sub-block layouts at their instance bases.  Written out here
// rather than reusing the model's tiling so a stride/base error in awm.cpp
// cannot hide behind identical code on both sides.
std::vector<expect_reg> awm_manifest()
{
    std::vector<expect_reg> t;
    auto tile = [&t](const expect_reg* sub, std::size_t n, uint32_t base) {
        for (std::size_t i = 0; i < n; ++i) {
            expect_reg e = sub[i];
            e.offset += base;
            t.push_back(e);
        }
    };

    tile(kExpAwmGlobal, count(kExpAwmGlobal), 0x000u);
    for (unsigned f = 0; f < 6; ++f) {
        tile(kExpAwmFreq, count(kExpAwmFreq), 0x100u + 0x40u * f);
    }
    const uint32_t cgm_bases[3] = {0x280u, 0x380u, 0x480u};
    for (unsigned c = 0; c < 3; ++c) {
        tile(kExpAwmCgm, count(kExpAwmCgm), cgm_bases[c]);
    }
    return t;
}

std::vector<expect_reg> manifest_of(const expect_reg* p, std::size_t n)
{
    return std::vector<expect_reg>(p, p + n);
}

// ---- Expected software semantics, independent of the model's helpers ------

uint32_t exp_sw_read(const expect_reg& r, uint32_t stored)
{
    return stored & r.rmask;
}

uint32_t exp_sw_write(const expect_reg& r, uint32_t stored, uint32_t in)
{
    return ((in & r.wmask) | (stored & ~r.wmask)) & ~r.sc;
}

std::string ctx_of(const expect_reg& r, const char* blk, const char* what)
{
    std::ostringstream o;
    o << blk << "." << r.name << " @0x" << std::hex << r.offset << std::dec
      << " [" << what << "]";
    return o.str();
}

// ===========================================================================
// Register-bus driver
// ===========================================================================

struct req {
    tlm::tlm_command cmd  = tlm::TLM_READ_COMMAND;
    uint64_t         addr = 0;
    unsigned         len  = 4;
    void*            data = nullptr;
    /// -1 => set streaming_width to len (the usual single-beat case).
    int              streaming_width = -1;
    uint8_t*         be      = nullptr;
    unsigned         be_len  = 0;
    sc_time          delay_in = SC_ZERO_TIME;
    smc::smc_axi_extension* ext = nullptr;
};

struct rsp {
    tlm::tlm_response_status status      = tlm::TLM_INCOMPLETE_RESPONSE;
    sc_time                  delay_delta = SC_ZERO_TIME;
    uint64_t                 addr_after  = 0;
    bool                     dmi_allowed = true;
};

struct driver : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<driver> sock;

    explicit driver(sc_module_name n) : sc_module(n), sock("sock") {}

    /// Detaches a bench-owned extension before the payload is destroyed.
    /// `~tlm_generic_payload` calls free() -- i.e. delete -- on every
    /// extension slot, including sticky ones, so a stack-allocated extension
    /// must be cleared even if b_transport unwinds.
    struct ext_detacher {
        tlm::tlm_generic_payload& gp;
        bool                      attached;
        ~ext_detacher()
        {
            if (attached) gp.clear_extension<smc::smc_axi_extension>();
        }
    };

    rsp send(const req& r)
    {
        tlm::tlm_generic_payload gp;
        sc_time delay = r.delay_in;

        gp.set_command(r.cmd);
        gp.set_address(r.addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(r.data));
        gp.set_data_length(r.len);
        gp.set_streaming_width(r.streaming_width < 0
                                   ? r.len
                                   : static_cast<unsigned>(r.streaming_width));
        gp.set_byte_enable_ptr(r.be);
        gp.set_byte_enable_length(r.be_len);
        gp.set_dmi_allowed(true);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        if (r.ext != nullptr) gp.set_extension(r.ext);
        const ext_detacher detach{gp, r.ext != nullptr};

        sock->b_transport(gp, delay);

        rsp out;
        out.status      = gp.get_response_status();
        out.delay_delta = delay - r.delay_in;
        out.addr_after  = gp.get_address();
        out.dmi_allowed = gp.is_dmi_allowed();
        return out;
    }

    /// Raw back-door access over transport_dbg.  Returns bytes transferred.
    unsigned dbg(tlm::tlm_command cmd, uint64_t addr, unsigned len, void* data,
                 int streaming_width = -1, uint8_t* be = nullptr,
                 unsigned be_len = 0)
    {
        tlm::tlm_generic_payload gp;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        gp.set_data_length(len);
        gp.set_streaming_width(streaming_width < 0
                                   ? len
                                   : static_cast<unsigned>(streaming_width));
        gp.set_byte_enable_ptr(be);
        gp.set_byte_enable_length(be_len);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return sock->transport_dbg(gp);
    }

    bool dmi(uint64_t addr, tlm::tlm_dmi& dmi_data)
    {
        tlm::tlm_generic_payload gp;
        uint32_t scratch = 0;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(addr);
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(&scratch));
        gp.set_data_length(4);
        gp.set_streaming_width(4);
        return sock->get_direct_mem_ptr(gp, dmi_data);
    }

    // ---- Convenience wrappers (assert TLM_OK) ----------------------------

    uint32_t read(uint64_t addr, unsigned len = 4)
    {
        uint32_t data = 0;
        req r;
        r.cmd = tlm::TLM_READ_COMMAND;
        r.addr = addr;
        r.len = len;
        r.data = &data;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, send(r).status);
        return data;
    }

    void write(uint64_t addr, uint32_t data, unsigned len = 4)
    {
        req r;
        r.cmd = tlm::TLM_WRITE_COMMAND;
        r.addr = addr;
        r.len = len;
        r.data = &data;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, send(r).status);
    }

    uint32_t read32(uint64_t addr)          { return read(addr, 4); }
    void     write32(uint64_t a, uint32_t d) { write(a, d, 4); }
    uint16_t read16(uint64_t addr)
    {
        return static_cast<uint16_t>(read(addr, 2));
    }
    void write16(uint64_t a, uint16_t d) { write(a, d, 2); }

    uint32_t dbg_read32(uint64_t addr)
    {
        uint32_t v = 0;
        EXPECT_EQ(4u, dbg(tlm::TLM_READ_COMMAND, addr, 4, &v));
        return v;
    }

    tlm::tlm_response_status try_access(tlm::tlm_command cmd, uint64_t addr,
                                        unsigned len)
    {
        uint8_t buf[8] = {0};
        req r;
        r.cmd = cmd;
        r.addr = addr;
        r.len = len;
        r.data = buf;
        return send(r).status;
    }
};

// ===========================================================================
// Reusable architectural checks over a manifest
// ===========================================================================

/// Every register reads its reset value through the software read mask.
void check_reset_image(driver& d, uint64_t base,
                       const std::vector<expect_reg>& m, const char* blk)
{
    for (const expect_reg& r : m) {
        EXPECT_EQ_CTX(exp_sw_read(r, r.reset), d.read32(base + r.offset),
                      ctx_of(r, blk, "reset"));
    }
}

/// Per-register RW/RO/self-clear contract: all-ones then all-zeros writes,
/// each checked against the manifest's own expectation of the stored value.
/// `dbg_oracle` reads raw storage over transport_dbg, which is the only way to
/// observe a write-only strobe's self-clear.
void check_reg_contract(driver& d, uint64_t base,
                        const std::vector<expect_reg>& m, const char* blk)
{
    for (const expect_reg& r : m) {
        uint32_t stored = r.reset;

        stored = exp_sw_write(r, stored, 0xFFFFFFFFu);
        d.write32(base + r.offset, 0xFFFFFFFFu);
        EXPECT_EQ_CTX(exp_sw_read(r, stored), d.read32(base + r.offset),
                      ctx_of(r, blk, "write-ones"));
        EXPECT_EQ_CTX(stored, d.dbg_read32(base + r.offset),
                      ctx_of(r, blk, "write-ones raw"));
        if (r.sc != 0u) {
            EXPECT_EQ_CTX(0u, d.dbg_read32(base + r.offset) & r.sc,
                          ctx_of(r, blk, "self-clear"));
        }

        stored = exp_sw_write(r, stored, 0x00000000u);
        d.write32(base + r.offset, 0x00000000u);
        EXPECT_EQ_CTX(exp_sw_read(r, stored), d.read32(base + r.offset),
                      ctx_of(r, blk, "write-zeros"));
        EXPECT_EQ_CTX(stored, d.dbg_read32(base + r.offset),
                      ctx_of(r, blk, "write-zeros raw"));
    }
}

/// Every 4-byte offset in the window that the manifest does NOT name must read
/// as zero and ignore writes, and must not alias a real register.
void check_holes(driver& d, uint64_t base, uint64_t window,
                 const std::vector<expect_reg>& m, const char* blk)
{
    std::vector<bool> mapped(static_cast<std::size_t>(window / 4), false);
    for (const expect_reg& r : m) {
        // A manifest offset outside the window would be a manifest bug and
        // would index out of range below, so report it rather than assume it.
        if (r.offset >= window || (r.offset & 0x3u) != 0u) {
            fail_at(__FILE__, __LINE__,
                    ctx_of(r, blk, "offset outside window or unaligned"));
            continue;
        }
        mapped[r.offset / 4] = true;
    }

    for (uint64_t off = 0; off < window; off += 4) {
        if (mapped[static_cast<std::size_t>(off / 4)]) continue;
        std::ostringstream o;
        o << blk << " hole @0x" << std::hex << off << std::dec;
        EXPECT_EQ_CTX(0u, d.read32(base + off), o.str() + " [RAZ]");
        d.write32(base + off, 0xFFFFFFFFu);
        EXPECT_EQ_CTX(0u, d.read32(base + off), o.str() + " [WI]");
        EXPECT_EQ_CTX(0u, d.dbg_read32(base + off), o.str() + " [raw]");
    }
}

/// Sub-word lane matrix for one register: every 1/2/4-byte lane, read and
/// write, checked against an independent lane model.
///
/// The model moves sub-word payload bytes with memcpy, so lane N of the 32-bit
/// register is byte N of the payload on a little-endian host.  That matches
/// every target this VP runs on; the expectations below assume it.
void check_lanes(driver& d, uint64_t base, const expect_reg& r,
                 const char* blk, uint32_t seed_raw)
{
    for (unsigned len : {1u, 2u, 4u}) {
        for (unsigned lane = 0; lane < 4u; lane += len) {
            const unsigned lane_bits = lane * 8u;
            const uint32_t lane_mask =
                (len == 4) ? 0xFFFFFFFFu
                           : (((uint32_t{1} << (len * 8u)) - 1u) << lane_bits);

            std::ostringstream o;
            o << blk << "." << r.name << " @0x" << std::hex << r.offset
              << " len=" << len << " lane=" << lane << std::dec;
            const std::string ctx = o.str();

            // Seed raw storage over the back door so the lane check starts
            // from a known value even for read-only registers.
            EXPECT_EQ(4u, d.dbg(tlm::TLM_WRITE_COMMAND, base + r.offset, 4,
                                &seed_raw));
            uint32_t stored = seed_raw;

            // Read the lane.
            uint32_t got = 0;
            req rq;
            rq.cmd  = tlm::TLM_READ_COMMAND;
            rq.addr = base + r.offset + lane;
            rq.len  = len;
            rq.data = &got;
            EXPECT_EQ_CTX(tlm::TLM_OK_RESPONSE, d.send(rq).status,
                          ctx + " [read status]");
            EXPECT_EQ_CTX(((exp_sw_read(r, stored) & lane_mask) >> lane_bits),
                          got, ctx + " [read]");

            // Write the lane: untouched lanes keep their raw value, the
            // touched lane takes the new bytes, then masks and self-clear
            // apply to the merged 32-bit word.
            uint32_t in = 0xA5C3F00Fu;
            const uint32_t merged =
                (stored & ~lane_mask) | ((in << lane_bits) & lane_mask);
            stored = exp_sw_write(r, stored, merged);

            req wq;
            wq.cmd  = tlm::TLM_WRITE_COMMAND;
            wq.addr = base + r.offset + lane;
            wq.len  = len;
            wq.data = &in;
            EXPECT_EQ_CTX(tlm::TLM_OK_RESPONSE, d.send(wq).status,
                          ctx + " [write status]");
            EXPECT_EQ_CTX(stored, d.dbg_read32(base + r.offset),
                          ctx + " [write raw]");
            EXPECT_EQ_CTX(exp_sw_read(r, stored), d.read32(base + r.offset),
                          ctx + " [write readback]");
        }
    }
}

/// Every self-clearing bit must return to zero after a write that sets it,
/// through each access width whose lane actually contains the bit.
void check_strobes(driver& d, uint64_t base, const expect_reg& r,
                   const char* blk)
{
    for (unsigned len : {1u, 2u, 4u}) {
        for (unsigned lane = 0; lane < 4u; lane += len) {
            const unsigned lane_bits = lane * 8u;
            const uint32_t lane_mask =
                (len == 4) ? 0xFFFFFFFFu
                           : (((uint32_t{1} << (len * 8u)) - 1u) << lane_bits);
            if ((r.sc & lane_mask) == 0u) continue;

            std::ostringstream o;
            o << blk << "." << r.name << " @0x" << std::hex << r.offset
              << " strobe len=" << len << " lane=" << lane << std::dec;

            uint32_t all_ones = 0xFFFFFFFFu;
            req wq;
            wq.cmd  = tlm::TLM_WRITE_COMMAND;
            wq.addr = base + r.offset + lane;
            wq.len  = len;
            wq.data = &all_ones;
            EXPECT_EQ_CTX(tlm::TLM_OK_RESPONSE, d.send(wq).status,
                          o.str() + " [status]");
            EXPECT_EQ_CTX(0u, d.dbg_read32(base + r.offset) & r.sc,
                          o.str() + " [raw self-clear]");
            EXPECT_EQ_CTX(0u, d.read32(base + r.offset) & r.sc,
                          o.str() + " [readback]");
        }
    }
}

// ===========================================================================
// Structural probe: exposes reg_block's protected callback hooks
// ===========================================================================

struct probe_cntl : smc::pll::pll_cntl {
    explicit probe_cntl(sc_module_name n) : pll_cntl(n) {}

    bool install_write(uint64_t off, regmodel::Register32::WriteFn fn)
    {
        return set_write_callback(off, std::move(fn));
    }
    bool install_read(uint64_t off, regmodel::Register32::ReadFn fn)
    {
        return set_read_callback(off, std::move(fn));
    }
};

// ===========================================================================
// Top-level TB
// ===========================================================================

struct tb : sc_core::sc_module {
    sc_core::sc_signal<bool> rst_n{"rst_n"};

    smc::pll::pll_wrapper dut;

    // Standalone children: the exhaustive per-register contract runs here so
    // the wrapper's REG_UPDATE lock observers cannot perturb a status register
    // mid-sweep.  Routing to the same registers is proved separately.
    smc::pll::pll_cntl m_cntl;
    smc::pll::cgm      m_cgm;
    smc::pll::awm      m_awm;
    probe_cntl         probe;

    driver drv;
    driver d_cntl;
    driver d_cgm;
    driver d_awm;
    driver probe_drv;

    // Expected register maps, built once from the independent manifest.
    const std::vector<expect_reg> man_cntl =
        manifest_of(kExpPllCntl, count(kExpPllCntl));
    const std::vector<expect_reg> man_cgm = manifest_of(kExpCgm, count(kExpCgm));
    const std::vector<expect_reg> man_awm = awm_manifest();

    SC_HAS_PROCESS(tb);

    explicit tb(sc_module_name n)
        : sc_module(n), dut("dut"), m_cntl("m_cntl"), m_cgm("m_cgm"),
          m_awm("m_awm"), probe("probe"), drv("drv"), d_cntl("d_cntl"),
          d_cgm("d_cgm"), d_awm("d_awm"), probe_drv("probe_drv")
    {
        dut.rst_n_i(rst_n);
        m_cntl.rst_n_i(rst_n);
        m_cgm.rst_n_i(rst_n);
        m_awm.rst_n_i(rst_n);
        probe.rst_n_i(rst_n);

        drv.sock.bind(dut.reg_socket);
        d_cntl.sock.bind(m_cntl.reg_socket);
        d_cgm.sock.bind(m_cgm.reg_socket);
        d_awm.sock.bind(m_awm.reg_socket);
        probe_drv.sock.bind(probe.reg_socket);

        SC_THREAD(run);
    }

    void settle() { for (int i = 0; i < 3; ++i) wait(SC_ZERO_TIME); }

    void pulse_reset()
    {
        rst_n.write(false);
        wait(10, SC_NS);
        rst_n.write(true);
        settle();
    }

    // Composed base of each child, paired with its window size.
    struct child_win { uint64_t base; uint64_t size; const char* name; };

    std::vector<child_win> children() const
    {
        using W = smc::pll::pll_wrapper;
        return {
            {W::OFF_PLL_CNTL, smc::pll::pll_cntl_cfg::WINDOW_SIZE, "pll_cntl"},
            {W::OFF_CGM_0,    smc::pll::cgm_cfg::WINDOW_SIZE,      "cgm_0"},
            {W::OFF_CGM_1,    smc::pll::cgm_cfg::WINDOW_SIZE,      "cgm_1"},
            {W::OFF_AWM_0,    smc::pll::awm_cfg::WINDOW_SIZE,      "awm_0"},
            {W::OFF_AWM_1,    smc::pll::awm_cfg::WINDOW_SIZE,      "awm_1"},
        };
    }

    void run();

    // Case groups (split out to keep each readable).
    void t_register_model();
    void t_composed_map();
    void t_bus_contract();
    void t_sideband();
    void t_debug_and_dmi();
    void t_lock_policy();
    void t_timing();
    void t_reset();
    void t_structural();
};

// ---------------------------------------------------------------------------
// Architectural: register model proved against the independent manifest
// ---------------------------------------------------------------------------
void tb::t_register_model()
{
    const std::vector<expect_reg>& mc = man_cntl;
    const std::vector<expect_reg>& mg = man_cgm;
    const std::vector<expect_reg>& ma = man_awm;

    std::cout << "\n--- Architectural: register model vs independent manifest ---\n";

    // 1. The model must contain exactly the registers the manifest names --
    //    a missing or extra table entry is caught here, not by sampling.
    EXPECT_EQ(mc.size(), m_cntl.num_registers());
    EXPECT_EQ(mg.size(), m_cgm.num_registers());
    EXPECT_EQ(ma.size(), m_awm.num_registers());
    EXPECT_EQ(smc::pll::pll_cntl_cfg::WINDOW_SIZE, m_cntl.window_size());
    EXPECT_EQ(smc::pll::cgm_cfg::WINDOW_SIZE, m_cgm.window_size());
    EXPECT_EQ(smc::pll::awm_cfg::WINDOW_SIZE, m_awm.window_size());
    std::cout << "  [PASS] register counts and window sizes match the manifest\n";

    // 2. Reset image of every register in every child map.
    pulse_reset();
    check_reset_image(d_cntl, 0, mc, "pll_cntl");
    check_reset_image(d_cgm, 0, mg, "cgm");
    check_reset_image(d_awm, 0, ma, "awm");
    std::cout << "  [PASS] reset value of every register (" << mc.size() + mg.size() + ma.size()
              << " registers)\n";

    // 3. Full RW / RO / self-clear contract of every register.
    check_reg_contract(d_cntl, 0, mc, "pll_cntl");
    check_reg_contract(d_cgm, 0, mg, "cgm");
    check_reg_contract(d_awm, 0, ma, "awm");
    std::cout << "  [PASS] read/write/self-clear mask of every register\n";

    // 4. Reserved holes are RAZ/WI and never alias a register.
    pulse_reset();
    check_holes(d_cntl, 0, smc::pll::pll_cntl_cfg::WINDOW_SIZE, mc, "pll_cntl");
    check_holes(d_cgm, 0, smc::pll::cgm_cfg::WINDOW_SIZE, mg, "cgm");
    check_holes(d_awm, 0, smc::pll::awm_cfg::WINDOW_SIZE, ma, "awm");
    // The hole writes above must not have disturbed any real register.
    check_reset_image(d_cntl, 0, mc, "pll_cntl");
    check_reset_image(d_cgm, 0, mg, "cgm");
    check_reset_image(d_awm, 0, ma, "awm");
    std::cout << "  [PASS] every reserved hole is RAZ/WI and non-aliasing\n";

    // 5. Sub-word lane matrix across each access-type class: RW 32-bit, RW
    //    16-bit-in-word, read-only, self-clearing, and reserved-mask.
    pulse_reset();
    check_lanes(d_cntl, 0, kExpPllCntl[6], "pll_cntl", 0x12345678u);  // AG_MUX RW32
    check_lanes(d_cntl, 0, kExpPllCntl[7], "pll_cntl", 0xFFFFFFFFu);  // GPIO sc
    check_lanes(d_cntl, 0, kExpPllCntl[3], "pll_cntl", 0xDEADBEEFu);  // AWM_0_STATUS RO
    check_lanes(d_cgm, 0, kExpCgm[0], "cgm", 0x0000FFFFu);            // ENABLES reserved
    check_lanes(d_cgm, 0, kExpCgm[7], "cgm", 0xFFFFFFFFu);            // REG_UPDATE sc
    check_lanes(d_cgm, 0, kExpCgm[9], "cgm", 0xCAFEBABEu);            // LOCK_MONITOR RO
    check_lanes(d_awm, 0, kExpAwmGlobal[10], "awm", 0xFFFFFFFFu);     // REG_UPDATE WO+sc
    check_lanes(d_awm, 0, kExpAwmGlobal[36], "awm", 0x000007FFu);     // LOCK_STATUS mixed
    std::cout << "  [PASS] 1/2/4-byte lane matrix on RW / RO / WO / self-clear\n";

    // 6. Every self-clearing field in every child, at every width that can
    //    reach it.  This is the claim the old test plan overstated.
    pulse_reset();
    unsigned strobes = 0;
    for (const expect_reg& r : mc) {
        if (r.sc != 0u) { check_strobes(d_cntl, 0, r, "pll_cntl"); ++strobes; }
    }
    for (const expect_reg& r : mg) {
        if (r.sc != 0u) { check_strobes(d_cgm, 0, r, "cgm"); ++strobes; }
    }
    for (const expect_reg& r : ma) {
        if (r.sc != 0u) { check_strobes(d_awm, 0, r, "awm"); ++strobes; }
    }
    EXPECT_EQ(4u, strobes);  // GPIO postdiv, cgm REG_UPDATE + SAMPLE_STROBE, awm REG_UPDATE
    std::cout << "  [PASS] every self-clearing field (" << strobes
              << ") at 8/16/32-bit\n";
}

// ---------------------------------------------------------------------------
// Architectural: composed address map, routing and boundaries
// ---------------------------------------------------------------------------
void tb::t_composed_map()
{
    using W  = smc::pll::pll_wrapper;
    using AW = smc::pll::awm;
    const std::vector<expect_reg>& mc = man_cntl;
    const std::vector<expect_reg>& mg = man_cgm;
    const std::vector<expect_reg>& ma = man_awm;

    std::cout << "\n--- Architectural: composed address map ---\n";

    // 1. Every register of every child is reachable at its composed address
    //    and holds its reset value there.
    pulse_reset();
    check_reset_image(drv, W::OFF_PLL_CNTL, mc, "wrap.pll_cntl");
    check_reset_image(drv, W::OFF_CGM_0, mg, "wrap.cgm_0");
    check_reset_image(drv, W::OFF_CGM_1, mg, "wrap.cgm_1");
    check_reset_image(drv, W::OFF_AWM_0, ma, "wrap.awm_0");
    check_reset_image(drv, W::OFF_AWM_1, ma, "wrap.awm_1");
    std::cout << "  [PASS] every register reachable at its composed address\n";

    // 2. Dirty every writable register across all five children with an
    //    address-derived pattern, then read the whole map back.  A stride,
    //    alias or cross-child leak shows up as a mismatched value.
    //
    //    The only exclusion is the commit strobe each child installs a lock
    //    observer on: writing it would move a status register in a *different*
    //    child and so is not an aliasing signal.  Those two offsets are proved
    //    by the strobe and lock matrices instead.  Plain self-clearing fields
    //    (GPIO postdiv, cgm SAMPLE_STROBE) stay in, since exp_sw_write models
    //    their self-clear.
    static constexpr uint64_t kNoObserver = UINT64_MAX;
    struct block {
        uint64_t                      base;
        const std::vector<expect_reg>* m;
        const char*                   name;
        uint64_t                      observed_off;
    };
    const std::vector<block> blocks = {
        {W::OFF_PLL_CNTL, &mc, "wrap.pll_cntl", kNoObserver},
        {W::OFF_CGM_0,    &mg, "wrap.cgm_0",    smc::pll::cgm::OFF_REG_UPDATE},
        {W::OFF_CGM_1,    &mg, "wrap.cgm_1",    smc::pll::cgm::OFF_REG_UPDATE},
        {W::OFF_AWM_0,    &ma, "wrap.awm_0",    AW::GLOBAL_REG_UPDATE},
        {W::OFF_AWM_1,    &ma, "wrap.awm_1",    AW::GLOBAL_REG_UPDATE},
    };

    auto pattern = [](uint64_t abs_off) {
        return static_cast<uint32_t>(0x9E3779B9u * (abs_off + 1u));
    };
    auto dirtied = [](const block& b, const expect_reg& r) {
        return r.wmask != 0u && r.offset != b.observed_off;
    };

    for (const block& b : blocks) {
        for (const expect_reg& r : *b.m) {
            if (!dirtied(b, r)) continue;
            drv.write32(b.base + r.offset, pattern(b.base + r.offset));
        }
    }
    for (const block& b : blocks) {
        for (const expect_reg& r : *b.m) {
            uint32_t stored = r.reset;
            if (dirtied(b, r)) {
                stored = exp_sw_write(r, stored, pattern(b.base + r.offset));
            }
            EXPECT_EQ_CTX(exp_sw_read(r, stored),
                          drv.read32(b.base + r.offset),
                          ctx_of(r, b.name, "dirty-all"));
        }
    }
    std::cout << "  [PASS] unique pattern per register: no aliasing or "
                 "cross-child leakage\n";

    // 3. Child boundaries and reserved gaps, byte-exact.
    pulse_reset();
    for (const child_win& c : children()) {
        std::ostringstream o;
        o << c.name << " boundary";
        // First and last byte of the window are inside.
        EXPECT_EQ_CTX(tlm::TLM_OK_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND, c.base, 1),
                      o.str() + " [first byte]");
        EXPECT_EQ_CTX(tlm::TLM_OK_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND,
                                     c.base + c.size - 1, 1),
                      o.str() + " [last byte]");
        // Last 32-bit word is inside; the next word is not.
        EXPECT_EQ_CTX(tlm::TLM_OK_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND,
                                     c.base + c.size - 4, 4),
                      o.str() + " [last word]");
        EXPECT_EQ_CTX(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND, c.base + c.size, 4),
                      o.str() + " [first word past end]");
        // Sub-word accesses at the very end of the window.
        EXPECT_EQ_CTX(tlm::TLM_OK_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND,
                                     c.base + c.size - 2, 2),
                      o.str() + " [last halfword]");
        EXPECT_EQ_CTX(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND,
                                     c.base + c.size + 2, 2),
                      o.str() + " [halfword past end]");
    }
    // The byte just below each child base that is not itself in a child.
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.try_access(tlm::TLM_READ_COMMAND, 0x0FC, 4));   // cntl gap
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.try_access(tlm::TLM_READ_COMMAND, 0x3FC, 4));   // pre-awm_0
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.try_access(tlm::TLM_READ_COMMAND, 0x9FC, 4));   // pre-awm_1
    // Named gaps, the end of the wrapper window, and beyond it.
    for (uint64_t gap : {UINT64_C(0x300), UINT64_C(0x900), UINT64_C(0xF00),
                         UINT64_C(0xFFC), UINT64_C(0x1000)}) {
        std::ostringstream o;
        o << "gap @0x" << std::hex << gap;
        EXPECT_EQ_CTX(tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      drv.try_access(tlm::TLM_READ_COMMAND, gap, 4), o.str());
    }
    std::cout << "  [PASS] byte-exact child boundaries and reserved gaps\n";

    // 4. Address-overflow safety.  `adr + len` wraps for an aligned address at
    //    the very top of the 64-bit space; a target that checks the sum would
    //    accept the access and alias into the window.
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.try_access(tlm::TLM_READ_COMMAND, UINT64_MAX, 1));
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              drv.try_access(tlm::TLM_READ_COMMAND, UINT64_MAX - 3, 4));
    // Same shapes straight at a child target, which is where the bound lives.
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              d_cntl.try_access(tlm::TLM_READ_COMMAND, UINT64_MAX - 3, 4));
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              d_cgm.try_access(tlm::TLM_WRITE_COMMAND, UINT64_MAX - 1, 2));
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              d_awm.try_access(tlm::TLM_READ_COMMAND, UINT64_MAX, 1));
    EXPECT_EQ(tlm::TLM_ADDRESS_ERROR_RESPONSE,
              d_cntl.try_access(tlm::TLM_READ_COMMAND,
                                smc::pll::pll_cntl_cfg::WINDOW_SIZE, 4));
    std::cout << "  [PASS] address-overflow-safe window bound\n";
}

// ---------------------------------------------------------------------------
// Architectural: TLM-2.0 bus contract
// ---------------------------------------------------------------------------
void tb::t_bus_contract()
{
    using W = smc::pll::pll_wrapper;
    std::cout << "\n--- Architectural: TLM-2.0 bus contract ---\n";
    pulse_reset();

    // The malformed-payload matrix is applied to every route and to a gap, so
    // no child can be more permissive than its siblings.
    const uint64_t targets[] = {W::OFF_PLL_CNTL, W::OFF_CGM_0, W::OFF_CGM_1,
                                W::OFF_AWM_0,    W::OFF_AWM_1, kDecodeGap};
    for (uint64_t t : targets) {
        const bool gap = (t == kDecodeGap);
        std::ostringstream o;
        o << "route @0x" << std::hex << t << std::dec;
        // Sized for the largest data_length used below, so an over-long access
        // stays in-bounds even though the target rejects it before any copy.
        uint8_t scratch[8] = {0};

        // A decode miss is reported by the wrapper before the payload shape is
        // ever inspected; inside a child the shape is checked first.
        const tlm::tlm_response_status null_exp =
            gap ? tlm::TLM_ADDRESS_ERROR_RESPONSE : tlm::TLM_GENERIC_ERROR_RESPONSE;

        {   // null data pointer
            req r; r.addr = t; r.len = 4; r.data = nullptr;
            const rsp s = drv.send(r);
            EXPECT_EQ_CTX(null_exp, s.status, o.str() + " [null ptr]");
            EXPECT_EQ_CTX(t, s.addr_after, o.str() + " [null ptr addr]");
        }
        {   // zero length
            req r; r.addr = t; r.len = 0; r.data = scratch;
            EXPECT_EQ_CTX(null_exp, drv.send(r).status, o.str() + " [len 0]");
        }
        for (unsigned bad : {3u, 5u, 6u, 7u, 8u}) {
            req r; r.addr = t; r.len = bad; r.data = scratch;
            r.streaming_width = static_cast<int>(bad);
            const tlm::tlm_response_status exp =
                gap ? tlm::TLM_ADDRESS_ERROR_RESPONSE
                    : tlm::TLM_BURST_ERROR_RESPONSE;
            std::ostringstream w; w << o.str() << " [len " << bad << "]";
            EXPECT_EQ_CTX(exp, drv.send(r).status, w.str());
        }
        {   // unsupported command
            req r; r.cmd = tlm::TLM_IGNORE_COMMAND; r.addr = t; r.data = scratch;
            const tlm::tlm_response_status exp =
                gap ? tlm::TLM_ADDRESS_ERROR_RESPONSE
                    : tlm::TLM_COMMAND_ERROR_RESPONSE;
            EXPECT_EQ_CTX(exp, drv.send(r).status, o.str() + " [ignore cmd]");
        }
        if (!gap) {
            // Misalignment for each width that can be misaligned.
            EXPECT_EQ_CTX(tlm::TLM_BURST_ERROR_RESPONSE,
                          drv.try_access(tlm::TLM_READ_COMMAND, t + 1, 2),
                          o.str() + " [2B @+1]");
            EXPECT_EQ_CTX(tlm::TLM_BURST_ERROR_RESPONSE,
                          drv.try_access(tlm::TLM_READ_COMMAND, t + 2, 4),
                          o.str() + " [4B @+2]");
            EXPECT_EQ_CTX(tlm::TLM_BURST_ERROR_RESPONSE,
                          drv.try_access(tlm::TLM_READ_COMMAND, t + 3, 2),
                          o.str() + " [2B @+3 straddles word]");
        }
    }
    std::cout << "  [PASS] malformed-payload matrix on every route and a gap\n";

    // Byte enables: refused for every byte-enable length, including a fully
    // enabled pattern, because the target does not model them at all.
    {
        uint32_t scratch = 0;
        uint8_t  be[8]   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        for (unsigned be_len : {0u, 1u, 2u, 4u, 8u}) {
            for (tlm::tlm_command cmd :
                 {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
                req r;
                r.cmd = cmd; r.addr = W::OFF_CGM_0; r.len = 4;
                r.data = &scratch; r.be = be; r.be_len = be_len;
                std::ostringstream o; o << "byte-enable len " << be_len;
                EXPECT_EQ_CTX(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                              drv.send(r).status, o.str());
            }
        }
        // A null byte-enable pointer with a stale non-zero length is legal.
        req ok; ok.addr = W::OFF_CGM_0; ok.len = 4; ok.data = &scratch;
        ok.be = nullptr; ok.be_len = 4;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, drv.send(ok).status);
    }
    std::cout << "  [PASS] byte enables refused at every byte-enable length\n";

    // Streaming width: a register access is one beat, so TLM-2.0 requires
    // streaming_width >= data_length.  Smaller (including 0) is an error;
    // larger is accepted and ignored.
    {
        uint32_t scratch = 0;
        for (unsigned len : {1u, 2u, 4u}) {
            for (unsigned sw = 0; sw <= 8; ++sw) {
                req r;
                r.cmd = tlm::TLM_READ_COMMAND;
                r.addr = W::OFF_CGM_0;
                r.len = len;
                r.data = &scratch;
                r.streaming_width = static_cast<int>(sw);
                const tlm::tlm_response_status exp =
                    (sw < len) ? tlm::TLM_BURST_ERROR_RESPONSE
                               : tlm::TLM_OK_RESPONSE;
                std::ostringstream o;
                o << "streaming_width " << sw << " len " << len;
                EXPECT_EQ_CTX(exp, drv.send(r).status, o.str());
            }
        }
    }
    std::cout << "  [PASS] streaming-width relations (0, <, ==, >)\n";
}

// ---------------------------------------------------------------------------
// Architectural: canonical AXI sideband
// ---------------------------------------------------------------------------
void tb::t_sideband()
{
    using W = smc::pll::pll_wrapper;
    std::cout << "\n--- Architectural: canonical AXI sideband ---\n";
    pulse_reset();

    // Distinctive, non-default field values so a field that is dropped or
    // rewritten anywhere along the route is visible.
    auto make_ext = [] {
        smc::smc_axi_extension e;
        e.source_id = smc::JTAG_ID;
        e.axi_id    = 0x5A5Au;
        e.axi_user  = 0x3Cu;
        e.set_priv(false);
        e.set_secure(true);
        e.set_fetch(true);
        e.set_locked(true);
        return e;
    };
    const smc::smc_axi_extension golden = make_ext();

    auto check_intact = [&](const smc::smc_axi_extension& e,
                            const std::string& ctx) {
        EXPECT_EQ_CTX(golden.source_id, e.source_id, ctx + " [source_id]");
        EXPECT_EQ_CTX(golden.axi_id, e.axi_id, ctx + " [axi_id]");
        EXPECT_EQ_CTX(golden.prot, e.prot, ctx + " [prot]");
        EXPECT_EQ_CTX(golden.axi_user, e.axi_user, ctx + " [axi_user]");
        EXPECT_EQ_CTX(golden.is_locked ? 1 : 0, e.is_locked ? 1 : 0,
                      ctx + " [is_locked]");
        EXPECT_EQ_CTX(golden.is_fetch ? 1 : 0, e.is_fetch ? 1 : 0,
                      ctx + " [is_fetch]");
        EXPECT_EQ_CTX(golden.is_secure ? 1 : 0, e.is_secure ? 1 : 0,
                      ctx + " [is_secure]");
        EXPECT_EQ_CTX(golden.is_user ? 1 : 0, e.is_user ? 1 : 0,
                      ctx + " [is_user]");
    };

    // Every child route, a decode gap, and an error shape inside a child.
    const struct { uint64_t addr; const char* what; tlm::tlm_response_status exp; }
    cases[] = {
        {W::OFF_PLL_CNTL,              "pll_cntl", tlm::TLM_OK_RESPONSE},
        {W::OFF_CGM_0,                 "cgm_0",    tlm::TLM_OK_RESPONSE},
        {W::OFF_CGM_1,                 "cgm_1",    tlm::TLM_OK_RESPONSE},
        {W::OFF_AWM_0,                 "awm_0",    tlm::TLM_OK_RESPONSE},
        {W::OFF_AWM_1,                 "awm_1",    tlm::TLM_OK_RESPONSE},
        {kDecodeGap,                    "gap",      tlm::TLM_ADDRESS_ERROR_RESPONSE},
        {W::OFF_CGM_0 + 2,             "misaligned in child",
                                                   tlm::TLM_BURST_ERROR_RESPONSE},
    };

    for (const auto& c : cases) {
        for (tlm::tlm_command cmd :
             {tlm::TLM_READ_COMMAND, tlm::TLM_WRITE_COMMAND}) {
            smc::smc_axi_extension ext = make_ext();
            uint32_t data = 0x11223344u;
            req r;
            r.cmd  = cmd;
            r.addr = c.addr;
            r.len  = 4;
            r.data = &data;
            r.ext  = &ext;
            const rsp s = drv.send(r);
            const std::string ctx = std::string("sideband via ") + c.what;
            EXPECT_EQ_CTX(c.exp, s.status, ctx + " [status]");
            EXPECT_EQ_CTX(c.addr, s.addr_after, ctx + " [address restored]");
            check_intact(ext, ctx);
        }
    }
    std::cout << "  [PASS] sideband preserved through every route, gap and "
                 "error shape\n";

    // An absent extension must be just as acceptable: the wrapper inspects but
    // never requires the sideband.
    {
        uint32_t data = 0;
        req r; r.addr = W::OFF_AWM_1; r.len = 4; r.data = &data; r.ext = nullptr;
        const rsp s = drv.send(r);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, s.status);
        EXPECT_EQ(W::OFF_AWM_1, s.addr_after);
    }
    std::cout << "  [PASS] absent sideband is accepted unchanged\n";
}

// ---------------------------------------------------------------------------
// Architectural: debug transfer and DMI policy
// ---------------------------------------------------------------------------
void tb::t_debug_and_dmi()
{
    using W  = smc::pll::pll_wrapper;
    using CG = smc::pll::cgm;
    using PC = smc::pll::pll_cntl;
    std::cout << "\n--- Architectural: transport_dbg and DMI policy ---\n";
    pulse_reset();

    // transport_dbg is a raw back door: it bypasses the write mask, so a value
    // no software write could produce is observable, and the software read is
    // still masked.
    {
        uint32_t raw = 0xFFFFFFFFu;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_WRITE_COMMAND,
                              W::OFF_CGM_0 + CG::OFF_ENABLES, 4, &raw));
        EXPECT_EQ(0xFFFFFFFFu, drv.dbg_read32(W::OFF_CGM_0 + CG::OFF_ENABLES));
        EXPECT_EQ(0x7u, drv.read32(W::OFF_CGM_0 + CG::OFF_ENABLES));
    }
    // Sub-word debug access hits the addressed lane only.
    {
        uint32_t seed = 0x00000000u;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_WRITE_COMMAND,
                              W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT, 4, &seed));
        uint16_t half = 0xBEEFu;
        EXPECT_EQ(2u, drv.dbg(tlm::TLM_WRITE_COMMAND,
                              W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT + 2, 2,
                              &half));
        EXPECT_EQ(0xBEEF0000u,
                  drv.dbg_read32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT));
        uint8_t byte = 0;
        EXPECT_EQ(1u, drv.dbg(tlm::TLM_READ_COMMAND,
                              W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT + 3, 1,
                              &byte));
        EXPECT_EQ(0xBEu, byte);
    }
    pulse_reset();

    // A debug write to a strobe must not fire the lock observers: that is what
    // "side-effect-free" has to mean for this model.
    {
        drv.write16(W::OFF_CGM_0 + CG::OFF_ENABLES, 0x3u);
        EXPECT_EQ(0u, drv.read32(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);
        uint32_t strobe = 0x1u;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_WRITE_COMMAND,
                              W::OFF_CGM_0 + CG::OFF_REG_UPDATE, 4, &strobe));
        EXPECT_EQ(0u, drv.read32(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);
        // ... and it does not self-clear either, since no write callback ran.
        EXPECT_EQ(0x1u, drv.dbg_read32(W::OFF_CGM_0 + CG::OFF_REG_UPDATE));
    }
    pulse_reset();

    // Refusals: reserved gap, out of window, bad width, misaligned, null
    // pointer, zero length, byte enables, short streaming width, and an
    // unsupported command all return 0 bytes.
    {
        uint32_t scratch = 0;
        uint8_t  be      = 0xFF;
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, kDecodeGap, 4, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, 0x1000, 4, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, UINT64_MAX - 3, 4, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, W::OFF_CGM_0, 3, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, W::OFF_CGM_0 + 2, 4, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, W::OFF_CGM_0, 4, nullptr));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, W::OFF_CGM_0, 0, &scratch));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, W::OFF_CGM_0, 4, &scratch,
                              -1, &be, 1));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_READ_COMMAND, W::OFF_CGM_0, 4, &scratch, 1));
        EXPECT_EQ(0u, drv.dbg(tlm::TLM_IGNORE_COMMAND, W::OFF_CGM_0, 4, &scratch));
        // A debug write into a reserved hole inside a window is accepted and
        // ignored, matching the RAZ/WI bus contract.
        uint32_t ones = 0xFFFFFFFFu;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_WRITE_COMMAND, W::OFF_PLL_CNTL + 0x08, 4,
                              &ones));
        EXPECT_EQ(0u, drv.dbg_read32(W::OFF_PLL_CNTL + 0x08));
    }
    std::cout << "  [PASS] transport_dbg raw back door and its refusals\n";

    // DMI is denied everywhere: strobes and lock observers mean a direct
    // pointer would bypass real side effects.
    {
        for (const child_win& c : children()) {
            tlm::tlm_dmi dmi_data;
            dmi_data.allow_read_write();
            EXPECT_TRUE(!drv.dmi(c.base, dmi_data));
            EXPECT_TRUE(!dmi_data.is_read_allowed());
            EXPECT_TRUE(!dmi_data.is_write_allowed());
        }
        tlm::tlm_dmi gap_dmi;
        gap_dmi.allow_read_write();
        EXPECT_TRUE(!drv.dmi(kDecodeGap, gap_dmi));
        tlm::tlm_dmi child_dmi;
        child_dmi.allow_read_write();
        EXPECT_TRUE(!d_cgm.dmi(0, child_dmi));

        // And every completed transaction clears the dmi_allowed hint.
        uint32_t scratch = 0;
        req r; r.addr = W::OFF_CGM_0; r.len = 4;
        r.data = &scratch;
        EXPECT_TRUE(!drv.send(r).dmi_allowed);
        req g; g.addr = kDecodeGap; g.len = 4; g.data = &scratch;
        EXPECT_TRUE(!drv.send(g).dmi_allowed);
    }
    std::cout << "  [PASS] DMI denied at wrapper and child, on hit and miss\n";
}

// ---------------------------------------------------------------------------
// Architectural: modelled lock policy
// ---------------------------------------------------------------------------
void tb::t_lock_policy()
{
    using W  = smc::pll::pll_wrapper;
    using PC = smc::pll::pll_cntl;
    using CG = smc::pll::cgm;
    using AW = smc::pll::awm;
    std::cout << "\n--- Architectural: lock policy ---\n";

    // CGM matrix: {cgm_enable} x {REG_UPDATE value}.  Lock is committed only
    // by the commit strobe and then follows cgm_enable; both the pll_cntl
    // aggregate status the firmware polls and the child's own mirror must
    // agree after every combination.
    for (unsigned idx = 0; idx < 2; ++idx) {
        const uint64_t cgm_base = (idx == 0) ? W::OFF_CGM_0 : W::OFF_CGM_1;
        const uint64_t st_off   = W::OFF_PLL_CNTL +
                                  ((idx == 0) ? PC::OFF_CGM_0_STATUS
                                              : PC::OFF_CGM_1_STATUS);
        for (unsigned enables : {0x0u, 0x1u, 0x2u, 0x3u}) {
            for (unsigned strobe : {0x0u, 0x1u}) {
                pulse_reset();
                std::ostringstream o;
                o << "cgm_" << idx << " enables=0x" << std::hex << enables
                  << " strobe=0x" << strobe << std::dec;

                // Start from a locked state so a policy that only ever sets
                // lock cannot pass by accident.
                drv.write16(cgm_base + CG::OFF_ENABLES, 0x1u);
                drv.write16(cgm_base + CG::OFF_REG_UPDATE, 0x1u);
                EXPECT_EQ_CTX(1u, drv.read16(st_off) & 0x1u,
                              o.str() + " [precondition locked]");

                drv.write16(cgm_base + CG::OFF_ENABLES,
                            static_cast<uint16_t>(enables));
                drv.write16(cgm_base + CG::OFF_REG_UPDATE,
                            static_cast<uint16_t>(strobe));

                // No strobe => lock keeps its previous value.
                const uint32_t exp_lock =
                    (strobe & 0x1u) ? ((enables & 0x1u) ? 1u : 0u) : 1u;
                EXPECT_EQ_CTX(exp_lock, drv.read16(st_off) & 0x1u,
                              o.str() + " [pll_cntl status]");
                EXPECT_EQ_CTX(exp_lock,
                              drv.read16(cgm_base + CG::OFF_CGM_STATUS) & 0x1u,
                              o.str() + " [cgm mirror]");
                // The strobe never latches.
                EXPECT_EQ_CTX(0u, drv.read16(cgm_base + CG::OFF_REG_UPDATE),
                              o.str() + " [strobe self-clear]");
            }
        }
    }
    std::cout << "  [PASS] CGM lock matrix: enable x commit strobe, both "
                 "mirrors\n";

    // AWM matrix: only the commit bit matters; the reported lock_detect is the
    // per-instance constant the firmware polls for (awm_0 == 7, awm_1 == 1).
    for (unsigned idx = 0; idx < 2; ++idx) {
        const uint64_t awm_base = (idx == 0) ? W::OFF_AWM_0 : W::OFF_AWM_1;
        const uint64_t st_off   = W::OFF_PLL_CNTL +
                                  ((idx == 0) ? PC::OFF_AWM_0_STATUS
                                              : PC::OFF_AWM_1_STATUS);
        const uint32_t lockval = (idx == 0) ? W::AWM0_LOCK_DETECT
                                            : W::AWM1_LOCK_DETECT;
        for (unsigned strobe : {0x0u, 0x1u, 0x2u, 0x3u}) {
            pulse_reset();
            std::ostringstream o;
            o << "awm_" << idx << " REG_UPDATE=0x" << std::hex << strobe
              << std::dec;
            drv.write32(awm_base + AW::GLOBAL_REG_UPDATE, strobe);

            const uint32_t exp = (strobe & W::REG_UPDATE_COMMIT) ? lockval : 0u;
            EXPECT_EQ_CTX(exp, drv.read16(st_off) & W::AWM_LOCK_DETECT_MASK,
                          o.str() + " [pll_cntl status]");
            EXPECT_EQ_CTX(exp,
                          (drv.read32(awm_base + AW::GLOBAL_LOCK_STATUS) >>
                           W::AWM_LOCK_DETECT_SHIFT) & W::AWM_LOCK_DETECT_MASK,
                          o.str() + " [awm mirror]");
            EXPECT_EQ_CTX(0u, drv.read32(awm_base + AW::GLOBAL_REG_UPDATE),
                          o.str() + " [strobe self-clear]");
        }
    }
    std::cout << "  [PASS] AWM lock matrix: both REG_UPDATE bits, both "
                 "mirrors\n";

    // The two AWM instances are independent: committing one must not move the
    // other's status.
    pulse_reset();
    drv.write32(W::OFF_AWM_0 + AW::GLOBAL_REG_UPDATE, 0x1u);
    EXPECT_EQ(W::AWM0_LOCK_DETECT,
              drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS) & 0x7u);
    EXPECT_EQ(0u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_1_STATUS) & 0x7u);
    drv.write32(W::OFF_AWM_1 + AW::GLOBAL_REG_UPDATE, 0x1u);
    EXPECT_EQ(W::AWM1_LOCK_DETECT,
              drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_1_STATUS) & 0x7u);
    EXPECT_EQ(W::AWM0_LOCK_DETECT,
              drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS) & 0x7u);
    std::cout << "  [PASS] AWM instances commit independently\n";

    // Observers see the post-lane-merge, pre-self-clear word.  A write that
    // touches only an untouched lane of the strobe word must therefore not be
    // able to re-commit from a stale bit.
    pulse_reset();
    {
        drv.write16(W::OFF_CGM_0 + CG::OFF_ENABLES, 0x1u);
        // Write the upper half of the REG_UPDATE word: the commit bit lives in
        // the lower half and reads back 0, so no lock may be asserted.
        uint16_t upper = 0xFFFFu;
        req r;
        r.cmd = tlm::TLM_WRITE_COMMAND;
        r.addr = W::OFF_CGM_0 + CG::OFF_REG_UPDATE + 2;
        r.len = 2;
        r.data = &upper;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, drv.send(r).status);
        EXPECT_EQ(0u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);

        // Same for a byte lane above the commit bit.
        uint8_t byte = 0xFFu;
        req b;
        b.cmd = tlm::TLM_WRITE_COMMAND;
        b.addr = W::OFF_CGM_0 + CG::OFF_REG_UPDATE + 1;
        b.len = 1;
        b.data = &byte;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, drv.send(b).status);
        EXPECT_EQ(0u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);

        // A byte write that does contain the commit bit still commits.
        byte = 0x1u;
        b.addr = W::OFF_CGM_0 + CG::OFF_REG_UPDATE;
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, drv.send(b).status);
        EXPECT_EQ(1u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);
    }
    std::cout << "  [PASS] observers cannot commit from a stale or untouched "
                 "lane\n";

    // Firmware-shaped sequence: program with 16-bit MMIO, strobe, busy-poll.
    pulse_reset();
    {
        drv.write16(W::OFF_CGM_0 + CG::OFF_ENABLES, 0x3u);
        drv.write16(W::OFF_CGM_0 + CG::OFF_FCW_INT, 20);
        drv.write16(W::OFF_CGM_0 + CG::OFF_REG_UPDATE, 0x1u);

        uint32_t lock = 0;
        int spins = 0;
        do {
            lock = drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u;
        } while (lock != 1u && ++spins < 1000);
        EXPECT_EQ(1u, lock);
        EXPECT_EQ(0, spins);  // the model locks on the strobe, with no settle
        EXPECT_EQ(20u, drv.read16(W::OFF_CGM_0 + CG::OFF_FCW_INT));
    }
    std::cout << "  [PASS] firmware CGM program + lock poll terminates "
                 "immediately\n";
}

// ---------------------------------------------------------------------------
// Architectural: annotated delay and CCI
// ---------------------------------------------------------------------------
void tb::t_timing()
{
    using W = smc::pll::pll_wrapper;
    std::cout << "\n--- Architectural: annotated delay and CCI ---\n";
    pulse_reset();

    auto broker = cci::cci_get_broker();
    const char* const handles[] = {
        "tb.dut.pll_cntl.access_delay_ns", "tb.dut.cgm_0.access_delay_ns",
        "tb.dut.cgm_1.access_delay_ns",    "tb.dut.awm_0.access_delay_ns",
        "tb.dut.awm_1.access_delay_ns",
    };
    const std::vector<child_win> kids = children();

    auto delay_of = [&](uint64_t addr) {
        uint32_t scratch = 0;
        req r;
        r.addr = addr;
        r.len = 4;
        r.data = &scratch;
        r.delay_in = sc_time(7, SC_NS);  // non-zero incoming delay
        const rsp s = drv.send(r);
        EXPECT_EQ(tlm::TLM_OK_RESPONSE, s.status);
        return s.delay_delta;
    };

    // Default: exactly one child delay is added; the wrapper itself adds none.
    for (std::size_t i = 0; i < kids.size(); ++i) {
        auto h = broker.get_param_handle(handles[i]);
        EXPECT_TRUE(h.is_valid());
        EXPECT_STREQ("1.0", h.get_cci_value().to_json());
        EXPECT_TIME_EQ_CTX(sc_time(1, SC_NS), delay_of(kids[i].base),
                           std::string(kids[i].name) + " default delay");
    }
    std::cout << "  [PASS] incoming delay preserved, exactly one child delay "
                 "added\n";

    // Live per-child mutation: only the mutated child's annotation changes.
    for (std::size_t i = 0; i < kids.size(); ++i) {
        auto h = broker.get_param_handle(handles[i]);
        const double newval = 2.0 + static_cast<double>(i);
        h.set_cci_value(cci::cci_value(newval));
        for (std::size_t j = 0; j < kids.size(); ++j) {
            const double expect_ns = (j <= i) ? 2.0 + static_cast<double>(j) : 1.0;
            std::ostringstream o;
            o << "after mutating " << kids[i].name << ", " << kids[j].name;
            EXPECT_TIME_EQ_CTX(sc_time(expect_ns, SC_NS),
                               delay_of(kids[j].base), o.str());
        }
    }
    std::cout << "  [PASS] per-child access_delay_ns mutates live and in "
                 "isolation\n";

    // Error and decode-miss paths annotate no delay at all.
    {
        uint32_t scratch = 0;
        const struct { uint64_t addr; unsigned len; const char* what; } bad[] = {
            {kDecodeGap,        4, "decode gap"},
            {0x1000,           4, "past window"},
            {W::OFF_CGM_0 + 2, 4, "misaligned"},
            {W::OFF_CGM_0,     3, "bad width"},
        };
        for (const auto& b : bad) {
            req r;
            r.addr = b.addr;
            r.len = b.len;
            r.data = &scratch;
            r.streaming_width = static_cast<int>(b.len);
            r.delay_in = sc_time(7, SC_NS);
            const rsp s = drv.send(r);
            EXPECT_TRUE(s.status != tlm::TLM_OK_RESPONSE);
            EXPECT_TIME_EQ_CTX(SC_ZERO_TIME, s.delay_delta,
                               std::string("no delay on ") + b.what);
        }
        // transport_dbg is untimed by construction.
        uint32_t v = 0;
        EXPECT_EQ(4u, drv.dbg(tlm::TLM_READ_COMMAND, W::OFF_CGM_0, 4, &v));
    }
    std::cout << "  [PASS] error, decode-miss and debug paths annotate no "
                 "delay\n";

    // Restore the defaults so later cases see the documented 1 ns.
    for (const char* h : handles) {
        broker.get_param_handle(h).set_cci_value(cci::cci_value(1.0));
    }
}

// ---------------------------------------------------------------------------
// Architectural: reset
// ---------------------------------------------------------------------------
void tb::t_reset()
{
    using W  = smc::pll::pll_wrapper;
    using PC = smc::pll::pll_cntl;
    using CG = smc::pll::cgm;
    using AW = smc::pll::awm;
    const std::vector<expect_reg>& mc = man_cntl;
    const std::vector<expect_reg>& mg = man_cgm;
    const std::vector<expect_reg>& ma = man_awm;

    std::cout << "\n--- Architectural: reset ---\n";

    // Dirty every writable register in every child (including the strobes and
    // the lock state they commit), then reset and require the complete reset
    // image back -- not a representative subset.
    pulse_reset();
    struct block { uint64_t base; const std::vector<expect_reg>* m; };
    const std::vector<block> blocks = {
        {W::OFF_PLL_CNTL, &mc}, {W::OFF_CGM_0, &mg}, {W::OFF_CGM_1, &mg},
        {W::OFF_AWM_0, &ma},    {W::OFF_AWM_1, &ma},
    };
    for (const block& b : blocks) {
        for (const expect_reg& r : *b.m) {
            if (r.wmask == 0u) continue;
            drv.write32(b.base + r.offset, 0xFFFFFFFFu);
        }
    }
    // Lock is set in both aggregate and mirror registers at this point.
    EXPECT_EQ(1u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);
    EXPECT_EQ(W::AWM0_LOCK_DETECT,
              drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS) & 0x7u);

    pulse_reset();
    check_reset_image(drv, W::OFF_PLL_CNTL, mc, "reset.pll_cntl");
    check_reset_image(drv, W::OFF_CGM_0, mg, "reset.cgm_0");
    check_reset_image(drv, W::OFF_CGM_1, mg, "reset.cgm_1");
    check_reset_image(drv, W::OFF_AWM_0, ma, "reset.awm_0");
    check_reset_image(drv, W::OFF_AWM_1, ma, "reset.awm_1");
    std::cout << "  [PASS] dirty-all then reset restores the complete reset "
                 "image\n";

    // Reset asserted in the same delta as a commit strobe: no stale lock may
    // survive into the reset state.
    {
        drv.write16(W::OFF_CGM_0 + CG::OFF_ENABLES, 0x1u);
        drv.write16(W::OFF_CGM_0 + CG::OFF_REG_UPDATE, 0x1u);
        EXPECT_EQ(1u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);

        rst_n.write(false);
        // Still the same delta: the reset SC_METHOD has not run yet.
        drv.write16(W::OFF_CGM_0 + CG::OFF_REG_UPDATE, 0x1u);
        drv.write32(W::OFF_AWM_0 + AW::GLOBAL_REG_UPDATE, 0x1u);
        wait(SC_ZERO_TIME);
        rst_n.write(true);
        settle();

        EXPECT_EQ(0u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_0_STATUS) & 0x1u);
        EXPECT_EQ(0u, drv.read16(W::OFF_CGM_0 + CG::OFF_CGM_STATUS) & 0x1u);
        EXPECT_EQ(0u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_AWM_0_STATUS) & 0x7u);
        EXPECT_EQ(0u, drv.read32(W::OFF_AWM_0 + AW::GLOBAL_LOCK_STATUS));
    }
    std::cout << "  [PASS] reset concurrent with a commit strobe leaves no "
                 "stale lock\n";

    // Repeated reset while traffic is in flight, and reprogramming after it.
    for (int i = 0; i < 3; ++i) {
        drv.write32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT, 0x3F3FFFFFu);
        pulse_reset();
        EXPECT_EQ(0u, drv.read32(W::OFF_PLL_CNTL + PC::OFF_AG_MUX_SELECT));
        drv.write16(W::OFF_CGM_1 + CG::OFF_ENABLES, 0x1u);
        drv.write16(W::OFF_CGM_1 + CG::OFF_REG_UPDATE, 0x1u);
        EXPECT_EQ(1u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_1_STATUS) & 0x1u);
    }
    pulse_reset();
    EXPECT_EQ(0u, drv.read16(W::OFF_PLL_CNTL + PC::OFF_CGM_1_STATUS) & 0x1u);
    std::cout << "  [PASS] repeated reset and reprogram cycles\n";
}

// ---------------------------------------------------------------------------
// Structural: model back doors.  These keep the debug infrastructure honest
// and are NOT evidence about PLL architectural behaviour.
// ---------------------------------------------------------------------------
void tb::t_structural()
{
    using W  = smc::pll::pll_wrapper;
    using PC = smc::pll::pll_cntl;
    using AW = smc::pll::awm;
    std::cout << "\n--- Structural: model back doors (not architectural) ---\n";
    pulse_reset();

    // peek/poke round-trip and miss paths.
    {
        uint32_t tmp = 0xA5A5A5A5u;
        EXPECT_TRUE(dut.awm_0().poke(AW::GLOBAL_LOCK_STATUS, 0x0038u));
        EXPECT_TRUE(dut.awm_0().peek(AW::GLOBAL_LOCK_STATUS, tmp));
        EXPECT_EQ(0x0038u, tmp);
        EXPECT_EQ(0x0038u, drv.read32(W::OFF_AWM_0 + AW::GLOBAL_LOCK_STATUS));

        EXPECT_TRUE(!dut.cntl().peek(0x1000, tmp));
        EXPECT_TRUE(!dut.cntl().poke(0x1000, 0x1u));
        EXPECT_TRUE(!dut.awm_0().peek(0xFFF0, tmp));
        EXPECT_TRUE(!dut.awm_0().poke(0xFFF0, 0x1u));
        EXPECT_TRUE(dut.cgm_0().peek(smc::pll::cgm::OFF_ENABLES, tmp));
        EXPECT_TRUE(dut.cgm_1().peek(smc::pll::cgm::OFF_ENABLES, tmp));
        EXPECT_TRUE(dut.awm_1().peek(AW::GLOBAL_LOCK_STATUS, tmp));
        EXPECT_EQ(smc::pll::pll_wrapper_cfg::WINDOW_SIZE, dut.window_size());
        EXPECT_EQ(0u, dut.base_addr());
        EXPECT_EQ(0u, dut.cntl().base_addr());
    }
    pulse_reset();

    // Per-register callback replacement, on hit and miss.
    {
        EXPECT_TRUE(!probe.install_write(
            0x1000, [](uint32_t cur, uint32_t) { return cur; }));
        EXPECT_TRUE(!probe.install_read(
            0x1000, [](uint32_t stored) { return stored; }));
        EXPECT_TRUE(probe.install_write(
            PC::OFF_AG_MUX_SELECT,
            [](uint32_t, uint32_t in) { return in & 0xFFu; }));
        probe_drv.write32(PC::OFF_AG_MUX_SELECT, 0xFFFFFFFFu);
        EXPECT_EQ(0xFFu, probe_drv.read32(PC::OFF_AG_MUX_SELECT));
        EXPECT_TRUE(probe.install_read(PC::OFF_AG_MUX_SELECT,
                                       [](uint32_t) { return 0xA5A5A5A5u; }));
        EXPECT_EQ(0xA5A5A5A5u, probe_drv.read32(PC::OFF_AG_MUX_SELECT));

        EXPECT_TRUE(!dut.awm_0().set_write_callback(
            0xFFF0, [](uint32_t cur, uint32_t) { return cur; }));
        EXPECT_TRUE(!dut.awm_0().set_read_callback(
            0xFFF0, [](uint32_t stored) { return stored; }));
    }
    std::cout << "  [PASS] peek/poke and callback replacement (structural)\n";

    // dump_state renders every composed child.
    {
        std::ostringstream oss;
        dut.dump_state(oss);
        const std::string s = oss.str();
        EXPECT_TRUE(s.find("pll_wrapper composed state") != std::string::npos);
        EXPECT_TRUE(s.find("reg_block state") != std::string::npos);
        EXPECT_TRUE(s.find("CGM_0_STATUS") != std::string::npos);
        EXPECT_TRUE(s.find("GLOBAL.LOCK_STATUS") != std::string::npos);
        EXPECT_TRUE(s.find("FREQUENCY5.FCW_INT0") != std::string::npos);
    }
    std::cout << "  [PASS] dump_state renders every child (structural)\n";
}

void tb::run()
{
    std::cout << "==== SMC pll_wrapper TB ====\n";

    pulse_reset();

    t_register_model();
    t_composed_map();
    t_bus_contract();
    t_sideband();
    t_debug_and_dmi();
    t_lock_policy();
    t_timing();
    t_reset();
    t_structural();

#ifdef PLL_UB_CANARY
    // Built only by `run_tests.sh --ubsan-canary`.  Proves the UBSan build
    // really does report undefined behaviour, so a clean --asan run means
    // something.  volatile keeps the shift out of the optimiser's hands.
    {
        volatile int shift = 33;
        volatile int value = 1;
        std::cout << "  [UB CANARY] " << (value << shift) << "\n";
    }
#endif

    if (g_failures == 0)
        std::cout << "\nALL TESTS PASSED\n";
    else
        std::cout << "\n" << g_failures << " FAILURE(S)\n";

    sc_core::sc_stop();
}

}  // namespace

int sc_main(int, char**)
{
    static cci_utils::consuming_broker cci_global_broker("GlobalBroker");
    cci::cci_register_broker(cci_global_broker);

    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
