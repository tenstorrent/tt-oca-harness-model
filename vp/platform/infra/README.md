# VP platform infrastructure

Header-only helpers shared by `sep-vp`, `smc-vp`, and `smu-vp`
(bus / SimpleBus, TLM map, ELF loader plumbing). The CMake target is
`platform-infra` (`INTERFACE`, SystemC 3.0.2).

There is no standalone test binary here. Exercise it through the
platform builds and `vp/platform/smu/run_tests.sh` (SMU interconnect).
