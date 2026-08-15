# Release Information

- Version: 2.1
- Release date: 15-Aug-2026
- Release name: release_2.0

# Release Details

## Included Models

All functional IPs from the OCH / `tt-oca-hw` SMU stack are modeled in this
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

- Interconnect and interrupt: SimpleBus, EL2 PIC, local-master alias remap, output remap, filter control
- Reset and CPU control: `sep_reset_ctrl`, `sep_cpu_ctrl`
- Crypto and entropy: HMAC, KMAC, OTBN, CSRNG, AES, Key Manager, EDN, Entropy Source
- Peripherals: secure DMA, mailbox, AON timer, eFuse/OTP, lifecycle controller, SPI controller (OpenTitan), SPI Flash (SFDP Profile 1)
- Integration: OTP key-request stub, mailbox host stub, MailboxBridge, `dma_sys_bus_adapter`, stdout / virt-console / SEP status report

## Testing Status

- All modeled peripheral IPs are unit tested (SEP: `sep/peripherals/run_all_peripherals.sh`; SMC: `smc/run_all_smc_tests.sh`)
- Firmware tests written by Vayavya under `sw/sep-vp-tests/` run successfully on the VP
- TT firmware tests under `sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests/`
  - Tests exercising modeled SEP IPs run successfully on the VP
  - Tests that require DTP (JTAG / iJTAG / JTAG2AXI / cross-trigger) are not exercised

### Tested on

- Ubuntu 22.04 LTS
- RHEL 8.10
- macOS 26.5.1 (Tahoe)

### Compiler versions tested

- GCC 11.4.0, C++17 (Accellera flow)
- GCC 9.5.0, C++17 (Accellera flow)
- GCC 11.2, C++20 (Accellera flow)

### Peripheral unit test & line coverage

All listed IPs **PASS** Release, Coverage, and CTest on the hosts below (ASAN is skipped on RHEL 8).  
`—` means that IP was not in the coverage job on that host. `n/a` means tests passed but the coverage report was not produced.

#### SEP (`sep/peripherals/run_all_peripherals.sh`)

RHEL 8.10 and Ubuntu 22.04 from CI on 13-Aug-2026 (`lcov` / `make coverage`).  
macOS from the 30-Jun-2026 campaign (clang / llvm-prof + Homebrew `lcov`).

| Peripheral                   | RHEL 8.10 | macOS   | Ubuntu 22.04 |
|------------------------------|----------:|--------:|-------------:|
| aes                          | 92.8%     | 92.6%   | 92.6%        |
| aon_timer                    | 98.5%     | 98.1%   | 98.5%        |
| csrng                        | 92.2%     | 91.3%   | 92.2%        |
| edn                          | 93.1%     | 93.0%   | 93.1%        |
| efuse                        | 100.0%    | 100.0%  | 100.0%       |
| el2_pic                      | 93.3%     | —       | 93.3%        |
| entropy_src                  | 91.4%     | 92.0%   | 91.4%        |
| hmac                         | 92.1%     | 91.5%   | 92.1%        |
| key_manager                  | 98.0%     | 97.9%   | 98.0%        |
| kmac                         | 80.9%     | 92.2%   | 80.9%        |
| lifecycle_ctrl               | 98.1%     | 97.5%   | 98.1%        |
| local_master_alias_remap_ctrl| 100.0%    | —       | 100.0%       |
| mailbox                      | 98.0%     | 97.3%   | 98.0%        |
| otbn                         | 91.9%     | 92.2%   | 91.9%        |
| secure_dma                   | 92.0%     | 92.1%   | 92.0%        |
| sep_cpu_ctrl                 | 100.0%    | —       | 100.0%       |
| sep_filter_ctrl              | 94.4%     | —       | 94.9%        |
| sep_output_remap_ctrl        | 97.8%     | —       | 97.8%        |
| sep_reset_ctrl               | 100.0%    | —       | 100.0%       |
| sep_scratch_cold             | 87.6%     | —       | 90.1%        |
| sep_scratch_warm             | n/a       | —       | n/a          |
| sep_status_report            | n/a       | —       | n/a          |
| sep_virt_console             | n/a       | —       | n/a          |
| spi_controller               | 95.6%     | 95.9%   | 95.6%        |
| spi_flash                    | 97.3%     | 98.4%   | 97.3%        |

#### SMC (`smc/run_all_smc_tests.sh`)

RHEL 8.10 and Ubuntu 22.04 from CI on 13-Aug-2026 (`gcovr` TOTAL, integer %).  
macOS from `./run_tests.sh --coverage` llvm-cov **src/** line coverage (15-Aug-2026, plus earlier reports still on disk).

| Peripheral          | RHEL 8.10 | macOS   | Ubuntu 22.04 |
|---------------------|----------:|--------:|-------------:|
| avsbus_controller   | —         | 100.0%  | —            |
| beu                 | —         | 100.0%  | —            |
| bootrom             | 91%       | 100.0%  | 91%          |
| clint               | 96%       | 96.7%   | 96%          |
| cpu_cluster         | —         | 84.7%   | —            |
| cpu_ctrl            | —         | 96.2%   | —            |
| dma                 | 98%       | 97.5%   | 98%          |
| i2c_controller      | —         | 100.0%  | —            |
| i3c_controller      | 96%       | 96.3%   | 96%          |
| memory_zeroer       | —         | 90.9%   | —            |
| octs_system_timer   | —         | 94.1%   | —            |
| plic                | 97%       | 97.9%   | 97%          |
| pll_wrapper         | —         | 84.8%   | —            |
| pvt_wrap            | 97%       | 97.3%   | 97%          |
| reset_unit          | 98%       | 98.6%   | 98%          |
| scratchpad_ram      | 96%       | 98.5%   | 96%          |
| smc_fabric          | 97%       | 98.1%   | 97%          |
| telemetry_receiver  | 98%       | 99.5%   | 98%          |
| uart                | —         | 99.8%   | —            |
| wdt                 | —         | 97.0%   | —            |

**Notes**
- RHEL: gcc/gcov; Ubuntu: gcc/gcovr; macOS: clang/llvm-cov (SMC) or clang/llvm-prof + `lcov` (SEP).
- SMC CI currently covers a subset of IPs (`bootrom`, `clint`, `dma`, `i3c_controller`, `plic`, `pvt_wrap`, `reset_unit`, `scratchpad_ram`, `telemetry_receiver`, `smc_fabric`).
- `cpu_cluster` coverage is from the 22-Jul-2026 llvm-cov report (`src/smc_cpu_cluster.cpp` + `src/iss_backend_whisper.cpp`); it is not in the public CI matrix (requires Whisper).

---

# Issues Fixed
Enhancement Request for macOS support

# Current Limitations

- **Unmodeled IP — Debug and Test Ports (DTP)**: The hardware DTP block
  (`hw/dtp` in `tt-oca-hw`) is not modeled. That includes the IEEE 1149.1 PTAP
  and STAPs, iJTAG / boundary-scan / DFT scan chains, JTAG2AXI, JTAG OTP
  AXI-Lite, IC_RESET TDRs, and the cross-trigger network. The VP uses ISS GDB
  for software debug; `jtag_axi_in` is present but idle. Tests or flows that
  require pin-level JTAG, scan, or DTP CSRs are out of scope.
- **Key Manager**: unit-level testing only (no DV tests)


# GCC and C++ Compatibility

| CXX_STD  | Compiler                    | SYSTEMC_API| Status         |
|----------|-----------------------------|------------|----------------|
| c++17    | gcc-toolset-9 (GCC 9.2)     | cxx201703L |  OK            |
| c++17    | system GCC 8.5              | cxx201703L |  OK            |
| c++20    | gcc-toolset-11 (GCC 11.2)   | cxx202002L |  OK            |
| c++20    | system GCC 8.5              | cxx201709L |  Not Supported |
| c++20    | gcc-toolset-9 (GCC 9.2)     | cxx201709L |  Not Supported |
