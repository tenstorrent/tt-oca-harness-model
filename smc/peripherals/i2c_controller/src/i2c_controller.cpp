// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file i2c_controller.cpp
 * @brief OCA I2C Controller — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/i2c_controller.h` for the full design description, register
 * map, and the functional-abstraction rationale.
 */

#include "i2c_controller.h"

#include "reg_access.h"
#include "sim_log.h"

#include <algorithm>
#include <cstring>
#include <iomanip>

namespace smc {

namespace {

// CONTROLLER_EVENTS bit positions (offset 0x78).
constexpr unsigned CE_NACK                   = 0;
constexpr unsigned CE_UNHANDLED_NACK_TIMEOUT = 1;
constexpr unsigned CE_BUS_TIMEOUT            = 2;
constexpr unsigned CE_ARBITRATION_LOST       = 3;

// TARGET_EVENTS bit positions (offset 0x7C).
constexpr unsigned TE_TX_PENDING      = 0;
constexpr unsigned TE_BUS_TIMEOUT     = 1;
constexpr unsigned TE_ARBITRATION_LOST = 2;
constexpr unsigned TE_START_DETECT    = 3;
constexpr unsigned TE_STOP_DETECT     = 4;

// TARGET_EVENTS bits that cause the target to stretch (drive TX_STRETCH).
constexpr uint32_t TE_STRETCH_MASK =
    (1u << TE_TX_PENDING) | (1u << TE_BUS_TIMEOUT) | (1u << TE_ARBITRATION_LOST);

constexpr uint32_t TARGET_NACK_SAT = 0xFFu; ///< TARGET_NACK_COUNT saturation.

} // anonymous namespace

// ===========================================================================
// Constructor
// ===========================================================================

i2c_controller::i2c_controller(sc_core::sc_module_name name,
                               i2c_controller_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params first (declared before ports/sized members in the header).
    , fmt_fifo_depth_p_("fmt_fifo_depth", cfg.fmt_fifo_depth,
          "Controller TX (FMT) FIFO depth in entries. "
          "Maps to the RTL FMTFIFO depth.")
    , rx_fifo_depth_p_("rx_fifo_depth", cfg.rx_fifo_depth,
          "Controller RX FIFO depth in bytes. Maps to the RTL RXFIFO depth.")
    , tx_fifo_depth_p_("tx_fifo_depth", cfg.tx_fifo_depth,
          "Target TX FIFO depth in bytes. Maps to the RTL TXFIFO depth.")
    , acq_fifo_depth_p_("acq_fifo_depth", cfg.acq_fifo_depth,
          "Target RX (ACQ) FIFO depth in entries. Maps to the RTL ACQFIFO depth.")
    , access_delay_ns_p_("access_delay_ns", cfg.access_delay_ns,
          "TLM register-access annotated delay in nanoseconds. "
          "Approximates AXI4-Lite bus latency. Mutable at run-time.")
    , xfer_delay_ns_p_("xfer_delay_ns", cfg.xfer_delay_ns,
          "Modelled FMT-drain / transaction latency in nanoseconds. Mutable.")
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
    , irq_o("irq_o")
    , cfg_(cfg)
{
    cfg_.fmt_fifo_depth = fmt_fifo_depth_p_.get_value();
    cfg_.rx_fifo_depth  = rx_fifo_depth_p_.get_value();
    cfg_.tx_fifo_depth  = tx_fifo_depth_p_.get_value();
    cfg_.acq_fifo_depth = acq_fifo_depth_p_.get_value();

    // Provenance metadata for tooling / introspection.
    fmt_fifo_depth_p_.add_metadata("rtl_param", cci::cci_value(std::string("FMTFIFO_DEPTH")));
    rx_fifo_depth_p_.add_metadata("rtl_param",  cci::cci_value(std::string("RXFIFO_DEPTH")));
    tx_fifo_depth_p_.add_metadata("rtl_param",  cci::cci_value(std::string("TXFIFO_DEPTH")));
    acq_fifo_depth_p_.add_metadata("rtl_param", cci::cci_value(std::string("ACQFIFO_DEPTH")));
    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    xfer_delay_ns_p_.add_metadata("unit",   cci::cci_value(std::string("nanoseconds")));

    if (cfg_.fmt_fifo_depth == 0 || cfg_.rx_fifo_depth == 0 ||
        cfg_.tx_fifo_depth == 0 || cfg_.acq_fifo_depth == 0)
        SC_REPORT_FATAL(name, "I2C FIFO depths must all be >= 1");

    xfer_delay_ = sc_core::sc_time(xfer_delay_ns_p_.get_value(), sc_core::SC_NS);

    // Offset -> storage-register dispatch table (common/include/reg_map.h).
    // Registers with model-side behaviour stay in the reg_read/reg_write switch.
    regmap_.add(i2c_controller_cfg::INTR_ENABLE,          "INTR_ENABLE",          intr_enable_)
           .add(i2c_controller_cfg::SMBUS_CTRL,           "SMBUS_CTRL",           smbus_ctrl_)
           .add(i2c_controller_cfg::CTRL,                 "CTRL",                 ctrl_)
           .add(i2c_controller_cfg::HOST_FIFO_CONFIG,     "HOST_FIFO_CONFIG",     host_fifo_config_)
           .add(i2c_controller_cfg::TARGET_FIFO_CONFIG,   "TARGET_FIFO_CONFIG",   target_fifo_config_)
           .add(i2c_controller_cfg::OVRD,                 "OVRD",                 ovrd_)
           .add(i2c_controller_cfg::VAL,                  "VAL",                  val_)
           .add(i2c_controller_cfg::TIMING0,              "TIMING0",              timing0_)
           .add(i2c_controller_cfg::TIMING1,              "TIMING1",              timing1_)
           .add(i2c_controller_cfg::TIMING2,              "TIMING2",              timing2_)
           .add(i2c_controller_cfg::TIMING3,              "TIMING3",              timing3_)
           .add(i2c_controller_cfg::TIMING4,              "TIMING4",              timing4_)
           .add(i2c_controller_cfg::TIMEOUT_CTRL,         "TIMEOUT_CTRL",         timeout_ctrl_)
           .add(i2c_controller_cfg::TARGET_ID,            "TARGET_ID",            target_id_)
           .add(i2c_controller_cfg::HOST_TIMEOUT_CTRL,    "HOST_TIMEOUT_CTRL",    host_timeout_ctrl_)
           .add(i2c_controller_cfg::TARGET_TIMEOUT_CTRL,  "TARGET_TIMEOUT_CTRL",  target_timeout_ctrl_)
           .add(i2c_controller_cfg::HOST_NACK_HANDLER_TIMEOUT, "HOST_NACK_HANDLER_TIMEOUT", nack_handler_timeout_)
           .add(i2c_controller_cfg::SMBUS_STATUS,         "SMBUS_STATUS",         smbus_status_);

    SIM_LOG_INFO(this,
        "CCI config resolved:"
        << "  fmt_fifo_depth=" << cfg_.fmt_fifo_depth
        << (fmt_fifo_depth_p_.is_preset_value() ? " [preset]" : " [default]")
        << "  rx_fifo_depth=" << cfg_.rx_fifo_depth
        << (rx_fifo_depth_p_.is_preset_value() ? " [preset]" : " [default]")
        << "  tx_fifo_depth=" << cfg_.tx_fifo_depth
        << "  acq_fifo_depth=" << cfg_.acq_fifo_depth
        << "  access_delay_ns=" << access_delay_ns_p_.get_value()
        << "  xfer_delay_ns=" << xfer_delay_ns_p_.get_value()
        << "  regmap_registers=" << regmap_.size());

    reg_socket.register_b_transport  (this, &i2c_controller::b_transport);
    reg_socket.register_transport_dbg(this, &i2c_controller::transport_dbg);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    // recompute_method is the SOLE driver of irq_o.
    SC_METHOD(recompute_method);
    sensitive << recompute_event_;
    dont_initialize();

    SC_METHOD(xfer_method);
    sensitive << xfer_event_;
    dont_initialize();
}

// ===========================================================================
// SC_METHOD processes
// ===========================================================================

void i2c_controller::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void i2c_controller::schedule_xfer()
{
    xfer_delay_ = sc_core::sc_time(xfer_delay_ns_p_.get_value(), sc_core::SC_NS);
    xfer_event_.notify(xfer_delay_);
}

void i2c_controller::reset_proc()
{
    if (rst_n_i.read()) return; // act only on assertion (low)

    intr_enable_.reset(0);
    smbus_ctrl_.reset(0);
    ctrl_.reset(0);
    host_fifo_config_.reset(0);
    target_fifo_config_.reset(0);
    ovrd_.reset(0);
    val_.reset(0);
    timing0_.reset(0); timing1_.reset(0); timing2_.reset(0);
    timing3_.reset(0); timing4_.reset(0);
    timeout_ctrl_.reset(0);
    target_id_.reset(0);
    host_timeout_ctrl_.reset(0);
    target_timeout_ctrl_.reset(0);
    nack_handler_timeout_.reset(0);
    smbus_status_.reset(0);

    intr_latched_      = 0;
    intr_force_        = 0;
    controller_events_ = 0;
    target_events_     = 0;
    target_ack_ctrl_   = 0;
    target_nack_count_ = 0;
    halted_            = false;

    fmt_.clear();
    rx_.clear();
    tx_.clear();
    acq_.clear();

    xfer_event_.cancel();
    schedule_recompute();
}

void i2c_controller::recompute_method()
{
    const bool irq = irq_active();
    if (!outputs_valid_ || irq != out_irq_) {
        out_irq_ = irq;
        irq_o.write(irq);
    }
    outputs_valid_ = true;
}

void i2c_controller::xfer_method()
{
    drain_fmt();
}

// ===========================================================================
// Controller / Target datapath
// ===========================================================================

void i2c_controller::push_fmt(uint32_t fdata)
{
    fmt_entry e;
    e.byte  = static_cast<uint8_t>((fdata >> i2c_fdata::FBYTE_LSB) & 0xFFu);
    e.start = regmodel::bit(fdata, i2c_fdata::START);
    e.stop  = regmodel::bit(fdata, i2c_fdata::STOP);
    e.readb = regmodel::bit(fdata, i2c_fdata::READB);
    e.rcont = regmodel::bit(fdata, i2c_fdata::RCONT);
    e.nakok = regmodel::bit(fdata, i2c_fdata::NAKOK);

    if (fmt_.size() >= cfg_.fmt_fifo_depth) {
        // FMT FIFO overflow -> parity/FIFO error interrupt; drop the entry.
        intr_latched_ |= (1u << i2c_intr::CONTROLLER_TX_FIFO_ERROR);
    } else {
        fmt_.push_back(e);
    }

    if ((ctrl_.read() & (1u << i2c_ctrl::ENABLEHOST)) && !halted_)
        schedule_xfer();
    schedule_recompute();
}

uint8_t i2c_controller::pop_rx()
{
    uint8_t v = 0;
    if (!rx_.empty()) {
        v = rx_.front();
        rx_.pop_front();
    }
    schedule_recompute();
    return v;
}

uint32_t i2c_controller::pop_acq()
{
    uint32_t v = 0;
    if (!acq_.empty()) {
        v = acq_.front().byte |
            (static_cast<uint32_t>(acq_.front().sig) << 8);
        acq_.pop_front();
    }
    schedule_recompute();
    return v;
}

void i2c_controller::apply_fifo_ctrl(uint32_t d)
{
    if (regmodel::bit(d, i2c_fifo_ctrl::RXRST))  rx_.clear();
    if (regmodel::bit(d, i2c_fifo_ctrl::FMTRST)) fmt_.clear();
    if (regmodel::bit(d, i2c_fifo_ctrl::ACQRST)) acq_.clear();
    if (regmodel::bit(d, i2c_fifo_ctrl::TXRST))  tx_.clear();
    schedule_recompute();
}

bool i2c_controller::target_match(uint8_t addr) const
{
    const uint32_t tid = target_id_.read();
    const uint8_t a0 = static_cast<uint8_t>( tid        & 0x7Fu);
    const uint8_t m0 = static_cast<uint8_t>((tid >> 7)  & 0x7Fu);
    const uint8_t a1 = static_cast<uint8_t>((tid >> 14) & 0x7Fu);
    const uint8_t m1 = static_cast<uint8_t>((tid >> 21) & 0x7Fu);
    if (m0 && ((addr & m0) == (a0 & m0))) return true;
    if (m1 && ((addr & m1) == (a1 & m1))) return true;
    return false;
}

void i2c_controller::acq_push(uint8_t byte, i2c_acq_signal sig)
{
    if (acq_.size() >= cfg_.acq_fifo_depth) {
        intr_latched_ |= (1u << i2c_intr::TARGET_RX_FIFO_ERROR);
        return;
    }
    acq_.push_back(acq_entry{byte, sig});
}

void i2c_controller::bump_target_nack()
{
    if (target_nack_count_ < TARGET_NACK_SAT) ++target_nack_count_;
}

void i2c_controller::drain_fmt()
{
    if (!(ctrl_.read() & (1u << i2c_ctrl::ENABLEHOST))) return;
    if (halted_) return;

    bool     seg_open  = false;
    bool     seg_nakok = false;
    uint8_t  seg_addr  = 0;
    i2c_dir  seg_dir   = i2c_dir::Write;
    unsigned seg_rlen  = 0;
    std::vector<uint8_t> seg_wdata;

    auto execute = [&]() {
        i2c_xfer x;
        x.addr       = seg_addr;
        x.dir        = seg_dir;
        x.write_data = seg_wdata;
        x.read_len   = seg_rlen;
        x.ack        = false;
        if (bus_model_) bus_model_(x);

        if (!x.ack) {
            if (!seg_nakok) {
                controller_events_ |= (1u << CE_NACK);
                halted_ = true;
            }
            return;
        }
        if (seg_dir == i2c_dir::Read) {
            for (uint8_t b : x.read_data) {
                if (rx_.size() >= cfg_.rx_fifo_depth)
                    intr_latched_ |= (1u << i2c_intr::RX_OVERFLOW);
                else
                    rx_.push_back(b);
            }
        }
    };

    while (!fmt_.empty()) {
        const fmt_entry e = fmt_.front();
        fmt_.pop_front();

        if (e.start) {
            if (seg_open) {
                execute();
                if (halted_) break;
            }
            seg_open  = true;
            seg_addr  = static_cast<uint8_t>(e.byte >> 1);
            seg_dir   = (e.byte & 1u) ? i2c_dir::Read : i2c_dir::Write;
            seg_rlen  = 0;
            seg_nakok = e.nakok;
            seg_wdata.clear();
        } else if (seg_open) {
            if (e.readb || seg_dir == i2c_dir::Read) {
                seg_dir  = i2c_dir::Read;
                seg_rlen += (e.byte == 0) ? 256u : e.byte;
            } else {
                seg_wdata.push_back(e.byte);
            }
            seg_nakok = seg_nakok || e.nakok;
        }
        // A non-START entry with no open segment is a programming error; the
        // RTL would treat it as stray data. Ignore it here.

        if (e.stop) {
            if (seg_open) {
                execute();
                seg_open = false;
            }
            if (!halted_)
                intr_latched_ |= (1u << i2c_intr::CMD_COMPLETE);
            if (halted_) break;
        }
    }

    // A trailing open segment (no STOP yet) is executed so reads surface
    // immediately in this loosely-timed model.
    if (seg_open && !halted_)
        execute();

    schedule_recompute();
}

// ===========================================================================
// Status / interrupt computation
// ===========================================================================

uint32_t i2c_controller::compute_status() const
{
    uint32_t s = 0;
    const bool fmt_full  = fmt_.size() >= cfg_.fmt_fifo_depth;
    const bool fmt_empty = fmt_.empty();
    const bool rx_full   = rx_.size()  >= cfg_.rx_fifo_depth;
    const bool rx_empty  = rx_.empty();
    const bool tx_full   = tx_.size()  >= cfg_.tx_fifo_depth;
    const bool tx_empty  = tx_.empty();
    const bool acq_full  = acq_.size() >= cfg_.acq_fifo_depth;
    const bool acq_empty = acq_.empty();

    if (fmt_full)                 s |= (1u << i2c_status::FMTFULL);
    if (rx_full)                  s |= (1u << i2c_status::RXFULL);
    if (fmt_empty)                s |= (1u << i2c_status::FMTEMPTY);
    if (fmt_empty && !halted_)    s |= (1u << i2c_status::HOSTIDLE);
    s |= (1u << i2c_status::TARGETIDLE); // functionally always idle (LT)
    if (rx_empty)                 s |= (1u << i2c_status::RXEMPTY);
    if (tx_full)                  s |= (1u << i2c_status::TXFULL);
    if (acq_full)                 s |= (1u << i2c_status::ACQFULL);
    if (tx_empty)                 s |= (1u << i2c_status::TXEMPTY);
    if (acq_empty)                s |= (1u << i2c_status::ACQEMPTY);
    return s;
}

uint32_t i2c_controller::compute_host_fifo_status() const
{
    const uint32_t fmtlvl = static_cast<uint32_t>(fmt_.size()) & 0xFFFu;
    const uint32_t rxlvl  = static_cast<uint32_t>(rx_.size())  & 0xFFFu;
    return fmtlvl | (rxlvl << 16);
}

uint32_t i2c_controller::compute_target_fifo_status() const
{
    const uint32_t txlvl  = static_cast<uint32_t>(tx_.size())  & 0xFFFu;
    const uint32_t acqlvl = static_cast<uint32_t>(acq_.size()) & 0xFFFu;
    return txlvl | (acqlvl << 16);
}

uint32_t i2c_controller::level_status() const
{
    uint32_t s = 0;

    const uint32_t hfc = host_fifo_config_.read();
    const unsigned rx_thresh  = hfc & 0xFFFu;
    const unsigned fmt_thresh = (hfc >> 16) & 0xFFFu;

    const uint32_t tfc = target_fifo_config_.read();
    const unsigned tx_thresh  = tfc & 0xFFFu;
    const unsigned acq_thresh = (tfc >> 16) & 0xFFFu;

    if (fmt_.size() < fmt_thresh) s |= (1u << i2c_intr::FMT_THRESHOLD);
    if (rx_.size()  > rx_thresh)  s |= (1u << i2c_intr::RX_THRESHOLD);
    if (acq_.size() > acq_thresh) s |= (1u << i2c_intr::ACQ_THRESHOLD);
    if (controller_events_ != 0)  s |= (1u << i2c_intr::CONTROLLER_HALT);
    if (target_events_ & TE_STRETCH_MASK) s |= (1u << i2c_intr::TX_STRETCH);
    if (tx_.size() < tx_thresh)   s |= (1u << i2c_intr::TX_THRESHOLD);
    // ACQ_STRETCH (software ACK-control stretch) is not modelled functionally.
    return s;
}

uint32_t i2c_controller::intr_state_read() const
{
    return level_status() | intr_latched_ | intr_force_;
}

bool i2c_controller::irq_active() const
{
    return (intr_state_read() & intr_enable_.read()) != 0;
}

// ===========================================================================
// Register decode
// ===========================================================================

bool i2c_controller::reg_read(uint64_t off, uint32_t& data)
{
    switch (off) {
    case i2c_controller_cfg::INTR_STATE:         data = intr_state_read();          return true;
    case i2c_controller_cfg::INTR_TEST:          data = intr_force_;                return true;
    case i2c_controller_cfg::STATUS:             data = compute_status();           return true;
    case i2c_controller_cfg::RDATA:              data = pop_rx();                   return true;
    case i2c_controller_cfg::FDATA:              data = 0;                          return true; // WO
    case i2c_controller_cfg::FIFO_CTRL:          data = 0;                          return true; // WO
    case i2c_controller_cfg::HOST_FIFO_STATUS:   data = compute_host_fifo_status(); return true;
    case i2c_controller_cfg::TARGET_FIFO_STATUS: data = compute_target_fifo_status();return true;
    case i2c_controller_cfg::ACQDATA:            data = pop_acq();                  return true;
    case i2c_controller_cfg::TXDATA:             data = 0;                          return true; // WO
    case i2c_controller_cfg::TARGET_NACK_COUNT:
        data = target_nack_count_;
        target_nack_count_ = 0; // rclr: read clears the count
        return true;
    case i2c_controller_cfg::TARGET_ACK_CTRL:    data = target_ack_ctrl_;           return true;
    case i2c_controller_cfg::ACQ_FIFO_NEXT_DATA:
        data = acq_.empty() ? 0u : acq_.front().byte;
        return true;
    case i2c_controller_cfg::CONTROLLER_EVENTS:  data = controller_events_;         return true;
    case i2c_controller_cfg::TARGET_EVENTS:      data = target_events_;             return true;
    default: break;
    }

    // Plain storage registers (INTR_ENABLE, CTRL, timing, thresholds, ...).
    if (uint32_t v = 0; regmap_.read(off, v)) { data = v; return true; }
    return false; // decode miss inside window -> ADDRESS_ERROR
}

bool i2c_controller::reg_write(uint64_t off, uint32_t data)
{
    switch (off) {
    case i2c_controller_cfg::INTR_STATE:
        intr_latched_ = regmodel::apply_w1c(intr_latched_, data, i2c_intr::W1C_MASK);
        schedule_recompute();
        return true;
    case i2c_controller_cfg::INTR_TEST:
        intr_latched_ |= (data & i2c_intr::W1C_MASK);
        intr_force_    = (data & i2c_intr::LEVEL_MASK);
        schedule_recompute();
        return true;
    case i2c_controller_cfg::CTRL:
        regmap_.write(off, data);
        if ((ctrl_.read() & (1u << i2c_ctrl::ENABLEHOST)) && !halted_ && !fmt_.empty())
            schedule_xfer();
        schedule_recompute();
        return true;
    case i2c_controller_cfg::FDATA:
        push_fmt(data);
        return true;
    case i2c_controller_cfg::FIFO_CTRL:
        apply_fifo_ctrl(data);
        return true;
    case i2c_controller_cfg::TXDATA:
        if (tx_.size() >= cfg_.tx_fifo_depth)
            intr_latched_ |= (1u << i2c_intr::TARGET_TX_FIFO_ERROR);
        else
            tx_.push_back(static_cast<uint8_t>(data & 0xFFu));
        schedule_recompute();
        return true;
    case i2c_controller_cfg::TARGET_NACK_COUNT:
        target_nack_count_ = data & TARGET_NACK_SAT;
        return true;
    case i2c_controller_cfg::TARGET_ACK_CTRL:
        target_ack_ctrl_ = data & i2c_controller_cfg::TARGET_ACK_NBYTES;
        if (data & 0x80000000u) bump_target_nack(); // NACK pulse
        schedule_recompute();
        return true;
    case i2c_controller_cfg::CONTROLLER_EVENTS:
        controller_events_ =
            regmodel::apply_w1c(controller_events_, data,
                                i2c_controller_cfg::CONTROLLER_EVT_MASK);
        if (controller_events_ == 0) halted_ = false; // clearing events resumes
        if ((ctrl_.read() & (1u << i2c_ctrl::ENABLEHOST)) && !halted_ && !fmt_.empty())
            schedule_xfer();
        schedule_recompute();
        return true;
    case i2c_controller_cfg::TARGET_EVENTS:
        target_events_ =
            regmodel::apply_w1c(target_events_, data,
                                i2c_controller_cfg::TARGET_EVT_MASK);
        schedule_recompute();
        return true;
    // Read-only registers: writes are silently ignored (WI).
    case i2c_controller_cfg::STATUS:
    case i2c_controller_cfg::HOST_FIFO_STATUS:
    case i2c_controller_cfg::TARGET_FIFO_STATUS:
    case i2c_controller_cfg::VAL:
    case i2c_controller_cfg::SMBUS_STATUS:
    case i2c_controller_cfg::ACQ_FIFO_NEXT_DATA:
    case i2c_controller_cfg::RDATA:
    case i2c_controller_cfg::ACQDATA:
        return true;
    default: break;
    }

    // Plain storage registers (mask contract enforced by the register type).
    if (regmap_.write(off, data)) { schedule_recompute(); return true; }
    return false; // decode miss inside window -> ADDRESS_ERROR
}

// ===========================================================================
// TLM-2.0 callbacks
// ===========================================================================

void i2c_controller::b_transport(tlm::tlm_generic_payload& gp,
                                 sc_core::sc_time& delay)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (cmd != tlm::TLM_READ_COMMAND && cmd != tlm::TLM_WRITE_COMMAND) {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (len != 4) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (adr >= i2c_controller_cfg::WINDOW_SIZE || (adr & 0x3u) != 0) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // Optional AXI sideband extension is forwarded but not enforced here.
    smc::smc_axi_extension* ext = nullptr;
    gp.get_extension(ext);
    (void)ext;

    bool ok;
    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = 0;
        ok = reg_read(adr, v);
        if (ok) std::memcpy(buf, &v, 4);
        SIM_LOG_TRACE(this, "read  off=0x" << std::hex << adr
                            << " data=0x" << v << (ok ? "" : " [decode-miss]"));
    } else {
        uint32_t v = 0;
        std::memcpy(&v, buf, 4);
        ok = reg_write(adr, v);
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << adr
                            << " data=0x" << v << (ok ? "" : " [decode-miss]"));
    }

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                              : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    gp.set_dmi_allowed(false); // RDATA/ACQDATA reads have side effects
}

unsigned int i2c_controller::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (len != 4 || (adr & 0x3u) != 0 || adr >= i2c_controller_cfg::WINDOW_SIZE)
        return 0;

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = dbg_reg(adr); // side-effect-free peek
        std::memcpy(buf, &v, 4);
        return 4;
    }
    if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t v = 0;
        std::memcpy(&v, buf, 4);
        if (!reg_write(adr, v)) return 0;
        return 4;
    }
    return 0;
}

// ===========================================================================
// Test-bench back door
// ===========================================================================

void i2c_controller::set_bus_model(i2c_bus_model_fn fn)
{
    bus_model_ = std::move(fn);
}

bool i2c_controller::target_write(uint8_t addr, const std::vector<uint8_t>& data,
                                  bool stop)
{
    if (!(ctrl_.read() & (1u << i2c_ctrl::ENABLETARGET)) || !target_match(addr)) {
        bump_target_nack();
        return false;
    }
    acq_push(static_cast<uint8_t>(addr << 1), i2c_acq_signal::Start);
    for (uint8_t b : data) acq_push(b, i2c_acq_signal::Data);
    target_events_ |= (1u << TE_START_DETECT);
    if (stop) {
        acq_push(0, i2c_acq_signal::Stop);
        target_events_ |= (1u << TE_STOP_DETECT);
    }
    schedule_recompute();
    return true;
}

bool i2c_controller::target_read(uint8_t addr, unsigned nbytes,
                                 std::vector<uint8_t>& out, bool stop)
{
    out.clear();
    if (!(ctrl_.read() & (1u << i2c_ctrl::ENABLETARGET)) || !target_match(addr)) {
        bump_target_nack();
        return false;
    }
    acq_push(static_cast<uint8_t>((addr << 1) | 1u), i2c_acq_signal::Start);
    for (unsigned i = 0; i < nbytes && !tx_.empty(); ++i) {
        out.push_back(tx_.front());
        tx_.pop_front();
    }
    target_events_ |= (1u << TE_START_DETECT);
    if (stop) {
        acq_push(0, i2c_acq_signal::Stop);
        target_events_ |= (1u << TE_STOP_DETECT);
    }
    schedule_recompute();
    return true;
}

uint32_t i2c_controller::dbg_reg(uint64_t off) const
{
    switch (off) {
    case i2c_controller_cfg::INTR_STATE:         return intr_state_read();
    case i2c_controller_cfg::INTR_TEST:          return intr_force_;
    case i2c_controller_cfg::STATUS:             return compute_status();
    case i2c_controller_cfg::RDATA:              return rx_.empty() ? 0u : rx_.front();
    case i2c_controller_cfg::FDATA:              return 0u;
    case i2c_controller_cfg::FIFO_CTRL:          return 0u;
    case i2c_controller_cfg::HOST_FIFO_STATUS:   return compute_host_fifo_status();
    case i2c_controller_cfg::TARGET_FIFO_STATUS: return compute_target_fifo_status();
    case i2c_controller_cfg::ACQDATA:
        return acq_.empty() ? 0u
             : (acq_.front().byte |
                (static_cast<uint32_t>(acq_.front().sig) << 8));
    case i2c_controller_cfg::TXDATA:             return 0u;
    case i2c_controller_cfg::TARGET_NACK_COUNT:  return target_nack_count_;
    case i2c_controller_cfg::TARGET_ACK_CTRL:    return target_ack_ctrl_;
    case i2c_controller_cfg::ACQ_FIFO_NEXT_DATA: return acq_.empty() ? 0u : acq_.front().byte;
    case i2c_controller_cfg::CONTROLLER_EVENTS:  return controller_events_;
    case i2c_controller_cfg::TARGET_EVENTS:      return target_events_;
    default: break;
    }
    if (uint32_t v = 0; regmap_.read(off, v)) return v;
    return 0u;
}

unsigned i2c_controller::dbg_rx_count()  const { return static_cast<unsigned>(rx_.size());  }
unsigned i2c_controller::dbg_fmt_count() const { return static_cast<unsigned>(fmt_.size()); }
unsigned i2c_controller::dbg_tx_count()  const { return static_cast<unsigned>(tx_.size());  }
unsigned i2c_controller::dbg_acq_count() const { return static_cast<unsigned>(acq_.size()); }

void i2c_controller::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] I2C controller state dump\n";
    os << "  CCI parameters:\n"
       << "    fmt_fifo_depth=" << cfg_.fmt_fifo_depth
       << (fmt_fifo_depth_p_.is_preset_value() ? " [preset]" : " [default]") << "\n"
       << "    rx_fifo_depth="  << cfg_.rx_fifo_depth
       << (rx_fifo_depth_p_.is_preset_value() ? " [preset]" : " [default]") << "\n"
       << "    tx_fifo_depth="  << cfg_.tx_fifo_depth  << "\n"
       << "    acq_fifo_depth=" << cfg_.acq_fifo_depth << "\n"
       << "    access_delay_ns=" << access_delay_ns_p_.get_value()
       << (access_delay_ns_p_.is_preset_value() ? " [preset]" : " [default]") << "\n"
       << "    xfer_delay_ns="   << xfer_delay_ns_p_.get_value() << "\n";
    os << std::hex << std::setfill('0');
    os << "  CTRL=0x"  << ctrl_.read()
       << " INTR_STATE=0x" << intr_state_read()
       << " INTR_ENABLE=0x" << intr_enable_.read()
       << " CONTROLLER_EVENTS=0x" << controller_events_
       << " TARGET_EVENTS=0x" << target_events_ << "\n";
    os << std::dec << std::setfill(' ');
    os << "  FIFOs: fmt=" << fmt_.size() << " rx=" << rx_.size()
       << " tx=" << tx_.size() << " acq=" << acq_.size()
       << " halted=" << halted_
       << " target_nack_count=" << target_nack_count_ << "\n";
    os << "  irq=" << irq_active() << "\n";
}

} // namespace smc
