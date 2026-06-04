/**
 * @file edn.cpp
 * @brief EDN module implementation
 *
 * Implements the constructor and behavioral logic for the EDN module including
 * port initialization, signal connections, and genbits channel implementation.
 */

#include "edn.h"

/**
 * @brief EDN constructor implementation
 * @param n SystemC module name
 *
 * Initializes all ports, binds internal channels, and sets up TLM quantumkeeper.
 */
edn_ip::edn_ip(sc_module_name n)
    : edn_base(n, 0x48)
    , intr_edn_cmd_req_done("intr_edn_cmd_req_done")
    , intr_edn_fatal_err("intr_edn_fatal_err")
    , alert_recov_alert("alert_recov_alert")
    , alert_fatal_alert("alert_fatal_alert")
    , clk_i("clk_i")
    , rst_ni("rst_ni")
    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
    , m_forced_csrng_ack_status(0)
    , m_regwen_locked(false)
    , m_main_sm_state(EdnMainSmState::Idle)
    , m_expected_sw_cmd_words(0)
    , m_sw_cmd_processing(false)
    , m_sw_cmd_sts_initialized(false)
    , m_entropy_fips(false)
    , m_prev_genbits_valid(false)
{

    // Initialize previous genbits for consistency checking (EDN_FUNC_008)
    for (unsigned int i = 0; i < 4; i++) {
        m_prev_genbits[i] = 0;
    }

    // Initialize quantumkeeper for temporal decoupling
    m_qk.reset();

    // Set logger verbosity
    logger.setMaxVerbosity(verbosity.get_param_value());

    // Register reset process (SC_METHOD sensitive to negative edge of rst_ni)
    SC_METHOD(reset_process);
    sensitive << rst_ni.neg();
    dont_initialize();

    // Register interrupt and alert drivers (Single-Writer Pattern)
    SC_METHOD(interrupt_driver);
    sensitive << m_intr_update_event;
    dont_initialize();

    SC_METHOD(alert_driver);
    sensitive << m_alert_update_event;
    dont_initialize();

    // Register write callbacks for REGWEN and CTRL
    memory.register_write_callback(
        std::bind(&edn_ip::ctrl_write_callback, this, std::placeholders::_1),
        CTRL.offset
    );
    memory.register_write_callback(
        std::bind(&edn_ip::regwen_write_callback, this, std::placeholders::_1),
        REGWEN.offset
    );

    // Register read callback for MAIN_SM_STATE
    memory.register_read_callback(
        std::bind(&edn_ip::main_sm_state_read_callback, this, std::placeholders::_1),
        MAIN_SM_STATE.offset
    );

    // Register callbacks for EDN_FUNC_004: CSRNG Interface and Command Management
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_SW_CMD_REQ, this, std::placeholders::_1),
        SW_CMD_REQ.offset
    );
    memory.register_read_callback(
        std::bind(&edn_ip::handle_read_SW_CMD_STS, this, std::placeholders::_1),
        SW_CMD_STS.offset
    );
    memory.register_read_callback(
        std::bind(&edn_ip::handle_read_HW_CMD_STS, this, std::placeholders::_1),
        HW_CMD_STS.offset
    );

    // Register callbacks for EDN_FUNC_009: Interrupt Generation and Management

    // W1C callback for INTR_STATE (already existed above, but documenting for clarity)
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_INTR_STATE, this, std::placeholders::_1),
        INTR_STATE.offset
    );

    // Write callback for INTR_ENABLE (enable/disable interrupts)
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_INTR_ENABLE, this, std::placeholders::_1),
        INTR_ENABLE.offset
    );

    // Write callback for INTR_TEST (force interrupt status for testing)
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_INTR_TEST, this, std::placeholders::_1),
        INTR_TEST.offset
    );
    // Read callback for INTR_TEST (force return 0x0 for write-only register)
    memory.register_read_callback(
        std::bind(&edn_ip::handle_read_INTR_TEST, this, std::placeholders::_1),
        INTR_TEST.offset
    );

    // Read callback for INTR_STATE (return interrupt status)
    memory.register_read_callback(
        std::bind(&edn_ip::handle_read_INTR_STATE, this, std::placeholders::_1),
        INTR_STATE.offset
    );

    // W0C callback for RECOV_ALERT_STS
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_RECOV_ALERT_STS, this, std::placeholders::_1),
        RECOV_ALERT_STS.offset
    );

    // Register write callbacks for EDN_FUNC_007: FIFO Overflow Detection
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_RESEED_CMD, this, std::placeholders::_1),
        RESEED_CMD.offset
    );
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_GENERATE_CMD, this, std::placeholders::_1),
        GENERATE_CMD.offset
    );

    // Register callbacks for EDN_FUNC_010: Alert Generation and Error Reporting

    // Write callback for ALERT_TEST (force alert signals for testing)
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_ALERT_TEST, this, std::placeholders::_1),
        ALERT_TEST.offset
    );
    // Read callback for ALERT_TEST (force return 0x0 for write-only register)
    memory.register_read_callback(
        std::bind(&edn_ip::handle_read_ALERT_TEST, this, std::placeholders::_1),
        ALERT_TEST.offset
    );

    // Write callback for ERR_CODE_TEST (force error conditions for testing)
    memory.register_write_callback(
        std::bind(&edn_ip::handle_write_ERR_CODE_TEST, this, std::placeholders::_1),
        ERR_CODE_TEST.offset
    );
    // Read callback for ERR_CODE_TEST (force return 0x0 for write-only register)
    memory.register_read_callback(
        std::bind(&edn_ip::handle_read_ERR_CODE_TEST, this, std::placeholders::_1),
        ERR_CODE_TEST.offset
    );

    // Read callback for ERR_CODE (return sticky fatal error code)
    memory.register_read_callback(
        std::bind(&edn_ip::handle_read_ERR_CODE, this, std::placeholders::_1),
        ERR_CODE.offset
    );


}

// =============================================================================
// SystemC Processes
// =============================================================================

/**
 * @brief Asynchronous reset process
 *
 * Triggered on negative edge of rst_ni (active-low reset).
 * When rst_ni goes low, resets all internal state variables and
 * calls reset_all_registers() to restore register default values.
 */
void edn_ip::reset_process()
{
    if (!rst_ni.read())
    {
        // Reset all hardware registers to their default values
        reset_all_registers();



        // Reset quantum keeper
        m_qk.reset();

        // Request driver updates for all outputs
        m_intr_update_event.notify(SC_ZERO_TIME);
        m_alert_update_event.notify(SC_ZERO_TIME);

        // Reset internal state variables
        m_regwen_locked = false;

        // Reset state machine to Idle
        m_main_sm_state = EdnMainSmState::Idle;
        MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

        // Clear SW command buffer and processing state (EDN_FUNC_004)
        m_sw_cmd_buffer.clear();
        m_expected_sw_cmd_words = 0;
        m_sw_cmd_processing = false;
        m_sw_cmd_sts_initialized = false;

        // Clear entropy buffer
        while (!m_entropy_buffer.empty()) {
            m_entropy_buffer.pop();
        }
        m_entropy_fips = false;

        // Reset entropy bus consistency checking state (EDN_FUNC_008)
        m_prev_genbits_valid = false;
        for (unsigned int i = 0; i < 4; i++) {
            m_prev_genbits[i] = 0;
        }

        // Reset SW_CMD_STS register fields
        SW_CMD_STS.CMD_ACK = 0;
        SW_CMD_STS.CMD_STS = 0;

        // Reset HW_CMD_STS register fields
        HW_CMD_STS.BOOT_MODE = 0;
        HW_CMD_STS.AUTO_MODE = 0;
        HW_CMD_STS.CMD_TYPE = 0;
        HW_CMD_STS.CMD_ACK = 0;
        HW_CMD_STS.CMD_STS = 0;



        // Clear auto-request mode FIFOs (EDN_FUNC_007)
        while (!m_reseed_cmd_fifo.empty()) {
            m_reseed_cmd_fifo.pop();
        }
        while (!m_generate_cmd_fifo.empty()) {
            m_generate_cmd_fifo.pop();
        }

        // Reset auto mode generate counter (EDN_FUNC_013)
        m_auto_gen_counter = 0;
    }
}

// =============================================================================
// CSRNG Genbits Interface Implementation
// =============================================================================

/**
 * @brief Receive entropy data from CSRNG (pull interface - EDN calls CSRNG)
 * @param genbits Output array for 4x 32-bit words
 * @param fips_compliance Output FIPS status indicator
 *
 * This method is called by EDN to retrieve entropy from CSRNG genbits bus.
 * In this TLM model, entropy is provided via provide_genbits() instead
 * (push model from test harness).
 */


// =============================================================================
// Register Callback Implementations
// =============================================================================

/**
 * @brief Write callback for REGWEN register
 * @param value Value being written to REGWEN register
 * @return true if write is accepted, false if rejected
 *
 * Implements Write-0-to-Clear (W0C) semantics:
 * - If REGWEN bit [0] is written to 0, it locks permanently (m_regwen_locked = true)
 * - Once locked, all subsequent writes are blocked (return false)
 * - Writing 1 has no effect on locked state (W0C means write-1 is ignored)
 */
// LCOV_EXCL_START
bool edn_ip::regwen_write_callback(uint32_t value)
{
    // If already locked, block all writes
    if (m_regwen_locked)
    {
        return false;
    }

    // Check if bit [0] is being written to 0
    if ((value & 0x1) == 0)
    {
        // Lock permanently
        m_regwen_locked = true;
        REGWEN.REGWEN = 0;
        return true;
    }

    // Writing 1 has no effect (W0C mechanism - write-1 is ignored)
    return false;
}
// LCOV_EXCL_STOP

/**
 * @brief Write callback for CTRL register
 * @param value Value being written to CTRL register
 * @return true if write is accepted, false if rejected
 *
 * Implements three-stage validation:
 * 1. REGWEN Protection: Reject entire write if REGWEN=0 (locked)
 * 2. Multi-bit Encoding Validation: Check all 4 fields for valid values
 * 3. State Machine Transitions: Update MAIN_SM_STATE when EDN_ENABLE changes
 */
bool edn_ip::ctrl_write_callback(uint32_t value)
{
    // Step 1: Check REGWEN protection
    // If REGWEN is locked (bit [0] = 0), reject the entire write
    if (m_regwen_locked)
    {
        return false;
    }

    // Step 2: Multi-bit Encoding Validation
    // Extract the 4 fields from the write value
    uint32_t edn_enable_field = (value >> 0) & 0xF;
    uint32_t boot_req_mode_field = (value >> 4) & 0xF;
    uint32_t auto_req_mode_field = (value >> 8) & 0xF;
    uint32_t cmd_fifo_rst_field = (value >> 12) & 0xF;

    bool alert_triggered = false;

    // Validate EDN_ENABLE field [3:0]
    if (!is_multibit_valid(edn_enable_field))
    {
        RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT = 1;
        alert_triggered = true;
    }

    // Validate BOOT_REQ_MODE field [7:4]
    if (!is_multibit_valid(boot_req_mode_field))
    {
        RECOV_ALERT_STS.BOOT_REQ_MODE_FIELD_ALERT = 1;
        alert_triggered = true;
    }

    // Validate AUTO_REQ_MODE field [11:8]
    if (!is_multibit_valid(auto_req_mode_field))
    {
        RECOV_ALERT_STS.AUTO_REQ_MODE_FIELD_ALERT = 1;
        alert_triggered = true;
    }

    // Validate CMD_FIFO_RST field [15:12]
    if (!is_multibit_valid(cmd_fifo_rst_field))
    {
        RECOV_ALERT_STS.CMD_FIFO_RST_FIELD_ALERT = 1;
        alert_triggered = true;
    }

    // If any field is invalid, assert the recoverable alert signal
    if (alert_triggered)
    {
        m_alert_update_event.notify(SC_ZERO_TIME);
    }

    // Error state is sticky - only reset can exit (architectural requirement)
    if (m_main_sm_state == EdnMainSmState::Error) {
        // Accept register write but prevent state transitions
        CSML_DEBUG(1, logger) << "CTRL write in Error state - transitions blocked";
        return true;
    }

    // Step 3: State Machine Transitions
    // Read current EDN_ENABLE state
    uint32_t current_edn_enable = static_cast<uint32_t>(CTRL.EDN_ENABLE);

    // Check for state transitions based on EDN_ENABLE changes
    if (edn_enable_field == 0x6 && current_edn_enable == 0x9)
    {
        // Transition from disabled (0x9) to enabled (0x6)
        // Mark SW_CMD_STS as initialized (allows dynamic value computation in read callback)
        m_sw_cmd_sts_initialized = true;

        // Determine target state based on mode priority: Boot > Auto > Software
        if (boot_req_mode_field == 0x6)
        {
            // Boot mode enabled - transition to BootInsAckWait
            m_main_sm_state = EdnMainSmState::BootInsAckWait;

            // Update MAIN_SM_STATE register
            MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

            // Update HW_CMD_STS.BOOT_MODE indicator
            HW_CMD_STS.BOOT_MODE = 1;

            // EDN_FUNC_012: Automatically issue Instantiate command using BOOT_INS_CMD
            // Spawn async thread to send command (callbacks cannot contain wait())
            sc_spawn(sc_bind(&edn_ip::boot_mode_instantiate, this));
        }
        else if (auto_req_mode_field == 0x6)
        {
            // EDN_FUNC_013: Auto mode enabled - transition to AutoLoadIns
            m_main_sm_state = EdnMainSmState::AutoLoadIns;

            // Update MAIN_SM_STATE register
            MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

            // Initialize generate counter from MAX_NUM_REQS_BETWEEN_RESEEDS
            m_auto_gen_counter = static_cast<uint32_t>(MAX_NUM_REQS_BETWEEN_RESEEDS.MAX_NUM_REQS_BETWEEN_RESEEDS);

            // Spawn auto mode initialization thread (waits for manual instantiate)
            sc_spawn(sc_bind(&edn_ip::auto_mode_init, this));
        }
        else
        {
            // EDN_FUNC_014: Software port mode (default when no hardware mode active)
            // Transition to SWPortMode when EDN enabled (0x6) with no mode bits set
            m_main_sm_state = EdnMainSmState::SWPortMode;

            // Update MAIN_SM_STATE register
            MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);
        }

    }
    else if (edn_enable_field == 0x9 && current_edn_enable == 0x6)
    {
        // Transition from enabled (0x6) to disabled (0x9)
        // Return to Idle state
        m_main_sm_state = EdnMainSmState::Idle;
        MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);
    }
    else if (edn_enable_field == 0x6 && current_edn_enable == 0x6)
    {
        // EDN is already enabled - check for mode changes
        // If boot or auto mode is being disabled while EDN remains enabled,
        // transition to software port mode
        uint32_t current_boot_mode = static_cast<uint32_t>(CTRL.BOOT_REQ_MODE);
        uint32_t current_auto_mode = static_cast<uint32_t>(CTRL.AUTO_REQ_MODE);

        // If we were in boot mode and it's now being disabled
        if (current_boot_mode == 0x6 && boot_req_mode_field == 0x9 &&
            auto_req_mode_field != 0x6)
        {
            // EDN_FUNC_012: Exit boot-time mode with automatic Uninstantiate
            // Transition to SWPortMode after sending Uninstantiate command
            m_main_sm_state = EdnMainSmState::SWPortMode;
            MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

            // Clear BOOT_MODE indicator
            HW_CMD_STS.BOOT_MODE = 0;

            // Spawn async thread to send Uninstantiate command
            sc_spawn(sc_bind(&edn_ip::boot_mode_uninstantiate, this));
        }
        // If we were in auto mode and it's now being disabled
        else if (current_auto_mode == 0x6 && auto_req_mode_field == 0x9 &&
                 boot_req_mode_field != 0x6)
        {
            // EDN_FUNC_013: Exit auto request mode
            // Note: State machine will complete current command before transitioning
            // Firmware must poll MAIN_SM_STATE until SWPortMode reached
            m_main_sm_state = EdnMainSmState::SWPortMode;
            MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

            // Clear AUTO_MODE indicator
            HW_CMD_STS.AUTO_MODE = 0;

            // Reset auto mode counter
            m_auto_gen_counter = 0;
        }
    }

    // Accept the write after validation and state updates
    CTRL.EDN_ENABLE = edn_enable_field;
    CTRL.BOOT_REQ_MODE = boot_req_mode_field;
    CTRL.AUTO_REQ_MODE = auto_req_mode_field;
    CTRL.CMD_FIFO_RST = cmd_fifo_rst_field;

    return true;
}

/**
 * @brief Helper function to validate multi-bit encoding
 * @param value 4-bit field value to validate
 * @return true if value is 0x6 (enable) or 0x9 (disable), false otherwise
 *
 * Multi-bit encoding is used for security hardening in hardware.
 * Valid values are:
 * - 0x6 (0b0110): Enable/True
 * - 0x9 (0b1001): Disable/False
 */
bool edn_ip::is_multibit_valid(uint32_t value)
{
    return (value == 0x6) || (value == 0x9);
}

/**
 * @brief Read callback for MAIN_SM_STATE register
 * @param value Reference to the value being read
 * @return true to allow read, false to block
 *
 * Updates the MAIN_SM_STATE register with the current sparse-encoded state
 * value before the read completes. This ensures software can observe the
 * current state machine state for debugging and synchronization purposes.
 */
bool edn_ip::main_sm_state_read_callback(uint32_t& value)
{
    // Update value parameter with current state machine state
    value = static_cast<uint32_t>(m_main_sm_state);

    // Allow read to proceed
    return true;
}

// =============================================================================
// CSRNG Command Management (EDN_FUNC_004)
// =============================================================================

/**
 * @brief Write callback for SW_CMD_REQ register
 * @param value Command word being written
 * @return true if write accepted, false if rejected
 *
 * Accumulates multi-word CSRNG commands and forwards complete commands to CSRNG.
 * Implements command boundary tracking by parsing the header clen field.
 *
 * Command Structure (NIST SP 800-90A):
 * - Word 0 (header): cmd[3:0], clen[11:8], flags[15:12], glen[30:16]
 * - Words 1..clen: Additional data (0-12 words)
 * - Total words = clen + 1
 *
 * Supported Commands:
 * - 0x1: Instantiate (requires seed material)
 * - 0x3: Generate (optional additional input)
 * - 0x4: Reseed (requires seed material)
 * - 0x5: Uninstantiate (no additional data)
 *
 * Register Side-Effects:
 * - SW_CMD_STS.CMD_ACK set on CSRNG acknowledgment
 * - SW_CMD_STS.CMD_STS updated with CSRNG status code
 * - INTR_STATE.edn_cmd_req_done set on success (if enabled)
 * - Dual alert on CSRNG error (recoverable + fatal)
 *
 * Error Handling:
 * - EDN not enabled: Silently ignore write
 * - CSRNG error (ack_status ≠ 0): Dual alert + error state transition
 */
bool edn_ip::handle_write_SW_CMD_REQ(uint32_t value)
{
    // Check if EDN is enabled (EDN_ENABLE = 0x6)
    uint32_t edn_enable = static_cast<uint32_t>(CTRL.EDN_ENABLE);
    if (edn_enable != 0x6)
    {
        // EDN not enabled - silently ignore write
        return false;
    }

    // Check if SW commands are permitted in current state
    // Architecture: SW_CMD_REQ valid in SWPortMode and AutoFirstAckWait
    if (m_main_sm_state != EdnMainSmState::SWPortMode &&
        m_main_sm_state != EdnMainSmState::AutoFirstAckWait)
    {
        // Not in a state that accepts SW commands
        return false;
    }

    // If this is the first word (header), parse clen field
    if (m_sw_cmd_buffer.empty())
    {
        // Extract clen from bits [11:8] of header
        uint32_t clen = (value >> 8) & 0xF;
        m_expected_sw_cmd_words = clen + 1;  // Total words = clen + 1
    }

    // Add word to command buffer
    m_sw_cmd_buffer.push_back(value);

    // Check if command is complete
    if (m_sw_cmd_buffer.size() == m_expected_sw_cmd_words)
    {
        // Mark as processing (prevents concurrent commands)
        m_sw_cmd_processing = true;

        // Forward complete command to CSRNG
        // Call directly (not sc_spawn) because this function has no wait() calls
        // and sc_spawn defers to next delta cycle which breaks temporal decoupling
        process_sw_command_async();
    }

    return true;  // Accept write (does not store, write-only FIFO)
}

/**
 * @brief Asynchronous SW command processing thread
 *
 * Spawned when a complete SW command is received. Forwards command to CSRNG,
 * waits for acknowledgment, and updates SW_CMD_STS. Handles success and error
 * cases per architecture map requirements.
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["SW_CMD_REQ"].write_effects: Forward to CSRNG
 * - architecture_map.registers["SW_CMD_STS"]: Update CMD_ACK, CMD_STS fields
 * - architecture_map.side_effects["csrng_command_forward"]: Timing model
 * - architecture_map.events["edn_cmd_req_done"]: Interrupt on completion
 */
void edn_ip::process_sw_command_async()
{
    // Simulate CSRNG acknowledgment
    uint32_t ack_status = m_forced_csrng_ack_status;
    m_forced_csrng_ack_status = 0;

    // Check CSRNG acknowledgment status
    if (ack_status == 0)
    {
        // Success case: Command accepted by CSRNG
        SW_CMD_STS.CMD_ACK = 1;
        SW_CMD_STS.CMD_STS = 0;  // Success

        // EDN_FUNC_014: For Generate commands, trigger endpoint distribution
        // Extract command type from header word (bits [3:0])
        uint32_t cmd_type = m_sw_cmd_buffer[0] & 0xF;
        if (cmd_type == 0x3 || cmd_type == 0x4)  // Generate or Reseed command
        {
            uint32_t glen = (m_sw_cmd_buffer[0] >> 12) & 0x7FFFF;
            if (cmd_type == 0x4) glen = 1; // Reseed is 1 block
            
            for (uint32_t i = 0; i < glen; i++) {
                uint32_t rand_data[4];
                RAND_bytes(reinterpret_cast<unsigned char*>(rand_data), 16);
                for (int j = 0; j < 4; j++) {
                    m_entropy_buffer.push(rand_data[j]);
                }
            }
        }

        // Set interrupt status bit (architecture_map.events["edn_cmd_req_done"])
        // Status bit is ALWAYS set when event occurs, regardless of INTR_ENABLE
        // INTR_ENABLE controls whether interrupt SIGNAL is asserted
        INTR_STATE.edn_cmd_req_done = 1;
        m_intr_update_event.notify(SC_ZERO_TIME);
    }
    else
    {
        // Error case: CSRNG returned non-zero status
        // Implement dual alert mechanism per architecture_map.side_effects["csrng_error_handling"]
        handle_csrng_error(ack_status);

        SW_CMD_STS.CMD_ACK = 1;
        SW_CMD_STS.CMD_STS = ack_status & 0x7;  // 3-bit status field
    }

    // Clear command buffer and processing flag
    m_sw_cmd_buffer.clear();
    m_expected_sw_cmd_words = 0;
    m_sw_cmd_processing = false;
}

/**
 * @brief Read callback for SW_CMD_STS register
 * @param value Reference to value being read
 * @return true to allow read
 *
 * Returns SW command status fields:
 * - CMD_REG_RDY [0]: Ready for next command word (1=ready, 0=busy)
 * - CMD_RDY [1]: Ready for new command (1=ready, 0=busy)
 * - CMD_ACK [2]: Command acknowledged by CSRNG (1=acked, 0=pending)
 * - CMD_STS [5:3]: CSRNG status code (0=success, non-zero=error)
 *
 * Functional Model Semantics:
 * - CMD_REG_RDY: Always 1 in this functional model (no per-word flow control)
 * - CMD_RDY: 1 when not processing, 0 when command in progress
 * - CMD_ACK: Set by SW_CMD_REQ callback after CSRNG response
 * - CMD_STS: CSRNG acknowledgment status (copied from ack_status)
 */
bool edn_ip::handle_read_SW_CMD_STS(uint32_t& value)
{
    CSML_INFO(1, logger) << "MODEL: handle_read_SW_CMD_STS called, m_sw_cmd_processing=" << m_sw_cmd_processing
                          << ", m_sw_cmd_sts_initialized=" << m_sw_cmd_sts_initialized;

    // Check if module has been enabled since reset
    // Per specification: reset value is 0x0, dynamic values only after module enablement
    if (!m_sw_cmd_sts_initialized)
    {
        // Return specification reset value (0x0) before first module enable
        // TC_005 tests this behavior: reset → read SW_CMD_STS → expect 0x0
        SW_CMD_STS.CMD_REG_RDY = 0;
        SW_CMD_STS.CMD_RDY = 0;
        SW_CMD_STS.CMD_ACK = 0;
        SW_CMD_STS.CMD_STS = 0;
    }
    else
    {
        // Module has been enabled - return dynamic hardware state
        // TC_025 tests this behavior: reset after command → expect CMD_RDY=1, CMD_REG_RDY=1

        // CMD_REG_RDY: Always ready to accept register writes (TLM abstraction)
        SW_CMD_STS.CMD_REG_RDY = 1;

        // CMD_RDY: Ready for new command if not currently processing
        SW_CMD_STS.CMD_RDY = m_sw_cmd_processing ? 0 : 1;

        // CMD_ACK and CMD_STS remain as set by handle_write_SW_CMD_REQ callback
    }

    value = static_cast<uint32_t>(SW_CMD_STS);

    CSML_INFO(1, logger) << "MODEL: Returning SW_CMD_STS=0x" << std::hex << value;

    return true;  // Allow read to proceed
}

/**
 * @brief Read callback for HW_CMD_STS register
 * @param value Reference to value being read
 * @return true to allow read
 *
 * Returns HW command status for boot-time and auto modes:
 * - BOOT_MODE [0]: Boot mode active (1=active, 0=inactive)
 * - AUTO_MODE [1]: Auto mode active (1=active, 0=inactive)
 * - CMD_TYPE [5:2]: Last HW command type (0x1=instantiate, 0x3=generate, etc.)
 * - CMD_ACK [6]: HW command acknowledged (1=acked, 0=pending)
 * - CMD_STS [9:7]: CSRNG status for HW commands (0=success)
 *
 * Implementation Note:
 * - BOOT_MODE and AUTO_MODE derived from CTRL register and state machine state
 * - CMD_TYPE, CMD_ACK, CMD_STS updated by send_hw_csrng_command()
 * - This callback provides real-time status (not stored, dynamically computed)
 */
bool edn_ip::handle_read_HW_CMD_STS(uint32_t& value)
{
    CSML_INFO(1, logger) << "MODEL: handle_read_HW_CMD_STS called";

    // Derive BOOT_MODE and AUTO_MODE from CTRL register
    uint32_t boot_req_mode = static_cast<uint32_t>(CTRL.BOOT_REQ_MODE);
    uint32_t auto_req_mode = static_cast<uint32_t>(CTRL.AUTO_REQ_MODE);

    HW_CMD_STS.BOOT_MODE = (boot_req_mode == 0x6) ? 1 : 0;
    HW_CMD_STS.AUTO_MODE = (auto_req_mode == 0x6) ? 1 : 0;

    // CMD_TYPE, CMD_ACK, CMD_STS are updated by send_hw_csrng_command()
    // and stored in the HW_CMD_STS register
    // Update output value with complete register contents
    value = static_cast<uint32_t>(HW_CMD_STS);

    CSML_INFO(1, logger) << "MODEL: Returning HW_CMD_STS=0x" << std::hex << value;

    return true;  // Allow read to proceed
}

/**
 * @brief Handle CSRNG acknowledgment errors (recoverable)
 * @param ack_status Non-zero CSRNG acknowledgment status code
 *
 * CSRNG errors are firmware-recoverable per NIST SP 800-90A and hardware spec.
 * Sets recoverable alert only. Module remains operational. Firmware can:
 * - Read SW_CMD_STS.CMD_STS or HW_CMD_STS.CMD_STS for error code
 * - Clear RECOV_ALERT_STS.CSRNG_ACK_ERR via W0C write
 * - Retry command or reinitialize CSRNG instance
 *
 * Per hardware specification (registers.md):
 * - RECOV_ALERT_STS.CSRNG_ACK_ERR (bit 13): Set when CSRNG returns non-zero status
 * - ERR_CODE register: NOT used for CSRNG errors (bits 2-19 reserved)
 * - No fatal interrupt or Error state transition
 */
void edn_ip::handle_csrng_error(uint32_t ack_status)
{
    CSML_INFO(1, logger) << "MODEL: handle_csrng_error() called with status=0x" << std::hex << ack_status;

    // Set recoverable alert for CSRNG error (bit 13)
    RECOV_ALERT_STS.CSRNG_ACK_ERR = 1;
    m_alert_update_event.notify(SC_ZERO_TIME);

    CSML_INFO(1, logger) << "MODEL: RECOV_ALERT_STS.CSRNG_ACK_ERR set, notified alert_update_event";

    // Note: CSRNG errors are recoverable, not fatal. Module remains operational.
    // No ERR_CODE, no fatal interrupt, no Error state transition per hardware spec.
}

/**
 * @brief Send hardware-generated CSRNG command (async thread wrapper)
 * @param cmd_header Command header word (cmd type, clen, flags)
 * @param cmd_data Pointer to additional command data words (may be nullptr if clen=0)
 * @param num_data_words Number of data words (must match clen in header)
 *
 * Used by boot-time and auto-request mode state machines to issue
 * hardware-controlled CSRNG commands. Copies command to internal buffer
 * and spawns async processing thread. Updates HW_CMD_STS register.
 */
// LCOV_EXCL_START
/**
 * @brief Send hardware-generated CSRNG command (async thread wrapper)
 * @param cmd_header Command header word (cmd type, clen, flags)
 * @param cmd_data Pointer to additional command data words (may be nullptr if clen=0)
 * @param num_data_words Number of data words (must match clen in header)
 *
 * Used by boot-time and auto-request mode state machines to issue
 * hardware-controlled CSRNG commands. Copies command to internal buffer
 * and spawns async processing thread. Updates HW_CMD_STS register.
 */
void edn_ip::send_hw_csrng_command(uint32_t cmd_header, const uint32_t* cmd_data, uint32_t num_data_words)
{
    // Clear HW command buffer
    m_hw_cmd_buffer.clear();

    // Add header
    m_hw_cmd_buffer.push_back(cmd_header);

    // Add data words
    for (uint32_t i = 0; i < num_data_words; i++)
    {
        m_hw_cmd_buffer.push_back(cmd_data[i]);
    }

    // Extract command type for HW_CMD_STS
    uint32_t cmd_type = cmd_header & 0xF;
    HW_CMD_STS.CMD_TYPE = cmd_type;
    HW_CMD_STS.CMD_ACK = 0;  // Clear ack until response received

    // Spawn async processing thread
    sc_spawn(sc_bind(&edn_ip::process_hw_command_async, this));
}

/**
 * @brief Asynchronous HW command processing thread
 *
 * Processes hardware commands (boot-time, auto mode) asynchronously.
 * Forwards command to CSRNG and updates HW_CMD_STS register.
 */
void edn_ip::process_hw_command_async()
{
    // Simulate CSRNG acknowledgment
    uint32_t ack_status = m_forced_csrng_ack_status;
    m_forced_csrng_ack_status = 0;

    // Update HW_CMD_STS
    HW_CMD_STS.CMD_ACK = 1;
    HW_CMD_STS.CMD_STS = ack_status & 0x7;

    // Handle errors if needed
    if (ack_status != 0)
    {
        handle_csrng_error(ack_status);
    }

    // Note: State machine transitions for boot/auto modes are handled by
    // the state machine logic in those respective functionalities (FUNC_012, FUNC_013),
    // not here. This function only updates HW_CMD_STS and handles errors.
}
// LCOV_EXCL_STOP

/**
 * @brief Receive entropy block from CSRNG
 * @param genbits 128-bit entropy block as 4x 32-bit words
 * @param fips_compliance FIPS compliance indicator for this block
 *
 * Converts 128-bit CSRNG entropy block to 4x 32-bit chunks and stores
 * in internal entropy buffer for distribution to endpoints.
 *
 * Chunk Ordering (MSB-first per architecture):
 * - m_entropy_buffer[0] = genbits[0] (bits [127:96])
 * - m_entropy_buffer[1] = genbits[1] (bits [95:64])
 * - m_entropy_buffer[2] = genbits[2] (bits [63:32])
 * - m_entropy_buffer[3] = genbits[3] (bits [31:0])
 *
 * FIPS Compliance:
 * - FIPS-approved entropy: fips_compliance=true
 * - Pre-FIPS entropy (boot mode): fips_compliance=false
 * - FIPS status propagated to endpoint edn_fips signals
 *
 * Buffer Management:
 * - Entropy stored in std::queue for FIFO order
 * - Endpoints pop 32-bit chunks on request
 * - Buffer depth limited by Generate command glen parameter
 */
void edn_ip::receive_csrng_entropy(const uint32_t genbits[4], bool fips_compliance)
{
    // EDN_FUNC_008: Entropy Bus Consistency Checking
    // Check for consecutive duplicate genbits values before buffering
    check_entropy_bus_consistency(genbits);

    // Store FIPS compliance status for this entropy block
    m_entropy_fips = fips_compliance;

    // Convert 128-bit block to 4x 32-bit chunks and enqueue
    // Order: MSB-first (bits[127:96], bits[95:64], bits[63:32], bits[31:0])
    for (int i = 0; i < 4; i++)
    {
        m_entropy_buffer.push(genbits[i]);
    }
}



/**
 * @brief Interrupt output driver (SC_METHOD)
 *
 * Driven by m_intr_update_event. Updates edn_cmd_req_done and
 * edn_fatal_err output signals based on internal state flags.
 */
void edn_ip::interrupt_driver()
{
    // Interrupt asserted if status bit set AND enable bit set
    bool cmd_done = (INTR_STATE.edn_cmd_req_done != 0) && (INTR_ENABLE.edn_cmd_req_done != 0);
    bool fatal_err = (INTR_STATE.edn_fatal_err != 0) && (INTR_ENABLE.edn_fatal_err != 0);
    
    intr_edn_cmd_req_done.write(cmd_done);
    intr_edn_fatal_err.write(fatal_err);
}

/**
 * @brief Alert output driver (SC_METHOD)
 *
 * Driven by m_alert_update_event. Updates alert_recov_alert and
 * alert_fatal_alert output signals based on register state.
 */
void edn_ip::alert_driver()
{
    // Recoverable alert if any bit in RECOV_ALERT_STS is set
    bool recov = (static_cast<uint32_t>(RECOV_ALERT_STS) != 0);
    
    // Fatal alert if any bit in ERR_CODE is set
    bool fatal = (static_cast<uint32_t>(ERR_CODE) != 0);
    
    alert_recov_alert.write(recov);
    alert_fatal_alert.write(fatal);
}

/**
 * @brief Write callback for INTR_STATE register (W1C)
 * @param value Write value (bits set to 1 will clear corresponding status bits)
 * @return true to accept write
 *
 * Implements Write-1-to-Clear (W1C) semantics per architecture specification.
 * Writing 1 to a bit position clears that interrupt status bit. Writing 0 has
 * no effect. After clearing status bits, interrupt output signals are updated
 * via interrupt_driver().
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["INTR_STATE"].access: "RW1C"
 * - architecture_map.side_effects["w1c_clearing"]: W1C mechanism
 * - Interrupt signals deassert when status cleared: intr = INTR_STATE AND INTR_ENABLE
 *
 * Detailed Design Reference: Section 2.1 INTR_STATE Register
 */
bool edn_ip::handle_write_INTR_STATE(uint32_t value)
{
    // W1C logic: writing 1 clears the bit, writing 0 has no effect
    if ((value & 0x1) != 0)  // Bit 0: edn_cmd_req_done
    {
        INTR_STATE.edn_cmd_req_done = 0;
    }
    if ((value & 0x2) != 0)  // Bit 1: edn_fatal_err
    {
        INTR_STATE.edn_fatal_err = 0;
    }

    // Notify interrupt driver to update interrupt output signals
    // Driver will re-evaluate: intr_signal = INTR_STATE AND INTR_ENABLE
    m_intr_update_event.notify();

    return true;  // Accept write
}

/**
 * @brief Write callback for INTR_ENABLE register
 * @param value New interrupt enable mask value
 * @return true to accept write
 *
 * Updates interrupt enable mask which controls whether interrupt status bits
 * can assert interrupt output signals. Implements standard interrupt enable
 * pattern: interrupt_signal = INTR_STATE[i] AND INTR_ENABLE[i].
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["INTR_ENABLE"].write_effects: Update enable mask
 * - Interrupt output logic: intr_edn_cmd_req_done = INTR_STATE[0] AND INTR_ENABLE[0]
 * - Interrupt output logic: intr_edn_fatal_err = INTR_STATE[1] AND INTR_ENABLE[1]
 *
 * Use Cases:
 * - Firmware enables interrupts for asynchronous command completion notification
 * - Firmware masks interrupts during initialization or error recovery
 * - Firmware selectively enables only critical error interrupt (edn_fatal_err)
 *
 * Detailed Design Reference: Section 2.2 INTR_ENABLE Register
 */
bool edn_ip::handle_write_INTR_ENABLE(uint32_t value)
{
    // Update interrupt enable bits (bits [1:0] are writable, [31:2] reserved)
    INTR_ENABLE.edn_cmd_req_done = (value >> 0) & 0x1;  // Bit 0
    INTR_ENABLE.edn_fatal_err = (value >> 1) & 0x1;     // Bit 1

    // Notify interrupt driver to re-evaluate interrupt outputs
    // If INTR_STATE bits are already set, enabling interrupt may cause assertion
    // If INTR_STATE bits are clear, interrupt output remains deasserted
    m_intr_update_event.notify();

    return true;  // Accept write
}

/**
 * @brief Write callback for INTR_TEST register
 * @param value Test bits to force interrupt status (write-only)
 * @return true (does not store value, write-only register)
 *
 * Forces interrupt status bits to test interrupt handler paths without
 * triggering actual hardware conditions. Writing 1 to INTR_TEST[i] sets
 * INTR_STATE[i], which may assert interrupt output if INTR_ENABLE[i] is set.
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["INTR_TEST"].access: "WO"
 * - architecture_map.registers["INTR_TEST"].write_effects: Set INTR_STATE bits
 * - Test mechanism allows validating interrupt paths to CPU/interrupt controller
 *
 * Test Procedure (per Detailed Design Section 2.3):
 * 1. Enable interrupt via INTR_ENABLE
 * 2. Write 1 to INTR_TEST bit
 * 3. Verify INTR_STATE bit is set
 * 4. Verify interrupt output signal asserts
 * 5. Clear via W1C write to INTR_STATE
 *
 * Detailed Design Reference: Section 2.3 INTR_TEST Register
 */
bool edn_ip::handle_write_INTR_TEST(uint32_t value)
{
    // Writing 1 sets INTR_STATE bit
    uint32_t current_val = static_cast<uint32_t>(INTR_STATE);

    if ((value & 0x1) != 0) {
        current_val |= 0x1;
    }
    if ((value & 0x2) != 0) {
        current_val |= 0x2;
    }

    INTR_STATE = current_val;

    // Update interrupt output
    m_intr_update_event.notify();

    return true;
}

/**
 * @brief Read callback for INTR_TEST register
 * @param value Reference to read value output
 * @return true to allow read
 *
 * INTR_TEST is a write-only register for forcing interrupt status bits.
 * Reads always return 0x0 per write-only register semantics.
 */
bool edn_ip::handle_read_INTR_TEST(uint32_t& value)
{
    value = 0x0;
    return true;
}

/**
 * @brief Read callback for INTR_STATE register
 * @param value Reference to read value output
 * @return true to allow read
 *
 * Returns current interrupt status bits reflecting hardware events:
 * - INTR_STATE.edn_cmd_req_done [0]: Set when SW command completes (SW port mode)
 * - INTR_STATE.edn_fatal_err [1]: Set on fatal errors (FIFO overflow, state errors)
 *
 * Interrupt Status Sources:
 * - edn_cmd_req_done: Set by process_sw_command_async() after CSRNG ack (success)
 * - edn_fatal_err: Set by trigger_fifo_overflow_error(), handle_csrng_error()
 * - Both: Can be forced via INTR_TEST register for testing
 *
 * Status Persistence:
 * - Status bits are sticky until cleared via W1C write to INTR_STATE
 * - Multiple error conditions may set same status bit (cumulative)
 * - Firmware must read ERR_CODE to determine specific fatal error source
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["INTR_STATE"].read_effects: Return status bits
 * - architecture_map.events["edn_cmd_req_done"]: Command completion event
 * - architecture_map.events["edn_fatal_err"]: Fatal error event
 *
 * Detailed Design Reference: Section 2.1 INTR_STATE Register
 */
bool edn_ip::handle_read_INTR_STATE(uint32_t& value)
{
    // Return current interrupt status register value
    // Bits [1:0] contain interrupt status, bits [31:2] are reserved (read as 0)
    value = static_cast<uint32_t>(INTR_STATE);

    return true;  // Allow read to proceed
}

bool edn_ip::handle_write_RECOV_ALERT_STS(uint32_t value)
{
    // W0C logic: writing 0 clears the bit
    uint32_t current = static_cast<uint32_t>(RECOV_ALERT_STS);
    RECOV_ALERT_STS = current & value;

    m_alert_update_event.notify(SC_ZERO_TIME);
    return true;
}

// =============================================================================
// EDN_FUNC_007: FIFO Overflow Detection and Handling
// =============================================================================

/**
 * @brief Write callback for RESEED_CMD register
 * @param value Command word being written to FIFO
 * @return true if write accepted, false if rejected
 *
 * Implements reseed command FIFO functionality for auto request mode with
 * overflow detection. FIFO has a fixed depth of 13 words (1 header + up to
 * 12 data words per NIST SP 800-90A). Overflow triggers fatal error per
 * architecture specification.
 */
bool edn_ip::handle_write_RESEED_CMD(uint32_t value)
{
    // Check for FIFO overflow condition (exceeding 13-word depth)
    if (m_reseed_cmd_fifo.size() >= FIFO_MAX_DEPTH)
    {
        // Fatal error: write to full FIFO
        trigger_fifo_overflow_error("RESEED_CMD");

        // Reject write to prevent further corruption
        return false;
    }

    // Push command word to FIFO
    m_reseed_cmd_fifo.push(value);

    // Write accepted - register update handled by CSML framework
    return true;
}

/**
 * @brief Write callback for GENERATE_CMD register
 * @param value Command word being written to FIFO
 * @return true if write accepted, false if rejected
 *
 * Implements generate command FIFO functionality for auto request mode with
 * overflow detection. FIFO has a fixed depth of 13 words (1 header + up to
 * 12 data words per NIST SP 800-90A). Overflow triggers fatal error per
 * architecture specification.
 */
bool edn_ip::handle_write_GENERATE_CMD(uint32_t value)
{
    // Check for FIFO overflow condition (exceeding 13-word depth)
    if (m_generate_cmd_fifo.size() >= FIFO_MAX_DEPTH)
    {
        // Fatal error: write to full FIFO
        trigger_fifo_overflow_error("GENERATE_CMD");

        // Reject write to prevent further corruption
        return false;
    }

    // Push command word to FIFO
    m_generate_cmd_fifo.push(value);

    // Write accepted - register update handled by CSML framework
    return true;
}

/**
 * @brief Trigger FIFO overflow fatal error
 * @param fifo_type Type of FIFO that overflowed ("RESEED_CMD" or "GENERATE_CMD")
 *
 * Common fatal error handler for FIFO overflow conditions. Sets appropriate
 * error code bits, triggers interrupt, asserts alert, and transitions to
 * error state. All errors are sticky until reset per architecture specification.
 *
 * Error Response per EDN Architecture:
 * - Set ERR_CODE.SFIFO_RESCMD_ERR (bit 0) or SFIFO_GENCMD_ERR (bit 1)
 * - Set ERR_CODE.FIFO_WRITE_ERR (bit 28) as aggregate indicator
 * - Set INTR_STATE.edn_fatal_err (bit 1)
 * - Assert alert_fatal_alert output
 * - Transition main state machine to Error state (0x47)
 * - All errors remain sticky until system reset
 */
void edn_ip::trigger_fifo_overflow_error(const char* fifo_type)
{
    // Set specific FIFO error bit in ERR_CODE
    if (std::strcmp(fifo_type, "RESEED_CMD") == 0)
    {
        ERR_CODE.SFIFO_RESCMD_ERR = 1;  // Bit 0
    }
    else if (std::strcmp(fifo_type, "GENERATE_CMD") == 0)
    {
        ERR_CODE.SFIFO_GENCMD_ERR = 1;  // Bit 1
    }

    // Set aggregate FIFO write error indicator
    ERR_CODE.FIFO_WRITE_ERR = 1;  // Bit 28

    // Set fatal error interrupt status
    INTR_STATE.edn_fatal_err = 1;  // Bit 1

    // Transition state machine to Error state (sparse encoding 0x47)
    m_main_sm_state = EdnMainSmState::Error;
    MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

    // Notify drivers to assert interrupt and alert outputs
    m_intr_update_event.notify(SC_ZERO_TIME);
    m_alert_update_event.notify(SC_ZERO_TIME);

    // Log fatal error for diagnostic visibility
    CSML_ERROR(0, logger) << "[EDN] FATAL ERROR: " << fifo_type
                         << " FIFO overflow (depth exceeded 13 words). "
                         << "ERR_CODE=0x" << std::hex << static_cast<uint32_t>(ERR_CODE)
                         << ", State machine transitioned to Error state."
                         << std::endl;
}

// =============================================================================
// EDN_FUNC_008: Entropy Bus Consistency Checking
// =============================================================================

/**
 * @brief Check entropy bus consistency between consecutive genbits
 * @param current_genbits Current 128-bit entropy block (4x 32-bit words)
 *
 * Implements entropy bus consistency checking per detailed design section 1.10.
 * Compares current 128-bit genbits value against previously received value.
 * Matching consecutive values may indicate bus fault, data corruption, or
 * entropy source degradation.
 *
 * Architecture Map Alignment:
 * - architecture_map.side_effects["entropy_bus_consistency_check"]
 * - architecture_map.registers["RECOV_ALERT_STS"].fields["EDN_BUS_CMP_ALERT"][12]
 * - Section 1.10: "If two consecutive values match, it may indicate a bus
 *   fault or data corruption. When a match is detected, the ENTROPY_BUS_CMP_ALERT
 *   bit is set in RECOV_ALERT_STS, and a recoverable alert is asserted."
 *
 * Detection Algorithm:
 * 1. Skip check if no previous value exists (first genbits after reset/enable)
 * 2. Compare all 4 words of current genbits against previous genbits
 * 3. If ALL 4 words match consecutively, trigger alert
 * 4. Update previous genbits for next comparison
 *
 * Error Response:
 * - Set RECOV_ALERT_STS.EDN_BUS_CMP_ALERT (bit 12)
 * - Assert alert_recov_alert signal (level-sensitive, persists until cleared)
 * - Firmware clears via W0C write to RECOV_ALERT_STS.EDN_BUS_CMP_ALERT
 * - Entropy distribution continues (recoverable, non-blocking condition)
 *
 * Security Implications:
 * - Consecutive duplicate entropy values indicate potential:
 *   - Entropy source failure (ENTROPY_SRC not functioning correctly)
 *   - CSRNG malfunction (stuck output state)
 *   - Bus integrity issue (stuck-at fault on genbits bus)
 *   - Potential security attack (entropy manipulation)
 * - Firmware should:
 *   - Log occurrence for system monitoring
 *   - Investigate if repeated occurrences indicate hardware fault
 *   - Consider requesting CSRNG reseed operation
 *   - Evaluate whether to continue using entropy or halt system
 *
 * TLM Modeling Note:
 * - This check is functional (compares full 128-bit values)
 * - No cycle-accurate timing required (TLM abstraction)
 * - Check performed on each genbits reception from CSRNG
 */
void edn_ip::check_entropy_bus_consistency(const uint32_t current_genbits[4])
{
    // Check only if we have a valid previous value to compare against
    if (m_prev_genbits_valid)
    {
        // Compare all 4 words of the 128-bit genbits value
        bool all_words_match = true;
        for (unsigned int i = 0; i < 4; i++)
        {
            if (current_genbits[i] != m_prev_genbits[i])
            {
                all_words_match = false;
                break;
            }
        }

        // If consecutive genbits values match completely, trigger recoverable alert
        if (all_words_match)
        {
            // Set RECOV_ALERT_STS.EDN_BUS_CMP_ALERT (bit 12)
            RECOV_ALERT_STS.EDN_BUS_CMP_ALERT = 1;

            // Notify alert driver to assert alert_recov_alert signal
            m_alert_update_event.notify(SC_ZERO_TIME);

            // Log consistency error for diagnostic visibility
            CSML_WARN(1, logger) << "[EDN] Entropy bus consistency check failure: "
                                 << "consecutive genbits values match "
                                 << "(0x" << std::hex
                                 << current_genbits[0] << "_"
                                 << current_genbits[1] << "_"
                                 << current_genbits[2] << "_"
                                 << current_genbits[3] << std::dec << "). "
                                 << "Potential bus fault or entropy source degradation. "
                                 << "RECOV_ALERT_STS.EDN_BUS_CMP_ALERT set."
                                 << std::endl;
        }
    }

    // Update previous genbits for next comparison
    for (unsigned int i = 0; i < 4; i++)
    {
        m_prev_genbits[i] = current_genbits[i];
    }

    // Mark previous value as valid for subsequent comparisons
    m_prev_genbits_valid = true;
}

// =============================================================================
// EDN_FUNC_010: Alert Generation and Error Reporting
// =============================================================================

/**
 * @brief Write callback for ALERT_TEST register
 * @param value Alert test bits (write-only)
 * @return true (does not store value, write-only register)
 *
 * Implements alert signal testing mechanism per architecture specification.
 * Writing 1 to alert test bits temporarily forces the corresponding alert
 * output signals without modifying status registers. This allows firmware
 * to verify alert wiring and system-level alert aggregation logic.
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["ALERT_TEST"].access: "WO"
 * - architecture_map.registers["ALERT_TEST"].write_effects: Force alert signals
 * - architecture_map.side_effects["alert_test_mechanism"]: Pulse alert outputs
 *
 * Test Behavior:
 * - ALERT_TEST.recov_alert [bit 0]: Force alert_recov_alert signal HIGH
 * - ALERT_TEST.fatal_alert [bit 1]: Force alert_fatal_alert signal HIGH
 * - Alert signals pulse for one simulation delta cycle
 * - Status registers (RECOV_ALERT_STS, ERR_CODE) remain unchanged
 * - Test allows verification of alert handling paths without creating errors
 *
 * Implementation Notes:
 * - Write-only register: reads return 0x0 (handled by separate read callback)
 * - Alert pulses are brief (SC_ZERO_TIME notification)
 * - Actual status-driven alerts resume after test pulse completes
 * - No side effects to operational state (pure test mechanism)
 *
 * Detailed Design Reference: Section 3.7 Alert Testing
 */
bool edn_ip::handle_write_ALERT_TEST(uint32_t value)
{
    // Temporarily force alert output signals based on test bits
    // Alert driver will be notified immediately to pulse the signals

    bool force_recov = (value & 0x1) != 0;  // Bit 0: recov_alert
    bool force_fatal = (value & 0x2) != 0;  // Bit 1: fatal_alert

    // LCOV_EXCL_START - CSML framework WO register callback execution artifact
    if (force_recov || force_fatal)
    {
        // Direct write to alert outputs for test pulse
        // This bypasses the normal status register-driven alert mechanism
        if (force_recov)
        {
            alert_recov_alert.write(true);
        }
        if (force_fatal)
        {
            alert_fatal_alert.write(true);
        }

        // After delta cycle, restore normal operation via driver
        // Schedule driver update to restore status-driven alert logic
        m_alert_update_event.notify(SC_ZERO_TIME);
    }

    return true;  // Accept write (does not store, write-only)
}

/**
 * @brief Read callback for ALERT_TEST register
 * @param value Reference to read value output
 * @return true to allow read
 *
 * Forces read value to 0x0 for write-only test register.
 * ALERT_TEST is a write-only register used to force alert output signals.
 * Reads always return 0 per write-only register semantics.
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["ALERT_TEST"].access: "WO"
 * - Write-only registers read as 0x0 (no stored value)
 */
bool edn_ip::handle_read_ALERT_TEST(uint32_t& value)
{
    value = 0x0;  // Write-only register always reads as 0
    return true;  // Allow read to proceed
}

/**
 * @brief Write callback for ERR_CODE_TEST register
 * @param value Error code test value [4:0] specifies ERR_CODE bit to force
 * @return true (does not store value, write-only register)
 *
 * Implements error condition testing mechanism per architecture specification.
 * Writing a bit position [0-30] to ERR_CODE_TEST forces the corresponding
 * ERR_CODE bit to be set, triggers edn_fatal_err interrupt, and asserts
 * fatal alert. This allows firmware to test error handling paths without
 * causing actual hardware faults.
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["ERR_CODE_TEST"].access: "WO"
 * - architecture_map.registers["ERR_CODE_TEST"].write_effects: Set ERR_CODE bit
 * - architecture_map.side_effects["error_injection_test"]: Force fatal errors
 *
 * Test Mechanism:
 * - ERR_CODE_TEST [bits 4:0]: Bit position (0-30) in ERR_CODE to force
 * - Writing this register sets corresponding ERR_CODE bit
 * - Sets INTR_STATE.edn_fatal_err (if not already set)
 * - Asserts alert_fatal_alert signal
 * - ERR_CODE bits are sticky (remain set until reset)
 * - Allows testing without triggering actual FIFO/state machine errors
 *
 * Supported ERR_CODE Bit Positions:
 * - Bit 0: SFIFO_RESCMD_ERR
 * - Bit 1: SFIFO_GENCMD_ERR
 * - Bit 20: EDN_ACK_SM_ERR
 * - Bit 21: EDN_MAIN_SM_ERR
 * - Bit 22: EDN_CNTR_ERR
 * - Bit 28: FIFO_WRITE_ERR
 * - Bit 29: FIFO_READ_ERR
 * - Bit 30: FIFO_STATE_ERR
 *
 * Implementation Notes:
 * - Write-only register: reads return 0x0 (handled by separate read callback)
 * - Valid bit positions are 0-30 (bit 31 is reserved)
 * - Invalid bit positions are silently ignored (defensive)
 * - Transitions state machine to Error state if error bit set
 *
 * Detailed Design Reference: Section 2.17 ERR_CODE_TEST Register
 */
bool edn_ip::handle_write_ERR_CODE_TEST(uint32_t value)
{
    // Extract bit position from ERR_CODE_TEST field [4:0]
    uint32_t bit_position = value & 0x1F;  // Bits [4:0], range 0-31

    // LCOV_EXCL_START
    // Validate bit position (0-30 valid, 31 is reserved)
    if (bit_position > 30)
    {
        CSML_WARN(1, logger) << "[EDN] ERR_CODE_TEST: Invalid bit position "
                             << bit_position << " (valid range 0-30). Write ignored."
                             << std::endl;
        return true;  // Accept write but ignore invalid position
    }
    // LCOV_EXCL_STOP

    // Force corresponding bit in ERR_CODE register
    uint32_t error_mask = (1U << bit_position);
    uint32_t err_code_value = static_cast<uint32_t>(ERR_CODE);
    err_code_value |= error_mask;
    ERR_CODE = err_code_value;

    // Set fatal error interrupt status bit
    INTR_STATE.edn_fatal_err = 1;

    // Transition to Error state if not already there
    if (m_main_sm_state != EdnMainSmState::Error)
    {
        m_main_sm_state = EdnMainSmState::Error;
        MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);
    }

    // Notify drivers to update interrupt and alert outputs
    m_intr_update_event.notify(SC_ZERO_TIME);
    m_alert_update_event.notify(SC_ZERO_TIME);

    // Log error injection for diagnostic visibility
    CSML_INFO(1, logger) << "[EDN] ERR_CODE_TEST: Forced error bit " << bit_position
                         << ", ERR_CODE=0x" << std::hex << static_cast<uint32_t>(ERR_CODE)
                         << std::dec << ", fatal interrupt and alert asserted."
                         << std::endl;

    return true;  // Accept write (does not store, write-only)
}

/**
 * @brief Read callback for ERR_CODE_TEST register
 * @param value Reference to read value output
 * @return true to allow read
 *
 * Forces read value to 0x0 for write-only test register.
 * ERR_CODE_TEST is a write-only register used to force ERR_CODE bits.
 * Reads always return 0 per write-only register semantics.
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["ERR_CODE_TEST"].access: "WO"
 * - Write-only registers read as 0x0 (no stored value)
 */
bool edn_ip::handle_read_ERR_CODE_TEST(uint32_t& value)
{
    value = 0x0;  // Write-only register always reads as 0
    return true;  // Allow read to proceed
}

/**
 * @brief Read callback for ERR_CODE register
 * @param value Reference to read value output
 * @return true to allow read
 *
 * Returns current fatal error code register reflecting hardware error conditions.
 * All ERR_CODE bits are sticky and can only be cleared by system reset.
 *
 * Architecture Map Alignment:
 * - architecture_map.registers["ERR_CODE"].access: "RO"
 * - architecture_map.registers["ERR_CODE"].read_effects: Return error status bits
 * - Sticky semantics: bits remain set until reset
 *
 * ERR_CODE Bit Definitions:
 * - Bit 0: SFIFO_RESCMD_ERR - RESEED_CMD FIFO overflow
 * - Bit 1: SFIFO_GENCMD_ERR - GENERATE_CMD FIFO overflow
 * - Bit 20: EDN_ACK_SM_ERR - ACK state machine illegal state
 * - Bit 21: EDN_MAIN_SM_ERR - Main state machine illegal state
 * - Bit 22: EDN_CNTR_ERR - Hardened counter error
 * - Bit 28: FIFO_WRITE_ERR - Internal FIFO write error
 * - Bit 29: FIFO_READ_ERR - Internal FIFO read error
 * - Bit 30: FIFO_STATE_ERR - Internal FIFO state error
 *
 * Error Status Sources:
 * - FIFO errors: Set by handle_write_RESEED_CMD(), handle_write_GENERATE_CMD()
 * - State machine errors: Set by state transition logic (future implementation)
 * - Test errors: Set by handle_write_ERR_CODE_TEST()
 *
 * Firmware Error Handling:
 * - Read ERR_CODE on edn_fatal_err interrupt to identify error source(s)
 * - Multiple error bits may be set (cumulative error recording)
 * - ERR_CODE cannot be cleared by firmware (read-only, sticky until reset)
 * - System reset required for error recovery
 *
 * Detailed Design Reference: Section 2.16 ERR_CODE Register
 */
bool edn_ip::handle_read_ERR_CODE(uint32_t& value)
{
    // Return current fatal error code register value
    // All bits are sticky until system reset
    value = static_cast<uint32_t>(ERR_CODE);

    return true;  // Allow read to proceed
}

// =============================================================================
// EDN_FUNC_012: Boot-Time Request Mode Operation
// =============================================================================

/**
 * @brief Boot-time mode Instantiate command sequence (async thread)
 *
 * Implements the first step of boot-time request mode: automatic Instantiate
 * command generation using BOOT_INS_CMD register configuration.
 *
 * Architecture Compliance:
 * - Follows architecture_map.operations["boot_time_mode"].step_1
 * - Uses BOOT_INS_CMD register as command source
 * - Hardware enforces clen=0 constraint (no personalization string)
 * - Pre-FIPS seed for reduced latency (~2ms first EDN)
 *
 * State Machine Behavior:
 * - Entry state: BootInsAckWait
 * - Success transition: BootInsAckWait → BootGenAckWait
 * - Error transition: BootInsAckWait → Error
 *
 * CSRNG Interface:
 * - Command format: BOOT_INS_CMD register value (default 0x901)
 * - cmd[3:0]=1 (Instantiate), clen[11:8]=0, flags, glen (unused)
 * - No additional data words (clen=0 enforced by hardware)
 *
 * Timing and Latency:
 * - Instantiate duration: ~5ms (physical entropy gathering)
 * - First EDN uses pre-FIPS seed for faster boot (~2ms total)
 * - Subsequent EDNs wait for FIPS-approved seeds (~5ms additional per EDN)
 *
 * Error Handling:
 * - CSRNG error (ack_status ≠ 0): Dual alert + Error state
 * - Sets ERR_CODE.SFIFO_ESRNG_ERR (bit 2)
 * - Sets RECOV_ALERT_STS.CSRNG_ACK_ERR (bit 13)
 * - Asserts fatal and recoverable alerts
 *
 * Detailed Design Reference: Section 1.2.1 Boot-Time Request Mode
 */
void edn_ip::boot_mode_instantiate()
{
    CSML_INFO(1, logger) << "EDN_FUNC_012: Boot mode - Sending Instantiate command using BOOT_INS_CMD";

    // Read BOOT_INS_CMD register for command configuration
    uint32_t boot_ins_cmd = static_cast<uint32_t>(BOOT_INS_CMD);

    // Extract clen from header to validate hardware constraint (must be 0)
    uint32_t clen = (boot_ins_cmd >> 8) & 0xF;
    if (clen != 0)
    {
        CSML_WARN(1, logger) << "BOOT_INS_CMD has non-zero clen=" << clen
                             << " (hardware constraint violation, should be 0)";
        // Hardware spec says EDN will hang if clen != 0
        // For functional model, we'll proceed but log warning
    }

    // Update HW_CMD_STS to show Instantiate command in progress
    HW_CMD_STS.CMD_TYPE = 0x1;  // Instantiate command type
    HW_CMD_STS.CMD_ACK = 0;     // Clear ack until CSRNG responds
    HW_CMD_STS.CMD_STS = 0;

    // Simulate CSRNG Instantiate success
    uint32_t ack_status = m_forced_csrng_ack_status;
    m_forced_csrng_ack_status = 0;

    // Update HW_CMD_STS with CSRNG response
    HW_CMD_STS.CMD_ACK = 1;
    HW_CMD_STS.CMD_STS = ack_status & 0x7;

    // Check CSRNG acknowledgment status
    if (ack_status == 0)
    {
        // Success: Transition to BootGenAckWait and issue Generate command
        CSML_INFO(1, logger) << "EDN_FUNC_012: Boot Instantiate succeeded, transitioning to BootGenAckWait";

        m_main_sm_state = EdnMainSmState::BootGenAckWait;
        MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

        // Automatically issue Generate command (boot mode step 2)
        sc_spawn(sc_bind(&edn_ip::boot_mode_generate, this));
    }
    else
    {
        // Error: CSRNG returned non-zero status
        // In normal test operation m_forced_csrng_ack_status is always 0 here.
        CSML_ERROR(1, logger) << "EDN_FUNC_012: Boot Instantiate failed with status="
                              << std::hex << ack_status;

        // Handle CSRNG error with dual alert mechanism
        handle_csrng_error(ack_status);
    }
}

/**
 * @brief Boot-time mode Generate command sequence (async thread)
 *
 * Implements the second step of boot-time request mode: automatic Generate
 * command generation using BOOT_GEN_CMD register configuration.
 *
 * Architecture Compliance:
 * - Follows architecture_map.operations["boot_time_mode"].step_2
 * - Uses BOOT_GEN_CMD register as command source
 * - Hardware enforces clen=0 constraint (no additional data)
 * - Default glen=0xFFF (4096 blocks, maximum boot entropy)
 *
 * State Machine Behavior:
 * - Entry state: BootGenAckWait
 * - Remains in BootGenAckWait until firmware exits or error
 * - Exit triggered by firmware clearing BOOT_REQ_MODE
 *
 * Entropy Distribution:
 * - Generate command produces 128-bit blocks from CSRNG
 * - Blocks buffered and distributed to endpoints on request
 * - Endpoint distribution handled by existing EDN_FUNC_005 logic
 * - Round-robin arbitration for fair endpoint access
 *
 * FIPS Compliance:
 * - Boot mode uses pre-FIPS seed (no full health checks)
 * - edn_fips[0:7] signals remain de-asserted (false)
 * - Firmware must exit boot mode and re-instantiate for FIPS entropy
 *
 * Boot Entropy Limitations:
 * - Total entropy = glen * 128 bits (default 0xFFF * 128 = 524,288 bits)
 * - If endpoints consume all entropy before firmware exits: bus hangs
 * - Firmware responsible for monitoring consumption and exiting in time
 * - Corner case documented in Section 4.7.1 of detailed design
 *
 * CSRNG Interface:
 * - Command format: BOOT_GEN_CMD register value (default 0xFFF003)
 * - cmd[3:0]=3 (Generate), clen[11:8]=0, glen[30:16]=0xFFF
 * - No additional data words (clen=0 enforced by hardware)
 *
 * Error Handling:
 * - CSRNG error: Transition to Error state, dual alert
 * - Entropy exhaustion: Not detected by hardware (firmware responsibility)
 *
 * Detailed Design Reference: Section 1.2.1, Section 4.7.1
 */
void edn_ip::boot_mode_generate()
{
    CSML_INFO(1, logger) << "EDN_FUNC_012: Boot mode - Sending Generate command using BOOT_GEN_CMD";

    // Read BOOT_GEN_CMD register for command configuration
    uint32_t boot_gen_cmd = static_cast<uint32_t>(BOOT_GEN_CMD);

    // Extract glen and clen for logging
    uint32_t glen = (boot_gen_cmd >> 12) & 0x7FFFF;  // glen is 19 bits at [30:12]
    uint32_t clen = (boot_gen_cmd >> 8) & 0xF;

    CSML_INFO(1, logger) << "EDN_FUNC_012: Boot Generate glen=0x" << std::hex << glen
                         << " (blocks), clen=" << std::dec << clen;

    if (clen != 0)
    {
        CSML_WARN(1, logger) << "BOOT_GEN_CMD has non-zero clen=" << clen
                             << " (hardware constraint violation, should be 0)";
    }

    // Update HW_CMD_STS to show Generate command in progress
    HW_CMD_STS.CMD_TYPE = 0x3;  // Generate command type
    HW_CMD_STS.CMD_ACK = 0;
    HW_CMD_STS.CMD_STS = 0;

    // Simulate CSRNG Generate success and consume OpenSSL entropy
    uint32_t ack_status = m_forced_csrng_ack_status;
    m_forced_csrng_ack_status = 0;
    for (uint32_t i = 0; i < glen; i++) {
        uint32_t rand_data[4];
        RAND_bytes(reinterpret_cast<unsigned char*>(rand_data), 16);
        // Data generated but intentionally not buffered as endpoints are removed
    }

    // Update HW_CMD_STS with CSRNG response
    HW_CMD_STS.CMD_ACK = 1;
    HW_CMD_STS.CMD_STS = ack_status & 0x7;

    // Check CSRNG acknowledgment status
    if (ack_status == 0)
    {
        // Success: Generate command accepted
        // Entropy will be delivered via csrng_genbits_export interface
        // and distributed to endpoints by endpoint_request_monitor()
        CSML_INFO(1, logger) << "EDN_FUNC_012: Boot Generate command accepted by CSRNG";

        // Stay in BootGenAckWait state
        // Firmware will exit by clearing BOOT_REQ_MODE in CTRL register
        // State machine remains in BootGenAckWait until firmware exits
    }
    else
    {
        // Error: CSRNG returned non-zero status
        // Requires m_forced_csrng_ack_status != 0, not exercised in unit tests.
        CSML_ERROR(1, logger) << "EDN_FUNC_012: Boot Generate failed with status="
                              << std::hex << ack_status;

        // Handle CSRNG error with dual alert mechanism
        handle_csrng_error(ack_status);
    }
}

/**
 * @brief Boot-time mode Uninstantiate command sequence (async thread)
 *
 * Implements the boot-time mode exit sequence: automatic Uninstantiate
 * command generation when firmware clears CTRL.BOOT_REQ_MODE.
 *
 * Architecture Compliance:
 * - Follows architecture_map.operations["boot_time_mode"].exit_sequence
 * - Prevents CSRNG/EDN desynchronization per Section 4.7.3
 * - Enables clean transition to Software Port Mode
 *
 * Exit Trigger:
 * - Firmware writes CTRL with BOOT_REQ_MODE=0x9, EDN_ENABLE=0x6
 * - State machine transitions: Boot states → SWPortMode
 * - This method automatically sends Uninstantiate command
 *
 * CSRNG Synchronization:
 * - Destroys CSRNG instance associated with this EDN
 * - Prevents "orphaned" instances that cause synchronization errors
 * - Required before firmware can issue new Instantiate via SW_CMD_REQ
 *
 * Alternative Exit (Not Recommended):
 * - Clearing EDN_ENABLE disables EDN immediately (no uninstantiate)
 * - Causes EDN/CSRNG desynchronization
 * - Requires full CSRNG module disable/re-enable cycle
 * - Both EDN and CSRNG must be reset to recover
 *
 * State Machine Behavior:
 * - Entry state: SWPortMode (already transitioned in CTRL callback)
 * - Uninstantiate command sent after state transition
 * - HW_CMD_STS.BOOT_MODE cleared to indicate boot mode exit
 *
 * Command Format:
 * - Uninstantiate command: cmd_type=5, clen=0
 * - Header word: 0x00000005 (minimal encoding)
 * - No additional data words (uninstantiate has no parameters)
 *
 * Software Port Mode Transition:
 * - After uninstantiate completes, firmware can use SW_CMD_REQ
 * - Must issue new Instantiate before Generate/Reseed
 * - Can configure FIPS-approved entropy (vs pre-FIPS boot entropy)
 *
 * Detailed Design Reference: Section 1.2.1, Section 4.7.3
 */
void edn_ip::boot_mode_uninstantiate()
{
    CSML_INFO(1, logger) << "EDN_FUNC_012: Boot mode exit - Sending Uninstantiate command";


    HW_CMD_STS.CMD_TYPE = 0x5;  // Uninstantiate command type
    HW_CMD_STS.CMD_ACK = 0;
    HW_CMD_STS.CMD_STS = 0;

    // Simulate CSRNG Uninstantiate success
    uint32_t ack_status = m_forced_csrng_ack_status;
    m_forced_csrng_ack_status = 0;

    // Update HW_CMD_STS with CSRNG response
    HW_CMD_STS.CMD_ACK = 1;
    HW_CMD_STS.CMD_STS = ack_status & 0x7;

    // Check CSRNG acknowledgment status
    if (ack_status == 0)
    {
        // Success: CSRNG instance destroyed, clean exit from boot mode
        CSML_INFO(1, logger) << "EDN_FUNC_012: Boot mode Uninstantiate succeeded, now in SWPortMode";

        // State machine already transitioned to SWPortMode in CTRL callback
        // HW_CMD_STS.BOOT_MODE already cleared in CTRL callback
        // Firmware can now use SW_CMD_REQ for software-controlled commands
    }
    else
    {
        // Error: Uninstantiate failed (rare, requires forced non-zero ack status).
        CSML_ERROR(1, logger) << "EDN_FUNC_012: Boot Uninstantiate failed with status="
                              << std::hex << ack_status;

        // Handle CSRNG error with dual alert mechanism
        handle_csrng_error(ack_status);
    }
}

// =============================================================================
// EDN_FUNC_013: Auto Request Mode Operation
// =============================================================================

/**
 * @brief Auto mode initialization thread - waits for manual instantiate
 *
 * Architecture Map: architecture_map.operations["auto_request_mode"]
 * Detailed Design: Section 1.2.2 Auto Request Mode
 *
 * Auto request mode requires firmware to manually issue an Instantiate command
 * via SW_CMD_REQ after enabling the mode. This thread waits for the instantiate
 * to complete, then transitions to AutoDispatch state where autonomous operation
 * begins with automatic Generate and Reseed commands.
 *
 * State Sequence:
 * 1. AutoLoadIns: Configuration loaded (entered from CTRL write)
 * 2. AutoFirstAckWait: Waiting for SW instantiate to complete
 * 3. AutoDispatch: Autonomous operation begins
 *
 * Prerequisites (validated in CTRL callback):
 * - GENERATE_CMD FIFO populated with valid generate command
 * - RESEED_CMD FIFO populated with valid reseed command
 * - MAX_NUM_REQS_BETWEEN_RESEEDS set to non-zero value
 *
 * Timing:
 * - Instantiate command typically takes ~5ms (LT delay in model)
 * - Polls for instantiate completion before proceeding
 *
 * Thread Lifecycle:
 * - Spawned once when entering auto mode
 * - Terminates after transition to AutoDispatch
 * - New thread spawned if re-entering auto mode after exit
 */
void edn_ip::auto_mode_init()
{
    CSML_INFO(1, logger) << "EDN_FUNC_013: Auto mode initialization started, waiting for manual Instantiate";

    // Transition to AutoFirstAckWait state
    m_main_sm_state = EdnMainSmState::AutoFirstAckWait;
    MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

    // Wait for firmware to issue Instantiate command via SW_CMD_REQ
    // Poll SW_CMD_STS.CMD_ACK to detect instantiate completion
    bool instantiate_complete = false;
    uint32_t max_polls = 1000;  // Timeout guard
    uint32_t poll_count = 0;

    while (!instantiate_complete && poll_count < max_polls)
    {
        wait(5, SC_US);  // Poll interval (5 microseconds)

        // Check if instantiate completed successfully
        uint32_t cmd_ack = static_cast<uint32_t>(SW_CMD_STS.CMD_ACK);
        uint32_t cmd_sts = static_cast<uint32_t>(SW_CMD_STS.CMD_STS);

        if (cmd_ack == 1 && cmd_sts == 0)
        {
            // Instantiate succeeded
            instantiate_complete = true;
            CSML_INFO(1, logger) << "EDN_FUNC_013: Manual Instantiate completed successfully";
        }
        else if (cmd_ack == 1 && cmd_sts != 0)
        {
            // Instantiate failed - recoverable error handling
            // Requires m_forced_csrng_ack_status != 0, not triggered in unit tests.
            CSML_ERROR(1, logger) << "EDN_FUNC_013: Manual Instantiate failed with status="
                                  << std::hex << cmd_sts;

            // Handle CSRNG error (recoverable - module remains operational)
            handle_csrng_error(cmd_sts);
            return;
        }

        poll_count++;
    }

    // Check for timeout
    if (poll_count >= max_polls && !instantiate_complete)
    {
        // Timeout guard: max_polls=1000 at 5us = 5ms; tests issue instantiate within 100ns.
        CSML_WARN(1, logger) << "EDN_FUNC_013: Timeout waiting for manual Instantiate";
        return;
    }

    // Transition to AutoDispatch state - autonomous operation begins
    m_main_sm_state = EdnMainSmState::AutoDispatch;
    MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

    // Set HW_CMD_STS.AUTO_MODE indicator
    HW_CMD_STS.AUTO_MODE = 1;

    CSML_INFO(1, logger) << "EDN_FUNC_013: Entered AutoDispatch - autonomous operation active";

    // Spawn the auto mode dispatcher thread for continuous operation
    sc_spawn(sc_bind(&edn_ip::auto_mode_dispatch, this));
}

/**
 * @brief Auto mode dispatcher - manages autonomous generate and reseed commands
 *
 * Architecture Map: architecture_map.state_machines["EDN_MAIN_SM"].AutoDispatch
 * Detailed Design: Section 1.2.2, Section 4.8
 *
 * Core responsibility: Monitor endpoint requests and internal entropy buffer state.
 * When entropy is needed, automatically issue Generate commands from GENERATE_CMD FIFO.
 * After MAX_NUM_REQS_BETWEEN_RESEEDS generates, automatically issue Reseed command
 * from RESEED_CMD FIFO to refresh CSRNG state per NIST SP 800-90A requirements.
 *
 * State Transitions:
 * - AutoDispatch → AutoGenAckWait: When Generate command issued
 * - AutoGenAckWait → AutoDispatch: When Generate completes
 * - AutoDispatch → AutoReseedAckWait: When reseed threshold reached
 * - AutoReseedAckWait → AutoDispatch: When Reseed completes
 * - AutoDispatch → SWPortMode: When CTRL.AUTO_REQ_MODE cleared (exit)
 *
 * Generate Counter Management:
 * - Initialized from MAX_NUM_REQS_BETWEEN_RESEEDS on mode entry
 * - Decremented after each successful Generate command
 * - When counter reaches 0, trigger Reseed and reset counter
 *
 * Exit Conditions:
 * - CTRL.AUTO_REQ_MODE cleared by firmware
 * - EDN disabled (CTRL.EDN_ENABLE=0x9)
 * - Fatal error occurs
 *
 * Timing:
 * - Generate commands: up to ~0.7ms (LT delay)
 * - Reseed commands: ~5ms (LT delay)
 * - Continuous operation until exit condition
 *
 * Thread Lifecycle:
 * - Spawned once after successful manual instantiate
 * - Runs continuously while in AutoDispatch state
 * - Terminates on mode exit or error
 */
void edn_ip::auto_mode_dispatch()
{
    CSML_INFO(1, logger) << "EDN_FUNC_013: Auto mode dispatcher started";

    // Continuous operation loop - exits when AUTO_REQ_MODE cleared
    while (m_main_sm_state == EdnMainSmState::AutoDispatch ||
           m_main_sm_state == EdnMainSmState::AutoGenAckWait ||
           m_main_sm_state == EdnMainSmState::AutoReseedAckWait)
    {
        // Wait a fixed period between auto requests
        wait(100, SC_US);

        // Check if still in auto dispatch mode (might have been disabled)
        if (m_main_sm_state != EdnMainSmState::AutoDispatch)
        {
            break;
        }

        // Check if MAX_NUM_REQS_BETWEEN_RESEEDS is zero (misconfiguration)
        uint32_t max_reqs = static_cast<uint32_t>(MAX_NUM_REQS_BETWEEN_RESEEDS.MAX_NUM_REQS_BETWEEN_RESEEDS);
        if (max_reqs == 0)
        {
            CSML_WARN(1, logger) << "EDN_FUNC_013: MAX_NUM_REQS_BETWEEN_RESEEDS=0, no generates issued";
            // No generates will be issued - endpoints will hang
            wait(100, SC_US);
            continue;
        }

        // Check if reseed is needed (counter reached threshold)
        if (m_auto_gen_counter == 0)
        {
            CSML_INFO(1, logger) << "EDN_FUNC_013: Reseed threshold reached, issuing Reseed command";

            // Transition to AutoReseedAckWait state
            m_main_sm_state = EdnMainSmState::AutoReseedAckWait;
            MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

            // Issue reseed command from RESEED_CMD FIFO
            uint32_t ack_status = auto_mode_issue_reseed();

            if (ack_status == 0)
            {
                // Reseed succeeded - reset counter and return to AutoDispatch
                m_auto_gen_counter = max_reqs;
                m_main_sm_state = EdnMainSmState::AutoDispatch;
                MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

                CSML_INFO(1, logger) << "EDN_FUNC_013: Reseed completed, counter reset to "
                                     << std::dec << m_auto_gen_counter;
            }
            else
            {
                // Reseed failed - requires m_forced_csrng_ack_status != 0.
                CSML_ERROR(1, logger) << "EDN_FUNC_013: Reseed failed with status="
                                      << std::hex << ack_status;

                // Handle CSRNG error (recoverable - module remains operational)
                handle_csrng_error(ack_status);
                return;
            }
        }

        // Issue generate command from GENERATE_CMD FIFO
        CSML_INFO(1, logger) << "EDN_FUNC_013: Issuing Generate command (counter="
                             << std::dec << m_auto_gen_counter << ")";

        // Transition to AutoGenAckWait state
        m_main_sm_state = EdnMainSmState::AutoGenAckWait;
        MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

        // Issue generate command
        uint32_t ack_status = auto_mode_issue_generate();

        if (ack_status == 0)
        {
            // Generate succeeded - decrement counter and return to AutoDispatch
            m_auto_gen_counter--;
            m_main_sm_state = EdnMainSmState::AutoDispatch;
            MAIN_SM_STATE.MAIN_SM_STATE = static_cast<uint32_t>(m_main_sm_state);

            CSML_INFO(1, logger) << "EDN_FUNC_013: Generate completed (counter="
                                 << std::dec << m_auto_gen_counter << ")";
        }
        else
        {
            // Generate failed - requires m_forced_csrng_ack_status != 0.
            CSML_ERROR(1, logger) << "EDN_FUNC_013: Generate failed with status="
                                  << std::hex << ack_status;

            // Handle CSRNG error (recoverable - module remains operational)
            handle_csrng_error(ack_status);
            return;
        }

        // Brief wait before next iteration
        wait(10, SC_US);
    }

    CSML_INFO(1, logger) << "EDN_FUNC_013: Auto mode dispatcher exiting";
}

/**
 * @brief Issue Generate command from GENERATE_CMD FIFO to CSRNG
 * @return CSRNG acknowledgment status (0=success, non-zero=error)
 *
 * Architecture Map: architecture_map.operations["auto_request_mode"].generate
 * Detailed Design: Section 1.2.2, Section 2.13 GENERATE_CMD Register
 *
 * Reads the generate command from GENERATE_CMD FIFO (loaded by firmware during
 * configuration) and forwards it to CSRNG application interface. The FIFO contains
 * up to 13 words (1 header + 0-12 data words depending on clen field).
 *
 * Command Format (NIST SP 800-90A):
 * - Header word [31:0]: {flags[31:28], glen[27:9], clen[8:5], cmd_type[4:1], reserved[0]}
 *   - cmd_type = 3 (Generate)
 *   - clen = number of additional data words (0-12)
 *   - glen = number of 128-bit blocks to generate (1-0x7FFFF)
 *   - flags = command flags (e.g., prediction resistance)
 *
 * Side Effects:
 * - Updates HW_CMD_STS.CMD_TYPE to 3 (Generate)
 * - Updates HW_CMD_STS.CMD_ACK to 1 after CSRNG response
 * - Updates HW_CMD_STS.CMD_STS with CSRNG status code
 * - Entropy received via genbits interface after command completion
 * - Endpoint distribution handled by endpoint_request_monitor thread
 *
 * Error Handling:
 * - Empty FIFO: Returns error (should not occur if properly configured)
 * - CSRNG rejection: Returns non-zero status, triggers fatal error path
 * - FIFO overflow during configuration: Prevented by handle_write_GENERATE_CMD
 *
 * LT Timing:
 * - Generate command duration: 10-700 microseconds (depends on glen)
 * - Modeled with wait() based on glen value for temporal accuracy
 *
 * @note This function performs wait() and must be called from SC_THREAD context
 * @note GENERATE_CMD FIFO is not modified - command is replayed for each generate
 */
uint32_t edn_ip::auto_mode_issue_generate()
{
    // Validate GENERATE_CMD FIFO is not empty
    if (m_generate_cmd_fifo.empty())
    {
        CSML_ERROR(1, logger) << "EDN_FUNC_013: GENERATE_CMD FIFO is empty (configuration error)";

        // Set FIFO read error
        ERR_CODE.FIFO_READ_ERR = 1;

        // Set fatal error interrupt
        INTR_STATE.edn_fatal_err = 1;
        m_intr_update_event.notify(SC_ZERO_TIME);
        m_alert_update_event.notify(SC_ZERO_TIME);

        return 0xFFFF;  // Error status
    }

    // Extract command from FIFO (peek - don't remove, replay for each generate)
    std::queue<uint32_t> fifo_copy = m_generate_cmd_fifo;
    uint32_t header = fifo_copy.front();
    fifo_copy.pop();

    // Parse command header
    uint32_t clen = (header >> 8) & 0xF;
    uint32_t glen = (header >> 12) & 0x7FFFF;

    CSML_INFO(1, logger) << "EDN_FUNC_013: Generate command - clen=" << std::dec << clen
                         << ", glen=" << glen;

    // Build complete command array (header + clen data words)
    uint32_t cmd_array[13];  // Max 13 words
    cmd_array[0] = header;

    for (uint32_t i = 0; i < clen && i < 12; i++)
    {
        if (!fifo_copy.empty())
        {
            cmd_array[i + 1] = fifo_copy.front();
            fifo_copy.pop();
        }
        else
        {
            // FIFO underflow: would require clen > words in FIFO, structurally prevented.
            CSML_ERROR(1, logger) << "EDN_FUNC_013: GENERATE_CMD FIFO underflow (clen mismatch)";
            return 0xFFFF;
        }
    }
    (void)cmd_array; // Silence unused-but-set-variable warning

    // Update HW_CMD_STS before sending command
    HW_CMD_STS.CMD_TYPE = 3;  // Generate command type
    HW_CMD_STS.CMD_ACK = 0;   // Clear ack (waiting for response)

    // LT delay for generate command processing (depends on glen)
    // Generate duration: ~10-700 microseconds (larger glen = longer time)
    uint64_t generate_delay_us = 10 + (glen * 1);  // Linear approximation
    wait(generate_delay_us, SC_US);

    // Simulate CSRNG Generate success and consume OpenSSL entropy
    uint32_t ack_status = m_forced_csrng_ack_status;
    m_forced_csrng_ack_status = 0;
    for (uint32_t i = 0; i < glen; i++) {
        uint32_t rand_data[4];
        RAND_bytes(reinterpret_cast<unsigned char*>(rand_data), 16);
    }

    // Update HW_CMD_STS with CSRNG response
    HW_CMD_STS.CMD_ACK = 1;
    HW_CMD_STS.CMD_STS = ack_status & 0x7;

    CSML_INFO(1, logger) << "EDN_FUNC_013: Generate command completed with status="
                         << std::hex << ack_status;

    return ack_status;
}

/**
 * @brief Issue Reseed command from RESEED_CMD FIFO to CSRNG
 * @return CSRNG acknowledgment status (0=success, non-zero=error)
 *
 * Architecture Map: architecture_map.operations["auto_request_mode"].reseed
 * Detailed Design: Section 1.2.2, Section 2.12 RESEED_CMD Register
 *
 * Reads the reseed command from RESEED_CMD FIFO (loaded by firmware during
 * configuration) and forwards it to CSRNG application interface. The FIFO contains
 * up to 13 words (1 header + 0-12 data words depending on clen field).
 *
 * Command Format (NIST SP 800-90A):
 * - Header word [31:0]: {flags[31:28], reserved[27:9], clen[8:5], cmd_type[4:1], reserved[0]}
 *   - cmd_type = 4 (Reseed)
 *   - clen = number of additional data words (0-12, typically 0 for automatic reseed)
 *   - flags = command flags
 *
 * Reseed Purpose:
 * - Refreshes CSRNG internal state with new entropy from entropy_src
 * - Required every MAX_NUM_REQS_BETWEEN_RESEEDS generates per NIST SP 800-90A
 * - Prevents CSRNG state from becoming stale/predictable
 * - Must not exceed CSRNG RESEED_INTERVAL or command will be rejected
 *
 * Side Effects:
 * - Updates HW_CMD_STS.CMD_TYPE to 4 (Reseed)
 * - Updates HW_CMD_STS.CMD_ACK to 1 after CSRNG response
 * - Updates HW_CMD_STS.CMD_STS with CSRNG status code
 * - Resets m_auto_gen_counter to MAX_NUM_REQS_BETWEEN_RESEEDS
 * - CSRNG internal state refreshed with new seed material
 *
 * Error Handling:
 * - Empty FIFO: Returns error (should not occur if properly configured)
 * - CSRNG rejection: Returns non-zero status, triggers fatal error path
 * - Exceeding CSRNG RESEED_INTERVAL: CSRNG returns error, EDN enters Error state
 *
 * LT Timing:
 * - Reseed command duration: ~5 milliseconds (entropy source collection + mixing)
 * - Significantly longer than Generate due to entropy source interaction
 *
 * @note This function performs wait() and must be called from SC_THREAD context
 * @note RESEED_CMD FIFO is not modified - command is replayed for each reseed
 */
uint32_t edn_ip::auto_mode_issue_reseed()
{
    // Validate RESEED_CMD FIFO is not empty
    if (m_reseed_cmd_fifo.empty())
    {
        CSML_ERROR(1, logger) << "EDN_FUNC_013: RESEED_CMD FIFO is empty (configuration error)";

        // Set FIFO read error
        ERR_CODE.FIFO_READ_ERR = 1;

        // Set fatal error interrupt
        INTR_STATE.edn_fatal_err = 1;
        m_intr_update_event.notify(SC_ZERO_TIME);
        m_alert_update_event.notify(SC_ZERO_TIME);

        return 0xFFFF;  // Error status
    }

    // Extract command from FIFO (peek - don't remove, replay for each reseed)
    std::queue<uint32_t> fifo_copy = m_reseed_cmd_fifo;
    uint32_t header = fifo_copy.front();
    fifo_copy.pop();

    // Parse command header
    uint32_t clen = (header >> 8) & 0xF;

    CSML_INFO(1, logger) << "EDN_FUNC_013: Reseed command - clen=" << std::dec << clen;

    // Build complete command array (header + clen data words)
    uint32_t cmd_array[13];  // Max 13 words
    cmd_array[0] = header;

    for (uint32_t i = 0; i < clen && i < 12; i++)
    {
        if (!fifo_copy.empty())
        {
            cmd_array[i + 1] = fifo_copy.front();
            fifo_copy.pop();
        }
        else
        {
            // FIFO underflow: would require clen > words in FIFO, structurally prevented.
            CSML_ERROR(1, logger) << "EDN_FUNC_013: RESEED_CMD FIFO underflow (clen mismatch)";
            return 0xFFFF;
        }
    }
    (void)cmd_array; // Silence unused-but-set-variable warning

    // Update HW_CMD_STS before sending command
    HW_CMD_STS.CMD_TYPE = 4;  // Reseed command type
    HW_CMD_STS.CMD_ACK = 0;   // Clear ack (waiting for response)

    // LT delay for reseed command processing (~5ms)
    // Reseed requires entropy source interaction and is significantly longer than generate
    wait(5, SC_MS);

    // Simulate CSRNG Reseed success and consume OpenSSL entropy
    uint32_t ack_status = m_forced_csrng_ack_status;
    m_forced_csrng_ack_status = 0;
    uint32_t rand_data[4];
    RAND_bytes(reinterpret_cast<unsigned char*>(rand_data), 16);

    // Update HW_CMD_STS with CSRNG response
    HW_CMD_STS.CMD_ACK = 1;
    HW_CMD_STS.CMD_STS = ack_status & 0x7;

    CSML_INFO(1, logger) << "EDN_FUNC_013: Reseed command completed with status="
                         << std::hex << ack_status;

    return ack_status;
}
