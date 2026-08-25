// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// ===========================================================================
// include/iss_hart.h
//
// PURPOSE
// -------
// Pure abstract C++ interface that decouples `smc_cpu_cluster` (the SystemC
// TLM-2.0 wrapper module) from any particular Instruction Set Simulator
// implementation.  The wrapper talks to harts ONLY through this interface;
// the concrete `iss_backend_whisper` (the sole backend in this project)
// implements it in terms of Tenstorrent Whisper's `WdRiscv::Hart` API.
//
// Why an interface?
//   1. Keeps Whisper headers out of `smc_cpu_cluster.h` (build hygiene).
//   2. Makes the wrapper unit-testable with a mock hart.
//   3. Documents the minimal contract a backend must satisfy.
//
// Defined in:  Low-Level Design §3 - CPU Cluster, §A - ISS Integration
// Used by:     smc_cpu_cluster.cpp  (cluster owns N iss_hart pointers)
// Implemented by: iss_backend_whisper.cpp
// ===========================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace smc {

// ---------------------------------------------------------------------------
// Reset configuration passed to a backend at construction time.
// ---------------------------------------------------------------------------
struct iss_hart_config
{
    unsigned    hart_index   = 0;            // index into the system (0..N-1)
    unsigned    num_harts    = 1;            // total harts in the system
    uint64_t    hart_id      = 0;            // value of MHARTID CSR
    uint64_t    reset_pc     = 0x80000000;   // boot vector
    std::string isa          = "rv64imafdc"; // ISA string for Hart::configIsa
    uint64_t    mem_size     = 1ull << 32;   // 4 GiB sparse address space
};

// ---------------------------------------------------------------------------
// Per-step commit log entry (§3.9 "Test-bench / debug API").  Populated by
// step()/step(n).  The wrapper keeps the LAST entry; if a richer history is
// needed (Dromajo / Rocket-Verilator co-sim, §3.10), the consumer copies the
// entry into a side ring after each step.
// ---------------------------------------------------------------------------
struct commit_entry
{
    uint64_t pc         = 0;     // PC of the just-retired instruction
    uint32_t opcode     = 0;     // 32-bit instruction word at that PC
    bool     trapped    = false; // true if instruction trapped (exception/IRQ)
    uint64_t trap_cause = 0;     // MCAUSE (valid only when trapped == true)
    bool     was_wfi    = false; // true if the retired insn was WFI
    uint8_t  priv       = 0;     // 0=U, 1=S, 3=M after retirement
};

// ---------------------------------------------------------------------------
// Abstract hart.  Every method maps 1:1 to a SystemC-side need.
// ---------------------------------------------------------------------------
class iss_hart
{
public:
    virtual ~iss_hart() = default;

    // -- Lifecycle -----------------------------------------------------------
    // Place hart in its post-reset state; PC <- reset_pc, registers cleared.
    virtual void     reset() = 0;

    // -- Execution -----------------------------------------------------------
    // Execute exactly ONE retired instruction.  Updates internal WFI flag if
    // the instruction was `wfi`.  Returns false on fatal ISS error.
    virtual bool     step() = 0;

    // Execute up to N retired instructions; stop early if the hart enters
    // WFI or step() returns false.  Returns the number of instructions
    // actually retired.  Default impl loops on step(); backends MAY
    // override for efficiency (e.g. Whisper amortises some per-call cost).
    virtual unsigned step(unsigned n)
    {
        unsigned retired = 0;
        for (unsigned k = 0; k < n; ++k) {
            if (is_wfi())             break;
            if (!step())              break;
            ++retired;
        }
        return retired;
    }

    // -- Program counter -----------------------------------------------------
    virtual void     set_reset_pc(uint64_t addr) = 0;
    virtual uint64_t get_pc() const = 0;

    // -- Interrupt injection -------------------------------------------------
    // Poke the MIP CSR.  Called by the cluster's IRQ aggregator SC_METHOD
    // whenever any of irq_sw / irq_timer / irq_ext changes on this hart.
    // The implementation uses Whisper's `externalPokeCsr` so the ISS won't
    // overwrite the value during the next singleStep.
    virtual void     poke_mip(uint64_t mip_value) = 0;

    // -- CSR read (debug + IRQ aggregation reads MIE/MSTATUS) ----------------
    virtual uint64_t read_csr(uint32_t csr_number) const = 0;

    // -- Current privilege mode (RISC-V: 0=U, 1=S, 3=M) ----------------------
    // Used by smc_axi_extension to set prot[2] on each emitted transaction.
    // Returns the live privilege mode, NOT the snapshot from last_commit().
    virtual uint8_t  current_priv() const = 0;

    // -- WFI handling --------------------------------------------------------
    // True iff the last `step()` retired a WFI and the hart should now park.
    virtual bool     is_wfi() const = 0;

    // Clear the WFI flag (called when MIP becomes non-zero so the cluster
    // can resume calling step()).
    virtual void     clear_wfi() = 0;

    // -- NMI injection (§3.7 BEU NMI line, §3.9 inject_nmi debug API) -------
    // Cause = 0 maps to Whisper's generic NMI; non-zero is propagated
    // verbatim into the pendingNmis_ set.  No effect if the hart is in
    // debug mode.  Wakes a parked hart (clears WFI).
    virtual void     inject_nmi(uint64_t cause = 0) = 0;

    // -- Commit log accessor (§3.9 commit_log()) -----------------------------
    // Returns the entry populated by the most recent step()/step(n).  Valid
    // until the next step() call.  When step(n) > 1 retires multiple
    // instructions, the entry refers to the LAST retired instruction; the
    // consumer is expected to drive the loop one step at a time if it
    // needs every entry (Dromajo / Rocket-Verilator co-sim use case).
    virtual const commit_entry& last_commit() const = 0;

    // -- Firmware loading (called once before reset) ------------------------
    // Loads one or more ELF files into the simulated memory.  Returns false
    // if any file failed to load.
    virtual bool     load_elf(const std::vector<std::string>& elf_paths) = 0;

    // -- Memory peek/poke (used by debug master / testbench, not by harts) --
    virtual bool     mem_read (uint64_t addr, unsigned size, uint64_t& data) const = 0;
    virtual bool     mem_write(uint64_t addr, unsigned size, uint64_t  data) = 0;
};

} // namespace smc
