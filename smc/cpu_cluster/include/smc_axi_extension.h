// ===========================================================================
// include/smc_axi_extension.h
//
// PURPOSE
// -------
// TLM-2.0 ignorable extension carried on every transaction issued by
// `smc_cpu_cluster`.  Encodes the AXI-side metadata that the SMC fabric and
// downstream filters need to make routing / access-control decisions:
//
//   source_id   -- the SMC-wide initiator ID (constant per cluster instance,
//                  configurable; defaults to SMC_CPU_SOURCE_ID = 0x10).
//   axi_id      -- transaction ID.  We use the hart index, so MMIO peers can
//                  identify the originating hart for diagnostics.
//   prot[0]     -- 1 == data, 0 == instruction (= !is_fetch).
//   prot[1]     -- 1 == non-secure, 0 == secure.  We do not yet model
//                  TrustZone-style secure mode in SMC, so this is hard-wired
//                  to 1 today.
//   prot[2]     -- 1 == privileged (M or S), 0 == user (U).  Driven from
//                  `iss_hart::privilegeMode()` at the time the access is
//                  emitted.  Tracks the spec'd
//                  `prot[2] = (mstatus.MPRV ? mstatus.MPP : priv) != U`.
//   prot[3]     -- 1 == locked (atomic).  Set for AMO / LR-SC sequences so
//                  the fabric treats them as exclusive.
//
// Per the §3.10 Modeling notes ("Atomicity: Spike's AMO/LR-SC sequences are
// issued as a single TLM transaction with prot[3]=1"); the wrapper sets the
// bit when the in-flight access originated from an AMO instruction.
//
// This is a `tlm_extension` (ignorable): targets that do not understand it
// just see a standard generic_payload.  The fabric / filter modules retrieve
// it via `trans.get_extension<smc_axi_extension>()`.
//
// Shared with every SMC IP; PLIC pulls this in via smc_tlm_extensions.h.
// ===========================================================================

#pragma once

#include <tlm.h>

#include <cstdint>

namespace smc {

class smc_axi_extension : public tlm::tlm_extension<smc_axi_extension>
{
public:
    static constexpr uint16_t SMC_CPU_SOURCE_ID = 0x10;

    uint16_t source_id  = SMC_CPU_SOURCE_ID;
    uint16_t axi_id     = 0;        // hart index by default
    uint8_t  prot       = 0b0111;   // [3]=lock=0, [2]=priv=1, [1]=ns=1, [0]=data=1
    bool     is_locked  = false;    // mirror of prot[3], convenience
    bool     is_fetch   = false;    // mirror of !prot[0]
    bool     is_secure  = false;    // mirror of !prot[1]
    bool     is_user    = false;    // mirror of !prot[2]

    smc_axi_extension() = default;

    // Required tlm_extension overrides ---------------------------------------
    tlm::tlm_extension_base* clone() const override
    {
        return new smc_axi_extension(*this);
    }
    void copy_from(const tlm::tlm_extension_base& other) override
    {
        *this = static_cast<const smc_axi_extension&>(other);
    }

    // Convenience setters that keep prot[*] and the booleans coherent.
    void set_priv(bool priv)
    {
        is_user = !priv;
        if (priv) prot |=  (1u << 2);
        else      prot &= ~(1u << 2);
    }
    void set_secure(bool secure)
    {
        is_secure = secure;
        if (secure) prot &= ~(1u << 1);
        else        prot |=  (1u << 1);
    }
    void set_fetch(bool fetch)
    {
        is_fetch = fetch;
        if (fetch) prot &= ~(1u << 0);
        else       prot |=  (1u << 0);
    }
    void set_locked(bool locked)
    {
        is_locked = locked;
        if (locked) prot |=  (1u << 3);
        else        prot &= ~(1u << 3);
    }
};

}  // namespace smc
