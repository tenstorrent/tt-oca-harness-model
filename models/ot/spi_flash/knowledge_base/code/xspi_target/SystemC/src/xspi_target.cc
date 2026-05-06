/***************************************************************************
 * Copyright 1996-2024 Synopsys, Inc.
 *
 * This Synopsys software and all associated documentation are proprietary
 * to Synopsys, Inc. and may only be used pursuant to the terms and
 * conditions of a written license agreement with Synopsys, Inc.
 * All other use, reproduction, modification, or distribution of the
 * Synopsys software or the associated documentation is strictly prohibited.
 ***************************************************************************/
 

/***************************************************************************
 * Generated snippet, used for detecting user edits.
 * CHECKSUM:735ba6e448c31e26229f6e3820fa2e9005b6d3ee
 ***************************************************************************/
 
#include "xspi_target.h"


xspi_target::xspi_target(sc_core::sc_module_name name) : xspi_targetBase(name) {
    // Use Base's scml2::memory for all model read/write (no internal vector)
    xspi_device_model.set_mem(&mem);
}

void xspi_target::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
   // Extract opcode and address from transaction
   uint8_t opcode;
   uint32_t address;

   xspi_target_trans* target_trans = trans.get_extension<xspi_target_trans>();

   if (target_trans == nullptr) {
		   std::cerr << "[xspi_target] Error: xspi_target_trans extension not found in transaction" << std::endl;
		   trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		   return;
   }

   address = static_cast<uint32_t>(trans.get_address());
   opcode = target_trans->xspi_target_opcode;

   // Prepare buffers
   std::vector<uint8_t> rx_buffer;
   std::vector<uint8_t> tx_buffer;

   // Handle TLM_READ_COMMAND (read operations)
   if (trans.get_command() == tlm::TLM_READ_COMMAND) {

	   // Pre-size rx_buffer for read operations
	   rx_buffer.resize(trans.get_data_length());

	   // Process command
	   bool success = xspi_device_model.process_command(opcode, address, rx_buffer);

	   if (success) {
		   // Copy read data back to TLM payload
		   if (rx_buffer.size() <= trans.get_data_length()) {
			   memcpy(trans.get_data_ptr(), rx_buffer.data(), rx_buffer.size());
			   trans.set_data_length(rx_buffer.size());
			   trans.set_response_status(tlm::TLM_OK_RESPONSE);
		   } else {
			   trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		   }
	   } else {
		   trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
	   }
   }
   // Handle TLM_WRITE_COMMAND (write operations)
   else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
	   // Extract write data from TLM payload
	   tx_buffer.assign(trans.get_data_ptr(),
						trans.get_data_ptr() + trans.get_data_length());

	   // Process command
	   bool success = xspi_device_model.process_command(opcode, address, rx_buffer, tx_buffer);

	   // Set response status
	   if (success) {
		   trans.set_response_status(tlm::TLM_OK_RESPONSE);
	   } else {
		   trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
	   }
   }
   else {
	   trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
   }

   // LT Model: Set delay to zero (no timing simulation)
   delay = sc_core::SC_ZERO_TIME;
}


