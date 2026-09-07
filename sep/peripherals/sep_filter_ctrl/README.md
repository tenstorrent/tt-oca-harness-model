# SEP Filter Control — SystemC / TLM-2.0 Loosely-Timed Model

AXI security filter table (`sep_filter_ctrl_ip`), BlockByDefault, and
`filter_skip_i`. One class, two `sep-vp` instances (outbound 32 entries,
inbound 16).

Architecture and programming are in the hardware TRM.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model
- `doc/test_plan.adoc` — standalone cases

## Build and test

```bash
cd sep/peripherals/sep_filter_ctrl
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--debug`, `--ctest`, `--clean`, `--cppcheck`.
