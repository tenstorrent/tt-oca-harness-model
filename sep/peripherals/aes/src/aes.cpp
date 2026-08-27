// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aes.cpp
 * @brief Implementation of the AES TLM model behavioral logic
 * 
 * This file implements the core behavioral functionality of the AES model including:
 * - Asynchronous reset and life cycle escalation handling
 * - Cipher mode execution (ECB, CBC, CFB, OFB, CTR) using OpenSSL
 * - Security hardening features: PRNG reseeding and shadowed registers
 * - Register callback handlers for all TL-UL accessible registers
 * - Functional timing delay logic using SystemC TLM loosely-timed abstraction
 */

#include "aes.h"
#include <cstring>
#include <random>

namespace {

/// One shadow primitive's worth of a control register. aes_ctrl_reg_shadowed.sv
/// instantiates a prim_subreg_shadow per field precisely so an update error in one
/// field does not prevent the others from committing, so the field boundaries are
/// what the two-write protocol resolves against.
struct shadow_field {
    unsigned int lsb;
    unsigned int width;
};

constexpr shadow_field CTRL_SHADOWED_FIELDS[] = {
    {0, 2},   // OPERATION
    {2, 6},   // MODE
    {8, 3},   // KEY_LEN
    {11, 1},  // SIDELOAD
    {12, 3},  // PRNG_RESEED_RATE
    {15, 1},  // MANUAL_OPERATION
};

constexpr shadow_field CTRL_AUX_SHADOWED_FIELDS[] = {
    {0, 1},   // KEY_TOUCH_FORCES_RESEED
    {1, 1},   // FORCE_MASKS
};

constexpr shadow_field CTRL_GCM_SHADOWED_FIELDS[] = {
    {0, 6},   // PHASE
    {6, 5},   // NUM_VALID_BYTES
};

/// Maps a raw PHASE write onto the sparse one-hot encoding of gcm_phase_e.
/// This is only the first half of gcm_phase_get; the legal-transition filter
/// that follows it needs model state and lives in resolve_gcm_phase().
uint32_t legalise_gcm_phase(uint32_t phase)
{
    switch (phase) {
        case 0x01: case 0x02: case 0x04:
        case 0x08: case 0x10: case 0x20:
            return phase;
        default:
            return 0x01;  // unsupported values map to GCM_INIT
    }
}

/// Zero and anything above 16 are mapped to a full block.
uint32_t sanitise_num_valid_bytes(uint32_t num_valid_bytes)
{
    return ((num_valid_bytes == 0u) || (num_valid_bytes > 16u)) ? 16u : num_valid_bytes;
}

constexpr uint32_t field_mask(const shadow_field &f)
{
    return ((f.width >= 32u) ? 0xFFFFFFFFu : ((1u << f.width) - 1u)) << f.lsb;
}

/// Map a raw CTRL_SHADOWED write onto the legal encodings, mirroring the
/// `*_get` always_comb blocks of aes_ctrl_reg_shadowed.sv. The shadow primitives
/// are fed these sanitised values, so both the update comparison and the value
/// software reads back are post-sanitisation: writing an illegal MODE twice
/// commits AES_NONE rather than raising an update error.
uint32_t sanitise_ctrl_shadowed(uint32_t value)
{
    uint32_t operation = value & 0x3u;
    switch (operation) {
        case 0x1: case 0x2: break;
        default: operation = 0x1; break;  // unsupported values map to AES_ENC
    }

    uint32_t mode = (value >> 2) & 0x3Fu;
    switch (mode) {
        case 0x01: case 0x02: case 0x04: case 0x08: case 0x10: case 0x20: break;
        default: mode = 0x3F; break;      // unsupported values map to AES_NONE
    }

    uint32_t key_len = (value >> 8) & 0x7u;
    switch (key_len) {
        case 0x1: case 0x2: case 0x4: break;
        default: key_len = 0x4; break;    // unsupported values map to AES_256
    }

    const uint32_t sideload = (value >> 11) & 0x1u;

    uint32_t reseed_rate = (value >> 12) & 0x7u;
    switch (reseed_rate) {
        case 0x1: case 0x2: case 0x4: break;
        default: reseed_rate = 0x1; break;  // unsupported values map to PER_1
    }

    const uint32_t manual_operation = (value >> 15) & 0x1u;

    return operation | (mode << 2) | (key_len << 8) | (sideload << 11)
         | (reseed_rate << 12) | (manual_operation << 15);
}

/// CTRL_AUX_SHADOWED is two single-bit fields with no illegal encodings.
uint32_t sanitise_ctrl_aux_shadowed(uint32_t value)
{
    return value & 0x3u;
}

/// Resolve the second write of the two-write protocol. Fields whose staged and
/// incoming values agree take the new value; the rest hold what they had.
/// Returns true when every field committed.
bool resolve_shadow_fields(uint32_t staged, uint32_t incoming, uint32_t current,
                           const shadow_field *fields, size_t count, uint32_t &result)
{
    result = current;
    bool all_committed = true;

    for (size_t i = 0; i < count; ++i) {
        const uint32_t mask = field_mask(fields[i]);
        if ((staged & mask) == (incoming & mask)) {
            result = (result & ~mask) | (incoming & mask);
        } else {
            all_committed = false;
        }
    }

    return all_committed;
}

} // namespace

// =============================================================================
// Constructor and Destructor
// =============================================================================

/**
 * @brief Constructor for the aes_model class
 * @param n SystemC module name
 * 
 * Performs the following initialization steps:
 * 1. Initializes all internal state flags (idle, error, alert states)
 * 2. Sets up SystemC processes (reset, escalation, update handlers)
 * 3. Initializes the OpenSSL cipher context (EVP_CIPHER_CTX)
 * 4. Registers memory callbacks with the CSML base layer
 */
    
aes_model::aes_model(sc_module_name n)
    : aes_base(n, 0x8C)
    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
    , clk_i("clk_i")
    , rst_ni("rst_ni")
    , keymgr_tl_socket("keymgr_tl_socket")
    , alert_recov_ctrl_update_err("alert_recov_ctrl_update_err")
    , alert_fatal_fault("alert_fatal_fault")
    , idle_o("idle_o")
    , lc_escalate_en("lc_escalate_en")
    , m_qk()
    , m_clk_freq_hz(100e6)
    , m_cipher_state(CipherState::IDLE)
    , m_is_idle(true)
    , m_in_error_state(false)
    , m_alert_fatal(false)
    , m_alert_recoverable(false)
    , m_current_mode(AESMode::AES_NONE)
    , m_current_operation(AESOperation::AES_ENC)
    , m_current_key_len(AESKeyLen::AES_128)
    , m_sideload_enabled(false)
    , m_manual_operation(false)
    , m_data_in_written_mask(0)
    , m_data_out_read_mask(0)
    , m_iv_written_mask(0)
    , m_key_share0_written_mask(0)
    , m_key_share1_written_mask(0)
    , m_input_ready(true)
    , m_output_valid(false)
    , m_stall(false)
    , m_output_lost(false)
    , m_ctrl_shadowed_first_write_pending(false)
    , m_ctrl_shadowed_shadow_value(0)
    , m_ctrl_aux_shadowed_first_write_pending(false)
    , m_ctrl_aux_shadowed_shadow_value(0)
    , m_ctrl_gcm_shadowed_first_write_pending(false)
    , m_ctrl_gcm_shadowed_shadow_value(0)
    , m_gcm_phase(GCMPhase::GCM_INIT)
    , m_gcm_num_valid_bytes(16)
    , m_gcm_init_done(false)
    , m_gcm_first_block(true)
    , m_gcm_hash_subkey{}
    , m_gcm_s{}
    , m_gcm_ghash{}
    , m_prng_reseed_rate(1)
    , m_block_counter(0)
    , m_key_touch_forces_reseed(true)
    , m_prng_reseed_needed(false)
    , m_cipher_ctx(nullptr)
{
    // Initialize temporal decoupling quantum keeper
    m_qk.reset();

    // Initialize CSML logger
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Initialize OpenSSL cipher context
    m_cipher_ctx = EVP_CIPHER_CTX_new();
    if (!m_cipher_ctx) {
        CSML_ERROR(0, logger) << "[AES] Failed to create OpenSSL cipher context" << std::endl;
        sc_stop();  // Equivalent to FATAL - stop simulation
    }

    // Initialize alert outputs to inactive state
    alert_recov_ctrl_update_err.initialize(false);
    alert_fatal_fault.initialize(false);

    // Initialize idle output to idle state
    idle_o.initialize(true);

    // Initialize internal state arrays
    m_key_share0.fill(0);
    m_key_share1.fill(0);
    m_iv.fill(0);
    m_data_in.fill(0);
    m_data_out.fill(0);
    m_saved_input_block.fill(0);

    // Initialize KeyMgr push sideload buffers
    std::memset(m_keymgr_share0, 0, sizeof(m_keymgr_share0));
    std::memset(m_keymgr_share1, 0, sizeof(m_keymgr_share1));
    m_keymgr_key_valid = false;

    // Register KeyMgr push sideload handler
    keymgr_tl_socket.register_b_transport(this, &aes_model::keymgr_b_transport);

    // Register SystemC processes
    SC_METHOD(reset_process);
    sensitive << rst_ni.neg();
    dont_initialize();

    SC_METHOD(escalation_monitor);
    sensitive << lc_escalate_en.pos();
    dont_initialize();

    SC_METHOD(update_idle_status);
    sensitive << rst_ni << idle_update_event;
    dont_initialize();

    SC_METHOD(update_alert_outputs);
    sensitive << alert_update_event;
    dont_initialize();

    // Register all callbacks
    register_all_callbacks();
}

/** 
 * @brief Destructor for the aes_model class
 * 
 * Ensures proper cleanup of external resources, specifically the 
 * OpenSSL EVP_CIPHER_CTX allocated during construction to prevent
 * memory leaks.
 */
aes_model::~aes_model()
{
    if (m_cipher_ctx) {
        EVP_CIPHER_CTX_free(m_cipher_ctx);
    }
}

/**
 * @brief TLM b_transport handler for KeyMgr push sideload socket
 *
 * KM writes 8 key words at offsets 0x00–0x1C (KEY_SHARE0), 8 words at
 * 0x20–0x3C (KEY_SHARE1), then KEY_CTRL=1 at 0x40 to commit the key.
 */
void aes_model::keymgr_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{
    tlm::tlm_command cmd = trans.get_command();
    if (cmd != tlm::TLM_WRITE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    uint64_t offset = trans.get_address();
    uint32_t* data  = reinterpret_cast<uint32_t*>(trans.get_data_ptr());

    if (offset <= 0x1C && (offset % 4) == 0) {
        // KEY_SHARE0 words: offsets 0x00-0x1C
        m_keymgr_share0[offset / 4] = *data;
    } else if (offset >= 0x20 && offset <= 0x3C && (offset % 4) == 0) {
        // KEY_SHARE1 words: offsets 0x20-0x3C
        m_keymgr_share1[(offset - 0x20) / 4] = *data;
    } else if (offset == 0x40) {
        // KEY_CTRL: bit[0]=1 commits key valid, bit[0]=0 clears it
        m_keymgr_key_valid = (*data & 0x1u) != 0;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

// =============================================================================
// SystemC Processes
// =============================================================================

/** 
 * @brief SystemC process for handling asynchronous resets
 * 
 * Triggered by any transition on the active-low rst_ni port. If rst_ni
 * is low, it clears all internal state, resets all hardware registers
 * to their default values, and randomizes sensitive data (Key/IV)
 * as a security precaution.
 */
void aes_model::reset_process()
{
    if (!rst_ni.read())
    {
        // Reset all internal state
        m_cipher_state = CipherState::IDLE;
        m_is_idle = true;
        m_in_error_state = false;
        m_alert_fatal = false;
        m_alert_recoverable = false;

        m_current_mode = AESMode::AES_NONE;
        m_current_operation = AESOperation::AES_ENC;
        m_current_key_len = AESKeyLen::AES_128;
        m_sideload_enabled = false;
        m_manual_operation = false;

        m_data_in_written_mask = 0;
        m_data_out_read_mask = 0;
        m_iv_written_mask = 0;
        m_key_share0_written_mask = 0;
        m_key_share1_written_mask = 0;
        m_iv_configured = false;

        m_input_ready = true;
        m_output_valid = false;
        m_stall = false;
        m_output_lost = false;

        m_ctrl_shadowed_first_write_pending = false;
        m_ctrl_aux_shadowed_first_write_pending = false;
        m_ctrl_gcm_shadowed_first_write_pending = false;

        // Track the CTRL_GCM_SHADOWED reset value: GCM_INIT with a full block.
        m_gcm_phase = GCMPhase::GCM_INIT;
        m_gcm_num_valid_bytes = 16;
        m_gcm_init_done = false;
        m_gcm_first_block = true;
        m_gcm_hash_subkey.fill(0);
        m_gcm_s.fill(0);
        m_gcm_ghash.fill(0);

        // Reset PRNG reseed tracking (). Both defaults track the register reset
        // values: CTRL_SHADOWED.PRNG_RESEED_RATE is PER_1 and
        // CTRL_AUX_SHADOWED.KEY_TOUCH_FORCES_RESEED is 1.
        m_prng_reseed_rate = 1; // PER_1
        m_block_counter = 0;
        m_key_touch_forces_reseed = true;
        m_prng_reseed_needed = false;

        // Clear internal arrays with pseudo-random data
        clear_registers_with_prng();

        // Reset all registers to their default values
        reset_all_registers();

        // Reset quantum keeper
        m_qk.reset();

        // Update status register and request alert output update
        update_status_register();
        request_alert_update();
    }
}

/** 
 * @brief SystemC process for monitoring life cycle escalation signals
 * 
 * Periodically or on-event monitors the lc_escalate_en port. If enabled,
 * it triggers a terminal fatal alert, effectively locking the module
 * until a system-level reset is applied.
 */
void aes_model::escalation_monitor()
{
    // : Life cycle escalation triggers fatal alert
    if (lc_escalate_en.read())
    {
        // trigger_fatal_alert() handles all ERROR state transitions
        trigger_fatal_alert();
    }
}

/** 
 * @brief Process to update the idle status output port (idle_o)
 * 
 * Centrally manages the idle status output to avoid multiple driver errors.
 * It is triggered by the idle_update_event and reflects the current 
 * functional state of the block.
 */
void aes_model::update_idle_status()
{
    if (m_in_error_state)
    {
        idle_o.write(false);
    }
    else
    {
        idle_o.write(m_is_idle);
    }
}

/** 
 * @brief Process to update the hardware alert output ports
 * 
 * Manages the alert_recov_ctrl_update_err and alert_fatal_fault ports.
 * Triggered by alert_update_event, ensuring consistent signaling of
 * error conditions to the system.
 */
void aes_model::update_alert_outputs()
{
    alert_recov_ctrl_update_err.write(m_alert_recoverable);
    alert_fatal_fault.write(m_alert_fatal);
}

// =============================================================================
// Helper Methods
// =============================================================================

/** 
 * @brief Checks if a life cycle escalation is active and triggers fatal alert
 * 
 * Verifies the state of the lc_escalate_en port. If asserted, it calls 
 * trigger_fatal_alert() to move the module into its terminal error state.
 */
void aes_model::check_escalation()
{
    if (lc_escalate_en.read())
    {
        trigger_fatal_alert();
        m_in_error_state = true;
        m_is_idle = false;
        m_cipher_state = CipherState::ERROR;
        update_status_register();
    }
}

/** 
 * @brief Triggers a fatal hardware alert and enters the terminal lockup state
 * 
 * Implements the security behavior for fatal faults:
 * 1. Sets the fatal alert flag and updates the STATUS register.
 * 2. Enters the terminal lockup state (m_in_error_state = true).
 * 3. Sets the FSM state to ERROR.
 * 4. Clears sensitive internal registers (Key/IV) with pseudo-random data.
 * 5. Logs a critical warning.
 * 
 * Recovery from this state requires a full hardware reset.
 */
void aes_model::trigger_fatal_alert()
{
    // : Fatal alert behavior
    // 1. Set alert flags
    m_alert_fatal = true;
    STATUS.ALERT_FATAL_FAULT = 1;

    // 2. Enter terminal ERROR state
    m_in_error_state = true;
    m_is_idle = false;
    m_cipher_state = CipherState::ERROR;

    // Request idle status update (avoids multiple driver conflicts)
    idle_update_event.notify();

    // 3. Clear sensitive registers with pseudo-random data before lockup
    clear_registers_with_prng();

    // 4. Update registers with cleared values
    for (int i = 0; i < 8; i++) {
        KEY_SHARE0[i] = m_key_share0[i];
        KEY_SHARE1[i] = m_key_share1[i];
    }
    for (int i = 0; i < 4; i++) {
        IV[i] = m_iv[i];
        DATA_IN[i] = m_data_in[i];
        DATA_OUT[i] = m_data_out[i];
    }

    // 5. Update status and alert outputs
    update_status_register();
    request_alert_update();
    // Note: update_idle_status() is an SC_METHOD that will be automatically triggered

    // Log fatal alert for debugging
    CSML_WARN(1, logger) << "[AES] FATAL ALERT: AES unit entering terminal ERROR state. "
                         << "System reset (rst_ni) required for recovery." << std::endl;
}

/** 
 * @brief Triggers a recoverable hardware alert
 * 
 * Typically triggered by a mismatch during a shadowed register update
 * (CTRL_SHADOWED or CTRL_AUX_SHADOWED). Unlike fatal alerts, this does
 * not lock the module and can be cleared by correcting the register update.
 */
void aes_model::trigger_recoverable_alert()
{
    m_alert_recoverable = true;
    STATUS.ALERT_RECOV_CTRL_UPDATE_ERR = 1;
    request_alert_update();
}

/** 
 * @brief Requests an update of the hardware alert output ports
 * 
 * Notifies the alert_update_event with SC_ZERO_TIME delay to trigger
 * the update_alert_outputs process in the next simulation delta cycle.
 */
void aes_model::request_alert_update()
{
    alert_update_event.notify(SC_ZERO_TIME);
}

/** 
 * @brief Updates all bitfields of the hardware STATUS register
 * 
 * Synchronizes the internal boolean flags (idle, ready, valid, error)
 * with their respective bitfields in the memory-mapped STATUS register.
 * This ensures that read operations from the TL-UL interface receive
 * up-to-date status information.
 */
void aes_model::update_status_register()
{
    STATUS.IDLE = m_is_idle ? 1 : 0;

    // STALL: Back-pressure condition in automatic mode when cipher done but output unread
    // In automatic mode, if output is valid but hasn't been read, we're in back-pressure state
    // Note: m_is_idle stays false in automatic mode until output read, so check cipher_state instead
    if (!m_manual_operation && m_output_valid && m_cipher_state == CipherState::IDLE) {
        m_stall = true;
    } else {
        m_stall = false;
    }
    STATUS.STALL = m_stall ? 1 : 0;

    STATUS.OUTPUT_LOST = m_output_lost ? 1 : 0;
    STATUS.OUTPUT_VALID = m_output_valid ? 1 : 0;

    // INPUT_READY: Ready to accept new input
    // Internal m_input_ready (gates DATA_IN writes): idle AND no unread output (both modes)
    // STATUS.INPUT_READY (status reporting):
    //   - Automatic mode: idle AND no unread output (back-pressure)
    //   - Manual mode: idle (no back-pressure, different from internal gating)
    if (m_is_idle && !m_output_valid) {
        m_input_ready = true;
        STATUS.INPUT_READY = 1;
    } else {
        m_input_ready = false;
        // In manual mode, show INPUT_READY=1 if idle, even with unread output (no back-pressure)
        if (m_manual_operation && m_is_idle) {
            STATUS.INPUT_READY = 1;
        } else {
            STATUS.INPUT_READY = 0;
        }
    }

    STATUS.ALERT_RECOV_CTRL_UPDATE_ERR = m_alert_recoverable ? 1 : 0;
    STATUS.ALERT_FATAL_FAULT = m_alert_fatal ? 1 : 0;
}

/**
 * @brief Computes the full 256-bit encryption key from its two shares
 * @param[out] full_key 32-byte array to store the reconstructed key
 * 
 * Implements the security masking requirement by XORing m_key_share0
 * and m_key_share1 word-by-word. This provides protection against 
 * certain side-channel attacks by never storing the raw key in a single
 * register set.
 */
void aes_model::compute_full_key(std::array<uint8_t, 32>& full_key)
{
    // XOR KEY_SHARE0 and KEY_SHARE1 to get the actual key
    for (int i = 0; i < 8; i++) {
        uint32_t xor_word = m_key_share0[i] ^ m_key_share1[i];
        // Little-endian byte order
        full_key[i * 4 + 0] = (xor_word >> 0) & 0xFF;
        full_key[i * 4 + 1] = (xor_word >> 8) & 0xFF;
        full_key[i * 4 + 2] = (xor_word >> 16) & 0xFF;
        full_key[i * 4 + 3] = (xor_word >> 24) & 0xFF;
    }
}

/** 
 * @brief Selects the appropriate OpenSSL EVP_CIPHER based on internal config
 * @return Pointer to the EVP_CIPHER structure or nullptr if the configuration is invalid
 * 
 * Maps the current m_current_mode (ECB, CBC, CFB, OFB, CTR) and 
 * m_current_key_len (128, 192, 256 bits) to the corresponding 
 * OpenSSL cryptographic primitive.
 */
const EVP_CIPHER* aes_model::get_openssl_cipher()
{
    // Determine cipher based on mode and key length
    switch (m_current_mode) {
        case AESMode::AES_ECB:
            switch (m_current_key_len) {
                case AESKeyLen::AES_128: return EVP_aes_128_ecb();
                case AESKeyLen::AES_192: return EVP_aes_192_ecb();
                case AESKeyLen::AES_256: return EVP_aes_256_ecb();
            }
            break;

        case AESMode::AES_CBC:
            switch (m_current_key_len) {
                case AESKeyLen::AES_128: return EVP_aes_128_cbc();
                case AESKeyLen::AES_192: return EVP_aes_192_cbc();
                case AESKeyLen::AES_256: return EVP_aes_256_cbc();
            }
            break;

        case AESMode::AES_CFB:
            switch (m_current_key_len) {
                case AESKeyLen::AES_128: return EVP_aes_128_cfb128();
                case AESKeyLen::AES_192: return EVP_aes_192_cfb128();
                case AESKeyLen::AES_256: return EVP_aes_256_cfb128();
            }
            break;

        case AESMode::AES_OFB:
            switch (m_current_key_len) {
                case AESKeyLen::AES_128: return EVP_aes_128_ofb();
                case AESKeyLen::AES_192: return EVP_aes_192_ofb();
                case AESKeyLen::AES_256: return EVP_aes_256_ofb();
            }
            break;

        case AESMode::AES_CTR:
            switch (m_current_key_len) {
                case AESKeyLen::AES_128: return EVP_aes_128_ctr();
                case AESKeyLen::AES_192: return EVP_aes_192_ctr();
                case AESKeyLen::AES_256: return EVP_aes_256_ctr();
            }
            break;

        default:
            return nullptr;
    }

    return nullptr;
}

/** 
 * @brief Increments the IV interpreted as a 128-bit big-endian counter
 * 
 * Implements the counter increment logic required for AES-CTR mode
 * according to NIST SP 800-38A Appendix B.1. The increment affects
 * the entire 128-bit block.
 */
void aes_model::increment_ctr_iv()
{
    // NIST SP 800-38A: counter is a 128-bit big-endian integer.
    // iv_data[0] is the MSByte, iv_data[15] is the LSByte.
    // Increment propagates from ctr[15] (LSByte of BE integer) toward ctr[0].
    //
    // OT AES register layout (LE word storage):
    //   IV[0] = bits[31:0]  → ctr[0..3]   where ctr[0]=iv_data[0] (MSByte of BE)
    //   IV[3] = bits[127:96]→ ctr[12..15] where ctr[15]=iv_data[15] (LSByte of BE)

    uint8_t ctr[16];
    for (int i = 0; i < 4; i++) {
        ctr[i * 4 + 0] = (m_iv[i] >> 0)  & 0xFF;
        ctr[i * 4 + 1] = (m_iv[i] >> 8)  & 0xFF;
        ctr[i * 4 + 2] = (m_iv[i] >> 16) & 0xFF;
        ctr[i * 4 + 3] = (m_iv[i] >> 24) & 0xFF;
    }

    for (int i = 15; i >= 0; i--) {
        if (++ctr[i] != 0) break;
    }

    for (int i = 0; i < 4; i++) {
        m_iv[i] = (static_cast<uint32_t>(ctr[i * 4 + 0]) << 0)  |
                  (static_cast<uint32_t>(ctr[i * 4 + 1]) << 8)  |
                  (static_cast<uint32_t>(ctr[i * 4 + 2]) << 16) |
                  (static_cast<uint32_t>(ctr[i * 4 + 3]) << 24);
    }
}

/** 
 * @brief Randomizes/clears sensitive internal registers (Key/IV)
 * 
 * Uses a pseudo-random number generator to clear the internal storage
 * of keys, IVs, and input/output data. This is used during reset, 
 * fatal alerts, and manual clear operations to ensure no sensitive
 * data remains visible in the model's memory.
 */
void aes_model::clear_registers_with_prng()
{
    // Use pseudo-random data to clear sensitive registers
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;

    for (auto& val : m_key_share0) val = dis(gen);
    for (auto& val : m_key_share1) val = dis(gen);
    for (auto& val : m_iv) val = dis(gen);
    for (auto& val : m_data_in) val = dis(gen);
    for (auto& val : m_data_out) val = dis(gen);
}

/** 
 * @brief Calculates the functional timing delay for a single block operation
 * @return sc_time representing the calculated delay in seconds
 * 
 * Implements functional timing based on the currently configured mode
 * and key length. Masking overhead is included in the delay calculation
 * to provide a realistic simulation of hardware throughput.
 */
sc_time aes_model::calculate_cipher_delay()
{
    // Calculate functional delay based on key length and masking
    // Using unmasked timing for functional model
    int cycles;

    switch (m_current_key_len) {
        case AESKeyLen::AES_128:
            cycles = 12;
            break;
        case AESKeyLen::AES_192:
            cycles = 14;
            break;
        case AESKeyLen::AES_256:
            cycles = 16;
            break;
        default:
            cycles = 16;
            break;
    }

    // Convert cycles to time based on clock frequency from CCI parameter
    //double clk_freq = m_clk_freq_hz.get_value();
    double clk_freq = m_clk_freq_hz;
    double period_sec = 1.0 / clk_freq;
    double delay_sec = cycles * period_sec;

    return sc_time(delay_sec, SC_SEC);
}

/**
 * @brief Calculates the functional delay for register clearing operations
 * @param is_key_iv_clear true for KEY_IV_DATA_IN_CLEAR, false for DATA_OUT_CLEAR
 * @return sc_time representing the calculated delay in seconds
 * 
 * Implements functional timing for clearing operations (FUNC-AES-011). 
 * Clearing the key and IV registers typically takes longer than clearing
 * only the output registers.
 */
sc_time aes_model::calculate_clearing_delay(bool is_key_iv_clear)
{
    // FUNC-AES-011: Calculate functional delay for register clearing operations
    // Based on aes-detailed-design.md section 7.8.4:
    // - KEY_IV_DATA_IN_CLEAR: 50-200 cycles (20 registers: 8 KEY_SHARE0 + 8 KEY_SHARE1 + 4 IV + 4 DATA_IN)
    // - DATA_OUT_CLEAR: 20-40 cycles (4 registers)

    int cycles;
    if (is_key_iv_clear) {
        // KEY_IV_DATA_IN_CLEAR clears 20 registers + internal keys
        // Use middle of range: 125 cycles
        cycles = 125;
    } else {
        // DATA_OUT_CLEAR clears 4 registers
        // Use middle of range: 30 cycles
        cycles = 30;
    }

    // Convert cycles to time based on clock frequency
    // Default: 100 MHz clock frequency (10ns period)
    double clk_freq = 100e6;
    double period_sec = 1.0 / clk_freq;
    double delay_sec = cycles * period_sec;

    return sc_time(delay_sec, SC_SEC);
}

/** 
 * @brief Returns the reseed threshold based on the configured rate
 * @return uint32_t representing the block count limit
 * 
 * Maps the m_prng_reseed_rate (PER_1, PER_64, PER_8K) to a numeric
 * block counter threshold. This is used for automatic reseed logic
 * ().
 */
uint32_t aes_model::get_prng_reseed_threshold()
{
    // Return block count threshold based on PRNG_RESEED_RATE
    switch (m_prng_reseed_rate) {
        case 1: return 1;      // PER_1: reseed every block
        case 2: return 64;     // PER_64: reseed every 64 blocks
        case 4: return 8192;   // PER_8K: reseed every 8192 blocks
        default: return 1;    // Default to PER_1
    }
}

/** 
 * @brief Performs a PRNG reseed operation using OpenSSL RAND_bytes
 * 
 * Implements the security hardening requirement :
 * 1. Generates random entropy using OpenSSL RAND_bytes().
 * 2. Applies a functional delay for the reseed operation.
 * 3. Updates internal status and resets the block counter upon success.
 * 4. Logs the reseed event.
 */
void aes_model::perform_prng_reseed()
{
    // : PRNG Reseed with OpenSSL

    // Set non-idle during reseed
    m_is_idle = false;
    m_cipher_state = CipherState::CLEARING;
    update_status_register();

    // Generate random entropy using OpenSSL RAND_bytes
    unsigned char rand_buf[48]; // 384 bits
    if (RAND_bytes(rand_buf, sizeof(rand_buf)) != 1) {
        // RAND_bytes failed - log warning but continue
        CSML_WARN(1, logger) << "[AES] RAND_bytes failed - continuing without reseed" << std::endl;
    }

    // Apply functional delay for PRNG reseed operation
    // Approximate 10 cycles for reseed
    // Default: 100 MHz clock frequency (10ns period)
    double clk_freq = 100e6;
    double period_sec = 1.0 / clk_freq;
    double delay_sec = 10 * period_sec;
    sc_time reseed_delay(delay_sec, SC_SEC);

    m_qk.inc(reseed_delay);
    if (m_qk.need_sync()) {
        m_qk.sync();
    }

    // Reset block counter after reseed
    m_block_counter = 0;
    m_prng_reseed_needed = false;

    // Return to idle
    m_cipher_state = CipherState::IDLE;
    m_is_idle = true;
    update_status_register();
}

// =============================================================================
// GCM Datapath
// =============================================================================

/**
 * @brief Encrypts one block with the bare block cipher
 *
 * GCM needs the forward cipher on its own, independent of the configured
 * OPERATION: the hash subkey, S and the counter blocks are all produced by
 * encryption even when the module is decrypting.
 */
bool aes_model::aes_encrypt_block(const uint8_t* in, uint8_t* out)
{
    std::array<uint8_t, 32> full_key;
    compute_full_key(full_key);

    const EVP_CIPHER* cipher = nullptr;
    switch (m_current_key_len) {
        case AESKeyLen::AES_128: cipher = EVP_aes_128_ecb(); break;
        case AESKeyLen::AES_192: cipher = EVP_aes_192_ecb(); break;
        case AESKeyLen::AES_256: cipher = EVP_aes_256_ecb(); break;
        default: return false;
    }

    if (!EVP_CipherInit_ex(m_cipher_ctx, cipher, nullptr, full_key.data(), nullptr, 1)) {
        return false;
    }
    EVP_CIPHER_CTX_set_padding(m_cipher_ctx, 0);

    int outlen = 0;
    if (!EVP_CipherUpdate(m_cipher_ctx, out, &outlen, in, 16)) {
        return false;
    }

    int final_len = 0;
    return EVP_CipherFinal_ex(m_cipher_ctx, out + outlen, &final_len) != 0;
}

/**
 * @brief Multiplies the accumulator by the hash subkey in GF(2^128)
 *
 * The GCM field uses the reversed bit order of the polynomial
 * x^128 + x^7 + x^2 + x + 1, so the reduction constant appears as 0xE1 in the
 * most significant byte. This is the functional equivalent of the parallel
 * aes_gf_mult in aes_ghash.sv.
 */
void aes_model::ghash_mul(std::array<uint8_t, 16>& acc) const
{
    std::array<uint8_t, 16> z{};
    std::array<uint8_t, 16> v = acc;

    for (int bit = 0; bit < 128; ++bit) {
        if ((m_gcm_hash_subkey[bit >> 3] >> (7 - (bit & 7))) & 1) {
            for (int i = 0; i < 16; ++i) {
                z[i] ^= v[i];
            }
        }

        const bool lsb_set = (v[15] & 1) != 0;
        for (int i = 15; i > 0; --i) {
            v[i] = static_cast<uint8_t>((v[i] >> 1) | ((v[i - 1] & 1) << 7));
        }
        v[0] >>= 1;
        if (lsb_set) {
            v[0] ^= 0xE1;
        }
    }

    acc = z;
}

/**
 * @brief XORs a block into the accumulator and multiplies by H
 *
 * Blocks shorter than 16 bytes are zero-padded, which is what NUM_VALID_BYTES
 * selects for the final AAD or payload block.
 */
void aes_model::ghash_absorb(const uint8_t* block, size_t len)
{
    for (size_t i = 0; i < len && i < 16; ++i) {
        m_gcm_ghash[i] ^= block[i];
    }
    ghash_mul(m_gcm_ghash);
}

/**
 * @brief Derives H and S once the key and IV are available
 *
 * GCM_INIT in aes_ghash.sv is two cipher passes: the hash subkey
 * H = E(K, 0^128), then S = E(K, J0) with the GHASH state initialised to S and
 * S subtracted again, which leaves the accumulator at zero. Software provides
 * J0 in the IV registers.
 *
 * @return true once H and S are valid
 */
bool aes_model::ensure_gcm_init()
{
    if (m_gcm_init_done) {
        return true;
    }

    // Both passes need the key; J0 comes from the IV registers.
    if (!m_sideload_enabled) {
        if (m_key_share0_written_mask != 0xFF || m_key_share1_written_mask != 0xFF) {
            return false;
        }
    } else if (!load_sideload_key()) {
        return false;
    }

    if (!m_iv_configured) {
        return false;
    }

    const std::array<uint8_t, 16> zero_block{};
    if (!aes_encrypt_block(zero_block.data(), m_gcm_hash_subkey.data())) {
        CSML_ERROR(0, logger) << "[AES] GCM hash subkey derivation failed" << std::endl;
        trigger_fatal_alert();
        return false;
    }

    uint8_t j0[16];
    for (int i = 0; i < 4; ++i) {
        const uint32_t word = m_iv[i];
        j0[i * 4 + 0] = (word >> 0) & 0xFF;
        j0[i * 4 + 1] = (word >> 8) & 0xFF;
        j0[i * 4 + 2] = (word >> 16) & 0xFF;
        j0[i * 4 + 3] = (word >> 24) & 0xFF;
    }

    if (!aes_encrypt_block(j0, m_gcm_s.data())) {
        CSML_ERROR(0, logger) << "[AES] GCM S derivation failed" << std::endl;
        trigger_fatal_alert();
        return false;
    }

    m_gcm_ghash.fill(0);
    m_gcm_first_block = true;
    m_gcm_init_done = true;
    return true;
}

/**
 * @brief Runs one GCM block according to the committed phase
 *
 * Mirrors the per-phase behaviour of the GHASH_IDLE arm of aes_ghash.sv:
 * which operand is absorbed depends on the phase and, for GCM_TEXT, on the
 * operation, since authentication always covers the ciphertext.
 */
void aes_model::perform_gcm_block()
{
    if (!ensure_gcm_init()) {
        return;
    }

    uint8_t input_data[16];
    for (int i = 0; i < 4; ++i) {
        const uint32_t word = m_data_in[i];
        input_data[i * 4 + 0] = (word >> 0) & 0xFF;
        input_data[i * 4 + 1] = (word >> 8) & 0xFF;
        input_data[i * 4 + 2] = (word >> 16) & 0xFF;
        input_data[i * 4 + 3] = (word >> 24) & 0xFF;
    }

    const size_t valid = (m_gcm_num_valid_bytes > 16u) ? 16u : m_gcm_num_valid_bytes;
    uint8_t output_data[16] = {0};
    bool produces_output = false;

    switch (m_gcm_phase) {
        case GCMPhase::GCM_INIT:
            // ensure_gcm_init() has already done the work; nothing is emitted.
            break;

        case GCMPhase::GCM_RESTORE:
            // GCM_SAVE exported GHASH + S, so removing S recovers the accumulator.
            for (int i = 0; i < 16; ++i) {
                m_gcm_ghash[i] = input_data[i] ^ m_gcm_s[i];
            }
            m_gcm_first_block = false;
            break;

        case GCMPhase::GCM_AAD:
            ghash_absorb(input_data, valid);
            m_gcm_first_block = false;
            break;

        case GCMPhase::GCM_TEXT: {
            // The payload is enciphered in counter mode, with the IV registers
            // holding the counter block.
            uint8_t counter[16];
            for (int i = 0; i < 4; ++i) {
                const uint32_t word = m_iv[i];
                counter[i * 4 + 0] = (word >> 0) & 0xFF;
                counter[i * 4 + 1] = (word >> 8) & 0xFF;
                counter[i * 4 + 2] = (word >> 16) & 0xFF;
                counter[i * 4 + 3] = (word >> 24) & 0xFF;
            }

            uint8_t keystream[16];
            if (!aes_encrypt_block(counter, keystream)) {
                CSML_ERROR(0, logger) << "[AES] GCM keystream generation failed" << std::endl;
                trigger_fatal_alert();
                return;
            }

            // Bytes past NUM_VALID_BYTES are not part of the message and must not
            // reach either the output or the authentication tag.
            for (size_t i = 0; i < 16; ++i) {
                output_data[i] = (i < valid) ? (input_data[i] ^ keystream[i]) : 0;
            }

            uint8_t ciphertext[16];
            for (size_t i = 0; i < 16; ++i) {
                const uint8_t byte = (m_current_operation == AESOperation::AES_ENC)
                                       ? output_data[i] : input_data[i];
                ciphertext[i] = (i < valid) ? byte : 0;
            }
            ghash_absorb(ciphertext, 16);

            increment_ctr_iv();
            m_gcm_first_block = false;
            produces_output = true;
            break;
        }

        case GCMPhase::GCM_SAVE:
            // Export GHASH + S so the accumulator never leaves the block in the clear.
            for (int i = 0; i < 16; ++i) {
                output_data[i] = m_gcm_ghash[i] ^ m_gcm_s[i];
            }
            produces_output = true;
            break;

        case GCMPhase::GCM_TAG:
            // Software supplies the length block; the tag is the final GHASH plus S.
            ghash_absorb(input_data, 16);
            for (int i = 0; i < 16; ++i) {
                output_data[i] = m_gcm_ghash[i] ^ m_gcm_s[i];
            }
            produces_output = true;
            break;
    }

    if (m_current_mode != AESMode::AES_ECB) {
        for (int i = 0; i < 4; ++i) {
            IV[i] = m_iv[i];
        }
        m_iv_written_mask = 0;
        m_iv_configured = true;
    }

    if (produces_output) {
        for (int i = 0; i < 4; ++i) {
            m_data_out[i] = (output_data[i * 4 + 0] << 0) |
                            (output_data[i * 4 + 1] << 8) |
                            (output_data[i * 4 + 2] << 16) |
                            (output_data[i * 4 + 3] << 24);
            DATA_OUT[i] = m_data_out[i];
        }
    }
}

/** 
 * @brief Checks if a PRNG reseed is needed and performs it if so
 * 
 * Evaluates the block counter against the current reseed threshold.
 * If the limit is reached or a pending reseed flag is set, it triggers
 * the manual perform_prng_reseed() operation.
 */
void aes_model::check_and_perform_automatic_prng_reseed()
{
    // : Automatic PRNG reseed based on block counter
    uint32_t threshold = get_prng_reseed_threshold();

    if (m_block_counter >= threshold) {
        perform_prng_reseed();
    } else if (m_prng_reseed_needed) {
        // Deferred reseed request (e.g., from key touch)
        perform_prng_reseed();
    }
}

/** 
 * @brief Implementation of the KEY_IV_DATA_IN_CLEAR trigger
 * 
 * Executed as an asynchronous thread when TRIGGER.KEY_IV_DATA_IN_CLEAR
 * is written. Performs the following:
 * 1. Sets idle_o to low for the duration of the operation.
 * 2. Applies the calculated clearing delay (FUNC-AES-011).
 * 3. Randomizes internal key, IV, and input buffers.
 * 4. Restores idle status and updates registers.
 */
void aes_model::enter_busy(CipherState state)
{
    m_cipher_state = state;
    m_is_idle = false;
    update_status_register();
}

void aes_model::perform_key_iv_data_in_clear()
{
    // FUNC-AES-011: Asynchronous KEY_IV_DATA_IN_CLEAR operation.
    // The busy status was already published by enter_busy() in the TRIGGER write.

    // Clear registers with pseudo-random data
    clear_registers_with_prng();

    // FUNC-AES-011: Apply functional timing delay using quantum keeper
    sc_time clearing_delay = calculate_clearing_delay(true);
    m_qk.inc(clearing_delay);

    // Synchronize if quantum exceeded (TLM LT temporal decoupling)
    if (m_qk.need_sync()) {
        m_qk.sync();
    }

    // Update registers with cleared values
    for (int i = 0; i < 8; i++) {
        KEY_SHARE0[i] = m_key_share0[i];
        KEY_SHARE1[i] = m_key_share1[i];
    }
    for (int i = 0; i < 4; i++) {
        IV[i] = m_iv[i];
        DATA_IN[i] = m_data_in[i];
    }

    // Clear tracking masks
    m_iv_written_mask = 0;
    m_iv_configured = false;
    m_data_in_written_mask = 0;
    m_key_share0_written_mask = 0;
    m_key_share1_written_mask = 0;

    // Return to IDLE state
    m_cipher_state = CipherState::IDLE;
    m_is_idle = true;
    update_status_register();
}

/** 
 * @brief Implementation of the DATA_OUT_CLEAR trigger
 * 
 * Executed as an asynchronous thread when TRIGGER.DATA_OUT_CLEAR
 * is written. Performs the following:
 * 1. Sets idle_o to low for the duration of the operation.
 * 2. Applies the calculated clearing delay (FUNC-AES-011).
 * 3. Randomizes internal data_out buffers.
 * 4. Restores idle status and updates registers.
 */
void aes_model::perform_data_out_clear()
{
    // FUNC-AES-011: Asynchronous DATA_OUT_CLEAR operation.
    // The busy status was already published by enter_busy() in the TRIGGER write.

    // Clear DATA_OUT registers with pseudo-random data
    for (auto& val : m_data_out) {
        std::random_device rd;
        val = rd();
    }

    // FUNC-AES-011: Apply functional timing delay using quantum keeper
    sc_time clearing_delay = calculate_clearing_delay(false);
    m_qk.inc(clearing_delay);

    // Synchronize if quantum exceeded (TLM LT temporal decoupling)
    if (m_qk.need_sync()) {
        m_qk.sync();
    }

    // Update registers with cleared values and clear output_valid
    for (int i = 0; i < 4; i++) {
        DATA_OUT[i] = m_data_out[i];
    }
    m_output_valid = false;

    // Return to IDLE state
    m_cipher_state = CipherState::IDLE;
    m_is_idle = true;
    update_status_register();
}

/** 
 * @brief Implementation of the PRNG_RESEED register trigger
 * 
 * Executed as an asynchronous thread when TRIGGER.PRNG_RESEED is written.
 * Calls the blocking perform_prng_reseed() method to fetch new entropy.
 */
void aes_model::perform_prng_reseed_async()
{
    // FUNC-AES-011: Asynchronous PRNG reseed operation
    // This method is spawned to make STATUS.IDLE=0 observable during operation
    // Simply calls the synchronous perform_prng_reseed() which already handles timing
    perform_prng_reseed();
}

/** 
 * @brief Requests and loads the sideload key from the Key Manager
 * @return true if the key was successfully loaded, false otherwise
 * 
 * Implements the sideload key path. If sideloading is
 * enabled, it sends a request to the Key Manager via keymgr_key_port.
 * On success, it populates the internal key share registers.
 */
bool aes_model::load_sideload_key()
{
    if (!m_sideload_enabled || !m_keymgr_key_valid) {
        return false;
    }

    // Load key shares from KM push buffers
    for (int i = 0; i < 8; i++) {
        m_key_share0[i] = m_keymgr_share0[i];
        m_key_share1[i] = m_keymgr_share1[i];
    }

    m_key_share0_written_mask = 0xFF;
    m_key_share1_written_mask = 0xFF;

    return true;
}

// =============================================================================
// Cipher Operations
// =============================================================================

/** 
 * @brief Evaluates whether all conditions for an automatic cipher start are met
 * @return true if all required data has been written, false otherwise
 * 
 * In automatic mode (MANUAL_OPERATION = 0), the cipher starts as soon
 * as all 32-bit words of the key, IV (if needed), and DATA_IN have
 * been written by software. This method checks the internal bitmasks
 * to determine if the hardware is ready to proceed.
 */
bool aes_model::check_auto_start_conditions()
{
    // Check if all required inputs are ready for auto-start

    // Must be in automatic mode
    if (m_manual_operation) {
        return false;
    }

    // Must be idle
    if (!m_is_idle) {
        return false;
    }

    // Output must have been consumed (OUTPUT_VALID == 0)
    // This prevents overwriting unread output data (stall condition)
    if (m_output_valid) {
        return false;
    }

    // Mode must be valid (not AES_NONE)
    if (m_current_mode == AESMode::AES_NONE) {
        return false;
    }

    // All DATA_IN registers must be written (mask = 0x0F for 4 registers)
    if (m_data_in_written_mask != 0x0F) {
        return false;
    }

    // Key must be configured (all key share registers written or sideload enabled)
    if (!m_sideload_enabled) {
        if (m_key_share0_written_mask != 0xFF || m_key_share1_written_mask != 0xFF) {
            return false;
        }
    } else {
        // : Load key from key manager if sideload enabled
        if (!load_sideload_key()) {
            return false; // Sideload key not valid
        }
    }

    // IV must be configured for modes that require it
    if (m_current_mode != AESMode::AES_ECB) {
        // Non-ECB modes require IV to be valid (either explicitly written or auto-updated)
        if (!m_iv_configured) {
            return false;
        }
    }

    // GCM_INIT and GCM_SAVE take no input block, so they are driven by the
    // CTRL_GCM_SHADOWED write rather than by DATA_IN completing.
    if (m_current_mode == AESMode::AES_GCM) {
        if (m_gcm_phase == GCMPhase::GCM_INIT || m_gcm_phase == GCMPhase::GCM_SAVE) {
            return false;
        }
    }

    return true;
}

/** 
 * @brief Core encryption/decryption logic calling OpenSSL EVP functions
 * 
 * This method performs the heavy lifting for the cryptographic operation:
 * 1. Validates the current cipher configuration.
 * 2. Reconstructs the full 128/192/256-bit key from shares.
 * 3. Prepares the IV for non-ECB modes.
 * 4. Initializes the OpenSSL EVP_Cipher context.
 * 5. Executes the block cipher operation (encryption or decryption).
 * 6. Handles block chaining (IV updates) for CBC/CFB/OFB/CTR modes.
 * 7. Updates the DATA_OUT registers with the result.
 * 
 * Errors during initialization or execution trigger a fatal hardware alert.
 */
void aes_model::execute_encryption_decryption()
{
    // GCM does not map onto a single EVP call: the phases drive GHASH and the
    // counter separately, so it has its own datapath.
    if (m_current_mode == AESMode::AES_GCM) {
        perform_gcm_block();
        return;
    }

    // For CBC/CFB decryption, save the input ciphertext block before decryption
    // This will be used to update the IV after decryption completes
    if ((m_current_mode == AESMode::AES_CBC || m_current_mode == AESMode::AES_CFB)
            && m_current_operation == AESOperation::AES_DEC) {
        for (int i = 0; i < 4; i++) {
            m_saved_input_block[i] = m_data_in[i];
        }
    }

    // Prepare input data (little-endian)
    uint8_t input_data[16];
    for (int i = 0; i < 4; i++) {
        uint32_t word = m_data_in[i];
        input_data[i * 4 + 0] = (word >> 0) & 0xFF;
        input_data[i * 4 + 1] = (word >> 8) & 0xFF;
        input_data[i * 4 + 2] = (word >> 16) & 0xFF;
        input_data[i * 4 + 3] = (word >> 24) & 0xFF;
    }

    // Prepare key
    std::array<uint8_t, 32> full_key;
    compute_full_key(full_key);

    // Prepare IV if needed
    uint8_t iv_data[16];
    if (m_current_mode != AESMode::AES_ECB) {
        for (int i = 0; i < 4; i++) {
            uint32_t word = m_iv[i];
            iv_data[i * 4 + 0] = (word >> 0) & 0xFF;
            iv_data[i * 4 + 1] = (word >> 8) & 0xFF;
            iv_data[i * 4 + 2] = (word >> 16) & 0xFF;
            iv_data[i * 4 + 3] = (word >> 24) & 0xFF;
        }
    }

    // Get OpenSSL cipher
    const EVP_CIPHER* cipher = get_openssl_cipher();
    if (!cipher) {
        CSML_ERROR(0, logger) << "[AES] Invalid cipher mode configuration" << std::endl;
        trigger_fatal_alert();
        return;
    }

    // Initialize cipher context
    int encrypt = (m_current_operation == AESOperation::AES_ENC) ? 1 : 0;

    if (m_current_mode == AESMode::AES_ECB) {
        if (!EVP_CipherInit_ex(m_cipher_ctx, cipher, nullptr, full_key.data(), nullptr, encrypt)) {
            CSML_ERROR(0, logger) << "[AES] Failed to initialize cipher" << std::endl;
            trigger_fatal_alert();
            return;
        }
    } else {
        if (!EVP_CipherInit_ex(m_cipher_ctx, cipher, nullptr, full_key.data(), iv_data, encrypt)) {
            CSML_ERROR(0, logger) << "[AES] Failed to initialize cipher with IV" << std::endl;
            trigger_fatal_alert();
            return;
        }
    }

    // Disable padding for block cipher modes
    EVP_CIPHER_CTX_set_padding(m_cipher_ctx, 0);

    // Perform encryption/decryption
    uint8_t output_data[16];
    int outlen;

    if (!EVP_CipherUpdate(m_cipher_ctx, output_data, &outlen, input_data, 16)) {
        CSML_ERROR(0, logger) << "[AES] Cipher operation failed" << std::endl;
        trigger_fatal_alert();
        return;
    }

    int final_len;
    if (!EVP_CipherFinal_ex(m_cipher_ctx, output_data + outlen, &final_len)) {
        CSML_ERROR(0, logger) << "[AES] Cipher finalization failed" << std::endl;
        trigger_fatal_alert();
        return;
    }

    // Store output data (little-endian)
    for (int i = 0; i < 4; i++) {
        m_data_out[i] = (output_data[i * 4 + 0] << 0) |
                        (output_data[i * 4 + 1] << 8) |
                        (output_data[i * 4 + 2] << 16) |
                        (output_data[i * 4 + 3] << 24);
    }

    // Update IV based on mode
    switch (m_current_mode) {
        case AESMode::AES_CBC:
            if (m_current_operation == AESOperation::AES_ENC) {
                // CBC encryption: IV = output ciphertext
                for (int i = 0; i < 4; i++) {
                    m_iv[i] = m_data_out[i];
                }
            } else {
                // CBC decryption: IV = input ciphertext (saved before decryption)
                for (int i = 0; i < 4; i++) {
                    m_iv[i] = m_saved_input_block[i];
                }
            }
            break;

        case AESMode::AES_CFB:
            if (m_current_operation == AESOperation::AES_ENC) {
                // CFB encryption: next IV = output ciphertext
                for (int i = 0; i < 4; i++) {
                    m_iv[i] = m_data_out[i];
                }
            } else {
                // CFB decryption: next IV = input ciphertext (saved before decryption)
                for (int i = 0; i < 4; i++) {
                    m_iv[i] = m_saved_input_block[i];
                }
            }
            break;

        case AESMode::AES_OFB:
            {
                // OFB: IV = cipher output (before XOR with plaintext)
                // OpenSSL maintains the IV internally in the context
                // Extract the updated IV from the cipher context
                unsigned char iv_buffer[EVP_MAX_IV_LENGTH];
                size_t iv_len = EVP_CIPHER_CTX_get_iv_length(m_cipher_ctx);
                if (iv_len == 16) {
                    // Get the IV from the context using OpenSSL 3.0+ compatible API
                    if (EVP_CIPHER_CTX_get_updated_iv(m_cipher_ctx, iv_buffer, iv_len) == 1) {
                        // Convert from byte array back to 32-bit words (little-endian)
                        for (int i = 0; i < 4; i++) {
                            m_iv[i] = (static_cast<uint32_t>(iv_buffer[i * 4 + 0]) << 0) |
                                      (static_cast<uint32_t>(iv_buffer[i * 4 + 1]) << 8) |
                                      (static_cast<uint32_t>(iv_buffer[i * 4 + 2]) << 16) |
                                      (static_cast<uint32_t>(iv_buffer[i * 4 + 3]) << 24);
                        }
                    }
                }
            }
            break;

        case AESMode::AES_CTR:
            // Increment counter
            increment_ctr_iv();
            break;

        default:
            break;
    }

    // Update IV registers
    if (m_current_mode != AESMode::AES_ECB) {
        for (int i = 0; i < 4; i++) {
            IV[i] = m_iv[i];
        }
        m_iv_written_mask = 0; // Reset IV written mask after auto-update
        m_iv_configured = true; // IV remains configured after auto-update
    }

    // Write output to DATA_OUT registers
    for (int i = 0; i < 4; i++) {
        DATA_OUT[i] = m_data_out[i];
    }
}

/** 
 * @brief Main cipher processing thread; manages FSM and functional delays
 * 
 * This is the central control thread (SC_THREAD) of the AES model.
 * It waits for start triggers (manual or automatic), manages the internal
 * FSM transitions (IDLE -> ROUND -> FINISH), applies functional timing
 * delays, and triggers the actual cryptographic execution. It also
 * handles error transitions and terminal state lockup.
 */
void aes_model::perform_cipher_operation()
{
    // Check escalation before starting
    check_escalation();
    if (m_in_error_state) {
        // The caller already published the busy status. Nothing will run now, so
        // hand the idle indication back rather than stranding software polling
        // for an operation that was abandoned before it began.
        m_cipher_state = CipherState::IDLE;
        m_is_idle = true;
        update_status_register();
        return;
    }

    // : Check and perform automatic PRNG reseed before cipher operation
    check_and_perform_automatic_prng_reseed();

    // Transition to INIT state
    m_cipher_state = CipherState::INIT;
    m_is_idle = false;
    update_status_register();

    // FUNC-AES-011: Apply functional timing delay using quantum keeper
    sc_time delay = calculate_cipher_delay();

    sc_time before_inc = sc_time_stamp();
    m_qk.inc(delay);

    // Synchronize if quantum exceeded (TLM LT temporal decoupling)
    bool did_sync = false;
    if (m_qk.need_sync()) {
        m_qk.sync();
        did_sync = true;
    }
    sc_time after_sync = sc_time_stamp();

    std::string msg = "[AES_TIMING] Cipher timing: delay=" + delay.to_string() +
                     " key_len=" + std::to_string((int)m_current_key_len) +
                     " before=" + before_inc.to_string() +
                     " after=" + after_sync.to_string() +
                     " did_sync=" + (did_sync ? "yes" : "no");
    CSML_INFO(1, logger) << msg << std::endl;

    // Execute the cipher operation
    execute_encryption_decryption();

    // Keep busy state visible by adding explicit wait
    // This allows tests to observe the busy state before completion
    //wait(20, SC_NS);

    // Transition to FINISH state
    m_cipher_state = CipherState::FINISH;

    // : Increment block counter for PRNG reseed tracking
    m_block_counter++;

    // The GCM phases that only feed GHASH produce no result block, so there is
    // nothing for software to collect and no reason to back-pressure on a read.
    const bool produces_output = !(m_current_mode == AESMode::AES_GCM &&
                                   (m_gcm_phase == GCMPhase::GCM_INIT ||
                                    m_gcm_phase == GCMPhase::GCM_RESTORE ||
                                    m_gcm_phase == GCMPhase::GCM_AAD));

    if (produces_output) {
        // Check for output overwrite in manual mode
        if (m_manual_operation && m_output_valid) {
            m_output_lost = true;
        }

        // Set output valid
        m_output_valid = true;
        m_data_out_read_mask = 0;
    }

    // Clear input data written mask for next input
    m_data_in_written_mask = 0;

    // Transition back to IDLE
    m_cipher_state = CipherState::IDLE;

    // FUNC-AES-012: In automatic mode, remain non-idle until output is read (back-pressure)
    // In manual mode, return to idle immediately
    if (m_manual_operation || !produces_output) {
        m_is_idle = true;
    } else {
        // Automatic mode: stay non-idle until output read
        m_is_idle = false;
    }

    // Update status register (INPUT_READY will be computed based on idle and output_valid)
    update_status_register();
}

// =============================================================================
// Register Callbacks
// =============================================================================

/** 
 * @brief Write callback for the ALERT_TEST register
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return false (Register is write-only and self-clearing, no hardware storage)
 * 
 * Implements the software-triggered alert testing (). Writing
 * bit 0 triggers a recoverable alert, and bit 1 triggers a fatal alert
 * leading to terminal lockup.
 */
bool aes_model::handle_write_ALERT_TEST(uint32_t value, uint32_t write_mask)
{
    // : Alert test register
    // ALERT_TEST is write-only and self-clearing

    ALERT_TEST = value & 0x3;
    // Bit 0: Trigger recoverable alert test
    if (value & 0x1) {
        trigger_recoverable_alert();
    }

    // Bit 1: Trigger fatal alert test
    if (value & 0x2) {
        // : Fatal alert test causes terminal ERROR state
        // This simulates a critical fault condition:
        // - FSM invalid state, round counter error, control register storage error,
        // - lc_escalate_en assertion, or TL-UL fatal integrity failure
        // Result: AES enters terminal lockup requiring system reset
        trigger_fatal_alert();
    }

    // Self-clearing: don't actually write to register
    // return false;
    return true;
}

/** 
 * @brief Write callback for the KEY_SHARE0 register set
 * @param index Array index (0-7) of the 32-bit word being written
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return true on success, false if write is ignored due to state/sideload
 * 
 * Implements security hardening (, FUNC-AES-010). Writes are 
 * ignored if the module is busy, sideloading is enabled, or the module 
 * is in an error state. Optionally triggers a PRNG reseed if 
 * KEY_TOUCH_FORCES_RESEED is enabled.
 */
bool aes_model::handle_write_KEY_SHARE0(unsigned int index, uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // Ignore writes when not idle
    if (!m_is_idle) {
        return false;
    }

    // Ignore writes when sideload enabled
    if (m_sideload_enabled) {
        return false;
    }

    // Store the key share
    m_key_share0[index] = value;
    m_key_share0_written_mask |= (1 << index);

    // : Trigger PRNG reseed if KEY_TOUCH_FORCES_RESEED enabled
    if (m_key_touch_forces_reseed) {
        m_prng_reseed_needed = true;
        m_block_counter = 0; // Reset block counter on key update
    }

    return true;
}

/** 
 * @brief Write callback for the KEY_SHARE1 register set
 * @param index Array index (0-7) of the 32-bit word being written
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return true on success, false if write is ignored due to state/sideload
 * 
 * Functional counterpart to KEY_SHARE0. Provides the second share for
 * key reconstructed via XOR.
 */
bool aes_model::handle_write_KEY_SHARE1(unsigned int index, uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // Ignore writes when not idle
    if (!m_is_idle) {
        return false;
    }

    // Ignore writes when sideload enabled
    if (m_sideload_enabled) {
        return false;
    }

    // Store the key share
    m_key_share1[index] = value;
    m_key_share1_written_mask |= (1 << index);

    // : Trigger PRNG reseed if KEY_TOUCH_FORCES_RESEED enabled
    if (m_key_touch_forces_reseed) {
        m_prng_reseed_needed = true;
        m_block_counter = 0; // Reset block counter on key update
    }

    return true;
}

/** 
 * @brief Write callback for the IV register set
 * @param index Array index (0-3) of the 32-bit word being written
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return true on success, false if write is ignored (FUNC-AES-010)
 * 
 * Marks the IV as fully configured once all 4 words are written. Writes
 * are ignored when the module is not in IDLE state.
 */
bool aes_model::handle_write_IV(unsigned int index, uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // FUNC-AES-010: Enforce IDLE-based write protection (silent ignore)
    if (!m_is_idle) {
        // Silently ignore writes when not idle (per specification)
        return false;
    }

    // Store the IV value
    m_iv[index] = value;
    m_iv_written_mask |= (1 << index);

    // Mark IV as configured when all 4 registers have been written
    if (m_iv_written_mask == 0x0F) {
        m_iv_configured = true;
        
        // Check for auto-start conditions (same as DATA_IN callback)
        // Per OpenTitan spec: auto-start requires KEY + IV + DATA_IN all ready
        if (check_auto_start_conditions()) {
            enter_busy(CipherState::INIT);
            sc_spawn(sc_bind(&aes_model::perform_cipher_operation, this));
        }
    }

    return true;
}

/** 
 * @brief Read callback for the IV register set
 * @param index Array index (0-3)
 * @param[out] value 32-bit data read from the register
 * @param read_mask Bitmask of valid bits in the read transaction
 * @return true
 */
bool aes_model::handle_read_IV(unsigned int index, uint32_t& value, uint32_t read_mask)
{
    // Return current IV value
    value = m_iv[index];
    return true;
}

/** 
 * @brief Write callback for the DATA_IN register set
 * @param index Array index (0-3) of the 32-bit word being written
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return true on success, false if write is ignored (module busy/error)
 * 
 * Writing all 4 DATA_IN words triggers an automatic cipher start if
 * MANUAL_OPERATION is 0 and other configuration requirements are met.
 */
bool aes_model::handle_write_DATA_IN(unsigned int index, uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // Check if input ready
    if (!m_input_ready) {
        return false;
    }

    // Store the input data
    m_data_in[index] = value;
    m_data_in_written_mask |= (1 << index);

    // Check for auto-start conditions
    if (check_auto_start_conditions()) {
        enter_busy(CipherState::INIT);
        sc_spawn(sc_bind(&aes_model::perform_cipher_operation, this));
    }

    return true;
}

/** 
 * @brief Read callback for the DATA_OUT register set
 * @param index Array index (0-3)
 * @param[out] value 32-bit data read from the register
 * @param read_mask Bitmask of valid bits in the read transaction
 * @return true
 * 
 * Implements output back-pressure (FUNC-AES-012). The module transitions
 * to IDLE only after all 4 words of DATA_OUT have been read by software.
 */
bool aes_model::handle_read_DATA_OUT(unsigned int index, uint32_t& value, uint32_t read_mask)
{
    // Check if output valid
    if (!m_output_valid) {
        value = 0;
        return true;
    }

    // Return output data
    value = m_data_out[index];
    m_data_out_read_mask |= (1 << index);

    // Check if all DATA_OUT registers read
    if (m_data_out_read_mask == 0x0F) {
        // Clear output valid (back-pressure release)
        m_output_valid = false;
        m_stall = false;
        m_data_out_read_mask = 0;

        // FUNC-AES-012: In automatic mode, return to IDLE after output is fully read
        if (!m_manual_operation && m_cipher_state == CipherState::IDLE) {
            m_is_idle = true;
        }

        // Update status register (INPUT_READY will become 1 if idle)
        update_status_register();
    }

    return true;
}

/** 
 * @brief Write callback for the CTRL_SHADOWED register
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return true on successful update, false if shadowed mismatch or write ignored
 * 
 * Implements the secure two-write shadowed protocol. Mismatches trigger
 * a recoverable alert. Success updates the internal mode, operation, 
 * and key length configuration and resets all data tracking masks.
 */
bool aes_model::handle_write_CTRL_SHADOWED(uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // Ignore writes when not idle
    if (!m_is_idle) {
        return false;
    }

    // Shadowed register two-write protocol
    const uint32_t sanitised = sanitise_ctrl_shadowed(value);

    if (!m_ctrl_shadowed_first_write_pending) {
        // First write: stage the sanitised value
        m_ctrl_shadowed_first_write_pending = true;
        m_ctrl_shadowed_shadow_value = sanitised;
        return false;
    }

    m_ctrl_shadowed_first_write_pending = false;

    uint32_t committed = 0;
    const bool all_committed = resolve_shadow_fields(
        m_ctrl_shadowed_shadow_value, sanitised, static_cast<uint32_t>(CTRL_SHADOWED),
        CTRL_SHADOWED_FIELDS,
        sizeof(CTRL_SHADOWED_FIELDS) / sizeof(CTRL_SHADOWED_FIELDS[0]), committed);

    if (all_committed) {
        // Clear recoverable alert if set
        if (m_alert_recoverable) {
            m_alert_recoverable = false;
            STATUS.ALERT_RECOV_CTRL_UPDATE_ERR = 0;
            request_alert_update();
        }
    } else {
        trigger_recoverable_alert();
    }

    // Internal configuration tracks the register, not the attempted write, so a
    // field held back by an update error keeps driving its previous behaviour.
    const uint32_t operation = committed & 0x3;
    const uint32_t mode = (committed >> 2) & 0x3F;
    const uint32_t key_len = (committed >> 8) & 0x7;
    const uint32_t sideload = (committed >> 11) & 0x1;
    const uint32_t prng_reseed_rate = (committed >> 12) & 0x7;
    const uint32_t manual_op = (committed >> 15) & 0x1;

    switch (operation) {
        case 0x1: m_current_operation = AESOperation::AES_ENC; break;
        case 0x2: m_current_operation = AESOperation::AES_DEC; break;
        default: m_current_operation = AESOperation::AES_ENC; break;
    }

    switch (mode) {
        case 0x01: m_current_mode = AESMode::AES_ECB; break;
        case 0x02: m_current_mode = AESMode::AES_CBC; break;
        case 0x04: m_current_mode = AESMode::AES_CFB; break;
        case 0x08: m_current_mode = AESMode::AES_OFB; break;
        case 0x10: m_current_mode = AESMode::AES_CTR; break;
        case 0x20: m_current_mode = AESMode::AES_GCM; break;
        default: m_current_mode = AESMode::AES_NONE; break;
    }

    switch (key_len) {
        case 0x1: m_current_key_len = AESKeyLen::AES_128; break;
        case 0x2: m_current_key_len = AESKeyLen::AES_192; break;
        case 0x4: m_current_key_len = AESKeyLen::AES_256; break;
        default: m_current_key_len = AESKeyLen::AES_256; break;
    }

    m_sideload_enabled = (sideload != 0);
    m_manual_operation = (manual_op != 0);

    // : Store PRNG reseed rate configuration
    m_prng_reseed_rate = prng_reseed_rate;
    m_block_counter = 0; // Reset block counter on new configuration

    // Writing CTRL_SHADOWED signals new message start. aes_control_fsm.sv gates
    // ctrl_we_o on the storage error alone, so an update error does not stop the
    // write from clearing the key and IV state.
    m_data_in_written_mask = 0;
    m_iv_written_mask = 0;
    m_iv_configured = false;
    m_key_share0_written_mask = 0;
    m_key_share1_written_mask = 0;
    m_output_lost = false;
    m_output_valid = false;
    update_status_register();

    // Write to register for readback (CSML requires explicit write in callback)
    CTRL_SHADOWED = committed;

    return all_committed;
}

/** 
 * @brief Read callback for the CTRL_SHADOWED register
 * @return true
 * 
 * Reading this register resets the state of the two-write shadowed 
 * update protocol.
 */
bool aes_model::handle_read_CTRL_SHADOWED(uint32_t& value, uint32_t read_mask)
{
    // Read resets write sequence
    m_ctrl_shadowed_first_write_pending = false;

    // Return current value
    value = CTRL_SHADOWED;
    return true;
}

/** 
 * @brief Write callback for the CTRL_AUX_SHADOWED register
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return true on successful update, false if restricted or shadowed mismatch
 * 
 * Implements security configuration (). Locked if 
 * CTRL_AUX_REGWEN is 0. Uses the two-write shadowed protocol similar 
 * to CTRL_SHADOWED.
 */
bool aes_model::handle_write_CTRL_AUX_SHADOWED(uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // Check if locked
    if (CTRL_AUX_REGWEN.CTRL_AUX_REGWEN == 0) {
        return false;
    }

    // Shadowed register two-write protocol
    const uint32_t sanitised = sanitise_ctrl_aux_shadowed(value);

    if (!m_ctrl_aux_shadowed_first_write_pending) {
        m_ctrl_aux_shadowed_first_write_pending = true;
        m_ctrl_aux_shadowed_shadow_value = sanitised;
        return false;
    }

    m_ctrl_aux_shadowed_first_write_pending = false;

    uint32_t committed = 0;
    const bool all_committed = resolve_shadow_fields(
        m_ctrl_aux_shadowed_shadow_value, sanitised,
        static_cast<uint32_t>(CTRL_AUX_SHADOWED), CTRL_AUX_SHADOWED_FIELDS,
        sizeof(CTRL_AUX_SHADOWED_FIELDS) / sizeof(CTRL_AUX_SHADOWED_FIELDS[0]), committed);

    if (all_committed) {
        if (m_alert_recoverable) {
            m_alert_recoverable = false;
            STATUS.ALERT_RECOV_CTRL_UPDATE_ERR = 0;
            request_alert_update();
        }
    } else {
        trigger_recoverable_alert();
    }

    // : Extract KEY_TOUCH_FORCES_RESEED field
    m_key_touch_forces_reseed = ((committed >> 0) & 0x1) != 0;

    // Write to register for readback (CSML requires explicit write in callback)
    CTRL_AUX_SHADOWED = committed;

    return all_committed;
}

/**
 * @brief Filters an attempted PHASE write against the legal transitions
 * @param requested One-hot phase already legalised by legalise_gcm_phase()
 * @return The phase that will actually be written
 *
 * Only a subset of transitions is accepted; a rejected request leaves the phase
 * where it is rather than raising an error. Leaving GCM_INIT additionally
 * requires the initialisation to have completed, and returning to AAD or TEXT
 * from GCM_SAVE is only allowed once at least one block has been absorbed.
 * Transcribed from the second unique case of gcm_phase_get.
 */
uint32_t aes_model::resolve_gcm_phase(uint32_t requested) const
{
    const uint32_t current = static_cast<uint32_t>(m_gcm_phase);

    const uint32_t INIT    = static_cast<uint32_t>(GCMPhase::GCM_INIT);
    const uint32_t RESTORE = static_cast<uint32_t>(GCMPhase::GCM_RESTORE);
    const uint32_t AAD     = static_cast<uint32_t>(GCMPhase::GCM_AAD);
    const uint32_t TEXT    = static_cast<uint32_t>(GCMPhase::GCM_TEXT);
    const uint32_t SAVE    = static_cast<uint32_t>(GCMPhase::GCM_SAVE);
    const uint32_t TAG     = static_cast<uint32_t>(GCMPhase::GCM_TAG);

    switch (current) {
        case 0x01:  // GCM_INIT
            return (m_gcm_init_done &&
                    (requested == RESTORE || requested == AAD ||
                     requested == TEXT    || requested == TAG)) ? requested : current;

        case 0x02:  // GCM_RESTORE
            return (requested == INIT || requested == AAD ||
                    requested == TEXT) ? requested : current;

        case 0x04:  // GCM_AAD
            return (requested == INIT ||
                    requested == TEXT ||
                    (requested == SAVE && !m_gcm_first_block) ||
                    requested == TAG) ? requested : current;

        case 0x08:  // GCM_TEXT
            return (requested == INIT ||
                    (requested == SAVE && !m_gcm_first_block) ||
                    requested == TAG) ? requested : current;

        case 0x10:  // GCM_SAVE
        case 0x20:  // GCM_TAG
            return (requested == INIT) ? requested : current;

        default:
            return current;
    }
}

/**
 * @brief Write callback for CTRL_GCM_SHADOWED
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return true if every field committed
 *
 * Shadowed exactly like the other control registers, with one primitive per
 * field. GCM_INIT and GCM_SAVE consume no input block, so the phase write is
 * what sets them running.
 */
bool aes_model::handle_write_CTRL_GCM_SHADOWED(uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // Ignore writes when not idle
    if (!m_is_idle) {
        return false;
    }

    // Sanitisation precedes the shadow comparison, as it does for CTRL_SHADOWED.
    const uint32_t phase = resolve_gcm_phase(legalise_gcm_phase(value & 0x3Fu));
    const uint32_t num_valid_bytes = sanitise_num_valid_bytes((value >> 6) & 0x1Fu);
    const uint32_t sanitised = phase | (num_valid_bytes << 6);

    if (!m_ctrl_gcm_shadowed_first_write_pending) {
        m_ctrl_gcm_shadowed_first_write_pending = true;
        m_ctrl_gcm_shadowed_shadow_value = sanitised;
        return false;
    }

    m_ctrl_gcm_shadowed_first_write_pending = false;

    uint32_t committed = 0;
    const bool all_committed = resolve_shadow_fields(
        m_ctrl_gcm_shadowed_shadow_value, sanitised,
        static_cast<uint32_t>(CTRL_GCM_SHADOWED), CTRL_GCM_SHADOWED_FIELDS,
        sizeof(CTRL_GCM_SHADOWED_FIELDS) / sizeof(CTRL_GCM_SHADOWED_FIELDS[0]), committed);

    if (all_committed) {
        if (m_alert_recoverable) {
            m_alert_recoverable = false;
            STATUS.ALERT_RECOV_CTRL_UPDATE_ERR = 0;
            request_alert_update();
        }
    } else {
        trigger_recoverable_alert();
    }

    m_gcm_phase = static_cast<GCMPhase>(committed & 0x3Fu);
    m_gcm_num_valid_bytes = (committed >> 6) & 0x1Fu;

    CTRL_GCM_SHADOWED = committed;

    // These two phases are not driven by DATA_IN, so run them here.
    if (m_current_mode == AESMode::AES_GCM) {
        if (m_gcm_phase == GCMPhase::GCM_INIT) {
            ensure_gcm_init();
        } else if (m_gcm_phase == GCMPhase::GCM_SAVE) {
            perform_gcm_block();
            m_output_valid = true;
            update_status_register();
        }
    }

    return all_committed;
}

/**
 * @brief Read callback for CTRL_GCM_SHADOWED
 * @param[out] value 32-bit data read from the register
 * @param read_mask Bitmask of valid bits in the read transaction
 * @return true
 *
 * Reading resets the two-write sequence, matching the .re input of the shadow
 * primitives.
 */
bool aes_model::handle_read_CTRL_GCM_SHADOWED(uint32_t& value, uint32_t read_mask)
{
    m_ctrl_gcm_shadowed_first_write_pending = false;
    value = CTRL_GCM_SHADOWED;
    return true;
}

/** 
 * @brief Read callback for the CTRL_AUX_SHADOWED register
 * @return true
 * 
 * Resets the shadowed write protocol state.
 */
bool aes_model::handle_read_CTRL_AUX_SHADOWED(uint32_t& value, uint32_t read_mask)
{
    m_ctrl_aux_shadowed_first_write_pending = false;
    value = CTRL_AUX_SHADOWED;
    return true;
}

/** 
 * @brief Write callback for the CTRL_AUX_REGWEN register
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return true if locked, false otherwise
 * 
 * Implements a read-write-once-to-clear (RW0C) policy. Writing 0 
 * permanently locks the CTRL_AUX_SHADOWED register until the next 
 * system reset.
 */
bool aes_model::handle_write_CTRL_AUX_REGWEN(uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // RW0C: write 1 is ignored, write 0 locks
    if ((value & 0x1) == 0) {
        // Lock CTRL_AUX_SHADOWED by clearing the bit
        CTRL_AUX_REGWEN = 0x0;
        return true;
    }
    // Ignore write of 1 (cannot unlock once locked)
    return false;
}

/** 
 * @brief Write callback for the TRIGGER register
 * @param value 32-bit data being written
 * @param write_mask Bitmask of valid bits in the write transaction
 * @return false (Register is write-only and self-clearing)
 * 
 * Handles manual triggers (, FUNC-AES-011):
 * - Bit 0: START (Manually start cipher if idle)
 * - Bit 1: KEY_IV_DATA_IN_CLEAR (Securely clear sensitive buffers)
 * - Bit 2: DATA_OUT_CLEAR (Clear result buffer)
 * - Bit 3: PRNG_RESEED (Manual PRNG reseed using OpenSSL)
 * 
 * All bits are considered self-clearing and do not store state.
 */
bool aes_model::handle_write_TRIGGER(uint32_t value, uint32_t write_mask)
{
    // : Reject writes when in terminal error state
    if (m_in_error_state) {
        return false;
    }

    // START bit
    if (value & 0x1) {
        if (m_manual_operation && m_is_idle && m_current_mode != AESMode::AES_NONE) {
            // Set OUTPUT_LOST flag if overwriting unread output (before precondition check)
            // This flag is set whenever TRIGGER.START is written while OUTPUT_VALID=1,
            // regardless of whether the operation actually starts
            if (m_output_valid) {
                m_output_lost = true;
                update_status_register();
            }

            // Spec (theory_of_operation.md line 124): manual mode starts "irrespective of
            // the status of the IV and input data registers" — DATA_IN and IV are NOT checked
            // here. Those checks belong to automatic mode (see check_auto_start_conditions()).
            // Only the key must be present for the cipher to operate.
            bool key_ready = false;
            if (m_sideload_enabled) {
                key_ready = load_sideload_key();
            } else {
                key_ready = (m_key_share0_written_mask == 0xFF && m_key_share1_written_mask == 0xFF);
            }

            if (key_ready) {
                enter_busy(CipherState::INIT);
                sc_spawn(sc_bind(&aes_model::perform_cipher_operation, this));
            } else {
                CSML_WARN(1, logger) << "[AES] TRIGGER.START ignored: key not ready" << std::endl;
            }
        }
    }

    // KEY_IV_DATA_IN_CLEAR bit
    if (value & 0x2) {
        if (m_is_idle) {
            enter_busy(CipherState::CLEARING);
            sc_spawn(sc_bind(&aes_model::perform_key_iv_data_in_clear, this));
        }
    }

    // DATA_OUT_CLEAR bit
    // Allowed when idle OR when in stall state (output_valid=1, cipher done but
    // output not yet consumed). DATA_OUT_CLEAR is one of the ways to exit stall.
    if (value & 0x4) {
        if (m_is_idle || m_output_valid) {
            enter_busy(CipherState::CLEARING);
            sc_spawn(sc_bind(&aes_model::perform_data_out_clear, this));
        }
    }

    // PRNG_RESEED bit ()
    if (value & 0x8) {
        if (m_is_idle) {
            enter_busy(CipherState::CLEARING);
            sc_spawn(sc_bind(&aes_model::perform_prng_reseed_async, this));
        }
    }

    // Trigger bits are self-clearing
    return false;
}

/** 
 * @brief Read callback for the STATUS register
 * @param[out] value Current status register value
 * @param read_mask Bitmask of valid bits
 * @return true
 * 
 * Calls update_status_register() before returning to ensure that software 
 * reads the most current representation of the module's internal state.
 */
bool aes_model::handle_read_STATUS(uint32_t& value, uint32_t read_mask)
{
    // Update and return status
    update_status_register();
    value = STATUS;
    return true;
}

// =============================================================================
// Callback Registration
// =============================================================================

/** 
 * @brief Registers all hardware register callbacks with the CSML model
 * 
 * Loops through all memory-mapped registers defined in the base class 
 * and binds the corresponding handle_write_* and handle_read_* methods 
 * to the CSML memory's callback interface. This enables the TL-UL 
 * target socket to trigger behavioral logic on register accesses.
 */
void aes_model::register_all_callbacks()
{
    // ALERT_TEST
    {
        std::function<bool(uint32_t)> write_cb = [this](uint32_t value) {
            return this->handle_write_ALERT_TEST(value, ALERT_TEST.write_bit_mask);
        };
        memory.register_write_callback(write_cb, ALERT_TEST.offset);
    }

    // KEY_SHARE0
    for (unsigned int i = 0; i < 8; i++) {
        std::function<bool(uint32_t)> write_cb = [this, i](uint32_t value) {
            return this->handle_write_KEY_SHARE0(i, value, KEY_SHARE0[i].write_bit_mask);
        };
        memory.register_write_callback(write_cb, KEY_SHARE0[i].offset);
    }

    // KEY_SHARE1
    for (unsigned int i = 0; i < 8; i++) {
        std::function<bool(uint32_t)> write_cb = [this, i](uint32_t value) {
            return this->handle_write_KEY_SHARE1(i, value, KEY_SHARE1[i].write_bit_mask);
        };
        memory.register_write_callback(write_cb, KEY_SHARE1[i].offset);
    }

    // IV
    for (unsigned int i = 0; i < 4; i++) {
        std::function<bool(uint32_t)> write_cb = [this, i](uint32_t value) {
            return this->handle_write_IV(i, value, IV[i].write_bit_mask);
        };
        memory.register_write_callback(write_cb, IV[i].offset);

        std::function<bool(uint32_t&)> read_cb = [this, i](uint32_t& value) {
            return this->handle_read_IV(i, value, IV[i].read_bit_mask);
        };
        memory.register_read_callback(read_cb, IV[i].offset);
    }

    // DATA_IN
    for (unsigned int i = 0; i < 4; i++) {
        std::function<bool(uint32_t)> write_cb = [this, i](uint32_t value) {
            return this->handle_write_DATA_IN(i, value, DATA_IN[i].write_bit_mask);
        };
        memory.register_write_callback(write_cb, DATA_IN[i].offset);
    }

    // DATA_OUT
    for (unsigned int i = 0; i < 4; i++) {
        std::function<bool(uint32_t&)> read_cb = [this, i](uint32_t& value) {
            return this->handle_read_DATA_OUT(i, value, DATA_OUT[i].read_bit_mask);
        };
        memory.register_read_callback(read_cb, DATA_OUT[i].offset);
    }

    // CTRL_SHADOWED
    {
        std::function<bool(uint32_t)> write_cb = [this](uint32_t value) {
            return this->handle_write_CTRL_SHADOWED(value, CTRL_SHADOWED.write_bit_mask);
        };
        memory.register_write_callback(write_cb, CTRL_SHADOWED.offset);

        std::function<bool(uint32_t&)> read_cb = [this](uint32_t& value) {
            return this->handle_read_CTRL_SHADOWED(value, CTRL_SHADOWED.read_bit_mask);
        };
        memory.register_read_callback(read_cb, CTRL_SHADOWED.offset);
    }

    // CTRL_AUX_SHADOWED
    {
        std::function<bool(uint32_t)> write_cb = [this](uint32_t value) {
            return this->handle_write_CTRL_AUX_SHADOWED(value, CTRL_AUX_SHADOWED.write_bit_mask);
        };
        memory.register_write_callback(write_cb, CTRL_AUX_SHADOWED.offset);

        std::function<bool(uint32_t&)> read_cb = [this](uint32_t& value) {
            return this->handle_read_CTRL_AUX_SHADOWED(value, CTRL_AUX_SHADOWED.read_bit_mask);
        };
        memory.register_read_callback(read_cb, CTRL_AUX_SHADOWED.offset);
    }

    // CTRL_AUX_REGWEN
    {
        std::function<bool(uint32_t)> write_cb = [this](uint32_t value) {
            return this->handle_write_CTRL_AUX_REGWEN(value, CTRL_AUX_REGWEN.write_bit_mask);
        };
        memory.register_write_callback(write_cb, CTRL_AUX_REGWEN.offset);
    }

    // TRIGGER
    {
        std::function<bool(uint32_t)> write_cb = [this](uint32_t value) {
            return this->handle_write_TRIGGER(value, TRIGGER.write_bit_mask);
        };
        memory.register_write_callback(write_cb, TRIGGER.offset);

        // TRIGGER is write-only with self-clearing bits - read always returns 0
        std::function<bool(uint32_t&)> read_cb = [](uint32_t& value) {
            value = 0x0;  // All bits self-cleared
            return true;
        };
        memory.register_read_callback(read_cb, TRIGGER.offset);
    }

    // STATUS
    {
        std::function<bool(uint32_t&)> read_cb = [this](uint32_t& value) {
            return this->handle_read_STATUS(value, STATUS.read_bit_mask);
        };
        memory.register_read_callback(read_cb, STATUS.offset);
    }

    // CTRL_GCM_SHADOWED
    {
        std::function<bool(uint32_t)> write_cb = [this](uint32_t value) {
            return this->handle_write_CTRL_GCM_SHADOWED(value, CTRL_GCM_SHADOWED.write_bit_mask);
        };
        memory.register_write_callback(write_cb, CTRL_GCM_SHADOWED.offset);

        std::function<bool(uint32_t&)> read_cb = [this](uint32_t& value) {
            return this->handle_read_CTRL_GCM_SHADOWED(value, CTRL_GCM_SHADOWED.read_bit_mask);
        };
        memory.register_read_callback(read_cb, CTRL_GCM_SHADOWED.offset);
    }
}
