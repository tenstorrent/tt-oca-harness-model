// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <systemc>
#include <cassert>
#include <cstring>
#include <functional>
#include "sep_cpu_ctrl.h"
#include "sep_axi_extension.h"

// 64-to-32 bit bus adapter for DMA sys_initiator_socket (64-bit) → SimpleBus (32-bit).
class dma_sys_bus_adapter : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<dma_sys_bus_adapter, 64>    tgt;
    tlm_utils::simple_initiator_socket<dma_sys_bus_adapter, 32> ini;

    SC_HAS_PROCESS(dma_sys_bus_adapter);
    dma_sys_bus_adapter(sc_core::sc_module_name n)
        : sc_module(n), tgt("tgt"), ini("ini") {
        tgt.register_b_transport(this, &dma_sys_bus_adapter::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        ini->b_transport(trans, delay);
    }
};

/**
 * @brief 32-to-64 bit bus adapter for the OT Mailbox IP (Port 0, SEP side).
 *
 * The mailbox registers are 64-bit wide (regwidth=64, accesswidth=64 per RDL)
 * but the SEP RISC-V core uses a 32-bit data bus. This adapter sits between the
 * SEP SimpleBus and the mailbox unit, doing the width conversion that
 * axi_lite_dw_converter does in RTL, so software only needs normal 32-bit
 * loads and stores.
 *
 * The adapter is channel-agnostic: it widens the access and passes the address
 * through untouched, leaving the channel/port decode to the mailbox unit. Only
 * the register offset within a block matters here, which is why the FIFO
 * registers are recognised modulo the 0x800 block size.
 *
 * Access rules
 * ------------
 * WRITE_DATA / READ_DATA — one FIFO push or pop per bus transaction, matching
 * the RTL, which asserts push/pop for any access to the register regardless of
 * which byte lanes are active.
 *
 * All other registers have functional bits only in [31:0]. A low-word access is
 * zero-extended and forwarded; an access to the upper half touches reserved
 * bits only, so a read returns zero and a write is dropped, both with a normal
 * response as the RTL byte-lane logic would give.
 */
struct MailboxBridge : public sc_core::sc_module {

    tlm_utils::simple_target_socket<MailboxBridge>    tsock;
    tlm_utils::simple_initiator_socket<MailboxBridge> isock;

    static constexpr uint64_t WRITE_DATA_OFFSET = 0x00;
    static constexpr uint64_t READ_DATA_OFFSET  = 0x08;

    /// @brief Address span of one mailbox register block (axil_mailbox_*_wrap.rdl)
    static constexpr uint64_t MAILBOX_BLOCK_SIZE = 0x800;

    SC_HAS_PROCESS(MailboxBridge);

    MailboxBridge(sc_core::sc_module_name n)
        : sc_module(n), tsock("tsock"), isock("isock")
    {
        tsock.register_b_transport(this, &MailboxBridge::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& txn, sc_core::sc_time& delay)
    {
        const uint64_t addr  = txn.get_address();
        const unsigned len   = txn.get_data_length();
        uint8_t*       dptr  = txn.get_data_ptr();

        if (len == 8) {
            isock->b_transport(txn, delay);
            return;
        }

        assert(len == 4 && "MailboxBridge: only 4-byte or 8-byte accesses supported");

        const uint64_t reg_base = addr & ~static_cast<uint64_t>(0x7);
        const bool     is_high  = (addr & 0x4) != 0;

        // Register offset within the addressed channel/port block, so the FIFO
        // registers are recognised on every channel rather than only channel 0.
        const uint64_t reg_off  = (addr % MAILBOX_BLOCK_SIZE) & ~static_cast<uint64_t>(0x7);

        if (txn.is_write()) {
            uint32_t word = 0;
            std::memcpy(&word, dptr, 4);

            if (reg_off == WRITE_DATA_OFFSET) {
                // One push per bus transaction, with the unaddressed half zeroed.
                // axi_lite_mailbox.sv does exactly this --
                //   mbox_w_data_o[i*8+:8] = w.strb[i] ? w.data[i*8+:8] : '0
                // -- so a narrowed 32-bit store enqueues a single zero-extended
                // entry rather than half of one. Pairing the halves into one entry
                // instead (as this bridge used to) makes a lone 32-bit write
                // vanish: nothing is enqueued and no write-threshold interrupt is
                // ever raised, which is what firmware writing a single word sees.
                const uint64_t value = is_high
                    ? (static_cast<uint64_t>(word) << 32)
                    :  static_cast<uint64_t>(word);
                forward_write64(reg_base, value, txn, delay);
            } else {
                if (is_high) {
                    // Upper half of a non-FIFO register is reserved: dropped.
                    txn.set_response_status(tlm::TLM_OK_RESPONSE);
                    return;
                }
                forward_write64(reg_base, static_cast<uint64_t>(word), txn, delay);
            }

        } else {
            if (reg_off == READ_DATA_OFFSET) {
                // One pop per bus transaction, mirroring the write side and
                // axi_lite_mailbox.sv, which asserts mbox_r_pop_o for every read
                // of this address and returns the whole entry for the fabric to
                // narrow. Latching the upper half to serve a following +4 read
                // would leave the reader's pop count out of step with the
                // writer's push count as soon as either side used a lone 32-bit
                // access.
                uint64_t val64 = 0;
                forward_read64(reg_base, val64, txn, delay);
                const uint32_t part = static_cast<uint32_t>(
                    is_high ? (val64 >> 32) : (val64 & 0xFFFFFFFFULL));
                std::memcpy(dptr, &part, 4);
            } else {
                if (is_high) {
                    // Upper half of a non-FIFO register is reserved: reads zero.
                    uint32_t zeroes = 0;
                    std::memcpy(dptr, &zeroes, 4);
                    txn.set_response_status(tlm::TLM_OK_RESPONSE);
                    return;
                }
                uint64_t val64 = 0;
                forward_read64(reg_base, val64, txn, delay);
                const uint32_t lo = static_cast<uint32_t>(val64 & 0xFFFFFFFFULL);
                std::memcpy(dptr, &lo, 4);
            }
        }
    }

private:
    void forward_write64(uint64_t addr, uint64_t value,
                         tlm::tlm_generic_payload& orig, sc_core::sc_time& delay)
    {
        uint8_t buf[8];
        std::memcpy(buf, &value, 8);
        tlm::tlm_generic_payload fwd;
        fwd.set_command(tlm::TLM_WRITE_COMMAND);
        fwd.set_address(addr);
        fwd.set_data_ptr(buf);
        fwd.set_data_length(8);
        fwd.set_streaming_width(8);
        fwd.set_byte_enable_ptr(nullptr);
        fwd.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        isock->b_transport(fwd, delay);
        orig.set_response_status(fwd.get_response_status());
    }

    void forward_read64(uint64_t addr, uint64_t& value,
                        tlm::tlm_generic_payload& orig, sc_core::sc_time& delay)
    {
        uint8_t buf[8] = {};
        tlm::tlm_generic_payload fwd;
        fwd.set_command(tlm::TLM_READ_COMMAND);
        fwd.set_address(addr);
        fwd.set_data_ptr(buf);
        fwd.set_data_length(8);
        fwd.set_streaming_width(8);
        fwd.set_byte_enable_ptr(nullptr);
        fwd.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        isock->b_transport(fwd, delay);
        std::memcpy(&value, buf, 8);
        orig.set_response_status(fwd.get_response_status());
    }
};

/**
 * @brief Fixed local-alias remapper (mirrors hw/ip/local_alias_remap/rtl/axi_local_alias_remap.sv).
 *
 * Reads sep_cpu_ctrl's SEP_LOCAL_BASE_ADDR / SEP_REGION_SIZE registers at
 * transport time (not just at elaboration), so firmware writes to those CSRs
 * take effect immediately on the next transaction — matching how the RTL wires
 * these registers directly into the fixed remapper's runtime inputs.
 *
 * The SimpleBus port bound to this adapter's `tgt` socket already subtracted
 * its static PortMapping base (`static_bus_offset`) before delivery; this
 * adapter adds that back to recover the true global address, applies the
 * live local_base/region_size window check, then forwards the corrected
 * local address on `ini` (back into local_alias_remap_ip's programmable
 * 16-region table, which is a separate hardware block).
 */
/**
 * @brief Local-alias window remap on the DMA's egress path
 *
 * Models u_dma_local_alias_remap in sep_dma_wrap.sv, an axi_window_remap
 * instance sitting between the DMA's AXI master and the local crossbar:
 *
 *   local_alias_base_i = sep_local_base_addr   (cpu_ctrl.SEP_LOCAL_BASE_ADDR)
 *   region_size_i      = SEP_LOCAL_ALIAS_REGION_SIZE = 0x3000_0000
 *   target_base_i      = SEP_LOCAL_ALIAS_REGION_BASE = 0x1000_0000
 *
 * so an address inside the window becomes
 * `addr - local_base + 0x1000_0000`, and anything outside passes through
 * untouched. At the reset base of 0xD000_0000 that is a flat -0xC000_0000,
 * which is why the CPU TCMs at 0xC000_xxxx sit below the window and stay
 * directly reachable — the RTL comment above the instance calls this out as
 * deliberate.
 *
 * Unlike local_alias_remap_adapter, which serves the CPU path (same
 * target_base = 0x1000_0000, but a programmable region size and port-local
 * coordinates), this one is on an initiator and works in global bus addresses
 * with the fixed region constants the RTL hard-wires.
 */
class dma_alias_remap_adapter : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<dma_alias_remap_adapter>    tgt;
    tlm_utils::simple_initiator_socket<dma_alias_remap_adapter> ini;

    static constexpr uint64_t REGION_SIZE = 0x30000000ULL;
    static constexpr uint64_t TARGET_BASE = 0x10000000ULL;

    SC_HAS_PROCESS(dma_alias_remap_adapter);

    dma_alias_remap_adapter(sc_core::sc_module_name n, sep_cpu_ctrl_ip* cpu_ctrl)
        : sc_module(n), tgt("tgt"), ini("ini"), cpu_ctrl_(cpu_ctrl)
    {
        tgt.register_b_transport(this, &dma_alias_remap_adapter::b_transport);
        tgt.register_transport_dbg(this, &dma_alias_remap_adapter::transport_dbg);
    }

private:
    sep_cpu_ctrl_ip* cpu_ctrl_;

    uint64_t remap(uint64_t addr) const {
        const uint64_t local_base =
            static_cast<uint64_t>(cpu_ctrl_->SEP_LOCAL_BASE_ADDR);

        if (addr >= local_base && addr < local_base + REGION_SIZE)
            return addr - local_base + TARGET_BASE;

        return addr;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        const uint64_t orig = trans.get_address();
        trans.set_address(remap(orig));
        ini->b_transport(trans, delay);
        trans.set_address(orig);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        const uint64_t orig = trans.get_address();
        trans.set_address(remap(orig));
        unsigned int ret = ini->transport_dbg(trans);
        trans.set_address(orig);
        return ret;
    }
};

class local_alias_remap_adapter : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<local_alias_remap_adapter>    tgt;
    tlm_utils::simple_initiator_socket<local_alias_remap_adapter> ini;

    SC_HAS_PROCESS(local_alias_remap_adapter);

    local_alias_remap_adapter(sc_core::sc_module_name n,
                               sep_cpu_ctrl_ip* cpu_ctrl,
                               uint64_t static_bus_offset)
        : sc_module(n), tgt("tgt"), ini("ini")
        , cpu_ctrl_(cpu_ctrl), static_bus_offset_(static_bus_offset)
    {
        tgt.register_b_transport(this, &local_alias_remap_adapter::b_transport);
        tgt.register_transport_dbg(this, &local_alias_remap_adapter::transport_dbg);
    }

private:
    sep_cpu_ctrl_ip* cpu_ctrl_;
    uint64_t         static_bus_offset_;

    uint64_t remap(uint64_t addr) const {
        const uint64_t local_base  = static_cast<uint64_t>(cpu_ctrl_->SEP_LOCAL_BASE_ADDR);
        const uint64_t region_size = static_cast<uint64_t>(
            static_cast<uint32_t>(cpu_ctrl_->SEP_REGION_SIZE));

        // SimpleBus b_transport subtracts static_bus_offset_ (0xC000_0000) so
        // `addr` is port-local; some ISS store paths deliver the leftover
        // global address instead. Discriminate: every local coordinate on this
        // 1 GiB port is < 0xC000_0000, every global address in the port is not.
        const uint64_t global_addr =
            (addr >= static_bus_offset_) ? addr : addr + static_bus_offset_;

        // target_base is SEP_LOCAL_ALIAS_REGION_BASE (0x1000_0000), matching
        // axi_local_alias_remap / dma_alias_remap_adapter. A reset
        // SEP_LOCAL_BASE_ADDR of 0xD000_0000 therefore maps 0xD080_2000 →
        // 0x1080_2000. target_base = 0 was the pre-#3711 formula and sent
        // those accesses to 0x0080_2000, which nothing answers.
        static constexpr uint64_t TARGET_BASE = 0x10000000ULL;
        if (global_addr >= local_base && global_addr < local_base + region_size)
            return global_addr - local_base + TARGET_BASE;

        // Outside the currently-configured dynamic window (e.g. before firmware
        // widens SEP_REGION_SIZE past its 16 MiB reset default): fall back to
        // the VP's static default mapping instead of returning a global address
        // unchanged. This PortMapping covers the whole 0xC0000000-0xFFFFFFFF
        // range, so an unchanged address would re-enter this same adapter and
        // recurse forever.
        return (addr >= static_bus_offset_) ? addr - static_bus_offset_ : addr;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        const uint64_t orig = trans.get_address();
        trans.set_address(remap(orig));
        ini->b_transport(trans, delay);
        trans.set_address(orig);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        const uint64_t orig = trans.get_address();
        trans.set_address(remap(orig));
        unsigned int ret = ini->transport_dbg(trans);
        trans.set_address(orig);
        return ret;
    }
};

/**
 * @brief Outbound filter merge point (mirrors sep_system_peripherals.sv's
 * u_outbound_filter_mux, an axi_mux instance with THREE slave inputs:
 * sep_ap_remapped_axi_req, sep_stee_remapped_axi_req, and the raw
 * SEP_EXT_TO_SMU leg straight off u_axi_demux — the SMU leg carries no
 * address remap of its own, it merges in unmodified).
 *
 * RTL chains {AP remap, STEE remap, raw SMU} -> axi_mux -> outbound filter
 * (BlockByDefault=1), so the filter's permission check runs on all three
 * post-merge. This merges ap_output_remap's and stee_output_remap's
 * remapped_socket outputs, plus the SMU window's raw forwarded traffic, onto
 * the single downstream socket feeding sep_filter_ctrl_ip's data_socket.
 *
 * axi_mux itself is a generic external AXI library component (deps/axi/), not
 * SEP-specific — arbitration policy is irrelevant at this LT abstraction level
 * since SystemC's single-threaded execution already serializes any concurrent
 * b_transport calls from the three upstream sources.
 */
class outbound_filter_mux : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<outbound_filter_mux>    ap_tgt;
    tlm_utils::simple_target_socket<outbound_filter_mux>    stee_tgt;
    tlm_utils::simple_target_socket<outbound_filter_mux>    smu_tgt;
    tlm_utils::simple_initiator_socket<outbound_filter_mux> ini;

    SC_HAS_PROCESS(outbound_filter_mux);

    // SimpleBus subtracts SMU_GLOBAL_BASE_ADDR before delivery. RTL's
    // SEP_EXT_TO_SMU demux leg carries the full global address into the
    // outbound filter, so the SMU input re-adds the live CSR base.
    std::function<uint64_t()> smu_window_base_fn;

    outbound_filter_mux(sc_core::sc_module_name n)
        : sc_module(n), ap_tgt("ap_tgt"), stee_tgt("stee_tgt"), smu_tgt("smu_tgt"), ini("ini")
    {
        ap_tgt.register_b_transport(this, &outbound_filter_mux::b_transport);
        ap_tgt.register_transport_dbg(this, &outbound_filter_mux::transport_dbg);
        stee_tgt.register_b_transport(this, &outbound_filter_mux::b_transport);
        stee_tgt.register_transport_dbg(this, &outbound_filter_mux::transport_dbg);
        smu_tgt.register_b_transport(this, &outbound_filter_mux::b_transport_smu);
        smu_tgt.register_transport_dbg(this, &outbound_filter_mux::transport_dbg_smu);
    }

private:
    uint64_t smu_base() const {
        return smu_window_base_fn ? smu_window_base_fn() : 0x80000000ULL;
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        ini->b_transport(trans, delay);
    }
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        return ini->transport_dbg(trans);
    }

    void b_transport_smu(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        const uint64_t local = trans.get_address();
        trans.set_address(smu_base() + local);
        ini->b_transport(trans, delay);
        trans.set_address(local);
    }
    unsigned int transport_dbg_smu(tlm::tlm_generic_payload& trans) {
        const uint64_t local = trans.get_address();
        trans.set_address(smu_base() + local);
        const unsigned n = ini->transport_dbg(trans);
        trans.set_address(local);
        return n;
    }
};

/**
 * @brief SMN inbound global->local remapper (mirrors
 * u_inbound_global_to_local_addr_remap, an axi_local_alias_remap instance,
 * in sep_system_peripherals.sv).
 *
 * This is the SEP-boundary-crossing counterpart to local_alias_remap_adapter:
 * that one translates the CPU's local-alias addresses within SEP; this one
 * translates a GLOBAL address (as seen by an external master like SMC/AP)
 * into a local SEP address, downstream of inbound_filter.
 *
 *   local_alias_base_i -> SEP_GLOBAL_BASE_ADDR (a different register from the
 *                          CPU path's SEP_LOCAL_BASE_ADDR)
 *   region_size_i       -> SEP_REGION_SIZE (the SAME shared register the
 *                          CPU-path adapter also reads)
 *   target_base_i       -> hardwired '0 in RTL (not register-backed)
 *
 * Unlike local_alias_remap_adapter, passthrough-on-no-match is safe here
 * without a static-offset fallback: this sits on a dedicated external-facing
 * socket (smn_inbound_socket) and re-enters the SEP bus as a NEW initiator,
 * not the same target port, so there is no risk of re-decode recursion.
 */
class smn_inbound_remap_adapter : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<smn_inbound_remap_adapter>    tgt;
    tlm_utils::simple_initiator_socket<smn_inbound_remap_adapter> ini;

    SC_HAS_PROCESS(smn_inbound_remap_adapter);

    smn_inbound_remap_adapter(sc_core::sc_module_name n, sep_cpu_ctrl_ip* cpu_ctrl)
        : sc_module(n), tgt("tgt"), ini("ini"), cpu_ctrl_(cpu_ctrl)
    {
        tgt.register_b_transport(this, &smn_inbound_remap_adapter::b_transport);
        tgt.register_transport_dbg(this, &smn_inbound_remap_adapter::transport_dbg);
        default_ext_.source_id = sep::SMC_SOURCE_ID;
    }

private:
    sep_cpu_ctrl_ip* cpu_ctrl_;

    uint64_t remap(uint64_t addr) const {
        const uint64_t global_base  = static_cast<uint64_t>(cpu_ctrl_->SEP_GLOBAL_BASE_ADDR);
        const uint64_t region_size  = static_cast<uint64_t>(
            static_cast<uint32_t>(cpu_ctrl_->SEP_REGION_SIZE));

        if (addr >= global_base && addr < global_base + region_size)
            return addr - global_base;   // target_base = 0
        return addr;                     // outside window: passthrough
    }

    // Inbound traffic carries the source ID its originator stamped, so an
    // extension already on the payload is left alone. One is supplied only when
    // absent: without it the inbound filter would fall back to
    // sep_axi_extension's SEP_SOURCE_ID default and treat external traffic as a
    // trusted local master. SMC is the only master on this port in this
    // platform, hence SMC_SOURCE_ID.
    sep::sep_axi_extension default_ext_;

    void stamp_default_source(tlm::tlm_generic_payload& trans) {
        if (trans.get_extension<sep::sep_axi_extension>()) return;
        trans.set_extension(&default_ext_);
    }

    void unstamp_default_source(tlm::tlm_generic_payload& trans) {
        if (trans.get_extension<sep::sep_axi_extension>() == &default_ext_)
            trans.clear_extension<sep::sep_axi_extension>();
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        const uint64_t orig = trans.get_address();
        stamp_default_source(trans);
        trans.set_address(remap(orig));
        ini->b_transport(trans, delay);
        trans.set_address(orig);
        unstamp_default_source(trans);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans) {
        const uint64_t orig = trans.get_address();
        stamp_default_source(trans);
        trans.set_address(remap(orig));
        unsigned int ret = ini->transport_dbg(trans);
        trans.set_address(orig);
        unstamp_default_source(trans);
        return ret;
    }
};
