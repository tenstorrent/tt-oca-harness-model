// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <tlm_utils/simple_target_socket.h>
#include <systemc>
#include <iostream>
#include <cstring>

// Stdout device — memory-mapped debug output sink.
// Single-byte writes print characters; 4-byte writes handle test completion
// magic sequences and the LOAD_NMI_ADDR (0x81) mailbox command.
class stdout_device : public sc_module {
public:
    tlm_utils::simple_target_socket<stdout_device> sock;
    sc_core::sc_out<uint32_t> nmi_vec_o;

    stdout_device(sc_module_name name)
        : sc_module(name), sock("sock"), nmi_vec_o("nmi_vec_o"), last_word_(0) {
        sock.register_b_transport(this, &stdout_device::b_transport);
        sock.register_transport_dbg(this, &stdout_device::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        (void)delay;
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
            handle_write(trans.get_data_ptr(), trans.get_data_length());
    }

    unsigned transport_dbg(tlm::tlm_generic_payload& trans) {
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
            handle_write(trans.get_data_ptr(), trans.get_data_length());
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return trans.get_data_length();
    }

private:
    uint32_t last_word_;

    void handle_write(uint8_t* ptr, unsigned len) {
        if (len == 1) {
            std::cout << *reinterpret_cast<char*>(ptr);
            std::cout.flush();
            last_word_ = static_cast<uint8_t>(*ptr);
        } else if (len == 4) {
            uint32_t value = *reinterpret_cast<uint32_t*>(ptr);
            if (last_word_ == 0xA5A55A5Au) {
                if (value == 0xCAFEBABEu) {
                    std::cout << "\n[VP] SIMULATION OF THE TEST PASSED\n";
                    std::cout.flush();
                    last_word_ = 0;
                    return;
                } else if (value == 0xDEADBEEFu) {
                    std::cout << "\n[VP] SIMULATION OF THE TEST FAILED\n";
                    std::cout.flush();
                    last_word_ = 0;
                    return;
                }
            }
            last_word_ = value;
            if ((value & 0xFF) == 0x81u) {
                nmi_vec_o.write(value & 0xFFFFFF00u);
            } else if (value != 0xA5A55A5Au) {
                std::cout << static_cast<char>(value & 0xFF);
                std::cout.flush();
            }
        }
    }
};

// Reset generation unit. rst_ni covers POR and watchdog reset; cold_rst_ni
// covers POR only, matching sep_reset_n versus sep_cpu_reset_n in RTL.
class reset_generation_unit : public sc_module {
public:
    sc_out<bool> rst_ni;
    sc_out<bool> cold_rst_ni;
    sc_in<bool>  reset_req_i;

    SC_HAS_PROCESS(reset_generation_unit);

    // Reset is already asserted when simulation starts. This avoids an ISS
    // execution window before the first delta-delayed falling edge.
    void end_of_elaboration() { rst_ni.write(false); cold_rst_ni.write(false); }

    reset_generation_unit(sc_module_name name)
        : sc_module(name), rst_ni("rst_ni"), cold_rst_ni("cold_rst_ni"),
          reset_req_i("reset_req_i") {
        SC_THREAD(por_thread);
        SC_THREAD(external_reset_thread);
        SC_METHOD(external_reset_method);
        sensitive << reset_req_i;
        dont_initialize();
    }

private:
    static constexpr int RESET_HOLD_NS = 10;
    const sc_core::sc_time RESET_HOLD{RESET_HOLD_NS, sc_core::SC_NS};

    sc_event start_monitor_ev_;
    sc_event external_reset_ev_;
    bool     external_reset_in_progress_ = false;

    void do_reset_pulse(bool cold) {
        std::cout << "[" << sc_core::sc_time_stamp()
                  << "] reset_generation_unit: issuing "
                  << (cold ? "cold" : "warm") << " reset pulse\n";
        rst_ni->write(false);
        if (cold)
            cold_rst_ni->write(false);
        wait(RESET_HOLD);
        rst_ni->write(true);
        if (cold)
            cold_rst_ni->write(true);
    }

    void por_thread() {
        wait(RESET_HOLD);
        rst_ni->write(true);
        cold_rst_ni->write(true);
        if (reset_req_i.read())
            wait(reset_req_i.negedge_event());
        start_monitor_ev_.notify();
    }

    void external_reset_method() {
        if (reset_req_i.read())
            external_reset_ev_.notify(SC_ZERO_TIME);
    }

    void external_reset_thread() {
        wait(start_monitor_ev_);
        while (true) {
            wait(external_reset_ev_);
            if (external_reset_in_progress_ || !reset_req_i.read())
                continue;
            external_reset_in_progress_ = true;
            std::cout << "[" << sc_core::sc_time_stamp()
                      << "] reset_generation_unit: external reset request asserted\n";
            do_reset_pulse(false);
            wait(reset_req_i.negedge_event());
            external_reset_in_progress_ = false;
        }
    }
};
