### SEP Boot ROM (bring-up / Phase 1)

This directory contains a SEP Boot ROM image intended to validate the **boot execution path**
(reset -> fetch from Boot ROM -> initialize DCCM runtime data -> visible PASS/FAIL).

### Build

From repo root:

```bash
make -C sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode
```

SPI image targets:
- `make -C sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode non_secure_boot_spi`
- `make -C sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode secure_boot_ephemeral`

`secure_boot_ephemeral` creates a new RSA-3072 key on every invocation, emits
the matching ROM slot-0 digest, rebuilds the Boot ROM, and signs the SPI image.
The key is stored with mode `0600` below the ignored `build/secure/` directory.
No fixed private key or secure SPI image is kept in the repository.

Outputs (under that `bootcode/` directory):
- `build/boot_rom.elf`
- `build/boot_rom.vmem` (64-bit VMEM for Boot ROM preload)
- `build/boot_rom.itcm.hex` (Verilog hex for ICCM preload)
- `build/boot_rom.dtcm.hex` (empty compatibility file)
- `build/secure/boot_rom.elf` and `build/secure/secure_boot.spi_preload`
  (matching ephemeral secure-boot artifacts)

Notes:
- `.text`, metadata, `.rodata`, and the `.data` load image are stored in Boot ROM.
- Runtime `.data`, `.bss`, stack, and mutable state live in DCCM.
- UVM `init_tcms()` initializes DCCM ECC before reset release; ROM startup copies
  `.data` from ROM and clears `.bss`.

### Run in simulation (UVM)

Use the existing UVM testcase entry `sep_rom_non_secure_boot_test`:

```bash
cd dv/sep/tb/sim
python3 run.py sep_rom_non_secure_boot_test --no-wave --stack sim
```

Or run with `ttem` directly using:
- `+STARTUP=BACKDOOR_TCM`
- `+BOOT_ROM_VMEM_FILE=$ROOT/fw/sep/bootcode/build/boot_rom.vmem`
- `+ITCM_HEX_FILE=$ROOT/fw/sep/bootcode/build/boot_rom.itcm.hex`
- `+TEXT_SIZE=<text section size>`
- `+DATA_SIZE=0`
- `+ENTRY_POINT=<entry point hex address>`

For the SystemC VP secure-boot path, use
`vp/platform/sep/config/accellera_config_secure_boot.ini`. It selects the
generated secure artifacts and the OTBN RSA-3072 algorithm.

Run the front-door positive and wrong-key negative cases together with:

```bash
python3 sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode/tools/run_secure_boot_vp_tests.py
```

The negative case keeps the ROM trust digest from key A and signs its manifest
with independently generated key B; passing means the ROM reports
`PUBK_HASH_MISMATCH` and never reaches signature success.

Completion signaling:
- **UVM wrapper (`sep_wrap_uvm_top.sv`)**: word-write magic sequence to `0x8000_0000`:
  - `0xA5A55A5A` then `0xCAFEBABE` = PASS
  - `0xA5A55A5A` then `0xDEADBEEF` = FAIL
