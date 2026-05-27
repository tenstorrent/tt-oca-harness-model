// SPDX-License-Identifier: Apache-2.0
/**
 * @file plic.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC PLIC.
 *
 * This module is a **cycle-approximate, transaction-level model** of the
 * RISC-V Platform-Level Interrupt Controller (PLIC) as instantiated in the
 * SMC chip.  It is intended for software bring-up, integration testing, and
 * early firmware development — not for micro-architectural timing analysis.
 *
 * ---
 * ## Authoritative references
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/smc/smc_cpu/data/registers/rdl/plic.rdl` | **Ground-truth register map** |
 * | `hw/smc/smc_cpu/chipyard_generated_files/1core/OCAH1CORECluster_TLPLIC.sv` | Functional RTL reference |
 * | `fw/smc/common/drivers/riscv_plic0.c` | Firmware driver; expected access patterns |
 * | RISC-V PLIC Specification v1.0 | Protocol and arbitration semantics |
 * | `01_SMC_Architecture.md §5` | SMC-specific sizing parameters |
 * | `02_SMC_IP_LowLevel_Design.md §4` | TLM-2.0 interface contract |
 *
 * ---
 * ## Register map (from plic.rdl, offsets relative to PLIC base)
 *
 * ```
 * Offset        Size      SW     Description
 * ──────────────────────────────────────────────────────────────────────────
 * 0x000000      4B×337    RW     PRIORITY[0..336]: 3-bit priority per source.
 *                                  Index 0 is reserved (RAZ/WI).
 *                                  Sources are 1..336; priority 0 disables.
 *
 * 0x001000      4B×11     RO     PENDING[0..10]: one bit per source, 32-per-word.
 *                                  Bit j of word w = source (32w+j).
 *                                  Bit 0 of word 0 (source 0) is always 0.
 *
 * 0x002000      4B×11     RW     ENABLE[ctx=0][0..10]  (CORE0_MEIP, ctx stride=0x80)
 * 0x002080      4B×11     RW     ENABLE[ctx=1]         (CORE1_MEIP)
 * 0x002100      4B×11     RW     ENABLE[ctx=2]         (CORE2_MEIP)
 * 0x002180      4B×11     RW     ENABLE[ctx=3]         (CORE3_MEIP)
 * 0x002200      4B×11     RW     ENABLE[ctx=4]         (CORE0_SEIP)
 * 0x002280      4B×11     RW     ENABLE[ctx=5]         (CORE1_SEIP)
 * 0x002300      4B×11     RW     ENABLE[ctx=6]         (CORE2_SEIP)
 * 0x002380      4B×11     RW     ENABLE[ctx=7]         (CORE3_SEIP)
 *
 * 0x200000      4B        RW     THRESHOLD[ctx=0]      (CORE0_MEIP, ctx stride=0x1000)
 * 0x200004      4B        RW     CLAIM/COMPLETE[ctx=0] read→claim, write→complete
 * 0x201000      4B        RW     THRESHOLD[ctx=1]      (CORE1_MEIP)
 * 0x201004      4B        RW     CLAIM/COMPLETE[ctx=1]
 *  ...
 * 0x207000      4B        RW     THRESHOLD[ctx=7]      (CORE3_SEIP)
 * 0x207004      4B        RW     CLAIM/COMPLETE[ctx=7]
 * ```
 *
 * ### Context mapping (all M-mode first, then all S-mode)
 * | ctx | RDL name        | Hart | Privilege |
 * |-----|-----------------|------|-----------|
 * | 0   | CORE0_MEIP      | 0    | M-mode    |
 * | 1   | CORE1_MEIP      | 1    | M-mode    |
 * | 2   | CORE2_MEIP      | 2    | M-mode    |
 * | 3   | CORE3_MEIP      | 3    | M-mode    |
 * | 4   | CORE0_SEIP      | 0    | S-mode    |
 * | 5   | CORE1_SEIP      | 1    | S-mode    |
 * | 6   | CORE2_SEIP      | 2    | S-mode    |
 * | 7   | CORE3_SEIP      | 3    | S-mode    |
 *
 * ---
 * ## Interrupt flow (RISC-V PLIC protocol)
 *
 * 1. A peripheral drives `src_in[i]` high (active-high, level-sensitive).
 *    `src_in[i]` maps to interrupt **source ID = i + 1**.
 * 2. `pending[i+1]` is **latched on the rising edge** of `src_in[i]`.
 *    It remains set until the source is claimed, even if the line drops.
 * 3. For each context `c`, `ctx_out[c]` asserts when there exists at least one
 *    source `s` satisfying all three conditions:
 *    - `pending[s]` is set
 *    - `enable[c][s]` is set
 *    - `priority[s] > threshold[c]`   (strictly greater; priority 0 = disabled)
 * 4. The CPU (firmware) reads `CLAIM/COMPLETE[c]` to **atomically claim** the
 *    highest-priority eligible source: returns its ID, clears `pending[s]`,
 *    and sets an internal `claim_in_flight` latch that prevents the same
 *    source from re-asserting until `complete` is written.
 * 5. After the ISR executes, firmware writes the source ID back to
 *    `CLAIM/COMPLETE[c]` to **complete** the interrupt:
 *    - If `src_in` is still high: `pending` is immediately re-latched.
 *    - If `src_in` has gone low: `pending` is re-armed on the next rising edge.
 *    This guarantees that a device whose line stays asserted does not fire
 *    continuously, while a device that pulses its line does not lose events.
 *
 * ---
 * ## Compliance notes (cross-checked against plic.rdl and RTL)
 *
 * - **Priority width**: 3-bit (field `value[2:0]` in plic.rdl); values 0..7.
 *   Priority 0 permanently disables a source regardless of threshold.
 * - **Threshold width**: 3-bit (field `value[2:0]` in plic.rdl); values 0..7.
 *   Threshold 0 passes all non-zero-priority sources.
 * - **Tie-breaking**: among equal-priority pending sources, the **lowest source
 *   ID wins** (RISC-V PLIC spec §3; confirmed by plic.rdl description field).
 * - **Source 0**: architecturally reserved; PRIORITY reads return 0, writes are
 *   ignored; bit 0 of PENDING word 0 is always 0.
 * - **PENDING register**: software read-only (`SW=r, HW=rw` per plic.rdl).
 *   Software writes are silently discarded.
 * - **CLAIM/COMPLETE**: same physical address, different semantics — reading
 *   claims the top interrupt; writing signals completion (`SW=rw` per plic.rdl).
 * - **DMI**: never granted because CLAIM reads have a side-effect (claim()).
 * - **Source count**: 336 sources (IDs 1–336), matching `PRIORITY[337]` in
 *   plic.rdl and `PLIC0_MAX_INTERRUPTS 336` in `fw/smc/common/drivers/riscv_plic0.c`.
 */

#ifndef SMC_PLIC_H_
#define SMC_PLIC_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <vector>

#include <cci_configuration>

#include "smc_tlm_extensions.h"

namespace smc {

// ---------------------------------------------------------------------------
// plic_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Compile-time sizing and address-map constants for the SMC PLIC.
 *
 * All address offsets are **relative to the PLIC base address** and are
 * taken directly from `hw/smc/smc_cpu/data/registers/rdl/plic.rdl`.
 * Override `num_sources` and `num_contexts` at construction time to model
 * a different SMC variant (e.g. 1-core vs 4-core).
 *
 * The default values match the 4-core SMC configuration:
 * - 336 interrupt sources (plic.rdl: PRIORITY[337]; firmware: PLIC0_MAX_INTERRUPTS 336)
 * - 8 contexts = 4 cores × {M-mode, S-mode}
 */
struct plic_cfg {
    /// Number of interrupt sources.  Valid range: 1..1023.
    /// Source IDs are 1-based; source 0 is architecturally reserved.
    /// Must match the number of entries in PRIORITY[] minus 1 (for source 0).
    unsigned num_sources  = 336; // plic.rdl: PRIORITY[337]; fw: PLIC0_MAX_INTERRUPTS 336

    /// Number of interrupt contexts.  Valid range: >= 1.
    /// One context per hart × privilege-mode combination.
    /// SMC default: 4 cores × {M-mode, S-mode} = 8 contexts.
    unsigned num_contexts = 8;   // plic.rdl: 4×MEIP + 4×SEIP

    // ------------------------------------------------------------------
    // Address map offsets (all in bytes, from PLIC base).
    // Source: hw/smc/smc_cpu/data/registers/rdl/plic.rdl
    // ------------------------------------------------------------------

    /// Base of per-source priority registers.
    /// Layout: PRIORITY[src] at PRIORITY_BASE + 4*src (src=0 reserved).
    static constexpr uint64_t PRIORITY_BASE   = 0x000000;

    /// Base of read-only pending-bit registers.
    /// Layout: 32 sources packed per 32-bit word; word w at PENDING_BASE + 4*w.
    static constexpr uint64_t PENDING_BASE    = 0x001000;

    /// Base of per-context interrupt-enable registers.
    /// Layout: context c, word w at ENABLE_BASE + c*ENABLE_STRIDE + 4*w.
    static constexpr uint64_t ENABLE_BASE     = 0x002000;

    /// Byte stride between consecutive context enable banks (0x80 = 128 B = 32 words).
    /// The stride is fixed by the RISC-V PLIC spec regardless of num_sources.
    static constexpr uint64_t ENABLE_STRIDE   = 0x000080;

    /// Base of per-context threshold and claim/complete registers.
    /// Layout: context c at CONTEXT_BASE + c*CONTEXT_STRIDE.
    static constexpr uint64_t CONTEXT_BASE    = 0x200000;

    /// Byte stride between consecutive context control blocks (0x1000 = 4 KB).
    static constexpr uint64_t CONTEXT_STRIDE  = 0x001000;

    /// Byte offset of THRESHOLD within a context control block.
    static constexpr uint64_t CONTEXT_THR_OFF = 0x000000;

    /// Byte offset of CLAIM/COMPLETE within a context control block.
    /// Reading this address claims the top interrupt; writing completes it.
    static constexpr uint64_t CONTEXT_CC_OFF  = 0x000004;

    /// Total size of the PLIC register window (4 MB).
    /// Accesses at or beyond WINDOW_SIZE return TLM_ADDRESS_ERROR_RESPONSE.
    static constexpr uint64_t WINDOW_SIZE     = 0x400000;
};

// ---------------------------------------------------------------------------
// plic
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the SMC PLIC.
 *
 * ### Ports
 *
 * | Port | Direction | Width | Description |
 * |------|-----------|-------|-------------|
 * | `reg_socket` | target | — | AXI4-Lite-style TLM-2.0 target socket; attach the CPU cluster bus bridge here |
 * | `src_in[i]`  | input  | 1b | Interrupt source i+1 (active-high, level-sensitive) |
 * | `ctx_out[c]` | output | 1b | Interrupt output for context c (active-high) |
 * | `rst_n_i`    | input  | 1b | Synchronous active-low reset (matches other SMC IPs) |
 *
 * ### Internal SC_METHOD processes
 *
 * | Method | Sensitivity | Purpose |
 * |--------|-------------|---------|
 * | `reset_proc`   | `rst_n_i` | Zeroes all registers on assertion (low) |
 * | `src_method`   | `src_in`  | Latches rising-edge events as pending bits |
 * | `output_method`| `recompute_event_` | Sole driver of all `ctx_out` signals |
 *
 * `output_method` is the **only** process that writes to `ctx_out`.  All other
 * code paths update internal state and call `schedule_recompute()`, which
 * fires `recompute_event_` at SC_ZERO_TIME.  This enforces SystemC's
 * single-driver rule while keeping the arbitration logic DRY.
 *
 * ### TLM-2.0 interface notes
 *
 * - Only 32-bit, naturally-aligned accesses are accepted; misaligned or
 *   non-word-size transfers receive `TLM_BURST_ERROR_RESPONSE`.
 * - Accesses outside `plic_cfg::WINDOW_SIZE` receive `TLM_ADDRESS_ERROR_RESPONSE`.
 * - A fixed 2 ns annotated delay is added per access (`access_delay_`).
 * - DMI is never granted because CLAIM reads have a side-effect.
 * - The optional `smc_axi_extension` is forwarded but not acted upon here;
 *   access-control filtering is the responsibility of the upstream `axi_filter`
 *   (`02_SMC_IP_LowLevel_Design.md §2`).
 * - `transport_dbg` provides back-door read/write access with no side effects
 *   (CLAIM/COMPLETE reads return the top-pending source without claiming it).
 */
class plic : public sc_core::sc_module {
protected:
    // ------------------------------------------------------------------
    // CCI configuration parameters.
    //
    // IMPORTANT: these are declared *before* the public sc_vector ports
    // so that they are initialised first in the member-initialiser list.
    // src_in and ctx_out are sized using the values these params carry
    // (which may be preset values injected before module construction).
    //
    // num_sources / num_contexts are immutable after initialisation: the
    // sc_vectors cannot be resized at run-time.  access_delay_ns is
    // mutable; b_transport reads it on every transaction.
    // ------------------------------------------------------------------

    /// Number of interrupt sources (1..1023).
    /// Override before construction via broker.set_preset_cci_value("…plic.num_sources", …).
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_sources_p_;

    /// Number of interrupt contexts (harts × privilege modes).
    /// Override before construction via broker.set_preset_cci_value("…plic.num_contexts", …).
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_contexts_p_;

    /// TLM register-access annotated delay in nanoseconds.
    /// Mutable — can be changed between transactions via a broker handle.
    cci::cci_param<double> access_delay_ns_p_;

public:
    SC_HAS_PROCESS(plic);

    /// TLM-2.0 target socket for register access.
    /// Connect the CPU cluster bus bridge initiator socket here.
    tlm_utils::simple_target_socket<plic> reg_socket;

    /// Interrupt source inputs, one per source (index i → source ID i+1).
    /// Active-high, level-sensitive.  Pending is latched on the rising edge.
    /// Sized from num_sources_p_ at construction (picks up any CCI preset).
    sc_core::sc_vector<sc_core::sc_in<bool>>  src_in;

    /// Interrupt outputs, one per context.  Active-high.
    /// Driven by `output_method`; asserts when at least one enabled,
    /// above-threshold, pending source exists for that context.
    /// Sized from num_contexts_p_ at construction.
    sc_core::sc_vector<sc_core::sc_out<bool>> ctx_out;

    /// Active-low synchronous reset.  When asserted (low), all priority,
    /// pending, enable, and threshold registers are cleared and ctx_out is
    /// deasserted.  Matches the reset polarity of all other SMC IPs.
    sc_core::sc_in<bool> rst_n_i;

    /**
     * @brief Construct the PLIC module.
     * @param name    SystemC module name.
     * @param cfg     Sizing and address-map configuration; defaults to the
     *                4-core SMC configuration (336 sources, 8 contexts).
     */
    explicit plic(sc_core::sc_module_name name, plic_cfg cfg = plic_cfg{});

    // ------------------------------------------------------------------
    // Debug back-door API
    //
    // These functions bypass the TLM socket and read internal state
    // directly.  They are intended for test-bench assertions and debug
    // only (03_SMC_Test_Plan.md §3).  They do NOT affect fabric coverage
    // or filter-path simulation.
    // ------------------------------------------------------------------

    /**
     * @brief Read the current priority of a source (back-door, no side effects).
     * @param src  Source ID (1..num_sources).  Returns 0 for out-of-range.
     * @return     3-bit priority value (0..7).
     */
    uint32_t dbg_priority(unsigned src) const;

    /**
     * @brief Query whether a source is currently pending (back-door).
     * @param src  Source ID (1..num_sources).  Returns false for out-of-range.
     * @return     True if the source's pending bit is set.
     */
    bool dbg_pending(unsigned src) const;

    /**
     * @brief Query whether a source is enabled in a given context (back-door).
     * @param ctx  Context index (0..num_contexts-1).
     * @param src  Source ID (1..num_sources).
     * @return     True if the enable bit for (ctx, src) is set.
     */
    bool dbg_enable(unsigned ctx, unsigned src) const;

    /**
     * @brief Read the interrupt threshold for a context (back-door).
     * @param ctx  Context index (0..num_contexts-1).  Returns 0 out-of-range.
     * @return     3-bit threshold value (0..7).
     */
    uint32_t dbg_threshold(unsigned ctx) const;

    /**
     * @brief Return the source ID of the highest-priority pending interrupt
     *        for a context, without performing a claim (back-door).
     *
     * Equivalent to what a CLAIM read would return, but with no state change.
     * Useful for test-bench assertions before the firmware reads the PLIC.
     *
     * @param ctx  Context index (0..num_contexts-1).  Returns 0 out-of-range.
     * @return     Source ID of the top pending interrupt, or 0 if none.
     */
    uint32_t dbg_claim_top(unsigned ctx) const;

    /**
     * @brief Print a human-readable snapshot of all PLIC state to @p os.
     *
     * Outputs: module name, num_sources/contexts, list of all pending sources
     * with their priorities, and per-context threshold / output / best-pending
     * values.  Intended for post-failure debug dumps in simulation.
     *
     * @param os  Output stream (defaults to std::cout).
     */
    void dump_state(std::ostream& os = std::cout) const;

private:
    // ------------------------------------------------------------------
    // TLM-2.0 callbacks (registered with reg_socket in the constructor)
    // ------------------------------------------------------------------

    /**
     * @brief TLM-2.0 blocking transport — main register-access entry point.
     *
     * Accepts only 32-bit, naturally-aligned accesses.  Returns:
     * - `TLM_BURST_ERROR_RESPONSE`   for misaligned or non-word accesses
     * - `TLM_ADDRESS_ERROR_RESPONSE` for addresses >= WINDOW_SIZE
     * - `TLM_COMMAND_ERROR_RESPONSE` for commands other than READ/WRITE
     * - `TLM_OK_RESPONSE`            on success (annotates `access_delay_`)
     *
     * A read of CLAIM/COMPLETE has the side-effect of claiming the top
     * interrupt (calling `claim()`); see the RISC-V PLIC spec §4.
     */
    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);

    /**
     * @brief TLM-2.0 debug transport — back-door register access, no side effects.
     *
     * Accepts the same address space as `b_transport` but never calls
     * `claim()` on a CLAIM/COMPLETE read.  Returns 4 on success, 0 on error.
     * Used by the test bench to inspect register state without perturbing it.
     */
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

    // ------------------------------------------------------------------
    // SC_METHOD processes
    // ------------------------------------------------------------------

    /**
     * @brief SC_METHOD: active-low reset handler.
     *
     * Sensitive to `rst_n_i`.  Zeroes all priority, pending, enable,
     * threshold, and claim_in_flight state when `rst_n_i` is asserted (low),
     * then triggers an output recompute to deassert all `ctx_out` signals.
     * Returns immediately (no action) when `rst_n_i` is deasserted (high).
     */
    void reset_proc();

    /**
     * @brief SC_METHOD: interrupt source edge detector.
     *
     * Sensitive to every element of `src_in`.  On each invocation it scans
     * all sources for a 0→1 transition (rising edge) by comparing the new
     * level against `last_level_`.  For each newly-rising source that is **not**
     * currently in-flight (`claim_in_flight_` is clear), `pending_` is latched.
     * Triggers an output recompute if any pending bit changed.
     *
     * The `claim_in_flight_` guard prevents a source from re-asserting
     * `pending_` between a claim and its matching complete, even if the
     * hardware line stays high throughout (RISC-V PLIC spec §4).
     */
    void src_method();

    /**
     * @brief SC_METHOD: sole driver of all `ctx_out` signals.
     *
     * Sensitive to `recompute_event_`.  Calls `best_pending()` for each
     * context and drives `ctx_out[c]` accordingly.  Uses `ctx_out_cache_`
     * to suppress spurious sc_signal write events when the output value
     * has not actually changed.
     *
     * This is the **only** place `ctx_out` is written.  All other code paths
     * modify internal state and call `schedule_recompute()` to trigger this
     * method at SC_ZERO_TIME.
     */
    void output_method();

    /**
     * @brief Post `recompute_event_` at SC_ZERO_TIME.
     *
     * Called after any state change that may affect `ctx_out` (priority
     * write, enable write, threshold write, claim, complete, reset).
     * SC_ZERO_TIME ensures `output_method` runs in the next delta cycle,
     * after the current transaction/method has fully updated state.
     */
    void schedule_recompute();

    // ------------------------------------------------------------------
    // Register decode helpers
    // ------------------------------------------------------------------

    /**
     * @brief Decode a 32-bit register read from byte offset @p off.
     *
     * Address regions (per plic_cfg constants):
     * - `[0, PENDING_BASE)`:          PRIORITY registers (source 0 → RAZ)
     * - `[PENDING_BASE, ENABLE_BASE)`: PENDING registers (read-only bitfields)
     * - `[ENABLE_BASE, CONTEXT_BASE)`: ENABLE registers (per-context bitmaps)
     * - `[CONTEXT_BASE, WINDOW_SIZE)`: THRESHOLD and CLAIM/COMPLETE
     *
     * A CLAIM/COMPLETE read at `CONTEXT_CC_OFF` calls `claim()` as a side
     * effect (this is the standard RISC-V PLIC claim mechanism).
     *
     * @param off   Byte offset within the PLIC window.
     * @param data  Output: read data (0 for reserved/out-of-range addresses).
     * @return      True if @p off is within the PLIC window; false otherwise.
     */
    bool reg_read(uint64_t off, uint32_t& data);

    /**
     * @brief Decode a 32-bit register write of @p data to byte offset @p off.
     *
     * Follows the same address partitioning as `reg_read`.
     * PENDING registers are read-only; writes are silently discarded.
     * CLAIM/COMPLETE writes call `complete()` with the written source ID.
     *
     * @param off   Byte offset within the PLIC window.
     * @param data  Write data.
     * @return      True if @p off is within the PLIC window; false otherwise.
     */
    bool reg_write(uint64_t off, uint32_t data);

    // ------------------------------------------------------------------
    // Claim / complete
    // ------------------------------------------------------------------

    /**
     * @brief Atomically claim the highest-priority eligible interrupt for context @p ctx.
     *
     * Finds the best pending source via `best_pending()`, clears its
     * `pending_` bit, and sets `claim_in_flight_[src]` to prevent
     * re-assertion until `complete()` is called.  Returns 0 if nothing
     * is pending or eligible for this context.
     *
     * In the LT model this is called directly from `b_transport`; atomicity
     * is guaranteed because b_transport itself is atomic in LT.
     *
     * @param ctx  Context making the claim.
     * @return     Source ID of the claimed interrupt, or 0 if none.
     */
    uint32_t claim(unsigned ctx);

    /**
     * @brief Signal completion of interrupt @p src in context @p ctx.
     *
     * Clears `claim_in_flight_[src]`.  If the source line (`last_level_`)
     * is still asserted after completion, `pending_` is immediately
     * re-latched so the interrupt will re-fire — this is the correct RISC-V
     * PLIC behavior for level-sensitive sources that remain asserted through
     * their ISR.
     *
     * The @p ctx argument is accepted for API symmetry but is not used:
     * the RISC-V PLIC spec allows any context to complete any in-flight
     * interrupt.
     *
     * @param ctx  Context completing the interrupt (informational only).
     * @param src  Source ID to complete (1..num_sources; no-op if out-of-range
     *             or not currently in-flight).
     */
    void complete(unsigned ctx, uint32_t src);

    /**
     * @brief Find the highest-priority pending source eligible for context @p ctx.
     *
     * A source `s` is eligible for context `c` when all three hold:
     * - `pending_[s]` is set
     * - `enable_[c][word_index(s)] & bit_mask(s)` is non-zero
     * - `priority_[s] > threshold_[c]`  (strictly greater; priority 0 is disabled)
     *
     * Tie-breaking: if two sources share the highest eligible priority, the
     * one with the **lower source ID** wins (RISC-V PLIC spec §3).
     *
     * Implementation note: sources are scanned in ascending ID order (1, 2,
     * …, num_sources).  The update condition `prio == best_prio && best_src == 0`
     * naturally preserves the first (lowest-ID) match at each priority level,
     * implementing the tie-break without an explicit compare.
     *
     * @param ctx  Context index.
     * @return     Source ID of the top eligible interrupt, or 0 if none.
     */
    uint32_t best_pending(unsigned ctx) const;

    // ------------------------------------------------------------------
    // Internal state
    // ------------------------------------------------------------------

    /// Configuration snapshot taken at construction time.
    plic_cfg cfg_;

    /// Per-source 3-bit priority (index 0 unused; src 0 reserved by spec).
    /// Size: num_sources + 1.
    std::vector<uint32_t> priority_;

    /// Per-source pending latch.
    /// Set on the rising edge of src_in; cleared by claim(); may be
    /// immediately re-set by complete() if the source line is still high.
    /// Size: num_sources + 1.
    std::vector<bool> pending_;

    /// Previous level of each src_in, used for rising-edge detection in
    /// src_method().  Size: num_sources + 1.
    std::vector<bool> last_level_;

    /// True between claim() and its matching complete() for each source.
    /// Guards against spurious re-assertion of pending_ while an ISR is
    /// running with the source line still high.
    /// Size: num_sources + 1.
    std::vector<bool> claim_in_flight_;

    /// Per-context, per-word enable bitmaps.
    /// enable_[ctx][w] holds enable bits for sources 32w .. 32w+31.
    /// Dimensions: num_contexts × ceil((num_sources+1) / 32).
    std::vector<std::vector<uint32_t>> enable_;

    /// Per-context interrupt threshold (3-bit, 0..7).
    /// A source is masked unless priority[s] > threshold[ctx].
    /// Size: num_contexts.
    std::vector<uint32_t> threshold_;

    /// Cached last-driven value of each ctx_out signal.
    /// Prevents spurious sc_signal write events when the computed output
    /// has not changed (sc_signal::write() triggers an event even on
    /// same-value writes in some SystemC implementations).
    /// Size: num_contexts.
    std::vector<bool> ctx_out_cache_;

    /// Notified at SC_ZERO_TIME whenever any internal state that may affect
    /// ctx_out changes.  output_method() is sensitive to this event and is
    /// the sole writer of ctx_out[].
    sc_core::sc_event recompute_event_;
};

} // namespace smc

#endif // SMC_PLIC_H_
