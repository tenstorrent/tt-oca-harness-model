# SS-1.3 OTBN key-dump program

`keydump.s` is the OTBN program loaded (hand-loaded into IMEM via AXI) by
`sep_km_otbn_sideload_kat_test_seq`. It reads the KM-sideloaded keymgr key
from the `KEY_S0/KEY_S1` WSRs, reconstructs `key = share0 ^ share1`, and
stores the 384-bit result to DMEM (`result_lo` @ 0x00, `result_hi` @ 0x20)
for the host to read back.

The assembled IMEM words are embedded as a `localparam` in the sequence (the same
lightweight pattern as `drbg/sep_drbg_real_sink_otbn_*`). To regenerate them:

```bash
source bin/setup_env.sh
R=$OCH_ROOT
cd $R/fw/sep/tests/otbn_km_sideload_keydump/otbn_src
export PYTHONPATH="$R/vendor/opentitan/upstream/hw/ip/otbn/util:$R/vendor/opentitan/upstream/hw/ip/otbn:$R/dv/sep/tests/common_otbn:$PYTHONPATH"
python3 $R/vendor/opentitan/upstream/hw/ip/otbn/util/otbn_as.py keydump.s -o keydump.o
riscv32-unknown-elf-ld --no-check-sections --no-warn-rwx-segments \
    -T $R/dv/sep/tests/common_otbn/otbn_app.ld keydump.o -o keydump.elf
riscv32-unknown-elf-objcopy -O binary --only-section=.text keydump.elf keydump.text.bin
python3 -c "import struct; d=open('keydump.text.bin','rb').read(); d+=b'\x00'*((-len(d))%4); print('\n'.join('32\'h%08X,'%struct.unpack('<I',d[i:i+4])[0] for i in range(0,len(d),4)))"
```

WSR indices are resolved by the assembler from symbolic names:
`KEY_S0_L=4, KEY_S0_H=5, KEY_S1_L=6, KEY_S1_H=7`.
