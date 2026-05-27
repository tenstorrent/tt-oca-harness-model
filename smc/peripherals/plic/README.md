# SMC PLIC — SystemC / TLM-2.0 Loosely-Timed Model

A standards-compliant RISC-V PLIC modeled in Accellera SystemC 2.3.x +
TLM-2.0 (Loosely-Timed). Implements the SMC PLIC IP described in:

- `01_SMC_Architecture.md` §5 (IP #2) — modeling parameters and role
- `02_SMC_IP_LowLevel_Design.md` §4 — TLM interface and register map
- `hw/smc/smc_cpu/data/registers/rdl/plic.rdl` — authoritative register map
- `hw/smc/smc_cpu/chipyard_generated_files/1core/OCAH1CORECluster_TLPLIC.sv`
  — functional reference RTL

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP
library (CPU cluster, fabric, mailbox, BEU, …) can wire up exactly as
specified in the low-level design. It runs the SMC firmware's PLIC
driver (`fw/smc/common/drivers/riscv_plic0.c`) without modification.

---

## Layout

```
plic/
├── CMakeLists.txt
├── README.md                       (this file)
├── include/
│   ├── smc_tlm_extensions.h        Shared GP extension (smc_axi_extension)
│   └── plic.h                      SC_MODULE(plic) declaration
├── src/
│   └── plic.cpp                    Implementation
└── test/
    ├── CMakeLists.txt
    └── plic_tb.cpp                 Self-checking test bench
```

When the rest of the SMC IP library exists, drop `include/` and `src/`
into `libsmc/plic/` and replace the local `smc_tlm_extensions.h` with
the project-wide canonical version.

---

## Module interface

```cpp
SC_MODULE(plic) {
    tlm_utils::simple_target_socket<plic>      reg_socket;   // AXI4-Lite, 32-bit
    sc_vector<sc_in <bool>>                    src_in;       // num_sources level inputs
    sc_vector<sc_out<bool>>                    ctx_out;      // num_contexts (M+S per core)
    sc_in<bool>                                rst_n_i;
    explicit plic(sc_module_name, plic_cfg = plic_cfg{});
};
```

`plic_cfg` defaults to the SMC values from §2.1 of the architecture:

| Field          | Default | Note                                  |
|----------------|---------|---------------------------------------|
| `num_sources`  | 332     | Spec maximum (326 active)             |
| `num_contexts` | 8       | 4 cores × {M-mode, S-mode}            |

Both can be overridden for SiP variants with different core counts.

### Register window (BASE = `0xC400_0000`)

Standard RISC-V PLIC layout, 4 MB:

| Offset                              | Register                          | Access |
|-------------------------------------|-----------------------------------|--------|
| `0x000000 + 4·src`                  | `priority[src]` (3 bits used)     | RW     |
| `0x001000 + 4·word`                 | `pending[word]` (32 src/word)     | RO     |
| `0x002000 + 0x80·ctx + 4·word`      | `enable[ctx][word]`               | RW     |
| `0x200000 + 0x1000·ctx + 0x0`       | `threshold[ctx]` (3 bits used)    | RW     |
| `0x200000 + 0x1000·ctx + 0x4`       | `claim/complete[ctx]`             | RW†    |

† Read = atomic claim (highest pending source); Write = complete (re-arms
source if its line is still high). Both have side effects on `pending_`
and on `ctx_out[ctx]`.

All accesses must be 4-byte aligned, 4 bytes wide. Other sizes return
`TLM_BURST_ERROR_RESPONSE`; out-of-window addresses return
`TLM_ADDRESS_ERROR_RESPONSE`. DMI is never advertised.

### Interrupt semantics

Per the RISC-V PLIC specification:

- Sources are **level-sensitive**. The model latches `pending_[src]` on
  the rising edge of `src_in[src-1]`.
- A source whose `priority == 0` is permanently disabled.
- A source is visible to context `c` only when:
  `pending && enable[c][word] && (priority > threshold[c])`.
- `claim` returns the visible source with the highest priority; ties are
  broken by the lowest source ID (matches the Chipyard PLIC behaviour
  observed in `OCAH1CORECluster_PLICFanIn.sv`).
- After `claim`, the bit stays cleared until the matching `complete`
  write. If the underlying source line is still high at that moment, the
  pending bit is re-armed immediately (level-triggered re-assertion).
- `ctx_out[c]` is recomputed after every state change and only written
  when the value flips, to avoid spurious `sc_signal` events.

### Test-bench back-door

`transport_dbg` is implemented for memory-window registers and is
side-effect free (the claim register returns the *prospective* claim
without clearing pending). The module also exposes:

```cpp
uint32_t dbg_priority   (unsigned src) const;
bool     dbg_pending    (unsigned src) const;
bool     dbg_enable     (unsigned ctx, unsigned src) const;
uint32_t dbg_threshold  (unsigned ctx) const;
uint32_t dbg_claim_top  (unsigned ctx) const;
void     dump_state     (std::ostream& = std::cout) const;
```

These satisfy `03_SMC_Test_Plan.md` §3 inspection requirements.

---

## Building

The model needs Accellera SystemC ≥ 2.3.4 (preferably 3.0). It links
against `SystemC::systemc` if a CMake-installed SystemC is available;
otherwise it falls back to the `SYSTEMC_HOME` environment variable.

```bash
cd plic
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Expected output:

```
==== SMC PLIC TB ====
  [PASS] reset clears state
  [PASS] priority R/W + 3-bit truncation
  [PASS] source 0 reserved
  [PASS] pending latched on rising edge
  [PASS] disabled source does not drive ctx_out
  [PASS] per-context enable independence
  [PASS] threshold gating
  [PASS] best-pending arbitration
  [PASS] claim/complete with line still high
  [PASS] complete after line de-asserted
  [PASS] TLM error responses
  [PASS] transport_dbg has no side effects
  [PASS] cross-context isolation
  [PASS] reset returns to clean state

ALL TESTS PASSED
```

---

## Wiring the PLIC into `smc_top`

Excerpt from how the architectural top-level instantiates and binds the
PLIC (see `02_SMC_IP_LowLevel_Design.md` "Top-level Integration"):

```cpp
// In smc_top constructor (4-core variant)
smc::plic ic("plic", smc::plic_cfg{ .num_sources=332, .num_contexts=8 });

// Fabric routes BASE+0x0400_0000 .. BASE+0x043F_FFFF here
fabric.to_plic.bind(ic.reg_socket);

// Reset distribution
ic.rst_n_i(rstu.rst_core_smc_n_o);

// Source aggregation -- one entry per spec-defined IRQ ID.
// Examples (see fw/smc/common/drivers/riscv_plic0.c for the full list):
ic.src_in[ MBOX0_OUT_IRQ - 1 ](mb.outbound_irq_o[0]);
ic.src_in[ MBOX0_IN_IRQ  - 1 ](mb.inbound_irq_o [0]);
ic.src_in[ UART0_IRQ     - 1 ](uart[0].irq_o);
ic.src_in[ I2C0_IRQ      - 1 ](i2c [0].irq_fmt_threshold_o);
ic.src_in[ AVS_IRQ       - 1 ](avs.irq_o);
ic.src_in[ PVT_TEMP_IRQ  - 1 ](pvt.temp_irq_o);
ic.src_in[ DMA_IRQ       - 1 ](dma.irq_o);
ic.src_in[ LOG0_IRQ      - 1 ](log[0].irq_o);
ic.src_in[ TELEMETRY_IRQ - 1 ](trx[0].irq_o);
// ... 332 lines total ...

// Context outputs go to the CPU cluster.  Convention:
//   ctx_out[2*hart + 0]  ->  meip_in[hart]   (M-mode)
//   ctx_out[2*hart + 1]  ->  seip_in[hart]   (S-mode)
for (unsigned h = 0; h < NCORES; ++h) {
    ic.ctx_out[2*h + 0](cpu.meip_in[h]);
    ic.ctx_out[2*h + 1](cpu.seip_in[h]);
}
```

Inbound `axi_filter` instances belong **between** the fabric and
`reg_socket`, exactly as documented in
`02_SMC_IP_LowLevel_Design.md` §2.

---

## Modeling notes

- **Loosely-timed**: every register access annotates a small fixed
  delay (`access_delay_`, default 2 ns). Override via the cfg struct
  if performance studies need a different profile.
- **Quantum**: the PLIC is purely target-side, so it does not own a
  `tlm_quantumkeeper`. It is invoked under the caller's quantum
  (CPU cluster, JTAG2AXI, BMC over `sys_axi_in`, …).
- **No DMI**: claim has read side-effects so DMI is intentionally
  refused.
- **Multi-clock**: per `01_SMC_Architecture.md` §3, the PLIC sits on
  `clk_smc_i`. In LT this is abstract — the only timing knob is
  `access_delay_`.
- **Whisper ISS interaction**: when the CPU cluster runs Whisper (the
  Tenstorrent / WD RISC-V ISS used by the SMC programme — see
  `02_SMC_IP_LowLevel_Design.md` Appendix A), Whisper's built-in PLIC
  model must be disabled (Appendix A.7.2) so that all PLIC traffic
  flows through this SystemC model. Whisper loads/stores to the PLIC
  window are routed via the ISS's MMIO callback hook into the bus
  bridge → `reg_socket`, and the PLIC's `ctx_out[2h+m]` drive the
  cluster's `meip_in[h]` / `seip_in[h]` ports, which the IRQ
  aggregator (Appendix A.6) then latches into each hart's `mip` CSR
  via Whisper's external-interrupt injection API. The integration
  contract is identical in shape to a Spike integration; only the
  C++ symbol names differ. See `02_PLIC_LowLevel_Design.md §13.5` for
  the full round-trip diagram.
- **Verification hooks**: see `dbg_*` methods and the included test
  bench. Coverage instrumentation can be added by hooking into
  `reg_read` / `reg_write` (one entry per register class) and into
  `claim()` / `complete()`.
- **Future AT extension**: outstanding-transaction modeling, fabric
  back-pressure and accurate latency are deliberately not in scope.
  The single point of change to add them is `b_transport`'s `delay`
  argument and a per-context queue around `claim()`.

---

## Conformance summary

| Spec requirement                                              | Status |
|---------------------------------------------------------------|--------|
| `SC_MODULE(plic)` shape from `02_..._LowLevel_Design.md` §4   | ✅      |
| 332 sources / 8 contexts default                              | ✅      |
| RISC-V PLIC register layout (`plic.rdl`)                      | ✅      |
| AXI4-Lite, 32-bit aligned `b_transport`                       | ✅      |
| Priority + source-id tie-break arbitration                    | ✅      |
| Threshold gating                                              | ✅      |
| Atomic claim (single `b_transport`)                           | ✅      |
| Complete with level-sensitive re-arm                          | ✅      |
| Per-context enable / threshold isolation                      | ✅      |
| Reset clears all state                                        | ✅      |
| `smc_axi_extension` honoured (placeholder; filtering external)| ✅      |
| `transport_dbg` back-door, no side effects                    | ✅      |
| `dump_state()` and per-bit `dbg_*` accessors                  | ✅      |
| TLM error response on misaligned / out-of-window accesses     | ✅      |
| DMI refused (claim has side effects)                          | ✅      |
