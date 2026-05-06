# xSPI Controller SystemC TLM Modeling Guide
## IP6522 (xSPI Controller) + IP6182 (Soft PHY)

> **Approach:** The reference implementations in the archives below are **not to be modified**. They serve purely as behavioral reference when generating a new SystemC TLM model from scratch.

---

### 1. Scope

The new model is a **Loosely Timed (LT) SystemC TLM 2.0** functional model of the xSPI Controller (**IP6522**) paired with the xSPI Octal-DDR Soft PHY (**IP6182**). It is not cycle-accurate. Timing, DDR signal encoding, and PHY-layer behavior are abstracted. The focus is correct functional behavior of the register interface, operating modes, and flash command execution as seen by software.

---

### 2. Reference Sources

| Source | What to take from it |
|---|---|
| `cdns_xspi_ug_v1.06.docx` | Primary spec — all operating modes (Direct, STIG, ACMD, XIP, Boot), PoR/discovery flow, command encoding |
| `cdns_xspi_GA_FULL_Register_Reference_Manual.docx` | All register offsets, bitfields, reset values, R/O and W1C semantics |
| JESD216A (SFDP standard) | SFDP ROM layout — header, parameter header, basic flash parameter table (16 DWORDs) |
| **Archive 1** — `cdns_xspi_cntr_model/` | Register model, SFDP discovery logic, Direct mode, STIG mode engine, TLM extension definitions |
| **Archive 2** — `cdns_xspi_acmd/` | PIO mode (READ/PROGRAM DMA), ACMD descriptor engine, chaining, per-thread status/interrupts |

---

### 3. Feature-by-Feature Reference Guide

#### 3.1 Registers
- Source of truth: **Register Reference Manual** (all offsets, bitfields, reset values, R/O, W1C).
- Cross-check the auto-generated `cdns_xspi_ctrl_regBase.h` in Archive 1 to see how each register maps to scml2 constructs and which ones have `handle_write_*` callbacks.
- Key register groups: `ctrl_cmd_stat_a` (0x00–0x1FF), `cmn_seq_regs_a` (sequence configs), PHY regs.

#### 3.2 Power-on Reset & SFDP Discovery (UG §4.3, Tables 4.36–4.50)
- Arrives via a TLM write on `PoR_input_signals` socket carrying an `xspi_PoR_trans` extension.
- Extension fields: `discovery_inhibit`, `discovery_num_lines`, `discovery_abnum`, `discovery_bank`, `discovery_cmd_type`, `discovery_dummy_cnt`, CRC config fields, `boot_en`.
- If `discovery_inhibit = 0`: read SFDP from flash, parse JESD216A basic table, configure all 10 sequence registers (`global_seq_cfg`, `rst_seq_cfg`, `ers_seq_cfg`, `prog_seq_cfg`, `read_seq_cfg`, `stat_seq_cfg`, `we_seq_cfg`).
- **Refer to:** `cdns_xspi_ctrl_reg.cc::b_transport_por_input()` and `start_discovery()` in Archive 1 for the exact 10-step configuration sequence.

#### 3.3 Direct Mode (UG §4.4 — Direct Access)
- Active when `ctrl_config.work_mode` bits [6:5] = `2'b00`. AXI slave transactions are directly converted to flash commands.
- READ → send `READ_ZERO_LATENCY` opcode on `xspi_bus_socket`.
- WRITE → send `WREN` then `PAGE_PROGRAM` on `xspi_bus_socket`.
- Address remapping via `direct_access_rmp` / `direct_access_rmp_1` must be applied before the flash address is sent.
- **Refer to:** `cdns_xspi_ctrl_reg.cc::handle_direct_mode()` in Archive 1.

#### 3.4 STIG Mode (UG §4.4 — STIG)
- Active when `ctrl_config.work_mode` bits [6:5] = `2'b01`. Triggered by a write to `cmd_reg0`.
- Decode fields from `cmd_reg0–4`: opcode, address, data byte count, `INSTR_LINK` (enables a two-phase command+data sequence across two `cmd_reg0` writes).
- An SC_THREAD fires on the trigger event, executes the xSPI bus transaction, and sets `cmd_status.COMPLETE`.
- instr_type encodings: READ=1, WRITE=2, data-phase=0x7F or 0x80.
- **Refer to:** `stig_instruction` struct and `stig_engine_thread()` in Archive 1. Five handlers: read, write, control (WREN/WRDI/RDSR), suspend/resume, and read SFDP.

#### 3.5 PIO Mode (UG §4.4.2)
- Active when `ctrl_config.work_mode` bits [6:5] = `2'b11` (ACMD global) and `cmd_reg0[31:30] = 2'b01`.
- Thread ID in `cmd_reg0[26:24]` (0–7). Snapshot only the `cmd_reg` fields needed for the command type at trigger time.
- READ: DMA xSPI address → system memory via `axi_master_socket`.
- PROGRAM: DMA system memory → xSPI address via `axi_master_socket`.
- After completion: update `TRD_STATUS`, write `cmd_status`, assert `TRD_COMP_INT` or `TRD_ERR_INT` if `cmd_reg0.INT = 1`.
- **Refer to:** `cdns_xspi_acmd.cpp::pio_handle_trigger()` and `pio_execute_command()` in Archive 2.

#### 3.6 ACMD / CDMA Mode (UG §4.4.1)
- Active when `ctrl_config.work_mode` bits [6:5] = `2'b11` and `cmd_reg0[31:30] = 2'b00`.
- `cmd_reg2`/`cmd_reg3` = 64-bit descriptor pointer (must be 64-byte aligned).
- Each 64-byte descriptor (UG Table 4.6): `next_pointer`, `system_memory_pointer`, `xspi_pointer`, command word (`cmd_type[15:0]` + `flags[31:16]` + `counter[47:32]`), status word (written back by controller at offset +40).
- State machine: FETCH → VALIDATE → EXECUTE → WRITEBACK_STATUS → (CONT+next≠0 → FETCH : COMPLETE).
- Key flags: CONT (chain), INT (interrupt at chain end), MB_XIP_EN (only valid on READ — reject otherwise with DSC_ERROR).
- **Refer to:** `cdns_xspi_acmd.cpp::cdns_xspi_acmd_dma` in Archive 2 — all state machine transitions, descriptor parsing, and status writeback.

#### 3.7 Interrupts (UG §5 — Interrupt Handling)
- `TRD_COMP_INT` (bit per thread) — set on PIO/ACMD thread completion if INT flag was set.
- `TRD_ERR_INT` (bit per thread) — set on error.
- `CMD_IGNORED` in `intr_status` — set when a cmd_reg0 write targets a thread that is already busy.
- All three are W1C. `interrupt_out` is driven high when either `TRD_COMP_INT` or `TRD_ERR_INT` is non-zero.
- **Refer to:** `cdns_xspi_acmd.cpp::update_interrupt()` and `signal_cmd_ignored()` in Archive 2.

#### 3.8 XIP Mode (UG §4.4 — XIP)
- Enabled per-bank via `xip_mode_cfg.xip_en`. Only READ sequences are valid while XIP is active.
- On the next READ after enable, insert mode byte value `xip_en_mb_val` in the flash transaction (between address and data phases) to command the flash into XIP state.
- Exit: set `direct_access_cfg.mode_bit_xip_dis` → insert `xip_dis_mb_val` on next READ.
- **No reference implementation exists** — implement from scratch using UG spec alone.

#### 3.9 Boot Engine (UG §4.2 — Automated Boot)
- When `boot_en = 1` arrives in `xspi_PoR_trans`, the controller autonomously DMAs a flash boot region to system memory without any CPU involvement.
- On success: assert `boot_comp` output signal.
- On failure: assert `boot_error`; populate `boot_status` (`boot_dqs_err`, `boot_crc_err`, `boot_bus_err`).
- **No reference implementation exists** — implement as an SC_THREAD launched from the PoR handler.

---

### 4. PHY Abstraction (IP6182)

- PHY registers (`phy_dq_timing_reg`, `phy_dqs_timing_reg`, `phy_dll_master_ctrl_reg`, `phy_dll_slave_ctrl_reg`, `phy_gate_lpbk_ctrl_reg`, `phy_ie_timing_reg`, `phy_static_togg_reg`, `phy_ctrl_reg`, `phy_tsel_reg`, `phy_gpio_ctrl_0/1`) — modeled as **read/write register storage only**. No PHY timing or DDR encoding is simulated.
- The `xspi_bus_socket` carries TLM generic payloads with a `cdns_extension` TLM extension. The extension encodes opcode, bank/CS, address, data bytes, and flags — eliminating the need to model DDR wire framing.
- **Refer to:** `cdns_extension.h` in Archive 1 for the extension field definitions.

---

### 5. TLM Interfaces and Extensions

| Interface | Direction | Carries |
|---|---|---|
| `t_axi_slave_socket` | Target | AXI register/memory access from CPU |
| `PoR_input_signals` | Target | `xspi_PoR_trans` extension — bootstrap + discovery + boot signals |
| `xspi_bus_socket[N]` | Initiator | `cdns_extension` — opcode, address, data, bank select to flash targets |
| `i_dma_socket` | Initiator | AXI master DMA for PIO/ACMD system memory transfers |
| `interrupt_out` | Signal out | Active when `TRD_COMP_INT` or `TRD_ERR_INT` ≠ 0 |
| `reset_in` | Signal in | Active-low reset |

**`xspi_PoR_trans` key fields:** `discovery_inhibit`, `discovery_num_lines`, `discovery_abnum`, `discovery_bank`, `discovery_cmd_type`, `discovery_dummy_cnt`, CRC config fields, `boot_en`, `boot_comp`, `boot_error`.

**`cdns_extension` key fields:** `opcode`, `bank_num`, `data_bytes`, `address`, `write_data`, `instr_type`, flags.

> Implement `clone()` and `copy_from()` correctly in both extensions — the reference archives leave these as stubs returning `nullptr`, which breaks any TLM socket that clones payloads.

---

### 6. Modeling Abstractions

1. **Not cycle-accurate** — no clock period, no DDR timing, no DLL calibration sequence.
2. **PHY layer fully abstracted** — IP6182 DDR/DQS encoding is not modeled; PHY registers are store-and-acknowledge only.
3. **CRC not computed** — `discovery_seq_crc_en` is accepted but no CRC is run over SFDP data.
4. **Interrupt enables** — `TRD_COMP_INT` / `TRD_ERR_INT` should be gated by their respective enable registers before driving `interrupt_out`; reference archives do not check the enable masks.
5. **Long/short polling** — registers stored; no polling loop executed.
6. **Erase and Reset commands** — functionally stubbed in all reference archives; model behavior per spec (update flash state machine, assert `boot_error` if boot-triggered erase fails).
7. **Address remapping** — `direct_access_rmp` / `direct_access_rmp_1` registers are present in all reference archives but never applied to the outgoing address; implement from the Register Manual spec.
