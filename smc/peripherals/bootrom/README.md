# SMC Boot ROM — SystemC / TLM-2.0 Loosely-Timed Model

A standards-compliant **SMC Boot ROM** modelled in Accellera SystemC
3.0.2 + TLM-2.0 (Loosely-Timed) and parameterised through SystemC
CCI 1.0.2. It is a generic, role-agnostic read-only `mem` block in the SMC
IP library: the same `smc::bootrom` backs the SMC CPU-cluster boot path
and — being structurally identical — the SEP (Secure Enclave Processor)
boot ROM, with the role selected entirely through CCI presets.

Because there is no separate `smc_boot_rom.rdl`, the model is specified
and validated against the authoritative read-only `mem` contract that
does exist:

- `tt-oca-harness/meta/registers/rdl/sep_boot_rom.rdl` — authoritative
  memory map (`mem`, 64-bit wide, software read-only).
- `tt-oca-harness/dv/oss/shims/sep/memories/tests/conformance/test_sep_boot_rom_rw.py`
  — read-after-reset, write-ignore (claims C2 / C3 / C4).
- `tt-oca-harness/dv/oss/shims/sep/memories/tests/conformance/test_sep_boot_rom_preload.py`
  — preload format and zero-init contract.
- `tt-oca-harness/hw/smc/data/scripts/bootrom.rv64.img` — sample binary
  preload image (raw little-endian 8-byte words).
- `tt-oca-harness/dv/smc/tb/meta/scripts/bootrom_sanity.rv64.hex` — sample
  hex preload (one 64-bit big-endian ASCII word per line).

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP
library wires up exactly as for the PLIC and CLINT. It runs the boot
firmware's preload pipeline unmodified — pass an `.img` or `.hex` file
through the `init_file` CCI param and the ROM exposes the correct bytes
at offset 0.

Architecture, CSRs, and programming are in the hardware TRM. Model and
test docs:

- `doc/index.adoc` — entry (includes the two pages below)
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

---

## Layout

```
bootrom/
├── CMakeLists.txt
├── README.md                       (this file)
├── run_tests.sh                    Build + run helper (Release / ASan / coverage)
├── include/
│   ├── smc_tlm_extensions.h        Shared GP extension (smc_axi_extension)
│   └── bootrom.h                   SC_MODULE(bootrom) declaration + cci_param
├── src/
│   └── bootrom.cpp                 Implementation
├── test/
│   ├── CMakeLists.txt
│   ├── bootrom_tb.cpp              Primary test bench (hex preload)
│   ├── bootrom_bin_tb.cpp          Secondary test bench (binary preload)
│   └── fixtures/
│       ├── bootrom_sanity.rv64.hex Hex preload fixture
│       └── bootrom.rv64.img        Binary preload fixture (verbatim)
└── doc/
    ├── index.adoc
    ├── implementation.adoc
    └── test_plan.adoc
```

---

## Module interface

```cpp
SC_MODULE(bootrom) {
    tlm_utils::simple_target_socket<bootrom>  reg_socket;   // AXI4-style, 1/2/4/8 B
    sc_in<bool>                               rst_n_i;      // active-low, no-op (no mutable state)
    explicit bootrom(sc_module_name, bootrom_cfg = bootrom_cfg{});
};
```

`bootrom_cfg` defaults match the read-only `mem` declaration from the RDL:

| Field              | Default     | Note                                              |
|--------------------|-------------|---------------------------------------------------|
| `size_bytes`       | `0x10000`   | 64 KiB (8192 × 8 bytes; matches `NUM_ENTRIES`).    |
| `init_file`        | `""`        | Empty = zero-initialise.                          |
| `init_file_format` | `"auto"`    | Pick `bin` for `.img`/`.bin`, else `hex`.         |
| `access_delay_ns`  | `1.0`       | TLM annotated delay (claim C2 = 1-cycle `rvalid`).|

All four are exposed as CCI parameters (the first three immutable, the
last mutable). Set them via the CCI broker before constructing:

```cpp
broker.set_preset_cci_value("smc.bootrom.init_file",
                            cci::cci_value(std::string("bootrom.hex")));
broker.set_preset_cci_value("smc.bootrom.init_file_format",
                            cci::cci_value(std::string("hex")));
broker.set_preset_cci_value("smc.bootrom.access_delay_ns",
                            cci::cci_value(2.0));
```

See `doc/implementation.adoc` for the CCI catalogue and `smc-vp` bind.

---

## Behaviour highlights

- **Reads**: 1/2/4/8 B naturally-aligned; returns the byte slice at the
  requested offset, with a `b_transport` delay of `access_delay_ns` ns.
- **Writes**: silently discarded — the bus sees `TLM_OK_RESPONSE` and
  the ROM contents do not change (claim C4).
- **Reset**: active-low, structural no-op (the ROM has no mutable
  state; reads always succeed even while reset is asserted).
- **Preload**: hex (one 64-bit big-endian ASCII word per line) or
  binary (raw little-endian 8-byte words). Both produce **byte-
  identical** in-memory images for the same logical content.
- **Out-of-window**, misaligned, wrong-width, or byte-enabled accesses
  return the canonical TLM error responses (see spec §11).

---

## Build & test

```bash
./run_tests.sh                   # Release build + run both test bench binaries
./run_tests.sh --ctest           # Run via ctest (both binaries)
./run_tests.sh --asan            # AddressSanitizer (+ LSan on Linux)
./run_tests.sh --coverage        # LLVM source-based coverage (Clang) or gcov
./run_tests.sh --clean           # Wipe build/ first
```

`SYSTEMC_HOME` and `CCI_HOME` are auto-probed for the common macOS
(Homebrew) and Linux (system + `/usr/local`) install locations; set
them manually for any non-standard prefix. There is no
`smc-bootrom-test` firmware directory; see `doc/test_plan.adoc`.

---

## Current status

Release, ASan, and coverage are **three separate** `./run_tests.sh`
invocations. Coverage must be ≥ 95% line on `src/bootrom.cpp` (the
orchestrator fails below that). Benches: `bootrom_tb`, `bootrom_bin_tb`,
`bootrom_neg_tb`. Re-run `./run_tests.sh --coverage` for current
percentages; do not treat a checked-in snapshot as the gate.

---

## Citation

Modelled after, and consistent with, the SMC PLIC and CLINT SystemC
sub-packages in `smc/peripherals/`. See those READMEs and the matching
specification / low-level-design documents for the shared SMC IP
conventions (CCI declaration order, single-driver discipline, error
taxonomy, build & packaging).
