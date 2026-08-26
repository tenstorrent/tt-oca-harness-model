// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "efuse.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <vector>
#include <openssl/sha.h>

static const std::vector<uint32_t> k_zero8(8, 0u);

const sc_core::sc_time efuse_model::LOCKED_FIELD_PULSE_WIDTH(1, sc_core::SC_NS);

// =============================================================================
// Constructor
// =============================================================================

efuse_model::efuse_model(sc_module_name n, int log_verbosity)
    : efuse_base(n, sep_efuse::EFUSE_WINDOW_SIZE, sep_efuse::SHIM_CTRL_WINDOW_SIZE)
    , verbosity("verbosity", log_verbosity)
    , locks_lo("locks_lo", 0u)
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
    , sec_disable_token_digest("sec_disable_token_digest", k_zero8)
    , sec_disable_rev_enable("sec_disable_rev_enable", true)
    , secure_tm("secure_tm", false)
    , fuse_preload_file("fuse_preload_file", std::string())
    , m_req_error(false)
    , m_program_addr_error(false)
    , m_read_addr_error(false)
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

    SC_METHOD(drive_locked_field_irq);
    sensitive << m_locked_field_raise << m_locked_field_pulse_end;
    dont_initialize();
}

// =============================================================================
// Locked-field access interrupt
//
// The refusal is detected inside b_transport, but the pin is driven from a process:
// a signal may have only one driver, and the callers are whichever process happens to
// be on the bus. So a refusal only requests the pulse, and drive_locked_field_irq --
// the sole driver -- raises and lowers it.
//
// Refusals closer together than the pulse width merge into one pulse, since the
// second finds the pin already high and only extends it. The RTL merges them the same
// way within a cycle, and an interrupt with no count behind it cannot express "twice"
// in any case.
// =============================================================================

void efuse_model::raise_locked_field_access()
{
    m_locked_field_pending = true;
    m_locked_field_raise.notify(sc_core::SC_ZERO_TIME);
}

void efuse_model::drive_locked_field_irq()
{
    if (m_locked_field_pending) {
        m_locked_field_pending = false;
        locked_field_access_irq_o.write(true);
        m_locked_field_pulse_end.notify(LOCKED_FIELD_PULSE_WIDTH);
    } else {
        locked_field_access_irq_o.write(false);
    }
}

bool efuse_model::refuse_locked_write(unsigned int word)
{
    if (!is_write_locked(word))
        return false;
    raise_locked_field_access();
    return true;
}

// =============================================================================
// Callback Registration
// =============================================================================

void efuse_model::register_callbacks()
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
        [this](uint32_t v){ return handle_write_EFUSE_PROGRAM_CTRL(v); },
        EFUSE_PROGRAM_CTRL.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_EFUSE_READ_CTRL(v); },
        EFUSE_READ_CTRL.offset);
    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_EFUSE_INTERFACE_CTRL_STATUS(v); },
        EFUSE_INTERFACE_CTRL_STATUS.offset);

    memory.register_write_callback(
        [this](uint32_t v){ return handle_write_TOKEN_EOP(v); },
        TOKEN_EOP.offset);

    register_lock_read_callbacks();
}

// =============================================================================
// Token Matching
//
// Software stages a 256-bit token in eight input words, then pulses a per-token go
// bit in TOKEN_EOP. Hardware hashes the token with SHA-256 and compares the digest
// against a reference, publishing MATCH or MISMATCH in the corresponding result
// register. The plaintext token is never compared and the reference is never a
// plaintext token: both sides of the comparison are digests.
//
// The whole sequence completes inside the triggering write. In silicon the hash
// takes tens of cycles, during which the result reads 0x00, but software has no way
// to observe that window here: its next read already sees the answer.
// =============================================================================

void efuse_model::sha256_token(const uint32_t token_words[8], uint32_t digest_words[8])
{
    // Message layout follows sha256_token_hash.sv, which streams token_i[7] into
    // message word w[0]: the most significant token word is hashed first, and each
    // word goes in big-endian, SHA-256's native order. Getting this backwards still
    // produces a valid digest, just not the one the reference was computed from, so
    // it would show up only as an unexplained mismatch.
    uint8_t message[32];
    for (int w = 0; w < 8; w++) {
        const uint32_t word = token_words[7 - w];
        message[w * 4 + 0] = static_cast<uint8_t>(word >> 24);
        message[w * 4 + 1] = static_cast<uint8_t>(word >> 16);
        message[w * 4 + 2] = static_cast<uint8_t>(word >>  8);
        message[w * 4 + 3] = static_cast<uint8_t>(word);
    }

    uint8_t digest[SHA256_DIGEST_LENGTH];
    SHA256(message, sizeof(message), digest);

    // The RTL places H0 in the most significant 32 bits of the 256-bit digest vector,
    // and the reference is a 256-bit register whose word 0 is bits [31:0]. So word 0
    // holds H7 and word 7 holds H0 -- the words run opposite to the digest byte order.
    for (int i = 0; i < 8; i++) {
        const uint8_t *h = &digest[(7 - i) * 4];
        digest_words[i] = (static_cast<uint32_t>(h[0]) << 24)
                        | (static_cast<uint32_t>(h[1]) << 16)
                        | (static_cast<uint32_t>(h[2]) <<  8)
                        |  static_cast<uint32_t>(h[3]);
    }
}

void efuse_model::evaluate_token(unsigned int token_base_word, const uint32_t reference[8],
                                 sep_efuse::TOKEN_MATCH_type<32> &result)
{
    uint32_t token[8];
    for (int i = 0; i < 8; i++)
        token[i] = memory.memory_block[token_base_word + i];

    uint32_t digest[8];
    sha256_token(token, digest);

    bool match = true;
    for (int i = 0; i < 8; i++) {
        if (digest[i] != reference[i]) {
            match = false;
            break;
        }
    }

    result = match ? TOKEN_MATCH : TOKEN_MISMATCH;
}

bool efuse_model::handle_write_TOKEN_EOP(uint32_t value)
{
    // Each go bit is a singlepulse, so TOKEN_EOP itself is left at zero: the register
    // is write-only in the RDL and reads back nothing regardless.
    if (value & (1u << 0)) {
        // The RMA references are fuse-backed. RTL calls these fields
        // RMA_SIP_TOKEN_DIGEST and RMA_CHIPLET_TOKEN_DIGEST; they sit where this
        // model calls them RMA_SIP_TOKEN and RMA_CHIPLET_TOKEN, and they hold
        // digests, not tokens, despite the shorter names here.
        uint32_t reference[8];
        for (int i = 0; i < 8; i++)
            reference[i] = memory.memory_block[RMA_SIP_TOKEN[i].offset];
        evaluate_token(RMA_SIP_TOKEN_I[0].offset, reference, RMA_SIP_TOKEN_MATCH);
    }

    if (value & (1u << 8)) {
        uint32_t reference[8];
        for (int i = 0; i < 8; i++)
            reference[i] = memory.memory_block[RMA_CHIPLET_TOKEN[i].offset];
        evaluate_token(RMA_CHIPLET_TOKEN_I[0].offset, reference, RMA_CHIPLET_TOKEN_MATCH);
    }

    if (value & (1u << 16)) {
        // No fuse holds this reference; see sec_disable_token_digest in efuse.h.
        const std::vector<uint32_t> ref = sec_disable_token_digest.get_param_value();
        uint32_t reference[8] = {};
        for (int i = 0; i < 8 && i < static_cast<int>(ref.size()); i++)
            reference[i] = ref[i];
        evaluate_token(SEC_DISABLE_TOKEN_I[0].offset, reference, SEC_DISABLE_TOKEN_MATCH);
    }

    TOKEN_EOP = 0u;

    apply_transient_rma();

    // A SEC_DISABLE match asserts security_disable_o, which lifecycle_ctrl consumes.
    notify_shadow_change();
    return true;
}

/*
 * Transient RMA: advance the lifecycle state on a token match alone.
 *
 * With the TRANSIENT_RMA_EN fuse set, efuse_shadow_regs.sv:338-346 moves LC_STATE with
 * no software write at all -- a matching SiP token sets bit [1], a matching chiplet
 * token sets bit [2]. It is the branch the RTL takes when no bus access is in flight,
 * so it is continuous there; here the token comparison is what changes, and it only
 * changes on a TOKEN_EOP write, so that is where this runs.
 *
 * The guards are the ones the software path already enforces, which is the point of
 * the mechanism: transient RMA removes the need for the write, not the need for the
 * token. Chiplet still requires RMA_SiP established and a state that is not PROD, and
 * the chiplet branch does not fall through to the SiP one when its guard fails --
 * matching a chiplet token must not quietly perform a SiP transition instead.
 */
void efuse_model::apply_transient_rma()
{
    if ((static_cast<uint32_t>(TRANSIENT_RMA_EN) & 0x1u) == 0)
        return;

    const uint32_t cur = m_lc_state_val & 0xFu;

    // Terminal states short-circuit ahead of this branch in the RTL, so they are
    // immovable here too, demoted PROD included.
    if (cur == LC_RAW_PROD_END || cur == LC_RAW_RMA_CHIP_0 || cur == LC_RAW_RMA_CHIP_1 ||
        (cur == LC_RAW_PROD && m_prod_dbg_active))
        return;

    const bool sip_match  = static_cast<uint32_t>(RMA_SIP_TOKEN_MATCH) == TOKEN_MATCH;
    const bool chip_match = static_cast<uint32_t>(RMA_CHIPLET_TOKEN_MATCH) == TOKEN_MATCH;

    uint32_t next = cur;
    if (chip_match) {
        if (cur != LC_RAW_PROD && (cur & 0x2u))
            next = cur | 0x4u;
    } else if (sip_match) {
        next = cur | 0x2u;
    }

    if (next == cur || !lc_state_raw_valid(next))
        return;

    m_lc_state_val = lc_state_encode(next);
    LC_STATE       = m_lc_state_val;
    CSML_INFO(1, logger) << "transient RMA: LC_STATE advanced to 0x" << std::hex << next
                         << std::dec << " on token match" << std::endl;
}

// =============================================================================
// Lock Policy
//
// LOCKS carries a write-lock/read-lock pair per fuse field, in map order, and the
// bits are write-1-to-set and sticky until reset. Both the shadow path and the OTP
// path are gated by them, but they refuse differently: a shadow access returns
// DENY_WORD with a normal bus response, while an OTP command is dropped before it
// reaches the macro and reports done-with-error plus a sticky req_error.
//
// Word ranges are given as the shadow map byte offsets they correspond to, divided
// by four -- the array and the shadow map are the same 8192 bits, so one number
// indexes both. LOCKS_LO and LOCKS_HI are deliberately absent: they govern other
// fields and are themselves always readable, sticky rather than lockable.
// =============================================================================

const efuse_model::lock_region efuse_model::k_lock_regions[] = {
    // first        last          hi     write-lock bit
    { 0x00C / 4, 0x00C / 4, false,  0 },  // LC_STATE
    { 0x010 / 4, 0x010 / 4, false,  2 },  // SBOOT_DIS
    { 0x014 / 4, 0x014 / 4, false,  4 },  // TRANSIENT_RMA_EN
    { 0x018 / 4, 0x01C / 4, false,  6 },  // SIP_DIS_{LO,HI}
    { 0x020 / 4, 0x024 / 4, false,  8 },  // SYS_DIS_{LO,HI}
    { 0x028 / 4, 0x044 / 4, false, 10 },  // RMA_SIP_TOKEN[8]
    { 0x048 / 4, 0x064 / 4, false, 12 },  // RMA_CHIPLET_TOKEN[8]
    { 0x068 / 4, 0x084 / 4, false, 14 },  // CLASS_KEY[8]
    { 0x088 / 4, 0x088 / 4, false, 16 },  // CHIPLET_PUBK_REVOKE
    { 0x08C / 4, 0x0A8 / 4, false, 18 },  // BL1_VERSION[8]
    { 0x0AC / 4, 0x0C8 / 4, false, 20 },  // BL2_VERSION[8]
    { 0x0CC / 4, 0x0E8 / 4, false, 22 },  // CHIPLET_UID[8]
    { 0x0EC / 4, 0x108 / 4, false, 24 },  // SIP_PUBK[8]
    { 0x10C / 4, 0x128 / 4, false, 26 },  // SIP_UID[8]
    { 0x12C / 4, 0x148 / 4, false, 28 },  // SYS_PUBK[8]
    { 0x14C / 4, 0x168 / 4, false, 30 },  // SYS_UID[8]
    { 0x16C / 4, 0x16C / 4, true,   0 },  // STATUS_RPT
    { 0x170 / 4, 0x170 / 4, true,   2 },  // SEP_ROM_CTRL
    // One pair covers the whole SPI control group: the field-enable register plus
    // the discovery and PHY timing registers that follow it.
    { 0x174 / 4, 0x194 / 4, true,   4 },  // SEP_SPI_CTRL_FIELD_EN .. SPI_RB_VALID_TIME
    { 0x198 / 4, 0x1B4 / 4, true,   6 },  // PUBLIC_KEY_0[8]
    { 0x1B8 / 4, 0x1D4 / 4, true,   8 },  // PUBLIC_KEY_1[8]
    { 0x1D8 / 4, 0x214 / 4, true,  10 },  // RESERVED_0[16]
    { 0x218 / 4, 0x254 / 4, true,  12 },  // RESERVED_1[16]
    { 0x258 / 4, 0x294 / 4, true,  14 },  // RESERVED_2[16]
    { 0x298 / 4, 0x2D4 / 4, true,  16 },  // RESERVED_3[16]
    { 0x2D8 / 4, 0x314 / 4, true,  18 },  // RESERVED_4[16]
    { 0x318 / 4, 0x354 / 4, true,  20 },  // RESERVED_5[16]
    { 0x358 / 4, 0x394 / 4, true,  22 },  // RESERVED_6[16]
    { 0x398 / 4, 0x3D0 / 4, true,  24 },  // RESERVED_7[16]
    { 0x3D4 / 4, 0x3F0 / 4, true,  26 },  // RESERVED_LAST_256[8]
    { 0x3F4 / 4, 0x3F8 / 4, true,  28 },  // RESERVED_LAST_64_{LO,HI}
    { 0x3FC / 4, 0x3FC / 4, true,  30 },  // RESERVED_LAST_32
};

const unsigned int efuse_model::k_lock_region_count =
    sizeof(efuse_model::k_lock_regions) / sizeof(efuse_model::k_lock_regions[0]);

const efuse_model::lock_region *efuse_model::region_for_word(unsigned int word) const
{
    for (unsigned int i = 0; i < k_lock_region_count; i++) {
        if (word >= k_lock_regions[i].first_word && word <= k_lock_regions[i].last_word)
            return &k_lock_regions[i];
    }
    return nullptr;   // not a lockable field
}

bool efuse_model::is_write_locked(unsigned int word) const
{
    const lock_region *r = region_for_word(word);
    if (r == nullptr)
        return false;
    const uint32_t locks = r->in_locks_hi ? m_locks_hi_val : m_locks_lo_val;
    return (locks >> r->write_bit) & 1u;
}

bool efuse_model::is_read_locked(unsigned int word) const
{
    const lock_region *r = region_for_word(word);
    if (r == nullptr)
        return false;
    const uint32_t locks = r->in_locks_hi ? m_locks_hi_val : m_locks_lo_val;
    return (locks >> (r->write_bit + 1u)) & 1u;
}

void efuse_model::register_lock_read_callbacks()
{
    // Every word of every lockable field needs its read intercepted, since the
    // deny decision is per access rather than baked into the register's masks.
    // Registering here replaces the default masked read, which is safe because all
    // shadow registers are fully readable (read_mask 0xFFFFFFFF) when unlocked.
    for (unsigned int i = 0; i < k_lock_region_count; i++) {
        for (unsigned int word = k_lock_regions[i].first_word;
             word <= k_lock_regions[i].last_word; word++) {
            memory.register_read_callback(
                [this, word](uint32_t &value) {
                    if (is_read_locked(word)) {
                        value = DENY_WORD;
                        raise_locked_field_access();
                    } else {
                        value = memory.memory_block[word];
                    }
                    return true;
                }, word);
        }
    }
}

// =============================================================================
// Fuse Array
//
// The array is the model's source of truth. The shadow map is the same 8192 bits
// viewed as words (see the m_fuse comment in efuse.h), so a shadow register at
// byte offset N and array word N/4 are two names for one thing.
// =============================================================================

void efuse_model::fuse_or_word(unsigned int word_index, uint32_t value)
{
    if (word_index < NUM_FUSE_WORDS)
        m_fuse[word_index] |= value;
}

uint32_t efuse_model::fuse_read_word(uint32_t bit_addr) const
{
    const uint32_t word = bit_addr / 32;
    return (word < NUM_FUSE_WORDS) ? m_fuse[word] : 0u;
}

void efuse_model::fuse_program_bit(uint32_t bit_addr)
{
    const uint32_t word = bit_addr / 32;
    if (word < NUM_FUSE_WORDS) {
        // OR, never assign: the Samsung macro programs with `d_latch = din | fuse`
        // and reads its redundant banks together, so software cannot clear a bit
        // it has already burned.
        m_fuse[word] |= (1u << (bit_addr % 32));
    }
}

bool efuse_model::preload_fuses_from_file(const std::string &path)
{
    if (!load_preload_file(path))
        return false;
    sense_fuses_into_shadows();
    return true;
}

bool efuse_model::load_preload_file(const std::string &path)
{
    // Follow the convention the platform already uses for its other image paths
    // (spiPreload, the ELF): a relative path is relative to the .ini that named it,
    // not to wherever the simulator was launched from, and sep/main.cpp exports
    // SEP_VP_INI_DIR for that. With no such directory — a standalone peripheral
    // testbench — the path is used exactly as given.
    std::string resolved = path;
    if (const char *ini_dir = std::getenv("SEP_VP_INI_DIR")) {
        const std::filesystem::path p(path);
        if (p.is_relative())
            resolved = (std::filesystem::path(ini_dir) / p).lexically_normal().string();
    }

    std::ifstream in(resolved);
    if (!in) {
        CSML_ERROR(0, logger) << "fuse_preload_file: cannot open '" << resolved
                              << "'; leaving the array erased" << std::endl;
        return false;
    }

    // Two image formats are in circulation and both are accepted:
    //
    //   - one ASCII bit per line, LSB first, 8192 lines for SEP. What the OCA
    //     testbench's +sep_preload_efuse consumes, and what this model has always read.
    //   - $readmemh hex words, 256 of them, word 0 first, optionally with `@addr`
    //     origin markers and `//` comments. What the harness DV images are, and what
    //     Verilog's own $readmemh reads.
    //
    // Sniffed rather than configured: a token wider than one character cannot be a
    // bit, and one that is not cannot be a hex word of any sensible width, so the two
    // never collide in practice and a run does not have to declare which it has.
    //
    // Longer files (the SMU/SMC bit images run to 24576) are truncated to what this
    // block implements rather than refused. Everything is parsed into a scratch image
    // and committed only on success, so a malformed file leaves the array as it was
    // instead of half-loaded.
    std::vector<std::string> tokens;
    std::string line;
    while (std::getline(in, line)) {
        const size_t comment = line.find("//");
        if (comment != std::string::npos)
            line.erase(comment);
        std::istringstream ls(line);
        std::string tok;
        while (ls >> tok)
            tokens.push_back(tok);
    }

    bool hex_words = false;
    for (const std::string &t : tokens) {
        if (t.size() > 1 && t[0] != '@') {
            hex_words = true;
            break;
        }
    }

    std::array<uint32_t, NUM_FUSE_WORDS> image{};
    unsigned int loaded_bits = 0;

    if (hex_words) {
        unsigned int word = 0;
        for (const std::string &t : tokens) {
            if (t[0] == '@') {
                word = static_cast<unsigned int>(std::strtoul(t.c_str() + 1, nullptr, 16));
                continue;
            }
            if (t.size() > 8 || t.find_first_not_of("0123456789abcdefABCDEF")
                                    != std::string::npos) {
                CSML_ERROR(0, logger) << "fuse_preload_file: '" << resolved
                                      << "' has '" << t
                                      << "' where a 32-bit hex word was expected"
                                      << std::endl;
                return false;
            }
            if (word < NUM_FUSE_WORDS) {
                image[word] = static_cast<uint32_t>(std::stoul(t, nullptr, 16));
                loaded_bits = (word + 1) * 32;
            }
            word++;
        }
    } else {
        unsigned int bit = 0;
        for (const std::string &t : tokens) {
            if (bit >= NUM_FUSE_BITS)
                break;
            if (t == "1")
                image[bit / 32] |= (1u << (bit % 32));
            else if (t != "0") {
                CSML_ERROR(0, logger) << "fuse_preload_file: '" << resolved
                                      << "' bit " << bit << " is '" << t
                                      << "', expected 0 or 1" << std::endl;
                return false;
            }
            bit++;
        }
        loaded_bits = bit;
    }

    m_fuse = image;
    CSML_INFO(1, logger) << "fuse_preload_file: loaded " << loaded_bits << " bits from '"
                         << resolved << "' (" << (hex_words ? "hex-word" : "bit-per-line")
                         << " format)" << std::endl;
    return true;
}

void efuse_model::sense_fuses_into_shadows()
{
    // Stands in for the shadow-register FSM, which on reset release bulk-reads all
    // 256 words out of the array and latches them before raising fuse_sense_done.
    // The VP does it in one step at elaboration, so efuse_sense_done is already
    // set by the time firmware runs and the pre-sense window is never observable.
    // Writing the backing store directly deliberately bypasses the software write
    // masks: this is the hardware load path, not a bus write, and most of these
    // registers are read-only to software by design.
    for (unsigned int word = 0; word < NUM_FUSE_WORDS; word++)
        memory.memory_block[word] = m_fuse[word];

    // The WOSET mirrors are flops the sense FSM loads, so they have to be re-derived
    // here rather than left holding whatever the previous image or config put there.
    // The locks matter most: is_write_locked() reads m_locks_lo_val, so an image with
    // lock fuses burned -- as the RTL's default preload has -- would otherwise sense
    // into the shadow register and still leave the policy wide open.
    m_locks_lo_val            = memory.memory_block[LOCKS_LO.offset];
    m_locks_hi_val            = memory.memory_block[LOCKS_HI.offset];
    m_lc_state_val            = memory.memory_block[LC_STATE.offset];
    m_sip_dis_lo_val          = memory.memory_block[SIP_DIS_LO.offset];
    m_sip_dis_hi_val          = memory.memory_block[SIP_DIS_HI.offset];
    m_sys_dis_lo_val          = memory.memory_block[SYS_DIS_LO.offset];
    m_sys_dis_hi_val          = memory.memory_block[SYS_DIS_HI.offset];
    m_chiplet_pubk_revoke_val = memory.memory_block[CHIPLET_PUBK_REVOKE.offset];
    for (int i = 0; i < 8; i++) {
        m_bl1_version_val[i]  = memory.memory_block[BL1_VERSION[i].offset];
        m_bl2_version_val[i]  = memory.memory_block[BL2_VERSION[i].offset];
        m_chiplet_uid_cache[i] = memory.memory_block[CHIPLET_UID[i].offset];
    }

    notify_shadow_change();
}

// =============================================================================
// Fuse Load — called from end_of_elaboration
//
// Everything fuse-backed is written into the array first and then sensed into the
// shadow registers, so the two can never disagree: a field configured here reads
// back the same whether software goes through the shadow map or through the OTP
// read CSR. Registers that are not fuse-backed (the token match results) are set
// directly, since no fuse holds them.
// =============================================================================

void efuse_model::load_fuses()
{
    // Helper: safely read element i from a vector param (0 if out of range)
    auto vi = [](const std::vector<uint32_t>& v, int i) -> uint32_t {
        return (i < (int)v.size()) ? v[i] : 0u;
    };

    // An image and the per-field parameters are two ways of describing the same array,
    // not a base and an overlay, so exactly one of them applies. The image wins when
    // present, which is what +sep_preload_efuse does in the RTL testbench.
    //
    // They are deliberately not combined. Combining them can only mean ORing, since
    // fuses go 0->1 and assigning would let a parameter at its default of 0 unburn a
    // field the image had set — but ORing two encodings of one field corrupts it. The
    // clearest case is LC_STATE: an image holding 0xF0 (TEST_DEV) ORed with a parameter
    // encoding raw 1 (0xE1) yields 0xF1, which is not a legal differential code at all,
    // so the part would come up with no valid lifecycle state. A run that wants an image
    // with changes should change the image.
    const std::string preload = fuse_preload_file.get_param_value();
    const bool have_image = !preload.empty() && load_preload_file(preload);

    if (have_image) {
        CSML_INFO(1, logger) << "fuse_preload_file supplied: the image defines the fuse "
                                "array; per-field parameters are not applied"
                             << std::endl;
        sense_fuses_into_shadows();
        load_non_fuse_defaults();
        return;
    }

    // csml_reg::offset is a word index into the backing store, which is also the
    // array word index — the shadow map and the array are the same 8192 bits.
    auto set = [this](const csml_reg<32> &reg, uint32_t value) {
        fuse_or_word(reg.offset, value);
    };

    m_locks_lo_val = locks_lo.get_param_value();
    set(LOCKS_LO, m_locks_lo_val);

    // The parameter is the raw 4-bit state; the fuse holds the differential code.
    // Encoding here rather than asking configs to spell out 0xF0 keeps them readable
    // and keeps one spelling of the state across efuse and lifecycle_ctrl. A raw 0
    // encodes to 0xF0 rather than 0, so this is also what stops "no parameter set"
    // from looking like a legal TEST_DEV part.
    m_lc_state_val = lc_state_encode(lc_state.get_param_value());
    set(LC_STATE, m_lc_state_val);

    set(SBOOT_DIS,        sboot_dis.get_param_value());
    set(TRANSIENT_RMA_EN, transient_rma_en.get_param_value());

    m_sip_dis_lo_val = sip_dis_lo.get_param_value();
    m_sip_dis_hi_val = sip_dis_hi.get_param_value();
    set(SIP_DIS_LO, m_sip_dis_lo_val);
    set(SIP_DIS_HI, m_sip_dis_hi_val);

    m_sys_dis_lo_val = sys_dis_lo.get_param_value();
    m_sys_dis_hi_val = sys_dis_hi.get_param_value();
    set(SYS_DIS_LO, m_sys_dis_lo_val);
    set(SYS_DIS_HI, m_sys_dis_hi_val);

    auto rma_sip_v     = rma_sip_token.get_param_value();
    auto rma_chiplet_v = rma_chiplet_token.get_param_value();
    auto class_key_v   = class_key.get_param_value();
    for (int i = 0; i < 8; i++) {
        set(RMA_SIP_TOKEN[i],     vi(rma_sip_v, i));
        set(RMA_CHIPLET_TOKEN[i], vi(rma_chiplet_v, i));
        set(CLASS_KEY[i],         vi(class_key_v, i));
    }

    m_chiplet_pubk_revoke_val = chiplet_pubk_revoke.get_param_value();
    set(CHIPLET_PUBK_REVOKE, m_chiplet_pubk_revoke_val);

    auto bl1_v = bl1_version.get_param_value();
    auto bl2_v = bl2_version.get_param_value();
    for (int i = 0; i < 8; i++) {
        m_bl1_version_val[i] = vi(bl1_v, i);
        m_bl2_version_val[i] = vi(bl2_v, i);
        set(BL1_VERSION[i], m_bl1_version_val[i]);
        set(BL2_VERSION[i], m_bl2_version_val[i]);
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
        set(CHIPLET_UID[i],  m_chiplet_uid_cache[i]);
        set(SIP_UID[i],      vi(sip_uid_v, i));
        set(SYS_UID[i],      vi(sys_uid_v, i));
        set(SIP_PUBK[i],     vi(sip_pubk_v, i));
        set(SYS_PUBK[i],     vi(sys_pubk_v, i));
        set(PUBLIC_KEY_0[i], vi(pk0_v, i));
        set(PUBLIC_KEY_1[i], vi(pk1_v, i));
    }

    set(STATUS_RPT,            status_rpt.get_param_value());
    set(SEP_ROM_CTRL,          sep_rom_ctrl.get_param_value());
    set(SEP_SPI_CTRL_FIELD_EN, sep_spi_ctrl_field_en.get_param_value());
    set(SPI_DISCOVERY_CTRL,    spi_discovery_ctrl.get_param_value());
    set(SPI_PHY_DQ_TIMING,     spi_phy_dq_timing.get_param_value());
    set(SPI_PHY_DQS_TIMING,    spi_phy_dqs_timing.get_param_value());
    set(SPI_PHY_GATE_LPBK,     spi_phy_gate_lpbk.get_param_value());
    set(SPI_PHY_DLL_SLAVE,     spi_phy_dll_slave.get_param_value());
    set(SPI_PHY_DLL_MASTER,    spi_phy_dll_master.get_param_value());
    set(SPI_PHY_MISC,          spi_phy_misc.get_param_value());
    set(SPI_RB_VALID_TIME,     spi_rb_valid_time.get_param_value());

    sense_fuses_into_shadows();
    load_non_fuse_defaults();
}

void efuse_model::load_non_fuse_defaults()
{
    // No fuse holds these: in silicon they are comparator outputs, so they apply
    // whether the array came from an image or from parameters. Presetting them lets a
    // test start from a known match without staging a token first; they are otherwise
    // overwritten by the next TOKEN_EOP.
    RMA_SIP_TOKEN_MATCH     = rma_sip_token_match.get_param_value();
    RMA_CHIPLET_TOKEN_MATCH = rma_chiplet_token_match.get_param_value();
    SEC_DISABLE_TOKEN_MATCH = sec_disable_token_match.get_param_value();
}

void efuse_model::end_of_elaboration()
{
    // Sampled once: the strap is latched at reset in silicon and nothing changes it
    // afterwards, so re-reading the parameter per access could only introduce a
    // difference the hardware cannot have.
    m_secure_tm_active = secure_tm.get_param_value();
    if (m_secure_tm_active)
        CSML_WARN(0, logger) << "secure_tm asserted — fuse commands are blocked and "
                                "hardware secrets read as zero" << std::endl;

    load_fuses();
}

// =============================================================================
// WOSET Callback Handlers
// =============================================================================

bool efuse_model::handle_write_LOCKS_LO(uint32_t value)
{
    m_locks_lo_val |= value;
    LOCKS_LO = m_locks_lo_val;
    return true;
}

bool efuse_model::handle_write_LOCKS_HI(uint32_t value)
{
    m_locks_hi_val |= value;
    LOCKS_HI = m_locks_hi_val;
    return true;
}

// The WOSET fields below are the shadow registers software is allowed to set, so
// they are also the only ones where a write lock has anything to prevent: the rest
// of the map is read-only to software already, and dropping a write it could never
// have performed would be indistinguishable. Each returns true when refused, which
// completes the bus transaction normally -- a locked write is silently dropped in
// hardware, not turned into a fault.

/*
 * LC_STATE is the one shadow field that is not a plain woset: efuse_shadow_regs.sv
 * runs the write through a transition machine and stores the differential encoding of
 * the result, so the low byte is {~raw, raw} and a blind OR of the written word would
 * corrupt it -- 0xF0 (TEST_DEV) OR 0xE1 (PROD) is 0xF1, which is not an encoding at
 * all. The machine works on the raw nibble instead, which is also why firmware only
 * has to put the destination state in the low nibble for the write to take.
 *
 * The rules, in the order the RTL applies them:
 *   - PROD_END and both RMA_CHIPLET states are terminal, as is PROD once the lifecycle
 *     controller reports a demote (prod_dbg_active); no write moves them.
 *   - The intended destination, cur | write, must be a valid state, checked before any
 *     per-bit gating so that a write which would land nowhere is rejected whole.
 *   - Per bit: [0] PROD is ungated; [1] RMA_SIP needs the SiP token; [2] RMA_CHIPLET
 *     needs the chiplet token, and RMA_SIP already established, and not PROD; [3]
 *     PROD_END is unreachable from PROD.
 *   - What gating produced is checked again, since dropping a gated bit can leave an
 *     invalid state; if so the write is dropped.
 *
 * The RTL leaves the upper three bytes of the shadow word writable, but there they
 * carry neighbouring fuse fields; this model gives every field its own register, so
 * the upper bytes of this one name nothing and stay zero.
 */
bool efuse_model::handle_write_LC_STATE(uint32_t value)
{
    if (refuse_locked_write(LC_STATE.offset))
        return true;

    const uint32_t cur     = m_lc_state_val & 0xFu;
    const bool     is_prod = (cur == LC_RAW_PROD);
    const bool     frozen  = cur == LC_RAW_PROD_END   ||
                             cur == LC_RAW_RMA_CHIP_0 ||
                             cur == LC_RAW_RMA_CHIP_1 ||
                             (is_prod && m_prod_dbg_active);

    uint32_t next = cur;

    if (frozen) {
        if (value & 0xFu)
            CSML_REPORT(WARNING, "EFUSE", "LC_STATE transition ignored — state is terminal");
    } else if (!lc_state_raw_valid(cur | (value & 0xFu))) {
        CSML_REPORT(WARNING, "EFUSE", "LC_STATE write ignored — destination is not a valid state");
    } else {
        const uint32_t w         = value & 0xFu;
        const bool     sip_match = static_cast<uint32_t>(RMA_SIP_TOKEN_MATCH) == TOKEN_MATCH;
        const bool     chip_match= static_cast<uint32_t>(RMA_CHIPLET_TOKEN_MATCH) == TOKEN_MATCH;
        const bool     sip_done  = (cur & 0x2u) != 0;

        uint32_t cand = cur;
        cand |= w & 0x1u;
        if (sip_match)                            cand |= w & 0x2u;
        if (!is_prod && sip_done && chip_match)   cand |= w & 0x4u;
        if (!is_prod)                             cand |= w & 0x8u;

        if (lc_state_raw_valid(cand))
            next = cand;
        else
            CSML_REPORT(WARNING, "EFUSE", "LC_STATE write ignored — gating left an invalid state");
    }

    m_lc_state_val = lc_state_encode(next);
    LC_STATE = m_lc_state_val;
    notify_shadow_change();
    return true;
}

bool efuse_model::handle_write_SIP_DIS_LO(uint32_t value)
{
    if (refuse_locked_write(SIP_DIS_LO.offset))
        return true;
    m_sip_dis_lo_val |= value;
    SIP_DIS_LO = m_sip_dis_lo_val;
    notify_shadow_change();
    return true;
}

bool efuse_model::handle_write_SIP_DIS_HI(uint32_t value)
{
    if (refuse_locked_write(SIP_DIS_HI.offset))
        return true;
    m_sip_dis_hi_val |= value;
    SIP_DIS_HI = m_sip_dis_hi_val;
    notify_shadow_change();
    return true;
}

bool efuse_model::handle_write_SYS_DIS_LO(uint32_t value)
{
    if (refuse_locked_write(SYS_DIS_LO.offset))
        return true;
    m_sys_dis_lo_val |= value;
    SYS_DIS_LO = m_sys_dis_lo_val;
    notify_shadow_change();
    return true;
}

bool efuse_model::handle_write_SYS_DIS_HI(uint32_t value)
{
    if (refuse_locked_write(SYS_DIS_HI.offset))
        return true;
    m_sys_dis_hi_val |= value;
    SYS_DIS_HI = m_sys_dis_hi_val;
    notify_shadow_change();
    return true;
}

bool efuse_model::handle_write_CHIPLET_PUBK_REVOKE(uint32_t value)
{
    if (refuse_locked_write(CHIPLET_PUBK_REVOKE.offset))
        return true;
    m_chiplet_pubk_revoke_val |= value;
    CHIPLET_PUBK_REVOKE = m_chiplet_pubk_revoke_val;
    return true;
}

bool efuse_model::handle_write_BL1_VERSION(int idx, uint32_t value)
{
    if (refuse_locked_write(BL1_VERSION[idx].offset))
        return true;
    m_bl1_version_val[idx] |= value;
    BL1_VERSION[idx] = m_bl1_version_val[idx];
    return true;
}

bool efuse_model::handle_write_BL2_VERSION(int idx, uint32_t value)
{
    if (refuse_locked_write(BL2_VERSION[idx].offset))
        return true;
    m_bl2_version_val[idx] |= value;
    BL2_VERSION[idx] = m_bl2_version_val[idx];
    return true;
}

// =============================================================================
// Interface-Ctrl Program and Read Sequences
//
// Both complete within the write that pulses `go`, so a polling loop sees done on
// its very next read. That collapses the shim handshake and the OTP access time to
// zero, which software cannot distinguish from a fast macro: it only ever observes
// done, status and the data register.
// =============================================================================

// program_busy/done/status and read_busy/done/status are hardware-owned (hw=w,
// sw=r in the RDL), so a software write must leave them alone. The firmware's
// sequences depend on it: both deassert `go` with a second write while polling for
// done, and letting that write clear done would hang the poll forever.
static constexpr uint32_t HW_STATUS_BITS = (1u << 24) | (1u << 25) | (1u << 26);

/*
 * Secure test mode swallows fuse commands whole.
 *
 * efuse_guard.sv:110-114 replaces the request with an empty one and the response with
 * the default, and is explicit that this is not an error capture: no status bit is set
 * and req_error stays clear. Nothing is burned and nothing is read.
 *
 * Where the model has to differ: with the response tied off, the RTL's interface FSM
 * waits for a completion that never arrives, so it stalls until the request timeout
 * expires -- or forever, with the timeout disabled. b_transport cannot stall without
 * wedging the kernel, so the command completes immediately here, reporting done with
 * no error. The security-relevant half is exact; only the stall is missing.
 */
bool efuse_model::secure_tm_blocks_command(const char *what)
{
    if (!m_secure_tm_active)
        return false;
    CSML_WARN(1, logger) << what << " command dropped — secure_tm blocks the fuse "
                                    "interface" << std::endl;
    return true;
}

bool efuse_model::handle_write_EFUSE_PROGRAM_CTRL(uint32_t value)
{
    value = (value & ~HW_STATUS_BITS)
          | (static_cast<uint32_t>(EFUSE_PROGRAM_CTRL) & HW_STATUS_BITS);

    if (value & (1u << 17)) {   // efuse_program_go pulsed
        const uint32_t bit_addr  = value & 0xFFFFu;
        const bool     data      = (value & (1u << 16)) != 0;
        const bool     read_back = (value & (1u << 18)) != 0;
        const bool     enabled   = (value & (1u << 27)) != 0;

        value &= ~(1u << 17);   // singlepulse: never reads back as set
        value &= ~(1u << 24);   // program_busy = 0, the access is already over
        value |=  (1u << 25);   // program_done = 1

        bool error = false;

        if (secure_tm_blocks_command("program")) {
            // Dropped before the guard's error paths, so no status and no req_error.
            EFUSE_PROGRAM_INTERFACE_RD_DATA = 0u;
            value &= ~(1u << 26);
            EFUSE_PROGRAM_CTRL = value;
            update_status_register();
            return true;
        }

        if (!enabled) {
            // program_enable gates the command; efuse_program_interface.sv answers
            // done-with-error rather than stalling.
            error = true;
        } else if (!data) {
            // Programming a 0 is meaningless in a one-time-programmable array, and
            // the RTL rejects it outright instead of treating it as a no-op.
            error = true;
        } else if (bit_addr >= NUM_FUSE_BITS) {
            error = true;
            m_program_addr_error = true;
        } else if (is_write_locked(bit_addr / 32)) {
            // efuse_guard drops a locked command before it reaches the macro, so
            // nothing is burned and the interface reports the refusal.
            error = true;
        } else if (bit_addr == LC_STATE_BIT_POSITION + 1
                   && static_cast<uint32_t>(RMA_SIP_TOKEN_MATCH) != TOKEN_MATCH) {
            // The two RMA advance bits are the point of token matching: efuse_guard
            // refuses to burn either one unless the corresponding token has matched,
            // so possession of the token is what authorises the lifecycle transition.
            error = true;
        } else if (bit_addr == LC_STATE_BIT_POSITION + 2
                   && static_cast<uint32_t>(RMA_CHIPLET_TOKEN_MATCH) != TOKEN_MATCH) {
            error = true;
        } else {
            fuse_program_bit(bit_addr);
        }

        if (error) {
            value |= (1u << 26);   // program_status = 1
            m_req_error = true;
        } else {
            value &= ~(1u << 26);
        }

        // The read-back path returns the word as it now stands, so firmware
        // verifying its own program sees the bit it just burned.
        EFUSE_PROGRAM_INTERFACE_RD_DATA =
            (read_back && !error) ? fuse_read_word(bit_addr) : 0u;

        update_status_register();
    }
    EFUSE_PROGRAM_CTRL = value;
    return true;
}

bool efuse_model::handle_write_EFUSE_READ_CTRL(uint32_t value)
{
    value = (value & ~HW_STATUS_BITS)
          | (static_cast<uint32_t>(EFUSE_READ_CTRL) & HW_STATUS_BITS);

    if (value & (1u << 16)) {   // efuse_read_go pulsed
        const uint32_t bit_addr = value & 0xFFFFu;
        const bool     enabled  = (value & (1u << 28)) != 0;

        value &= ~(1u << 16);   // singlepulse
        value &= ~(1u << 24);   // read_busy = 0
        value |=  (1u << 25);   // read_done = 1

        bool error = false;

        if (secure_tm_blocks_command("read")) {
            EFUSE_READ_INTERFACE_RD_DATA = 0u;
            value &= ~(1u << 26);
            EFUSE_READ_CTRL = value;
            update_status_register();
            return true;
        }

        if (!enabled) {
            error = true;
        } else if (bit_addr >= NUM_FUSE_BITS) {
            error = true;
            m_read_addr_error = true;
        } else if (is_read_locked(bit_addr / 32)) {
            // Same guard as the program side. Unlike the shadow path, the OTP read
            // returns zero rather than DENY_WORD: the refusal is reported through
            // read_status and req_error, not smuggled into the data.
            error = true;
        }

        if (error) {
            value |= (1u << 26);   // read_status = 1
            m_req_error = true;
        } else {
            value &= ~(1u << 26);
        }

        // A refused read returns zero rather than stale data, so a caller that
        // ignores the status bit cannot mistake a denial for a cleared fuse.
        EFUSE_READ_INTERFACE_RD_DATA = error ? 0u : fuse_read_word(bit_addr);

        update_status_register();
    }
    EFUSE_READ_CTRL = value;
    return true;
}

// -----------------------------------------------------------------------------
// STATUS — sticky error bits with separate write-1-to-clear controls
// -----------------------------------------------------------------------------

void efuse_model::update_status_register()
{
    uint32_t status = 1u;  // efuse_sense_done: the array is sensed at elaboration
    if (m_req_error)          status |= (1u << 4);
    if (m_program_addr_error) status |= (1u << 5);
    if (m_read_addr_error)    status |= (1u << 6);
    EFUSE_INTERFACE_CTRL_STATUS = status;
}

bool efuse_model::handle_write_EFUSE_INTERFACE_CTRL_STATUS(uint32_t value)
{
    // The clear bits are the only writable ones, and they are pulses: writing 1
    // clears the matching error and the clear bit itself never reads back as set.
    if (value & (1u << 8))  m_req_error          = false;
    if (value & (1u << 9))  m_program_addr_error = false;
    if (value & (1u << 10)) m_read_addr_error    = false;

    // update_status_register() has already stored the authoritative value, so the
    // raw write must not be applied on top of it.
    update_status_register();
    return true;
}
