# SMC VP Address-Map Realignment to RTL `smc_top`

| | |
|---|---|
| **Status** | **IMPLEMENTED** — D1–D4 locked; Phases 1–4 landed 2026-08-18 |
| **Date** | 2026-08-18 |
| **AsciiDoc source** | [`smc-vp-rtl-address-map-realignment.adoc`](smc-vp-rtl-address-map-realignment.adoc) |
| **Implementation steps** | [`smc-vp-rtl-address-map-realignment-implementation.md`](smc-vp-rtl-address-map-realignment-implementation.md) |

> All four decisions are locked and every phase has landed. The one remaining
> deliberate deviation from `smc_local_xbar_pkg.sv` is the VP-only AOU park at
> `0xC000_C000` (D1=A), which has no RTL slot. `smc-map-coherence-test` guards
> the rest against drift.

This document proposes realigning the SMC virtual-platform address map (`smc-vp` / `smu-vp`) to the current RTL `smc_top` map. It records mismatches confirmed against **`tt-oca-harness`**, the proposed target map, files that must move together, and questions reviewers must answer before code changes start.

---

## Problem

The VP peripheral map is multi-sourced and has drifted from RTL. `vp/platform/smc/src/smc_platform.cpp` and `sw/smc-vp-tests/common/smc_common.h` place several modeled IPs on the wrong slot (or on a slot RTL assigns to a different owner). Comments that cite “`smc_top.rdl`” are themselves stale — for example “`system_timer_octs` @ `0xC000_E000` per `smc_top.rdl`” is false against the current RDL, where OCTS is at `0xC000_A000`.

Firmware tests in `sw/smc-vp-tests/` are **one ELF per IP**. That is useful for isolation, but it cannot catch “this window is the wrong owner” because each test only touches the base it was written against.

---

## Source of truth

| Role | Artifact | Location (`tt-oca-harness`) |
|------|----------|-------------------------------|
| Register map (SoT) | `addrmap smc_top #(BASE_ADDR = 0xC000_0000)` | `hw/sys/smc/regs/smc.rdl` |
| Generated C header | `smc_top_regs.h` / `smc_addr.h` | `hw/sys/smc/bootrom/prod/registers/smc_top_regs.h`, `hw/sys/smc/regs/gen/c/smc_addr.h` |
| Generated SV header | `smc_reg.svh` | `hw/sys/smc/regs/gen/svh/smc_reg.svh` |
| Local / front-port decode | `smc_local_xbar_pkg.sv` | `hw/sys/smc/rtl/crossbars/smc_local_xbar_pkg.sv` |
| Peripheral AXI-Lite decode | `smc_periph_axi_lite_xbar_pkg.sv` | `hw/sys/smc/rtl/crossbars/smc_periph_axi_lite_xbar_pkg.sv` |
| Internal AXI-Lite decode | `smc_internal_axi_lite_xbar_pkg.sv` | `hw/sys/smc/rtl/crossbars/smc_internal_axi_lite_xbar_pkg.sv` |
| Adopter external window | `smc_ip_integration.sv` (`ExtPllBase` / `ExtPvtBase`) | `hw/top/smc_ip_integration.sv` |

There is **no** file named `smc_top.rdl` in the current harness tree. Reviews that said “`smc_top.rdl` — tt-oca-hw” mean the `addrmap smc_top` inside **`smc.rdl`**. That is the correct SoT.

**Cross-check rule:** a VP base is “RTL-aligned” only when it matches **both** `smc.rdl` **and** the corresponding xbar package. If those two disagree, stop and raise it — do not pick a third number.

### What is *not* source of truth

Do **not** copy bases from these (older map: OCTS @ `0xE000`, AVSBus @ `0x8000`, UART @ `0xA000`, GPIO @ `0x4000`):

- VP comments in `smc_platform.cpp` / `smc_common.h` (“OCTS @ E000 per `smc_top.rdl`”)
- `smc/cpu_cluster/doc/05_RTL_SystemC_Integration_Guide.md` §6
- `smc/doc/systemc_tlm2_integration_guide.adoc` address-map table (mixed current/stale)
- Harness `hw/sys/smc/dv/docs/SMC_VPLAN.adoc` (AVSBus still @ `0xC000_8000`, GPIO @ `0xC000_4000`)
- Fabric comments claiming “verbatim from `smc_local_xbar_pkg.sv`” for PLIC @ `0xC080_0000` — RTL has `FRONT_PORT_PLIC_BASE = 0xC4000000`

---

## Scope

**In scope**

- SMC local-alias aperture decode in `smc-vp` / `smu-vp` (`smc_platform`, `smc_fabric`, FW headers, CCI presets)
- Moving already-modeled IPs onto RTL bases (AVSBus, UART, OCTS, PLL, PVT, cpu_ctrl CSR window)
- Fabric window ends that disagree with the RTL xbar (`periph_main`, DFX, PLIC/CLINT/BEU)
- A cumulative firmware map-coherence test
- Doc/comment cleanup so stale RDL cites are not presented as current

**Out of scope** (track separately)

- New functional models for RTL IPs VP lacks (`gpio_intf`, eFuse, `dtp_ctrl`, `dfx_ctrl`, `misc_wrap`) — after remap those slots become stubs at the *correct* base
- Cycle-accurate UART log-engine, eFuse programming, or DFX behaviour
- Changing PeakRDL / RTL — if `smc.rdl` itself is wrong, that is an RTL ticket

---

## Confirmed RTL vs VP audit

Bases use `BASE_ADDR = 0xC000_0000`. “VP today” = `smc_platform.cpp` + `smc_common.h` as of this draft.

### Peripheral slots

| Address | RTL (`smc.rdl` / periph xbar) | VP today | Issue |
|---------|-------------------------------|----------|--------|
| `0xC000_3000` | `gpio_intf[65]` stride `0x10` | `pll_wrap` (4 KiB) | Wrong IP. RTL PLL is in `smc_external` |
| `0xC000_4000` | `avsbus_controller` | AOU (`0x80`) | Wrong IP. AOU not in `smc_top`; steals AVSBus slot |
| `0xC000_6000` | `uart_wrap` (4 UARTs, stride `0x400`) | Gap → stub | RTL UART not at RTL base |
| `0xC000_7000` | `smc_efuse_map` | Gap → stub | Missing in VP |
| `0xC000_8000` | `efuse_interface_ctrl` | AVSBus | Wrong IP |
| `0xC000_9000` | `telemetry_receiver_wrap` | telemetry ×3 | **Already aligned — keep** |
| `0xC000_A000` | `system_timer_octs` | UART0 | Wrong IP |
| `0xC000_B000` | `dtp_ctrl` | UART1 | Wrong IP / unmodeled |
| `0xC000_B800` | `dfx_ctrl` | UART2 region | Wrong / unmodeled |
| `0xC000_C000` | Unmapped in `smc_top` | UART2 | Sim-only UART |
| `0xC000_D000` | Unmapped in `smc_top` | UART3 | Sim-only UART |
| `0xC000_E000` | Unmapped in `smc_top` | `octs_system_timer` | OCTS at wrong base; “RDL @ E000” comments are false |

### Additional mismatches

| Item | RTL | VP today | Issue |
|------|-----|----------|--------|
| PLL | `smc_external + 0x2000` = `0xC040_2000` | `0xC000_3000` | Occupies GPIO |
| PVT | `smc_external + 0x3000` = `0xC040_3000` | `0xC040_2000` | VP PVT on RTL PLL slot |
| `cpu_ctrl` | `0xC003_9000` (front-port) | Also `@ 0xC040_0000` | Extra periph bind; drop it |
| PLIC | `0xC400_0000` | `0xC080_0000` | Stale “verbatim xbar”; occupies RTL `ecam_region` |
| CLINT | `0xC800_0000` | `0xC0C0_0000` | Same |
| BEU[0] | `0xC801_0000` (+ N×`0x1000`) | `0xC0C1_0000` | Same |
| DFX / DFT CSR | `0xC000_B800` | `0xC000_F800` | Wrong fabric window |
| `periph_main` end | `0xC000_B800` | `0xC000_E800` | VP swallows `0xC000_C000..E7FF` |
| DMA/zeroer end | `0xC003_9000` (size `0x1000`) | `0xC003_A000` | VP overlaps `cpu_ctrl` |

### Already aligned (do not move)

| Block | Base | Note |
|-------|------|------|
| WDT[0..3] | `0xC000_0000 + N×0x400` | Keep |
| Reset unit | `0xC000_2000` | Keep; `misc_wrap` @ `0xC000_2800` is separate |
| I2C wrap | `0xC000_5000` (+`0x200` / +`0x400`) | Keep |
| Telemetry | `0xC000_9000` (+`0x100`) | Keep |
| DMA / zeroer | `0xC003_8000` / `0xC003_8200` | Keep bases; fix fabric end |
| `cpu_ctrl` front-port | `0xC003_9000` | Keep; drop `0xC040_0000` |
| OCA I3C wrap 0 | `0xC003_A000` | Keep (SystemC packing stride is a known model limit) |
| SPM ROM / bootrom | `0xC004_0000` | Keep |
| SPM / scratch | `0xC006_0000` | Keep |

### RTL UART wrap layout (needed for the UART move)

`uart_wrap` @ `0xC000_6000` (window `0x1000`), instances stride `0x400`:

| Inst | Wrap base | 16550 (`uart`) | Log engine |
|------|-----------|----------------|------------|
| 0 | `0xC000_6000` | `0xC000_6100` | `0xC000_6200` |
| 1 | `0xC000_6400` | `0xC000_6500` | `0xC000_6600` |
| 2 | `0xC000_6800` | `0xC000_6900` | `0xC000_6A00` |
| 3 | `0xC000_6C00` | `0xC000_6D00` | `0xC000_6E00` |

VP today uses four 4 KiB windows at `0xC000_A000 + N×0x1000` with 16550 at offset 0. Realigning UART is both a **base** and **sub-decode** change: `SMC_UARTn_BASE` must be the **16550** base (`0xC000_6100 + N×0x400`), or `printf` / `start.S` will hit the log-engine ctrl word.

---

## Proposed target map

Local alias `LOCAL_BASE = 0xC000_0000`. After realignment, every modeled target sits at the RTL base. Unmodeled RTL slots stay as **named stubs** at the RTL base (coherence test can tell “stub at right place” from “wrong IP”).

| Block | Target base | Window | VP action | Phase |
|-------|-------------|--------|-----------|-------|
| WDT[0..3] | `0xC000_0000 + N×0x400` | `0x400` each | Keep | — |
| Reset unit | `0xC000_2000` | `0x800` (xbar) | Keep; optional grow | 1 |
| `misc_wrap` | `0xC000_2800` | `0x800` | Named stub | 1 |
| `gpio_intf` | `0xC000_3000` | `0x1000` | Named stub; remove `pll_wrap` | 1 |
| AVSBus | `0xC000_4000` | `0x1000` | Move from `0x8000` | 1 |
| I2C | `0xC000_5000` | `0x1000` | Keep | — |
| `uart_wrap` | `0xC000_6000` | `0x1000` | Move 4 UARTs; 16550 @ `+0x100` | 1 |
| `smc_efuse_map` | `0xC000_7000` | part of eFuse | Named stub | 1 |
| `efuse_interface_ctrl` | `0xC000_8000` | part of eFuse | Named stub; remove AVSBus | 1 |
| Telemetry | `0xC000_9000` | `0x300` used | Keep | — |
| OCTS | `0xC000_A000` | `0x1000` | Move from `0xE000` | 1 |
| `dtp_ctrl` | `0xC000_B000` | `0x800` | Named stub | 1 |
| `dfx_ctrl` | `0xC000_B800` | `0x800` | Named stub; fabric DFT moves | 2 |
| base_config / remap / … | `0xC001_0000` .. `0xC003_8000` | as xbar | Keep fabric | — |
| DMA / zeroer | `0xC003_8000` | ends `0xC003_9000` | Keep bases; shrink fabric end | 2 |
| `cpu_ctrl` | `0xC003_9000` | `0x1000` | Keep front-port; drop `0xC040_0000` | 1 |
| OCA I3C | `0xC003_A000` | `0x6000` | Keep | — |
| SPM ROM | `0xC004_0000` | — | Keep | — |
| SPM / scratch | `0xC006_0000` | — | Keep | — |
| `smc_cla` / DFD | `0xC016_0000` | `0x100000` | Keep fabric | — |
| `smc_external` | `0xC040_0000` | `0x400000` | Keep; re-decode PLL/PVT | 1 |
| PLL | `0xC040_2000` | wrap size | Move from `0xC000_3000` | 1 |
| PVT | `0xC040_3000` | wrap size | Move from `0xC040_2000` | 1 |
| PLIC | `0xC400_0000` | 64 MiB | Move; expand MMIO ([D2](#d2--plic--clint--beu-align-now-or-later)) | 3 |
| CLINT | `0xC800_0000` | with BEU | Move from `0xC0C0_0000` | 3 |
| BEU[N] | `0xC801_0000 + N×0x1000` | — | Move from `0xC0C1_0000` | 3 |
| AOU CSRs | [D1](#d1--where-do-aou-csrs-live) | `0x80` | Must leave `0xC000_4000` | 1 |

---

## Decisions (locked 2026-08-18)

| ID | Choice | Meaning |
|----|--------|---------|
| **D1** | **A** | AOU CSRs park at VP-only `0xC000_C000` (must leave `0xC000_4000` for AVSBus) |
| **D2** | **P1** | PLIC/CLINT/BEU move to RTL bases in **Phase 3**; expand `mmio_hi` ≥ `0xC802_0000` |
| **D3** | **U1** | `SMC_UARTn_BASE` = 16550 base (`0xC000_6100 + N×0x400`) |
| **D4** | **H1** | Hand-written constants + `smc-map-coherence-test` |

Phase 1 ≠ Phase 3 in the same PR. Named stubs at correct RTL bases are OK.

### D1 — Where do AOU CSRs live?

AOU is **not** in `smc_top`. Today’s `0xC000_4000` collides with RTL AVSBus. SEP reaches the same CSRs at `0x4000_4000` via SMU remap (`addr | 0xC000_0000`), so the choice also moves `smu-aou-ext-test`.

| ID | Placement | Pros | Cons |
|----|-----------|------|------|
| **A** | VP-only gap `0xC000_C000`, window `0x80` | No RTL collision; inside today’s MMIO | Invented slot; labeled VP-only; move UART first |
| **B** | Inside `smc_external`, e.g. `0xC040_1000` | Adopter window | Still invented; RTL DECERR if FW runs on silicon |
| **C** | No SMC local map — SMU/D2D only | Honest | Breaks standalone `smc-aou-test` |
| **D** | Official RTL slot later | True SoT | Blocked on RTL; Phase 1 can still vacate `0x4000` |

**Locked:** **A** (Phase 1). **D** remains the long-term RTL goal. Reject leaving AOU at `0xC000_4000`.

### D2 — PLIC / CLINT / BEU: align now or later?

RTL places these **outside** the 16 MiB local-alias (`[0xC000_0000, 0xC100_0000)`):

- PLIC `0xC400_0000`
- CLINT `0xC800_0000`, BEU `0xC801_0000`

Today `cluster.mmio_hi = 0xC100_0000`, so `0xC400_0000` never enters fabric as MMIO.

| ID | Choice | Implication |
|----|--------|-------------|
| **P1** | Phase 3: expand `mmio_hi` ≥ `0xC802_0000`, move fabric + FW bases | Correct vs RTL; touches Zephyr/BEU/PLIC paths |
| **P2** | Keep current local-alias bases; document as **known VP alias** | Smaller near-term; still occupies RTL `ecam` @ `0xC080_0000` |

**Locked:** **P1** in Phase 3 (not same PR as Phase 1 peripheral shuffle).

### D3 — UART 16550 offset

| ID | Choice | Implication |
|----|--------|-------------|
| **U1** | `SMC_UARTn_BASE` = 16550 (`0xC000_6100 + N×0x400`) | `printf` / `start.S` keep `BASE + THR` |
| **U2** | `SMC_UARTn_BASE` = wrap (`0xC000_6000 + N×0x400`); FW adds `+0x100` | Easy to get wrong |

**Locked:** **U1**.

### D4 — How does VP stop drifting again?

| ID | Choice | Implication |
|----|--------|-------------|
| **H1** | Hand-written constants + `smc-map-coherence-test` vs checked-in `smc_addr.h` table | Low ceremony; Phase 1 |
| **H2** | Vendor `smc_top_regs.h` / `smc_addr.h` into FW + platform | Sync process when RDL moves |
| **H3** | Generator in this repo from `smc.rdl` | Best long-term; large up-front |

**Locked:** **H1**. **H2** on next harness drop (not blocking Phase 1).

---

## Proposed phases

Each phase is a **separate reviewable PR**. Do **not** combine Phase 1 and Phase 3.

### Phase 1 — Peripheral shuffle (inside today’s 16 MiB MMIO)

**Goal:** Modeled periphs in `[0xC000_2000, 0xC000_B800)` / `smc_external` at RTL bases; AOU vacates `0xC000_4000`.

1. Move AVSBus → `0xC000_4000`
2. Move OCTS → `0xC000_A000`
3. Move UART wrap → `0xC000_6000` with 16550 at `+0x100` (D3)
4. Move PLL → `0xC040_2000`, PVT → `0xC040_3000`
5. Drop extra `cpu_ctrl` @ `0xC040_0000`; FW uses `0xC003_9000` only
6. Park AOU at agreed slot (D1)
7. Named stubs for gpio / eFuse / DTP / `misc_wrap` (optional, useful for coherence)
8. Update `smc_common.h`, `printf.c` / `start.S`, CCI INI, READMEs
9. Add `smc-map-coherence-test`
10. Re-run every `sw/smc-vp-tests/` ELF + SMU AOU ext test

**Not in Phase 1:** PLIC/CLINT/BEU, fabric `periph_main` end, DFX window.

### Phase 2 — Fabric windows vs RTL xbar

1. `PERIPH_MAIN_END`: `0xC000_E800` → `0xC000_B800`
2. `DFT_CSR_*`: `0xC000_F800` → `0xC000_B800` (size `0x800`)
3. `DACCEL_DMA_ZEROER_END`: `0xC003_A000` → `0xC003_9000`
4. Fix fabric unit tests / docs; delete stale “verbatim xbar” comments (PLIC waits for Phase 3)

### Phase 3 — Core-local PLIC / CLINT / BEU (if D2 = P1)

1. `cluster.mmio_hi` → ≥ `0xC802_0000`
2. Fabric `FRONT_PLIC_*` / `FRONT_CLINT_BEU_*` → RTL
3. Update `SMC_PLIC_BASE` / `SMC_CLINT_BASE` / `SMC_BEU_BASE` + FW tests
4. Zephyr DTS / `mmio_poke` if needed
5. Confirm `ecam_region` @ `0xC080_0000` no longer swallowed by PLIC

### Phase 4 — Doc hygiene

Grep and fix stale “current map” claims (`0xC000_E000`, `0xC000_A000`, `0xC000_8000`, `0xC080_0000`, …) across integration guides, fabric docs, Zephyr/D2D, AOU/OCTS/AVSBus/PLL/BEU docs.

Detailed step lists: [`smc-vp-rtl-address-map-realignment-implementation.md`](smc-vp-rtl-address-map-realignment-implementation.md).

---

## File inventory (after approval)

### Platform and fabric

- `vp/platform/smc/src/smc_platform.cpp` — `A_*`, routers
- `vp/platform/smc/smc_platform.hpp` — comments
- `vp/platform/smc/main.cpp`, `config/smc_platform_vp.ini` — `mmio_hi` (Phase 3)
- `vp/platform/smu/main.cpp` — MMIO presets if duplicated
- `smc/smc_fabric/include/smc_fabric.h` — window constants (Phase 2/3)
- `smc/smc_fabric/test/smc_fabric_tb.cpp`

### Firmware and headers

- `sw/smc-vp-tests/common/smc_common.h` — **single FW header** for bases
- `common/printf.c`, `start.S`
- `smc-avsbus-test/`, `smc-octs-timer-test/`, `smc-octs-timer-secondary-test/`
- `smc-aou-test/`, `smc-pll-wrapper-test/`, `smc-pvt-wrap-test/`
- `smc-beu-test/`, `smc-beu-error-test/`
- `sw/smu-vp-tests/smu-aou-ext-test/`
- `sw/zephyr-smc/` if old bases
- **New:** `sw/smc-vp-tests/smc-map-coherence-test/`

### IP comments that bake absolute bases

- `avsbus_controller.h` (`0xC000_8000`)
- `octs_system_timer` README (`0xC000_E000`)
- `uart/`, `cpu_ctrl/`, BEU platform test plan docs

---

## Firmware: keep per-IP smokes, add a map-coherence ELF

Keep one ELF per IP.

Add **`smc-map-coherence-test`** (Phase 1) that:

1. Writes a distinctive token to each *modeled* window and reads it back (UART SCR, AVSBus, OCTS, telemetry, AOU `ip_version`, PLL/PVT).
2. Reads known RO reset values where they exist.
3. For each *stubbed* RTL slot (gpio, eFuse, DTP, DFX): read and check stub contract so a wrong model on that slot fails.
4. Walks `{name, base, kind}` from the same literals as `smc_common.h`.

Cheap substitute for vendored `smc_addr.h` until D4/H2.

---

## Risks and non-goals

- **Silent dual-map** — platform C++ and `smc_common.h` must move in one PR.
- **UART sub-decode** — wrap base without `+0x100` → hung console; lock U1/U2 first.
- **PLIC outside 16 MiB** — Phase 3 without expanding `mmio_hi` “loses” PLIC.
- **SEP/SMU AOU** — parking at `0xC000_C000` may imply `0x4000_C000` on SEP; confirm remap.
- **Do not** invent a second `smc_top` in this repo — remap copies RTL numbers; it is not a new SoT.

---

## Review checklist

Please answer explicitly in review:

- [ ] Confirm `hw/sys/smc/regs/smc.rdl` `addrmap smc_top` is the SoT for VP bases
- [ ] D1 AOU: A / B / C / D (recommend A now, D later)
- [ ] D2 PLIC/CLINT/BEU: P1 or P2
- [ ] D3 UART: U1 or U2
- [ ] D4 drift: H1 / H2 / H3
- [ ] Phase split OK (1 ≠ 3 in same PR)
- [ ] Named stubs at unmodeled RTL slots OK in Phase 1
- [ ] `smc-map-coherence-test` wanted in Phase 1

---

## References

| Topic | Path |
|-------|------|
| RTL register map | `tt-oca-harness/hw/sys/smc/regs/smc.rdl` |
| RTL periph xbar | `tt-oca-harness/hw/sys/smc/rtl/crossbars/smc_periph_axi_lite_xbar_pkg.sv` |
| RTL local xbar | `tt-oca-harness/hw/sys/smc/rtl/crossbars/smc_local_xbar_pkg.sv` |
| RTL external decode | `tt-oca-harness/hw/top/smc_ip_integration.sv` |
| VP map today | `vp/platform/smc/src/smc_platform.cpp`, `sw/smc-vp-tests/common/smc_common.h`, `smc/smc_fabric/include/smc_fabric.h` |
| Implementation steps | [`smc-vp-rtl-address-map-realignment-implementation.md`](smc-vp-rtl-address-map-realignment-implementation.md) |
| Docs to update (Phase 4) | `systemc_tlm2_integration_guide.adoc`, `smc-sep-d2d-interconnect.adoc`, `05_RTL_SystemC_Integration_Guide.md` |
