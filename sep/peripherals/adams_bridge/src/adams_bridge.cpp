// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file adams_bridge.cpp
 * @brief Loosely-timed implementation of the Adams Bridge PQC engine.
 *
 * Completion model (this is the part firmware depends on, see
 * sw/sep-vp-tests/.../common/abr_mldsa.h):
 *
 *   reset / zeroize : READY=1, VALID=0, ERROR=0
 *   command issued  : READY=0, VALID=0
 *   command done    : VALID=1, READY stays 0
 *   controller error: ERROR=1, READY and VALID stay 0
 *
 * Adams Bridge parks the sequencer at the operation's end state (or its error
 * state) rather than returning to RESET, so READY does not re-assert on
 * completion. Firmware must issue a zeroize between back-to-back operations, and
 * detects that zeroize by polling for READY=1.
 */

#include "adams_bridge.h"

#include <algorithm>
#include <cstring>

using sc_core::SC_NS;
using sc_core::SC_SEC;
using sc_core::SC_ZERO_TIME;
using sc_core::sc_time;

namespace {

/// MLDSA_NAME reads back "MLDSA-87" in the RTL's byte-pair-swapped layout.
constexpr unsigned int MLDSA_NAME0_VALUE = 0x44534D4Cu;
constexpr unsigned int MLDSA_NAME1_VALUE = 0x3837412Du;

/// MLKEM_NAME reads back "KEM-1024" in the same layout (abr_params_pkg.sv).
constexpr unsigned int MLKEM_NAME0_VALUE = 0x4D2D4B45u;
constexpr unsigned int MLKEM_NAME1_VALUE = 0x32343130u;

/// Both engines report version "2.0.1" (abr_params_pkg.sv MLDSA/MLKEM_CORE_VERSION).
constexpr unsigned int VERSION0_VALUE = 0x302E322Eu;
constexpr unsigned int VERSION1_VALUE = 0x00003100u;

constexpr unsigned int CTRL_CMD_MASK    = 0x7u;
constexpr unsigned int CTRL_ZEROIZE     = 1u << 3;
constexpr unsigned int CTRL_PCR_SIGN    = 1u << 4;
constexpr unsigned int CTRL_EXTERNAL_MU = 1u << 5;
constexpr unsigned int CTRL_STREAM_MSG  = 1u << 6;

constexpr unsigned int STROBE_ALL = 0xFu;

bool in_window(unsigned int word, unsigned int byte_offset, unsigned int count)
{
    return word >= abr::w(byte_offset) && word < abr::w(byte_offset) + count;
}

} // namespace

// =============================================================================
// Construction
// =============================================================================

abr_ip::abr_ip(sc_core::sc_module_name n, unsigned int memory_size)
    : abr_base(n, memory_size),
      clk_i("clk_i"),
      rst_ni("rst_ni"),
      intr_abr_error("intr_abr_error"),
      intr_abr_notif("intr_abr_notif"),
      keymgr_tl_socket("keymgr_tl_socket"),
      keymgr_mldsa_seed_socket("keymgr_mldsa_seed_socket"),
      keymgr_mlkem_d_socket("keymgr_mlkem_d_socket"),
      keymgr_mlkem_z_socket("keymgr_mlkem_z_socket"),
      keymgr_mlkem_msg_socket("keymgr_mlkem_msg_socket"),
      verbosity("verbosity", REG_DEFAULT_VERBOSITY),
      default_clk_freq_hz("default_clk_freq_hz", 100000000.0),
      // Cycle counts are order-of-magnitude figures for ML-DSA-87 / ML-KEM-1024
      // on the Adams Bridge datapath. They set relative cost, not cycle accuracy.
      mldsa_keygen_cycles("mldsa_keygen_cycles", 26000),
      mldsa_sign_cycles("mldsa_sign_cycles", 78000),
      mldsa_verify_cycles("mldsa_verify_cycles", 30000),
      mlkem_keygen_cycles("mlkem_keygen_cycles", 12000),
      mlkem_encaps_cycles("mlkem_encaps_cycles", 15000),
      mlkem_decaps_cycles("mlkem_decaps_cycles", 18000),
      // ABR_ZEROIZE sweeps ABR_MEM_MAX_DEPTH (1536) memory words and returns to
      // RESET two cycles later (abr_ctrl.sv:1708-1712, 2198-2212).
      zeroize_cycles("zeroize_cycles", 1538),
      kv_access_cycles("kv_access_cycles", 32),
      mldsa_stream_ready_cycles("mldsa_stream_ready_cycles", 2000),
      max_msg_stream_bytes("max_msg_stream_bytes", 1 << 20),
      m_qk(),
      m_crypto(std::make_unique<abr::abr_fips_backend>()),
      m_mldsa_cmd(abr::MldsaCmd::NONE),
      m_mlkem_cmd(abr::MlkemCmd::NONE),
      m_mldsa_zeroize_req(false),
      m_mlkem_zeroize_req(false),
      m_pcr_sign(false),
      m_external_mu(false),
      m_stream_msg(false),
      m_kv_mldsa_seed_valid(false),
      m_kv_mldsa_seed_present(false),
      m_kv_mlkem_seed_valid(false),
      m_kv_mlkem_msg_valid(false),
      m_stream_state(StreamState::IDLE),
      m_mldsa_epoch(0u),
      m_privkey_readable(false),
      m_in_reset(false)
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");

    m_qk.reset();

    // Inactive until interrupt_update_thread computes the first live value.
    // HMAC does the same; an uninitialized sc_out can glitch PIC sources 35/36.
    intr_abr_error.initialize(false);
    intr_abr_notif.initialize(false);

    // Identity registers are hardware-tied in RTL (RDL declares them hw=w with
    // no reset value), so seed both the live value and the reset value.
    MLDSA_NAME[0].reset_value = MLDSA_NAME0_VALUE;
    MLDSA_NAME[1].reset_value = MLDSA_NAME1_VALUE;
    MLDSA_VERSION[0].reset_value = VERSION0_VALUE;
    MLDSA_VERSION[1].reset_value = VERSION1_VALUE;
    MLKEM_NAME[0].reset_value = MLKEM_NAME0_VALUE;
    MLKEM_NAME[1].reset_value = MLKEM_NAME1_VALUE;
    MLKEM_VERSION[0].reset_value = VERSION0_VALUE;
    MLKEM_VERSION[1].reset_value = VERSION1_VALUE;

    // An idle engine is READY and asks for no message.
    MLDSA_STATUS.reset_value = 0x1u;

    reset_all_registers();

    for (auto &entry : m_kv_entries) {
        entry.assign(abr::KV_ENTRY_BYTES, 0u);
    }

    register_callbacks();
    keymgr_tl_socket.register_b_transport(this, &abr_ip::keymgr_b_transport);
    keymgr_mldsa_seed_socket.register_b_transport(this, &abr_ip::km_mldsa_seed_b_transport);
    keymgr_mlkem_d_socket.register_b_transport(this, &abr_ip::km_mlkem_d_b_transport);
    keymgr_mlkem_z_socket.register_b_transport(this, &abr_ip::km_mlkem_z_b_transport);
    keymgr_mlkem_msg_socket.register_b_transport(this, &abr_ip::km_mlkem_msg_b_transport);

    SC_METHOD(reset_handler);
    sensitive << rst_ni;
    dont_initialize();

    SC_THREAD(mldsa_engine_thread);
    SC_THREAD(mlkem_engine_thread);
    SC_THREAD(interrupt_update_thread);

    REG_INFO(1, logger) << "[ABR] Adams Bridge instantiated: aperture=" << memory_size
                         << " B, crypto backend=" << m_crypto->name()
                         << (m_crypto->is_standards_conformant()
                                 ? " (standards-conformant)"
                                 : " (functional stand-in, not NIST-conformant)")
                         << std::endl;
}

abr_ip::~abr_ip() = default; // LCOV_EXCL_LINE — sc_main uses quick_exit() so this never runs

void abr_ip::set_crypto_backend(std::unique_ptr<abr::abr_crypto_backend> backend)
{
    if (backend == nullptr) {
        REG_ERROR(0, logger) << "[ABR] set_crypto_backend(nullptr) ignored" << std::endl;
        return;
    }
    m_crypto = std::move(backend);
    REG_INFO(1, logger) << "[ABR] crypto backend replaced with " << m_crypto->name()
                         << std::endl;
}

const char *abr_ip::crypto_backend_name() const
{
    return m_crypto->name();
}

bool abr_ip::crypto_backend_is_conformant() const
{
    return m_crypto->is_standards_conformant();
}

// =============================================================================
// Callback registration
// =============================================================================

void abr_ip::register_callbacks()
{
    memory.register_write_callback(
        [this](DT v) { return mldsa_ctrl_write(v); }, abr::w(abr::OFF_MLDSA_CTRL));
    memory.register_write_callback(
        [this](DT v) { return mlkem_ctrl_write(v); }, abr::w(abr::OFF_MLKEM_CTRL));

    // While the core asks for the message, MSG[0] writes feed the stream instead
    // of the fixed 16-word block.
    for (unsigned int i = 0; i < abr::N_MLDSA_MSG; ++i) {
        memory.register_write_callback(
            [this, i](DT v) { return mldsa_msg_write(i, v); },
            abr::w(abr::OFF_MLDSA_MSG) + i);
    }
    memory.register_write_callback([this](DT v) { return mldsa_msg_strobe_write(v); },
                                   abr::w(abr::OFF_MLDSA_MSG_STROBE));

    memory.register_write_callback([this](DT v) { return kv_mldsa_seed_rd_ctrl_write(v); },
                                   abr::w(abr::OFF_KV_MLDSA_SEED_RD_CTRL));
    memory.register_write_callback([this](DT v) { return kv_mlkem_seed_rd_ctrl_write(v); },
                                   abr::w(abr::OFF_KV_MLKEM_SEED_RD_CTRL));
    memory.register_write_callback([this](DT v) { return kv_mlkem_msg_rd_ctrl_write(v); },
                                   abr::w(abr::OFF_KV_MLKEM_MSG_RD_CTRL));
    memory.register_write_callback(
        [this](DT v) { return kv_mlkem_sharedkey_wr_ctrl_write(v); },
        abr::w(abr::OFF_KV_MLKEM_SHAREDKEY_WR_CTRL));

    memory.register_write_callback([this](DT v) { return error_internal_intr_write(v); },
                                   abr::w(abr::OFF_ERROR_INTERNAL_INTR));
    memory.register_write_callback([this](DT v) { return notif_internal_intr_write(v); },
                                   abr::w(abr::OFF_NOTIF_INTERNAL_INTR));
    memory.register_write_callback([this](DT v) { return error_intr_trig_write(v); },
                                   abr::w(abr::OFF_ERROR_INTR_TRIG));
    memory.register_write_callback([this](DT v) { return notif_intr_trig_write(v); },
                                   abr::w(abr::OFF_NOTIF_INTR_TRIG));

    // Enable changes can expose or mask an already-pending status bit.
    memory.register_write_callback(
        [this](DT v) {
            global_intr_en_r.handle_write(v, global_intr_en_r.write_bit_mask);
            return intr_enable_write(v);
        },
        abr::w(abr::OFF_GLOBAL_INTR_EN));
    memory.register_write_callback(
        [this](DT v) {
            error_intr_en_r.handle_write(v, error_intr_en_r.write_bit_mask);
            return intr_enable_write(v);
        },
        abr::w(abr::OFF_ERROR_INTR_EN));
    memory.register_write_callback(
        [this](DT v) {
            notif_intr_en_r.handle_write(v, notif_intr_en_r.write_bit_mask);
            return intr_enable_write(v);
        },
        abr::w(abr::OFF_NOTIF_INTR_EN));
}

// =============================================================================
// Timing helpers
// =============================================================================

double abr_ip::clk_freq()
{
    // clk_i carries a frequency, not a toggling clock; it may be unbound in
    // register-only testbenches.
    double freq = 0.0;
    if (clk_i.get_interface() != nullptr) {
        freq = clk_i.read();
    }
    if (freq <= 0.0) {
        freq = default_clk_freq_hz.get_param_value();
    }
    return (freq > 0.0) ? freq : 100000000.0;
}

sc_time abr_ip::cycles_to_time(int cycles)
{
    if (cycles <= 0) {
        return SC_ZERO_TIME;
    }
    return sc_time(static_cast<double>(cycles) / clk_freq(), SC_SEC);
}

void abr_ip::consume_cycles(int cycles)
{
    const sc_time t = cycles_to_time(cycles);
    if (t == SC_ZERO_TIME) {
        return;
    }

    // Temporal decoupling: accumulate locally and only hand control back to the
    // kernel once the global quantum is exhausted.
    m_qk.inc(t);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }
}

// =============================================================================
// Reset
// =============================================================================

void abr_ip::reset_handler()
{
    if (rst_ni.read()) {
        m_in_reset = false;
        return;
    }

    m_in_reset = true;

    reset_all_registers();

    ++m_mldsa_epoch;
    m_mldsa_abort_ev.notify(SC_ZERO_TIME);
    m_stream_state = StreamState::IDLE;
    m_privkey_readable = false;
    m_mldsa_cmd = abr::MldsaCmd::NONE;
    m_mlkem_cmd = abr::MlkemCmd::NONE;
    m_mldsa_zeroize_req = false;
    m_mlkem_zeroize_req = false;
    m_pcr_sign = false;
    m_external_mu = false;
    m_stream_msg = false;

    m_msg_stream.clear();
    m_pcr_digest.clear();
    m_kv_mldsa_seed.clear();
    m_kv_mlkem_seed.clear();
    m_kv_mlkem_msg.clear();
    m_last_shared_key.clear();
    m_kv_mldsa_seed_valid = false;
    m_kv_mldsa_seed_present = false;
    m_kv_mlkem_seed_valid = false;
    m_kv_mlkem_msg_valid = false;

    for (auto &entry : m_kv_entries) {
        std::fill(entry.begin(), entry.end(), 0u);
    }
    for (auto &lane : m_km_share) {
        lane = km_share_lane{};
    }

    // The interrupt outputs have a single driver (interrupt_update_thread), so
    // reset asks it to recompute rather than writing the ports here -- SystemC
    // rejects a second writer on an sc_signal.
    request_interrupt_update();

    m_qk.reset();

    REG_INFO(2, logger) << "[ABR] reset complete" << std::endl;
}

// =============================================================================
// Register <-> byte conversion (little-endian 32-bit words, matching
// SEP firmware and NIST ACVP packing: byte b lives in word[b/4] bits (b%4)*8).
// =============================================================================

template <typename Vec>
void abr_ip::regs_to_bytes(Vec &regs, unsigned int count, abr::bytes &out)
{
    out.assign(static_cast<std::size_t>(count) * 4u, 0u);
    for (unsigned int i = 0; i < count; ++i) {
        const unsigned int word = static_cast<unsigned int>(regs[i]);
        out[i * 4u + 0u] = static_cast<uint8_t>(word & 0xFFu);
        out[i * 4u + 1u] = static_cast<uint8_t>((word >> 8) & 0xFFu);
        out[i * 4u + 2u] = static_cast<uint8_t>((word >> 16) & 0xFFu);
        out[i * 4u + 3u] = static_cast<uint8_t>((word >> 24) & 0xFFu);
    }
}

template <typename Vec>
void abr_ip::bytes_to_regs(const abr::bytes &in, Vec &regs, unsigned int count)
{
    for (unsigned int i = 0; i < count; ++i) {
        const std::size_t b = static_cast<std::size_t>(i) * 4u;
        unsigned int word = 0u;
        for (unsigned int lane = 0u; lane < 4u; ++lane) {
            if (b + lane < in.size()) {
                word |= static_cast<unsigned int>(in[b + lane]) << (lane * 8u);
            }
        }
        regs[i] = word;
    }
}

template <typename Vec>
void abr_ip::clear_regs(Vec &regs, unsigned int count)
{
    for (unsigned int i = 0; i < count; ++i) {
        regs[i] = 0u;
    }
}

// =============================================================================
// Status helpers
// =============================================================================

void abr_ip::set_mldsa_busy()
{
    MLDSA_STATUS.READY = 0u;
    MLDSA_STATUS.VALID = 0u;
    MLDSA_STATUS.ERROR = 0u;
    // The engine only accepts streamed message data while it is idle.
    MLDSA_STATUS.MSG_STREAM_READY = 0u;
}

void abr_ip::finish_mldsa()
{
    // The sequencer parks at the operation's end state, so READY stays low
    // until a zeroize. Firmware relies on this to detect completion via VALID.
    m_stream_state = StreamState::IDLE;
    MLDSA_STATUS.MSG_STREAM_READY = 0u;
    MLDSA_STATUS.VALID = 1u;
    MLDSA_STATUS.ERROR = 0u;
    raise_notif();
}

void abr_ip::set_mlkem_busy()
{
    MLKEM_STATUS.READY = 0u;
    MLKEM_STATUS.VALID = 0u;
    MLKEM_STATUS.ERROR = 0u;
}

void abr_ip::finish_mlkem()
{
    if (m_qk.get_local_time() > SC_ZERO_TIME) {
        m_qk.sync();
    }

    MLKEM_STATUS.VALID = 1u;
    MLKEM_STATUS.ERROR = 0u;
    raise_notif();
}

// =============================================================================
// Interrupts
// =============================================================================

void abr_ip::raise_notif()
{
    notif_internal_intr_r.sts = 1u;
    {
        const auto count = static_cast<uint32_t>(notif_cmd_done_intr_count_r);
        if (count != 0xffffffffu)
            notif_cmd_done_intr_count_r = count + 1u;
    }
    notif_cmd_done_intr_count_incr_r.pulse = 1u;
    request_interrupt_update();
}

void abr_ip::raise_error()
{
    error_internal_intr_r.sts = 1u;
    {
        const auto count = static_cast<uint32_t>(error_internal_intr_count_r);
        if (count != 0xffffffffu)
            error_internal_intr_count_r = count + 1u;
    }
    error_internal_intr_count_incr_r.pulse = 1u;
    request_interrupt_update();
}

void abr_ip::request_interrupt_update()
{
    m_intr_update_ev.notify(SC_ZERO_TIME);
}

void abr_ip::interrupt_update_thread()
{
    while (true) {
        wait(m_intr_update_ev);

        const bool err_pending = (error_internal_intr_r.sts.get() != 0u) &&
                                 (error_intr_en_r.en.get() != 0u);
        const bool notif_pending = (notif_internal_intr_r.sts.get() != 0u) &&
                                   (notif_intr_en_r.en.get() != 0u);

        error_global_intr_r.agg_sts = err_pending ? 1u : 0u;
        notif_global_intr_r.agg_sts = notif_pending ? 1u : 0u;

        intr_abr_error.write(err_pending && (global_intr_en_r.error_en.get() != 0u));
        intr_abr_notif.write(notif_pending && (global_intr_en_r.notif_en.get() != 0u));
    }
}

bool abr_ip::error_internal_intr_write(DT value)
{
    if ((value & 0x1u) != 0u) {
        error_internal_intr_r.sts = 0u;
        error_internal_intr_count_incr_r.pulse = 0u;
        request_interrupt_update();
    }
    return true;
}

bool abr_ip::notif_internal_intr_write(DT value)
{
    if ((value & 0x1u) != 0u) {
        notif_internal_intr_r.sts = 0u;
        notif_cmd_done_intr_count_incr_r.pulse = 0u;
        request_interrupt_update();
    }
    return true;
}

bool abr_ip::error_intr_trig_write(DT value)
{
    if ((value & 0x1u) != 0u) {
        // Write-1-to-set test trigger; the trigger bit itself is self-clearing.
        raise_error();
    }
    return true;
}

bool abr_ip::notif_intr_trig_write(DT value)
{
    if ((value & 0x1u) != 0u) {
        raise_notif();
    }
    return true;
}

bool abr_ip::intr_enable_write(DT)
{
    request_interrupt_update();
    return true;
}

// =============================================================================
// CTRL decode
// =============================================================================

bool abr_ip::mldsa_ctrl_write(DT value)
{
    // CTRL is write-only and hardware-cleared once latched, so nothing is
    // retained in the register shadow.
    MLDSA_CTRL = 0u;

    // ZEROIZE is a single-cycle pulse honoured in any state; every other field is
    // written only while READY (abr_reg.rdl swwe = abr_ready).
    if (m_in_reset) {
        return true;
    }
    if ((value & CTRL_ZEROIZE) != 0u) {
        request_mldsa_zeroize();
        return true;
    }
    if (MLDSA_STATUS.READY.get() == 0u) {
        return true;
    }

    // CTRL is written whole, so STREAM_MSG takes the written value and holds it
    // until zeroize.
    m_stream_msg = (value & CTRL_STREAM_MSG) != 0u;

    // NONE and the unused encodings 5-7 leave the sequencer in RESET
    // (abr_ctrl.sv ABR_RESET default branch).
    const unsigned int cmd = value & CTRL_CMD_MASK;
    if (cmd == static_cast<unsigned int>(abr::MldsaCmd::NONE) ||
        cmd > static_cast<unsigned int>(abr::MldsaCmd::KEYGEN_SIGN)) {
        return true;
    }

    m_mldsa_cmd = static_cast<abr::MldsaCmd>(cmd);
    m_pcr_sign = (value & CTRL_PCR_SIGN) != 0u;
    m_external_mu = (value & CTRL_EXTERNAL_MU) != 0u;
    m_privkey_readable = false;
    set_mldsa_busy();

    // VERIFY preloads the inverted challenge, so a verify that stops early can
    // never leave a result equal to c~ (abr_ctrl.sv:808-811).
    if (m_mldsa_cmd == abr::MldsaCmd::VERIFYING) {
        for (unsigned int i = 0; i < abr::N_VERIFY_RES; ++i) {
            MLDSA_VERIFY_RES[i] = ~static_cast<unsigned int>(MLDSA_SIGNATURE[i]);
        }
    }

    // PCR_SIGN is legal only with KEYGEN+SIGN; otherwise the controller error
    // flag parks the sequencer until zeroize (abr_ctrl.sv pcr_sign_input_invalid).
    if (m_pcr_sign && m_mldsa_cmd != abr::MldsaCmd::KEYGEN_SIGN) {
        m_mldsa_cmd = abr::MldsaCmd::NONE;
        MLDSA_STATUS.ERROR = 1u;
        raise_error();
        return true;
    }

    m_mldsa_start_ev.notify(SC_ZERO_TIME);
    return true;
}

void abr_ip::request_mldsa_zeroize()
{
    ++m_mldsa_epoch;
    m_mldsa_abort_ev.notify(SC_ZERO_TIME);
    m_mldsa_cmd = abr::MldsaCmd::NONE;
    m_stream_state = StreamState::IDLE;
    m_mldsa_zeroize_req = true;
    set_mldsa_busy();
    m_mldsa_start_ev.notify(SC_ZERO_TIME);
}

bool abr_ip::mlkem_ctrl_write(DT value)
{
    const unsigned int cmd = value & 0x7u;
    const bool zeroize_req = (value & (1u << 3)) != 0u;

    MLKEM_CTRL = 0u;

    if (zeroize_req) {
        // The RTL has one zeroize for both engines (abr_ctrl.sv:721), and it clears
        // the ML-DSA state too, so a running ML-DSA operation must end with it.
        request_mldsa_zeroize();
        m_mlkem_zeroize_req = true;
        set_mlkem_busy();
        m_mlkem_start_ev.notify(SC_ZERO_TIME);
        return true;
    }

    if (cmd == static_cast<unsigned int>(abr::MlkemCmd::NONE)) {
        return true;
    }

    if (cmd > static_cast<unsigned int>(abr::MlkemCmd::KEYGEN_DECAPS)) {
        REG_DEBUG(3, logger) << "[ABR] MLKEM_CTRL invalid command " << cmd << std::endl;
        set_mlkem_busy();
        m_mlkem_cmd = abr::MlkemCmd::NONE;
        // Same as MLDSA_CTRL: this is a b_transport callback, so no wait().
        MLKEM_STATUS.VALID = 0u;
        MLKEM_STATUS.ERROR = 1u;
        raise_error();
        return true;
    }

    m_mlkem_cmd = static_cast<abr::MlkemCmd>(cmd);
    set_mlkem_busy();
    m_mlkem_start_ev.notify(SC_ZERO_TIME);
    return true;
}

bool abr_ip::mldsa_msg_write(unsigned int index, DT value)
{
    // MSG is writable while READY or while the core asks for the message
    // (swwe = abr_ready | stream_msg_rdy); only MSG[0] writes in the latter feed
    // the stream (abr_ctrl.sv:793, 1838-1840).
    if (m_stream_state != StreamState::RDY) {
        if (MLDSA_STATUS.READY.get() != 0u) {
            MLDSA_MSG[index] = value;
        }
        return true;
    }

    MLDSA_MSG[index] = value;
    if (index != 0u) {
        return true;
    }

    const unsigned int strobe = MLDSA_MSG_STROBE.STROBE.get();
    const std::size_t cap = static_cast<std::size_t>(std::max(0, max_msg_stream_bytes.get_param_value()));
    for (unsigned int b = 0; b < 4u; ++b) {
        if ((strobe & (1u << b)) == 0u) {
            continue;
        }
        if (m_msg_stream.size() < cap) {
            m_msg_stream.push_back(static_cast<uint8_t>((value >> (b * 8u)) & 0xFFu));
        } else {
            REG_WARN(1, logger) << "[ABR] message stream exceeded " << cap
                                 << " B; byte dropped" << std::endl;
        }
    }

    // Any strobe other than all-four ends the stream, including zero.
    if (strobe != STROBE_ALL) {
        m_stream_state = StreamState::DONE;
        MLDSA_STATUS.MSG_STREAM_READY = 0u;
        m_stream_done_ev.notify(SC_ZERO_TIME);
    }
    return true;
}

bool abr_ip::mldsa_msg_strobe_write(DT value)
{
    // MSG_STROBE is writable only while the core asks for the message.
    if (m_stream_state == StreamState::RDY) {
        MLDSA_MSG_STROBE = value & STROBE_ALL;
    }
    return true;
}

bool abr_ip::bus_write_allowed(unsigned int word) const
{
    // abr_ctrl.sv: SEED and the seed read control swwe = abr_ready & ~kv_mldsa_seed_data_present.
    if (m_kv_mldsa_seed_present && (in_window(word, abr::OFF_MLDSA_SEED, abr::N_MLDSA_SEED) ||
                                    in_window(word, abr::OFF_KV_MLDSA_SEED_RD_CTRL, 1u))) {
        return false;
    }
    if (MLDSA_STATUS.READY.get() != 0u) {
        return true;
    }
    return !(in_window(word, abr::OFF_ABR_ENTROPY, abr::N_ENTROPY) ||
             in_window(word, abr::OFF_MLDSA_SEED, abr::N_MLDSA_SEED) ||
             in_window(word, abr::OFF_MLDSA_SIGN_RND, abr::N_SIGN_RND) ||
             in_window(word, abr::OFF_MLDSA_EXTERNAL_MU, abr::N_EXTERNAL_MU) ||
             in_window(word, abr::OFF_MLDSA_CTX_CONFIG, 1u) ||
             in_window(word, abr::OFF_MLDSA_CTX, abr::N_MLDSA_CTX) ||
             in_window(word, abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) ||
             in_window(word, abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE) ||
             in_window(word, abr::OFF_MLDSA_PRIVKEY_IN, abr::N_MLDSA_PRIVKEY) ||
             in_window(word, abr::OFF_KV_MLDSA_SEED_RD_CTRL, 1u));
}

bool abr_ip::bus_read_masked(unsigned int word) const
{
    const bool valid = MLDSA_STATUS.VALID.get() != 0u;
    if (in_window(word, abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) ||
        in_window(word, abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE)) {
        return !valid;
    }
    if (in_window(word, abr::OFF_MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY)) {
        return !(valid && m_privkey_readable);
    }
    return false;
}

// =============================================================================
// Key Vault
// =============================================================================

void abr_ip::load_kv_entry(unsigned int entry, const abr::bytes &material)
{
    if (entry >= abr::KV_NUM_ENTRIES) {
        REG_WARN(1, logger) << "[ABR] load_kv_entry: entry " << entry
                             << " out of range" << std::endl;
        return;
    }

    m_kv_entries[entry].assign(abr::KV_ENTRY_BYTES, 0u);
    const std::size_t n = std::min<std::size_t>(material.size(), abr::KV_ENTRY_BYTES);
    std::copy_n(material.begin(), n, m_kv_entries[entry].begin());
}

void abr_ip::set_pcr_digest(abr::bytes digest)
{
    m_pcr_digest = std::move(digest);
}

void abr_ip::keymgr_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
    const sc_dt::uint64 addr = trans.get_address();
    unsigned char *ptr = trans.get_data_ptr();
    const unsigned int len = trans.get_data_length();

    if (ptr == nullptr || (addr + len) > (abr::KV_NUM_ENTRIES * abr::KV_ENTRY_BYTES)) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        for (unsigned int i = 0; i < len; ++i) {
            const std::size_t flat = static_cast<std::size_t>(addr) + i;
            m_kv_entries[flat / abr::KV_ENTRY_BYTES][flat % abr::KV_ENTRY_BYTES] = ptr[i];
        }
        REG_DEBUG(3, logger) << "[ABR] KV sideload write " << len << " B at 0x" << std::hex
                           << addr << std::dec << std::endl;
    } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        for (unsigned int i = 0; i < len; ++i) {
            const std::size_t flat = static_cast<std::size_t>(addr) + i;
            ptr[i] = m_kv_entries[flat / abr::KV_ENTRY_BYTES][flat % abr::KV_ENTRY_BYTES];
        }
    }

    delay += cycles_to_time(kv_access_cycles.get_param_value());
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void abr_ip::km_mldsa_seed_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
    km_share_b_transport(abr::KV_ENTRY_MLDSA_SEED, trans, delay);
}

void abr_ip::km_mlkem_d_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
    km_share_b_transport(abr::KV_ENTRY_MLKEM_D, trans, delay);
}

void abr_ip::km_mlkem_z_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
    km_share_b_transport(abr::KV_ENTRY_MLKEM_Z, trans, delay);
}

void abr_ip::km_mlkem_msg_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
{
    km_share_b_transport(abr::KV_ENTRY_MLKEM_MSG, trans, delay);
}

void abr_ip::km_share_b_transport(unsigned int lane, tlm::tlm_generic_payload &trans,
                                  sc_time &delay)
{
    if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    unsigned char *ptr = trans.get_data_ptr();
    const unsigned int len = trans.get_data_length();
    const sc_dt::uint64 offset = trans.get_address();
    if (ptr == nullptr || len < 4u || (offset % 4u) != 0u) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    uint32_t data = 0u;
    std::memcpy(&data, ptr, sizeof(data));
    km_share_lane &s = m_km_share[lane];

    if (offset <= 0x1Cu) {
        s.share0[offset / 4u] = data;
    } else if (offset >= abr::KM_SHARE1_BASE && offset <= (abr::KM_SHARE1_BASE + 0x1Cu)) {
        s.share1[(offset - abr::KM_SHARE1_BASE) / 4u] = data;
    } else if (offset == abr::KM_KEY_CTRL_OFF) {
        const bool valid = (data & 0x1u) != 0u;
        abr::bytes material(abr::KV_ENTRY_BYTES, 0u);
        if (valid) {
            for (unsigned int i = 0; i < abr::KM_SHARE_WORDS; ++i) {
                const uint32_t word = s.share0[i] ^ s.share1[i];
                material[i * 4u + 0u] = static_cast<uint8_t>(word & 0xFFu);
                material[i * 4u + 1u] = static_cast<uint8_t>((word >> 8) & 0xFFu);
                material[i * 4u + 2u] = static_cast<uint8_t>((word >> 16) & 0xFFu);
                material[i * 4u + 3u] = static_cast<uint8_t>((word >> 24) & 0xFFu);
            }
        } else {
            s = km_share_lane{};
        }
        load_kv_entry(lane, material);
        REG_DEBUG(3, logger) << "[ABR] KM share lane " << lane
                              << (valid ? " committed" : " shredded") << std::endl;
    } else {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    delay += cycles_to_time(kv_access_cycles.get_param_value());
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

bool abr_ip::kv_fetch(unsigned int entry, abr::bytes &dest, unsigned int bytes_wanted)
{
    // read_entry is a 5-bit field and KV_NUM_ENTRIES is 32, so every caller
    // passes an in-range index. Keep the guard so a future wider field cannot
    // walk off m_kv_entries.
    if (entry >= abr::KV_NUM_ENTRIES) {
        return false; // LCOV_EXCL_LINE — read_entry is 5 bits, so this is unreachable
    }

    const abr::bytes &src = m_kv_entries[entry];
    const bool has_material =
        std::any_of(src.begin(), src.end(), [](uint8_t b) { return b != 0u; });
    if (!has_material) {
        return false;
    }

    dest.assign(src.begin(), src.begin() + std::min<std::size_t>(bytes_wanted, src.size()));
    dest.resize(bytes_wanted, 0u);
    return true;
}

void abr_ip::reverse_le_dwords(abr::bytes &v)
{
    const std::size_t nwords = v.size() / 4u;
    for (std::size_t i = 0; i < nwords / 2u; ++i) {
        for (std::size_t b = 0; b < 4u; ++b) {
            std::swap(v[i * 4u + b], v[(nwords - 1u - i) * 4u + b]);
        }
    }
}

bool abr_ip::mldsa_acquire_message(uint64_t epoch, abr::bytes &ctx, abr::bytes &msg)
{
    ctx.clear();
    if (m_pcr_sign && !m_pcr_digest.empty()) {
        msg = m_pcr_digest;
        return true;
    }
    if (!m_stream_msg) {
        // Hashed as 0 || 0 || MSG: a non-streamed message has an empty context
        // whatever CTX_CONFIG holds (abr_ctrl.sv msg_p_reg).
        regs_to_bytes(MLDSA_MSG, abr::N_MLDSA_MSG, msg);
        return true;
    }

    const unsigned int ctx_len = MLDSA_CTX_CONFIG.CTX_SIZE.get();
    if (ctx_len > 0u) {
        abr::bytes ctx_words;
        regs_to_bytes(MLDSA_CTX, abr::N_MLDSA_CTX, ctx_words);
        ctx.assign(ctx_words.begin(), ctx_words.begin() + ctx_len);
    }

    m_msg_stream.clear();
    m_stream_state = StreamState::RDY;
    MLDSA_STATUS.MSG_STREAM_READY = 1u;
    while (m_stream_state == StreamState::RDY && epoch == m_mldsa_epoch) {
        wait(m_stream_done_ev | m_mldsa_abort_ev);
    }
    if (epoch != m_mldsa_epoch) {
        return false;
    }
    msg = m_msg_stream;
    m_msg_stream.clear();
    return true;
}

bool abr_ip::kv_mldsa_seed_rd_ctrl_write(DT value)
{
    kv_mldsa_seed_rd_ctrl.handle_write(value, kv_mldsa_seed_rd_ctrl.write_bit_mask);

    if ((value & 0x1u) == 0u) {
        return true;
    }

    m_kv_mldsa_seed_present = true;
    const unsigned int entry = (value >> 1) & 0x1Fu;
    const bool ok = kv_fetch(entry, m_kv_mldsa_seed, abr::MLDSA_SEED_BYTES);
    if (ok) {
        // abr_ctrl.sv writes KV-sourced seed in reverse dword order
        // (SEED_NUM_DWORDS-1-dword). Palindromic seeds are invariant.
        reverse_le_dwords(m_kv_mldsa_seed);
    }

    m_kv_mldsa_seed_valid = ok;
    kv_mldsa_seed_rd_status.VALID = ok ? 1u : 0u;
    kv_mldsa_seed_rd_status.ERROR = ok ? abr::KV_SUCCESS : abr::KV_READ_FAIL;
    kv_mldsa_seed_rd_status.READY = 1u;

    // read_en is hardware-cleared once the copy completes.
    kv_mldsa_seed_rd_ctrl.read_en = 0u;

    REG_DEBUG(3, logger) << "[ABR] KV ML-DSA seed read entry " << entry << ": "
                       << (ok ? "ok" : "fail") << std::endl;
    return true;
}

bool abr_ip::kv_mlkem_seed_rd_ctrl_write(DT value)
{
    kv_mlkem_seed_rd_ctrl.handle_write(value, kv_mlkem_seed_rd_ctrl.write_bit_mask);

    if ((value & 0x1u) == 0u) {
        return true;
    }

    const unsigned int entry = (value >> 1) & 0x1Fu;
    const bool ok = kv_fetch(entry, m_kv_mlkem_seed, abr::MLKEM_SEED_BYTES);

    m_kv_mlkem_seed_valid = ok;
    kv_mlkem_seed_rd_status.VALID = ok ? 1u : 0u;
    kv_mlkem_seed_rd_status.ERROR = ok ? abr::KV_SUCCESS : abr::KV_READ_FAIL;
    kv_mlkem_seed_rd_status.READY = 1u;
    kv_mlkem_seed_rd_ctrl.read_en = 0u;
    return true;
}

bool abr_ip::kv_mlkem_msg_rd_ctrl_write(DT value)
{
    kv_mlkem_msg_rd_ctrl.handle_write(value, kv_mlkem_msg_rd_ctrl.write_bit_mask);

    if ((value & 0x1u) == 0u) {
        return true;
    }

    const unsigned int entry = (value >> 1) & 0x1Fu;
    const bool ok = kv_fetch(entry, m_kv_mlkem_msg, abr::MLKEM_MSG_BYTES);

    m_kv_mlkem_msg_valid = ok;
    kv_mlkem_msg_rd_status.VALID = ok ? 1u : 0u;
    kv_mlkem_msg_rd_status.ERROR = ok ? abr::KV_SUCCESS : abr::KV_READ_FAIL;
    kv_mlkem_msg_rd_status.READY = 1u;
    kv_mlkem_msg_rd_ctrl.read_en = 0u;
    return true;
}

bool abr_ip::kv_mlkem_sharedkey_wr_ctrl_write(DT value)
{
    kv_mlkem_sharedkey_wr_ctrl.handle_write(value, kv_mlkem_sharedkey_wr_ctrl.write_bit_mask);

    if ((value & 0x1u) == 0u) {
        return true;
    }

    const unsigned int entry = (value >> 1) & 0x1Fu;
    const bool ok = (entry < abr::KV_NUM_ENTRIES) &&
                    (m_last_shared_key.size() == abr::MLKEM_SHARED_KEY_BYTES);

    if (ok) {
        load_kv_entry(entry, m_last_shared_key);
    }

    kv_mlkem_sharedkey_wr_status.VALID = ok ? 1u : 0u;
    kv_mlkem_sharedkey_wr_status.ERROR = ok ? abr::KV_SUCCESS : abr::KV_WRITE_FAIL;
    kv_mlkem_sharedkey_wr_status.READY = 1u;
    kv_mlkem_sharedkey_wr_ctrl.write_en = 0u;
    return true;
}

// =============================================================================
// Zeroize
// =============================================================================

void abr_ip::zeroize()
{
    clear_regs(ABR_ENTROPY, abr::N_ENTROPY);
    clear_regs(MLDSA_SEED, abr::N_MLDSA_SEED);
    clear_regs(MLDSA_SIGN_RND, abr::N_SIGN_RND);
    clear_regs(MLDSA_MSG, abr::N_MLDSA_MSG);
    clear_regs(MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    clear_regs(MLDSA_EXTERNAL_MU, abr::N_EXTERNAL_MU);
    clear_regs(MLDSA_CTX, abr::N_MLDSA_CTX);
    clear_regs(MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    clear_regs(MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    clear_regs(MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY);
    clear_regs(MLDSA_PRIVKEY_IN, abr::N_MLDSA_PRIVKEY);

    clear_regs(MLKEM_SEED_D, abr::N_MLKEM_SEED);
    clear_regs(MLKEM_SEED_Z, abr::N_MLKEM_SEED);
    clear_regs(MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY);
    clear_regs(MLKEM_MSG, abr::N_MLKEM_MSG);
    clear_regs(MLKEM_DECAPS_KEY, abr::N_MLKEM_DECAPS_KEY);
    clear_regs(MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY);
    clear_regs(MLKEM_CIPHERTEXT, abr::N_MLKEM_CIPHERTEXT);

    MLDSA_CTX_CONFIG = 0u;
    MLDSA_MSG_STROBE = 0u;

    m_msg_stream.clear();
    m_stream_state = StreamState::IDLE;
    m_privkey_readable = false;

    std::fill(m_kv_mldsa_seed.begin(), m_kv_mldsa_seed.end(), 0u);
    std::fill(m_kv_mlkem_seed.begin(), m_kv_mlkem_seed.end(), 0u);
    std::fill(m_kv_mlkem_msg.begin(), m_kv_mlkem_msg.end(), 0u);
    std::fill(m_last_shared_key.begin(), m_last_shared_key.end(), 0u);
    m_kv_mldsa_seed.clear();
    m_kv_mlkem_seed.clear();
    m_kv_mlkem_msg.clear();
    m_last_shared_key.clear();
    m_kv_mldsa_seed_valid = false;
    m_kv_mldsa_seed_present = false;
    m_kv_mlkem_seed_valid = false;
    m_kv_mlkem_msg_valid = false;

    m_pcr_sign = false;
    m_external_mu = false;
    m_stream_msg = false;

    REG_DEBUG(3, logger) << "[ABR] zeroize complete" << std::endl;
}

// =============================================================================
// ML-DSA engine
// =============================================================================

bool abr_ip::mldsa_wait(int cycles, uint64_t epoch)
{
    const sc_time t = cycles_to_time(cycles);
    if (t > SC_ZERO_TIME && epoch == m_mldsa_epoch) {
        wait(t, m_mldsa_abort_ev);
    }
    return epoch == m_mldsa_epoch;
}

void abr_ip::mldsa_engine_thread()
{
    while (true) {
        if (!m_mldsa_zeroize_req && m_mldsa_cmd == abr::MldsaCmd::NONE) {
            wait(m_mldsa_start_ev);
        }

        const uint64_t epoch = m_mldsa_epoch;
        // An abort notified for an earlier epoch must not cut this one's waits short.
        m_mldsa_abort_ev.cancel();

        if (m_mldsa_zeroize_req) {
            m_mldsa_zeroize_req = false;
            zeroize();
            if (!mldsa_wait(zeroize_cycles.get_param_value(), epoch)) {
                continue;
            }
            // Zeroize is the only path that returns the sequencer to RESET.
            MLDSA_STATUS.READY = 1u;
            MLDSA_STATUS.VALID = 0u;
            MLDSA_STATUS.ERROR = 0u;
            MLDSA_STATUS.MSG_STREAM_READY = 0u;
            continue;
        }

        const abr::MldsaCmd cmd = m_mldsa_cmd;
        m_mldsa_cmd = abr::MldsaCmd::NONE;
        switch (cmd) {
        case abr::MldsaCmd::KEYGEN:
            do_mldsa_keygen(epoch);
            break;
        case abr::MldsaCmd::SIGNING:
            do_mldsa_sign(false, epoch);
            break;
        case abr::MldsaCmd::VERIFYING:
            do_mldsa_verify(epoch);
            break;
        case abr::MldsaCmd::KEYGEN_SIGN:
            do_mldsa_sign(true, epoch);
            break;
        case abr::MldsaCmd::NONE: // LCOV_EXCL_LINE — only zeroize wakes the engine with no command
        default:                  // LCOV_EXCL_LINE
            break;                // LCOV_EXCL_LINE
        }
    }
}

void abr_ip::do_mldsa_keygen(uint64_t epoch)
{
    abr::bytes seed;
    const bool kv_seed = m_kv_mldsa_seed_valid;
    if (kv_seed) {
        seed = m_kv_mldsa_seed;
    } else {
        regs_to_bytes(MLDSA_SEED, abr::N_MLDSA_SEED, seed);
    }

    abr::bytes pubkey;
    abr::bytes privkey;
    m_crypto->mldsa_keygen(seed, pubkey, privkey);

    if (!mldsa_wait(mldsa_keygen_cycles.get_param_value(), epoch)) {
        return;
    }

    bytes_to_regs(pubkey, MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    bytes_to_regs(privkey, MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY);
    // The private key is released only by a standalone KEYGEN from a software
    // seed (abr_ctrl.sv mldsa_privkey_lock).
    m_privkey_readable = !kv_seed;

    REG_INFO(2, logger) << "[ABR] ML-DSA keygen complete" << std::endl;
    finish_mldsa();
}

void abr_ip::do_mldsa_sign(bool keygen_first, uint64_t epoch)
{
    abr::bytes privkey;

    if (keygen_first) {
        abr::bytes seed;
        if (m_kv_mldsa_seed_valid) {
            seed = m_kv_mldsa_seed;
        } else {
            regs_to_bytes(MLDSA_SEED, abr::N_MLDSA_SEED, seed);
        }

        abr::bytes pubkey;
        m_crypto->mldsa_keygen(seed, pubkey, privkey);
        if (!mldsa_wait(mldsa_keygen_cycles.get_param_value(), epoch)) {
            return;
        }

        bytes_to_regs(pubkey, MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
        bytes_to_regs(privkey, MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY);
    } else {
        regs_to_bytes(MLDSA_PRIVKEY_IN, abr::N_MLDSA_PRIVKEY, privkey);
    }

    abr::bytes mu;
    if (m_external_mu) {
        regs_to_bytes(MLDSA_EXTERNAL_MU, abr::N_EXTERNAL_MU, mu);
    } else {
        abr::bytes ctx;
        abr::bytes msg;
        if (!mldsa_wait(mldsa_stream_ready_cycles.get_param_value(), epoch) ||
            !mldsa_acquire_message(epoch, ctx, msg)) {
            return;
        }
        m_crypto->mldsa_compute_mu_from_sk(privkey, ctx, msg, mu);
    }

    abr::bytes rnd;
    regs_to_bytes(MLDSA_SIGN_RND, abr::N_SIGN_RND, rnd);

    abr::bytes signature;
    m_crypto->mldsa_sign(privkey, mu, rnd, signature);

    if (!mldsa_wait(mldsa_sign_cycles.get_param_value(), epoch)) {
        return;
    }

    bytes_to_regs(signature, MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);

    REG_INFO(2, logger) << "[ABR] ML-DSA sign complete (keygen_first=" << keygen_first
                         << ", external_mu=" << m_external_mu << ")" << std::endl;
    finish_mldsa();
}

void abr_ip::do_mldsa_verify(uint64_t epoch)
{
    abr::bytes pubkey;
    regs_to_bytes(MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY, pubkey);

    abr::bytes signature;
    regs_to_bytes(MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE, signature);

    // The RTL decodes the signature and range-checks z before it reaches the
    // message hash, and decodes the hint only after it (abr_seq.sv:521-537, 649).
    // Either failure ends the operation with the preloaded ~c~ in VERIFY_RES.
    if (!mldsa_wait(mldsa_stream_ready_cycles.get_param_value(), epoch)) {
        return;
    }
    const abr::MldsaSigEncoding encoding = m_crypto->mldsa_check_signature(signature);
    if (encoding == abr::MldsaSigEncoding::Z_OUT_OF_RANGE) {
        REG_INFO(2, logger) << "[ABR] ML-DSA verify: z out of range" << std::endl;
        finish_mldsa();
        return;
    }

    abr::bytes mu;
    if (m_external_mu) {
        regs_to_bytes(MLDSA_EXTERNAL_MU, abr::N_EXTERNAL_MU, mu);
    } else {
        abr::bytes ctx;
        abr::bytes msg;
        if (!mldsa_acquire_message(epoch, ctx, msg)) {
            return;
        }
        m_crypto->mldsa_compute_mu(pubkey, ctx, msg, mu);
    }

    abr::bytes ctilde;
    const bool computed = encoding == abr::MldsaSigEncoding::OK &&
                          m_crypto->mldsa_verify(pubkey, mu, signature, ctilde);

    if (!mldsa_wait(mldsa_verify_cycles.get_param_value(), epoch)) {
        return;
    }

    // Hardware reports the recomputed c-tilde; firmware compares it against the
    // first 64 bytes of the signature. There is no pass/fail status bit.
    if (computed) {
        bytes_to_regs(ctilde, MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    }

    REG_INFO(2, logger) << "[ABR] ML-DSA verify complete" << std::endl;
    finish_mldsa();
}

// =============================================================================
// ML-KEM engine
// =============================================================================

void abr_ip::mlkem_engine_thread()
{
    while (true) {
        wait(m_mlkem_start_ev);

        if (m_in_reset) {
            continue;
        }

        if (m_mlkem_zeroize_req) {
            m_mlkem_zeroize_req = false;
            zeroize();
            consume_cycles(zeroize_cycles.get_param_value());
            if (m_qk.get_local_time() > SC_ZERO_TIME) {
                m_qk.sync();
            }
            MLKEM_STATUS.READY = 1u;
            MLKEM_STATUS.VALID = 0u;
            MLKEM_STATUS.ERROR = 0u;
            continue;
        }

        switch (m_mlkem_cmd) {
        case abr::MlkemCmd::KEYGEN:
            do_mlkem_keygen();
            break;
        case abr::MlkemCmd::ENCAPS:
            do_mlkem_encaps();
            break;
        case abr::MlkemCmd::DECAPS:
            do_mlkem_decaps(false);
            break;
        case abr::MlkemCmd::KEYGEN_DECAPS:
            do_mlkem_decaps(true);
            break;
        case abr::MlkemCmd::NONE: // LCOV_EXCL_LINE — invalid cmds never wake the engine
        default:                  // LCOV_EXCL_LINE
            break;                // LCOV_EXCL_LINE
        }

        m_mlkem_cmd = abr::MlkemCmd::NONE;
    }
}

void abr_ip::do_mlkem_keygen()
{
    abr::bytes d;
    if (m_kv_mlkem_seed_valid) {
        d = m_kv_mlkem_seed;
    } else {
        regs_to_bytes(MLKEM_SEED_D, abr::N_MLKEM_SEED, d);
    }

    abr::bytes z;
    regs_to_bytes(MLKEM_SEED_Z, abr::N_MLKEM_SEED, z);

    abr::bytes ek;
    abr::bytes dk;
    m_crypto->mlkem_keygen(d, z, ek, dk);

    consume_cycles(mlkem_keygen_cycles.get_param_value());

    bytes_to_regs(ek, MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY);
    bytes_to_regs(dk, MLKEM_DECAPS_KEY, abr::N_MLKEM_DECAPS_KEY);

    REG_INFO(2, logger) << "[ABR] ML-KEM keygen complete" << std::endl;
    finish_mlkem();
}

void abr_ip::do_mlkem_encaps()
{
    abr::bytes ek;
    regs_to_bytes(MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY, ek);

    abr::bytes msg;
    if (m_kv_mlkem_msg_valid) {
        msg = m_kv_mlkem_msg;
    } else {
        regs_to_bytes(MLKEM_MSG, abr::N_MLKEM_MSG, msg);
    }

    abr::bytes ct;
    abr::bytes ss;
    m_crypto->mlkem_encaps(ek, msg, ct, ss);

    consume_cycles(mlkem_encaps_cycles.get_param_value());

    bytes_to_regs(ct, MLKEM_CIPHERTEXT, abr::N_MLKEM_CIPHERTEXT);
    bytes_to_regs(ss, MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY);
    m_last_shared_key = ss;

    REG_INFO(2, logger) << "[ABR] ML-KEM encaps complete" << std::endl;
    finish_mlkem();
}

void abr_ip::do_mlkem_decaps(bool keygen_first)
{
    abr::bytes dk;

    if (keygen_first) {
        abr::bytes d;
        if (m_kv_mlkem_seed_valid) {
            d = m_kv_mlkem_seed;
        } else {
            regs_to_bytes(MLKEM_SEED_D, abr::N_MLKEM_SEED, d);
        }

        abr::bytes z;
        regs_to_bytes(MLKEM_SEED_Z, abr::N_MLKEM_SEED, z);

        abr::bytes ek;
        m_crypto->mlkem_keygen(d, z, ek, dk);
        consume_cycles(mlkem_keygen_cycles.get_param_value());

        bytes_to_regs(ek, MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY);
        bytes_to_regs(dk, MLKEM_DECAPS_KEY, abr::N_MLKEM_DECAPS_KEY);
    } else {
        regs_to_bytes(MLKEM_DECAPS_KEY, abr::N_MLKEM_DECAPS_KEY, dk);
    }

    abr::bytes ct;
    regs_to_bytes(MLKEM_CIPHERTEXT, abr::N_MLKEM_CIPHERTEXT, ct);

    abr::bytes ss;
    m_crypto->mlkem_decaps(dk, ct, ss);

    consume_cycles(mlkem_decaps_cycles.get_param_value());

    bytes_to_regs(ss, MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY);
    m_last_shared_key = ss;

    REG_INFO(2, logger) << "[ABR] ML-KEM decaps complete (keygen_first=" << keygen_first
                         << ")" << std::endl;
    finish_mlkem();
}
