# SMC CPU Cluster — Documentation

Numbered documents follow the [Component Developer Guide](../../../doc/component-developer-guide.md) layout (`01` Specification, `02` LLD, `03` Test Plan).

## Document index

| Markdown | PDF | Contents |
|----------|-----|----------|
| [01_CPU_Cluster_Specification.md](01_CPU_Cluster_Specification.md) | [01_CPU_Cluster_Specification.pdf](01_CPU_Cluster_Specification.pdf) | Purpose, module boundary, configuration, requirements |
| [02_CPU_Cluster_LowLevel_Design.md](02_CPU_Cluster_LowLevel_Design.md) | [02_CPU_Cluster_LowLevel_Design.pdf](02_CPU_Cluster_LowLevel_Design.pdf) | Architecture, implementation, diagrams, build graph |
| [03_CPU_Cluster_Test_Plan.md](03_CPU_Cluster_Test_Plan.md) | [03_CPU_Cluster_Test_Plan.pdf](03_CPU_Cluster_Test_Plan.pdf) | Verification strategy, test cases, coverage plan |
| [04_CCI_Integration_Guide.md](04_CCI_Integration_Guide.md) | [04_CCI_Integration_Guide.pdf](04_CCI_Integration_Guide.pdf) | SystemC CCI 1.0 adoption (supplementary) |

SMC-wide PDF references (when present alongside this tree):

- `01_SMC_Architecture.pdf` — chiplet architecture §3
- `02_SMC_IP_LowLevel_Design.pdf` — IP LLD §3
- `03_SMC_Test_Plan.pdf` — project test methodology

## PDF generation

```bash
cd smc/cpu_cluster
./doc/build_docs.sh
```

Requires **pandoc** and **Chrome/Chromium**. Stylesheet: `print.css`.
