# Overview

The `VeeR-ISS` folder is a copy of the git repo: https://github.com/chipsalliance/VeeR-ISS, with the following commit

```
commit e6b4fb17bd9bf15a9df225bea804be663648903a (HEAD -> main, origin/main, origin/HEAD)
Merge: 666c94e 677b491
Author: Karol Gugala <kgugala@antmicro.com>
Date:   Tue Apr 18 15:40:49 2023 +0200

    Merge pull request #34 from antmicro/ci-compilers
    
    Add compiler matrix to the CI
```

# Changes

The following files have been changed to integrate this as a SystemC model into the Tenstorrent SEP SystemC Platform
1. System.cpp - The default read and write  memory callbacks have been commented out
2. Memory.hpp - The read call before write has been commented. The read call is done to book keep the previous value
3. Hart.hpp  - Following have been added as public members:
```
     std::atomic<bool> meiPending_ = false;
    void setExternalInterrupt(bool value)
    {
      meiPending_ = value;
    }

    bool nmiActive_ = false;
    bool isNmiActive() const

	###This has been moved from protected to public:
    void externalInvalidateLr(uint64_t addr, unsigned size)
    {
      std::lock_guard<std::mutex> lock(memory_.lrMutex_);
      memory_.invalidateLrs(addr, size);
      invalidateDecodeCache(addr, size);
    }

   ### API added for getting nmiPc
      URV getNmiPc()
      { 
         return nmiPc_; 
      }
```
   Added `gdbAccessInProgress_` flag with `setGdbAccessInProgress()`/`gdbAccessInProgress()` accessors. This allows the SystemC TLM wrapper to distinguish debugger-originated memory operations (e.g. breakpoint insertion) from normal timed accesses.
4. Hart.cpp
   a. peekCsr() and isInterruptPossible() functions have been edited to include the above boolean flag meiPending_
   b. `nmiActive_` is set to true in `Hart::initiateNmi()` and cleared in `Hart::execMret()`.
5. gdb.cpp
   a. Set/clear `gdbAccessInProgress_` while servicing each GDB remote protocol packet so that debugger memory reads/writes can be routed through `transport_dbg`.
5. Added CMakeLists.txt file for compilation
6. VeeR EL2 and Smepmp CSRs (CsRegs.hpp/.cpp, PmpManager.hpp, Hart.hpp/.cpp)
   a. `mscause` (0x7FF) is implemented with the EL2 4-bit width.
   b. `mseccfg` (0x747) and, on RV32, `mseccfgh` (0x757, reads zero) are defined. MML and MMWP are sticky until reset; RLB can be set only while it is already set or no PMP entry is locked.
   c. While `mseccfg.RLB` is set, locked `pmpcfg` bytes and their `pmpaddr` registers remain writable.
   d. While `mseccfg.MMWP` is set, a machine-mode fetch, load or store that matches no PMP entry raises the corresponding access fault (`mscause` = 8).
   e. `mseccfg.MML` is stored but its rule reinterpretation is not modelled: PMP checks behave as with MML clear.

