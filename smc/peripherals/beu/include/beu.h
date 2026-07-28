// SPDX-License-Identifier: Apache-2.0
/**
 * @file beu.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC per-core Bus
 *        Error Unit (BEU).
 *
 * This module is a **transaction-level, register-accurate model** of one
 * per-core Bus Error Unit instantiated in the SMC CPU cluster (the
 * Rocket-chip `BusErrorUnit` behind an AXI4-Lite register window; the SMC
 * packs `NUM_CORES` of them at `0xC801_0000 + N*0x1000`).  It is intended for
 * firmware bring-up, driver development, and integration testing — not for
 * micro-architectural timing, TileLink bus modeling, or cache ECC modeling.
 *
 * The BEU is a **passive error recorder + interrupt generator**: cache / bus
 * error sources feed it, it latches the first (highest-priority) enabled error
 * into `CAUSE`/`PHYS_ADDR`, accrues per-source sticky status, and raises two
 * interrupt lines — a **local** (NMI-like, bypasses the PLIC) line and a
 * **PLIC** line — each gated by its own per-source mask.
 *
 * ---
 * ## Authoritative references (tt-oca-hw)
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/smc/smc_cpu/data/registers/rdl/bus_error_unit.rdl` | **Ground-truth** register map |
 * | `hw/smc/smc_cpu/chipyard_generated_files/4core/OCAH4CORECluster_BusErrorUnit.sv` | Behaviour (Rocket BusErrorUnit) |
 * | `hw/smc/doc/interrupts.adoc` | BEU NMI-like local delivery + PLIC path |
 * | `hw/smc/doc/memmap.adoc` | `0xC801_0000 + N*0x1000`, 4 KB window each |
 *
 * ---
 * ## Register map (per core, 4 KiB window; registers in low 0x30; all 64-bit, 8-byte-aligned)
 *
 * ```
 * Offset  Name             SW   Notes
 * ─────────────────────────────────────────────────────────────────────────
 * 0x00    CAUSE            rw   [2:0] cause_enum of latched error; HW records
 *                               first enabled error while CAUSE==0; SW writes 0 to re-arm
 * 0x08    PHYS_ADDR        ro   [55:0] physical address of the latched error (HW-written)
 * 0x10    ENABLE           rw   [7:0] per-source recording enable (reset: all sources on)
 * 0x18    PLIC_ENABLE      rw   [7:0] per-source PLIC (global) interrupt mask
 * 0x20    ACCRUED_ENABLE   rw   [7:0] per-source sticky accrued error status (HW-set, W-clear)
 * 0x28    LOCAL_ENABLE     rw   [7:0] per-source local (NMI-like) interrupt mask
 * ```
 *
 * Only bits {1,2,5,6,7} are defined in every 8-bit field (see @ref beu_src);
 * bits {0,3,4} are reserved and read as zero.  Offsets inside the window with
 * no register decode return `TLM_ADDRESS_ERROR_RESPONSE`.
 *
 * ---
 * ## Functional model
 *
 * The five hardware error sources (ICache bus, ICache correctable, DCache bus,
 * DCache correctable, DCache uncorrectable) are **not** wired as ports (there
 * is no cache in this model).  They are driven through the test-bench back door
 * @ref inject_error, mirroring how `i2c_controller` abstracts its bus behind a
 * bus-model callback.
 *
 * On an injected error `s` (bit index == @ref beu_src == `CAUSE` encoding):
 *  1. `ACCRUED_ENABLE.s` is set unconditionally (raw sticky status).
 *  2. If `ENABLE.s` is set **and** `CAUSE == 0`, latch `CAUSE = s` and
 *     `PHYS_ADDR = addr` (records the first enabled error until SW clears it).
 *  3. Recompute the two interrupt lines.
 *
 * Interrupt outputs (single-driver `recompute_method`):
 *  - `irq_local_o = (ACCRUED_ENABLE & LOCAL_ENABLE) != 0`
 *  - `irq_plic_o  = (ACCRUED_ENABLE & PLIC_ENABLE ) != 0`
 *
 * Software clears an interrupt by writing 0s to the relevant `ACCRUED_ENABLE`
 * bits, and re-arms `CAUSE`/`PHYS_ADDR` recording by writing `CAUSE = 0`.
 *
 * ### Loosely-timed
 * `b_transport` never calls `wait()`; it only adds `access_delay_ns` to the
 * annotated `delay`.  Temporal decoupling (the quantum keeper) is the
 * initiator's responsibility — see the test bench driver.
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name              | Type   | Default | Mutability | Purpose |
 * |-------------------|--------|---------|------------|---------|
 * | `access_delay_ns` | double | 2.0     | mutable    | TLM `b_transport` annotated delay (AXI4-Lite latency). |
 */

#ifndef SMC_BEU_H_
#define SMC_BEU_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <iostream>

#include <cci_configuration>

#include "reg_access.h"
#include "reg_map.h"
#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// Error sources / CAUSE encoding
// ---------------------------------------------------------------------------

/**
 * @brief BEU error source == bit position in the 8-bit fields == `CAUSE` value.
 *
 * Matches `cause_enum` in `bus_error_unit.rdl` and the Rocket BusErrorUnit
 * input ordering.  Values 0 (no error), 3 (ICache uncorrectable) and 4 are
 * reserved / unused by the hardware.
 */
enum class beu_src : uint8_t {
    ICACHE_TLBUS         = 1, ///< ICache TileLink bus error   (itl_error)
    ICACHE_CORRECTABLE   = 2, ///< ICache ECC correctable error (iec_error)
    DCACHE_TLBUS         = 5, ///< DCache TileLink bus error   (dtl_error)
    DCACHE_CORRECTABLE   = 6, ///< DCache ECC correctable error (dec_error)
    DCACHE_UNCORRECTABLE = 7  ///< DCache ECC uncorrectable error (deu_error)
};

// ---------------------------------------------------------------------------
// beu_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Timing defaults and fixed address-map / field constants.
 *
 * The register offsets, field masks, and bit positions are compile-time fixed
 * by `bus_error_unit.rdl`.
 */
struct beu_cfg {
    double access_delay_ns = 2.0; ///< TLM annotated delay (AXI4-Lite latency).

    /// Per-instance address window in bytes.  The SoC allocates 4 KiB per BEU
    /// (`0xC801_0000 + N*0x1000`, see `hw/smc/doc/memmap.adoc`); the six
    /// registers occupy the low `0x30`.  8-byte-aligned offsets in
    /// `[0x30, 0x1000)` decode to no register and return `TLM_ADDRESS_ERROR`.
    static constexpr uint64_t WINDOW_SIZE = 0x1000;
    /// Register access stride in bytes (64-bit registers).
    static constexpr unsigned REG_WIDTH = 8;

    // ---- Register byte offsets (bus_error_unit.rdl) --------------------
    static constexpr uint64_t CAUSE          = 0x00;
    static constexpr uint64_t PHYS_ADDR      = 0x08;
    static constexpr uint64_t ENABLE         = 0x10;
    static constexpr uint64_t PLIC_ENABLE    = 0x18;
    static constexpr uint64_t ACCRUED_ENABLE = 0x20;
    static constexpr uint64_t LOCAL_ENABLE   = 0x28;

    // ---- Field masks ---------------------------------------------------
    /// Defined source bits {1,2,5,6,7} in every 8-bit field.
    static constexpr uint64_t VALID_MASK = (1u << 1) | (1u << 2) |
                                           (1u << 5) | (1u << 6) | (1u << 7);
    /// CAUSE holds a 3-bit enumeration.
    static constexpr uint64_t CAUSE_MASK = 0x7u;
    /// PHYS_ADDR is 56 bits wide.
    static constexpr uint64_t PHYS_MASK  = (uint64_t{1} << 56) - 1;
};

// ---------------------------------------------------------------------------
// beu
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of one SMC per-core Bus Error Unit.
 *
 * ### Ports
 *
 * | Port          | Dir | Description |
 * |---------------|-----|-------------|
 * | `reg_socket`  | tgt | AXI4-Lite-style TLM-2.0 target socket (64-bit access). |
 * | `rst_n_i`     | in  | Active-low asynchronous reset. |
 * | `irq_local_o` | out | Local (NMI-like) interrupt; OR of `ACCRUED & LOCAL_ENABLE`. |
 * | `irq_plic_o`  | out | PLIC (global) interrupt; OR of `ACCRUED & PLIC_ENABLE`. |
 *
 * `recompute_method` is the sole driver of `irq_local_o` / `irq_plic_o`
 * (single-driver discipline, as in the UART / PLIC / I2C models); every
 * state-changing path calls `schedule_recompute()`.
 */
class beu : public sc_core::sc_module {
protected:
    // CCI parameter (declared before ports for construction ordering parity).
    cci::cci_param<double> access_delay_ns_p_;

public:
    SC_HAS_PROCESS(beu);

    /// AXI4-Lite-style TLM-2.0 target socket (64-bit access).
    tlm_utils::simple_target_socket<beu, 64> reg_socket;

    /// Active-low asynchronous reset.
    sc_core::sc_in<bool>  rst_n_i;
    /// Local (NMI-like) interrupt output (active-high).
    sc_core::sc_out<bool> irq_local_o;
    /// PLIC (global) interrupt output (active-high).
    sc_core::sc_out<bool> irq_plic_o;

    /**
     * @brief Construct the BEU model.
     * @param name SystemC module name.
     * @param cfg  Timing defaults.  CCI presets take priority.
     */
    explicit beu(sc_core::sc_module_name name, beu_cfg cfg = beu_cfg{});

    // ------------------------------------------------------------------
    // Test-bench back door
    // ------------------------------------------------------------------

    /**
     * @brief Emulate a hardware error event feeding this BEU.
     *
     * Sets the source's accrued status; if recording is enabled for the source
     * and `CAUSE` is currently clear, latches `CAUSE` and `PHYS_ADDR`.
     * @param src       Error source (see @ref beu_src).
     * @param phys_addr Physical address of the error (low 56 bits are kept).
     */
    void inject_error(beu_src src, uint64_t phys_addr);

    /// Side-effect-free register peek (no state change).
    uint64_t dbg_reg(uint64_t off) const;

    /// Print a human-readable snapshot to @p os.
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
    void reset_proc();       ///< SC_METHOD on rst_n_i: clear all state.
    void recompute_method(); ///< SC_METHOD on recompute_event_: drive IRQ lines.

    void schedule_recompute();

    // ------------------------------------------------------------------
    // Register decode helpers
    // ------------------------------------------------------------------
    bool reg_read (uint64_t off, uint64_t& data);
    bool reg_write(uint64_t off, uint64_t data);

    bool irq_local_active() const;
    bool irq_plic_active()  const;

    // ------------------------------------------------------------------
    // Internal state
    // ------------------------------------------------------------------
    beu_cfg cfg_;

    // Plain masked RW registers owned by the offset->register dispatch table.
    regmodel::Register64 enable_{beu_cfg::VALID_MASK, beu_cfg::VALID_MASK,
                                 beu_cfg::VALID_MASK};   ///< ENABLE (all sources on at reset)
    regmodel::Register64 plic_enable_{beu_cfg::VALID_MASK, beu_cfg::VALID_MASK, 0};
    regmodel::Register64 local_enable_{beu_cfg::VALID_MASK, beu_cfg::VALID_MASK, 0};

    regmodel::RegisterMap64 regmap_; ///< offset -> storage register dispatch.

    // Registers with model-side behaviour (kept out of the map by design).
    uint64_t cause_     = 0; ///< CAUSE[2:0] latched error (0 = none).
    uint64_t phys_addr_ = 0; ///< PHYS_ADDR[55:0] latched error address.
    uint64_t accrued_   = 0; ///< ACCRUED_ENABLE[7:0] sticky per-source status.

    // Output cache to suppress redundant sc_signal writes.
    bool out_irq_local_ = false;
    bool out_irq_plic_  = false;
    bool outputs_valid_ = false;

    sc_core::sc_event recompute_event_; ///< Triggers recompute_method().
};

} // namespace smc

#endif // SMC_BEU_H_
