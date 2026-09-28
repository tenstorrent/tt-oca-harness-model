// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include <cstdint>

// PrivilegeLevel / MachineMode — mirror of vp/platform/infra/irq_if.h
// so the test binary has no dependency on the VP platform tree.
using PrivilegeLevel = uint32_t;
constexpr uint32_t MachineMode = 0b11;

// Minimal hart stub for el2_pic standalone tests.
// Records every interaction the PIC would have made with a real VeeRISSTlm hart,
// allowing test assertions on claim_id, eip state, and interrupt priority.
struct MockHart {
    bool     eip_asserted   = false;
    uint32_t claim_id       = 0;
    uint32_t meicidpl       = 0;  // mirrors poke_csr(0xBCB, eff_prio)

    // Settable so tests can program the priority-threshold CSRs, matching
    // what real firmware must (re)write when switching priord modes — the
    // real VeeR ISS doesn't implement these, so they default to 0, same as
    // an un-programmed hart.
    uint32_t meipt          = 0;  // el2_pic_ctrl.sv's Priority Threshold input, CSR 0xBC9
    uint32_t meicurpl_csr   = 0;  // el2_pic_ctrl.sv's Current Priority Level input, CSR 0xBCC

    // Call tallies, not just final state. A claim that retargets from one
    // source to another must not pass through a deasserted EIP on the way, and
    // the end state alone cannot distinguish "never dropped" from
    // "dropped and raised again inside the same delta".
    unsigned trigger_count = 0;
    unsigned clear_count   = 0;
    unsigned claim_writes  = 0;

    void trigger_external_interrupt(PrivilegeLevel /*level*/) { eip_asserted = true; ++trigger_count; }
    void clear_external_interrupt(PrivilegeLevel /*level*/)   { eip_asserted = false; claim_id = 0; ++clear_count; }
    void set_pic_claim_id(uint32_t id)                        { claim_id = id; ++claim_writes; }
    bool peek_csr(uint32_t csr_num, uint32_t &val) {
        val = (csr_num == 0xBC9) ? meipt : (csr_num == 0xBCC) ? meicurpl_csr : 0;
        return true;
    }
    bool poke_csr(uint32_t /*csr_num*/, uint32_t val)         { meicidpl = val; return true; }

    void reset() {
        eip_asserted = false; claim_id = 0; meicidpl = 0; meipt = 0; meicurpl_csr = 0;
        trigger_count = 0; clear_count = 0; claim_writes = 0;
    }
};
