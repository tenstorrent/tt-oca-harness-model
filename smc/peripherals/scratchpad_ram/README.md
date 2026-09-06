# SMC Scratchpad RAM — SystemC / TLM-2.0 Loosely-Timed Model

A standards-compliant **SMC Scratchpad RAM** modelled in Accellera SystemC
2.3.x / 3.0 + TLM-2.0 (Loosely-Timed) and parameterised through SystemC
CCI 1.0.  It is the read-write sibling of the Boot ROM model: a true
byte-addressable SRAM that the SMC Rocket CPU cluster uses for early-boot
stack/data storage and fast local scratch.

The model tracks the on-chip scratchpad instantiated by the SMC CPU cluster:

- `tt-oca-hw/hw/smc/smc_cpu/data/registers/rdl/spm_memory.rdl` — the
  read-write `mem` declaration (`mem`, 64-bit wide, `sw=rw, hw=rw`).
- `tt-oca-hw/hw/smc/smc_cpu/chipyard_config/OCAH1CORECluster.scala` —
  `WithScratchpadWithECC(base = 0xC0040000, size = 0x10000, banks = 1,
  partitions = 4, ecc = SECDED)`.
- `tt-oca-hw/hw/smc/smc_cpu/chipyard_generated_files/1core/OCAH1CORECluster_TLRAM.sv`
  — the generated TileLink SRAM RTL (64-bit data, 8-bit byte mask, SECDED
  ECC, sub-word read-modify-write).
- `tt-oca-hw/hw/smc/data/registers/rdl/smc_top.rdl` — SMC top-level address
  map (`spm_memory @ BASE_ADDR + 0x06_0000`).

Architecture, CSRs, and programming are in the hardware TRM. Model and
test docs:

- `doc/index.adoc` — entry (includes the two pages below)
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP
library wires up exactly as for the Boot ROM, PLIC, and CLINT.

---

## Layout

```
scratchpad_ram/
├── CMakeLists.txt
├── README.md                       (this file)
├── run_tests.sh                    Build + run helper (Release / ASan / coverage)
├── include/
│   ├── smc_tlm_extensions.h        Shared GP extension (smc_axi_extension)
│   └── scratchpad_ram.h            SC_MODULE(scratchpad_ram) declaration + cci_param
├── src/
│   └── scratchpad_ram.cpp          Implementation
├── test/
│   ├── CMakeLists.txt
│   ├── scratchpad_ram_tb.cpp       Primary self-checking bench (rw, byte-enable, ECC)
│   ├── scratchpad_ram_neg_tb.cpp   Negative-path / edge-case bench
│   └── fixtures/
│       └── scratchpad_sanity.rv64.hex  Hex preload fixture
└── doc/
    ├── index.adoc
    ├── implementation.adoc
    └── test_plan.adoc
```

---

## Module interface

```cpp
SC_MODULE(scratchpad_ram) {
    tlm_utils::simple_target_socket<scratchpad_ram> reg_socket; // AXI4 / TL-style, 1/2/4/8 B
    sc_in<bool>                                     rst_n_i;    // active-low; SRAM retains contents
    explicit scratchpad_ram(sc_module_name, scratchpad_ram_cfg = scratchpad_ram_cfg{});
};
```

`scratchpad_ram_cfg` defaults match the generated 1-core scratchpad:

| Field              | Default     | Note                                                 |
|--------------------|-------------|------------------------------------------------------|
| `size_bytes`       | `0x10000`   | 64 KiB (matches `WithScratchpadWithECC size=0x10000`). |
| `init_file`        | `""`        | Empty = zero-initialise (cold-boot state).           |
| `init_file_format` | `"auto"`    | Pick `bin` for `.img`/`.bin`, else `hex`.            |
| `access_delay_ns`  | `2.0`       | Pipelined ~2-cycle read latency.                     |
| `ecc_enabled`      | `true`      | SECDED ECC behaviour (injection + single-bit scrub). |

All five are exposed as CCI parameters (`access_delay_ns` is mutable, the
rest immutable).  Set them via the CCI broker before constructing:

```cpp
broker.set_preset_cci_value("smc.scratchpad_ram.size_bytes",
                            cci::cci_value(uint64_t(0x10000)));
broker.set_preset_cci_value("smc.scratchpad_ram.access_delay_ns",
                            cci::cci_value(2.0));
```

See `doc/implementation.adoc` for the CCI catalogue and `smc-vp` bind.

---

## Behaviour highlights

- **Reads**: 1/2/4/8 B naturally-aligned; return the byte slice at the
  requested offset; honour byte-enables; `b_transport` delay of
  `access_delay_ns` ns.
- **Writes**: commit to the SRAM array; **byte-enables select which bytes
  change** (the RTL implements sub-word writes via a read-modify-write under
  the SECDED code) — this is the key difference from the Boot ROM.
- **Read-after-write**: returns the just-written value (true RAM).
- **Reset**: active-low; a structural no-op for the array — SRAM retains its
  contents across a logical reset (only the RTL bus pipeline registers reset).
- **SECDED ECC**: behavioural abstraction via an injection API.  A correctable
  (single-bit) error is scrubbed transparently on read; an uncorrectable
  (double-bit) error makes the read return `TLM_GENERIC_ERROR_RESPONSE` until
  the word is overwritten.
- **Out-of-window**, misaligned, or wrong-width accesses return the canonical
  TLM error responses (see spec §11).

### Unmodelled RTL features

- **Atomic memory operations (AMOs)** — the TileLink SRAM supports
  arithmetic/logical atomics via its RMW ALU; the LT model treats writes as
  plain stores (documented future-work item).
- **Bit-accurate SECDED** — modelled behaviourally via the injection API, not
  a true Hamming code.

---

## Build & test

```bash
./run_tests.sh                   # Release build + run both test benches
./run_tests.sh --ctest           # Run via ctest (both binaries)
./run_tests.sh --asan            # AddressSanitizer (+ LSan on Linux)
./run_tests.sh --coverage        # LLVM source-based coverage (Clang) or gcov
./run_tests.sh --clean           # Wipe build/ first
```

`SYSTEMC_HOME` and `CCI_HOME` are auto-probed for the common macOS
(Homebrew) and Linux (system + `/usr/local`) install locations; set them
manually for any non-standard prefix. Platform firmware:
`cd sw/smc-vp-tests && ./run_smc_vp_tests.sh smc-dma-test` (scratchpad
is the DMA/memory-zeroer target). Full commands are in
`doc/test_plan.adoc`.

---

## Current status

| Metric                       | Value                                |
|------------------------------|--------------------------------------|
| Test cases (across two TBs)  | All PASS (ctest: 2/2)                |
| AddressSanitizer             | 0 errors                             |
| Function coverage            | 100% (`src/scratchpad_ram.cpp`)      |
| Line coverage                | 98.5% (`src/scratchpad_ram.cpp`)     |
| Region coverage              | 93.6% (`src/scratchpad_ram.cpp`)     |
| Branch coverage              | 83.5% (`src/scratchpad_ram.cpp`)     |

---

## Citation

Modelled after, and consistent with, the SMC Boot ROM, PLIC, and CLINT
SystemC sub-packages in `smc/peripherals/`.  See those READMEs and the
matching specification / low-level-design documents for the shared SMC IP
conventions (CCI declaration order, single-driver discipline, error
taxonomy, build & packaging).
