# EL2 PIC — SystemC / TLM-2.0 Loosely-Timed Model

MMIO PIC model (`el2_pic_model`) for the SEP VeeR EL2. RISC-V PIC CSRs
(`meivt`, `meipt`, `meihap`, …) live in the ISS wrapper, not here.

Architecture and programming are in the hardware TRM and the VeeR EL2 spec.

- `doc/index.adoc` — landing page
- `doc/implementation.adoc` — SystemC/TLM model
- `doc/test_plan.adoc` — standalone cases

## Build and test

```bash
cd sep/peripherals/el2_pic
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage   # ≥ 95% line; do not combine with --asan
```

Also `--debug`, `--ctest`, `--clean`, `--cppcheck`. `SYSTEMC_HOME` / `CCI_HOME`
come from `setup_build_env.sh` (same as the other SEP IPs).
