# AOU_CORE — SystemC / TLM-2.0 Loosely-Timed Model

AXI-over-UCIe bridge model for firmware bring-up and `smc-vp` integration.
CSR ground truth: `aou-rtl/csr/aou-core.rdl`.

| Doc | Role |
|------|------|
| [doc/01_AOU_Specification.md](doc/01_AOU_Specification.md) | Software-visible behaviour |
| [doc/02_AOU_LowLevel_Design.md](doc/02_AOU_LowLevel_Design.md) | TLM API, internals, VP wiring |
| [doc/03_AOU_Test_Plan.md](doc/03_AOU_Test_Plan.md) | Unit + platform tests |

## Layout

```
aou/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── deps.env.example
├── doc/
├── include/aou_core.h
├── src/aou_core.cpp
└── test/aou_core_tb.cpp
```

## Quick start

```bash
cp deps.env.example deps.env   # set SYSTEMC_HOME / CCI_HOME
./run_tests.sh
./run_tests.sh --coverage      # aou_core.cpp: 100% line coverage
./run_tests.sh --asan          # AddressSanitizer + UBSan, clean
```

`./run_tests.sh --asan` links `-fsanitize=address`, which requires a 64-bit
`libasan` for the active compiler. On RHEL8 the `gcc-toolset-*` devtoolsets
only package a 32-bit `libasan` (`.../lib/gcc/x86_64-redhat-linux/<ver>/32/`)
and the link fails with `cannot find -lasan`; this matches the same gap
`.github/workflows/ci.yml` works around by skipping the ASAN phase on RHEL8
(Ubuntu CI still runs it). Use an Ubuntu host/container, or a toolchain with
a 64-bit `libasan`, to exercise `--asan` locally.

Linked into `smc-vp` (`vp/platform/smc/`). Platform smoke test:
`sw/smc-vp-tests/smc-aou-test` (`SMC_AOU_BASE = 0xC000_E000`).

## Abstraction

Loosely-timed: APB CSR map, activate/deactivate, AXI slave→peer master
forwarding. No FDI flit, credit, or QoS timing.
