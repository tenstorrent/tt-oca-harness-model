// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Vayavya Labs Pvt. Ltd.
/******************************************************************************
 * @file kmac.h
 * @brief KMAC SystemC TLM model header
 *
 * This file defines the KMAC (Keccak Message Authentication Code) SystemC
 * TLM model class with all port interfaces including TLM target socket,
 * KeyMgr sideload interface, application interfaces, and control signals.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "kmac_base.h"
#include "kmac_interface.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <tlm_utils/tlm_quantumkeeper.h>
#include <tlm_utils/simple_target_socket.h>
#include <queue>
#include <vector>
#include <cstring>

/******************************************************************************
 * @class kmac_ip
 * @brief KMAC SystemC TLM model
 *
 * Top-level KMAC model providing cryptographic hash operations (SHA3, SHAKE,
 * cSHAKE, KMAC) with hardware application interfaces, KeyMgr sideload, and
 * entropy management. Uses OpenSSL for functional cryptographic operations.
 ******************************************************************************/
class kmac_ip : public kmac_base
{
public:
    SC_HAS_PROCESS(kmac_ip);

    /**
     * @brief KMAC model constructor
     * @param n Module name
     * @param memory_size Memory map size in bytes (default 0x1000)
     * @param num_app_intf Number of application interfaces (default 3)
     * @param en_masking Enable first-order masking (default true)
     */
#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
    csml_param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
    kmac_ip(sc_module_name n,
            unsigned int memory_size = 0x1000,
            unsigned int num_app_intf = 3,
            bool en_masking = true);

    /// @brief Destructor
    ~kmac_ip();

    // =========================================================================
    // Port Interfaces
    // =========================================================================

    /// @brief KeyMgr push sideload socket (KeyMgr writes key shares via TLM)
    tlm_utils::simple_target_socket<kmac_ip, 32> keymgr_tl_socket;

    /// @brief Application interfaces array (KeyMgr, LC_CTRL, ROM_CTRL)
    ///
    /// Deliberately left unbound at the platform. kmac_wrapper.sv ties app_i to
    /// zero and leaves app_o unused, so SEP has no hardware-initiated KMAC
    /// operations in silicon either; the KeyMgr sideload key arrives over
    /// keymgr_tl_socket instead. The handlers exist so the interface can be
    /// exercised from the unit testbench, not because the platform will drive
    /// them.
    sc_export<kmac_app_if>* app_export;

    /// @brief Idle status output (true when FSM in IDLE state)
    sc_out<bool> idle_o;

    // Three independent interrupt outputs, matching the three vectors sep.sv
    // places on sep_internal_interrupts[20:22]. Each carries one INTR_STATE bit
    // gated by the matching INTR_ENABLE bit; they are not OR-reduced here
    // because the PIC gives them separate slots.

    /// @brief kmac_done interrupt (INTR_STATE.kmac_done & INTR_ENABLE.kmac_done)
    sc_out<bool> intr_kmac_done;

    /// @brief fifo_empty interrupt (INTR_STATE.fifo_empty & INTR_ENABLE.fifo_empty)
    sc_out<bool> intr_fifo_empty;

    /// @brief kmac_err interrupt (INTR_STATE.kmac_err & INTR_ENABLE.kmac_err)
    sc_out<bool> intr_kmac_err;

    // Alert outputs. sep_crypto.sv gives KMAC two alert channels and folds them,
    // with every other crypto block's, into the single crypto_alert_o that
    // sep.sv drives onto sep_internal_interrupts[32].

    /// @brief Recoverable alert, mirrors STATUS.ALERT_RECOV_CTRL_UPDATE_ERR
    sc_out<bool> alert_recov_operation_err;

    /// @brief Fatal alert, mirrors STATUS.ALERT_FATAL_FAULT
    sc_out<bool> alert_fatal_fault;

    /// @brief Life cycle escalation input (triggers security response)
    sc_in<bool> lc_escalate_en_i;

    /// @brief Active-low reset input (asynchronous reset)
    sc_in<bool> rst_ni;

    /// @brief Primary clock input signal
    sc_in<bool> clk_i;

    // =========================================================================
    // Configuration Parameters
    // =========================================================================

    /// @brief Number of application interfaces
    const unsigned int NumAppIntf;

    /// @brief Enable first-order masking (affects key/state handling)
    const bool EnMasking;



    // Nested class forward declaration and friend access
    class kmac_app_handler;
    friend class kmac_app_handler;
    friend class testbench;

private:
    /**
     * @brief FSM State Enumeration
     *
     * Defines the five primary states of the KMAC state machine.
     * States are mutually exclusive and tracked via STATUS register bits.
     */
    enum class KmacState {
        IDLE,               ///< Idle state, ready for configuration and START command
        ABSORB,             ///< Absorbing message data from MSG_FIFO
        SQUEEZE,            ///< Digest available, can read STATE or issue RUN
        ERROR,              ///< Error state, requires err_processed recovery
        ESCALATION_LOCKED   ///< FUNC-KMAC-021: Life cycle escalation state, reset-only recovery
    };

    /**
     * @brief Internal FSM state tracking
     *
     * Tracks current operational state of KMAC engine.
     * Updated by command processing callbacks.
     */
    KmacState fsm_state;

    /**
     * @brief Event for idle_o port driver
     *
     * Notified when FSM state changes, triggers idle_o signal update.
     * Implements single-writer pattern for idle_o port.
     */
    sc_event idle_update_event;

    /**
     * @brief Event for the interrupt port driver
     *
     * Notified when INTR_STATE or INTR_ENABLE changes, triggers an update of
     * the three interrupt outputs. Implements the single-writer pattern.
     */
    sc_event intr_update_event;

    /**
     * @brief Event for the alert port driver
     *
     * Notified when either STATUS alert mirror changes. Implements the
     * single-writer pattern for the two alert outputs.
     */
    sc_event alert_update_event;

    /**
     * @brief Whether the message FIFO has been full since the last Start
     *
     * RTL only raises the fifo_empty interrupt once the FIFO has actually
     * filled, so software is not interrupted while hardware is keeping up.
     * Cleared on Start and on reset.
     */
    bool fifo_full_seen;

    /**
     * @brief Deferred drain of the message FIFO once the engine catches up
     *
     * Message bytes are absorbed into the digest as soon as they are packed,
     * so the queue and fifo_depth exist purely to model occupancy. Draining
     * them on a delayed event rather than inside the write keeps the full
     * condition observable to software for the interval the engine would
     * really have taken to consume a full FIFO, which is what makes the
     * fill/stall/drain/refill handshake behave as it does in hardware.
     */
    sc_event fifo_drain_event;

    /**
     * @brief Whether the Process command has been issued for this message
     *
     * One of the three RDL preconditions for the fifo_empty interrupt: once
     * Process is written, software has no more data to supply and the
     * interrupt is pointless. Cleared on Start and on reset.
     */
    bool process_issued;

    /**
     * @brief Event for STATUS register dynamic updates
     *
     * Notified when FSM state changes to update STATUS.sha3_idle/absorb/squeeze bits.
     */
    sc_event status_update_event;

    /**
     * @brief OpenSSL EVP message digest context
     *
     * Stores OpenSSL EVP_MD_CTX context for cryptographic operations.
     * Initialized on START command based on CFG_SHADOWED mode/kstrength.
     * Cleaned up on DONE command or reset.
     */
    void* evp_md_ctx;

    /**
     * @brief OpenSSL EVP_MAC algorithm object (for KMAC mode)
     *
     * Stores OpenSSL EVP_MAC* fetched for KMAC-128 or KMAC-256.
     * Used in KMAC mode (CFG_SHADOWED.kmac_en = 1).
     * Cleaned up on DONE command or reset.
     */
    void* evp_mac;

    /**
     * @brief OpenSSL EVP_MAC context (for KMAC mode)
     *
     * Stores OpenSSL EVP_MAC_CTX* for KMAC operations.
     * Initialized on START command when kmac_en=1.
     * Cleaned up on DONE command or reset.
     */
    void* evp_mac_ctx;

    // =========================================================================
    // FUNC-KMAC-001: SHA3 Internal State Variables
    // =========================================================================

    /**
     * @brief Packer buffer for byte/halfword/word accumulation
     *
     * Accumulates partial writes (1/2/4 bytes) until a complete 64-bit entry
     * is formed. Index range: [0..7] bytes.
     * Managed by handle_write_MSG_FIFO based on packer_position counter.
     */
    uint8_t packer_buffer[8];

    /**
     * @brief Packer byte position counter
     *
     * Tracks current byte position within packer_buffer (0-7).
     * When position reaches 8, the complete 64-bit entry is pushed to msg_fifo
     * and position resets to 0.
     * Range: 0-7 bytes.
     */
    unsigned int packer_position;

    /**
     * @brief Message FIFO queue
     *
     * Internal queue storing 64-bit message entries for SHA3 absorption.
     * Depth configured via MsgFifoDepth parameter (default 10 entries).
     * Populated by MSG_FIFO window writes (0x800-0xFFC).
     * Consumed by PROCESS command via EVP_DigestUpdate.
     */
    std::queue<uint64_t> msg_fifo;

    /**
     * @brief Current FIFO depth (occupied entries)
     *
     * Tracks number of 64-bit entries currently stored in msg_fifo.
     * Exposed via STATUS.fifo_depth[12:8] field.
     * Range: 0 to MsgFifoDepth (default 0-10).
     */
    unsigned int fifo_depth;

    /**
     * @brief Maximum FIFO depth (build-time parameter)
     *
     * Configured via MsgFifoDepth parameter (default 10 entries).
     * Used for backpressure detection in handle_write_MSG_FIFO.
     */
    const unsigned int max_fifo_depth;

    /**
     * @brief Digest output buffer
     *
     * Stores the final digest computed by EVP_DigestFinal_ex during PROCESS.
     * Size: 200 bytes (accommodates largest SHA3-512 digest = 64 bytes).
     * Valid only in SQUEEZE state, returned via STATE window reads (0x400-0x5FC).
     * - SHA3-224: 28 bytes
     * - SHA3-256: 32 bytes
     * - SHA3-384: 48 bytes
     * - SHA3-512: 64 bytes
     */
    uint8_t digest_buffer[200];

    /**
     * @brief Digest share0 buffer (for EnMasking=1 mode)
     *
     * Stores the first share of the masked digest.
     * When EnMasking=true: digest = share0 XOR share1.
     * Accessible via STATE window share0 region (0x400-0x4C7).
     * Size: 200 bytes (matches Keccak state capacity).
     */
    uint8_t digest_share0[200];

    /**
     * @brief Digest share1 buffer (for EnMasking=1 mode)
     *
     * Stores the second share of the masked digest.
     * When EnMasking=true: digest = share0 XOR share1.
     * Accessible via STATE window share1 region (0x500-0x5C7).
     * Size: 200 bytes (matches Keccak state capacity).
     */
    uint8_t digest_share1[200];

    /**
     * @brief Computed digest size in bytes
     *
     * Stores the actual digest size returned by EVP_DigestFinal_ex.
     * Used for bounds checking during STATE window reads.
     * Values: 28 (SHA3-224), 32 (SHA3-256), 48 (SHA3-384), 64 (SHA3-512).
     */
    unsigned int digest_size;

    /// @brief Full XOF output buffer for extended output (KMAC/SHAKE RUN commands).
    /// Sized to hold multiple rate-sized blocks so successive RUN commands can
    /// window through a single EVP_DigestFinalXOF result (EVP_DigestFinalXOF may
    /// only be called once per context).
    uint8_t xof_full_output[4096];

    /// @brief Current offset in xof_full_output for RUN command slicing
    size_t xof_output_offset;

    /// @brief Total XOF output length requested (for bounds checking)
    size_t xof_total_length;

    // =========================================================================
    // FUNC-KMAC-007: Application Interface State Variables
    // =========================================================================

    /**
     * @brief Application interface handler instances (one per app)
     *
     * Array of internal handler objects that implement kmac_app_if interface.
     * Each handler is bound to corresponding app_export[i] sc_export.
     * Dynamically allocated based on NumAppIntf parameter.
     */
    kmac_app_handler** app_handlers;

    /**
     * @brief Application interface active flag
     *
     * Indicates whether any application interface operation is currently active.
     * When true, software MMIO access is blocked (MSG_FIFO, CMD writes generate errors).
     * STATE window reads return 0 for key protection.
     */
    bool app_interface_active;

    /**
     * @brief Currently selected application index
     *
     * Index of application interface currently processing (0=KeyMgr, 1=LC_CTRL, 2=ROM_CTRL).
     * Valid only when app_interface_active==true.
     * Fixed-priority arbitration: lower index = higher priority.
     */
    unsigned int active_app_index;

    /**
     * @brief Application operation done flag
     *
     * Indicates that application interface operation has completed and digest is ready.
     * Returned via is_done() interface method.
     */
    bool app_operation_done;

    /**
     * @brief Application operation error flag
     *
     * Indicates that application interface operation encountered an error (e.g., KeyNotValid).
     * Returned via has_error() interface method.
     */
    bool app_operation_error;


    // Note: Application interface now uses shared msg_fifo for message data
    // (aligned with OpenTitan hardware where both SW and App paths use MSG_FIFO)

    /**
     * @brief Application digest output (share0)
     *
     * Stores digest share0 result (256 bits = 32 bytes) for application interface.
     * Returned via get_digest() interface method.
     */
    uint32_t app_digest_share0[8];

    /**
     * @brief Application digest output (share1)
     *
     * Stores digest share1 result (256 bits = 32 bytes) for application interface.
     * If EnMasking==false, this contains zeros.
     * Returned via get_digest() interface method.
     */
    uint32_t app_digest_share1[8];

    /**
     * @brief Application interface algorithm configuration
     *
     * Stores algorithm type for each application interface:
     * - Index 0 (KeyMgr): KMAC mode with sideloaded key
     * - Index 1 (LC_CTRL): cSHAKE128 mode
     * - Index 2 (ROM_CTRL): cSHAKE256 mode
     *
     * Hardcoded per AppCfg parameter, cannot be changed at runtime.
     * Values: 0=KMAC, 1=cSHAKE128, 2=cSHAKE256
     */
    unsigned int app_algorithm[3];

    // =========================================================================
    // FUNC-KMAC-006: KeyMgr Push Sideload Buffers
    // =========================================================================

    /// @brief KEY_SHARE0 words pushed by KM (offsets 0x00-0x1C)
    uint32_t m_keymgr_share0[8];

    /// @brief KEY_SHARE1 words pushed by KM (offsets 0x20-0x3C)
    uint32_t m_keymgr_share1[8];

    /// @brief Number of key bytes written to share0 region (tracks key length)
    size_t m_keymgr_key_len_bytes;

    /// @brief Set true when KM writes KEY_CTRL=1 at offset 0x40
    bool m_keymgr_key_valid;

    /// @brief TLM b_transport handler for KeyMgr push sideload socket
    void keymgr_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);

    // =========================================================================
    // FUNC-KMAC-014: Software Mode Entropy State Variables
    // =========================================================================

    /**
     * @brief Software mode seed write counter
     *
     * Tracks the number of ENTROPY_SEED register writes received (0-5).
     * After 6th write (count reaches 6), PRNG activates and further writes ignored.
     * Only increments when CFG_SHADOWED.entropy_mode=sw_mode (0x2) and entropy_ready=1.
     * Reset to 0 on reset or when entropy_mode changes.
     * Range: 0-6 (6 indicates seeding complete).
     */
    unsigned int sw_seed_write_count;

    /**
     * @brief Software mode seed storage buffer
     *
     * Stores the 6x32-bit entropy seed chunks written to ENTROPY_SEED register.
     * Each write loads into corresponding index (write 0 → seed_buffer[0], etc.).
     * Forms complete 192-bit entropy seed for Bivium PRNG (functionally abstracted).
     * Valid only after sw_seed_write_count reaches 6.
     */
    uint32_t sw_seed_buffer[6];

    /**
     * @brief Software mode PRNG ready flag
     *
     * Indicates that SW mode entropy seeding is complete and PRNG is operational.
     * Set to true after 6th ENTROPY_SEED write.
     * When true, KMAC operations can proceed (no SwHashingWithoutEntropyReady error).
     * When false with entropy_mode=sw_mode, operations blocked.
     * Reset to false on reset or when entropy_mode changes.
     */
    bool entropy_sw_mode_ready;

    // =========================================================================
    // Methods
    // =========================================================================

    /**
     * @brief Reset and escalation handling process
     */
    void reset_and_escalation_process();

    /**
     * @brief idle_o port driver SC_METHOD
     *
     * Single-writer method for idle_o signal.
     * Sensitive to idle_update_event.
     * Updates idle_o based on fsm_state == IDLE.
     */
    void idle_o_driver();

    /**
     * @brief Interrupt port driver SC_METHOD
     *
     * Single-writer method for the three interrupt outputs.
     * Sensitive to intr_update_event.
     * Drives each output from its own INTR_STATE bit gated by INTR_ENABLE.
     */
    void intr_o_driver();

    /**
     * @brief Alert port driver SC_METHOD
     *
     * Single-writer method for the two alert outputs. Sensitive to
     * alert_update_event, which is notified whenever the STATUS alert mirrors
     * change.
     */
    void alert_o_driver();

    /**
     * @brief Evaluate and update interrupt outputs
     *
     * Notifies intr_update_event to trigger the driver.
     * Call this after any change to INTR_STATE or INTR_ENABLE.
     */
    void evaluate_interrupt();

    /**
     * @brief Evaluate and update alert outputs
     *
     * Notifies alert_update_event. Call this after any change to
     * STATUS.ALERT_RECOV_CTRL_UPDATE_ERR or STATUS.ALERT_FATAL_FAULT.
     */
    void evaluate_alert();

    /**
     * @brief Recompute INTR_STATE.fifo_empty from the live FIFO condition
     *
     * fifo_empty is a status interrupt in RTL: sw = r, driven directly by
     * hardware rather than latched and cleared by software. It is raised only
     * while the message FIFO is genuinely writable by software, which the RDL
     * defines as no application interface active, SHA3 in Absorb, and Process
     * not yet issued. The FIFO must also have been full at some point since
     * the last Start, otherwise the engine drains faster than software can
     * fill it and interrupting would be pointless. That last precondition also
     * suppresses the continuous retrigger the FIFO's Pass=1 behaviour would
     * otherwise cause, since data passes straight through and leaves the depth
     * at zero.
     */
    void update_fifo_empty_interrupt();

    /**
     * @brief Drain the modelled FIFO occupancy after the engine catches up
     *
     * SC_METHOD sensitive to fifo_drain_event. Clears the occupancy and
     * re-evaluates the fifo_empty interrupt, which is where the interrupt
     * actually gets raised in a fill/stall/drain sequence.
     */
    void fifo_drain_process();

    // FUNC-KMAC-005: Key Management Callback Handlers

    /**
     * @brief CFG_SHADOWED register write callback handler (shadow register validation)
     * @return true if write successful
     */
    bool handle_write_CFG_SHADOWED(uint32_t value, uint8_t byte_enable);

    /**
     * @brief ENTROPY_REFRESH_THRESHOLD_SHADOWED write callback handler
     *
     * Implements the two-write shadow protocol the RDL requires for this
     * register: stage the first write, commit on a matching second write, and
     * raise STATUS.ALERT_RECOV_CTRL_UPDATE_ERR on a mismatch.
     * @return false always, because the handler owns the field update
     */
    bool handle_write_ENTROPY_REFRESH_THRESHOLD_SHADOWED(uint32_t value,
                                                         uint8_t byte_enable);

    /**
     * @brief Trigger the automatic PRNG reseed once the hash count reaches the
     *        configured threshold
     *
     * Called after every hash-count change and after a threshold commit, since
     * lowering the threshold can satisfy the comparison immediately.
     */
    void check_entropy_refresh_threshold();

    /**
     * @brief KEY_SHARE0 register write callback handler (CFG_REGWEN protected)
     * @return true if write successful
     */
    bool handle_write_KEY_SHARE0(unsigned int index, uint32_t value, uint8_t byte_enable);

    /**
     * @brief KEY_SHARE1 register write callback handler (CFG_REGWEN protected)
     * @return true if write successful
     */
    bool handle_write_KEY_SHARE1(unsigned int index, uint32_t value, uint8_t byte_enable);

    /**
     * @brief KEY_LEN register write callback handler (CFG_REGWEN protected)
     * @return true if write successful
     */
    bool handle_write_KEY_LEN(uint32_t value, uint8_t byte_enable);

    /**
     * @brief ENTROPY_PERIOD register write callback handler (CFG_REGWEN protected)
     * @return true if write successful
     */
    bool handle_write_ENTROPY_PERIOD(uint32_t value, uint8_t byte_enable);

    /**
     * @brief PREFIX register write callback handler (CFG_REGWEN protected)
     * @param index PREFIX register index (0-10)
     * @return true if write successful
     */
    bool handle_write_PREFIX(unsigned int index, uint32_t value, uint8_t byte_enable);

    /**
     * @brief Helper function to zeroize key registers on error or reset
     */
    void zeroize_key_registers();

    // FUNC-KMAC-014: Software Mode Entropy Seeding Callback

    /**
     * @brief ENTROPY_SEED register write callback handler
     * @param value Written 32-bit seed chunk value
     * @param write_mask Write enable mask for ENTROPY_SEED register
     * @return true if write accepted and processed, false if ignored
     *
     * Implements software mode entropy seeding sequence per FUNC-KMAC-014:
     * - Validates CFG_SHADOWED.entropy_mode = 0x2 (sw_mode)
     * - Validates CFG_SHADOWED.entropy_ready = 1
     * - Loads written value into sw_seed_buffer[sw_seed_write_count]
     * - Increments sw_seed_write_count (0 → 1 → ... → 6)
     * - After 6th write: Sets entropy_sw_mode_ready = true, activates PRNG
     * - Writes after 6th are ignored (no effect, PRNG already seeded)
     * - Each write loads into corresponding Bivium state chunk (functionally abstracted)
     *
     * Prerequisites:
     * - CFG_SHADOWED.entropy_mode must be 0x2 (sw_mode)
     * - CFG_SHADOWED.entropy_ready must be 1
     *
     * Side effects:
     * - Updates sw_seed_buffer[sw_seed_write_count]
     * - Increments sw_seed_write_count
     * - Sets entropy_sw_mode_ready after 6th write
     * - Enables KMAC operations (prevents SwHashingWithoutEntropyReady error)
     *
     * Architecture Map Reference:
     * - State Machine: Entropy_Generator_FSM (RESET → SW_SEED_WAIT → READY)
     * - Register: ENTROPY_SEED (offset 0x2C, 32-bit WO)
     */
    bool handle_write_ENTROPY_SEED(uint32_t value, uint32_t write_mask);

    /**
     * @brief CMD register write callback handler
     * @param value Written value to CMD register
     * @param write_mask Write enable mask for CMD register
     * @return true if write successful, false otherwise
     *
     * Implements FSM state transitions based on sparse-encoded commands:
     * - START (0x1D): IDLE → ABSORB
     * - PROCESS (0x2E): ABSORB → SQUEEZE
     * - RUN (0x31): Remain in SQUEEZE, execute additional rounds
     * - DONE (0x16): Any state → IDLE
     *
     * Also handles auxiliary command bits:
     * - entropy_req: Triggers PRNG reseed
     * - hash_cnt_clr: Clears ENTROPY_REFRESH_HASH_CNT
     * - err_processed: Recovers from ERROR state
     *
     * Side effects:
     * - Updates fsm_state
     * - Updates CFG_REGWEN.en (auto-clear on START, auto-set on DONE)
     * - Notifies idle_update_event and status_update_event
     * - Generates SwCmdSequence error for invalid command sequences
     */
    bool handle_write_CMD(uint32_t value, uint32_t write_mask);

    /**
     * @brief STATUS register read callback handler
     * @param value Reference to return STATUS register value
     * @param read_mask Read enable mask for STATUS register
     * @return true if read successful, false otherwise
     *
     * Dynamically constructs STATUS register value from current FSM state:
     * - Bit 0 (sha3_idle): 1 if fsm_state == IDLE
     * - Bit 1 (sha3_absorb): 1 if fsm_state == ABSORB
     * - Bit 2 (sha3_squeeze): 1 if fsm_state == SQUEEZE
     * - Bits [12:8] (fifo_depth): Current MSG_FIFO depth (placeholder 0)
     * - Bit 14 (fifo_empty): FIFO empty status (placeholder 1)
     * - Bit 15 (fifo_full): FIFO full status (placeholder 0)
     * - Bit 16 (ALERT_FATAL_FAULT): Fatal error status
     * - Bit 17 (ALERT_RECOV_CTRL_UPDATE_ERR): Shadow register mismatch
     *
     * All status bits are read-only and dynamically computed.
     */
    bool handle_read_STATUS(uint32_t& value, uint32_t read_mask);

    /**
     * @brief Updates FSM state and triggers notifications
     * @param new_state New FSM state to transition to
     *
     * Helper function to update fsm_state and notify driver events.
     * Ensures consistent state update and signal notification pattern.
     */
    void update_fsm_state(KmacState new_state);

    /**
     * @brief MSG_FIFO window write callback handler
     * @param index Array index within MSG_FIFO register array (0-511)
     * @param value Written data value
     * @param byte_enable Byte enable signals from TLM transaction
     * @return true if write successful, false otherwise
     *
     * Implements message data absorption with byte packing:
     * - Only accepts writes in ABSORB state (returns SwPushedMsgFifo error 0x02 otherwise)
     * - Accumulates bytes/halfwords/words into packer_buffer based on byte_enable
     * - Pushes complete 64-bit entries to msg_fifo when packer_position reaches 8
     * - Calls EVP_DigestUpdate for each complete 64-bit entry
     * - Updates fifo_depth and STATUS.fifo_empty/fifo_full bits
     * - Implements backpressure via temporal decoupling when FIFO full
     * - Applies msg_endianness byte-swap if CFG_SHADOWED.msg_endianness=1
     *
     * Side effects:
     * - Updates packer_buffer and packer_position
     * - Pushes to msg_fifo queue
     * - Updates fifo_depth
     * - Calls OpenSSL EVP_DigestUpdate
     * - Generates SwPushedMsgFifo error (0x02) for invalid state
     */
    bool handle_write_MSG_FIFO(unsigned int index, uint32_t value, uint8_t byte_enable);

    /**
     * @brief STATE window read callback handler
     * @param index Array index within STATE register array (0-127)
     * @param value Reference to return read data value
     * @return true if read successful, false otherwise
     *
     * Implements conditional digest output access:
     * - Returns 0 if fsm_state == IDLE or ABSORB (key protection mechanism)
     * - Returns valid digest data only in SQUEEZE state
     * - Index decoding:
     *   - 0-63: share0 region (0x400-0x4FC, 256 bytes)
     *   - 64-127: share1 region (0x500-0x5FC, 256 bytes)
     * - If EnMasking=1: Returns digest_share0 for share0 region, digest_share1 for share1 region
     * - If EnMasking=0: Returns digest_buffer for share0 region, 0 for share1 region
     * - Applies state_endianness byte-swap if CFG_SHADOWED.state_endianness=1
     * - Bounds checking: Returns 0 for indices beyond digest_size
     *
     * Side effects: None (read-only operation)
     */
    bool handle_read_STATE(unsigned int index, uint32_t& value);

    // =========================================================================
    // FUNC-KMAC-007: Application Interface Methods
    // =========================================================================

    /**
     * @brief Internal handler for application interface data requests
     * @param app_index Application interface index (0-2)
     * @param data 64-bit data word
     * @param strobe Byte enable mask (bit 0 = byte 0 valid, etc.)
     * @param last True if this is the final data beat
     *
     * Called by kmac_app_handler::app_request() to process application data.
     * Implements:
     * - Fixed-priority arbitration (only first app request accepted if multiple)
     * - Data accumulation in app_message_buffer
     * - Automatic KMAC operation trigger on last==true
     * - KeyMgr-specific: sideloaded key validation, output length encoding
     * - Software lockout enforcement
     */
    void handle_app_request(unsigned int app_index, uint64_t data, uint8_t strobe, bool last);

    /**
     * @brief Execute application interface KMAC/cSHAKE operation
     * @param app_index Application interface index (0-2)
     *
     * Executes complete hash operation for application interface:
     * 1. Initialize OpenSSL context based on app algorithm
     * 2. Load sideloaded key (KeyMgr only)
     * 3. Process message from app_message_buffer
     * 4. Append right_encode(256) for KMAC (KeyMgr only)
     * 5. Finalize and compute digest
     * 6. Split digest into two shares (or share0 only if EnMasking==false)
     * 7. Set app_operation_done flag
     *
     * Side effects:
     * - Updates app_digest_share0/1
     * - Sets app_operation_done or app_operation_error
     * - Clears app_message_buffer
     * - Returns FSM to IDLE state
     */
    void execute_app_operation(unsigned int app_index);

    /**
     * @brief Clear application interface state after operation completion
     *
     * Resets all application interface state variables to allow next operation:
     * - Clears app_interface_active flag
     * - Resets app_operation_done and app_operation_error
     * - Clears app_message_buffer
     * - Releases software lockout
     */
    void clear_app_state();

    /// @brief Logger instance for structured logging
    mutable CsmlLogger logger;

    // =========================================================================
    // FUNC-KMAC-024: Temporal Decoupling and Timing Abstraction
    // =========================================================================

    /**
     * @brief TLM-2.0 quantum keeper for temporal decoupling
     *
     * Manages local time accumulation and synchronization with the SystemC kernel.
     * Used to implement non-cycle-accurate timing model with nominal delays for:
     * - FIFO full backpressure (100 cycles / clk_i frequency)
     * - Entropy request latency (configured via ENTROPY_PERIOD)
     * - State machine transitions (START, PROCESS, DONE commands)
     * - Application interface response timing
     *
     * Temporal decoupling allows the model to accumulate local time and only
     * synchronize with the SystemC kernel when the quantum is exceeded, improving
     * simulation performance while maintaining functional correctness.
     *
     * Architecture alignment: architecture_map.timing_constraints
     * Functionality: FUNC-KMAC-024 (Temporal Decoupling and Timing Abstraction)
     */
    tlm_utils::tlm_quantumkeeper m_qk;

    // Fixed: Member variables for CFG_SHADOWED state (replacing static locals)
    bool m_shadow_pending;
    uint32_t m_shadow_pending_value;

    // ENTROPY_REFRESH_THRESHOLD_SHADOWED is a second shadowed register and
    // carries its own independent phase tracking; a partial write to one must
    // not be completed by a write to the other.
    bool m_threshold_shadow_pending;
    uint32_t m_threshold_shadow_pending_value;

    /// @brief Number of automatic threshold-triggered reseeds since reset.
    /// Not a register; exists so tests can tell an automatic reseed apart from
    /// a hash counter that simply never advanced.
    uint32_t m_auto_reseed_count;
};
