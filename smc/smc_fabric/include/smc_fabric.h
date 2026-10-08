// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// include/smc_fabric.h
//
// SystemC TLM-2.0 LT model of the SMC Fabric.
//
// Temporal decoupling: Bucket D (pure target).  No quantum keeper.
// Every b_transport handler adds reg_access_ns to the incoming delay and
// forwards to the appropriate downstream socket.
//
// Routing summary
// ---------------
//  jtag_axi_in / mmio_in / data_accel_in / log_in
//    → alias remap → local/global aperture split
//    → local:    to_local_addr() → route_local()
//    → outbound: route_outbound() → [output remap] → outbound filter → output_axi
//
//  sys_axi_in
//    → inbound filter → to_local_addr() → route_local()
//
//  sep_axi_in
//    → to_local_addr() → route_local()  (always local, no filter)
//
// See doc/implementation.adoc.
// ===========================================================================

#pragma once

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "reg_access.h"
#include <cci_configuration>

#include <array>
#include <cstdint>
#include <limits>

namespace smc {

class smc_axi_extension;

class smc_fabric : public sc_core::sc_module
{
public:
    // -----------------------------------------------------------------------
    // Construction parameters (see doc/implementation.adoc)
    // -----------------------------------------------------------------------
    struct config {
        uint64_t local_base_addr      = 0xC000'0000ULL;
        uint64_t global_base_addr     = 0x4000'0000ULL;
        uint64_t region_size          = 0x0100'0000ULL;  // live alias-aperture size

        bool     no_addr_remap        = false;  // smc_config_pkg::NO_ADDR_REMAP (=0 → remap on)

        unsigned num_inbound_filters  = 16;     // informational; table always 16-deep
        unsigned num_outbound_filters = 16;
        unsigned max_write_txns       = 4;      // LT: informational only
        unsigned max_read_txns        = 4;

        double   reg_access_ns        = 1.0;    // ns added to delay per CSR access
        double   hang_det_cycle_ns    = 1.0;    // LT detector cycle duration
    };

    cci::cci_param<double> hang_det_cycle_ns_p_;

    // -----------------------------------------------------------------------
    // Target sockets — inbound masters → fabric
    // -----------------------------------------------------------------------
    tlm_utils::simple_target_socket<smc_fabric, 64> jtag_axi_in   {"jtag_axi_in"};
    tlm_utils::simple_target_socket<smc_fabric, 64> mmio_in       {"mmio_in"};
    // data_accel_in fans in >1 upstream master (DMA engine + memory_zeroer's
    // DMA-write-back master both target this port on real platforms — see
    // doc/implementation.adoc), so it is a multi-bind socket; every other
    // inbound-master socket here has exactly one upstream master and stays a
    // plain single-bind simple_target_socket.
    tlm_utils::multi_passthrough_target_socket<smc_fabric, 64> data_accel_in {"data_accel_in"};
    tlm_utils::simple_target_socket<smc_fabric, 64> log_in        {"log_in"};
    tlm_utils::simple_target_socket<smc_fabric, 64> sys_axi_in    {"sys_axi_in"};
    tlm_utils::simple_target_socket<smc_fabric, 64> sep_axi_in    {"sep_axi_in"};

    // -----------------------------------------------------------------------
    // Initiator sockets — local targets
    // -----------------------------------------------------------------------
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_front_port          {"to_front_port"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_data_accel_ctrl     {"to_data_accel_ctrl"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_periph              {"to_periph"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_dfd_apb             {"to_dfd_apb"};

    // -----------------------------------------------------------------------
    // Initiator sockets — internal CSR targets
    //
    // Remap/filter sockets (to_aR_ctrl … to_outbound_filter_ctrl) are handled
    // INTERNALLY by the fabric — writes update the routing tables directly and
    // do not forward to an external module.  The sockets still exist so a
    // testbench can bind stubs and observe traffic if desired.
    //
    // to_cpu_ctrl / to_mailbox / to_dft_csr are forwarded to bound downstream
    // modules in the usual way.
    // -----------------------------------------------------------------------
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_cpu_ctrl            {"to_cpu_ctrl"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_aR_ctrl             {"to_aR_ctrl"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_mR_ctrl             {"to_mR_ctrl"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_xR_ctrl             {"to_xR_ctrl"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_inbound_filter_ctrl {"to_inbound_filter_ctrl"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_outbound_filter_ctrl{"to_outbound_filter_ctrl"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_mailbox             {"to_mailbox"};
    tlm_utils::simple_initiator_socket<smc_fabric, 64> to_dft_csr             {"to_dft_csr"};

    // -----------------------------------------------------------------------
    // System NoC output
    // -----------------------------------------------------------------------
    tlm_utils::simple_initiator_socket<smc_fabric, 64> output_axi             {"output_axi"};

    // -----------------------------------------------------------------------
    // Signals
    // -----------------------------------------------------------------------
    sc_core::sc_in<bool> rst_n_i{"rst_n_i"};
    sc_core::sc_out<bool> axi_hang_irq_o{"axi_hang_irq_o"};

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    SC_HAS_PROCESS(smc_fabric);
    explicit smc_fabric(sc_core::sc_module_name name, const config& cfg);
    explicit smc_fabric(sc_core::sc_module_name name);   // uses config{} defaults

    // -----------------------------------------------------------------------
    // Testbench / debug API
    // -----------------------------------------------------------------------
    uint64_t read_global_base() const { return cfg_.global_base_addr; }
    uint64_t read_region_size() const { return cfg_.region_size;      }
    void     write_global_base(uint64_t v);
    void     write_region_size(uint64_t v);

    // Remap region descriptor (shared by the alias-remap and output-remap
    // tables; each bank uses the subset of fields its RDL defines).
    //
    // Alias remap (alias_remap.rdl, tt-oca-hw #2464), 8 entries × 0x20 stride:
    //   REGION_START @0x00  start_addr[55:12]           -> start (low 12 bits 0)
    //   REGION_END   @0x08  end_addr[55:12], exclusive  -> end   (low 12 bits 0)
    //   REGION_ATTRS @0x10  offset[55:12] | cacheable[59:56] | valid[63]
    //   Translation on a hit: addr[55:12] + offset[55:12] (mod 2^44), low 12
    //   bits preserved; AxCACHE replaced by cacheable[3:0].
    //
    // Output remap (output_remap.rdl, tt-oca-hw #2572), 8 entries × 0x08:
    //   REGION_ATTRS @0x00  offset[55:0] | valid[63]
    //   Translation when valid: addr[55:20] replaced by offset[55:20]; when
    //   clear the address passes through unchanged.  start/end/cacheable
    //   are unused.
    struct alias_region {
        uint64_t start     = 0;    // alias: start_addr[55:12] << 12
        uint64_t end       = 0;    // alias: end_addr[55:12] << 12 (exclusive)
        int64_t  offset    = 0;    // alias: offset[55:12] << 12; output: offset[55:0]
        uint8_t  cacheable = 0;    // alias: 4-bit AxCACHE replacement value
        bool     valid     = false;
    };
    alias_region get_alias_region(unsigned n) const { return alias_regions_.at(n); }

    // Last alias-remap decision (testbench-visible, not an sc_out).
    struct remap_debug_info {
        bool     hit          = false;
        unsigned region_index = 0;
    };
    remap_debug_info get_remap_debug() const { return remap_debug_; }

    // Filter entry — bit-exact image of filter_ctrl.rdl (FILTER_CONFIG /
    // START_ADDR / END_ADDR).  Read-only view for testbench inspection.
    //   FILTER_CONFIG: read_en[0] write_en[1] addr_mode[4] allow_ns[8]
    //                  data_bus_width[14:12](RO) src_id[19:16] group_id[23:20]
    //                  allow_burst[24] locked[63]
    //   START_ADDR:    start_addr[55:0]
    //   END_ADDR:      end_addr[55:0]  (reset 0x7 = min granularity)
    struct filter_entry {
        uint64_t start_addr  = 0;
        uint64_t end_addr    = 0x7;   // inclusive; RDL reset default
        uint8_t  src_id      = 0;     // 0 ⇒ source-ID match ignored
        uint8_t  group_id    = 0;     // group-ID filtering disabled in SMC
        bool     read_en     = false; // range permits reads
        bool     write_en    = false; // range permits writes
        bool     addr_mode   = false; // entry enable (RDL addr_mode)
        bool     allow_ns    = false; // exact-match NS value (tx_ns == allow_ns)
        bool     allow_burst = false; // 4 KiB vs 8 B match granularity
        bool     locked      = false; // write-once lock of this entry
    };
    filter_entry get_inbound_filter_entry (unsigned n) const { return inbound_filter_.at(n);  }
    filter_entry get_outbound_filter_entry(unsigned n) const { return outbound_filter_.at(n); }

private:
    // -----------------------------------------------------------------------
    // b_transport callbacks
    // -----------------------------------------------------------------------
    void bt_jtag      (tlm::tlm_generic_payload&, sc_core::sc_time&);
    void bt_mmio      (tlm::tlm_generic_payload&, sc_core::sc_time&);
    // data_accel_in is a multi_passthrough_target_socket, so its fw-interface
    // callbacks carry a leading bound-initiator index (unused here — every
    // bound master is routed identically by bt_internal()).
    void bt_data_accel(int, tlm::tlm_generic_payload&, sc_core::sc_time&);
    void bt_log       (tlm::tlm_generic_payload&, sc_core::sc_time&);
    void bt_sys_axi   (tlm::tlm_generic_payload&, sc_core::sc_time&);
    void bt_sep_axi   (tlm::tlm_generic_payload&, sc_core::sc_time&);

    // DMI — always denied (fabric never grants DMI).
    bool get_direct_mem_ptr(tlm::tlm_generic_payload&, tlm::tlm_dmi&);
    // Multi-socket overload for data_accel_in (see bt_data_accel).
    bool get_direct_mem_ptr(int, tlm::tlm_generic_payload&, tlm::tlm_dmi&);

    // -----------------------------------------------------------------------
    // Internal-master routing (shared by jtag/mmio/data_accel/log)
    // -----------------------------------------------------------------------
    void bt_internal(tlm::tlm_generic_payload&, sc_core::sc_time&,
                     const char* ingress);

    // -----------------------------------------------------------------------
    // Routing helpers
    // -----------------------------------------------------------------------
    // Alias remap (axi_alias_remap.sv): first valid region containing addr wins;
    // the address is rebased by offset[55:12] and, when `ext` is present, its
    // AxCACHE is replaced by the region's cacheable[3:0].  Miss: passthrough.
    uint64_t apply_alias_remap(uint64_t addr, smc_axi_extension* ext);
    bool     is_local(uint64_t addr) const;
    uint32_t to_local_addr(uint64_t addr) const;

    void route_local   (tlm::tlm_generic_payload&, sc_core::sc_time&);
    void route_outbound(tlm::tlm_generic_payload&, sc_core::sc_time&);

    // Bit-exact image of axi_filter_wrap: lowest-index "hit" (addr_mode &
    // in-range & src-id & NS) wins, then read_en/write_en decides; on no hit
    // the result is !block_by_default.
    bool filter_lookup(const std::array<filter_entry, 16>& table,
                       uint64_t addr, uint8_t src_id, bool ns,
                       bool is_write, bool block_by_default) const;

    bool inbound_filter_allow (uint64_t addr, uint8_t src_id, bool ns,
                               bool is_write) const;
    bool outbound_filter_allow(uint64_t addr, uint8_t src_id, bool ns,
                               bool is_write) const;

    enum class outbound_path { plain, mmode, xvisor };
    outbound_path classify_outbound(uint64_t addr) const;

    // Bit-exact image of output_remap.sv: index = (addr-window_base)[22:20],
    // out = {table[idx].offset[55:20], (addr-window_base)[19:0]}.
    uint64_t apply_output_remap(uint64_t addr,
                                const std::array<alias_region, 8>& table,
                                uint64_t window_start,
                                uint8_t src_id_override,
                                smc_axi_extension* ext) const;

    // Returns the downstream initiator socket for a 32-bit local address.
    // Returns nullptr when no target matches (caller issues deny response).
    tlm_utils::simple_initiator_socket<smc_fabric, 64>*
    local_decode(uint32_t addr32);

    static void fill_deny_response(tlm::tlm_generic_payload&);
    void        invalidate_all_dmi();

    // -----------------------------------------------------------------------
    // Internal CSR handlers
    // Each is called from route_local() when the address falls in the
    // corresponding sub-range. They update internal tables and set the TLM
    // response status directly on `trans`.
    // -----------------------------------------------------------------------

    // SMC base config CSRs (LOCAL_BASE, GLOBAL_BASE, REGION_SIZE).
    // Returns true when the offset matches a fabric-owned register; false
    // means the access falls elsewhere in the smc_base_config window and is
    // treated as RAZ/WI by this LT model.
    bool handle_global_csr(tlm::tlm_generic_payload& trans, uint32_t sub_offset);

    void handle_alias_remap    (tlm::tlm_generic_payload& trans, uint32_t sub_offset);
    void handle_mmode_remap    (tlm::tlm_generic_payload& trans, uint32_t sub_offset);
    void handle_xvisor_remap   (tlm::tlm_generic_payload& trans, uint32_t sub_offset);
    void handle_inbound_filter (tlm::tlm_generic_payload& trans, uint32_t sub_offset);
    void handle_outbound_filter(tlm::tlm_generic_payload& trans, uint32_t sub_offset);

    // Helper for handle_alias_remap: bit-exact alias_remap.rdl image
    // (REGION_START @0x00, REGION_END @0x08, REGION_ATTRS @0x10; 0x18 is
    // RAZ/WI).  64-bit accesses at a register base read/write the whole
    // register; 32-bit accesses see the low word at +0x00 and the high word
    // at +0x04 of each register.
    static void handle_remap_entry(alias_region& r,
                                   uint32_t      field_off,
                                   tlm::tlm_generic_payload& trans);

    // Helper shared by handle_mmode_remap / handle_xvisor_remap.  Each output
    // remap entry is a single 64-bit REGION_ATTRS register: offset[55:0] plus
    // valid[63] (output_remap.rdl, tt-oca-hw #2572).  offset is stored in
    // alias_region::offset and valid in alias_region::valid.
    static void handle_output_remap_entry(alias_region& r,
                                           uint32_t      field_off,
                                           tlm::tlm_generic_payload& trans);

    // Pack / unpack the alias-remap REGION_ATTRS register image.
    static uint64_t alias_attrs_image(const alias_region& r);
    static void     alias_attrs_apply(alias_region& r, uint64_t image);

    // -----------------------------------------------------------------------
    // SC_METHOD: clear all tables on active-low reset
    // -----------------------------------------------------------------------
    void reset_proc();

    enum class hang_leg : unsigned { sys = 0, sep = 1, data_accel = 2 };
    struct hang_state {
        unsigned outstanding_count = 0;
        bool timed_out = false;
        uint32_t threshold_snapshot = 0;
    };
    void hang_begin(hang_leg leg);
    void hang_complete(hang_leg leg);
    void hang_timeout(hang_leg leg);
    void hang_timeout_sys();
    void hang_timeout_sep();
    void hang_timeout_data_accel();
    void update_hang_irq();
    void request_hang_irq_update();
    /// Live HANG_DET_CTRL.irq[12] for one detector. Software reads this to
    /// see which master hung. It is a level, not a latch: high only while
    /// enable, irq_en, and (timeout or irq_test) are all true.
    bool hang_irq_level(hang_leg leg) const;
    regmodel::Register64& hang_ctrl(hang_leg leg);
    regmodel::Register64& hang_threshold(hang_leg leg);

    // -----------------------------------------------------------------------
    // Source-ID constants (AXI USER field) — smc_pkg.sv:277-280
    // -----------------------------------------------------------------------
    static constexpr uint8_t OTHERS_SRC_ID = 0x0;  // Xvisor-remapped traffic
    static constexpr uint8_t SMC_SRC_ID    = 0x3;  // plain outbound path
    static constexpr uint8_t MMODE_SRC_ID  = 0xC;  // M-mode-remapped traffic
    static constexpr uint8_t SEP_SRC_ID    = 0xF;  // SEP inbound (not remapped in LT)

    // -----------------------------------------------------------------------
    // Local crossbar absolute address map (base = LOCAL_BASE = 0xC000_0000)
    // Verbatim from meta/crossbars/smc_local_xbar_pkg.sv and
    // meta/crossbars/smc_internal_axi_lite_xbar_pkg.sv.
    //
    // route_local()/local_decode() compare the masked 32-bit address (upper 7
    // bits forced to local_base[31:25]) against these absolute ranges, exactly
    // as the RTL crossbar does after the input-stage address masking.
    // -----------------------------------------------------------------------
    // front_port (AXI4) — smc_local_xbar_pkg.sv
    static constexpr uint32_t FRONT_WDT_DEBUG_BASE = 0xC000'0000u;
    static constexpr uint32_t FRONT_WDT_DEBUG_END  = 0xC000'1000u;
    static constexpr uint32_t FRONT_CPU_CTRL_BASE  = 0xC003'9000u;
    static constexpr uint32_t FRONT_CPU_CTRL_END   = 0xC003'A000u;
    static constexpr uint32_t FRONT_SPM_BASE       = 0xC004'0000u;  // scratchpad RAM
    static constexpr uint32_t FRONT_SPM_END        = 0xC016'0000u;
    // FRONT_PORT_PLIC_{BASE,SIZE} = 0xC400_0000 / 0x400_0000 and
    // FRONT_PORT_CLINT_BEU_{BASE,SIZE} = 0xC800_0000 / 0x2_0000.
    //
    // Both lie above LOCAL_ALIAS_REGION_SIZE, so internal masters (jtag / mmio /
    // dma / log) cannot reach them through this decode -- matching RTL, where
    // these blocks live inside the cluster on the rocket-chip periphery bus and
    // the harts reach them directly, never via smc_input_fabric.  This decode is
    // the path for external masters (sys / sep) arriving through the front port.
    static constexpr uint32_t FRONT_PLIC_BASE      = 0xC400'0000u;  // 64 MB window
    static constexpr uint32_t FRONT_PLIC_END       = 0xC800'0000u;
    static constexpr uint32_t FRONT_CLINT_BEU_BASE = 0xC800'0000u;  // 128 KB
    static constexpr uint32_t FRONT_CLINT_BEU_END  = 0xC802'0000u;

    // data_accel_ctrl (AXI4) — smc_local_xbar_pkg.sv
    // DATA_ACCEL_CTRL_DMA_ZEROER_{BASE,SIZE} = 0xC003_8000 / 0x1000, so the
    // window ends at 0xC003_9000 and no longer overlaps cpu_ctrl.
    static constexpr uint32_t DACCEL_DMA_ZEROER_BASE = 0xC003'8000u;  // DMA + zeroer ctrl
    static constexpr uint32_t DACCEL_DMA_ZEROER_END  = 0xC003'9000u;

    // periph_reg (AXI4-Lite 32) — smc_local_xbar_pkg.sv
    // PERIPH_REG_PERIPH_MAIN_{BASE,SIZE} = 0xC000_2000 / 0x9800.
    static constexpr uint32_t PERIPH_MAIN_BASE = 0xC000'2000u;
    static constexpr uint32_t PERIPH_MAIN_END  = 0xC000'B800u;

    // VP-ONLY: AOU CSR park (realignment decision D1=A).  The AOU is not part
    // of smc_top, so it has no RTL slot; it is parked in the gap just above
    // dfx_ctrl and routed to the peripheral port.  This window is the single
    // deliberate deviation from smc_local_xbar_pkg.sv — every other range in
    // this file is the RTL value.  Remove it if the AOU ever gains a real slot.
    static constexpr uint32_t AOU_PARK_BASE = 0xC000'C000u;
    static constexpr uint32_t AOU_PARK_END  = 0xC000'D000u;
    static constexpr uint32_t PERIPH_EXT_BASE  = 0xC040'0000u;
    static constexpr uint32_t PERIPH_EXT_END   = 0xC080'0000u;
    // OCA I3C wraps live above the DMA window (smc_top.rdl); routed to
    // to_periph so the existing i3c_controller model can sit there.
    static constexpr uint32_t PERIPH_I3C_BASE  = 0xC003'A000u;
    static constexpr uint32_t PERIPH_I3C_END   = 0xC004'0000u;

    // smc_dfd_reg (APB) — smc_local_xbar_pkg.sv
    static constexpr uint32_t DFD_REGS_BASE = 0xC016'0000u;
    static constexpr uint32_t DFD_REGS_END  = 0xC026'0000u;

    // local_reg → internal AXI-Lite sub-crossbar — smc_internal_axi_lite_xbar_pkg.sv
    // DFX_CSR_DFX_CSR_{BASE,SIZE} = 0xC000_B800 / 0x800 (also LOCAL_REG_DFX_CTRL
    // in smc_local_xbar_pkg.sv).  Decoded ahead of PERIPH_MAIN in route_local().
    static constexpr uint32_t DFT_CSR_BASE   = 0xC000'B800u;
    static constexpr uint32_t DFT_CSR_END    = 0xC000'C000u;
    static constexpr uint32_t SMC_BASE_CONFIG_BASE = 0xC001'0000u;
    static constexpr uint32_t SMC_BASE_CONFIG_END  = 0xC001'2000u;
    static constexpr uint32_t AR_CTRL_BASE   = 0xC001'2000u;  // alias remap CSRs
    static constexpr uint32_t AR_CTRL_END    = 0xC001'3000u;
    static constexpr uint32_t MR_CTRL_BASE   = 0xC001'3000u;  // M-mode remap CSRs
    static constexpr uint32_t MR_CTRL_END    = 0xC001'4000u;
    static constexpr uint32_t XR_CTRL_BASE   = 0xC001'4000u;  // Xvisor remap CSRs
    static constexpr uint32_t XR_CTRL_END    = 0xC001'5000u;
    static constexpr uint32_t IB_FILTER_BASE = 0xC001'5000u;  // inbound filter CSRs
    static constexpr uint32_t IB_FILTER_END  = 0xC001'6000u;
    static constexpr uint32_t OB_FILTER_BASE = 0xC001'6000u;  // outbound filter CSRs
    static constexpr uint32_t OB_FILTER_END  = 0xC001'7000u;
    static constexpr uint32_t MAILBOX_BASE   = 0xC001'8000u;
    static constexpr uint32_t MAILBOX_END    = 0xC003'8000u;

    // smc_base_config.rdl offsets within SMC_BASE_CONFIG_BASE.
    //   GLOBAL_BASE  0xC001_0000 (RW, default 0x4000_0000)
    //   LOCAL_BASE   0xC001_0008 (RO, default 0xC000_0000)
    //   REGION_SIZE  0xC001_0010 (RW, default 0x0100_0000)
    static constexpr uint32_t GCSR_GLOBAL_BASE = 0x00u;
    static constexpr uint32_t GCSR_LOCAL_BASE  = 0x08u;
    static constexpr uint32_t GCSR_REGION_SIZE = 0x10u;
    static constexpr uint32_t GCSR_CLOCK_GATE_CONTROL = 0x18u;
    static constexpr uint32_t GCSR_HANG_SYS_CTRL      = 0x20u;
    static constexpr uint32_t GCSR_HANG_SYS_THRESHOLD = 0x28u;
    static constexpr uint32_t GCSR_HANG_SEP_CTRL      = 0x30u;
    static constexpr uint32_t GCSR_HANG_SEP_THRESHOLD = 0x38u;
    static constexpr uint32_t GCSR_HANG_DATA_CTRL      = 0x40u;
    static constexpr uint32_t GCSR_HANG_DATA_THRESHOLD = 0x48u;

    // M-mode/Xvisor remap windows — smc_pkg.sv:283-286 (relative to local/global base)
    //   MMODE_REGION_MEM_BASE_ADDR  0xC100_0000 → START 0x0100_0000, SIZE 0x0080_0000
    //   XVISOR_REGION_MEM_BASE_ADDR 0xC180_0000 → START 0x0180_0000, SIZE 0x0080_0000
    static constexpr uint64_t MMODE_REMAP_START  = 0x0100'0000ULL;
    static constexpr uint64_t MMODE_REMAP_SIZE   = 0x0080'0000ULL;
    static constexpr uint64_t XVISOR_REMAP_START = 0x0180'0000ULL;
    static constexpr uint64_t XVISOR_REMAP_SIZE  = 0x0080'0000ULL;

    // Output-remap region granularity — smc_pkg.sv:251 / output_remap.sv:19,72-82.
    // IdxStart=20 → 1 MB regions; 8 regions tile each 8 MB window and the region
    // index is taken from adjusted_addr[IdxStart +: $clog2(8)] = bits [22:20].
    // REGION_ATTRS.offset is a 56-bit field (output_remap.rdl / .sv:52-54).
    static constexpr unsigned OUTPUT_REMAP_IDX_START = 20;
    static constexpr uint64_t OUTPUT_REMAP_OFFSET_MASK = 0x00FF'FFFF'FFFF'FFFFULL;
    static constexpr uint64_t OUTPUT_REMAP_VALID       = 1ULL << 63;

    // Alias-remap register fields — alias_remap.rdl / smc_pkg.sv:279-282
    // (NUM_ALIAS_REMAP_REGIONS=8, ALIAS_REMAP_IDX_START=12, 56-bit AXI address).
    static constexpr unsigned ALIAS_REMAP_IDX_START      = 12;
    static constexpr uint64_t ALIAS_ADDR_FIELD_MASK      = 0x00FF'FFFF'FFFF'F000ULL; // [55:12]
    static constexpr uint64_t ALIAS_ATTRS_CACHEABLE_MASK = 0x0F00'0000'0000'0000ULL; // [59:56]
    static constexpr unsigned ALIAS_ATTRS_CACHEABLE_SHIFT = 56;
    static constexpr uint64_t ALIAS_ATTRS_VALID          = 1ULL << 63;
    static constexpr uint64_t ALIAS_ATTRS_RW_MASK        = 0x8FFF'FFFF'FFFF'F000ULL;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    config cfg_;

    regmodel::Register64 clock_gate_control_{
        0x0000'0000'3F00'3FFFULL, 0x0000'0000'3F00'3FFFULL,
        0x0000'0000'1F00'0000ULL};
    regmodel::Register64 hang_sys_ctrl_{0x111ULL, 0x111ULL, 0};
    regmodel::Register64 hang_sys_threshold_{0xF'FFFFULL, 0xF'FFFFULL, 0x1000};
    regmodel::Register64 hang_sep_ctrl_{0x111ULL, 0x111ULL, 0};
    regmodel::Register64 hang_sep_threshold_{0xF'FFFFULL, 0xF'FFFFULL, 0x1000};
    regmodel::Register64 hang_data_accel_ctrl_{0x111ULL, 0x111ULL, 0};
    regmodel::Register64 hang_data_accel_threshold_{0xF'FFFFULL, 0xF'FFFFULL, 0x1000};
    std::array<hang_state, 3> hang_states_{};
    sc_core::sc_event hang_sys_timeout_event_;
    sc_core::sc_event hang_sep_timeout_event_;
    sc_core::sc_event hang_data_accel_timeout_event_;
    sc_core::sc_event hang_irq_update_event_;

    std::array<alias_region, 8> alias_regions_{};   // ALIAS_REMAP[0..7]
    std::array<alias_region, 8> mmode_regions_{};   // MMODE_REMAP[0..7]
    std::array<alias_region, 8> xvisor_regions_{};  // XVISOR_REMAP[0..7]

    std::array<filter_entry, 16> inbound_filter_{};
    std::array<filter_entry, 16> outbound_filter_{};

    remap_debug_info remap_debug_{};
};

}  // namespace smc
