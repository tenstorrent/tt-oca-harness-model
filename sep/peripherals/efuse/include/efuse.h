#pragma once
#include "efuse_base.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <array>
#include <string>
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
    const uint32_t* get_chiplet_uid()   const { return m_chiplet_uid_cache; }

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
    csml_param<std::vector<uint32_t>> sip_pubk;
    csml_param<std::vector<uint32_t>> sys_pubk;
    csml_param<std::vector<uint32_t>> public_key_0;
    csml_param<std::vector<uint32_t>> public_key_1;

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
     * Path to a fuse array image in the RTL's `.preload` format: 8192 lines, one
     * ASCII '0' or '1' per line, LSB first, as consumed by +sep_preload_efuse.
     * Empty (the default) leaves the array erased, which is what
     * +SEP_EFUSE_NO_PRELOAD gives on RTL. The per-field params above are applied
     * on top of whatever the file provides.
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
    static void sha256_token(const uint32_t token_words[8], uint32_t digest_words[8]);
    void evaluate_token(unsigned int token_base_word, const uint32_t reference[8],
                        sep_efuse::TOKEN_MATCH_type<32> &result);

    // Interface-ctrl program/read sequences and status clears
    bool handle_write_EFUSE_WRITE_CTRL(uint32_t value);
    bool handle_write_EFUSE_READ_CTRL(uint32_t value);
    bool handle_write_EFUSE_INTERFACE_CTRL_STATUS(uint32_t value);
};
