/******************************************************************************
 * @file entropy_src_test.h
 * @brief entropy_src test harness class
 *
 * Extends entropy_src_basetest with 32-bit and 8-bit TLM register access
 * helpers.  All helpers issue transactions through
 * entropy_src_basetest::initiator_socket, which the testbench binds to
 * entropy_src::reg_socket.
 *
 * A single sc_in<bool> port (intr_i) is declared to observe the combined
 * interrupt output driven by the DUT.
 *
 * Reference:
 *   - entropy_src/docs/sections/entropy_src-port-interfaces.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include "entropy_src_basetest.h"
#include "csml_logger.h"

#include <cstdint>

/******************************************************************************
 * @class entropy_src_test
 * @brief entropy_src test harness providing register access helpers
 *
 * Inherits the initiator_socket and register property enumerations from
 * entropy_src_basetest and adds:
 *   - register_read_32 / register_write_32 for 32-bit word accesses
 *   - register_read_8  / register_write_8  for 8-bit byte accesses
 *   - A single sc_in<bool> port to observe the DUT combined interrupt output
 *
 * The testbench binds initiator_socket to entropy_src::reg_socket and
 * connects the sc_in<bool> port to an sc_signal<bool> wire that is
 * also driven by the DUT's sc_out<bool> intr_o port.
 ******************************************************************************/
class entropy_src_test : public entropy_src_basetest
{
public:
    // =========================================================================
    // Interrupt observation port
    // =========================================================================

    /**
     * @brief Receives the combined interrupt output (intr_o) from the DUT.
     *
     * Bound by the testbench to sig_intr which is also driven by
     * entropy_src_ip::intr_o.
     */
    sc_core::sc_in<bool> intr_i;

    // =========================================================================
    // Constructor / Destructor
    // =========================================================================

    /**
     * @brief Constructor
     *
     * Initialises all port names and the CSML logger.
     *
     * @param name SystemC module name
     */
    entropy_src_test(sc_module_name name)
        : entropy_src_basetest(name)
        , intr_i("intr_i")
        , logger()
    {
        logger.setMaxVerbosity(2);
        logger.setLogFormat(
            "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);
    }

    /// @brief Destructor
    ~entropy_src_test() {}

    // =========================================================================
    // Register access helpers
    // =========================================================================

    /**
     * @brief 32-bit register read via TLM b_transport (data_length = 4)
     *
     * Constructs a TLM_READ_COMMAND payload addressed to @p offset with a
     * 4-byte data buffer and issues it through initiator_socket->b_transport.
     *
     * @param offset     Byte offset of the target register within the DUT
     *                   address space (use Register_offset enum values)
     * @param read_value Reference to receive the 32-bit register value
     */
    void register_read_32(unsigned int offset, uint32_t& read_value);

    /**
     * @brief 32-bit register write via TLM b_transport (data_length = 4)
     *
     * Constructs a TLM_WRITE_COMMAND payload addressed to @p offset with a
     * 4-byte data buffer containing @p write_value and issues it through
     * initiator_socket->b_transport.
     *
     * @param offset      Byte offset of the target register within the DUT
     *                    address space (use Register_offset enum values)
     * @param write_value 32-bit value to write
     */
    void register_write_32(unsigned int offset, uint32_t write_value);

    /**
     * @brief 8-bit register read via TLM b_transport (data_length = 1)
     *
     * Constructs a TLM_READ_COMMAND payload with a 1-byte data buffer and
     * issues it through initiator_socket->b_transport.
     *
     * @param offset     Byte offset within the DUT address space
     * @param read_value Reference to receive the 8-bit value
     */
    void register_read_8(unsigned int offset, uint8_t& read_value);

    /**
     * @brief 8-bit register write via TLM b_transport (data_length = 1)
     *
     * Constructs a TLM_WRITE_COMMAND payload with a 1-byte data buffer and
     * issues it through initiator_socket->b_transport.
     *
     * @param offset      Byte offset within the DUT address space
     * @param write_value 8-bit value to write
     */
    void register_write_8(unsigned int offset, uint8_t write_value);

private:
    /// CSML logger for test harness diagnostics
    CsmlLogger logger;
};
