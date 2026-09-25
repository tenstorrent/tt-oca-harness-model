# Static IP Test-Audit Index

## Scope and method

This directory indexes static audits of the standalone SEP and SMC IP models and their tests. The audit method was source-to-test comparison: model behavior, public interfaces, register and transport semantics, test assertions, build wiring, sanitizer/coverage runners, and obvious coverage shortcuts were read and compared.

Original audit: static source-to-test comparison with **no** measured coverage.

**Revalidated 2026-09-24** on branch `fix/pdroy-test-quality-gaps`. Fail-closed runner fixes and two SEP coverage-config test gates were applied. SMC (21 IPs) passed Release/ASan/Coverage/CTest. Lifecycle and OTBN `--coverage` were re-run after the test gates (98.8% and 99.2%). Other individual plans remain static-audit unless they note a closure.

**Revalidated 2026-09-25** on `origin/main` @ `9aa2434a` (includes PR #254 audit-work and PR #251 / `fb479bc2` SMC remediations). Source was checked against the findings below; individual plan files were not rewritten. Tracking table: [`../STATUS.md`](../STATUS.md).

Of **52** P0 / Critical / High items re-checked on `main`: **18 closed**, **4 partial**, **30 still open**. The 43 IP plans themselves are unchanged snapshots.

**Remediated 2026-09-25** on branch `fix/internal-review-p0-20-ips`: all 20 IPs in the must-fix list have their cited P0 issue resolved and pass separate Release, ASan/UBSan, and ≥95% coverage gates. The integrated SEP VP builds with the new production entropy-source → CSRNG → EDN connections.

- Fail-closed/suite execution: HMAC, KMAC, Secure DMA, SEP CPU control, EDN, SPI flash, WDT.
- Standalone or production interfaces: SEP memory, CSRNG, entropy source, I2C, I3C.
- Direct model safety/correctness: SPI controller, SMC DMA, PVT, AVSBus, reset unit, scratchpad RAM, memory zeroer, PLIC.

Detailed closure evidence is recorded in [`../STATUS.md`](../STATUS.md). Medium/Low protocol, timing, reset, and independent-oracle gaps remain out of scope; the original per-IP plans remain audit snapshots rather than being rewritten as new plans.

Inventory is complete for the requested standalone set: all 25 SEP plans and all 18 SMC plans are present.

The tiers below classify the **quality of existing evidence**, not model correctness:

- **Strongest** — substantial public-interface/frontdoor checks with useful independent assertions; still has documented gaps and is not automatic sign-off.
- **Medium** — meaningful functional checks exist, but protocol, timing, reset, integration, or oracle gaps materially limit confidence.
- **Weak / false-green risk** — missing or unexecuted tests, fail-open runners, white-box or unconditional-pass coverage, absent production frontdoors, or critical defect candidates undermine the apparent result.

Model-defect candidates are not treated as confirmed hardware/model bugs unless the cited plan has direct source-level evidence. Findings that depend on an RDL, RTL, or architectural contract remain confirmation items.

## SEP plans

| IP | Evidence tier | Audit |
|---|---|---|
| Adams Bridge | Medium | [Plan](sep/adams_bridge.md) |
| AES | Medium | [Plan](sep/aes.md) |
| AON timer | Weak / false-green risk | [Plan](sep/aon_timer.md) |
| CSRNG | Weak / false-green risk | [Plan](sep/csrng.md) |
| EDN | Weak / false-green risk | [Plan](sep/edn.md) |
| eFuse | Strongest | [Plan](sep/efuse.md) |
| EL2 PIC | Strongest | [Plan](sep/el2_pic.md) |
| Entropy source | Weak / false-green risk | [Plan](sep/entropy_src.md) |
| HMAC | Weak / false-green risk | [Plan](sep/hmac.md) |
| Key manager | Medium | [Plan](sep/key_manager.md) |
| KMAC | Weak / false-green risk | [Plan](sep/kmac.md) |
| Lifecycle controller | Medium | [Plan](sep/lifecycle_ctrl.md) |
| Local master alias remap controller | Strongest | [Plan](sep/local_master_alias_remap_ctrl.md) |
| Mailbox | Strongest | [Plan](sep/mailbox.md) |
| OTBN | Weak / false-green risk | [Plan](sep/otbn.md) |
| Secure DMA | Weak / false-green risk | [Plan](sep/secure_dma.md) |
| SEP CPU control | Weak / false-green risk | [Plan](sep/sep_cpu_ctrl.md) |
| SEP filter control | Medium | [Plan](sep/sep_filter_ctrl.md) |
| SEP memory | Weak / false-green risk | [Plan](sep/sep_memory.md) |
| SEP output remap controller | Medium | [Plan](sep/sep_output_remap_ctrl.md) |
| SEP reset controller | Medium | [Plan](sep/sep_reset_ctrl.md) |
| SEP scratch cold | Medium | [Plan](sep/sep_scratch_cold.md) |
| SEP scratch warm | Strongest | [Plan](sep/sep_scratch_warm.md) |
| SPI controller | Weak / false-green risk | [Plan](sep/spi_controller.md) |
| SPI flash | Medium | [Plan](sep/spi_flash.md) |

## SMC plans

| IP | Evidence tier | Audit |
|---|---|---|
| AVSBus controller | Weak / false-green risk | [Plan](smc/avsbus_controller.md) |
| BEU | Weak / false-green risk | [Plan](smc/beu.md) |
| Boot ROM | Weak / false-green risk | [Plan](smc/bootrom.md) |
| CLINT | Weak / false-green risk | [Plan](smc/clint.md) |
| CPU control | Weak / false-green risk | [Plan](smc/cpu_ctrl.md) |
| DMA | Weak / false-green risk | [Plan](smc/dma.md) |
| I2C controller | Medium | [Plan](smc/i2c_controller.md) |
| I3C controller | Medium | [Plan](smc/i3c_controller.md) |
| Memory zeroer | Medium | [Plan](smc/memory_zeroer.md) |
| OCTS system timer | Strongest | [Plan](smc/octs_system_timer.md) |
| PLIC | Medium | [Plan](smc/plic.md) |
| PLL wrapper | Strongest | [Plan](smc/pll_wrapper.md) |
| PVT wrapper | Medium | [Plan](smc/pvt_wrap.md) |
| Reset unit | Strongest | [Plan](smc/reset_unit.md) |
| Scratchpad RAM | Strongest | [Plan](smc/scratchpad_ram.md) |
| Telemetry receiver | Weak / false-green risk | [Plan](smc/telemetry_receiver.md) |
| UART | Medium | [Plan](smc/uart.md) |
| WDT | Medium | [Plan](smc/wdt.md) |

## Prioritized cross-IP themes

## Closures (2026-09-24, confirmed on `main` 2026-09-25)

| Item | Status |
|---|---|
| Lifecycle coverage ignores failed configs | **Closed** — workers return nonzero; merge aborted. `--coverage` PASS **98.8%**, all 8 configs succeed. |
| OTBN coverage ignores failed algorithms | **Closed** — same fail-closed merge. RSA-vector tests run only for RSA-2048/`unknown_algo`. `--coverage` PASS **99.2%**, all 10 algorithms succeed. |
| Lifecycle skip-as-pass | **Closed** — mismatched CCI cases log a skip; they no longer call `report_test_pass`. |
| SMC ASan `\|\| true` on auxiliary benches | **Closed** for [AVSBus](smc/avsbus_controller.md), [BEU](smc/beu.md), [I2C](smc/i2c_controller.md), [I3C](smc/i3c_controller.md), [Reset unit](smc/reset_unit.md), [Scratchpad RAM](smc/scratchpad_ram.md), [Telemetry](smc/telemetry_receiver.md). Combined child exits fail ASan. SMC orchestrator ASan PASS. |

## Closures (2026-09-25 on `origin/main`)

| Item | Status |
|---|---|
| CLINT held reset stops ticking | **Closed** — deassert re-arms `tick_event_` (`fb479bc2`). |
| CLINT tick bench omitted from default/ASan | **Closed** — `clint_tick_tb` runs in Release and ASan. |
| CLINT / CPU control / Boot ROM / PLIC null `data_ptr` | **Closed** for those four IPs. Still open on PVT, AVSBus, reset unit, scratchpad. |
| CPU control double-shifted subword writes | **Closed** — `reg_write` inserts the lane into `raw`; tb asserts `+4` / `+7`. |
| Boot ROM aux binaries omitted | **Closed** — `bootrom_bin_tb` and `bootrom_neg_tb` run in Release and ASan. |
| Telemetry no ATB frontdoor | **Closed** — `atvalid_i` / `atdata_i` plus a production-path test. |
| Telemetry early-last stale assembly | **Closed** — leftover beats zeroed before decode. |
| UART no serial datapath | **Partial** — bit-serial `tx_o`/`rx_i` exists; many tests still use `inject_rx_char`. |
| AON timer gold-files ghost count | **Closed** — wait distinguishes timeout vs CTRL event; TC_AON_056 asserts no increment. |

Still **open on the `main` baseline** under P0 fail-closed: EDN coverage-only suites; omitted WDT tick and SPI-flash SystemC binaries; HMAC `sc_stop`; KMAC uncalled FUNC023/024; Secure DMA disabled suites; SEP CPU `quick_exit(0)`. These are closed on `fix/internal-review-p0-20-ips`.

Must-fix IP list: [`../STATUS.md`](../STATUS.md).

### P0 — Make every quality gate fail closed

Coverage or sanitizer success is not meaningful when child failures are ignored, important binaries are omitted, or tests can stop without producing a failing exit:

- ~~Coverage runners suppress failed configurations in Lifecycle and OTBN.~~ **Closed 2026-09-24.**
- Most EDN suites run only in the coverage configuration, while hand-written paths are excluded: [EDN](sep/edn.md).
- ~~ASan paths discard auxiliary-bench failures~~ **Closed 2026-09-24** for AVSBus, BEU, I2C, I3C, reset unit, scratchpad RAM, and telemetry.
- ~~Default or ASan phases omit behaviorally important binaries in Boot ROM and CLINT.~~ **Closed 2026-09-25 on `main`.** Still open: [WDT](smc/wdt.md) tick bench and [SPI flash](sep/spi_flash.md) SystemC wrapper.
- False-success mechanisms include uncounted `sc_stop()` failures in [HMAC](sep/hmac.md), uncalled suites and unconditional passes in [KMAC](sep/kmac.md), disabled suites in [Secure DMA](sep/secure_dma.md), and unconditional success exit in [SEP CPU control](sep/sep_cpu_ctrl.md).

Branch update: the still-open fail-closed items above are resolved on `fix/internal-review-p0-20-ips`; see the status file for per-IP evidence.

### P0 — Establish executable production-interface coverage

Several important behaviors have no standalone executable proof or are tested in a harness rather than in the DUT:

- [SEP memory](sep/sep_memory.md) has no standalone test executable, ASan run, or coverage run.
- [EDN](sep/edn.md) endpoint behavior is implemented by the testbench, and CSRNG interaction uses backdoors.
- ~~[Telemetry receiver](smc/telemetry_receiver.md) has no production ATB data/valid frontdoor.~~ **Closed 2026-09-25 on `main`.**
- [UART](smc/uart.md) now has bit-serial `tx_o`/`rx_i`, but most character tests remain `inject_rx_char` / `dbg_tx_pop`.
- Hardware/client paths are missing or bypassed in [CSRNG](sep/csrng.md), [Entropy source](sep/entropy_src.md), [I2C](smc/i2c_controller.md), and [I3C](smc/i3c_controller.md).

Branch update: SEP memory, EDN, CSRNG, entropy source, I2C, and I3C now have the required standalone frontdoor, production interface, or explicit production-adapter contract on `fix/internal-review-p0-20-ips`. UART's remaining helper-driven tests are P1 work.

### P0 — Reproduce direct safety and correctness candidates

The plans identify source-level candidates that need focused reproducer tests and, where the contract is clear, fixes:

- ~~A held reset can stop automatic ticking in [CLINT](smc/clint.md).~~ **Closed 2026-09-25 on `main`.**
- ~~Legal nonzero-lane subword writes appear double-shifted in [SMC CPU control](smc/cpu_ctrl.md).~~ **Closed 2026-09-25 on `main`.**
- Oversized commands can exceed fixed buffers in [SEP SPI controller](sep/spi_controller.md) (`LEN` is still 20 bits; 512-byte stack buffers).
- ~~Early-last telemetry decoding can reuse stale assembly bytes in [Telemetry receiver](smc/telemetry_receiver.md).~~ **Closed 2026-09-25 on `main`.**
- Legal-shaped malformed reads/writes can dereference or overwrite through null/short buffers. **Closed** for Boot ROM, PLIC, CLINT, CPU control. **Still open** in [PVT wrapper](smc/pvt_wrap.md), [AVSBus](smc/avsbus_controller.md), [Reset unit](smc/reset_unit.md), and [Scratchpad RAM](smc/scratchpad_ram.md).
- Failed DMA operations can be reported as completed in [SMC DMA](smc/dma.md), while its negative bench does not start the transfer thread.

Branch update: the SPI-controller bound, malformed-buffer handling, DMA completion accounting, PLIC ownership, memory-zeroer sideband, and WDT runner/alignment findings are resolved on `fix/internal-review-p0-20-ips`. BEU semantics and broader protocol-matrix work remain.

### P1 — Add a shared TLM protocol conformance matrix

Nearly every plan lacks some combination of command validation, null-pointer handling, length/alignment boundaries, byte enables, streaming width, response status, read-buffer preservation, incoming/additive delay, `transport_dbg`, and DMI policy. This is systemic across both SEP and SMC; representative plans are [AES](sep/aes.md), [Mailbox](sep/mailbox.md), [SEP filter control](sep/sep_filter_ctrl.md), [I2C](smc/i2c_controller.md), [PLIC](smc/plic.md), and [UART](smc/uart.md).

Build one reusable payload-matrix helper and apply it to every target socket. Keep each IP's side-effect expectations explicit, especially for FIFO ports, W1C registers, debug writes, and rejected accesses.

### P1 — Enforce canonical SMC sideband and initiator payloads

Canonical AXI sideband evidence is missing broadly. Direct contract gaps include outgoing transactions without `smc::smc_axi_extension` in [Memory zeroer](smc/memory_zeroer.md), unverified outgoing metadata/lifetime in [SMC DMA](smc/dma.md), and absent target-side handling in [I3C](smc/i3c_controller.md), [OCTS](smc/octs_system_timer.md), and [PLL wrapper](smc/pll_wrapper.md). Other SMC targets fetch and discard the extension without field-integrity tests.

Use protocol-aware downstream monitors to assert command, address, length, byte enables, streaming width, delay, response, extension fields, and extension ownership on every outgoing transaction.

### P1 — Replace white-box execution with independent oracles

Friend access, direct callback/helper invocation, state pokes, duplicated algorithms, and unconditional passes inflate execution without proving public behavior:

- Heavy white-box/coverage-only paths: [CSRNG](sep/csrng.md), [KMAC](sep/kmac.md), [OTBN](sep/otbn.md), [Secure DMA](sep/secure_dma.md), and [Entropy source](sep/entropy_src.md).
- Permissive or missing assertions: [AON timer](sep/aon_timer.md), [HMAC](sep/hmac.md), [SPI controller](sep/spi_controller.md), and [Mailbox](sep/mailbox.md).
- Correlated expected-value logic: [Key manager](sep/key_manager.md), [SMC DMA](smc/dma.md), [Boot ROM](smc/bootrom.md), [Scratchpad RAM](smc/scratchpad_ram.md), and [Telemetry receiver](smc/telemetry_receiver.md).

Retain helper tests as clearly labeled unit tests, but do not count them as frontdoor/IP conformance. Prefer fixed published vectors, literal boundary expectations, protocol monitors, and architectural state.

### P1 — Test time, reset, and concurrency at boundaries

Broad sleeps and post-completion checks miss event-ordering failures. Highest-priority examples are the event-interruption behavior accepted by [AON timer](sep/aon_timer.md), in-flight reset and reseed/cipher work in [AES](sep/aes.md), synchronous unobservable busy/timing in [Memory zeroer](smc/memory_zeroer.md), reset/tick races in [CLINT](smc/clint.md) and [WDT](smc/wdt.md), and transfer/reset ordering in [SMC DMA](smc/dma.md).

Assert behavior at `T-ε`, `T`, `T+ε`, and delta-cycle boundaries; reset each process while idle, waiting, active, completing, and interrupting.

### P2 — Make quality metrics match the claim

After semantic repairs:

1. Run separate Release, ASan+UBSan, and Coverage builds.
2. Run every semantic binary in every applicable phase.
3. Require clean sanitizer logs and fail on any child exit.
4. Enforce line coverage on each touched hand-written source/header, not only an aggregate directory.
5. Remove exclusions of reachable hand-written code and stop counting dump/log formatting or switch-arm peeks as functional assurance.
6. Record measured results only after these gates pass; this index intentionally records none.

## Recommended remediation order

1. **Harness integrity:** fix ignored exits, omitted binaries/suites, unconditional passes, non-failing timeouts, and broad report suppression.
2. **Missing frontdoors/tests:** add a SEP memory bench and production-interface coverage for EDN, UART (beyond inject helpers), and hardware-client paths (CSRNG, entropy, I2C, I3C).
3. **Critical reproducers:** add minimal tests for the direct source-level candidates above; confirm contract-dependent findings against RDL/RTL before changing behavior.
4. **TLM and AXI conformance:** deploy shared target-payload matrices and SMC initiator monitors.
5. **Temporal/reset/concurrency:** add exact boundary and in-flight reset tests.
6. **Oracle quality:** replace direct state pokes, copied algorithms, weak nonzero checks, and WARN-only outcomes with independent assertions.
7. **Measured gates:** only then rerun separate Release, ASan+UBSan, and per-touched-file Coverage and publish the measured evidence.

## Excluded integration scope

These standalone plans do not audit `smc_fabric`, CPU clusters, VP platforms, or AOU. They need separate integration audits covering routing, address maps, cross-IP wiring, sideband propagation, reset topology, CCI preset timing, firmware-visible behavior, and end-to-end error handling. Standalone IP evidence must not be used as a substitute for those integration audits.
