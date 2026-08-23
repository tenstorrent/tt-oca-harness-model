#pragma once

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <systemc>
#include <cstring>
#include "otbn_interfaces.h"
#include "sep_memory.h"

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

// Tied-off manager port — errors every transaction.
//
// sep_dma_wrap grounds the DMA's SYS port (.sys_i('0)) and stubs its CTN
// response channel so d_valid never asserts, so only the OT-internal port is
// usable from SEP. Silicon would hang on a transfer programmed with ASID 0x9
// or 0xA; a VP that hangs is useless for debug, so this reports a bus error
// instead, which surfaces as ERROR_CODE.bus_error and stops the transfer.
// Models a manager port that SEP leaves tied off (the DMA's CTN and SYS legs).
// sep_dma_wrap.sv grounds sys_i and stubs ctn_tl_d2h with d_valid low, so in
// hardware these transactions never complete and the DMA stalls. b_transport
// cannot express "never responds" without hanging the kernel, so this errors
// instead: the transfer still fails, just visibly rather than by wedging.
class dead_manager_port_stub : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<dead_manager_port_stub> socket;

    explicit dead_manager_port_stub(sc_core::sc_module_name n)
        : sc_module(n), socket("socket") {
        socket.register_b_transport(this,   &dead_manager_port_stub::b_transport);
        socket.register_transport_dbg(this, &dead_manager_port_stub::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        delay = sc_core::SC_ZERO_TIME;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    }
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return 0;
    }
};