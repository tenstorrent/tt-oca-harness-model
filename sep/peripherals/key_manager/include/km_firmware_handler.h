/**
 * @file km_firmware_handler.h
 * @brief Key Manager TT — Firmware handler (KM CPU abstraction).
 *
 * Pure C++ class replacing PicoRV32 ISS. Implements the key management firmware
 * operations described in docs/01_key_manager_Specification/doc/firmware.adoc,
 * whose ROM sources are under docs/01_key_manager_Specification/dv/fw/.
 *
 * The SC_THREAD (fw_thread) lives in key_manager_model and calls:
 *   1. boot()            — one-time initialisation (DRBG, KPV shred, engine init)
 *   2. process_messages()— called each time m_mailbox.inbound_msg_event() fires
 *
 * Message container format (firmware.adoc, message container):
 *   Word 0 (header): [31:24] HEADER_CRC8 | [23:16] PAYLOAD_LEN | [15:8] CMD_ID | [7:0] SEQ_NUM
 *   Words 1..N       payload words (PAYLOAD_LEN words)
 *   Word N+1         PAYLOAD_CRC32 (only present if PAYLOAD_LEN > 0)
 *   The last word in the message has the mailbox SEPARATOR bit set.
 *
 * RESP_CMD return argument bit layouts are confirmed from the command
 * response tables in firmware.adoc.
 */
#pragma once
#include "km_kpv.h"
#include "km_mailbox.h"
#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <cstring>

namespace keymgr_tt {

// ----------------------------------------------------------------------------
// km_firmware_handler
// ----------------------------------------------------------------------------
class km_firmware_handler {
public:
    // -----------------------------------------------------------------------
    // Command IDs  (firmware.adoc)
    // -----------------------------------------------------------------------
    enum cmd_id_t : uint8_t {
        CMD_HW_VER             = 0x00,
        CMD_ROM_VER            = 0x01,
        CMD_SRAM_VER           = 0x02,
        CMD_STAT               = 0x03,
        CMD_RECOV_ACK          = 0x04,  ///< Acknowledge recoverable fault (from rom_defs.h)
        CMD_EXEC_ROM           = 0x10,  ///< Lock to ROM; disable mutable-firmware handover
        CMD_SRAM_LOAD_EXEC     = 0x11,  ///< Load and run mutable firmware image
        CMD_SRAM_EXEC          = 0x12,  ///< Jump to already-loaded mutable firmware
        CMD_KEY_GENERATE       = 0x22,
        CMD_KEY_REVOKE         = 0x23,
        CMD_KEY_TRANSFER       = 0x24,
        CMD_ENGINE_SHRED       = 0x25,
        CMD_KEY_LOAD           = 0x26,  ///< Host-supplied key into the KPV
        CMD_ABR_SK_TRANSFER      = 0x27,  ///< Capture ML-KEM shared key from Adams Bridge
        CMD_OTP_READ_LOCK_COLD = 0x28,  ///< Cold-domain OTP read lock
    };

    // -----------------------------------------------------------------------
    // Response IDs  (firmware.adoc)
    // -----------------------------------------------------------------------
    enum resp_id_t : uint8_t {
        RESP_CMD                    = 0x00,
        RESP_KM_READY               = 0x55,  ///< Boot-complete announcement (from rom_defs.h)
        RESP_ABR_SHARED_KEY_READY   = 0x56,  ///< Never emitted: CMD_ABR_SK_TRANSFER not wired
        RESP_RECOVERABLE_FAULT      = 0xFE,
        RESP_UNRECOVERABLE_FAULT    = 0xFF,
    };

    // -----------------------------------------------------------------------
    // Return codes  (firmware.adoc / rom_defs.h)
    //   Order confirmed from rom_defs.h enum rom_ret_code_e.
    // -----------------------------------------------------------------------
    enum ret_code_t : int32_t {
        RET_SUCCESS     =  0,
        RET_FAILURE     = -1,
        RET_HEADER_CRC  = -2,
        RET_CMD_NOSEQ   = -3,
        RET_INVALID_CMD = -4,
        RET_INVALID_LEN = -5,
        RET_PAYLOAD_CRC = -6,
        RET_INVALID_ARG = -7,
    };

    // -----------------------------------------------------------------------
    // Recoverable fault codes  (firmware.adoc §Recoverable Fault Codes)
    //   Payload of RESP_RECOVERABLE_FAULT.
    // -----------------------------------------------------------------------
    enum recoverable_fault_code_t : int8_t {
        RFAULT_KEY_SLOT_CRC   = -1,
        RFAULT_RX_BUFF_OFLOW  = -2,
        RFAULT_MBOX_OVERFLOW  = -3,
        RFAULT_MBOX_UNDERFLOW = -4,
        RFAULT_FLUSHED_BY_SEP = -5,
    };

    // -----------------------------------------------------------------------
    // Unrecoverable fault codes  (firmware.adoc §Unrecoverable Fault Codes)
    //   Payload of RESP_UNRECOVERABLE_FAULT.
    // -----------------------------------------------------------------------
    enum unrecoverable_fault_code_t : int8_t {
        UFAULT_WIPE_STATE      = -1,
        UFAULT_ROM_PARITY      = -2,
        UFAULT_SRAM_PARITY     = -3,
        UFAULT_ROM_WRITE       = -4,
        UFAULT_SRAM_WRITE_LOCK = -5,
        UFAULT_AXI_DECERR      = -6,
        UFAULT_AXI_SLVERR      = -7,
        UFAULT_DRBG_ERR        = -8,
        UFAULT_ILLEGAL_INSN    = -9,
        UFAULT_BUS_ERROR       = -10,
        UFAULT_EBREAK          = -11,
        UFAULT_SPURIOUS_IRQ    = -12,
    };

    // -----------------------------------------------------------------------
    // DEST_VALID / DEST_ENGINE bitmask  (rom_defs.h)
    //   Bits 4-7 address the Adams Bridge seed ports, written through
    //   key_manager_model::key_transfer_via_socket.
    // -----------------------------------------------------------------------
    enum dest_t : uint8_t {
        DEST_HMAC            = (1u << 0),
        DEST_KMAC            = (1u << 1),
        DEST_AES             = (1u << 2),
        DEST_OTBN            = (1u << 3),
        DEST_ABR_MLDSA_SEED  = (1u << 4),
        DEST_ABR_MLKEM_D     = (1u << 5),
        DEST_ABR_MLKEM_Z     = (1u << 6),
        DEST_ABR_MLKEM_MSG   = (1u << 7),
        DEST_ABR_ALL         = 0xF0u,
    };

    /// Largest key the vault will hold, in 32-bit words. The wire encoding is
    /// word-count-minus-1 in 7 bits, so this is 128 (ROM_KM_MAX_KEY_WORDS), and a
    /// key that wide spans MAX_SLOTS_PER_KEY consecutive slots.
    static constexpr int KEY_LOAD_MAX_WORDS = km_kpv::MAX_KEY_WORDS;

    // -----------------------------------------------------------------------
    // Key-transfer callback type
    //   Called when CMD_KEY_TRANSFER or CMD_ENGINE_SHRED is processed.
    //   Arguments: dest_mask (DEST_* flags), key_words ptr, word_count
    //   Return true on success.
    //   Default stub: logs and returns true (no actual socket write).
    // -----------------------------------------------------------------------
    using key_xfer_fn_t = std::function<bool(uint8_t dest_mask,
                                             const uint32_t* words,
                                             int word_count,
                                             bool set_valid)>;

    // -----------------------------------------------------------------------
    // Version constants
    //   HW version confirmed from km_csr_regs.h: KM_CSR_VERSION_REG_REG_DEFAULT=0x00010000
    //   ROM version: no spec value provided; using 1.0.0 as default
    // -----------------------------------------------------------------------
    static constexpr uint8_t HW_MAJOR  = 1;
    static constexpr uint8_t HW_MINOR  = 0;
    static constexpr uint8_t HW_PATCH  = 0;
    static constexpr uint8_t ROM_MAJOR = 1;
    static constexpr uint8_t ROM_MINOR = 0;
    static constexpr uint8_t ROM_PATCH = 0;

    // -----------------------------------------------------------------------
    // Demotion-state callback type
    //   Called at KDF invocation time to read live DEMOTE_1/DEMOTE_2 values
    //   from lc_ctrl. Returns a 4-bit packed value:
    //     bits[1:0] = demote_1_value (domain-1, BL1 firmware)
    //     bits[3:2] = demote_2_value (domain-2, BL2 firmware)
    //   Each 2-bit field is differentially encoded as {~v, v}, so the undemoted value
    //   is 0xA and not 0x0 — 0b00 is not a legal code for either rail pair.
    //   Wired to lc_ctrl_model::get_demote_state() at start_of_simulation.
    // -----------------------------------------------------------------------
    using demote_fn_t = std::function<uint32_t()>;

    // -----------------------------------------------------------------------
    // OTP data (pushed from sep_efuse at start-of-simulation)
    //   lc_state and chiplet_uid come from sep_efuse (fuse-burned at manufacture).
    //   Demotion state is NOT here — it comes from lc_ctrl at runtime via
    //   m_get_demote callback (DEMOTE_1/2 are written by BL1/BL2 firmware).
    // -----------------------------------------------------------------------
    struct km_otp_data_t {
        uint32_t lc_state       = 0;      ///< Life-cycle state from OTP (sep_efuse)
        uint32_t chiplet_uid[8] = {};     ///< 256-bit chiplet UID from OTP (sep_efuse)
    };

    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------
    km_firmware_handler(km_kpv& kpv, km_mailbox& mailbox, drbg_fn_t get_random);

    /// Set optional key-transfer callback (to write keys to crypto engine sockets).
    void set_key_transfer_callback(key_xfer_fn_t fn) { m_key_xfer = fn; }

    /// Receive OTP data from sep_efuse (called from platform after end_of_elaboration).
    void set_otp_data(const km_otp_data_t& d) { m_otp_data = d; }

    /// Set demotion-state callback (wired to lc_ctrl at start_of_simulation).
    /// KM calls this at KDF invocation time to read live DEMOTE_1/2 from lc_ctrl.
    void set_demote_callback(demote_fn_t fn) { m_get_demote = fn; }

    // -----------------------------------------------------------------------
    // Main entry points (called from key_manager_model SC_THREAD)
    // -----------------------------------------------------------------------

    /// Reset firmware state (call from reset_process when rst_ni is asserted).
    /// Clears sequence counters and ready flags so boot() must run again.
    void reset();

    /// One-time boot sequence: KPV shred, engine key zero-init.
    void boot();

    /// Process all complete messages currently waiting in the inbound mailbox.
    void process_messages();

    // -----------------------------------------------------------------------
    // Status
    // -----------------------------------------------------------------------
    bool is_initialized()        const { return m_initialized; }
    bool is_sram_loaded()        const { return m_sram_loaded; }
    bool is_halted()             const { return m_halted; }
    bool recoverable_err_active() const { return m_recoverable_fault_active; }

    /// Push RESP_UNRECOVERABLE_FAULT to outbound FIFO and set halted flag.
    /// Called from wipe_process() after engine shredding (from rom_isr.c behaviour).
    void send_wipe_fault();

    /// Halt firmware — fw_thread checks this and stops accepting messages.
    void halt() { m_halted = true; }

private:
    // -----------------------------------------------------------------------
    // Message I/O
    // -----------------------------------------------------------------------

    /// Drain the inbound FIFO into m_rx_buffer, returning true and handing over
    /// the words only once a separator completes the frame.
    bool receive_message(std::vector<uint32_t>& words);

    /// Build and push a complete response message into the outbound mailbox.
    void push_message(uint8_t resp_id, uint8_t seq,
                      const std::vector<uint32_t>& payload);

    /// Convenience: send RESP_CMD with no return argument.
    void send_resp_cmd(uint8_t src_seq, uint8_t cmd_id, int32_t ret_code);

    /// Convenience: send RESP_CMD with one return argument word.
    void send_resp_cmd_arg(uint8_t src_seq, uint8_t cmd_id,
                           int32_t ret_code, uint32_t ret_arg);

    /// Send RESP_RECOVERABLE_FAULT with the given fault code.
    void send_recoverable_fault(recoverable_fault_code_t code);

    /// Send RESP_UNRECOVERABLE_FAULT with the given fault code.
    void send_unrecoverable_fault(unrecoverable_fault_code_t code);

    // -----------------------------------------------------------------------
    // Command handlers  (return true if a response was sent)
    // -----------------------------------------------------------------------
    bool handle_cmd_hw_ver         (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_rom_ver        (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_sram_ver       (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_stat           (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_recov_ack      (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_exec_rom       (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_sram_load_exec (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_sram_exec      (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_key_generate   (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_key_revoke     (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_key_transfer   (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_engine_shred   (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_key_load       (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_abr_sk_transfer  (uint8_t seq, const std::vector<uint32_t>& p);
    bool handle_cmd_otp_read_lock_cold(uint8_t seq, const std::vector<uint32_t>& p);

    // -----------------------------------------------------------------------
    // CRC helpers
    // -----------------------------------------------------------------------
    static uint8_t  crc8_header (uint32_t header_24bit);
    static uint32_t crc32_payload(const uint32_t* words, size_t count);

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------
    km_kpv&       m_kpv;
    km_mailbox&   m_mailbox;
    drbg_fn_t     m_get_random;
    key_xfer_fn_t m_key_xfer;       ///< Optional key-transfer callback
    /// Live demotion state from lc_ctrl. The stub stands in for an unconnected OTP port
    /// rather than for an undemoted part, which would read 0xA.
    demote_fn_t   m_get_demote = []() { return 0u; };

    bool          m_initialized             = false;
    bool          m_sram_loaded             = false;
    bool          m_halted                  = false; ///< Set after unrecoverable fault; blocks further commands
    bool          m_recoverable_fault_active = false; ///< RECOVERABLE_ERR bit (cleared by CMD_RECOV_ACK)
    uint8_t       m_next_rx_seq             = 0;     ///< Next expected inbound sequence number
    uint8_t       m_tx_seq                  = 0;     ///< Next outbound response sequence number
    bool          m_has_valid_rx            = false; ///< True once a fully valid command has been received
    uint8_t       m_last_valid_rx_seq       = 0;     ///< Sequence number of last fully validated command

    km_otp_data_t m_otp_data;                        ///< OTP data pushed from sep_efuse at start-of-simulation

    bool    m_rom_locked           = false;  ///< CMD_EXEC_ROM latched: handover refused
    uint8_t m_otp_read_lock_cold   = 0;      ///< KMCSR OTP_READ_LOCK_COLD [5:0], write-one-to-set

    /// Inbound reassembly buffer, sized like the firmware's SRAM message buffer:
    /// header + ROM_KM_MAX_PAYLOAD_LEN + payload CRC. A message may be written by
    /// SEP in several FIFO-sized instalments, so partial frames persist here
    /// between calls to process_messages().
    static constexpr size_t MSGBUF_WORDS = 1 + 255 + 1;
    std::vector<uint32_t> m_rx_buffer;

    // -----------------------------------------------------------------------
    // Key handle registry
    //   Opaque handles (1..255) map to KPV base slot indices.
    //   Handle 0 is the null handle (never allocated).
    //
    //   Permitted sideload destinations live here rather than in the KPV
    //   control register, mirroring hardware: the vault stores opaque blobs and
    //   the firmware key registry owns the destination policy.
    // -----------------------------------------------------------------------
    struct km_handle_entry_t {
        bool     valid      = false;
        uint8_t  base_slot  = 0;
        uint8_t  num_slots  = 0;   ///< Slots spanned; recorded at allocation
        uint8_t  dest_valid = 0;   ///< Destinations this key may be transferred to
        uint32_t crc32      = 0;   ///< CRC-32C over the key material as loaded
    };
    km_handle_entry_t m_handle_registry[256] = {};   ///< index = handle; [0] = null

    /// Next handle to hand out. Handles are issued monotonically and never
    /// recycled (rom_keyreg_generate), so revoking a key frees its slots but not
    /// its handle number. Exhausted once 255 handles have been issued.
    uint8_t m_next_handle = 1;

    /// Record a new handle for the key based at `base_slot`. Returns the handle,
    /// or -1 when the registry is exhausted.
    int  alloc_handle(uint8_t base_slot, uint8_t num_slots, uint8_t dest_valid,
                      uint32_t crc32);

    /// Mark handle as invalid. The handle number is not returned to the pool.
    void free_handle(uint8_t handle);

    // -----------------------------------------------------------------------
    // Shared key provisioning  (rom_load_key)
    //
    // CMD_KEY_GENERATE and CMD_KEY_LOAD are one code path in firmware: generate
    // fills a buffer from the DRBG and hands it to the same loader. Keeping that
    // structure here is what stops the two commands drifting apart.
    //
    // Ordering matters and mirrors the firmware: the handle is registered before
    // the vault is touched, so a registry-exhaustion failure cannot leave a
    // written, write-locked slot stranded with no handle referring to it.
    // -----------------------------------------------------------------------

    /// Provisioning outcome, distinguishing the cases the firmware distinguishes.
    enum load_key_rc_t : int {
        LOAD_OK          =  0,
        LOAD_BAD_ARGS    = -1,  ///< Bad length/destination, or no slot run fits
        LOAD_NO_HANDLES  = -2,  ///< Handle registry exhausted; vault untouched
        LOAD_KPV_FAILED  = -3,  ///< Vault write refused; handle rolled back
    };

    /// Place `words` in the vault and return a handle in `handle_out`.
    load_key_rc_t load_key(const uint32_t* words, int len, uint8_t dest_valid,
                           uint8_t& handle_out);
};

} // namespace keymgr_tt
