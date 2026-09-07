# SEP CPU Control — SystemC / TLM-2.0 Loosely-Timed Model

SEP system CSRs, NMI vector, inbound window, and timeouts
(`sep_cpu_ctrl_ip`). Alias remap, output remap, and filter CSRs are
separate peripherals.

Architecture and programming are in the hardware TRM.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model
- `doc/test_plan.adoc` — standalone cases

## Build and test

```bash
cd sep/peripherals/sep_cpu_ctrl
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--debug`, `--ctest`, `--clean`, `--cppcheck`.
