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
- `make -C sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/bootcode secure_boot_spi`

Outputs (under that `bootcode/` directory):
- `build/boot_rom.elf`
- `build/boot_rom.vmem` (64-bit VMEM for Boot ROM preload)
- `build/boot_rom.itcm.hex` (Verilog hex for ICCM preload)
- `build/boot_rom.dtcm.hex` (empty compatibility file)

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

Completion signaling:
- **UVM wrapper (`sep_wrap_uvm_top.sv`)**: word-write magic sequence to `0x8000_0000`:
  - `0xA5A55A5A` then `0xCAFEBABE` = PASS
  - `0xA5A55A5A` then `0xDEADBEEF` = FAIL
