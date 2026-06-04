#pragma once
#include "efuse_base.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <vector>

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

class efuse_model : public efuse_base
{
public:
    SC_HAS_PROCESS(efuse_model);

    efuse_model(sc_module_name n,
                    int log_verbosity = CSML_DEFAULT_VERBOSITY);

    void end_of_elaboration() override;

    // OTP data accessors for other models (e.g. keymgr_tt).
    // Safe to call after end_of_elaboration().
    uint32_t        get_lc_state()    const { return m_lc_state_val; }
    const uint32_t* get_chiplet_uid() const { return m_chiplet_uid_cache; }

    // -----------------------------------------------------------------------
    // CCI parameters — all configurable via ini file, no recompile needed
    // -----------------------------------------------------------------------
    CsmlLogger      logger;
    csml_param<int> verbosity;

    // Scalars
    csml_param<uint32_t> locks_lo;
    csml_param<uint32_t> lc_state;
    csml_param<uint32_t> sboot_dis;
    csml_param<uint32_t> transient_rma_en;
    csml_param<uint32_t> sip_dis_lo;
    csml_param<uint32_t> sip_dis_hi;
    csml_param<uint32_t> sys_dis_lo;
    csml_param<uint32_t> sys_dis_hi;
    csml_param<uint32_t> chiplet_pubk_revoke;
    csml_param<uint32_t> status_rpt;
    csml_param<uint32_t> sep_rom_ctrl;
    csml_param<uint32_t> sep_spi_ctrl_field_en;
    csml_param<uint32_t> spi_discovery_ctrl;
    csml_param<uint32_t> spi_phy_dq_timing;
    csml_param<uint32_t> spi_phy_dqs_timing;
    csml_param<uint32_t> spi_phy_gate_lpbk;
    csml_param<uint32_t> spi_phy_dll_slave;
    csml_param<uint32_t> spi_phy_dll_master;
    csml_param<uint32_t> spi_phy_misc;
    csml_param<uint32_t> spi_rb_valid_time;
    csml_param<uint32_t> rma_sip_token_match;
    csml_param<uint32_t> rma_chiplet_token_match;
    csml_param<uint32_t> sec_disable_token_match;

    // Arrays (256-bit = 8 × 32-bit words; ini: JSON array [w0, w1, ..., w7])
    csml_param<std::vector<uint32_t>> rma_sip_token;
    csml_param<std::vector<uint32_t>> rma_chiplet_token;
    csml_param<std::vector<uint32_t>> class_key;
    csml_param<std::vector<uint32_t>> bl1_version;
    csml_param<std::vector<uint32_t>> bl2_version;
    csml_param<std::vector<uint32_t>> chiplet_uid;
    csml_param<std::vector<uint32_t>> sip_uid;
    csml_param<std::vector<uint32_t>> sys_uid;
    csml_param<std::vector<uint32_t>> sip_pubk;
    csml_param<std::vector<uint32_t>> sys_pubk;
    csml_param<std::vector<uint32_t>> public_key_0;
    csml_param<std::vector<uint32_t>> public_key_1;

private:
    uint32_t m_chiplet_uid_cache[8] = {};  // stable pointer for get_chiplet_uid()

    // WOSET shadow values — accumulate set bits across firmware writes
    uint32_t m_locks_lo_val;
    uint32_t m_locks_hi_val;
    uint32_t m_lc_state_val;
    uint32_t m_sip_dis_lo_val;
    uint32_t m_sip_dis_hi_val;
    uint32_t m_sys_dis_lo_val;
    uint32_t m_sys_dis_hi_val;
    uint32_t m_chiplet_pubk_revoke_val;
    uint32_t m_bl1_version_val[8];
    uint32_t m_bl2_version_val[8];

    void register_callbacks();
    void load_fuses();

    // WOSET callback handlers
    bool handle_write_LOCKS_LO(uint32_t value);
    bool handle_write_LOCKS_HI(uint32_t value);
    bool handle_write_LC_STATE(uint32_t value);
    bool handle_write_SIP_DIS_LO(uint32_t value);
    bool handle_write_SIP_DIS_HI(uint32_t value);
    bool handle_write_SYS_DIS_LO(uint32_t value);
    bool handle_write_SYS_DIS_HI(uint32_t value);
    bool handle_write_CHIPLET_PUBK_REVOKE(uint32_t value);
    bool handle_write_BL1_VERSION(int idx, uint32_t value);
    bool handle_write_BL2_VERSION(int idx, uint32_t value);

    // Interface-ctrl completion stubs
    bool handle_write_EFUSE_WRITE_CTRL(uint32_t value);
    bool handle_write_EFUSE_READ_CTRL(uint32_t value);
};
