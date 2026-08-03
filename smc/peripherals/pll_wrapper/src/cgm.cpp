// SPDX-License-Identifier: Apache-2.0
/**
 * @file cgm.cpp
 * @brief CGM register table (from cgm.rdl) + model construction.
 */

#include "cgm.h"

namespace smc {
namespace pll {

namespace {

// Register table transcribed from cgm.rdl.  For RW fields rmask==wmask (the
// field bits); RO status registers have wmask==0; self-clearing strobes carry
// a non-zero self_clear_mask so a written 1 never latches.
//
//                offset  name                 reset    rmask    wmask    sc
constexpr reg_spec kCgmRegs[] = {
    {0x00, "ENABLES",            0x0004u, 0x0007u, 0x0007u, 0x0000u},
    {0x04, "FCW_INT",            0x0010u, 0x00FFu, 0x00FFu, 0x0000u},
    {0x08, "FCW_FRAC",           0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x0C, "PREDIV",             0x0000u, 0x0003u, 0x0003u, 0x0000u},
    {0x10, "POSTDIV_ARRAY_0",    0x0000u, 0x0FFFu, 0x0FFFu, 0x0000u},
    {0x18, "POSTDIV_CONFIG",     0x0000u, 0x00FFu, 0x00FFu, 0x0000u},
    {0x1C, "LOCK_CONFIG",        0x0000u, 0x007Fu, 0x007Fu, 0x0000u},
    {0x20, "REG_UPDATE",         0x0000u, 0x0001u, 0x0001u, 0x0001u},
    {0x38, "OPEN_LOOP",          0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x3C, "LOCK_MONITOR",       0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x40, "SAMPLE_STROBE",      0x0000u, 0x0001u, 0x0001u, 0x0001u},
    {0x44, "DM0_READ",           0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x48, "DM1_READ",           0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x4C, "CGM_STATUS",         0x0000u, 0x0007u, 0x0000u, 0x0000u},
    {0x50, "DM2_READ",           0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x54, "DM3_READ",           0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x58, "DCO_CODE_INST_READ", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x5C, "MONITOR_CONFIG",     0x0000u, 0x0001u, 0x0001u, 0x0000u},
    {0x60, "FREQ_MONITOR_CONFIG",0x0000u, 0x007Fu, 0x007Fu, 0x0000u},
    {0x64, "FREQ_MONITOR_COUNT", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x6C, "DM0_CONFIG",         0x0000u, 0x007Fu, 0x007Fu, 0x0000u},
    {0x8C, "FCW_INC",            0x0000u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x90, "SSC_HALF_PERIOD",    0x0000u, 0xFFFFu, 0xFFFFu, 0x0000u},
};

}  // namespace

cgm::cgm(sc_core::sc_module_name name, cgm_cfg cfg)
    : reg_block(name, kCgmRegs, sizeof(kCgmRegs) / sizeof(kCgmRegs[0]),
                cgm_cfg::WINDOW_SIZE, cfg.base_addr, cfg.access_delay_ns)
{
}

}  // namespace pll
}  // namespace smc
