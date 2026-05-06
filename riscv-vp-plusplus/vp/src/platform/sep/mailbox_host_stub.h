#pragma once

#include <tlm_utils/simple_initiator_socket.h>
#include <systemc>

/**
 * @brief Host loopback stub for the OT Mailbox IP (Port 1, 64-bit side).
 *
 * This module connects to Port 1 of the mailbox and runs a simple thread
 * that polls the STATUS register. When it sees that the read FIFO is not
 * empty (STATUS.empty == 0), it reads a 64-bit message from READ_DATA and
 * echoes it back by writing it directly into WRITE_DATA.
 */
struct mailbox_host_stub : public sc_core::sc_module {
    tlm_utils::simple_initiator_socket<mailbox_host_stub> isock;

    SC_HAS_PROCESS(mailbox_host_stub);

    mailbox_host_stub(sc_core::sc_module_name name) : sc_module(name), isock("isock") {
        SC_THREAD(run);
    }

    void run() {
        while (true) {
            // Poll STATUS register (offset 0x10)
            uint64_t status = 0;
            read64(0x10, status);
            
            // STATUS bit 0 is 'empty'. If 0, data is available.
            if ((status & 1) == 0) {
                // Read from Port 1 READ_DATA (offset 0x08)
                uint64_t data = 0;
                read64(0x08, data);
                
                // Echo perfectly back to Port 1 WRITE_DATA (offset 0x00)
                write64(0x00, data);
            }
            
            // Wait to yield SystemC scheduler and limit polling frequency
            sc_core::sc_time delay(1, sc_core::SC_US);
            sc_core::wait(delay);
        }
    }

private:
    void read64(uint64_t addr, uint64_t& val) {
        tlm::tlm_generic_payload txn;
        txn.set_command(tlm::TLM_READ_COMMAND);
        txn.set_address(addr);
        txn.set_data_ptr(reinterpret_cast<unsigned char*>(&val));
        txn.set_data_length(8);
        txn.set_streaming_width(8);
        txn.set_byte_enable_ptr(nullptr);
        txn.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        isock->b_transport(txn, delay);
    }

    void write64(uint64_t addr, uint64_t val) {
        tlm::tlm_generic_payload txn;
        txn.set_command(tlm::TLM_WRITE_COMMAND);
        txn.set_address(addr);
        txn.set_data_ptr(reinterpret_cast<unsigned char*>(&val));
        txn.set_data_length(8);
        txn.set_streaming_width(8);
        txn.set_byte_enable_ptr(nullptr);
        txn.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        isock->b_transport(txn, delay);
    }
};
