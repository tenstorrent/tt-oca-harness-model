// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// src/smc_fabric.cpp
//
// Implementation of the SMC Fabric LT SystemC model.
// See include/smc_fabric.h and doc/ for full design rationale.
// ===========================================================================

#include "smc_fabric.h"

#include "reg_access.h"
#include "sim_log.h"
#include "smc_axi_extension.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>

namespace smc {

namespace {

const char* tlm_cmd_str(const tlm::tlm_generic_payload& trans)
{
    if (trans.is_read())  return "read";
    if (trans.is_write()) return "write";
    return "other";
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

smc_fabric::smc_fabric(sc_core::sc_module_name name)
    : smc_fabric(name, config{})
{}

smc_fabric::smc_fabric(sc_core::sc_module_name name, const config& cfg)
    : sc_core::sc_module(name)
    , cfg_(cfg)
{
    // Register b_transport callbacks on all target sockets.
    jtag_axi_in  .register_b_transport(this, &smc_fabric::bt_jtag);
    mmio_in      .register_b_transport(this, &smc_fabric::bt_mmio);
    data_accel_in.register_b_transport(this, &smc_fabric::bt_data_accel);
    log_in       .register_b_transport(this, &smc_fabric::bt_log);
    sys_axi_in   .register_b_transport(this, &smc_fabric::bt_sys_axi);
    sep_axi_in   .register_b_transport(this, &smc_fabric::bt_sep_axi);

    // DMI is never granted; register a uniform deny handler.  data_accel_in
    // is a multi_passthrough_target_socket, so it uses the tagged overload.
    jtag_axi_in  .register_get_direct_mem_ptr(this, &smc_fabric::get_direct_mem_ptr);
    mmio_in      .register_get_direct_mem_ptr(this, &smc_fabric::get_direct_mem_ptr);
    data_accel_in.register_get_direct_mem_ptr(this,
        static_cast<bool (smc_fabric::*)(int, tlm::tlm_generic_payload&, tlm::tlm_dmi&)>(
            &smc_fabric::get_direct_mem_ptr));
    log_in       .register_get_direct_mem_ptr(this, &smc_fabric::get_direct_mem_ptr);
    sys_axi_in   .register_get_direct_mem_ptr(this, &smc_fabric::get_direct_mem_ptr);
    sep_axi_in   .register_get_direct_mem_ptr(this, &smc_fabric::get_direct_mem_ptr);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i.neg();   // event finder: resolved after port binding
    dont_initialize();

    SIM_LOG_INFO(this,
        "smc_fabric constructed: "
        << std::hex
        << "local_base=0x"   << cfg_.local_base_addr
        << " global_base=0x" << cfg_.global_base_addr
        << " region_size=0x" << cfg_.region_size
        << std::dec
        << " no_addr_remap=" << (cfg_.no_addr_remap ? "true" : "false")
        << " reg_access_ns=" << cfg_.reg_access_ns);
}

// ---------------------------------------------------------------------------
// Testbench API
// ---------------------------------------------------------------------------

void smc_fabric::write_global_base(uint64_t v)
{
    cfg_.global_base_addr = v;
    invalidate_all_dmi();
}

void smc_fabric::write_region_size(uint64_t v)
{
    cfg_.region_size = v;
    invalidate_all_dmi();
}

// ---------------------------------------------------------------------------
// b_transport — internal masters (JTAG, MMIO, DMA, Log)
// All four paths are identical: alias remap → local/outbound split.
// ---------------------------------------------------------------------------

void smc_fabric::bt_internal(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time&         delay,
                              const char*               ingress)
{
    const uint64_t orig = trans.get_address();
    uint64_t addr = apply_alias_remap(orig);
    trans.set_address(addr);

    const bool local = is_local(addr);
    if (remap_debug_.hit) {
        SIM_LOG_TRACE(this,
            ingress << " " << tlm_cmd_str(trans)
            << " orig=0x" << std::hex << orig
            << " mapped=0x" << addr
            << std::dec << " alias[" << remap_debug_.region_index << "]"
            << " -> " << (local ? "local" : "outbound"));
    } else {
        SIM_LOG_TRACE(this,
            ingress << " " << tlm_cmd_str(trans)
            << " orig=0x" << std::hex << orig
            << " mapped=0x" << addr
            << " no_alias -> " << (local ? "local" : "outbound"));
    }

    if (local) {
        trans.set_address(to_local_addr(addr));
        route_local(trans, delay);
    } else {
        route_outbound(trans, delay);
    }
}

void smc_fabric::bt_jtag      (tlm::tlm_generic_payload& t, sc_core::sc_time& d) { bt_internal(t, d, "jtag_axi_in"); }
void smc_fabric::bt_mmio      (tlm::tlm_generic_payload& t, sc_core::sc_time& d) { bt_internal(t, d, "mmio_in"); }
void smc_fabric::bt_data_accel(int /*id*/, tlm::tlm_generic_payload& t, sc_core::sc_time& d) { bt_internal(t, d, "data_accel_in"); }
void smc_fabric::bt_log       (tlm::tlm_generic_payload& t, sc_core::sc_time& d) { bt_internal(t, d, "log_in"); }

// ---------------------------------------------------------------------------
// b_transport — sys_axi_in: inbound filter then local route
// ---------------------------------------------------------------------------

void smc_fabric::bt_sys_axi(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time&         delay)
{
    auto*   ext    = trans.get_extension<smc_axi_extension>();
    uint8_t src_id = ext ? static_cast<uint8_t>(ext->source_id & 0xFu) : 0u;
    // prot[1] in smc_axi_extension = NS bit (1 = non-secure).
    bool    ns     = ext ? ((ext->prot >> 1) & 1u) != 0u : true;

    SIM_LOG_TRACE(this,
        "sys_axi_in " << tlm_cmd_str(trans)
        << " addr=0x" << std::hex << trans.get_address()
        << std::dec << " src_id=" << static_cast<unsigned>(src_id)
        << " ns=" << ns);

    if (!inbound_filter_allow(trans.get_address(), src_id, ns,
                              trans.is_write())) {
        SIM_LOG_DEBUG(this,
            "sys_axi_in inbound filter deny addr=0x" << std::hex
            << trans.get_address() << std::dec
            << " src_id=" << static_cast<unsigned>(src_id)
            << " ns=" << ns);
        fill_deny_response(trans);
        trans.set_dmi_allowed(false);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    const uint32_t local = to_local_addr(trans.get_address());
    SIM_LOG_TRACE(this,
        "sys_axi_in -> route_local local=0x" << std::hex << local);
    trans.set_address(local);
    route_local(trans, delay);
}

// ---------------------------------------------------------------------------
// b_transport — sep_axi_in: bypass all remap and filter, truncate and route
// ---------------------------------------------------------------------------

void smc_fabric::bt_sep_axi(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time&         delay)
{
    const uint32_t local = to_local_addr(trans.get_address());
    SIM_LOG_TRACE(this,
        "sep_axi_in " << tlm_cmd_str(trans)
        << " addr=0x" << std::hex << trans.get_address()
        << " -> route_local local=0x" << local << " (no filter)");
    trans.set_address(local);
    route_local(trans, delay);
}

// ---------------------------------------------------------------------------
// DMI — always denied
// ---------------------------------------------------------------------------

bool smc_fabric::get_direct_mem_ptr(tlm::tlm_generic_payload& /*trans*/,
                                     tlm::tlm_dmi&             dmi_data)
{
    dmi_data.allow_read_write();
    dmi_data.set_start_address(0);
    dmi_data.set_end_address(std::numeric_limits<sc_dt::uint64>::max());
    return false;
}

// Tagged overload for data_accel_in (a multi_passthrough_target_socket);
// identical deny-DMI behavior, ignores which bound master asked.
bool smc_fabric::get_direct_mem_ptr(int /*id*/,
                                     tlm::tlm_generic_payload& trans,
                                     tlm::tlm_dmi&             dmi_data)
{
    return get_direct_mem_ptr(trans, dmi_data);
}

// ---------------------------------------------------------------------------
// Routing helpers
// ---------------------------------------------------------------------------

uint64_t smc_fabric::apply_alias_remap(uint64_t addr)
{
    for (unsigned i = 0; i < alias_regions_.size(); ++i) {
        const alias_region& r = alias_regions_[i];
        if (r.valid && addr >= r.start && addr < r.end) {
            remap_debug_ = {true, i};
            return addr + static_cast<uint64_t>(r.offset);
        }
    }
    remap_debug_ = {false, 0};
    return addr;
}

bool smc_fabric::is_local(uint64_t addr) const
{
    return (addr >= cfg_.local_base_addr  &&
            addr <  cfg_.local_base_addr  + LOCAL_ALIAS_REGION_SIZE) ||
           (addr >= cfg_.global_base_addr &&
            addr <  cfg_.global_base_addr + LOCAL_ALIAS_REGION_SIZE);
}

uint32_t smc_fabric::to_local_addr(uint64_t addr) const
{
    // Force bits[31:25] to match local_base[31:25] so local-alias and
    // global-alias transactions decode identically in the local crossbar.
    return (static_cast<uint32_t>(cfg_.local_base_addr) & 0xFE00'0000u)
         | (static_cast<uint32_t>(addr)                 & 0x01FF'FFFFu);
}

// ---------------------------------------------------------------------------
// route_local — decode 32-bit masked address → target socket or CSR handler
// ---------------------------------------------------------------------------

void smc_fabric::route_local(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time&         delay)
{
    uint32_t a = static_cast<uint32_t>(trans.get_address());

    // --- DFT CSRs (forwarded downstream) ---
    if (a >= DFT_CSR_BASE && a < DFT_CSR_END) {
        SIM_LOG_TRACE(this,
            "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
            << " -> to_dft_csr");
        to_dft_csr->b_transport(trans, delay);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    // --- SMC base config window (fabric global CSRs) ---
    if (a >= SMC_BASE_CONFIG_BASE && a < SMC_BASE_CONFIG_END) {
        SIM_LOG_TRACE(this,
            "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
            << " -> smc_base_config_csr");
        if (!handle_global_csr(trans, a - SMC_BASE_CONFIG_BASE)) {
            if (trans.is_read())
                std::memset(trans.get_data_ptr(), 0, trans.get_data_length());
            trans.set_response_status(tlm::TLM_OK_RESPONSE); // unmodelled base_config regs: RAZ/WI
        }
        trans.set_dmi_allowed(false);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    // --- Alias remap CSRs (handled internally, table updated directly) ---
    if (a >= AR_CTRL_BASE && a < AR_CTRL_END) {
        SIM_LOG_TRACE(this,
            "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
            << " -> alias_remap_csr");
        handle_alias_remap(trans, a - AR_CTRL_BASE);
        trans.set_dmi_allowed(false);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    // --- M-mode remap CSRs ---
    if (a >= MR_CTRL_BASE && a < MR_CTRL_END) {
        SIM_LOG_TRACE(this,
            "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
            << " -> mmode_remap_csr");
        handle_mmode_remap(trans, a - MR_CTRL_BASE);
        trans.set_dmi_allowed(false);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    // --- Xvisor remap CSRs ---
    if (a >= XR_CTRL_BASE && a < XR_CTRL_END) {
        SIM_LOG_TRACE(this,
            "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
            << " -> xvisor_remap_csr");
        handle_xvisor_remap(trans, a - XR_CTRL_BASE);
        trans.set_dmi_allowed(false);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    // --- Inbound filter CSRs ---
    if (a >= IB_FILTER_BASE && a < IB_FILTER_END) {
        SIM_LOG_TRACE(this,
            "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
            << " -> inbound_filter_csr");
        handle_inbound_filter(trans, a - IB_FILTER_BASE);
        trans.set_dmi_allowed(false);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    // --- Outbound filter CSRs ---
    if (a >= OB_FILTER_BASE && a < OB_FILTER_END) {
        SIM_LOG_TRACE(this,
            "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
            << " -> outbound_filter_csr");
        handle_outbound_filter(trans, a - OB_FILTER_BASE);
        trans.set_dmi_allowed(false);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    // --- Mailbox CSRs (forwarded downstream) ---
    if (a >= MAILBOX_BASE && a < MAILBOX_END) {
        SIM_LOG_TRACE(this,
            "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
            << " -> to_mailbox");
        to_mailbox->b_transport(trans, delay);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    // --- Regular local targets ---
    auto* sock = local_decode(a);
    if (!sock) {
        SIM_LOG_DEBUG(this,
            "route_local decode miss " << tlm_cmd_str(trans)
            << " addr=0x" << std::hex << a);
        fill_deny_response(trans);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    const char* tgt = "unknown";
    if (sock == &to_front_port)      tgt = "to_front_port";
    else if (sock == &to_data_accel_ctrl) tgt = "to_data_accel_ctrl";
    else if (sock == &to_dfd_apb)    tgt = "to_dfd_apb";
    else if (sock == &to_periph)      tgt = "to_periph";

    SIM_LOG_TRACE(this,
        "route_local " << tlm_cmd_str(trans) << " addr=0x" << std::hex << a
        << " -> " << tgt);
    (*sock)->b_transport(trans, delay);
}

// ---------------------------------------------------------------------------
// route_outbound — M-mode/Xvisor remap → outbound filter → output_axi
// ---------------------------------------------------------------------------

void smc_fabric::route_outbound(tlm::tlm_generic_payload& trans,
                                 sc_core::sc_time&         delay)
{
    auto*   ext    = trans.get_extension<smc_axi_extension>();
    uint8_t src_id = ext ? static_cast<uint8_t>(ext->source_id & 0xFu)
                         : SMC_SRC_ID;
    bool    ns     = ext ? ((ext->prot >> 1) & 1u) != 0u : false;

    uint64_t addr = trans.get_address();
    const char* path = "plain";

    SIM_LOG_TRACE(this,
        "route_outbound " << tlm_cmd_str(trans)
        << " enter addr=0x" << std::hex << addr
        << std::dec << " src_id=" << static_cast<unsigned>(src_id)
        << " ns=" << ns);

    if (!cfg_.no_addr_remap) {
        switch (classify_outbound(addr)) {
        case outbound_path::mmode:
            path = "mmode";
            addr = apply_output_remap(addr, mmode_regions_,
                                      MMODE_REMAP_START, MMODE_SRC_ID, ext);
            break;
        case outbound_path::xvisor:
            path = "xvisor";
            addr = apply_output_remap(addr, xvisor_regions_,
                                      XVISOR_REMAP_START, OTHERS_SRC_ID, ext);
            break;
        case outbound_path::plain:
        default:
            if (ext) ext->source_id = SMC_SRC_ID;
            break;
        }
        trans.set_address(addr);
        SIM_LOG_TRACE(this,
            "route_outbound " << path << " remapped=0x" << std::hex << addr);
    } else {
        SIM_LOG_TRACE(this, "route_outbound bypass remap (no_addr_remap)");
    }
    // bypass (no_addr_remap=true): address and source_id pass through unchanged.

    // Refresh src_id from extension after any remap overwrote it.
    if (ext) src_id = static_cast<uint8_t>(ext->source_id & 0xFu);

    if (!outbound_filter_allow(addr, src_id, ns, trans.is_write())) {
        SIM_LOG_DEBUG(this,
            "route_outbound filter deny addr=0x" << std::hex << addr << std::dec
            << " src_id=" << static_cast<unsigned>(src_id)
            << " ns=" << ns);
        fill_deny_response(trans);
        trans.set_dmi_allowed(false);
        delay += sc_core::sc_time(cfg_.reg_access_ns, sc_core::SC_NS);
        return;
    }

    SIM_LOG_TRACE(this,
        "route_outbound -> output_axi addr=0x" << std::hex << addr);
    output_axi->b_transport(trans, delay);
}

// ---------------------------------------------------------------------------
// Filter logic
// ---------------------------------------------------------------------------

// Bit-exact image of axi_filter_wrap + traffic_filter:
//   filter_hit[f] = addr_mode & in_range & pass_src_id & pass_ns (group/burst
//                   filtering disabled in SMC) — see smc_input/output_fabric.
//   lowest-index hit wins (lzc); decision = read_en/write_en of that filter.
//   no hit → !BlockByDefault.
// SMC enables EnSrcIdFilter and EnNsFilter; EnGroupIdFilter is 0.  Bursts are
// not modelled in LT (single-beat payloads), so pass_burst is always true and
// allow_burst only selects the address-match granularity (4 KiB vs 8 B).
bool smc_fabric::filter_lookup(const std::array<filter_entry, 16>& table,
                                uint64_t addr, uint8_t src_id, bool ns,
                                bool is_write, bool block_by_default) const
{
    for (const auto& e : table) {
        if (!e.addr_mode)                            continue;  // disabled
        const unsigned shift = e.allow_burst ? 12u : 3u;        // granularity
        const uint64_t gmask = (1ULL << shift) - 1ULL;
        const uint64_t lo    = e.start_addr & ~gmask;
        const uint64_t hi    = e.end_addr   |  gmask;           // inclusive
        if (addr < lo || addr > hi)                  continue;  // out of range
        if (e.src_id != 0 && src_id != e.src_id)     continue;  // src-id match
        if (ns != e.allow_ns)                        continue;  // exact NS match
        // EnGroupIdFilter=0 → group-id always passes; first (lowest) hit wins.
        return is_write ? e.write_en : e.read_en;
    }
    return !block_by_default;
}

bool smc_fabric::inbound_filter_allow(uint64_t addr, uint8_t src_id,
                                       bool ns, bool is_write) const
{
    // smc_input_fabric: BlockByDefault = 1.
    return filter_lookup(inbound_filter_, addr, src_id, ns, is_write, true);
}

bool smc_fabric::outbound_filter_allow(uint64_t addr, uint8_t src_id,
                                        bool ns, bool is_write) const
{
    // smc_output_fabric: BlockByDefault = 0.
    return filter_lookup(outbound_filter_, addr, src_id, ns, is_write, false);
}

// ---------------------------------------------------------------------------
// Outbound path classification (address-based)
// ---------------------------------------------------------------------------

smc_fabric::outbound_path smc_fabric::classify_outbound(uint64_t addr) const
{
    auto in_win = [&](uint64_t base, uint64_t start, uint64_t sz) {
        return addr >= base + start && addr < base + start + sz;
    };

    if (in_win(cfg_.local_base_addr,  MMODE_REMAP_START, MMODE_REMAP_SIZE) ||
        in_win(cfg_.global_base_addr, MMODE_REMAP_START, MMODE_REMAP_SIZE))
        return outbound_path::mmode;

    if (in_win(cfg_.local_base_addr,  XVISOR_REMAP_START, XVISOR_REMAP_SIZE) ||
        in_win(cfg_.global_base_addr, XVISOR_REMAP_START, XVISOR_REMAP_SIZE))
        return outbound_path::xvisor;

    return outbound_path::plain;
}

// Bit-exact image of output_remap.sv (lines 67-83):
//   adjusted = addr - window_base   (RegionBase)
//   idx      = adjusted[IdxStart +: clog2(NumRegions)]   ← bits [22:20]
//   out      = { offset[55:IdxStart], adjusted[IdxStart-1:0] }
// There is no per-entry valid bit; every region is always active, so the
// raw REGION_ATTRS.offset[55:0] register simply replaces the upper address
// bits.  (At reset offset==0, matching the RTL default.)
uint64_t smc_fabric::apply_output_remap(uint64_t                          addr,
                                         const std::array<alias_region, 8>& table,
                                         uint64_t                           window_start,
                                         uint8_t                            src_id_override,
                                         smc_axi_extension*                 ext) const
{
    const unsigned idx_start = OUTPUT_REMAP_IDX_START;
    const uint64_t low_mask  = (1ULL << idx_start) - 1ULL;        // [19:0]
    const uint64_t win_size  = static_cast<uint64_t>(table.size()) << idx_start;

    // RegionBase is the window base that addr fell into (local or global alias).
    uint64_t base = cfg_.global_base_addr + window_start;
    if (addr >= cfg_.local_base_addr + window_start &&
        addr <  cfg_.local_base_addr + window_start + win_size)
        base = cfg_.local_base_addr + window_start;

    const uint64_t adjusted = addr - base;
    const unsigned idx = static_cast<unsigned>((adjusted >> idx_start) &
                                               (table.size() - 1));

    if (ext) ext->source_id = src_id_override;

    const uint64_t offset =
        static_cast<uint64_t>(table[idx].offset) & OUTPUT_REMAP_OFFSET_MASK;
    return (offset & ~low_mask) | (adjusted & low_mask);
}

// ---------------------------------------------------------------------------
// Local address decode → initiator socket
// ---------------------------------------------------------------------------

tlm_utils::simple_initiator_socket<smc_fabric, 64>*
smc_fabric::local_decode(uint32_t a)
{
    // front_port: WDT/debug, cpu_ctrl, scratchpad RAM, PLIC, CLINT/BEU.
    // PLIC/CLINT lie above LOCAL_ALIAS_REGION_SIZE (16 MB), so internal masters
    // only reach them via the global alias aperture; sys/sep bypass the demux
    // and may hit any decoded local target directly.
    if ((a >= FRONT_WDT_DEBUG_BASE && a < FRONT_WDT_DEBUG_END) ||
        (a >= FRONT_CPU_CTRL_BASE  && a < FRONT_CPU_CTRL_END)  ||
        (a >= FRONT_SPM_BASE       && a < FRONT_SPM_END)       ||
        (a >= FRONT_PLIC_BASE      && a < FRONT_PLIC_END)      ||
        (a >= FRONT_CLINT_BEU_BASE && a < FRONT_CLINT_BEU_END))
        return &to_front_port;

    if (a >= DACCEL_DMA_ZEROER_BASE && a < DACCEL_DMA_ZEROER_END)
        return &to_data_accel_ctrl;

    if (a >= DFD_REGS_BASE && a < DFD_REGS_END)
        return &to_dfd_apb;

    if ((a >= PERIPH_MAIN_BASE && a < PERIPH_MAIN_END) ||
        (a >= PERIPH_EXT_BASE  && a < PERIPH_EXT_END)  ||
        (a >= PERIPH_I3C_BASE  && a < PERIPH_I3C_END)  ||
        // VP-only AOU park — see AOU_PARK_BASE in the header.
        (a >= AOU_PARK_BASE    && a < AOU_PARK_END))
        return &to_periph;

    // No matching local target → caller issues TLM_ADDRESS_ERROR_RESPONSE.
    return nullptr;
}

// ---------------------------------------------------------------------------
// Error response
// ---------------------------------------------------------------------------

void smc_fabric::fill_deny_response(tlm::tlm_generic_payload& trans)
{
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    trans.set_dmi_allowed(false);

    if (trans.is_read()) {
        constexpr uint32_t POISON = 0xBADC'AB1Eu;
        unsigned char* ptr = trans.get_data_ptr();
        unsigned int   len = trans.get_data_length();

        // Fill complete 32-bit words.
        const unsigned words = len / 4u;
        for (unsigned i = 0; i < words; ++i)
            std::memcpy(ptr + i * 4u, &POISON, 4u);

        // Fill any trailing bytes from the least-significant byte of POISON.
        for (unsigned i = words * 4u; i < len; ++i)
            ptr[i] = reinterpret_cast<const unsigned char*>(&POISON)[i % 4u];
    }
}

// ---------------------------------------------------------------------------
// DMI invalidation — called on every remap/filter/global-CSR write
// ---------------------------------------------------------------------------

void smc_fabric::invalidate_all_dmi()
{
    // The fabric never grants DMI (get_direct_mem_ptr always returns false),
    // so there are no outstanding grants to revoke.  This call is a no-op
    // intentionally; the hook exists so future DMI-granting paths only need
    // to add socket calls here without touching each CSR write-path.
}

// ---------------------------------------------------------------------------
// Reset — SC_METHOD sensitive to negedge of rst_n_i
// ---------------------------------------------------------------------------

void smc_fabric::reset_proc()
{
    alias_regions_.fill({});
    mmode_regions_.fill({});
    xvisor_regions_.fill({});
    inbound_filter_.fill({});
    outbound_filter_.fill({});
    remap_debug_ = {};

    // Restore RW registers to power-on defaults.
    cfg_.global_base_addr = 0x4000'0000ULL;
    cfg_.region_size      = 0x0100'0000ULL;
}

// ---------------------------------------------------------------------------
// Internal CSR handlers
// ---------------------------------------------------------------------------

// SMC base config CSRs — LOCAL_BASE (RO), GLOBAL_BASE (RW), REGION_SIZE (RW).
// sub_offset is relative to SMC_BASE_CONFIG_BASE.
bool smc_fabric::handle_global_csr(tlm::tlm_generic_payload& trans,
                                    uint32_t                  sub_offset)
{
    if (trans.get_command() == tlm::TLM_IGNORE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        trans.set_dmi_allowed(false);
        return true;
    }
    auto read32  = [&](uint32_t val) {
        if (trans.get_data_length() >= 4u)
            std::memcpy(trans.get_data_ptr(), &val, 4u);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    };
    auto write32 = [&](uint32_t& dst) {
        uint32_t val = 0;
        std::memcpy(&val, trans.get_data_ptr(),
                    std::min(trans.get_data_length(), 4u));
        dst = val;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    };

    if (sub_offset == GCSR_LOCAL_BASE) {
        if (trans.is_read())
            read32(static_cast<uint32_t>(cfg_.local_base_addr));
        else
            trans.set_response_status(tlm::TLM_OK_RESPONSE); // RO: ignore write
        return true;
    }
    if (sub_offset == GCSR_GLOBAL_BASE) {
        if (trans.is_write()) {
            uint32_t tmp = static_cast<uint32_t>(cfg_.global_base_addr);
            write32(tmp);
            cfg_.global_base_addr = static_cast<uint64_t>(tmp);
            invalidate_all_dmi();
        } else {
            read32(static_cast<uint32_t>(cfg_.global_base_addr));
        }
        return true;
    }
    if (sub_offset == GCSR_REGION_SIZE) {
        if (trans.is_write()) {
            uint32_t tmp = static_cast<uint32_t>(cfg_.region_size);
            write32(tmp);
            cfg_.region_size = static_cast<uint64_t>(tmp);
            invalidate_all_dmi();
        } else {
            read32(static_cast<uint32_t>(cfg_.region_size));
        }
        return true;
    }
    return false; // not a fabric-owned CSR; caller should forward to to_cpu_ctrl
}

// Shared field handler for alias_region entries.
// field_off is the byte offset within one entry (0x00 / 0x08 / 0x10 / 0x18).
void smc_fabric::handle_remap_entry(alias_region&             r,
                                     uint32_t                  field_off,
                                     tlm::tlm_generic_payload& trans)
{
    if (trans.get_command() == tlm::TLM_IGNORE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (trans.is_write()) {
        uint64_t val = 0;
        std::memcpy(&val, trans.get_data_ptr(),
                    std::min(trans.get_data_length(), 8u));
        switch (field_off) {
        case 0x00: r.start     = val;                        break;
        case 0x08: r.end       = val;                        break;
        case 0x10: r.offset    = static_cast<int64_t>(val); break;
        case 0x18:
            r.valid     = (val & 1u) != 0u;
            r.cacheable = (val & 2u) != 0u;
            break;
        default: break;
        }
    } else {
        uint64_t val = 0;
        switch (field_off) {
        case 0x00: val = r.start;                                  break;
        case 0x08: val = r.end;                                    break;
        case 0x10: val = static_cast<uint64_t>(r.offset);         break;
        case 0x18:
            val = static_cast<uint64_t>(r.valid     ? 1u : 0u)
                | static_cast<uint64_t>(r.cacheable ? 2u : 0u);
            break;
        default: break;
        }
        unsigned len = std::min(trans.get_data_length(), 8u);
        std::memcpy(trans.get_data_ptr(), &val, len);
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

// Alias remap CSRs — 8 entries × 0x20 bytes each.
void smc_fabric::handle_alias_remap(tlm::tlm_generic_payload& trans,
                                     uint32_t                  sub_offset)
{
    unsigned entry = sub_offset / 0x20u;
    uint32_t field = sub_offset % 0x20u;
    if (entry >= alias_regions_.size()) { fill_deny_response(trans); return; }
    handle_remap_entry(alias_regions_[entry], field, trans);
    invalidate_all_dmi();
}

// Shared field handler for output-remap (M-mode / Xvisor) entries.
// Bit-exact image of output_remap.rdl: each entry is one 64-bit REGION_ATTRS
// register holding offset[55:0] at byte offset 0x00.  There is no valid bit.
// A 64-bit access (len>=8) at field 0x00 reads/writes the whole register;
// 32-bit accesses see offset[31:0] at 0x00 and offset[55:32] at 0x04.
// The raw register image is stored in alias_region::offset.
void smc_fabric::handle_output_remap_entry(alias_region&             r,
                                            uint32_t                  field_off,
                                            tlm::tlm_generic_payload& trans)
{
    if (trans.get_command() == tlm::TLM_IGNORE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    uint64_t reg = static_cast<uint64_t>(r.offset) & OUTPUT_REMAP_OFFSET_MASK;
    unsigned len = std::min(trans.get_data_length(), 8u);

    if (trans.is_write()) {
        if (field_off == 0x00 && len >= 8u) {
            uint64_t val = 0;
            std::memcpy(&val, trans.get_data_ptr(), 8u);
            reg = val;
        } else {
            uint32_t val = 0;
            std::memcpy(&val, trans.get_data_ptr(), std::min(len, 4u));
            if (field_off == 0x00)            // offset[31:0]
                reg = (reg & 0xFFFF'FFFF'0000'0000ULL) | val;
            else if (field_off == 0x04)       // offset[55:32]
                reg = (reg & 0x0000'0000'FFFF'FFFFULL) |
                      (static_cast<uint64_t>(val & 0x00FF'FFFFu) << 32);
        }
        r.offset = static_cast<int64_t>(reg & OUTPUT_REMAP_OFFSET_MASK);
    } else {
        uint64_t val = 0;
        if (field_off == 0x00)
            val = (len >= 8u) ? reg : (reg & 0xFFFF'FFFFULL);
        else if (field_off == 0x04)
            val = (reg >> 32) & 0x00FF'FFFFULL;
        std::memcpy(trans.get_data_ptr(), &val, len);
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

// M-mode remap CSRs — 8 entries × one 64-bit REGION_ATTRS register each
// (stride 0x08), bit-exact to output_remap.rdl.  Region slots
// are fixed by MMODE_REMAP_START/SIZE and IdxStart=20; the offset register
// supplies the destination's upper address bits (see apply_output_remap).
void smc_fabric::handle_mmode_remap(tlm::tlm_generic_payload& trans,
                                     uint32_t                  sub_offset)
{
    unsigned entry = sub_offset / 0x08u;
    uint32_t field = sub_offset % 0x08u;
    if (entry >= mmode_regions_.size()) { fill_deny_response(trans); return; }
    handle_output_remap_entry(mmode_regions_[entry], field, trans);
    invalidate_all_dmi();
}

// Xvisor remap — same layout as M-mode.
void smc_fabric::handle_xvisor_remap(tlm::tlm_generic_payload& trans,
                                      uint32_t                  sub_offset)
{
    unsigned entry = sub_offset / 0x08u;
    uint32_t field = sub_offset % 0x08u;
    if (entry >= xvisor_regions_.size()) { fill_deny_response(trans); return; }
    handle_output_remap_entry(xvisor_regions_[entry], field, trans);
    invalidate_all_dmi();
}

// 64-bit data bus → data_bus_width encoded as log2(8) = 3 (RO field).
static constexpr uint64_t FILTER_DBW_LOG2 = 3;
static constexpr uint64_t FILTER_ADDR_MASK = 0x00FF'FFFF'FFFF'FFFFULL;

// Pack the filter_entry fields into the 64-bit FILTER_CONFIG register image.
static uint64_t filter_cfg_pack(const smc_fabric::filter_entry& e)
{
    uint64_t cfg = 0;
    cfg |= (e.read_en     ? 1ULL : 0ULL) << 0;
    cfg |= (e.write_en    ? 1ULL : 0ULL) << 1;
    cfg |= (e.addr_mode   ? 1ULL : 0ULL) << 4;
    cfg |= (e.allow_ns    ? 1ULL : 0ULL) << 8;
    cfg |= FILTER_DBW_LOG2 << 12;                              // data_bus_width (RO)
    cfg |= static_cast<uint64_t>(e.src_id   & 0xFu) << 16;
    cfg |= static_cast<uint64_t>(e.group_id & 0xFu) << 20;
    cfg |= (e.allow_burst ? 1ULL : 0ULL) << 24;
    cfg |= (e.locked      ? 1ULL : 0ULL) << 63;
    return cfg;
}

// Unpack a FILTER_CONFIG write into the filter_entry (data_bus_width is RO;
// locked is write-one-to-set and cannot be cleared by software).
static void filter_cfg_unpack(smc_fabric::filter_entry& e, uint64_t cfg)
{
    e.read_en     = ((cfg >> 0)  & 1u) != 0u;
    e.write_en    = ((cfg >> 1)  & 1u) != 0u;
    e.addr_mode   = ((cfg >> 4)  & 1u) != 0u;
    e.allow_ns    = ((cfg >> 8)  & 1u) != 0u;
    e.src_id      = static_cast<uint8_t>((cfg >> 16) & 0xFu);
    e.group_id    = static_cast<uint8_t>((cfg >> 20) & 0xFu);
    e.allow_burst = ((cfg >> 24) & 1u) != 0u;
    e.locked      = e.locked || (((cfg >> 63) & 1u) != 0u);   // woset
}

// Shared helper for inbound_filter / outbound_filter CSR access.
// sub_offset is relative to the filter array base.
// Filter entry layout — bit-exact to filter_ctrl.rdl:
//   16 entries × 0x20 bytes; each entry holds three 64-bit registers:
//   +0x00  FILTER_CONFIG  read_en[0] write_en[1] addr_mode[4] allow_ns[8]
//                         data_bus_width[14:12](RO) src_id[19:16]
//                         group_id[23:20] allow_burst[24] locked[63]
//   +0x08  START_ADDR     start_addr[55:0]
//   +0x10  END_ADDR       end_addr[55:0]
//   +0x18  (reserved within the 0x20 stride)
// 32-bit accesses see the low/high word of the addressed 64-bit register.
static void handle_filter_entry_csr(smc_fabric::filter_entry& e,
                                     uint32_t                  field_off,
                                     tlm::tlm_generic_payload& trans)
{
    if (trans.get_command() == tlm::TLM_IGNORE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    enum reg_sel { CFG, START, END, RSVD };
    reg_sel which = RSVD;
    bool    high  = false;
    switch (field_off) {
    case 0x00: which = CFG;   high = false; break;
    case 0x04: which = CFG;   high = true;  break;
    case 0x08: which = START; high = false; break;
    case 0x0C: which = START; high = true;  break;
    case 0x10: which = END;   high = false; break;
    case 0x14: which = END;   high = true;  break;
    default:   which = RSVD;                break;
    }

    auto reg_val = [&]() -> uint64_t {
        switch (which) {
        case CFG:   return filter_cfg_pack(e);
        case START: return e.start_addr & FILTER_ADDR_MASK;
        case END:   return e.end_addr   & FILTER_ADDR_MASK;
        default:    return 0ULL;
        }
    };

    const unsigned len = std::min(trans.get_data_length(), 8u);

    if (trans.is_write()) {
        // Once locked, the filter configuration is read-only.
        if (e.locked) { trans.set_response_status(tlm::TLM_OK_RESPONSE); return; }

        const uint64_t cur = reg_val();
        uint64_t nv;
        if (!high && len >= 8u) {
            uint64_t v = 0;
            std::memcpy(&v, trans.get_data_ptr(), 8u);
            nv = v;
        } else {
            uint32_t v = 0;
            std::memcpy(&v, trans.get_data_ptr(), std::min(len, 4u));
            // 32-bit lane merge into the 64-bit register image (shared helper).
            nv = high ? regmodel::apply_write_mask(
                            cur, static_cast<uint64_t>(v) << 32, 0xFFFF'FFFF'0000'0000ULL)
                      : regmodel::apply_write_mask(
                            cur, static_cast<uint64_t>(v), 0x0000'0000'FFFF'FFFFULL);
        }
        switch (which) {
        case CFG:   filter_cfg_unpack(e, nv);                break;
        case START: e.start_addr = nv & FILTER_ADDR_MASK;    break;
        case END:   e.end_addr   = nv & FILTER_ADDR_MASK;    break;
        default:    break;                                   // reserved: ignore
        }
    } else {
        const uint64_t rv = reg_val();
        uint64_t out = (!high && len >= 8u) ? rv
                     : high ? ((rv >> 32) & 0xFFFF'FFFFULL)
                            : (rv & 0xFFFF'FFFFULL);
        std::memcpy(trans.get_data_ptr(), &out, len);
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void smc_fabric::handle_inbound_filter(tlm::tlm_generic_payload& trans,
                                        uint32_t                  sub_offset)
{
    unsigned entry = sub_offset / 0x20u;
    uint32_t field = sub_offset % 0x20u;
    if (entry >= inbound_filter_.size()) { fill_deny_response(trans); return; }
    handle_filter_entry_csr(inbound_filter_[entry], field, trans);
}

void smc_fabric::handle_outbound_filter(tlm::tlm_generic_payload& trans,
                                         uint32_t                  sub_offset)
{
    unsigned entry = sub_offset / 0x20u;
    uint32_t field = sub_offset % 0x20u;
    if (entry >= outbound_filter_.size()) { fill_deny_response(trans); return; }
    handle_filter_entry_csr(outbound_filter_[entry], field, trans);
}

}  // namespace smc
