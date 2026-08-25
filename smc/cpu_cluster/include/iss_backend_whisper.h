// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// include/iss_backend_whisper.h
//
// PURPOSE
// -------
// Header for the *concrete* Whisper-based implementation of the abstract
// `smc::iss_hart` interface.  This class owns:
//
//   * A reference to the shared `WdRiscv::System<uint64_t>` (the multi-hart
//     simulation engine, owned by `smc_cpu_cluster`).
//   * A `std::shared_ptr<WdRiscv::Hart<uint64_t>>` for ONE hart in that
//     system.
//   * Cached `tohost` / `fromhost` symbol addresses (resolved after ELF load).
//   * The `wfi_active_` flag and a reusable `DecodedInst` scratch object.
//
// Whisper-specific includes (Hart.hpp, System.hpp, CsRegs.hpp, ...) are kept
// out of this header by using forward declarations; only the .cpp file pulls
// the heavy Whisper headers in.  This keeps build times manageable for any
// translation unit that just needs the `iss_hart` interface.
//
// Constructed by: smc_cpu_cluster::smc_cpu_cluster()  (one per hart)
// Implements:     smc::iss_hart
// Uses:           Whisper APIs from System.hpp / Hart.hpp / DecodedInst.hpp
// ===========================================================================

#pragma once

#include "iss_hart.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declarations -- avoid pulling Whisper headers into clients.
namespace WdRiscv {
    template <typename URV> class Hart;
    template <typename URV> class System;
    class DecodedInst;
}

namespace smc {

class iss_backend_whisper final : public iss_hart
{
public:
    // Constructed with a reference to the shared System and the hart's config.
    iss_backend_whisper(WdRiscv::System<uint64_t>& sys,
                        const iss_hart_config&     cfg);

    ~iss_backend_whisper() override;

    // -- iss_hart contract ---------------------------------------------------
    void     reset()                                              override;
    bool     step()                                               override;
    unsigned step(unsigned n)                                     override;

    void     set_reset_pc(uint64_t addr)                          override;
    uint64_t get_pc() const                                       override;

    void     poke_mip(uint64_t mip_value)                         override;
    uint64_t read_csr(uint32_t csr_number) const                  override;
    uint8_t  current_priv() const                                 override;

    bool     is_wfi()   const                                     override;
    void     clear_wfi()                                          override;

    void     inject_nmi(uint64_t cause = 0)                       override;

    const commit_entry& last_commit() const                       override
    { return last_commit_; }

    bool     load_elf(const std::vector<std::string>& elf_paths)  override;

    bool     mem_read (uint64_t a, unsigned s, uint64_t& d) const override;
    bool     mem_write(uint64_t a, unsigned s, uint64_t  d)       override;

    // -- Whisper-specific introspection (used by smc_cpu_cluster) -----------
    uint64_t tohost_addr()   const { return tohost_addr_; }
    uint64_t fromhost_addr() const { return fromhost_addr_; }

private:
    // Whisper objects (owned through shared_ptr inside Whisper's System).
    WdRiscv::System<uint64_t>&                 sys_;
    std::shared_ptr<WdRiscv::Hart<uint64_t>>   hart_;

    // Scratch DecodedInst is reused across step() calls to avoid per-step
    // heap traffic -- it's filled by Hart::singleStep().
    std::unique_ptr<WdRiscv::DecodedInst>      di_;

    // Static config snapshot.
    iss_hart_config                            cfg_;

    // Resolved after load_elf().  0 if symbol not present in the ELF.
    uint64_t                                   tohost_addr_   = 0;
    uint64_t                                   fromhost_addr_ = 0;

    // WFI state -- set by step() when the retired insn is `wfi`, cleared by
    // clear_wfi() (which the cluster calls when MIP becomes non-zero).
    bool                                       wfi_active_    = false;

    // Last-retired-instruction snapshot, populated by step()/step(n).
    commit_entry                               last_commit_   {};
};

} // namespace smc
