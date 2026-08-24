# SMC I2C Controller — SystemC / TLM-2.0 LT Model

Loosely-timed SystemC model of one OCA I2C core (`smc::i2c_controller`).
Architecture, CSRs, and programming are in the hardware TRM. Model and
test docs:

- `doc/index.adoc` — entry (includes the two pages below)
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

## Build & test

```bash
./run_tests.sh              # Release build + run both benches
./run_tests.sh --ctest
./run_tests.sh --asan
./run_tests.sh --coverage   # do not combine with --asan
./run_tests.sh --clean
```

`run_tests.sh` probes `SYSTEMC_HOME` / `CCI_HOME`. Platform firmware:
`cd sw/smc-vp-tests && ./run_smc_vp_tests.sh smc-i2c-loopback-test`.
Full commands are in `doc/test_plan.adoc`.
