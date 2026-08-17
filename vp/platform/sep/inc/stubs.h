#pragma once

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <systemc>
#include <cstring>
#include "otbn_interfaces.h"
#include "sep_memory.h"

// Host loopback stub for the OT Mailbox IP (Port 1, 64-bit side).
// Polls the STATUS register and echoes any received message back via WRITE_DATA.
struct mailbox_host_stub : public sc_core::sc_module {
    tlm_utils::simple_initiator_socket<mailbox_host_stub> isock;

    SC_HAS_PROCESS(mailbox_host_stub);

    mailbox_host_stub(sc_core::sc_module_name name) : sc_module(name), isock("isock") {
        SC_THREAD(run);
    }

    void run() {
        while (true) {
            uint64_t status = 0;
            read64(0x10, status);
            if ((status & 1) == 0) {
                uint64_t data = 0;
                read64(0x08, data);
                write64(0x00, data);
            }
            sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
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

// OTBN OTP key request stub — no-op; OTBN never receives a real scramble key in VP.
class otp_key_req_stub : public otp_key_req_if, public sc_module {
public:
    SC_HAS_PROCESS(otp_key_req_stub);
    otp_key_req_stub(sc_module_name name) : sc_module(name) {}
    void request_scramble_key() override {}
};

// OTBN OTP key response stub — returns dummy zero keys; always reports key available.
class otp_key_rsp_stub : public otp_key_rsp_if, public sc_module {
public:
    SC_HAS_PROCESS(otp_key_rsp_stub);
    otp_key_rsp_stub(sc_module_name name) : sc_module(name) {}
    bool key_available() override { return true; }
    void get_scramble_key(uint32_t key[4], uint32_t& nonce, uint32_t& seed) override {
        key[0] = key[1] = key[2] = key[3] = 0;
        nonce = 0;
        seed  = 0;
    }
};

// Output remap stub — silently accepts transactions from remapped_socket.
// AP/STEE output remap destinations are external to the VP address space
// (AP-side DDR), so a stub is the correct VP target.
class remap_output_stub : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<remap_output_stub> socket;

    explicit remap_output_stub(sc_core::sc_module_name n)
        : sc_module(n), socket("socket") {
        socket.register_b_transport(this,   &remap_output_stub::b_transport);
        socket.register_transport_dbg(this, &remap_output_stub::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        delay = sc_core::SC_ZERO_TIME;
        if (trans.is_read()) {
            unsigned char* p = trans.get_data_ptr();
            if (p && trans.get_data_length() > 0)
                std::memset(p, 0, trans.get_data_length());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        if (trans.is_read()) {
            unsigned char* p = trans.get_data_ptr();
            if (p && trans.get_data_length() > 0)
                std::memset(p, 0, trans.get_data_length());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }
};