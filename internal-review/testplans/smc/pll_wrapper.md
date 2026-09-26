# SMC `pll_wrapper` test audit and remediation plan

## Scope and audit basis

Static audit only; no model or test changes were made.

Audited model files:

- `smc/peripherals/pll_wrapper/include/pll_wrapper.h`
- `smc/peripherals/pll_wrapper/include/pll_reg_block.h`
- `smc/peripherals/pll_wrapper/include/pll_cntl.h`
- `smc/peripherals/pll_wrapper/include/cgm.h`
- `smc/peripherals/pll_wrapper/include/awm.h`
- `smc/peripherals/pll_wrapper/src/pll_wrapper.cpp`
- `smc/peripherals/pll_wrapper/src/pll_reg_block.cpp`
- `smc/peripherals/pll_wrapper/src/pll_cntl.cpp`
- `smc/peripherals/pll_wrapper/src/cgm.cpp`
- `smc/peripherals/pll_wrapper/src/awm.cpp`

Audited test/build files:

- `smc/peripherals/pll_wrapper/test/pll_wrapper_tb.cpp`
- `smc/peripherals/pll_wrapper/test/CMakeLists.txt`
- `smc/peripherals/pll_wrapper/CMakeLists.txt`
- `smc/peripherals/pll_wrapper/run_tests.sh`
- `smc/peripherals/pll_wrapper/doc/test_plan.adoc`
- `smc/scripts/enforce_line_coverage.sh`

## Behavior inventory

- Wrapper routes one target socket to five child register blocks: PLL control, two CGMs, and two AWMs.
- Child windows are rebased and forwarded using the original generic payload, then the address is restored.
- Shared `reg_block` accepts naturally aligned 1/2/4-byte transactions contained in one 32-bit register.
- Table-driven masks implement RW, RO/WI, RAZ, and self-clearing fields; unmapped in-child offsets are RAZ/WI.
- AWM table tiles global, six frequency, and three CGM sub-block layouts.
- Reset restores all table entries from reset arrays.
- Public/protected back doors support raw peek/poke, callback replacement, and write observers.
- Wrapper observers convert CGM/AWM REG_UPDATE writes into synthetic lock status updates.
- Child access delays are mutable CCI parameters; wrapper itself adds no additional delay.
- No `transport_dbg`, DMI callback, or AXI sideband-specific handling is implemented.

## Existing test classification matrix

| Area | Existing coverage | Classification |
|---|---|---|
| Reset | Representative defaults in all child types and repeated reset | Semantic, sampled rather than exhaustive |
| Register masks | Representative PLL/CGM/AWM RW and RO | Semantic, sampled |
| Strobes | GPIO singlepulse and AWM REG_UPDATE readback | Semantic; CGM sample strobe omitted |
| Routing | All child families, independence, inter-child gaps | Semantic |
| Sub-word | 8/16/32-bit lanes and low-half RMW preservation | Good baseline |
| Lock behavior | Firmware-style CGM/AWM REG_UPDATE and polling | Semantic but synthetic/immediate |
| TLM negative | Null, zero, invalid width/alignment, BE, SW, command, gaps | Good baseline |
| CCI | Child handle discovery/mutation | Value only; exact delay not asserted |
| Debug/callbacks | Poke status, subclass exposes protected callbacks, miss paths, dump | Explicit coverage shortcuts |
| Build/coverage | Five model sources aggregated into one gate | Per-file weakness can be hidden |

## Evidence findings

### High

1. **The model does not include or inspect the canonical AXI extension.**  
   Neither wrapper nor shared register block includes `smc_axi_extension.h`; forwarding preserves any attached extension only accidentally because the same payload object is reused (`pll_wrapper.cpp:150-166`). No test attaches sideband. Add the canonical include/inspection at the SMC-facing target and verify field preservation through every child route and error gap.

2. **UBSan is absent.**  
   `CMakeLists.txt:27-31` enables only AddressSanitizer.

3. **Coverage is gated only on the aggregate of five source files.**  
   `run_tests.sh:251-258` lists all five model sources plus the test, while `enforce_line_coverage.sh:46-81` computes one aggregate `src/` percentage. A poorly covered `awm.cpp`, lock observer, or routing file can be hidden by heavily executed table/base code. Enforce ≥95% per touched model source and include relevant inline headers.

4. **Large register tables are only sampled, so map transcription errors can survive with high line coverage.**  
   The PLL control table has 22 entries (`pll_cntl.cpp:29-52`), CGM 23 entries (`cgm.cpp:20-43`), and AWM dynamically tiles many entries (`awm.cpp:32-156`). Tests validate a small subset. Declarative table lines execute during construction, inflating coverage without proving each offset/reset/rmask/wmask/self-clear contract.

### Medium

5. **Tests deliberately bypass sockets and replace internal callbacks to raise coverage.**  
   `probe_cntl` exposes protected callback hooks (`pll_wrapper_tb.cpp:192-205`); sections 19–20 install arbitrary callbacks and test miss paths (`:548-615`). Direct `poke` is also used as a hardware oracle (`:340-345`). These verify debug infrastructure, not PLL architectural behavior, and should be classified structural.

6. **The checked-in test plan overstates strobe coverage.**  
   It claims “REG_UPDATE / SAMPLE_STROBE / postdiv” coverage, but the test checks GPIO postdiv and AWM REG_UPDATE (`pll_wrapper_tb.cpp:293-306`), not CGM `SAMPLE_STROBE` at `cgm.cpp:31`. Add every self-clearing field, including CGM REG_UPDATE and SAMPLE_STROBE and both AWM REG_UPDATE bits under 8/16/32-bit writes.

7. **Exact annotated delay is not tested.**  
   CCI mutation is checked only as JSON (`pll_wrapper_tb.cpp:357-371`). Assert incoming delay plus exactly one child delay, ensure wrapper does not double-count, verify live mutation per child, and test gap/error delay policy.

8. **Wrapper route boundary tests are sparse.**  
   Gaps 0x300, 0x900, and 0xf00 are covered, but exact first/last bytes of each child, sub-word accesses at child ends, 0x0fff, 0x1000, `UINT64_MAX`, and attempts that begin inside a child but extend beyond it need explicit checks.

9. **Address-overflow-safe bounds are not used in `reg_block`.**  
   The check `adr + len > window_size_` (`pll_reg_block.cpp:153-158`) can wrap for very large addresses. Wrapper routing comparisons avoid addition with payload length and may route a huge-overflow shape only by base compare; test and harden both layers.

10. **`transport_dbg` and DMI contracts are absent.**  
    Only blocking transport is registered in both wrapper and child (`pll_wrapper.cpp:51`, `pll_reg_block.cpp:36`). Add explicit tests for unsupported debug transfer/DMI behavior, or implement side-effect-free debug reads if required by platform tooling.

11. **Byte-enable handling is all-or-nothing and not tested for pointer+zero length.**  
    `pll_reg_block.cpp:161-164` rejects any non-null BE pointer. Tests cover one pointer/length pair. Add BE length 0/1/2/4 and clarify whether full enabled lanes should be accepted.

12. **Streaming-width behavior is incomplete.**  
    The target accepts any `streaming_width >= len` (`pll_reg_block.cpp:166-169`). Tests cover only less-than and equal. Add zero and greater-than for each width; document the TLM2 rationale.

13. **Sub-word register contract needs broader independent testing.**  
    Existing tests cover AG_MUX low-half and one byte lane. Missing all four byte lanes, both halfwords, RO partial writes, reserved masks, self-clear strobe in upper/lower lane, and unmapped word lanes.

14. **Synthetic lock behavior is immediate and under-constrained.**  
    CGM lock follows ENABLES bit0 immediately on REG_UPDATE (`pll_wrapper.cpp:94-121`); AWM lock becomes fixed 7/1 regardless of configuration (`:124-148`). Tests prove polling terminates but not whether required enable/config combinations, unlock causes, reset, or timing match firmware/RTL expectations.

15. **Observer semantics can react to writes that the register itself masks/self-clears.**  
    Observers receive post-lane-merge, pre-self-clear `data` (`pll_reg_block.cpp:191-210`). This is intentional for strobes, but tests should cover partial writes to untouched lanes and prove stale bits cannot trigger a lock.

16. **No reset-during-access/lock transition scenario exists.**  
    Reset is an SC_METHOD on each child. Test reset asserted with lock set, during same delta as REG_UPDATE, and repeated reset while CCI/register accesses occur.

17. **The wrapper does not itself validate command/pointer/length before route.**  
    It routes based only on starting address (`pll_wrapper.cpp:150-172`) and relies on children. This is acceptable if every routed target enforces the same contract, but tests should assert address restoration and payload response for all error shapes and boundaries.

### Low

18. **CTest failure regex is narrower than the harness summary.**  
    `test/CMakeLists.txt:7-10` only matches lines beginning `FAIL `. Process exit is still authoritative, but a consistent failure regex is preferable.

19. **No report suppression was found.**  
    This is positive relative to several sibling IPs.

20. **No private-to-public macro was found.**  
    The subclass exposing protected callbacks is still a white-box shortcut, but it is explicit and type-safe.

## Missing scenario inventory

### TLM2, sideband, address span

- Canonical extension attached/absent through all five routes and decode gaps.
- Non-zero delay in, exact one-child delay out, child CCI mutation, no double annotation.
- Null, zero, invalid widths, all alignments, all streaming relations, BE lengths, stale response/DMI.
- Exact child starts/ends; accesses whose length crosses child window; 0xfff/0x1000/`UINT64_MAX`.
- Address restored after both successful and failing forwarded transactions.
- Debug and DMI policy.

### Register tables

- Data-driven independent expected manifest for every PLL control, CGM, and AWM register: offset, reset, read mask, write mask, self-clear mask, access width.
- First/last register and every documented hole in each child.
- All six AWM frequency instances and all three AWM CGM instances; independence and stride.
- Every byte/halfword lane on RW, RO, self-clear, and hole entries.
- Every reset value after dirtying all writable registers, not a representative subset.

### Lock, timing, reset

- CGM0/1 enabled/disabled combinations and bit0-clear REG_UPDATE.
- AWM0/1 required configuration/enable states, both REG_UPDATE bits, lock clear/unlock policy.
- Same-delta reset versus REG_UPDATE and repeated programming.
- Any intended lock acquisition delay/event; firmware polling timeout behavior.
- Status mirroring in both PLL control and child block across reset/reprogram.

## Proposed test matrix

| ID | Layer | Scenario | Oracle |
|---|---|---|---|
| PLL-TLM-01 | Wrapper | Full malformed payload matrix per route/gap | Exact response, restored address, delay |
| PLL-TLM-02 | Wrapper | Canonical AXI extension forwarding | All fields unchanged at child monitor |
| PLL-TLM-03 | Wrapper/child | DMI/debug policy | Explicit denial or side-effect-free contract |
| PLL-MAP-01 | MMIO | Generated independent manifest for every register | Literal RDL-derived reset/rmask/wmask/sc |
| PLL-MAP-02 | MMIO | Every child/hole boundary and AWM stride | No overlap/alias, correct gap fault |
| PLL-LANE-01 | MMIO | 1/2/4-byte lane matrix by access type | Independent lane/mask expectation |
| PLL-STROBE-01 | MMIO | Every singlepulse/self-clear field | Immediate observer effect, readback zero |
| PLL-LOCK-01 | MMIO | CGM/AWM configuration and REG_UPDATE matrix | Both mirrored status registers |
| PLL-TIME-01 | CCI/MMIO | Per-child mutable delay and no wrapper double count | Exact annotation |
| PLL-RST-01 | Kernel | Dirty-all reset and reset concurrent with strobe | Complete reset image/no stale lock |
| PLL-BUILD-01 | Coverage | Per-source/header threshold | Every touched model file ≥95% |
| PLL-BUILD-02 | Sanitizer | Test-only UB canary | UBSan fails |

## Verdict

**Good representative register-fabric test, weak exhaustive register-model proof.** Routing, sub-word access, immediate lock polling, and malformed payload basics are covered. High coverage is misleading because construction executes large declarative tables while only a small register subset is semantically checked, and the aggregate gate can hide weak files. Add canonical AXI sideband coverage, UBSan, exact timing, exhaustive independent RDL-manifest tests, boundary/overflow cases, and separate structural callback/back-door tests from architectural claims.
