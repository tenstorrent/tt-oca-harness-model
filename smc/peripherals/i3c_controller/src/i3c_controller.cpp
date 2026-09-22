// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file i3c_controller.cpp
 * @brief OCA I3C Controller — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/i3c_controller.h` for the full design description, register
 * map, command/response descriptor formats, transaction-engine behaviour, and
 * CCI catalogue.
 */

#include "i3c_controller.h"

#include "reg_access.h"
#include "sim_log.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

namespace smc {

namespace {

// Bit accessor from the shared register-model library (common/include/reg_access.h).
using regmodel::bit;

// ---------------------------------------------------------------------------
// HCI regular-transfer DAT descriptor field accessors (TCRI 7.1.2.2).
//
//   [2:0]   CMD_ATTR     (0 = regular transfer; only attr modelled)
//   [6:3]   TID          transaction ID (echoed in the response)
//   [14:7]  CCC_CODE
//   [15]    CP           command present: 1 = CCC transfer
//   [20:16] DEV_INDEX    DAT index (0..31)
//   [29]    RNW          0 = write, 1 = read
//   [63:48] DATA_LENGTH  bytes to transfer
// ---------------------------------------------------------------------------
constexpr uint8_t  cmd_attr   (uint64_t d) { return  uint8_t(d & 0x7); }
constexpr uint8_t  cmd_tid    (uint64_t d) { return  uint8_t((d >> 3) & 0xF); }
constexpr uint8_t  cmd_ccc    (uint64_t d) { return  uint8_t((d >> 7) & 0xFF); }
constexpr bool     cmd_cp     (uint64_t d) { return  ((d >> 15) & 0x1) != 0; }
constexpr uint8_t  cmd_devidx (uint64_t d) { return  uint8_t((d >> 16) & 0x1F); }
constexpr bool     cmd_rnw    (uint64_t d) { return  ((d >> 29) & 0x1) != 0; }
constexpr uint16_t cmd_length (uint64_t d) { return  uint16_t((d >> 48) & 0xFFFF); }

/// Build a 32-bit response descriptor (memmap.adoc RESPONSE_PORT).
///   [3:0] TID, [27:16] DATA_LENGTH, [31:28] ERROR
constexpr uint32_t make_response(uint8_t tid, uint16_t data_len, i3c_err err)
{
    return (uint32_t(tid) & 0xF) |
           ((uint32_t(data_len) & 0xFFF) << 16) |
           ((uint32_t(err) & 0xF) << 28);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

i3c_controller::i3c_controller(sc_core::sc_module_name name, i3c_controller_cfg cfg)
    : sc_core::sc_module(name)
    , num_instances_p_(
          "num_instances", cfg.num_instances,
          "Number of independent I3C instances behind the AXI-Lite aperture "
          "(1..6). Sizes the per-instance interrupt and bus output vectors. "
          "Matches smc_config_pkg::NUM_I3C (6).")
    , access_delay_ns_p_(
          "access_delay_ns", cfg.access_delay_ns,
          "TLM b_transport annotated delay in nanoseconds. Approximates "
          "AXI4-Lite register-access latency. Mutable at run time.")
    , xfer_delay_ns_p_(
          "xfer_delay_ns", cfg.xfer_delay_ns,
          "Modelled transaction-processing latency in nanoseconds: simulated "
          "time from a command being enqueued to its response being produced.")
    , cmd_fifo_depth_p_(
          "cmd_fifo_depth", cfg.cmd_fifo_depth,
          "HCI command/response queue depth (entries).")
    , rx_fifo_depth_p_(
          "rx_fifo_depth", cfg.rx_fifo_depth,
          "RX data FIFO depth (DWORDs).")
    , tx_fifo_depth_p_(
          "tx_fifo_depth", cfg.tx_fifo_depth,
          "TX data FIFO depth (DWORDs).")
    , ibi_fifo_depth_p_(
          "ibi_fifo_depth", cfg.ibi_fifo_depth,
          "IBI status/data queue depth (DWORDs).")
    , reg_socket("reg_socket")
    , irq_o("irq_o", num_instances_p_.get_value())
    , scl_o("scl_o", num_instances_p_.get_value())
    , sda_o("sda_o", num_instances_p_.get_value())
    , scl_oe_o("scl_oe_o", num_instances_p_.get_value())
    , sda_oe_o("sda_oe_o", num_instances_p_.get_value())
    , sel_od_pp_o("sel_od_pp_o", num_instances_p_.get_value())
    , recovery_payload_available_o("recovery_payload_available_o", num_instances_p_.get_value())
    , recovery_image_activated_o("recovery_image_activated_o", num_instances_p_.get_value())
    , cfg_(cfg)
{
    // Sync cfg_ with CCI-resolved values.
    cfg_.num_instances  = num_instances_p_.get_value();
    cfg_.access_delay_ns = access_delay_ns_p_.get_value();
    cfg_.xfer_delay_ns  = xfer_delay_ns_p_.get_value();
    cfg_.cmd_fifo_depth = cmd_fifo_depth_p_.get_value();
    cfg_.rx_fifo_depth  = rx_fifo_depth_p_.get_value();
    cfg_.tx_fifo_depth  = tx_fifo_depth_p_.get_value();
    cfg_.ibi_fifo_depth = ibi_fifo_depth_p_.get_value();

    // Provenance metadata.
    num_instances_p_.add_metadata("rtl_param", cci::cci_value(std::string("NUM_I3C")));
    num_instances_p_.add_metadata("valid_range", cci::cci_value(std::string("1..6")));
    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    xfer_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));

    if (cfg_.num_instances == 0 ||
        cfg_.num_instances > i3c_controller_cfg::MAX_INSTANCES) {
        SC_REPORT_FATAL(name, "i3c_controller num_instances must be in 1..6");
    }
    if (cfg_.access_delay_ns < 0.0 || cfg_.xfer_delay_ns < 0.0) {
        SC_REPORT_FATAL(name, "i3c_controller delays must be >= 0");
    }
    if (cfg_.cmd_fifo_depth == 0 || cfg_.tx_fifo_depth == 0 ||
        cfg_.rx_fifo_depth == 0 || cfg_.ibi_fifo_depth == 0) {
        SC_REPORT_FATAL(name, "i3c_controller FIFO depths must be >= 1");
    }

    access_delay_ = sc_core::sc_time(cfg_.access_delay_ns, sc_core::SC_NS);
    xfer_delay_   = sc_core::sc_time(cfg_.xfer_delay_ns,   sc_core::SC_NS);

    // Per-instance state.
    inst_.resize(cfg_.num_instances);
    for (auto& s : inst_) {
        s.dat.assign(i3c_controller_cfg::DAT_WORDS, 0);
        s.dct.assign(i3c_controller_cfg::DCT_WORDS, 0);
        s.valid = true;
    }

    reg_socket.register_b_transport  (this, &i3c_controller::b_transport);
    reg_socket.register_transport_dbg(this, &i3c_controller::transport_dbg);

    SC_METHOD(output_method);
    sensitive << recompute_event_;
    dont_initialize();

    SC_METHOD(xfer_method);
    sensitive << xfer_event_;
    dont_initialize();

    SC_METHOD(reset_method);
    sensitive << rst_n_i.neg();
    dont_initialize();

    SIM_LOG_INFO(this,
        "i3c_controller instantiated: num_instances=" << cfg_.num_instances
        << ", cmd_fifo=" << cfg_.cmd_fifo_depth
        << ", tx_fifo=" << cfg_.tx_fifo_depth
        << ", rx_fifo=" << cfg_.rx_fifo_depth
        << ", ibi_fifo=" << cfg_.ibi_fifo_depth);
}

void i3c_controller::reset_method()
{
    xfer_event_.cancel();
    for (auto& s : inst_) {
        auto bus_model = std::move(s.bus_model);
        s = inst_state{};
        s.dat.assign(i3c_controller_cfg::DAT_WORDS, 0);
        s.dct.assign(i3c_controller_cfg::DCT_WORDS, 0);
        s.bus_model = std::move(bus_model);
        s.valid = true;
    }
    schedule_recompute();
}

void i3c_controller::start_of_simulation()
{
    schedule_recompute();
}

void i3c_controller::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void i3c_controller::schedule_xfer()
{
    xfer_event_.notify(xfer_delay_);
}

// ---------------------------------------------------------------------------
// Threshold helpers
// ---------------------------------------------------------------------------

unsigned i3c_controller::tx_buf_thld(const inst_state& s) const
{
    const unsigned n = (s.data_buffer_thld_ctrl >> 0) & 0x7;
    return 1u << (n + 1);
}
unsigned i3c_controller::rx_buf_thld(const inst_state& s) const
{
    const unsigned n = (s.data_buffer_thld_ctrl >> 8) & 0x7;
    return 1u << (n + 1);
}
unsigned i3c_controller::cmd_empty_thld(const inst_state& s) const
{
    const unsigned n = (s.queue_thld_ctrl >> 0) & 0xFF;
    return n ? n : 1u;
}
unsigned i3c_controller::resp_buf_thld(const inst_state& s) const
{
    const unsigned n = (s.queue_thld_ctrl >> 8) & 0xFF;
    return n ? n : 1u;
}
unsigned i3c_controller::ibi_status_thld(const inst_state& s) const
{
    const unsigned n = (s.queue_thld_ctrl >> 24) & 0xFF;
    return n ? n : 1u;
}

// ---------------------------------------------------------------------------
// Computed read-only registers
// ---------------------------------------------------------------------------

uint32_t i3c_controller::compute_hc_capabilities() const
{
    // Advertise PIO mode + controller + target support (informational).
    return 0x0000'0007;
}

uint32_t i3c_controller::compute_dat_section_offset() const
{
    // [11:0] table offset, [18:12] table size (entries), [22:19] entry size.
    return uint32_t(i3c_controller_cfg::DAT_BASE) |
           (uint32_t(i3c_controller_cfg::DAT_ENTRIES) << 12) |
           (uint32_t(i3c_controller_cfg::DAT_DWORDS)  << 19);
}

uint32_t i3c_controller::compute_dct_section_offset(const inst_state& s) const
{
    // [11:0] table offset, [18:12] table size, [22:19] entry size,
    // [31:19] ENTDAA index (RW, stored in s.dct_section_offset upper bits).
    const uint32_t fixed = uint32_t(i3c_controller_cfg::DCT_BASE) |
                           (uint32_t(i3c_controller_cfg::DCT_ENTRIES) << 12) |
                           (uint32_t(i3c_controller_cfg::DCT_DWORDS)  << 19);
    const uint32_t entdaa = s.dct_section_offset & 0xFF80'0000u; // [31:23] rw
    return (fixed & ~0xFF80'0000u) | entdaa;
}

uint32_t i3c_controller::compute_queue_size(const inst_state& s) const
{
    auto log2_depth = [](unsigned depth) -> uint32_t {
        // Encode as log2(depth)-1 (i3c-core convention; min 0).
        unsigned n = 0;
        while ((1u << (n + 1)) < depth) ++n;
        return n;
    };
    const uint32_t cr  = cfg_.cmd_fifo_depth & 0xFF;
    const uint32_t ibi = cfg_.ibi_fifo_depth & 0xFF;
    const uint32_t rx  = log2_depth(cfg_.rx_fifo_depth) & 0xFF;
    const uint32_t tx  = log2_depth(cfg_.tx_fifo_depth) & 0xFF;
    (void)s;
    return cr | (ibi << 8) | (rx << 16) | (tx << 24);
}

uint32_t i3c_controller::compute_present_state(const inst_state& s) const
{
    // bit2 AC_CURRENT_OWN: this controller owns the bus when enabled.
    return bit(s.hc_control, hc_control::BUS_ENABLE) ? (1u << 2) : 0u;
}

// ---------------------------------------------------------------------------
// PIO interrupt level / aggregation
// ---------------------------------------------------------------------------

bool i3c_controller::irq_level(const inst_state& s) const
{
    // Level threshold bits.
    uint32_t pio_level = 0;
    const unsigned tx_free = cfg_.tx_fifo_depth - unsigned(s.tx_q.size());
    if (tx_free >= tx_buf_thld(s))
        pio_level |= (1u << pio_intr::TX_THLD_STAT);
    if (s.rx_q.size() >= rx_buf_thld(s))
        pio_level |= (1u << pio_intr::RX_THLD_STAT);
    const unsigned cmd_free = cfg_.cmd_fifo_depth - unsigned(s.cmd_q.size());
    if (cmd_free >= cmd_empty_thld(s))
        pio_level |= (1u << pio_intr::CMD_QUEUE_READY_STAT);
    if (s.resp_q.size() >= resp_buf_thld(s))
        pio_level |= (1u << pio_intr::RESP_READY_STAT);
    if (s.ibi_q.size() >= ibi_status_thld(s))
        pio_level |= (1u << pio_intr::IBI_STATUS_THLD_STAT);

    const uint32_t pio = (s.pio_intr_status | pio_level) & pio_intr::W1C_MASK;
    const bool pio_irq = (pio & s.pio_intr_signal_enable) != 0;
    const bool hc_irq  = (s.intr_status & s.intr_signal_enable) != 0;
    return pio_irq || hc_irq;
}

// ---------------------------------------------------------------------------
// output_method — sole driver of all outputs
// ---------------------------------------------------------------------------

void i3c_controller::output_method()
{
    for (unsigned i = 0; i < cfg_.num_instances; ++i) {
        inst_state& s = inst_[i];
        const bool irq = irq_level(s);
        irq_o[i].write(irq);
        s.cache_irq = irq;

        // Bit-level bus signalling is abstracted: hold the bus idle (released).
        scl_o[i].write(true);
        sda_o[i].write(true);
        scl_oe_o[i].write(false);
        sda_oe_o[i].write(false);
        sel_od_pp_o[i].write(false);
        recovery_payload_available_o[i].write(false);
        recovery_image_activated_o[i].write(false);
    }
}

// ---------------------------------------------------------------------------
// Reset-control / queue handling
// ---------------------------------------------------------------------------

void i3c_controller::apply_reset_control(inst_state& s, uint32_t data)
{
    if (bit(data, reset_control::SOFT_RST)) {
        // Soft reset: FSMs + queues, but NOT the CSR configuration.
        s.cmd_q.clear(); s.resp_q.clear();
        s.tx_q.clear();  s.rx_q.clear(); s.ibi_q.clear();
        s.cmd_lo_seen = false;
        s.pio_intr_status = 0;
        s.intr_status = 0;
    }
    if (bit(data, reset_control::CMD_QUEUE_RST))  { s.cmd_q.clear(); s.cmd_lo_seen = false; }
    if (bit(data, reset_control::RESP_QUEUE_RST))   s.resp_q.clear();
    if (bit(data, reset_control::TX_FIFO_RST))      s.tx_q.clear();
    if (bit(data, reset_control::RX_FIFO_RST))      s.rx_q.clear();
    if (bit(data, reset_control::IBI_QUEUE_RST))    s.ibi_q.clear();
    // All RESET_CONTROL bits are self-clearing (read back as 0).
}

void i3c_controller::enqueue_command(inst_state& s, uint64_t desc)
{
    if (s.cmd_q.size() >= cfg_.cmd_fifo_depth) {
        // Command queue full: warn (RTL drops/stalls).
        s.intr_status |= (1u << hc_intr::HC_WARN_CMD_SEQ_STALL_STAT);
        return;
    }
    s.cmd_q.push_back(desc);

    const unsigned idx = unsigned(&s - inst_.data());
    if (bit(s.hc_control, hc_control::BUS_ENABLE)) {
        xfer_pending_.push_back(idx);
        schedule_xfer();
    }
}

// ---------------------------------------------------------------------------
// xfer_method — process one queued command per xfer_delay tick
// ---------------------------------------------------------------------------

void i3c_controller::xfer_method()
{
    if (xfer_pending_.empty()) return;
    const unsigned inst = xfer_pending_.front();
    xfer_pending_.pop_front();

    process_command(inst);

    // Continue draining any remaining work.
    if (!xfer_pending_.empty()) {
        xfer_event_.notify(xfer_delay_);
    }
}

void i3c_controller::process_command(unsigned inst)
{
    inst_state& s = inst_[inst];
    if (s.cmd_q.empty()) return;
    if (!bit(s.hc_control, hc_control::BUS_ENABLE)) return;

    const uint64_t desc = s.cmd_q.front();
    s.cmd_q.pop_front();

    const uint8_t  tid     = cmd_tid(desc);
    const bool     rnw     = cmd_rnw(desc);
    const uint8_t  devidx  = cmd_devidx(desc);
    const bool     is_ccc  = cmd_cp(desc);
    const uint16_t length  = cmd_length(desc);
    (void)cmd_attr(desc);

    // Resolve dynamic address from the DAT window (DWORD0 [22:16]).
    uint8_t dyn_addr = 0;
    if (devidx < i3c_controller_cfg::DAT_ENTRIES) {
        dyn_addr = uint8_t((s.dat[size_t(devidx) * i3c_controller_cfg::DAT_DWORDS] >> 16) & 0x7F);
    }

    i3c_xfer x;
    x.dev_index    = devidx;
    x.dynamic_addr = dyn_addr;
    x.ccc_code     = cmd_ccc(desc);
    x.data_length  = length;
    if (is_ccc) x.kind = rnw ? i3c_xfer_kind::CccRead  : i3c_xfer_kind::CccWrite;
    else        x.kind = rnw ? i3c_xfer_kind::PrivateRead : i3c_xfer_kind::PrivateWrite;

    const bool is_write = !rnw;

    // For writes, drain payload from the TX FIFO.
    uint16_t actual_len = 0;
    bool underflow = false;
    if (is_write) {
        const unsigned dwords_needed = (length + 3u) / 4u;
        if (s.tx_q.size() < dwords_needed) {
            underflow = true;
        } else {
            x.write_data.reserve(length);
            for (unsigned d = 0; d < dwords_needed; ++d) {
                const uint32_t w = s.tx_q.front(); s.tx_q.pop_front();
                for (unsigned b = 0; b < 4 && x.write_data.size() < length; ++b)
                    x.write_data.push_back(uint8_t((w >> (8 * b)) & 0xFF));
            }
            actual_len = length;
        }
    }

    i3c_err err = i3c_err::Success;

    if (underflow) {
        err = i3c_err::OverflowUnder;
        s.pio_intr_status |= (1u << pio_intr::TRANSFER_ERR_STAT);
    } else if (!s.bus_model) {
        // No target attached: address NACK.
        err = i3c_err::AddressNack;
        s.pio_intr_status |= (1u << pio_intr::TRANSFER_ERR_STAT);
    } else {
        s.bus_model(x);
        if (!x.ack) {
            err = (x.error == i3c_err::Success) ? i3c_err::AddressNack : x.error;
            s.pio_intr_status |= (1u << pio_intr::TRANSFER_ERR_STAT);
        } else {
            err = x.error;
            if (!is_write) {
                // Read: push returned bytes into the RX FIFO.
                const uint16_t want = length;
                const size_t   got  = x.read_data.size() < want ? x.read_data.size() : want;
                const unsigned dwords = unsigned((got + 3) / 4);
                const unsigned rx_free = cfg_.rx_fifo_depth - unsigned(s.rx_q.size());
                if (dwords > rx_free) {
                    err = i3c_err::OverflowUnder;
                    s.pio_intr_status |= (1u << pio_intr::TRANSFER_ERR_STAT);
                } else {
                    for (unsigned d = 0; d < dwords; ++d) {
                        uint32_t w = 0;
                        for (unsigned b = 0; b < 4; ++b) {
                            const size_t idx = size_t(d) * 4 + b;
                            if (idx < got) w |= uint32_t(x.read_data[idx]) << (8 * b);
                        }
                        s.rx_q.push_back(w);
                    }
                    actual_len = uint16_t(got);
                }
            }
            if (err != i3c_err::Success)
                s.pio_intr_status |= (1u << pio_intr::TRANSFER_ERR_STAT);
        }
    }

    // Generate the response descriptor.
    const uint32_t resp = make_response(tid, actual_len, err);
    if (s.resp_q.size() < cfg_.cmd_fifo_depth) {
        s.resp_q.push_back(resp);
    } else {
        s.intr_status |= (1u << hc_intr::HC_INTERNAL_ERR_STAT);
    }

    schedule_recompute();
}

// ---------------------------------------------------------------------------
// Register read (has side effects: FIFO ports pop)
// ---------------------------------------------------------------------------

bool i3c_controller::reg_read(unsigned inst, uint64_t loff, uint32_t& data)
{
    inst_state& s = inst_[inst];
    data = 0;

    // DAT / DCT direct-access windows.
    if (loff >= i3c_controller_cfg::DAT_BASE && loff < i3c_controller_cfg::DAT_END) {
        data = s.dat[(loff - i3c_controller_cfg::DAT_BASE) / 4];
        return true;
    }
    if (loff >= i3c_controller_cfg::DCT_BASE && loff < i3c_controller_cfg::DCT_END) {
        data = s.dct[(loff - i3c_controller_cfg::DCT_BASE) / 4];
        return true;
    }

    switch (loff) {
    case i3c_controller_cfg::HCI_VERSION:
        data = i3c_controller_cfg::HCI_VERSION_VALUE; break;
    case i3c_controller_cfg::HC_CONTROL:
        // MODE_SELECTOR reads as 1 (PIO mode, read-only).
        data = (s.hc_control & hc_control::WMASK) | (1u << hc_control::MODE_SELECTOR);
        break;
    case i3c_controller_cfg::CONTROLLER_DEVICE_ADDR:
        data = s.controller_device_addr; break;
    case i3c_controller_cfg::HC_CAPABILITIES:
        data = compute_hc_capabilities(); break;
    case i3c_controller_cfg::RESET_CONTROL:
        data = 0; break; // self-clearing
    case i3c_controller_cfg::PRESENT_STATE:
        data = compute_present_state(s); break;
    case i3c_controller_cfg::INTR_STATUS:
        data = s.intr_status & hc_intr::W1C_MASK; break;
    case i3c_controller_cfg::INTR_STATUS_ENABLE:
        data = s.intr_status_enable; break;
    case i3c_controller_cfg::INTR_SIGNAL_ENABLE:
        data = s.intr_signal_enable; break;
    case i3c_controller_cfg::INTR_FORCE:
        data = 0; break; // write-only
    case i3c_controller_cfg::DAT_SECTION_OFFSET:
        data = compute_dat_section_offset(); break;
    case i3c_controller_cfg::DCT_SECTION_OFFSET:
        data = compute_dct_section_offset(s); break;
    case i3c_controller_cfg::PIO_SECTION_OFFSET:
        data = uint32_t(i3c_controller_cfg::COMMAND_PORT); break;
    case i3c_controller_cfg::RESPONSE_PORT:
        if (!s.resp_q.empty()) { data = s.resp_q.front(); s.resp_q.pop_front(); }
        else                    data = 0;
        schedule_recompute();
        break;
    case i3c_controller_cfg::XFER_DATA_PORT: // read = RX FIFO pop
        if (!s.rx_q.empty()) { data = s.rx_q.front(); s.rx_q.pop_front(); }
        else                  data = 0;
        schedule_recompute();
        break;
    case i3c_controller_cfg::IBI_PORT:
        if (!s.ibi_q.empty()) { data = s.ibi_q.front(); s.ibi_q.pop_front(); }
        else                   data = 0;
        schedule_recompute();
        break;
    case i3c_controller_cfg::QUEUE_THLD_CTRL:
        data = s.queue_thld_ctrl; break;
    case i3c_controller_cfg::DATA_BUFFER_THLD_CTRL:
        data = s.data_buffer_thld_ctrl; break;
    case i3c_controller_cfg::QUEUE_SIZE:
        data = compute_queue_size(s); break;
    case i3c_controller_cfg::ALT_QUEUE_SIZE:
        data = 0; break;
    case i3c_controller_cfg::PIO_INTR_STATUS: {
        // Latched W1C bits OR'd with current level threshold bits.
        uint32_t level = 0;
        const unsigned tx_free = cfg_.tx_fifo_depth - unsigned(s.tx_q.size());
        if (tx_free >= tx_buf_thld(s))           level |= (1u << pio_intr::TX_THLD_STAT);
        if (s.rx_q.size() >= rx_buf_thld(s))     level |= (1u << pio_intr::RX_THLD_STAT);
        const unsigned cmd_free = cfg_.cmd_fifo_depth - unsigned(s.cmd_q.size());
        if (cmd_free >= cmd_empty_thld(s))       level |= (1u << pio_intr::CMD_QUEUE_READY_STAT);
        if (s.resp_q.size() >= resp_buf_thld(s)) level |= (1u << pio_intr::RESP_READY_STAT);
        if (s.ibi_q.size() >= ibi_status_thld(s))level |= (1u << pio_intr::IBI_STATUS_THLD_STAT);
        data = (s.pio_intr_status | level) & pio_intr::W1C_MASK;
        break;
    }
    case i3c_controller_cfg::PIO_INTR_STATUS_ENABLE:
        data = s.pio_intr_status_enable; break;
    case i3c_controller_cfg::PIO_INTR_SIGNAL_ENABLE:
        data = s.pio_intr_signal_enable; break;
    case i3c_controller_cfg::PIO_CONTROL:
        data = s.pio_control; break;
    case i3c_controller_cfg::STBY_CR_EXTCAP_HEADER:
        data = i3c_controller_cfg::STBY_CR_EXTCAP_HDR; break;
    case i3c_controller_cfg::STBY_CR_CONTROL:
        data = s.stby_cr_control; break;
    case i3c_controller_cfg::STBY_CR_DEVICE_ADDR:
        data = s.stby_cr_device_addr; break;
    case i3c_controller_cfg::STBY_CR_CAPABILITIES:
        data = 0; break;
    default:
        // Hole inside the window: RAZ.
        break;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Register write
// ---------------------------------------------------------------------------

bool i3c_controller::reg_write(unsigned inst, uint64_t loff, uint32_t data)
{
    inst_state& s = inst_[inst];

    // DAT / DCT direct-access windows.
    if (loff >= i3c_controller_cfg::DAT_BASE && loff < i3c_controller_cfg::DAT_END) {
        s.dat[(loff - i3c_controller_cfg::DAT_BASE) / 4] = data;
        return true;
    }
    if (loff >= i3c_controller_cfg::DCT_BASE && loff < i3c_controller_cfg::DCT_END) {
        s.dct[(loff - i3c_controller_cfg::DCT_BASE) / 4] = data;
        return true;
    }

    switch (loff) {
    case i3c_controller_cfg::HC_CONTROL: {
        const bool was_enabled = bit(s.hc_control, hc_control::BUS_ENABLE);
        s.hc_control = data & hc_control::WMASK;
        // ABORT and RESUME are self-clearing.
        if (bit(data, hc_control::ABORT)) {
            // Flush pending commands; report abort.
            s.cmd_q.clear(); s.cmd_lo_seen = false;
            s.pio_intr_status |= (1u << pio_intr::TRANSFER_ABORT_STAT);
        }
        s.hc_control &= ~((1u << hc_control::ABORT) | (1u << hc_control::RESUME));
        // Newly enabled: drain any queued commands.
        const bool now_enabled = bit(s.hc_control, hc_control::BUS_ENABLE);
        if (!was_enabled && now_enabled && !s.cmd_q.empty()) {
            for (size_t k = 0; k < s.cmd_q.size(); ++k) xfer_pending_.push_back(inst);
            schedule_xfer();
        }
        break;
    }
    case i3c_controller_cfg::CONTROLLER_DEVICE_ADDR:
        s.controller_device_addr = data; break;
    case i3c_controller_cfg::RESET_CONTROL:
        apply_reset_control(s, data & reset_control::MASK); break;
    case i3c_controller_cfg::INTR_STATUS:
        s.intr_status = regmodel::apply_w1c(s.intr_status, data, hc_intr::W1C_MASK);
        break; // W1C
    case i3c_controller_cfg::INTR_STATUS_ENABLE:
        s.intr_status_enable = data; break;
    case i3c_controller_cfg::INTR_SIGNAL_ENABLE:
        s.intr_signal_enable = data; break;
    case i3c_controller_cfg::INTR_FORCE:
        // Force = write-1-to-set on the same W1C bits (masked to the valid set).
        s.intr_status = regmodel::apply_woset(s.intr_status, data & hc_intr::W1C_MASK);
        break;
    case i3c_controller_cfg::DCT_SECTION_OFFSET:
        s.dct_section_offset = data & 0xFF80'0000u; break; // only ENTDAA index rw
    case i3c_controller_cfg::COMMAND_PORT:
        if (!s.cmd_lo_seen) {
            s.cmd_lo = data;
            s.cmd_lo_seen = true;
        } else {
            const uint64_t desc = (uint64_t(data) << 32) | s.cmd_lo;
            s.cmd_lo_seen = false;
            enqueue_command(s, desc);
        }
        break;
    case i3c_controller_cfg::XFER_DATA_PORT: // write = TX FIFO push
        if (s.tx_q.size() < cfg_.tx_fifo_depth) s.tx_q.push_back(data);
        else s.pio_intr_status |= (1u << pio_intr::TRANSFER_ERR_STAT);
        break;
    case i3c_controller_cfg::QUEUE_THLD_CTRL:
        s.queue_thld_ctrl = data; break;
    case i3c_controller_cfg::DATA_BUFFER_THLD_CTRL:
        s.data_buffer_thld_ctrl = data; break;
    case i3c_controller_cfg::PIO_INTR_STATUS:
        s.pio_intr_status = regmodel::apply_w1c(s.pio_intr_status, data, pio_intr::W1C_MASK);
        break; // W1C
    case i3c_controller_cfg::PIO_INTR_STATUS_ENABLE:
        s.pio_intr_status_enable = data; break;
    case i3c_controller_cfg::PIO_INTR_SIGNAL_ENABLE:
        s.pio_intr_signal_enable = data; break;
    case i3c_controller_cfg::PIO_CONTROL:
        s.pio_control = data & 0x7; break;
    case i3c_controller_cfg::STBY_CR_CONTROL:
        // CR_REQUEST_SEND (bit5) is self-clearing.
        s.stby_cr_control = data & ~(1u << 5); break;
    case i3c_controller_cfg::STBY_CR_DEVICE_ADDR:
        s.stby_cr_device_addr = data; break;

    // Read-only registers: writes ignored.
    case i3c_controller_cfg::HCI_VERSION:
    case i3c_controller_cfg::HC_CAPABILITIES:
    case i3c_controller_cfg::PRESENT_STATE:
    case i3c_controller_cfg::DAT_SECTION_OFFSET:
    case i3c_controller_cfg::PIO_SECTION_OFFSET:
    case i3c_controller_cfg::RESPONSE_PORT:
    case i3c_controller_cfg::IBI_PORT:
    case i3c_controller_cfg::QUEUE_SIZE:
    case i3c_controller_cfg::ALT_QUEUE_SIZE:
    case i3c_controller_cfg::STBY_CR_EXTCAP_HEADER:
    case i3c_controller_cfg::STBY_CR_CAPABILITIES:
        break;

    default:
        // Hole inside the window: WI.
        break;
    }

    schedule_recompute();
    return true;
}

// ---------------------------------------------------------------------------
// b_transport — the only blocking TLM entry point
// ---------------------------------------------------------------------------

void i3c_controller::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

    if (length != 4 || (addr & 0x3u) != 0 || gp.get_streaming_width() != length) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (gp.get_byte_enable_ptr() != nullptr && gp.get_byte_enable_length() != 0) {
        gp.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }

    const uint64_t aperture = uint64_t(cfg_.num_instances) *
                              i3c_controller_cfg::INSTANCE_SPACING;
    if (addr >= aperture) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    const unsigned inst = unsigned(addr / i3c_controller_cfg::INSTANCE_SPACING);
    const uint64_t loff = addr % i3c_controller_cfg::INSTANCE_SPACING;

    bool ok = true;
    if (gp.is_read()) {
        uint32_t data = 0;
        ok = reg_read(inst, loff, data);
        if (ok) std::memcpy(buf, &data, 4);
        SIM_LOG_TRACE(this, "read  inst=" << inst << " off=0x" << std::hex << loff
                            << " data=0x" << data);
    } else if (gp.is_write()) {
        uint32_t data = 0;
        std::memcpy(&data, buf, 4);
        ok = reg_write(inst, loff, data);
        SIM_LOG_TRACE(this, "write inst=" << inst << " off=0x" << std::hex << loff
                            << " data=0x" << data);
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    if (!ok) {
        SIM_LOG_DEBUG(this, "TLM decode miss inst=" << inst << " off=0x"
                            << std::hex << loff);
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    access_delay_ = sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    delay += access_delay_;
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
}

// ---------------------------------------------------------------------------
// transport_dbg — back-door register access (no delay, no FIFO side effects)
// ---------------------------------------------------------------------------

unsigned int i3c_controller::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const sc_dt::uint64 addr   = gp.get_address();
    const unsigned      length = gp.get_data_length();
    unsigned char* const buf   = gp.get_data_ptr();

    const uint64_t aperture = uint64_t(cfg_.num_instances) *
                              i3c_controller_cfg::INSTANCE_SPACING;
    if (length != 4 || (addr & 0x3u) != 0 || addr >= aperture) return 0;

    const unsigned inst = unsigned(addr / i3c_controller_cfg::INSTANCE_SPACING);
    const uint64_t loff = addr % i3c_controller_cfg::INSTANCE_SPACING;
    inst_state& s = inst_[inst];

    // Side-effect-free back door: only CSR/table storage, never FIFO ports.
    if (gp.is_read()) {
        uint32_t data = 0;
        if (loff >= i3c_controller_cfg::DAT_BASE && loff < i3c_controller_cfg::DAT_END)
            data = s.dat[(loff - i3c_controller_cfg::DAT_BASE) / 4];
        else if (loff >= i3c_controller_cfg::DCT_BASE && loff < i3c_controller_cfg::DCT_END)
            data = s.dct[(loff - i3c_controller_cfg::DCT_BASE) / 4];
        else
            return 0; // only tables are safe for back-door access
        std::memcpy(buf, &data, 4);
    } else {
        uint32_t data = 0;
        std::memcpy(&data, buf, 4);
        if (loff >= i3c_controller_cfg::DAT_BASE && loff < i3c_controller_cfg::DAT_END)
            s.dat[(loff - i3c_controller_cfg::DAT_BASE) / 4] = data;
        else if (loff >= i3c_controller_cfg::DCT_BASE && loff < i3c_controller_cfg::DCT_END)
            s.dct[(loff - i3c_controller_cfg::DCT_BASE) / 4] = data;
        else
            return 0;
    }
    return length;
}

// ---------------------------------------------------------------------------
// Configuration / debug API
// ---------------------------------------------------------------------------

void i3c_controller::set_bus_model(unsigned inst, bus_model_fn fn)
{
    if (inst >= cfg_.num_instances) {
        SIM_LOG_WARN(this, "set_bus_model: instance " << inst << " out of range");
        return;
    }
    inst_[inst].bus_model = std::move(fn);
}

bool i3c_controller::inject_ibi(unsigned inst, uint8_t addr,
                                const std::vector<uint8_t>& payload)
{
    if (inst >= cfg_.num_instances) return false;
    inst_state& s = inst_[inst];

    const unsigned dwords = unsigned((payload.size() + 3) / 4);
    if (s.ibi_q.size() + 1 + dwords > cfg_.ibi_fifo_depth) return false;

    // IBI status descriptor: [7:0]=addr<<1, [15:8]=payload length (bytes).
    const uint32_t status = (uint32_t(addr & 0x7F) << 1) |
                            ((uint32_t(payload.size() & 0xFF)) << 8);
    s.ibi_q.push_back(status);
    for (unsigned d = 0; d < dwords; ++d) {
        uint32_t w = 0;
        for (unsigned b = 0; b < 4; ++b) {
            const size_t idx = size_t(d) * 4 + b;
            if (idx < payload.size()) w |= uint32_t(payload[idx]) << (8 * b);
        }
        s.ibi_q.push_back(w);
    }
    schedule_recompute();
    return true;
}

uint32_t i3c_controller::dbg_read(uint64_t off) const
{
    // Non-mutating snapshot read (does not pop FIFOs).
    const uint64_t aperture = uint64_t(cfg_.num_instances) *
                              i3c_controller_cfg::INSTANCE_SPACING;
    if (off >= aperture) return 0;
    const unsigned inst = unsigned(off / i3c_controller_cfg::INSTANCE_SPACING);
    const uint64_t loff = off % i3c_controller_cfg::INSTANCE_SPACING;
    const inst_state& s = inst_[inst];

    if (loff >= i3c_controller_cfg::DAT_BASE && loff < i3c_controller_cfg::DAT_END)
        return s.dat[(loff - i3c_controller_cfg::DAT_BASE) / 4];
    if (loff >= i3c_controller_cfg::DCT_BASE && loff < i3c_controller_cfg::DCT_END)
        return s.dct[(loff - i3c_controller_cfg::DCT_BASE) / 4];

    switch (loff) {
    case i3c_controller_cfg::HCI_VERSION:    return i3c_controller_cfg::HCI_VERSION_VALUE;
    case i3c_controller_cfg::HC_CONTROL:
        return (s.hc_control & hc_control::WMASK) | (1u << hc_control::MODE_SELECTOR);
    case i3c_controller_cfg::CONTROLLER_DEVICE_ADDR: return s.controller_device_addr;
    case i3c_controller_cfg::HC_CAPABILITIES: return compute_hc_capabilities();
    case i3c_controller_cfg::PRESENT_STATE:   return compute_present_state(s);
    case i3c_controller_cfg::INTR_STATUS:     return s.intr_status & hc_intr::W1C_MASK;
    case i3c_controller_cfg::INTR_STATUS_ENABLE: return s.intr_status_enable;
    case i3c_controller_cfg::INTR_SIGNAL_ENABLE: return s.intr_signal_enable;
    case i3c_controller_cfg::DAT_SECTION_OFFSET: return compute_dat_section_offset();
    case i3c_controller_cfg::DCT_SECTION_OFFSET: return compute_dct_section_offset(s);
    case i3c_controller_cfg::PIO_SECTION_OFFSET: return uint32_t(i3c_controller_cfg::COMMAND_PORT);
    case i3c_controller_cfg::QUEUE_THLD_CTRL:      return s.queue_thld_ctrl;
    case i3c_controller_cfg::DATA_BUFFER_THLD_CTRL: return s.data_buffer_thld_ctrl;
    case i3c_controller_cfg::QUEUE_SIZE:           return compute_queue_size(s);
    case i3c_controller_cfg::PIO_INTR_STATUS_ENABLE: return s.pio_intr_status_enable;
    case i3c_controller_cfg::PIO_INTR_SIGNAL_ENABLE: return s.pio_intr_signal_enable;
    case i3c_controller_cfg::PIO_CONTROL:          return s.pio_control;
    case i3c_controller_cfg::STBY_CR_EXTCAP_HEADER: return i3c_controller_cfg::STBY_CR_EXTCAP_HDR;
    case i3c_controller_cfg::STBY_CR_CONTROL:       return s.stby_cr_control;
    case i3c_controller_cfg::STBY_CR_DEVICE_ADDR:   return s.stby_cr_device_addr;
    default: return 0;
    }
}

void i3c_controller::dump_state(unsigned inst, std::ostream& os) const
{
    if (inst >= cfg_.num_instances) { os << "i3c_controller: bad instance\n"; return; }
    const inst_state& s = inst_[inst];
    os << "i3c_controller[" << inst << "] @ " << sc_core::sc_time_stamp() << "\n"
       << std::hex << std::setfill('0')
       << "  HC_CONTROL      = 0x" << std::setw(8)
       << ((s.hc_control & hc_control::WMASK) | (1u << hc_control::MODE_SELECTOR)) << "\n"
       << "  INTR_STATUS     = 0x" << std::setw(8) << (s.intr_status & hc_intr::W1C_MASK) << "\n"
       << "  PIO_INTR_STATUS = 0x" << std::setw(8) << (s.pio_intr_status & pio_intr::W1C_MASK) << "\n"
       << std::dec << std::setfill(' ')
       << "  queues: cmd=" << s.cmd_q.size() << " resp=" << s.resp_q.size()
       << " tx=" << s.tx_q.size() << " rx=" << s.rx_q.size()
       << " ibi=" << s.ibi_q.size()
       << "  bus_enable=" << bit(s.hc_control, hc_control::BUS_ENABLE)
       << "  irq=" << s.cache_irq << "\n";
}

} // namespace smc
