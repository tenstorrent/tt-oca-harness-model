/**
 * @file el2_pic.cpp
 * @brief Behavioural implementation of the VeeR EL2 PIC model.
 */
#include "VeeR-ISSTlm.hpp"
#include "el2_pic.h"

namespace el2_pic {

el2_pic_model::el2_pic_model(sc_module_name n)
    : el2_pic_base(n)
    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
    , clk_i("clk_i")
    , rst_ni("rst_ni")
    , irq_in("irq_in", NUM_INTERRUPTS)
    , irq_tie_low_("irq_tie_low")
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    source_pending_.fill(false);
    last_effective_level_.fill(false);

    // Reset handler: SystemC method sensitive to negative edge of rst_ni
    SC_METHOD(reset_process);
    sensitive << rst_ni.neg();
    dont_initialize();

    register_all_callbacks();

    CSML_INFO(1, logger) << "el2_pic instantiated, NUM_INTERRUPTS=" << NUM_INTERRUPTS << std::endl;
}

// Deferred to before_end_of_elaboration so that the parent has finished
// binding pic->irq_in[i] to actual sc_signals. Accessing
// value_changed_event() on an unbound sc_in raises "port is not bound".
//
// The PIC has 256 slots but a given integration drives only a handful, so any
// port the parent left open is tied low here first. Without this, widening
// NUM_INTERRUPTS would break every existing parent at once: the sensitivity
// lookup below would fail immediately, and sc_start() would abort with E109 on
// the unbound ports regardless.
void el2_pic_model::before_end_of_elaboration()
{
    irq_tie_low_.init(NUM_INTERRUPTS);

    for (unsigned i = 0; i < NUM_INTERRUPTS; ++i) {
        if (!irq_in[i].get_interface()) {
            irq_tie_low_[i].write(false);
            irq_in[i](irq_tie_low_[i]);
        }

        sc_core::sc_spawn_options opt;
        opt.spawn_method();
        opt.set_sensitivity(&irq_in[i].value_changed_event());
        opt.dont_initialize();
        sc_core::sc_spawn(sc_bind(&el2_pic_model::gateway_changed, this, i),
                          sc_core::sc_gen_unique_name("gateway"), &opt);
    }
}

// =============================================================================
// Reset
// =============================================================================

void el2_pic_model::reset_process()
{
    reset_all_registers();
    source_pending_.fill(false);
    last_effective_level_.fill(false);

    if (eip_asserted_ && hart_ != nullptr) {
        hart_->clear_external_interrupt(MachineMode);
    }
    eip_asserted_ = false;
    current_claim_id_ = 0;

    CSML_INFO(2, logger) << "reset asserted — PIC state cleared" << std::endl;
}

// =============================================================================
// Gateway sensitivity (one spawn per source)
// =============================================================================

void el2_pic_model::gateway_changed(unsigned source_id)
{
    if (source_id == 0 || source_id >= NUM_INTERRUPTS) return;
    recompute_pending_for_source(source_id);
    reevaluate_arbitration();
}

void el2_pic_model::recompute_pending_for_source(unsigned source_id)
{
    bool raw = irq_in[source_id].read();

    // Apply polarity: meigwctrl.polarity=0 means active-high, 1 means active-low.
    bool polarity = (MEIGWCTRL[source_id].polarity.get() != 0);
    bool effective = polarity ? !raw : raw;

    bool is_edge = (MEIGWCTRL[source_id].irq_type.get() != 0);

    if (is_edge) {
        // Edge mode: latch on the qualifying transition (0 → 1 of effective).
        if (effective && !last_effective_level_[source_id]) {
            source_pending_[source_id] = true;
        }
    } else {
        // Level mode: pending tracks the effective level directly.
        source_pending_[source_id] = effective;
    }
    last_effective_level_[source_id] = effective;
}

// source_pending_ is the only pending state; the meip registers are computed
// from it on demand by read_MEIP. Deliberately no mirror into MEIP storage:
// csml's read_registers() consults the read callback on the debug path too
// (csml_register.h:359, reached from transport_dbg), so stored values are never
// observable, and a mirror would be state that can drift with nothing to catch
// it. This also matches the RTL, where intpend is combinational off the
// gateways rather than a register (el2_pic_ctrl.sv:488).
uint32_t el2_pic_model::pending_word(unsigned w) const
{
    uint32_t bits = 0;
    for (unsigned b = 0; b < 32; ++b) {
        const unsigned s = w * 32 + b;
        if (s < NUM_INTERRUPTS && source_pending_[s]) bits |= (1u << b);
    }
    return bits;
}

// =============================================================================
// Priority arbitration
// =============================================================================
//
// Mirrors el2_pic_ctrl.sv arbitration: when priord=0 highest raw value wins
// (15=highest); when priord=1 priorities are bitwise-inverted before comparison
// so 0=highest (raw 0 → eff 15). meipt/meicurpl thresholds are assumed 0
// because the VeeR ISS does not implement those CSRs (0 means no threshold).

void el2_pic_model::reevaluate_arbitration()
{
    bool priord = (MPICCFG.priord.get() != 0);

    unsigned best_id = 0;
    unsigned best_eff_prio = 0;

    for (unsigned s = 1; s < NUM_INTERRUPTS; ++s) {
        if (!source_pending_[s]) continue;
        if (MEIE[s].inten.get() == 0) continue;
        unsigned raw_prio = MEIPL[s].intpriority.get();
        if (raw_prio == 0) continue;
        // RTL: intpriority_reg_inv[i] = intpriord ? ~intpriority_reg[i] : intpriority_reg[i]
        unsigned eff_prio = priord ? (~raw_prio & 0xF) : raw_prio;
        if (eff_prio > best_eff_prio) {
            best_eff_prio = eff_prio;
            best_id = s;
        }
    }

    if (hart_ == nullptr) return;

    if (best_id != 0) {
        // Apply meipt/meicurpl threshold (RTL: mexintpend = prio > meipt_eff && prio > meicurpl_eff).
        uint32_t meipt_raw = 0, meicurpl_raw = 0;
        hart_->peek_csr(0xBC9, meipt_raw);
        hart_->peek_csr(0xBCC, meicurpl_raw);
        // Matches el2_pic_ctrl.sv exactly: meipt/meicurpl are inverted the same
        // way intpriority is, with no raw==0 special case. Hardware does not
        // rescale these when priord changes — firmware must reprogram them for
        // the active mode's encoding (raw 0 means "no threshold" only when
        // priord=0; under priord=1 that inverts to 0xF, the maximum).
        unsigned meipt_eff    = priord ? (~meipt_raw    & 0xF) : (meipt_raw    & 0xF);
        unsigned meicurpl_eff = priord ? (~meicurpl_raw & 0xF) : (meicurpl_raw & 0xF);
        if (best_eff_prio <= meipt_eff || best_eff_prio <= meicurpl_eff) {
            if (eip_asserted_) {
                hart_->clear_external_interrupt(MachineMode);
                eip_asserted_ = false;
                current_claim_id_ = 0;
            }
            return;
        }

        // A winning source exists above threshold. Update MEIHAP.claim_id, drive external IRQ.
        if (best_id != current_claim_id_ || !eip_asserted_) {
            hart_->set_pic_claim_id(best_id);
            // RTL (dec_tlu_ctl.sv): on take_ext_int_start, meicidpl ← pic_pl.
            // meicurpl is firmware-only (CSRW); hardware never auto-updates it.
            hart_->poke_csr(0xBCB, best_eff_prio);
            hart_->trigger_external_interrupt(MachineMode);
            current_claim_id_ = best_id;
            eip_asserted_ = true;
            CSML_INFO(2, logger) << "arbiter: claim_id=" << best_id
                                  << " eff_prio=" << best_eff_prio
                                  << " priord=" << priord
                                  << " external IRQ asserted" << std::endl;
        }
    } else {
        // Nothing pending+enabled. Drop external IRQ if currently asserted.
        if (eip_asserted_) {
            hart_->clear_external_interrupt(MachineMode);
            eip_asserted_ = false;
            current_claim_id_ = 0;
            CSML_INFO(2, logger) << "arbiter: no pending source, external IRQ cleared" << std::endl;
        }
    }
}

// =============================================================================
// Register callbacks
// =============================================================================

bool el2_pic_model::post_write_MEIPL(unsigned source_id)
{
    CSML_DEBUG(3, logger) << "MEIPL[" << source_id << "] = " << MEIPL[source_id].intpriority.get() << std::endl;
    reevaluate_arbitration();
    return true;
}

bool el2_pic_model::post_write_MEIE(unsigned source_id)
{
    CSML_DEBUG(3, logger) << "MEIE[" << source_id << "] = " << MEIE[source_id].inten.get() << std::endl;
    reevaluate_arbitration();
    return true;
}

bool el2_pic_model::post_write_MEIGWCTRL(unsigned source_id)
{
    // Gateway sensitivity changed (level↔edge or polarity). Re-evaluate this
    // source's pending bit so the new rules apply to the current input level.
    CSML_DEBUG(3, logger) << "MEIGWCTRL[" << source_id << "] type="
                          << MEIGWCTRL[source_id].irq_type.get()
                          << " polarity=" << MEIGWCTRL[source_id].polarity.get() << std::endl;
    recompute_pending_for_source(source_id);
    reevaluate_arbitration();
    return true;
}

bool el2_pic_model::write_MEIGWCLR(unsigned source_id, uint32_t value)
{
    // Write-only: any write clears the latched edge-pending state.
    (void)value;
    CSML_DEBUG(3, logger) << "MEIGWCLR[" << source_id << "] cleared" << std::endl;
    source_pending_[source_id] = false;
    // For level mode, re-evaluate (signal may still be asserted).
    if (MEIGWCTRL[source_id].irq_type.get() == 0) {
        recompute_pending_for_source(source_id);
    }
    reevaluate_arbitration();
    return true;
}

bool el2_pic_model::read_MEIP(unsigned word_idx, uint32_t &value)
{
    // Only words 0..NUM_PEND_WORDS-1 have this callback registered, and
    // pending_word() returns 0 for any word past the last source anyway, so no
    // range guard is needed here.
    value = pending_word(word_idx);
    return true;
}

// =============================================================================
// Callback registration
// =============================================================================

void el2_pic_model::register_all_callbacks()
{
    // Per-source post-write callbacks for MEIPL, MEIE, MEIGWCTRL. We use
    // post_write_callback so the default csml_reg::handle_write still updates
    // the underlying storage. Skip index 0 (reserved per spec).
    for (unsigned i = 1; i < NUM_INTERRUPTS; ++i) {
        memory.register_post_write_callback(
            std::bind(&el2_pic_model::post_write_MEIPL, this, i),
            MEIPL[i].offset);

        memory.register_post_write_callback(
            std::bind(&el2_pic_model::post_write_MEIE, this, i),
            MEIE[i].offset);

        memory.register_post_write_callback(
            std::bind(&el2_pic_model::post_write_MEIGWCTRL, this, i),
            MEIGWCTRL[i].offset);
    }

    // MEIGWCLR is write-only with a side effect; override the default
    // write callback so we don't store anything and instead clear edge state.
    for (unsigned i = 1; i < NUM_INTERRUPTS; ++i) {
        std::function<bool(uint32_t)> write_cb = [this, i](uint32_t value) {
            return this->write_MEIGWCLR(i, value);
        };
        memory.register_write_callback(write_cb, MEIGWCLR[i].offset);

        // Reads return 0 per spec.
        std::function<bool(uint32_t &)> read_cb = [](uint32_t &v) {
            v = 0;
            return true;
        };
        memory.register_read_callback(read_cb, MEIGWCLR[i].offset);
    }

    // MEIP[0..NUM_PEND_WORDS-1] are RO; override the read callback to return the
    // live computed pending bitmap rather than whatever happens to be stored.
    for (unsigned w = 0; w < NUM_PEND_WORDS; ++w) {
        std::function<bool(uint32_t &)> read_cb = [this, w](uint32_t &v) {
            return this->read_MEIP(w, v);
        };
        memory.register_read_callback(read_cb, MEIP[w].offset);

        // Writes to MEIP are ignored (RO to SW).
        std::function<bool(uint32_t)> write_cb = [](uint32_t value) {
            (void)value;
            return false;
        };
        memory.register_write_callback(write_cb, MEIP[w].offset);
    }

    // MPICCFG is plain R/W storage; no callback required. The default
    // csml_reg handle_write registered by the base ctor handles it.
}

} // namespace el2_pic
