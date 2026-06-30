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
- Peripherals: DMA, UART, GPIO, HMAC, KMAC, OTBN, CSRNG, AES, SPI controller(Open Titan), SPI Flash(SFDP Profile 1 commands), Key Manager, EDN, Entropy Source, Mailbox, AON Timer, SEP Efuse/OTP, Lifecycle Controller
- Integration stubs/Adapters: OTP key request stub, Mailbox host stub, GPIO loopback bridge, MailboxBridge, dma_sys_bus_adapter


## Testing Status

- All peripheral models are unit tested
- Firmware tests written by Vayavya under `sw/sep-vp-tests/` run successfully on the VP
- TT firmware tests under `sw/tt-oca-hw-main/dv/sep/tests/` run successfully on the VP
- TT firmware tests under `sw/tt-oca-hw-main/fw/sep/tests/`
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

| Peripheral      | RHEL 8.10 | macOS   | Ubuntu 22.04 |
|-----------------|----------:|--------:|-------------:|
| aes             | 91.9%     | 92.6%   | TBD          |
| aon_timer       | 98.5%     | 98.1%   | TBD          |
| csrng           | 91.4%     | 91.3%   | TBD          |
| edn             | 92.6%     | 93.0%   | TBD          |
| efuse           | 100.0%    | 100.0%  | TBD          |
| entropy_src     | 89.8%     | 92.0%   | TBD          |
| gpio            | 87.4%     | 87.5%   | TBD          |
| hmac            | 91.7%     | 91.5%   | TBD          |
| key_manager     | 97.8%     | 97.9%   | TBD          |
| kmac            | 91.9%     | 92.2%   | TBD          |
| lifecycle_ctrl  | 97.9%     | 97.5%   | TBD          |
| mailbox         | 98.2%     | 97.3%   | TBD          |
| otbn            | 91.5%     | 92.2%   | TBD          |
| secure_dma      | 91.9%     | 92.1%   | TBD          |
| spi_controller  | 96.1%     | 95.9%   | TBD          |
| spi_flash       | 97.8%     | 98.4%   | TBD          |
| uart_16550      | 97.4%     | 96.0%   | TBD          |

**Notes**
- RHEL: gcc/gcov; macOS: clang/llvm-prof + Homebrew `lcov`.

---

# Issues Fixed
Enhancement Request for macOS support

# Current Limitations

- **Unmodeled IPs**: Tests referencing the following will fail or produce no output:
  - `sep_outbound_filter`
  - `PIC` (Platform Interrupt Controller)
  - `sep_cpu_ctrl`, `sep_reset_ctrl`
  - `local_master_alias_remap_ctrl`
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
