// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file plic.cpp
 * @brief SMC PLIC — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/plic.h` for the full design description, register map, and
 * compliance notes.
 */

#include "plic.h"

#include "sim_log.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

namespace smc {

namespace {

/// Mask applied to all PRIORITY and THRESHOLD register writes.
/// The field is 3-bit ([2:0]) per plic.rdl, giving values 0..7
/// (7 non-zero priority levels; 0 = source disabled).
constexpr unsigned PRIORITY_MASK  = 0x7u;
constexpr unsigned THRESHOLD_MASK = 0x7u;

/// Return the index into the 32-bit enable/pending word array for source @p src.
/// Sources are packed 32-per-word: source s lives in word (s >> 5).
inline unsigned word_index(unsigned src) { return src >> 5; }

/// Return the single-bit mask for source @p src within its enable/pending word.
/// Source s is bit (s & 0x1F) within word word_index(s).
inline uint32_t bit_mask(unsigned src) { return 1u << (src & 0x1Fu); }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

/**
 * @brief Construct the PLIC, validate configuration, allocate state arrays,
 *        register TLM callbacks, and register SC_METHOD processes.
 *
 * Three SC_METHOD processes are registered:
 * - `reset_proc`   sensitive to `rst_n_i`
 * - `src_method`   sensitive to every element of `src_in`
 * - `output_method` sensitive to `recompute_event_`
 *
 * `dont_initialize()` is called on all three so they do not fire during
 * the elaboration phase before simulation begins.
 */
plic::plic(sc_core::sc_module_name name, plic_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params are initialised first (declared before ports in plic.h).
    // cfg.num_sources / cfg.num_contexts become the DEFAULT values; a broker
    // preset set before this module is constructed will override them.
    , num_sources_p_(
          "num_sources",
          cfg.num_sources,
          "Number of interrupt sources (1..1023). "
          "Source IDs are 1-based; source 0 is architecturally reserved. "
          "Must match PRIORITY[337] in plic.rdl (fw: PLIC0_MAX_INTERRUPTS).")
    , num_contexts_p_(
          "num_contexts",
          cfg.num_contexts,
          "Number of interrupt contexts (harts × privilege modes). "
          "SMC default: 4 cores × {M-mode, S-mode} = 8.")
    , access_delay_ns_p_(
          "access_delay_ns",
          2.0,
          "TLM register-access annotated delay in nanoseconds. "
          "Approximates AXI4-Lite bus latency. Mutable at run-time.")
    // Ports sized from the (possibly preset-overridden) CCI param values.
    , reg_socket("reg_socket")
    , src_in   ("src_in",  num_sources_p_.get_value())
    , ctx_out  ("ctx_out", num_contexts_p_.get_value())
    , rst_n_i  ("rst_n_i")
    , cfg_(cfg)
{
    // Sync cfg_ with the CCI-resolved values so the rest of the constructor
    // and all member functions that read cfg_.num_* see consistent data.
    cfg_.num_sources  = num_sources_p_.get_value();
    cfg_.num_contexts = num_contexts_p_.get_value();

    // Attach metadata so tools and testbenches can introspect provenance.
    num_sources_p_.add_metadata("rdl_field",   cci::cci_value(std::string("PRIORITY[337]")));
    num_sources_p_.add_metadata("fw_define",   cci::cci_value(std::string("PLIC0_MAX_INTERRUPTS")));
    num_sources_p_.add_metadata("valid_range", cci::cci_value(std::string("1..1023")));
    num_contexts_p_.add_metadata("formula",    cci::cci_value(std::string("num_harts * num_privilege_modes")));
    access_delay_ns_p_.add_metadata("unit",      cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));

    if (cfg_.num_sources == 0 || cfg_.num_sources > 1023) {
        SC_REPORT_FATAL(name, "PLIC num_sources must be in 1..1023");
    }
    if (cfg_.num_contexts == 0) {
        SC_REPORT_FATAL(name, "PLIC num_contexts must be >= 1");
    }

    // Report the resolved CCI configuration so every simulation run shows
    // whether values came from a preset (tool/TB override) or the default.
    SIM_LOG_INFO(this,
        "CCI config resolved:"
        << "  num_sources=" << cfg_.num_sources
        << (num_sources_p_.is_preset_value()     ? " [preset]"  : " [default]")
        << "  num_contexts=" << cfg_.num_contexts
        << (num_contexts_p_.is_preset_value()    ? " [preset]"  : " [default]")
        << "  access_delay_ns=" << access_delay_ns_p_.get_value()
        << (access_delay_ns_p_.is_preset_value() ? " [preset]"  : " [default]"));

    // Source IDs are 1-based; we keep index 0 allocated (always 0) so that
    // the address arithmetic `priority_[src]` is identical to the spec
    // (e.g. PRIORITY register at offset 4*src directly maps to priority_[src]).
    const unsigned N = cfg_.num_sources + 1;
    const unsigned W = (N + 31) / 32; // number of 32-bit enable/pending words

    priority_       .assign(N, 0);
    pending_        .assign(N, false);
    last_level_     .assign(N, false);
    claim_in_flight_.assign(N, false);
    enable_         .assign(cfg_.num_contexts, std::vector<uint32_t>(W, 0));
    threshold_      .assign(cfg_.num_contexts, 0);
    ctx_out_cache_  .assign(cfg_.num_contexts, false);

    reg_socket.register_b_transport   (this, &plic::b_transport);
    reg_socket.register_transport_dbg (this, &plic::transport_dbg);

    SC_METHOD(src_method);
    for (auto& p : src_in) sensitive << p;
    dont_initialize();

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    // output_method is the sole owner of every ctx_out signal.  SystemC
    // requires exactly one driver per sc_signal; all other code paths
    // mutate internal state and call schedule_recompute() instead.
    SC_METHOD(output_method);
    sensitive << recompute_event_;
    dont_initialize();
}

// ---------------------------------------------------------------------------
// schedule_recompute
// ---------------------------------------------------------------------------

/**
 * @brief Notify `recompute_event_` at SC_ZERO_TIME to trigger `output_method`.
 *
 * SC_ZERO_TIME ensures the event fires in the next delta cycle, after the
 * current transaction or SC_METHOD has finished updating internal state.
 * Multiple calls within the same delta are collapsed by the SystemC kernel
 * into a single notification.
 */
void plic::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

// ---------------------------------------------------------------------------
// reset_proc
// ---------------------------------------------------------------------------

/**
 * @brief SC_METHOD: clear all PLIC state on active-low reset assertion.
 *
 * Called whenever `rst_n_i` changes value.  Returns immediately (no action)
 * when `rst_n_i` is deasserted (high) — reset is only processed on the
 * falling edge.  Zeroes priority, pending, last_level, claim_in_flight,
 * enable, and threshold arrays, then triggers an output recompute so
 * all `ctx_out` signals are driven low within the same time step.
 */
void plic::reset_proc()
{
    if (rst_n_i.read()) return; // act only on assertion (low)

    std::fill(priority_       .begin(), priority_       .end(), 0u);
    std::fill(pending_        .begin(), pending_        .end(), false);
    std::fill(last_level_     .begin(), last_level_     .end(), false);
    std::fill(claim_in_flight_.begin(), claim_in_flight_.end(), false);
    for (auto& v : enable_) std::fill(v.begin(), v.end(), 0u);
    std::fill(threshold_.begin(), threshold_.end(), 0u);

    schedule_recompute();
}

// ---------------------------------------------------------------------------
// src_method
// ---------------------------------------------------------------------------

/**
 * @brief SC_METHOD: rising-edge detector — latches pending bits from src_in.
 *
 * Called whenever any element of `src_in` changes.  Scans every source for
 * a low-to-high transition.  When a rising edge is detected on source `s`:
 * - If `claim_in_flight_[s]` is clear, `pending_[s]` is set.
 * - If `claim_in_flight_[s]` is set (ISR in progress for this source),
 *   the rising edge is ignored; `complete()` will re-arm `pending_` once
 *   the ISR finishes (RISC-V PLIC spec §4: "the PLIC will not set the pending
 *   bit again until the interrupt completes").
 *
 * `last_level_` is updated unconditionally so the next invocation can detect
 * falling edges correctly.
 */
void plic::src_method()
{
    bool any_change = false;
    for (unsigned i = 0; i < cfg_.num_sources; ++i) {
        const unsigned src = i + 1; // source IDs are 1-based
        const bool now = src_in[i].read();
        const bool was = last_level_[src];

        if (now && !was) {
            // Rising edge: latch as pending unless an ISR is already handling
            // this source (claim_in_flight_ guard).
            if (!claim_in_flight_[src]) {
                pending_[src] = true;
                any_change    = true;
            }
        }
        last_level_[src] = now;
    }
    if (any_change) schedule_recompute();
}

// ---------------------------------------------------------------------------
// output_method
// ---------------------------------------------------------------------------

/**
 * @brief SC_METHOD: recompute and drive all ctx_out signals.
 *
 * The sole writer of `ctx_out[]`.  Calls `best_pending()` for each context
 * and drives the output high if any eligible interrupt is pending.  Uses
 * `ctx_out_cache_` to avoid emitting a spurious sc_signal event when the
 * computed value is unchanged.
 */
void plic::output_method()
{
    for (unsigned c = 0; c < cfg_.num_contexts; ++c) {
        const uint32_t top = best_pending(c);
        const bool     hi  = (top != 0);
        if (ctx_out_cache_[c] != hi) {
            ctx_out_cache_[c] = hi;
            ctx_out[c].write(hi);
        }
    }
}

// ---------------------------------------------------------------------------
// best_pending
// ---------------------------------------------------------------------------

/**
 * @brief Return the source ID of the highest-priority eligible interrupt for
 *        context @p ctx, or 0 if none.
 *
 * Eligibility: `pending_[s]` && `enable_[ctx][s]` && `priority_[s] > threshold_[ctx]`.
 *
 * Tie-breaking: the lowest source ID wins among equal-priority candidates.
 * Implemented implicitly: sources are iterated in ascending ID order and the
 * equal-priority branch (`prio == best_prio && best_src == 0`) only fires
 * when no candidate has been found yet, so the first (lowest-ID) eligible
 * source at the maximum priority is always retained.
 */
uint32_t plic::best_pending(unsigned ctx) const
{
    const uint32_t thr = threshold_[ctx];
    uint32_t best_src  = 0;
    uint32_t best_prio = 0;

    for (unsigned src = 1; src <= cfg_.num_sources; ++src) {
        if (!pending_[src]) continue;

        const uint32_t prio = priority_[src];
        // Priority 0 permanently disables a source (spec §3).
        // Sources with priority <= threshold are masked (strictly greater required).
        if (prio == 0 || prio <= thr) continue;

        // Check the per-context enable bit for this source.
        const uint32_t word = word_index(src);
        if ((enable_[ctx][word] & bit_mask(src)) == 0) continue;

        // Update the running best: higher priority always wins; on tie the
        // first candidate seen (lowest source ID, because we iterate ascending)
        // is kept (best_src == 0 only on the very first eligible hit).
        if (prio > best_prio || (prio == best_prio && best_src == 0)) {
            best_prio = prio;
            best_src  = src;
        }
    }
    return best_src;
}

// ---------------------------------------------------------------------------
// claim / complete
// ---------------------------------------------------------------------------

/**
 * @brief Atomically claim the highest-priority pending interrupt for context @p ctx.
 *
 * Finds the top source via `best_pending()`, clears its `pending_` bit, and
 * sets `claim_in_flight_` to block re-assertion until `complete()` is called.
 * If nothing is eligible, returns 0 and changes no state.
 *
 * In the LT model, atomicity is guaranteed: `b_transport` is a blocking call
 * and the SystemC kernel does not schedule other SC_METHODs between the
 * `best_pending` call and the state update within a single b_transport
 * invocation.
 */
uint32_t plic::claim(unsigned ctx)
{
    const uint32_t src = best_pending(ctx);
    if (src != 0) {
        pending_[src]         = false;
        claim_in_flight_[src] = true;
        schedule_recompute();
    }
    return src;
}

/**
 * @brief Signal completion of interrupt @p src (any context may complete).
 *
 * Clears `claim_in_flight_[src]`.  Per the RISC-V PLIC spec, if the source
 * line is still asserted at the time of completion, `pending_` is immediately
 * re-latched — the device stays pending without requiring another rising edge.
 * If the line has dropped, pending re-arming waits for the next rising edge
 * detected by `src_method()`.
 *
 * The @p ctx parameter is accepted for interface symmetry but is not used;
 * the spec does not restrict which context may issue a complete.
 */
void plic::complete(unsigned ctx, uint32_t src)
{
    (void)ctx;
    if (src == 0 || src > cfg_.num_sources) return;

    claim_in_flight_[src] = false;

    // Re-latch pending immediately if the source line is still asserted.
    // This handles the common case where the ISR runs while the device line
    // remains high (e.g. the ISR acknowledges the device which then deasserts).
    if (last_level_[src] && !pending_[src]) {
        pending_[src] = true;
    }
    schedule_recompute();
}

// ---------------------------------------------------------------------------
// reg_read
// ---------------------------------------------------------------------------

/**
 * @brief Decode and execute a 32-bit register read at byte offset @p off.
 *
 * Address regions and behaviour:
 *
 * | Region                          | Behaviour |
 * |---------------------------------|-----------|
 * | [0, PENDING_BASE)               | PRIORITY[src=off>>2]: returns 3-bit priority; RAZ for src=0 or out-of-range |
 * | [PENDING_BASE, ENABLE_BASE)     | PENDING[w]: packed 32-bit bitmap of pending bits (RO) |
 * | [ENABLE_BASE, CONTEXT_BASE)     | ENABLE[ctx][w]: per-context enable bitmap |
 * | [CONTEXT_BASE, WINDOW_SIZE)     | THRESHOLD or CLAIM/COMPLETE per context |
 * | >= WINDOW_SIZE                  | Returns false → caller issues TLM_ADDRESS_ERROR_RESPONSE |
 *
 * @note Reading CLAIM/COMPLETE (`CONTEXT_CC_OFF`) calls `claim()` which clears
 *       `pending_` and sets `claim_in_flight_`.  This is intentional and
 *       matches the RISC-V PLIC specification §4.
 *
 * @param off   Byte offset within the PLIC register window.
 * @param data  Output: read data (set to 0 on reserved/RAZ addresses).
 * @return      True if @p off is inside the PLIC window; false if outside.
 */
bool plic::reg_read(uint64_t off, uint32_t& data)
{
    data = 0;

    // PRIORITY region: one 32-bit register per source, indexed by off >> 2.
    if (off < cfg_.PENDING_BASE) {
        const uint32_t src = static_cast<uint32_t>(off >> 2);
        if (src == 0 || src > cfg_.num_sources) return true; // RAZ: reserved/out-of-range
        data = priority_[src] & PRIORITY_MASK;
        return true;
    }

    // PENDING region: 32 sources packed per word; word w starts at PENDING_BASE + 4w.
    if (off >= cfg_.PENDING_BASE && off < cfg_.ENABLE_BASE) {
        const uint32_t w = static_cast<uint32_t>((off - cfg_.PENDING_BASE) >> 2);
        const uint32_t W = (cfg_.num_sources + 1 + 31) / 32;
        if (w >= W) return true; // out-of-range word → RAZ
        uint32_t v = 0;
        for (unsigned b = 0; b < 32; ++b) {
            const unsigned src = w * 32 + b;
            if (src >= 1 && src <= cfg_.num_sources && pending_[src]) v |= (1u << b);
        }
        data = v;
        return true;
    }

    // ENABLE region: context c, word w at ENABLE_BASE + c*ENABLE_STRIDE + 4w.
    if (off >= cfg_.ENABLE_BASE && off < cfg_.CONTEXT_BASE) {
        const uint64_t rel = off - cfg_.ENABLE_BASE;
        const uint32_t ctx = static_cast<uint32_t>(rel / cfg_.ENABLE_STRIDE);
        const uint32_t w   = static_cast<uint32_t>((rel % cfg_.ENABLE_STRIDE) >> 2);
        if (ctx >= cfg_.num_contexts) return true;
        const uint32_t W = (cfg_.num_sources + 1 + 31) / 32;
        if (w >= W) return true;
        data = enable_[ctx][w];
        return true;
    }

    // CONTEXT region: threshold + claim/complete per context.
    if (off >= cfg_.CONTEXT_BASE && off < cfg_.WINDOW_SIZE) {
        const uint64_t rel    = off - cfg_.CONTEXT_BASE;
        const uint32_t ctx    = static_cast<uint32_t>(rel / cfg_.CONTEXT_STRIDE);
        const uint32_t suboff = static_cast<uint32_t>(rel % cfg_.CONTEXT_STRIDE);
        if (ctx >= cfg_.num_contexts) return true;

        if (suboff == cfg_.CONTEXT_THR_OFF) {
            data = threshold_[ctx] & THRESHOLD_MASK;
            return true;
        }
        if (suboff == cfg_.CONTEXT_CC_OFF) {
            data = claim(ctx); // side-effect: claims the top interrupt
            return true;
        }
        return true; // reserved within context block → RAZ
    }

    return false; // outside window → caller issues TLM_ADDRESS_ERROR_RESPONSE
}

// ---------------------------------------------------------------------------
// reg_write
// ---------------------------------------------------------------------------

/**
 * @brief Decode and execute a 32-bit register write of @p data at byte offset @p off.
 *
 * | Region                         | Behaviour |
 * |--------------------------------|-----------|
 * | [0, PENDING_BASE)              | PRIORITY[src]: stores data[2:0]; WI for src=0 or out-of-range |
 * | [PENDING_BASE, ENABLE_BASE)    | PENDING: read-only per spec; write silently discarded |
 * | [ENABLE_BASE, CONTEXT_BASE)    | ENABLE[ctx][w]: stores data masked to valid source bits |
 * | [CONTEXT_BASE, WINDOW_SIZE)    | THRESHOLD: stores data[2:0]; CLAIM/COMPLETE: calls complete() |
 * | >= WINDOW_SIZE                 | Returns false → caller issues TLM_ADDRESS_ERROR_RESPONSE |
 *
 * Enable register masking:
 * - Bit 0 of word 0 (source 0) is always forced to 0 (source 0 reserved).
 * - Bits above `num_sources` in the last word are masked out so they cannot
 *   spuriously affect arbitration if sources are added later.
 *
 * @param off   Byte offset within the PLIC register window.
 * @param data  Write data.
 * @return      True if @p off is inside the PLIC window; false if outside.
 */
bool plic::reg_write(uint64_t off, uint32_t data)
{
    // PRIORITY
    if (off < cfg_.PENDING_BASE) {
        const uint32_t src = static_cast<uint32_t>(off >> 2);
        if (src == 0 || src > cfg_.num_sources) return true; // WI: reserved/out-of-range
        priority_[src] = data & PRIORITY_MASK;
        schedule_recompute();
        return true;
    }

    // PENDING is read-only (SW=r per plic.rdl); silently discard writes.
    if (off >= cfg_.PENDING_BASE && off < cfg_.ENABLE_BASE) {
        return true;
    }

    // ENABLE
    if (off >= cfg_.ENABLE_BASE && off < cfg_.CONTEXT_BASE) {
        const uint64_t rel = off - cfg_.ENABLE_BASE;
        const uint32_t ctx = static_cast<uint32_t>(rel / cfg_.ENABLE_STRIDE);
        const uint32_t w   = static_cast<uint32_t>((rel % cfg_.ENABLE_STRIDE) >> 2);
        if (ctx >= cfg_.num_contexts) return true;
        const uint32_t W = (cfg_.num_sources + 1 + 31) / 32;
        if (w >= W) return true;

        // Build a mask of writable bits for this word.
        uint32_t mask = 0xFFFFFFFFu;
        // Bit 0 of word 0 corresponds to source 0 (reserved); force it to 0.
        if (w == 0) mask &= ~0x1u;
        // Mask off bits beyond num_sources in the last word to prevent
        // out-of-range enable bits from affecting best_pending().
        if (w == W - 1) {
            const unsigned valid_bits = (cfg_.num_sources + 1) - (W - 1) * 32u;
            if (valid_bits < 32) mask &= ((1u << valid_bits) - 1u);
        }
        enable_[ctx][w] = data & mask;
        schedule_recompute();
        return true;
    }

    // CONTEXT
    if (off >= cfg_.CONTEXT_BASE && off < cfg_.WINDOW_SIZE) {
        const uint64_t rel    = off - cfg_.CONTEXT_BASE;
        const uint32_t ctx    = static_cast<uint32_t>(rel / cfg_.CONTEXT_STRIDE);
        const uint32_t suboff = static_cast<uint32_t>(rel % cfg_.CONTEXT_STRIDE);
        if (ctx >= cfg_.num_contexts) return true;

        if (suboff == cfg_.CONTEXT_THR_OFF) {
            threshold_[ctx] = data & THRESHOLD_MASK;
            schedule_recompute();
            return true;
        }
        if (suboff == cfg_.CONTEXT_CC_OFF) {
            complete(ctx, data); // data holds the source ID to complete
            return true;
        }
        return true; // reserved within context block → WI
    }

    return false;
}

// ---------------------------------------------------------------------------
// b_transport
// ---------------------------------------------------------------------------

/**
 * @brief TLM-2.0 blocking transport — main CPU-facing register access path.
 *
 * Validates the transaction, delegates to `reg_read` or `reg_write`, annotates
 * `access_delay_`, and sets the TLM response status.
 *
 * ### Validation rules
 * - Data length must be exactly 4 bytes (32-bit access).
 * - Address must be 4-byte aligned.  Violations → `TLM_BURST_ERROR_RESPONSE`.
 * - Address must be < `WINDOW_SIZE`.  Violation → `TLM_ADDRESS_ERROR_RESPONSE`.
 * - Command must be TLM_READ_COMMAND or TLM_WRITE_COMMAND.  Other commands
 *   → `TLM_COMMAND_ERROR_RESPONSE`.
 *
 * ### smc_axi_extension handling
 * The optional `smc_axi_extension` (source_id, prot, etc.) is extracted but
 * not acted upon in this model.  Access-control filtering (e.g. blocking
 * non-M-mode writes to PLIC threshold) is the responsibility of the
 * `axi_filter` module upstream (`02_SMC_IP_LowLevel_Design.md §2`).
 *
 * ### DMI
 * Always denied (`set_dmi_allowed(false)`) because CLAIM reads have a
 * side effect; granting DMI would allow the initiator to bypass `claim()`.
 */
void plic::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (buf == nullptr) {
        gp.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (len != 4 || (adr & 0x3u) != 0) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    if (adr >= cfg_.WINDOW_SIZE) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // Extract optional AXI sideband extension.  Not enforced here; kept as
    // a hook for future integration with the upstream axi_filter.
    smc::smc_axi_extension* ext = nullptr;
    gp.get_extension(ext);
    (void)ext;

    bool ok;
    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = 0;
        ok = reg_read(adr, v);
        if (ok) std::memcpy(buf, &v, 4);
        SIM_LOG_TRACE(this, "read  off=0x" << std::hex << adr << " data=0x" << v);
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t v = 0;
        std::memcpy(&v, buf, 4);
        ok = reg_write(adr, v);
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << adr << " data=0x" << v);
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (!ok) SIM_LOG_DEBUG(this, "TLM decode miss at off=0x" << std::hex << adr);

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                              : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    gp.set_dmi_allowed(false); // PLIC claim has side effects; DMI must never be granted
}

// ---------------------------------------------------------------------------
// transport_dbg
// ---------------------------------------------------------------------------

/**
 * @brief TLM-2.0 debug transport — back-door register access with no side effects.
 *
 * Accepts the same address space as `b_transport` but differs in one key way:
 * a read of the CLAIM/COMPLETE register returns `best_pending(ctx)` without
 * calling `claim()`, so the pending state is not disturbed.  This allows
 * test-bench code and debug tools to inspect which interrupt would be claimed
 * without perturbing simulation state.
 *
 * Writes are forwarded to `reg_write()` unchanged (write-side has no special
 * side-effect behaviour in this model).
 *
 * @return  Number of bytes transferred (4 on success, 0 on error).
 */
unsigned int plic::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (buf == nullptr)
        return 0;

    if (len != 4 || (adr & 0x3u) != 0 || adr >= cfg_.WINDOW_SIZE) return 0;

    if (cmd == tlm::TLM_READ_COMMAND) {
        // Special case: CLAIM/COMPLETE read — peek without claiming.
        if (adr >= cfg_.CONTEXT_BASE) {
            const uint64_t rel    = adr - cfg_.CONTEXT_BASE;
            const uint32_t ctx    = static_cast<uint32_t>(rel / cfg_.CONTEXT_STRIDE);
            const uint32_t suboff = static_cast<uint32_t>(rel % cfg_.CONTEXT_STRIDE);
            if (ctx < cfg_.num_contexts && suboff == cfg_.CONTEXT_CC_OFF) {
                uint32_t v = best_pending(ctx); // no claim() call
                std::memcpy(buf, &v, 4);
                return 4;
            }
        }
        uint32_t v = 0;
        if (!reg_read(adr, v)) return 0;
        std::memcpy(buf, &v, 4);
        return 4;
    }
    if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t v = 0;
        std::memcpy(&v, buf, 4);
        if (!reg_write(adr, v)) return 0;
        return 4;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Debug back-door API
// ---------------------------------------------------------------------------

/**
 * @brief Return the current priority of source @p src (back-door, no side effects).
 */
uint32_t plic::dbg_priority(unsigned src) const
{
    if (src == 0 || src > cfg_.num_sources) return 0;
    return priority_[src];
}

/**
 * @brief Return true if source @p src is currently pending (back-door).
 */
bool plic::dbg_pending(unsigned src) const
{
    if (src == 0 || src > cfg_.num_sources) return false;
    return pending_[src];
}

/**
 * @brief Return true if source @p src is enabled for context @p ctx (back-door).
 */
bool plic::dbg_enable(unsigned ctx, unsigned src) const
{
    if (ctx >= cfg_.num_contexts || src == 0 || src > cfg_.num_sources) return false;
    return (enable_[ctx][word_index(src)] & bit_mask(src)) != 0;
}

/**
 * @brief Return the interrupt threshold for context @p ctx (back-door).
 */
uint32_t plic::dbg_threshold(unsigned ctx) const
{
    if (ctx >= cfg_.num_contexts) return 0;
    return threshold_[ctx];
}

/**
 * @brief Return the top-pending source ID for context @p ctx without claiming it (back-door).
 */
uint32_t plic::dbg_claim_top(unsigned ctx) const
{
    if (ctx >= cfg_.num_contexts) return 0;
    return best_pending(ctx);
}

/**
 * @brief Print a human-readable snapshot of all PLIC state to @p os.
 *
 * Intended for post-failure debug dumps.  Output includes module name,
 * configuration, all currently pending sources with their priorities, and
 * per-context threshold / output / best-pending values.
 */
void plic::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] PLIC state dump\n";
    os << "  CCI parameters:\n"
       << "    num_sources="     << num_sources_p_.get_value()
       << (num_sources_p_.is_preset_value()     ? " [preset]"  : " [default]") << "\n"
       << "    num_contexts="    << num_contexts_p_.get_value()
       << (num_contexts_p_.is_preset_value()    ? " [preset]"  : " [default]") << "\n"
       << "    access_delay_ns=" << access_delay_ns_p_.get_value()
       << (access_delay_ns_p_.is_preset_value() ? " [preset]"  : " [default]") << "\n";
    os << "  pending sources:";
    for (unsigned s = 1; s <= cfg_.num_sources; ++s)
        if (pending_[s]) os << " #" << s << "(p=" << priority_[s] << ")";
    os << "\n";
    for (unsigned c = 0; c < cfg_.num_contexts; ++c) {
        os << "  ctx[" << c << "] thr=" << threshold_[c]
           << " out=" << ctx_out_cache_[c]
           << " best_pending=" << best_pending(c) << "\n";
    }
}

} // namespace smc
