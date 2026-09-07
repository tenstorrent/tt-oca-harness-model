# SEP Scratch Cold — SystemC / TLM-2.0 Loosely-Timed Model

Eight 64-bit cold-retention scratch words (`sep_scratch_cold_ip`).
`COLD_SCRATCH[1]` / `[2]` also feed the VP virt-console and SEP status
decoders (the ROM `STATUS_OUT()` / `simput*()` path).

Architecture and programming are in the hardware TRM.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model
- `doc/test_plan.adoc` — standalone cases

## Build and test

```bash
cd sep/peripherals/sep_scratch_cold
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--debug`, `--ctest`, `--clean`, `--cppcheck`.
