# SMC UART 16550 — SystemC / TLM-2.0 Loosely-Timed Model

An NS16550A-compatible UART modeled in Accellera SystemC 2.3.x + TLM-2.0
(Loosely-Timed). Architecture, CSRs, and programming are in the hardware
TRM. Model and test docs:

- `doc/index.adoc` — entry (includes the two pages below)
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

The model is a drop-in `SC_MODULE`. It is a **functional** model:
register-level behaviour and character-level data flow are reproduced
faithfully, while bit-level serial timing is abstracted away.

---

## Layout

```
uart/
├── CMakeLists.txt
├── README.md                       (this file)
├── run_tests.sh                    Build + run convenience script
├── deps.env.example                Template for local dependency paths
├── doc/
│   ├── index.adoc
│   ├── implementation.adoc
│   └── test_plan.adoc
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

The test bench prints `ALL TESTS PASSED` on success. There is no
`smc-uart-test` firmware directory; UART0 is the `printf` path for the
tests that are in `sw/smc-vp-tests/`. Full commands are in
`doc/test_plan.adoc`.

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

See `doc/implementation.adoc` for TLM/process details.
