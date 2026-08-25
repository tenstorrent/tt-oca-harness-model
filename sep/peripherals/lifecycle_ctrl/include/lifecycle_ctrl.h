// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "lifecycle_ctrl_base.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <functional>
#include <utility>

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

    /// The encoding the eFuse LC_STATE field carries, `{~raw, raw}`, and its check.
    static constexpr uint32_t lc_state_encode(uint32_t raw) {
        return (((~raw) & 0xFu) << 4) | (raw & 0xFu);
    }
    static constexpr bool lc_state_code_valid(uint32_t code) {
        return ((code >> 4) & 0xFu) == ((~code) & 0xFu);
    }

    /**
     * @brief The block's inputs, every one of which comes from the eFuse wrapper.
     *
     * sep_lifecycle_ctrl.sv is combinational on `shadow_regs_i`, `security_disable_i`
     * and `secure_tm_i`, so this is the whole port bundle and FEAT_CTRL is recomputed
     * from it on each call. Passing the bundle rather than offering a setter per input
     * preserves that: there is no state here that can be left half-updated.
     *
     * These are inputs and not configuration because two of them are software-writable
     * in silicon -- eFuse `SiP_DIS` (0x14) and `SYS_DIS` (0x1C) are `sw = rw` with
     * `onwrite = woset` -- so firmware disabling a feature has to reach FEAT_CTRL while
     * the simulation is running.
     *
     * `lc_state_code` is the 8-bit fuse word rather than a decoded state, because
     * decoding is this block's job: the RTL reads it as a `{diff_n, diff_p}` pair
     * through prim_diff_decode_multi, and a pair that is not a legal encoding raises
     * `lc_sigint_err_o` and zeroes the vector. That fail-safe is the reason the state is
     * encoded this way at all, so a tampered -- or blank -- lifecycle state has to
     * produce it here too.
     */
    struct lc_inputs {
        uint32_t lc_state_code    = lc_state_encode(LC_STATE_TEST_DEV);
        uint64_t sip_dis          = 0;
        uint64_t sys_dis          = 0;
        bool     security_disable = false;
        bool     secure_tm        = true;
    };

    /// Apply a new input bundle and recompute FEAT_CTRL.
    void set_inputs(const lc_inputs &in);

    /// True when the last LC_STATE code was not a legal differential pair. Mirrors
    /// `lc_sigint_err_o`; note that `security_disable` still overrides the vector it
    /// zeroes, exactly as it does in the RTL.
    bool get_lc_sigint_err() const { return m_lc_sigint_err; }

    /// The live feature vector, as `feat_ctrl_o`. Same value software reads from
    /// FEAT_CTRL_LO/HI, joined into the 64 bits the RTL struct actually is.
    uint64_t get_feat_ctrl() const {
        return (static_cast<uint64_t>(static_cast<uint32_t>(FEAT_CTRL_HI)) << 32)
             |  static_cast<uint32_t>(FEAT_CTRL_LO);
    }

    /**
     * @brief `feat_ctrl_o.sep_debug`, bit 0 of the vector.
     *
     * Called out separately because it is the only consumer of the feature vector
     * inside SEP, and it is a security decision rather than a feature enable: sep.sv
     * routes it to the inbound filter's `filter_skip_i`, which bypasses all match and
     * permission checking.
     */
    bool get_sep_debug() const { return (get_feat_ctrl() & 0x1u) != 0; }

    /**
     * @brief `prod_dbg_active_o`, the OR of the two demote bits.
     *
     * sep_crypto.sv takes this straight back into the eFuse, where it freezes LC_STATE:
     * a part demoted into PROD_DBG must not be able to walk itself onwards to another
     * lifecycle state. Note that it is the raw demote bits, not "PROD and demoted" —
     * the eFuse is what qualifies it with the current state.
     */
    bool get_prod_dbg_active() const {
        return ((static_cast<uint32_t>(DEMOTE_1) | static_cast<uint32_t>(DEMOTE_2)) & 0x1u) != 0;
    }

    /**
     * @brief Ask to be told when the feature vector changes.
     *
     * `feat_ctrl_o` is combinational, so anything driven from it has to follow the
     * inputs. That is not hypothetical here: a demote write or a `woset` write to the
     * eFuse SiP_DIS/SYS_DIS masks can flip `sep_debug`, and with it whether inbound
     * traffic is filtered at all.
     */
    void set_feat_ctrl_change_callback(std::function<void()> cb) {
        m_feat_ctrl_change_cb = std::move(cb);
    }

    /**
     * @brief Live demotion state for the key manager's OTP_DEMOTION_STATE register.
     *
     * Bits[1:0] are demote_1 (domain-1, written by BL1) and bits[3:2] demote_2
     * (domain-2, written by BL2). Each pair is differentially encoded, because
     * sep_crypto.sv drives this interface from prim_diff_encode_multi, whose output is
     * `{diff_n, diff_p}` with `diff_n = ~data` -- so each rail pair is `{~v, v}` and an
     * undemoted domain reads `2'b10`, not `2'b00`.
     *
     * That makes 0xA the reset value rather than 0x0. It matters because `2'b00` is not
     * a legal code at all: firmware that dual-rail checks this register would read the
     * un-encoded form as an integrity error.
     */
    uint32_t get_demote_state() const {
        const uint32_t d1 = static_cast<uint32_t>(DEMOTE_1) & 0x1u;
        const uint32_t d2 = static_cast<uint32_t>(DEMOTE_2) & 0x1u;
        return (((~d2) & 0x1u) << 3) | (d2 << 2) | (((~d1) & 0x1u) << 1) | d1;
    }

    // -----------------------------------------------------------------------
    // CCI parameters — all configurable via ini file, no recompile needed
    // -----------------------------------------------------------------------
    /**
     * These parameters seed the input bundle at end_of_elaboration and are what a
     * standalone testbench configures, since it has no eFuse to ask. On a platform the
     * eFuse model overrides them through set_inputs() before simulation starts and again
     * whenever the shadow registers change, matching sep_lifecycle_ctrl.sv, which has no
     * state of its own.
     *
     * lc_state is the raw 4-bit value here, not the differential code, so configs stay
     * readable; it is encoded when it seeds the bundle.
     */
    CsmlLogger           logger;
    csml_param<int>      verbosity;
    csml_param<uint32_t> lc_state;         ///< Raw 4-bit LC state (default: 0x0 = TEST_DEV)
    csml_param<uint32_t> sip_dis_lo;       ///< SIP feature disable mask [31:0]
    csml_param<uint32_t> sip_dis_hi;       ///< SIP feature disable mask [63:32]
    csml_param<uint32_t> sys_dis_lo;       ///< SYS feature disable mask [31:0]
    csml_param<uint32_t> sys_dis_hi;       ///< SYS feature disable mask [63:32]
    csml_param<bool>     security_disable; ///< When true, forces FEAT_CTRL=all-ones
    /**
     * The test_en strap, which gates the test section of FEAT_CTRL: false clears bits
     * [47:32]. Unlike the others this is not an eFuse value -- sep_efuse_wrapper.sv
     * latches it from `sep_straps_i.test_straps.test_en` when fuse sense completes -- so
     * it stays a parameter on a platform too, and defaults to the strap being deasserted.
     */
    csml_param<bool>     secure_tm;

private:
    void compute_feat_ctrl();

    lc_inputs m_in;                  ///< Live inputs; see set_inputs().
    bool      m_lc_sigint_err = false;
    std::function<void()> m_feat_ctrl_change_cb;

    bool on_demote_write(lc_ctrl::DEMOTE_type<32> &reg, const char *name, DT value);
    bool on_demote_hi_write(lc_ctrl::DEMOTE_HI_type<32> &reg, DT value);
    bool on_demote_1_write(DT value);
    bool on_demote_2_write(DT value);
};
