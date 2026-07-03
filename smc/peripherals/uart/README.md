# SMC UART 16550 — SystemC / TLM-2.0 Loosely-Timed Model

An NS16550A-compatible UART modeled in Accellera SystemC 2.3.x + TLM-2.0
(Loosely-Timed). Implements the SMC UART IP described in:

- `doc/01_UART_Specification.md` — externally-observable behaviour
- `doc/02_UART_LowLevel_Design.md` — TLM interface, register map, internals
- `doc/03_UART_Test_Plan.md` — verification strategy and test list
- `doc/04_UART_Function_Flow.md` — function call graph and data-flow diagrams
- `hw/comp/uart_16550/data/registers/rdl/uart_16550_*.rdl` — authoritative register maps
- `hw/comp/uart_16550/rtl/uart_16550.sv` (+ `uart_core/uart_tx/uart_rx.sv`)
  — functional reference RTL

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP
library wires up exactly as specified in the low-level design. It is a
**functional** model: register-level behaviour and character-level data flow
are reproduced faithfully (and are bit-compatible with the RDL register
layout), while bit-level serial timing is abstracted away.

---

## Layout

```
uart/
├── CMakeLists.txt
├── README.md                       (this file)
├── run_tests.sh                    Build + run convenience script
├── deps.env.example                Template for local dependency paths
├── doc/
│   ├── 01_UART_Specification.md
│   ├── 02_UART_LowLevel_Design.md
│   ├── 03_UART_Test_Plan.md
│   └── 04_UART_Function_Flow.md
│   └── figures/01_block_diagram.svg
├── include/
│   ├── smc_tlm_extensions.h        Shared GP extension (smc_axi_extension)
│   └── uart.h                      SC_MODULE(uart) declaration
├── src/
│   └── uart.cpp                    Implementation
└── test/
    ├── CMakeLists.txt
    └── uart_tb.cpp                 Self-checking test bench
```

---

## Building and testing

First-time setup (point the build at your SystemC + CCI installs):

```bash
cp deps.env.example deps.env
# edit SYSTEMC_HOME and CCI_HOME in deps.env
./run_tests.sh
```

`run_tests.sh` options:

| Flag | Effect |
|------|--------|
| (none) | Incremental Release build + run |
| `--clean` | Wipe `build/` first |
| `--ctest` | Run via `ctest` |
| `--asan` | Build with AddressSanitizer (Linux: + LeakSanitizer) |
| `--coverage` | Build with coverage; print a line report |

The test bench prints `ALL TESTS PASSED` on success.

---

## Configuration (CCI)

| CCI parameter | Type | Default | Notes |
|---------------|------|---------|-------|
| `tx_fifo_depth` | `cci_param<unsigned>` (immutable) | 32 | TX FIFO depth (1..4096) |
| `rx_fifo_depth` | `cci_param<unsigned>` (immutable) | 32 | RX FIFO depth (1..4096) |
| `access_delay_ns` | `cci_param<double>` (mutable) | 2.0 | Annotated TLM access latency |

Set presets via the broker before construction:

```cpp
broker.set_preset_cci_value("top.uart0.rx_fifo_depth", cci::cci_value(64u));
```

---

## Ports

| Port | Dir | Description |
|------|-----|-------------|
| `reg_socket` | target | TLM-2.0 LT register socket (AXI4-Lite-style) |
| `rst_n_i` | in | Active-low asynchronous reset |
| `tx_o` / `rx_i` | out / in | Serial lines (idle high) |
| `cts_ni` `dsr_ni` `ri_ni` `dcd_ni` | in | Modem inputs (active-low) |
| `rts_no` `dtr_no` `out1_no` `out2_no` | out | Modem outputs (active-low) |
| `rxrdy_o` `txrdy_o` | out | DMA handshake |
| `err_o` | out | Aggregate RX error flag |
| `irq_o` | out | Interrupt output (active-high) |

Port names follow the `uart_16550.sv` component top level; the `uart_wrap`
platform exposes them per-instance with a `uart_` prefix.

---

## Test-bench back door

The model provides side-effect-aware hooks for verification (no serial line
needed):

- `inject_rx_char(ch, perr, ferr, berr)` — deliver an RX character
- `dbg_tx_pop(ch)` — pop a transmitted character
- `dbg_tx_count()` / `dbg_rx_count()` — FIFO occupancy
- `dbg_reg(off)` — side-effect-free register peek
- `dump_state(os)` — human-readable state dump

See `doc/02_UART_LowLevel_Design.md` §12 for details.
