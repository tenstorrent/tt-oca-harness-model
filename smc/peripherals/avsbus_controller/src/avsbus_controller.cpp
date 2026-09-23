// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file avsbus_controller.cpp
 * @brief SMC AVSBus Controller — SystemC/TLM-2.0 LT implementation.
 */

#include "avsbus_controller.h"

#include "reg_access.h"
#include "sim_log.h"

#include <cstring>
#include <iomanip>

namespace smc {

// ===========================================================================
// CRC-3 (matches avsbus_crc3.sv)
// ===========================================================================

uint8_t avsbus_controller::crc3(uint32_t msg)
{
    const uint8_t c2 = static_cast<uint8_t>(
        ((msg >> 30) & 1) ^ ((msg >> 27) & 1) ^ ((msg >> 26) & 1) ^
        ((msg >> 25) & 1) ^ ((msg >> 23) & 1) ^ ((msg >> 20) & 1) ^
        ((msg >> 19) & 1) ^ ((msg >> 18) & 1) ^ ((msg >> 16) & 1) ^
        ((msg >> 13) & 1) ^ ((msg >> 12) & 1) ^ ((msg >> 11) & 1) ^
        ((msg >>  9) & 1) ^ ((msg >>  6) & 1) ^ ((msg >>  5) & 1) ^
        ((msg >>  4) & 1) ^ ((msg >>  2) & 1));
    const uint8_t c1 = static_cast<uint8_t>(
        ((msg >> 31) & 1) ^ ((msg >> 29) & 1) ^ ((msg >> 26) & 1) ^
        ((msg >> 25) & 1) ^ ((msg >> 24) & 1) ^ ((msg >> 22) & 1) ^
        ((msg >> 19) & 1) ^ ((msg >> 18) & 1) ^ ((msg >> 17) & 1) ^
        ((msg >> 15) & 1) ^ ((msg >> 12) & 1) ^ ((msg >> 11) & 1) ^
        ((msg >> 10) & 1) ^ ((msg >>  8) & 1) ^ ((msg >>  5) & 1) ^
        ((msg >>  4) & 1) ^ ((msg >>  3) & 1) ^ ((msg >>  1) & 1));
    const uint8_t c0 = static_cast<uint8_t>(
        ((msg >> 31) & 1) ^ ((msg >> 28) & 1) ^ ((msg >> 27) & 1) ^
        ((msg >> 26) & 1) ^ ((msg >> 24) & 1) ^ ((msg >> 21) & 1) ^
        ((msg >> 20) & 1) ^ ((msg >> 19) & 1) ^ ((msg >> 17) & 1) ^
        ((msg >> 14) & 1) ^ ((msg >> 13) & 1) ^ ((msg >> 12) & 1) ^
        ((msg >> 10) & 1) ^ ((msg >>  7) & 1) ^ ((msg >>  6) & 1) ^
        ((msg >>  5) & 1) ^ ((msg >>  3) & 1) ^ ((msg >>  0) & 1));
    return static_cast<uint8_t>((c2 << 2) | (c1 << 1) | c0);
}

// ===========================================================================
// Constructor
// ===========================================================================

avsbus_controller::avsbus_controller(sc_core::sc_module_name name,
                                     avsbus_controller_cfg cfg)
    : sc_core::sc_module(name)
    , command_fifo_depth_p_("command_fifo_depth", cfg.command_fifo_depth,
          "Command FIFO depth (immutable). Matches COMMAND_FIFO_DEPTH.")
    , readback_fifo_depth_p_("readback_fifo_depth", cfg.readback_fifo_depth,
          "Readback FIFO depth (immutable). Matches READBACK_FIFO_DEPTH.")
    , access_delay_ns_p_("access_delay_ns", cfg.access_delay_ns,
          "TLM register-access annotated delay in nanoseconds.")
    , xfer_delay_ns_p_("xfer_delay_ns", cfg.xfer_delay_ns,
          "Loosely-timed delay from command launch to slave response.")
    , resync_delay_ns_p_("resync_delay_ns", cfg.resync_delay_ns,
          "Modelled duration of a slave-resync pulse.")
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
    , irq_o("irq_o")
    , avs_gpio_enable_o("avs_gpio_enable_o")
    , cfg_(cfg)
{
    if (command_fifo_depth_p_.get_value() == 0 ||
        readback_fifo_depth_p_.get_value() == 0) {
        SC_REPORT_FATAL("avsbus_controller",
            "command_fifo_depth and readback_fifo_depth must be > 0");
    }

    // Plain storage registers; FIFO / status / IRQ stay in the switch.
    regmap_.add(avsbus_controller_cfg::AVS_INTERRUPT_MASK, "AVS_INTERRUPT_MASK", irq_mask_)
           .add(avsbus_controller_cfg::AVS_CFG_0,          "AVS_CFG_0",          cfg0_)
           .add(avsbus_controller_cfg::AVS_CFG_1,          "AVS_CFG_1",          cfg1_)
           .add(avsbus_controller_cfg::AVS_CONFIG,         "AVS_CONFIG",         config_);

    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    xfer_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    resync_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));

    SIM_LOG_INFO(this,
        "CCI config resolved:"
        << "  cmd_fifo=" << command_fifo_depth_p_.get_value()
        << "  rb_fifo="  << readback_fifo_depth_p_.get_value()
        << "  access_delay_ns=" << access_delay_ns_p_.get_value()
        << "  xfer_delay_ns="   << xfer_delay_ns_p_.get_value());

    reg_socket.register_b_transport  (this, &avsbus_controller::b_transport);
    reg_socket.register_transport_dbg(this, &avsbus_controller::transport_dbg);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    SC_METHOD(recompute_method);
    sensitive << recompute_event_;
    dont_initialize();

    SC_METHOD(resync_method);
    sensitive << resync_done_event_;
    dont_initialize();

    SC_THREAD(xfer_thread);

    // Drive reset-time outputs once elaboration can see the initial values.
    schedule_recompute();
}

// ===========================================================================
// Processes
// ===========================================================================

void avsbus_controller::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void avsbus_controller::schedule_xfer()
{
    if (xfer_pending_) return;
    if (cmd_fifo_.empty()) return;
    if (slave_in_resync_) return;
    // Readback-full back-pressure is handled inside complete_one_xfer so that
    // path stays reachable; we still schedule and early-out there.

    xfer_pending_ = true;
    bus_is_idle_  = false;
    xfer_event_.notify(sc_core::sc_time(xfer_delay_ns_p_.get_value(),
                                        sc_core::SC_NS));
    schedule_recompute();
}

void avsbus_controller::reset_proc()
{
    if (rst_n_i.read()) return;

    irq_mask_.reset(avsbus_controller_cfg::IRQ_MASK_RST);
    cfg0_.reset(avsbus_controller_cfg::CFG_0_RESET);
    cfg1_.reset(avsbus_controller_cfg::CFG_1_RESET);
    config_.reset(avsbus_controller_cfg::CONFIG_RESET);

    cmd_fifo_.clear();
    rb_fifo_.clear();

    irq_status_         = 0;
    latest_slave_frame_ = 0x0000FFFFu;
    total_retries_      = 0;
    master_is_retrying_ = false;
    bus_is_idle_        = true;
    slave_in_resync_    = false;
    xfer_event_.cancel();
    resync_done_event_.cancel();
    xfer_pending_       = false;
    retries_this_cmd_   = 0;
    current_cmd_        = 0;

    // Cancel pending timed events by marking idle; thread will re-check flags.
    schedule_recompute();
}

void avsbus_controller::recompute_method()
{
    const bool irq = irq_active();
    const bool gpio = (config_.raw() & avsbus_controller_cfg::CONFIG_MASK) != 0;

    if (!outputs_valid_ || irq != out_irq_) {
        out_irq_ = irq;
        irq_o.write(irq);
    }
    if (!outputs_valid_ || gpio != out_gpio_en_) {
        out_gpio_en_ = gpio;
        avs_gpio_enable_o.write(gpio);
    }
    outputs_valid_ = true;
}

void avsbus_controller::resync_method()
{
    slave_in_resync_ = false;
    schedule_recompute();
    schedule_xfer();
}

void avsbus_controller::xfer_thread()
{
    while (true) {
        wait(xfer_event_);
        // Reset may have cancelled the intent.
        if (!xfer_pending_) continue;
        complete_one_xfer();
    }
}

void avsbus_controller::start_resync()
{
    slave_in_resync_ = true;
    bus_is_idle_     = false;
    // Leave any in-flight xfer_pending_ set: when the timed event fires during
    // resync, complete_one_xfer early-outs and schedule_xfer resumes after
    // resync_method clears the flag.
    resync_done_event_.notify(sc_core::sc_time(resync_delay_ns_p_.get_value(),
                                               sc_core::SC_NS));
    schedule_recompute();
}

// ===========================================================================
// Interrupt helpers
// ===========================================================================

bool avsbus_controller::irq_active() const
{
    // Mask bit = 1 disables that source (RDL).
    return (irq_status_ & ~irq_mask_.raw() & avs_irq::ALL_MASK) != 0;
}

void avsbus_controller::set_irq_bit(unsigned bit)
{
    if (bit > 8) return;
    irq_status_ |= (1u << bit);
    schedule_recompute();
}

void avsbus_controller::clear_irq_bits(uint32_t w1c_data)
{
    irq_status_ = regmodel::apply_w1c(irq_status_, w1c_data, avs_irq::ALL_MASK);
    schedule_recompute();
}

void avsbus_controller::inject_interrupt(unsigned bit)
{
    set_irq_bit(bit);
}

// ===========================================================================
// Status builders
// ===========================================================================

unsigned avsbus_controller::max_retries() const
{
    return static_cast<unsigned>((cfg0_.raw() >> 16) & 0xFFu);
}

uint32_t avsbus_controller::normal_status() const
{
    uint32_t v = total_retries_ & 0xFFFFu;
    if (master_is_retrying_)                         v |= (1u << avs_status::MASTER_IS_RETRYING_SHIFT);
    if (cmd_fifo_.empty())                           v |= (1u << avs_status::CMD_FIFO_EMPTY_SHIFT);
    if (cmd_fifo_.size() >= cmd_depth())             v |= (1u << avs_status::CMD_FIFO_FULL_SHIFT);
    if (rb_fifo_.size()  >= rb_depth())              v |= (1u << avs_status::READBACK_FIFO_FULL_SHIFT);
    if (!rb_fifo_.empty())                           v |= (1u << avs_status::READBACK_HAS_DATA_SHIFT);
    if (bus_is_idle_ && !slave_in_resync_)           v |= (1u << avs_status::BUS_IS_IDLE_SHIFT);
    if (slave_in_resync_)                            v |= (1u << avs_status::SLAVE_IN_RESYNC_SHIFT);
    return v;
}

uint32_t avsbus_controller::fifos_status() const
{
    const unsigned cmd_occ = static_cast<unsigned>(cmd_fifo_.size());
    const unsigned rb_occ  = static_cast<unsigned>(rb_fifo_.size());
    const unsigned cmd_vac = (cmd_occ >= cmd_depth()) ? 0u : (cmd_depth() - cmd_occ);
    const unsigned rb_vac  = (rb_occ  >= rb_depth())  ? 0u : (rb_depth()  - rb_occ);
    return ((rb_vac  & 0xFu) << 24) |
           ((rb_occ  & 0xFu) << 16) |
           ((cmd_vac & 0xFu) <<  8) |
           ((cmd_occ & 0xFu) <<  0);
}

uint32_t avsbus_controller::slave_status() const
{
    const uint32_t ack = avs_rb::slave_ack(latest_slave_frame_);
    const uint32_t sr  = avs_rb::status_resp(latest_slave_frame_);
    return ((ack & 0x3u) << 16) | (sr & 0x1Fu);
}

// ===========================================================================
// Protocol
// ===========================================================================

uint32_t avsbus_controller::default_slave_response(uint32_t cmd) const
{
    // Synthetic happy-path response: ACK_OK, VDone+AVS_Control status,
    // echo write data / provide a canned read value, CRC field filled.
    const uint32_t r_or_w = avs_cmd::r_or_w(cmd);
    const uint32_t code   = avs_cmd::cmd_code(cmd);
    uint32_t data = 0xFFFFu;
    if (r_or_w == avs_cmd::READ) {
        switch (code) {
        case 0x0: data = 0x03E8u; break; // 1000 mV
        case 0x1: data = 0x1010u; break; // rise/fall rates
        case 0x2: data = 0x0064u; break; // 1.00 A (10 mA LSB)
        case 0x3: data = 0x00FAu; break; // 25.0 C
        case 0x5: data = 0x0003u; break; // max power
        case 0xE: data = 0x0000u; break; // status
        case 0xF: data = 0x0002u; break; // version 1.3.x-ish
        default:  data = 0xAAAAu; break;
        }
    } else {
        // After a write the readback CMD_DATA field is 0xFFFF per RDL.
        data = 0xFFFFu;
    }
    const uint32_t status = (1u << 4) | (1u << 2); // VDone | AVS_Control
    // Build a provisional subframe, then overwrite CRC with crc3 of the
    // wire-style message (preamble 01 | payload[29:3] | crc).
    uint32_t frame = avs_rb::pack(avs_rb::ACK_OK, status, data, 0);
    // For CRC, use a wire-like word: {2'b01, cmd[29:3], crc_placeholder}.
    const uint32_t wire_body = (0x1u << 30) | ((cmd >> 3) & 0x07FFFFFFu) << 3;
    const uint8_t  c = crc3(wire_body); // CRC over body with crc bits 0 → result
    // Recompute with CRC bits included as 0 is what the RTL generator does
    // when producing the TX CRC; for the response we just stamp `c`.
    frame = (frame & ~uint32_t{0x7}) | (c & 0x7u);
    return frame;
}

void avsbus_controller::push_cmd(uint32_t cmd)
{
    if (cmd_fifo_.size() >= cmd_depth()) {
        set_irq_bit(avs_irq::CMD_FIFO_OVERFLOW);
        // Drop — matches RTL (command discarded when FIFO full).
        return;
    }
    cmd_fifo_.push_back(cmd);
    if (cmd_fifo_.size() >= cmd_depth()) {
        set_irq_bit(avs_irq::CMD_FIFO_FULL);
    }
    schedule_recompute();
    schedule_xfer();
}

uint32_t avsbus_controller::peek_readback() const
{
    if (rb_fifo_.empty()) return 0xDEADBEEFu; // matches RTL empty-peek sentinel
    return rb_fifo_.front();
}

uint32_t avsbus_controller::pop_readback()
{
    if (rb_fifo_.empty()) {
        set_irq_bit(avs_irq::READBACK_UNDERFLOW);
        return 0u;
    }
    const uint32_t v = rb_fifo_.front();
    rb_fifo_.pop_front();
    // Level IRQs that track occupancy: clear sticky FULL/HAS_DATA only via
    // CLEAR register (RTL sticky).  Re-assert HAS_DATA if still non-empty.
    if (!rb_fifo_.empty()) {
        set_irq_bit(avs_irq::READBACK_HAS_DATA);
    }
    schedule_recompute();
    schedule_xfer(); // free slot may unblock a pending command
    return v;
}

void avsbus_controller::apply_response(uint32_t /*cmd*/, uint32_t resp,
                                       bool count_as_retry)
{
    latest_slave_frame_ = resp;

    if (count_as_retry) {
        total_retries_ = (total_retries_ + 1u) & 0xFFFFu;
    }

    if (rb_fifo_.size() >= rb_depth()) {
        set_irq_bit(avs_irq::READBACK_OVERFLOW);
        schedule_recompute();
        return;
    }
    rb_fifo_.push_back(resp);
    set_irq_bit(avs_irq::READBACK_HAS_DATA);
    if (rb_fifo_.size() >= rb_depth()) {
        set_irq_bit(avs_irq::READBACK_FIFO_FULL);
    }
    schedule_recompute();
}

void avsbus_controller::complete_one_xfer()
{
    xfer_pending_ = false;

    if (cmd_fifo_.empty() || slave_in_resync_) {
        bus_is_idle_ = cmd_fifo_.empty() && !slave_in_resync_;
        master_is_retrying_ = false;
        schedule_recompute();
        return;
    }
    if (rb_fifo_.size() >= rb_depth()) {
        // Back-pressure: wait for software to free a readback slot.
        bus_is_idle_ = false;
        schedule_recompute();
        return;
    }

    current_cmd_ = cmd_fifo_.front();
    const uint32_t resp = slave_model_ ? slave_model_(current_cmd_)
                                       : default_slave_response(current_cmd_);
    const uint32_t ack  = avs_rb::slave_ack(resp);

    const bool needs_retry = (ack == avs_rb::ACK_BUSY) || (ack == avs_rb::ACK_BAD_CRC);

    if (needs_retry) {
        master_is_retrying_ = true;
        total_retries_ = (total_retries_ + 1u) & 0xFFFFu;
        ++retries_this_cmd_;
        if (retries_this_cmd_ > max_retries()) {
            // Exhausted: push the failure response, drop the command, raise IRQ.
            master_is_retrying_ = false;
            retries_this_cmd_   = 0;
            cmd_fifo_.pop_front();
            apply_response(current_cmd_, resp, /*count_as_retry=*/false);
            set_irq_bit(avs_irq::MAX_RETRIES);
        } else {
            // Keep command at head; re-schedule another attempt.
            schedule_recompute();
            xfer_pending_ = true;
            xfer_event_.notify(sc_core::sc_time(xfer_delay_ns_p_.get_value(),
                                                sc_core::SC_NS));
            return;
        }
    } else {
        // Success or non-retryable failure (ACK_BAD_DATA / ACK_OK).
        master_is_retrying_ = false;
        retries_this_cmd_   = 0;
        cmd_fifo_.pop_front();
        apply_response(current_cmd_, resp, /*count_as_retry=*/false);
    }

    bus_is_idle_ = cmd_fifo_.empty();
    schedule_recompute();
    schedule_xfer(); // kick next command if any
}

// ===========================================================================
// Register decode
// ===========================================================================

bool avsbus_controller::reg_read(uint64_t off, uint32_t& data)
{
    switch (off) {
    case avsbus_controller_cfg::AVS_CMD:
        data = 0; // write-only → RAZ
        return true;
    case avsbus_controller_cfg::AVS_READBACK:
        data = pop_readback();
        return true;
    case avsbus_controller_cfg::AVS_DEBUG_READBACK:
        data = peek_readback();
        return true;
    case avsbus_controller_cfg::AVS_LATEST_SLAVE_SUBFRAME:
        data = latest_slave_frame_;
        return true;
    case avsbus_controller_cfg::AVS_NORMAL_STATUS:
        data = normal_status();
        return true;
    case avsbus_controller_cfg::AVS_SLAVE_STATUS:
        data = slave_status();
        return true;
    case avsbus_controller_cfg::AVS_FIFOS_STATUS:
        data = fifos_status();
        return true;
    case avsbus_controller_cfg::AVS_INTERRUPT:
        data = irq_status_ & avs_irq::ALL_MASK;
        return true;
    case avsbus_controller_cfg::AVS_INTERRUPT_CLEAR:
        data = 0; // write-only → RAZ
        return true;
    default:
        break;
    }
    if (uint32_t v = 0; regmap_.read(off, v)) { data = v; return true; }
    return false;
}

bool avsbus_controller::reg_write(uint64_t off, uint32_t data)
{
    switch (off) {
    case avsbus_controller_cfg::AVS_CMD:
        push_cmd(data);
        return true;
    case avsbus_controller_cfg::AVS_READBACK:
    case avsbus_controller_cfg::AVS_DEBUG_READBACK:
    case avsbus_controller_cfg::AVS_LATEST_SLAVE_SUBFRAME:
    case avsbus_controller_cfg::AVS_NORMAL_STATUS:
    case avsbus_controller_cfg::AVS_SLAVE_STATUS:
    case avsbus_controller_cfg::AVS_FIFOS_STATUS:
    case avsbus_controller_cfg::AVS_INTERRUPT:
        // Read-only / WI.
        return true;
    case avsbus_controller_cfg::AVS_INTERRUPT_CLEAR:
        clear_irq_bits(data);
        return true;
    case avsbus_controller_cfg::AVS_CFG_1: {
        // Detect FORCE_SLAVE_RESYNC_OPERATION (bit 9, singlepulse).
        const bool force = regmodel::bit(data, 9);
        cfg1_.write(data & ~uint32_t{1u << 9}); // pulse not sticky
        if (force) start_resync();
        schedule_recompute();
        return true;
    }
    default:
        break;
    }
    if (regmap_.write(off, data)) {
        schedule_recompute();
        return true;
    }
    return false;
}

// ===========================================================================
// TLM
// ===========================================================================

void avsbus_controller::b_transport(tlm::tlm_generic_payload& gp,
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
    if (len != avsbus_controller_cfg::REG_WIDTH) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (adr >= avsbus_controller_cfg::WINDOW_SIZE ||
        (adr % avsbus_controller_cfg::REG_WIDTH) != 0) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

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
    gp.set_dmi_allowed(false);
}

unsigned int avsbus_controller::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (len != avsbus_controller_cfg::REG_WIDTH ||
        (adr % avsbus_controller_cfg::REG_WIDTH) != 0 ||
        adr >= avsbus_controller_cfg::WINDOW_SIZE)
        return 0;

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = dbg_reg(adr);
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
// Back doors
// ===========================================================================

void avsbus_controller::set_slave_model(avs_slave_model_fn fn)
{
    slave_model_ = std::move(fn);
}

void avsbus_controller::inject_readback(uint32_t frame, bool count_as_retry)
{
    apply_response(/*cmd=*/0, frame, count_as_retry);
}

uint32_t avsbus_controller::dbg_reg(uint64_t off) const
{
    switch (off) {
    case avsbus_controller_cfg::AVS_CMD:                   return 0;
    case avsbus_controller_cfg::AVS_READBACK:              return peek_readback();
    case avsbus_controller_cfg::AVS_DEBUG_READBACK:        return peek_readback();
    case avsbus_controller_cfg::AVS_LATEST_SLAVE_SUBFRAME: return latest_slave_frame_;
    case avsbus_controller_cfg::AVS_NORMAL_STATUS:         return normal_status();
    case avsbus_controller_cfg::AVS_SLAVE_STATUS:          return slave_status();
    case avsbus_controller_cfg::AVS_FIFOS_STATUS:          return fifos_status();
    case avsbus_controller_cfg::AVS_INTERRUPT:             return irq_status_ & avs_irq::ALL_MASK;
    case avsbus_controller_cfg::AVS_INTERRUPT_CLEAR:       return 0;
    default: break;
    }
    if (uint32_t v = 0; regmap_.read(off, v)) return v;
    return 0u;
}

void avsbus_controller::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] AVSBus Controller state dump\n";
    os << "  CCI: cmd_fifo_depth=" << cmd_depth()
       << " rb_fifo_depth=" << rb_depth()
       << " access_delay_ns=" << access_delay_ns_p_.get_value()
       << " xfer_delay_ns=" << xfer_delay_ns_p_.get_value() << "\n";
    os << std::hex << std::setfill('0');
    os << "  irq_status=0x" << irq_status_
       << " irq_mask=0x" << irq_mask_.raw()
       << " cfg0=0x" << cfg0_.raw()
       << " cfg1=0x" << cfg1_.raw()
       << " config=0x" << config_.raw() << "\n";
    os << "  latest_slave=0x" << latest_slave_frame_
       << " total_retries=0x" << total_retries_ << "\n";
    os << std::dec << std::setfill(' ');
    os << "  cmd_fifo=" << cmd_fifo_.size()
       << " rb_fifo=" << rb_fifo_.size()
       << " idle=" << bus_is_idle_
       << " retrying=" << master_is_retrying_
       << " resync=" << slave_in_resync_
       << " irq=" << irq_active() << "\n";
}

} // namespace smc
