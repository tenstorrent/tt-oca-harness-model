# SEP Firmware Tests — Build and Run Guide

Firmware tests under `fw/sep/tests/` target the VeeR EL2 core on the SEP
subsystem. They run on `sep-vp`.

The host-agnostic runner at the `sw/sep-vp-tests/` root is the supported
entry point (there is no `bin/sep_fw_standalone.sh` in this tree):

```bash
cd sw/sep-vp-tests
./run_sep_vp_tests.sh --build-vp          # rebuild sep-vp, then run all
./run_sep_vp_tests.sh sep-hmac-test       # one test by name
```

See [`sw/sep-vp-tests/README.md`](../../../README.md) for toolchain and
`sep-vp` detection.

**VP memory map (from `vp/platform/sep/inc/Args.hpp`):**

| Region | Address | Size |
|---|---|---|
| ITCM | `0xC0000000` | 256 KB |
| DTCM | `0xC0040000` | 128 KB |
| SRAM | `0x10000000` | 256 KB |

---

## Build and run from `fw/sep/tests`

After dependencies are set up (the runner does this):

```bash
cd sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests
./run_all_tests.sh              # build + run all tests
./run_all_tests.sh --clean      # clean + build + run all
./run_test.sh <test_name>       # build + run one test
```

Or drive `sep-vp` directly:

```bash
vp/build_sep/bin/sep-vp \
    vp/platform/sep/config/accellera_config.ini \
    sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests/<test_name>/<test_name>.elf
```

---

## VP Limitations

Tests that depend on unmodeled RTL behaviour can fail even when the helper
runs. The SEP outbound filter, some PIC sources, and other gaps are noted
in `sep/doc/implementation.adoc`.
