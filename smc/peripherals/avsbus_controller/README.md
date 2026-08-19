# SMC AVSBus Controller — SystemC / TLM-2.0 Loosely-Timed Model

Register-accurate functional model of the SMC **AVSBus 1.3.1** controller
(`hw/ip/avsbus_controller`), mapped at `0xC000_4000`.

| Doc | Role |
|-----|------|
| `doc/01_AVSBUS_CONTROLLER_Specification.md` | Externally-observable behaviour |
| `doc/02_AVSBUS_CONTROLLER_LowLevel_Design.md` | TLM interface, register map, internals |
| `doc/03_AVSBUS_CONTROLLER_Test_Plan.md` | Unit verification strategy |
| `hw/ip/avsbus_controller/data/registers/rdl/avsbus_controller.rdl` | Ground-truth register map |

## Layout

```
avsbus_controller/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── deps.env.example
├── doc/
├── include/avsbus_controller.h
├── src/avsbus_controller.cpp
└── test/
    ├── avsbus_controller_tb.cpp
    └── avsbus_controller_neg_tb.cpp
```

## Quick start

```bash
cp deps.env.example deps.env   # set SYSTEMC_HOME / CCI_HOME
./run_tests.sh                 # build + run both benches
./run_tests.sh --ctest
./run_tests.sh --asan
./run_tests.sh --coverage
```

Produces `libsmc_avsbus_controller.a`. Linked into `smc-vp` via
`vp/platform/smc/CMakeLists.txt`. Platform smoke test:
`sw/smc-vp-tests/smc-avsbus-test` (`./run_smc_vp_tests.sh smc-avsbus-test`).

## Abstraction

Loosely-timed: `AVS_CMD` writes enqueue into an 8-deep command FIFO; after
`xfer_delay_ns` a slave response is pushed into an 8-deep readback FIFO.
Bit-serial AVS wires and clock dividers are not cycle-accurate.  Optional
`set_slave_model()` callback overrides the default happy-path responder.
