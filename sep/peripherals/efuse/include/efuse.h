// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "efuse_base.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <array>
#include <functional>
#include <string>
#include <utility>
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

    /**
     * @brief `locked_field_access_interrupt_o` — a shadow access the locks refused.
     *
     * efuse_shadow_reg_access_control.sv raises this on either offence: a read of a
     * read-locked field, or a write to a write-locked one. Both still complete on the
     * bus (the read returns DENY_WORD, the write is dropped), so this pin is the only
     * notification software gets; there is no status bit to poll and nothing to clear.
     *
     * Note the scope. The OTP program and read commands are gated separately, by
     * efuse_guard, which reports through req_error instead -- so a refused *fuse
     * command* does not raise this, only a refused *shadow access* does.
     *
     * In silicon it is a combinational pulse, high for the cycle of the offending
     * access. The model pulses it for LOCKED_FIELD_PULSE_WIDTH, which the PIC latches
     * when the source is configured edge-triggered, exactly as it latches the RTL's.
     */
    sc_out<bool> locked_field_access_irq_o;

    /// Width of that pulse. Arbitrary but non-zero: a delta-cycle pulse can be missed
    /// by a level-configured gateway, and there is no clock here to be a cycle of.
    static const sc_core::sc_time LOCKED_FIELD_PULSE_WIDTH;

    // OTP data accessors for other models (e.g. keymgr_tt, lifecycle_ctrl).
    // Safe to call after end_of_elaboration().

    /**
     * @brief The lifecycle state as a raw 4-bit value, decoded from the fuse.
     *
     * The fuse holds the differential code; consumers compare against the raw
     * encodings (LC_STATE_TEST_DEV and friends), so the decode belongs here rather
     * than being repeated in each of them. An illegal code yields
     * LC_STATE_RAW_INVALID, which every consumer treats as INVALID -- so a corrupted
     * or blank fuse fails closed instead of resolving to whichever state its low
     * nibble happens to name.
     */
    uint32_t get_lc_state() const {
        return lc_state_code_valid(m_lc_state_val) ? (m_lc_state_val & 0xFu)
                                                   : LC_STATE_RAW_INVALID;
    }

    /// The raw LC_STATE fuse contents, i.e. what software reads from the register.
    uint32_t        get_lc_state_code() const { return m_lc_state_val; }

    /**
     * @brief The chiplet UID as the key manager receives it.
     *
     * Zeroed in secure test mode: efuse_shadow_regs.sv gates the hardware secret
     * outputs on secure_tm, so a part strapped for test derives keys from nothing.
     * The bus view is untouched -- this gate is on the hardware port, not the read
     * path -- which is why it lives here and not in the read callbacks.
     */
    const uint32_t* get_chiplet_uid() const {
        return m_secure_tm_active ? m_zero_uid : m_chiplet_uid_cache;
    }

    /// Whether secure test mode is in force, as `secure_tm_o`. Read by lc_ctrl.
    bool get_secure_tm() const { return m_secure_tm_active; }

    /// The two 64-bit feature-disable masks from the shadow registers, as
    /// sep_lifecycle_ctrl.sv reads them out of `shadow_regs_i`. Both are `woset`, so
    /// they grow as firmware writes them and these accessors track that.
    uint64_t get_sip_dis() const {
        return (static_cast<uint64_t>(m_sip_dis_hi_val) << 32) | m_sip_dis_lo_val;
    }
    uint64_t get_sys_dis() const {
        return (static_cast<uint64_t>(m_sys_dis_hi_val) << 32) | m_sys_dis_lo_val;
    }

    /**
     * @brief Whether security disable is in force, as `security_disable_o`.
     *
     * efuse_security_tokens.sv computes `tt_rev_d_out[0] && (sec_disable_token_match ==
     * TOKEN_MATCH)`: the token has to match *and* the silicon revision has to permit the
     * feature, which prim_rev_cell exists to deny on A2/B1 parts. Matching a token is
     * therefore not sufficient on its own, and sec_disable_rev_enable is how a run picks
     * which side of that cut it models.
     */
    /// Not const: csml_param::get_param_value() is not a const member.
    bool get_security_disable() {
        return sec_disable_rev_enable.get_param_value()
            && static_cast<uint32_t>(SEC_DISABLE_TOKEN_MATCH) == TOKEN_MATCH;
    }

    /**
     * @brief Ask to be told when the shadow registers change.
     *
     * The shadow outputs are combinational in silicon, so consumers see a `woset` write
     * to LC_STATE, SiP_DIS or SYS_DIS immediately. A VP consumer that sampled the
     * accessors once at start of simulation would instead freeze at the boot value,
     * which is wrong precisely when it matters: firmware disabling a feature through the
     * eFuse has to reach whatever depends on it.
     */
    void set_shadow_change_callback(std::function<void()> cb) {
        m_shadow_change_cb = std::move(cb);
    }

    /**
     * @brief Backdoor-load the fuse array from a `.preload` image and sense it.
     *
     * The model equivalent of the testbench's +sep_preload_efuse: replaces the
     * array wholesale and re-derives the shadow registers from it, so both views
     * agree afterwards. Intended for testbenches and for configuring a run before
     * firmware starts; it is not reachable from the register interface, because no
     * such path exists in silicon either.
     *
     * @param path File with one ASCII '0' or '1' per line, LSB first.
     * @return true if the image was read; false leaves the array untouched.
     */
    bool preload_fuses_from_file(const std::string &path);

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
    csml_param<std::vector<uint32_t>> sip_pubk_hash0;
    csml_param<std::vector<uint32_t>> sys_pubk_hash;
    csml_param<std::vector<uint32_t>> chiplet_pubk_hash0;
    csml_param<std::vector<uint32_t>> chiplet_pubk_hash1;
    csml_param<std::vector<uint32_t>> sip_pubk_hash1;
    csml_param<std::vector<uint32_t>> sep_chiplet_id;
    csml_param<std::vector<uint32_t>> sep_sip_id;
    csml_param<std::vector<uint32_t>> sep_sys_id;

    /**
     * Expected SHA-256 digest of the secure-disable token, as eight 32-bit words
     * with word 0 holding digest bits [31:0].
     *
     * This is a parameter rather than a fuse because it is one in silicon too: there
     * is no secure-disable fuse anywhere in the map. efuse_security_tokens.sv builds
     * the reference from the 256-bit SEP_SEC_DISABLE_TOKEN parameter through 32
     * prim_rev_cell instances, so the value can be changed by a metal-only ECO. Only
     * the digest is ever in gates; the token itself is never stored.
     *
     * Defaults to zero, as the RTL parameter does, which means no token can match --
     * a real SHA-256 digest is never zero. The production value is embedded at
     * synthesis and is not in either repository, so a config supplies whichever
     * digest the run is meant to accept, exactly as the testbench overrides the
     * parameter.
     */
    csml_param<std::vector<uint32_t>> sec_disable_token_digest;

    /**
     * Whether this silicon revision permits security disable at all.
     *
     * Models `tt_rev_d_out[0]` from the prim_rev_cell in efuse_security_tokens.sv, whose
     * comment is "Disable sec_disable_feature during A2/B1": on those revisions the
     * feature is metal-tied off and a matching token still does nothing. Defaults to
     * true, the later-revision behaviour, since it has no effect until a token matches
     * anyway.
     */
    csml_param<bool> sec_disable_rev_enable;

    /**
     * `secure_tm_i` — the latched test_en strap that puts the part in secure test mode.
     *
     * efuse_guard.sv empties every fuse command while this is asserted, and
     * efuse_shadow_regs.sv zeroes the secrets handed to the key manager, so a part in
     * secure test mode can neither burn nor read the array and yields no keys. It is a
     * parameter here for the same reason it is a strap there: nothing in the design
     * sets it, and a functional part never asserts it.
     *
     * The lifecycle controller consumes it too, to gate FEAT_CTRL's test group. On a
     * platform it is sourced from here rather than from lc_ctrl's own parameter,
     * because in silicon the eFuse wrapper is what latches the strap and fans it out.
     */
    csml_param<bool> secure_tm;

    /**
     * Path to a fuse array image, in either format the two testbenches use:
     *
     *   - the OCA `.preload` format, 8192 lines of one ASCII '0' or '1', LSB first,
     *     as consumed by +sep_preload_efuse;
     *   - a `$readmemh` image, 256 hex words with word 0 first, `@addr` origins and
     *     `//` comments allowed, as the harness DV images are written.
     *
     * The format is detected from the file, not declared. Empty (the default) leaves
     * the array erased, which is what +SEP_EFUSE_NO_PRELOAD gives on RTL. When an
     * image is supplied, it defines the fuse array and per-field params are not
     * applied.
     */
    csml_param<std::string> fuse_preload_file;

    // OTP array size, in bits and 32-bit words (sep_efuse_pkg::NumEfuseBits).
    static constexpr unsigned int NUM_FUSE_BITS  = 8192;
    static constexpr unsigned int NUM_FUSE_WORDS = NUM_FUSE_BITS / 32;

    /**
     * Bit index of LC_STATE in the array (sep_pkg::LC_STATE_BIT_POSITION). LOCKS is
     * 64 bits wide and comes first, so LC_STATE starts at bit 64. Bits 65 and 66 --
     * the two RMA advance bits -- are individually gated by the token matches.
     */
    static constexpr unsigned int LC_STATE_BIT_POSITION = 64;

    /**
     * Value returned for an access the lock policy refuses. The shadow path
     * answers with this and a normal (non-error) bus response, so software has to
     * recognise the sentinel rather than take a fault.
     */
    static constexpr uint32_t DENY_WORD = 0xBADCAB1Eu;

    /**
     * TOKEN_MATCH.token_match_status encodings (efuse_mmr.rdl).
     *
     * The two live codes are bitwise complements, because the comparison is carried
     * on three redundant differential lanes: a stuck-at fault yields neither code but
     * 0x3F, and 0x00 means no comparison has completed yet. The VP produces only
     * MATCH and MISMATCH -- 0x3F needs a fault the model has no way to inject, and
     * 0x00 is unobservable here because the hash completes inside the triggering
     * write rather than over several cycles.
     */
    static constexpr uint32_t TOKEN_MATCH    = 0x15u;   ///< 6'b010101
    static constexpr uint32_t TOKEN_MISMATCH = 0x2Au;   ///< 6'b101010

    /**
     * @name LC_STATE differential encoding
     *
     * The LC_STATE fuse does not hold the 4-bit lifecycle state directly. It holds
     * `{~raw[3:0], raw[3:0]}`, so TEST_DEV (raw 0) is stored as 0xF0 -- see
     * sep_efuse_map.rdl, where the field's reset value is 0xf0. Encoding it this way
     * means a stuck bit or a single-bit flip produces a code that is not a legal
     * encoding at all, rather than silently promoting the part to a more permissive
     * state.
     *
     * Two consequences worth knowing. An erased fuse array reads 0x00, which is *not*
     * a valid code (0x00 would require ~raw == raw), so a blank part has no valid
     * lifecycle state -- that is true of the RTL too. And the config parameter is the
     * raw 4-bit value, because that is the form the lifecycle controller documents and
     * compares against; the encoding happens on the way into the array.
     * @{
     */
    static constexpr uint32_t lc_state_encode(uint32_t raw) {
        return (((~raw) & 0xFu) << 4) | (raw & 0xFu);
    }

    static constexpr bool lc_state_code_valid(uint32_t code) {
        return ((code >> 4) & 0xFu) == ((~code) & 0xFu);
    }

    /**
     * Returned for a code that is not a legal differential encoding. Any value the
     * lifecycle controller does not recognise means INVALID and forces FEAT_CTRL to 0;
     * 0x4 is the value its own documentation uses for that case.
     */
    static constexpr uint32_t LC_STATE_RAW_INVALID = 0x4u;
    /** @} */

    /**
     * @name Raw lifecycle states and the transition machine's inputs
     *
     * efuse_pkg::lc_state_raw_e. Only these seven codes exist; everything else is
     * invalid, and is_valid_lc_state() is what the transition machine consults twice
     * per write -- once on the intended destination and once on what per-bit gating
     * actually produced.
     * @{
     */
    static constexpr uint32_t LC_RAW_TEST_DEV   = 0x0u;
    static constexpr uint32_t LC_RAW_PROD       = 0x1u;
    static constexpr uint32_t LC_RAW_RMA_SIP_0  = 0x2u;
    static constexpr uint32_t LC_RAW_RMA_SIP_1  = 0x3u;
    static constexpr uint32_t LC_RAW_RMA_CHIP_0 = 0x6u;
    static constexpr uint32_t LC_RAW_RMA_CHIP_1 = 0x7u;
    static constexpr uint32_t LC_RAW_PROD_END   = 0x8u;

    static constexpr bool lc_state_raw_valid(uint32_t s) {
        return s == LC_RAW_TEST_DEV  || s == LC_RAW_PROD       ||
               s == LC_RAW_RMA_SIP_0 || s == LC_RAW_RMA_SIP_1  ||
               s == LC_RAW_RMA_CHIP_0|| s == LC_RAW_RMA_CHIP_1 ||
               s == LC_RAW_PROD_END;
    }

    /**
     * @brief `prod_dbg_active_i`, from the lifecycle controller's demote bits.
     *
     * A platform feeds this back from lc_ctrl; a standalone testbench leaves it false.
     * Combined with LC_STATE == PROD it makes the state terminal.
     */
    void set_prod_dbg_active(bool active) { m_prod_dbg_active = active; }
    /** @} */

private:
    /**
     * @brief One fuse field's extent, paired with the lock bits that govern it.
     *
     * LOCKS holds two bits per field, write lock then read lock, in the same order
     * the fields appear in the map. A field can span several words -- the eight-word
     * keys and tokens share a single lock pair -- so the region is a word range
     * rather than a single register.
     */
    struct lock_region {
        unsigned int first_word;   ///< inclusive index into the array / shadow map
        unsigned int last_word;    ///< inclusive
        bool         in_locks_hi;  ///< which LOCKS register holds the pair
        unsigned int write_bit;    ///< read lock is always write_bit + 1
    };

    static const lock_region  k_lock_regions[];
    static const unsigned int k_lock_region_count;

    const lock_region *region_for_word(unsigned int word) const;
    bool is_write_locked(unsigned int word) const;
    bool is_read_locked(unsigned int word) const;
    void register_lock_read_callbacks();

    // Locked-field interrupt. Requested from a bus callback, driven from a process;
    // see the comment on drive_locked_field_irq's definition.
    sc_core::sc_event m_locked_field_raise;
    sc_core::sc_event m_locked_field_pulse_end;
    bool              m_locked_field_pending = false;
    void raise_locked_field_access();
    void drive_locked_field_irq();

    /// True when a write to `word` must be refused, raising the interrupt if so.
    /// Wraps is_write_locked() so no caller can refuse silently by accident.
    bool refuse_locked_write(unsigned int word);

    /**
     * @brief The fuse array — the model's source of truth for fuse contents.
     *
     * Flat image of the OTP, addressed by bit index as the program and read CSRs
     * address it. The shadow register map is the same 8192 bits seen as words:
     * sep_efuse's schema packs fields sequentially by width, so a shadow register
     * at byte offset N mirrors fuse bits [N*8 .. N*8+31], i.e. array word N/4.
     * That is why the shadow map is exactly 0x400 bytes. It is also why
     * LC_STATE_BIT_POSITION is 64: LOCKS is 64 bits wide and comes first.
     *
     * Programming is one-way. The Samsung macro ORs new data over old
     * (`d_latch = din | fuse`) and reads the redundant banks together, so a set
     * bit can never be cleared by software.
     */
    std::array<uint32_t, NUM_FUSE_WORDS> m_fuse{};

    // Sticky error flags behind EFUSE_INTERFACE_CTRL_STATUS, cleared by its W1S
    // clear bits rather than by writing the error bits themselves.
    bool m_req_error;
    bool m_program_addr_error;
    bool m_read_addr_error;

    uint32_t m_chiplet_uid_cache[8] = {};  // stable pointer for get_chiplet_uid()
    const uint32_t m_zero_uid[8] = {};     // what get_chiplet_uid() returns under secure_tm

    // Sampled from the secure_tm parameter at elaboration, so the accessors and the
    // command gate agree and neither has to reach into a csml_param on every call.
    bool m_secure_tm_active = false;

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

    bool m_prod_dbg_active = false;

    std::function<void()> m_shadow_change_cb;
    void notify_shadow_change() { if (m_shadow_change_cb) m_shadow_change_cb(); }

    void register_callbacks();
    void load_fuses();
    void load_non_fuse_defaults();

    // Fuse array helpers. `bit_addr` is a bit index into the array, as the
    // program and read CSRs use; out-of-range addresses are the caller's to check.
    bool     load_preload_file(const std::string &path);
    void     sense_fuses_into_shadows();
    void     fuse_or_word(unsigned int word_index, uint32_t value);
    uint32_t fuse_read_word(uint32_t bit_addr) const;
    void     fuse_program_bit(uint32_t bit_addr);
    void     update_status_register();

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

    // Token matching
    bool handle_write_TOKEN_EOP(uint32_t value);
    void apply_transient_rma();
    static void sha256_token(const uint32_t token_words[8], uint32_t digest_words[8]);
    void evaluate_token(unsigned int token_base_word, const uint32_t reference[8],
                        sep_efuse::TOKEN_MATCH_type<32> &result);

    // Interface-ctrl program/read sequences and status clears
    bool secure_tm_blocks_command(const char *what);
    bool handle_write_EFUSE_PROGRAM_CTRL(uint32_t value);
    bool handle_write_EFUSE_READ_CTRL(uint32_t value);
    bool handle_write_EFUSE_INTERFACE_CTRL_STATUS(uint32_t value);
};
