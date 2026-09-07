# SEP virtual platform (`sep-vp`)

Platform wiring for `vp/platform/sep/`. The top module is still named
`och_sep_ss`; the sources are `sep_platform.hpp` and `src/sep_platform.cpp`.

```
main.cpp                  sc_main
sep_platform.hpp          och_sep_ss platform module
src/sep_platform.cpp      construction and wiring
inc/                      helpers (Args, adapters, xbar policy, stubs)
config/                   CCI / VeeR ISS runtime files
docs/                     abstractions and AXI notes
```

Build from `vp/` (SystemC 3.0.2, CCI 1.0.2, C++20 default):

```bash
cd vp
./configure_vp.sh
cmake --build build --target sep-vp -j
# binary: vp/build/bin/sep-vp
```

Firmware tests: `sw/sep-vp-tests/run_sep_vp_tests.sh`.
Subsystem book: `sep/doc/index.adoc`.
