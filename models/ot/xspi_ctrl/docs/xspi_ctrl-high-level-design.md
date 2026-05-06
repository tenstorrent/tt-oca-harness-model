# xSPI Controller Model High-Level Design Document

## TABLE OF CONTENTS

1. [Introduction](#1-introduction)
   - [1.1 Objective](#11-objective)
   - [1.2 Scope](#12-scope)
   - [1.3 Acronyms](#13-acronyms)
   - [1.4 Is List](#14-is-list)
   - [1.5 Is Not List](#15-is-not-list)

2. [Functional Description](#2-functional-description)
   - [2.1 xSPI Controller Configuration Parameters](#21-xspi-controller-configuration-parameters)
   - [2.2 Port Interfaces](#22-port-interfaces)
   - [2.3 Memory-Mapped Registers](#23-memory-mapped-registers)

3. [Use Model](#3-use-model)
   - [3.1 Callbacks on Memory-Mapped Registers / Bit-Fields](#31-callbacks-on-memory-mapped-registers--bit-fields)

4. [Assumptions](#4-assumptions)

---

## 1. Introduction

### 1.1 Objective

This document provides the design specifications for the xSPI Controller (xSPI-CTRL) as a SystemC TLM 2.0 compliant model. It covers IP6522 (xSPI Controller) combined with IP6182 (xSPI Octal-DDR Soft PHY). The model is implemented at the Loosely Timed (LT) abstraction level with timing annotation and temporal decoupling. The intended use case is embedded software development and validation. In this design, behavior, communication, and timing are separated as far as the LT abstraction allows. The functional description of xSPI-CTRL, its internal registers, and its interface ports are discussed in detail.

### 1.2 Scope

The scope of this document is to describe the design details of the xSPI Controller TLM model. It focuses on implementation-level details of the xSPI Controller that form the basis for developing the LT model. The model correctly represents the register interface, all six operating modes (Direct, STIG, PIO, ACMD/CDMA, XIP, and Boot), the Power-on Reset and SFDP discovery flow, and the flash command execution behavior as observed by software. PHY-layer (IP6182) wiring, DDR signal encoding, CRC computation, ECC logic, and Automotive Safety Feature (ASF) behavior are outside scope. The interface details used for communicating with the external world — flash device models, the CPU/bus fabric, and system memory — are discussed in full.

### 1.3 Acronyms

| Acronym | Expansion |
| --- | --- |
| **LT** | Loosely Timed |
| **TLM** | Transaction-Level Modeling |
| **AXI** | Advanced eXtensible Interface |
| **STIG** | Software-Triggered Instruction Group |
| **PIO** | Programmed I/O |
| **ACMD** | Advanced Command (descriptor-based DMA engine) |
| **CDMA** | Command DMA |
| **XIP** | Execute In Place |
| **PoR** | Power-on Reset |
| **SFDP** | Serial Flash Discoverable Parameters |
| **PHY** | Physical Layer |
| **DMA** | Direct Memory Access |
| **DQS** | Data Strobe Signal |
| **JEDEC** | Joint Electron Device Engineering Council |
| **JESD216A** | JEDEC standard for SFDP |
| **DDR** | Double Data Rate |
| **SDR** | Single Data Rate |
| **CS** | Chip Select |
| **WREN** | Write Enable |
| **ASF** | Automotive Safety Features |
| **ECC** | Error Correction Code |
| **CRC** | Cyclic Redundancy Check |
| **W1C** | Write-1-to-Clear |
| **RO** | Read Only |
| **RW** | Read-Write |
| **WO** | Write Only |
| **IP** | Intellectual Property block |

### 1.4 Is List

The following features and behaviors are included in the xSPI Controller TLM model.

#### 1. Operating Modes

- **Direct Mode** (`ctrl_config.work_mode` bits [6:5] = `2'b00`): AXI slave read transactions are converted to a flash READ command (READ_ZERO_LATENCY opcode 0x03) on `xspi_bus_socket`; AXI slave write transactions are converted to a WREN followed by a PAGE_PROGRAM command sequence. The active flash device is selected via `direct_access_cfg.dac_bank_num`. Address remapping via `direct_access_rmp` / `direct_access_rmp_1` is applied before the flash address is sent.
- **STIG Mode** (`ctrl_config.work_mode` bits [6:5] = `2'b01`): A write to `cmd_reg0` triggers an SC_THREAD that decodes the opcode, address, data byte count, and `INSTR_LINK` (two-phase chained instruction) from `cmd_reg0` through `cmd_reg4`. On completion, `cmd_status.COMPLETE` is set. Five STIG handlers are modeled: READ, WRITE, control (WREN/WRDI/RDSR), suspend/resume, and READ_SFDP.
- **PIO Mode** (`ctrl_config.work_mode` bits [6:5] = `2'b11`, `cmd_reg0[31:30] = 2'b01`): Up to 8 concurrent SC_THREADs (thread ID in `cmd_reg0[26:24]`) perform DMA READ or PROGRAM transfers between flash and system memory via `i_dma_socket`. Per-thread status is tracked in `TRD_STATUS` and `cmd_status`. Supported PIO commands: READ, PROGRAM, SECTOR_ERASE, CHIP_ERASE, SOFT_RESET, and JEDEC_RESET.
- **ACMD / CDMA Mode** (`ctrl_config.work_mode` bits [6:5] = `2'b11`, `cmd_reg0[31:30] = 2'b00`): Descriptor-based DMA engine. Each 64-byte-aligned descriptor is fetched from system memory via `cmd_reg2`/`cmd_reg3` (64-bit pointer) and processed through the state machine: FETCH → VALIDATE → EXECUTE → WRITEBACK_STATUS → (CONT + next → FETCH : COMPLETE). Up to 8 concurrent threads are supported. Descriptor chaining (CONT flag), interrupt-on-chain-end (INT flag), error writeback, and the MB_XIP_EN flag (valid only on READ descriptors) are all modeled.
- **XIP Mode**: Enabled per-bank via `xip_mode_cfg.xip_en`. On the next READ after enabling, the model inserts `xip_en_mb_val` as a mode byte between the address and data phases of the flash command to enter XIP state. Exit is triggered by setting `direct_access_cfg.mode_bit_xip_dis`, which causes `xip_dis_mb_val` to be inserted on the following READ. Non-READ commands while XIP is active generate `dir_cmd_err` or `DSC_ERROR`.
- **Boot Mode**: Triggered when `boot_en = 1` in `xspi_PoR_trans` and `boot_available = 1` in `ctrl_features_reg`. The controller autonomously DMA-transfers the flash boot region to system memory without CPU involvement via an SC_THREAD launched from the PoR handler. On success, `boot_comp` is asserted in the `xspi_PoR_trans` response; on failure, `boot_error` is asserted and `boot_status` (`boot_dqs_err`, `boot_crc_err`, `boot_bus_err`) is populated.

#### 2. Power-on Reset and SFDP Discovery

- PoR arrives as a single TLM write on `PoR_input_signals` carrying an `xspi_PoR_trans` extension with fields: `discovery_inhibit`, `discovery_num_lines`, `discovery_abnum`, `discovery_bank`, `discovery_cmd_type`, `discovery_dummy_cnt`, CRC configuration fields, `boot_en`, `boot_comp`, and `boot_error`.
- When `discovery_inhibit = 0`: reads the SFDP ROM from flash using the READ_SFDP opcode (0x5A) on `xspi_bus_socket[discovery_bank]`, parses the JESD216A basic parameter table (16 DWORDs), and auto-configures all 10 sequence registers (`global_seq_cfg`, `rst_seq_cfg_0/1`, `ers_seq_cfg_0/1/2`, `prog_seq_cfg_0/1/2`, `read_seq_cfg_0/1/2`, `stat_seq_cfg_0` through `stat_seq_cfg_10`, and `we_seq_cfg_0`).
- When `discovery_inhibit = 1`: the SFDP read is skipped; sequence registers retain their reset or previously programmed values; `discovery_control.discovery_comp` is set immediately without issuing any flash transaction.
- Discovery mode selection is encoded in `discovery_num_lines`: 0x0=Auto, 0x1=1-1-1 SDR, 0x2=2-2-2, 0x4=4-4-4, 0x8=8-8-8, 0xC=8-8-8 Legacy HyperFlash / xSPI Profile 2.0, 0xE=1-1-1 SPI-NAND.
- Discovery result is reported via `discovery_control.discovery_comp` (done flag) and `discovery_control.discovery_fail` (result encoding). Initialization completion is signaled via `ctrl_status.init_comp`; failure via `ctrl_status.init_fail` (0b00=xSPI/SPI-NAND detected, 0b01=failed, 0b10=Legacy SPI detected).
- Post-discovery, `discovery_control` fields reflect actual device parameters: `discovery_extop_en`, `discovery_extop_val`, `discovery_cmd_type`, `discovery_dummy_cnt`, and `discovery_abnum`.

#### 3. Flash Command Set

- **STIG command types:** READ (instr_type=1), WRITE (instr_type=2), control (WREN/WRDI/RDSR), suspend/resume, and READ_SFDP. STIG INSTR_LINK chaining uses two `cmd_reg0` writes to constitute a paired command-then-data phase; `ctrl_status.gcmd_eng_mc_busy` indicates the controller is awaiting the second write.
- **ACMD/CDMA command types:** ERASE_SECTORS (0x1000), FULL_CHIP_ERASE (0x1001), DEVICE_RESET (0x1100), JEDEC_RESET (0x1101), PROGRAM (0x2100), and READ (0x2200).
- **PIO command types:** READ, PROGRAM, SECTOR_ERASE, CHIP_ERASE, SOFT_RESET, and JEDEC_RESET.
- **Flash opcodes used:** READ_ZERO_LATENCY (0x03), READ_FAST (0x0B), READ_SFDP (0x5A), PROGRAM (0x02), PROGRAM_4BYTE (0x12), ERASE_64KB (0xD8 / 0xDC), WRITE_ENABLE (0x06), WRITE_DISABLE (0x04), READ_STATUS_REG (0x05), RESET_ENABLE (0x66), RESET (0x99), and SUSPEND/RESUME opcodes from SFDP DWORD 13.

#### 4. DMA and Data Transfer

- **AXI Master DMA (`i_dma_socket`):** used by PIO and ACMD modes to transfer data between system memory and flash. AXI burst length is governed by `dma_settings.burst_sel` (max burst = `burst_sel + 1`); outstanding transactions are controlled via `dma_settings.OTE`; the transfer word size (byte / 16-bit / 32-bit / 64-bit) is set by `dma_settings.word_size`.
- **AXI Slave DMA (SDMA):** used when the host manages data movement over the AXI slave interface in STIG/ACMD modes. Transfer size is reported in `sdma_size`; the current address is maintained in `sdma_addr0`/`sdma_addr1`; thread/direction information is in `sdma_trd_info`; the trigger condition is reported via `intr_status.sdma_trigg`.
- **ACMD descriptor chaining:** the CONT flag chains execution to `next_pointer` after completing the current descriptor; the INT flag asserts the interrupt at chain end.
- **ACMD descriptor status writeback:** a 32-bit status word (DSC_ERROR, BUS_ERROR, CRC_ERROR, DQS_ERROR, DEVICE_ERROR, ECC_CORR_ERROR, FAIL, COMPLETE) is written back to descriptor offset +40 in system memory on every EXECUTE cycle.
- **DMA error address capture:** `dma_target_error_l` / `dma_target_error_h` capture the 64-bit system memory address of the failing AXI master transaction.
- **RWDS byte masking:** `direct_access_cfg.rwds_cap_en` enables translation of AXI slave write strobes to the RWDS byte mask for octal-DDR and 16-bit-addressed devices in Direct mode.

#### 5. Address Management

- **Direct-mode address remapping:** when `direct_access_cfg.rmp_addr_en = 1`, the incoming AXI slave address is adjusted as (address − N), where N is the 64-bit value held in `direct_access_rmp` (lower 32 bits) and `direct_access_rmp_1` (upper 32 bits), before the flash address is forwarded on `xspi_bus_socket`.
- **Bank (chip-select) selection per mode:** `direct_access_cfg.dac_bank_num` selects the bank for Direct mode; `bank_num` in ACMD/PIO descriptor or command register selects the bank for ACMD/PIO modes; `discovery_bank` is used during SFDP discovery. Up to 8 banks (CS[0:7]) are supported.
- **Address width selection:** 3-byte or 4-byte addressing is selected via `discovery_abnum` after discovery, and applies to all subsequent flash transactions.

#### 6. Sequence Configuration

- All sequence register groups (`rst_seq_cfg`, `ers_seq_cfg`, `prog_seq_cfg`, `read_seq_cfg`, `stat_seq_cfg`, and `we_seq_cfg`) are written by SFDP discovery and are also directly software-programmable. The active device profile (`global_seq_cfg.seq_type`) selects which register sub-group is applied per command: Profile 1 (xSPI NOR, `seq_type=0`), Profile 2 HyperFlash (`seq_type=1`), Profile 2 HyperRAM (`seq_type=2`), and SPI NAND (`seq_type=3`).
- Page-size bounds are enforced: `global_seq_cfg.seq_page_size_rd` (encoded as 2^N, default 0xF = unlimited) bounds READ transfer sizes; `global_seq_cfg.seq_page_size_pgm` (encoded as 2^N, default 0x8 = 256 bytes) bounds PROGRAM transfer sizes in Direct, PIO, and ACMD modes.
- SPI NAND spare-area extension is modeled via `global_seq_cfg_1.nand_spare_area`; the extra byte count is added to the page data size for NAND READ PAGE and PROGRAM PAGE operations in PIO and ACMD modes.
- CS# maximum active time enforcement (`tCMS`) is activated when `global_seq_cfg.seq_tcms_en = 1`; the limit is taken from `dev_active_max_reg`.

#### 7. Interrupt and Status Reporting

- **Thread completion interrupt (TRD_COMP_INT):** one bit per thread in `trd_comp_intr_status`; set when a PIO or ACMD thread completes and the INT flag was set in the command or descriptor; W1C; gated by `trd_comp_intr_en` before driving `interrupt_out`.
- **Thread error interrupt (TRD_ERR_INT):** one bit per thread in `trd_error_intr_status`; set on thread error; W1C; gated by `trd_error_intr_en` before driving `interrupt_out`.
- **`interrupt_out` / `int_out` signal:** driven high when either `trd_comp_intr_status` or `trd_error_intr_status` is non-zero after masking by the respective enable registers; active-high.
- **`intr_status` general interrupt register (W1C):** key bits include `ctrl_idle` (controller returned to idle), `cdma_terr` (AXI master bus error on descriptor fetch/writeback), `ddma_terr` (AXI master bus error on data DMA), `cmd_ignored` (write to `cmd_reg0` targeting a busy thread), `sdma_trigg` (slave DMA trigger met), `sdma_err` (illegal AXI slave access in STIG/ACMD mode), `stig_done` (last instruction in a STIG chained sequence completed), `dir_crc_err`, `dir_dqs_err`, `dir_cmd_err`, `dir_ecc_corr_err`, `dir_dev_err`, and `gp_open_drain_0` through `gp_open_drain_3`.
- **`intr_enable` register:** per-bit enable mask; the rising edge of an enabled `intr_status` bit drives the interrupt line.
- **Controller status (`ctrl_status`):** `ctrl_busy`, `acmd_eng_busy`, `gcmd_eng_busy`, `gcmd_eng_mc_busy` (STIG chain waiting for second write), `mdma_busy`, `sdma_busy`, `discovery_busy`, `init_comp`, and `init_fail`.
- **Thread status (`trd_status`):** one `trd_busy` bit per ACMD/PIO thread; cleared when the thread completes.
- **STIG command status (`cmd_status`):** completion and error encoding for STIG; thread selected for ACMD/PIO reporting via `cmd_status_ptr.thrd_status_sel` (indirect two-register access pattern).
- **Boot status (`boot_status`):** `boot_dqs_err`, `boot_crc_err`, `boot_bus_err`; populated on boot failure; readable by software.

#### 8. Device Reset Control

- **Software-controlled hardware reset:** `reset_pin_settings.sw_ctrled_hw_rst` is driven to RESET# or DQ3 (selected by `sw_ctrled_hw_rst_option`); per-bank enable bits `sw_ctrled_hw_rst_bank0` through `sw_ctrled_hw_rst_bank7`; `rst_dq3_enable` gates the DQ3 direction; all are software-visible control fields.
- **JEDEC reset timing:** `jedec_rst_timing_reg.tCSH_delay` and `tCSL_delay` are software-programmed values stored and passed as timing hints; actual pin timing is not enforced in the LT model.

#### 9. Write Protection

- **Write-protect control:** `wp_settings.wp` is driven to DQ2 (Write Protect pin) when `wp_settings.wp_enable = 1`; effective only in single/dual SPI modes; stored and observable by software. The WP state is passed via the `cdns_extension` WP field on flash transactions.

#### 10. Legacy SPI Clock Mode

- **CPOL/CPHA selection:** `clock_mode_settings.spi_clock_mode` selects SPI Mode 0 (CPOL=0, CPHA=0) or Mode 3 (CPOL=1, CPHA=1) for legacy SDR transfers; stored and used to condition the `cdns_extension` clock-edge encoding for flash command generation.

#### 11. TLM Interfaces

- **`t_reg_socket` (target):** scml2 LT register-file target socket; routes to eight internal register sub-regions via `t_reg_socket_router`.
- **`t_axi_slave_socket` (target):** accepts 64-bit AXI register/memory accesses from CPU; routes to the register model or Direct-mode flash forwarding.
- **`PoR_input_signals` (target):** receives `xspi_PoR_trans` TLM extension carrying bootstrap, discovery, and boot configuration; triggers PoR handling and SFDP discovery.
- **`xspi_bus_socket[N]` (initiator):** sends `cdns_extension` TLM payloads (opcode, bank/CS, address, data, flags) to flash device models; N corresponds to `NUM_TARGETS` (1, 2, 4, or 8).
- **`i_dma_socket` (initiator):** 64-bit AXI master socket used for PIO and ACMD data DMA transfers and ACMD descriptor fetch/writeback.
- **`int_out` / `interrupt_out` (signal out):** driven high on active TRD_COMP_INT or TRD_ERR_INT condition after enable-register masking.
- **`reset_in` (signal in):** active-low reset; resets all registers to their reset values and aborts all in-progress operations.

#### 12. Hardware Feature Identification

- **`ctrl_features_reg`:** read-only register reporting `n_threads` (encoded 1/2/4/8 threads), `n_banks` (encoded 1/2/4/8 banks), `asf_available`, `boot_available`, `dma_addr_width`, and `dma_data_width`; software reads this to determine IP configuration. Initialized from `scml_property` values at elaboration; write-ignore restriction applied.
- **`xspi_ctrl_version`:** read-only fields `xspi_ctrl_magic_number` (0x6522), `xspi_ctrl_rev`, and `xspi_ctrl_fix`; software-visible identification. Write-ignore restriction applied.

#### 13. Polling Configuration

- `long_polling` register: software-programmed wait count before re-polling the flash device status register after detecting it busy; reset value = 1000 (0x3E8).
- `short_polling` register: minimum inter-poll interval; reset value = 500 (0x1F4).
- Both are stored and passed to the model's polling parameters; no active polling loop executes in the LT model.

---

### 1.5 Is Not List

The following features are out of scope for the xSPI Controller TLM model.

#### 1. Cycle-Accurate Timing and DDR Framing

- No clock period, no DDR timing parameters (tCSH, tCSL, cssot, cseot, csda, rst_recovery), no DLL calibration sequence, no setup/hold constraints are modeled.
- `b_transport` is used exclusively; no `nb_transport` / AT-level protocol is implemented. All flash READ, PROGRAM, ERASE, and status-poll sequences complete within the same `b_transport` call with zero simulated time advancement.
- DDR wire framing, DQS strobe generation, DQ bus turnaround, and PHY timing alignment are not modeled.

#### 2. PHY Layer (IP6182)

- All PHY registers (`phy_dq_timing_reg`, `phy_dqs_timing_reg`, `phy_dll_master_ctrl_reg`, `phy_dll_slave_ctrl_reg`, `phy_gate_lpbk_ctrl_reg`, `phy_ie_timing_reg`, `phy_static_togg_reg`, `phy_ctrl_reg`, `phy_tsel_reg`, `phy_gpio_ctrl_0`, `phy_gpio_ctrl_1`, `phy_wr_deskew_pd_ctrl_0_reg`) are modeled as read/write register storage only; no PHY timing or pin-level behavior is simulated.
- PHY observation registers (`phy_obs_reg_0`, `phy_dll_obs_reg_0`, `phy_dll_obs_reg_1`) return reset state only; DLL lock status is not simulated.
- PHY resynchronization (`dll_phy_update_cnt`, `dll_phy_ctrl`) is stored; no effect in the LT model.

#### 3. CRC Computation

- Actual CRC calculation over SFDP data, flash read data, or program data is not performed. CRC configuration fields (`discovery_seq_crc_en`, `seq_crc_en`, `seq_crc_variant`, `seq_crc_oe`, `seq_crc_chunk_size`) are accepted and stored. `dir_crc_err` and descriptor `CRC_ERROR` bits will not be generated from data content in the LT model.

#### 4. ECC

- ECC correction and detection logic is not modeled. `ECC_CORR_ERROR` and `ECC_STAT` fields in the descriptor status are defined in the register model but ECC computation is not performed.

#### 5. PHY Loopback Test Mode

- PHY loopback control (`phy_gate_lpbk_ctrl_reg.lpbk_en`, `lpbk_internal`, `loopback_control`, `lpbk_fail_muxsel`, `lpbk_err_check_timing`) is intended for PHY characterization only; stored as register memory with no behavioral effect.

#### 6. ASF (Automotive Safety Features)

- ASF fault injection, parity checking, and error reporting behavior are not modeled. `ctrl_features_reg.asf_available` is correctly initialized so software reads the correct capability flag, but no ASF functional behavior is implemented.

#### 7. DLL Bypass Debug Mode

- `phy_dll_master_ctrl_reg.param_dll_bypass_mode` is stored as register memory only; not modeled functionally.

#### 8. ONFI / SD-eMMC PHY Feature Flags

- `phy_features_reg` fields (`onfi_40`, `onfi_41`, `sdr_16bit`, `xspi`, `sd_emmc`, `bank_num`, `dll_tap_num`, `aging`, `dfi_clock_ratio`, `per_bit_deskew`) are hardware capability flags; read-only; no behavioral effect in the TLM model.

#### 9. Per-Bit Write Deskew

- `phy_wr_deskew_pd_ctrl_0_reg` (phase detect block for DQ write path) is stored as register memory only; no behavioral effect in the LT model.

#### 10. GP Open-Drain Input Pin Transitions

- Physical transitions on `xspi_dfi_gp_open_drain[0:3]` pins are not modeled. The `gp_open_drain_0` through `gp_open_drain_3` interrupt status bits in `intr_status` remain zero in the LT model unless explicitly driven by the test environment.

---

Pre-emption is not supported in the xSPI Controller model due to the blocking transport nature of the TLM LT model. The model is not timing-accurate and is therefore not suitable for performance measurements or DDR signal-integrity analysis.

---

## 2. Functional Description

### 2.1 xSPI Controller Configuration Parameters

These are the build-time (instantiation-time) configuration parameters for the xSPI Controller TLM model. Each parameter is a static hardware attribute that determines the structural shape or capability set of a specific IP variant. They are exposed as read-only fields in `ctrl_features_reg` (hardware reports its own configuration to software) and as the structural `scml_property<int> NUM_TARGETS` parameter that sizes the `xspi_bus_socket` array.

None of these parameters can be changed by register writes at runtime. The register model enforces `set_write_ignore_restriction` on all `ctrl_features_reg` fields; any attempt to write them is silently ignored.

| Parameter Name | Type | Default / Valid Range | Description |
| --- | --- | --- | --- |
| `NUM_TARGETS` | `int` (scml_property) | Default: 1; valid: 1, 2, 4, 8 | Sizes the `xspi_bus_socket` vector at elaboration time. The number of TLM initiator sockets bound to flash device models equals the number of physical CS lines. Cannot be changed after elaboration. Must match `n_banks`. |
| `n_threads` | `uint` (4-bit encoded) | Default: 3 (= 8 threads); valid: 0=1, 1=2, 2=4, 3=8 | Determines how many concurrent PIO/ACMD execution threads the model instantiates. Controls the size of the thread-state arrays and the active width of `TRD_STATUS`, `trd_comp_intr_status`, and `trd_error_intr_status` bitmaps that software reads. Stored in `ctrl_features_reg[3:0]` (read-only to software). |
| `n_banks` | `uint` (4-bit encoded) | Default: 3 (= 8 banks); valid: 0=1, 1=2, 2=4, 3=8 | Maximum number of flash CS lines (banks) physically wired. Governs valid `bank_num` / `dac_bank_num` values accepted in commands and the upper bound of `discovery_bank` (3-bit, 0–7). Stored in `ctrl_features_reg` (read-only). Must equal `NUM_TARGETS`. The `NUM_TARGETS` scml_property defaults to 1 at the property level while the register default-bin encodes 8 banks (value 3); both representations must be kept consistent at instantiation. |
| `boot_available` | `bool` (1-bit) | Default: 1 (boot engine present); valid: 0, 1 | Indicates whether the Automated Boot Engine block is synthesized into this variant. When 0, the boot engine SC_THREAD must not be instantiated and `boot_en` arriving via `xspi_PoR_trans` is treated as a no-op. When 1, the model launches the boot DMA sequence on `boot_en=1`. Stored in `ctrl_features_reg` (read-only). |
| `asf_available` | `bool` (1-bit) | Default: 0 (ASF not present); valid: 0, 1 | Indicates whether Automotive Safety Feature (ASF) hardware is synthesized into this variant. The TLM model does not simulate ASF behavior, but `ctrl_features_reg.asf_available` must be initialized correctly so that software reads the correct capability report. |
| `dma_addr_width` | `uint` (1-bit) | Default: 1 (64-bit); valid: 0=32-bit, 1=64-bit | Controls the address pointer width for both `i_dma_socket` (AXI master) and the AXI slave DMA interfaces. When 0, all system memory pointers are 32-bit; when 1, they are 64-bit. The reference socket is declared 64-bit wide; the parameter controls the software-visible capability report in `ctrl_features_reg`. |
| `dma_data_width` | `uint` (1-bit) | Default: 1 (64-bit); valid: 0=32-bit, 1=64-bit | Controls the data bus width for AXI master and slave DMA interfaces. Affects the maximum valid `dma_settings.word_size`: when 0 (32-bit), a 64-bit `word_size` selection is illegal. Stored in `ctrl_features_reg` (read-only). |

**Note:** The following candidates were considered but excluded as configuration parameters. `dma_intf` is always 0 (AXI4 only, no variant). `sfr_intf` has no TLM behavioral difference. Version identification fields (`xspi_ctrl_rev`, `xspi_ctrl_fix`, `xspi_ctrl_magic_number`) are fixed silicon constants, not variant-selection knobs. Runtime register fields and PHY feature flags carry no build-time variant meaning.

---

### 2.2 Port Interfaces

The xSPI Controller TLM model exposes six distinct interface categories. The register bus provides software-visible access to the complete register map. The AXI slave data interface handles Direct-mode memory-mapped flash forwarding. The PoR bootstrap socket delivers power-on reset and SFDP discovery configuration. The xSPI flash bus socket array drives flash device models. The AXI master DMA socket issues PIO and ACMD data transfers to system memory. Hardware signal ports carry reset, interrupt, and boot engine completion notifications.

All sockets use a 64-bit address and data bus width, matching `dma_addr_width = 1` (64-bit) and `dma_data_width = 1` (64-bit) default configuration parameters reported by `ctrl_features_reg`.

#### Port Interface Table

| Interface Category | Port Name | Port Type | Description |
| --- | --- | --- | --- |
| **Register Bus** | `t_reg_socket` | `scml2::ft_target_socket<32>` | scml2 LT register-file target socket. Routes to eight internal register sub-regions via `t_reg_socket_router`: `ctrl_cmd_stat_a` (0x000), `ctrl_cfg_common_a` (0x200), `cmn_seq_regs_a` (0x380), `dev_seq_regs_a` (0x400), `ctrl_consts_a` (0xF00), `rf_minictrl_regs_a` (0x1000), `dataslice_Rfile_a` (0x2000), and `ctb_Rfile_a` (0x2080). Protocol engine set to `scml2::LT`. |
| **AXI Slave Data** | `t_axi_slave_socket` | `tlm_utils::simple_target_socket<xspi_ctrl, 64>` | 64-bit TLM-2.0 target socket. Accepts AXI register-access and Direct-mode memory transactions from the CPU/bus fabric. In Direct mode, read transactions map to flash READ commands; write transactions map to a WREN followed by PAGE_PROGRAM. Non-Direct-mode accesses route to the register model. Carries an optional `axi_trans` TLM extension encoding AXI4 burst, lock, cache, protection, QoS, and strobe fields. |
| **PoR Bootstrap** | `PoR_input_signals` | `tlm_utils::simple_target_socket<xspi_ctrl, 64>` | 64-bit TLM-2.0 target socket. Receives a single TLM write carrying the `xspi_PoR_trans` extension at power-on reset. Triggers SFDP discovery (when `discovery_inhibit = 0`) and the automated boot sequence (when `boot_en = 1` and `boot_available = 1`). Handled by `b_transport_por_input()`. |
| **xSPI Flash Bus** | `xspi_bus_socket[NUM_TARGETS]` | `scml2::vector<tlm_utils::simple_initiator_socket<xspi_ctrl, 64>>` | Array of `NUM_TARGETS` (1, 2, 4, or 8) 64-bit TLM-2.0 initiator sockets; one socket per physical chip-select (CS[0:7]). Each transaction carries a `cdns_extension` TLM extension encoding the xSPI bus operation (opcode, bank/CS, address, data, flags). Used by all operating modes: Direct, STIG, PIO, ACMD, SFDP discovery, and XIP mode-byte insertion. Array sized at elaboration time by `scml_property<int> NUM_TARGETS`. |
| **AXI Master DMA** | `i_dma_socket` | `tlm_utils::simple_initiator_socket<xspi_ctrl, 64>` | 64-bit TLM-2.0 initiator socket. Issues AXI master read and write transactions to system memory. Used by PIO mode (READ: flash-to-memory; PROGRAM: memory-to-flash) and ACMD mode (descriptor fetch from `cmd_reg2/cmd_reg3` pointer, data transfer, and 32-bit status writeback at descriptor offset +40). Carries an optional `axi_trans` extension for AXI4 burst attributes. Maximum AXI burst length controlled by `dma_settings.burst_sel`; outstanding transaction enable via `dma_settings.OTE`; word size via `dma_settings.word_size`. |
| **Reset Signal** | `reset_in` | `sc_core::sc_in<bool>` | Active-low synchronous reset input. When asserted (logic false), resets all registers to their defined reset values, clears all in-progress STIG, PIO, and ACMD thread states, de-asserts `int_out`, and brings the controller to idle. Managed by `reset_in_protocol_engine` (`scml2::reset_slave_engine`, `active_level = false`). |
| **Interrupt Output** | `int_out` | `sc_core::sc_out<bool>` | Active-high interrupt output (preferred name per guide: `interrupt_out`). Driven high when either `trd_comp_intr_status` (any `trdN_comp` bit set, gated by `trd_comp_intr_en`) or `trd_error_intr_status` (any `trdN_error_stat` bit set, gated by `trd_error_intr_en`) is non-zero after masking. Managed by `int_out_protocol_engine` (`scml2::interrupt_master_engine`, `active_level = false`). |

**Note:** `boot_comp` and `boot_error` are output fields of the `xspi_PoR_trans` extension (written back by the model after PoR handling), not standalone TLM signal ports. The `ctrl_busy` hardware output pin is abstracted in the LT model and is only observable via the `ctrl_status` register.

#### xspi_PoR_trans Extension Key Fields

| Field | Type | Direction | Description |
| --- | --- | --- | --- |
| `discovery_inhibit` | `uint8_t` | Input | When set, skips SFDP ROM read; sequence registers retain their values. |
| `discovery_num_lines` | `uint8_t` | Input | 4-bit bootstrap specifying initial protocol mode for SFDP discovery (0x0=Auto, 0x1=1-1-1 SDR, 0x2=2-2-2, 0x4=4-4-4, 0x8=8-8-8, 0xC=HyperFlash/Profile-2, 0xE=SPI-NAND). |
| `discovery_abnum` | `uint8_t` | Input | Initial addressing mode: 0=3-byte, 1=4-byte. |
| `discovery_bank` | `uint8_t` | Input | 3-bit bank selection (0x0–0x7) for the SFDP discovery READ_SFDP transaction. |
| `discovery_cmd_type` | `uint8_t` | Input | 2-bit command mode: 0=DDR disabled, 1=DDR enabled, 2=DTR enabled (QUAD only). |
| `discovery_dummy_cnt` | `uint8_t` | Input | Dummy clock count for SFDP read: 0=8 cycles, 1=20 cycles. |
| `discovery_extop_en` | `uint8_t` | Input | Extended opcode enable for SFDP discovery: 0=disabled, 1=enabled. |
| `discovery_extop_val` | `uint8_t` | Input | Extended opcode value: 0=repetition mode (0x5A 0x5A), 1=negation mode (0x5A 0xA5). |
| `discovery_seq_crc_en` | `uint8_t` | Input | CRC enable for SFDP discovery sequence; stored; not computed in LT model. |
| `discovery_seq_crc_variant` | `uint8_t` | Input | CRC variant (address-phase only vs. full sequence); stored; not computed. |
| `discovery_seq_crc_oe` | `uint8_t` | Input | DDR CRC toggle enable; stored; not computed. |
| `discovery_seq_crc_chunk_size` | `uint8_t` | Input | 3-bit CRC chunk size; stored; not computed. |
| `init_rb_valid_time` | `uint32_t` | Input | Time from PoR when the device becomes accessible; accepted and passed through; no timing enforcement in LT model. |
| `boot_en` | `uint8_t` | Input | Enables automated boot sequence on PoR; active only when `boot_available = 1`. |
| `boot_comp` | `uint8_t` | Output | Set by the model on successful completion of the automated boot DMA sequence. |
| `boot_error` | `uint8_t` | Output | Set by the model when the boot sequence is interrupted by an error. |

#### Register Sub-Region Routing (t_reg_socket)

Transactions received on `t_reg_socket` are dispatched through `t_reg_socket_router` (8 mapping entries) to the following sub-regions:

| Mapping Index | Base Address | Register File | Contents |
| --- | --- | --- | --- |
| 3 | 0x000 | `ctrl_cmd_stat_a` | Command registers (`cmd_reg0–5`), `cmd_status_ptr`, `cmd_status`, `ctrl_status`, `trd_status`, `intr_status`, `intr_enable`, `trd_comp_intr_status`, `trd_error_intr_status`, `trd_error_intr_en`, `dma_target_error_l/h`, `boot_status` |
| 2 | 0x200 | `ctrl_cfg_common_a` | `long_polling`, `short_polling`, `ctrl_config`, `dma_settings`, `sdma_size`, `sdma_trd_info`, `sdma_addr0/1`, `discovery_control`, `xip_mode_cfg`, `direct_access_cfg`, `direct_access_rmp/1` |
| 0 | 0x380 | `cmn_seq_regs_a` | `global_seq_cfg`, `global_seq_cfg_1`, `rst_seq_cfg_0/1`, `ers_seq_cfg_0/1/2`, `prog_seq_cfg_0/1/2`, `read_seq_cfg_0/1/2`, `stat_seq_cfg_0–10`, `we_seq_cfg_0` |
| 6 | 0x400 | `dev_seq_regs_a` | Per-device sequence registers (device-specific timing/sequence overrides) |
| 4 | 0xF00 | `ctrl_consts_a` | Read-only constants: `xspi_ctrl_version`, `ctrl_features_reg` |
| 7 | 0x1000 | `rf_minictrl_regs_a` | `wp_settings`, `reset_pin_settings`, `clock_mode_settings`, `jedec_rst_timing_reg`, `dev_delay_reg`, `rst_recovery_reg`, `dev_active_max_reg`, `hf_offset_reg`, `dll_phy_update_cnt`, `dll_phy_ctrl` |
| 5 | 0x2000 | `dataslice_Rfile_a` | PHY data-slice registers: `phy_dq_timing_reg`, `phy_dqs_timing_reg`, `phy_dll_master_ctrl_reg`, `phy_dll_slave_ctrl_reg`, `phy_ie_timing_reg`, `phy_obs_reg_0`, `phy_dll_obs_reg_0/1`, `phy_static_togg_reg`, `phy_wr_deskew_pd_ctrl_0_reg`, `phy_version_reg`, `phy_features_reg` (all store-only; no behavioral effect) |
| 1 | 0x2080 | `ctb_Rfile_a` | PHY control-top registers: `phy_ctrl_reg`, `phy_tsel_reg`, `phy_gpio_ctrl_0/1`, `phy_gpio_status_0/1` (store-only; no behavioral effect) |

---

### 2.3 Memory-Mapped Registers

The xSPI Controller's registers are accessed through the peripheral bus at the module's base address. All configuration, status, and data transfer is handled via these registers. The register space is divided into eight logically grouped sub-regions routed through an internal scml2 address router.

**Address Region Map:**

| Region Name | Base Offset | Size | Description |
| --- | --- | --- | --- |
| `ctrl_cmd_stat_a` | `0x000` | 0x15C bytes | Command, status, and interrupt registers |
| `ctrl_cfg_common_a` | `0x200` | 0x64 bytes | Controller configuration, DMA settings, SDMA, discovery |
| `cmn_seq_regs_a` | `0x380` | 0x24 bytes | Common sequence configuration (XIP, global seq, direct access) |
| `dev_seq_regs_a` | `0x400` | 0x7C bytes | Per-device sequence configuration (reset, erase, program, read, status, WE) |
| `ctrl_consts_a` | `0xF00` | 0x08 bytes | Read-only hardware identification registers |
| `rf_minictrl_regs_a` | `0x1000` | 0x38 bytes | Write protect, reset pin, clock mode, timing, PHY resync |
| `dataslice_Rfile_a` | `0x2000` | 0x78 bytes | PHY data-slice registers (store-only; no behavioral effect) |
| `ctb_Rfile_a` | `0x2080` | 0x18 bytes | PHY control block registers (store-only; no behavioral effect) |

#### Group 1: Command and Status Registers (`ctrl_cmd_stat_a`, base = 0x000)

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `cmd_reg0` | `0x000` | 32 | WO (trigger) | `0x00000000` | Command Register 0. Writing this register triggers flash command execution in STIG, PIO, and ACMD/CDMA work modes. Encoding varies by work mode: STIG (opcode + address + data byte count + INSTR_LINK); PIO (thread ID [26:24], cmd_type [15:0], mode bits [31:30]=0b01); ACMD (thread ID [26:24], mode bits [31:30]=0b00). |
| `cmd_reg1` | `0x004` | 32 | RW | `0x00000000` | Command Register 1. STIG/PIO: holds xSPI flash address lower 32 bits. ACMD: xSPI Address Lower. |
| `cmd_reg2` | `0x008` | 32 | RW | `0x00000000` | Command Register 2. STIG: system memory pointer [31:0]. ACMD: 64-bit descriptor address LSB (lower 32 bits). |
| `cmd_reg3` | `0x00C` | 32 | RW | `0x00000000` | Command Register 3. ACMD: 64-bit descriptor address MSB (upper 32 bits). STIG: upper address bits. |
| `cmd_reg4` | `0x010` | 32 | RW | `0x00000000` | Command Register 4. PIO: DATA_CNT (byte count for READ/PROGRAM) or SECT_CNT (sector count for ERASE). STIG: data byte count. |
| `cmd_reg5` | `0x014` | 32 | RW | `0x00000000` | Command Register 5. PIO mode only: xSPI Address Upper [63:32] for 64-bit flash addressing. Not used in STIG or ACMD modes. |

#### Mode-Dependent Bitfield Layouts for `cmd_reg0`–`cmd_reg5`

The six command registers `cmd_reg0` through `cmd_reg5` (offsets 0x000–0x014) share a single fixed address space but encode fundamentally different bitfield meanings depending on `ctrl_config.work_mode` bits [6:5] and, when that field is `2'b11`, on `cmd_reg0[31:30]` (PIO vs CDMA). The Register Reference Manual explicitly notes for all six registers that "the definition changes depending on the workmode." Software must configure the correct work mode via `ctrl_config` before staging values into these registers and triggering execution via `cmd_reg0`.

**Register-to-mode assignment summary:**

| Register | Offset | STIG (`2'b01`) | PIO (`2'b11`, `cmd_reg0[31:30]=2'b01`) | CDMA (`2'b11`, `cmd_reg0[31:30]=2'b00`) |
| --- | --- | --- | --- | --- |
| `cmd_reg0` | `0x000` | Write-only trigger; value is ignored; STIG engine consumes instruction from `cmd_reg1`–`cmd_reg4` | [31:30]=0b01 (PIO select); [26:24]=TRD_NUM; [22:20]=BANK/CS; [19]=DMA_SEL; [18]=INT; [17]=MB_XIP_DIS; [16]=MB_XIP_EN; [15:0]=CMD_TYPE | [31:30]=0b00 (ACMD select); [26:24]=TRD_NUM; [23:0] reserved |
| `cmd_reg1` | `0x004` | STIG instruction bits[31:0] — flash opcode, I/O width, edge mode, INSTR_TYPE | xSPI Address Lower [31:0] — combined with `cmd_reg5` to form 64-bit flash address | Not consumed (ACMD uses descriptor in `cmd_reg2`/`cmd_reg3`) |
| `cmd_reg2` | `0x008` | STIG instruction bits[63:32] — address continuation, address count, dummy count | SYS_ADDR_PTR_L — lower 32 bits of system memory DMA pointer (READ/PROGRAM only) | Descriptor address bits[31:0] — 64-byte-aligned head of descriptor chain |
| `cmd_reg3` | `0x00C` | STIG instruction bits[95:64] — remaining address bits and data byte count | SYS_ADDR_PTR_H — upper 32 bits of system memory DMA pointer (READ/PROGRAM only) | Descriptor address bits[63:32] — upper word of descriptor chain head address |
| `cmd_reg4` | `0x010` | STIG instruction bits[127:96] — includes INSTR_LINK (bit 28), INSTR_TYPE (bits[6:0]), write data byte | PIO SECTOR_ERASE: SECT_CNT (sector count = field+1); PIO READ/PROGRAM: DATA_CNT (byte count = field+1); PIO CHIP_ERASE/RESET: not consumed | Not consumed (count comes from descriptor Command Counter field[63:48]) |
| `cmd_reg5` | `0x014` | Not used — STIG 128-bit instruction is fully encoded in `cmd_reg1`–`cmd_reg4` | xSPI Address Upper [63:32] — upper 32 bits of 64-bit flash address; required for SECTOR_ERASE, READ, PROGRAM; not required for CHIP_ERASE/RESET | Not used — ACMD flash address is taken from xSPI Pointer field inside the fetched descriptor |

**Key behavioral rules derived from the mode-dependent layout:**

- `cmd_reg0` in STIG mode is a pure write-only trigger; any value written is discarded. The STIG engine reads the instruction exclusively from the staged values in `cmd_reg1`–`cmd_reg4`.
- `cmd_reg4` has a secondary sub-mode dependency within PIO: the same 32-bit field means SECT_CNT (number of sectors to erase, offset by 1) for SECTOR_ERASE commands, and DATA_CNT (number of bytes to transfer, offset by 1) for READ and PROGRAM commands. The PIO engine determines which interpretation to apply by reading CMD_TYPE from the snapshotted `cmd_reg0` value at dispatch time.
- In ACMD mode only `cmd_reg0` (TRD_NUM), `cmd_reg2` (descriptor address LSB), and `cmd_reg3` (descriptor address MSB) are consumed at trigger time. `cmd_reg1`, `cmd_reg4`, and `cmd_reg5` are ignored by the ACMD engine.
- Software must stage all required registers before writing `cmd_reg0`, as the write to `cmd_reg0` atomically snapshots and dispatches the staged values.

**Remaining Group 1 registers (not mode-dependent):**

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `cmd_status_ptr` | `0x040` | 32 | RW | `0x00000000` | Command Status Pointer. ACMD mode: write thread ID (0–7) to `thrd_status_sel[2:0]` to select which thread's status is returned on the next read of `cmd_status`. |
| `cmd_status` | `0x044` | 32 | RO | `0x00000000` | Command Status Register. Reports STIG completion/error status (COMPLETE, error encoding), or the selected ACMD/PIO thread status (DSC_ERROR, BUS_ERROR, CRC_ERROR, DQS_ERROR, DEVICE_ERROR, ECC_CORR_ERROR, FAIL, COMPLETE). Write-ignore restriction. |
| `ctrl_status` | `0x100` | 32 | RO | `0x00000000` | General Controller Status. Key fields: `ctrl_busy`, `acmd_eng_busy`, `gcmd_eng_busy`, `gcmd_eng_mc_busy`, `mdma_busy`, `sdma_busy`, `discovery_busy`, `init_comp`, `init_fail[1:0]`. Write-ignore restriction. |
| `trd_status` | `0x104` | 32 | RO | `0x00000000` | ACMD/PIO Thread Status. `trd_busy[N]` — one bit per thread (bit N=1 means thread N is active); up to 8 bits [7:0] used. Write-ignore restriction. |
| `intr_status` | `0x110` | 32 | W1C | `0x00000000` | General Interrupt Status Register. All active bits are W1C. Key bits: `ctrl_idle`, `cdma_terr`, `ddma_terr`, `cmd_ignored`, `sdma_trigg`, `sdma_err`, `stig_done`, `dir_crc_err`, `dir_dqs_err`, `dir_cmd_err`, `dir_ecc_corr_err`, `dir_dev_err`, `gp_open_drain_0–3`. |
| `intr_enable` | `0x114` | 32 | RW | `0x00000000` | General Interrupt Enable. Per-bit enable mask corresponding to `intr_status` bits. Rising edge of an enabled bit drives the interrupt output. |
| `trd_comp_intr_status` | `0x120` | 32 | W1C | `0x00000000` | Thread Completion Interrupt Status. One bit per ACMD/PIO thread (bits [7:0]). Set when a thread completes with the INT flag set. W1C. Gated by `trd_comp_intr_en` before driving `interrupt_out`. |
| `trd_error_intr_status` | `0x130` | 32 | W1C | `0x00000000` | Thread Error Interrupt Status. One bit per ACMD/PIO thread (bits [7:0]). Set on thread error. W1C. |
| `trd_error_intr_en` | `0x134` | 32 | RW | `0x00000000` | Thread Error Interrupt Enable. Per-bit enable mask for `trd_error_intr_status`. Rising edge of an enabled error bit drives `interrupt_out`. |
| `dma_target_error_l` | `0x150` | 32 | RO | `0x00000000` | DMA Target Error Address Lower [31:0]. Captures lower 32 bits of the AXI master address that caused `cdma_terr` or `ddma_terr`. Write-ignore restriction. |
| `dma_target_error_h` | `0x154` | 32 | RO | `0x00000000` | DMA Target Error Address Upper [63:32]. Captures upper 32 bits of the failing AXI master address. Write-ignore restriction. |
| `boot_status` | `0x158` | 32 | RO | `0x00000000` | Boot Status Register. Key bits: `boot_dqs_err`, `boot_crc_err`, `boot_bus_err`. Populated on boot failure. Write-ignore restriction. |

#### Group 2: Controller Configuration Registers (`ctrl_cfg_common_a`, base = 0x200)

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `long_polling` | `0x208` | 32 | RW | `0x000003E8` | Long Polling Wait Count. Wait count before re-polling flash device status after detecting it busy. Reset value = 1000 (0x3E8). |
| `short_polling` | `0x20C` | 32 | RW | `0x000001F4` | Short Polling Cycle Count. Minimum inter-poll interval in system clocks. Reset value = 500 (0x1F4). |
| `ctrl_config` | `0x230` | 32 | RW | `0x00000000` | Device Control Register. Key field: `work_mode` bits [6:5] — `2'b00`=DIRECT, `2'b01`=STIG, `2'b10` reserved, `2'b11`=ACMD (PIO when `cmd_reg0[31:30]=2'b01`, CDMA when `2'b00`). `cont_on_err` — when set, ACMD/PIO thread continues past page/sector boundary on error. |
| `dma_settings` | `0x23C` | 32 | RW | `0x000D0000` | AXI Interface Settings. Key fields: `burst_sel` (max AXI burst length = `burst_sel + 1`), `OTE` (outstanding transaction enable), `word_size` (byte / 16-bit / 32-bit / 64-bit), `sdma_err_rsp` (AXI slave error response type). |
| `sdma_size` | `0x240` | 32 | RO (hw-updated) | `0x00000000` | Slave DMA Transfer Size. Updated by hardware during STIG/ACMD SDMA transfers. Write-ignore restriction. |
| `sdma_trd_info` | `0x244` | 32 | RO (hw-updated) | `0x00000000` | Slave DMA Thread Information. Fields: `sdma_trd` (thread ID), `sdma_dir` (direction). Write-ignore restriction. |
| `sdma_addr0` | `0x24C` | 32 | RO (hw-updated) | `0x00000000` | Slave DMA Current Address Lower [31:0]. Write-ignore restriction. |
| `sdma_addr1` | `0x250` | 32 | RO (hw-updated) | `0x00000000` | Slave DMA Current Address Upper [63:32]. Write-ignore restriction. |
| `discovery_control` | `0x260` | 32 | Mixed | `0x00000000` | Discovery Control / Status. Software-writable fields: `discovery_inhibit`, `discovery_num_lines`, `discovery_abnum`, `discovery_bank`, `discovery_cmd_type`, `discovery_dummy_cnt`, `discovery_extop_en`, `discovery_extop_val`. Hardware-set (write-ignore): `discovery_comp`, `discovery_fail`. |

#### Group 3: Common Sequence Configuration Registers (`cmn_seq_regs_a`, base = 0x380)

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `xip_mode_cfg` | `0x388` | 32 | RW | `0x0000FF00` | XIP Mode Configuration. Key fields: `xip_en` (per-bank XIP enable); `xip_en_mb_val` (mode byte inserted on XIP entry READ); `xip_dis_mb_val[15:8]` (default=0xFF; mode byte inserted on XIP exit READ). |
| `global_seq_cfg` | `0x390` | 32 | RW | `0x0000208F` | Global Sequence Configuration. Key fields: `seq_type` (device profile: 0=Profile-1 xSPI/NOR, 1=Profile-2 HyperFlash, 2=Profile-2 HyperRAM, 3=SPI NAND); `seq_page_size_rd` (2^N, default 0xF); `seq_page_size_pgm` (2^N, default 0x8); CRC config fields; `seq_data_swap`; `seq_data_per_addr`; `seq_tcms_en`. Auto-populated by SFDP discovery. |
| `global_seq_cfg_1` | `0x394` | 32 | RW | `0x00000000` | Global Sequence Configuration 1. Key field: `nand_spare_area` (extends page data size for SPI NAND). Auto-populated by SFDP discovery. |
| `direct_access_cfg` | `0x398` | 32 | RW | `0x00000000` | Direct Access Configuration. Key fields: `dac_bank_num[2:0]` (chip-select for Direct mode, 0–7); `rmp_addr_en` (enable address remapping); `mode_bit_xip_dis` (insert XIP exit mode byte on next Direct-mode READ); `rwds_cap_en` (AXI write-strobe to RWDS byte-mask translation). |
| `direct_access_rmp` | `0x39C` | 32 | RW | `0x00000000` | Direct Access Address Remap Register Lower [31:0]. When `rmp_addr_en = 1`, flash address = incoming AXI address − N (N[31:0] stored here). |
| `direct_access_rmp_1` | `0x3A0` | 32 | RW | `0x00000000` | Direct Access Address Remap Register Upper [63:32]. Upper 32 bits of the 64-bit remap offset N. |

#### Group 4: Device Sequence Configuration Registers (`dev_seq_regs_a`, base = 0x400)

All registers in this group are auto-populated by SFDP discovery. The confirmed bit layout for `_cfg_0` style registers is: `p1_cmd_val[7:0]`, `p1_cmd_ios[9:8]`, `p1_cmd_edge[10]`, `p1_addr_cnt[15:12]`.

**Reset Sequence**

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `rst_seq_cfg_0` | `0x400` | 32 | RW | `0x00019966` | Reset Sequence Config 0. Configures RESET command for Profile 1 and SPI NAND. Fields: `rst_seq_p1_cmd0_val`=0x66 (RESET_ENABLE), `rst_seq_p1_cmd1_val`=0x99 (RESET), `rst_seq_p1_cmd0_en`=1. |
| `rst_seq_cfg_1` | `0x404` | 32 | RW | `0xD0669900` | Reset Sequence Config 1. Extension opcode values and data byte for Profile 1 and SPI NAND RESET sequences. |

**Erase Sequence**

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `ers_seq_cfg_0` | `0x410` | 32 | RW | `0x00DF3020` | Erase Sector Sequence Config 0. Fields: `erss_seq_p1_cmd_val`=0x20 (SECTOR_ERASE), `erss_seq_p1_addr_cnt`=3, `erss_seq_p1_cmd_ext_val`=0xDF. |
| `ers_seq_cfg_1` | `0x414` | 32 | RW | `0x0000000C` | Erase Sequence Config 1. Field: `erss_seq_p1_sect_size`=12 (sector size = 2^12 = 4096 bytes). |
| `ers_seq_cfg_2` | `0x418` | 32 | RW | `0x009F0060` | Chip Erase Sequence Config 2. Fields: `ersa_seq_p1_cmd_val`=0x60 (CHIP_ERASE), `ersa_seq_p1_cmd_ext_val`=0x9F. |

**Program Sequence**

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `prog_seq_cfg_0` | `0x420` | 32 | RW | `0x00003002` | Program Sequence Config 0. Fields: `prog_seq_p1_cmd_val`=0x02 (PAGE_PROGRAM), `prog_seq_p1_addr_cnt`=3. |
| `prog_seq_cfg_1` | `0x424` | 32 | RW | `0x0000FD00` | Program Sequence Config 1. Fields: `prog_seq_p1_cmd_ext_en`=0, `prog_seq_p1_cmd_ext_val`=0xFD. |
| `prog_seq_cfg_2` | `0x428` | 32 | RW | `0x00000002` | Program Sequence Config 2. Profile 2 (HyperFlash/HyperRAM) parameters: target, burst type (Linear), latency count. |

**Read Sequence**

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `read_seq_cfg_0` | `0x430` | 32 | RW | `0x00003003` | Read Sequence Config 0. Fields: `read_seq_p1_cmd_val`=0x03 (READ), `read_seq_p1_addr_cnt`=3. |
| `read_seq_cfg_1` | `0x434` | 32 | RW | `0x0000FC00` | Read Sequence Config 1. Fields: `read_seq_p1_cmd_ext_val`=0xFB, cache random read enable, mode-byte dummy count. |
| `read_seq_cfg_2` | `0x438` | 32 | RW | `0x00000F0A` | Read Sequence Config 2. Profile 2 parameters: burst type (Linear), HyperFlash boundary enable, latency count=15 cycles. |

**Write Enable Sequence**

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `we_seq_cfg_0` | `0x440` | 32 | RW | `0x01F90006` | Write Enable Sequence Config. Fields: `we_seq_p1_cmd_val`=0x06 (WRITE_ENABLE), `we_seq_p1_cmd_ext_val`=0xF9, `we_seq_p1_en`=1. |

**Status Check Sequence**

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `stat_seq_cfg_0` | `0x450` | 32 | RW | (from SFDP) | Status Check Sequence Config 0. I/O configuration for Profile 1 status-register polling (command I/O width, edge, address count, data I/O width). |
| `stat_seq_cfg_1` | `0x454` | 32 | RW | (from SFDP) | Status Check Sequence Config 1. Dummy count and address enable for device-ready, program-fail, and erase-fail status checks. |
| `stat_seq_cfg_2` through `stat_seq_cfg_10` | `0x458`–`0x478` | 32 | RW | (from SFDP) | Additional status check sequence configuration registers. Fields cover bit masks for device-ready status, program-fail, erase-fail detection for Profile 1 and Profile 2 devices. |

#### Group 5: Hardware Identification Registers (`ctrl_consts_a`, base = 0xF00)

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `xspi_ctrl_version` | `0xF00` | 32 | RO | `0x65220602` | Controller Version. Fields: `xspi_ctrl_magic_number`=0x6522 (IP6522 identifier) [31:16]; `xspi_ctrl_rev`=6 [15:8]; `xspi_ctrl_fix`=2 [7:0]. Write-ignore restriction. |
| `ctrl_features_reg` | `0xF04` | 32 | RO | `0x03130703` | Controller Features. Fields (initialized from scml_property at elaboration): `n_threads`=3 (8-thread); `asf_available`=0; `boot_available`=1; `dma_intf`=0; `dma_addr_width`=1 (64-bit); `dma_data_width`=1 (64-bit); `sfr_intf`=1; `n_banks`=3 (8 banks). Write-ignore restriction. |

#### Group 6: Mini-Controller Configuration Registers (`rf_minictrl_regs_a`, base = 0x1000)

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `wp_settings` | `0x1000` | 32 | RW | `0x00000001` | Write Protect Settings. `wp`=1 (WP# pin deasserted at reset) [bit 0]; `wp_enable`=0 [bit 1]. Effective in single/dual SPI modes only. |
| `reset_pin_settings` | `0x1004` | 32 | RW | `0x00000001` | Software Controlled Hardware Reset. `sw_ctrled_hw_rst`=1 (RESET# pin deasserted) [bit 0]; per-bank enable bits `sw_ctrled_hw_rst_bank0–7`; `sw_ctrled_hw_rst_option` (RESET# or DQ3); `rst_dq3_enable`. |
| `clock_mode_settings` | `0x1008` | 32 | RW | `0x00000000` | Clock Mode Settings. `spi_clock_mode` — 0=SPI Mode 0 (CPOL=0, CPHA=0); 1=SPI Mode 3 (CPOL=1, CPHA=1) for legacy SDR transfers. |
| `jedec_rst_timing_reg` | `0x100C` | 32 | RW | `0x00008080` | JEDEC Reset Timing. `tCSH_delay` (CS# high time) and `tCSL_delay` (CS# low time) for JEDEC reset sequences. Stored; no pin-level timing enforced. |
| `dev_delay_reg` | `0x1010` | 32 | RW | `0x01000100` | Device Delay Register. `cssot_delay` (CS# setup before first SCK edge), `cseot_delay` (CS# hold after last SCK edge), `csda_min_delay` (minimum CS# deassert time). Stored; no simulated delay. |
| `rst_recovery_reg` | `0x1014` | 32 | RW | (default) | Reset Recovery Delay. `rst_recovery` — post-reset idle wait count in xspi_clk cycles. Stored; no cycle-accurate enforcement. |
| `dev_active_max_reg` | `0x1018` | 32 | RW | (default) | Device Maximum Active Time (tCMS). Enforced when `global_seq_cfg.seq_tcms_en = 1`. Relevant for RAM devices requiring periodic refresh. |
| `hf_offset_reg` | `0x1020` | 32 | RW | (default) | HyperFlash CA Reserved-Area Offset. `hf_offset_index` and `hf_offset_size` — location and size of the reserved area in the HyperFlash Command-Address field. Applicable for Legacy HyperFlash and xSPI Profile 2.0 devices. |
| `dll_phy_update_cnt` | `0x1030` | 32 | RW | (default) | DLL PHY Resynchronization Count. `resync_cnt` — number of slave DLL resynchronization cycles. Stored; no behavioral effect in LT model. |
| `dll_phy_ctrl` | `0x1034` | 32 | RW | (default) | DLL PHY Control. DFI control update request fields (`resync_idle_cnt`, `resync_high_wait_cnt`, `dll_rst_n`, `dfi_ctrlupd_req`). Stored; no behavioral effect in LT model. |

#### Group 7: PHY Data-Slice Registers (`dataslice_Rfile_a`, base = 0x2000)

These registers are modeled as read/write register storage only. No PHY timing or pin-level behavior is simulated.

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `phy_dq_timing_reg` | `0x2000` | 32 | RW | (default) | PHY DQ Timing Register. Store-only; no behavioral effect. |
| `phy_dqs_timing_reg` | `0x2004` | 32 | RW | (default) | PHY DQS Timing Register. Store-only; no behavioral effect. |
| `phy_dll_master_ctrl_reg` | `0x2008` | 32 | RW | (default) | PHY DLL Master Control Register. Store-only; no behavioral effect. |
| `phy_dll_slave_ctrl_reg` | `0x200C` | 32 | RW | (default) | PHY DLL Slave Control Register. Store-only; no behavioral effect. |
| `phy_ie_timing_reg` | `0x2010` | 32 | RW | (default) | PHY Input Enable Timing Register. Store-only; no behavioral effect. |
| `phy_obs_reg_0` | `0x2020` | 32 | RO | `0x00000000` | PHY Observation Register 0. Returns reset state only. |
| `phy_dll_obs_reg_0` | `0x2030` | 32 | RO | `0x00000000` | PHY DLL Observation Register 0. DLL lock status — returns reset state only. |
| `phy_dll_obs_reg_1` | `0x2034` | 32 | RO | `0x00000000` | PHY DLL Observation Register 1. Returns reset state only. |
| `phy_static_togg_reg` | `0x2038` | 32 | RW | (default) | PHY Static Toggle Register. Store-only; no behavioral effect. |
| `phy_wr_deskew_pd_ctrl_0_reg` | `0x2040` | 32 | RW | (default) | PHY Write Deskew Phase Detect Control Register. Store-only; no behavioral effect. |
| `phy_version_reg` | `0x2060` | 32 | RO | `0x61820701` | PHY Version Register. Returns fixed version constant (IP6182 identifier). |
| `phy_features_reg` | `0x2064` | 32 | RO | (default) | PHY Features Register. Hardware capability flags (`onfi_40`, `onfi_41`, `sdr_16bit`, `xspi`, `sd_emmc`, etc.). Read-only; no behavioral effect. |

#### Group 8: PHY Control Block Registers (`ctb_Rfile_a`, base = 0x2080)

These registers are modeled as read/write register storage only. No PHY timing or pin-level behavior is simulated.

| Register Name | Offset | Bits | Access | Reset Value | Description |
| --- | --- | --- | --- | --- | --- |
| `phy_ctrl_reg` | `0x2080` | 32 | RW | `0x00004310` | PHY Control Register. Global PHY control (drive strength, termination, DFI settings). Store-only. |
| `phy_tsel_reg` | `0x2084` | 32 | RW | `0x00000000` | PHY Termination Select Register. On-die termination enable/disable. Store-only. |
| `phy_gpio_ctrl_0` | `0x2088` | 32 | RW | `0x00000000` | PHY GPIO Control Register 0. Store-only. |
| `phy_gpio_ctrl_1` | `0x208C` | 32 | RW | `0x00000000` | PHY GPIO Control Register 1. Store-only. |
| `phy_gpio_status_0` | `0x2090` | 32 | RO | `0x00000000` | PHY GPIO Status Register 0. Returns reset state only. Write-ignore restriction. |
| `phy_gpio_status_1` | `0x2094` | 32 | RO | `0x00000000` | PHY GPIO Status Register 1. Returns reset state only. Write-ignore restriction. |

---

## 3. Use Model

The xSPI Controller is modeled at the LT abstraction level with timing annotation and temporal decoupling. The intended use case is embedded software development and register-interface validation. All six operating modes (Direct, STIG, PIO, ACMD/CDMA, XIP, and Boot) are modeled as described in Section 2.

Software interacts with the controller through two primary paths:

1. **Register access path:** The CPU writes configuration and command registers via `t_reg_socket` or `t_axi_slave_socket`. The register model dispatches writes to the appropriate `handle_write_*` callback, which applies the corresponding hardware side effect immediately within the `b_transport` call.
2. **Flash execution path:** Writing `cmd_reg0` dispatches a flash operation to the appropriate engine (STIG SC_THREAD, PIO SC_THREAD per thread ID, or ACMD descriptor engine). On completion, status registers (`cmd_status`, `trd_status`, `trd_comp_intr_status`, `ctrl_status`) are updated and `interrupt_out` is driven if the appropriate enable conditions are met.

**Register multiplexing note:** The six physical registers `cmd_reg0`–`cmd_reg5` (offsets 0x000–0x014) are multiplexed across operating modes. The same physical address carries a fundamentally different bitfield encoding depending on `ctrl_config.work_mode` bits [6:5] and (for `2'b11`) on `cmd_reg0[31:30]`. Software must set the desired work mode in `ctrl_config` before staging values into these registers. The model-internal `current_work_mode` variable (updated by `handle_write_ctrl_config`) governs which decoding path each engine callback selects at trigger time. See Section 2.3 (Mode-Dependent Bitfield Layouts for `cmd_reg0`–`cmd_reg5`) for the full per-register, per-mode field breakdown.

### 3.1 Callbacks on Memory-Mapped Registers / Bit-Fields

Register callbacks are classified into three types:

- **Write callbacks** (`handle_write_<REG>`): writing this register triggers an immediate hardware action beyond simple storage.
- **Read callbacks** (`handle_read_<REG>`): reading this register requires dynamic assembly of live hardware state that cannot be served from a static stored value.
- **W1C write callbacks**: a specialization of write callbacks where scml2 `set_clear_on_write_1` is applied to individual bitfields and the outer callback propagates interrupt output de-assertion.

Registers with `set_write_ignore_restriction` and PHY sub-region registers (`dataslice_Rfile_a`, `ctb_Rfile_a`) are excluded — they are store-and-acknowledge or write-ignore registers with no behavioral side effects.

All callback names follow the `handle_write_<REG_NAME>` / `handle_read_<REG_NAME>` convention.

#### Group 1: Command and Status Registers (`ctrl_cmd_stat_a`, base = 0x000)

| Callback Name | Offset | Type | Trigger Condition | Side-Effect Description | Affected State / Signals |
| --- | --- | --- | --- | --- | --- |
| `handle_write_cmd_reg0` | `0x000` | Write | Any write to `cmd_reg0` | **Primary dispatch trigger. Mode-dependent layout.** At callback entry, reads `current_work_mode` (updated by `handle_write_ctrl_config`) to select the dispatch path. **STIG** (`work_mode`=`2'b01`): the written value is entirely ignored; fires `cmd_trigger_event` to wake `stig_engine_thread()`, which decodes the instruction from the already-staged `cmd_reg1`–`cmd_reg4` values. **`work_mode`=`2'b11`:** if `cmd_reg0[31:30]=2'b01`, **PIO** path — applies reserved-bit mask `~((0x7u<<27)|(0x1u<<23))` then `pio_handle_trigger()`; if `[31:30]=2'b00`, **CDMA** path — `cdma_handle_trigger()`. If the target ACMD thread is already busy, sets `intr_status.cmd_ignored` (W1C) and returns without dispatching. In STIG chained mode (`INSTR_LINK=1` in `cmd_reg4[28]`), the first `cmd_reg0` write arms the chain (`gcmd_eng_mc_busy` set); the second write fires the xSPI bus transaction. | `ctrl_status.ctrl_busy`, `ctrl_status.gcmd_eng_busy`, `ctrl_status.gcmd_eng_mc_busy`, `ctrl_status.acmd_eng_busy`, `trd_status.trd_busy[N]`, `intr_status.cmd_ignored`, `cmd_status`, `trd_comp_intr_status`, `trd_error_intr_status`, `int_out` |
| `handle_write_cmd_reg1` | `0x004` | Write | Write to `cmd_reg1` | **Mode-dependent layout.** Stores the raw 32-bit value unconditionally. Interpretation at dispatch time: **STIG** — consumed as instruction bits[31:0] (opcode, I/O width, edge mode, INSTR_TYPE) by `decode_instruction()`; **PIO** — consumed as xSPI Address Lower [31:0], combined with `cmd_reg5` to form the 64-bit flash address; **ACMD** — not consumed (descriptor address is in `cmd_reg2`/`cmd_reg3`). No immediate decoding occurs at write time. | `stig_instruction.opcode` (STIG); PIO thread snapshot `cmd_reg1` (xSPI address [31:0]); not used in ACMD |
| `handle_write_cmd_reg2` | `0x008` | Write | Write to `cmd_reg2` | **Mode-dependent layout.** Stores the raw 32-bit value unconditionally. Interpretation at dispatch time: **STIG** — consumed as instruction bits[63:32] (address continuation, address count, dummy count) by `decode_instruction()`; **PIO** — consumed as SYS_ADDR_PTR_L (lower 32 bits of the system memory DMA pointer, READ/PROGRAM only); **ACMD** — consumed as descriptor address [31:0] (`desc_addr = (cmd_reg3 << 32) | cmd_reg2`; must be 64-byte aligned). | STIG instruction bits[63:32]; PIO thread snapshot `cmd_reg2` (SYS_ADDR_PTR_L); ACMD descriptor address [31:0] |
| `handle_write_cmd_reg3` | `0x00C` | Write | Write to `cmd_reg3` | **Mode-dependent layout.** Stores the raw 32-bit value unconditionally. Interpretation at dispatch time: **STIG** — consumed as instruction bits[95:64] (remaining address bits and data byte count) by `decode_instruction()`; **PIO** — consumed as SYS_ADDR_PTR_H (upper 32 bits of the system memory DMA pointer, READ/PROGRAM only); **ACMD** — consumed as descriptor address [63:32] (upper word of the 64-bit descriptor chain head address). | STIG instruction bits[95:64]; PIO thread snapshot `cmd_reg3` (SYS_ADDR_PTR_H); ACMD descriptor address [63:32] |
| `handle_write_cmd_reg4` | `0x010` | Write | Write to `cmd_reg4` | **Mode-dependent layout; secondary sub-mode dependency within PIO.** Stores the raw 32-bit value unconditionally. Interpretation at dispatch time: **STIG** — consumed as instruction bits[127:96] by `decode_instruction()`; key sub-fields: bit[28]=INSTR_LINK (chains instruction with next `cmd_reg0` write), bits[6:0]=INSTR_TYPE (selects instruction variant); **PIO SECTOR_ERASE (CMD_TYPE=0x1000)** — interpreted as SECT_CNT (number of sectors to erase = field+1); **PIO READ/PROGRAM (CMD_TYPE=0x2200/0x2100)** — interpreted as DATA_CNT (byte count to transfer = field+1); the PIO engine selects the interpretation at `pio_handle_trigger()` time by reading CMD_TYPE from the already-snapshotted `cmd_reg0`; **PIO CHIP_ERASE/RESET** — not consumed; **ACMD** — not consumed (count is in descriptor Command Counter field[63:48]). | STIG instruction bits[127:96] including INSTR_LINK and INSTR_TYPE; PIO thread snapshot `cmd_reg4` (SECT_CNT or DATA_CNT per CMD_TYPE); not used in ACMD |
| `handle_write_cmd_reg5` | `0x014` | Write | Write to `cmd_reg5` | **Mode-dependent layout. Valid only in PIO mode.** Stores the raw 32-bit value. At PIO dispatch time (`pio_handle_trigger()`), snapshotted as xSPI Address Upper [63:32] only for SECTOR_ERASE, READ, and PROGRAM command types; combined with `cmd_reg1[31:0]` to form the full 64-bit flash address (`xspi_addr = (cmd_reg5 << 32) | cmd_reg1`). Not snapshotted for CHIP_ERASE, RESET_SOFT, RESET_JEDEC. In STIG mode: not used (the 128-bit STIG instruction is fully encoded in `cmd_reg1`–`cmd_reg4`). In ACMD mode: not used (flash address comes from the xSPI Pointer field inside the fetched descriptor). The callback exists to maintain scml2 register-file memory consistency regardless of operating mode. | PIO thread snapshot `cmd_reg5` (xSPI address [63:32]); not used in STIG or ACMD |
| `handle_write_cmd_status_ptr` | `0x040` | Write | Write to `cmd_status_ptr` | Selects which PIO/ACMD thread's status is returned on the next read of `cmd_status`. `thrd_status_sel[2:0]` is clamped to [0, MAX_THREADS-1]. | Internal thread selector; affects next `cmd_status` read |
| `handle_read_cmd_status` | `0x044` | Read | Any read of `cmd_status` | **Volatile indirect read.** Returns the live completion/error status of the thread selected by `cmd_status_ptr.thrd_status_sel`. In STIG mode returns STIG engine status; in ACMD/PIO mode returns `thread_status_[sel].to_reg()`. | Reads `thread_status_[thrd_status_sel]` (live array) |
| `handle_write_intr_status` | `0x110` | Write (W1C) | Write to `intr_status` | **W1C interrupt status clear.** Each bit set in the written value clears the corresponding `intr_status` bit. After clearing, re-evaluates `interrupt_out` gated by `intr_enable`. | `intr_status` bits, `int_out` re-evaluation |
| `handle_write_intr_enable` | `0x114` | Write | Write to `intr_enable` | Updating the per-bit interrupt enable mask requires immediate re-evaluation of `interrupt_out`. | `int_out`, interrupt masking logic |
| `handle_write_trd_comp_intr_status` | `0x120` | Write (W1C) | Write to `trd_comp_intr_status` | **W1C thread completion interrupt clear.** Bits set in the written value clear corresponding `trdN_comp` bits. After clearing, re-evaluates `int_out`: if all masked bits are zero, de-asserts `int_out`. | `trd_comp_intr_status[7:0]`, `int_out` |
| `handle_write_trd_error_intr_status` | `0x130` | Write (W1C) | Write to `trd_error_intr_status` | **W1C thread error interrupt clear.** Same W1C mechanism; bits set in the written value clear `trdN_error_stat` bits. After clearing, re-evaluates `int_out`. | `trd_error_intr_status[7:0]`, `int_out` |
| `handle_write_trd_error_intr_en` | `0x134` | Write | Write to `trd_error_intr_en` | Updating the per-thread error interrupt enable mask requires immediate re-evaluation of `int_out`. | `int_out`, thread error interrupt masking |

#### Group 2: Controller Configuration Registers (`ctrl_cfg_common_a`, base = 0x200)

| Callback Name | Offset | Type | Trigger Condition | Side-Effect Description | Affected State / Signals |
| --- | --- | --- | --- | --- | --- |
| `handle_write_long_polling` | `0x208` | Write | Write to `long_polling` | Stores the updated wait-count value used in flash device status re-poll sequencing. Callback ensures model-internal shadow state stays synchronized. | Internal polling parameter `long_polling_val` |
| `handle_write_short_polling` | `0x20C` | Write | Write to `short_polling` | Stores the minimum inter-poll clock count. | Internal polling parameter `short_polling_val` |
| `handle_write_ctrl_config` | `0x230` | Write | Write to `ctrl_config` | **Operating mode switch.** `work_mode` bits [6:5] determine how subsequent `cmd_reg0` writes are dispatched (Direct / STIG / ACMD global). `cont_on_err` changes ACMD/PIO error-recovery behavior. Callback updates internal `current_work_mode` state variable. | `current_work_mode`, ACMD/PIO `cont_on_err` policy |
| `handle_write_dma_settings` | `0x23C` | Write | Write to `dma_settings` | Updates AXI master interface parameters applied to live DMA transactions: `burst_sel`, `OTE`, `word_size`, `sdma_err_rsp`. | `dma_burst_length`, `dma_word_size`, `sdma_err_resp` internal parameters |
| `handle_write_discovery_control` | `0x260` | Write | Write to `discovery_control` | Updates software-writable discovery configuration fields (`discovery_inhibit`, `discovery_num_lines`, `discovery_bank`, etc.). Hardware-set sub-fields (`discovery_comp`, `discovery_fail`) are protected by write-ignore restriction on those specific bits. | Discovery configuration state for next PoR sequence |

#### Group 3: Common Sequence Configuration Registers (`cmn_seq_regs_a`, base = 0x380)

| Callback Name | Offset | Type | Trigger Condition | Side-Effect Description | Affected State / Signals |
| --- | --- | --- | --- | --- | --- |
| `handle_write_xip_mode_cfg` | `0x388` | Write | Write to `xip_mode_cfg` | **XIP mode state change.** Writing `xip_en` arms the XIP entry sequence: on the next READ the model inserts `xip_en_mb_val` as a mode byte. Writing `xip_dis_mb_val` updates the exit mode-byte value. Non-READ commands while XIP is active are rejected with `dir_cmd_err` / `DSC_ERROR`. | `xip_active_banks` bitmask, `xip_en_mb_val`, `xip_dis_mb_val` |
| `handle_write_global_seq_cfg` | `0x390` | Write | Write to `global_seq_cfg` | **Device-type and page-size configuration.** `seq_type` selects the active device profile; `seq_page_size_rd` and `seq_page_size_pgm` bound transfer sizes; `seq_tcms_en` activates tCMS enforcement. | `active_device_profile`, `read_page_size`, `program_page_size`, `tcms_enabled` |
| `handle_write_global_seq_cfg_1` | `0x394` | Write | Write to `global_seq_cfg_1` | Updates `nand_spare_area` field affecting byte-count arithmetic for SPI NAND operations in ACMD and PIO modes. | `nand_spare_area` parameter |
| `handle_write_direct_access_cfg` | `0x398` | Write | Write to `direct_access_cfg` | **Direct-mode bank selection and XIP exit.** `dac_bank_num` selects the chip-select for Direct mode; `rmp_addr_en` enables address remapping; `mode_bit_xip_dis` causes the next Direct-mode READ to insert `xip_dis_mb_val`; `rwds_cap_en` gates RWDS byte-mask translation. | `active_dac_bank`, `rmp_addr_en`, `xip_exit_pending`, `rwds_cap_en` |
| `handle_write_direct_access_rmp` | `0x39C` | Write | Write to `direct_access_rmp` | Stores lower 32 bits of the 64-bit address remap offset N; applied to all subsequent Direct-mode flash address calculations when `rmp_addr_en = 1`. | `rmp_offset[31:0]` |
| `handle_write_direct_access_rmp_1` | `0x3A0` | Write | Write to `direct_access_rmp_1` | Stores upper 32 bits of the 64-bit address remap offset N. | `rmp_offset[63:32]` |

#### Group 4: Device Sequence Configuration Registers (`dev_seq_regs_a`, base = 0x400)

| Callback Name | Offset | Type | Trigger Condition | Side-Effect Description | Affected State / Signals |
| --- | --- | --- | --- | --- | --- |
| `handle_write_rst_seq_cfg_0` | `0x400` | Write | Write to `rst_seq_cfg_0` | Updates RESET sequence parameters (RESET_ENABLE opcode 0x66, RESET opcode 0x99) used for ACMD DEVICE_RESET / JEDEC_RESET and PIO SOFT/JEDEC reset. | `rst_seq_p1_cmd0_val`, `rst_seq_p1_cmd1_val`, reset sequence enable bits |
| `handle_write_rst_seq_cfg_1` | `0x404` | Write | Write to `rst_seq_cfg_1` | Updates RESET sequence extension values and data byte for Profile 1 and SPI NAND. | `rst_seq_p1_cmd0_ext_val`, `rst_seq_p1_cmd1_ext_val`, `rst_seq_p1_data_val` |
| `handle_write_ers_seq_cfg_0` | `0x410` | Write | Write to `ers_seq_cfg_0` | Updates ERASE SECTOR sequence opcode (0xD8 or 0xDC for ERASE_64KB) and address count for Profile 1. | `ers_seq_p1_cmd_val`, `ers_seq_p1_addr_cnt` |
| `handle_write_ers_seq_cfg_1` | `0x414` | Write | Write to `ers_seq_cfg_1` | Updates ERASE sequence timing parameters (dummy count, extension opcode enable). | `ers_seq_p1_dummy_cnt`, `ers_seq_p1_cmd_ext_en` |
| `handle_write_ers_seq_cfg_2` | `0x418` | Write | Write to `ers_seq_cfg_2` | Updates FULL CHIP ERASE (ERSA) opcode (0x60) and Profile 2 erase parameters. | `ersa_seq_p1_cmd_val`, Profile 2 erase burst type |
| `handle_write_prog_seq_cfg_0` | `0x420` | Write | Write to `prog_seq_cfg_0` | Updates PROGRAM (PAGE_PROGRAM) sequence opcode (0x02) and 3-byte address count. | `prog_seq_p1_cmd_val`, `prog_seq_p1_addr_cnt` |
| `handle_write_prog_seq_cfg_1` | `0x424` | Write | Write to `prog_seq_cfg_1` | Updates PROGRAM sequence extension opcode enable and extension value (0xFD) for Profile 1. | `prog_seq_p1_cmd_ext_en`, `prog_seq_p1_cmd_ext_val` |
| `handle_write_prog_seq_cfg_2` | `0x428` | Write | Write to `prog_seq_cfg_2` | Updates PROGRAM sequence parameters for Profile 2 (HyperFlash/HyperRAM): target, burst type, latency count. | `prog_seq_p2_target`, `prog_seq_p2_burst_type`, `prog_seq_p2_latency_cnt` |
| `handle_write_read_seq_cfg_0` | `0x430` | Write | Write to `read_seq_cfg_0` | Updates READ sequence opcode (0x03) and 3-byte address count for Profile 1 and SPI NAND. | `read_seq_p1_cmd_val`, `read_seq_p1_addr_cnt` |
| `handle_write_read_seq_cfg_1` | `0x434` | Write | Write to `read_seq_cfg_1` | Updates READ sequence extension opcode value (0xFB), cache-random-read enable, and mode-byte dummy count. | `read_seq_p1_cmd_ext_val`, `read_seq_p1_cache_random_read_en` |
| `handle_write_read_seq_cfg_2` | `0x438` | Write | Write to `read_seq_cfg_2` | Updates READ sequence Profile 2 parameters: burst type, HyperFlash boundary enable, latency count (default 15 cycles). | `read_seq_p2_latency_cnt`, `read_seq_p2_hf_bound_en` |
| `handle_write_we_seq_cfg_0` | `0x440` | Write | Write to `we_seq_cfg_0` | Updates WRITE ENABLE (WREN opcode 0x06) sequence parameters. `we_seq_p1_en` enables automatic WREN prefix before any PROGRAM or ERASE command. | `we_seq_p1_cmd_val`, `we_seq_p1_en`, `we_seq_p1_cmd_ext_val` |
| `handle_write_stat_seq_cfg_0` | `0x450` | Write | Write to `stat_seq_cfg_0` | Updates STATUS CHECK sequence I/O configuration for Profile 1. Used for flash device-ready polling after PROGRAM/ERASE in all modes. | `stat_seq_p1_cmd_ios`, `stat_seq_p1_addr_cnt`, `stat_seq_p1_data_ios` |
| `handle_write_stat_seq_cfg_1` | `0x454` | Write | Write to `stat_seq_cfg_1` | Updates STATUS CHECK dummy count and address enable for device-ready, program-fail, and erase-fail checks. | `stat_seq_p1_dev_rdy_dummy_cnt`, `stat_seq_p1_prog_fail_addr_en` |
| `handle_write_stat_seq_cfg_2` through `handle_write_stat_seq_cfg_10` | `0x458`–`0x478` | Write | Write to respective `stat_seq_cfg_N` | Updates status-check bit mask fields for device-ready, program-fail, and erase-fail detection (Profile 1 and Profile 2). | Status-check mask parameters per profile |

#### Group 5: Mini-Controller Configuration Registers (`rf_minictrl_regs_a`, base = 0x1000)

| Callback Name | Offset | Type | Trigger Condition | Side-Effect Description | Affected State / Signals |
| --- | --- | --- | --- | --- | --- |
| `handle_write_wp_settings` | `0x1000` | Write | Write to `wp_settings` | Updates Write Protect pin (DQ2) control: `wp` value (1=WP# deasserted) and `wp_enable` gate. The new WP state is applied to the `cdns_extension` WP field on the next flash transaction. | `wp_pin_level`, `wp_enabled` state; `cdns_extension` WP field |
| `handle_write_reset_pin_settings` | `0x1004` | Write | Write to `reset_pin_settings` | Updates software-controlled hardware reset pin control: `sw_ctrled_hw_rst`, `sw_ctrled_hw_rst_option`, `rst_dq3_enable`, and per-bank enable bits `sw_ctrled_hw_rst_bank0–7`. New state applied to `cdns_extension` on next flash transaction. | `hw_rst_level`, `hw_rst_option`, per-bank `rst_bank_enable[7:0]` |
| `handle_write_clock_mode_settings` | `0x1008` | Write | Write to `clock_mode_settings` | Updates SPI clock mode selection: `spi_clock_mode=0` selects SPI Mode 0; `spi_clock_mode=1` selects SPI Mode 3. Used to condition `cdns_extension` clock-edge encoding for legacy SDR transactions. | `spi_cpol`, `spi_cpha`; `cdns_extension` clock-mode field |
| `handle_write_jedec_rst_timing_reg` | `0x100C` | Write | Write to `jedec_rst_timing_reg` | Stores `tCSH_delay` and `tCSL_delay` for JEDEC hardware reset sequences. Passed to `cdns_extension` as timing hints; no cycle-accurate enforcement. | `jedec_tCSH`, `jedec_tCSL` timing parameters |
| `handle_write_dev_delay_reg` | `0x1010` | Write | Write to `dev_delay_reg` | Stores relative device-selection delays (`cssot_delay`, `cseot_delay`, `csda_min_delay`). Passed as `cdns_extension` timing hints; no cycle-accurate simulation. | `cssot_delay`, `cseot_delay`, `csda_min_delay` |
| `handle_write_rst_recovery_reg` | `0x1014` | Write | Write to `rst_recovery_reg` | Stores `rst_recovery` — reset recovery delay after a hardware reset pulse; used in JEDEC/hardware reset sequencing. Stored; no cycle-accurate enforcement. | `rst_recovery_count` parameter |
| `handle_write_dev_active_max_reg` | `0x1018` | Write | Write to `dev_active_max_reg` | Stores `dev_active_max` — maximum CS# active cycle count (tCMS). Enforced when `global_seq_cfg.seq_tcms_en = 1`; updates the internal tCMS limit used in ACMD/PIO transaction sequencing. | `tcms_limit` parameter |
| `handle_write_hf_offset_reg` | `0x1020` | Write | Write to `hf_offset_reg` | Stores `hf_offset_index` and `hf_offset_size` for HyperFlash CA reserved-area configuration. Updates CA field construction parameters for Profile 2 flash transactions. | `hf_ca_reserved_index`, `hf_ca_reserved_size` |
| `handle_write_dll_phy_update_cnt` | `0x1030` | Write | Write to `dll_phy_update_cnt` | Stores `resync_cnt` — slave DLL resynchronization cycle count. PHY DLL resynchronization is not simulated; stored only. Callback maintains scml2 framework memory consistency. | `dll_resync_cnt` (stored; no behavioral effect) |
| `handle_write_dll_phy_ctrl` | `0x1034` | Write | Write to `dll_phy_ctrl` | Stores DFI control update request fields (`dll_rst_n`, `dfi_ctrlupd_req`, `resync_idle_cnt`, `resync_high_wait_cnt`, etc.). PHY resync not modeled; stored only for memory consistency. | `dll_phy_ctrl` shadow register (stored; no behavioral effect) |

#### Registers Explicitly Excluded from Callbacks

| Register | Offset | Reason |
| --- | --- | --- |
| `cmd_status` | `0x044` | Write-ignore; read requires `handle_read_cmd_status` (listed above) |
| `ctrl_status` | `0x100` | Write-ignore; updated exclusively by model internals |
| `trd_status` | `0x104` | Write-ignore; updated by thread dispatch and completion logic |
| `sdma_size` | `0x240` | Write-ignore; hardware-updated during SDMA transfers |
| `sdma_trd_info` | `0x244` | Write-ignore; hardware-updated |
| `sdma_addr0` | `0x24C` | Write-ignore; hardware-updated |
| `sdma_addr1` | `0x250` | Write-ignore; hardware-updated |
| `dma_target_error_l` | `0x150` | Write-ignore; captured on AXI master bus error |
| `dma_target_error_h` | `0x154` | Write-ignore; captured on AXI master bus error |
| `boot_status` | `0x158` | Write-ignore; populated on boot engine failure |
| `xspi_ctrl_version` | `0xF00` | Write-ignore; fixed silicon constants |
| `ctrl_features_reg` | `0xF04` | Write-ignore; initialized from scml_property at elaboration |
| `discovery_control.discovery_comp/fail/inhibit` | `0x260` (sub-fields) | Write-ignore on hardware-set sub-fields |
| All `dataslice_Rfile_a` registers (0x2000–0x2077) | `0x2000+` | PHY store-only; no behavioral effect |
| All `ctb_Rfile_a` registers (0x2080–0x2097) | `0x2080+` | PHY store-only; no behavioral effect |

---

## 4. Assumptions

### 1. Abstraction Level

- **Loosely Timed (LT), not cycle-accurate** — no clock period, no DDR timing parameters, no DLL calibration sequence, and no setup/hold constraints are modeled. `b_transport` is used exclusively; no `nb_transport` / AT protocol is implemented.
- **Transaction-level flash bus** — all xSPI bus operations (command opcode, bank/CS, address, data, instruction type) are encoded in a `cdns_extension` TLM payload carried on `xspi_bus_socket[N]`. No DDR wire framing, DQS strobe, or DQ bus turnaround is modeled.
- **No simulation time consumed by flash operations** — flash READ, PROGRAM, ERASE, and status-poll sequences complete within the same `b_transport` call with zero simulated time advancement; device latency is not modeled.

### 2. Operating Modes

- **All six operating modes are modeled:** Direct (`work_mode=2'b00`), STIG (`work_mode=2'b01`), PIO (`work_mode=2'b11`, `cmd_reg0[31:30]=2'b01`), ACMD/CDMA (`work_mode=2'b11`, `cmd_reg0[31:30]=2'b00`), XIP (per-bank via `xip_mode_cfg.xip_en`), and Boot (via the physical `boot_en` input asserted during PoR, `boot_en=1` in `xspi_PoR_trans`).
- **Direct mode** translates AXI slave READ transactions to a `READ_ZERO_LATENCY` opcode on `xspi_bus_socket` and AXI slave WRITE transactions to a WREN followed by PAGE_PROGRAM sequence; the active bank is selected by `direct_access_cfg.dac_bank_num`.
- **STIG mode** is triggered by a write to `cmd_reg0`; an SC_THREAD decodes opcode, address, data byte count, and `INSTR_LINK` (two-phase chain) from `cmd_reg0–4`, executes the flash transaction, and sets `cmd_status.COMPLETE`. Five STIG handlers are modeled: READ, WRITE, control (WREN/WRDI/RDSR), suspend/resume, and READ_SFDP.
- **PIO mode** supports up to 8 concurrent SC_THREADs (thread ID from `cmd_reg0[26:24]`); each thread snapshots only the `cmd_reg` fields required by its command type at trigger time. Supported PIO commands: READ, PROGRAM, SECTOR_ERASE, CHIP_ERASE, SOFT_RESET, JEDEC_RESET.
- **ACMD/CDMA mode** uses a 64-byte-aligned descriptor fetched from system memory via `cmd_reg2`/`cmd_reg3`; the descriptor state machine executes FETCH → VALIDATE → EXECUTE → WRITEBACK_STATUS → (CONT + next ≠ 0 → FETCH : COMPLETE); descriptor chaining (CONT flag), interrupt-on-chain-end (INT flag), error writeback, and `MB_XIP_EN` (valid on READ only; generates `DSC_ERROR` otherwise) are all modeled.
- **XIP mode** is per-bank; on the next READ after `xip_en` is set the model inserts `xip_en_mb_val` as a mode byte; exit is triggered by `direct_access_cfg.mode_bit_xip_dis`, which inserts `xip_dis_mb_val` on the following READ; non-READ commands while XIP is active generate `dir_cmd_err` / `DSC_ERROR`.
- **Boot mode** is an SC_THREAD launched from the PoR handler when `boot_en=1` and `boot_available=1`; on success `boot_comp` is asserted in `xspi_PoR_trans`; on failure `boot_error` is asserted and `boot_status` (`boot_dqs_err`, `boot_crc_err`, `boot_bus_err`) is populated.

### 3. Power-on Reset and SFDP Discovery

- **PoR arrives as a single TLM write** on `PoR_input_signals` carrying an `xspi_PoR_trans` extension; the model processes it synchronously in `b_transport_por_input()`.
- **SFDP discovery is fully modeled** when `discovery_inhibit=0`: the model issues READ_SFDP (opcode 0x5A) on `xspi_bus_socket[discovery_bank]`, parses the JESD216A basic parameter table (16 DWORDs), and auto-configures all 10 sequence register groups.
- **When `discovery_inhibit=1`:** the SFDP read is skipped; sequence registers retain their reset or previously programmed values; `discovery_control.discovery_comp` is set immediately without issuing any flash transaction.
- **CRC fields in `xspi_PoR_trans`** (`discovery_seq_crc_en`, `discovery_seq_crc_variant`, `discovery_seq_crc_oe`, `discovery_seq_crc_chunk_size`) are accepted and stored to sequence registers; no CRC polynomial computation is performed.
- **`init_rb_valid_time`** (present in KB extension, absent from guide field list but not contradicted) is accepted and passed through as part of the PoR extension; no timing enforcement is applied.
- **`discovery_extop_en` / `discovery_extop_val`** (KB-sourced, silent omission in guide) are accepted as valid PoR extension fields configuring extended-opcode mode for SFDP discovery.
- **Discovery outcome** is reported via `discovery_control.discovery_comp` and `discovery_control.discovery_fail`; initialization result is reflected in `ctrl_status.init_comp` and `ctrl_status.init_fail` (0b00=xSPI/SPI-NAND detected, 0b01=failed, 0b10=Legacy SPI detected).

### 4. Flash Device Interaction

- **All flash transactions are atomic TLM calls** on `xspi_bus_socket[bank]`; no intermediate bus arbitration, DDR preamble/postamble, or CS# toggle timing is modeled.
- **Four device profiles are modeled** via `global_seq_cfg.seq_type`: Profile 1 (xSPI NOR, `seq_type=0`), Profile 2 HyperFlash (`seq_type=1`), Profile 2 HyperRAM (`seq_type=2`), and SPI NAND (`seq_type=3`). The active profile governs which sequence register group is applied per command.
- **Page size bounds** are enforced from `global_seq_cfg`: `seq_page_size_rd` (2^N bytes, default 0xF = unlimited) bounds READ transfer sizes; `seq_page_size_pgm` (2^N bytes, default 0x8 = 256 bytes) bounds PROGRAM transfer sizes in Direct, PIO, and ACMD modes.
- **SPI NAND spare area** is modeled via `global_seq_cfg_1.nand_spare_area`; the extra byte count is added to the page data size for NAND READ PAGE and PROGRAM PAGE operations in PIO and ACMD modes.
- **Address remapping** in Direct mode: when `direct_access_cfg.rmp_addr_en=1`, the flash address is computed as (incoming AXI address − N), where N is the 64-bit value in `direct_access_rmp` / `direct_access_rmp_1`.
- **Device delay registers** (`dev_delay_reg.cssot_delay`, `cseot_delay`, `csda_min_delay`) and `rst_recovery_reg` are accepted and stored; no simulated delay is imposed on flash transactions.
- **JEDEC reset timing** (`jedec_rst_timing_reg.tCSH_delay`, `tCSL_delay`) and `dev_active_max_reg` (CS# maximum active time / tCMS) are stored; no pin-level timing enforcement is applied.
- **Software-controlled hardware reset** (`reset_pin_settings.sw_ctrled_hw_rst`, per-bank enable bits, `sw_ctrled_hw_rst_option`, `rst_dq3_enable`) are stored as software-visible fields; actual RESET# / DQ3 pin assertion is not simulated.
- **Write-protect** (`wp_settings.wp`, `wp_settings.wp_enable`) is stored and observable; DQ2 pin-level assertion is not modeled.
- **Legacy SPI clock mode** (`clock_mode_settings.spi_clock_mode`) is stored and used to condition flash command generation; no electrical CPOL/CPHA transition is modeled.
- **`hf_offset_reg`** (KB-sourced, absent from guide but without contradiction) is included as a software-programmable register for Profile 2 (HyperFlash / xSPI Profile 2.0) CA reserved-area configuration.

### 5. DMA and AXI Transactions

- **AXI master DMA (`i_dma_socket`)** is a 64-bit TLM-2.0 initiator socket (`dma_addr_width=1`, `dma_data_width=1` by default); used for PIO READ/PROGRAM and ACMD descriptor DMA transfers between system memory and flash.
- **AXI DMA parameters** are software-programmable at runtime via `dma_settings`: `burst_sel`, `OTE`, `word_size`, and `sdma_err_rsp`; the model applies these parameters to live DMA calls.
- **AXI slave (`t_axi_slave_socket`)** accepts 64-bit TLM-2.0 transactions; in Direct mode, AXI reads and writes are forwarded to flash; in all other modes, accesses route to the register model. The optional `axi_trans` extension encoding AXI4 burst, lock, cache, protection, QoS, and strobe fields is accepted.
- **ACMD descriptor alignment** is enforced: the 64-bit descriptor pointer in `cmd_reg2`/`cmd_reg3` must be 64-byte aligned; misalignment generates `TRD_ERR_INT` and sets the thread error path without issuing a flash transaction.
- **ACMD descriptor writeback** — the controller writes the 8-byte status word back to system memory at descriptor offset +40 on every EXECUTE cycle (both completion and error paths).
- **Slave DMA (SDMA)** — `sdma_size`, `sdma_trd_info`, `sdma_addr0/1` are hardware-updated (write-ignore to software) during STIG and ACMD SDMA transfers.

### 6. Thread and Concurrency Model

- **Up to 8 concurrent PIO threads and 8 concurrent ACMD threads** are modeled, controlled by the `n_threads` configuration parameter (encoded 0–3 for 1/2/4/8 threads). The active width of `TRD_STATUS`, `trd_comp_intr_status`, and `trd_error_intr_status` reflects the configured thread count.
- **`CMD_IGNORED`** is set in `intr_status` when a `cmd_reg0` write targets a thread whose `TRD_STATUS` bit is already set; the new operation is silently discarded without overwriting the in-progress thread state.
- **`ctrl_busy`** (`ctrl_status` bit) is set when any thread becomes active and cleared when all threads return to IDLE. The hardware `ctrl_busy` output pin is abstracted and is only observable via the `ctrl_status` register — it is not exposed as a separate TLM signal port.
- **Thread status for ACMD mode** is accessed via a two-register indirect read: write thread ID to `cmd_status_ptr` (0x040), then read `cmd_status` (0x044).

### 7. Interrupt Model

- **`interrupt_out`** (`sc_out<bool>`) is driven high when `trd_comp_intr_status` or `trd_error_intr_status` is non-zero after enable-register masking; driven low when both registers are cleared.
- **`TRD_COMP_INT`** (one bit per thread in `trd_comp_intr_status`) is set on PIO/ACMD thread completion when the INT flag was set in `cmd_reg0` or the descriptor; W1C; gated by `trd_comp_intr_en` before driving `interrupt_out`. Note: the KB reference implementation does not apply the `trd_comp_intr_en` gate; the new model must apply it per guide specification. This is an ambiguous item — the implementer must decide whether to apply the enable gate (per guide spec) or replicate the KB reference behavior (no gate).
- **`TRD_ERR_INT`** (one bit per thread in `trd_error_intr_status`) is set on thread error; W1C; gated by `trd_error_intr_en` before driving `interrupt_out`.
- **`CMD_IGNORED`** in `intr_status` is W1C; set synchronously when a duplicate trigger arrives for a busy thread.
- **`intr_enable`** register gates `intr_status` bits (e.g., `stig_done`, `dir_cmd_err`, `cdma_terr`, `sdma_trigg`) before driving `interrupt_out`; rising-edge triggered.

### 8. Reset Behavior

- **Active-low `reset_in` signal** triggers `reset_model()`, which resets all register bitfields to their hardware reset values; all in-progress PIO and ACMD threads are aborted; `TRD_STATUS`, `trd_comp_intr_status`, `trd_error_intr_status`, `ctrl_status`, and `intr_status` are all cleared.
- **Read-only registers** (`ctrl_features_reg`, `xspi_ctrl_version`) are not affected by reset; they are initialized at elaboration time from `scml_property` values and remain constant.
- **PHY registers** (`dataslice_Rfile_a`, `ctb_Rfile_a`) are included in `reset_model()` and return to their reset values; no PHY behavioral side effect occurs.
- **Polling parameters** (`long_polling`, `short_polling`) are reset to their non-zero default values (1000 and 500 respectively) as configured by `configure_default_bins`.

### 9. PHY Layer Exclusions (IP6182)

- **PHY is fully abstracted** — DDR/Octal-DDR wire framing, DQS strobe generation, DQ bus turnaround, and PHY timing alignment are not modeled.
- **All PHY registers** (`phy_dq_timing_reg`, `phy_dqs_timing_reg`, `phy_dll_master_ctrl_reg`, `phy_dll_slave_ctrl_reg`, `phy_gate_lpbk_ctrl_reg`, `phy_ie_timing_reg`, `phy_static_togg_reg`, `phy_ctrl_reg`, `phy_tsel_reg`, `phy_gpio_ctrl_0/1`, `phy_wr_deskew_pd_ctrl_0_reg`) are modeled as read/write register storage only; no behavioral side effect.
- **PHY observation registers** (`phy_obs_reg_0`, `phy_dll_obs_reg_0`, `phy_dll_obs_reg_1`) are read-only; they return reset state only; DLL lock status is not simulated.
- **PHY resynchronization** (`dll_phy_update_cnt`, `dll_phy_ctrl`, `dll_phy_ctrl.dll_rst_n`, `dll_phy_ctrl.dfi_ctrlupd_req`) — stored; no effect in the LT model.
- **PHY loopback test mode** (`phy_gate_lpbk_ctrl_reg.lpbk_en`, `lpbk_internal`, `loopback_control`) — stored; no effect.
- **DLL bypass debug mode** (`phy_dll_master_ctrl_reg.param_dll_bypass_mode`) — stored; not modeled functionally.
- **ONFI/SD-eMMC/per-bit-deskew feature flags** (`phy_features_reg.*`) — read-only hardware capability flags; no behavioral effect.

### 10. Excluded Features

- **CRC computation** — `discovery_seq_crc_en`, `seq_crc_en`, `seq_crc_variant`, `seq_crc_oe`, and `seq_crc_chunk_size` fields are accepted and stored; no CRC polynomial is computed over SFDP data, flash read data, or program data; `dir_crc_err` and descriptor `CRC_ERROR` bits are not generated from data content.
- **ECC** — `ECC_CORR_ERROR` and `ECC_STAT` fields in the descriptor status are defined in the register model; ECC computation and error injection are not performed.
- **ASF (Automotive Safety Features)** — `ctrl_features_reg.asf_available` is correctly initialized from `scml_property` so software reads the right capability; ASF fault injection, parity checking, and error reporting are not modeled.
- **Polling loops** — `long_polling` and `short_polling` register values are stored and shadowed internally; no active polling loop executes in the LT model; status poll results are returned immediately to the calling engine.
- **Pin-level signal timing** — `dev_delay_reg`, `jedec_rst_timing_reg`, `rst_recovery_reg`, and `dev_active_max_reg` are stored; no cycle-accurate delay is applied to any transaction.
- **GP open-drain input pins** — `xspi_dfi_gp_open_drain[0:3]` pin transitions are not modeled; `gp_open_drain_0–3` interrupt status bits remain zero unless explicitly driven by the test environment.
- **Address remapping in KB reference archives** — the reference implementations in Archive 1 and Archive 2 do not apply `direct_access_rmp` to the outgoing flash address; the new model must implement address remapping from the register specification.
- **`clone()` / `copy_from()` stubs** — the reference archives leave both TLM extension methods as stubs returning `nullptr`; the new model must implement them correctly for proper TLM socket operation.

### 11. Ambiguous Items

The following items were identified during source reconciliation between the guide and the knowledge base. They are documented here for implementer awareness.

| Item | Conflict Type | Description | Resolution |
| --- | --- | --- | --- |
| `trd_comp_intr_en` register callback | Silent Omission | Guide §3.7 states `TRD_COMP_INT` should be gated by `trd_comp_intr_en` before driving `interrupt_out`; KB `cdns_xspi_ctrl_regBase.h` does not declare a callback for this register, and the reference implementation does not check completion-interrupt enable masks. | No address collision. Implementer must decide: apply the enable gate per guide specification, or follow the KB reference (no gate). If the guide spec is followed, `handle_write_trd_comp_intr_en` should be added. |
| `init_rb_valid_time` in `xspi_PoR_trans` | Silent Omission | KB `cdns_extension.h` defines `init_rb_valid_time` (time from PoR when device becomes accessible); guide §3.2 does not list it. | No contradiction. Field is accepted and passed through; no timing enforcement in LT model. Included. |
| `discovery_extop_en` / `discovery_extop_val` in `xspi_PoR_trans` | Silent Omission | KB `cdns_extension.h` initializes both fields; guide §3.2 does not enumerate them in the field list. | No contradiction. Consistent with READ_SFDP opcode 0x5A and extended-opcode mode. Included. |
| `hf_offset_reg` (HyperFlash CA offset) | Silent Omission | KB documents `hf_offset_reg` for Legacy HyperFlash and xSPI Profile 2.0 CA reserved-area configuration; guide does not mention it. | No contradiction. Included as a software-programmable configuration register for Profile 2 devices. |
| `int_out` (KB) vs. `interrupt_out` (guide / ACMD archive) | Naming Variance | `cdns_xspi_ctrl_regBase.h` declares `sc_out<bool> int_out`; the guide interface table and ACMD archive use `interrupt_out`. | These are the same logical signal. The preferred name per the guide is `interrupt_out`. Implementer must bind both references to the same `sc_out<bool>`. |
| `ctrl_busy` output pin vs. register field | Granularity Difference | KB states `ctrl_status.ctrl_busy` "is also routed to the controller interface via the ctrl_busy pin"; guide TLM interface table does not list `ctrl_busy` as a TLM signal port. | The LT model abstracts the pin; `ctrl_busy` is observable only via the `ctrl_status` register. No separate TLM socket required. |
| `n_banks` vs. `NUM_TARGETS` default values | Naming Variance | `NUM_TARGETS` scml_property defaults to 1 at the property level while `ctrl_features_reg.n_banks` default-bin encodes 8 banks (value 3). | These are two representations of the same underlying quantity. `NUM_TARGETS` must be set at instantiation to size the socket array; `n_banks` must be initialized to the matching encoded value. Implementer must keep them consistent. |
| `boot_comp` / `boot_error` as `xspi_PoR_trans` fields vs. separate ports | Granularity Difference | Guide §3.9 states "assert `boot_comp` output signal" implying hardware output pins; guide interface table does not list them as separate TLM signal ports. | The KB encoding as `xspi_PoR_trans` output fields (written back by the model after PoR handling) is the correct TLM-LT abstraction. Resolved: `boot_comp` and `boot_error` are output fields of `xspi_PoR_trans`, not standalone TLM signal ports. |
