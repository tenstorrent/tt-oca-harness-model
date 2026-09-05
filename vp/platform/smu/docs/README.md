# SMU Virtual Platform (`smu-vp`)

`smu-vp` integrates the SMC (`vp/platform/smc`) and SEP (`vp/platform/sep`)
virtual platforms into one SystemC process, connected by the SMU on-die
interconnect (RTL reference: `hw/smu/rtl/smu.sv`). It exists to run
SMC and SEP firmware that talks to each other, and to carry chiplet-facing
AXI through the AOU (AXI-over-UCIe) LT stub on `ext_in`/`ext_out` (RTL
`smu_axi_in`/`smu_axi_out`; see `tt-oca-hw` `doc/architecture.adoc`).

Directory shape matches `vp/platform/smc` and `vp/platform/sep`:

```
main.cpp                  sc_main
smu_platform.hpp          platform composition
src/smu_platform.cpp      socket wiring
inc/                      interconnect models
config/                   CCI inis
docs/                     this folder
test/ + run_tests.sh      interconnect unit suite (SMU-only extra)
```

## Topology

```
SEP (och_sep_ss1)                              SMC (dut)
sep_ext_to_smc_axi --[axi_window_remap]--> sep_axi_in     (dedicated path)
sep_smn_inbound_axi <---[smu_axi_xbar]----- output_axi    (crossbar path)
sep_smn_outbound_axi -->[smu_axi_xbar]----> sys_axi_in / ext_out
                       ---[smu_axi_xbar]----> sys_axi_in

                       smu_axi_xbar.ext_out --> aou_axi_s (local AOU TX)
                       smu_axi_xbar.ext_in  <-- aou_axi_m (local AOU RX)
                       aou_peer_  ==>  remote-die AOU stub (axi_m -> stub_sysmem)
```

* **Dedicated SEP→SMC path** — SEP CPU accesses to its SMC global window
  `[0x4000_0000, +2 MiB)` forward through `sep_smc_global_port`
  (`forward_en`), which re-adds the window base so the boundary carries
  global addresses like the RTL port. `axi_window_remap` rebases
  `[0x4000_0000, +1 GiB)` to alias `0x0` (RTL `SEP_SMC_REGION_*`), and the
  SMC fabric's `sep_axi_in` maps the alias into the SMC local map
  (`0xC000_0000 | (addr & 0x1FF_FFFF)`).
* **Crossbar paths** — `smu_axi_xbar` is a 3x3 non-reflexive router with
  CCI-programmable apertures: `smc_out`→`sep_in`/`ext_out`,
  `sep_out`→`smc_in`/`ext_out` (SEP `smn_outbound_axi`, after the outbound
  filter), `ext_in`→`sep_in`/`smc_in` (D2D inbound). `ext_out` is the static
  catch-all, as on RTL.
* **D2D (AoU stub)** — `ext_out`/`ext_in` bind through the local AOU
  (`dut.aou_axi_s` / `dut.aou_axi_m`). The peer AOU inside `smc_platform`
  is the remote-die stand-in. Firmware must write `aou_init.activate_start`
  before catch-all traffic forwards (both cores ENABLED).
* **SMC→SEP** — SMC CPU traffic to the SEP global aperture (default base
  `0x5000_0000`) exits via the fabric's `output_axi`, crosses the xbar to
  `sep_in`, and the SEP's inbound remap subtracts `sep_global_base` to
  produce a SEP-local bus address.

## Building

`smu-vp` builds as part of the VP tree when `WHISPER_HOME` is set (same
dependencies as `smc-vp` — see `sw/smc-vp-tests/README.md`):

```bash
cmake -S vp -B vp/build_smc -DCMAKE_BUILD_TYPE=Release   # plus SYSTEMC_HOME/CCI_HOME/WHISPER_HOME env
cmake --build vp/build_smc --target smu-vp -j
# binary: vp/build_smc/bin/smu-vp
```

The SMC CPU cluster + Whisper archives are linked into `smu-vp` as a shared
library (`libsmc_cluster_smu`) with Whisper's symbols hidden, so the two
WdRiscv forks (Whisper for the SMC cluster, VeeR-ISS for the SEP) link
cleanly into one executable.

## Running

```
smu-vp <smc-cci-ini> <smc-elf> <sep-cci-ini> <sep-elf> [sim_time_ms]
```

Example (from `sw/smu-vp-tests/smu-link-test/`):

```bash
../../../vp/build_smc/bin/smu-vp \
    ../../../vp/platform/smu/config/smc_smu_vp.ini smu_link_smc \
    ../../../vp/platform/smu/config/sep_smu_config.ini smu_link_sep 50
```

Both subsystems keep their standalone CCI namespaces: the SMC platform is
`dut` (smc-vp inis apply unchanged) and the SEP platform is `och_sep_ss1`
(sep-vp inis apply unchanged; `smu-vp` overrides `targets` with the
`<sep-elf>` argument). Relative paths inside the SEP ini (`@include`,
`configFile`) resolve against the ini's own directory — run with the ini in
place, not a copy elsewhere.

The process-wide TLM quantum is installed once in `smu-vp` `sc_main` before
`smu_platform` constructs the SMC (`dut`) or SEP (`och_sep_ss1`) instances.
Resolution: `global_quantum_ns`, else `och_sep_ss1.globalQuantumNs`, else
`dut.cluster.quantum_ns`, else `simtlm::DEFAULT_GLOBAL_QUANTUM_NS` (1000 ns /
1 µs). Both ISS sides and standalone `sep-vp` / `smc-vp` use that same
default. Neither ISS overwrites a quantum already installed by `sc_main`.

`dut` is the CCI hierarchical name of the SMC platform (same as standalone
`smc-vp`), so `dut.cluster.*` keys from smc-vp INIs apply unchanged.

`smu-vp` applies these integration presets after loading both inis:

| Preset | Value | Why |
|---|---|---|
| `och_sep_ss1.smc_global.forward_en` | `true` | SEP SMC-window traffic forwards to the real SMC instead of the fallback stub |
| `och_sep_ss1.sep_global_base` | `0x5000_0000` | SEP inbound remap base — forced equal to `smu_xbar.sep_global_base` (the RTL CSR contract) |
| `smu_xbar.sep_global_base` / `smc_global_base` | `0x5000_0000` / `0x4000_0000` | crossbar aperture bases |
| `smu_xbar.sep_region_size` / `smc_region_size` | 16 MiB defaults | applied *before* the SMC ini so `smc_smu_vp.ini` may widen them |

The shipped `config/smc_smu_vp.ini` widens `smu_xbar.sep_region_size` to
512 MiB (so the aperture covers the SEP SRAM alias at
`0x5000_0000 + 0x1000_0000`) and lowers `dut.cluster.mmio_lo` to
`0x4000_0000` (so SMC CPU accesses to the global apertures route into the
fabric instead of the cluster's stubbed data socket).

### SMC scratchpad caveat

The SMC fabric's coarse FRONT_SPM decode starts at `0xC004_0000`, but the
platform front-port router places the **boot ROM at `0xC004_0000`–`0xC006_0000`**
and the **scratchpad RAM at `0xC006_0000`**. SEP→SMC writes aimed at the
scratchpad must use window offset `0x6_0000+` (e.g. SEP address
`0x4006_1000` → SMC-local `0xC006_1000`); writes in the `0x4_xxxx` alias
slot land in the ROM and vanish (reads return 0).

## Tests

* **Unit tests** (`test/`, `run_tests.sh`) — `smu_axi_xbar` and
  `axi_window_remap` route/remap/error/debug coverage. Release, ASan, and
  coverage runs follow the repo's three-separate-runs rule:

  ```bash
  ./run_tests.sh             # Release
  ./run_tests.sh --asan      # ASan+UBSan, leak-clean
  ./run_tests.sh --coverage  # >=95% line coverage gate
  ```

* **Platform tests** (`sw/smu-vp-tests/`) — self-checking SMC+SEP firmware
  ports of the tt-oca-hw SMU interconnect tests (`smu-link-test`,
  `smu-xbar-test`, `smu-traffic-test`, `smu-aou-ext-test`). Run via
  `sw/smu-vp-tests/run_smu_vp_tests.sh`. CI runs both the unit tests and
  this firmware suite (`smu-unit-tests` / `smu-vp` on Ubuntu,
  `smu-unit-tests-rhel8` / `smu-vp-rhel8` on RHEL 8).
