#pragma once
#include "lifecycle_ctrl_base.h"
#include "csml_logger.h"
#include "csml_parameter.h"

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

class lifecycle_ctrl_model : public lifecycle_ctrl_base
{
public:
    SC_HAS_PROCESS(lifecycle_ctrl_model);

    // LC state encoding (4-bit, from fuse_map.LC_STATE)
    static constexpr uint32_t LC_STATE_TEST_DEV    = 0x0;  // 4'b0000
    static constexpr uint32_t LC_STATE_PROD        = 0x1;  // 4'b0001 (PROD_DBG_1/2 when DEMOTE_1/2=1)
    static constexpr uint32_t LC_STATE_PROD_END    = 0x8;  // 4'b1000
    // Range states — matched via mask: (lc_state & LC_STATE_RANGE_MASK) == LC_STATE_RMA_*_BASE
    static constexpr uint32_t LC_STATE_RANGE_MASK  = 0xE;
    static constexpr uint32_t LC_STATE_RMA_SIP_BASE     = 0x2;  // 4'b001x — 0x2, 0x3
    static constexpr uint32_t LC_STATE_RMA_CHIPLET_BASE = 0x6;  // 4'b011x — 0x6, 0x7
    // Any value not matching the above encodings is INVALID (FEAT_CTRL=0)

    lifecycle_ctrl_model(sc_module_name n,
                  int log_verbosity = CSML_DEFAULT_VERBOSITY);

    void end_of_elaboration() override;

    /// Return live demotion state for KM KDF input.
    /// Bits[1:0] = demote_1_value (domain-1, written by BL1 firmware).
    /// Bits[3:2] = demote_2_value (domain-2, written by BL2 firmware).
    uint32_t get_demote_state() const {
        uint8_t d1 = static_cast<uint32_t>(DEMOTE_1) & 0x1u;
        uint8_t d2 = static_cast<uint32_t>(DEMOTE_2) & 0x1u;
        return (d2 << 2) | d1;
    }

    // -----------------------------------------------------------------------
    // CCI parameters — all configurable via ini file, no recompile needed
    // -----------------------------------------------------------------------
    CsmlLogger           logger;
    csml_param<int>      verbosity;
    csml_param<uint32_t> lc_state;        ///< 4-bit LC state (default: 0x0 = TEST_DEV)
    csml_param<uint32_t> sip_dis_lo;      ///< SIP feature disable mask [31:0]
    csml_param<uint32_t> sip_dis_hi;      ///< SIP feature disable mask [63:32]
    csml_param<uint32_t> sys_dis_lo;      ///< SYS feature disable mask [31:0]
    csml_param<uint32_t> sys_dis_hi;      ///< SYS feature disable mask [63:32]
    csml_param<bool>     security_disable; ///< When true, forces FEAT_CTRL=all-ones
    csml_param<bool>     secure_tm;        ///< When false, clears test bits [47:32] in FEAT_CTRL

private:
    void compute_feat_ctrl();

    bool on_demote_1_write(DT value);
    bool on_demote_2_write(DT value);
};
