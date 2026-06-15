// SPDX-License-Identifier: Apache-2.0
/**
 * @file clint.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC CLINT.
 *
 * This module is a **loosely-timed, transaction-level model** of the
 * RISC-V Core-Local Interruptor (CLINT) as instantiated in the SMC chip.
 * It is intended for software bring-up, integration testing, and early
 * firmware development — not for micro-architectural timing analysis.
 *
 * ---
 * ## Authoritative references
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/smc/smc_cpu/data/registers/rdl/clint.rdl` | **Ground-truth register map** |
 * | `hw/smc/smc_cpu/chipyard_generated_files/{1,4}core/OCAH{1,4}CORECluster_CLINT.sv` | Functional RTL reference |
 * | `fw/smc/common/drivers/riscv_clint0.c` | Firmware driver; expected access patterns |
 * | RISC-V Privileged Architecture, machine-mode timer (`mtime`/`mtimecmp`) and software interrupt (`msip`) | Protocol semantics |
 * | `tt-oca-hw.pdf §6.6.6` "RISC-V CLINT and Precise System Management Timing" | SMC-specific role and address window |
 * | `01_PLIC_Specification.md`, `02_PLIC_LowLevel_Design.md`           | Sister IP — same SMC modelling conventions |
 *
 * ---
 * ## Register map (from clint.rdl, offsets relative to CLINT base)
 *
 * ```
 * Offset       Size       SW    Description
 * ──────────────────────────────────────────────────────────────────────────
 * 0x0000       4B × N    RW    MSIP[h]   — Machine Software Interrupt Pending
 *                              bit[0] = 1 raises msip_o[h]; bits[31:1] RAZ/WI.
 *                              Layout: MSIP[h] at 0x0 + 4*h.
 *
 * 0x4000       8B × N    RW    MTIMECMP[h] — 64-bit per-hart timer comparator.
 *                              mtip_o[h] = (MTIME >= MTIMECMP[h]).
 *                              Layout: MTIMECMP[h] at 0x4000 + 8*h.
 *
 * 0xBFF8       8B        RW    MTIME — global 64-bit free-running counter.
 *                              Increments by 1 every `tick_period_ns`
 *                              ns of simulation time.
 *
 * Window size 0x10000 (64 KiB).  Holes RAZ/WI; out-of-window → bus error.
 * ```
 *
 * Both MTIME and MTIMECMP[h] may be accessed as a **single 64-bit access**
 * (data_length = 8) **or** as two consecutive 32-bit accesses (low at the
 * register's base offset, high at +4).  The firmware driver
 * (`riscv_clint0.c`) uses the 32-bit form because RV32 hosts cannot issue
 * 64-bit MMIO; the model therefore must support both.
 *
 * ---
 * ## Interrupt outputs (RISC-V mtime/msip semantics)
 *
 * For each hart `h ∈ [0, num_harts)`:
 *
 * - `msip_o[h]` is asserted iff `MSIP[h].bit[0] == 1`.  It feeds the hart's
 *   `mip.MSIP` CSR directly (bypassing the PLIC).  Software clears it by
 *   writing 0 to MSIP[h]; software raises it (typically targeting another
 *   hart for an inter-processor interrupt) by writing 1.
 *
 * - `mtip_o[h]` is asserted iff `MTIME >= MTIMECMP[h]`.  It feeds the
 *   hart's `mip.MTIP` CSR.  Software clears it by writing a new MTIMECMP
 *   value strictly greater than the current MTIME.
 *
 * The CLINT never claims, never queues, and never multiplexes; both lines
 * are pure level-sensitive functions of the register file and free-running
 * MTIME counter.
 *
 * ---
 * ## Compliance notes (cross-checked against clint.rdl, RTL, firmware)
 *
 * - **MSIP width**: bit[0] is the IPI; bits[31:1] are reserved (RAZ/WI),
 *   matching `MSIP.value[0:0]` and `MSIP.rsvd0[31:1]` in the RDL.
 * - **MTIME / MTIMECMP width**: 64 bits, `regwidth = accesswidth = 64`
 *   in the RDL.  The model rejects writes that touch MTIMECMP/MTIME with
 *   `data_length` other than 4 or 8.
 * - **MTIP comparator**: `time_0 >= pad` in the Chipyard RTL
 *   (`OCAH1CORECluster_CLINT.sv:174`), reproduced verbatim here.
 * - **MSIP wiring**: `auto_int_out_h_0 = ipi_h` in the Chipyard 4-core
 *   RTL — one MSIP and one MTIP per hart.
 * - **Reset value**: MTIME = 0, MSIP[h] = 0 for all h
 *   (matches RTL `time_0 <= 64'h0`, `ipi_X <= 1'h0`).
 *   MTIMECMP[h] reset is **not specified by the RTL** (`pad` register has
 *   no reset); this model defaults MTIMECMP[h] = 0xFFFF_FFFF_FFFF_FFFF so
 *   that MTIP[h] is **deasserted** out of reset.  Firmware is expected to
 *   program MTIMECMP[h] to a meaningful value before unmasking MTIP.
 * - **Tick rate**: configurable via the `tick_period_ns` CCI parameter
 *   (default 100 ns ⇒ 10 MHz).  Setting `tick_period_ns = 0` disables
 *   automatic ticking (MTIME may still be advanced via debug writes — useful
 *   for unit tests that control time deterministically).
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name              | Type     | Default        | Mutability | Purpose |
 * |-------------------|----------|----------------|------------|---------|
 * | `num_harts`       | unsigned | 4              | immutable  | Per-hart MSIP/MTIMECMP/output count. |
 * | `tick_period_ns`  | double   | 100.0          | immutable  | MTIME tick period; 0 disables auto tick. |
 * | `access_delay_ns` | double   | 2.0            | mutable    | TLM `b_transport` annotated delay. |
 *
 * Override before construction via the CCI broker:
 *
 * @code
 *   broker.set_preset_cci_value("…clint.num_harts",       cci::cci_value(2u));
 *   broker.set_preset_cci_value("…clint.tick_period_ns",  cci::cci_value(1000.0));
 *   broker.set_preset_cci_value("…clint.access_delay_ns", cci::cci_value(5.0));
 * @endcode
 */

#ifndef SMC_CLINT_H_
#define SMC_CLINT_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <iostream>
#include <vector>

#include <cci_configuration>

#include "smc_tlm_extensions.h"

namespace smc {

// ---------------------------------------------------------------------------
// clint_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Compile-time defaults and address-map constants for the SMC CLINT.
 *
 * Address offsets are taken directly from
 * `hw/smc/smc_cpu/data/registers/rdl/clint.rdl`.  `num_harts` and
 * `tick_period_ns` may be overridden via CCI presets; the address-map
 * constants are fixed by the RDL and must not be changed.
 *
 * The default `num_harts = 4` matches the SMC 4-core configuration.
 */
struct clint_cfg {
    /// Number of harts the CLINT services. Valid range: 1..4095.
    /// Determines MSIP[*] count, MTIMECMP[*] count, msip_o[*] / mtip_o[*]
    /// vector sizes.
    unsigned num_harts = 4;     // tt-oca-hw.pdf §6.6: 4-core SMC default

    /// Default MTIME tick period, in nanoseconds.  0 disables auto tick.
    /// Configurable via the `tick_period_ns` CCI parameter.
    double tick_period_ns = 100.0;  // 10 MHz default

    /// Per-IP register-window base address inside the SMC fabric
    /// (0xC800_0000, per tt-oca-hw.pdf §6.6.6).  Informational only — the
    /// model exposes byte offsets relative to its own window.
    static constexpr uint64_t SMC_BASE_ADDR = 0xC800'0000ULL;

    static constexpr uint64_t MSIP_BASE     = 0x0000;   ///< MSIP[h] = MSIP_BASE + 4*h
    static constexpr uint64_t MSIP_STRIDE   = 0x4;
    static constexpr uint64_t MTIMECMP_BASE = 0x4000;   ///< MTIMECMP[h] = MTIMECMP_BASE + 8*h
    static constexpr uint64_t MTIMECMP_STRIDE = 0x8;
    static constexpr uint64_t MTIME_OFFSET  = 0xBFF8;   ///< 64-bit global MTIME
    static constexpr uint64_t WINDOW_SIZE   = 0x10000;  ///< 64 KiB total
};

// ---------------------------------------------------------------------------
// clint
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the SMC CLINT.  CCI-parameterised.
 *
 * ### Ports
 *
 * | Port         | Direction | Width | Description |
 * |--------------|-----------|-------|-------------|
 * | `reg_socket` | target    | —     | TLM-2.0 target socket; AXI4-Lite-style register access |
 * | `msip_o[h]`  | output    | 1b    | Machine software interrupt for hart h |
 * | `mtip_o[h]`  | output    | 1b    | Machine timer interrupt for hart h (level: MTIME ≥ MTIMECMP[h]) |
 * | `rst_n_i`    | input     | 1b    | Active-low synchronous reset |
 *
 * ### Internal SC processes
 *
 * | Process        | Sensitivity              | Purpose |
 * |----------------|--------------------------|---------|
 * | `reset_proc`   | `rst_n_i` value-change   | Zero MTIME / MSIP, set MTIMECMP=max, recompute |
 * | `tick_method`  | `tick_event_` (period)   | Increment MTIME, schedule next tick, recompute |
 * | `output_method`| `recompute_event_`       | **Sole driver** of msip_o[*] and mtip_o[*] |
 *
 * `output_method` is the only process that writes the output sc_signals,
 * satisfying SystemC 3.0's strict single-driver rule (see PLIC for the
 * same pattern).  All other code paths mutate internal state and call
 * `schedule_recompute()`.
 *
 * ### TLM-2.0 interface notes
 *
 * - Accepts both **32-bit** and **64-bit** naturally-aligned accesses.
 *   - MSIP[h] is 4 B only.
 *   - MTIMECMP[h] and MTIME are 8 B at their canonical offset, **and** may
 *     be accessed as two consecutive 32-bit halves (low at base, high at
 *     base+4) — required for the firmware driver pattern in
 *     `riscv_clint0.c`.
 * - Misaligned, undersize, or wrong-width accesses → `TLM_BURST_ERROR_RESPONSE`.
 * - Out-of-window addresses (≥ `WINDOW_SIZE`) → `TLM_ADDRESS_ERROR_RESPONSE`.
 * - A 2 ns annotated delay is added per access (configurable via the
 *   `access_delay_ns` CCI parameter).
 * - DMI is never granted — MTIME observers must go through `b_transport`
 *   so the model can present a coherent 64-bit snapshot.
 * - The optional `smc_axi_extension` is forwarded but not enforced;
 *   access-control filtering belongs to the upstream `axi_filter`.
 * - `transport_dbg` provides side-effect-free reads/writes (in the CLINT
 *   nothing has read side-effects, so dbg and b_transport are symmetric).
 */
class clint : public sc_core::sc_module {
protected:
    // ------------------------------------------------------------------
    // CCI configuration parameters.
    //
    // Declared *before* the public sc_vector ports so that they are
    // initialised first in the member-initialiser list — msip_o / mtip_o
    // are sized using the (possibly preset-overridden) value of num_harts.
    //
    // num_harts and tick_period_ns are immutable after init: changing them
    // mid-simulation would invalidate existing port bindings or break
    // already-scheduled tick events.  access_delay_ns is mutable and re-read
    // on every transaction.
    // ------------------------------------------------------------------

    /// Number of harts (1..4095).  Sizes msip_o[], mtip_o[], MSIP[], MTIMECMP[].
    /// Override before construction via broker.set_preset_cci_value("…clint.num_harts", …).
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_harts_p_;

    /// MTIME auto-tick period in nanoseconds.  Default 100 ns ⇒ 10 MHz.
    /// Set to 0.0 to disable auto-tick (test benches may then drive MTIME via
    /// register writes for full timing control).
    cci::cci_param<double, cci::CCI_IMMUTABLE_PARAM> tick_period_ns_p_;

    /// TLM register-access annotated delay in nanoseconds.  Mutable.
    cci::cci_param<double> access_delay_ns_p_;

public:
    SC_HAS_PROCESS(clint);

    /// TLM-2.0 target socket for register access.
    /// Connect the CPU cluster bus bridge initiator socket here.
    tlm_utils::simple_target_socket<clint> reg_socket;

    /// Machine software interrupt outputs, one per hart.  Active-high.
    /// Sized from num_harts_p_ at construction (picks up any CCI preset).
    sc_core::sc_vector<sc_core::sc_out<bool>> msip_o;

    /// Machine timer interrupt outputs, one per hart.  Active-high; level.
    /// Asserts iff MTIME ≥ MTIMECMP[h].
    sc_core::sc_vector<sc_core::sc_out<bool>> mtip_o;

    /// Active-low synchronous reset.  When asserted (low), all MSIP bits are
    /// cleared, MTIME is zeroed, and MTIMECMP[h] is set to UINT64_MAX so
    /// that mtip_o[h] is deasserted out of reset.
    sc_core::sc_in<bool> rst_n_i;

    /**
     * @brief Construct the CLINT module.
     * @param name  SystemC module name.
     * @param cfg   Sizing and timing defaults.  CCI presets (if any) take
     *              priority over these values.
     */
    explicit clint(sc_core::sc_module_name name, clint_cfg cfg = clint_cfg{});

    // ------------------------------------------------------------------
    // Debug back-door API (const, no side effects)
    // ------------------------------------------------------------------

    /// Read the current MTIME counter (back-door, no side effects).
    uint64_t dbg_mtime() const;

    /// Read MTIMECMP[h] (back-door, no side effects).
    uint64_t dbg_mtimecmp(unsigned hart) const;

    /// Read MSIP[h] (back-door, no side effects). Returns 0 or 1.
    uint32_t dbg_msip(unsigned hart) const;

    /// Compute MTIP[h] = (MTIME ≥ MTIMECMP[h]) without touching state.
    bool dbg_mtip(unsigned hart) const;

    /// Print a human-readable snapshot of CLINT state to @p os.
    void dump_state(std::ostream& os = std::cout) const;

    /**
     * @brief Force MTIME to a specific value (back-door, test-only).
     *
     * Provided so test benches can deterministically advance simulated
     * time to any value (e.g. just below MTIMECMP, then over) without
     * waiting for `tick_period_ns × delta` of wall-clock simulation.
     * Always triggers a recompute so msip_o / mtip_o reflect the change.
     *
     * @param value New 64-bit MTIME value.
     */
    void dbg_set_mtime(uint64_t value);

private:
    // ------------------------------------------------------------------
    // TLM-2.0 callbacks (registered with reg_socket in the constructor)
    // ------------------------------------------------------------------

    void          b_transport (tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int  transport_dbg(tlm::tlm_generic_payload& gp);

    // ------------------------------------------------------------------
    // SC_METHOD processes
    // ------------------------------------------------------------------

    /// Active-low reset handler. Sensitive to `rst_n_i.value_changed_event()`;
    /// acts only on the assertion edge (level low).
    void reset_proc();

    /// Periodic tick: increment MTIME, schedule next tick, recompute outputs.
    /// Driven by `tick_event_` notifications, which are re-armed at the end
    /// of each invocation if `tick_period_ns > 0`.
    void tick_method();

    /// Sole driver of all `msip_o[*]` and `mtip_o[*]` signals.
    /// Sensitive to `recompute_event_`.
    void output_method();

    /// Post `recompute_event_` at SC_ZERO_TIME (next delta cycle).
    void schedule_recompute();

    // ------------------------------------------------------------------
    // Register decode helpers
    // ------------------------------------------------------------------

    /// Decode a register read.  Returns false for out-of-window offsets.
    /// `access_size` is 4 or 8 bytes; on success, `data_lo` carries the low
    /// 32 bits (always populated) and `data_hi` the high 32 bits (only
    /// meaningful when `access_size == 8`).
    bool reg_read(uint64_t off, unsigned access_size,
                  uint32_t& data_lo, uint32_t& data_hi) const;

    /// Decode a register write.  See `reg_read` for parameter conventions.
    bool reg_write(uint64_t off, unsigned access_size,
                   uint32_t data_lo, uint32_t data_hi);

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    clint_cfg cfg_;                   ///< Resolved configuration (from CCI).
    uint64_t  mtime_ = 0;             ///< 64-bit global free-running counter.
    std::vector<uint64_t> mtimecmp_;  ///< Per-hart 64-bit comparator (size = num_harts).
    std::vector<uint8_t>  msip_;      ///< Per-hart 1-bit IPI register (size = num_harts).
    std::vector<bool>     msip_cache_;///< Last-driven msip_o value (for idempotence).
    std::vector<bool>     mtip_cache_;///< Last-driven mtip_o value (for idempotence).

    sc_core::sc_event recompute_event_; ///< Triggers `output_method`.
    sc_core::sc_event tick_event_;      ///< Triggers `tick_method`.

    sc_core::sc_time access_delay_;     ///< Cached value of access_delay_ns_p_.
    sc_core::sc_time tick_period_;      ///< Cached value of tick_period_ns_p_.
};

} // namespace smc

#endif // SMC_CLINT_H_
