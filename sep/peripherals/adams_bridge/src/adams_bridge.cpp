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
 *
 * Adams Bridge parks the sequencer at the operation's end state rather than
 * returning to RESET, so READY does not re-assert on completion. Firmware must
 * issue a zeroize between back-to-back operations, and detects that zeroize by
 * polling for READY=1.
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

/// MLKEM_NAME follows the same encoding for "MLKEM-10". Not checked by any
/// current firmware test; present so the identity registers are never blank.
constexpr unsigned int MLKEM_NAME0_VALUE = 0x4B454D4Cu;
constexpr unsigned int MLKEM_NAME1_VALUE = 0x31304D2Du;

constexpr unsigned int VERSION0_VALUE = 0x00000001u;
constexpr unsigned int VERSION1_VALUE = 0x00000000u;

/// Upper bound on the streamed-message accumulator, so a runaway producer
/// cannot grow it without limit.
constexpr std::size_t MAX_MSG_STREAM_BYTES = 65536u;

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
      zeroize_cycles("zeroize_cycles", 64),
      kv_access_cycles("kv_access_cycles", 32),
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
      m_kv_mlkem_seed_valid(false),
      m_kv_mlkem_msg_valid(false),
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

    // READY | MSG_STREAM_READY: an idle engine can accept a streamed message.
    MLDSA_STATUS.reset_value = 0x5u;

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

    // Streaming-message mode appends MSG writes to an accumulator instead of
    // treating the 16 words as a fixed-size block.
    for (unsigned int i = 0; i < abr::N_MLDSA_MSG; ++i) {
        memory.register_write_callback(
            [this, i](DT v) { return mldsa_msg_write(i, v); },
            abr::w(abr::OFF_MLDSA_MSG) + i);
    }

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
    // Publish the result at the correct simulated time: an LT model must
    // resynchronize before a side effect becomes visible to other processes.
    // consume_cycles() already synced when the latency exceeded the quantum;
    // this catches a sub-quantum operation that only accumulated local time.
    if (m_qk.get_local_time() > SC_ZERO_TIME) {
        m_qk.sync();
    }

    // The sequencer parks at the operation's end state, so READY stays low
    // until a zeroize. Firmware relies on this to detect completion via VALID.
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
    const unsigned int cmd = value & 0x7u;
    const bool zeroize_req = (value & (1u << 3)) != 0u;

    m_pcr_sign = (value & (1u << 4)) != 0u;
    m_external_mu = (value & (1u << 5)) != 0u;
    m_stream_msg = (value & (1u << 6)) != 0u;

    // CTRL is write-only and hardware-cleared once latched, so nothing is
    // retained in the register shadow.
    MLDSA_CTRL = 0u;

    if (zeroize_req) {
        m_mldsa_zeroize_req = true;
        set_mldsa_busy();
        m_mldsa_start_ev.notify(SC_ZERO_TIME);
        return true;
    }

    if (cmd == static_cast<unsigned int>(abr::MldsaCmd::NONE)) {
        return true;
    }

    if (cmd > static_cast<unsigned int>(abr::MldsaCmd::KEYGEN_SIGN)) {
        REG_DEBUG(3, logger) << "[ABR] MLDSA_CTRL invalid command " << cmd << std::endl;
        set_mldsa_busy();
        m_mldsa_cmd = abr::MldsaCmd::NONE;
        // Do not call finish_mldsa(): it may m_qk.sync()/wait(), and this
        // callback runs inside b_transport.
        MLDSA_STATUS.VALID = 0u;
        MLDSA_STATUS.ERROR = 1u;
        raise_error();
        return true;
    }

    m_mldsa_cmd = static_cast<abr::MldsaCmd>(cmd);
    set_mldsa_busy();
    m_mldsa_start_ev.notify(SC_ZERO_TIME);
    return true;
}

bool abr_ip::mlkem_ctrl_write(DT value)
{
    const unsigned int cmd = value & 0x7u;
    const bool zeroize_req = (value & (1u << 3)) != 0u;

    MLKEM_CTRL = 0u;

    if (zeroize_req) {
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
    MLDSA_MSG[index] = value;

    // Accumulate unconditionally: firmware streams the message *before* issuing
    // the command that carries CTRL.STREAM_MSG, so the flag is not yet set when
    // these writes arrive. The command decides whether to consume the stream or
    // the fixed 16-word MSG block.
    //
    // MSG_STROBE selects which bytes of the word are live, which is how a
    // message that is not a multiple of 4 bytes terminates.
    if (m_msg_stream.size() < MAX_MSG_STREAM_BYTES) {
        const unsigned int strobe = MLDSA_MSG_STROBE.STROBE.get();
        for (unsigned int b = 0; b < 4u; ++b) {
            if ((strobe & (1u << b)) != 0u) {
                const unsigned int shift = b * 8u;
                m_msg_stream.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
            }
        }
    } else {
        REG_WARN(1, logger) << "[ABR] message stream exceeded "
                             << MAX_MSG_STREAM_BYTES << " B; write dropped"
                             << std::endl;
    }

    return true;
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

void abr_ip::resolve_sign_message(abr::bytes &msg)
{
    if (m_pcr_sign && !m_pcr_digest.empty()) {
        msg = m_pcr_digest;
        return;
    }
    if (m_stream_msg && !m_msg_stream.empty()) {
        msg = m_msg_stream;
        return;
    }
    regs_to_bytes(MLDSA_MSG, abr::N_MLDSA_MSG, msg);
}

bool abr_ip::kv_mldsa_seed_rd_ctrl_write(DT value)
{
    kv_mldsa_seed_rd_ctrl.handle_write(value, kv_mldsa_seed_rd_ctrl.write_bit_mask);

    if ((value & 0x1u) == 0u) {
        return true;
    }

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
    MLDSA_MSG_STROBE.reset();

    m_msg_stream.clear();

    std::fill(m_kv_mldsa_seed.begin(), m_kv_mldsa_seed.end(), 0u);
    std::fill(m_kv_mlkem_seed.begin(), m_kv_mlkem_seed.end(), 0u);
    std::fill(m_kv_mlkem_msg.begin(), m_kv_mlkem_msg.end(), 0u);
    std::fill(m_last_shared_key.begin(), m_last_shared_key.end(), 0u);
    m_kv_mldsa_seed.clear();
    m_kv_mlkem_seed.clear();
    m_kv_mlkem_msg.clear();
    m_last_shared_key.clear();
    m_kv_mldsa_seed_valid = false;
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

void abr_ip::mldsa_engine_thread()
{
    while (true) {
        wait(m_mldsa_start_ev);

        if (m_in_reset) {
            continue;
        }

        if (m_mldsa_zeroize_req) {
            m_mldsa_zeroize_req = false;
            zeroize();
            consume_cycles(zeroize_cycles.get_param_value());
            if (m_qk.get_local_time() > SC_ZERO_TIME) {
                m_qk.sync();
            }
            // Zeroize is the only path that returns the sequencer to RESET.
            MLDSA_STATUS.READY = 1u;
            MLDSA_STATUS.VALID = 0u;
            MLDSA_STATUS.ERROR = 0u;
            MLDSA_STATUS.MSG_STREAM_READY = 1u;
            continue;
        }

        switch (m_mldsa_cmd) {
        case abr::MldsaCmd::KEYGEN:
            do_mldsa_keygen();
            break;
        case abr::MldsaCmd::SIGNING:
            do_mldsa_sign(false);
            break;
        case abr::MldsaCmd::VERIFYING:
            do_mldsa_verify();
            break;
        case abr::MldsaCmd::KEYGEN_SIGN:
            do_mldsa_sign(true);
            break;
        case abr::MldsaCmd::NONE: // LCOV_EXCL_LINE — invalid cmds never wake the engine
        default:                  // LCOV_EXCL_LINE
            break;                // LCOV_EXCL_LINE
        }

        m_mldsa_cmd = abr::MldsaCmd::NONE;
    }
}

void abr_ip::do_mldsa_keygen()
{
    abr::bytes seed;
    if (m_kv_mldsa_seed_valid) {
        seed = m_kv_mldsa_seed;
    } else {
        regs_to_bytes(MLDSA_SEED, abr::N_MLDSA_SEED, seed);
    }

    abr::bytes pubkey;
    abr::bytes privkey;
    m_crypto->mldsa_keygen(seed, pubkey, privkey);

    consume_cycles(mldsa_keygen_cycles.get_param_value());

    bytes_to_regs(pubkey, MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    bytes_to_regs(privkey, MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY);

    REG_INFO(2, logger) << "[ABR] ML-DSA keygen complete" << std::endl;
    finish_mldsa();
}

void abr_ip::do_mldsa_sign(bool keygen_first)
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
        consume_cycles(mldsa_keygen_cycles.get_param_value());

        bytes_to_regs(pubkey, MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
        bytes_to_regs(privkey, MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY);
    } else {
        regs_to_bytes(MLDSA_PRIVKEY_IN, abr::N_MLDSA_PRIVKEY, privkey);
    }

    abr::bytes mu;
    if (m_external_mu) {
        regs_to_bytes(MLDSA_EXTERNAL_MU, abr::N_EXTERNAL_MU, mu);
    } else {
        abr::bytes msg;
        resolve_sign_message(msg);

        abr::bytes ctx;
        const unsigned int ctx_len = MLDSA_CTX_CONFIG.CTX_SIZE.get();
        if (ctx_len > 0u) {
            abr::bytes ctx_words;
            regs_to_bytes(MLDSA_CTX, abr::N_MLDSA_CTX, ctx_words);
            ctx.assign(ctx_words.begin(),
                       ctx_words.begin() + std::min<std::size_t>(ctx_len, ctx_words.size()));
        }

        m_crypto->mldsa_compute_mu_from_sk(privkey, ctx, msg, mu);
    }

    abr::bytes rnd;
    regs_to_bytes(MLDSA_SIGN_RND, abr::N_SIGN_RND, rnd);

    abr::bytes signature;
    m_crypto->mldsa_sign(privkey, mu, rnd, signature);

    consume_cycles(mldsa_sign_cycles.get_param_value());

    bytes_to_regs(signature, MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    m_msg_stream.clear();

    REG_INFO(2, logger) << "[ABR] ML-DSA sign complete (keygen_first=" << keygen_first
                         << ", external_mu=" << m_external_mu << ")" << std::endl;
    finish_mldsa();
}

void abr_ip::do_mldsa_verify()
{
    abr::bytes pubkey;
    regs_to_bytes(MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY, pubkey);

    abr::bytes signature;
    regs_to_bytes(MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE, signature);

    abr::bytes mu;
    if (m_external_mu) {
        regs_to_bytes(MLDSA_EXTERNAL_MU, abr::N_EXTERNAL_MU, mu);
    } else {
        abr::bytes msg;
        resolve_sign_message(msg);

        abr::bytes ctx;
        const unsigned int ctx_len = MLDSA_CTX_CONFIG.CTX_SIZE.get();
        if (ctx_len > 0u) {
            abr::bytes ctx_words;
            regs_to_bytes(MLDSA_CTX, abr::N_MLDSA_CTX, ctx_words);
            ctx.assign(ctx_words.begin(),
                       ctx_words.begin() + std::min<std::size_t>(ctx_len, ctx_words.size()));
        }

        m_crypto->mldsa_compute_mu(pubkey, ctx, msg, mu);
    }

    abr::bytes ctilde;
    m_crypto->mldsa_verify(pubkey, mu, signature, ctilde);

    consume_cycles(mldsa_verify_cycles.get_param_value());

    // Hardware reports the recomputed c-tilde; firmware compares it against the
    // first 64 bytes of the signature. There is no pass/fail status bit.
    bytes_to_regs(ctilde, MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    m_msg_stream.clear();

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
