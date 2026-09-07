# Local Master Alias Remap Control — SystemC / TLM-2.0 Loosely-Timed Model

16-region START/END/ATTRS alias remap (`local_alias_remap_ip`), 4 KiB
granularity. Architecture and programming are in the hardware TRM.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model
- `doc/test_plan.adoc` — standalone cases

## Build and test

```bash
cd sep/peripherals/local_master_alias_remap_ctrl
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--debug`, `--ctest`, `--clean`, `--cppcheck`.
