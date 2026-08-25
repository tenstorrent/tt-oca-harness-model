# SEP Memory

SystemC TLM-2.0 loosely-timed sparse store used for SEP SRAM, ROM,
ITCM, and DTCM. Apertures are in the hardware TRM. This tree has the
model and how to build it. Sources live under `model/`, not `include/`.

## Status

| Item | State |
|---|---|
| TLM `b_transport` / DMI / `transport_dbg` | Implemented |
| ROM ignore-write on `b_transport` | Implemented |
| ECC / BIST | Not modelled |
| Standalone tests | None; `./run_tests.sh` is build-only |
| Wired into `sep-vp` | Four instances + `spi_mux` stub |

## Files

```
model/inc/sep_memory.h         SEPMemory
model/src/sep_memory.cpp       TLM + load_if
doc/implementation.adoc        SystemC/TLM model
doc/test_plan.adoc             how to build + platform tests
```

Storage is `PagedMemory` from `sep/utils/paged-memory/paged_mem.h`.

## Building and testing

```bash
./run_tests.sh              # Release build of the library
./run_tests.sh --debug
./run_tests.sh --clean
```

There is no unit-test binary. Firmware coverage is on `sep-vp`
(`rom_sanity_test`, `sram_perf_test`, and every image loaded into
ROM/ITCM). Commands are in `doc/test_plan.adoc`.
