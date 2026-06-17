# SMC Fabric — SystemC / TLM-2.0 IP Documentation

This directory contains the low-level design documentation for the `smc_fabric`
SystemC module — the TLM-2.0 Loosely-Timed (LT) model of the SMC dual-network
interconnect (AXI4 HP + AXI4-Lite LP routing, address remap, and outbound
filtering).

## Document Index

| Markdown | PDF | Contents |
|----------|-----|----------|
| [01_overview_and_architecture.md](01_overview_and_architecture.md) | [01_overview_and_architecture.pdf](01_overview_and_architecture.pdf) | Purpose, block diagram, data-path flows, key design decisions |
| [02_tlm_interface.md](02_tlm_interface.md) | [02_tlm_interface.pdf](02_tlm_interface.pdf) | TLM-2.0 sockets, `sc_in` signals, constructor parameters |
| [03_internal_architecture.md](03_internal_architecture.md) | [03_internal_architecture.pdf](03_internal_architecture.pdf) | Sub-block decomposition and LT modelling strategy for each |
| [04_register_interface.md](04_register_interface.md) | [04_register_interface.pdf](04_register_interface.pdf) | Full register map and LT access semantics |
| [05_systemc_implementation.md](05_systemc_implementation.md) | [05_systemc_implementation.pdf](05_systemc_implementation.pdf) | C++ header sketch, implementation patterns, integration guide |

## PDF generation

Each Markdown document has a matching PDF in this directory. Regenerate all
PDFs with:

```bash
./doc/build_docs.sh
```

Requires **pandoc** and **Chrome/Chromium**. Stylesheet: `print.css`.

---

## Quick Facts

| Property | Value |
|----------|-------|
| SystemC module name | `smc_fabric` |
| TLM data width | 64-bit |
| Temporal decoupling | **Bucket D — pure target** (no quantum keeper; adds `reg_access_ns` to incoming `delay`) |
| LT routing latency | Zero; register access adds `reg_access_ns` (default 1 ns) |
| Output remap | Enabled by default (`no_addr_remap = false`); matches `smc_config_pkg` |
