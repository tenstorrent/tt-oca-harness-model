# SMC CLINT — SystemC / TLM-2.0 Loosely-Timed Model

RISC-V CLINT (`smc::clint`): `mtime`, `mtimecmp`, `msip`. Architecture
and programming are in the hardware TRM.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases

## Build and test

```bash
cd smc/peripherals/clint
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--ctest` and `--clean`. `SYSTEMC_HOME` / `CCI_HOME` are auto-probed;
overrides go in `deps.env`.
