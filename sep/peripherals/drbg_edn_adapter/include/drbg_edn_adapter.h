// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
//
// Cycle-accurate model of hw/ip/drbg/rtl/drbg_axis_edn_adapter.sv.
// A 32-bit AXI-Stream entropy beat is staged (depth 4) and dealt, round-robin,
// to native EDN endpoints. clear_i flushes the stage and every endpoint.
// endpoint_cancel_i[i] flushes only endpoint i and keeps it out of arbitration
// through the cycle after cancel drops (ep_flush_q).

#pragma once

#include <systemc>
#include <cci_configuration>

#include <cstdint>
#include <deque>
#include <vector>

namespace sep {

class drbg_edn_adapter : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> clk_i{"clk_i"};
    sc_core::sc_in<bool> rst_ni{"rst_ni"};
    sc_core::sc_in<bool> clear_i{"clear_i"};

    sc_core::sc_vector<sc_core::sc_in<bool>> endpoint_cancel_i{"endpoint_cancel_i"};
    sc_core::sc_in<bool>     tvalid_i{"tvalid_i"};
    sc_core::sc_in<uint32_t> tdata_i{"tdata_i"};
    sc_core::sc_in<uint32_t> tstrb_i{"tstrb_i"};
    sc_core::sc_in<bool>     tuser_i{"tuser_i"};
    sc_core::sc_out<bool>    tready_o{"tready_o"};

    sc_core::sc_vector<sc_core::sc_in<bool>>      edn_req_i{"edn_req_i"};
    sc_core::sc_vector<sc_core::sc_out<bool>>     edn_ack_o{"edn_ack_o"};
    sc_core::sc_vector<sc_core::sc_out<uint32_t>> edn_bus_o{"edn_bus_o"};
    sc_core::sc_vector<sc_core::sc_out<bool>>     edn_fips_o{"edn_fips_o"};

    SC_HAS_PROCESS(drbg_edn_adapter);

    explicit drbg_edn_adapter(sc_core::sc_module_name name);

    unsigned num_endpoints() const { return n_; }

private:
    static constexpr unsigned kStageDepth = 4;

    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_endpoints_p_;

    enum class ack_st { disabled, ep_clear, idle, data_wait, ack };

    struct beat {
        uint32_t data = 0;
        bool     fips = false;
    };
    struct endpoint {
        bool   flush_q = true;
        bool   full    = false;
        beat   held{};
        ack_st st = ack_st::disabled;
    };

    unsigned            n_ = 4;
    bool                under_rst_ = true;
    std::deque<beat>    stage_;
    std::vector<endpoint> ep_;
    std::vector<bool>   mask_;

    void on_clk();
    void on_rst();
    void drive_combo();
    void reset_state();
    void step();

    sc_core::sc_event state_ev_;
};

}  // namespace sep
