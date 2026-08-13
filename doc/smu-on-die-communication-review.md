# SMU On-Die Communication — Implementation & Test Review

**Date:** 2026-08-07
**Scope:** `smu-vp` — the SMC↔SEP on-die interconnect modeling work in this
repository (branch `feature/smc-sep-integration`), its test status, and how it
relates to the SMU DV tests in `tt-oca-hw`.
**Companion documents:** `doc/smc-sep-d2d-interconnect.adoc` (design + as-built
status), `vp/platform/smu/README.md` (user guide), `sw/smu-vp-tests/README.md`
(test guide).

---

## 1. What was implemented

The goal: model the SMU on-die path connecting SMC and SEP, and expose ports
for the AXI-over-D2D model (in development separately; binds later).

Topology (RTL reference `hw/smu/rtl/smu.sv`):

```
SEP (och_sep_ss1)                              SMC (dut)
sep_ext_to_smc_axi --[axi_window_remap]--> sep_axi_in     (dedicated path)
sep_smn_inbound_axi <---[smu_axi_xbar]----- output_axi    (crossbar path)
                       ---[smu_axi_xbar]----> sys_axi_in

smu_axi_xbar.ext_out --> aou_axi_s (local AOU TX / D2D stub)
smu_axi_xbar.ext_in  <-- aou_axi_m (local AOU RX)
aou_peer_            ==> remote-die AOU (axi_m -> stub_sysmem)
```

### New models (`vp/platform/smu/inc/`, header-only)

| Model | RTL counterpart | Function |
|---|---|---|
| `smu_axi_xbar.h` | `smu_axi_xbar` | 3x3 non-reflexive AXI router (SEP/SMC/external masters × SEP/SMC/external targets). CCI-programmable apertures (`sep_global_base`/`sep_region_size`, `smc_global_base`/`smc_region_size`); `ext_out` is the static catch-all. Response routing by TLM socket association (no ID-remap table needed at LT fidelity). |
| `axi_window_remap.h` | `axi_window_remap` | In-window rebase (`addr − (local_alias_base − target_base)`), out-of-window passthrough. Used for the SEP SMC-window `[0x4000_0000, +1 GiB)` → alias `0x0` (RTL `SEP_SMC_REGION_*`). |

### Boundary changes

| Side | Change |
|---|---|
| SEP (`vp/platform/sep`) | New `sep_smc_global_port` (`inc/smc_global_port.h`) replaces the bare `SEPMemory` stub: `forward_en=false` (default) keeps standalone `sep-vp` behavior byte-identical (seeded RW stub); `forward_en=true` (preset by `smu-vp`) re-adds the window base and forwards on the new 64-bit `sep_ext_to_smc_axi` initiator (global addresses, like the RTL port). New 64-bit target `sep_smn_inbound_axi` with `fwd_sep_inbound` (subtracts `sep_global_base`, re-issues on the internal 32-bit SimpleBus — the 32↔64 width bridge). |
| SMC (`vp/platform/smc`) | `output_axi`, `sys_axi_in`, `jtag_axi_in`, `sep_axi_in`, `aou_axi_s`, `aou_axi_m` are exported platform sockets. Standalone `smc-vp` re-binds stub/idle terminators. `aou_axi_m` is a plain `tlm_initiator_socket` (hierarchical re-export). |
| SMU (`vp/platform/smu`) | New `smu-vp` executable (`main.cpp`): instantiates both platforms + interconnect, applies the integration presets, binds xbar `ext_out`/`ext_in` through the local AOU (RTL `smu_axi_out`/`smu_axi_in`). The SMC CPU cluster + Whisper are linked as `libsmc_cluster_smu` (shared lib, Whisper symbols hidden) so the two WdRiscv forks (Whisper, VeeR-ISS) coexist in one process. |

### Configuration notes that matter

* `smc_smu_vp.ini` widens `smu_xbar.sep_region_size` to 512 MiB so the SEP
  SRAM alias (`0x5000_0000 + 0x1000_0000`) is inside the aperture, and lowers
  `dut.cluster.mmio_lo` to `0x4000_0000` so SMC CPU traffic to the global
  apertures reaches the fabric (not the cluster's stubbed data socket).
* The SMC scratchpad is at **`0xC006_0000`**, not `0xC004_0000` — the
  `0xC004_0000` slot in the fabric's coarse FRONT_SPM range is the boot ROM.
  SEP→SMC traffic must use window offset `0x6_0000+`.
* The SEP ini resolves `@include`/`configFile` relative to its own directory —
  run it in place; a copy elsewhere silently drops the included settings.

---

## 2. Test status

### 2.1 Passing

| Suite / test | Result | Notes |
|---|---|---|
| SMU unit tests (`vp/platform/smu/test/`) | **PASS** | `smu_axi_xbar` + `axi_window_remap`: route/remap/passthrough/error-propagation/debug-transport/runtime-reprogram. Three separate runs per repo rule: Release, ASan+UBSan (clean log), Coverage — **100% line coverage on both model headers** (`build_cov/coverage-report/index.html`). |
| **SMU platform test** (`sw/smu-vp-tests/smu-link-test/`) | **PASS** | Bidirectional SMC↔SEP firmware handshake on `smu-vp` — see §3. |
| `smc-vp` firmware suite (`sw/smc-vp-tests/`) | **13/13 PASS** | No regressions from the boundary export changes. |
| `sep-vp` spot regression (A/B vs pre-change binary) | **PASS** | `rom_test`, `sep-mailbox-test` (11/11), `sep-spi-dma-test` (with staged fixture), `sep-gpio-test` — all identical to the pre-change binary. |

### 2.2 Failing / hanging — all pre-existing (reproduce identically on the
pre-change `vp/build` binary dated Jul 8)

| Test | Symptom | Evidence it predates this work |
|---|---|---|
| `sep-hmac-test` | Hangs: no test output, 100% CPU indefinitely | Old binary hangs the same way. Also: `run_sep_vp_tests.sh` has no per-test timeout, so one hang stalls the whole suite. |
| `uart_16550_test` | No test output; runs until killed | Identical on old binary (appears to need its UART TCP client on port 8000). |
| `sep-efuse-test` | 22 passed / 5 failed | Identical 22/5 split on old binary. |
| `sep-spi-dma-test` (unstaged) | Fails with undriven-flash data (`0xff`) | Harness gotcha, not a product bug: must be run via `make sim-staged` (stages `flash_memory.bin`). Passes when staged. |

Also noted: standalone `sep-vp` never self-exits after a test's PASS banner
(`sc_start()` unbounded, `_exit: j _exit` in the test CRT) — CI only builds
`sep-vp` and runs the SEP *peripheral* unit tests, so the firmware suite was
never automated as a pass/fail gate. Worth fixing separately (per-test
timeout + PASS grep in `run_sep_vp_tests.sh`).

---

## 3. SEP↔SMC communication tests — what was added

Yes. One new **bidirectional, self-checking platform test** plus the unit
tests for the two new interconnect models.

### `sw/smu-vp-tests/smu-link-test/` (new)

Two firmware halves running concurrently in one `smu-vp` process, each
verifying its receive side:

| Direction | Producer → address | Path | Consumer verifies |
|---|---|---|---|
| SEP→SMC (dedicated) | SEP writes magic+doorbell to `0x4006_1000`/`0x4006_1004` | SEP bus → `smc_global` (`forward_en`) → `sep_ext_to_smc_axi` → `axi_window_remap` (−`0x4000_0000`) → `sep_axi_in` → fabric alias | SMC polls scratchpad `0xC006_1000`/`4`, checks both words. SEP also **reads its own writes back** through the same path, proving the forward path (not the fallback stub) carried them. |
| SMC→SEP (crossbar) | SMC writes response to `0x6000_8000` | cluster mmio → fabric outbound → `output_axi` → `smu_axi_xbar` (SEP aperture hit) → `sep_in` → inbound remap (−`0x5000_0000`) | SEP polls local SRAM `0x1000_8000` for the response word. |

Runner: `sw/smu-vp-tests/run_smu_vp_tests.sh` — a test passes only when
**both** halves print PASS (SMC on UART0, SEP on the virtconsole) and neither
prints FAIL. Both firmwares have bounded spin loops (FAIL on timeout), so a
broken path fails fast instead of hanging.

### Unit tests (`vp/platform/smu/test/smu_interconnect_tb.cpp`)

`smu_axi_xbar`: all 6 route legs, no-route→error response, response-error
propagation, debug transport, runtime CCI aperture reprogramming.
`axi_window_remap`: in/out-of-window, boundary addresses, address restoration
on return, debug transport, error propagation.

---

## 4. tt-oca-hw DV test porting status

**No DV test has been ported 1:1.** `smu-link-test` was written new for the
VP. The SMU/SMC↔SEP-related tests that exist in the checked-out
`sw/tt-oca-hw-main` tree, and how they map onto current VP coverage:

| tt-oca-hw DV test | Location | What it does | VP status |
|---|---|---|---|
| `smc_cpu_traffic_sep_axi_test` | `dv/smu/.../testlist_smc_chiplet.yaml` | SMC CPU firmware (`cpu_traffic`) + 50 external AXI transactions over the SEP AXI path (read-only) | **Functionally covered** by `smu-link-test` (SMC→SEP leg) — but as a single handshake, not a 50-transaction traffic stream. Port candidate. |
| `smc_cpu_traffic_sep_plus_ext_axi_test` | same | Same, plus concurrent external (D2D-side) AXI traffic | Not covered — needs the D2D boundary active (`ext_in`/`ext_out` are stub-terminated today). Natural first test once the AXI-over-D2D model lands. |
| `local_fabric_sep_input_wr_rd_test` | `dv/smc/.../testlist_smc.yaml` | Fabric-level write/read through the SEP input port (`sep_axi_in`) | **Functionally covered**: `smu-link-test`'s SEP→SMC leg drives exactly this port end-to-end; the fabric's own unit suite (`smc/smc_fabric`) covers it at IP level. |
| `sep_load_and_run_binary_test` | `dv/smc/.../testlist_smc.yaml` | SMC loads a SEP binary (`hello_world_adjusted.hex`) and runs it — SMC→SEP provisioning | Not covered — needs the SMC mailbox / SEP boot-control modeling (open item: SMC mailbox IP is a stub). |
| `smc_gpio_filter_access_sep_test` | `dv/smu/.../testlist_smc_chiplet.yaml` | SEP-side access through the SMC GPIO filter | Not covered — filter/inbound-filter CSR programming is modeled in the fabric, but no SEP-origin filter test exists yet. |

Note: `smc_sep_xbar_test` and `smc_sep_interoperability_test` (referenced in
earlier revisions of the design doc) do **not** exist in this checkout of
`tt-oca-hw` — nothing to port under those names here.

### Recommended port order (future work)

1. **`smc_cpu_traffic_sep_axi_test`** — closest to what exists; extend
   `smu-link-test` (or add a sibling) with a multi-transaction SMC→SEP
   traffic loop instead of a single handshake.
2. **`smc_cpu_traffic_sep_plus_ext_axi_test`** — as soon as the
   AXI-over-D2D model binds to `ext_in`/`ext_out`.
3. **`sep_load_and_run_binary_test`** — after the SMC mailbox IP exists
   (currently a stub; blocks RTL-style SMC→SEP provisioning flows).

---

## 5. Open items

| Item | State |
|---|---|
| D2D link model | Not in this repo — in development separately; binds to `smu_axi_xbar.ext_in`/`ext_out` |
| Aperture programming via CSRs (RTL: `sep_cpu_ctrl` → xbar) | Not plumbed — integrator presets keep both sides consistent (`smu-vp` forces the base) |
| SMC mailbox IP | Stub — blocks `sep_load_and_run_binary_test`-style flows |
| `smu-vp` in CI | Not wired yet (alongside the existing `smc-vp`/`sep-vp` jobs) |
| `run_sep_vp_tests.sh` hardening | Needs per-test timeout + PASS grep (pre-existing hang/never-exit behavior, see §2.2) |
