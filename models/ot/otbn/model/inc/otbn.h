#pragma once

/**
 * @file otbn.h
 * @brief OTBN (OpenTitan Big Number Accelerator) TLM model main header
 * 
 * This header defines the complete OTBN TLM model including:
 * - State machine enumerations and error bit constants
 * - EDN (Entropy Distribution Network) interface for algorithm random number access
 * - Algorithm base class and concrete algorithm implementations (RSA-2048, Summation)
 * - Main otbn_ip class with ports, registers, and behavioral logic
 * 
 * OTBN is a cryptographic accelerator for big number operations supporting:
 * - RSA-2048 encryption/decryption and signature operations
 * - ECDSA P-256 signing (future)
 * - X25519 key exchange (future)
 * - Custom algorithms via the otbn_algorithm base class
 * 
 * Key components:
 * - 8KB Instruction Memory (IMEM) for algorithm firmware
 * - 4KB Data Memory (DMEM) with 3KB host-accessible, 1KB protected
 * - 32 256-bit Wide Data Registers (WDR) for big number operations
 * - External interfaces: EDN (entropy), Key Manager (sideload keys), OTP, Life Cycle
 */


#include "otbn_base.h"
#include "otbn_interfaces.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <functional>
#include <cstring>
/**
 * @brief OTBN State Machine States
 * 
 * Defines the possible states of the OTBN accelerator.
 * Transitions managed by CMD register writes and operation completion.
 */
enum otbn_state_t {
    OTBN_STATE_IDLE = 0x00,                  ///< Ready for new command
    OTBN_STATE_BUSY_EXECUTE = 0x01,          ///< Executing algorithm
    OTBN_STATE_BUSY_SEC_WIPE_DMEM = 0x02,    ///< Secure wiping DMEM
    OTBN_STATE_BUSY_SEC_WIPE_IMEM = 0x03,    ///< Secure wiping IMEM
    OTBN_STATE_BUSY_SEC_WIPE_INT = 0x04,     ///< Secure wiping internal state (boot-time)
    OTBN_STATE_LOCKED = 0xFF                 ///< Terminal error state (requires reset)
};

/**
 * @brief OTBN Error Bit Constants (ERR_BITS register)
 * 
 * TLM-relevant functional errors. Excludes RTL-level errors not applicable to TLM abstraction.
 * Software errors (bits 0-7) are recoverable unless CTRL.software_errs_fatal is set.
 * Hardware/fatal errors (bits 16-23) always cause LOCKED state.
 */
namespace otbn_err_bits {
    // Software Errors (bits 0-7) - Only functional errors relevant for TLM
    constexpr uint32_t BAD_DATA_ADDR = (1 << 0);        ///< Algorithm accesses invalid DMEM address
    constexpr uint32_t KEY_INVALID = (1 << 5);           ///< Key Manager key not valid
    constexpr uint32_t RND_REP_CHK_FAIL = (1 << 6);     ///< RND repetition check fail
    constexpr uint32_t RND_FIPS_CHK_FAIL = (1 << 7);    ///< RND FIPS check fail

    // Hardware/Fatal Errors (bits 16-23) - TLM functional errors only
    constexpr uint32_t ILLEGAL_BUS_ACCESS = (1 << 21);  ///< Bus access during inappropriate state
    constexpr uint32_t LIFECYCLE_ESCALATION = (1 << 22); ///< Life Cycle escalation
    constexpr uint32_t FATAL_SOFTWARE = (1 << 23);       ///< Software/algorithm error
}

/**
 * @brief OTBN Fatal Alert Cause Constants (FATAL_ALERT_CAUSE register)
 * 
 * Indicates which fatal error triggered the fatal alert and LOCKED state.
 * Values persist until reset.
 */
namespace otbn_fatal_cause {
    constexpr uint32_t ILLEGAL_BUS_ACCESS = (1 << 5);   ///< Bit 5: Bus access violation
    constexpr uint32_t LIFECYCLE_ESCALATION = (1 << 6); ///< Bit 6: LC controller escalation
    constexpr uint32_t FATAL_SOFTWARE = (1 << 7);       ///< Bit 7: Algorithm/software error
}

/**
 * @brief EDN Interface (Pure C++ per otbn_plan.md)
 *
 * Pure C++ interface to Entropy Distribution Network.
 * Per otbn_plan.md lines 26-43: "The model implements the interface to EDN
 * (Entropy Distribution Network) as a pure C++ interface (not SystemC)."
 */
class edn_if {
public:

    //Default constructor
    edn_if() {}


     //Fills the provided buffer with N bytes of entropy from EDN.
     //The caller is responsible for allocating and freeing the buffer.
    virtual void fill_rand(unsigned char *data, size_t N) = 0;

    //Virtual destructor
    virtual ~edn_if() {}
};

/**
 * @brief OTBN Algorithm Base Class
 *
 * Abstract base class for OTBN cryptographic algorithms.
 * Algorithms can access DMEM and optionally CSR/WDR registers via callbacks.
 *
 * Per otbn_plan.md specification:
 * - Constructor takes dmem_size parameter
 * - Algorithms can register callbacks for CSR/WDR register access
 * - execute() method takes char* dmem pointer (caller manages allocation)
 * - Returns 64-bit instruction count for performance modeling
 */
class otbn_algorithm {
public:

     //Status return type for algorithm operations
    enum status_t {
        SUCCESS = 0,
        ERROR = 1
    };


    otbn_algorithm(size_t dmem_size, bool is_key_required = false) 
        : m_dmem_size(dmem_size), m_is_key_required(is_key_required)
    {
        // Initialize logger
        logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);
    }

    //Register callback for CSR register reads
    void register_csr_read_cb(std::function<status_t(uint32_t /*address*/, uint32_t* /*data*/)> fn) {
        m_csr_read_cb = fn;
    }

    //Register callback for CSR register writes
    void register_csr_write_cb(std::function<status_t(uint32_t /*address*/, uint32_t /*data*/)> fn) {
        m_csr_write_cb = fn;
    }

    //Register callback for WDR register reads
    void register_wdr_read_cb(std::function<status_t(uint32_t /*address*/, uint64_t* /*data*/)> fn) {
        m_wdr_read_cb = fn;
    }

    //Register callback for WDR register writes
    void register_wdr_write_cb(std::function<status_t(uint32_t /*address*/, uint64_t* /*data*/)> fn) {
        m_wdr_write_cb = fn;
    }

    //Register callback for checking key registration status
    void register_key_status_cb(std::function<bool()> fn) {
        m_key_status_cb = fn;
    }

    //Register callback for setting ERR_BITS register
    void register_err_bits_write_cb(std::function<void(uint32_t /*err_bits*/)> fn) {
        m_err_bits_write_cb = fn;
    }

    // Register callback for RND data reads
    void register_rnd_read_cb(std::function<status_t(uint32_t* /*data*/)> fn) {
        m_rnd_read_cb = fn;
    }

    // NEW: cycle count hook used for timing (wait(cycle_count, SC_NS))
    virtual uint64_t get_cycle_count() = 0;

    //Execute the algorithm
    virtual status_t execute(char *dmem) = 0;

    //Get instruction count from last execution
    virtual uint64_t get_instruction_count() = 0;

    //Reset the algorithm state

    virtual void reset() = 0;

    //Set output stream objects for debug/info messages
    virtual void message_objects(std::ostream &debug, std::ostream &info) = 0;

    //Virtual destructor
    virtual ~otbn_algorithm() {}

protected:
    size_t m_dmem_size;  // DMEM size in bytes (from constructor)
    bool m_is_key_required;  // Whether algorithm requires WDR key registers

    // Optional callbacks for CSR/WDR register access
    std::function<status_t(uint32_t, uint32_t*)> m_csr_read_cb;
    std::function<status_t(uint32_t, uint32_t)> m_csr_write_cb;
    std::function<status_t(uint32_t, uint64_t*)> m_wdr_read_cb;
    std::function<status_t(uint32_t, uint64_t*)> m_wdr_write_cb;
    std::function<bool()> m_key_status_cb;
    std::function<void(uint32_t)> m_err_bits_write_cb;
    std::function<status_t(uint32_t*)> m_rnd_read_cb;
    CsmlLogger logger;  // Logger instance for structured logging
};

// Forward declarations for algorithm classes
// (Full definitions are in model/algo/ headers, included in otbn.cpp)
class otbn_algorithm_rsa_2048;
class otbn_algorithm_rsa_2048_key_enabled;
class otbn_algorithm_summation;
class otbn_algorithm_otbn_loop;
class otbn_algorithm_rnd_test;
class otbn_algorithm_p256_ecdsa;


class otbn_ip : public otbn_base
{
public:
   SC_HAS_PROCESS(otbn_ip);

   // Interrupt and Alert outputs
   sc_out<bool> intr_done;
   sc_out<bool> alert_fatal;
   sc_out<bool> alert_recov;

   // Clock and Reset inputs
   sc_in<double> clk_core;
   sc_in<double> clk_edn;
   sc_in<double> clk_otp;
   sc_in<bool> rst_n;

   // EDN (Entropy Distribution Network) Interfaces
   // RND interface - for AIS31-compliant random numbers
    sc_port<edn_req_if> edn_rnd_req;
    sc_port<edn_rsp_if> edn_rnd_rsp;

   // URND interface - for local PRNG seeding
   sc_port<edn_req_if> edn_urnd_req;
   sc_export<edn_rsp_if> edn_urnd_rsp;

   /*
    * Key Manager TLM Target Socket 
    *
    * Per specification: "The model has a simple target socket that is connected to
    * an external key manager. The key manager can program the keys in the WDR
    * registers - KEY_S0_L/KEY_S0_H and KEY_S1_L/KEY_S1_H wide registers."
    *
    * Address Map (as seen by Key Manager, matches otbn_wrapper_key.rdl):
    *   0x000-0x01F: KEY_S0_L (256 bits = 32 bytes) → WDR20  share0[255:0]
    *   0x020-0x02F: KEY_S0_H (128 bits = 16 bytes) → WDR21  share0[383:256], upper 128b zero-padded
    *   0x030-0x04F: KEY_S1_L (256 bits = 32 bytes) → WDR22  share1[255:0]
    *   0x050-0x05F: KEY_S1_H (128 bits = 16 bytes) → WDR23  share1[383:256], upper 128b zero-padded
    *   0x060:       KEY_CTRL (bit 0 = KEY_VALID)
    */
   tlm_utils::simple_target_socket<otbn_ip> keymgr_tl_socket;

   // OTP Controller Interface - for scrambling keys
   sc_port<otp_key_req_if> otp_key_req;
   sc_export<otp_key_rsp_if> otp_key_rsp;

   // Life Cycle Controller Interfaces
   // Escalation interface
   sc_in<bool> lc_escalate_req;
   sc_out<bool>  lc_escalate_rsp;

   // RMA (Return Merchandise Authorization) interface
   sc_in<bool> lc_rma_req;
   sc_out<bool>  lc_rma_rsp;

   /// CSML Logger instance
   CsmlLogger logger;  ///< Logger for debug and tracing
   csml_param<int> verbosity;                   ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
   csml_param<std::string> algorithm_type;      ///< Algorithm type ("rsa_2048", "p256_ecdsa", etc.)

   /*
    * Constructor
    * @param n Module name
    * @param memory_size Total memory size in bytes (typically 0x10000 = 64KB)
    * @param algorithm_type Algorithm to use ("rsa_2048", "summation", etc.)
    * @param urnd_seed Initial seed for URND PRNG (default: 0x12345678)
    *
    * Per otbn_plan.md: URND uses C rand() with configurable srand() seed
    */
#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
   otbn_ip(sc_module_name n, unsigned int memory_size,
        unsigned int urnd_seed = 0x12345678,
        int log_verbosity = CSML_DEFAULT_VERBOSITY);
   ~otbn_ip();

   /*
    * Set EDN interface (pure C++ per otbn_plan.md)
    * @param edn_interface Pointer to EDN interface implementation
    *
    * Optional: Provides pure C++ EDN interface for RND operations.
    * If not set, uses default SystemC interface.
    */
   void set_edn_interface(edn_if* edn_interface) {
       edn_rnd_interface = edn_interface;
   }


private:
   // State machine and tracking variables
   otbn_state_t current_state;
   uint32_t insn_count_value;
   uint32_t err_bits_accumulator;
   bool operation_done;
   bool key_registered;
   
   // LOAD_CHECKSUM CRC-32-IEEE accumulator
   uint32_t load_checksum_crc;

   // URND PRNG seed (per otbn_plan.md specification)
   unsigned int urnd_prng_seed;

   // EDN interface pointer (pure C++ per otbn_plan.md)
   // This is optional - if provided, used for RND operations
   edn_if* edn_rnd_interface;

   // WDR (Wide Data Registers) storage - 256-bit registers
   // Per otbn_plan.md line 11: "The model implements 32-bit CSR and 256-bit WDR"
   // OTBN has 32 WDRs (w0-w31), each 256 bits = 32 bytes = 4 uint64_t words
   static const int NUM_WDR_REGISTERS = 32;
   uint64_t wdr_registers[NUM_WDR_REGISTERS][4];  // [register_index][word_index]

   // Key Manager sideload keys (mapped to specific WDRs)
   // KEY_S0_L = WDR20, KEY_S0_H = WDR21, KEY_S1_L = WDR22, KEY_S1_H = WDR23
   static const int WDR_KEY_S0_L = 20;
   static const int WDR_KEY_S0_H = 21;
   static const int WDR_KEY_S1_L = 22;
   static const int WDR_KEY_S1_H = 23;

   // Pending output states for deferred updates (to avoid multiple driver conflicts)
   bool pending_intr_done;
   bool pending_alert_fatal;
   bool pending_alert_recov;

   // Events for deferred updates
   sc_event interrupt_update_event;
   sc_event alert_update_event;
   sc_event lc_escalate_event;  // Event triggered when lc_escalate_req is asserted
  

   // Current algorithm instance
   otbn_algorithm* current_algorithm;

   // CRC-32-IEEE calculation helper
   uint32_t crc32_ieee_update(uint32_t crc, uint32_t data, bool is_imem, uint16_t word_index);

    // Interface channel implementations (to bind sc_exports)
    // class edn_rnd_rsp_channel; // Removed: edn_rnd_rsp is now a port
    class edn_urnd_rsp_channel;
   class otp_key_rsp_channel;
   // class lc_escalate_rsp_channel; // Removed - lc_escalate_rsp is now sc_out<bool>
   // class lc_rma_rsp_channel; // Removed - lc_rma_rsp is now sc_out<bool>

    // edn_rnd_rsp_channel* edn_rnd_rsp_impl; // Removed
    edn_urnd_rsp_channel* edn_urnd_rsp_impl;
   otp_key_rsp_channel* otp_key_rsp_impl;
   // lc_escalate_rsp_channel* lc_escalate_rsp_impl; // Removed - lc_escalate_rsp is now sc_out<bool>
   // lc_rma_rsp_channel* lc_rma_rsp_impl; // Removed - lc_rma_rsp is now sc_out<bool>

   // Register write callbacks
   bool cmd_write_callback(uint32_t value);
   bool intr_state_write_callback(uint32_t value);
   bool intr_test_write_callback(uint32_t value);
   bool alert_test_write_callback(uint32_t value);
   bool err_bits_write_callback(uint32_t value);

   // Register read callbacks
   bool status_read_callback(uint32_t& value);
   bool insn_cnt_read_callback(uint32_t& value);
   bool err_bits_read_callback(uint32_t& value);
   bool fatal_alert_cause_read_callback(uint32_t& value);
   bool load_checksum_read_callback(uint32_t& value);

   // Memory callbacks
   bool imem_write_callback(uint32_t value, uint32_t index);
   bool imem_read_callback(uint32_t& value, uint32_t index);
   bool dmem_write_callback(uint32_t value, uint32_t index);
   bool dmem_read_callback(uint32_t& value, uint32_t index);

   // CSR/WDR callback handlers for algorithm access
   // These are registered with the algorithm to allow register access during execution
   otbn_algorithm::status_t csr_read_handler(uint32_t address, uint32_t* data);
   otbn_algorithm::status_t csr_write_handler(uint32_t address, uint32_t data);
    otbn_algorithm::status_t wdr_read_handler(uint32_t address, uint64_t* data);
    otbn_algorithm::status_t wdr_write_handler(uint32_t address, uint64_t* data);
    otbn_algorithm::status_t rnd_read_handler(uint32_t* data);

   // Key Manager TLM transport handler (per otbn_plan.md line 21)
   void keymgr_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);

   // Helper functions
   void select_algorithm(const std::string& algo_name);
   void execute_algorithm();
   void secure_wipe_dmem();
   void secure_wipe_imem();
   void internal_secure_wipe();

   // Reset handler
   void reset_handler();

   // Life Cycle Controller monitoring thread
   void lc_monitor_thread();
   
   // Method to monitor lc_escalate_req and trigger lc_escalate_event
   void lc_escalate_monitor_method();

   // Deferred update methods (SC_METHOD to avoid multiple driver conflicts)
   void request_interrupt_update();
   void interrupt_update_method();
   void request_alert_update();
   void alert_update_method();
};

