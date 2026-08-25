// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file pll_reg_block.cpp
 * @brief Implementation of the shared table-driven register-file base.
 */

#include "pll_reg_block.h"

#include "sim_log.h"

#include <cstring>
#include <utility>

namespace smc {
namespace pll {

reg_block::reg_block(sc_core::sc_module_name name,
                     const reg_spec* specs, std::size_t n,
                     uint64_t window_size,
                     uint64_t base_addr,
                     double   access_delay_ns)
    : sc_core::sc_module(name)
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
    , access_delay_ns_p_("access_delay_ns", access_delay_ns,
                         "TLM register-access annotated delay (ns). "
                         "Mutable at run-time.")
    , window_size_(window_size)
    , base_addr_(base_addr)
{
    access_delay_ns_p_.add_metadata("unit",
                                    cci::cci_value(std::string("nanoseconds")));

    build(specs, n);

    reg_socket.register_b_transport(this, &reg_block::b_transport);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    do_reset();

    SIM_LOG_INFO(this,
                 "reg_block instantiated: base=0x"
                     << std::hex << base_addr_ << " window=0x" << window_size_
                     << std::dec << " registers=" << map_.size());
}

void reg_block::build(const reg_spec* specs, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) {
        const reg_spec& s = specs[i];

        regs_.emplace_back(s.rmask, s.wmask, s.reset);
        regmodel::Register32& r = regs_.back();

        // Every register gets its own dedicated read + write callback instance
        // (captured by value, so each closure carries only that register's own
        // masks).  A model can replace any of these with bespoke behaviour via
        // set_write_callback() / set_read_callback() after base construction.
        const uint32_t rmask = s.rmask;
        const uint32_t wmask = s.wmask;
        const uint32_t sc    = s.self_clear_mask;

        // Write callback: mask-merge through this register's write mask, then
        // force its self-clearing / single-pulse bits back to 0 (sc == 0 for a
        // plain RW/RO register, so those merge normally).
        r.on_write([wmask, sc](uint32_t cur, uint32_t in) {
            const uint32_t merged = regmodel::apply_write_mask(cur, in, wmask);
            return merged & ~sc;
        });

        // Read callback: expose only this register's software-readable bits
        // (reserved / write-only bits read as 0).
        r.on_read([rmask](uint32_t stored) { return stored & rmask; });

        map_.add(s.offset, s.name, r);
        resets_.push_back(s.reset);
    }
}

bool reg_block::set_write_callback(uint64_t offset,
                                   regmodel::Register32::WriteFn fn)
{
    regmodel::Register32* r = map_.find(offset);
    if (r == nullptr) return false;
    r->on_write(std::move(fn));
    return true;
}

bool reg_block::set_read_callback(uint64_t offset,
                                  regmodel::Register32::ReadFn fn)
{
    regmodel::Register32* r = map_.find(offset);
    if (r == nullptr) return false;
    r->on_read(std::move(fn));
    return true;
}

void reg_block::reset_proc()
{
    if (rst_n_i.read()) return;  // active-low
    do_reset();
}

void reg_block::do_reset()
{
    std::size_t i = 0;
    for (auto& r : regs_) {
        r.reset(resets_[i]);
        ++i;
    }
}

bool reg_block::peek(uint64_t offset, uint32_t& out) const
{
    const regmodel::Register32* r = map_.find(offset);
    if (r == nullptr) return false;
    out = r->raw();
    return true;
}

bool reg_block::poke(uint64_t offset, uint32_t value)
{
    regmodel::Register32* r = map_.find(offset);
    if (r == nullptr) return false;
    r->set_raw(value);
    return true;
}

void reg_block::b_transport(tlm::tlm_generic_payload& gp,
                            sc_core::sc_time& delay)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const unsigned         len = gp.get_data_length();
    unsigned char* const   ptr = gp.get_data_ptr();

    if (ptr == nullptr || len == 0) {
        gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    // The PLL firmware uses 16-bit and 32-bit accesses (and could use 8-bit).
    // Accept naturally aligned 1/2/4-byte accesses only.
    if ((len != 1 && len != 2 && len != 4) ||
        (adr & (static_cast<uint64_t>(len) - 1)) != 0) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    if (adr + len > window_size_) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        SIM_LOG_DEBUG(this,
                      "TLM decode miss at off=0x" << std::hex << adr);
        return;
    }

    if (gp.get_byte_enable_ptr() != nullptr) {
        gp.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }

    if (gp.get_streaming_width() < len) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    // Map the (possibly sub-word) access onto its containing 32-bit register.
    const uint64_t reg_off   = adr & ~UINT64_C(0x3);
    const unsigned lane      = static_cast<unsigned>(adr & 0x3u);   // 0..3
    const unsigned lane_bits = lane * 8u;
    const uint32_t lane_mask =
        (len == 4) ? 0xFFFFFFFFu
                   : ((((uint32_t{1} << (len * 8u)) - 1u)) << lane_bits);

    // A single access must not straddle two registers.
    if (lane + len > 4u) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t word = 0;
        // Unmapped-but-in-window reads as zero (reserved => RAZ).
        map_.read(reg_off, word);
        const uint32_t lane_val = (word & lane_mask) >> lane_bits;
        std::memcpy(ptr, &lane_val, len);
        SIM_LOG_TRACE(this,
                      "read off=0x" << std::hex << adr << " (reg 0x" << reg_off
                                    << ") data=0x" << lane_val << std::dec);
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t incoming = 0;
        std::memcpy(&incoming, ptr, len);
        // Read-modify-write the containing word: touched lane takes the new
        // bytes, untouched lanes keep their current backing value (so the
        // register write callback leaves them unchanged).
        const regmodel::Register32* cur_reg = map_.find(reg_off);
        const uint32_t cur = (cur_reg != nullptr) ? cur_reg->raw() : 0u;
        const uint32_t data =
            (cur & ~lane_mask) | ((incoming << lane_bits) & lane_mask);
        // Unmapped-but-in-window writes are ignored (reserved => WI).
        map_.write(reg_off, data);
        SIM_LOG_TRACE(this,
                      "write off=0x" << std::hex << adr << " (reg 0x" << reg_off
                                     << ") data=0x" << data << std::dec);
        // Fire any side-effect observers (e.g. REG_UPDATE -> lock status).
        auto range = write_observers_.equal_range(reg_off);
        for (auto it = range.first; it != range.second; ++it) {
            it->second(data);
        }
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
}

void reg_block::observe_write(uint64_t offset, WriteObserver fn)
{
    write_observers_.emplace(offset, std::move(fn));
}

void reg_block::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] reg_block state (base=0x" << std::hex
       << base_addr_ << ", window=0x" << window_size_ << "):\n";
    map_.for_each([&](const regmodel::RegisterMap32::Entry& e) {
        os << "  @0x" << std::hex << e.offset << "  " << e.name << " = 0x"
           << e.reg->raw() << "\n";
    });
    os << std::dec;
}

}  // namespace pll
}  // namespace smc
