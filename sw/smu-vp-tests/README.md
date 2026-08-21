# SMU VP Software Tests

Bare-metal firmware tests for the SMU Virtual Platform (`smu-vp`), which
integrates the SMC and SEP platforms over the SMU on-die interconnect
(see `vp/platform/smu/docs/README.md`). Each test builds **two** firmware
halves — an RV64 image for the SMC CVA6 cluster and an RV32 image for the
SEP VeeR core — reusing the `sw/smc-vp-tests/` and `sw/sep-vp-tests/`
startup/printf/linker infrastructure, and runs them concurrently on
`smu-vp`.

These are VP-adapted ports of the tt-oca-hw SMU firmware/DV tests that
exercise interconnect behaviour the VP actually models. RTL-only
machinery (CLA, fuse-sense, AXI filter programming, cocotb `ext_in`
master, mailbox, SPI/WDT/OTBN) is omitted; see the skip list at the
bottom.

## Dependencies

Same as `sw/smc-vp-tests/` (RISC-V GNU toolchain, SystemC/CCI, Whisper,
Boost) — see `sw/smc-vp-tests/README.md`. The toolchain prefix defaults to
`riscv64-unknown-elf-`; on Homebrew use `RISCV_PREFIX=riscv64-elf-`. The
runner auto-detects a few common prefixes.

## Running

```bash
# All SMU tests:
./run_smu_vp_tests.sh

# A single test:
./run_smu_vp_tests.sh smu-link-test
```

Environment overrides: `VP` (smu-vp binary), `SMC_INI`, `SEP_INI`,
`SIM_TIME_MS` (default 50 ms), `RISCV_PREFIX`, `RISCV_TOOLCHAIN_PATH`.

CI (Ubuntu and RHEL 8) runs the interconnect unit tests
(`vp/platform/smu/run_tests.sh`, Release / ASan / coverage on Ubuntu;
Release / coverage on RHEL) and this firmware suite via `smu-vp` /
`smu-vp-rhel8` jobs.

A test passes when **both** firmware halves print their PASS banner (SMC on
UART0, SEP on the virtconsole) and neither prints FAIL.

## Tests

| Test | tt-oca-hw source | What it exercises |
|---|---|---|
| `smu-link-test` | (VP original) | Bidirectional SMU on-die link: SEP→SMC over the dedicated `sep_ext_to_smc_axi` path (SEP writes magic+doorbell into the SMC scratchpad via the `0x4000_0000` window; SMC polls it), then SMC→SEP over the SMU crossbar (SMC writes a response into SEP SRAM via the `0x5000_0000` global aperture; SEP polls it). The SEP also reads its own writes back through the dedicated path to prove the forward path (not the fallback stub) carried them. |
| `smu-xbar-test` | `fw/smc/tests/smc_sep_xbar` + `smu_bidirect` / `fw/sep/tests/sep_smu_bidirect` | Same two AXI paths with the hardware handshake tokens (`0x13579BDF` / `0xC001CAFE` / `0x5E9ACCE5` / `0xD0E0F00D`). CLA / fuse-sense / filter programming omitted. |
| `smu-traffic-test` | DV `smc_cpu_traffic_sep_axi_test` | 50 SMC→SEP write+readback beats through `output_axi` → xbar → SEP SRAM. |
| `smu-aou-ext-test` | `fw/smc/tests/smu_sep_ext_axi` | Activate AOU, then both CPUs write/readback a catch-all address through `xbar.ext_out` → local AOU → peer → `stub_sysmem`. SEP also reads AOU `ip_version` through the dedicated SMC window (`0x4000_C000`) and programs the outbound filter before its `smn_outbound` beat. |

## Address plan (smu-link-test)

| Producer | Address | Path | Consumer-visible address |
|---|---|---|---|
| SEP | `0x4006_1000` / `0x4006_1004` | SEP bus → `smc_global` (forward) → `sep_ext_to_smc_axi` → remap (−`0x4000_0000`) → `sep_axi_in` → fabric alias | SMC scratchpad `0xC006_1000` / `0xC006_1004` |
| SMC | `0x6000_8000` | cluster mmio → fabric outbound → `output_axi` → xbar (SEP aperture hit) → `sep_in` → SEP inbound remap (−`0x5000_0000`) | SEP SRAM `0x1000_8000` |

Note the SMC scratchpad sits at `0xC006_0000`, not `0xC004_0000` (the
`0xC004_0000` slot is the boot ROM) — see the caveat in
`vp/platform/smu/docs/README.md`.

## Address plan (smu-aou-ext-test)

Hardware uses catch-all `0x8000_1000`. On the VP that address is inside
cluster fast-mem `[0x8000_0000, 0x9000_0000)`, so the store would never
leave the ISS. The VP test uses `0xA000_1000` (MMIO window, misses both
SMU apertures). SEP uses `0xA000_1008` on the same page after programming
outbound filter 0, and reads AOU `ip_version` at `0x4000_C000`.

## Not ported (VP does not model the stimulus)

- Cocotb pin-toggle / CLA / fuse / filter-programming / mailbox tests
- `sep_load_and_run_binary_test` (SMC mailbox stub)
- Remap / SPI / WDT / OTBN / AES / efuse SMU tests
- Cocotb `ext_in` master (peer `axi_s` is idle-terminated)
