/*
 * xspi_target_sc_wrapper.h
 *
 *  Created on: Jan 21, 2026
 *      Author: ctr-sdangi
 */

#ifndef TESTS_UNITTESTS_XSPI_TARGET_SC_WRAPPER_H_
#define TESTS_UNITTESTS_XSPI_TARGET_SC_WRAPPER_H_


#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include "xspi_target.h"
#include "SystemC/include/cdns_extension.h"
/**
 * @class xspi_target_sc_module
 * @brief SystemC Module Wrapper for XSPI Target Model
 *
 * This class provides a SystemC module interface for the XSPI target model,
 * allowing it to be integrated with SystemC testbenches via TLM2.0.
 */
class xspi_target_sc_module : public sc_core::sc_module {
public:
    /** @brief TLM Target Socket (64-bit data width) */
    tlm_utils::simple_target_socket<xspi_target_sc_module, 64> target_socket;

    /**
     * @brief Constructor
     * @param name SystemC module name
     */
    SC_HAS_PROCESS(xspi_target_sc_module);
    xspi_target_sc_module(sc_core::sc_module_name name);

    /**
     * @brief TLM b_transport callback
     *
     * Called by the initiator socket to send transactions to this target.
     * Extracts opcode and address from the transaction and calls process_command().
     *
     * @param trans TLM generic payload transaction
     * @param delay Time delay (ignored in LT model)
     */
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

private:
    /** @brief Instance of the XSPI target model */
    xspi_target_model target_model;



};








#endif /* TESTS_UNITTESTS_XSPI_TARGET_SC_WRAPPER_H_ */
