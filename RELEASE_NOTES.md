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
   -Tests exercising the current sep models run successfully on the VP 
   -Tests exercising the sep models which are currently not part of the VP are not tested.

---

# Issues Fixed
None
---

# Current Limitations

- **Unmodeled IPs**: Tests referencing the following will fail or produce no output:
  - `sep_outbound_filter`
  - `PIC` (Platform Interrupt Controller)
  - `sep_cpu_ctrl`, `sep_reset_ctrl`
  - `local_master_alias_remap_ctrl`
  - `och_sep_cdns_spi_ctrl`, `och_sep_spi_mux_ctrl`
- **Key Manager**: unit-level testing only (no DV tests)
- **Compiler versions tested**:
  - GCC 11.4.0, C++17 (Accellera flow)
  - GCC 9.5.0, C++17 (Accellera flow)
  - GCC 11.2, C++20 (Accellera flow)


# GCC and C++ Compatibility

| CXX_STD  | Compiler                    | SYSTEMC_API| Status         |
|----------|-----------------------------|------------|----------------|
| c++17    | gcc-toolset-9 (GCC 9.2)     | cxx201703L |  OK            |
| c++17    | system GCC 8.5              | cxx201703L |  OK            |
| c++20    | gcc-toolset-11 (GCC 11.2)   | cxx202002L |  OK            |
| c++20    | system GCC 8.5              | cxx201709L |  Not Supported |
| c++20    | gcc-toolset-9 (GCC 9.2)     | cxx201709L |  Not Supported |
