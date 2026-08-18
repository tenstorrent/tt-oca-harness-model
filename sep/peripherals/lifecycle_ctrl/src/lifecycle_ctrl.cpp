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
    , secure_tm("secure_tm", true)
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
}

void lifecycle_ctrl_model::end_of_elaboration()
{
    // Seed from the parameter. On a full platform the eFuse model overrides this via
    // set_lc_state() before simulation starts, because the fuse is the real source;
    // the parameter is what a standalone peripheral testbench uses.
    m_lc_state = lc_state.get_param_value() & 0xFu;
    compute_feat_ctrl();
}

void lifecycle_ctrl_model::set_lc_state(uint32_t raw)
{
    // sep_lifecycle_ctrl.sv takes lc_state from the eFuse shadow registers, so on a
    // platform this is how the value arrives and FEAT_CTRL is recomputed from it.
    m_lc_state = raw & 0xFu;
    compute_feat_ctrl();
}

// Mirrors the combinational always_comb block in sep_lifecycle_ctrl.sv.
void lifecycle_ctrl_model::compute_feat_ctrl()
{
    uint64_t sip_dis = ((uint64_t)sip_dis_hi.get_param_value() << 32) | sip_dis_lo.get_param_value();
    uint64_t sys_dis = ((uint64_t)sys_dis_hi.get_param_value() << 32) | sys_dis_lo.get_param_value();
    bool     demote1 = (static_cast<uint32_t>(DEMOTE_1) & 0x1) != 0;
    bool     demote2 = (static_cast<uint32_t>(DEMOTE_2) & 0x1) != 0;
    uint8_t  lc      = static_cast<uint8_t>(m_lc_state);

    uint64_t feat_ctrl = 0;

    if (security_disable.get_param_value()) {
        feat_ctrl = 0xFFFFFFFFFFFFFFFFULL;

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

    if (!secure_tm.get_param_value())
        feat_ctrl &= ~TEST_BITS_MASK;                       // gate test bits [47:32]

    FEAT_CTRL_LO = static_cast<uint32_t>(feat_ctrl & 0xFFFFFFFF);
    FEAT_CTRL_HI = static_cast<uint32_t>(feat_ctrl >> 32);
}

bool lifecycle_ctrl_model::on_demote_1_write(DT value)
{
    uint32_t current = static_cast<uint32_t>(DEMOTE_1);
    bool     locked  = (current >> 1) & 0x1;

    if (locked) {
        CSML_REPORT(WARNING, "LC_CTRL", "DEMOTE_1 write ignored — lock bit is set");
        return true;
    }

    if (value & 0x1) current |= 0x1;   // W1S demote (swwe = ~lock, checked above)
    if (value & 0x2) current |= 0x2;   // W1S lock

    DEMOTE_1 = current;
    compute_feat_ctrl();
    return true;
}

bool lifecycle_ctrl_model::on_demote_2_write(DT value)
{
    uint32_t current = static_cast<uint32_t>(DEMOTE_2);
    bool     locked  = (current >> 1) & 0x1;

    if (locked) {
        CSML_REPORT(WARNING, "LC_CTRL", "DEMOTE_2 write ignored — lock bit is set");
        return true;
    }

    if (value & 0x1) current |= 0x1;   // W1S demote
    if (value & 0x2) current |= 0x2;   // W1S lock

    DEMOTE_2 = current;
    compute_feat_ctrl();
    return true;
}
