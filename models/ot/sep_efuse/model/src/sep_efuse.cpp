#include "sep_efuse.h"
#include <functional>

static const std::vector<uint32_t> k_zero8(8, 0u);

// =============================================================================
// Constructor
// =============================================================================

sep_efuse_model::sep_efuse_model(sc_module_name n, int log_verbosity)
    : sep_efuse_base(n, 0x644)
    , verbosity("verbosity", log_verbosity)
    , lc_state("lc_state", 0u)
    , sboot_dis("sboot_dis", 0u)
    , transient_rma_en("transient_rma_en", 0u)
    , sip_dis_lo("sip_dis_lo", 0u)
    , sip_dis_hi("sip_dis_hi", 0u)
    , sys_dis_lo("sys_dis_lo", 0u)
    , sys_dis_hi("sys_dis_hi", 0u)
    , chiplet_pubk_revoke("chiplet_pubk_revoke", 0u)
    , status_rpt("status_rpt", 0u)
    , sep_rom_ctrl("sep_rom_ctrl", 0u)
    , sep_spi_ctrl_field_en("sep_spi_ctrl_field_en", 0u)
    , spi_discovery_ctrl("spi_discovery_ctrl", 0u)
    , spi_phy_dq_timing("spi_phy_dq_timing", 0u)
    , spi_phy_dqs_timing("spi_phy_dqs_timing", 0u)
    , spi_phy_gate_lpbk("spi_phy_gate_lpbk", 0u)
    , spi_phy_dll_slave("spi_phy_dll_slave", 0u)
    , spi_phy_dll_master("spi_phy_dll_master", 0u)
    , spi_phy_misc("spi_phy_misc", 0u)
    , spi_rb_valid_time("spi_rb_valid_time", 0u)
    , rma_sip_token_match("rma_sip_token_match", 0u)
    , rma_chiplet_token_match("rma_chiplet_token_match", 0u)
    , sec_disable_token_match("sec_disable_token_match", 0u)
    , rma_sip_token("rma_sip_token", k_zero8)
    , rma_chiplet_token("rma_chiplet_token", k_zero8)
    , class_key("class_key", k_zero8)
    , bl1_version("bl1_version", k_zero8)
    , bl2_version("bl2_version", k_zero8)
    , chiplet_uid("chiplet_uid", k_zero8)
    , sip_uid("sip_uid", k_zero8)
    , sys_uid("sys_uid", k_zero8)
    , sip_pubk("sip_pubk", k_zero8)
    , sys_pubk("sys_pubk", k_zero8)
    , public_key_0("public_key_0", k_zero8)
    , public_key_1("public_key_1", k_zero8)
    , m_locks_lo_val(0), m_locks_hi_val(0)
    , m_lc_state_val(0)
    , m_sip_dis_lo_val(0), m_sip_dis_hi_val(0)
    , m_sys_dis_lo_val(0), m_sys_dis_hi_val(0)
    , m_chiplet_pubk_revoke_val(0)
    , m_bl1_version_val{}
    , m_bl2_version_val{}
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);
    reset_all_registers();
    register_callbacks();
}

// =============================================================================
// Callback Registration
// =============================================================================

void sep_efuse_model::register_callbacks()
{
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_LOCKS_LO(v); }, LOCKS_LO.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_LOCKS_HI(v); }, LOCKS_HI.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_LC_STATE(v); }, LC_STATE.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_SIP_DIS_LO(v); }, SIP_DIS_LO.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_SIP_DIS_HI(v); }, SIP_DIS_HI.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_SYS_DIS_LO(v); }, SYS_DIS_LO.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_SYS_DIS_HI(v); }, SYS_DIS_HI.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_CHIPLET_PUBK_REVOKE(v); },
        CHIPLET_PUBK_REVOKE.offset);
    for (int i = 0; i < 8; i++) {
        memory.register_write_callback(
            [this, i](uint32_t v){ return handle_write_BL1_VERSION(i, v); },
            BL1_VERSION[i].offset);
        memory.register_write_callback(
            [this, i](uint32_t v){ return handle_write_BL2_VERSION(i, v); },
            BL2_VERSION[i].offset);
    }
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_EFUSE_WRITE_CTRL(v); },
        EFUSE_WRITE_CTRL.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_EFUSE_READ_CTRL(v); },
        EFUSE_READ_CTRL.offset);
}

// =============================================================================
// Fuse Load — called from end_of_elaboration
// =============================================================================

void sep_efuse_model::load_fuses()
{
    // Helper: safely read element i from a vector param (0 if out of range)
    auto vi = [](const std::vector<uint32_t>& v, int i) -> uint32_t {
        return (i < (int)v.size()) ? v[i] : 0u;
    };

    m_lc_state_val = lc_state.get_param_value();
    LC_STATE        = m_lc_state_val;

    SBOOT_DIS        = sboot_dis.get_param_value();
    TRANSIENT_RMA_EN = transient_rma_en.get_param_value();

    m_sip_dis_lo_val = sip_dis_lo.get_param_value();
    m_sip_dis_hi_val = sip_dis_hi.get_param_value();
    SIP_DIS_LO = m_sip_dis_lo_val;
    SIP_DIS_HI = m_sip_dis_hi_val;

    m_sys_dis_lo_val = sys_dis_lo.get_param_value();
    m_sys_dis_hi_val = sys_dis_hi.get_param_value();
    SYS_DIS_LO = m_sys_dis_lo_val;
    SYS_DIS_HI = m_sys_dis_hi_val;

    auto rma_sip_v     = rma_sip_token.get_param_value();
    auto rma_chiplet_v = rma_chiplet_token.get_param_value();
    auto class_key_v   = class_key.get_param_value();
    for (int i = 0; i < 8; i++) {
        RMA_SIP_TOKEN[i]     = vi(rma_sip_v, i);
        RMA_CHIPLET_TOKEN[i] = vi(rma_chiplet_v, i);
        CLASS_KEY[i]         = vi(class_key_v, i);
    }

    m_chiplet_pubk_revoke_val = chiplet_pubk_revoke.get_param_value();
    CHIPLET_PUBK_REVOKE = m_chiplet_pubk_revoke_val;

    auto bl1_v = bl1_version.get_param_value();
    auto bl2_v = bl2_version.get_param_value();
    for (int i = 0; i < 8; i++) {
        m_bl1_version_val[i] = vi(bl1_v, i);
        m_bl2_version_val[i] = vi(bl2_v, i);
        BL1_VERSION[i] = m_bl1_version_val[i];
        BL2_VERSION[i] = m_bl2_version_val[i];
    }

    auto chiplet_uid_v = chiplet_uid.get_param_value();
    auto sip_uid_v     = sip_uid.get_param_value();
    auto sys_uid_v     = sys_uid.get_param_value();
    auto sip_pubk_v    = sip_pubk.get_param_value();
    auto sys_pubk_v    = sys_pubk.get_param_value();
    auto pk0_v         = public_key_0.get_param_value();
    auto pk1_v         = public_key_1.get_param_value();
    for (int i = 0; i < 8; i++) {
        m_chiplet_uid_cache[i] = vi(chiplet_uid_v, i);
        CHIPLET_UID[i] = m_chiplet_uid_cache[i];
        SIP_UID[i]     = vi(sip_uid_v, i);
        SYS_UID[i]     = vi(sys_uid_v, i);
        SIP_PUBK[i]    = vi(sip_pubk_v, i);
        SYS_PUBK[i]    = vi(sys_pubk_v, i);
        PUBLIC_KEY_0[i]= vi(pk0_v, i);
        PUBLIC_KEY_1[i]= vi(pk1_v, i);
    }

    STATUS_RPT            = status_rpt.get_param_value();
    SEP_ROM_CTRL          = sep_rom_ctrl.get_param_value();
    SEP_SPI_CTRL_FIELD_EN = sep_spi_ctrl_field_en.get_param_value();
    SPI_DISCOVERY_CTRL    = spi_discovery_ctrl.get_param_value();
    SPI_PHY_DQ_TIMING     = spi_phy_dq_timing.get_param_value();
    SPI_PHY_DQS_TIMING    = spi_phy_dqs_timing.get_param_value();
    SPI_PHY_GATE_LPBK     = spi_phy_gate_lpbk.get_param_value();
    SPI_PHY_DLL_SLAVE     = spi_phy_dll_slave.get_param_value();
    SPI_PHY_DLL_MASTER    = spi_phy_dll_master.get_param_value();
    SPI_PHY_MISC          = spi_phy_misc.get_param_value();
    SPI_RB_VALID_TIME     = spi_rb_valid_time.get_param_value();

    RMA_SIP_TOKEN_MATCH     = rma_sip_token_match.get_param_value();
    RMA_CHIPLET_TOKEN_MATCH = rma_chiplet_token_match.get_param_value();
    SEC_DISABLE_TOKEN_MATCH = sec_disable_token_match.get_param_value();
}

void sep_efuse_model::end_of_elaboration()
{
    load_fuses();
}

// =============================================================================
// WOSET Callback Handlers
// =============================================================================

bool sep_efuse_model::handle_write_LOCKS_LO(uint32_t value)
{
    m_locks_lo_val |= value;
    LOCKS_LO = m_locks_lo_val;
    return true;
}

bool sep_efuse_model::handle_write_LOCKS_HI(uint32_t value)
{
    m_locks_hi_val |= value;
    LOCKS_HI = m_locks_hi_val;
    return true;
}

bool sep_efuse_model::handle_write_LC_STATE(uint32_t value)
{
    m_lc_state_val |= value;
    LC_STATE = m_lc_state_val;
    return true;
}

bool sep_efuse_model::handle_write_SIP_DIS_LO(uint32_t value)
{
    m_sip_dis_lo_val |= value;
    SIP_DIS_LO = m_sip_dis_lo_val;
    return true;
}

bool sep_efuse_model::handle_write_SIP_DIS_HI(uint32_t value)
{
    m_sip_dis_hi_val |= value;
    SIP_DIS_HI = m_sip_dis_hi_val;
    return true;
}

bool sep_efuse_model::handle_write_SYS_DIS_LO(uint32_t value)
{
    m_sys_dis_lo_val |= value;
    SYS_DIS_LO = m_sys_dis_lo_val;
    return true;
}

bool sep_efuse_model::handle_write_SYS_DIS_HI(uint32_t value)
{
    m_sys_dis_hi_val |= value;
    SYS_DIS_HI = m_sys_dis_hi_val;
    return true;
}

bool sep_efuse_model::handle_write_CHIPLET_PUBK_REVOKE(uint32_t value)
{
    m_chiplet_pubk_revoke_val |= value;
    CHIPLET_PUBK_REVOKE = m_chiplet_pubk_revoke_val;
    return true;
}

bool sep_efuse_model::handle_write_BL1_VERSION(int idx, uint32_t value)
{
    m_bl1_version_val[idx] |= value;
    BL1_VERSION[idx] = m_bl1_version_val[idx];
    return true;
}

bool sep_efuse_model::handle_write_BL2_VERSION(int idx, uint32_t value)
{
    m_bl2_version_val[idx] |= value;
    BL2_VERSION[idx] = m_bl2_version_val[idx];
    return true;
}

// =============================================================================
// Interface-Ctrl Completion Stubs
// Instantly reflect done=1 when FW pulses write_go/read_go, so polling loops
// exit on the very next read without hanging.
// =============================================================================

bool sep_efuse_model::handle_write_EFUSE_WRITE_CTRL(uint32_t value)
{
    if (value & (1u << 17)) {   // efuse_write_go pulsed
        value &= ~(1u << 17);   // clear singlepulse bit
        value &= ~(1u << 24);   // write_busy = 0
        value |=  (1u << 25);   // write_done = 1
        value &= ~(1u << 26);   // write_status = 0 (pass)
    }
    EFUSE_WRITE_CTRL = value;
    return true;
}

bool sep_efuse_model::handle_write_EFUSE_READ_CTRL(uint32_t value)
{
    if (value & (1u << 16)) {   // efuse_read_go pulsed
        value &= ~(1u << 16);   // clear singlepulse bit
        value &= ~(1u << 24);   // read_busy = 0
        value |=  (1u << 25);   // read_done = 1
        value &= ~(1u << 26);   // read_status = 0 (pass)
    }
    EFUSE_READ_CTRL = value;
    return true;
}
