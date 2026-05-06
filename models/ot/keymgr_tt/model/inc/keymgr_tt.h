#pragma once
#include "keymgr_tt_base.h"
#include "km_kpv.h"
#include "km_mailbox.h"
#include "km_firmware_handler.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <memory>
#include <queue>

/**
 * @brief Key Manager TT TLM model.
 *
 * Extends keymgr_tt_base with:
 *   - Active-low reset input (rst_ni)
 *   - IRQ output to SEP CPU
 *   - CsmlLogger for diagnostics
 *   - km_kpv  : internal Key and Policy Vault storage
 *   - km_mailbox : internal mailbox FIFO model
 *   - register_all_callbacks() : wires side-effect handlers onto csml_memory
 */
class keymgr_tt_model : public keymgr_tt_base
{
public:
    SC_HAS_PROCESS(keymgr_tt_model);

    keymgr_tt_model(sc_module_name name,
                    int log_verbosity = CSML_DEFAULT_VERBOSITY);

    // -----------------------------------------------------------------------
    // Ports
    // -----------------------------------------------------------------------
    sc_in<bool>  rst_ni;   ///< Active-low reset
    sc_in<bool>  wipe_ni;  ///< Active-low emergency wipe (highest-priority interrupt)
    sc_out<bool> irq;      ///< IRQ to SEP CPU

    /// KM → crypto engine key storage (one socket per engine).
    /// CMD_KEY_TRANSFER / CMD_ENGINE_SHRED write key words to these sockets.
    /// DEST_VALID bit mapping (KeyManager.md): bit0=HMAC, bit1=KMAC, bit2=AES, bit3=OTBN
    tlm_utils::simple_initiator_socket<keymgr_tt_model, 32> hmac_key_socket;
    tlm_utils::simple_initiator_socket<keymgr_tt_model, 32> kmac_key_socket;
    tlm_utils::simple_initiator_socket<keymgr_tt_model, 32> aes_key_socket;
    tlm_utils::simple_initiator_socket<keymgr_tt_model, 32> otbn_key_socket;

    CsmlLogger logger;
    csml_param<int> verbosity; ///< Logging verbosity (runtime-overridable via ini file)

    /// Receive OTP data from sep_efuse (call after end_of_elaboration).
    void set_otp_data(const keymgr_tt::km_firmware_handler::km_otp_data_t& d) {
        m_firmware->set_otp_data(d);
    }

    /// Wire live demotion-state source from lc_ctrl (call at start_of_simulation).
    /// KM reads this callback at KDF invocation time, not at elaboration.
    void set_demote_callback(keymgr_tt::km_firmware_handler::demote_fn_t fn) {
        m_firmware->set_demote_callback(fn);
    }

private:
    // -----------------------------------------------------------------------
    // Internal components
    // -----------------------------------------------------------------------
    keymgr_tt::km_kpv     m_kpv;      ///< Internal Key & Policy Vault
    keymgr_tt::km_mailbox m_mailbox;   ///< Internal mailbox FIFOs

    std::unique_ptr<keymgr_tt::km_firmware_handler> m_firmware;  ///< KM CPU abstraction

    // -----------------------------------------------------------------------
    // Firmware thread
    // -----------------------------------------------------------------------
    void fw_thread();   ///< SC_THREAD: boot + message processing loop

    // -----------------------------------------------------------------------
    // Random word generation (OpenSSL RAND_bytes)
    // -----------------------------------------------------------------------
    uint32_t get_random_word();

    // -----------------------------------------------------------------------
    // Key transfer to crypto engines via initiator sockets
    // -----------------------------------------------------------------------
    bool key_transfer_via_socket(uint8_t dest_mask, const uint32_t* words, int count,
                                 bool set_valid = true);

    // -----------------------------------------------------------------------
    // SystemC processes
    // -----------------------------------------------------------------------
    void reset_process();          ///< SC_METHOD: responds to rst_ni de-assertion
    void wipe_process();           ///< SC_METHOD: responds to wipe_ni assertion (emergency key shred)
    void irq_update_process();     ///< SC_METHOD: drives irq based on MB_IRQS & MB_IRQEN

    /// Update level-sensitive IRQ bits: bit[0]=OUTBOUND_READ_DATA_AVAIL, bit[1]=INBOUND_WRITE_SPACE_AVAIL
    void update_level_irq_bits();

    sc_event m_irq_update_event;   ///< Notified whenever interrupt state may have changed

    // -----------------------------------------------------------------------
    // Callback registration
    // -----------------------------------------------------------------------
    void register_all_callbacks();

    // -----------------------------------------------------------------------
    // Register callback handlers
    // -----------------------------------------------------------------------
    // Mailbox
    bool handle_write_MB_WDATA  (uint32_t value);
    bool handle_write_MB_WSEP   (uint32_t value);
    bool handle_read_MB_RDATA   (uint32_t &value);
    bool handle_read_MB_STATUS  (uint32_t &value);
    bool handle_write_MB_STATUS (uint32_t value);  ///< W1C for overflow/underflow bits[23:20]
    bool handle_write_MB_IRQS   (uint32_t value);
    bool handle_write_MB_CTRL   (uint32_t value);

    // KPVLP  (slot/word index captured by lambda in register_all_callbacks)
    bool handle_write_KPVLP_KEY (unsigned int slot, unsigned int word, uint32_t value);
    bool handle_write_KPVLP_CTRL(unsigned int slot, uint32_t value);
    bool handle_read_KPVLP_STATUS(uint32_t &value);
};

