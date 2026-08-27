# AOU_CORE — SystemC / TLM-2.0 Loosely-Timed Model

Architecture, CSRs, and programming sequences are in the hardware TRM
(`aou-core.rdl`). This tree has the model, its test plan, and how to
run the tests.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — sockets, CCI, `smc-vp` / `smu-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

AXI-over-UCIe bridge model for firmware bring-up and VP integration.

## Layout

```
aou/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── deps.env.example
├── doc/
│   ├── index.adoc
│   ├── implementation.adoc
│   └── test_plan.adoc
├── include/aou_core.h
├── src/aou_core.cpp
└── test/aou_core_tb.cpp
```

## Quick start

```bash
cp deps.env.example deps.env   # set SYSTEMC_HOME / CCI_HOME
./run_tests.sh
./run_tests.sh --coverage
./run_tests.sh --asan
```

`./run_tests.sh --asan` links `-fsanitize=address`, which requires a 64-bit
`libasan` for the active compiler. On RHEL8 the `gcc-toolset-*` devtoolsets
only package a 32-bit `libasan` (`.../lib/gcc/x86_64-redhat-linux/<ver>/32/`)
and the link fails with `cannot find -lasan`; this matches the same gap
`.github/workflows/ci.yml` works around by skipping the ASAN phase on RHEL8
(Ubuntu CI still runs it). Use an Ubuntu host/container, or a toolchain with
a 64-bit `libasan`, to exercise `--asan` locally.

Linked into `smc-vp` (`vp/platform/smc/`). Platform smoke test:
`sw/smc-vp-tests/smc-aou-test` (`SMC_AOU_BASE = 0xC000_C000`). Full commands
are in `doc/test_plan.adoc`.

## Abstraction

Loosely-timed: APB CSR map, activate/deactivate, AXI slave→peer master
forwarding. No FDI flit, credit, or QoS timing.
