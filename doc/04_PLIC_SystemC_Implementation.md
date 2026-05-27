# SMC PLIC — SystemC / TLM-2.0 Implementation & Architecture

**Version**: 1.0
**Status**: Draft, code-validated against SystemC 3.0.2 Accellera reference
**Companion artefacts**:
  - `01_SMC_Architecture.md` §5 (IP #2) — modeling parameters
  - `02_SMC_IP_LowLevel_Design.md` §4 — public TLM interface
  - `03_SMC_Test_Plan.md` §3 — verification requirements
  - `plic_systemc/` — source code, build system, self-checking test bench
  - `hw/smc/smc_cpu/data/registers/rdl/plic.rdl` — authoritative register map
  - `hw/smc/smc_cpu/chipyard_generated_files/1core/OCAH1CORECluster_TLPLIC.sv` — functional reference RTL

---

## Contents

1. [Purpose & scope of this document](#1-purpose--scope-of-this-document)
2. [PLIC role in the SMC](#2-plic-role-in-the-smc)
3. [Architectural overview](#3-architectural-overview)
4. [SystemC module structure](#4-systemc-module-structure)
5. [TLM-2.0 interface](#5-tlm-20-interface)
6. [Register interface & address decode](#6-register-interface--address-decode)
7. [Internal data structures](#7-internal-data-structures)
8. [Behavioural algorithms](#8-behavioural-algorithms)
9. [Process / event topology — single-driver discipline](#9-process--event-topology--single-driver-discipline)
10. [Reset behaviour](#10-reset-behaviour)
11. [Loosely-timed timing model](#11-loosely-timed-timing-model)
12. [Error handling](#12-error-handling)
13. [Debug & verification hooks](#13-debug--verification-hooks)
14. [Integration with `smc_top` and the CPU cluster](#14-integration-with-smc_top-and-the-cpu-cluster)
15. [Verification & coverage plan](#15-verification--coverage-plan)
16. [Modeling decisions & trade-offs](#16-modeling-decisions--trade-offs)
17. [Known limitations & future work](#17-known-limitations--future-work)
18. [Code walkthrough — function reference](#18-code-walkthrough--function-reference)
19. [Build, packaging and dependency notes](#19-build-packaging-and-dependency-notes)
20. [Glossary](#20-glossary)

---

## 1. Purpose & scope of this document

`02_SMC_IP_LowLevel_Design.md` §4 defines **what** the PLIC SystemC
module looks like from the outside (sockets, signals, register layout).
This document defines **how** the model is implemented internally:

- the data structures it owns,
- the SystemC process topology that drives them,
- the algorithms used for source latching, arbitration, and
  claim/complete,
- the design decisions taken to satisfy SystemC 3.0's strict single-driver
  rule, the RISC-V PLIC v1.0 spec, and the SMC's loosely-timed budget,
- the verification strategy implemented in the included test bench, and
- the exact integration recipe required to drop the model into
  `smc_top` and run firmware against it.

It is the implementation contract that any future maintainer (or an
alternate implementation that wants to remain bit-compatible with the
SMC PLIC) should be able to read in isolation.

---

## 2. PLIC role in the SMC

The PLIC (Platform-Level Interrupt Controller) is the single
prioritised aggregator that turns the ~332 device-level interrupt lines
present in the SMC chiplet into the four to eight per-hart **MEIP /
SEIP** lines that the Rocket cores' RISC-V CSRs consume. Without the
PLIC the firmware would have to poll every peripheral; with it, the
SMC reacts to mailbox traffic, FLR requests, errors, telemetry events
and timer expiries as M-mode (or S-mode) external interrupts.

| Role                        | Concretely in the SMC                          |
|-----------------------------|------------------------------------------------|
| Interrupt aggregator        | Mailboxes, BEUs, WDTs, eFuse, AVS, PVT, UART, I²C, LogEngine, GPIO, telemetry, DMA, PVT temp threshold, SiP/SEP-external lines |
| Priority arbiter            | 7 priority levels, tie-break by lowest source ID |
| Per-context masking         | 8 contexts (4 cores × {M, S}), independent enable + threshold |
| Atomic claim/complete       | Single TLM transaction = single CSR access from Spike or Rocket |
| Filter target               | Reachable from `sys_axi_in`, `sep_axi_in`, `jtag_axi_in` through the SMC fabric and the inbound `axi_filter` instances |

`fw/smc/common/drivers/riscv_plic0.c` is the firmware driver that runs
against this model in early bring-up; the driver is unmodified RISC-V
PLIC code and exercises every register described in §6.

---

## 3. Architectural overview

```
                                                       ┌──────────────────────┐
 src_in[0]    (level)   ┌─────────────┐                │   ctx_out[0]  → MEIP[0]
 src_in[1]    (level) ──►│  Gateway /  │                │   ctx_out[1]  → SEIP[0]
   ...                  │  edge-detect│                │   ctx_out[2]  → MEIP[1]
 src_in[N-1]  (level) ──►│  + level   │                │   ctx_out[3]  → SEIP[1]
                        │  latch      │                │       ...
                        └──────┬──────┘                │   ctx_out[7]  → SEIP[3]
                               │                       └──────────▲───────────┘
                               ▼                                  │
                      ┌────────────────┐    ┌───────────────────────────────┐
                      │  pending_[]    │    │  output_method (single owner) │
                      │  in_flight_[]  │◄───┤  - reads pending/enable/      │
                      │  last_level_[] │    │    threshold/priority         │
                      └───┬────────────┘    │  - drives ctx_out[]           │
                          │                 └───────────────▲───────────────┘
                          │ schedule_recompute()            │  recompute_event_
                          ▼                                 │
                      ┌────────────────────────────────────┐│
                      │  best_pending(ctx)                 ││
                      │  - filter pending  by enable[ctx]  ││
                      │  - filter prio > threshold[ctx]    ││
                      │  - argmax(prio, -src_id)           ││
                      └────────────────────────────────────┘│
                                                            │
                ┌───────────────────────────────────────────┴────┐
                │                  Register file                  │
                │  priority_[] · enable_[ctx][] · threshold_[]   │
                │  pending_[] (RO from SW)                       │
                └───────────────────────▲───────────────────────┘
                                        │
                                        │ b_transport / transport_dbg
                                        │
                            ┌───────────┴────────────┐
                            │   reg_socket           │
                            │ (simple_target_socket) │
                            └───────────▲────────────┘
                                        │
                                        │  AXI4-Lite, 32-bit aligned, 4-byte
                                        │
                                        │  ◄── axi_filter[k]  ◄── smc_fabric
                                        │  ◄── (optional debug back-door)
                                        ▼
                                 (CPU cluster, JTAG2AXI, BMC over sys_axi_in)
```

Key flow: any state change that may affect any context output (input
edge, register write, claim, complete, reset) triggers the internal
`recompute_event_`. A single `SC_METHOD output_method` is sensitive to
that event and is the only writer of `ctx_out[]`. This satisfies
SystemC 3.0's strict one-driver-per-`sc_signal` rule (see §9).

---

## 4. SystemC module structure

```cpp
namespace smc {

struct plic_cfg {
    unsigned num_sources  = 332;          // 1..1023
    unsigned num_contexts = 8;            // 4 cores * {M, S}

    static constexpr uint64_t PRIORITY_BASE   = 0x000000;
    static constexpr uint64_t PENDING_BASE    = 0x001000;
    static constexpr uint64_t ENABLE_BASE     = 0x002000;
    static constexpr uint64_t ENABLE_STRIDE   = 0x000080;
    static constexpr uint64_t CONTEXT_BASE    = 0x200000;
    static constexpr uint64_t CONTEXT_STRIDE  = 0x001000;
    static constexpr uint64_t CONTEXT_THR_OFF = 0x000000;
    static constexpr uint64_t CONTEXT_CC_OFF  = 0x000004;
    static constexpr uint64_t WINDOW_SIZE     = 0x400000; // 4 MB
};

class plic : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(plic);

    tlm_utils::simple_target_socket<plic>     reg_socket;
    sc_core::sc_vector<sc_core::sc_in<bool>>  src_in;       // num_sources
    sc_core::sc_vector<sc_core::sc_out<bool>> ctx_out;      // num_contexts
    sc_core::sc_in<bool>                      rst_n_i;

    explicit plic(sc_core::sc_module_name name,
                  plic_cfg cfg = plic_cfg{});

    // Test-bench back-door (no socket, no side effects)
    uint32_t dbg_priority   (unsigned src)              const;
    bool     dbg_pending    (unsigned src)              const;
    bool     dbg_enable     (unsigned ctx, unsigned s)  const;
    uint32_t dbg_threshold  (unsigned ctx)              const;
    uint32_t dbg_claim_top  (unsigned ctx)              const;
    void     dump_state     (std::ostream& = std::cout) const;

private:
    void b_transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    unsigned int transport_dbg(tlm::tlm_generic_payload&);

    void reset_proc();          // SC_METHOD on rst_n_i
    void src_method();          // SC_METHOD on every src_in[i]
    void output_method();       // SC_METHOD on recompute_event_
    void schedule_recompute();

    bool reg_read (uint64_t off, uint32_t& data);
    bool reg_write(uint64_t off, uint32_t  data);

    uint32_t claim   (unsigned ctx);
    void     complete(unsigned ctx, uint32_t src);
    uint32_t best_pending(unsigned ctx) const;

    plic_cfg cfg_;
    std::vector<uint32_t> priority_;        // [num_sources+1]
    std::vector<bool>     pending_;         // [num_sources+1]
    std::vector<bool>     last_level_;      // edge-detect helper
    std::vector<bool>     claim_in_flight_; // [num_sources+1]
    std::vector<std::vector<uint32_t>> enable_; // [ctx][word]
    std::vector<uint32_t> threshold_;       // [num_contexts]
    std::vector<bool>     ctx_out_cache_;   // last-driven ctx_out value
    sc_core::sc_event     recompute_event_;
    sc_core::sc_time      access_delay_ = sc_core::sc_time(2, sc_core::SC_NS);
};

} // namespace smc
```

Why every choice on this list:

- **`SC_HAS_PROCESS` + class (not `SC_MODULE` macro)** — the macro pulls
  in a default constructor; we need to forward `plic_cfg` so the field
  vectors can be sized from constructor arguments.
- **`tlm_utils::simple_target_socket`** — matches every other SMC IP
  per §6.1 of the architecture document. Default data width 64-bit is
  fine because we reject any `data_length != 4`.
- **`sc_vector<sc_in<bool>>`** for sources — lets the size be passed at
  construction time and exposes individual `[i]` ports for binding from
  every SMC peripheral's `irq_o`.
- **`sc_in<bool> rst_n_i`** — single boolean reset, sampled level-low
  per the convention of every other SMC IP (`02_..._LowLevel_Design.md`
  "Convention used throughout this document").
- **One STL vector per state class** — over a fixed-size `std::array`
  because the size depends on `cfg_`. The number of words used by
  `enable_` is `ceil((num_sources+1) / 32)` — for the spec defaults
  that is 11 words per context.
- **`ctx_out_cache_`** — `sc_signal` change events only fire when the
  written value differs from the current value; nevertheless we
  maintain an explicit cache so that `output_method` is idempotent and
  cheap (no spurious socket activity, predictable wake patterns).

---

## 5. TLM-2.0 interface

### 5.1 Socket binding

```cpp
reg_socket.register_b_transport   (this, &plic::b_transport);
reg_socket.register_transport_dbg (this, &plic::transport_dbg);
```

DMI is **deliberately not registered**. The PLIC's claim/complete
register has a read side-effect (claim returns the current top and
clears its pending bit), so direct memory access would silently corrupt
the model.

### 5.2 `b_transport`

```cpp
void plic::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay) {
    const auto cmd = gp.get_command();
    const auto adr = gp.get_address();
    const auto len = gp.get_data_length();
    auto* buf      = gp.get_data_ptr();

    if (len != 4 || (adr & 0x3u) != 0) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (adr >= cfg_.WINDOW_SIZE) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    smc::smc_axi_extension* ext = nullptr;
    gp.get_extension(ext);                    // honoured but not enforced

    bool ok;
    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = 0; ok = reg_read(adr, v); if (ok) std::memcpy(buf, &v, 4);
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t v = 0; std::memcpy(&v, buf, 4); ok = reg_write(adr, v);
    } else {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    delay += access_delay_;
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                              : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    gp.set_dmi_allowed(false);
}
```

Behavioural points:

| Aspect                                | Behaviour                          |
|---------------------------------------|------------------------------------|
| Required access width                 | Exactly 4 bytes                    |
| Required alignment                    | 4-byte                             |
| Out-of-window address                 | `TLM_ADDRESS_ERROR_RESPONSE`       |
| Misaligned / oversize / undersize     | `TLM_BURST_ERROR_RESPONSE`         |
| Unsupported command                   | `TLM_COMMAND_ERROR_RESPONSE`       |
| Extension `smc_axi_extension`         | Looked up; not enforced (filtering belongs in `axi_filter`, see §16) |
| `delay` annotation                    | += `access_delay_` (default 2 ns)  |
| DMI hint                              | Always cleared                     |
| Byte-enable                           | Ignored (32-bit register accesses) |

### 5.3 `transport_dbg`

`transport_dbg` is the side-effect-free back door. It implements the
same address decode as `b_transport` but with one critical difference:
**reads of the claim/complete register do not claim**. Instead they
return the current best-pending source ID. Writes are forwarded to
`reg_write`, which means a debug write to claim/complete still performs
a `complete()` — that is intentional, because completing an interrupt
from the test bench is a legitimate stimulus operation.

### 5.4 `smc_axi_extension`

The shared SMC GP extension carries `source_id`, `prot`, `cacheable`,
`non_secure`, `axi_id`, `axi_user`. Per `02_..._LowLevel_Design.md`
§2 ("AXI Filter"), all access-control checks belong in the inbound
filter that sits in front of the PLIC. The PLIC therefore inspects but
does not enforce the extension. This keeps responsibility cleanly
divided and matches the test plan, which exercises filter rules
through `axi_filter` events rather than through the PLIC.

---

## 6. Register interface & address decode

The 4 MB window is divided into four banks. Each bank has its own
arithmetic for source / context indexing; the decode logic in
`reg_read` and `reg_write` performs a strict range check before
indexing.

### 6.1 Address layout

| Offset (BASE+`0xC400_0000`)                                   | Register                          | Access | Note |
|---------------------------------------------------------------|-----------------------------------|--------|------|
| `0x000000 + 4·src`                                            | `priority[src]` — bits[2:0]       | RW     | src ∈ 1..332; src=0 reads as 0 (RAZ) |
| `0x001000 + 4·word`                                           | `pending[word]` — 32 src/word     | RO     | word ∈ 0..10 for 332 sources |
| `0x002000 + 0x80·ctx + 4·word`                                | `enable[ctx][word]`               | RW     | bit 0 of word 0 is forced to 0 (src 0 reserved) |
| `0x200000 + 0x1000·ctx + 0x0`                                 | `threshold[ctx]` — bits[2:0]      | RW     | values 0..7 |
| `0x200000 + 0x1000·ctx + 0x4`                                 | `claim/complete[ctx]`             | RW†    | read = claim; write = complete |

† Both directions have side effects on `pending_` / `claim_in_flight_`
and may change `ctx_out`. `transport_dbg` reads bypass the side
effects; writes do not.

### 6.2 Decode pseudocode

```
if   off in PRIORITY range :    handle priority(off)
elif off in PENDING  range :    handle pending(off)
elif off in ENABLE   range :    ctx, word = decode(off, ENABLE_BASE,  ENABLE_STRIDE);  handle enable(ctx, word)
elif off in CONTEXT  range :    ctx, sub  = decode(off, CONTEXT_BASE, CONTEXT_STRIDE); handle context(ctx, sub)
else                       :    out-of-window error
```

The implementation is a flat `if/else` ladder rather than a hash table
or switch on a bank-id because register access is rare in absolute
terms (firmware-driven, not on the data path) and the linear sequence
is easier to inspect, fits in I-cache, and gives obvious branch
behaviour.

### 6.3 RDL conformance

The OCAH `plic.rdl` declares 337 priority entries and 11 pending words.
The SMC architecture document §2.1 caps the active source count at 332
("332 sources, 326 active"). The implementation chooses 332 as the
public configuration default but parameterises `num_sources`. The
extra slots in the RDL are present for future expansion and decode to
RAZ in this model — exactly mirroring how unused bits 332..336 in the
real RDL behave.

### 6.4 Source-0 reserved-ness

The RISC-V PLIC spec reserves source ID 0 as "no interrupt". The
implementation enforces this in three places:

1. `reg_read` / `reg_write` of `priority[0]` returns / accepts but
   silently ignores writes (RAZ/WI).
2. `reg_write` of any `enable[ctx][0]` masks bit 0 to zero before
   storing.
3. `best_pending` iterates from `src=1` upwards, so source 0 cannot
   ever be returned as the claim result.

### 6.5 Last-word source masking

For 332 sources the last enable word is `word 10`, which contains
sources 320..331 in bits 0..11. Bits 12..31 are masked to zero on
writes and read as zero. This prevents firmware writing junk into the
"reserved" tail and seeing it appear in subsequent reads — important
because `riscv_plic0.c` walks all 11 words during initialisation.

---

## 7. Internal data structures

| Member            | Type                                  | Purpose |
|-------------------|---------------------------------------|---------|
| `priority_`       | `vector<uint32_t>` size N+1           | Per-source priority register backing store |
| `pending_`        | `vector<bool>`     size N+1           | Per-source latched pending bit |
| `last_level_`     | `vector<bool>`     size N+1           | Edge-detection helper for level→edge translation |
| `claim_in_flight_`| `vector<bool>`     size N+1           | Set by `claim()`, cleared by `complete()` |
| `enable_`         | `vector<vector<uint32_t>>` ctx × W    | Per-context, per-word enable bitmap |
| `threshold_`      | `vector<uint32_t>` size num_contexts  | Per-context priority threshold |
| `ctx_out_cache_`  | `vector<bool>`     size num_contexts  | Last value driven onto `ctx_out[c]` |
| `recompute_event_`| `sc_event`                            | Internal scheduler event for `output_method` |
| `access_delay_`   | `sc_time`                             | LT delay annotated per `b_transport` |

`N = num_sources`, `W = ceil((N+1)/32)`.

The `+1` in every `[N+1]`-sized vector is deliberate: index 0 is
unused (source IDs start at 1 by spec). Keeping the index space
identical to the spec's source IDs eliminates a constant `-1` in every
arithmetic site and makes the code trivially comparable to the RTL and
the firmware driver.

`std::vector<bool>` is the standard library's bit-packed specialisation.
It costs ~`N/8` bytes per signal vector — an acceptable footprint for
332 sources (~83 bytes per vector × 4 vectors ≈ 330 bytes total).

---

## 8. Behavioural algorithms

### 8.1 Source latching (`src_method`)

```cpp
void plic::src_method() {
    bool any_change = false;
    for (unsigned i = 0; i < cfg_.num_sources; ++i) {
        const unsigned src = i + 1;
        const bool now = src_in[i].read();
        const bool was = last_level_[src];
        if (now && !was) {
            if (!claim_in_flight_[src]) {
                pending_[src] = true;
                any_change    = true;
            }
        }
        last_level_[src] = now;
    }
    if (any_change) schedule_recompute();
}
```

Semantics — derived from RISC-V PLIC v1.0 §4 "Interrupt Gateways":

- The PLIC sees device sources as **level-sensitive** (every SMC source
  meets this contract; OpenTitan-style peripherals all expose
  level-active interrupts).
- The gateway converts level → IP (interrupt-pending message) on the
  rising edge and waits for completion before allowing a new IP from
  the same source.
- Therefore: rising edge sets `pending_` only when not in-flight;
  falling edge does **not** clear `pending_`; level-active state is
  re-checked at `complete()` time.

Why an `SC_METHOD` (not an `SC_THREAD`):

- We want the body to run once per change of any source line, with no
  state suspended across calls. `SC_METHOD` is exactly that primitive
  and avoids the cost of a per-thread stack.
- Sensitivity is constructed once at elaboration time (`for (auto& p
  : src_in) sensitive << p;`), so the scheduler wakes us only when at
  least one source toggles.

Cost: O(num_sources) per invocation. For the SMC default (332 sources)
this is < 1 µs of host CPU time per fully-saturated wave-front and
typically much less because most invocations are caused by a single
source flipping.

### 8.2 Best-pending arbitration (`best_pending`)

```cpp
uint32_t plic::best_pending(unsigned ctx) const {
    const uint32_t thr = threshold_[ctx];
    uint32_t best_src  = 0;
    uint32_t best_prio = 0;
    for (unsigned src = 1; src <= cfg_.num_sources; ++src) {
        if (!pending_[src]) continue;
        const uint32_t prio = priority_[src];
        if (prio == 0 || prio <= thr) continue;
        const uint32_t word = word_index(src);
        if ((enable_[ctx][word] & bit_mask(src)) == 0) continue;
        if (prio > best_prio || (prio == best_prio && best_src == 0)) {
            best_prio = prio;
            best_src  = src;
        }
    }
    return best_src;
}
```

Filter chain (in order):

1. `pending`        — only pending sources are eligible.
2. `priority != 0`  — RISC-V semantics: priority 0 disables the source.
3. `priority > threshold[ctx]` — strictly greater, per the spec.
4. `enable[ctx]`    — context must have the source enabled.

Tie-break: the loop visits sources in increasing ID order and only
overwrites `best_src` when it finds a strictly higher priority, so the
**lowest source ID wins ties**. This matches the Chipyard PLIC fan-in
(see `OCAH1CORECluster_PLICFanIn.sv`) and the SiFive PLIC manual.

Why a linear scan rather than a `std::priority_queue` — the original
low-level design proposal in `02_..._LowLevel_Design.md` §4 mentions a
`priority_queue`, but a heap requires explicit `update_key` / `remove`
operations which `std::priority_queue` does not support. Implementing
those would mean either a custom heap or a `std::set`, both of which
have worse constants for N ≤ 1024 and are far more error-prone. The
linear scan runs in ~ N memory loads, vectorises easily on a modern
CPU, and produces the correct answer trivially.

### 8.3 Claim / complete handshake

```cpp
uint32_t plic::claim(unsigned ctx) {
    const uint32_t src = best_pending(ctx);
    if (src != 0) {
        pending_[src]         = false;
        claim_in_flight_[src] = true;
        schedule_recompute();
    }
    return src;
}

void plic::complete(unsigned ctx, uint32_t src) {
    (void)ctx;
    if (src == 0 || src > cfg_.num_sources) return;
    claim_in_flight_[src] = false;
    if (last_level_[src] && !pending_[src]) pending_[src] = true;
    schedule_recompute();
}
```

State machine per source ID:

```
                   src_in rising edge && !in_flight
       ┌────────────────────────────────────────────┐
       │                                            │
       ▼                                            │
   IDLE ──── src_in rising edge ────► PENDING ─── claim() ─► IN_FLIGHT
                                          ▲                       │
                                          │                       │
                                          └── complete() ─────────┘
                                              if src_in still high
```

Notes:

- `complete(ctx, src)` deliberately ignores `ctx`. The RISC-V PLIC spec
  says the EOI write effectively addresses the source globally — it
  clears the in-flight latch regardless of which context performed the
  claim. Most software writes back from the same context but the spec
  does not require it.
- The check `last_level_[src] && !pending_[src]` is the
  level-re-assertion hook. If the source line is still high at
  completion time, the next interrupt fires immediately.
- Out-of-range source IDs in `complete()` are silently dropped, again
  per spec (the PLIC must tolerate spurious EOI writes).
- Both functions call `schedule_recompute()` so that `ctx_out` is
  re-evaluated. Even inside the same `b_transport` call, this is
  cheap: the event is notified with `SC_ZERO_TIME`, runs in the next
  delta cycle, and only `ctx_out_cache_` differences propagate to
  signals.

### 8.4 Output recompute (`output_method`)

```cpp
void plic::output_method() {
    for (unsigned c = 0; c < cfg_.num_contexts; ++c) {
        const uint32_t top = best_pending(c);
        const bool     hi  = (top != 0);
        if (ctx_out_cache_[c] != hi) {
            ctx_out_cache_[c] = hi;
            ctx_out[c].write(hi);
        }
    }
}
```

This is the **single owner of `ctx_out`** (see §9). It:

- Reads the (possibly just-mutated) internal state.
- Computes `best_pending` per context (cost: O(num_sources × num_contexts)).
- Writes the new value to each `ctx_out[c]` only if it differs from the
  cached one.

For the SMC defaults this is 332 × 8 = 2,656 memory loads per
recompute. This is unmeasurable in any realistic firmware workload —
the recompute is only triggered by edges, register writes, and
claim/complete operations.

---

## 9. Process / event topology — single-driver discipline

SystemC 3.0 enforces a strict rule: **each `sc_signal<T>` must have
exactly one driver process**. Earlier versions warned about violations;
3.0 raises a fatal error.

The PLIC has multiple state-mutation paths:

| Mutator                       | Caller / context                                |
|-------------------------------|-------------------------------------------------|
| `reset_proc`                  | SC_METHOD on `rst_n_i.value_changed_event()`    |
| `src_method`                  | SC_METHOD on every `src_in[i]` change           |
| `b_transport` → `reg_write`   | The driver process (CPU bridge, JTAG2AXI, BMC)  |
| `b_transport` → `claim`       | Same                                            |
| `b_transport` → `complete`    | Same                                            |

If any of these wrote to `ctx_out[]` directly, the signal would have
multiple drivers (the SC_METHODs are distinct processes; `b_transport`
runs in the calling driver's process context) and SystemC 3.0 would
abort. Even on older SystemC versions the same race produces
`unstable signal` reports.

The implementation's solution:

1. Each mutator only updates **internal C++ state** (`pending_`,
   `priority_`, `enable_`, `threshold_`, `claim_in_flight_`, …).
2. After every mutation it calls `schedule_recompute()`, which is just
   `recompute_event_.notify(SC_ZERO_TIME)`.
3. `output_method` is the only `SC_METHOD` sensitive to
   `recompute_event_`, and it is the only function that ever writes
   to `ctx_out[]`.

This pattern has the additional benefit that all output transitions
happen in a deterministic delta cycle after the source change — a
clean serialisation point for waveform analysis and assertions.

```
   reset_proc      src_method       b_transport (driver thread)
        │                │                │
        │ schedule_      │ schedule_      │ schedule_
        ▼ recompute()    ▼ recompute()    ▼ recompute()
                  recompute_event_
                         │
                         ▼ delta+1
                   output_method
                         │
                         ▼
                    ctx_out[0..C-1]
```

---

## 10. Reset behaviour

```cpp
void plic::reset_proc() {
    if (rst_n_i.read()) return;            // act on assertion (low)
    std::fill(priority_       .begin(), priority_       .end(), 0u);
    std::fill(pending_        .begin(), pending_        .end(), false);
    std::fill(last_level_     .begin(), last_level_     .end(), false);
    std::fill(claim_in_flight_.begin(), claim_in_flight_.end(), false);
    for (auto& v : enable_) std::fill(v.begin(), v.end(), 0u);
    std::fill(threshold_.begin(), threshold_.end(), 0u);
    schedule_recompute();
}
```

- Triggered on every change of `rst_n_i` (`SC_METHOD … sensitive <<
  rst_n_i; dont_initialize();`).
- Acts only on the falling edge (i.e. while reset is asserted). On
  deassertion the model continues to operate normally with the just-
  cleared state.
- Clears `last_level_` so the *next* rising edge of every source is
  freshly latched (otherwise a source held high through reset would
  never re-pend).
- Calls `schedule_recompute()` so that `ctx_out` falls in a single
  predictable delta cycle.
- Does **not** reset the `recompute_event_` — events are stateless;
  they are simply re-notified.

The PLIC is on `clk_smc_i`, so the relevant reset is `rst_core_smc_n`
sourced from the SMC reset unit (`02_..._LowLevel_Design.md` §10).
`smc_top` binds:

```cpp
ic.rst_n_i(rstu.rst_core_smc_n_o);
```

---

## 11. Loosely-timed timing model

The PLIC is purely a target IP — it never initiates traffic. There is
no `tlm_quantumkeeper`; the PLIC participates in whatever quantum the
caller (typically `smc_cpu_cluster`) is running.

Timing annotations:

- `b_transport` adds `access_delay_` (default 2 ns, configurable at
  construction by overriding the field — exposed as a future config
  knob if needed).
- `transport_dbg` adds nothing — debug accesses are zero-time by
  TLM-2.0 convention.
- All internal events (`recompute_event_`) are notified with
  `SC_ZERO_TIME`, so the output update is instantaneous from the
  loosely-timed perspective.

For a future approximately-timed (AT) extension the natural change
points are:

1. Add a per-context queue around `claim()` to model fabric latency.
2. Replace the `access_delay_` constant with a histogram-driven model.
3. Use `nb_transport_fw/bw` instead of `b_transport`.

None of these are in scope for the LT model.

---

## 12. Error handling

| Condition                         | Detection                            | Response                          |
|-----------------------------------|--------------------------------------|-----------------------------------|
| Address ≥ window size             | `b_transport` precheck               | `TLM_ADDRESS_ERROR_RESPONSE`      |
| Misaligned address                | `b_transport` precheck               | `TLM_BURST_ERROR_RESPONSE`        |
| Wrong access width (≠ 4 bytes)    | `b_transport` precheck               | `TLM_BURST_ERROR_RESPONSE`        |
| Unsupported command (TLM_IGNORE)  | `b_transport` switch                 | `TLM_COMMAND_ERROR_RESPONSE`      |
| Reserved register / hole          | `reg_read`/`reg_write` returns true with data=0 | `TLM_OK_RESPONSE`, RAZ/WI |
| Construction with bad cfg         | Constructor                          | `SC_REPORT_FATAL`                 |
| Out-of-range source in `complete` | Silently dropped                     | (per RISC-V PLIC spec)            |

`SC_REPORT_FATAL` is used only at elaboration time so failures are
caught before `sc_start`. Runtime failures use TLM response codes so
that the surrounding test bench / firmware sees a deterministic error
on the bus rather than a simulator abort.

---

## 13. Debug & verification hooks

### 13.1 Back-door inspection

```cpp
uint32_t dbg_priority   (unsigned src)              const;
bool     dbg_pending    (unsigned src)              const;
bool     dbg_enable     (unsigned ctx, unsigned s)  const;
uint32_t dbg_threshold  (unsigned ctx)              const;
uint32_t dbg_claim_top  (unsigned ctx)              const; // peeks
void     dump_state     (std::ostream& = std::cout) const;
```

- All `dbg_*` accessors are `const`; they read internal state directly
  and bypass the TLM socket.
- `dbg_claim_top` returns the prospective claim without performing
  it — useful for `EXPECT_EQ` assertions in the test bench without
  perturbing model state.
- `dump_state` writes a human-readable summary to the supplied stream;
  by default `std::cout`. Used for waveform-correlated debug.

### 13.2 `transport_dbg`

The TLM standard `transport_dbg` is registered. It is symmetrical with
`b_transport` for reads/writes of the priority, enable, threshold, and
pending registers, but **does not claim** when reading
claim/complete — instead it returns `best_pending(ctx)`. This lets a
test bench use the standard TLM debug API to inspect PLIC state without
side effects.

### 13.3 What `03_SMC_Test_Plan.md` §3 requires

| Test plan requirement                 | Implementation hook                      |
|---------------------------------------|------------------------------------------|
| Inspect any register without claiming | `dbg_*` + `transport_dbg`                |
| Force a pending bit                   | Drive `src_in` from the test bench       |
| Observe context output transitions    | Bind `ctx_out` to `sc_signal` in the TB  |
| Cover claim/complete races            | TB drives `b_transport` while raising `src_in` |
| Cover threshold edge cases            | TB writes threshold via `b_transport`, observes `dbg_claim_top` |
| Coverage of `axi_filter` denial       | Reach the PLIC through filter, not directly |

### 13.4 Self-checking test bench

`plic_systemc/test/plic_tb.cpp` covers 14 scenarios:

1. Reset clears all state.
2. Priority R/W with 3-bit truncation.
3. Source 0 reserved (RAZ/WI).
4. Pending latched on rising edge.
5. Disabled source does not drive `ctx_out`.
6. Per-context enable independence.
7. Threshold gating (priority ≤ threshold).
8. Best-pending arbitration (priority order + low-id tie-break).
9. Claim/complete with line still high — re-arms.
10. Complete after line de-asserted — does not re-arm.
11. Misaligned / oversize / out-of-window TLM error responses.
12. `transport_dbg` claim has no side effects.
13. Cross-context isolation (enable + threshold independence).
14. Reset returns to clean state from a non-trivial state.

All 14 PASS on the reference build (SystemC 3.0.2 / Apple Clang 17).

---

## 14. Integration with `smc_top` and the CPU cluster

### 14.1 Address binding

The SMC fabric (§1 of `02_..._LowLevel_Design.md`) routes
`BASE+0x0400_0000 .. BASE+0x043F_FFFF` to the PLIC via its `to_plic`
initiator socket:

```cpp
fabric.to_plic.bind(ic.reg_socket);
```

When access control is enabled, an `axi_filter` instance is inserted
between the fabric and the PLIC:

```cpp
fabric.to_plic.bind(plic_filter.upstream);
plic_filter.downstream.bind(ic.reg_socket);
```

This pattern matches every protected target in the SMC.

### 14.2 Reset distribution

```cpp
ic.rst_n_i(rstu.rst_core_smc_n_o);
```

Per `01_SMC_Architecture.md` §3, `Core Reset` resets the CPU cluster,
PLIC, CLINT, WDT, BEU. The PLIC therefore inherits the reset path of
`rst_core_smc_n_o`, not the cold/cool primary resets.

### 14.3 Source aggregation (excerpt)

```cpp
// Source IDs are spec-defined; see fw/smc/common/drivers/riscv_plic0.h.
ic.src_in[ MBOX0_OUT_IRQ - 1 ](mb.outbound_irq_o[0]);
ic.src_in[ MBOX0_IN_IRQ  - 1 ](mb.inbound_irq_o [0]);
ic.src_in[ UART0_IRQ     - 1 ](uart[0].irq_o);
ic.src_in[ I2C0_IRQ      - 1 ](i2c [0].irq_fmt_threshold_o);
ic.src_in[ AVS_IRQ       - 1 ](avs.irq_o);
ic.src_in[ PVT_TEMP_IRQ  - 1 ](pvt.temp_irq_o);
ic.src_in[ DMA_IRQ       - 1 ](dma.irq_o);
ic.src_in[ LOG0_IRQ      - 1 ](log[0].irq_o);
ic.src_in[ TELEMETRY_IRQ - 1 ](trx[0].irq_o);
// ...repeated for every IRQ ID in the spec...
```

Tying unused source ports low is necessary to avoid `sc_signal::write`
warnings during elaboration:

```cpp
sc_signal<bool> tie_low("plic_src_tie_low");
tie_low.write(false);
for (unsigned i = 0; i < cfg.num_sources; ++i)
    if (!ic.src_in[i].get_interface())
        ic.src_in[i].bind(tie_low);
```

### 14.4 Context outputs to the CPU cluster

The convention used everywhere in the SMC docs:

```cpp
for (unsigned h = 0; h < NCORES; ++h) {
    ic.ctx_out[2*h + 0](cpu.meip_in[h]);   // M-mode external
    ic.ctx_out[2*h + 1](cpu.seip_in[h]);   // S-mode external
}
```

For the 1-core SMC variant `NCORES=1` and only `ctx_out[0..1]` are
used. For the 4-core variant all 8 outputs are wired.

### 14.5 Spike interaction

When `smc_cpu_cluster` runs Spike (Appendix A of
`02_..._LowLevel_Design.md`), Spike's built-in `plic_t` must be
disabled (Appendix A.7.2). Loads/stores from Spike to the PLIC window
fall through `simif_t::mmio_load/store` → bus bridge → `reg_socket`,
exactly like any other AXI-Lite access. The IRQ aggregator
(Appendix A.6) latches the model's `ctx_out[2h+m]` lines into Spike's
`mip` CSR via `processor_t::set_irqs()`. Round trip:

```
Spike load  0xC400_0000+offset
       │
       ▼
 simif_t::mmio_load
       │
       ▼
 iss_bus_bridge::xfer
       │
       ▼
 cpu_cluster.mmio_socket → fabric → axi_filter → plic.reg_socket → b_transport
                                                                           │
                                                                           ▼ schedule_recompute()
                                                                  output_method writes ctx_out
                                                                           │
                                                                           ▼
                                                                  cpu_cluster.meip_in[h] (sc_signal)
                                                                           │
                                                                           ▼
                                                                  irq_aggregator (SC_METHOD)
                                                                           │
                                                                           ▼
                                                                  hart.set_irqs() → Spike mip CSR
```

This is the contract that lets the SMC firmware boot, take an
interrupt, claim it via PLIC, service it, and write complete — all
while running on Spike, without any Spike-internal PLIC.

---

## 15. Verification & coverage plan

### 15.1 Test taxonomy

| Tier              | Goal                                       | Where                                |
|-------------------|--------------------------------------------|--------------------------------------|
| Unit              | Per-register R/W, per-flow correctness     | `plic_tb.cpp` (this document)        |
| Integration       | Fabric → filter → PLIC → CPU cluster       | `smc_top` integration test (future)  |
| System            | Firmware running real PLIC driver under Spike | `smc_top` boot test (future)      |
| Co-simulation     | Spike vs Rocket commit-log diff with PLIC events | future, see App. A.12             |

### 15.2 Coverage hooks

Coverage instrumentation points (left as `// TODO: cover_xx` markers in
follow-up commits):

- Per-register R/W coverage on every `reg_read`/`reg_write` exit.
- Per-source coverage in `src_method` (rising edge observed).
- Per-context coverage in `output_method` (rising/falling).
- Cross coverage of `(priority × threshold)` to ensure every
  (priority, threshold) pair is exercised at least once.
- Cross coverage of `(claim_in_flight × line_high_at_complete)` to
  cover both re-arming paths.

### 15.3 Negative tests

| Negative scenario              | Expected                          | Covered                |
|--------------------------------|-----------------------------------|------------------------|
| Misaligned access              | `TLM_BURST_ERROR_RESPONSE`        | `plic_tb.cpp` test 11  |
| Oversize access                | `TLM_BURST_ERROR_RESPONSE`        | test 11                |
| Out-of-window access           | `TLM_ADDRESS_ERROR_RESPONSE`      | test 11                |
| Source 0 priority/enable write | RAZ/WI                            | tests 3, 6             |
| Spurious EOI write             | Silent drop                       | (covered by `complete` precondition) |

---

## 16. Modeling decisions & trade-offs

### 16.1 Why not enforce `smc_axi_extension` in the PLIC?

`02_..._LowLevel_Design.md` §2 places filtering responsibility in
`axi_filter`. Enforcing it again inside the PLIC would:

- Duplicate state (filter rules and PLIC rules diverge over time).
- Hide test failures (tests targeting `axi_filter` would pass even if
  the filter were broken).
- Couple two modules that the spec explicitly keeps separate.

The PLIC therefore *honours* the extension (looks it up so it can be
extended later without an ABI break) but does not act on it.

### 16.2 Why a linear scan instead of a heap?

For `N ≤ 1024` sources, the linear scan has lower constants than any
heap-based alternative, vectorises trivially, has no allocator
interactions, and cannot get out of sync with `pending_` /
`priority_` / `enable_`. The heap approach in the original design
proposal was an optimisation guess; profiling on representative SMC
firmware would consistently show that arbitration is *not* the
bottleneck.

### 16.3 Why `vector<bool>` for `pending_`?

Bit-packed; cheap memcpy on reset; trivially serialisable for VCD
back-ends. The slight cost of bit-shifting per access is far below the
arithmetic of `best_pending()` and is dominated by the cost of
iterating sources at all.

### 16.4 Why a dedicated `recompute_event_` instead of `notify(SC_ZERO_TIME)` on `ctx_out`?

Because writing to `sc_signal` from multiple processes is forbidden in
SystemC 3.0. `recompute_event_` introduces a single synchronous owner
(`output_method`) without changing any external observable behaviour.

### 16.5 Why no DMI?

Claim has read side effects; DMI would let the CPU bypass them. We
explicitly clear `dmi_allowed` on every transaction and never register
`get_direct_mem_ptr`.

### 16.6 Why is `complete()` context-agnostic?

The RISC-V PLIC spec defines complete as "write the source ID back to
*any* context's claim/complete register". The in-flight latch is
per-source, not per-context. Cross-context completes are unusual but
legal (and have been observed in shared-driver flows). We keep the
spec-mandated behaviour and silently ignore the `ctx` argument.

### 16.7 Why `SC_METHOD(reset_proc)` on `value_changed_event` rather than `posedge`?

`sc_in<bool>` doesn't expose a "negedge" event the way `sc_in<sc_logic>`
might. The simplest portable approach is to be sensitive to value
changes and gate the body on `read() == false`. This also handles
glitches harmlessly — we only act on the assertion edge.

---

## 17. Known limitations & future work

| Item                                              | Status / planned action                                  |
|---------------------------------------------------|----------------------------------------------------------|
| Approximately-timed (AT) protocol                 | Out of scope; replace `b_transport` with `nb_transport`; preserve `output_method` topology. |
| Outstanding-transaction fabric back-pressure      | Not modelled (LT). Future AT extension. |
| Per-source latency profile (e.g., synchronisation across `clk_smc_i` ↔ `clk_periph_i`) | Not modelled; absorbed into the LT zero-time abstraction. |
| Coverage instrumentation                          | Stubs; full SVA/coverage hooks pending integration with the project's coverage harness. |
| VCD waveform export                               | Currently per-test-bench only; intended to be lifted into a project-wide tracing harness. |
| HW-realistic edge-vs-level mode                   | Not modelled — every SMC source is level. If a future SMC variant adds an edge-only source, add a `gateway_mode_t` enum and switch in `src_method`. |
| RDL-driven register code generation               | Currently hand-written; the project plan (§6.4 of `01_..._Architecture.md`) expects every register set to be JSON-/RDL-generated. Migrating the PLIC to that flow is a mechanical refactor. |
| Multi-PLIC deployments (per-cluster PLIC)         | The model is parameterised; instantiate two `plic` objects with disjoint `src_in` / `ctx_out` ranges if needed. |

---

## 18. Code walkthrough — function reference

### Constructor — `plic::plic(name, cfg)`

1. Validates `cfg_.num_sources` ∈ [1, 1023] and `cfg_.num_contexts ≥ 1`.
2. Sizes every internal vector to N+1 / W / num_contexts.
3. Registers `b_transport` and `transport_dbg` on `reg_socket`.
4. Creates three `SC_METHOD` processes:
   - `src_method`     — sensitive to every `src_in[i]`.
   - `reset_proc`     — sensitive to `rst_n_i`.
   - `output_method`  — sensitive to `recompute_event_`.
5. All processes use `dont_initialize()` so the model starts in a
   clean idle state before the first explicit reset.

### `b_transport`

See §5.2.

### `transport_dbg`

See §5.3.

### `reg_read(off, &data)`

Address-space dispatch (`if/else if/else`). Returns `true` for any
in-window access (data = 0 for reserved cells). Returns `false` only
for out-of-window accesses.

### `reg_write(off, data)`

Symmetric to `reg_read`. Side effects:

- Writing `priority[src]` truncates to 3 bits and schedules recompute.
- Writing `pending[*]` is silently dropped (RO).
- Writing `enable[ctx][word]` masks bit 0 of word 0 and bits beyond
  `num_sources` in the last word, then schedules recompute.
- Writing `threshold[ctx]` truncates to 3 bits and schedules recompute.
- Writing `claim/complete[ctx]` invokes `complete(ctx, data)`.

### `claim(ctx)` / `complete(ctx, src)` / `best_pending(ctx)`

See §8.2 and §8.3.

### `src_method` / `output_method` / `reset_proc`

See §8.1, §8.4, §10.

### `dbg_*` / `dump_state`

See §13.1.

---

## 19. Build, packaging and dependency notes

### 19.1 Source layout

```
plic_systemc/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── smc_tlm_extensions.h   (~70 LOC)
│   └── plic.h                 (~150 LOC)
├── src/
│   └── plic.cpp               (~350 LOC)
└── test/
    ├── CMakeLists.txt
    └── plic_tb.cpp            (~400 LOC)
```

Total payload: ~970 lines of C++17 incl. tests, ~520 lines excl. tests.

### 19.2 Dependencies

| Dependency                | Why                          | Version       |
|---------------------------|------------------------------|---------------|
| Accellera SystemC         | Core simulator, TLM-2.0      | ≥ 2.3.4 (3.0.2 verified) |
| C++17 compiler            | Structured bindings, `if constexpr` | Apple Clang 17 / GCC 9+ / Clang 10+ |
| CMake                     | Build system                 | ≥ 3.16        |
| Optional: GoogleTest      | (Future) replacement for the hand-rolled test bench | ≥ 1.10 |

No dependency on Spike or any ISS — the PLIC is purely a target IP and
can be unit-tested without any CPU model.

### 19.3 Linkage

```
libsmc_plic.a   ←  src/plic.cpp + include/*.h
plic_tb         ←  test/plic_tb.cpp + libsmc_plic.a + libsystemc.a
```

When the full SMC IP library is assembled (`libsmc.a` per
`02_..._LowLevel_Design.md` "Build & packaging"), the PLIC sub-library
is folded in unchanged.

### 19.4 Compiler flags

Reference flags used in CI:

```
-std=c++17 -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -O2
```

`-Wno-deprecated-declarations` suppresses the noise from
SystemC 3.0's deprecation warnings on a few legacy Accellera APIs that
the project still uses elsewhere.

### 19.5 Reproducible test command

```bash
SYSTEMC_HOME=/opt/homebrew/opt/systemc \
  cmake -S plic_systemc -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Expected output: `100% tests passed, 0 tests failed out of 1`.

---

## 20. Glossary

- **PLIC** — Platform-Level Interrupt Controller (RISC-V).
- **Context** — A {hart, privilege} pair that the PLIC can target. The
  SMC has 8 contexts: `{core 0..3} × {M-mode, S-mode}`.
- **Claim** — CPU-side read of `claim/complete` register; returns the
  highest-priority pending source ID and clears its pending bit.
- **Complete** — CPU-side write of source ID to `claim/complete`;
  signals end-of-handler so the source can re-pend.
- **In-flight** — A source between claim and complete. New rising
  edges on its line are ignored until complete; if the line is still
  high at complete, pending is re-armed.
- **Threshold** — Per-context priority cut-off; sources with `priority
  ≤ threshold` are masked.
- **Gateway** — Per-source spec-level abstraction that converts the
  raw interrupt source into IP/EOI messages. In this model the gateway
  is folded into `src_method` + `claim_in_flight_`.
- **LT (Loosely-Timed)** — TLM-2.0 modeling style: `b_transport`,
  optional DMI, coarse `sc_time` annotations.
- **AT (Approximately-Timed)** — TLM-2.0 modeling style: `nb_transport`
  with `BEGIN_REQ/END_REQ/BEGIN_RESP/END_RESP` phases.
- **DMI** — Direct Memory Interface; lets initiators bypass
  `b_transport` for memory-like targets. Refused by the PLIC because
  claim has side effects.
- **smc_axi_extension** — Shared TLM GP extension carrying
  `source_id`, `prot`, `cacheable`, `non_secure`, `axi_id`,
  `axi_user`. Defined in `01_..._Architecture.md` §6.1.
- **RDL** — SystemRDL register description language; the
  authoritative specification of the PLIC register layout lives in
  `hw/smc/smc_cpu/data/registers/rdl/plic.rdl`.

---

*End of document.*
