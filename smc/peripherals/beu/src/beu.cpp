// SPDX-License-Identifier: Apache-2.0
/**
 * @file beu.cpp
 * @brief SMC per-core Bus Error Unit — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/beu.h` for the full design description, register map, and the
 * functional-abstraction rationale.
 */

#include "beu.h"

#include "reg_access.h"
#include "sim_log.h"

#include <cstring>
#include <iomanip>

namespace smc {

// ===========================================================================
// Constructor
// ===========================================================================

beu::beu(sc_core::sc_module_name name, beu_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params first (declared before ports/sized members in the header).
    , access_delay_ns_p_("access_delay_ns", cfg.access_delay_ns,
          "TLM register-access annotated delay in nanoseconds. "
          "Approximates AXI4-Lite bus latency. Mutable at run-time.")
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
    , irq_local_o("irq_local_o")
    , irq_plic_o("irq_plic_o")
    , cfg_(cfg)
{
    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));

    // Offset -> storage-register dispatch table (common/include/reg_map.h).
    // CAUSE / PHYS_ADDR / ACCRUED_ENABLE carry model-side behaviour and stay
    // in the reg_read/reg_write switch below.
    regmap_.add(beu_cfg::ENABLE,       "ENABLE",       enable_)
           .add(beu_cfg::PLIC_ENABLE,  "PLIC_ENABLE",  plic_enable_)
           .add(beu_cfg::LOCAL_ENABLE, "LOCAL_ENABLE", local_enable_);

    SIM_LOG_INFO(this,
        "CCI config resolved:"
        << "  access_delay_ns=" << access_delay_ns_p_.get_value()
        << (access_delay_ns_p_.is_preset_value() ? " [preset]" : " [default]")
        << "  regmap_registers=" << regmap_.size());

    reg_socket.register_b_transport  (this, &beu::b_transport);
    reg_socket.register_transport_dbg(this, &beu::transport_dbg);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    // recompute_method is the SOLE driver of irq_local_o / irq_plic_o.
    SC_METHOD(recompute_method);
    sensitive << recompute_event_;
    dont_initialize();
}

// ===========================================================================
// SC_METHOD processes
// ===========================================================================

void beu::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void beu::reset_proc()
{
    if (rst_n_i.read()) return; // act only on assertion (low)

    // ENABLE resets to "all sources enabled"; masks and status reset to 0.
    enable_.reset(beu_cfg::VALID_MASK);
    plic_enable_.reset(0);
    local_enable_.reset(0);

    cause_     = 0;
    phys_addr_ = 0;
    accrued_   = 0;

    schedule_recompute();
}

void beu::recompute_method()
{
    const bool loc = irq_local_active();
    const bool plc = irq_plic_active();
    if (!outputs_valid_ || loc != out_irq_local_) {
        out_irq_local_ = loc;
        irq_local_o.write(loc);
    }
    if (!outputs_valid_ || plc != out_irq_plic_) {
        out_irq_plic_ = plc;
        irq_plic_o.write(plc);
    }
    outputs_valid_ = true;
}

// ===========================================================================
// Interrupt aggregation
// ===========================================================================

bool beu::irq_local_active() const
{
    return (accrued_ & local_enable_.read() & beu_cfg::VALID_MASK) != 0;
}

bool beu::irq_plic_active() const
{
    return (accrued_ & plic_enable_.read() & beu_cfg::VALID_MASK) != 0;
}

// ===========================================================================
// Register decode
// ===========================================================================

bool beu::reg_read(uint64_t off, uint64_t& data)
{
    switch (off) {
    case beu_cfg::CAUSE:          data = cause_;     return true;
    case beu_cfg::PHYS_ADDR:      data = phys_addr_; return true;
    case beu_cfg::ACCRUED_ENABLE: data = accrued_;   return true;
    default: break;
    }
    // Plain storage registers (ENABLE, PLIC_ENABLE, LOCAL_ENABLE).
    if (uint64_t v = 0; regmap_.read(off, v)) { data = v; return true; }
    return false; // decode miss inside window -> ADDRESS_ERROR
}

bool beu::reg_write(uint64_t off, uint64_t data)
{
    switch (off) {
    case beu_cfg::CAUSE:
        // SW writes CAUSE (typically 0) to acknowledge / re-arm recording.
        cause_ = data & beu_cfg::CAUSE_MASK;
        schedule_recompute();
        return true;
    case beu_cfg::PHYS_ADDR:
        // Read-only to software (HW-written); writes are ignored (WI).
        return true;
    case beu_cfg::ACCRUED_ENABLE:
        // SW clears (ack) per-source sticky bits by writing 0s; 1s preserve.
        // Software must not be able to set accrued status bits.
        accrued_ &= (data & beu_cfg::VALID_MASK);
        schedule_recompute();
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

void beu::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (cmd != tlm::TLM_READ_COMMAND && cmd != tlm::TLM_WRITE_COMMAND) {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    // Registers are 64-bit (accesswidth=64 in bus_error_unit.rdl).
    if (len != beu_cfg::REG_WIDTH) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (adr >= beu_cfg::WINDOW_SIZE || (adr % beu_cfg::REG_WIDTH) != 0) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // Optional AXI sideband extension is forwarded but not enforced here.
    smc::smc_axi_extension* ext = nullptr;
    gp.get_extension(ext);
    (void)ext;

    bool ok;
    if (cmd == tlm::TLM_READ_COMMAND) {
        uint64_t v = 0;
        ok = reg_read(adr, v);
        if (ok) std::memcpy(buf, &v, beu_cfg::REG_WIDTH);
        SIM_LOG_TRACE(this, "read  off=0x" << std::hex << adr
                            << " data=0x" << v << (ok ? "" : " [decode-miss]"));
    } else {
        uint64_t v = 0;
        std::memcpy(&v, buf, beu_cfg::REG_WIDTH);
        ok = reg_write(adr, v);
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << adr
                            << " data=0x" << v << (ok ? "" : " [decode-miss]"));
    }

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                              : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    gp.set_dmi_allowed(false);
}

unsigned int beu::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (len != beu_cfg::REG_WIDTH || (adr % beu_cfg::REG_WIDTH) != 0 ||
        adr >= beu_cfg::WINDOW_SIZE)
        return 0;

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint64_t v = dbg_reg(adr); // side-effect-free peek
        std::memcpy(buf, &v, beu_cfg::REG_WIDTH);
        return beu_cfg::REG_WIDTH;
    }
    if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint64_t v = 0;
        std::memcpy(&v, buf, beu_cfg::REG_WIDTH);
        if (!reg_write(adr, v)) return 0;
        return beu_cfg::REG_WIDTH;
    }
    return 0;
}

// ===========================================================================
// Test-bench back door
// ===========================================================================

void beu::inject_error(beu_src src, uint64_t phys_addr)
{
    const unsigned bit = static_cast<unsigned>(src);
    const uint64_t mask = uint64_t{1} << bit;

    // 1. Raw accrued status is always set, regardless of the recording enable.
    accrued_ |= (mask & beu_cfg::VALID_MASK);

    // 2. Latch the first enabled error into CAUSE / PHYS_ADDR (holds until SW
    //    writes CAUSE=0).
    if ((enable_.read() & mask) && cause_ == 0) {
        cause_     = bit;
        phys_addr_ = phys_addr & beu_cfg::PHYS_MASK;
    }

    // 3. Re-evaluate the interrupt lines.
    schedule_recompute();
}

uint64_t beu::dbg_reg(uint64_t off) const
{
    switch (off) {
    case beu_cfg::CAUSE:          return cause_;
    case beu_cfg::PHYS_ADDR:      return phys_addr_;
    case beu_cfg::ACCRUED_ENABLE: return accrued_;
    default: break;
    }
    if (uint64_t v = 0; regmap_.read(off, v)) return v;
    return 0u;
}

void beu::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] BEU state dump\n";
    os << "  CCI parameters:\n"
       << "    access_delay_ns=" << access_delay_ns_p_.get_value()
       << (access_delay_ns_p_.is_preset_value() ? " [preset]" : " [default]")
       << "\n";
    os << std::hex << std::setfill('0');
    os << "  CAUSE=0x"          << cause_
       << " PHYS_ADDR=0x"       << phys_addr_
       << " ENABLE=0x"          << enable_.read()
       << " ACCRUED=0x"         << accrued_
       << " PLIC_ENABLE=0x"     << plic_enable_.read()
       << " LOCAL_ENABLE=0x"    << local_enable_.read() << "\n";
    os << std::dec << std::setfill(' ');
    os << "  irq_local=" << irq_local_active()
       << " irq_plic="  << irq_plic_active() << "\n";
}

} // namespace smc
