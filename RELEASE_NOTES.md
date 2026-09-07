# Release Information

- Version: 2.1
- Release date: 15-Aug-2026
- Notes updated: 07-Sep-2026
- Release name: release_2.1

# Release Details

## Included Models

All functional IPs from the OCH / `tt-oca-harness` SMU stack are modeled in this
virtual platform, except Debug and Test Ports (DTP). See
[Current Limitations](#current-limitations).

Platforms: `smc-vp`, `sep-vp`, `smu-vp`.

### Cores and memories

- RISC-V cores: SMC CPU cluster (Whisper ISS), SEP VeeR EL2
- Memories: SMC boot ROM and scratchpad SRAM; SEP SRAM, ROM, ITCM, DTCM; SEP cold/warm scratch

### SMC

- Interconnect: `smc_fabric` (local/global decode, remap, filters, inbound ports)
- Interrupt and timers: PLIC, CLINT, OCTS system timer, per-core WDT, per-core BEU
- Reset and CPU control: reset unit, `cpu_ctrl`
- Peripherals: UART (×4), I2C (×3), I3C (×6), DMA, memory zeroer, AVSBus, telemetry receiver (×3), PLL wrapper, PVT wrapper
- Integration: AoU core; DFT CSR and DFD APB windows are RAZ/WI stubs so MMIO decode does not hang

### SEP

- Interconnect and interrupt: SimpleBus (enforcing the SEP crossbar's per-initiator connectivity matrix), EL2 PIC, local-master alias remap, output remap, filter control
- Reset and CPU control: `sep_reset_ctrl`, `sep_cpu_ctrl`
- Crypto and entropy: Adams Bridge (ML-DSA-87 / ML-KEM-1024), HMAC, KMAC, OTBN, CSRNG, AES, Key Manager, EDN, Entropy Source
- Peripherals: secure DMA, mailbox, AON timer, eFuse/OTP, lifecycle controller, SPI controller (OpenTitan), SPI Flash (SFDP Profile 1)
- Integration: SEP↔SMU AXI boundary (`smn_inbound_axi`, `smn_outbound_axi`, `sep_ext_to_smc_axi`), with the inbound-window CSRs exported so the SMU crossbar sizes its SEP aperture from them; OTP key-request stub, mailbox host stub, MailboxBridge, `dma_sys_bus_adapter`, stdout / virt-console / SEP status report

## Testing Status

- Modeled peripheral IPs are unit tested (SEP: `sep/peripherals/run_all_peripherals.sh`; SMC: `smc/run_all_smc_tests.sh`). `sep_memory` is excluded from the SEP orchestrator by design (no standalone coverage build).
- SEP CPU (VeeR-ISS TLM wrapper) has standalone tests at `sep/cpu/` (`./run_tests.sh`, `--asan`, `--coverage`). Public CI runs those three invocations after the SEP peripheral suite.
- Firmware tests under `sw/sep-vp-tests/` run successfully on the VP.
- TT firmware tests under `sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests/`
  - Tests exercising modeled SEP IPs run successfully on the VP
  - Tests that require DTP (JTAG / iJTAG / JTAG2AXI / cross-trigger) are not exercised

Each orchestrator runs **three separate** builds: Release, ASan+UBSan, and coverage. ASan and coverage must not be combined. The coverage gate is **≥ 95% line coverage** on model `src/` (and `include/` / `algo/` where the IP extracts those). A figure below 95% fails the stage.

### Tested on

- Ubuntu 22.04 LTS
- RHEL 8.10
- macOS (Apple Clang; Tahoe 26.x)

### Compiler versions tested

- GCC 11.4.0 / 11.2, C++20 (default; required for `smc-vp` / `smu-vp`)
- GCC 11.4.0 / 9.5.0, C++17 (SEP / Accellera flow only)
- Apple Clang, C++20 (macOS)

Public CI (`.github/workflows/ci.yml` and `ci-rhel8.yml`) pins:

| Library | Version |
|---------|---------|
| SystemC | 3.0.2 |
| CCI | 1.0.2 |
| C++ standard | 20 |
| Boost | ≥ 1.74 (`iostreams`, `program_options`); RHEL 8 CI builds **1.84.0** |
| OpenSSL | 3.x (≥ 3.0); RHEL 8 CI builds **3.3.2** |
| Whisper | `a53d0f3e` |
| Zephyr | v4.3.0 |
| RISC-V GCC | xpack **15.2.0-1** (`riscv-none-elf-`) |

RHEL 8 ASan uses `gcc-toolset-12-libasan-devel` and `gcc-toolset-12-libubsan-devel`; ASan is **not** skipped on RHEL 8. CCI 1.0.1, Boost.Regex, Boost.Log, and libvncserver are not required.

### Peripheral unit tests and coverage

Coverage percentages are produced by the orchestrators (not a checked-in snapshot). Re-run them for current numbers:

```bash
sep/peripherals/run_all_peripherals.sh          # Release + ASan + coverage
smc/run_all_smc_tests.sh                        # same four stages (Release / ASan / coverage / CTest)
sep/cpu/run_tests.sh && sep/cpu/run_tests.sh --asan && sep/cpu/run_tests.sh --coverage
```

#### SEP (`sep/peripherals/run_all_peripherals.sh`)

All of the following IPs are in the orchestrator (and in public CI):

`adams_bridge`, `aes`, `aon_timer`, `csrng`, `edn`, `efuse`, `el2_pic`, `entropy_src`, `hmac`, `key_manager`, `kmac`, `lifecycle_ctrl`, `local_master_alias_remap_ctrl`, `mailbox`, `otbn`, `secure_dma`, `sep_cpu_ctrl`, `sep_filter_ctrl`, `sep_output_remap_ctrl`, `sep_reset_ctrl`, `sep_scratch_cold`, `sep_scratch_warm`, `spi_controller`, `spi_flash`.

- `sep_memory` is skipped by design (`SKIP=("sep_memory" "cpu")`).
- `sep_status_report` and `sep_virt_console` have no standalone suites. Both decoders live inside `sep_scratch_cold`, tapped on `COLD_SCRATCH[1]` and `COLD_SCRATCH[2]` (the registers the ROM's `STATUS_OUT()` and `simput*()` write).
- `sep_scratch_warm` is a store-only stub with a standalone `run_tests.sh`.

#### SMC (`smc/run_all_smc_tests.sh`)

Local default set: `avsbus_controller`, `beu`, `bootrom`, `clint`, `i2c_controller`, `cpu_ctrl`, `i3c_controller`, `dma`, `memory_zeroer`, `pll_wrapper`, `pvt_wrap`, `plic`, `reset_unit`, `scratchpad_ram`, `telemetry_receiver`, `uart`, `wdt`, `smc_fabric`, `octs_system_timer`, `aou`. `cpu_cluster` is added automatically when `WHISPER_HOME` and Boost are present.

Public CI currently runs a subset:

- `smc-unit-tests`: `bootrom`, `clint`, `dma`, `i3c_controller`, `plic`, `pvt_wrap`, `reset_unit`, `scratchpad_ram`, `telemetry_receiver`, `aou`
- `smc-fabric-tests`: `smc_fabric`

`cpu_cluster` is not in the public CI matrix (requires Whisper).

**Notes**

- RHEL: gcc/gcov; Ubuntu: gcc/gcovr; macOS: clang/llvm-cov (SMC) or clang/llvm-prof + Homebrew `lcov` (SEP).
- Coverage and ASan use isolated build directories (`build_cov/`, `build_asan/`). Never reuse `build/` for an instrumented run.

---

# Current Limitations

- **Unmodeled IP — Debug and Test Ports (DTP)**: The hardware DTP block
  (`hw/dtp` in `tt-oca-harness`) is not modeled. That includes the IEEE 1149.1 PTAP
  and STAPs, iJTAG / boundary-scan / DFT scan chains, JTAG2AXI, JTAG OTP
  AXI-Lite, IC_RESET TDRs, and the cross-trigger network. The VP uses ISS GDB
  for software debug; `jtag_axi_in` is present but idle. Tests or flows that
  require pin-level JTAG, scan, or DTP CSRs are out of scope.
- **Unmodeled IPs**: Tests referencing the following will fail or produce no output:
  - `och_sep_cdns_spi_ctrl`
- **eFuse fuse state comes from an image, not per-field parameters**: the array is
  loaded from a `.preload` file in the RTL's `+sep_preload_efuse` format, and a
  platform run defaults to the RTL's own `default_efuse.preload`, so the VP starts
  from the same part the RTL does. The per-field parameters (`lc_state`, `locks_lo`,
  `chiplet_uid`, ...) remain for standalone peripheral testbenches, but an image and
  those parameters are alternatives rather than a base and an overlay: when
  `fuse_preload_file` is set the image defines the array and the parameters are
  ignored. Combining them could only mean ORing, since fuses go 0->1, and ORing two
  encodings of one field corrupts it — `LC_STATE` holds `{~raw[3:0], raw[3:0]}`, so
  0xF0 (TEST_DEV) ORed with the encoding of raw 1 gives 0xF1, which is not a legal
  code at all. To run with different fuses, change the image.
- **Lifecycle state has one source**: `sep_lifecycle_ctrl.sv` reads it from the eFuse
  shadow, so `och_sep_ss` now wires `lc_ctrl` from the eFuse model and `FEAT_CTRL` is
  computed from the fuse. `och_sep_ss1.lc_ctrl.lc_state` no longer applies on a
  platform run — it is only for a standalone lifecycle testbench, where there is no
  eFuse to ask. Note that an erased array reads 0x00, which is not a legal
  differential code, so a blank part has no valid lifecycle state and `FEAT_CTRL`
  comes up zeroed; that matches the RTL.
- **Mailbox 64-bit register halves**: a 32-bit write to the upper half of any
  mailbox register other than `WRITE_DATA` is refused with an error response.
  `WRITE_DATA` and `READ_DATA` follow the RTL, pushing and popping once per bus
  transaction with the unaddressed half zeroed; the remaining registers would
  need byte-enable-aware forwarding to be updated a half at a time.
- **Stubbed IP — `och_sep_spi_mux_ctrl`**: mapped at `0x20001000` as a functional RW
  register stub that reads back the `0x00000002` silicon reset default, so a driver's
  mux-select write does not fault. SPI leg selection and forced chip-select are not
  modeled.
- **Key Manager**: unit-level testing only. There is no dedicated keymgr firmware
  test; sideload sequences in KMAC/OTBN tests may exercise the hardware port.

# GCC and C++ Compatibility

| CXX_STD  | Compiler                    | SYSTEMC_API| Status         |
|----------|-----------------------------|------------|----------------|
| c++17    | gcc-toolset-9 (GCC 9.2)     | cxx201703L |  OK (SEP / Accellera) |
| c++17    | system GCC 8.5              | cxx201703L |  OK (SEP / Accellera) |
| c++20    | gcc-toolset-11+ (GCC 11.2)  | cxx202002L |  OK (default; required for SMC/SMU) |
| c++20    | system GCC 8.5              | cxx201709L |  Not Supported |
| c++20    | gcc-toolset-9 (GCC 9.2)     | cxx201709L |  Not Supported |

`vp/configure_vp.sh` defaults to **C++20**. Point `SYSTEMC_HOME` at a SystemC 3.0.2
tree built with the same `-std=c++NN`. Mismatches fail at link time with an
undefined `sc_api_version_*` symbol.
