# SMC CPU Cluster — SystemC / TLM-2.0 Loosely-Timed Model

A SystemC TLM-2.0 LT model of the SMC CPU Cluster IP, wrapping the
Tenstorrent **Whisper** RISC-V ISS as the per-hart instruction-set
simulator backend. Implements the cluster described in:

- `doc/01_SMC_Architecture.pdf` §3 (IP #1) — modeling parameters and role
- `doc/02_SMC_IP_LowLevel_Design.pdf` §3 — TLM interface, register map,
  IRQ aggregation, control-register file, and bus-bridge routing
- `doc/03_SMC_Test_Plan.pdf` §A.1 / §A.6 — verification tier matrix
- `doc/04_CPU_Cluster_Test_Plan.md` — IP-specific test plan (`cluster_tb`,
  TC-CPU-001..010, PLIC CPU→PLIC→CPU, scratchpad INIT_MEM_DONE)
- `doc/05_CCI_Integration_Guide.md` — adoption guide for SystemC CCI
  parameter exposure (`smc_cpu_cluster::config` → broker-mediated
  presets)
- `doc/06_CPU_Cluster_Architecture.md` — markdown top-level architecture
  & implementation reference: chiplet view, internal block view, class
  / component / sequence diagrams, file layout, build-graph, and §3
  conformance summary (Spike-free, Whisper-only)

The model is a drop-in `sc_module` that the rest of the SMC SystemC IP
library (PLIC, CLINT, fabric, mailbox, …) can wire up exactly as
specified in the low-level design. Up to four `rv64imafdc` harts run in
parallel under the global TLM quantum, with per-hart `step()` loops
driven by independent `SC_THREAD`s.

---

## Layout

```
cpu_cluster/
├── CMakeLists.txt                     Top-level build (Whisper + SystemC + Boost)
├── README.md                          (this file)
├── run_tests.sh                       Build-and-test driver script
├── doc/
│   ├── 01_SMC_Architecture.pdf        SMC-wide architecture (§3 covers CPU cluster)
│   ├── 02_SMC_IP_LowLevel_Design.pdf  SMC-wide IP low-level design (§3 covers CPU cluster)
│   ├── 03_SMC_Test_Plan.pdf           SMC-wide test plan (§A.1–A.6, §B.3 cover CPU cluster)
│   ├── 04_CPU_Cluster_Test_Plan.md    IP-specific test plan (refines §B.3 TC-CPU-001..010)
│   ├── 05_CCI_Integration_Guide.md    SystemC CCI adoption guide for `smc_cpu_cluster::config`
│   └── figures/                       Block diagrams / pipeline SVGs
├── external/
│   └── whisper-cmake/                 CMake wrapper for the pre-built Whisper archive
├── include/                           Public headers (same layout as peripherals/plic)
│   ├── iss_hart.h                     Abstract ISS-hart interface
│   ├── iss_backend_whisper.h          Whisper backend, header
│   ├── smc_axi_extension.h            Shared TLM GP extension (initiator-side)
│   └── smc_cpu_cluster.h              SC_MODULE(smc_cpu_cluster) declaration
├── src/
│   ├── iss_backend_whisper.cpp        Whisper backend, implementation
│   └── smc_cpu_cluster.cpp            Implementation
└── test/
    ├── CMakeLists.txt
    ├── cluster_tb.cpp                 Self-checking bench (all scenarios)
    └── include/
        └── smc_test_utils.h           Shared test fixtures / helpers
```

When the rest of the SMC IP library exists, these sources drop under the
canonical `libsmc/cpu/` root.  `include/smc_axi_extension.h` is the
project-wide canonical extension; PLIC's `smc_tlm_extensions.h` includes it.

---

## Module interface

```cpp
SC_MODULE(smc_cpu_cluster) {
    // §3.3 -- three initiator sockets (data / mmio / ifetch).
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> data;
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> mmio;
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> ifetch;

    // §3.8 -- CPU-Control register-file target (8 KB).
    tlm_utils::simple_target_socket<smc_cpu_cluster, 64>    ctrl;

    // §3.7 -- per-hart IRQ inputs (CLINT msip / mtip, PLIC meip).
    sc_vector<sc_in<bool>> irq_sw;
    sc_vector<sc_in<bool>> irq_timer;
    sc_vector<sc_in<bool>> irq_ext;

    explicit smc_cpu_cluster(sc_module_name, const config&);
};
```

`config` defaults to the SMC values from §2.1 of the architecture:

| Field             | Default       | Note                                       |
|-------------------|---------------|--------------------------------------------|
| `num_harts`       | 1             | 1..4                                       |
| `reset_pc`        | `0x80000000`  | Per-hart reset vector (overridable)        |
| `isa`             | `rv64imafdc`  | Whisper config string                      |
| `fast_mem_lo/hi`  | `0..0x80000000` | Internal flat-array memory window         |
| `mmio_lo/hi`      | `0x80000000..0x90000000` | §3.6 MMIO carve-out             |
| `quantum_ns`      | 1000          | TLM LT global quantum (~1 µs)              |
| `quantum_insts`   | 1000          | K in `step(K)` per scheduling slice        |
| `amo_lock_detect` | true          | Propagate AMO/LR-SC via `prot[3]`          |
| `source_id`       | `0x10`        | SMC_CPU_SOURCE_ID on `smc_axi_extension`   |

Per-core variants (2-core, 4-core) only need to override `num_harts`.

### Bus interface

- Three initiator sockets (`data`, `mmio`, `ifetch`) – §3.6 routes each
  address to the appropriate egress based on the `mmio_lo/hi` carve-out.
- One target socket (`ctrl`) implementing the CPU-Control register
  file at BASE+`0x001_0000` (RESET_VECTOR_N, CORE_ENABLE, LOCAL_BASE,
  GLOBAL_BASE, REGION_SIZE, INIT_MEM_DONE, DISABLE_SRAM_AUTO_INIT,
  MEM_REPAIR_STATUS). See §3.8.
- All outgoing transactions carry an `smc_axi_extension` (§3.10).
  Targets that don't care simply ignore it.
- DMI is never granted on the `ctrl` socket (writes have side effects).

### IRQ semantics

Per the RISC-V privileged spec and §3.7:

- `irq_sw[i]`     ↔ MSIP   (CLINT software IRQ)
- `irq_timer[i]`  ↔ MTIP   (CLINT timer IRQ)
- `irq_ext[i]`    ↔ MEIP   (PLIC M-mode external IRQ)

Each hart owns an `SC_METHOD` `irq_aggregator(i)` sensitive to the
three signal inputs. On any change the aggregator updates the
corresponding bit of the hart's `MIP` CSR inside Whisper, so the next
`singleStep()` observes the new interrupt state.

### Debug back-door

The cluster exposes a typed debug API for the test bench:

```cpp
bool                          load_elf(const std::vector<std::string>& elfs);
iss_hart&                     hart(unsigned i);
WdRiscv::System<uint64_t>&    whisper_system();
void                          inject_nmi(unsigned hart_idx, uint64_t cause = 0);
uint64_t                      reset_vector_n(unsigned i) const;
void                          set_init_mem_done(bool);
void                          set_mem_repair_status(uint32_t);
```

These satisfy `03_SMC_Test_Plan.pdf` §A.3 inspection requirements.

---

## Verification scope

Regression is the self-checking bench `test/cluster_tb` (see
`doc/04_CPU_Cluster_Test_Plan.md`). It verifies the **cluster wrapper** and
**wiring to peer IPs**, not full SoC firmware or every peripheral matrix.

| What | Where | In `cluster_tb`? |
|------|--------|------------------|
| PLIC IP matrix (sources, thresholds, claim rules) | `peripherals/plic` + `plic_tb` | Linked `plic.cpp` only; exhaustive cases stay in `plic_tb` |
| **Cluster ↔ PLIC** (CPU MMIO, `irq_ext`, firmware ISR) | `04_CPU_Cluster_Test_Plan.md` §8.7 | **Yes** — CPU → PLIC → CPU |
| CLINT IP (mtime / mtimecmp / MSIP MMIO) | `peripherals/clint` (future) | **No** — `irq_sw` / `irq_timer` wire stubs only (§8.6) |
| Scratchpad map on `cluster.data` | Future fabric router | Init handshake only (`ScratchpadSramStub`, TC-CPU-004/005) |
| Production `riscv_plic0.c` / full IRQ map | SMC firmware test plan | Open |

---

## Building

The model needs:

- Accellera SystemC ≥ 2.3.4 built with **C++20** (matches this project)
- A pre-built Whisper tree containing `build-Linux/librvcore.a`
- Boost ≥ 1.74 (header-only is sufficient; `program_options` shared
  lib is linked when present)
- Accellera SystemC CCI 1.0 when `SMC_BUILD_PLIC_INTEGRATION=ON` (default) --
  providing
  `include/cci_configuration` and `lib[64]/libcci-config.so` +
  `lib[64]/libcci-inspection.so`.  Disable with
  `-DSMC_BUILD_PLIC_INTEGRATION=OFF` if you don't have CCI.

Set the environment variables before configuring:

```bash
export SYSTEMC_HOME=/path/to/systemc            # contains include/systemc.h
export WHISPER_HOME=/path/to/whisper-source     # contains build-Linux/librvcore.a
export BOOST_DIR=/path/to/boost                 # contains include/boost/version.hpp
export CCI_HOME=/path/to/systemc-cci-install    # contains include/cci_configuration
                                                # (required when SMC_BUILD_PLIC_INTEGRATION=ON)
```

Then:

```bash
cd cpu_cluster
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Or use the wrapper script (same style as `peripherals/plic/run_tests.sh`):

```bash
./run_tests.sh                # incremental build + run cluster_tb
./run_tests.sh --clean        # wipe build/, then configure/build/run
./run_tests.sh --ctest        # run via ctest
./run_tests.sh --coverage     # build with gcov, run, render gcovr report
```

### Expected output

```
==== SMC CPU Cluster TB ====
  [PASS] smoke: Whisper System constructs
  ...
  [PASS] CPU configures PLIC, handles MEIP (CPU->PLIC->CPU)
  [PASS] scratchpad SRAM INIT_MEM_DONE handshake

ALL TESTS PASSED
```

---

## Wiring the cluster into `smc_top`

Excerpt from how the architectural top-level instantiates and binds the
cluster (see `02_SMC_IP_LowLevel_Design.pdf` "Top-level Integration"):

```cpp
// In smc_top constructor (4-core variant)
smc::smc_cpu_cluster::config cfg{};
cfg.num_harts   = 4;
cfg.reset_pc    = 0x80000000;
cfg.isa         = "rv64imafdc";
smc::smc_cpu_cluster cluster("cluster", cfg);

// Fabric routes high-perf traffic and MMIO from the cluster.
cluster.data  .bind(fabric.from_cpu_data);
cluster.mmio  .bind(fabric.from_cpu_mmio);
cluster.ifetch.bind(fabric.from_cpu_ifetch);

// CPU-Control register file (lives at BASE + 0x001_0000)
fabric.to_cpu_ctrl.bind(cluster.ctrl);

// CLINT + PLIC drive per-hart IRQ inputs.
for (unsigned h = 0; h < cluster.num_harts(); ++h) {
    clint.msip_out [h](cluster.irq_sw   [h]);
    clint.mtip_out [h](cluster.irq_timer[h]);
    plic .ctx_out  [2*h + 0](cluster.irq_ext[h]);   // M-mode context
}
```

Per `02_SMC_IP_LowLevel_Design.pdf` §3.10, every outgoing transaction
from the cluster carries an `smc_axi_extension` whose `source_id`
identifies the cluster (`SMC_CPU_SOURCE_ID = 0x10`).

---

## Modeling notes

- **Loosely-timed**: each hart owns a `tlm_quantumkeeper`. The global
  quantum (default 1 µs) is set at construction; `step(K)` runs `K`
  instructions per scheduling slice (`quantum_insts`, default 1000).
- **Per-hart concurrency**: one `SC_THREAD` per hart, cooperatively
  scheduled. Only one hart runs at a time inside `singleStep()`, so
  the memory callbacks safely use a single `current_hart_` for
  quantum-keeper accounting.
- **Bus-bridge routing (§3.6)**: `pick_socket(addr)` returns `data`,
  `mmio`, or `ifetch` based on `fast_mem_lo/hi` and `mmio_lo/hi`.
  Whisper's `MEM_CALLBACKS` does not distinguish fetch from load
  today, so `ifetch` is reserved for forward compatibility.
- **AMO / LR-SC (§3.10)**: when `amo_lock_detect` is true, the wrapper
  asserts `prot[3] = 1` on transactions originating from atomic
  instructions; targets may filter on this.
- **CPU-Control register file (§3.8)**: the `ctrl` target socket
  implements an 8 KiB register window. Writes update `regs_`; reads
  return the current value. RESET_VECTOR_N writes take effect on the
  next `hart(i).reset()`.
- **WFI (§3.7)**: when a hart executes `wfi`, its `SC_THREAD` waits
  on `wfi_event_[i]`. Any rising edge on its IRQ aggregator wakes it.
- **CORE_ENABLE (§3.8)**: bit `i` enables hart `i`. When toggled high,
  the wrapper notifies `core_enable_event_[i]` so the corresponding
  thread leaves its wait-for-enable barrier.
- **Verification hooks**: see the `hart(i).*` API and the included
  Tier-0/Tier-1 tests. Coverage instrumentation is enabled by
  passing `-DSMC_ENABLE_COVERAGE=ON` at configure time and building
  the `coverage` custom target after `ctest`.

---

## Conformance summary

| Spec requirement                                                       | Status |
|------------------------------------------------------------------------|--------|
| `SC_MODULE(smc_cpu_cluster)` shape from `02_…_LowLevel_Design.pdf` §3 | ✅      |
| 1..4 harts, `rv64imafdc`, reset-PC configurable                        | ✅      |
| Three initiator sockets (data / mmio / ifetch) §3.3 / §3.6             | ✅      |
| `ctrl` target socket with CPU-Control register file §3.8               | ✅      |
| Per-hart IRQ inputs (msip / mtip / meip) §3.7                          | ✅      |
| `smc_axi_extension` on every outgoing GP §3.10                          | ✅      |
| AMO / LR-SC propagation via `prot[3]`                                  | ✅      |
| TLM LT quantum + per-hart `tlm_quantumkeeper`                          | ✅      |
| WFI sleep / IRQ wake-up                                                | ✅      |
| `load_elf()` + per-hart debug API §A.3                                 | ✅      |
| Self-checking `cluster_tb` (§B.3 TC-CPU-001..010 partial/full)         | ✅      |
| PLIC CPU→PLIC→CPU + scratchpad TC-CPU-004/005 in `cluster_tb`          | ✅      |
| Full PLIC IP matrix (separate from cluster integration)                | `plic_tb` |
| CLINT IP MMIO (cluster IRQ wire stubs only in `cluster_tb`)            | future  |
| Coverage via `-DSMC_ENABLE_COVERAGE=ON` §A.4                            | ✅      |
