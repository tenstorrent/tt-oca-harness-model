// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include "edn_endpoint_if.h"
#include "reg_param.h"

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <deque>

class sep_entropy_pool_ip : public sc_core::sc_module
{
public:
    static constexpr uint64_t APERTURE_SIZE = 0x1'0000u;
    static constexpr uint64_t STATUS_OFFSET = 0x00u;
    static constexpr uint64_t IRQ_CAUSE_OFFSET = 0x08u;
    static constexpr uint64_t DATA_OFFSET = 0x10u;
    static constexpr unsigned FIFO_DEPTH = 32u;
    static constexpr unsigned LOW_WATERMARK = 8u;
    static constexpr unsigned POOL_ENDPOINT_ID = 2u;

    tlm_utils::simple_target_socket<sep_entropy_pool_ip> reg_socket{"reg_socket"};
    sc_core::sc_port<edn_endpoint_if, 1, sc_core::SC_ZERO_OR_MORE_BOUND>
        entropy_source{"entropy_source"};

    sc_core::sc_in<bool> rst_ni{"rst_ni"};
    sc_core::sc_out<bool> pool_low_o{"pool_low_o"};
    sc_core::sc_out<bool> fill_stall_o{"fill_stall_o"};
    sc_core::sc_out<bool> pool_error_o{"pool_error_o"};

    regmodel::Param<double> access_delay_ns_p_;
    regmodel::Param<double> fill_stall_ns_p_;

    SC_HAS_PROCESS(sep_entropy_pool_ip);
    explicit sep_entropy_pool_ip(sc_core::sc_module_name name);

    // Labeled integration/test hook corresponding to one native EDN ack.
    // Production platform traffic normally arrives through entropy_source.
    bool feed_word32(uint32_t word, bool fips);

    unsigned fifo_level() const { return static_cast<unsigned>(fifo_.size()); }

private:
    std::deque<uint64_t> fifo_;
    uint32_t packer_lo_ = 0;
    bool packer_half_full_ = false;
    bool source_armed_ = false;
    bool source_waiting_ = false;
    bool fill_stall_ = false;
    bool pool_error_ = false;

    sc_core::sc_event data_consumed_event_;
    sc_core::sc_event stall_timeout_event_;
    sc_core::sc_event output_update_event_;

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& trans);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload&, tlm::tlm_dmi&);

    void reset_process();
    void feeder_thread();
    void stall_timeout_process();
    void update_outputs();

    void clear_entropy_path();
    void arm_stall_timer();
    void cancel_stall_timer();
    uint64_t status_word() const;
    uint64_t irq_cause_word() const;
    bool valid_read_payload(const tlm::tlm_generic_payload& trans) const;
};
