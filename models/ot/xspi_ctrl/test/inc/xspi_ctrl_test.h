/******************************************************************************
 * @file xspi_ctrl_test.h
 * @brief xspi_ctrl test harness class
 *
 * Extends xspi_ctrl_basetest with register access helpers for 8-bit and
 * 32-bit TLM transactions, following the same pattern as dma_test.h.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once
#include "xspi_ctrl_basetest.h"
#include "csml_logger.h"

/**
 * @class xspi_ctrl_test
 * @brief xspi_ctrl test harness providing register read/write helpers
 *
 * All helpers issue TLM b_transport transactions through initiator_socket,
 * which is bound to the DUT's target_socket by the testbench.
 */
class xspi_ctrl_test : public xspi_ctrl_basetest
{
public:
    /**
     * @brief Constructor
     * @param name SystemC module name
     */
    xspi_ctrl_test(sc_module_name name) : xspi_ctrl_basetest(name), logger()
    {
        logger.setMaxVerbosity(2);
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);
    }

    ~xspi_ctrl_test() {}

    /**
     * @brief 32-bit register read via TLM b_transport (data_length=4)
     * @param offset   Byte offset within the DUT address space
     * @param read_value Reference to receive the 32-bit value
     */
    void register_read_32(unsigned int offset, uint32_t& read_value);

    /**
     * @brief 32-bit register write via TLM b_transport (data_length=4)
     * @param offset      Byte offset within the DUT address space
     * @param write_value 32-bit value to write
     */
    void register_write_32(unsigned int offset, uint32_t write_value);

    /**
     * @brief 8-bit register read via TLM b_transport (data_length=1)
     * @param offset     Byte offset within the DUT address space
     * @param read_value Reference to receive the 8-bit value
     */
    void register_read_8(unsigned int offset, uint8_t& read_value);

    /**
     * @brief 8-bit register write via TLM b_transport (data_length=1)
     * @param offset      Byte offset within the DUT address space
     * @param write_value 8-bit value to write
     */
    void register_write_8(unsigned int offset, uint8_t write_value);

private:
    /// CSML logger for test diagnostics
    CsmlLogger logger;
};
