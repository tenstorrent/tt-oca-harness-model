// SPDX-License-Identifier: Apache-2.0
/**
 * @file reset_unit.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC Reset Unit.
 *
 * This module is a **loosely-timed, transaction-level model** of the
 * System Management Controller (SMC) Reset Unit instantiated in the OCA
 * hardware platform.  It is intended for software bring-up, integration
 * testing, and early firmware development — not for micro-architectural
 * timing analysis or clock-domain-crossing (CDC) verification.
 *
 * The Reset Unit is the central reset controller of the SMC: it sequences
 * the chip-level cold / primary / core / WDT resets, distributes per-clock-
 * domain synchronised resets, drives the 32 per-subsystem reset-control
 * bundles, implements the PCIe Function-Level-Reset (FLR) "cool reset" flow,
 * and exposes a 32-bit AXI-Lite register block to firmware.
 *
 * ---
 * ## Authoritative references (tt-oca-hw)
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/smc/smc_reset_unit/data/registers/rdl/reset_unit.rdl`      | **Ground-truth register map** |
 * | `hw/smc/smc_reset_unit/data/registers/rtl/reset_unit_reg_pkg.sv` | Register sizes / offsets (PeakRDL) |
 * | `hw/smc/smc_reset_unit/rtl/smc_reset_unit.sv`                  | Top-level structural wrapper |
 * | `hw/smc/smc_reset_unit/rtl/smc_reset_ctrl.sv`                  | Cold/primary/core/WDT reset derivation |
 * | `hw/smc/smc_reset_unit/rtl/smc_subsystem_resets.sv`           | Per-subsystem reset-control + lock logic |
 * | `hw/smc/smc_reset_unit/rtl/smc_cool_reset_wrap.sv`            | FLR cool-reset counters + isolate-request logic |
 * | `hw/smc/smc_reset_unit/rtl/smc_reset_sync.sv`                 | Per-clock-domain reset synchronisers |
 * | `hw/smc/smc_reset_unit/rtl/smc_reset_unit_pkg.sv`            | `reset_ctrl_t` bundle definition |
 * | `hw/smc/smc_pkg.sv`                                           | `jtag_smc_reset_ctrl_t` override struct |
 * | `hw/smc/data/registers/rdl/smc_top.rdl`                       | Top-level map (`smc_reset_unit @ BASE_ADDR + 0x000_2000`) |
 *
 * ---
 * ## Register map (from reset_unit.rdl, offsets relative to the IP base)
 *
 * All registers are 32-bit; the block decodes an 8-bit address window
 * (`RESET_UNIT_REG_MIN_ADDR_WIDTH = 8`, `RESET_UNIT_REG_SIZE = 0xCC`).
 *
 * ```
 * Offset  Name                                 SW   Reset        Description
 * ───────────────────────────────────────────────────────────────────────────
 * 0x20    SS_CONFIG                            rw   0x00000000   Per-SS config (locked by SS_CONFIG_LOCK)
 * 0x24    SS_CONFIG_LOCK                       rw   0x00000000   woset: write-1-to-set sticky lock
 * 0x40    SS_COLD_RESET_N                      rw   0x00000000   Per-SS active-low cold reset (locked by SS_COLD_RESET_LOCK)
 * 0x44    SS_WARM_RESET_N                      rw   0xFFFFFFFF   Per-SS active-low warm reset
 * 0x48    SS_CONFIG_HOLD                       rw   0x00000000   Preserve boot config on warm reset
 * 0x4C    SS_SRAM_HOLD                         rw   0x00000000   Preserve SRAM/MBIST state on warm reset
 * 0x50    SS_CRITICAL_HOLD                     rw   0x00000000   Preserve security/RAS/debug on warm reset
 * 0x54    SS_DEBUG_HOLD                        rw   0x00000000   Preserve debug regs on warm reset
 * 0x60    SS_RESET_COMPLETE                    r    0x00000000   Per-SS reset-complete status (HW driven)
 * 0x70    SS_COLD_RESET_LOCK                   rw   0x00000000   woset: write-1-to-set sticky lock
 * 0x80    SS_FORCE_TO_REF_CLK                  rw   0x00000000   Force SS clock to ref clk during reset
 * 0x90    STRAPS_LO                            r    (HW)         Captured straps[31:0]
 * 0x94    STRAPS_HI                            r    (HW)         Captured straps[63:32]
 * 0xA8    SYNC_REG                             rw   0x00000000   bit0: global sync IRQ
 * 0xB0    ISOLATE_REQ_REG                      rw   0x00000000   SW isolate request (per-SS)
 * 0xB4    ISOLATE_REQ_PINEN_REG                rw   0x00000000   Enable external pin → isolate
 * 0xB8    ISOLATE_REQ_SMC_REG                  rw   0x00000000   bit0: HW-set on FLR, SW-cleared
 * 0xBC    ISOLATE_REQ_SMCEN_REG                rw   0x00000000   Enable FLR lock → isolate
 * 0xC0    ISOLATE_REQ_VIS                      r    (HW)         bit0=isolate_req_pin, bit4=cool_n_in, bit8=cool_n_out
 * 0xC4    ISOLATE_REQ_FLR_COUNTER_VALUE        rw   0x00000000   Cycles after FLR before cool reset asserts
 * 0xC8    ISOLATE_REQ_FLR_RESET_COUNTER_VALUE  rw   0x00000000   Cool-reset assertion duration (0 ⇒ no FLR)
 * ```
 *
 * Holes inside the window are RAZ/WI; accesses ≥ `WINDOW_SIZE` →
 * `TLM_ADDRESS_ERROR_RESPONSE`.
 *
 * ---
 * ## Reset derivation (functional abstraction of smc_reset_ctrl.sv)
 *
 * The RTL deglitches cold reset over 32 ref-clk cycles, extends it for 255
 * cycles after de-assertion, and stretches powergood by 32 cycles.  These
 * are **timing details irrelevant to functional/firmware modelling** and are
 * abstracted to zero latency in the LT model (documented in the low-level
 * design as a deliberate simplification).  The combinational relationships
 * are reproduced exactly:
 *
 * ```
 * powergood_stable       = powergood_i                         (stretch abstracted)
 * fuse_reset_n           = jtag.fuse_ovrd  ? jtag.fuse_val  : fuse_reset_ni
 * cold_reset_n           = jtag.cold_ovrd  ? jtag.cold_val  : rst_cold_ni
 * cool_from_flr_n        = jtag.cool_ovrd  ? jtag.cool_val  : flr_cool_n   (FLR-generated)
 * stable_cold_rst_n      = powergood_stable & cold_reset_n    (deglitch/extend abstracted)
 * stable_cool_rst_n      = rst_cool_ni                        (cool-from-pin, deglitch abstracted)
 * wdt_reset_n            = rst_ext_wdt_ni & ~smc_wdt_second_timeout_i
 * rst_primary_n          = stable_cold_rst_n & stable_cool_rst_n & cool_from_flr_n
 * rst_core_n             = wdt_reset_n & rst_primary_n & fuse_reset_n
 * rst_core_int_n         = jtag.core_ovrd  ? jtag.core_val  : rst_core_n
 * rst_wdt_n              = wdt_reset_n
 * ```
 *
 * The per-clock-domain synchronisers (`smc_reset_sync.sv`) are modelled as
 * zero-delay pass-through, so e.g. `rst_primary_smc_clk_no == rst_primary_n`.
 *
 * ---
 * ## FLR "cool reset" flow (functional abstraction of smc_cool_reset_wrap.sv)
 *
 * On a rising edge of `cfg_flr_pf_active_i` the model:
 *  1. Sets `ISOLATE_REQ_SMC_REG.bit0` (HW set; software clears it by writing).
 *  2. If `ISOLATE_REQ_FLR_RESET_COUNTER_VALUE != 0`, schedules the cool-reset
 *     pulse on `rst_cool_no` (active-low): asserted (0) after
 *     `FLR_COUNTER_VALUE × ref_clk_period_ns`, then released (1) after a
 *     further `FLR_RESET_COUNTER_VALUE × ref_clk_period_ns`.
 *     A zero reset-counter value suppresses the flow entirely (matches RTL).
 *
 * The FLR counter registers live in the **cold-reset** domain and therefore
 * retain their values across a cool reset (the LT model honours this).
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name               | Type     | Default | Mutability | Purpose |
 * |--------------------|----------|---------|------------|---------|
 * | `num_subsystems`   | unsigned | 32      | immutable  | Sizes `ss_reset_ctrl_o[]` (1..32; reg fields are 32-bit). |
 * | `ref_clk_period_ns`| double   | 10.0    | immutable  | Reference-clock period used for FLR counter timing. |
 * | `access_delay_ns`  | double   | 2.0     | mutable    | TLM `b_transport` annotated delay (AXI-Lite latency). |
 *
 * Override before construction via the CCI broker:
 * @code
 *   broker.set_preset_cci_value("…reset_unit.ref_clk_period_ns", cci::cci_value(10.0));
 *   broker.set_preset_cci_value("…reset_unit.access_delay_ns",   cci::cci_value(2.0));
 * @endcode
 */

#ifndef SMC_RESET_UNIT_H_
#define SMC_RESET_UNIT_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <cci_configuration>

#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// reset_ctrl_t — per-subsystem reset-control bundle
// ---------------------------------------------------------------------------

/**
 * @brief Per-subsystem reset-control bundle (mirror of
 *        `smc_reset_unit_pkg::reset_ctrl_t`).
 *
 * Driven on every element of `reset_unit::ss_reset_ctrl_o`.  All reset members
 * are active-low.  Provides the `operator==`, stream insertion, and `sc_trace`
 * overloads required to use it as the data type of an `sc_signal`.
 */
struct reset_ctrl_t {
    bool force_to_ref_clk_n   = false; ///< Force subsystem clock to ref clk (active-low).
    bool cold_reset_n         = false; ///< Per-subsystem cold reset (active-low).
    bool warm_reset_n         = true;  ///< Per-subsystem warm reset (active-low). Reset value 1.
    bool config_state_hold    = false; ///< Preserve boot config across warm reset.
    bool critical_signal_hold = false; ///< Preserve security/RAS/debug across warm reset.
    bool sram_hold            = false; ///< Preserve SRAM/MBIST state across warm reset.
    bool debug_hold           = false; ///< Preserve debug registers across warm reset.

    bool operator==(const reset_ctrl_t& o) const {
        return force_to_ref_clk_n   == o.force_to_ref_clk_n &&
               cold_reset_n         == o.cold_reset_n       &&
               warm_reset_n         == o.warm_reset_n       &&
               config_state_hold    == o.config_state_hold  &&
               critical_signal_hold == o.critical_signal_hold &&
               sram_hold            == o.sram_hold          &&
               debug_hold           == o.debug_hold;
    }
    bool operator!=(const reset_ctrl_t& o) const { return !(*this == o); }
};

/// Stream insertion (required by `sc_signal<reset_ctrl_t>`).
inline std::ostream& operator<<(std::ostream& os, const reset_ctrl_t& r)
{
    os << "{cold_n=" << r.cold_reset_n << " warm_n=" << r.warm_reset_n
       << " cfg_hold=" << r.config_state_hold << " crit_hold=" << r.critical_signal_hold
       << " sram_hold=" << r.sram_hold << " dbg_hold=" << r.debug_hold
       << " force_ref_n=" << r.force_to_ref_clk_n << "}";
    return os;
}

/// SystemC tracing hook (required for `sc_signal<reset_ctrl_t>`; traces fields).
inline void sc_trace(sc_core::sc_trace_file* tf, const reset_ctrl_t& r,
                     const std::string& nm)
{
    sc_core::sc_trace(tf, r.cold_reset_n,         nm + ".cold_reset_n");
    sc_core::sc_trace(tf, r.warm_reset_n,         nm + ".warm_reset_n");
    sc_core::sc_trace(tf, r.config_state_hold,    nm + ".config_state_hold");
    sc_core::sc_trace(tf, r.critical_signal_hold, nm + ".critical_signal_hold");
    sc_core::sc_trace(tf, r.sram_hold,            nm + ".sram_hold");
    sc_core::sc_trace(tf, r.debug_hold,           nm + ".debug_hold");
    sc_core::sc_trace(tf, r.force_to_ref_clk_n,   nm + ".force_to_ref_clk_n");
}

// ---------------------------------------------------------------------------
// jtag_reset_ctrl — JTAG override bundle
// ---------------------------------------------------------------------------

/**
 * @brief JTAG reset-override bundle (mirror of `smc_pkg::jtag_smc_reset_ctrl_t`).
 *
 * Each `*_ovrd` flag, when set, forces the corresponding reset to its `*_val`
 * value, bypassing the functional derivation.  Per-subsystem cold/warm
 * overrides are 32-bit masks (bit i selects subsystem i).  Apply with
 * `reset_unit::set_jtag_ctrl()`.
 */
struct jtag_reset_ctrl {
    // Override-enable flags.
    bool     fuse_reset_n_ovrd  = false;
    bool     cold_reset_n_ovrd  = false;
    bool     cool_reset_n_ovrd  = false;
    bool     core_reset_n_ovrd  = false;
    uint32_t ss_cold_reset_n_ovrd = 0;
    uint32_t ss_warm_reset_n_ovrd = 0;

    // Override values (active-low resets default de-asserted = 1).
    bool     fuse_reset_n_val = true;
    bool     cold_reset_n_val = true;
    bool     cool_reset_n_val = true;
    bool     core_reset_n_val = true;
    uint32_t ss_cold_reset_n_val = 0;
    uint32_t ss_warm_reset_n_val = 0;
};

// ---------------------------------------------------------------------------
// reset_unit_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Compile-time defaults and address-map constants for the SMC Reset Unit.
 *
 * `num_subsystems`, `ref_clk_period_ns`, and `access_delay_ns` may be
 * overridden via CCI presets.  The register offsets are fixed by
 * `reset_unit.rdl` and must not be changed.
 */
struct reset_unit_cfg {
    /// Number of subsystem reset-control bundles driven on `ss_reset_ctrl_o`.
    /// Valid 1..32 (the register fields are 32-bit).  Default 32 matches RTL.
    unsigned num_subsystems = 32;

    /// Reference-clock period (ns) used to translate the FLR counter register
    /// values into simulated time.  Default 10 ns ⇒ 100 MHz ref clock.
    double ref_clk_period_ns = 10.0;

    /// TLM `b_transport` annotated delay (ns) — approximates AXI-Lite latency.
    double access_delay_ns = 2.0;

    // ------------------------------------------------------------------
    // Register byte offsets (from reset_unit.rdl).
    // ------------------------------------------------------------------
    static constexpr uint64_t SS_CONFIG                           = 0x20;
    static constexpr uint64_t SS_CONFIG_LOCK                      = 0x24;
    static constexpr uint64_t SS_COLD_RESET_N                     = 0x40;
    static constexpr uint64_t SS_WARM_RESET_N                     = 0x44;
    static constexpr uint64_t SS_CONFIG_HOLD                      = 0x48;
    static constexpr uint64_t SS_SRAM_HOLD                        = 0x4C;
    static constexpr uint64_t SS_CRITICAL_HOLD                    = 0x50;
    static constexpr uint64_t SS_DEBUG_HOLD                       = 0x54;
    static constexpr uint64_t SS_RESET_COMPLETE                   = 0x60;
    static constexpr uint64_t SS_COLD_RESET_LOCK                  = 0x70;
    static constexpr uint64_t SS_FORCE_TO_REF_CLK                 = 0x80;
    static constexpr uint64_t STRAPS_LO                           = 0x90;
    static constexpr uint64_t STRAPS_HI                           = 0x94;
    static constexpr uint64_t SYNC_REG                            = 0xA8;
    static constexpr uint64_t ISOLATE_REQ_REG                     = 0xB0;
    static constexpr uint64_t ISOLATE_REQ_PINEN_REG               = 0xB4;
    static constexpr uint64_t ISOLATE_REQ_SMC_REG                 = 0xB8;
    static constexpr uint64_t ISOLATE_REQ_SMCEN_REG               = 0xBC;
    static constexpr uint64_t ISOLATE_REQ_VIS                     = 0xC0;
    static constexpr uint64_t ISOLATE_REQ_FLR_COUNTER_VALUE       = 0xC4;
    static constexpr uint64_t ISOLATE_REQ_FLR_RESET_COUNTER_VALUE = 0xC8;

    /// Decoded address window (8-bit address ⇒ 256 B; RESET_UNIT_REG_SIZE=0xCC).
    static constexpr uint64_t WINDOW_SIZE = 0x100;

    /// Per-IP base address inside the SMC fabric (informational; the model
    /// exposes offsets relative to its own window).  smc_top.rdl:
    /// `smc_reset_unit @ 0xC000_0000 + 0x000_2000`.
    static constexpr uint64_t SMC_BASE_ADDR = 0xC000'2000ULL;

    /// Hard upper bound on subsystems (fixed by the 32-bit register fields).
    static constexpr unsigned MAX_SUBSYSTEMS = 32;
};

// ---------------------------------------------------------------------------
// reset_unit
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the SMC Reset Unit.  CCI-parameterised.
 *
 * ### Ports
 *
 * | Port                          | Dir | Width | Description |
 * |-------------------------------|-----|-------|-------------|
 * | `reg_socket`                  | tgt | —     | AXI-Lite-style TLM-2.0 target socket (32-bit register access). |
 * | `powergood_i`                 | in  | 1b    | Power-good from pad (active-high). |
 * | `rst_cold_ni`                 | in  | 1b    | Cold reset from pad (active-low). |
 * | `fuse_reset_ni`               | in  | 1b    | Fuse-sensing-done reset (active-low). |
 * | `rst_ext_wdt_ni`              | in  | 1b    | External watchdog reset (active-low). |
 * | `smc_wdt_first_timeout_i`     | in  | 1b    | SMC WDT first timeout (informational; not in reset path). |
 * | `smc_wdt_second_timeout_i`    | in  | 1b    | SMC WDT second timeout (forces core/WDT reset). |
 * | `rst_cool_ni`                 | in  | 1b    | Incoming cool reset from primary chiplet (active-low). |
 * | `isolate_req_pin_i`           | in  | 1b    | External isolate-request pin. |
 * | `cfg_flr_pf_active_i`         | in  | 1b    | PCIe FLR active (rising edge starts cool-reset flow). |
 * | `ss_reset_complete_i`         | in  | 32b   | Per-subsystem reset-complete status. |
 * | `captured_straps_i`           | in  | 64b   | Captured GPIO straps (visible via STRAPS_LO/HI). |
 * | `powergood_stable_o`          | out | 1b    | Stable (stretched) power-good. |
 * | `rst_cold_stable_ref_clk_no`  | out | 1b    | Stable cold reset, ref-clk domain. |
 * | `rst_cold_stable_smc_clk_no`  | out | 1b    | Stable cold reset, SMC-clk domain. |
 * | `rst_primary_ref_clk_no`      | out | 1b    | Primary reset, ref-clk domain. |
 * | `rst_primary_smc_clk_no`      | out | 1b    | Primary reset, SMC-clk domain (also the register-block reset). |
 * | `rst_primary_periph_clk_no`   | out | 1b    | Primary reset, peripheral-clk domain. |
 * | `rst_core_smc_clk_no`         | out | 1b    | Core reset, SMC-clk domain. |
 * | `rst_wdt_smc_clk_no`          | out | 1b    | WDT reset, SMC-clk domain. |
 * | `rst_cool_no`                 | out | 1b    | Outgoing cool reset to secondary chiplets (active-low). |
 * | `skip_mem_repair_o`           | out | 1b    | Skip memory repair / MBIST during FLR. |
 * | `sync_irq_o`                  | out | 1b    | Global sync IRQ (SYNC_REG.bit0). |
 * | `isolate_req_o`               | out | 32b   | Per-subsystem isolate request. |
 * | `ss_config_o`                 | out | 32b   | Subsystem configuration register value. |
 * | `ss_reset_ctrl_o[i]`          | out | bundle| Per-subsystem reset-control bundle (@ref reset_ctrl_t). |
 *
 * ### Internal SC_METHOD processes
 *
 * | Method                 | Sensitivity         | Purpose |
 * |------------------------|---------------------|---------|
 * | `input_method`         | all input ports     | Recompute reset levels, clear register groups on reset-assert edges, detect FLR edge. |
 * | `flr_assert_method`    | `flr_assert_event_` | Drive the cool-reset pulse low. |
 * | `flr_deassert_method`  | `flr_deassert_event_`| Release the cool-reset pulse. |
 * | `output_method`        | `recompute_event_`  | **Sole driver** of every output signal. |
 *
 * `output_method` is the only process that writes the output signals,
 * satisfying SystemC's single-driver rule (same pattern as PLIC / CLINT).
 * All other code paths mutate internal state and call `schedule_recompute()`.
 *
 * ### Register reset domains (faithful to RTL)
 *
 * - **Primary domain** (`rst_primary_smc_clk_no` low) clears the register
 *   block + subsystem registers (SS_CONFIG, SS_*_HOLD, locks, SYNC, …;
 *   SS_WARM_RESET_N → 0xFFFFFFFF).
 * - **Cold domain** (`rst_cold_stable_smc_clk_no` low) clears the FLR /
 *   isolate registers (ISOLATE_REQ_*, FLR counters).  These therefore survive
 *   a cool/FLR reset, which only asserts the primary domain.
 *   `ISOLATE_REQ_PINEN_REG` is cleared by cold reset only when the isolate
 *   pin is low (matches the RTL's qualified reset).
 *
 * ### TLM-2.0 interface notes
 * - Only 32-bit, naturally-aligned accesses are accepted; others →
 *   `TLM_BURST_ERROR_RESPONSE`.
 * - Accesses ≥ `WINDOW_SIZE` → `TLM_ADDRESS_ERROR_RESPONSE`.
 * - Byte-enables are rejected (`TLM_BYTE_ENABLE_ERROR_RESPONSE`).
 * - DMI is never granted (register reads/writes have side effects).
 * - `transport_dbg` provides side-effect-free back-door register access.
 */
class reset_unit : public sc_core::sc_module {
protected:
    // ------------------------------------------------------------------
    // CCI configuration parameters (declared before the public ports so
    // they are constructed first — ss_reset_ctrl_o is sized from
    // num_subsystems_p_, which may carry a broker preset).
    // ------------------------------------------------------------------

    /// Number of subsystem reset bundles (1..32).  Immutable.
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_subsystems_p_;

    /// Reference-clock period (ns) for FLR counter timing.  Immutable.
    cci::cci_param<double, cci::CCI_IMMUTABLE_PARAM> ref_clk_period_ns_p_;

    /// TLM register-access annotated delay (ns).  Mutable.
    cci::cci_param<double> access_delay_ns_p_;

public:
    SC_HAS_PROCESS(reset_unit);

    /// AXI-Lite-style TLM-2.0 target socket (32-bit register access).
    tlm_utils::simple_target_socket<reset_unit> reg_socket;

    // ---- Inputs ----------------------------------------------------------
    sc_core::sc_in<bool>     powergood_i;
    sc_core::sc_in<bool>     rst_cold_ni;
    sc_core::sc_in<bool>     fuse_reset_ni;
    sc_core::sc_in<bool>     rst_ext_wdt_ni;
    sc_core::sc_in<bool>     smc_wdt_first_timeout_i;
    sc_core::sc_in<bool>     smc_wdt_second_timeout_i;
    sc_core::sc_in<bool>     rst_cool_ni;
    sc_core::sc_in<bool>     isolate_req_pin_i;
    sc_core::sc_in<bool>     cfg_flr_pf_active_i;
    sc_core::sc_in<uint32_t> ss_reset_complete_i;
    sc_core::sc_in<uint64_t> captured_straps_i;

    // ---- Outputs ---------------------------------------------------------
    sc_core::sc_out<bool>     powergood_stable_o;
    sc_core::sc_out<bool>     rst_cold_stable_ref_clk_no;
    sc_core::sc_out<bool>     rst_cold_stable_smc_clk_no;
    sc_core::sc_out<bool>     rst_primary_ref_clk_no;
    sc_core::sc_out<bool>     rst_primary_smc_clk_no;
    sc_core::sc_out<bool>     rst_primary_periph_clk_no;
    sc_core::sc_out<bool>     rst_core_smc_clk_no;
    sc_core::sc_out<bool>     rst_wdt_smc_clk_no;
    sc_core::sc_out<bool>     rst_cool_no;
    sc_core::sc_out<bool>     skip_mem_repair_o;
    sc_core::sc_out<bool>     sync_irq_o;
    sc_core::sc_out<uint32_t> isolate_req_o;
    sc_core::sc_out<uint32_t> ss_config_o;

    /// Per-subsystem reset-control bundles (sized from `num_subsystems`).
    sc_core::sc_vector<sc_core::sc_out<reset_ctrl_t>> ss_reset_ctrl_o;

    /**
     * @brief Construct the Reset Unit module.
     * @param name SystemC module name.
     * @param cfg  Sizing / timing defaults.  CCI presets take priority.
     */
    explicit reset_unit(sc_core::sc_module_name name,
                        reset_unit_cfg cfg = reset_unit_cfg{});

    // ------------------------------------------------------------------
    // Configuration / debug API
    // ------------------------------------------------------------------

    /// Apply a JTAG reset-override bundle and recompute outputs.
    void set_jtag_ctrl(const jtag_reset_ctrl& j);

    /// Current JTAG override bundle (back-door).
    const jtag_reset_ctrl& jtag_ctrl() const { return jtag_; }

    /// Back-door read of a register by byte offset (no side effects, no delay).
    uint32_t dbg_read(uint64_t off) const;

    /// Back-door view of the reset-control bundle for subsystem @p i.
    reset_ctrl_t dbg_ss_reset_ctrl(unsigned i) const;

    /// Number of subsystems (CCI-resolved value).
    unsigned num_subsystems() const { return cfg_.num_subsystems; }

    /// Print a human-readable snapshot of the register file + derived resets.
    void dump_state(std::ostream& os = std::cout) const;

private:
    // ------------------------------------------------------------------
    // TLM-2.0 callbacks
    // ------------------------------------------------------------------
    void         b_transport (tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

    // ------------------------------------------------------------------
    // SC_METHOD processes
    // ------------------------------------------------------------------
    void input_method();
    void flr_assert_method();
    void flr_deassert_method();
    void output_method();
    void schedule_recompute();

    /// SystemC lifecycle hook: seed reset-edge tracking + drive initial outputs.
    void start_of_simulation() override;

    // ------------------------------------------------------------------
    // Register decode helpers
    // ------------------------------------------------------------------
    bool reg_read (uint64_t off, uint32_t& data) const;
    bool reg_write(uint64_t off, uint32_t data);

    // ------------------------------------------------------------------
    // Reset-derivation helpers
    // ------------------------------------------------------------------

    /// Resolved (post-JTAG-override, post-abstraction) reset levels.
    struct derived_t {
        bool powergood_stable;
        bool stable_cold_rst_n;  ///< also the cold-domain (SMC/ref) reset level
        bool rst_primary_n;      ///< also the primary-domain (SMC/ref/periph) level
        bool rst_core_int_n;
        bool rst_wdt_n;
    };

    /// Combinationally derive all reset levels from the current input ports,
    /// JTAG overrides, and FLR cool-reset state.
    derived_t derive() const;

    /// Recompute reset levels, clear register groups on reset-assert edges,
    /// and request an output recompute.  Called from `input_method`, the FLR
    /// methods, and `set_jtag_ctrl`.
    void process_state_change();

    /// Clear the primary-domain register group (subsystem + register block).
    void clear_primary_regs();

    /// Clear the cold-domain register group (FLR / isolate registers).
    void clear_cold_regs(bool isolate_pin);

    /// Start the FLR cool-reset pulse sequence (called on cfg_flr rising edge).
    void flr_kick();

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------
    reset_unit_cfg  cfg_;       ///< Resolved configuration (from CCI).
    jtag_reset_ctrl jtag_;      ///< Current JTAG override bundle.

    // Register storage (primary-domain reset group).
    uint32_t ss_config_           = 0;
    uint32_t ss_config_lock_      = 0;
    uint32_t ss_cold_reset_n_     = 0;
    uint32_t ss_warm_reset_n_     = 0xFFFFFFFFu;
    uint32_t ss_config_hold_      = 0;
    uint32_t ss_sram_hold_        = 0;
    uint32_t ss_critical_hold_    = 0;
    uint32_t ss_debug_hold_       = 0;
    uint32_t ss_cold_reset_lock_  = 0;
    uint32_t ss_force_to_ref_clk_n_ = 0;
    uint32_t sync_reg_            = 0; ///< Only bit0 implemented.

    // Register storage (cold-domain reset group).
    uint32_t isolate_req_reg_       = 0;
    uint32_t isolate_req_pinen_reg_ = 0;
    uint32_t isolate_req_smc_reg_   = 0; ///< Only bit0 implemented.
    uint32_t isolate_req_smcen_reg_ = 0;
    uint32_t flr_counter_value_       = 0;
    uint32_t flr_reset_counter_value_ = 0;

    /// Synced copy of `ss_reset_complete_i` (read via SS_RESET_COMPLETE).
    uint32_t ss_reset_complete_ = 0;

    /// FLR-generated cool reset (active-low); 1 = de-asserted.  Drives
    /// `rst_cool_no` and feeds `rst_primary_n`.
    bool flr_cool_n_ = true;

    // Reset-assert edge tracking (previous derived levels).
    bool prev_primary_n_ = true;
    bool prev_cold_n_    = true;
    bool prev_flr_active_ = false;

    sc_core::sc_event recompute_event_;     ///< Triggers `output_method`.
    sc_core::sc_event flr_assert_event_;    ///< Triggers cool-reset assertion.
    sc_core::sc_event flr_deassert_event_;  ///< Triggers cool-reset release.

    sc_core::sc_time access_delay_;         ///< Cached `access_delay_ns_p_`.
    sc_core::sc_time ref_clk_period_;       ///< Cached `ref_clk_period_ns_p_`.

    // Output idempotence caches (avoid spurious value-changed events).
    std::vector<reset_ctrl_t> ss_ctrl_cache_;
};

} // namespace smc

#endif // SMC_RESET_UNIT_H_
