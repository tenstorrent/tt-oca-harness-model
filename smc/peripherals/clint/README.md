# SMC CLINT — SystemC / TLM-2.0 Loosely-Timed Model

A standards-compliant RISC-V Core-Local Interruptor (CLINT) modeled in
Accellera SystemC 2.3.x / 3.0 + TLM-2.0 (Loosely-Timed) and parameterised
through SystemC CCI 1.0. Implements the SMC CLINT IP described in:

- `tt-oca-hw.pdf` §6.6.6 "RISC-V CLINT and Precise System Management Timing"
- `tt-oca-hw.pdf` §6.6.3 (table, address window `0xC800_0000`–`0xC800_FFFF`)
- `hw/smc/smc_cpu/data/registers/rdl/clint.rdl` — authoritative register map
- `hw/smc/smc_cpu/chipyard_generated_files/{1,4}core/OCAH{1,4}CORECluster_CLINT.sv`
  — functional reference RTL (Chipyard / Rocket-Chip)
- `fw/smc/common/drivers/riscv_clint0.c` — firmware driver and the
  expected access patterns (32-bit half-word MTIME / MTIMECMP, "high-FF /
  low / high" comparator update sequence)

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP
library wires up exactly as for the PLIC. It runs the SMC firmware's
CLINT driver without modification and presents the same per-hart
`msip_o[h]` and `mtip_o[h]` lines that feed the cores'
`mip.MSIP` / `mip.MTIP` CSRs.

---

## Layout

```
clint/
├── CMakeLists.txt
├── README.md                       (this file)
├── run_tests.sh                    Build + run helper (Release / ASan / coverage)
├── include/
│   ├── smc_tlm_extensions.h        Thin shim → cpu_cluster/include/smc_axi_extension.h
│   │                               plus the fabric-wide source_id_t enum.
│   └── clint.h                     SC_MODULE(clint) declaration + cci_param
├── src/
│   └── clint.cpp                   Implementation
├── test/
│   ├── CMakeLists.txt
│   ├── clint_tb.cpp                Deterministic test bench (TC-1..TC-22)
│   └── clint_tick_tb.cpp           Auto-tick test bench (TA-1..TA-3)
└── doc/
    ├── 01_CLINT_Specification.md      External contract (RDL-traceable)
    ├── 02_CLINT_LowLevel_Design.md    Internal SystemC + CCI design
    ├── 03_CLINT_Test_Plan.md          Verification plan, traceability matrix
    ├── build_docs.sh                  Regenerate all three PDFs from .md
    ├── print.css                      PDF stylesheet (pandoc → Chrome headless)
    └── figures/*.svg                  Block / pipeline / state-machine diagrams
```

The canonical `smc::smc_axi_extension` class lives in
`smc/cpu_cluster/include/smc_axi_extension.h` and is shared by every SMC
IP.  CLINT's CMake adds that directory to its public include path via the
`SMC_AXI_EXTENSION_DIR` cache variable (mirroring PLIC's wiring), so the
`#include "smc_tlm_extensions.h"` in `clint.h` transparently pulls in the
canonical extension definition.

---

## Module interface

```cpp
SC_MODULE(clint) {
    tlm_utils::simple_target_socket<clint>     reg_socket;   // AXI4-Lite, 32 / 64-bit
    sc_vector<sc_out<bool>>                    msip_o;       // per-hart Machine SW interrupt
    sc_vector<sc_out<bool>>                    mtip_o;       // per-hart Machine Timer interrupt
    sc_in<bool>                                rst_n_i;
    explicit clint(sc_module_name, clint_cfg = clint_cfg{});
};
```

`clint_cfg` defaults match the SMC 4-core configuration from
`tt-oca-hw.pdf §6.6.6`:

| Field             | Default | Note                                            |
|-------------------|---------|-------------------------------------------------|
| `num_harts`       | 4       | 4 cores per chip; sizes MSIP / MTIMECMP / outputs |
| `tick_period_ns`  | 100.0   | 10 MHz default; 0 disables auto-tick (test mode) |

Both can be overridden via the corresponding CCI parameters (see
`02_CLINT_LowLevel_Design.md` §3 for the full CCI catalogue):

```cpp
broker.set_preset_cci_value("smc.clint.num_harts",       cci::cci_value(2u));
broker.set_preset_cci_value("smc.clint.tick_period_ns",  cci::cci_value(1000.0));
broker.set_preset_cci_value("smc.clint.access_delay_ns", cci::cci_value(5.0));
```

### Register window (BASE = `0xC800_0000`, 64 KiB)

Standard SiFive / RISC-V CLINT layout:

| Offset                 | Register          | Size       | Access |
|------------------------|-------------------|------------|--------|
| `0x0000 + 4·h`         | `MSIP[h]`         | 4 B        | RW     |
| `0x4000 + 8·h`         | `MTIMECMP[h]`     | 8 B        | RW     |
| `0xBFF8`               | `MTIME`           | 8 B        | RW     |

- `MSIP[h]`: only bit[0] is the IPI; bits[31:1] are RAZ/WI per `clint.rdl`.
- `MTIMECMP[h]` / `MTIME` may be accessed as a single 8-byte transaction
  **or** as two consecutive aligned 4-byte halves (low then high) — the
  firmware driver in `riscv_clint0.c` always uses the latter form.
- All other addresses inside the window are RAZ/WI (matching the
  Chipyard register router behaviour).
- Misaligned, undersize, or wrong-width accesses return
  `TLM_BURST_ERROR_RESPONSE`; out-of-window addresses return
  `TLM_ADDRESS_ERROR_RESPONSE`. DMI is never advertised.

### Interrupt semantics

For each hart `h ∈ [0, num_harts)`:

- `msip_o[h]` = `MSIP[h].bit[0]`. Software raises (typically as an IPI to
  another hart) by writing 1; clears by writing 0. Bypasses the PLIC.
- `mtip_o[h]` = `(MTIME ≥ MTIMECMP[h])`. Pure level function, recomputed
  whenever `MTIME` or `MTIMECMP[h]` change. Cleared by writing
  `MTIMECMP[h] > MTIME`. Asserts again automatically when MTIME catches
  up.
- Reset: `MTIME = 0`, `MSIP[h] = 0`, `MTIMECMP[h] = 0xFFFF…F`. The
  Chipyard RTL leaves `pad` (MTIMECMP) without an explicit reset value;
  this model defaults it to all-1s so MTIP is guaranteed deasserted out
  of reset until firmware programs a real comparator.

### Test-bench back-door

`transport_dbg` is implemented for the entire window and is symmetric
with `b_transport` (no read side-effects exist in the CLINT). The module
also exposes:

```cpp
uint64_t dbg_mtime    ()                      const;
uint64_t dbg_mtimecmp (unsigned hart)         const;
uint32_t dbg_msip     (unsigned hart)         const;  // 0 or 1
bool     dbg_mtip     (unsigned hart)         const;
void     dump_state   (std::ostream& = std::cout) const;
void     dbg_set_mtime(uint64_t value);               // back-door MTIME advance
```

`dbg_set_mtime` is the test-bench's preferred way to walk MTIME across
known MTIMECMP boundaries deterministically without waiting for
`tick_period_ns × N` of simulated wall-clock time.

---

## Building

The model needs Accellera SystemC ≥ 2.3.4 (preferably 3.0) and Accellera
SystemC CCI 1.0. It links against `SystemC::systemc` if a CMake-installed
SystemC is available; otherwise it falls back to the `SYSTEMC_HOME` env
var. CCI is located via `CCI_HOME` (default `/Users/pdroy/cci`).

```bash
cd peripherals/clint
./run_tests.sh                 # incremental build + run (Release)
./run_tests.sh --clean         # wipe build/ first
./run_tests.sh --ctest         # run via ctest instead of direct binary
./run_tests.sh --asan          # AddressSanitizer build + run + report
./run_tests.sh --coverage      # coverage build + run + HTML report
```

Expected output (`./run_tests.sh`, abbreviated):

```
==== SMC CLINT TB (CCI-compliant) ====
  DUT topology: 2 harts, tick_period_ns=0
  [PASS] reset clears MTIME / MSIP / MTIP; MTIMECMP=max
  [PASS] MSIP R/W truncates to bit[0]; msip_o tracks register
  [PASS] MSIP per-hart isolation
  [PASS] MTIME 64-bit R/W
  [PASS] MTIME 32-bit half-word R/W
  [PASS] firmware-style MTIME read sequence
  [PASS] MTIMECMP 64-bit R/W per hart
  [PASS] firmware-style MTIMECMP set sequence
  [PASS] MTIP comparator at -1 / == / +1 boundary
  [PASS] MTIP cleared by raising MTIMECMP
  [PASS] per-hart MTIMECMP independence
  [PASS] MSIP and MTIP are independent per hart
  [PASS] synthetic MTIME advance (auto-tick disabled)
  [PASS] transport_dbg back-door read/write
  [PASS] negative tests: window, alignment, width
  [PASS] CCI: discovery, introspection, mutation, immutability
  [PASS] dump_state contains expected fields

ALL TESTS PASSED
```

---

## Wiring the CLINT into `smc_top`

```cpp
// In smc_top constructor (4-core variant)
smc::clint clint("clint", smc::clint_cfg{ .num_harts = 4 });

// Fabric routes BASE+0x0800_0000 .. BASE+0x0800_FFFF here
fabric.to_clint.bind(clint.reg_socket);

// Reset distribution
clint.rst_n_i(rstu.rst_core_smc_n_o);

// Per-hart interrupt outputs feed the CPU cluster's mip CSR
for (unsigned h = 0; h < NCORES; ++h) {
    clint.msip_o[h](cpu.msip_in[h]);
    clint.mtip_o[h](cpu.mtip_in[h]);
}
```

Inbound `axi_filter` instances belong **between** the fabric and
`reg_socket`, exactly as documented for the PLIC in
`02_SMC_IP_LowLevel_Design.md` §2 — the CLINT's window is treated
identically by the access-control filter.

---

## Modeling notes

- **Loosely-timed**: every register access annotates a small fixed
  delay (`access_delay_ns_p_`, default 2 ns; mutable via CCI). Override
  for performance studies that need a different profile.
- **MTIME tick**: implemented as a single self-rearming SC_METHOD that
  posts a delayed event every `tick_period_ns`. This is dramatically
  cheaper than a continuously-clocked counter — typical firmware idle
  loops span millions of ticks where nothing else changes.
- **No DMI**: the comparator output `mtip_o` is a function of `mtime_`
  and `mtimecmp_`, both updated inside the model; DMI would let
  observers read inconsistent 64-bit snapshots (no atomic memcpy).
- **CCI-first configuration**: `num_harts`, `tick_period_ns`, and
  `access_delay_ns` are all real `cci::cci_param<T>` declarations.
  `num_harts` and `tick_period_ns` are `CCI_IMMUTABLE_PARAM` — they may
  be preset before construction but not mutated mid-simulation. The
  immutability test in `clint_tb.cpp` exercises this.
- **Multi-clock**: per `tt-oca-hw.pdf §6.6.6`, the CLINT sits inside the
  CPU sub-system on `clk_smc_i`. In LT this is abstract — the only
  timing knob is `access_delay_ns_p_`.
- **Verification hooks**: see `dbg_*` methods and the included test
  bench (17 test groups). Coverage instrumentation can be added by
  hooking into `reg_read` / `reg_write`.

---

## Conformance summary

| Spec requirement                                                     | Status |
|----------------------------------------------------------------------|--------|
| `SC_MODULE(clint)` shape from `02_SMC_IP_LowLevel_Design.md` §4      | OK     |
| 4 harts default (matches `tt-oca-hw.pdf §6.6.6`)                     | OK     |
| RISC-V CLINT register layout (`clint.rdl`)                           | OK     |
| MSIP bit[0] only (RAZ/WI for [31:1])                                 | OK     |
| MTIME / MTIMECMP 8-byte at canonical offset, 4-byte half-words ok    | OK     |
| MTIP = `(MTIME >= MTIMECMP[h])` per hart                             | OK     |
| Firmware "high-FF / low / high" MTIMECMP set sequence verified       | OK     |
| Reset clears MTIME / MSIP; MTIMECMP defaults to max (no spurious)    | OK     |
| Per-hart MSIP / MTIMECMP / MTIP independence                         | OK     |
| Single-driver discipline on `msip_o` / `mtip_o` (output_method)      | OK     |
| `smc_axi_extension` honoured (placeholder; filtering external)       | OK     |
| `transport_dbg` back-door, no side effects                           | OK     |
| `dump_state()` and `dbg_*` accessors                                 | OK     |
| TLM error response on misaligned / out-of-window / wrong-size        | OK     |
| DMI refused                                                          | OK     |
| CCI 1.0 parameterisation (`num_harts`, `tick_period_ns`, `access_delay_ns`) | OK |
| Immutability of `CCI_IMMUTABLE_PARAM` enforced by broker             | OK     |
| AddressSanitizer-clean (`./run_tests.sh --asan`)                     | OK     |
