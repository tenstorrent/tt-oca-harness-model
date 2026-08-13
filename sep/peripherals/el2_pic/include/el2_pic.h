/**
 * @file el2_pic.h
 * @brief VeeR EL2 PIC SystemC TLM model.
 *
 * The PIC sits between peripheral interrupt outputs and the VeeR EL2 hart.
 * Its job is:
 *   1. Apply per-source gateway rules (polarity + level/edge) to incoming
 *      sc_in<bool> signals → maintain a per-source "pending" bit
 *   2. Arbitrate the highest-priority enabled+pending source whose priority
 *      exceeds the hart's threshold (meipt)
 *   3. When the winner changes, poke the hart's MEIHAP claim_id field and
 *      assert/deassert the external interrupt line
 *
 * The hart, configured with fastExt=true, will then read MEIHAP on the
 * trap and fetch the handler PC from meivt + (claim_id << 2).
 *
 * Note: this model does not implement the priority stack (meipt push/pop
 * on entry/mret) — nested interrupts above threshold are not supported.
 * Single-priority firmware (the OCH SEP tests) is unaffected.
 */
#pragma once
#include "el2_pic_base.h"
#include "csml_parameter.h"
#include "csml_logger.h"

// Forward-declare the ISS wrapper to break the mutual include cycle.
// VeeR-ISSTlm.hpp includes this header, so we cannot include it here.
// el2_pic.cpp includes VeeR-ISSTlm.hpp for the full definition.
class VeeRISSTlm;

namespace el2_pic {

class el2_pic_model : public el2_pic_base {
public:
    SC_HAS_PROCESS(el2_pic_model);

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
    csml_param<int> verbosity;

    el2_pic_model(sc_module_name n);

    // Ports
    sc_in<bool> clk_i;
    sc_in<bool> rst_ni;

    /// Source 0 unused; sources 1..NUM_INTERRUPTS-1. A parent need only bind the
    /// sources it actually drives — the rest are tied low in
    /// before_end_of_elaboration, so widening the PIC does not oblige every
    /// integration to bind all 256.
    sc_vector<sc_in<bool>> irq_in;

    /// Hand the PIC a back-reference to the VeeR-ISSTlm wrapper so it can
    /// (a) set MEIHAP.claim_id via wrapper->set_pic_claim_id()
    /// (b) drive external IRQ via the existing trigger_external_interrupt/clear interface
    void bind_hart(VeeRISSTlm *hart) { hart_ = hart; }

    /// Called by VeeRISSTlm's postCsrInst callback when firmware writes
    /// meipt (0xBC9) or meicurpl (0xBCC), so threshold changes take effect
    /// immediately without waiting for the next gateway event.
    void notify_threshold_changed() { reevaluate_arbitration(); }

protected:
    // sc_spawn for per-source gateways must happen after port binding, so we
    // defer it to before_end_of_elaboration (overridden from sc_module).
    void before_end_of_elaboration() override;

private:
    // SystemC processes
    void reset_process();
    void gateway_changed(unsigned source_id);   ///< sc_spawn'd per source

    // Behavioural register callbacks
    bool post_write_MEIPL(unsigned source_id);
    bool post_write_MEIE(unsigned source_id);
    bool post_write_MEIGWCTRL(unsigned source_id);
    bool write_MEIGWCLR(unsigned source_id, uint32_t value);
    bool read_MEIP(unsigned word_idx, uint32_t &value);

    // Helpers
    void recompute_pending_for_source(unsigned source_id);
    void reevaluate_arbitration();
    void register_all_callbacks();

    /// Compute one meip word from source_pending_. Bit Y of word w is source
    /// w*32+Y. Called per read rather than mirrored into MEIP storage — see the
    /// note in el2_pic.cpp.
    uint32_t pending_word(unsigned w) const;

    // Per-source latched state used by the gateway. For edge mode, this is
    // a sticky bit set whenever the polarity-adjusted level is asserted and
    // cleared by meigwclr[i] only while that level is inactive. For level mode
    // it is just the live polarity-adjusted level.
    std::array<bool, NUM_INTERRUPTS> source_pending_{};

    // Tie-off for sources the parent left unbound. Sized lazily in
    // before_end_of_elaboration, since sc_vector cannot be resized afterwards.
    sc_vector<sc_signal<bool>> irq_tie_low_;

    // Currently-driven external IRQ state (avoid duplicate trigger/clear).
    bool eip_asserted_ = false;
    unsigned current_claim_id_ = 0;

    // Hart back-reference; set via bind_hart() during elaboration.
    VeeRISSTlm *hart_ = nullptr;

    CsmlLogger logger;
};

} // namespace el2_pic
