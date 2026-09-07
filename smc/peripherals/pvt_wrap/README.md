# SMC PVT Wrapper — SystemC / TLM-2.0 Loosely-Timed Model

Process / voltage / temperature CSRs (`smc::pvt_wrap`). Architecture and
programming are in the hardware TRM.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model and `smc-vp` bind
- `doc/test_plan.adoc` — standalone cases and firmware tests

## Build and test

```bash
cd smc/peripherals/pvt_wrap
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--ctest` and `--clean`. Platform firmware:
`cd sw/smc-vp-tests && ./run_smc_vp_tests.sh smc-pvt-wrap-test`.
