# SMC VP RTL Address-Map Realignment — Implementation Steps

> **Gate 0 locked 2026-08-18:** D1=**A**, D2=**P1**, D3=**U1**, D4=**H1**. Phase 1 implementation in progress.

| | |
|---|---|
| **Parent plan** | [`smc-vp-rtl-address-map-realignment.adoc`](smc-vp-rtl-address-map-realignment.adoc) |
| **RTL source of truth** | `tt-oca-harness/hw/sys/smc/regs/smc.rdl` (`addrmap smc_top`) + xbar pkgs under `hw/sys/smc/rtl/crossbars/` |
| **Date** | 2026-08-18 |

---

## Overview

```text
Gate 0 (sign-off) ──► Phase 1 (periph shuffle + map-coherence ELF)
                           │
                           ├─► Phase 2 (fabric window ends)
                           ├─► Phase 3 (PLIC/CLINT/BEU)   [only if D2 = P1]
                           └─► Phase 4 (doc hygiene)
```

Keep **per-IP** `smc-*-test` ELFs. Add one **cumulative** `smc-map-coherence-test` in Phase 1.

---

## Gate 0 — Before any code

Until every item below is decided, Phase 1 PRs stay **blocked**.

| ID | Decision | Locked answer |
|----|----------|---------------|
| **D1** | AOU CSR placement | **A** — `0xC000_C000` (VP-only park); vacate `0xC000_4000` |
| **D2** | PLIC / CLINT / BEU | **P1** — Phase 3 (expand `mmio_hi`, move fabric + FW) |
| **D3** | UART macros | **U1** — 16550 base `0xC000_6100 + N×0x400` |
| **D4** | Anti-drift | **H1** — `smc-map-coherence-test` |
| — | Phase split | Phase 1 ≠ Phase 3 in same PR |
| — | Named stubs | Stubs OK at correct RTL base |
| — | Coherence ELF | In Phase 1 scope |

### Who to notify

- SMC VP / firmware owners (`smc_common.h`, all `smc-*-test`)
- SMU / D2D owners (`smu-aou-ext-test`, AOU window remap)
- Zephyr-on-SMC owners (DTS / `mmio_poke` if hard-coded)
- Doc consumers of old bases: `0xC000_E000` (OCTS), `0xC000_A000` (UART), `0xC000_8000` (AVSBus), `0xC000_4000` (AOU)

---

## Phase 1 — Peripheral shuffle (one PR)

| | |
|---|---|
| **Goal** | Modeled IPs sit at RTL bases; AOU vacates `0xC000_4000`; add map-coherence ELF; all FW tests green |
| **Out of scope** | PLIC/CLINT/BEU; fabric `periph_main` / DFX / DMA end fixes |

### Target moves (summary)

| Block | From (VP today) | To (RTL) |
|-------|-----------------|----------|
| AVSBus | `0xC000_8000` | `0xC000_4000` |
| OCTS | `0xC000_E000` | `0xC000_A000` |
| UART0–3 | `0xC000_A000`+ | Wrap `0xC000_6000`; 16550 @ `+0x100` (U1 → `0xC000_6100` + N×`0x400`) |
| PLL | `0xC000_3000` | `0xC040_2000` |
| PVT | `0xC040_2000` | `0xC040_3000` |
| AOU | `0xC000_4000` | D1 slot (recommend `0xC000_C000`) |
| cpu_ctrl | also `@ 0xC040_0000` | Front-port only `0xC003_9000` |

### Ordered steps

1. **Freeze decisions** — paste D1–D4 into the PR description.
2. **Update `smc_common.h`** (same PR as platform C++):
   - `SMC_AVSBUS_BASE` → `0xC000_4000`
   - `SMC_OCTS_TIMER_BASE` → `0xC000_A000`
   - UART macros per D3 (U1: `SMC_UART0_BASE = 0xC000_6100`, stride `0x400`)
   - `SMC_PLL_WRAP_BASE` → `0xC040_2000`, `SMC_PVT_WRAP_BASE` → `0xC040_3000`
   - cpu_ctrl → `0xC003_9000` only; drop `0xC040_0000` use
   - `SMC_AOU_BASE` → D1 choice
   - Remove stale “per smc_top.rdl” comments
3. **Update `smc_platform.cpp`** routes / `A_*` for the same moves; stub gpio @ `0x3000`, eFuse @ `0x8000`; drop dual cpu_ctrl bind.
4. **UART path** — `start.S` / `printf.c` use **16550** base (U1), not wrap `+0`.
5. **SMU AOU** — if park @ `0xC000_C000`, confirm `0x4000_xxxx` alias still matches.
6. **Add** `sw/smc-vp-tests/smc-map-coherence-test/` (below).
7. **Minimal README** updates for AVSBus / OCTS / AOU / PLL / PVT.
8. **Verify** (gate below).

### Files that must land together

**Platform**

- `vp/platform/smc/src/smc_platform.cpp`
- `vp/platform/smc/smc_platform.hpp` (comments)
- `vp/platform/smc/config/smc_platform_vp.ini` (if presets mention bases)

**Firmware**

- `sw/smc-vp-tests/common/smc_common.h`
- `common/start.S`, `common/printf.c`
- `smc-avsbus-test/`, `smc-octs-timer-test/`, `smc-octs-timer-secondary-test/`
- `smc-aou-test/`, `smc-pll-wrapper-test/`, `smc-pvt-wrap-test/`
- **new** `smc-map-coherence-test/`
- `sw/smu-vp-tests/smu-aou-ext-test/`

**Comments / docs (minimal)**

- `avsbus_controller.h`, OCTS README, AOU docs citing `0xC000_4000`, cpu_ctrl dual-base notes

### `smc-map-coherence-test`

Keep all per-IP ELFs. Add **one** cumulative ELF that:

1. For each **modeled** window — write token / read back (or known RO reset): UART SCR, AVSBus, OCTS, telemetry, AOU `ip_version`, PLL/PVT.
2. For each **stubbed** RTL slot (gpio, eFuse, DTP, …) — read and assert stub contract so a wrong model on that slot fails.
3. Drive from the **same** literals as `smc_common.h`.

Wire via normal `Makefile` so `./run_smc_vp_tests.sh` discovers it.

### Phase 1 verification

```bash
cd sw/smc-vp-tests
./run_smc_vp_tests.sh
./run_smc_vp_tests.sh smc-map-coherence-test
# SMU AOU if applicable
```

**Pass when**

- [x] All applicable `smc-*-test` ELFs green on **new** bases  
- [x] `smc-map-coherence-test` fails if any IP sits on a pre-remap or stub slot  
- [x] PR does **not** claim “full RTL match” (PLIC still old until Phase 3)

---

## Phase 2 — Fabric window ends (separate PR)

| Change | From | To |
|--------|------|-----|
| `PERIPH_MAIN_END` | `0xC000_E800` | `0xC000_B800` |
| `DFT_CSR_*` | `0xC000_F800` | `0xC000_B800` (size `0x800`) |
| `DACCEL_DMA_ZEROER_END` | `0xC003_A000` | `0xC003_9000` |

**Also:** bind DFX stub at RTL base; fix `smc_fabric` unit tests; update fabric docs; remove stale “verbatim xbar” comments for these windows.

**Verify:** `smc/smc_fabric` `./run_tests.sh` (+ ASAN/coverage); smoke `run_smc_vp_tests.sh`.

> **Done 2026-08-18.** Shrinking `periph_main` to the RTL end strands the D1=A
> AOU park at `0xC000_C000`, so the fabric carries one explicitly labelled
> **VP-only** `AOU_PARK` window (`0xC000_C000`–`0xC000_D000` → `to_periph`).
> That window is the sole deliberate deviation from `smc_local_xbar_pkg.sv`.
> `smc-map-coherence-test` no longer probes `0xC000_E000`: it is now a decode
> hole, and the CPU traps into the spin loop in `common/start.S` rather than
> reporting a failure.

---

## Phase 3 — PLIC / CLINT / BEU (only if D2 = P1)

| Block | VP today | RTL target |
|-------|----------|------------|
| PLIC | `0xC080_0000` | `0xC400_0000` |
| CLINT | `0xC0C0_0000` | `0xC800_0000` |
| BEU[N] | `0xC0C1_0000` | `0xC801_0000` + N×`0x1000` |

**Steps**

1. `cluster.mmio_hi` → ≥ `0xC802_0000` (CCI + INI + SMU presets if any)
2. Fabric `FRONT_PLIC_*` / `FRONT_CLINT_BEU_*` → RTL
3. Update `SMC_PLIC_BASE` / `SMC_CLINT_BASE` / `SMC_BEU_BASE` + BEU/PLIC FW tests
4. Zephyr DTS / `mmio_poke` if needed
5. Confirm `0xC080_0000` no longer swallowed as PLIC (RTL `ecam_region`)

**If D2 = P2:** skip this phase; document “known VP alias”; delete false xbar comments.

**Verify:** `smc-beu-test`, `smc-beu-error-test`, Zephyr smoke.

> **Done 2026-08-18 — step 1 above was wrong and is superseded.**
> `cluster.mmio_hi` did **not** need raising. `smc_addrmap_pkg` puts PLIC
> (`0xC400_0000`), CLINT (`0xC800_0000`) and per-core BEU
> (`0xC801_0000 + N×0x1000`) inside `SMC_CLUSTER`, as rocket-chip blocks on the
> cluster periphery bus. The harts reach them over that bus and never traverse
> `smc_input_fabric`, whose 16 MB `LOCAL_ALIAS_REGION_SIZE` demux would push
> those addresses outbound. Note `axi_front_port_req_i` is an **input** to
> `smc_cpu_wrapper`: the front port is the inbound path for *external* masters,
> not a CPU egress.
>
> Implemented instead: `cluster.data` (everything outside the unchanged
> `[mmio_lo, mmio_hi)` = 16 MB carve-out) binds straight to `front_port_router`,
> so the harts reach PLIC/CLINT/BEU directly. `addr_router::tgt` became a
> multi-bind socket so the fabric (external masters) and the cluster share one
> route table. The fabric's `FRONT_PLIC_*` / `FRONT_CLINT_BEU_*` windows moved
> to the RTL values for the external path.

---

## Phase 4 — Doc hygiene

```bash
rg -n '0xC000_E000|0xC000E000|0xC000_A000|0xC000_8000|0xC080_0000|0xC0C0_0000|0xC0C1_0000' \
  smc/doc sw/smc-vp-tests aou/doc doc/ smc/cpu_cluster/doc smc/smc_fabric/doc \
  smc/peripherals/*/README.md smc/peripherals/*/doc
```

Update integration guides, fabric docs, Zephyr/D2D docs, AOU/OCTS/AVSBus/PLL/BEU docs, and mark phases done in this file + parent plan.

> **Done 2026-08-18.** Files corrected:
>
> | File | What was stale |
> |------|----------------|
> | `smc/doc/systemc_tlm2_integration_guide.adoc` | Address-map table rows for GPIO, PLL, PVT, AVSBus, UART wrap, eFuse, OCTS, DTP/DFX; BEU "local alias" claim; UART sub-decode section |
> | `smc/cpu_cluster/doc/05_RTL_SystemC_Integration_Guide.md` | §6 base-address table carried the pre-realignment RDL map (GPIO `0x4000`, AVSBus `0x8000`, UART `0xA000`, OCTS `0xE000`, …); added PLL/PVT `smc_external` rows |
> | `doc/zephyr-on-smc.adoc` | UART0 `0xC000_A000`, PLIC `0xC080_0000`, CLINT `0xC0C0_0000` |
> | `doc/smc-sep-d2d-interconnect.adoc` | AOU CSRs at `0xC000_4000` / SEP view `0x4000_4000` (D1=A moved both) |
> | `smc/peripherals/beu/doc/04_BEU_Platform_Integration_Test_Plan.md` | Two-address-view section; the pre-integration "Evidence on main today" block is now labelled historical rather than rewritten |
> | `smc/peripherals/avsbus_controller/doc/01_AVSBUS_CONTROLLER_Specification.md` | Placement `0xC000_8000` |
> | `sw/smc-vp-tests/common/smc_common.h` | Comment headers for AVSBus, OCTS and BEU still named the old bases |
>
> Deliberately **not** changed: `PERIPH_EXT_END`/`A_PERIPH_EXT_HI` = `0xC080_0000`
> (that is the RTL `smc_external` end, not the old PLIC base), the
> `0xC000_E000` mention in `smc-map-coherence-test/main.c` (explains why the
> vacated slot is no longer probed), and `smc/peripherals/octs_system_timer/README.md`
> (`0xC000_A000` is now the correct base).

---

## Notification template (email / PR)

```text
Subject: [SMC VP] Address-map realignment Phase 1 — after D1–D4 sign-off

Parent plan: smc/smc-vp-rtl-address-map-realignment.adoc
Impl steps:  smc/smc-vp-rtl-address-map-realignment-implementation.md

Decisions locked:
  D1 AOU   = A (0xC000_C000)
  D2 PLIC  = P1
  D3 UART  = U1
  D4 drift = H1

Phase 1 WILL:
  - Move AVSBus, OCTS, UART (+16550 offset), PLL, PVT to RTL bases
  - Vacate AOU from 0xC000_4000 → 0xC000_C000
  - Drop dual cpu_ctrl at 0xC040_0000
  - Add smc-map-coherence-test (keep per-IP ELFs)
  - Update smc_common.h + affected smc-vp-tests / smu-aou-ext-test

Phase 1 will NOT:
  - Move PLIC/CLINT/BEU (Phase 3 if P1)
  - Change fabric periph_main / DFX / DMA ends (Phase 2)

Please ACK if you own FW, SMU AOU, or Zephyr-on-SMC consumers of these bases.
```

---

## Risks

| Risk | Mitigation |
|------|------------|
| Platform C++ and `smc_common.h` diverge | One PR for both |
| UART wrap without `+0x100` | Hung `printf` — lock U1 before coding |
| “Fully RTL-aligned” after Phase 1 | False if PLIC still `@ 0xC080_0000` (unless D2=P2) |
| AOU park vs SMU `0x4000_xxxx` | Confirm alias math before changing `smu-aou-ext-test` |

---

## Definition of done

- [x] Gate 0 decisions recorded  
- [x] Phase 1 merged — all `run_smc_vp_tests.sh` + map-coherence green  
- [x] Phase 2 merged — fabric unit tests green (97% line coverage; ASan not
      runnable on RHEL8 gcc-toolset-13, same gap `ci-rhel8.yml` documents)  
- [x] Phase 3 merged — PLIC/CLINT/BEU at RTL bases; harts reach them via the
      cluster-direct path; `smc-beu-test` / `smc-beu-error-test` green  
- [x] Phase 4 greps clean of stale “current map” claims  
- [x] Parent plan marked accepted / implemented  
- [ ] **Deferred to CI:** `sw/smu-vp-tests/run_smu_vp_tests.sh` (`smu-aou-ext-test`)

### Deferred verification — SEP-side AOU view

D1=A moved the AOU CSRs, so `smu-aou-ext-test/sep_main.c` now reads
`ip_version` at `0x4000_C000` instead of `0x4000_4000`. That suite was **not**
run locally; the `smu-vp` / `smu-vp-rhel8` CI jobs cover it.

`smu-vp` cannot be built on a stock RHEL8 dev host, for two reasons that both
predate this work:

1. `sep/peripherals/kmac` needs OpenSSL ≥ 3.0 (`openssl/core_names.h`) and RHEL8
   ships 1.1.1. Workaround: the distro's `openssl3-devel` installs a
   non-standard layout (headers `/usr/include/openssl3/openssl`, dev symlinks
   `/usr/lib64/openssl3`), so build a shim prefix with `include/openssl` and
   `lib/lib{crypto,ssl}.so` symlinks and pass `-DOPENSSL_ROOT_DIR=<shim>`. The
   runtime `libcrypto.so.3` is already on the default loader path. This works.
2. The vendored `sep/cpu/VeeR-ISS/Tlb.cpp` includes `<bitset>` *after*
   `VirtMem.hpp`, which libstdc++ **13** rejects (`partial specialization of
   std::hash<basic_string<...>> after instantiation`). `ci-rhel8.yml` pins
   `gcc-toolset-12`, whose headers accept it. No workaround without either
   installing toolset 12 or reordering the includes — both out of scope here.

The SMC side of the same move *is* covered locally and is green:
`smc-map-coherence-test` reads `AOU IP_VERSION` at `0xC000_C000` and asserts the
vacated `0xC000_4000` no longer returns that token, and `smc-aou-test` drives
the model through `SMC_AOU_BASE`. The SEP view is the same CSR through
`smu_axi_xbar`'s 16 MiB SMC window (`0x4000_0000` rebased to `0xC000_0000`), so
`0x4000_C000` resolves to the verified `0xC000_C000`.
