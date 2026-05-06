# Release Information

- Version: 2.0
- Release date: 06-May-2026
- Release name: Phase II release

# Release Details

 Includes the following models:
  - RISC-V core model (VeeR/EL2)
  - Memories: SRAM, ROM, ITCM, DTCM
  - Interconnect and interrupt controllers: SimpleBus, PLIC, CLINT, AV BUS
  - Reset and infrastructure: Reset generation unit (RSU), stdout device
  - Peripherals: DMA, UART, GPIO, HMAC, KMAC, OTBN, CSRNG/CRNG, AES, SPI controller, Key Manager, EDN, Entropy Source, Mailbox, AON Timer, SEP Efuse/OTP, Lifecycle Controller, xSPI Controller + Flash 
  - Integration stubs used by the subsystem: EDN request/response stubs, Reset Generation Unit, OTP key request stub, keymgr dummy, KMAC entropy/keymgr stubs, AES EDN/keymgr stubs, SPI device stub, I2C device stub, GPIO loopback bridge

- Testing status:
  - All models are unit tested.
  - System-level tests under `riscv-vp-plusplus/sw/sep-vp-tests` run successfully on the VP.
  - System-level DV tests under `riscv-vp-plusplus/sw/tt-tests` run successfully on the VP.


# Issues fixed

- None Reported 


# Current Limitations

- GCC/toolchain compatibility constraints:
  - Tested compiler versions for this release:
    - GCC 11.4.0 (Accellera flow) -- The code base compiles successfully
    - GCC 9.5.0 (Synopsys Virtualizer toolchain) -- The codebase compiles successfully with the Synopsys toolchain
  - Accellera SystemC 3.0.1 is validated with GCC 9.x (e.g., 9.4.0) and GCC 11.x (e.g., 11.4.0).
  - xSPI controller test `spi_sanity_cadence.c` does not pass. This test requires additional models (for example SPI mux and clock divider) that are not part of Phase II.
  - xSPI reset and JEDEC reset are not supported by the SPI flash model, therefore these reset flows have not been validated.
  - For models without DV tests (for example Key Manager), only unit-level testing has been performed.
- Synopsys Virtualizer support will be part fo the next release.
