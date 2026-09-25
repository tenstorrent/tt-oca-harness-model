// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#include "sep_entropy_pool.h"
#include "sim_log.h"

#include <cstring>
#include <limits>

sep_entropy_pool_ip::sep_entropy_pool_ip(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , access_delay_ns_p_("access_delay_ns", 1.0)
    , fill_stall_ns_p_("fill_stall_ns", 20'480.0)
{
    reg_socket.register_b_transport(this, &sep_entropy_pool_ip::b_transport);
    reg_socket.register_transport_dbg(this, &sep_entropy_pool_ip::transport_dbg);
    reg_socket.register_get_direct_mem_ptr(this, &sep_entropy_pool_ip::get_direct_mem_ptr);

    SC_METHOD(reset_process);
    sensitive << rst_ni.neg();

    SC_THREAD(feeder_thread);

    SC_METHOD(stall_timeout_process);
    sensitive << stall_timeout_event_;
    dont_initialize();

    SC_METHOD(update_outputs);
    sensitive << output_update_event_;

    SIM_LOG_INFO(this, "SEP entropy pool instantiated: depth=" << FIFO_DEPTH
                           << " low_watermark=" << LOW_WATERMARK);
}

void sep_entropy_pool_ip::clear_entropy_path()
{
    fifo_.clear();
    packer_lo_ = 0;
    packer_half_full_ = false;
    source_armed_ = false;
    source_waiting_ = false;
    fill_stall_ = false;
    pool_error_ = false;
    stall_timeout_event_.cancel();
    output_update_event_.notify(sc_core::SC_ZERO_TIME);
}

void sep_entropy_pool_ip::reset_process()
{
    if (!rst_ni.read())
        clear_entropy_path();
}

bool sep_entropy_pool_ip::feed_word32(uint32_t word, bool fips)
{
    (void)fips; // RTL carries the EDN FIPS bit but the pool does not expose it.
    if (!rst_ni.read())
        return false;

    if (!packer_half_full_) {
        packer_lo_ = word;
        packer_half_full_ = true;
    } else {
        if (fifo_.size() >= FIFO_DEPTH)
            return false;
        fifo_.push_back(static_cast<uint64_t>(packer_lo_) |
                        (static_cast<uint64_t>(word) << 32));
        packer_half_full_ = false;
    }

    source_armed_ = true;
    source_waiting_ = false;
    cancel_stall_timer();
    output_update_event_.notify(sc_core::SC_ZERO_TIME);
    return true;
}

void sep_entropy_pool_ip::arm_stall_timer()
{
    if (!source_armed_ || source_waiting_ || fifo_.size() >= FIFO_DEPTH)
        return;
    source_waiting_ = true;
    const double ns = fill_stall_ns_p_.get_param_value();
    if (ns <= 0.0)
        stall_timeout_event_.notify(sc_core::SC_ZERO_TIME);
    else
        stall_timeout_event_.notify(sc_core::sc_time(ns, sc_core::SC_NS));
}

void sep_entropy_pool_ip::cancel_stall_timer()
{
    source_waiting_ = false;
    fill_stall_ = false;
    stall_timeout_event_.cancel();
}

void sep_entropy_pool_ip::stall_timeout_process()
{
    if (rst_ni.read() && source_waiting_ && fifo_.size() < FIFO_DEPTH) {
        fill_stall_ = true;
        output_update_event_.notify(sc_core::SC_ZERO_TIME);
    }
}

void sep_entropy_pool_ip::feeder_thread()
{
    while (true) {
        if (!rst_ni.read()) {
            wait(rst_ni.posedge_event());
            continue;
        }

        if (entropy_source.size() == 0) {
            wait(data_consumed_event_ | rst_ni.negedge_event());
            continue;
        }

        if (fifo_.size() >= FIFO_DEPTH) {
            cancel_stall_timer();
            wait(data_consumed_event_ | rst_ni.negedge_event());
            continue;
        }

        uint32_t word = 0;
        bool fips = false;
        if (entropy_source->try_pop_entropy_word(POOL_ENDPOINT_ID, word, fips)) {
            (void)feed_word32(word, fips);
            continue;
        }

        arm_stall_timer();
        wait(entropy_source->entropy_available_event() |
             data_consumed_event_ | rst_ni.negedge_event());
    }
}

uint64_t sep_entropy_pool_ip::status_word() const
{
    const uint64_t level = static_cast<uint64_t>(fifo_.size()) & 0x3fu;
    const uint64_t low = fifo_.size() < LOW_WATERMARK ? (uint64_t{1} << 6) : 0u;
    const uint64_t stall = fill_stall_ ? (uint64_t{1} << 7) : 0u;
    const uint64_t error = pool_error_ ? (uint64_t{1} << 8) : 0u;
    return level | low | stall | error;
}

uint64_t sep_entropy_pool_ip::irq_cause_word() const
{
    return (fifo_.size() < LOW_WATERMARK ? uint64_t{1} : 0u) |
           (fill_stall_ ? (uint64_t{1} << 1) : 0u) |
           (pool_error_ ? (uint64_t{1} << 2) : 0u);
}

void sep_entropy_pool_ip::update_outputs()
{
    pool_low_o.write(!rst_ni.read() || fifo_.size() < LOW_WATERMARK);
    fill_stall_o.write(rst_ni.read() && fill_stall_);
    pool_error_o.write(rst_ni.read() && pool_error_);
}

bool sep_entropy_pool_ip::valid_read_payload(
    const tlm::tlm_generic_payload& trans) const
{
    const auto width = trans.get_streaming_width();
    const auto len = trans.get_data_length();
    return trans.get_data_ptr() != nullptr &&
           (len == sizeof(uint32_t) || len == sizeof(uint64_t)) &&
           (width == 0u || width >= len) &&
           trans.get_byte_enable_ptr() == nullptr &&
           (trans.get_address() % len) == 0u;
}

void sep_entropy_pool_ip::b_transport(tlm::tlm_generic_payload& trans,
                                      sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    delay += sc_core::sc_time(access_delay_ns_p_.get_param_value(), sc_core::SC_NS);

    if (!trans.is_read() && !trans.is_write()) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (trans.get_data_ptr() == nullptr) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (trans.get_byte_enable_ptr() != nullptr) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }
    const unsigned len = trans.get_data_length();
    if ((len != sizeof(uint32_t) && len != sizeof(uint64_t)) ||
        (trans.get_streaming_width() != 0u &&
         trans.get_streaming_width() < len)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (trans.get_address() >= APERTURE_SIZE ||
        (trans.get_address() % len) != 0u) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    if (trans.is_write()) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    uint64_t value = 0;
    switch (trans.get_address()) {
    case STATUS_OFFSET:
        value = status_word();
        break;
    case IRQ_CAUSE_OFFSET:
        value = irq_cause_word();
        break;
    case DATA_OFFSET:
        if (!rst_ni.read() || fifo_.empty()) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            std::memset(trans.get_data_ptr(), 0, len);
            return;
        }
        value = fifo_.front();
        fifo_.pop_front();
        data_consumed_event_.notify(sc_core::SC_ZERO_TIME);
        output_update_event_.notify(sc_core::SC_ZERO_TIME);
        break;
    default:
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        std::memset(trans.get_data_ptr(), 0, len);
        return;
    }

    std::memcpy(trans.get_data_ptr(), &value, len);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    SIM_LOG_TRACE(this, "read off=0x" << std::hex << trans.get_address()
                                      << " data=0x" << value);
}

unsigned sep_entropy_pool_ip::transport_dbg(tlm::tlm_generic_payload& trans)
{
    trans.set_dmi_allowed(false);
    if (!trans.is_read() || !valid_read_payload(trans) ||
        trans.get_address() >= APERTURE_SIZE)
        return 0;

    uint64_t value = 0;
    switch (trans.get_address()) {
    case STATUS_OFFSET:
        value = status_word();
        break;
    case IRQ_CAUSE_OFFSET:
        value = irq_cause_word();
        break;
    case DATA_OFFSET:
        if (!rst_ni.read() || fifo_.empty())
            return 0;
        value = fifo_.front(); // Debug inspection is intentionally non-destructive.
        break;
    default:
        return 0;
    }
    std::memcpy(trans.get_data_ptr(), &value, trans.get_data_length());
    return trans.get_data_length();
}

bool sep_entropy_pool_ip::get_direct_mem_ptr(tlm::tlm_generic_payload&,
                                              tlm::tlm_dmi& dmi)
{
    dmi.set_start_address(0);
    dmi.set_end_address(std::numeric_limits<sc_dt::uint64>::max());
    return false;
}
