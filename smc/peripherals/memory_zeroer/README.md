# SMC memory_zeroer — SystemC / TLM-2.0 Loosely-Timed Model

Functional LT model of the SMC AXI memory-zeroer control block. Software
programs a destination address and byte count, then writes `CTRL_STATUS` to
start a blocking (LT) burst of zero-fills over an initiator DMA socket.

Behaviour matches the Virtualizer reference at
`systemC_models_for_reference/AXI_zeroer`, rewritten with the SMC house style
(`regmodel`, CCI, `sim_log`, self-checking TB).

Authoritative sources:

- `axi_zeroer_ctrl.rdl` — register map (`DEST_ADDR` / `SIZE` / `CTRL_STATUS`)
- `systemC_models_for_reference/AXI_zeroer` — Virtualizer functional model
- `smc/doc/systemc_tlm2_integration_guide.adoc` — window `0xC003_8200`, size
  `0x18`, internal interrupt 3

---

## Layout

```
memory_zeroer/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── deps.env.example
├── include/
│   ├── memory_zeroer.h
│   └── smc_tlm_extensions.h
├── src/
│   └── memory_zeroer.cpp
└── test/
    ├── CMakeLists.txt
    └── memory_zeroer_tb.cpp
```

---

## Register map

| Offset | Name        | Access | Description |
|--------|-------------|--------|-------------|
| 0x00   | DEST_ADDR   | RW     | Byte address to write zeros to |
| 0x08   | SIZE        | RW     | Size in bytes |
| 0x10   | CTRL_STATUS | RW/RO  | `int_en[0]` (RW); `status[32]` busy (RO). Any write starts a job when `SIZE != 0`. |

---

## Ports

| Port | Direction | Role |
|------|-----------|------|
| `reg_socket` | target | 64-bit MMIO register port |
| `dma_socket` | initiator | Zero-fill writes into system memory |
| `rst_n_i` | in | Active-low reset |
| `irq_o` | out | Active-high completion IRQ (gated by `int_en`) |

---

## CCI parameters

| Name | Mutable | Default | Description |
|------|---------|---------|-------------|
| `chunk_size` | no | 4096 | DMA write chunk size (bytes) |
| `access_delay_ns` | yes | 2.0 | Annotated register-access delay |

---

## Building and testing

```bash
cp deps.env.example deps.env
# edit SYSTEMC_HOME and CCI_HOME
./run_tests.sh
```

Or with the shared SMC env:

```bash
source ../../deps.env   # or smc/deps.env
./run_tests.sh
```

Expected:

```
==== SMC memory_zeroer TB ====
  [PASS] ...
ALL TESTS PASSED
```
