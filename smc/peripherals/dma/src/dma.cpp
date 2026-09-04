// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file dma.cpp
 * @brief SMC DMA — SystemC/TLM-2.0 LT implementation.
 */

#include "dma.h"

#include <cstring>

namespace smc {

namespace {

constexpr uint32_t STATUS_BUSY_MASK = 0x00000001u;

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

dma::dma(sc_core::sc_module_name name, dma_cfg cfg)
    : sc_core::sc_module(name)
    , num_channels_p_(
          "num_channels",
          cfg.num_channels,
          "Number of DMA channels (1..16). Sizes the STATUS/NEXT_ID/DONE register arrays.")
    , access_delay_ns_p_(
          "access_delay_ns",
          cfg.access_delay_ns,
          "TLM b_transport annotated delay for register accesses (ns).")
    , transfer_delay_ns_p_(
          "transfer_delay_ns",
          cfg.transfer_delay_ns,
          "Additional annotated delay added to each DMA data transfer (ns).")
           , max_burst_bytes_p_(
                 "max_burst_bytes",
                 cfg.max_burst_bytes,
                 "Maximum number of bytes moved in one master-socket b_transport (>= 1).")
           , base_addr_p_(
                 "base_addr",
                 cfg.base_addr,
                 "Absolute DMA base address.  Transactions in [base_addr, base_addr + WINDOW_SIZE) "
                 "are decoded relative to base_addr; offset-only accesses also accepted for standalone benches.")
           , reg_socket("reg_socket")
           , mst_socket("mst_socket")
           , cfg_(cfg)
       {
           cfg_.num_channels    = num_channels_p_.get_value();
           cfg_.access_delay_ns = access_delay_ns_p_.get_value();
           cfg_.transfer_delay_ns = transfer_delay_ns_p_.get_value();
           cfg_.max_burst_bytes = max_burst_bytes_p_.get_value();
           cfg_.base_addr       = base_addr_p_.get_value();

           num_channels_p_.add_metadata("valid_range", cci::cci_value(std::string("1..16")));
           access_delay_ns_p_.add_metadata("unit",      cci::cci_value(std::string("nanoseconds")));
           access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));
           transfer_delay_ns_p_.add_metadata("unit",    cci::cci_value(std::string("nanoseconds")));
           transfer_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));
           base_addr_p_.add_metadata("rdl_block", cci::cci_value(std::string("dma_ctrl")));
           base_addr_p_.add_metadata("default",   cci::cci_value(std::string("0xC0038000")));

    if (cfg_.num_channels == 0 || cfg_.num_channels > 16) {
        SC_REPORT_FATAL(name, "dma num_channels must be in 1..16");
    }
    if (cfg_.max_burst_bytes == 0) {
        SC_REPORT_FATAL(name, "dma max_burst_bytes must be >= 1");
    }

    channels_.assign(cfg_.num_channels, channel_state{});

    reg_socket.register_b_transport  (this, &dma::b_transport);
    reg_socket.register_transport_dbg(this, &dma::transport_dbg);

    SC_THREAD(transfer_thread);

           SIM_LOG_INFO(this,
               "dma instantiated: num_channels=" << cfg_.num_channels
               << ", max_burst_bytes=" << cfg_.max_burst_bytes
               << ", access_delay_ns=" << cfg_.access_delay_ns
               << ", transfer_delay_ns=" << cfg_.transfer_delay_ns
               << ", base_addr=0x" << std::hex << cfg_.base_addr);
       }

// ---------------------------------------------------------------------------
// TLM target interface
// ---------------------------------------------------------------------------

void dma::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const tlm::tlm_command cmd = gp.get_command();
    uint64_t                 adr = gp.get_address();
    unsigned char*           ptr = gp.get_data_ptr();
    const unsigned int       len = gp.get_data_length();

    if (ptr == nullptr) {
        gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        SIM_LOG_DEBUG(this, "b_transport with null data pointer");
        return;
    }
    if (len != 4) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        SIM_LOG_DEBUG(this, "b_transport with unsupported data length: " << len);
        return;
    }
    if (gp.get_streaming_width() != 0 && gp.get_streaming_width() != len) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        SIM_LOG_DEBUG(this, "b_transport with streaming width mismatch: " << gp.get_streaming_width());
        return;
    }

    // Accept absolute platform addresses (subtract base_addr) or standalone-bench
    // offset-only addresses.  The window is not a power of two, so reject
    // anything outside [0, WINDOW_SIZE) rather than wrapping with modulo.
    const uint64_t off = normalize_addr(adr);
    if (off >= dma_cfg::WINDOW_SIZE) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        SIM_LOG_DEBUG(this, "b_transport out of window at addr=0x" << std::hex << adr);
        return;
    }

    // The canonical AXI sideband extension is read but not used for local
    // register access decisions; it is attached to outgoing master transactions.
    smc::smc_axi_extension* ext = nullptr;
    gp.get_extension(ext);
    (void)ext;

    uint32_t v = 0;
    bool     ok = false;
    if (cmd == tlm::TLM_READ_COMMAND) {
        ok = reg_read(off, v);
        std::memcpy(ptr, &v, 4);
        SIM_LOG_TRACE(this, "read  off=0x" << std::hex << off << " data=0x" << v);
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(&v, ptr, 4);
        ok = reg_write(off, v);
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << off << " data=0x" << v);
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        SIM_LOG_DEBUG(this, "b_transport with invalid command");
        return;
    }

    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    gp.set_dmi_allowed(false);

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
}

unsigned int dma::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const uint64_t adr = gp.get_address();
    const uint64_t off = normalize_addr(adr);
    unsigned char* ptr = gp.get_data_ptr();

    if (ptr == nullptr || gp.get_data_length() != 4 || off >= dma_cfg::WINDOW_SIZE) {
        return 0;
    }

    uint32_t v = 0;
    if (gp.is_read()) {
        if (!reg_read(off, v)) return 0;
        std::memcpy(ptr, &v, 4);
    } else if (gp.is_write()) {
        std::memcpy(&v, ptr, 4);
        if (!reg_write(off, v)) return 0;
    }
    return 4;
}

// ---------------------------------------------------------------------------
// Register decode helpers
// ---------------------------------------------------------------------------

bool dma::is_status_offset(uint64_t off, unsigned& channel) const
{
    if (off < dma_cfg::OFF_STATUS_0 || off >= dma_cfg::OFF_STATUS_0 + cfg_.num_channels * dma_cfg::STATUS_STRIDE) {
        return false;
    }
    if ((off - dma_cfg::OFF_STATUS_0) % dma_cfg::STATUS_STRIDE != 0) {
        return false;
    }
    channel = static_cast<unsigned>((off - dma_cfg::OFF_STATUS_0) / dma_cfg::STATUS_STRIDE);
    return true;
}

bool dma::is_next_id_offset(uint64_t off, unsigned& channel) const
{
    if (off < dma_cfg::OFF_NEXT_ID_0 || off >= dma_cfg::OFF_NEXT_ID_0 + cfg_.num_channels * dma_cfg::NEXT_ID_STRIDE) {
        return false;
    }
    if ((off - dma_cfg::OFF_NEXT_ID_0) % dma_cfg::NEXT_ID_STRIDE != 0) {
        return false;
    }
    channel = static_cast<unsigned>((off - dma_cfg::OFF_NEXT_ID_0) / dma_cfg::NEXT_ID_STRIDE);
    return true;
}

bool dma::is_done_offset(uint64_t off, unsigned& channel) const
{
    if (off < dma_cfg::OFF_DONE_0 || off >= dma_cfg::OFF_DONE_0 + cfg_.num_channels * dma_cfg::DONE_STRIDE) {
        return false;
    }
    if ((off - dma_cfg::OFF_DONE_0) % dma_cfg::DONE_STRIDE != 0) {
        return false;
    }
    channel = static_cast<unsigned>((off - dma_cfg::OFF_DONE_0) / dma_cfg::DONE_STRIDE);
    return true;
}

// ---------------------------------------------------------------------------
// Register read
// ---------------------------------------------------------------------------

uint64_t dma::normalize_addr(uint64_t addr) const
{
    const uint64_t base = base_addr_p_.get_value();
    if (addr >= base && addr < base + dma_cfg::WINDOW_SIZE)
        return addr - base;
    return addr;
}

bool dma::reg_read(uint64_t off, uint32_t& data)
{
    data = 0;

    unsigned channel = 0;
    if (is_status_offset(off, channel)) {
        data = channels_[channel].busy ? STATUS_BUSY_MASK : 0;
        return true;
    }
    if (is_next_id_offset(off, channel)) {
        data = start_transfer(channel);
        return true;
    }
    if (is_done_offset(off, channel)) {
        data = channels_[channel].done;
        return true;
    }

    switch (off) {
    case dma_cfg::OFF_CONFIG:
        data = config_;
        return true;
    case dma_cfg::OFF_DST_ADDRESS_LO:
        data = static_cast<uint32_t>(xfer_.dst);
        return true;
    case dma_cfg::OFF_DST_ADDRESS_HI:
        data = static_cast<uint32_t>(xfer_.dst >> 32);
        return true;
    case dma_cfg::OFF_SRC_ADDRESS_LO:
        data = static_cast<uint32_t>(xfer_.src);
        return true;
    case dma_cfg::OFF_SRC_ADDRESS_HI:
        data = static_cast<uint32_t>(xfer_.src >> 32);
        return true;
    case dma_cfg::OFF_LENGTH_LO:
        data = static_cast<uint32_t>(xfer_.length);
        return true;
    case dma_cfg::OFF_LENGTH_HI:
        data = static_cast<uint32_t>(xfer_.length >> 32);
        return true;
    case dma_cfg::OFF_DST_STRIDE_LO:
        data = static_cast<uint32_t>(xfer_.dst_stride);
        return true;
    case dma_cfg::OFF_DST_STRIDE_HI:
        data = static_cast<uint32_t>(xfer_.dst_stride >> 32);
        return true;
    case dma_cfg::OFF_SRC_STRIDE_LO:
        data = static_cast<uint32_t>(xfer_.src_stride);
        return true;
    case dma_cfg::OFF_SRC_STRIDE_HI:
        data = static_cast<uint32_t>(xfer_.src_stride >> 32);
        return true;
    case dma_cfg::OFF_NUM_REPETITIONS_LO:
        data = static_cast<uint32_t>(xfer_.repetitions);
        return true;
    case dma_cfg::OFF_NUM_REPETITIONS_HI:
        data = static_cast<uint32_t>(xfer_.repetitions >> 32);
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Register write
// ---------------------------------------------------------------------------

bool dma::reg_write(uint64_t off, uint32_t data)
{
    unsigned channel = 0;
    if (is_status_offset(off, channel) || is_next_id_offset(off, channel) || is_done_offset(off, channel)) {
        // STATUS / NEXT_ID / DONE are read-only or trigger-on-read.
        return true;
    }

    switch (off) {
    case dma_cfg::OFF_CONFIG:
        config_ = data;
        return true;
    case dma_cfg::OFF_DST_ADDRESS_LO:
        xfer_.dst = (xfer_.dst & ~0xFFFFFFFFULL) | data;
        return true;
    case dma_cfg::OFF_DST_ADDRESS_HI:
        xfer_.dst = (xfer_.dst & 0xFFFFFFFFULL) | (static_cast<uint64_t>(data) << 32);
        return true;
    case dma_cfg::OFF_SRC_ADDRESS_LO:
        xfer_.src = (xfer_.src & ~0xFFFFFFFFULL) | data;
        return true;
    case dma_cfg::OFF_SRC_ADDRESS_HI:
        xfer_.src = (xfer_.src & 0xFFFFFFFFULL) | (static_cast<uint64_t>(data) << 32);
        return true;
    case dma_cfg::OFF_LENGTH_LO:
        xfer_.length = (xfer_.length & ~0xFFFFFFFFULL) | data;
        return true;
    case dma_cfg::OFF_LENGTH_HI:
        xfer_.length = (xfer_.length & 0xFFFFFFFFULL) | (static_cast<uint64_t>(data) << 32);
        return true;
    case dma_cfg::OFF_DST_STRIDE_LO:
        xfer_.dst_stride = (xfer_.dst_stride & ~0xFFFFFFFFULL) | data;
        return true;
    case dma_cfg::OFF_DST_STRIDE_HI:
        xfer_.dst_stride = (xfer_.dst_stride & 0xFFFFFFFFULL) | (static_cast<uint64_t>(data) << 32);
        return true;
    case dma_cfg::OFF_SRC_STRIDE_LO:
        xfer_.src_stride = (xfer_.src_stride & ~0xFFFFFFFFULL) | data;
        return true;
    case dma_cfg::OFF_SRC_STRIDE_HI:
        xfer_.src_stride = (xfer_.src_stride & 0xFFFFFFFFULL) | (static_cast<uint64_t>(data) << 32);
        return true;
    case dma_cfg::OFF_NUM_REPETITIONS_LO:
        xfer_.repetitions = (xfer_.repetitions & ~0xFFFFFFFFULL) | data;
        return true;
    case dma_cfg::OFF_NUM_REPETITIONS_HI:
        xfer_.repetitions = (xfer_.repetitions & 0xFFFFFFFFULL) | (static_cast<uint64_t>(data) << 32);
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Transfer control
// ---------------------------------------------------------------------------

void dma::transfer_thread()
{
    while (true) {
        sc_core::wait(transfer_event_);
        while (!pending_.empty()) {
            pending_transfer pt = pending_.front();
            pending_.erase(pending_.begin());

            SIM_LOG_TRACE(this, "channel " << pt.channel << " executing transfer id="
                          << channels_[pt.channel].next_id);

            execute_transfer(pt);

            channels_[pt.channel].done += 1;
            channels_[pt.channel].busy = false;

            SIM_LOG_TRACE(this, "channel " << pt.channel << " transfer done");
        }
    }
}

uint32_t dma::start_transfer(unsigned channel)
{
    if (channel >= cfg_.num_channels) {
        return 0;
    }
    if (channels_[channel].busy) {
        return 0;
    }
    if (xfer_.length == 0) {
        return 0;
    }

    channels_[channel].busy = true;
    channels_[channel].next_id += 1;
    const uint32_t id = channels_[channel].next_id;

    SIM_LOG_TRACE(this, "channel " << channel << " scheduled transfer id=" << id
                  << " src=0x" << std::hex << xfer_.src << " dst=0x" << xfer_.dst
                  << " len=0x" << xfer_.length << " reps=" << std::dec << xfer_.repetitions);

    pending_.push_back(pending_transfer{channel, xfer_});
    transfer_event_.notify(sc_core::SC_ZERO_TIME);

    return id;
}

void dma::execute_transfer(const pending_transfer& pt)
{
    const uint64_t reps = pt.cfg.repetitions + 1;
    for (uint64_t r = 0; r < reps; ++r) {
        const uint64_t src = pt.cfg.src + r * pt.cfg.src_stride;
        const uint64_t dst = pt.cfg.dst + r * pt.cfg.dst_stride;

        uint64_t remaining = pt.cfg.length;
        uint64_t offset = 0;
        while (remaining > 0) {
            const uint64_t chunk = std::min(remaining, static_cast<uint64_t>(cfg_.max_burst_bytes));
            if (!copy_chunk(src + offset, dst + offset, chunk)) {
                SIM_LOG_WARN(this, "channel " << pt.channel << " copy_chunk failed at offset " << offset);
                return;
            }
            offset += chunk;
            remaining -= chunk;
        }
    }
}

bool dma::copy_chunk(uint64_t src, uint64_t dst, uint64_t len)
{
    if (len == 0) {
        return true;
    }

    std::vector<unsigned char> buf(len);

    // Read from source.
    {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        gp.set_command(tlm::TLM_READ_COMMAND);
        gp.set_address(src);
        gp.set_data_ptr(buf.data());
        gp.set_data_length(static_cast<unsigned int>(len));
        gp.set_streaming_width(static_cast<unsigned int>(len));
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        smc::smc_axi_extension ext;
        ext.source_id = smc::SMC_ID;
        gp.set_extension(&ext);

        delay += sc_core::sc_time(transfer_delay_ns_p_.get_value(), sc_core::SC_NS);
        mst_socket->b_transport(gp, delay);
        gp.clear_extension<smc::smc_axi_extension>();

        if (delay > sc_core::SC_ZERO_TIME) {
            sc_core::wait(delay);
        }
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            return false;
        }
    }

    // Write to destination.
    {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        gp.set_command(tlm::TLM_WRITE_COMMAND);
        gp.set_address(dst);
        gp.set_data_ptr(buf.data());
        gp.set_data_length(static_cast<unsigned int>(len));
        gp.set_streaming_width(static_cast<unsigned int>(len));
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        smc::smc_axi_extension ext;
        ext.source_id = smc::SMC_ID;
        gp.set_extension(&ext);

        delay += sc_core::sc_time(transfer_delay_ns_p_.get_value(), sc_core::SC_NS);
        mst_socket->b_transport(gp, delay);
        gp.clear_extension<smc::smc_axi_extension>();

        if (delay > sc_core::SC_ZERO_TIME) {
            sc_core::wait(delay);
        }
        if (gp.get_response_status() != tlm::TLM_OK_RESPONSE) {
            return false;
        }
    }

    return true;
}

} // namespace smc
