# SMC CPU Cluster — SystemC / TLM-2.0 Loosely-Timed Model

A SystemC TLM-2.0 LT model of the SMC CPU Cluster IP, wrapping the
Tenstorrent **Whisper** RISC-V ISS as the per-hart instruction-set
simulator backend. Implements the cluster described in:

- `doc/01_SMC_Architecture.pdf` §3 (IP #1) — modeling parameters and role
- `doc/02_SMC_IP_LowLevel_Design.pdf` §3 — TLM interface, register map,
  IRQ aggregation, control-register file, and bus-bridge routing
- `doc/03_SMC_Test_Plan.pdf` §A.1 / §A.6 — verification tier matrix
- `doc/01_CPU_Cluster_Specification.md` — IP specification (module boundary, config, requirements)
- `doc/02_CPU_Cluster_LowLevel_Design.md` — architecture & implementation reference
- `doc/03_CPU_Cluster_Test_Plan.md` — IP-specific test plan (`cluster_tb`, coverage ≥ 95%)
- `doc/04_CCI_Integration_Guide.md` — SystemC CCI parameter guide (supplementary)

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
│   ├── README.md                      Document index
│   ├── build_docs.sh                  Markdown → PDF (pandoc + Chrome)
│   ├── print.css                      PDF stylesheet
│   ├── 01_CPU_Cluster_Specification.md
│   ├── 02_CPU_Cluster_LowLevel_Design.md
│   ├── 03_CPU_Cluster_Test_Plan.md
│   └── 04_CCI_Integration_Guide.md    (supplementary — CCI adoption)
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

    explicit smc_cpu_cluster(sc_module_name name);
    smc_cpu_cluster(sc_module_name name, const config& cfg);  // legacy; see doc/04 CCI
};
```

Configuration is exposed via **SystemC CCI 1.0** `cci_param` members (broker
presets override defaults).  The `config` struct constructor remains for
backward compatibility.

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
`doc/03_CPU_Cluster_Test_Plan.md`). It verifies the **cluster wrapper** and
**wiring to peer IPs**, not full SoC firmware or every peripheral matrix.

| What | Where | In `cluster_tb`? |
|------|--------|------------------|
| PLIC IP matrix (sources, thresholds, claim rules) | `peripherals/plic` + `plic_tb` | Linked `plic.cpp` only; exhaustive cases stay in `plic_tb` |
| **Cluster ↔ PLIC** (CPU MMIO, `irq_ext`, firmware ISR) | `03_CPU_Cluster_Test_Plan.md` §8.7 | **Yes** — CPU → PLIC → CPU |
| CLINT IP (mtime / mtimecmp / MSIP MMIO) | `peripherals/clint` (future) | **No** — `irq_sw` / `irq_timer` wire stubs only (§8.6) |
| Scratchpad map on `cluster.data` | Future fabric router | Init handshake only (`ScratchpadSramStub`, TC-CPU-004/005) |
| Production `riscv_plic0.c` / full IRQ map | SMC firmware test plan | Open |

---

## Documentation

Numbered docs under `doc/` follow the Component Developer Guide layout
(`01` Specification, `02` LLD, `03` Test Plan). See `doc/README.md` for
the index. Regenerate PDFs with:

```bash
./doc/build_docs.sh    # requires pandoc + Chrome/Chromium
```

---

## Building

The model needs:

- Accellera SystemC ≥ 2.3.4 built with **C++20** (matches this project)
- A Whisper source tree (built automatically by `run_tests.sh` when needed)
- Boost ≥ 1.74 (headers + `program_options` library for Whisper)
- Accellera SystemC CCI 1.0 when `SMC_BUILD_PLIC_INTEGRATION=ON` (default)
- `liblz4` development package (RHEL/CentOS: `lz4-devel`) for Whisper's
  snapshot compression

Nothing above is vendored in this repo — install once per machine (or use a
shared prefix on NFS), record paths in `deps.env`, then use `./run_tests.sh`
for day-to-day builds.

### Quick start (after dependencies are installed)

```bash
cd smc/cpu_cluster
cp deps.env.example deps.env    # first time only — edit the four paths
./run_tests.sh
```

The script auto-builds Whisper when the static archives are missing.  You do
**not** need to run `make` inside Whisper manually unless you prefer to.

### First-time setup (new machine)

Work through these steps once.  Pick an install prefix (examples use
`$HOME/local/...`; a team shared tree under `/opt/...` works the same way).

#### 0. Toolchain

- **CMake** ≥ 3.20, **GCC** ≥ 11 with C++20 (GCC 13 / `gcc-toolset-13` on
  RHEL is what we use in CI-like environments).
- On RHEL 8: `sudo yum install cmake lz4-devel` and enable the devtoolset /
  gcc-toolset you plan to compile with.

#### 1. SystemC (C++20)

Download [Accellera SystemC](https://accellera.org/downloads/) (2.3.4 or
later), then build and install with C++20:

```bash
tar xf systemc-2.3.4.tar.gz && cd systemc-2.3.4
cmake -S . -B build \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_INSTALL_PREFIX="$HOME/local/systemc-2.3.4"
cmake --build build -j"$(nproc)"
cmake --install build
```

Verify: `$HOME/local/systemc-2.3.4/include/systemc.h` exists.

> SystemC and every consumer (this repo, Whisper, CCI) must agree on the same
> C++ standard.  Mixing a C++17 SystemC build with C++20 models causes link
> errors on `sc_api_version_*` symbols.

#### 2. Boost (≥ 1.74)

Whisper compiles against Boost headers and links `boost_program_options`.

**macOS (Homebrew):**

```bash
brew install boost
# BOOST_DIR=/opt/homebrew/opt/boost  (Apple Silicon)
```

**Linux — recommended:** build Boost with the **same compiler** you use for
Whisper and this repo.  The system `/usr/include/boost` package often fails
when you compile with `gcc-toolset-13`:

```bash
wget https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz
tar xf boost_1_84_0.tar.gz && cd boost_1_84_0
./bootstrap.sh --prefix="$HOME/local/boost-1.84.0"
./b2 -j"$(nproc)" install
```

Verify: `$HOME/local/boost-1.84.0/include/boost/version.hpp` exists.

#### 3. Whisper (source checkout)

Whisper is a **separate** Tenstorrent repository (ask your team for the clone
URL if it is not on the public internet).  Place it next to this repo or
anywhere you like:

```bash
cd /path/to/parent/of/tt-oca-sim
git clone <whisper-repo-url> whisper/whisper
```

Verify: `whisper/whisper/GNUmakefile` exists.

`run_tests.sh` builds Whisper automatically (`MEM_CALLBACKS=1`, C++20,
`BOOST_ROOT=$BOOST_DIR`) the first time you run it.  You only need the source
tree plus Boost — not a pre-built `librvcore.a`.

Suggested layout (auto-detected without setting `WHISPER_HOME`):

```
parent/
├── tt-oca-sim/          ← this repo
└── whisper/whisper/     ← GNUmakefile here
```

#### 4. SystemC CCI (PLIC integration tests)

Required when `SMC_BUILD_PLIC_INTEGRATION=ON` (the default).  Clone and
install [Accellera CCI](https://github.com/accellera-official/cci):

```bash
git clone https://github.com/accellera-official/cci.git
cd cci
cmake -S . -B build \
      -DCMAKE_CXX_STANDARD=20 \
      -DCMAKE_INSTALL_PREFIX="$HOME/local/cci-install"
cmake --build build -j"$(nproc)"
cmake --install build
```

Verify: `$HOME/local/cci-install/include/cci_configuration` and
`lib64/libcci-config.so` exist.

To skip CCI (cluster builds without the PLIC integration test):

```bash
cmake -S . -B build -DSMC_BUILD_PLIC_INTEGRATION=OFF
```

#### 5. Record paths (`deps.env`)

Copy the example file and fill in **your** install prefixes:

```bash
cd smc/cpu_cluster
cp deps.env.example deps.env
```

Edit `deps.env`:

```bash
export SYSTEMC_HOME=$HOME/local/systemc-2.3.4
export BOOST_DIR=$HOME/local/boost-1.84.0
export WHISPER_HOME=/path/to/whisper/whisper    # directory with GNUmakefile
export CCI_HOME=$HOME/local/cci-install
```

`deps.env` is gitignored — each developer keeps their own copy.  Alternatively
put the same `export` lines in your `~/.bashrc`.

#### 6. Build and run

```bash
./run_tests.sh
```

On first run you should see `>> Whisper not built yet; building in ...` if
Whisper has never been compiled on this machine.  Subsequent runs are
incremental.

### What the script auto-detects vs what you must install

| Item | You install / clone | Script does |
|------|---------------------|-------------|
| SystemC | Yes (step 1) | Probes Homebrew, `/usr/local`, `/usr`; else uses `deps.env` |
| Boost | Yes (step 2) | Same |
| Whisper **source** | Yes (step 3) | **Builds** archives if missing |
| CCI | Yes (step 4) | Probes `/usr/local/cci`, Homebrew; else uses `deps.env` |
| `lz4` | OS package (`lz4-devel`) | CMake links it when Whisper was built with LZ4 |

### Manual Whisper build (optional)

`run_tests.sh` normally handles this.  To build Whisper yourself:

```bash
export WHISPER_HOME=/path/to/whisper-source
export BOOST_ROOT=/path/to/boost    # same tree as BOOST_DIR

cd "$WHISPER_HOME"
make MEM_CALLBACKS=1 CXX_STD=c++20 \
    build-$(uname -s)/librvcore.a build-$(uname -s)/whisper
```

| Knob | Why |
|------|-----|
| `MEM_CALLBACKS=1` | **Required.** The cluster memory bridge uses Whisper's callback API. |
| `CXX_STD=c++20` | Match the C++20 standard this project and SystemC use. |
| `BOOST_ROOT` | Whisper compiles against Boost headers; use a Boost tree compatible with your compiler (not always `/usr/include/boost`). |

Whisper's default build enables LZ4 snapshot compression.  The CMake wrapper
in `external/whisper-cmake/` links `liblz4` automatically when needed.
Install `lz4-devel` on RHEL/CentOS if the link step cannot find it.

### Troubleshooting

| Symptom | Fix |
|---------|-----|
| `SYSTEMC_HOME not set` | Complete README step 1; set path in `deps.env`. |
| `BOOST_DIR not set` | Complete README step 2; on RHEL do not rely on `/usr/include/boost` with gcc-toolset — build Boost locally. |
| `WHISPER_HOME not set and no Whisper source tree found` | Complete README step 3; clone to `../whisper/whisper` or set `WHISPER_HOME` in `deps.env`. |
| Whisper compile errors in Boost headers | `BOOST_DIR` must match your compiler; rebuild Boost (step 2) and re-run `./run_tests.sh`. |
| Link: `undefined reference to LZ4F_*` | `sudo yum install lz4-devel` (or equivalent), `./run_tests.sh --clean`. |
| CMake: `CCI not found at CCI_HOME` | Complete README step 4, or `-DSMC_BUILD_PLIC_INTEGRATION=OFF`. |
| Link: `sc_api_version_*` undefined | SystemC was not built with C++20 — rebuild SystemC (step 1) and wipe `build/`. |

### Manual CMake (optional)

If you prefer not to use the script, export the same environment variables and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Script options

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
  `./run_tests.sh --coverage` (or `-DENABLE_COVERAGE=ON` at configure time);
  the `coverage` custom target renders gcovr HTML under `build/coverage/`.
  Current baseline: **97.1 %** line coverage on `src/`.

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
| Coverage via `-DENABLE_COVERAGE=ON` / `./run_tests.sh --coverage` §A.4   | ✅ (97.1 % line on `src/`) |
