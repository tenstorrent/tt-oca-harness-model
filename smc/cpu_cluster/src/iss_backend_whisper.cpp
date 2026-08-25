// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// src/iss_backend_whisper.cpp
//
// PURPOSE
// -------
// Implementation of `smc::iss_backend_whisper`.  This is the ONE place where
// SMC code touches Whisper APIs directly; everything else in the SMC build
// only ever sees the abstract `smc::iss_hart`.
//
// Responsibilities (one method, one mapping):
//
//   reset()        -> Hart::reset(false)
//   step()         -> Hart::singleStep(*di_)  + WFI detection
//   set_reset_pc() -> Hart::defineResetPc()
//   get_pc()       -> Hart::peekPc()
//   poke_mip()     -> Hart::externalPokeCsr(MIP, v, false)
//                     (clears wfi_active_ when v != 0)
//   read_csr()     -> Hart::peekCsr(CsrNumber{n})
//   is_wfi() / clear_wfi() -> local flag set by step() / cleared by IRQ
//   load_elf()     -> System::loadElfFiles + System::findElfSymbol
//   mem_read/write -> Hart::peekMemory / Hart::pokeMemory  (debug path)
//
// NOTE: load/store hot path is NOT here -- it goes through Whisper's read/
// write memory callbacks registered by `smc_cpu_cluster` directly on the
// `WdRiscv::System`.
// ===========================================================================

#include "iss_backend_whisper.h"

#include <stdexcept>

// Whisper headers (heavy -- only this TU pays for them).
#include "System.hpp"
#include "Hart.hpp"
#include "DecodedInst.hpp"
#include "CsRegs.hpp"
#include "InstId.hpp"
#include "PmaManager.hpp"
#include "Memory.hpp"

namespace smc {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
iss_backend_whisper::iss_backend_whisper(WdRiscv::System<uint64_t>& sys,
                                        const iss_hart_config&     cfg)
    : sys_(sys)
    , cfg_(cfg)
{
    hart_ = sys_.ithHart(cfg_.hart_index);
    if (!hart_) {
        throw std::runtime_error( // LCOV_EXCL_LINE
            "iss_backend_whisper: invalid hart index " +
            std::to_string(cfg_.hart_index));
    }

    // Configure ISA. updateMisa=true so MISA reset value reflects the string.
    hart_->configIsa(cfg_.isa, /*updateMisa*/ true);

    // Boot vector takes effect at the next reset() call.
    hart_->defineResetPc(cfg_.reset_pc);

    // Open the entire memory map as fully-attributed memory so loads,
    // stores, fetches and atomics all pass PMA checks.  The SMC fabric
    // does its own access control downstream; PMA is only the ISS's
    // first-line filter.  Pma::Attrib::Default == Read | Write | Exec |
    // Idempotent | Amo | Rsrv | MisalOk -- everything we need.  We then
    // additionally enable Cacheable so the MEM_CALLBACKS path is taken
    // (Whisper's non-cacheable path can short-circuit some accesses).
    WdRiscv::Pma pma;
    pma.enable(WdRiscv::Pma::Attrib::Default);
    pma.enable(WdRiscv::Pma::Attrib::Cacheable);
    hart_->definePmaRegion(/*ix*/ 0, /*low*/ 0,
                           /*high*/ cfg_.mem_size - 1, pma);

    // Reusable scratch DecodedInst -- avoids per-step heap traffic.
    di_ = std::make_unique<WdRiscv::DecodedInst>();
}

iss_backend_whisper::~iss_backend_whisper() = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void iss_backend_whisper::reset()
{
    hart_->reset(/*resetMemoryMappedRegisters*/ false);
    wfi_active_ = false;
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------
bool iss_backend_whisper::step()
{
    if (wfi_active_) {
        // A pending enabled interrupt may have redirected PC to mtvec while
        // wfi_active_ is still true (IRQ arrived between WFI retire and park).
        const uint64_t mstatus = hart_->peekCsr(WdRiscv::CsrNumber::MSTATUS, true);
        const uint64_t mip     = hart_->peekCsr(WdRiscv::CsrNumber::MIP, true);
        const uint64_t mie     = hart_->peekCsr(WdRiscv::CsrNumber::MIE, true);
        if ((mstatus & 8u) != 0u && (mip & mie) != 0u) {
            wfi_active_ = false; // LCOV_EXCL_LINE — requires MIP pending before poke_mip clears wfi
        } else {
            // Caller is expected to park us; do nothing.
            return true;
        }
    }

    hart_->singleStep(*di_);

    // Populate the commit-log entry (§3.9).  lastPc() is the PC of the
    // just-retired instruction; di_->inst() returns its 32-bit encoding.
    last_commit_.pc         = hart_->lastPc();
    last_commit_.opcode     = di_->inst();
    last_commit_.trapped    = hart_->lastInstructionTrapped();
    last_commit_.trap_cause = last_commit_.trapped ? hart_->lastTrapCause() : 0;
    last_commit_.priv       = static_cast<uint8_t>(hart_->privilegeMode());

    // WFI detection: the just-retired instruction was wfi.  We rely on the
    // decoded-instruction id rather than a PC-stall heuristic, because
    // Whisper's default wfiTimeout_ == 1 lets WFI advance the PC instead
    // of stalling on it.  Traps (including IRQ vectors) clear the WFI park.
    const bool retired_wfi = (di_->instId() == WdRiscv::InstId::wfi);
    if (last_commit_.trapped) {
        wfi_active_ = false;
    } else if (retired_wfi) {
        wfi_active_ = true;
    }
    last_commit_.was_wfi = retired_wfi;
    return true;
}

unsigned iss_backend_whisper::step(unsigned n)
{
    unsigned retired = 0;
    for (unsigned k = 0; k < n; ++k) {
        if (wfi_active_)         break;
        if (!step())             break; // LCOV_EXCL_LINE step() always returns true today
        ++retired;
    }
    return retired;
}

// ---------------------------------------------------------------------------
// Program counter
// ---------------------------------------------------------------------------
void iss_backend_whisper::set_reset_pc(uint64_t addr)
{
    hart_->defineResetPc(addr);
}

uint64_t iss_backend_whisper::get_pc() const
{
    return hart_->peekPc();
}

// ---------------------------------------------------------------------------
// Interrupt injection
// ---------------------------------------------------------------------------
void iss_backend_whisper::poke_mip(uint64_t mip_value)
{
    // externalPokeCsr sets an internal "externally driven" flag so Whisper
    // won't recompute MIP from its own ACLINT/CLINT model on the next step.
    hart_->externalPokeCsr(WdRiscv::CsrNumber::MIP, mip_value,
                           /*virtMode*/ false);
    if (mip_value != 0) {
        wfi_active_ = false;
    }
}

// ---------------------------------------------------------------------------
// CSR read
// ---------------------------------------------------------------------------
uint64_t iss_backend_whisper::read_csr(uint32_t csr_number) const
{
    return hart_->peekCsr(static_cast<WdRiscv::CsrNumber>(csr_number),
                          /*quiet*/ true);
}

uint8_t iss_backend_whisper::current_priv() const
{
    return static_cast<uint8_t>(hart_->privilegeMode());
}

// ---------------------------------------------------------------------------
// WFI flag
// ---------------------------------------------------------------------------
bool iss_backend_whisper::is_wfi()  const { return wfi_active_; }
void iss_backend_whisper::clear_wfi()     { wfi_active_ = false; }

// ---------------------------------------------------------------------------
// NMI injection (§3.7 BEU-NMI line, §3.9 inject_nmi debug API).
// Whisper's setPendingNmi pushes onto an internal pending set; the next
// step() will dispatch to nmiPc_.  Wakes a parked hart.
// ---------------------------------------------------------------------------
void iss_backend_whisper::inject_nmi(uint64_t cause)
{
    hart_->setPendingNmi(static_cast<uint64_t>(cause));
    wfi_active_ = false;
}

// ---------------------------------------------------------------------------
// ELF loading
// ---------------------------------------------------------------------------
bool iss_backend_whisper::load_elf(const std::vector<std::string>& elf_paths)
{
    if (!sys_.loadElfFiles(elf_paths, /*raw*/ false, /*verbose*/ false)) {
        return false;
    }

    WdRiscv::ElfSymbol sym;
    if (sys_.findElfSymbol("tohost",   sym)) tohost_addr_   = sym.addr_;   // LCOV_EXCL_LINE
    if (sys_.findElfSymbol("fromhost", sym)) fromhost_addr_ = sym.addr_; // LCOV_EXCL_LINE
    return true;
}

// ---------------------------------------------------------------------------
// Memory peek/poke (debug master path; goes through MEM_CALLBACKS in Whisper
// so the SMC fast-mem buffer / TLM bus see every access uniformly).
// ---------------------------------------------------------------------------
bool iss_backend_whisper::mem_read(uint64_t a, unsigned s, uint64_t& d) const
{
    switch (s) {
    case 1: { uint8_t  v = 0; bool ok = hart_->peekMemory(a, v, /*usePma*/false); d = v; return ok; }
    case 2: { uint16_t v = 0; bool ok = hart_->peekMemory(a, v, false);           d = v; return ok; }
    case 4: { uint32_t v = 0; bool ok = hart_->peekMemory(a, v, false);           d = v; return ok; }
    case 8: { uint64_t v = 0; bool ok = hart_->peekMemory(a, v, false);           d = v; return ok; }
    default:                                                                          return false;
    }
}

bool iss_backend_whisper::mem_write(uint64_t a, unsigned s, uint64_t d)
{
    switch (s) {
    case 1: return hart_->pokeMemory(a, uint8_t (d), /*usePma*/ false);
    case 2: return hart_->pokeMemory(a, uint16_t(d), false);
    case 4: return hart_->pokeMemory(a, uint32_t(d), false);
    case 8: return hart_->pokeMemory(a, uint64_t(d), false);
    default: return false;
    }
}

} // namespace smc
