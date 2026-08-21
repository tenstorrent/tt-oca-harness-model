/**
 * @file adams_bridge.h
 * @brief Loosely-timed TLM-2.0 model of the Adams Bridge (ABR) PQC engine.
 *
 * Adams Bridge is the post-quantum crypto accelerator in the SEP crypto
 * subsystem (tt-oca-hw/hw/sep/sep_crypto.sv, gated by SEP_ABR_EN). It exposes
 * ML-DSA-87 (FIPS 204) and ML-KEM-1024 (FIPS 203) behind one 64 KiB aperture
 * at SEP address 0x1094_0000 (sep_crypto_pkg::abr_rule).
 *
 * Modeling style
 * --------------
 * Loosely-timed TLM-2.0 with temporal decoupling via
 * tlm_utils::tlm_quantumkeeper, matching the other SEP crypto IPs (aes,
 * csrng, aon_timer):
 *
 *   - Register accesses are functional and untimed; csml_memory decodes them
 *     straight off the target socket.
 *   - Command execution runs in an SC_THREAD per algorithm engine. The engine
 *     charges the modeled operation latency to the quantum keeper with
 *     m_qk.inc(), then yields to the kernel only when the quantum is exhausted
 *     (m_qk.need_sync() / m_qk.sync()). Long PQC operations therefore cost
 *     simulated time without forcing a context switch per command.
 *   - Latency is derived from a modeled cycle count and the clk_i frequency,
 *     so it tracks the platform clock rather than being hard-coded. Cycle
 *     counts are csml_param tunables.
 *   - No cycle accuracy is claimed or attempted: only the ready/valid
 *     handshake ordering that firmware observes is guaranteed.
 *
 * The ML-DSA and ML-KEM engines are independent sequencers in hardware and are
 * modeled as two threads, so a command on one does not stall the other.
 *
 * Cryptographic fidelity
 * ----------------------
 * The math is delegated to an abr_crypto_backend (see abr_crypto.h). The
 * default is abr_fips_backend (PQClean ML-DSA-87 + ML-KEM-1024) and is
 * expected to match NIST ACVP vectors. abr_shake_backend remains available
 * via set_crypto_backend() for tests that only need round-trip behaviour.
 */

#pragma once

#include "abr_base.h"
#include "abr_crypto.h"

#include "csml_logger.h"
#include "csml_parameter.h"

#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

namespace abr {

/// MLDSA_CTRL.CTRL command encoding (abr_reg.rdl).
enum class MldsaCmd : unsigned int {
    NONE        = 0u,
    KEYGEN      = 1u,
    SIGNING     = 2u,
    VERIFYING   = 3u,
    KEYGEN_SIGN = 4u
};

/// MLKEM_CTRL.CTRL command encoding (abr_reg.rdl).
enum class MlkemCmd : unsigned int {
    NONE          = 0u,
    KEYGEN        = 1u,
    ENCAPS        = 2u,
    DECAPS        = 3u,
    KEYGEN_DECAPS = 4u
};

/// kv_*_rd_status.ERROR / kv_*_wr_status.ERROR encoding.
enum KvError : unsigned int {
    KV_SUCCESS    = 0u,
    KV_READ_FAIL  = 1u,
    KV_WRITE_FAIL = 2u
};

/// Number of Key Vault entries visible to ABR (kv_rd_ctrl.read_entry is 5 bits).
static constexpr unsigned int KV_NUM_ENTRIES = 32u;

/// Bytes per Key Vault entry (512-bit slots, of which ABR consumes 256 bits).
static constexpr unsigned int KV_ENTRY_BYTES = 64u;

/// HMAC-style dual-share sideload (matches hmac_wrapper_key.rdl / ABR wrapper_key_reg).
static constexpr unsigned int KM_SHARE_WORDS  = 8u;
static constexpr uint32_t     KM_SHARE0_BASE  = 0x00u;
static constexpr uint32_t     KM_SHARE1_BASE  = 0x20u;
static constexpr uint32_t     KM_KEY_CTRL_OFF = 0x40u;

/// KV entries served by the KM share sockets (firmware kv_read[0] = ML-DSA seed).
static constexpr unsigned int KV_ENTRY_MLDSA_SEED = 0u;
static constexpr unsigned int KV_ENTRY_MLKEM_D    = 1u;
static constexpr unsigned int KV_ENTRY_MLKEM_Z    = 2u;
static constexpr unsigned int KV_ENTRY_MLKEM_MSG  = 3u;

} // namespace abr

/**
 * @brief Adams Bridge functional model.
 *
 * Layers on top of abr_base (register declarations) with the command FSMs,
 * status/interrupt semantics, zeroization, Key Vault sideload and the
 * quantum-keeper-based timing described in the file comment.
 */
class abr_ip : public abr_base
{
  public:
    SC_HAS_PROCESS(abr_ip);

    /**
     * @brief Construct the ABR model.
     * @param n           SystemC instance name.
     * @param memory_size Aperture size in bytes; defaults to the RTL's 64 KiB.
     */
    explicit abr_ip(sc_core::sc_module_name n,
                    unsigned int memory_size = abr::ABR_APERTURE_SIZE);

    ~abr_ip() override;

    // =========================================================================
    // Ports
    // =========================================================================

    /// Clock frequency in Hz (abstract, not a pin-level clock). Drives latency.
    sc_core::sc_in<double> clk_i;

    /// Active-low reset. Falling edge clears all state and registers.
    sc_core::sc_in<bool> rst_ni;

    /// intr_abr_error_o — aggregated error interrupt (SEP PIC source 35).
    sc_core::sc_out<bool> intr_abr_error;

    /// intr_abr_notif_o — aggregated command-done interrupt (SEP PIC source 36).
    sc_core::sc_out<bool> intr_abr_notif;

    /**
     * @brief Flat Key Vault sideload port (unit tests / generic KV push).
     *
     * Address is the byte offset into the entry array
     * (`entry * KV_ENTRY_BYTES + n`). The key manager does **not** use this
     * socket; it writes HMAC-style XOR shares on the `keymgr_*_socket` ports
     * below, which reconstruct into entries 0–3.
     */
    tlm_utils::simple_target_socket_optional<abr_ip, 32> keymgr_tl_socket;

    /// KM DEST_ABR_MLDSA_SEED (0x10): dual-share reconstruct → KV entry 0.
    tlm_utils::simple_target_socket<abr_ip, 32> keymgr_mldsa_seed_socket;
    /// KM DEST_ABR_MLKEM_D (0x20): dual-share reconstruct → KV entry 1.
    tlm_utils::simple_target_socket<abr_ip, 32> keymgr_mlkem_d_socket;
    /// KM DEST_ABR_MLKEM_Z (0x40): dual-share reconstruct → KV entry 2.
    tlm_utils::simple_target_socket<abr_ip, 32> keymgr_mlkem_z_socket;
    /// KM DEST_ABR_MLKEM_MSG (0x80): dual-share reconstruct → KV entry 3.
    tlm_utils::simple_target_socket<abr_ip, 32> keymgr_mlkem_msg_socket;

    // =========================================================================
    // Configuration parameters
    // =========================================================================

    /// Logging verbosity: 0=error, 1=warn, 2=info, 3=debug.
    csml_param<int> verbosity;

    /// Fallback clock frequency (Hz) used when clk_i is unbound or zero.
    csml_param<double> default_clk_freq_hz;

    /// Modeled engine latencies, in core clock cycles.
    /// @{
    csml_param<int> mldsa_keygen_cycles;
    csml_param<int> mldsa_sign_cycles;
    csml_param<int> mldsa_verify_cycles;
    csml_param<int> mlkem_keygen_cycles;
    csml_param<int> mlkem_encaps_cycles;
    csml_param<int> mlkem_decaps_cycles;
    csml_param<int> zeroize_cycles;
    csml_param<int> kv_access_cycles;
    /// @}

    // =========================================================================
    // Test / integration hooks
    // =========================================================================

    /**
     * @brief Replace the cryptographic backend.
     *
     * Call before simulation starts. Passing nullptr is ignored so the model
     * always has a usable backend.
     */
    void set_crypto_backend(std::unique_ptr<abr::abr_crypto_backend> backend);

    /// Name of the installed backend (for tests and elaboration logging).
    const char *crypto_backend_name() const;

    /// True when the installed backend claims FIPS/ACVP bit-exactness.
    bool crypto_backend_is_conformant() const;

    /**
     * @brief Back-door load of a Key Vault entry, bypassing the sideload port.
     * @param entry Entry index (< KV_NUM_ENTRIES); out-of-range is ignored.
     */
    void load_kv_entry(unsigned int entry, const abr::bytes &material);

    /**
     * @brief Install the PCR digest used when CTRL.PCR_SIGN is set.
     *
     * The VP has no PCR bank. Platforms / tests that want PCR_SIGN to hash
     * something other than MSG call this with the vault digest. An empty
     * buffer restores the MSG fallback (benches without a PCR source).
     */
    void set_pcr_digest(abr::bytes digest);

  private:
    // =========================================================================
    // SystemC processes
    // =========================================================================

    /// Reset process: clears registers, state and outputs on rst_ni low.
    void reset_handler();

    /// ML-DSA sequencer; waits on m_mldsa_start_ev.
    void mldsa_engine_thread();

    /// ML-KEM sequencer; waits on m_mlkem_start_ev.
    void mlkem_engine_thread();

    /// Recomputes aggregated interrupt outputs; waits on m_intr_update_ev.
    void interrupt_update_thread();

    // =========================================================================
    // Callback registration
    // =========================================================================

    /// Install every functional read/write callback over the CSML defaults.
    void register_callbacks();

    /// Flat KV sideload write path (`keymgr_tl_socket`).
    void keymgr_b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

    /// HMAC-style dual-share sideload (one wrapper per KM destination).
    void km_mldsa_seed_b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
    void km_mlkem_d_b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
    void km_mlkem_z_b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
    void km_mlkem_msg_b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
    void km_share_b_transport(unsigned int lane, tlm::tlm_generic_payload &trans,
                              sc_core::sc_time &delay);

    // =========================================================================
    // Register callbacks
    // =========================================================================

    bool mldsa_ctrl_write(DT value);
    bool mlkem_ctrl_write(DT value);
    bool mldsa_msg_write(unsigned int index, DT value);

    bool kv_mldsa_seed_rd_ctrl_write(DT value);
    bool kv_mlkem_seed_rd_ctrl_write(DT value);
    bool kv_mlkem_msg_rd_ctrl_write(DT value);
    bool kv_mlkem_sharedkey_wr_ctrl_write(DT value);

    bool error_internal_intr_write(DT value);
    bool notif_internal_intr_write(DT value);
    bool error_intr_trig_write(DT value);
    bool notif_intr_trig_write(DT value);
    bool intr_enable_write(DT value);

    // =========================================================================
    // Engine helpers
    // =========================================================================

    void do_mldsa_keygen();
    void do_mldsa_sign(bool keygen_first);
    void do_mldsa_verify();

    void do_mlkem_keygen();
    void do_mlkem_encaps();
    void do_mlkem_decaps(bool keygen_first);

    /// Clear all key material, registers and internal buffers (CTRL.ZEROIZE).
    void zeroize();

    /// Copy KV entry into the seed registers backing @p dest.
    bool kv_fetch(unsigned int entry, abr::bytes &dest, unsigned int bytes_wanted);

    /// Reverse little-endian dwords in-place (RTL kv_mldsa_seed_write_offset).
    static void reverse_le_dwords(abr::bytes &v);

    /// MSG / stream / PCR_SIGN digest selection for mu computation.
    void resolve_sign_message(abr::bytes &msg);

    // =========================================================================
    // Timing
    // =========================================================================

    /// Current clock frequency in Hz, falling back to default_clk_freq_hz.
    double clk_freq();

    /// Convert a cycle count to simulated time at the current frequency.
    sc_core::sc_time cycles_to_time(int cycles);

    /**
     * @brief Charge @p cycles of engine latency to the quantum keeper.
     *
     * Accumulates local time and yields to the SystemC kernel only when the
     * global quantum is exhausted, which is the temporal-decoupling contract
     * for an LT model.
     */
    void consume_cycles(int cycles);

    // =========================================================================
    // Status / interrupt helpers
    // =========================================================================

    void set_mldsa_busy();
    void finish_mldsa(bool valid, bool error);
    void set_mlkem_busy();
    void finish_mlkem(bool valid, bool error);

    /// Raise the sticky notification (command-done) interrupt.
    void raise_notif();

    /// Raise the sticky error interrupt.
    void raise_error();

    /// Recompute aggregated status and schedule an output update.
    void request_interrupt_update();

    // =========================================================================
    // Register <-> byte helpers
    //
    // SEP firmware and NIST ACVP pack key/hash material as little-endian
    // 32-bit words (byte b -> word[b/4] bits (b%4)*8).
    // =========================================================================

    template <typename Vec>
    static void regs_to_bytes(Vec &regs, unsigned int count, abr::bytes &out);

    template <typename Vec>
    static void bytes_to_regs(const abr::bytes &in, Vec &regs, unsigned int count);

    template <typename Vec>
    static void clear_regs(Vec &regs, unsigned int count);

    // =========================================================================
    // State
    // =========================================================================

    /// Quantum keeper for LT temporal decoupling.
    tlm_utils::tlm_quantumkeeper m_qk;

    /// Installed cryptographic backend (never null after construction).
    std::unique_ptr<abr::abr_crypto_backend> m_crypto;

    /// Pending commands, latched from CTRL and consumed by the engine threads.
    abr::MldsaCmd m_mldsa_cmd;
    abr::MlkemCmd m_mlkem_cmd;

    /// Pending CTRL.ZEROIZE requests. Zeroization is run by the engine threads
    /// rather than inline in the write callback so it costs modeled time and
    /// observes the same busy/ready handshake as a command.
    bool m_mldsa_zeroize_req;
    bool m_mlkem_zeroize_req;

    /// CTRL modifier flags captured alongside the ML-DSA command.
    bool m_pcr_sign;
    bool m_external_mu;
    bool m_stream_msg;

    /// PCR digest used when PCR_SIGN is set; empty → fall back to MSG.
    abr::bytes m_pcr_digest;

    /// Streaming-message accumulator, filled by MSG writes when STREAM_MSG is set.
    abr::bytes m_msg_stream;

    /// Key Vault entry storage populated over keymgr_tl_socket / KM share sockets.
    std::array<abr::bytes, abr::KV_NUM_ENTRIES> m_kv_entries;

    /// Dual-share scratch for the four KM destination sockets (HMAC layout).
    struct km_share_lane {
        uint32_t share0[abr::KM_SHARE_WORDS]{};
        uint32_t share1[abr::KM_SHARE_WORDS]{};
    };
    std::array<km_share_lane, 4> m_km_share{};

    /// Seed material sourced from the Key Vault, overriding the seed registers.
    abr::bytes m_kv_mldsa_seed;
    abr::bytes m_kv_mlkem_seed;
    abr::bytes m_kv_mlkem_msg;
    bool m_kv_mldsa_seed_valid;
    bool m_kv_mlkem_seed_valid;
    bool m_kv_mlkem_msg_valid;

    /// Latched shared key, retained for the Key Vault export path.
    abr::bytes m_last_shared_key;

    /// True once an engine has been started and not yet reset.
    bool m_in_reset;

    sc_core::sc_event m_mldsa_start_ev;
    sc_core::sc_event m_mlkem_start_ev;
    sc_core::sc_event m_intr_update_ev;

    mutable CsmlLogger logger;
};
