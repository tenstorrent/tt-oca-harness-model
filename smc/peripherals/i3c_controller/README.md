# SMC I3C Controller — SystemC / TLM-2.0 Loosely-Timed Model

A transaction-level, register-accurate **OCA I3C Controller** modelled in
Accellera SystemC 3.0.2 + TLM-2.0 (Loosely-Timed) and parameterised
through SystemC CCI 1.0.2.  It models the multi-instance MIPI I3C Basic
v1.0/v1.1.1 + HCI v1.2 controller (`i3ccore_wrapper`, CHIPS-Alliance
i3c-core fork with OCA enhancements) instantiated in the System Management
Controller (SMC) peripheral sub-system: up to six independent I3C instances
behind a single AXI4-Lite slave, each with its own I3CCSR register block,
HCI command/response/TX/RX/IBI queues, device tables, and interrupt.

The model tracks the RTL instantiated in the SMC sub-system:

- `tt-oca-hw/hw/periph/i3ccore_wrap/rtl/i3ccore_wrapper.sv` — multi-instance
  top; AXI-Lite address demux (`INSTANCE_SPACING = 0x500`).
- `tt-oca-hw/hw/periph/i3ccore_wrap/rtl/i3c_wrapper.sv` — per-instance wrapper.
- `tt-oca-hw/hw/periph/i3ccore_wrap/rtl/i3c.sv` — main I3C module (HCI queues,
  controller/target FSMs, PHY).
- `tt-oca-hw/hw/periph/i3ccore_wrap/rtl/i3ccore_wrap_pkg.sv` —
  `MAX_NUM_I3CS = 6`, `I3C_INSTANCE_SPACING = 0x500`, `I3C_REG_ADDR_WIDTH = 11`.
- `tt-oca-hw/hw/periph/i3ccore_wrap/data/registers/rdl/oca_i3c_wrap.rdl` —
  `I3CCSR` register block.
- `tt-oca-hw/hw/periph/i3ccore_wrap/doc/{architecture,memmap,interface}.adoc` —
  block diagram, transaction flows, and the ground-truth register map.
- `tt-oca-hw/hw/smc/smc_peripherals/rtl/smc_peripherals.sv` —
  `u_i3ccore_wrapper` instantiation (`NUM_I3C = 6`, `BASE = 0xC000_5000`).
- `tt-oca-hw/hw/smc/smc_config_pkg.sv` — `NUM_I3C = 6`.
- `tt-oca-hw/hw/smc/data/registers/rdl/smc_top.rdl` —
  `oca_i3c_wrap_0 @ BASE_ADDR + 0x000_5000`, six instances at 0x500 spacing.

Architecture, CSRs, and programming are in the hardware TRM. Model and
test docs:

- `doc/index.adoc` — entry (includes the two pages below)
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

The model is a drop-in `SC_MODULE` that the rest of the SMC SystemC IP library
wires up exactly as for the Boot ROM, PLIC, CLINT, Scratchpad RAM, and Reset
Unit.

---

## Layout

```
i3c_controller/
├── CMakeLists.txt
├── README.md                       (this file)
├── run_tests.sh                    Build + run helper (Release / ASan / coverage / ctest)
├── include/
│   ├── smc_tlm_extensions.h        Shared GP extension (smc_axi_extension)
│   └── i3c_controller.h            SC_MODULE(i3c_controller) declaration + cci_param
├── src/
│   └── i3c_controller.cpp          Implementation
├── test/
│   ├── CMakeLists.txt
│   ├── i3c_controller_tb.cpp       Primary self-checking bench
│   └── i3c_controller_neg_tb.cpp   Negative-path / edge-case bench
└── doc/
    ├── index.adoc
    ├── implementation.adoc
    └── test_plan.adoc
```

---

## Module interface

```cpp
SC_MODULE(i3c_controller) {
    tlm_utils::simple_target_socket<i3c_controller> reg_socket; // AXI-Lite, 32-bit, multi-instance

    // Per-instance outputs (sized from num_instances)
    sc_vector<sc_out<bool>> irq_o;
    sc_vector<sc_out<bool>> scl_o, sda_o, scl_oe_o, sda_oe_o, sel_od_pp_o;
    sc_vector<sc_out<bool>> recovery_payload_available_o, recovery_image_activated_o;

    explicit i3c_controller(sc_module_name, i3c_controller_cfg = i3c_controller_cfg{});

    // Test/integration API
    void set_bus_model(unsigned inst, bus_model_fn);             // attach a target emulator
    bool inject_ibi(unsigned inst, uint8_t addr,
                    const std::vector<uint8_t>& payload = {});   // raise an IBI from a target
    uint32_t dbg_read(uint64_t off) const;                       // back-door snapshot read
    void dump_state(unsigned inst, std::ostream& = std::cout) const;
};
```

`i3c_controller_cfg` defaults:

| Field             | Default | Note                                                       |
|-------------------|---------|------------------------------------------------------------|
| `num_instances`   | `6`     | I3C instances (1..6); sizes the per-instance output vectors. |
| `access_delay_ns` | `2.0`   | TLM `b_transport` annotated delay (AXI-Lite latency).       |
| `xfer_delay_ns`   | `100.0` | Modelled enqueue-to-response transaction latency.           |
| `cmd_fifo_depth`  | `8`     | Command/response queue depth (entries).                     |
| `rx_fifo_depth`   | `64`    | RX data FIFO depth (DWORDs).                                |
| `tx_fifo_depth`   | `64`    | TX data FIFO depth (DWORDs).                                |
| `ibi_fifo_depth`  | `8`     | IBI status/data queue depth (DWORDs).                       |

All are exposed as CCI parameters (`access_delay_ns` / `xfer_delay_ns`
mutable, the rest immutable).  Set them via the broker before constructing:

```cpp
broker.set_preset_cci_value("smc.i3c.num_instances", cci::cci_value(6u));
broker.set_preset_cci_value("smc.i3c.xfer_delay_ns", cci::cci_value(100.0));
```

See `doc/implementation.adoc` for the CCI catalogue and `smc-vp` bind.

---

## Behaviour highlights

- **Multi-instance decode**: the single `reg_socket` carries the whole
  `num_instances × 0x500` aperture; `instance = offset / 0x500`,
  `local_offset = offset % 0x500` — exactly the RTL `axi_lite_demux`.
- **Register block** (per instance, 11-bit window): the I3CCSR HCI v1.2 map —
  `HCI_VERSION` (0x120), `HC_CONTROL`, `RESET_CONTROL`, `PRESENT_STATE`,
  `INTR_*` / `PIO_INTR_*` (RW1C), `COMMAND/RESPONSE/XFER_DATA/IBI` FIFO ports,
  `QUEUE_THLD_CTRL`, `DATA_BUFFER_THLD_CTRL`, `QUEUE_SIZE`, `STBY_CR_*`, and the
  `DAT`/`DCT` direct-access windows.  `HC_CONTROL.MODE_SELECTOR` reads as 1
  (PIO mode).  `RESET_CONTROL` bits are self-clearing.
- **HCI transaction engine** (descriptor-level abstraction of the
  controller/PHY FSMs): firmware pushes a 64-bit command descriptor (two
  writes) + TX payload; with `HC_CONTROL.BUS_ENABLE = 1` the engine resolves
  the target dynamic address from `DAT[dev_index]`, calls the attached **bus
  model**, drains/fills the TX/RX FIFOs, and pushes a response descriptor,
  raising `PIO_INTR_STATUS.RESP_READY_STAT`.
- **Bus model hook**: `set_bus_model()` lets a test bench emulate the I3C bus
  and its targets (ACK/NACK, return read data, set error codes).  With no bus
  model attached every transaction NACKs (`ERROR_ADDRESS_NACK`).
- **In-Band Interrupts**: `inject_ibi()` enqueues an IBI status descriptor
  (+ optional payload) and raises `IBI_STATUS_THLD_STAT`; firmware reads them
  back through `IBI_PORT`.
- **Interrupts**: per-instance `irq_o[i]` is the OR of the enabled-and-signalled
  `INTR_STATUS` and `PIO_INTR_STATUS` bits.  Threshold bits are level-sensitive
  (FIFO occupancy vs `QUEUE_THLD_CTRL` / `DATA_BUFFER_THLD_CTRL`); error/abort
  bits are latched write-1-to-clear.

### Unmodelled RTL details (documented simplifications)

- **Bit-level SCL/SDA signalling & OD/PP timing** — `scl_o`/`sda_o`/`*_oe_o`
  are held idle (released); no waveform-accurate bus timing.  The functional
  data movement happens through the bus-model callback.
- **CDC / metastability** — the `i3c_phy` dual-FF synchronisers are not
  modelled (firmware-irrelevant, mirroring the reset_unit's CDC abstraction).
- **External DAT/DCT memory** — only the CSR-visible direct-access windows
  (`0x300`/`0x400`) are modelled; the full 128-entry tables exported on the
  RTL `dat_mem`/`dct_mem` ports are abstracted away.
- **HDR modes, recovery (TCRI) datapath, scan/DFT** — recovery outputs are
  held idle; HDR/CRC paths are not exercised.

---

## Build & test

```bash
./run_tests.sh                   # Release build + run both test benches
./run_tests.sh --ctest           # Run via ctest (both binaries)
./run_tests.sh --asan            # AddressSanitizer (+ LSan on Linux)
./run_tests.sh --coverage        # Source-level coverage report
./run_tests.sh --clean           # Wipe build/ first
```

This repository mandates **C++20** (the local SystemC / CCI installs are
built `-std=c++20`).  Point the helper at the C++20 toolchain:

```bash
SYSTEMC_HOME=/path/to/systemc-3.0.2-cxx20 \
CCI_HOME=/path/to/cci-cxx20 \
./run_tests.sh --clean
```

`SYSTEMC_HOME` and `CCI_HOME` are otherwise auto-probed for the common install
locations. Platform firmware: `cd sw/smc-vp-tests && ./run_smc_vp_tests.sh
smc-i3c-loopback-test`. Full commands are in `doc/test_plan.adoc`.

---

## Current status

| Metric                       | Value                          |
|------------------------------|--------------------------------|
| Test cases (across two TBs)  | All PASS (ctest: 2/2)          |
| AddressSanitizer             | 0 errors                       |
| C++ standard                 | C++20 (repo toolchain)         |

---

## Citation

Modelled after, and consistent with, the SMC Boot ROM, PLIC, CLINT, Scratchpad
RAM, and Reset Unit SystemC sub-packages in `smc/peripherals/`.  See those
READMEs and the matching specification / low-level-design documents for the
shared SMC IP conventions (CCI declaration order, single-driver discipline,
error taxonomy, build & packaging).
