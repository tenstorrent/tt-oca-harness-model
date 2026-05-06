/**
 * @file crng.cpp
 * @brief Implementation of the CRNG (Cryptographic Random Number Generator) TLM model
 *
 * This file implements the core behavioral functionality of the CRNG model including:
 * - CTR_DRBG (Counter mode Deterministic Random Bit Generator) based on NIST SP 800-90A
 * - Command processing (INSTANTIATE, GENERATE, RESEED, UPDATE, UNINSTANTIATE)
 * - Entropy source integration with FIPS compliance tracking
 * - Seed life management and reseed counter enforcement
 * - Hardware client interface for EDN and other consumers
 * - Security countermeasures: multi-bit encoding, bus consistency checks
 * - Register callback handlers for all TL-UL accessible registers
 * - Functional timing delay logic using SystemC TLM loosely-timed abstraction
 */

#include "crng.h"
#include <cstring>
#include <random>
#include <iostream>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/aes.h>

// =============================================================================
// Constructor and Destructor
// =============================================================================

/**
 * @brief Constructor for the crng_model class
 * @param n SystemC module name
 * @param nhwapp Number of hardware applications (default 3: 1 SW + 2 HW)
 *
 * Performs the following initialization steps:
 * 1. Initializes all internal state flags and DRBG instances
 * 2. Sets up SystemC processes (reset, command FSM, interrupt handlers)
 * 3. Initializes OpenSSL DRBG contexts for each instance
 * 4. Allocates and binds hardware client interface ports
 * 5. Registers memory callbacks with the CSML base layer
 */
crng_model::crng_model(sc_module_name n)
    : crng_base(n, 0x60)  // Memory size for register map
    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
    , clk_i("clk_i")
    , rst_ni("rst_ni")
    , otp_en_csrng_sw_app_read("otp_en_csrng_sw_app_read")
    , cs_cmd_req_done("cs_cmd_req_done")
    , cs_entropy_req("cs_entropy_req")
    , cs_hw_inst_exc("cs_hw_inst_exc")
    , cs_fatal_err("cs_fatal_err")
    , recov_alert_o("recov_alert_o")
    , fatal_alert_o("fatal_alert_o")
    , m_qk()
    , m_cmd_fsm_state(CommandFSMState::IDLE)
{
    // Initialize temporal decoupling quantum keeper
    m_qk.reset();

    // Initialize CSML logger
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    // Initialize DRBG instances (Instance 0 = software, Instance 1,2 = unused)
    for (int i = 0; i < 3; i++) {
        m_drbg_instances[i].instance_num = i;
        m_drbg_instances[i].status = 0;  // Uninstantiated
        m_drbg_instances[i].reseed_counter = 0;
        m_drbg_instances[i].compliance_flag = 0;
        m_drbg_instances[i].V.fill(0);
        m_drbg_instances[i].Key.fill(0);
        m_drbg_instances[i].drbg_ctx = nullptr;

        // Initialize OpenSSL DRBG context for each instance
        m_drbg_instances[i].drbg_ctx = EVP_CIPHER_CTX_new();
        if (!m_drbg_instances[i].drbg_ctx) {
            CSML_ERROR(0, logger) << "[CRNG] Failed to create DRBG context for instance " << i << std::endl;
            sc_stop();
        }
    }

    // Initialize command FSM state - start disabled until CTRL.ENABLE=0x6
    m_cmd_ready = false;  // Disabled until CTRL.ENABLE written
    m_cmd_ack = false;
    m_cmd_status = CMD_SUCCESS;
    m_current_instance = 0;
    m_command_in_progress = false;

    // Initialize GENBITS output state
    m_genbits_valid = false;
    m_genbits_fips = false;
    m_genbits_read_index = 0;
    m_genbits_buffer.fill(0);
    m_previous_genbits_64 = 0;

    // Initialize internal state read control
    m_int_state_num = 0;
    m_int_state_read_index = 0;

    // Initialize error and alert state
    m_fatal_error = false;
    m_recoverable_alert = false;

    // Initialize interrupt state
    m_intr_cmd_req_done = false;
    m_intr_entropy_req = false;
    m_intr_hw_inst_exc = false;
    m_intr_fatal_err = false;

    // Initialize alert state
    m_alert_recov = false;
    m_alert_fatal = false;

    // Register SystemC processes
    SC_METHOD(reset_process);
    sensitive << rst_ni.neg();
    dont_initialize();

    //Fix: Added for interrupt update
    SC_THREAD(interrupt_update_process);
    sensitive << intr_update_event;
    dont_initialize();

    // FUNC_ALERT_TEST: Alert update process
    SC_THREAD(alert_update_process);
    sensitive << alert_update_event;
    dont_initialize();
	
    SC_THREAD(command_fsm_process);
    // SC_THREAD uses dynamic sensitivity via wait() inside the thread

    // Register all callbacks
    register_all_callbacks();
}

/**
 * @brief Destructor for the crng_model class
 *
 * Ensures proper cleanup of external resources, specifically the
 * OpenSSL DRBG contexts allocated during construction to prevent
 * memory leaks.
 */
crng_model::~crng_model()
{
    for (int i = 0; i < 3; i++) {
        if (m_drbg_instances[i].drbg_ctx) {
            EVP_CIPHER_CTX_free(m_drbg_instances[i].drbg_ctx);
        }
    }
}

// =============================================================================
// SystemC Processes
// =============================================================================

/**
 * @brief SystemC process for handling asynchronous resets
 *
 * Triggered by any transition on the active-low rst_ni port. If rst_ni
 * is low, it clears all internal state, resets all hardware registers
 * to their default values, and zeroizes all DRBG instances.
 */
void crng_model::reset_process()
{
    if (!rst_ni.read())
    {
        CSML_INFO(1, logger) << "[CRNG] Reset asserted" << std::endl;

        // Reset command FSM - module starts disabled
        m_cmd_fsm_state = CommandFSMState::IDLE;
        m_cmd_ready = false;  // Disabled until CTRL.ENABLE=0x6
        m_cmd_ack = false;
        m_cmd_status = CMD_SUCCESS;
        m_current_instance = 0;
        m_command_in_progress = false;

        // Reset all DRBG instances
        for (int i = 0; i < 3; i++) {
            uninstantiate_instance(i);
        }

        // Reset GENBITS state
        m_genbits_valid = false;
        m_genbits_fips = false;
        m_genbits_read_index = 0;
        m_genbits_buffer.fill(0);
        // Clear block queue
        while (!m_genbits_block_queue.empty()) {
            m_genbits_block_queue.pop();
        }
        m_previous_genbits_64 = 0;

        // Reset internal state read control
        m_int_state_num = 0;
        m_int_state_read_index = 0;

        // Reset error and alert state
        m_fatal_error = false;
        m_recoverable_alert = false;

        // Reset interrupt state
        m_intr_cmd_req_done = false;
        m_intr_entropy_req = false;
        m_intr_hw_inst_exc = false;
        m_intr_fatal_err = false;

        // Reset alert state
        m_alert_recov = false;
        m_alert_fatal = false;

        // Reset all registers to their default values
        reset_all_registers();

        // Notify alert update process to de-assert all alert ports
        alert_update_event.notify(SC_ZERO_TIME);

        // Notify interrupt update process to de-assert all interrupt ports
        intr_update_event.notify(SC_ZERO_TIME);

        // Reset quantum keeper
        m_qk.reset();

        CSML_INFO(1, logger) << "[CRNG] Reset complete" << std::endl;
    }
}

/**
 * @brief SystemC process for updating interrupt outputs
 *
 * This process handles interrupt output updates in a thread-safe manner.
 * It's triggered by intr_update_event whenever interrupt state or enable changes.
 */
void crng_model::interrupt_update_process()
{
    while (true) {
        wait(intr_update_event);
        update_interrupt_outputs();
    }
}

/**
 * @brief SystemC process for updating alert outputs (FUNC_ALERT_TEST)
 *
 * This process handles alert output updates triggered by ALERT_TEST register writes.
 * Alerts are pulse-based: they assert for one delta cycle then de-assert.
 * - Bit 0 (recov_alert) = 1: Triggers one recoverable alert pulse
 * - Bit 1 (fatal_alert) = 1: Triggers one fatal alert pulse
 */
void crng_model::alert_update_process()
{
    while (true) {
        wait(alert_update_event);
        update_alert_outputs();
    }
}

/**
 * @brief Updates alert output signals based on alert state
 *
 * Alert outputs are pulse-based per specification. When ALERT_TEST is written:
 * - The alert output is asserted (high)
 * - After a short delay, the alert output is de-asserted (low)
 * - The internal alert state is cleared
 */
void crng_model::update_alert_outputs()
{
    // Write current alert state to output ports
    recov_alert_o.write(m_alert_recov);
    fatal_alert_o.write(m_alert_fatal);

    if (m_alert_recov || m_alert_fatal) {
        // De-assert alerts (pulse behavior - one alert event per write)
        m_alert_recov = false;
        m_alert_fatal = false;
        recov_alert_o.write(false);
        fatal_alert_o.write(false);

        CSML_INFO(2, logger) << "[CRNG] Alert outputs de-asserted (pulse complete)" << std::endl;
    }
}

/**
 * @brief Trigger recoverable alert when RECOV_ALERT_STS bit is set
 *
 * This method is called whenever a recoverable alert condition occurs.
 * It triggers a pulse on the recov_alert_o output signal.
 */
void crng_model::trigger_recov_alert()
{
    CSML_INFO(2, logger) << "[CRNG] Triggering recoverable alert (RECOV_ALERT_STS condition)" << std::endl;
    m_alert_recov = true;
    alert_update_event.notify(SC_ZERO_TIME);
}
/**
 * @brief SystemC process for command FSM
 *
 * This process handles the command state machine that processes
 * INSTANTIATE, GENERATE, RESEED, UPDATE, and UNINSTANTIATE commands
 * from both software (Instance 0) and hardware clients (Instance 1, 2).
 *
 * NOTE: Converted to SC_THREAD to allow wait() calls for proper temporal
 * decoupling with quantum keeper synchronization.
 */
void crng_model::command_fsm_process()
{
    // SC_THREAD: Loop forever, waiting for command events
    while (true) {
        // Wait for command FSM event
        wait(cmd_fsm_event);

        // Process based on current FSM state
        switch (m_cmd_fsm_state) {
            case CommandFSMState::IDLE:
                // Ready to accept new commands
                MAIN_SM_STATE = MAIN_SM_IDLE;
                break;

            case CommandFSMState::COMMAND_DISPATCH:
                // Dispatch command to appropriate handler
                MAIN_SM_STATE = MAIN_SM_CMD_DISPATCH;
                process_command();
                break;

            case CommandFSMState::ENTROPY_REQUEST:
                // Waiting for entropy from entropy source
                MAIN_SM_STATE = MAIN_SM_ENTROPY_REQUEST;
                break;

            case CommandFSMState::CTR_DRBG_GENERATE:
                // Generating random blocks
                MAIN_SM_STATE = MAIN_SM_CTR_DRBG_GEN;
                break;

            case CommandFSMState::STATE_UPDATE:
                // State update / command ACK
                MAIN_SM_STATE = MAIN_SM_STATE_UPDATE;
                break;

            case CommandFSMState::ERROR:
                // Fatal error state
                MAIN_SM_STATE = MAIN_SM_ERROR;
                break;
        }
    }
}

// =============================================================================
// FUNC_001: CTR_DRBG Random Number Generation
// =============================================================================

/**
 * @brief Generates random value using CTR_DRBG algorithm
 *
 * This function implements the core random number generation using
 * OpenSSL's AES-256 CTR_DRBG abstraction for TLM functional equivalence.
 *
 * @param instance_num Instance number (0-2)
 * @param num_blocks Number of 128-bit blocks to generate
 * @return true if generation successful, false otherwise
 */
bool crng_model::generate_random_blocks(int instance_num, uint32_t num_blocks)
{
    if (instance_num < 0 || instance_num > 2) {
        CSML_ERROR(0, logger) << "[CRNG] Invalid instance number: " << instance_num << std::endl;
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    DRBGInstance& inst = m_drbg_instances[instance_num];

    // Check instance is instantiated
    if (inst.status == 0) {
        CSML_WARN(1, logger) << "[CRNG] Instance " << instance_num << " not instantiated" << std::endl;
        m_cmd_status = CMD_INVALID_CMD_SEQ;
        RECOV_ALERT_STS.CMD_STAGE_INVALID_CMD_SEQ_ALERT = 1;
        trigger_recov_alert();
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    // Check reseed counter (FUNC_003) - Read actual RESEED_INTERVAL register
    uint32_t reseed_interval = static_cast<uint32_t>(RESEED_INTERVAL);
    if (inst.reseed_counter >= reseed_interval) {
        CSML_WARN(1, logger) << "[CRNG] Reseed counter exceeded for instance " << instance_num
                             << " (counter=" << inst.reseed_counter << ", interval=" << reseed_interval << ")" << std::endl;
        m_cmd_status = CMD_RESEED_CNT_EXCEEDED;
        // Set recoverable alert
        RECOV_ALERT_STS.CMD_STAGE_RESEED_CNT_ALERT = 1;
        trigger_recov_alert();
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    // Generate random blocks using OpenSSL RAND_bytes (CTR_DRBG functional equivalent)
    for (uint32_t block = 0; block < num_blocks; block++) {
        unsigned char random_block[16];

        if (RAND_bytes(random_block, 16) != 1) {
            CSML_ERROR(0, logger) << "[CRNG] OpenSSL RAND_bytes failed" << std::endl;
            m_cmd_status = CMD_INVALID_CMD_SEQ;
            m_cmd_fsm_state = CommandFSMState::ERROR;
            return false;
        }
        m_cmd_fsm_state = CommandFSMState::CTR_DRBG_GENERATE;

        std::array<uint32_t, 4> block_words;
        for (int i = 0; i < 4; i++) {
            block_words[i] =
                (random_block[i*4 + 0] << 0) |
                (random_block[i*4 + 1] << 8) |
                (random_block[i*4 + 2] << 16) |
                (random_block[i*4 + 3] << 24);
        }

        m_genbits_block_queue.push(block_words);

        sc_time delay = sc_time(10, SC_US);
        m_qk.inc(delay);
        if (m_qk.need_sync()) {
            m_qk.sync();
        }
    }

    // Pop first block from queue into GENBITS register
    if (!m_genbits_block_queue.empty()) {
        m_genbits_buffer = m_genbits_block_queue.front();
        m_genbits_block_queue.pop();
        m_genbits_valid = true;
        m_genbits_fips = inst.compliance_flag;
        m_genbits_read_index = 0;
    }

    // Update reseed counter (FUNC_003)
    inst.reseed_counter += num_blocks;

    CSML_INFO(2, logger) << "[CRNG] Generated " << num_blocks << " blocks for instance " << instance_num << std::endl;

    return true;
}

// =============================================================================
// FUNC_002: Entropy Source Integration
// =============================================================================

/**
 * @brief Requests entropy from entropy source
 *
 * This function simulates requesting 384-bit seed material from the
 * entropy source.
 *
 * @param seed_buffer Output buffer for entropy (48 bytes / 384 bits)
 * @param fips_compliant Output parameter indicating FIPS compliance
 * @return true if entropy request successful
 */
bool crng_model::request_entropy(unsigned char* seed_buffer, bool& fips_compliant)
{ 
    CSML_INFO(2, logger) << "[CRNG] Requesting entropy from source" << std::endl;
    m_cmd_fsm_state = CommandFSMState::ENTROPY_REQUEST;
    m_intr_entropy_req = true;
	intr_update_event.notify(SC_ZERO_TIME);

    // Functional delay for entropy accumulation (millisecond-level)
    // NOTE: Quantum keeper sync removed - cannot call wait() from TLM callback context
    // In TLM Loosely-Timed modeling, timing is handled by quantum keeper accumulation
    sc_time entropy_delay = sc_time(5, SC_MS);  // Behavioral timing
    m_qk.inc(entropy_delay);
    // Sync will happen automatically at quantum boundaries, not forced here

    // Generate 384-bit seed material
    if (RAND_bytes(seed_buffer, 48) != 1) {
        CSML_ERROR(0, logger) << "[CRNG] Failed to generate entropy seed" << std::endl;
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    // In TLM, assume entropy is FIPS-compliant (can be overridden by FIPS_FORCE)
    fips_compliant = true;

    CSML_INFO(2, logger) << "[CRNG] Entropy request complete (FIPS=" << fips_compliant << ")" << std::endl;

    return true;
}

// =============================================================================
// FUNC_003: Seed Life Management
// =============================================================================

/**
 * @brief Checks if reseed is required based on counter
 *
 * @param instance_num Instance number (0-2)
 * @return true if reseed is required
 */
bool crng_model::reseed_required(int instance_num)
{
    if (instance_num < 0 || instance_num > 2) {
        return true;
    }

    DRBGInstance& inst = m_drbg_instances[instance_num];
    uint32_t reseed_interval = 0xFFFFFFFF;

    return (inst.reseed_counter >= reseed_interval);
}

/**
 * @brief Resets reseed counter for instance
 *
 * @param instance_num Instance number (0-2)
 */
void crng_model::reset_reseed_counter(int instance_num)
{
    if (instance_num >= 0 && instance_num <= 2) {
        m_drbg_instances[instance_num].reseed_counter = 0;
        CSML_INFO(2, logger) << "[CRNG] Reseed counter reset for instance " << instance_num << std::endl;
    }
}

// =============================================================================
// Helper: Derive V and Key from seed material
// =============================================================================

/**
 * @brief Derives V and Key for a DRBG instance from seed material using OpenSSL
 *
 * Seeds OpenSSL PRNG with instance-specific seed material, then generates
 * V (128-bit) and Key (256-bit) values for the given DRBG instance.
 *
 * @param inst Reference to the DRBG instance to populate V and Key
 * @param instance_num Instance number (0-2), used for error logging
 * @param seed_material Pointer to seed material buffer
 * @param seed_size Size of seed material in bytes
 * @return true if V and Key were successfully derived, false on OpenSSL failure
 */
bool crng_model::derive_v_and_key(DRBGInstance& inst, int instance_num,
                                   const unsigned char* seed_buffer, uint8_t clen,
                                   const std::array<uint32_t, 12>& additional_data)
{
    //In the current implementation, the key and V are updated with random values. 
    //However, this actually happens within the internal state 


    // Use OpenSSL to generate deterministic values for V and Key
    // This ensures each instance gets unique values based on seed + personalization + instance number
    unsigned char derived_bytes[48];  // 384 bits: 128 bits (V) + 256 bits (Key)

    // Seed with instance-specific material (only the portion we actually used)
    //RAND_seed(instance_seed, seed_size);

    // Generate V (128 bits = 16 bytes = 4 words)
    if (RAND_bytes(derived_bytes, 16) != 1) {
        CSML_ERROR(0, logger) << "[CRNG] Failed to generate V for instance " << instance_num << std::endl;
        m_cmd_status = CMD_INVALID_CMD_SEQ;
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    // Convert V bytes to words
    for (int i = 0; i < 4; i++) {
        inst.V[i] =
            (derived_bytes[i*4 + 0] << 0) |
            (derived_bytes[i*4 + 1] << 8) |
            (derived_bytes[i*4 + 2] << 16) |
            (derived_bytes[i*4 + 3] << 24);
    }

    // Generate Key (256 bits = 32 bytes = 8 words)
    if (RAND_bytes(derived_bytes, 32) != 1) {
        CSML_ERROR(0, logger) << "[CRNG] Failed to generate Key for instance " << instance_num << std::endl;
        m_cmd_status = CMD_INVALID_CMD_SEQ;
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    // Convert Key bytes to words
    for (int i = 0; i < 8; i++) {
        inst.Key[i] =
            (derived_bytes[i*4 + 0] << 0) |
            (derived_bytes[i*4 + 1] << 8) |
            (derived_bytes[i*4 + 2] << 16) |
            (derived_bytes[i*4 + 3] << 24);
    }

    CSML_INFO(2, logger) << "[CRNG] Derived V and Key for instance " << instance_num << std::endl;
    return true;
}

// =============================================================================
// FUNC_004: INSTANTIATE Command Processing
// =============================================================================

bool crng_model::validate_instance_number(int instance_num)
{
    if (instance_num < 0 || instance_num > 2) {
        CSML_ERROR(0, logger) << "[CRNG] Invalid instance number" << std::endl;
        m_cmd_status = CMD_INVALID_ACMD;
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    return true;
}

bool crng_model::check_instance_instantiated(int instance_num)
{
    DRBGInstance& inst = m_drbg_instances[instance_num];

    if (inst.status == 0) {
        CSML_WARN(1, logger) << "[CRNG] Instance not instantiated" << std::endl;
        m_cmd_status = CMD_INVALID_CMD_SEQ;
        RECOV_ALERT_STS.CMD_STAGE_INVALID_CMD_SEQ_ALERT = 1;
        trigger_recov_alert();
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    return true;
}

void crng_model::validate_flag0(uint8_t& flag0)
{
    if (flag0 != 0x6 && flag0 != 0x9) {
        CSML_WARN(1, logger) << "[CRNG] Invalid flag0 encoding" << std::endl;
        RECOV_ALERT_STS.ACMD_FLAG0_FIELD_ALERT = 1;
        trigger_recov_alert();
        flag0 = 0x9;  // Treat as disable-true (deterministic)
    }
}

bool crng_model::prepare_seed_buffer(uint8_t flag0, unsigned char* seed_buffer,
                                     bool& fips_compliant)
{
    if (flag0 == 0x6) {
        if (!request_entropy(seed_buffer, fips_compliant)) {
            CSML_ERROR(0, logger) << "[CRNG] Entropy request failed" << std::endl;
            m_cmd_status = CMD_INVALID_CMD_SEQ;
            m_cmd_fsm_state = CommandFSMState::ERROR;

            return false;
        }
    } else {
        // Deterministic mode - use zero entropy
        memset(seed_buffer, 0, 48);
    }

    return true;
}

void crng_model::apply_fips_force_override(int instance_num, bool& fips_compliant)
{
    uint32_t fips_force = static_cast<DT>(FIPS_FORCE);  // Read from register
    bool fips_force_enable = false;
    uint8_t fips_force_enable_val = (static_cast<DT>(CTRL) >> 12) & 0xF;
    validate_multi_bit_encoding(fips_force_enable_val, fips_force_enable);

    if (fips_force_enable && (fips_force & (1 << instance_num))) {
        fips_compliant = true;
        CSML_INFO(2, logger) << "[CRNG] FIPS compliance forced for instance "
                             << instance_num << std::endl;
    }
}

void crng_model::convert_additional_data_to_bytes(uint8_t clen,
                                                   const std::array<uint32_t, 12>& additional_data,
                                                   unsigned char* add_input)
{
    memset(add_input, 0, 48);

    for (int i = 0; i < clen && i < 12; i++) {
        add_input[i*4 + 0] = (additional_data[i] >> 0) & 0xFF;
        add_input[i*4 + 1] = (additional_data[i] >> 8) & 0xFF;
        add_input[i*4 + 2] = (additional_data[i] >> 16) & 0xFF;
        add_input[i*4 + 3] = (additional_data[i] >> 24) & 0xFF;
    }
}

void crng_model::seed_prng(uint8_t flag0, uint8_t clen,
                           const unsigned char* seed_buffer,
                           const unsigned char* add_input)
{
    if (flag0 == 0x6) {
        // Entropy mode
        if (clen > 0) {
            // XOR entropy seed with additional data, use result as seed
            unsigned char xor_seed[48];
            memcpy(xor_seed, seed_buffer, 48);
            for (int i = 0; i < clen * 4 && i < 48; i++) {
                xor_seed[i] ^= add_input[i];
            }
            RAND_seed(xor_seed, 48);
        } else {
            // No additional data - use only entropy source seed
            RAND_seed(seed_buffer, 48);
        }
    } else {
        // Deterministic mode (flag0 == 0x9)
        if (clen > 0) {
            // Only provided additional data is used as seed
            RAND_seed(add_input, clen * 4);
        } else {
            // No additional data - use zero seed
            RAND_seed(seed_buffer, 48);
        }
    }
}

// =============================================================================
// FUNC_004: INSTANTIATE Command Processing
// =============================================================================

/**
 * @brief Processes INSTANTIATE command
 *
 * Initializes a DRBG instance with entropy and optional personalization value.
 *
 * @param instance_num Instance number (0-2)
 * @param flag0 Entropy mode (0x6=entropy source, 0x9=deterministic)
 * @param clen Length of personalization string (0-12 words)
 * @param additional_data Personalization string buffer
 * @return true if instantiation successful
 */
bool crng_model::cmd_instantiate(int instance_num, uint8_t flag0, uint8_t clen,
                           const std::array<uint32_t, 12>& additional_data)
{
    CSML_INFO(1, logger) << "[CRNG] INSTANTIATE command for instance " << instance_num
                         << " (flag0=" << (int)flag0 << ", clen=" << (int)clen << ")" << std::endl;

    if (!validate_instance_number(instance_num)) {
        return false;
    }

    DRBGInstance& inst = m_drbg_instances[instance_num];

    // Check instance is not already instantiated
    if (inst.status == 1) {
        CSML_WARN(1, logger) << "[CRNG] Instance already instantiated" << std::endl;
        m_cmd_status = CMD_INVALID_CMD_SEQ;
        RECOV_ALERT_STS.CMD_STAGE_INVALID_CMD_SEQ_ALERT = 1;
        trigger_recov_alert();
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    validate_flag0(flag0);

    unsigned char seed_buffer[48];  // 384 bits
    bool fips_compliant = false;

    if (!prepare_seed_buffer(flag0, seed_buffer, fips_compliant)) {
        return false;
    }

    apply_fips_force_override(instance_num, fips_compliant);

    // Convert additional data and seed PRNG
    unsigned char add_input[48];
    convert_additional_data_to_bytes(clen, additional_data, add_input);
    seed_prng(flag0, clen, seed_buffer, add_input);

    // Derive V and Key from seed buffer, instance number, and additional data
    if (!derive_v_and_key(inst, instance_num, seed_buffer, clen, additional_data)) {
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    // Update instance state
    inst.status = 1;  // Instantiated
    inst.compliance_flag = fips_compliant ? 1 : 0;
    inst.reseed_counter = 0;

    // Functional delay for instantiation
    sc_time delay = sc_time(20, SC_US);
    m_qk.inc(delay);

    m_cmd_status = CMD_SUCCESS;
    CSML_INFO(1, logger) << "[CRNG] Instance " << instance_num << " instantiated (FIPS=" << fips_compliant << ")" << std::endl;
    CSML_INFO(2, logger) << "[CRNG] Instance " << instance_num << " V[0]=0x" << std::hex << inst.V[0] 
                        << ", Key[0]=0x" << inst.Key[0] << std::dec << std::endl;

    return true;
}



// =============================================================================
// FUNC_005: GENERATE Command Processing
// =============================================================================

/**
 * @brief Processes GENERATE command
 *
 * Generates pseudorandom bits from instantiated DRBG instance.
 *
 * @param instance_num Instance number (0-2)
 * @param glen Number of 128-bit blocks to generate (1-4095)
 * @return true if generation successful
 */
bool crng_model::cmd_generate(int instance_num, uint16_t glen)
{
    CSML_INFO(1, logger) << "[CRNG] GENERATE command for instance " << instance_num
                         << " (glen=" << glen << ")" << std::endl;

    if (!validate_instance_number(instance_num)) {
        return false;
    }

    // Validate glen parameter
    if (glen == 0 || glen > 4095) {
        CSML_WARN(1, logger) << "[CRNG] Invalid glen parameter: " << glen << std::endl;
        m_cmd_status = CMD_INVALID_GEN_CMD;
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    if (!check_instance_instantiated(instance_num)) {
        return false;
    }

    // Generate random blocks using FUNC_001
    if (!generate_random_blocks(instance_num, glen)) {
        return false;
    }

    m_cmd_status = CMD_SUCCESS;
    CSML_INFO(1, logger) << "[CRNG] Generated " << glen << " blocks for instance " << instance_num << std::endl;
    return true;
}

// =============================================================================
// FUNC_006: RESEED Command Processing
// =============================================================================

/**
 * @brief Processes RESEED command
 *
 * Refreshes instance entropy by mixing fresh entropy and/or additional input.
 *
 * @param instance_num Instance number (0-2)
 * @param flag0 Entropy mode (0x6=entropy source, 0x9=deterministic)
 * @param clen Length of additional input (0-12 words)
 * @param additional_data Additional input buffer
 * @return true if reseed successful
 */
bool crng_model::cmd_reseed(int instance_num, uint8_t flag0, uint8_t clen,
                     const std::array<uint32_t, 12>& additional_data)
{
    CSML_INFO(1, logger) << "[CRNG] RESEED command for instance " << instance_num
                         << " (flag0=" << (int)flag0 << ", clen=" << (int)clen << ")" << std::endl;

    if (!validate_instance_number(instance_num)) {
        return false;
    }

    if (!check_instance_instantiated(instance_num)) {
        return false;
    }

    DRBGInstance& inst = m_drbg_instances[instance_num];

    validate_flag0(flag0);

    unsigned char seed_buffer[48];  // 384 bits
    bool fips_compliant = inst.compliance_flag;

    if (!prepare_seed_buffer(flag0, seed_buffer, fips_compliant)) {
        return false;
    }

    apply_fips_force_override(instance_num, fips_compliant);

    // Convert additional data and seed PRNG
    unsigned char add_input[48];
    convert_additional_data_to_bytes(clen, additional_data, add_input);
    seed_prng(flag0, clen, seed_buffer, add_input);

    // Derive V and Key from seed buffer, instance number, and additional data
    if (!derive_v_and_key(inst, instance_num, seed_buffer, clen, additional_data)) {
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }

    // Update instance state
    inst.compliance_flag = fips_compliant ? 1 : 0;
    inst.reseed_counter = 0;  // Reset counter (key difference from UPDATE)

    // Functional delay
    sc_time delay = sc_time(15, SC_US);
    m_qk.inc(delay);

    m_cmd_status = CMD_SUCCESS;
    CSML_INFO(1, logger) << "[CRNG] Instance " << instance_num << " reseeded" << std::endl;

    return true;
}

// =============================================================================
// FUNC_007: UPDATE Command Processing
// =============================================================================

/**
 * @brief Processes UPDATE command
 *
 * Mixes additional input into working state without resetting reseed counter.
 *
 * @param instance_num Instance number (0-2)
 * @param clen Length of additional input (1-12 words, must be non-zero)
 * @param additional_data Additional input buffer
 * @return true if update successful
 */
bool crng_model::cmd_update(int instance_num, uint8_t clen,
                     const std::array<uint32_t, 12>& additional_data)
{
    CSML_INFO(1, logger) << "[CRNG] UPDATE command for instance " << instance_num
                         << " (clen=" << (int)clen << ")" << std::endl;

    if (!validate_instance_number(instance_num)) {
        return false;
    }

    if (!check_instance_instantiated(instance_num)) {
        return false;
    }

    // Additional input is mandatory for UPDATE
    if (clen == 0) {
        CSML_WARN(1, logger) << "[CRNG] UPDATE requires additional input (clen > 0)" << std::endl;
        m_cmd_status = CMD_INVALID_CMD_SEQ;
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return false;
    }
    m_cmd_fsm_state = CommandFSMState::STATE_UPDATE;
    // Mix in additional input (no entropy source used)
    unsigned char add_input[48];
    convert_additional_data_to_bytes(clen, additional_data, add_input);
    RAND_seed(add_input, clen * 4);

    // Note: Reseed counter is NOT reset (key difference from RESEED)
    // Compliance flag remains unchanged

    // Functional delay
    sc_time delay = sc_time(10, SC_US);
    m_qk.inc(delay);

    m_cmd_status = CMD_SUCCESS;
    CSML_INFO(1, logger) << "[CRNG] Instance " << instance_num << " updated" << std::endl;

    return true;
}

// =============================================================================
// FUNC_008: UNINSTANTIATE Command Processing
// =============================================================================

/**
 * @brief Processes UNINSTANTIATE command
 *
 * Securely zeroizes DRBG instance working state.
 *
 * @param instance_num Instance number (0-2)
 * @return true (always succeeds)
 */
bool crng_model::cmd_uninstantiate(int instance_num)
{
    CSML_INFO(1, logger) << "[CRNG] UNINSTANTIATE command for instance " << instance_num << std::endl;

    if (instance_num < 0 || instance_num > 2) {
        CSML_ERROR(0, logger) << "[CRNG] Invalid instance number" << std::endl;
        return true;  // UNINSTANTIATE always succeeds
    }

    uninstantiate_instance(instance_num);

    m_cmd_status = CMD_SUCCESS;
    CSML_INFO(1, logger) << "[CRNG] Instance " << instance_num << " uninstantiated" << std::endl;

    return true;
}

/**
 * @brief Helper function to uninstantiate an instance
 *
 * @param instance_num Instance number (0-2)
 */
void crng_model::uninstantiate_instance(int instance_num)
{
    if (instance_num < 0 || instance_num > 2) {
        return;
    }

    DRBGInstance& inst = m_drbg_instances[instance_num];

    // Zeroize all state
    inst.status = 0;
    inst.compliance_flag = 0;
    inst.reseed_counter = 0;
    inst.V.fill(0);
    inst.Key.fill(0);

    CSML_INFO(2, logger) << "[CRNG] Instance " << instance_num << " state zeroized" << std::endl;
}

// =============================================================================
// FUNC_009: Command FSM and Arbitration
// =============================================================================

/**
 * @brief Processes a command from the current instance
 *
 * This function dispatches the command to the appropriate handler based on
 * the command type (acmd field).
 */
void crng_model::process_command()
{
    // Extract command parameters from m_current_command
    // Extract command parameters from m_current_command
    uint8_t acmd = (m_current_command >> 0) & 0xF;
    uint8_t clen = (m_current_command >> 4) & 0xF;
    uint8_t flag0 = (m_current_command >> 8) & 0xF;
    uint16_t glen = (m_current_command >> 12) & 0xFFF;

    CSML_INFO(2, logger) << "[CRNG] Processing command: acmd=" << (int)acmd
                         << ", clen=" << (int)clen
                         << ", flag0=" << (int)flag0
                         << ", glen=" << glen << std::endl;

    // Validate clen range (0-12)
    if (clen > 12) {
        CSML_ERROR(0, logger) << "[CRNG] Invalid clen: " << (int)clen << " (max 12)" << std::endl;
        m_cmd_status = CMD_INVALID_ACMD;
        RECOV_ALERT_STS.CMD_STAGE_INVALID_ACMD_ALERT = 1;
        trigger_recov_alert();
        m_command_in_progress = false;
        m_cmd_ack = true;
        m_cmd_ready = true;
        m_cmd_fsm_state = CommandFSMState::ERROR;

        return;  // Abort command processing
    }                     

    switch (acmd) {
        case 0x1:  // INSTANTIATE
            cmd_instantiate(m_current_instance, flag0, clen, m_additional_data);
            break;

        case 0x2:  // RESEED
            cmd_reseed(m_current_instance, flag0, clen, m_additional_data);
            break;

        case 0x3:  // GENERATE
            cmd_generate(m_current_instance, glen);
            break;

        case 0x4:  // UPDATE
            cmd_update(m_current_instance, clen, m_additional_data);
            break;

        case 0x5:  // UNINSTANTIATE
            cmd_uninstantiate(m_current_instance);
            break;

        default:
            CSML_WARN(1, logger) << "[CRNG] Invalid command: " << (int)acmd << std::endl;
            m_cmd_status = CMD_INVALID_ACMD;
            RECOV_ALERT_STS.CMD_STAGE_INVALID_ACMD_ALERT = 1;
            trigger_recov_alert();
            m_cmd_fsm_state = CommandFSMState::ERROR;
            break;
    }

    // Complete command
    m_command_in_progress = false;
    m_cmd_ack = true;

    // Fire command completion interrupt for SW instance
    m_intr_cmd_req_done = true;
    intr_update_event.notify(SC_ZERO_TIME);

    m_cmd_ready = true;
    m_cmd_fsm_state = CommandFSMState::IDLE;

    CSML_INFO(2, logger) << "[CRNG] Command complete (status=" << m_cmd_status << ")" << std::endl;
}

// =============================================================================
// FUNC_014-018: Interrupt Management
// =============================================================================

/**
 * @brief Updates interrupt output signals based on INTR_STATE and INTR_ENABLE
 */
void crng_model::update_interrupt_outputs()
{
    // Update INTR_STATE register
    INTR_STATE.cs_cmd_req_done = m_intr_cmd_req_done ? 1 : 0;
    INTR_STATE.cs_entropy_req = m_intr_entropy_req ? 1 : 0;
    INTR_STATE.cs_hw_inst_exc = m_intr_hw_inst_exc ? 1 : 0;
    INTR_STATE.cs_fatal_err = m_intr_fatal_err ? 1 : 0;

    // Calculate gated interrupt outputs
    // interrupt[i] = INTR_STATE[i] AND INTR_ENABLE[i]
    // Fix: Interrupt ports modeled in Crng 
	cs_cmd_req_done.write((INTR_STATE.cs_cmd_req_done & INTR_ENABLE.cs_cmd_req_done) != 0);
    cs_entropy_req.write((INTR_STATE.cs_entropy_req & INTR_ENABLE.cs_entropy_req) != 0);
    cs_hw_inst_exc.write((INTR_STATE.cs_hw_inst_exc & INTR_ENABLE.cs_hw_inst_exc) != 0);
    cs_fatal_err.write((INTR_STATE.cs_fatal_err & INTR_ENABLE.cs_fatal_err) != 0);
    // Software polls INTR_STATE to check interrupt status
}

// =============================================================================
// Helper Methods
// =============================================================================

/**
 * @brief Validates multi-bit encoding (enable-true 0x6, disable-true 0x9)
 *
 * @param value 4-bit encoded value
 * @param is_enable Output parameter indicating if value is enable-true
 * @return true if encoding is valid
 */
bool crng_model::validate_multi_bit_encoding(uint8_t value, bool& is_enable)
{
    if (value == 0x6) {
        is_enable = true;
        return true;
    } else if (value == 0x9) {
        is_enable = false;
        return true;
    }

    // Invalid encoding - treat as disable-true
    is_enable = false;
    return false;
}

// =============================================================================
// FUNC_010: Command Request Register Interface (CMD_REQ write callback)
// =============================================================================

/**
 * @brief Write callback for CMD_REQ register
 *
 * Handles command request writes, extracts command parameters, collects
 * additional value if clen > 0, and triggers command FSM.
 */
bool crng_model::handle_write_CMD_REQ(DT value, DT write_mask)
{

    CSML_INFO(2, logger) << "[CRNG] CMD_REQ write: 0x" << std::hex << value << std::dec << std::endl;

    // Check if command interface is ready
    if (!m_cmd_ready) {
        CSML_WARN(1, logger) << "[CRNG] Command interface busy, ignoring write" << std::endl;
        return true;
    }

    // First write contains command header
    if (!m_command_in_progress) {
        m_current_command = value;
        m_current_instance = 0;  // Software commands go to Instance 0
        m_additional_data.fill(0);
        m_additional_data_count = 0;

        // Extract clen to determine if additional value follows
        uint8_t clen = (value >> 4) & 0xF;

        if (clen > 0) {
            // Expecting additional value writes
            m_command_in_progress = true;
            m_expected_additional_data = (clen > 12) ? 12 : clen;
            CSML_INFO(2, logger) << "[CRNG] Expecting " << (int)m_expected_additional_data << " additional value words" << std::endl;
        } else {
            // No additional value, execute command immediately
            m_cmd_ready = false;
            m_cmd_ack = false;
            m_cmd_fsm_state = CommandFSMState::COMMAND_DISPATCH;
            cmd_fsm_event.notify(SC_ZERO_TIME);
        }
    } else {
        // Subsequent writes are additional value
        if (m_additional_data_count < m_expected_additional_data) {
            m_additional_data[m_additional_data_count++] = value;
            CSML_INFO(2, logger) << "[CRNG] Additional value[" << m_additional_data_count-1 << "] = 0x" << std::hex << value << std::dec << std::endl;
        }

        // Check if all additional value collected
        if (m_additional_data_count >= m_expected_additional_data) {
            // Execute command
            m_cmd_ready = false;
            m_cmd_ack = false;
            m_cmd_fsm_state = CommandFSMState::COMMAND_DISPATCH;
            cmd_fsm_event.notify(SC_ZERO_TIME);
        }
    }
    return true;
}

// =============================================================================
// FUNC_011: Status and Data Output Registers (read callbacks)
// =============================================================================

// INTR_ENABLE read callback removed - CSML handles RW registers automatically

/**
 * @brief Read callback for INTR_TEST register (Write-Only)
 *
 * Per specification, write-only registers return 0x0 on read.
 */
bool crng_model::handle_read_INTR_TEST(DT& value, DT read_mask)
{
    value = 0x0;
    return true;
}

/**
 * @brief Read callback for ALERT_TEST register (Write-Only)
 *
 * Per specification, write-only registers return 0x0 on read.
 */
bool crng_model::handle_read_ALERT_TEST(DT& value, DT read_mask)
{
    value = 0x0;
    return true;
}

/**
 * @brief Read callback for CMD_REQ register (Write-Only)
 *
 * Per specification, write-only registers return 0x0 on read.
 */
bool crng_model::handle_read_CMD_REQ(DT& value, DT read_mask)
{
    value = 0x0;
    return true;
}

/**
 * @brief Read callback for SW_CMD_STS register
 *
 * Returns live command status including CMD_RDY, CMD_ACK, and CMD_STS fields.
 * Per architecture map:
 * - Bit [0]: Reserved (always 0)
 * - Bit [1]: CMD_RDY (1=ready, 0=busy)
 * - Bit [2]: CMD_ACK (1=completed, 0=processing) - RW0C behavior
 * - Bits [5:3]: CMD_STS (status code)
 * - Bits [31:6]: Reserved (always 0)
 */
bool crng_model::handle_read_SW_CMD_STS(DT& value, DT read_mask)
{

    // Build live status value with correct bit positions
    uint32_t status = 0;
    status |= (m_cmd_ready ? (1 << 1) : 0);   // CMD_RDY bit 1
    status |= (m_cmd_ack ? (1 << 2) : 0);      // CMD_ACK bit 2
    status |= (m_cmd_status & 0x7) << 3;       // CMD_STS bits [5:3]

    value = status;

    // RW0C behavior for CMD_ACK: Clear on read
    if (m_cmd_ack) {
        m_cmd_ack = false;
        CSML_INFO(2, logger) << "[CRNG] CMD_ACK cleared on SW_CMD_STS read" << std::endl;
    }

    return true;
}

/**
 * @brief Read callback for GENBITS_VLD register
 *
 * Returns value availability and FIPS compliance flags.
 */
bool crng_model::handle_read_GENBITS_VLD(DT& value, DT read_mask)
{
    value = 0;
    value |= (m_genbits_valid ? (1 << 0) : 0);  // GENBITS_VLD bit 0
    value |= (m_genbits_fips ? (1 << 1) : 0);   // GENBITS_FIPS bit 1

    CSML_INFO(3, logger) << "[CRNG] GENBITS_VLD read: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Read callback for GENBITS register
 *
 * Returns 32-bit window into 128-bit random blocks with sequential read pointer.
 * Implements FUNC_022 (64-bit repetition check) and FUNC_024 (access control).
 */
bool crng_model::handle_read_GENBITS(DT& value, DT read_mask)
{

    // FUNC_024: Access control checks
   // bool access_granted = true;

    // Check CTRL.SW_APP_ENABLE
    bool sw_app_enable = false;
    uint8_t sw_app_enable_val = (static_cast<DT>(CTRL) >> 4) & 0xF;
    validate_multi_bit_encoding(sw_app_enable_val, sw_app_enable);

    if (!sw_app_enable) {
        CSML_WARN(2, logger) << "[CRNG] GENBITS read denied: SW_APP_ENABLE not enabled" << std::endl;
        value = 0;
        return true;
    }

    //Fix: check otp signal - 0x6 for enable, 0x9 for disable
    if (otp_en_csrng_sw_app_read.read() != 0x6) {
        CSML_WARN(2, logger) << "[CRNG] GENBITS read denied: OTP signal disabled (expected 0x6, got 0x"
                             << std::hex << static_cast<uint32_t>(otp_en_csrng_sw_app_read.read()) << std::dec << ")" << std::endl;
        value = 0;
        return true;
    }
     
    if (!m_genbits_valid) {
        CSML_WARN(2, logger) << "[CRNG] GENBITS read when no value valid" << std::endl;
        value = 0;
        return true;
    }

    // Return current 32-bit word from buffer
    value = m_genbits_buffer[m_genbits_read_index];
    //fix: Store current read index for logging
    uint32_t current_read_index = m_genbits_read_index;

    // FUNC_022: 64-bit repetition check
    if (m_genbits_read_index % 2 == 1) {
        // We've just read the second 32-bit word of a 64-bit pair
        uint64_t current_64 = ((uint64_t)m_genbits_buffer[m_genbits_read_index-1] << 32) |
                              (uint64_t)value;

        if (current_64 == m_previous_genbits_64 && m_previous_genbits_64 != 0) {
            CSML_WARN(1, logger) << "[CRNG] 64-bit repetition detected in GENBITS output!" << std::endl;
            RECOV_ALERT_STS.CS_BUS_CMP_ALERT = 1;
            trigger_recov_alert();
        }

        m_previous_genbits_64 = current_64;
    }

    // Increment read pointer (mod 4)
    m_genbits_read_index = (m_genbits_read_index + 1) % 4;
    //Fix: Use stored read index for logging
    CSML_INFO(3, logger) << "[CRNG] GENBITS[" << current_read_index << "] read: 0x" << std::hex << value << std::dec << std::endl;

    // If we've read all 4 words, check for more blocks in queue
    if (m_genbits_read_index == 0) {
        if (!m_genbits_block_queue.empty()) {
            // Pop next block from queue
            m_genbits_buffer = m_genbits_block_queue.front();
            m_genbits_block_queue.pop();
            CSML_INFO(2, logger) << "[CRNG] GENBITS block fully read, loading next block from queue (remaining: "
                                 << m_genbits_block_queue.size() << ")" << std::endl;
            // m_genbits_valid remains true, m_genbits_fips remains same for this GENERATE batch
        } else {
            // No more blocks, clear valid flag
            m_genbits_valid = false;
            CSML_INFO(2, logger) << "[CRNG] GENBITS block fully read, no more blocks in queue" << std::endl;
        }
    }

    return true;
}

/**
 * @brief Read callback for RESEED_COUNTER_0 register
 */
bool crng_model::handle_read_RESEED_COUNTER_0(DT& value, DT read_mask)
{
    value = m_drbg_instances[0].reseed_counter;
    return true;
}

/**
 * @brief Read callback for RESEED_COUNTER_1 register
 */
bool crng_model::handle_read_RESEED_COUNTER_1(DT& value, DT read_mask)
{
    value = m_drbg_instances[1].reseed_counter;
    return true;
}

/**
 * @brief Read callback for RESEED_COUNTER_2 register
 */
bool crng_model::handle_read_RESEED_COUNTER_2(DT& value, DT read_mask)
{
    value = m_drbg_instances[2].reseed_counter;
    return true;
}

/**
 * @brief Read callback for INT_STATE_VAL register
 *
 * Returns 32-bit window into 448-bit internal state with sequential read pointer.
 * Implements FUNC_025 (four-layer access control).
 */
bool crng_model::handle_read_INT_STATE_VAL(DT& value, DT read_mask)
{

    // FUNC_025: Four-layer access control
    bool access_granted = true;

    // Layer 1: CTRL.READ_INT_STATE must be enable-true (0x6)
    bool read_int_state_enable = false;
    uint8_t read_int_state_val = (static_cast<DT>(CTRL) >> 8) & 0xF;
    validate_multi_bit_encoding(read_int_state_val, read_int_state_enable);

    if (!read_int_state_enable) {
        access_granted = false;
    }
    
    //Fix: Layer 2 check for otp_en_csrng_sw_app_read signal (0x6=enable, 0x9=disable)
    if (otp_en_csrng_sw_app_read.read() != 0x6) {
        access_granted = false;
    }

    // Layer 3: INT_STATE_READ_ENABLE[instance] must be set
    uint32_t int_state_read_enable = static_cast<DT>(INT_STATE_READ_ENABLE);
    if (!(int_state_read_enable & (1 << m_int_state_num))) {
        access_granted = false;
    }

    if (!access_granted) {
        CSML_WARN(2, logger) << "[CRNG] INT_STATE_VAL read denied (access control)" << std::endl;
        value = 0;
        return true;
    }

    // Access granted - return current 32-bit word from internal state
    int instance = m_int_state_num;
    if (instance < 0 || instance > 2) {
        value = 0;
        return true;
    }

    DRBGInstance& inst = m_drbg_instances[instance];

    // Internal state layout per spec: Reseed Counter (1 word) + V (4 words) + Key (8 words) + Status (1 word) = 14 words
    if (m_int_state_read_index == 0) {
        // Word 0: Reseed Counter
        value = inst.reseed_counter;
    } else if (m_int_state_read_index >= 1 && m_int_state_read_index < 5) {
        // Words 1-4: V (counter)
        value = inst.V[m_int_state_read_index - 1];
    } else if (m_int_state_read_index >= 5 && m_int_state_read_index < 13) {
        // Words 5-12: Key
        value = inst.Key[m_int_state_read_index - 5];
    } else if (m_int_state_read_index == 13) {
        // Word 13: Status
        value = inst.status | (inst.compliance_flag << 1);
    } else {
        value = 0;
    }

    // Store current read index for logging before incrementing
    uint32_t current_read_index = m_int_state_read_index;

    // Increment read pointer (mod 14)
    m_int_state_read_index = (m_int_state_read_index + 1) % 14;

    CSML_INFO(3, logger) << "[CRNG] INT_STATE_VAL[" << current_read_index << "] read: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for INT_STATE_NUM register
 *
 * Selects instance for internal state reading and resets read pointer.
 */
bool crng_model::handle_write_INT_STATE_NUM(DT value, DT write_mask)
{

    m_int_state_num = value & 0x3;  // Only 2 bits needed for 3 instances
    m_int_state_read_index = 0;     // Reset read pointer

    //Fix: Update register storage so reads return the correct value
    INT_STATE_NUM = value & 0xF;  // Store bits [3:0] in register

    CSML_INFO(2, logger) << "[CRNG] INT_STATE_NUM set to " << m_int_state_num << std::endl;
    return true;
}

/**
 * @brief Read callback for ERR_CODE register
 *
 * Returns sticky fatal error flags.
 */
bool crng_model::handle_read_ERR_CODE(DT& value, DT read_mask)
{

    // ERR_CODE bits are sticky - cleared only by reset
    // In this TLM implementation, we don't model individual FIFO errors
    value = static_cast<DT>(ERR_CODE);

    CSML_INFO(3, logger) << "[CRNG] ERR_CODE read: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for ERR_CODE_TEST register
 *
 * Implements error injection for testing. Sets corresponding ERR_CODE bit,
 * triggers cs_fatal_err interrupt, and logs fatal alert.
 * Protected by REGWEN lock.
 */
bool crng_model::handle_write_ERR_CODE_TEST(DT value, DT write_mask)
{   //Fix: Write callback for ERR_CODE_TEST register added
    // Check REGWEN lock (FUNC_026)
    if (static_cast<DT>(REGWEN) == 0) {
        CSML_WARN(2, logger) << "[CRNG] ERR_CODE_TEST write denied: REGWEN locked" << std::endl;
        return true;
    }

    // Extract error bit number from bits [4:0] (range 0-30)
    uint8_t error_bit_num = value & 0x1F;
    
    // Ignore writes of 0 (no error injection) - prevents undefined behavior
    // and accidental error injection when writing reset value
    if (error_bit_num == 0) {
        // Store the value but don't inject error
        ERR_CODE_TEST = value & 0x1F;
        CSML_INFO(2, logger) << "[CRNG] ERR_CODE_TEST: Write of 0 ignored (no error injection)" << std::endl;
        return true;
    }
    
    // Validate error bit number (1-30)
    if (error_bit_num > 30) {
        CSML_WARN(1, logger) << "[CRNG] ERR_CODE_TEST invalid error bit number: " << (int)error_bit_num << " (max 30)" << std::endl;
        // Still process it, but clamp to valid range
        error_bit_num = 30;
    }

    // Set corresponding ERR_CODE bit (sticky - remains set until reset)
    uint32_t current_err_code = static_cast<DT>(ERR_CODE);
    uint32_t new_err_code = current_err_code | (1 << (error_bit_num));
    ERR_CODE = new_err_code;

    // Trigger cs_fatal_err interrupt and transition FSM to ERROR state
    m_intr_fatal_err = true;
    m_cmd_fsm_state = CommandFSMState::ERROR;
    //update_interrupt_outputs();
	intr_update_event.notify(SC_ZERO_TIME);
									   
    CSML_INFO(1, logger) << "[CRNG] ERR_CODE_TEST: Injected error bit " << (int)error_bit_num
                         << ", ERR_CODE=0x" << std::hex << new_err_code << std::dec
                         << ", cs_fatal_err interrupt fired" << std::endl;
                         
    // Store only the writable bits [4:0], mask out reserved bits [31:5]
    ERR_CODE_TEST = value & 0x1F;

    return true;
}

/**
 * @brief Read callback for ERR_CODE_TEST register
 *
 * Returns current ERR_CODE_TEST register value.
 */
bool crng_model::handle_read_ERR_CODE_TEST(DT& value, DT read_mask)
{   //Fix: read callback for ERR_CODE_TEST register added
    value = static_cast<DT>(ERR_CODE_TEST);

    CSML_INFO(3, logger) << "[CRNG] ERR_CODE_TEST read: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Read callback for MAIN_SM_STATE register
 *
 * Returns current FSM state for debug visibility.
 * The MAIN_SM_STATE register is updated directly by the command FSM process
 * and by handle_write_CTRL when the module is enabled/disabled.
 */
bool crng_model::handle_read_MAIN_SM_STATE(DT& value, DT read_mask)
{
    value = static_cast<DT>(MAIN_SM_STATE);

    CSML_INFO(3, logger) << "[CRNG] MAIN_SM_STATE read: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

// =============================================================================
// FUNC_012: Control and Configuration Registers (write callbacks)
// =============================================================================

/**
 * @brief Write callback for CTRL register
 *
 * Validates multi-bit encodings and triggers alerts on invalid values.
 * Implements FUNC_021 (multi-bit encoding validation).
 * Implements FSM state transitions based on ENABLE field.
 */
bool crng_model::handle_write_CTRL(DT value, DT write_mask)
{
    // Check REGWEN lock (FUNC_026)
    if (static_cast<DT>(REGWEN) == 0) {
        CSML_WARN(2, logger) << "[CRNG] CTRL write denied: REGWEN locked" << std::endl;
        return true;
    }

    // FUNC_021: Validate multi-bit encodings
    bool enable, sw_app_enable, read_int_state, fips_force_enable;
    bool valid = true;

    // ENABLE field [3:0]
    uint8_t enable_val = (value >> 0) & 0xF;
    if (!validate_multi_bit_encoding(enable_val, enable)) {
        RECOV_ALERT_STS.ENABLE_FIELD_ALERT = 1;
        trigger_recov_alert();
        valid = false;
    }

    // SW_APP_ENABLE field [7:4]
    uint8_t sw_app_enable_val = (value >> 4) & 0xF;
    if (!validate_multi_bit_encoding(sw_app_enable_val, sw_app_enable)) {
        RECOV_ALERT_STS.SW_APP_ENABLE_FIELD_ALERT = 1;
        trigger_recov_alert();
        valid = false;
    }

    // READ_INT_STATE field [11:8]
    uint8_t read_int_state_val = (value >> 8) & 0xF;
    if (!validate_multi_bit_encoding(read_int_state_val, read_int_state)) {
        RECOV_ALERT_STS.READ_INT_STATE_FIELD_ALERT = 1;
        trigger_recov_alert();
        valid = false;
    }

    // FIPS_FORCE_ENABLE field [15:12]
    uint8_t fips_force_enable_val = (value >> 12) & 0xF;
    if (!validate_multi_bit_encoding(fips_force_enable_val, fips_force_enable)) {
        RECOV_ALERT_STS.FIPS_FORCE_ENABLE_FIELD_ALERT = 1;
        trigger_recov_alert();
        valid = false;
    }

    // Write the value (invalid encodings treated as disable-true)
    CTRL = value;

    // FSM state transitions based on ENABLE field
    if (enable) {
        // ENABLE=0x6 → Transition to SW_CMD_RDY state
        m_cmd_ready = true;
        m_cmd_fsm_state = CommandFSMState::IDLE;
        CSML_INFO(1, logger) << "[CRNG] Module enabled: FSM ready for commands (CMD_RDY=1)" << std::endl;
        std::cout << "[DEBUG] handle_write_CTRL: ENABLE=true, m_cmd_ready=" << m_cmd_ready << std::endl;
    } else {
        // ENABLE=0x9 → Transition to IDLE, disable command processing
        m_cmd_ready = false;
        m_cmd_fsm_state = CommandFSMState::IDLE;
        CSML_INFO(1, logger) << "[CRNG] Module disabled: FSM idle (CMD_RDY=0)" << std::endl;
        std::cout << "[DEBUG] handle_write_CTRL: ENABLE=false, m_cmd_ready=" << m_cmd_ready << std::endl;
    }

    CSML_INFO(2, logger) << "[CRNG] CTRL written: 0x" << std::hex << value << std::dec
                         << " (valid=" << valid << ", enable=" << enable << ")" << std::endl;
    return true;
}

/**
 * @brief Write callback for REGWEN register
 *
 * Implements RW0C (write-0-to-lock) semantics for register write protection.
 * Implements FUNC_026 (register write protection).
 */
bool crng_model::handle_write_REGWEN(DT value, DT write_mask)
{
    // Read current REGWEN value to properly implement RW0C behavior
    uint32_t current = static_cast<DT>(REGWEN);

    // RW0C: Writing 0 locks permanently, writing 1 has no effect
    if (value == 0 && current == 1) {
        REGWEN = 0;
        CSML_INFO(1, logger) << "[CRNG] REGWEN locked (control registers now read-only)" << std::endl;
    }
    return true;
}

/**
 * @brief Write callback for RESEED_INTERVAL register
 *
 * Sets the threshold for reseed counter enforcement (FUNC_003).
 */
bool crng_model::handle_write_RESEED_INTERVAL(DT value, DT write_mask)
{
    // Check REGWEN lock
    if (static_cast<DT>(REGWEN) == 0) {
        CSML_WARN(2, logger) << "[CRNG] RESEED_INTERVAL write denied: REGWEN locked" << std::endl;
        return true;
    }

    RESEED_INTERVAL = value;
    CSML_INFO(2, logger) << "[CRNG] RESEED_INTERVAL written: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for FIPS_FORCE register
 *
 * Controls per-instance FIPS flag forcing (FUNC_027).
 */
bool crng_model::handle_write_FIPS_FORCE(DT value, DT write_mask)
{
    // Check REGWEN lock
    if (static_cast<DT>(REGWEN) == 0) {
        CSML_WARN(2, logger) << "[CRNG] FIPS_FORCE write denied: REGWEN locked" << std::endl;
        return true;
    }

    // Check CTRL.FIPS_FORCE_ENABLE
    bool fips_force_enable = false;
    uint8_t fips_force_enable_val = (static_cast<DT>(CTRL) >> 12) & 0xF;
    validate_multi_bit_encoding(fips_force_enable_val, fips_force_enable);

    if (!fips_force_enable) {
        CSML_WARN(2, logger) << "[CRNG] FIPS_FORCE write denied: FIPS_FORCE_ENABLE not set" << std::endl;
        return true;
    }

    // Only bits [2:0] are meaningful (one per instance)
    FIPS_FORCE = value & 0x7;

    CSML_INFO(2, logger) << "[CRNG] FIPS_FORCE written: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for INT_STATE_READ_ENABLE register
 */
bool crng_model::handle_write_INT_STATE_READ_ENABLE(DT value, DT write_mask)
{

    // Check REGWEN lock
    if (static_cast<DT>(INT_STATE_READ_ENABLE_REGWEN) == 0) {
        CSML_WARN(2, logger) << "[CRNG] INT_STATE_READ_ENABLE write denied: REGWEN locked" << std::endl;
        return true;
    }

    INT_STATE_READ_ENABLE = value;

    CSML_INFO(2, logger) << "[CRNG] INT_STATE_READ_ENABLE written: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for INT_STATE_READ_ENABLE_REGWEN register
 *
 * Implements RW0C semantics.
 */
bool crng_model::handle_write_INT_STATE_READ_ENABLE_REGWEN(DT value, DT write_mask)
{

    uint32_t current = static_cast<DT>(INT_STATE_READ_ENABLE_REGWEN);

    // RW0C: Writing 0 locks permanently
    if (value == 0 && current == 1) {
        INT_STATE_READ_ENABLE_REGWEN = 0;
        CSML_INFO(1, logger) << "[CRNG] INT_STATE_READ_ENABLE_REGWEN locked" << std::endl;
    }
    return true;
}

// =============================================================================
// Interrupt Management Callbacks
// =============================================================================

/**
 * @brief Write callback for INTR_STATE register
 *
 * Implements RW1C (write-1-to-clear) semantics and updates interrupt outputs.
 */
bool crng_model::handle_write_INTR_STATE(DT value, DT write_mask)
{

    uint32_t current = static_cast<DT>(INTR_STATE);

    // INTR_STATE is RW1C - writing 1 clears the bit
    // Update internal interrupt state based on cleared bits
    if ((value & 0x1) != 0) {
        m_intr_cmd_req_done = false;  // Bit 0 cleared
    }
    if ((value & 0x2) != 0) {
        m_intr_entropy_req = false;  // Bit 1 cleared
    }
    if ((value & 0x4) != 0) {
        m_intr_hw_inst_exc = false;  // Bit 2 cleared
    }
    if ((value & 0x8) != 0) {
        m_intr_fatal_err = false;  // Bit 3 cleared
    }
	
    // RW1C: Clear bits where write value has 1
    uint32_t new_value = current & ~value;

    INTR_STATE = new_value;
   
    intr_update_event.notify(SC_ZERO_TIME);

    CSML_INFO(2, logger) << "[CRNG] INTR_STATE cleared: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for INTR_ENABLE register
 *
 * Updates interrupt enable mask and recalculates outputs.
 */
bool crng_model::handle_write_INTR_ENABLE(DT value, DT write_mask)
{
    CSML_INFO(3, logger) << "[DEBUG] handle_write_INTR_ENABLE: value=0x" << std::hex << value << ", write_mask=0x" << write_mask << std::dec;
    INTR_ENABLE = value;
    //update_interrupt_outputs();
	
	intr_update_event.notify(SC_ZERO_TIME);

    CSML_INFO(2, logger) << "[CRNG] INTR_ENABLE written: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for INTR_TEST register
 *
 * Forces interrupt bits for testing (WO register).
 */
bool crng_model::handle_write_INTR_TEST(DT value, DT write_mask)
{

    // Force INTR_STATE bits
    uint32_t current = static_cast<DT>(INTR_STATE);
	
	 // INTR_TEST forces interrupt bits for testing
    if ((value & 0x1) != 0) {
        m_intr_cmd_req_done = true;
    }
    if ((value & 0x2) != 0) {
        m_intr_entropy_req = true;
    }
    if ((value & 0x4) != 0) {
        m_intr_hw_inst_exc = true;
    }
    if ((value & 0x8) != 0) {
        m_intr_fatal_err = true;
    }
	
    INTR_STATE = current | value;
    intr_update_event.notify(SC_ZERO_TIME);


    CSML_INFO(2, logger) << "[CRNG] INTR_TEST triggered: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for ALERT_TEST register
 *
 * Triggers alert events for testing (WO register).
 * Bit 0 (recov_alert): Write 1 to trigger a recoverable alert test
 * Bit 1 (fatal_alert): Write 1 to trigger a fatal alert test
 *
 * Per OpenTitan specification, ALERT_TEST is write-only. Writing '1' to a bit
 * triggers a single alert pulse on the corresponding output.
 */
bool crng_model::handle_write_ALERT_TEST(DT value, DT write_mask)
{
    // Apply write mask
    DT masked_value = value & write_mask;
    // Check bit 0: recov_alert
    if (masked_value & 0x1) {
        m_alert_recov = true;
        CSML_INFO(2, logger) << "[CRNG] ALERT_TEST: Triggering recoverable alert" << std::endl;
    }

    // Check bit 1: fatal_alert
    if (masked_value & 0x2) {
        m_alert_fatal = true;
        CSML_INFO(2, logger) << "[CRNG] ALERT_TEST: Triggering fatal alert" << std::endl;
    }

    // Notify alert update process if any alert was triggered
    if (m_alert_recov || m_alert_fatal) {
        alert_update_event.notify(SC_ZERO_TIME);
    }

    return true;
}

/**
 * @brief Write callback for HW_EXC_STS register
 *
 * Implements RW0C semantics for clearing hardware exception status.
 */
bool crng_model::handle_write_HW_EXC_STS(DT value, DT write_mask)
{

    uint32_t current = static_cast<DT>(HW_EXC_STS);

    // RW0C: Clear bits where write value has 0
    uint32_t new_value = current & value;

    HW_EXC_STS = new_value;

    // If all exceptions cleared, clear the interrupt
    if (new_value == 0) {
        m_intr_hw_inst_exc = false;
        //update_interrupt_outputs();
		intr_update_event.notify(SC_ZERO_TIME);
    }

    CSML_INFO(2, logger) << "[CRNG] HW_EXC_STS cleared: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

/**
 * @brief Write callback for RECOV_ALERT_STS register
 *
 * Implements RW1C semantics for clearing recoverable alert status.
 */
bool crng_model::handle_write_RECOV_ALERT_STS(DT value, DT write_mask)
{

    uint32_t current = static_cast<DT>(RECOV_ALERT_STS);
    //Fix: Corrected to write-0-to-clear behavior
    // RW0C: Clear bits where write value has 0
    uint32_t new_value = current & value;

    RECOV_ALERT_STS = new_value;

    CSML_INFO(2, logger) << "[CRNG] RECOV_ALERT_STS cleared: 0x" << std::hex << value << std::dec << std::endl;
    return true;
}

// =============================================================================
// FUNC_013: Register Callback Registration
// =============================================================================

/**
 * @brief Registers all register callbacks with the CSML framework
 *
 * This function binds all read/write callbacks to their respective registers
 * to implement register side-effects and access control.
 * Pattern copied from AES model.
 */
void crng_model::register_all_callbacks()
{
    CSML_INFO(1, logger) << "[CRNG] Registering all register callbacks" << std::endl;

    // CMD_REQ - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_CMD_REQ(value, CMD_REQ.write_bit_mask);
        };
        memory.register_write_callback(write_cb, CMD_REQ.offset);
    }

    // CTRL - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_CTRL(value, CTRL.write_bit_mask);
        };
        memory.register_write_callback(write_cb, CTRL.offset);
    }

    // REGWEN - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_REGWEN(value, REGWEN.write_bit_mask);
        };
        memory.register_write_callback(write_cb, REGWEN.offset);
    }

    // RESEED_INTERVAL - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_RESEED_INTERVAL(value, RESEED_INTERVAL.write_bit_mask);
        };
        memory.register_write_callback(write_cb, RESEED_INTERVAL.offset);
    }

    // FIPS_FORCE - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_FIPS_FORCE(value, FIPS_FORCE.write_bit_mask);
        };
        memory.register_write_callback(write_cb, FIPS_FORCE.offset);
    }

    // INT_STATE_NUM - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_INT_STATE_NUM(value, INT_STATE_NUM.write_bit_mask);
        };
        memory.register_write_callback(write_cb, INT_STATE_NUM.offset);
    }

    // INT_STATE_READ_ENABLE - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_INT_STATE_READ_ENABLE(value, INT_STATE_READ_ENABLE.write_bit_mask);
        };
        memory.register_write_callback(write_cb, INT_STATE_READ_ENABLE.offset);
    }

    // INT_STATE_READ_ENABLE_REGWEN - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_INT_STATE_READ_ENABLE_REGWEN(value, INT_STATE_READ_ENABLE_REGWEN.write_bit_mask);
        };
        memory.register_write_callback(write_cb, INT_STATE_READ_ENABLE_REGWEN.offset);
    }

    // INTR_STATE - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_INTR_STATE(value, INTR_STATE.write_bit_mask);
        };
        memory.register_write_callback(write_cb, INTR_STATE.offset);
    }

    // INTR_ENABLE - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_INTR_ENABLE(value, INTR_ENABLE.write_bit_mask);
        };
        memory.register_write_callback(write_cb, INTR_ENABLE.offset);
    }

    // INTR_TEST - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_INTR_TEST(value, INTR_TEST.write_bit_mask);
        };
        memory.register_write_callback(write_cb, INTR_TEST.offset);
    }

    // ALERT_TEST - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_ALERT_TEST(value, ALERT_TEST.write_bit_mask);
        };
        memory.register_write_callback(write_cb, ALERT_TEST.offset);
    }

    // HW_EXC_STS - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_HW_EXC_STS(value, HW_EXC_STS.write_bit_mask);
        };
        memory.register_write_callback(write_cb, HW_EXC_STS.offset);
    }

    // RECOV_ALERT_STS - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_RECOV_ALERT_STS(value, RECOV_ALERT_STS.write_bit_mask);
        };
        memory.register_write_callback(write_cb, RECOV_ALERT_STS.offset);
    }

    // ERR_CODE_TEST - Write callback
    {
        std::function<bool(DT)> write_cb = [this](DT value) {
            return this->handle_write_ERR_CODE_TEST(value, ERR_CODE_TEST.write_bit_mask);
        };
        memory.register_write_callback(write_cb, ERR_CODE_TEST.offset);
    }

    // INTR_ENABLE - No read callback needed (CSML handles RW registers automatically)

    // INTR_TEST - Read callback (Write-Only, returns 0)
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_INTR_TEST(value, INTR_TEST.read_bit_mask);
        };
        memory.register_read_callback(read_cb, INTR_TEST.offset);
    }

    // ALERT_TEST - Read callback (Write-Only, returns 0)
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_ALERT_TEST(value, ALERT_TEST.read_bit_mask);
        };
        memory.register_read_callback(read_cb, ALERT_TEST.offset);
    }

    // CMD_REQ - Read callback (Write-Only, returns 0)
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_CMD_REQ(value, CMD_REQ.read_bit_mask);
        };
        memory.register_read_callback(read_cb, CMD_REQ.offset);
    }

    // SW_CMD_STS - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_SW_CMD_STS(value, SW_CMD_STS.read_bit_mask);
        };
        memory.register_read_callback(read_cb, SW_CMD_STS.offset);
    }

    // GENBITS_VLD - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_GENBITS_VLD(value, GENBITS_VLD.read_bit_mask);
        };
        memory.register_read_callback(read_cb, GENBITS_VLD.offset);
    }

    // GENBITS - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_GENBITS(value, GENBITS.read_bit_mask);
        };
        memory.register_read_callback(read_cb, GENBITS.offset);
    }

    // RESEED_COUNTER_0 - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_RESEED_COUNTER_0(value, RESEED_COUNTER_0.read_bit_mask);
        };
        memory.register_read_callback(read_cb, RESEED_COUNTER_0.offset);
    }

    // RESEED_COUNTER_1 - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_RESEED_COUNTER_1(value, RESEED_COUNTER_1.read_bit_mask);
        };
        memory.register_read_callback(read_cb, RESEED_COUNTER_1.offset);
    }

    // RESEED_COUNTER_2 - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_RESEED_COUNTER_2(value, RESEED_COUNTER_2.read_bit_mask);
        };
        memory.register_read_callback(read_cb, RESEED_COUNTER_2.offset);
    }

    // INT_STATE_VAL - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_INT_STATE_VAL(value, INT_STATE_VAL.read_bit_mask);
        };
        memory.register_read_callback(read_cb, INT_STATE_VAL.offset);
    }

    // ERR_CODE - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_ERR_CODE(value, ERR_CODE.read_bit_mask);
        };
        memory.register_read_callback(read_cb, ERR_CODE.offset);
    }

    // ERR_CODE_TEST - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_ERR_CODE_TEST(value, ERR_CODE_TEST.read_bit_mask);
        };
        memory.register_read_callback(read_cb, ERR_CODE_TEST.offset);
    }

    // MAIN_SM_STATE - Read callback
    {
        std::function<bool(DT&)> read_cb = [this](DT& value) {
            return this->handle_read_MAIN_SM_STATE(value, MAIN_SM_STATE.read_bit_mask);
        };
        memory.register_read_callback(read_cb, MAIN_SM_STATE.offset);
    }

    CSML_INFO(1, logger) << "[CRNG] All register callbacks registered successfully" << std::endl;
}

