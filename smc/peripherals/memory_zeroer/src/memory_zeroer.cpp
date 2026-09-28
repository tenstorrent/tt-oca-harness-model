// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file memory_zeroer.cpp
 * @brief Implementation of the SMC AXI memory-zeroer LT model.
 *
 * Behaviour matches the Virtualizer AXI_zeroer reference:
 *  - DEST_ADDR / SIZE are plain 64-bit RW registers
 *  - any write to CTRL_STATUS merges int_en[0] and, when SIZE != 0, runs a
 *    synchronous zero-fill over dma_socket (status[32] high during the job)
 *  - irq_o asserts on successful completion when int_en is set
 */

#include "memory_zeroer.h"

#include "sim_log.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace smc {

memory_zeroer::memory_zeroer(sc_core::sc_module_name name, memory_zeroer_cfg cfg)
    : sc_core::sc_module(name)
    , chunk_size_p_("chunk_size", cfg.chunk_size,
                    "DMA zero-fill chunk size in bytes (1..1048576). "
                    "Immutable after construction.")
    , access_delay_ns_p_("access_delay_ns", cfg.access_delay_ns,
                         "TLM register-access annotated delay (ns). "
                         "Mutable at run-time.")
    , reg_socket("reg_socket")
    , dma_socket("dma_socket")
    , rst_n_i("rst_n_i")
    , irq_o("irq_o")
    , cfg_(cfg)
{
    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase",
                                    cci::cci_value(std::string("annotated_delay")));

    const unsigned chunk = chunk_size_p_.get_value();
    if (chunk < 1u || chunk > (1u << 20)) {
        SC_REPORT_FATAL("memory_zeroer",
                        "chunk_size must be in [1, 1048576]");
    }

    // CTRL_STATUS write: merge int_en, then kick a job when SIZE != 0.
    ctrl_status_.on_write([this](uint64_t cur, uint64_t in) {
        const uint64_t next =
            regmodel::apply_write_mask(cur, in, memory_zeroer_cfg::CTRL_WMASK);
        // Deassert irq if software disables int_en.
        if ((next & memory_zeroer_cfg::CTRL_INT_EN_MASK) == 0) {
            update_irq(false);
        }
        // Store first so trigger_job / dbg see the new int_en.
        // Register::write assigns the callback return value after we return;
        // set_raw here so nested trigger_job sees the merged value.
        ctrl_status_.set_raw(next);
        trigger_job();
        return ctrl_status_.raw();
    });

    regmap_.add(memory_zeroer_cfg::OFF_DEST_ADDR, "DEST_ADDR", dest_addr_)
           .add(memory_zeroer_cfg::OFF_SIZE, "SIZE", size_)
           .add(memory_zeroer_cfg::OFF_CTRL_STATUS, "CTRL_STATUS", ctrl_status_);

    reg_socket.register_b_transport(this, &memory_zeroer::b_transport);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    do_reset();

    SIM_LOG_INFO(this,
                 "memory_zeroer instantiated: base=0x"
                     << std::hex << cfg_.base_addr << std::dec
                     << " chunk_size=" << chunk
                     << " access_delay_ns="
                     << access_delay_ns_p_.get_value()
                     << " regmap_registers=" << regmap_.size());
}

void memory_zeroer::reset_proc()
{
    if (rst_n_i.read()) return; // active-low
    do_reset();
}

void memory_zeroer::do_reset()
{
    dest_addr_.reset(UINT64_C(0));
    size_.reset(UINT64_C(0));
    ctrl_status_.reset(UINT64_C(0));
    out_irq_ = false;
    if (outputs_valid_)
        irq_o.write(false);
}

void memory_zeroer::end_of_elaboration()
{
    outputs_valid_ = true;
    irq_o.write(out_irq_);
}

void memory_zeroer::update_irq(bool asserted)
{
    if (asserted == out_irq_ && outputs_valid_) return;
    out_irq_ = asserted;
    if (outputs_valid_)
        irq_o.write(asserted);
}

void memory_zeroer::trigger_job()
{
    const uint64_t nbytes = size_.raw();
    if (nbytes == 0u) return;

    update_irq(false);

    const uint64_t busy =
        ctrl_status_.raw() | memory_zeroer_cfg::CTRL_STATUS_MASK;
    ctrl_status_.set_raw(busy);

    const uint64_t addr = dest_addr_.raw();
    SIM_LOG_TRACE(this,
                  "zero start dest=0x" << std::hex << addr
                                       << " size=0x" << nbytes << std::dec);
    const bool ok = perform_write_zeros(addr, nbytes);

    const uint64_t idle =
        ctrl_status_.raw() & ~memory_zeroer_cfg::CTRL_STATUS_MASK;
    ctrl_status_.set_raw(idle);

    if (ok &&
        (ctrl_status_.raw() & memory_zeroer_cfg::CTRL_INT_EN_MASK) != 0) {
        update_irq(true);
    }
}

bool memory_zeroer::perform_write_zeros(uint64_t addr, uint64_t nbytes)
{
    if (nbytes == 0u) return true;

    // Reject jobs whose span wraps the 64-bit address space. Unsigned
    // `addr + offset` would otherwise issue a DMA write into low memory.
    if (addr + nbytes < addr) {
        SIM_LOG_WARN(this,
                     "DMA zero rejected: dest=0x"
                         << std::hex << addr << " size=0x" << nbytes
                         << std::dec << " wraps past 2^64");
        return false;
    }

    const std::size_t chunk_cap =
        static_cast<std::size_t>(chunk_size_p_.get_value());
    std::vector<unsigned char> zero_buf(chunk_cap, 0);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    uint64_t remaining = nbytes;
    uint64_t offset    = 0u;

    while (remaining != 0u) {
        const std::size_t chunk = static_cast<std::size_t>(
            std::min(remaining, static_cast<uint64_t>(chunk_cap)));

        // Declared before the payload so the extension outlives it. Same
        // lifetime as dma.cpp copy_chunk: set_extension does not take
        // ownership, and clear_extension runs before either destructor.
        smc::smc_axi_extension ext;
        ext.source_id = smc::SMC_ID;

        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr + offset);
        trans.set_data_ptr(zero_buf.data());
        trans.set_data_length(static_cast<unsigned int>(chunk));
        trans.set_streaming_width(static_cast<unsigned int>(chunk));
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        // source_id_t has no dedicated zeroer ID; SMC_ID is the fabric's
        // "internal masters" value (smc_axi_extension.h), the same one
        // dma.cpp stamps. Other fields stay at the extension defaults.
        ext.set_priv(true);
        ext.set_secure(false);
        ext.set_fetch(false);
        ext.set_locked(false);
        trans.set_extension(&ext);
        dma_socket->b_transport(trans, delay);
        trans.clear_extension<smc::smc_axi_extension>();

        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            SIM_LOG_WARN(this,
                         "DMA zero write failed at addr=0x"
                             << std::hex << (addr + offset) << std::dec
                             << " status="
                             << trans.get_response_status());
            return false;
        }
        remaining -= chunk;
        offset += chunk;
    }
    return true;
}

bool memory_zeroer::reg_read(uint64_t offset, uint64_t& data)
{
    return regmap_.read(offset, data);
}

bool memory_zeroer::reg_write(uint64_t offset, uint64_t data)
{
    return regmap_.write(offset, data);
}

void memory_zeroer::b_transport(tlm::tlm_generic_payload& gp,
                                sc_core::sc_time& delay)
{
    gp.set_dmi_allowed(false);

    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const unsigned         len = gp.get_data_length();
    unsigned char* const   ptr = gp.get_data_ptr();

    if (ptr == nullptr) {
        gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    // 64-bit register file — require naturally aligned 8-byte accesses.
    // A zero length is a burst error, same as a short or unaligned beat.
    if (len == 0 || len != 8 || (adr & 0x7u) != 0) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    // Subtraction form: `adr + len` wraps near UINT64_MAX and would otherwise
    // accept an address that is outside the aperture.
    if (adr >= memory_zeroer_cfg::WINDOW_SIZE ||
        len > memory_zeroer_cfg::WINDOW_SIZE - adr) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        SIM_LOG_DEBUG(this,
                      "TLM decode miss at off=0x" << std::hex << adr);
        return;
    }

    if (gp.get_byte_enable_ptr() != nullptr) {
        // Virtualizer honours byte enables on CTRL_STATUS int_en; for a
        // simple LT model we reject explicit BE (full-word SW writes only).
        // Partial BE is uncommon on this CSR port in SMC firmware.
        gp.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }

    if (gp.get_streaming_width() < len) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    bool ok = false;
    if (cmd == tlm::TLM_READ_COMMAND) {
        uint64_t data = 0;
        ok = reg_read(adr, data);
        if (ok) {
            std::memcpy(ptr, &data, sizeof(data));
            SIM_LOG_TRACE(this,
                          "read off=0x" << std::hex << adr << " data=0x"
                                        << data << std::dec);
        }
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint64_t data = 0;
        std::memcpy(&data, ptr, sizeof(data));
        ok = reg_write(adr, data);
        if (ok) {
            SIM_LOG_TRACE(this,
                          "write off=0x" << std::hex << adr << " data=0x"
                                         << data << std::dec);
        }
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                              : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    if (!ok) {
        SIM_LOG_DEBUG(this,
                      "TLM decode miss at off=0x" << std::hex << adr);
    }
}

void memory_zeroer::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] memory_zeroer state\n"
       << "  DEST_ADDR   = 0x" << std::hex << dest_addr_.raw() << "\n"
       << "  SIZE        = 0x" << size_.raw() << "\n"
       << "  CTRL_STATUS = 0x" << ctrl_status_.raw() << std::dec
       << "  (int_en=" << dbg_int_en() << " busy=" << dbg_busy() << ")\n"
       << "  irq_o       = " << out_irq_ << "\n";
}

}  // namespace smc
