# SEP Scratch Warm — SystemC / TLM-2.0 Loosely-Timed Model

Eight 64-bit scratch words cleared on reset (`sep_scratch_warm_ip`).
Store-only stub: no extra behavioral logic beyond the register file.

Architecture and programming are in the hardware TRM.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model
- `doc/test_plan.adoc` — standalone cases

## Build and test

```bash
cd sep/peripherals/sep_scratch_warm
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--debug`, `--ctest`, `--clean`, `--cppcheck`.
