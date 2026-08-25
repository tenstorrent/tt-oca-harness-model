// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file telemetry_receiver.cpp
 * @brief SMC Telemetry Receiver — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/telemetry_receiver.h` for the full design description, register
 * map, ATB message format, and the list of deviations from the RTL.
 */

#include "telemetry_receiver.h"

#include "reg_access.h"
#include "sim_log.h"

#include <cstring>
#include <iomanip>

namespace smc {

namespace {

/// Bits needed to hold values `0 .. v` (ceil(log2(v))), as RTL `$clog2`.
unsigned clog2(unsigned v)
{
    unsigned r = 0;
    while ((1u << r) < v) ++r;
    return r;
}

} // namespace

// ===========================================================================
// Reference encoder (inverse of decode_message)
// ===========================================================================

std::vector<uint8_t> telemetry_encode_message(
    uint8_t probe_id,
    const std::vector<telemetry_counter_value>& counters,
    unsigned max_counters,
    bool set_last_packet)
{
    using cfg = telemetry_receiver_cfg;

    const unsigned num_packets = telemetry_packets_per_message(max_counters);
    std::vector<uint64_t> packets(num_packets, 0);

    // blocks[0] of packet 0 is the header: PROBE_ID sits at packet bits
    // [60:56], i.e. bits [6:2] of the header block's payload byte.
    telemetry_set_packet_block(
        packets[0], 0, true,
        static_cast<uint8_t>((probe_id & cfg::PROBE_ID_MASK)
                             << (cfg::PROBE_ID_LSB - cfg::BLOCK0_DATA_LSB)));

    // Counter payload, MSB byte first, four blocks per counter, running across
    // packet boundaries.
    unsigned pkt = 0;
    unsigned blk = 1;
    for (unsigned i = 0; i < max_counters; ++i) {
        const telemetry_counter_value c =
            (i < counters.size()) ? counters[i] : telemetry_counter_value{};
        for (int j = static_cast<int>(cfg::COUNTER_BYTES) - 1; j >= 0; --j) {
            telemetry_set_packet_block(
                packets[pkt], blk, c.vld,
                static_cast<uint8_t>((c.value >> (8 * j)) & 0xFFu));
            if (blk == cfg::BLOCKS_PER_PACKET - 1) { ++pkt; blk = 0; }
            else                                   { ++blk; }
        }
    }

    if (set_last_packet)
        packets[num_packets - 1] |= (uint64_t{1} << cfg::PACKET_LAST_BIT);

    // Flatten to byte beats: beat i of a packet carries packet bits [8i+7:8i].
    std::vector<uint8_t> beats;
    beats.reserve(num_packets * cfg::BEATS_PER_PACKET);
    for (uint64_t w : packets)
        for (unsigned i = 0; i < cfg::BEATS_PER_PACKET; ++i)
            beats.push_back(static_cast<uint8_t>((w >> (8 * i)) & 0xFFu));

    return beats;
}

// ===========================================================================
// Constructor
// ===========================================================================

telemetry_receiver::telemetry_receiver(sc_core::sc_module_name name,
                                     telemetry_receiver_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params first: the immutable ones size the buffers below.
    , buffer_depth_p_("buffer_depth", cfg.buffer_depth,
          "Telemetry message buffer depth in messages (>= 2). Sizes the "
          "circular queue and the BUFFER_THRESHOLD compare width.")
    , max_counters_p_("max_counters_per_message", cfg.max_counters_per_message,
          "Counters carried by one telemetry message (1..32). Sizes the ATB "
          "assembly buffer; TELEMETRY_COUNTER regs above this read 0.")
    , access_delay_ns_p_("access_delay_ns", cfg.access_delay_ns,
          "TLM register-access annotated delay in nanoseconds. "
          "Approximates AXI4-Lite bus latency. Mutable at run-time.")
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
    , afready_i("afready_i")
    , irq_o("irq_o")
    , afvalid_o("afvalid_o")
    , atready_o("atready_o")
    , debug_o("debug_o")
    , cfg_(cfg)
{
    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));

    // Adopt any CCI presets, then validate.
    cfg_.buffer_depth             = buffer_depth_p_.get_value();
    cfg_.max_counters_per_message = max_counters_p_.get_value();

    if (cfg_.buffer_depth < 2)
        SC_REPORT_FATAL(this->name(), "buffer_depth must be >= 2 "
                                      "(telemetry_receiver.sv paramCheckBufferDepth)");
    if (cfg_.max_counters_per_message < 1 ||
        cfg_.max_counters_per_message > telemetry_receiver_cfg::NUM_COUNTER_REGS)
        SC_REPORT_FATAL(this->name(),
                        "max_counters_per_message must be in 1..NUM_COUNTER_REGS(32)");

    threshold_wrap_mask_ = (1u << (clog2(cfg_.buffer_depth) + 1u)) - 1u;

    assembly_.assign(telemetry_packets_per_message(cfg_.max_counters_per_message) *
                     telemetry_receiver_cfg::BEATS_PER_PACKET, 0u);
    queue_.assign(cfg_.buffer_depth, telemetry_message{});
    empty_message_.counters.assign(cfg_.max_counters_per_message,
                                   telemetry_counter_value{});

    // Offset -> storage-register dispatch table (common/include/reg_map.h).
    // STATUS / INTR_STATUS / PROBE_ID / COUNTER* are hardware-computed and
    // CTRL / INTR_TEST carry write side effects, so those stay in reg_write().
    regmap_.add(telemetry_receiver_cfg::CTRL,        "CTRL",        ctrl_)
           .add(telemetry_receiver_cfg::INTR_ENABLE, "INTR_ENABLE", intr_enable_)
           .add(telemetry_receiver_cfg::INTR_TEST,   "INTR_TEST",   intr_test_);

    SIM_LOG_INFO(this,
        "CCI config resolved:"
        << "  buffer_depth=" << cfg_.buffer_depth
        << (buffer_depth_p_.is_preset_value() ? " [preset]" : " [default]")
        << "  max_counters_per_message=" << cfg_.max_counters_per_message
        << (max_counters_p_.is_preset_value() ? " [preset]" : " [default]")
        << "  access_delay_ns=" << access_delay_ns_p_.get_value()
        << (access_delay_ns_p_.is_preset_value() ? " [preset]" : " [default]")
        << "  assembly_beats=" << assembly_.size()
        << "  threshold_wrap_mask=0x" << std::hex << threshold_wrap_mask_);

    reg_socket.register_b_transport  (this, &telemetry_receiver::b_transport);
    reg_socket.register_transport_dbg(this, &telemetry_receiver::transport_dbg);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    SC_METHOD(af_handshake_method);
    sensitive << afready_i << af_event_;
    dont_initialize();

    // recompute_method is the SOLE driver of every output port.
    SC_METHOD(recompute_method);
    sensitive << recompute_event_;
    dont_initialize();
}

// ===========================================================================
// SC_METHOD processes
// ===========================================================================

void telemetry_receiver::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void telemetry_receiver::start_of_simulation()
{
    // Drive the outputs once so they are defined before any stimulus.
    schedule_recompute();
}

void telemetry_receiver::reset_proc()
{
    if (!rst_n_i.read()) {
        // Reset asserted: clear every storage element.
        ctrl_.reset(0);
        intr_enable_.reset(0);
        intr_test_.reset(0);
        missing_last_status_ = false;
        rx_flush();          // assembly buffer + message queue + debug latches
        accepting_ = false;  // atready_o low while in reset
    } else {
        accepting_ = true;
    }
    schedule_recompute();
}

void telemetry_receiver::af_handshake_method()
{
    // RTL: CTRL.TELEMETRY_TX_FLUSH.hwclr = afready_i && afvalid_o.
    if (afready_i.read() &&
        (ctrl_.raw() & telemetry_receiver_cfg::CTRL_TX_FLUSH) != 0) {
        ctrl_.set_raw(ctrl_.raw() & ~telemetry_receiver_cfg::CTRL_TX_FLUSH);
        SIM_LOG_DEBUG(this, "TX flush acknowledged; CTRL.TELEMETRY_TX_FLUSH cleared");
        schedule_recompute();
    }
}

void telemetry_receiver::recompute_method()
{
    const bool irq     = missing_last_status_ || threshold_irq_active();
    const bool afvalid = (ctrl_.raw() & telemetry_receiver_cfg::CTRL_TX_FLUSH) != 0;
    const bool atready = accepting_;

    uint32_t dbg = 0;
    if (dbg_missing_last_)  dbg |= telemetry_receiver_cfg::DBG_MISSING_LAST;
    if (count_ == cfg_.buffer_depth) dbg |= telemetry_receiver_cfg::DBG_BUFFER_FULL;
    if (count_ == 0)        dbg |= telemetry_receiver_cfg::DBG_BUFFER_EMPTY;
    if (dbg_assembly_full_) dbg |= telemetry_receiver_cfg::DBG_ASSEMBLY_FULL;

    if (!outputs_valid_ || irq != out_irq_) {
        out_irq_ = irq;
        irq_o.write(irq);
    }
    if (!outputs_valid_ || afvalid != out_afvalid_) {
        out_afvalid_ = afvalid;
        afvalid_o.write(afvalid);
    }
    if (!outputs_valid_ || atready != out_atready_) {
        out_atready_ = atready;
        atready_o.write(atready);
    }
    if (!outputs_valid_ || dbg != out_debug_) {
        out_debug_ = dbg;
        debug_o.write(dbg);
    }
    outputs_valid_ = true;
}

// ===========================================================================
// Interrupt aggregation
// ===========================================================================

unsigned telemetry_receiver::threshold_compare_value() const
{
    // RTL casts CTRL.BUFFER_THRESHOLD to message_buffer_ptr_t before
    // comparing, so values wider than the pointer truncate.
    const uint32_t field = (ctrl_.raw() & telemetry_receiver_cfg::CTRL_THRESHOLD_MASK) >>
                           telemetry_receiver_cfg::CTRL_THRESHOLD_SHIFT;
    return field & threshold_wrap_mask_;
}

bool telemetry_receiver::threshold_irq_active() const
{
    const bool test = (intr_test_.raw() &
                       telemetry_receiver_cfg::INTR_BUFFER_THRESHOLD) != 0;
    const bool en   = (intr_enable_.raw() &
                       telemetry_receiver_cfg::INTR_BUFFER_THRESHOLD) != 0;
    return ((count_ > threshold_compare_value()) || test) && en;
}

void telemetry_receiver::set_missing_last_status()
{
    // RTL gates the status update with INTR_ENABLE.MISSING_LAST at the moment
    // the event occurs; clearing the enable later does not clear the status.
    if ((intr_enable_.raw() & telemetry_receiver_cfg::INTR_MISSING_LAST) != 0)
        missing_last_status_ = true;
}

void telemetry_receiver::raise_missing_last()
{
    dbg_missing_last_ = true;
    set_missing_last_status();
    SIM_LOG_DEBUG(this, "missing-last event: assembly buffer filled without "
                        "a last_packet marker; partial message discarded");
}

// ===========================================================================
// Telemetry datapath
// ===========================================================================

uint64_t telemetry_receiver::packet_word(unsigned packet_index) const
{
    // Beat i of a packet carries packet bits [8i+7:8i].
    const unsigned base = packet_index * telemetry_receiver_cfg::BEATS_PER_PACKET;
    uint64_t w = 0;
    for (unsigned i = 0; i < telemetry_receiver_cfg::BEATS_PER_PACKET; ++i)
        w |= static_cast<uint64_t>(assembly_[base + i]) << (8 * i);
    return w;
}

telemetry_message telemetry_receiver::decode_message() const
{
    using cfg = telemetry_receiver_cfg;

    telemetry_message msg;
    msg.probe_id = telemetry_packet_probe_id(packet_word(0));
    msg.counters.assign(cfg_.max_counters_per_message, telemetry_counter_value{});

    // Counter payload starts at blocks[1] of packet 0 (blocks[0] is the
    // header) and runs MSB byte first, four blocks per counter.
    unsigned pkt = 0;
    unsigned blk = 1;
    for (unsigned i = 0; i < cfg_.max_counters_per_message; ++i) {
        bool     vld = true;
        uint32_t val = 0;
        for (int j = static_cast<int>(cfg::COUNTER_BYTES) - 1; j >= 0; --j) {
            const telemetry_block b = telemetry_packet_block(packet_word(pkt), blk);
            vld = vld && b.vld;
            val |= static_cast<uint32_t>(b.data) << (8 * j);
            if (blk == cfg::BLOCKS_PER_PACKET - 1) { ++pkt; blk = 0; }
            else                                   { ++blk; }
        }
        // An incomplete counter reads back as zero (RTL forces value to 0).
        msg.counters[i] = telemetry_counter_value{vld, vld ? val : 0u};
    }
    return msg;
}

void telemetry_receiver::queue_message(const telemetry_message& msg)
{
    if (count_ == cfg_.buffer_depth) {
        // Circular buffer overflow: the RTL drops the OLDEST entry by
        // advancing the read pointer, keeping the newest telemetry.
        rd_idx_ = (rd_idx_ + 1) % cfg_.buffer_depth;
        --count_;
        SIM_LOG_DEBUG(this, "message buffer overflow; oldest message dropped");
    }
    queue_[(rd_idx_ + count_) % cfg_.buffer_depth] = msg;
    ++count_;
    SIM_LOG_TRACE(this, "message queued: probe_id=" << unsigned(msg.probe_id)
                        << " fill_level=" << count_);
}

void telemetry_receiver::pop_message()
{
    // RTL: the pop is effective only when the buffer is not empty.
    if (count_ == 0) {
        SIM_LOG_DEBUG(this, "CTRL.BUFFER_POP ignored: message buffer empty");
        return;
    }
    rd_idx_ = (rd_idx_ + 1) % cfg_.buffer_depth;
    --count_;
    SIM_LOG_TRACE(this, "message popped; fill_level=" << count_);
}

void telemetry_receiver::rx_flush()
{
    beats_  = 0;
    rd_idx_ = 0;
    count_  = 0;
    dbg_missing_last_  = false;
    dbg_assembly_full_ = false;
}

const telemetry_message& telemetry_receiver::visible_message() const
{
    // RTL drives the CSR view from the buffer bottom, or zero when empty.
    return count_ == 0 ? empty_message_ : queue_[rd_idx_];
}

bool telemetry_receiver::push_atb_beat(uint8_t beat)
{
    if (!accepting_) {
        SIM_LOG_DEBUG(this, "ATB beat dropped: atready_o low (in reset)");
        return false;
    }

    assembly_[beats_++] = beat;

    if (beats_ % telemetry_receiver_cfg::BEATS_PER_PACKET == 0) {
        const unsigned pkt = beats_ / telemetry_receiver_cfg::BEATS_PER_PACKET - 1;
        if (telemetry_packet_last(packet_word(pkt))) {
            // End of message: decode, queue, restart the assembly buffer.
            queue_message(decode_message());
            beats_ = 0;
        } else if (beats_ == assembly_.size()) {
            // Buffer filled without a last_packet marker anywhere.
            dbg_assembly_full_ = true;
            raise_missing_last();
            beats_ = 0;
        }
    }

    schedule_recompute(); // fill level may have crossed the threshold
    return true;
}

unsigned telemetry_receiver::push_atb_beats(const std::vector<uint8_t>& beats)
{
    unsigned accepted = 0;
    for (uint8_t b : beats) {
        if (!push_atb_beat(b)) break;
        ++accepted;
    }
    return accepted;
}

// ===========================================================================
// Register decode
// ===========================================================================

int telemetry_receiver::counter_index(uint64_t off) const
{
    using cfg = telemetry_receiver_cfg;
    if (off < cfg::TELEMETRY_COUNTER0) return -1;
    const uint64_t idx = (off - cfg::TELEMETRY_COUNTER0) / cfg::REG_WIDTH;
    if (idx >= cfg::NUM_COUNTER_REGS) return -1;
    return static_cast<int>(idx);
}

bool telemetry_receiver::reg_read(uint64_t off, uint32_t& data) const
{
    using cfg = telemetry_receiver_cfg;

    switch (off) {
    case cfg::STATUS:
        data = (count_ == 0 ? cfg::STATUS_BUFFER_EMPTY : 0u) |
               (count_ == cfg_.buffer_depth ? cfg::STATUS_BUFFER_FULL : 0u);
        return true;
    case cfg::INTR_STATUS:
        data = (missing_last_status_ ? cfg::INTR_MISSING_LAST : 0u) |
               (threshold_irq_active() ? cfg::INTR_BUFFER_THRESHOLD : 0u);
        return true;
    case cfg::TELEMETRY_PROBE_ID:
        data = visible_message().probe_id & cfg::PROBE_ID_MASK;
        return true;
    case cfg::TELEMETRY_COUNTER_VLDS: {
        // Bit i is the valid bit of counter i; regs above the configured
        // counter count are tied off.
        const telemetry_message& m = visible_message();
        uint32_t vlds = 0;
        for (unsigned i = 0; i < cfg_.max_counters_per_message; ++i)
            if (m.counters[i].vld) vlds |= (1u << i);
        data = vlds;
        return true;
    }
    default: break;
    }

    if (const int idx = counter_index(off); idx >= 0) {
        const telemetry_message& m = visible_message();
        const unsigned u = static_cast<unsigned>(idx);
        data = (u < cfg_.max_counters_per_message) ? m.counters[u].value : 0u;
        return true;
    }

    // Plain storage registers (CTRL, INTR_ENABLE, INTR_TEST).
    if (uint32_t v = 0; regmap_.read(off, v)) { data = v; return true; }
    return false; // decode miss inside window -> ADDRESS_ERROR
}

bool telemetry_receiver::reg_write(uint64_t off, uint32_t data)
{
    using cfg = telemetry_receiver_cfg;

    switch (off) {
    case cfg::CTRL:
        // Pulse bits first. The RTL gives the flush precedence over the pop:
        // the flush resets both buffer pointers regardless of the pop.
        if ((data & cfg::CTRL_RX_FLUSH) != 0) {
            rx_flush();
            SIM_LOG_TRACE(this, "CTRL.TELEMETRY_RX_FLUSH: receiver flushed");
        } else if ((data & cfg::CTRL_BUFFER_POP) != 0) {
            pop_message();
        }
        // Store TX_FLUSH + BUFFER_THRESHOLD; the pulse bits read back as 0.
        regmap_.write(off, data);
        // A transmitter already holding afready high retires the flush at once.
        if ((ctrl_.raw() & cfg::CTRL_TX_FLUSH) != 0 && afready_i.read())
            af_event_.notify(sc_core::SC_ZERO_TIME);
        schedule_recompute();
        return true;

    case cfg::INTR_STATUS: {
        // MISSING_LAST is W1C; BUFFER_THRESHOLD is read-only (level).
        const uint32_t cur = missing_last_status_ ? cfg::INTR_MISSING_LAST : 0u;
        const uint32_t nxt = regmodel::apply_w1c(cur, data, cfg::INTR_MISSING_LAST);
        missing_last_status_ = (nxt & cfg::INTR_MISSING_LAST) != 0;
        schedule_recompute();
        return true;
    }

    case cfg::INTR_ENABLE:
        regmap_.write(off, data);
        schedule_recompute();
        return true;

    case cfg::INTR_TEST:
        // BUFFER_THRESHOLD is a held level; MISSING_LAST is a write pulse that
        // forces the interrupt through the same enable gate as a real event.
        regmap_.write(off, data);
        if ((data & cfg::INTR_MISSING_LAST) != 0) {
            set_missing_last_status();
            SIM_LOG_TRACE(this, "INTR_TEST.MISSING_LAST: interrupt forced");
        }
        schedule_recompute();
        return true;

    // Hardware-driven, read-only to software: writes are ignored (WI).
    case cfg::STATUS:
    case cfg::TELEMETRY_PROBE_ID:
    case cfg::TELEMETRY_COUNTER_VLDS:
        return true;

    default: break;
    }

    if (counter_index(off) >= 0) return true; // TELEMETRY_COUNTER[i] is RO
    return false; // decode miss inside window -> ADDRESS_ERROR
}

// ===========================================================================
// TLM-2.0 callbacks
// ===========================================================================

void telemetry_receiver::b_transport(tlm::tlm_generic_payload& gp,
                                    sc_core::sc_time& delay)
{
    using cfg = telemetry_receiver_cfg;

    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (cmd != tlm::TLM_READ_COMMAND && cmd != tlm::TLM_WRITE_COMMAND) {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    // Registers are 32-bit (accesswidth=32 in telemetry_receiver.rdl).
    if (len != cfg::REG_WIDTH) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (adr >= cfg::WINDOW_SIZE || (adr % cfg::REG_WIDTH) != 0) {
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
        if (ok) std::memcpy(buf, &v, cfg::REG_WIDTH);
        SIM_LOG_TRACE(this, "read  off=0x" << std::hex << adr
                            << " data=0x" << v << (ok ? "" : " [decode-miss]"));
    } else {
        uint32_t v = 0;
        std::memcpy(&v, buf, cfg::REG_WIDTH);
        ok = reg_write(adr, v);
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << adr
                            << " data=0x" << v << (ok ? "" : " [decode-miss]"));
    }

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                              : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    gp.set_dmi_allowed(false);
}

unsigned int telemetry_receiver::transport_dbg(tlm::tlm_generic_payload& gp)
{
    using cfg = telemetry_receiver_cfg;

    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (len != cfg::REG_WIDTH || (adr % cfg::REG_WIDTH) != 0 ||
        adr >= cfg::WINDOW_SIZE)
        return 0;

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = dbg_reg(adr); // side-effect-free peek
        std::memcpy(buf, &v, cfg::REG_WIDTH);
        return cfg::REG_WIDTH;
    }
    if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t v = 0;
        std::memcpy(&v, buf, cfg::REG_WIDTH);
        if (!reg_write(adr, v)) return 0;
        return cfg::REG_WIDTH;
    }
    return 0;
}

// ===========================================================================
// Debug / introspection
// ===========================================================================

uint32_t telemetry_receiver::dbg_reg(uint64_t off) const
{
    uint32_t v = 0;
    return reg_read(off, v) ? v : 0u;
}

void telemetry_receiver::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] telemetry_receiver state dump\n";
    os << "  CCI parameters:\n"
       << "    buffer_depth=" << cfg_.buffer_depth
       << (buffer_depth_p_.is_preset_value() ? " [preset]" : " [default]")
       << "  max_counters_per_message=" << cfg_.max_counters_per_message
       << (max_counters_p_.is_preset_value() ? " [preset]" : " [default]")
       << "  access_delay_ns=" << access_delay_ns_p_.get_value()
       << (access_delay_ns_p_.is_preset_value() ? " [preset]" : " [default]")
       << "\n";
    os << std::hex << std::setfill('0');
    os << "  CTRL=0x"        << ctrl_.read()
       << " INTR_ENABLE=0x"  << intr_enable_.read()
       << " INTR_TEST=0x"    << intr_test_.read() << "\n";
    os << std::dec << std::setfill(' ');
    os << "  queue: fill=" << count_ << "/" << cfg_.buffer_depth
       << " rd_idx=" << rd_idx_
       << " threshold=" << threshold_compare_value()
       << "  assembly: " << beats_ << "/" << assembly_.size() << " beats\n";
    os << "  irq=" << (missing_last_status_ || threshold_irq_active())
       << " (missing_last=" << missing_last_status_
       << " threshold=" << threshold_irq_active() << ")"
       << " afvalid=" << ((ctrl_.raw() & telemetry_receiver_cfg::CTRL_TX_FLUSH) != 0)
       << " atready=" << accepting_ << "\n";

    const telemetry_message& m = visible_message();
    os << "  visible message: probe_id=" << unsigned(m.probe_id) << "\n";
    for (unsigned i = 0; i < m.counters.size(); ++i)
        os << "    counter[" << i << "] vld=" << m.counters[i].vld
           << " value=0x" << std::hex << m.counters[i].value << std::dec << "\n";
}

} // namespace smc
