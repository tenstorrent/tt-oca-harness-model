// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "lifecycle_ctrl.h"

// Bit-range masks within the 64-bit FEAT_CTRL vector (from sep_lifecycle_ctrl.sv struct layout)
static constexpr uint64_t DEBUG_BITS_MASK = 0x00000000FFFFFFFFULL; // [31:0]  sep_debug..debug_reserved
static constexpr uint64_t TEST_BITS_MASK  = 0x0000FFFF00000000ULL; // [47:32] fuse_test..test_reserved
static constexpr uint64_t FUNC_BITS_MASK  = 0xFFFF000000000000ULL; // [63:48] func_reserved

lifecycle_ctrl_model::lifecycle_ctrl_model(sc_module_name n, int log_verbosity)
    : lifecycle_ctrl_base(n, 0x0018)
    , verbosity("verbosity", log_verbosity)
    , lc_state("lc_state", 0u)
    , sip_dis_lo("sip_dis_lo", 0u)
    , sip_dis_hi("sip_dis_hi", 0u)
    , sys_dis_lo("sys_dis_lo", 0u)
    , sys_dis_hi("sys_dis_hi", 0u)
    , security_disable("security_disable", false)
    // Defaults false because the RTL flop resets to 0 and then latches the test_en
    // strap, which a functional part does not assert. True models a part strapped for
    // test, where the DFT features in FEAT_CTRL[47:32] survive.
    , secure_tm("secure_tm", false)
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    reset_all_registers();

    memory.register_write_callback(
        [this](DT value) -> bool { return on_demote_1_write(value); },
        DEMOTE_1.offset
    );
    memory.register_write_callback(
        [this](DT value) -> bool { return on_demote_2_write(value); },
        DEMOTE_2.offset
    );
    memory.register_write_callback(
        [this](DT value) -> bool { return on_demote_hi_write(DEMOTE_1_HI, value); },
        DEMOTE_1_HI.offset
    );
    memory.register_write_callback(
        [this](DT value) -> bool { return on_demote_hi_write(DEMOTE_2_HI, value); },
        DEMOTE_2_HI.offset
    );
}

void lifecycle_ctrl_model::end_of_elaboration()
{
    // Seed the input bundle from the parameters. On a platform the eFuse model overrides
    // it via set_inputs() before simulation starts, because the shadow registers are the
    // real source; the parameters are what a standalone testbench configures.
    m_in.lc_state_code    = lc_state_encode(lc_state.get_param_value());
    m_in.sip_dis          = (static_cast<uint64_t>(sip_dis_hi.get_param_value()) << 32)
                          |  sip_dis_lo.get_param_value();
    m_in.sys_dis          = (static_cast<uint64_t>(sys_dis_hi.get_param_value()) << 32)
                          |  sys_dis_lo.get_param_value();
    m_in.security_disable = security_disable.get_param_value();
    m_in.secure_tm        = secure_tm.get_param_value();
    compute_feat_ctrl();
}

void lifecycle_ctrl_model::set_inputs(const lc_inputs &in)
{
    m_in = in;
    compute_feat_ctrl();
}

// Mirrors the combinational always_comb block in sep_lifecycle_ctrl.sv.
void lifecycle_ctrl_model::compute_feat_ctrl()
{
    const uint64_t sip_dis = m_in.sip_dis;
    const uint64_t sys_dis = m_in.sys_dis;
    const bool     demote1 = (static_cast<uint32_t>(DEMOTE_1) & 0x1) != 0;
    const bool     demote2 = (static_cast<uint32_t>(DEMOTE_2) & 0x1) != 0;

    // Decode the differential LC state. A pair that is not {~raw, raw} means the state
    // has been corrupted, and the RTL's response is to disable everything.
    const bool sigint = !lc_state_code_valid(m_in.lc_state_code);
    if (sigint && !m_lc_sigint_err)
        REG_REPORT(WARNING, "LC_CTRL", "LC_STATE is not a valid differential code — "
                                        "raising lc_sigint_err and zeroing FEAT_CTRL");
    m_lc_sigint_err = sigint;

    const uint8_t lc = static_cast<uint8_t>(m_in.lc_state_code & 0xFu);

    uint64_t feat_ctrl = 0;

    // security_disable is applied after the state chain in the RTL
    // (feat_ctrl_sec_disable), so it overrides even the sigint fail-safe. Keeping that
    // order matters: a part with security disabled stays open regardless of LC state.
    if (m_in.security_disable) {
        feat_ctrl = 0xFFFFFFFFFFFFFFFFULL;

    } else if (sigint) {
        feat_ctrl = 0;

    } else if (lc == LC_STATE_TEST_DEV) {                   // 4'b0000
        feat_ctrl = ~(sip_dis | sys_dis);
        if (demote1 || demote2)
            feat_ctrl |= DEBUG_BITS_MASK;                   // re-enable all debug bits

    } else if (lc == LC_STATE_PROD) {                       // 4'b0001
        feat_ctrl = ~(sip_dis | sys_dis) & FUNC_BITS_MASK; // base: func only, debug+test = 0
        if (demote1) {                                      // PROD_DBG_1: full debug, ~SIP_DIS.func
            feat_ctrl  = ~sip_dis & FUNC_BITS_MASK;
            feat_ctrl |= DEBUG_BITS_MASK;
        } else if (demote2) {                               // PROD_DBG_2: full debug, ~SYS_DIS.func
            feat_ctrl  = ~sys_dis & FUNC_BITS_MASK;
            feat_ctrl |= DEBUG_BITS_MASK;
        }

    } else if (lc == LC_STATE_PROD_END) {                   // 4'b1000
        feat_ctrl = ~(sip_dis | sys_dis) & FUNC_BITS_MASK; // func only, same as PROD base

    } else if ((lc & LC_STATE_RANGE_MASK) == LC_STATE_RMA_SIP_BASE) {     // 4'b001x — RMA_SIP
        feat_ctrl = ~sip_dis;

    } else if ((lc & LC_STATE_RANGE_MASK) == LC_STATE_RMA_CHIPLET_BASE) { // 4'b011x — RMA_CHIPLET
        feat_ctrl = 0xFFFFFFFFFFFFFFFFULL;

    } else {                                                // INVALID (0x4, 0x5, 0x9-0xF)
        feat_ctrl = 0;
    }

    if (!m_in.secure_tm)
        feat_ctrl &= ~TEST_BITS_MASK;                       // gate test bits [47:32]

    FEAT_CTRL_LO = static_cast<uint32_t>(feat_ctrl & 0xFFFFFFFF);
    FEAT_CTRL_HI = static_cast<uint32_t>(feat_ctrl >> 32);

    if (m_feat_ctrl_change_cb)
        m_feat_ctrl_change_cb();
}

/*
 * Low word of a DEMOTE register: demote[0], lock[1], rsvd[31:2].
 *
 * All three fields are onwrite = woset, so bits only ever set. swwe = ~lock applies to
 * the demote field alone -- lock and rsvd carry no swwe and stay writable while locked,
 * though rewriting a woset lock is a no-op anyway.
 */
bool lifecycle_ctrl_model::on_demote_write(lc_ctrl::DEMOTE_type<32> &reg,
                                           const char *name, DT value)
{
    const uint32_t current = static_cast<uint32_t>(reg);
    const bool     locked  = ((current >> 1) & 0x1u) != 0;

    if (locked && (value & 0x1u))
        REG_REPORT(WARNING, "LC_CTRL", std::string(name) +
                    " demote bit ignored — lock bit is set");

    uint32_t next = current | (value & ~0x1u);
    if (!locked)
        next |= value & 0x1u;

    reg = next;
    compute_feat_ctrl();
    return true;
}

bool lifecycle_ctrl_model::on_demote_1_write(DT value)
{
    return on_demote_write(DEMOTE_1, "DEMOTE_1", value);
}

bool lifecycle_ctrl_model::on_demote_2_write(DT value)
{
    return on_demote_write(DEMOTE_2, "DEMOTE_2", value);
}

// Upper word, rsvd[63:32]: woset, and no swwe, so the lock does not reach it.
bool lifecycle_ctrl_model::on_demote_hi_write(lc_ctrl::DEMOTE_HI_type<32> &reg, DT value)
{
    reg = static_cast<uint32_t>(reg) | value;
    return true;
}
