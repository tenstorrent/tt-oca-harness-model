# Internal-review status

**Source:** `origin/main` @ `9aa2434a` (2026-09-25).
**Method:** re-checked the findings in `testplans/` against current model, test, and runner source. This is not a new static audit of every Medium/Low item.

The original plans still describe the 2026-09-24 audit snapshot. Use this file plus the index in [`testplans/README.md`](testplans/README.md) for the `main` baseline and remediation status.

## P0 remediation branch

Branch `fix/internal-review-p0-20-ips` resolves the must-fix finding for each of the 20 selected IPs. All affected targets pass their separate Release, ASan/UBSan, and coverage runs; coverage meets the repository's 95% line gate. The integrated SEP VP also builds successfully with the new entropy-source → CSRNG → EDN production path.

| IP | Resolved P0 finding |
|---|---|
| SEP HMAC | Failure paths now increment the test failure count before stopping simulation; summaries and process exit fail closed. |
| SEP KMAC | FUNC023/024 execute and propagate verdicts; unconditional-pass TC-166/167 and `const_cast` configuration mutation were removed. |
| SEP Secure DMA | FUNC004/005, all FUNC003 cases, all FUNC006 cases, and basic tests execute in every required mode. |
| SEP CPU control | Removed unconditional `quick_exit(0)`; checks remain active in Release and `sc_main` returns the accumulated verdict. |
| SEP memory | Added a standalone frontdoor suite with malformed-payload, ROM, debug, DMI, and load-file checks; enabled Release/ASan/Coverage and the SEP orchestrator. |
| SEP EDN | Release and ASan now run all 14 suites; production CSRNG command/genbits ports are bound in the SEP VP. |
| SEP SPI flash | Release and ASan now execute both the core and SystemC wrapper tests. |
| SEP SPI controller | Encoded transfer lengths above the 512-byte buffer limit are rejected and regression-tested. |
| SEP CSRNG | Added two hardware-app command exports, genbits export, entropy-provider input, semantic interface tests, and SEP VP bindings. |
| SEP entropy source | Added a production 384-bit entropy-provider export with repetition-limit health scoring and frontdoor tests. |
| SMC DMA | Failed transfers no longer increment DONE; the negative bench starts the worker thread and checks DONE/BUSY. |
| SMC WDT | Release and ASan execute the tick bench; debug transport enforces alignment. |
| SMC PVT wrapper | Null and malformed payloads are rejected without unsafe copies; negative cases added. |
| SMC AVSBus | Null and malformed payloads are rejected without unsafe copies; negative cases added. |
| SMC reset unit | Null and malformed payloads are rejected without unsafe copies; negative cases added. |
| SMC scratchpad RAM | Null and malformed payloads are rejected without unsafe copies; negative cases added. |
| SMC memory zeroer | Outbound writes carry the canonical `smc::smc_axi_extension` with `SMC_ID`; monitor assertions added. |
| SMC I2C | Existing `target_write`/`target_read` are documented and verified as the production LT bus adapter used by the integrated platform, not test-only backdoors. |
| SMC I3C | The production LT IBI adapter is documented; inbound canonical AXI metadata is extracted for traceability and malformed-payload tests were added. |
| SMC PLIC | Completion of an interrupt not claimed by that context is ignored and regression-tested. |

Selected must-fix status on this branch: **20 resolved, 0 open**. This does not close the Medium/Low conformance and oracle work listed below.

## `main` baseline headline

| Bucket | Count |
|---|---|
| Standalone IPs in the audit | 43 (25 SEP + 18 SMC) |
| Discrete P0 / Critical / High items re-checked on `main` | 52 |
| **Closed on `main`** | **18** |
| Partial (behavior improved, original gap not gone) | 4 |
| **Still open** | **30** |

Closed items are concentrated in harness fail-closed work and a Sep-24 SMC correctness batch (`fb479bc2`) plus later AON/eFuse/KMAC/lifecycle/WDT quality commits. Most SEP crypto/RNG IPs and several SMC protocol IPs are still open.

## Closed on `main` (verified in source)

| Area | Finding | Evidence |
|---|---|---|
| Lifecycle | Coverage merge fails closed | `lifecycle_ctrl/run_tests.sh` `WORKER_FAILED` |
| Lifecycle | Skip is not recorded as pass | `testbench.cpp` skip `return` without `report_test_pass` |
| OTBN | Coverage merge fails closed | `otbn/run_tests.sh` `WORKER_FAILED` |
| SMC ASan | Auxiliary benches no longer `\|\| true` | AVSBus, BEU, I2C, I3C, reset unit, scratchpad, telemetry runners |
| CLINT | Held reset no longer permanently stops ticking | `clint.cpp` re-arms on deassert (`fb479bc2`) |
| CLINT | Tick bench runs in Release and ASan | `clint/run_tests.sh` |
| CLINT | Null `data_ptr` rejected | `clint.cpp` `b_transport` |
| CPU control | Subword write double-shift | `cpu_ctrl.cpp` `reg_write` copies lane into `raw`; tb oracle |
| CPU control | Null `data_ptr` rejected | `cpu_ctrl.cpp` / `cpu_ctrl_tb.cpp` |
| Boot ROM | Aux binaries run in Release and ASan | `bootrom/run_tests.sh` |
| Boot ROM | Null `data_ptr` rejected | `bootrom.cpp` |
| PLIC | Null `data_ptr` rejected | `plic.cpp` |
| Telemetry | Production ATB `atvalid_i` / `atdata_i` | model + `telemetry_receiver_tb.cpp` |
| Telemetry | Early `last_packet` zeros leftover assembly | `telemetry_receiver.cpp` `push_atb_beat` |
| UART | Bit-serial `tx_o` / `rx_i` path exists | `uart.cpp` serial threads + tb case |
| AON timer | Same-value CTRL no longer gold-files a ghost count | `aon_timer.cpp` elapsed-time check + TC_AON_056 |
| eFuse | Consumer accessors now asserted | `test_coverage.cpp` `get_security_disable` |

## Partial on `main`

| IP | What improved | What remains |
|---|---|---|
| UART | Bit-serial TX/RX and BREAK wake | Most character tests still use `inject_rx_char` / `dbg_tx_pop` |
| EDN | DUT has `entropy_endpoint` | FUNC005/011 still drive mock endpoint signals; CSRNG still backdoor |
| Secure DMA | FUNC-001–003 and 006–012 run | FUNC-004/005 still commented out; FUNC-003 mostly commented |
| I2C | AXI extension is fetched | Unused; target path still `target_write`/`target_read` backdoors |

## IPs that still need work on the `main` baseline

Prioritized for remaining P0 (fail-closed, missing frontdoor, or source-level defect). Medium/Low TLM-matrix debt is extra on almost every IP.

### Must-fix first (P0 open on `main`; resolved on the remediation branch)

| IP | Why it is still on the list |
|---|---|
| **SEP HMAC** | Failures call `sc_stop()` without incrementing fail counters; `sc_main` can still exit 0 |
| **SEP KMAC** | FUNC023/FUNC024 never called; TC-166/167 unconditional pass; `const_cast` on `EnMasking` |
| **SEP Secure DMA** | FUNC-004/005 disabled; most FUNC-003 cases commented out |
| **SEP CPU control** | `std::quick_exit(0)` always |
| **SEP memory** | No standalone test executable, ASan, or coverage |
| **SEP EDN** | Suites 1–14 only under Coverage; default/ASan is suite 4; endpoint/CSRNG still harness/backdoor |
| **SEP SPI flash** | Default and `--asan` skip `spi_flash_sc_test` |
| **SEP SPI controller** | COMMAND `LEN` is 20 bits; 512-byte stack buffers; no `>512` reject |
| **SEP CSRNG** | No hardware/app/genbits ports; friend/backdoor coverage |
| **SEP entropy source** | No external entropy-client path; health checks storage-only |
| **SMC DMA** | Failed copies still increment DONE; negative bench never `sc_start`s |
| **SMC WDT** | `wdt_tick_tb` still omitted from default and ASan |
| **SMC PVT wrapper** | Null/short buffer memcpy with no pointer check |
| **SMC AVSBus** | Null/short buffer memcpy with no pointer check |
| **SMC reset unit** | Null/short buffer memcpy with no pointer check |
| **SMC scratchpad RAM** | Null/short buffer memcpy with no pointer check (ASan child-exit hole is closed) |
| **SMC memory zeroer** | Outbound DMA has no `smc::smc_axi_extension` |
| **SMC I2C** | Target path is a backdoor; payload matrix incomplete |
| **SMC I3C** | IBI/bus via inject; inbound AXI extension never extracted |
| **SMC PLIC** | Unclaimed complete can still manufacture pending |

### Remaining quality work (not P0 model crash/fail-open, still not signed off)

| IP | Remaining theme |
|---|---|
| Lifecycle | No modeled reset; TLM status hidden; getter-not-port outputs |
| OTBN | Timeout is WARN-only; friend/coverage bypass |
| AON timer | Alert pulse needs a second bus op; EnableRacl never instantiated |
| eFuse | Mid-run preload backdoor; no reset port; TLM matrix |
| Adams Bridge, AES, HMAC oracles, Key manager, Mailbox, SEP filter/remap/reset/scratch, EL2 PIC | TLM matrix, timing/reset, independent oracles (original Medium/Strongest plans) |
| BEU | Accrued-status write contract vs RDL |
| UART | Serial exists; many tests still backdoor-driven |
| CLINT / CPU control / Boot ROM / Telemetry | P0 model bugs above are closed; TLM/AXI/timing matrix still open |
| OCTS, PLL wrapper | AXI sideband + TLM matrix |

## IPs that do not need a P0 model fix right now

These still have Medium/Low audit gaps. They are not on the must-fix-first list because the original P0 defect or fail-open hole was closed on `main`:

- CLINT (held-reset + tick runner + null pointer)
- SMC CPU control (subword write + null pointer)
- Telemetry receiver (ATB ports + stale assembly)
- Boot ROM (aux binaries + null pointer)

Sign-off still requires the shared TLM/AXI matrix (P1 in the index).
