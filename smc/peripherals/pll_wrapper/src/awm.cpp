// SPDX-License-Identifier: Apache-2.0
/**
 * @file awm.cpp
 * @brief AWM composite register table (from awm.rdl) + model construction.
 *
 * The GLOBAL / FREQUENCY / CGM sub-block layouts are transcribed once each,
 * then tiled at their instance base offsets to build the full awm register
 * table (matching the awm.rdl top-level address map).
 */

#include "awm.h"

#include <string>
#include <vector>

namespace smc {
namespace pll {

namespace {

// Layout of one register within a sub-block (offset relative to sub-block base).
struct sub_reg {
    uint32_t    offset;
    const char* name;
    uint32_t    reset;
    uint32_t    rmask;
    uint32_t    wmask;
    uint32_t    self_clear_mask;
};

// -- GLOBAL sub-block (awm.rdl addrmap GLOBAL, offsets 0x00..0xA8) -----------
constexpr sub_reg kGlobal[] = {
    {0x00, "RESOURCE_CONFIGURATION_ENABLES", 0x0300u, 0x1FFFu, 0x1FFFu, 0x0000u},
    {0x04, "DYNAMIC_SCHEME_0",  0x0000u, 0x0007u, 0x0007u, 0x0000u},
    {0x08, "DYNAMIC_SCHEME_1",  0x000Fu, 0x00FFu, 0x00FFu, 0x0000u},
    {0x0C, "DYNAMIC_SCHEME_2",  0xFFFFu, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x10, "DYNAMIC_SCHEME_3",  0xFFFFu, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x14, "DYNAMIC_SCHEME_4",  0xFFFFu, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x18, "DYNAMIC_SCHEME_5",  0x0000u, 0x01FFu, 0x01FFu, 0x0000u},
    {0x1C, "FCW_INT_BOUND",     0x1428u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x20, "FCW_FRAC_UPPERBOUND", 0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x24, "FCW_FRAC_LOWERBOUND", 0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x28, "REG_UPDATE",        0x0000u, 0x0000u, 0x0003u, 0x0003u},
    {0x34, "MEAS_CONFIG",       0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x38, "MEAS_DURATION0",    0x0000u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x3C, "MEAS_DURATION1",    0x0000u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x40, "MEAS_STATUS",       0x0000u, 0x001Fu, 0x0000u, 0x0000u},
    {0x44, "EXT_FREQ",          0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x48, "EXT_REF",           0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x4C, "EXT_DROOP_DUR0",    0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x50, "EXT_DROOP_DUR1",    0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x54, "EXT_DROOP_DUR2",    0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x58, "EXT_DROOP_DUR3",    0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x5C, "EXT_DROOP_TRAN01",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x60, "EXT_DROOP_TRAN23",  0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x64, "EXT_FLOOR_LOOPS",   0x0000u, 0x00FFu, 0x0000u, 0x0000u},
    {0x68, "EXT_FLOOR1_DURATION0", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x6C, "EXT_FLOOR2_DURATION0", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x70, "EXT_FLOOR3_DURATION0", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x74, "EXT_FLOOR1_DURATION1", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x78, "EXT_FLOOR2_DURATION1", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x7C, "EXT_FLOOR3_DURATION1", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x80, "EXT_FLOOR1_DURATION2", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x84, "EXT_FLOOR2_DURATION2", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x88, "EXT_FLOOR3_DURATION2", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x8C, "EXT_SAFETY_DURATION0", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x90, "EXT_SAFETY_DURATION1", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x94, "EXT_SAFETY_DURATION2", 0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0x98, "LOCK_STATUS",       0x0000u, 0x07FFu, 0x0007u, 0x0000u},
    {0x9C, "LOCK_MONITOR_0",    0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0xA0, "LOCK_MONITOR_1",    0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0xA4, "LOCK_MONITOR_2",    0x0000u, 0xFFFFu, 0x0000u, 0x0000u},
    {0xA8, "DROOP_PAUSE",       0x0000u, 0x0007u, 0x0007u, 0x0000u},
};

// -- FREQUENCYn sub-block (awm.rdl addrmap FREQUENCY0, offsets 0x00..0x1C) ---
constexpr sub_reg kFrequency[] = {
    {0x00, "FCW_INT0",         0x1010u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x04, "FCW_INT1",         0x1010u, 0xFFFFu, 0xFFFFu, 0x0000u},
    {0x08, "FCW_FRAC",         0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x0C, "FCW_FRAC1",        0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x10, "FCW_FRAC2",        0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x14, "FCW_FRAC3",        0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x18, "PREDIV",           0x0000u, 0x3FFFu, 0x3FFFu, 0x0000u},
    {0x1C, "FREQ_ACQ_ENABLES", 0x0001u, 0x0001u, 0x0001u, 0x0000u},
};

// -- CGMn sub-block (awm.rdl addrmap CGM0, offsets 0x00..0x60) ---------------
constexpr sub_reg kCgm[] = {
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

// Tile one sub-block table at @p base with names prefixed by @p prefix, and
// append the resulting reg_spec entries into @p out.  The `name` C-strings are
// interned in a static store so they outlive the module (reg_spec keeps a raw
// pointer, and the map only copies the offset key).
void tile(std::vector<reg_spec>& out,
          std::vector<std::string>& name_store,
          const sub_reg* sub, std::size_t n,
          uint64_t base, const std::string& prefix)
{
    for (std::size_t i = 0; i < n; ++i) {
        name_store.push_back(prefix + sub[i].name);
    }
    // name_store may reallocate above; take pointers only after it is stable.
    const std::size_t first = name_store.size() - n;
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(reg_spec{
            static_cast<uint32_t>(base + sub[i].offset),
            name_store[first + i].c_str(),
            sub[i].reset, sub[i].rmask, sub[i].wmask, sub[i].self_clear_mask});
    }
}

// Build (once) the full awm register table by tiling GLOBAL + 6x FREQUENCY +
// 3x CGM at their instance bases.  Returned by reference so it outlives the
// module construction (reg_block copies each entry during build()).
const std::vector<reg_spec>& awm_table()
{
    static std::vector<std::string> name_store;
    static std::vector<reg_spec>    table = [] {
        std::vector<reg_spec> t;
        // Reserve so the c_str() pointers taken in tile() stay valid.
        name_store.reserve(256);
        t.reserve(128);

        tile(t, name_store, kGlobal, sizeof(kGlobal) / sizeof(kGlobal[0]),
             awm::OFF_GLOBAL, "GLOBAL.");

        for (unsigned f = 0; f < 6; ++f) {
            tile(t, name_store, kFrequency,
                 sizeof(kFrequency) / sizeof(kFrequency[0]),
                 awm::frequency_base(f),
                 "FREQUENCY" + std::to_string(f) + ".");
        }

        const uint64_t cgm_bases[3] = {awm::OFF_CGM0, awm::OFF_CGM1,
                                       awm::OFF_CGM2};
        for (unsigned c = 0; c < 3; ++c) {
            tile(t, name_store, kCgm, sizeof(kCgm) / sizeof(kCgm[0]),
                 cgm_bases[c], "CGM" + std::to_string(c) + ".");
        }
        return t;
    }();
    return table;
}

}  // namespace

awm::awm(sc_core::sc_module_name name, awm_cfg cfg)
    : reg_block(name, awm_table().data(), awm_table().size(),
                awm_cfg::WINDOW_SIZE, cfg.base_addr, cfg.access_delay_ns)
{
}

}  // namespace pll
}  // namespace smc
