// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#include "drbg_edn_adapter.h"

#include "sim_log.h"

namespace sep {

drbg_edn_adapter::drbg_edn_adapter(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , num_endpoints_p_("num_endpoints", 4u,
                       "Native EDN clients (1..32). Sizes the cancel and req/ack ports.")
    , n_(num_endpoints_p_.get_value())
{
    if (n_ < 1u || n_ > 32u) {
        SC_REPORT_FATAL(name, "num_endpoints must be in 1..32");
    }
    endpoint_cancel_i.init(n_);
    edn_req_i.init(n_);
    edn_ack_o.init(n_);
    edn_bus_o.init(n_);
    edn_fips_o.init(n_);
    ep_.assign(n_, endpoint{});
    mask_.assign(n_, false);

    SC_METHOD(on_clk);
    sensitive << clk_i.pos();
    dont_initialize();

    SC_METHOD(on_rst);
    sensitive << rst_ni.neg();
    dont_initialize();

    // Single writer for every output. Clock and reset only update state.
    SC_METHOD(drive_combo);
    sensitive << clear_i << tstrb_i << rst_ni << state_ev_;
    for (unsigned i = 0; i < n_; ++i)
        sensitive << endpoint_cancel_i[i];
    dont_initialize();

    SIM_LOG_INFO(this, "drbg_edn_adapter endpoints=" << n_
                 << " stage_depth=" << kStageDepth);
}

void drbg_edn_adapter::on_clk()
{
    if (!rst_ni.read())
        reset_state();
    else
        step();
    state_ev_.notify(sc_core::SC_ZERO_TIME);
}

void drbg_edn_adapter::on_rst()
{
    if (!rst_ni.read()) {
        reset_state();
        state_ev_.notify(sc_core::SC_ZERO_TIME);
    }
}

void drbg_edn_adapter::reset_state()
{
    stage_.clear();
    under_rst_ = true;
    for (unsigned i = 0; i < n_; ++i) {
        ep_[i] = endpoint{};
        mask_[i] = false;
    }
}

void drbg_edn_adapter::step()
{
    const bool clear   = clear_i.read();
    const bool strobes = (tstrb_i.read() & 0xFu) == 0xFu;
    const bool stage_full = stage_.size() >= kStageDepth;
    const bool tready_now = !clear && !stage_full && strobes;
    const bool push = tvalid_i.read() && tready_now && !under_rst_;

    std::vector<bool> flush(n_);
    std::vector<bool> live(n_);
    std::vector<bool> req(n_);
    bool any_req = false;
    for (unsigned i = 0; i < n_; ++i) {
        flush[i] = clear || endpoint_cancel_i[i].read();
        live[i]  = !flush[i] && !ep_[i].flush_q;
        req[i]   = live[i] && edn_req_i[i].read() && !ep_[i].full;
        any_req  = any_req || req[i];
    }

    bool any_masked = false;
    for (unsigned i = 0; i < n_; ++i)
        any_masked = any_masked || (mask_[i] && req[i]);

    int winner = -1;
    if (any_masked) {
        for (unsigned i = 0; i < n_; ++i) {
            if (mask_[i] && req[i]) { winner = static_cast<int>(i); break; }
        }
    } else {
        for (unsigned i = 0; i < n_; ++i) {
            if (req[i]) { winner = static_cast<int>(i); break; }
        }
    }

    std::vector<bool> arb_in(n_);
    for (unsigned i = 0; i < n_; ++i)
        arb_in[i] = any_masked ? (mask_[i] && req[i]) : req[i];
    std::vector<bool> ppc(n_);
    bool acc = false;
    for (unsigned i = 0; i < n_; ++i) {
        acc = acc || arb_in[i];
        ppc[i] = acc;
    }

    const bool stage_rvalid = !stage_.empty();
    const bool ready = stage_rvalid;
    if (any_req && ready) {
        mask_[0] = false;
        for (unsigned i = 1; i < n_; ++i)
            mask_[i] = ppc[i - 1];
    } else if (any_req && !ready) {
        mask_ = ppc;
    }

    const bool grant = (winner >= 0) && ready;
    const bool stage_pop = any_req && stage_rvalid;
    const beat staged = stage_rvalid ? stage_.front() : beat{};

    for (unsigned i = 0; i < n_; ++i) {
        const bool enable = !flush[i];
        const bool sm_req = edn_req_i[i].read() && live[i];
        bool fifo_pop = false;
        bool fifo_clr = false;
        ack_st next = ep_[i].st;
        switch (ep_[i].st) {
        case ack_st::disabled:
            if (enable) {
                next = ack_st::ep_clear;
                fifo_clr = true;
            }
            break;
        case ack_st::ep_clear:
            next = ack_st::idle;
            break;
        case ack_st::idle:
            if (sm_req) {
                if (ep_[i].full) fifo_pop = true;
                next = ack_st::data_wait;
            }
            break;
        case ack_st::data_wait:
            if (ep_[i].full) next = ack_st::ack;
            break;
        case ack_st::ack:
            next = ack_st::idle;
            break;
        }
        if (!enable && ep_[i].st != ack_st::disabled) {
            next = ack_st::disabled;
            fifo_pop = false;
            fifo_clr = false;
        }

        const bool ep_push = live[i] && stage_pop && grant && winner == static_cast<int>(i);
        const bool clr = fifo_clr || flush[i];
        const bool wready = !ep_[i].full;
        const bool rvalid = ep_[i].full;
        const bool full_d = (rvalid ? !fifo_pop : ep_push) && !clr;
        if (ep_push && wready)
            ep_[i].held = staged;
        ep_[i].full = full_d;
        ep_[i].st = next;
        ep_[i].flush_q = flush[i];
    }

    if (clear) {
        stage_.clear();
    } else {
        if (stage_pop && !stage_.empty())
            stage_.pop_front();
        if (push && stage_.size() < kStageDepth)
            stage_.push_back(beat{tdata_i.read(), tuser_i.read()});
    }
    under_rst_ = false;
}

void drbg_edn_adapter::drive_combo()
{
    const bool rst = rst_ni.read();
    const bool clear = clear_i.read();
    const bool strobes = (tstrb_i.read() & 0xFu) == 0xFu;
    const bool full = stage_.size() >= kStageDepth;
    tready_o.write(rst && !clear && !full && strobes);

    for (unsigned i = 0; i < n_; ++i) {
        const bool flush = !rst || clear || endpoint_cancel_i[i].read();
        const bool ack = rst && !flush && ep_[i].st == ack_st::ack;
        edn_ack_o[i].write(ack);
        if (flush || !ep_[i].full) {
            edn_bus_o[i].write(0);
            edn_fips_o[i].write(false);
        } else {
            edn_bus_o[i].write(ep_[i].held.data);
            edn_fips_o[i].write(ep_[i].held.fips);
        }
    }
}

}  // namespace sep
