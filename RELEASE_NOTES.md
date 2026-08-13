# Release Information

- Version: 2.1
- Release date: 29-May-2026
- Release name: release_2.0

# Release Details

## Included Models

- RISC-V core model (VeeR EL2)
- Memories: SRAM, ROM, ITCM, DTCM
- Interconnect and interrupt controllers: SimpleBus, PLIC, CLINT
- Reset and infrastructure: Reset generation unit (RSU), stdout device
- Peripherals: DMA, HMAC, KMAC, OTBN, CSRNG, AES, SPI controller(Open Titan), SPI Flash(SFDP Profile 1 commands), Key Manager, EDN, Entropy Source, Mailbox, AON Timer, SEP Efuse/OTP, Lifecycle Controller, EL2 PIC, SEP CPU Ctrl, SEP Reset Ctrl, SEP Filter Ctrl (inbound/outbound), SEP Output Remap Ctrl (AP/STEE), Local Master Alias Remap Ctrl, SEP Scratch Cold, SEP Scratch Warm
- Integration stubs/Adapters: OTP key request stub, Mailbox host stub, MailboxBridge, dma_sys_bus_adapter


## Testing Status

- All peripheral models are unit tested
- Firmware tests written by Vayavya under `sw/sep-vp-tests/` run successfully on the VP
- TT firmware tests under `sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests/`
  - Tests exercising the current sep models run successfully on the VP
  - Tests exercising the sep models which are currently not part of the VP are not tested

### Tested on

- Ubuntu 22.04 LTS
- RHEL 8.10
- macOS 26.5.1 (Tahoe)

### Compiler versions tested

- GCC 11.4.0, C++17 (Accellera flow)
- GCC 9.5.0, C++17 (Accellera flow)
- GCC 11.2, C++20 (Accellera flow)

### Peripheral unit test & line coverage (`run_all_peripherals.sh`)

All peripherals **PASS** Debug, ASAN, Coverage, and CTest on **RHEL 8.10** and **macOS** (Jun 30, 2026).  
Line coverage % from merged `lcov` reports (`make coverage` / `run_tests.sh --coverage --clean`).

| Peripheral                        | RHEL 8.10 | macOS   | Ubuntu 22.04 |
|------------------------------------|----------:|--------:|-------------:|
| aes                                | 91.9%     | 92.6%   | 92.6%        |
| aon_timer                          | 98.5%     | 98.1%   | 98.5%        |
| csrng                              | 91.4%     | 91.3%   | 92.2%        |
| edn                                | 92.6%     | 93.0%   | 93.1%        |
| efuse                              | 100.0%    | 100.0%  | 100.0%       |
| el2_pic                            |           |         | 93.3%        |
| entropy_src                        | 89.8%     | 92.0%   | 91.4%        |
| hmac                               | 91.7%     | 91.5%   | 92.1%        |
| key_manager                        | 97.8%     | 97.9%   | 98.0%        |
| kmac                               | 91.9%     | 92.2%   | 80.9%        |
| lifecycle_ctrl                     | 97.9%     | 97.5%   | 98.1%        |
| local_master_alias_remap_ctrl      |           |         | 98.9%        |
| mailbox                            | 98.2%     | 97.3%   | 98.0%        |
| otbn                               | 91.5%     | 92.2%   | 91.9%        |
| secure_dma                         | 91.9%     | 92.1%   | 92.0%        |
| sep_cpu_ctrl                       |           |         | 99.3%        |
| sep_filter_ctrl                    |           |         | 91.6%        |
| sep_output_remap_ctrl              |           |         | 97.4%        |
| sep_reset_ctrl                     |           |         | 100.0%       |
| sep_scratch_cold                   |           |         | 90.1%        |
| spi_controller                     | 96.1%     | 95.9%   | 95.6%        |
| spi_flash                          | 97.8%     | 98.4%   | 97.3%        |

**Notes**
- RHEL: gcc/gcov; macOS: clang/llvm-prof + Homebrew `lcov`.

---

# Issues Fixed
Enhancement Request for macOS support

# Current Limitations

- **Unmodeled IPs**: Tests referencing the following will fail or produce no output:
  - `och_sep_cdns_spi_ctrl`, `och_sep_spi_mux_ctrl`
- **Key Manager**: unit-level testing only (no DV tests)


# GCC and C++ Compatibility

| CXX_STD  | Compiler                    | SYSTEMC_API| Status         |
|----------|-----------------------------|------------|----------------|
| c++17    | gcc-toolset-9 (GCC 9.2)     | cxx201703L |  OK            |
| c++17    | system GCC 8.5              | cxx201703L |  OK            |
| c++20    | gcc-toolset-11 (GCC 11.2)   | cxx202002L |  OK            |
| c++20    | system GCC 8.5              | cxx201709L |  Not Supported |
| c++20    | gcc-toolset-9 (GCC 9.2)     | cxx201709L |  Not Supported |
