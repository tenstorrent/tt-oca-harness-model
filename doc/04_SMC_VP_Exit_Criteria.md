# SMC Virtual Platform — Exit Criteria

**Document scope.** This document defines the binary, auditable conditions under which the SMC SystemC/TLM-2.0 Virtual Platform (VP) is declared *complete* and released to firmware, post-silicon, and software-validation consumers as `smc-vp-1.0.0`. Each criterion is paired with a hard pass condition and the artefact that proves it.

**Companion documents.**

- `01_SMC_Architecture.md` — architectural reference, IP list, modeling priority (P1/P2/P3).
- `02_SMC_IP_LowLevel_Design.md` — per-IP design, ISS integration, milestones M1–M4 (Appendix A §A.14).
- `03_SMC_Test_Plan.md` — test cases, coverage targets, regression tiers.

**Release rule.** The VP is released only when **all five gates** below are simultaneously green for two consecutive weekly regressions. Any gate failing demotes the candidate to the previous tag.

---

## Gate Summary

| Gate | Theme | # of criteria | Owner |
|---|---|---|---|
| 1 | Functional completeness (model surface) | 5 | Modeling Lead |
| 2 | Verification quality | 5 | Verification Lead |
| 3 | Performance & usability | 5 | Modeling Lead |
| 4 | Deliverables & process | 6 | Project Lead |
| 5 | Negative criteria (explicitly accepted limits) | 6 | Architecture Lead |

---

## Gate 1 — Functional Completeness

| ID | Criterion | Pass Condition | Evidence |
|---|---|---|---|
| F1 | All P1 IPs implemented | All 16 P1 IPs (Fabric, AXI Filter, CPU Cluster, PLIC, CLINT, Mailbox, OCTS, eFuse, UART, I2C, AVSBus, Reset Unit, Boot ROM, Scratchpad SRAM, Log Engine, I3C[0]) instantiate inside `smc_top` without `SC_REPORT_ERROR` | `make smc_top` clean build log |
| F2 | All P2 IPs implemented | DMA, Memory Zeroer, GPIO, Telemetry, PLL, MISC, Debug Module, WDT, BEU, PVT, I3C[1..5] instantiate and pass unit tests | Tier-1 unit-test report |
| F3 | Memory map matches HW spec | Every range in `01_SMC_Architecture.md` §2.2 routed by Fabric; no aliases, no gaps | `TC-FAB-MMAP-NNN` green |
| F4 | Register surface complete | Every register in test plan §B.1–B.25 responds with correct reset value and supports R/W as specified | Coverage report: 100 % register coverage |
| F5 | Cluster bring-up M4 reached | LLD §A.14 milestones M1 → M2 → M3 → M4 all green | M1–M4 milestone sign-off reports |

---

## Gate 2 — Verification Quality

| ID | Criterion | Pass Condition | Evidence |
|---|---|---|---|
| V1 | Unit-test pass rate | 100 % of `TC-<IP>-NNN` tests in test plan §B pass | Tier-1 nightly green for 7 consecutive days |
| V2 | SoC integration tests pass | 100 % of `TC-SOC-*` tests in test plan §C pass (Boot, FLR, IRQ, DMA, OCTS, Security, Logging, Telemetry, Stress) | Tier-2 weekly green for 2 consecutive weeks |
| V3 | Coverage thresholds met | Line ≥ 95 %; Branch ≥ 90 %; Register = 100 %; Reset values = 100 %; IRQ-source = 100 %; Functional-coverage scenarios = 100 % | `coverage/index.html` — all bars green |
| V4 | Long-soak clean | Tier-3 (8 h random + error injection + OCTS sync drift) with zero unexplained failures | Tier-3 log + triage notes |
| V5 | Commit-log diff vs Rocket-Verilator | Lockstep run of three sign-off firmware images (boot, FLR, eFuse-read) shows zero ISA-level divergence (allow-listed Rocket-vs-Spike CSR deltas excluded) | `tools/diff_commits.py` report (LLD §A.12) |

---

## Gate 3 — Performance & Usability

| ID | Criterion | Pass Condition | Evidence |
|---|---|---|---|
| P1 | Boot wall-clock | Boot to `OCCP_READY` ≤ 30 s on a single x86_64 host core in `SMC_TD_MODE=fast` | `bench/boot_time.csv` |
| P2 | Throughput | ≥ 50 MIPS aggregate across 4 Rocket harts at quantum = 1000 instructions | `bench/mips.csv` |
| P3 | IRQ-latency budget | Mean PLIC source → trap-entry ≤ 2 µs in fast mode; ≤ 50 ns in lockstep mode | `TC-IRQ-LAT-NNN` report |
| P4 | Determinism | Same firmware + seed produces byte-identical commit log on Linux x86_64 and macOS arm64 | `tools/replay_diff.py` report |
| P5 | Build hygiene | Zero `-Wall -Wextra -Wpedantic` warnings; zero new TSan/UBSan findings; zero new clang-tidy critical findings | CI build matrix |

---

## Gate 4 — Deliverables & Process

| ID | Criterion | Pass Condition | Evidence |
|---|---|---|---|
| D1 | Documentation in sync | Architecture, LLD and Test Plan PDFs regenerated; every IP has a §5.x description and a §B.x test plan | PDFs dated within ≤ 7 days of candidate tag |
| D2 | API stability declared | `iss_hart`, `smc_axi_extension`, public sockets of `smc_top` carry `SMC_VP_API_VERSION = "1.0"` and a documented backwards-compat policy | `include/smc/version.h` + CHANGELOG |
| D3 | Spike submodule pinned | `external/spike` SHA recorded in lockfile; local patches in `external/spike-cmake/patches/` carry `Upstream-Status:` headers (LLD §A.15) | Lockfile + patch headers |
| D4 | Reproducible build | Clean checkout on Ubuntu 24.04 / macOS 14: `cmake -S . -B build && cmake --build build` produces passing Tier-0 in < 5 min | CI pipeline `smc-vp-bootstrap.yml` green |
| D5 | Firmware sign-off | Three sign-off firmware images (`smc_boot.elf`, `smc_flr.elf`, `smc_telem.elf`) reach `tohost = PASS` under the VP | `firmware/signoff/results.json` |
| D6 | Handoff package | Tagged release `smc-vp-1.0.0` containing source, PDFs, coverage report, sign-off firmware results, performance numbers, known-issues list | Git tag + release notes |

---

## Gate 5 — Negative Criteria (Explicitly Out of Scope)

The VP exit can be granted with these limitations explicitly recorded; they are *non-goals* for the v1.0.0 release.

| ID | Out-of-scope item | Rationale |
|---|---|---|
| N1 | Cycle-accurate fabric arbitration | LT abstraction; AT and AT-with-TD are non-goals |
| N2 | RTL-level CDC, X-propagation, low-power gating | Domain of the RTL sim, not the VP |
| N3 | Physical-layer behaviour of I2C / I3C / AVS PHYs | Modelled at byte boundaries only |
| N4 | Spike interactive debug mode + big-endian builds | Off by default; require separate gates if ever enabled |
| N5 | I3C DMA mode | `MODE_SELECTOR = PIO` only (LLD §26) |
| N6 | SystemC-owned RAM (mode B) for ROM/SRAM as default | Only enabled in dedicated filter/security tests |

---

## Phase-to-Gate Mapping

| Phase | Cluster milestone | Gates closed at end of phase |
|---|---|---|
| Bring-up | M1 — scaffold + Spike PLIC | F1 (P1 only); V1 partial; D1 partial |
| First integration | M2 — SMC TLM PLIC + CLINT | F1 complete; V1 complete; V2 partial |
| Full surface | M3 — SMC Debug Module + P2 IPs | F2, F3, F4 complete; V2 complete |
| Sign-off | M4 — TD fast mode + WFI + commit-log diff | V3, V4, V5; P1–P5; D1–D6 |

---

## Sign-off Sheet

| Role | Name | Signature | Date |
|---|---|---|---|
| Modeling Lead |  |  |  |
| Verification Lead |  |  |  |
| Architecture Lead |  |  |  |
| Project Lead |  |  |  |
| Firmware Lead |  |  |  |

---

*End of document — `smc-vp-1.0.0` exit criteria.*
