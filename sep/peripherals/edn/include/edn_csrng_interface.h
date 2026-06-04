/**
 * @file edn_csrng_interface.h
 * @brief CSRNG interface definitions for EDN SystemC TLM model
 *
 * This file defines the abstract interface classes for communication between
 * EDN and CSRNG (Cryptographically Secure Random Number Generator) modules.
 * These interfaces enable EDN to request entropy generation and receive
 * generated entropy data per NIST SP 800-90A requirements.
 *
 * Interface Types:
 * - csrng_app_if: EDN → CSRNG command interface (instantiate, generate, reseed, uninstantiate)
 * - csrng_genbits_if: CSRNG → EDN entropy data interface (128-bit genbits)
 *
 * @note These are pure virtual interfaces to be implemented by test harness.
 * @note Model uses sc_port, test uses sc_export for proper port binding.
 */

#pragma once
#include <systemc.h>
#include <cstdint>

/**
 * @class csrng_app_if
 * @brief Abstract interface for CSRNG application commands
 *
 * EDN acts as initiator (master) to CSRNG, sending commands and receiving
 * acknowledgments. Command types conform to NIST SP 800-90A CTR_DRBG
 * specification with specific command envelopes.
 *
 * Command Structure:
 * - 32-bit header word: [31:16]=reserved, [15:12]=flags, [11:8]=clen, [7:4]=acmd, [3:0]=cmd
 * - 0-12 additional data words (specified by clen field)
 *
 * Command Types:
 * - 0x1: Instantiate (initialize DRBG instance)
 * - 0x3: Generate (request entropy generation with glen parameter)
 * - 0x4: Reseed (refresh entropy seed)
 * - 0x5: Uninstantiate (destroy DRBG instance)
 */
class csrng_app_if : public sc_interface
{
  public:
    /**
     * @brief Send command to CSRNG application interface
     * @param cmd_data Pointer to command data array (first word is header)
     * @param num_words Number of 32-bit words in command (1 to 13)
     * @param ack_status Output parameter for CSRNG acknowledgment status (0=success, non-zero=error)
     *
     * Sends CSRNG command and blocks until acknowledgment received.
     * Command header format: cmd[3:0], acmd[7:4], clen[11:8], flags[15:12]
     *
     * Expected behavior:
     * - Parse command header to determine command type
     * - Validate clen matches num_words-1
     * - Wait for CSRNG processing (functional delay, not cycle-accurate)
     * - Return acknowledgment status code
     */
    virtual void send_command(const uint32_t* cmd_data, uint32_t num_words, uint32_t& ack_status) = 0;

    /**
     * @brief Check if CSRNG is ready to accept new command
     * @return true if CSRNG can accept command, false if busy
     *
     * Non-blocking status check for command readiness.
     */
    virtual bool is_ready() = 0;

    /**
     * @brief Get current command acknowledgment status
     * @return Status code from last command (0=success, non-zero indicates error)
     */
    virtual uint32_t get_ack_status() = 0;
};

/**
 * @class csrng_genbits_if
 * @brief Abstract interface for receiving 128-bit entropy from CSRNG
 *
 * CSRNG acts as provider of generated entropy bits (genbits) to EDN.
 * EDN receives 128-bit blocks and performs width conversion to 32-bit
 * for distribution to peripheral endpoints.
 *
 * Data Format:
 * - 128-bit entropy block delivered as 4x 32-bit words
 * - FIPS compliance indicator accompanies each block
 * - Data valid until EDN acknowledges receipt
 */
class csrng_genbits_if : public sc_interface
{
  public:
    /**
     * @brief Receive 128-bit entropy block from CSRNG
     * @param genbits Output array for 4x 32-bit words (genbits[0]=LSB, genbits[3]=MSB)
     * @param fips_compliance Output FIPS status indicator (true=FIPS compliant, false=pre-FIPS)
     *
     * Blocking call that waits for CSRNG to provide entropy data.
     * EDN buffers received data for distribution to endpoints.
     *
     * FIPS Indicator:
     * - true: Entropy meets NIST SP 800-90A FIPS 140-2/3 requirements
     * - false: Boot-time pre-FIPS seed (fast but non-compliant)
     */
    virtual void receive_genbits(uint32_t genbits[4], bool& fips_compliance) = 0;

    /**
     * @brief Check if entropy data is available
     * @return true if CSRNG has entropy ready for consumption
     *
     * Non-blocking check for data availability.
     */
    virtual bool has_data() = 0;

    /**
     * @brief Provide 128-bit entropy to EDN (test harness implementation)
     * @param genbits Input array of 4x 32-bit words
     * @param fips_compliance FIPS compliance status for this entropy block
     *
     * Test harness uses this to push entropy to EDN model.
     * Model implementation should leave this as no-op.
     */
    virtual void provide_genbits(const uint32_t genbits[4], bool fips_compliance) = 0;
};
