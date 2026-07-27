# SMC DMA — SystemC/TLM-2.0 Loosely-Timed Model

This directory contains a functional, loosely-timed SystemC/TLM-2.0 model of the SMC DMA controller.

## Build and test

```bash
cd smc/peripherals/dma
./run_tests.sh
./run_tests.sh --asan
./run_tests.sh --coverage
```

The script auto-detects `SYSTEMC_HOME` and `CCI_HOME` from common install paths. Local overrides can be placed in `deps.env` (gitignored).

## CCI parameters

| Name              | Default | Mutability | Description |
|-------------------|---------|------------|-------------|
| `num_channels`    | 16      | immutable  | Number of DMA channels (1..16) |
| `access_delay_ns` | 2.0     | mutable    | Register access annotated delay |
| `transfer_delay_ns` | 0.0   | mutable    | Data transfer annotated delay |
| `max_burst_bytes` | 64      | immutable  | Maximum bytes per master transaction |

## Model notes

- The register map follows `vendor/pulp-platform/idma/overlay/rdl/dma_ctrl.rdl`.
- Transfers are initiated by reading a `NEXT_ID_N` register and execute asynchronously in a dedicated SystemC thread.
- Outgoing master transactions carry the canonical `smc::smc_axi_extension` with `source_id = smc::SMC_ID`.
