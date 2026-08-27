# SMC Fabric — SystemC/TLM-2.0 Loosely-Timed Model

Architecture, CSRs, and programming sequences are in the hardware TRM.
This tree has the model, its test plan, and how to run the tests.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — sockets, routing, constructor knobs, `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

## Layout

```
smc_fabric/
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── include/smc_fabric.h
├── src/smc_fabric.cpp
├── test/smc_fabric_tb.cpp
└── doc/
    ├── index.adoc
    ├── implementation.adoc
    └── test_plan.adoc
```

## Quick start

```bash
cd smc/smc_fabric
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage
```

`run_tests.sh` probes `SYSTEMC_HOME`. The linked SystemC library must be
built with **C++20**. Also `--ctest` and `--clean`. Full commands are in
`doc/test_plan.adoc`.

Produces `libsmc_fabric.a`, linked into `smc-vp` via
`vp/platform/smc/CMakeLists.txt`.

## Model notes

- Bucket D pure target: no quantum keeper; CSR accesses add `reg_access_ns`.
- Internal masters: alias remap, then a fixed 16 MB local/outbound split.
- `sys_axi_in` is inbound-filter then local; `sep_axi_in` is local only.
- Outgoing transactions carry `smc::smc_axi_extension`.
- DMI is never granted. Reset clears programmed remap and filter tables.
