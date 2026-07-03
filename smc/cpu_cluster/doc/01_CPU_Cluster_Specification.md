# SMC CPU Cluster — Specification

**Document:** `01_CPU_Cluster_Specification.md`  
**Module:** `smc::smc_cpu_cluster`  
**Status:** Active — SystemC / TLM-2.0 Loosely-Timed model

---

## 1. Purpose

The SMC CPU Cluster is the per-chiplet **RV64GC management processor** (1–4
harts) in the Open Chiplet Atlas SMC.  This SystemC model wraps the
Tenstorrent **Whisper** instruction-set simulator behind an `iss_hart`
interface so the rest of the SMC IP library can bind TLM sockets and IRQ
wires without depending on Whisper headers.

Normative hardware references:

| Document | Relevance |
|----------|-----------|
| `01_SMC_Architecture.pdf` §3 | IP role, parameters, chiplet context |
| `02_SMC_IP_LowLevel_Design.pdf` §3 | TLM sockets, register map, IRQ, routing |
| `03_SMC_Test_Plan.pdf` §A.1–A.6, §B.3 | Project-wide verification tiers |

Implementation detail: **`02_CPU_Cluster_LowLevel_Design.md`**.  
Verification detail: **`03_CPU_Cluster_Test_Plan.md`**.

---

## 2. Module boundary

```cpp
SC_MODULE(smc_cpu_cluster) {
    // Initiators (§3.6 bus-bridge routing)
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> data;
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> mmio;
    tlm_utils::simple_initiator_socket<smc_cpu_cluster, 64> ifetch;

    // CPU-Control register file target (§3.8, 8 KiB window)
    tlm_utils::simple_target_socket<smc_cpu_cluster, 64> ctrl;

    // Per-hart IRQ inputs (§3.7)
    sc_vector<sc_in<bool>> irq_sw;      // CLINT MSIP
    sc_vector<sc_in<bool>> irq_timer;   // CLINT MTIP
    sc_vector<sc_in<bool>> irq_ext;     // PLIC MEIP

    explicit smc_cpu_cluster(sc_module_name name);
    smc_cpu_cluster(sc_module_name name, const config& cfg);  // legacy struct
};
```

Every outgoing transaction carries `smc_axi_extension` (source ID, hart ID,
privilege, AMO lock hint).

---

## 3. Configuration

Runtime parameters are exposed as **SystemC CCI 1.0** `cci_param` members
(broker presets override defaults before construction).  The legacy
`smc_cpu_cluster::config` struct remains for backward-compatible
construction.

| Parameter | Default | Range / note |
|-----------|---------|--------------|
| `num_harts` | 1 | 1..4 |
| `reset_pc` | `0x80000000` | Per-hart boot vector |
| `isa` | `rv64imafdc` | Whisper ISA string |
| `fast_mem_lo` / `fast_mem_hi` | `0` / `0x80000000` | Internal flat memory window |
| `mmio_lo` / `mmio_hi` | `0x80000000` / `0x90000000` | MMIO carve-out (§3.6) |
| `quantum_ns` | 1000 | TLM LT global quantum (~1 µs) |
| `quantum_insts` | 1000 | Instructions per `step(K)` slice |
| `amo_lock_detect` | true | Assert `prot[3]` on AMO / LR-SC |
| `source_id` | `0x10` | `SMC_CPU_SOURCE_ID` on extension |
| `ctrl_size_bytes` | `0x2000` | CPU-Control aperture size |
| `local_base_default` | `0xC0000000` | RO `LOCAL_BASE` reset value |

See **`04_CCI_Integration_Guide.md`** for broker setup and migration notes.

---

## 4. Functional requirements (summary)

| ID | Requirement | Spec ref |
|----|-------------|----------|
| F1 | 1..4 `rv64imafdc` harts under TLM LT quantum | §3.4–3.5 |
| F2 | Fast-mem buffer + MMIO / data socket routing | §3.6 |
| F3 | CPU-Control CSR target at BASE+`0x001_0000` | §3.8 |
| F4 | IRQ aggregator maps MSIP/MTIP/MEIP → MIP | §3.7 |
| F5 | WFI park / IRQ wake per hart | §3.7 |
| F6 | `smc_axi_extension` on all initiator traffic | §3.10 |
| F7 | Whisper ISS via memory callbacks (`MEM_CALLBACKS=1`) | §A ISS integration |
| F8 | Debug API: `load_elf`, `hart(i)`, `inject_nmi`, CSR peek | §3.9 |

Full traceability matrix: **`03_CPU_Cluster_Test_Plan.md` §7**.

---

## 5. Out of scope

- Full PLIC IP verification (owned by `peripherals/plic/plic_tb`)
- CLINT MMIO model (IRQ wires are stubbed in `cluster_tb`)
- Production firmware / SoC-level tests
- Spike or non-Whisper ISS backends

---

## 6. Build & run

```bash
cd smc/cpu_cluster
cp deps.env.example deps.env   # first time — edit paths
./run_tests.sh
```

Dependencies: SystemC (C++20), Boost, Whisper source (auto-built by script),
SystemC CCI 1.0.  See **`README.md`** for first-time setup.

Coverage gate: **≥ 95%** line coverage on `src/` via `./run_tests.sh --coverage`
(current baseline **98.5%** — see test plan §9.2).
