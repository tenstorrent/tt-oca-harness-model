# SMC RTL to SystemC Integration Guide

**Document:** `05_RTL_SystemC_Integration_Guide.md`  
**Project:** SMC SystemC / TLM-2.0 model  
**RTL reference:** `/Users/pdroy/tt_oca_hw/tt-oca-hw/hw/smc`  
**Firmware reference:** `/Users/pdroy/tt_oca_hw/tt-oca-hw/fw/smc`

---

## Contents

1. [Purpose](#1-purpose)
2. [RTL Structure to Mirror](#2-rtl-structure-to-mirror)
3. [SystemC Integration Strategy](#3-systemc-integration-strategy)
4. [Step-by-Step Integration Plan](#4-step-by-step-integration-plan)
5. [CPU Cluster Details](#5-cpu-cluster-details)
6. [Memory Map and Register Headers](#6-memory-map-and-register-headers)
7. [Firmware and Driver Portability](#7-firmware-and-driver-portability)
8. [Bring-Up and Test Plan](#8-bring-up-and-test-plan)
9. [Open Items](#9-open-items)

---

## 1. Purpose

This guide describes how to integrate the SystemC submodules under `smc/`
so the virtual platform follows the RTL implementation in
`tt-oca-hw/hw/smc`.

The immediate goal is not cycle accuracy. The SystemC model should provide
a faithful programmer's view:

- the same CPU-facing memory map and reset vectors,
- the same software-visible register names and offsets,
- equivalent interrupt routing,
- equivalent ROM and scratch SRAM boot behavior,
- TLM paths that match the RTL AXI master/target topology closely enough to
  boot and test SMC firmware.

The current SystemC repo already contains useful pieces:

| SystemC area | Current role |
|---|---|
| `smc/cpu_cluster` | Whisper-backed RV64GC CPU cluster model |
| `smc/smc_fabric` | TLM fabric/router model |
| `smc/peripherals/bootrom` | ROM memory model |
| `smc/peripherals/scratchpad_ram` | Scratch SRAM model |
| `smc/peripherals/plic` | PLIC model |
| `smc/peripherals/clint` | CLINT model |
| `smc/peripherals/reset_unit` | Reset-unit model |
| `smc/peripherals/i3c_controller` | I3C-facing model scaffold |
| `smc/peripherals/uart` | UART model |

The integration work is to bind these pieces into an SMC-level platform that
matches the RTL boundary and can execute the ROM firmware from `tt-oca-hw`.

---

## 2. RTL Structure to Mirror

The RTL hierarchy has three important layers.

### 2.1 Top Level: `smc.sv`

`hw/smc/smc.sv` is the full SMC subsystem. It owns the chiplet-facing ports:

- clocks and resets: `clk_smc_i`, `clk_ref_i`, `clk_periph_i`,
  `powergood_i`, `rst_cold_ni`;
- inbound AXI from system, JTAG, SEP, and OTP/JTAG;
- outbound AXI to the system;
- peripheral AXI-Lite ports for PLL, PVT, GPIO, eFuse, I3C extension, DTP,
  and adopter extension;
- GPIO, SPI, JTAG, telemetry, DFD, lifecycle, mailbox, NDM reset, test, and
  strap-related signals;
- CPU memory macro ports for ROM, scratch RAM, and cache memories.

For SystemC, this should become an `smc_top` or `smc_platform` module that
owns:

- a clock/reset abstraction rather than every RTL clock gate,
- TLM sockets for system/SEP/JTAG inbound traffic and system outbound traffic,
- `sc_signal` or CCI-backed configuration for straps, lifecycle, boot mode,
  and chiplet role,
- submodule instances for CPU cluster, fabric, ROM, scratch SRAM, PLIC, CLINT,
  reset unit, and selected peripherals.

### 2.2 Core Logic: `smc_base.sv`

`hw/smc/smc_base.sv` is the RTL core of the subsystem. It connects:

- input fabric,
- local fabric,
- output fabric,
- internal registers,
- CPU wrapper,
- data accelerator / zeroer / DMA area,
- remap and filter CSRs,
- mailbox interrupts,
- PLIC-facing interrupt vector composition.

This is the best structural guide for the SystemC interconnect. The SystemC
fabric should route the same address regions to the same programmer-visible
blocks, even if timing and arbitration are abstracted.

### 2.3 CPU Boundary: `smc_cpu_wrapper.sv`

`hw/smc/smc_cpu/smc_cpu_wrapper.sv` is the most important RTL file for the
current SystemC CPU model. It wraps either `smc_1core_cpu` or `smc_4core_cpu`
and provides:

- an AXI front port from the local fabric,
- an AXI MMIO master port from the CPU cluster back into the SMC fabric,
- a CPU-control CSR window decoded from the front port,
- reset vector, core reset, uncore reset, debug reset, and drain/isolation
  controls,
- per-core watchdog timeout status,
- writeback PC status,
- cluster DED status,
- ROM, scratch RAM, and cache memory macro ports,
- SRAM auto-init completion.

The SystemC `smc_cpu_cluster` already models the CPU execution side. The next
integration step is to place it behind the same logical boundary:

| RTL signal group | SystemC equivalent |
|---|---|
| CPU MMIO AXI master | `smc_cpu_cluster::mmio` initiator socket |
| CPU front/L2 path | `smc_cpu_cluster::data` and `ifetch` initiator paths, routed through fabric or memory targets |
| CPU-control CSR window | `smc_cpu_cluster::ctrl` target socket |
| reset vector/core reset | CCI parameters plus reset-unit-driven control calls/signals |
| `interrupts_i` vector | PLIC/CLINT/watchdog/BEU signals mapped to hart IRQ inputs |
| `init_mem_done_o` | scratch SRAM initialization status signal |
| `wb_reg_pc_o` / DED / WDT | debug/status API surfaced via registers or trace hooks |

---

## 3. SystemC Integration Strategy

Use the RTL as the source of truth for visible behavior, but keep the SystemC
model loosely timed and firmware-oriented.

### 3.1 Keep These RTL Semantics

- `SMC_CPU_CONFIG` supports 1-core and 4-core configurations.
- 1-core has `NUM_CPU_INTERRUPTS = 104` and `NUM_EXT_INTERRUPTS = 32`.
- 4-core has `NUM_CPU_INTERRUPTS = 328` and `NUM_EXT_INTERRUPTS = 256`.
- Local alias base is `0xC000_0000`.
- The SMC local alias aperture is 16 MiB in RTL package constants.
- CPU-control is at `BASE + 0x001_0000`.
- ROM is at `BASE + 0x004_0000`.
- Scratch SRAM is at `BASE + 0x006_0000`.
- PLIC is at `BASE + 0x400_0000`.
- CLINT is at `BASE + 0x800_0000`.
- BEU windows start at `BASE + 0x801_0000 + N * 0x1000`.
- Interrupt source ID presented to PLIC is raw `cpu_interrupts_o` bit + 1.

### 3.2 Abstract These RTL Details

- Cycle-level AXI arbitration and outstanding transaction limits.
- Clock-gate implementation and PLL lock timing.
- SRAM macro pin protocols, cache tag/data SRAM arrays, and BIST/repair
  internals.
- Reset-domain crossing timing.
- DFT, scan, and physical pad behavior.

Model these only where firmware observes them through registers, status bits,
interrupts, or blocking behavior.

---

## 4. Step-by-Step Integration Plan

### Step 1: Create an SMC Platform Module

Add a top-level SystemC module under `smc/`, for example:

```cpp
namespace smc {

class smc_platform : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<smc_platform, 64> sys_axi_in;
    tlm_utils::simple_target_socket<smc_platform, 64> sep_axi_in;
    tlm_utils::simple_target_socket<smc_platform, 64> jtag_axi_in;
    tlm_utils::simple_initiator_socket<smc_platform, 64> sys_axi_out;

    SC_HAS_PROCESS(smc_platform);
    smc_platform(sc_core::sc_module_name name, const config& cfg);
};

} // namespace smc
```

The first version should instantiate CPU cluster, fabric, ROM, scratch SRAM,
PLIC, CLINT, and reset unit. GPIO, eFuse, PLL, I2C/I3C, mailbox, and status
peripherals can be added incrementally as firmware reaches them.

### Step 2: Encode RTL Configuration as CCI Parameters

Expose platform-wide parameters through CCI:

| Parameter | Initial value | Purpose |
|---|---:|---|
| `smc.cpu_config` | `SMC_4CORE` | Select 1-core or 4-core CPU model |
| `smc.local_base` | `0xC0000000` | Local alias base seen by firmware |
| `smc.region_size` | `0x01000000` | 16 MiB local alias aperture |
| `smc.global_base` | platform-defined | Programmable system aperture |
| `smc.rom_base` | `0xC0040000` | ROM image load base |
| `smc.sram_base` | `0xC0060000` | Scratch SRAM base |
| `smc.rom_image` | empty | Path to ROM ELF/bin/hex |
| `smc.primary_chiplet` | false | Strap-visible chiplet role |
| `smc.boot_stall` | false | Hold CPU before ROM execution |
| `smc.bl0_pllclk` | false | Firmware-visible PLL strap |
| `smc.disable_sram_auto_init` | false | SRAM auto-zero behavior |

Keep the existing CPU-cluster CCI parameters described in
`04_CCI_Integration_Guide.md`. Platform parameters should drive those CPU
parameters before CPU construction.

### Step 3: Build the Address Map from RTL Register Headers

Generate or import the C register header from the RTL RDL source:

- RTL RDL: `hw/smc/data/registers/rdl/smc_top.rdl`
- RTL C header: `fw/smc/prod_rom/registers/smc_top_regs.h`
- RTL SV header: `hw/smc/data/registers/svh/smc_top_reg.svh`

SystemC tests and firmware should use the same generated register names where
possible. Avoid hand-maintaining duplicate constants in SystemC.

Minimum required decode for ROM bring-up:

| Region | Base | Target |
|---|---:|---|
| WDT | `0xC0000000` | `smc::wdt` stage-1 (×N); stage-2 in cluster/`cpu_ctrl` |
| Reset Unit | `0xC0002000` | `reset_unit` |
| MISC / scratch | `0xC0002800` | scratch/status register model |
| CPU Control | `0xC0010000` | `smc_cpu_cluster::ctrl` |
| Mailbox | `0xC0018000` | mailbox stub or model |
| DMA / zeroer | `0xC0038000` | zeroer/DMA stub for SRAM init paths |
| ROM | `0xC0040000` | `bootrom` |
| Scratch SRAM | `0xC0060000` | `scratchpad_ram` |
| PLIC | `0xC4000000` | `plic` |
| CLINT | `0xC8000000` | `clint` |
| BEU | `0xC8010000 + N * 0x1000` | BEU stub/model |

### Step 4: Wire CPU Cluster Like `smc_cpu_wrapper.sv`

Bind CPU sockets to the platform fabric:

- CPU `ifetch` and `data` route to ROM, scratch SRAM, CLINT, PLIC, and MMIO
  according to the memory map.
- CPU `mmio` routes through the same local/output fabric model used by
  inbound system traffic.
- CPU `ctrl` is reachable at `BASE + 0x001_0000`.

Then add reset controls:

- reset vector defaults to ROM base `0xC0040000`;
- `BOOT_STALL` prevents the CPU step threads from running until released;
- CPU reset writes through the CPU-control model update hart run state;
- `init_mem_done` is set by scratch SRAM auto-init or by a zeroer model.

The RTL has a drain/isolation handshake around reset. In SystemC, model the
firmware-visible effect: reset must not leave pending transactions unresolved.
It is acceptable to drain by completing or rejecting outstanding TLM requests at
reset boundaries.

### Step 5: Add Interrupt Aggregation

Mirror the interrupt vector construction in `smc_base.sv`:

```text
cpu_interrupts_o[NUM_EXT_INTERRUPTS-1:0]      = ext_interrupts_i
cpu_interrupts_o[NUM_EXT_INTERRUPTS+:32]      = peripheral_interrupts_i
cpu_interrupts_o[NUM_EXT_INTERRUPTS+32+:32]   = mailbox_interrupts
cpu_interrupts_o[NUM_EXT_INTERRUPTS+64]       = TDR clock-stop interrupt
cpu_interrupts_o[NUM_EXT_INTERRUPTS+64+1]     = CLA interrupt
cpu_interrupts_o[NUM_EXT_INTERRUPTS+64+2]     = DMA interrupt
cpu_interrupts_o[NUM_EXT_INTERRUPTS+64+3]     = zeroer interrupt
```

Feed this vector into PLIC source lines. Remember that PLIC source ID is raw
bit index + 1.

For early ROM boot, the following can be stubs:

- external interrupts,
- TDR/CLA,
- DMA interrupt,
- zeroer interrupt if SRAM init is modeled as instantaneous.

For firmware tests, mailbox, I2C/I3C, UART, and OCTS interrupts need functional
source toggles once their register models are present.

### Step 6: Load ROM Firmware

Use the production ROM under:

```text
/Users/pdroy/tt_oca_hw/tt-oca-hw/fw/smc/prod_rom
```

The RTL ROM documentation places ROM at `0xC0040000`, and the linker script
uses:

| Memory | Origin | Purpose |
|---|---:|---|
| `rom` | `0xC0040000` | ROM text and read-only data |
| `testram` | `0xC0060000` | SRAM data, bss, stack, heap |

The SystemC platform should support at least one of these flows:

1. Load an ELF directly through `smc_cpu_cluster::load_elf`.
2. Convert ROM output to a flat binary/hex and preload `bootrom`.
3. Load ROM sections into `bootrom` and initialized data into SRAM according
   to ELF program headers.

For firmware realism, option 3 is best. For quick bring-up, option 1 is enough
to validate the CPU, address map, and register stubs.

### Step 7: Stub Only What Firmware Can Tolerate

A useful first boot target is to reach the ROM's early status writes without
initializing all external interfaces.

Required early stubs:

- strap registers,
- lifecycle/security register fields,
- eFuse read path or deterministic eFuse defaults,
- scratch/status registers,
- POST code scratch registers,
- SRAM init completion,
- optional no-op PLL success path,
- basic OCCP interface return path or controlled bypass.

Avoid returning decode errors from addresses the ROM polls during early boot.
For unimplemented but expected blocks, return stable reset values and add
warnings only once per address block.

### Step 8: Add Peripheral Models in Firmware Order

Integrate peripherals in the order the ROM touches them:

1. scratch/status/POST registers,
2. straps and reset-unit status,
3. eFuse shadow/config registers,
4. PLL/clock status registers,
5. SRAM zeroer status if auto-zero is disabled,
6. interface map dependencies,
7. I2C/I3C controller registers,
8. mailbox/OCCP transport,
9. security and lifecycle register paths,
10. watchdog, UART/log, telemetry, OCTS, and remaining interrupt sources.

This order keeps each increment testable with a firmware-visible milestone.

---

## 5. CPU Cluster Details

The RTL supports two generated Rocket configurations:

| Config | Cores | External interrupts | Total CPU interrupt bits | Default reset behavior |
|---|---:|---:|---:|---|
| `SMC_1CORE` | 1 | 32 | 104 | cores held in reset by default |
| `SMC_4CORE` | 4 | 256 | 328 | cores out of reset by default |

The current SystemC CPU cluster supports 1 to 4 harts through CCI. Use
`smc.cpu_config` to set the default hart count, then pass that value into
`smc_cpu_cluster::num_harts`.

Important RTL-to-SystemC mappings:

| RTL concept | SystemC behavior |
|---|---|
| `reset_vector_i[core]` | per-hart reset PC, default `0xC0040000` for ROM |
| `rst_core_ni` | hart running/held state |
| `rst_uncore_ni` | cluster-level reset and fabric visibility |
| `rst_debug_ni` | debug API availability or JTAG stub state |
| `interrupts_i` | PLIC/CLINT-driven hart interrupt inputs |
| `wdt_reset_o` | watchdog status and reset cause |
| `wb_reg_pc_o` | debug/status readable PC |
| `cluster_ded_o` | sticky fatal/error status |
| `disable_sram_auto_init_i` | choose HW auto-init vs firmware/zeroer path |
| `init_mem_done_o` | release boot once scratch SRAM is ready |

The RTL's `axi_isolate` wrappers prevent reset-time X propagation and bus
hangs. In the SystemC model, preserve the invariant rather than the exact
mechanism: a reset should complete all in-flight TLM transactions, reject new
ones during reset, and release the CPU only after memory initialization is
complete.

---

## 6. Memory Map and Register Headers

Use `fw/smc/prod_rom/registers/smc_top_regs.h` as the firmware-facing
definition of addresses and fields. It is generated from the RTL RDL and
contains the constants the ROM already includes.

The key base addresses are:

| Block | Base |
|---|---:|
| SMC top local alias | `0xC0000000` |
| WDT core 0 | `0xC0000000` |
| Reset unit | `0xC0002000` |
| MISC/scratch | `0xC0002800` |
| GPIO | `0xC0004000` |
| PVT | `0xC0007000` |
| AVSBus | `0xC0008000` |
| I2C | `0xC0009000` |
| UART | `0xC000A000` |
| eFuse map | `0xC000B000` |
| eFuse controller/shim | `0xC000C000` |
| Telemetry | `0xC000D000` |
| OCTS | `0xC000E000` |
| DTP/DFT | `0xC000F000` |
| CPU control | `0xC0010000` |
| Remap/filter CSRs | `0xC0012000` - `0xC0016FFF` |
| Mailbox | `0xC0018000` |
| DMA/zeroer | `0xC0038000` |
| ROM | `0xC0040000` |
| Scratch SRAM | `0xC0060000` |
| PLIC | `0xC4000000` |
| CLINT | `0xC8000000` |
| BEU | `0xC8010000 + N * 0x1000` |

Recommended implementation rule:

> SystemC should not define a second source of truth for these addresses.
> Either include generated C headers in firmware-only builds and generate
> matching C++ constants for the model, or add a small generator that emits both
> SystemC and firmware headers from the same RDL.

---

## 7. Firmware and Driver Portability

There is SMC firmware in `tt-oca-hw` that can be ported. The canonical tree is:

```text
/Users/pdroy/tt_oca_hw/tt-oca-hw/fw/smc/prod_rom
```

There is also a mirrored copy under:

```text
/Users/pdroy/tt_oca_hw/tt-oca-hw/hw/oss-example/tb/occp_i2c_rom_smoke/fw/prod_rom/source
```

Prefer the canonical `fw/smc/prod_rom` tree unless a specific OSS example test
requires the mirrored copy.

### 7.1 High-Confidence Reuse

These files should port with little or no source change if the SystemC platform
implements the same address map and reset values:

| Code | Why it ports well |
|---|---|
| `registers/smc_top_regs.h` | Generated register addresses and field masks; should be the shared source for firmware tests |
| `lib/include/smc_defines.h` | MMIO accessors and SMC address helpers; depends mostly on stable register constants |
| `lib/src/smc_post_code.c` | Writes boot progress through scratch/status registers |
| `lib/src/smc_scratchpad.c` | Uses scratch registers and SRAM offsets for SMC/SEP coordination |
| `lib/src/smc_ring_buffer.c` | Pure SRAM data structure with simple MMIO/SRAM assumptions |
| `lib/src/smc_status.c` | Status reporting on top of ring buffer and scratch/status definitions |
| `lib/src/smc_occp_status.c` | Structured OCCP status values; mostly protocol logic |
| `lib/src/smc_security.c` | Portable once lifecycle/eFuse/security registers return realistic values |
| `drivers/src/smc_strap.c` | Portable if reset-unit/GPIO strap register values are modeled consistently |
| `include/smc_rom_config.h` | Build-time version constants |
| `include/smc_rom_defs.h` | Constants used throughout ROM; validate SRAM/ROM bounds against SystemC map |

### 7.2 Moderate Reuse

These should compile with limited changes, but need corresponding SystemC
peripheral behavior:

| Code | Required SystemC support |
|---|---|
| `src/main.c` | Needs early boot register stubs, scratch/status, eFuse defaults, straps, and OCCP path |
| `boot/*.S` | Needs Whisper-compatible reset entry, trap setup, stack, and linker layout |
| `linker/quasar/smc_rom.ld` | Address layout is already aligned to RTL; may need build-system path changes |
| `drivers/src/smc_efuse.c` | Needs eFuse map/shim register model or deterministic shadow register preload |
| `drivers/src/smc_pll.c` | Current implementation is weak/stub-like; ports easily if SystemC returns PLL success/lock values |
| `lib/src/smc_interface_map.c` | Needs strap, security, eFuse, I2C/I3C address, and role inputs |
| `lib/src/occp.c` | Needs I2C/I3C/mailbox transport model and SRAM access checks |

### 7.3 Higher-Effort Reuse

These depend on detailed peripheral semantics or DV-only assumptions:

| Code | Porting concern |
|---|---|
| `drivers/src/i2c_target_driver.c` | Requires accurate I2C target controller registers, FIFOs, interrupts, and timing abstractions |
| `drivers/src/i3c_target_driver.c` | Requires Cadence I3C register behavior, DAT/DCT memories, interrupt model, and GPIO pad-function setup |
| `fw/smc/common/occp/occp_interfaces.c` | Master/test paths use GPIO overrides, random controller selection, and testbench-facing assumptions |
| `fw/smc/tests/**` and `fw/smc/tests_rom/**` | Useful as regression payloads, but many assume RTL/UVM BFMs and peripheral completeness |
| `dv/**/common/smc_api.py` and cocotb drivers | Good behavioral references, not direct SystemC firmware |

### 7.4 Practical Porting Recommendation

Start with a ROM smoke image that uses:

- `smc_top_regs.h`,
- `smc_defines.h`,
- POST code,
- scratchpad,
- status/ring buffer,
- strap driver,
- weak/no-op PLL,
- simple eFuse defaults.

Defer full OCCP over I2C/I3C until the SystemC I2C/I3C models can support the
firmware's register-level driver expectations.

This gives a low-risk milestone: boot ROM reaches `SMC_STATUS_BOOT_START` or
`SMC_STATUS_BOOT_COMPLETE` and writes the expected scratch/status markers.

---

## 8. Bring-Up and Test Plan

### 8.1 Integration Milestones

| Milestone | Expected result |
|---|---|
| M0: platform elaborates | `smc_platform` constructs with CPU, fabric, ROM, SRAM, PLIC, CLINT |
| M1: CPU reset vector | hart 0 starts at `0xC0040000` or loaded ELF entry |
| M2: ROM fetch | CPU fetches from `bootrom` and reads `.rodata` |
| M3: SRAM usable | `.data`, `.bss`, stack, and ring buffers access `0xC0060000+` |
| M4: scratch/status writes | POST/status registers show ROM progress |
| M5: strap/security path | ROM sees deterministic strap, lifecycle, and eFuse values |
| M6: PLIC/CLINT sanity | timer/software/external interrupts can wake a hart |
| M7: OCCP smoke | simple OCCP command path works over stubbed or modeled transport |
| M8: production ROM smoke | unmodified or minimally patched `fw/smc/prod_rom` reaches command loop |

### 8.2 Tests to Add

- Address-map decode test using generated `smc_top_regs.h` constants.
- ROM ELF load test that checks entry PC, stack, and first scratch writes.
- Reset/boot-stall test for `BOOT_STALL` asserted and released.
- SRAM init test for both auto-init enabled and disabled.
- Strap driver test that reads modeled strap values through firmware accessors.
- PLIC source-index test for 1-core and 4-core interrupt maps.
- CLINT MSIP/MTIP wake test.
- Smoke firmware test that runs a reduced ROM payload to `wfi`.
- Production ROM test that reports the last status code reached before an
  unimplemented peripheral blocks progress.

### 8.3 Pass Criteria

- No duplicate, hand-maintained memory map constants are introduced.
- The same ROM image can run on RTL and SystemC, or source differences are
  limited to build-system and transport stubs.
- The SystemC platform can boot with both 1-core and 4-core CPU configurations.
- Unimplemented peripherals fail in a controlled way with clear diagnostics,
  not by hanging the simulation.

---

## 9. Open Items

- Decide the canonical location and name for the SMC top-level SystemC module.
- Add a generated C++ register map from `smc_top.rdl` or a checked-in generated
  header synchronized with `smc_top_regs.h`.
- Decide whether ROM loading is ELF-section based or bootrom-image based.
- Define a minimal eFuse preload format for SystemC tests.
- Define I2C/I3C modeling depth needed for OCCP: register-accurate target,
  transaction-level shortcut, or both.
- Add a firmware build path in this repo, or consume prebuilt ELF/hex artifacts
  from `tt-oca-hw`.
- Align `MAILBOX_INTERUPT_ID_BASE` definitions before relying on mailbox PLIC
  IDs in firmware tests; the canonical interrupt rule is PLIC ID = raw
  `cpu_interrupts_o` bit + 1.
