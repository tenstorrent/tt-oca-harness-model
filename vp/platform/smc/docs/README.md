# SMC virtual platform (`smc-vp`)

Platform notes for `vp/platform/smc/`. The layout matches `vp/platform/sep/`:

```
main.cpp                  sc_main
smc_platform.hpp          platform module
src/smc_platform.cpp      construction and wiring
inc/                      helpers (routers, stubs, width adapter)
config/                   CCI ini
docs/                     this folder
```

Address map, fabric binding, and CCI presets are documented in
`smc/doc/systemc_tlm2_integration_guide.adoc` and
`smc/doc/platform_test_and_firmware_guide.adoc`.
