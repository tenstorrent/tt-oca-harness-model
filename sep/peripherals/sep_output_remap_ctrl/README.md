# SEP Output Remap Control — SystemC / TLM-2.0 Loosely-Timed Model

REGION_ATTRS output remap (`sep_output_remap_ctrl_ip`). One class, two
`sep-vp` instances (AP and STEE windows).

Architecture and programming are in the hardware TRM.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model
- `doc/test_plan.adoc` — standalone cases

## Build and test

```bash
cd sep/peripherals/sep_output_remap_ctrl
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--debug`, `--ctest`, `--clean`, `--cppcheck`.
