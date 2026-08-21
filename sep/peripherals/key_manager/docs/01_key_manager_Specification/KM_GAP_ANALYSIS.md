# SEP Key Manager (KM) Gap Analysis

---

> ## Applies to the pre-KPVLP-removal RTL revision
>
> The analysis below was written against an earlier KM RTL revision that exposed
> two external AXI-Lite slaves. The current reference RTL exposes only the
> mailbox, and the VP model has been aligned to it. The following parts of this
> document are therefore stale, and the rows that cite KPVLP tests describe
> coverage of hardware that no longer exists:
>
> | Item | Then | Now |
> |---|---|---|
> | SEP-visible windows | Mailbox + KPVLP (`0x10921000`) | Mailbox only; `0x10921000` unmapped |
> | Key provisioning | `CMD_KPVLP_SLOT_REQ` (0x20) then `CMD_KPVLP_KEY_REGISTER` (0x21) | `CMD_KEY_LOAD` (0x26); 0x20/0x21 return `RET_INVALID_CMD` |
> | Destination policy | `KPVLP_CTRL.DEST_VALID` per slot | Held by firmware alongside the handle |
> | `CTRL.UNLOCK_SEP` | Grants SEP access to a vault slot | Removed; no SEP path into the vault |
> | KPV control | `UNLOCK_SEP`, `DEST_VALID` | `ERASE` |
> | Sideload destinations | HMAC, KMAC, AES, OTBN | Adds Adams Bridge bits 4–7 |
> | `KEY_SIZE` units | Interpreted as a slot count | Word count minus one, up to 128 words over 8 slots |
> | Handle allocation | First free entry, recycled on revoke | Monotonic, never recycled |
>
> Sections 1.3 (KPVLP registers), the 0x20/0x21 command rows in Part 2, and the
> `UNLOCK_SEP` row in 1.2 no longer describe the design. Everything else —
> mailbox framing, KPV storage and scrambling, `CMD_KEY_GENERATE`,
> `CMD_KEY_REVOKE`, `CMD_KEY_TRANSFER`, `CMD_ENGINE_SHRED` — still applies.
>
> Adams Bridge is not yet modelled: transfers naming an ABR destination are
> accepted and the key material is dropped with a warning. `CMD_ABR_SK_TRANSFER`
> (0x27) reports `RET_FAILURE` rather than inventing a shared key, and neither
> `RESP_ABR_SHARED_KEY_READY` nor the ML-KEM shared-key interrupt is generated.
>
> Two further paths remain unmodelled and are not covered below: the KM's
> AXI-Lite pass-through to the system eFuse controller (KM-local `0x0001_1000`,
> remapped by the integrator), and the separation of the cold and warm reset
> domains — the model has a single reset input, so `OTP_READ_LOCK_COLD` is
> cleared on any reset where hardware would preserve it across a warm one.

---

## Purpose

This document provides a comprehensive gap analysis comparing the SEP Key Manager RDL and firmware specifications with the Test Plan (v6.2), identifying coverage status and any remaining gaps.

---

## Part 1: RDL to Test Plan Traceability Matrix

### 1.1 KMCSR Registers (km_csr.rdl)

> **Architectural constraint — AXI VIP cannot access KMCSR.**
> `key_manager.sv` exposes only two external AXI-Lite slave ports: Mailbox SEP
> side and KPVLP. KMCSR (KM-internal `0x0000_E000`) is wired exclusively to
> the KM PicoRV32 CPU's internal crossbar. Any AXI transaction to
> `0x0000_E000` issued from the SEP testbench (via `cpu_lsu_sqr`,
> `cpu_dbg_sqr`, or `ext_axi_sqr`) returns **DECERR** — the address is
> unmapped in the SEP AXI fabric. KMCSR tests must use one of:
> 1. **UVM HDL backdoor** (`uvm_hdl_read` on `field_storage` inside
>    `km_csr_reg` — see CLAUDE.md "KMCSR Register Access Constraint")
> 2. **Firmware TB protocol** (KM firmware reads KMCSR and writes results to
>    `TB_RESULT`/`TB_SIGNATURE` at offsets 0x110–0x114 after boot)
>
> `VERSION` (0x0001_0000) and `DEBUG` (0xCAFE_BEEF) have no storage flops;
> they are hardwired constants and require no runtime check.

| Register | Offset | Test Coverage | Tests | Status |
|----------|--------|---------------|-------|--------|
| VERSION | 0x000 | ✅ Covered | `sep_km_kmcsr_version_test` | Complete (RSVD/MAJOR/MINOR/PATCH layout) |
| CTRL | 0x004 | ✅ Covered | (Reserved, no test needed) | N/A |
| SOFT_RST_CODE | 0x008 | ✅ Covered | `sep_km_kmcsr_soft_reset_test` | Complete |
| IRQ_STATUS | 0x00C | ✅ Covered | `sep_km_kmcsr_irq_status_test`, `sep_km_drbg_irq_test`, `sep_km_wipe_irq_test`, `sep_km_mailbox_protocol_error_test` | Complete |
| IRQ_ENABLE | 0x010 | ✅ Covered | `sep_km_kmcsr_irq_enable_test` | Complete |
| SCRAMBLER_KEY | 0x014 | ✅ Covered | `sep_km_scrambler_key_test` | Complete |
| SCRAMBLER_CTRL | 0x018 | ✅ Covered | `sep_km_scrambler_enable_test` | Complete |
| SRAM_LOCK | 0x01C | ✅ Covered | `sep_km_sram_lock_test` | Complete |
| IRQ_SET | 0x020 | ✅ Covered | `sep_km_kmcsr_irq_set_test` | Complete |
| SRAM_WRITE_LOCK_VIOLATION | 0x024 | ✅ Covered | `sep_km_sram_lock_violation_test` | Complete |
| RECOVERABLE_ERR | 0x028 | ✅ Covered | `sep_km_kmcsr_recoverable_err_test` | Complete |
| OTP_LIFE_CYCLE | 0x030 | ✅ Covered | `sep_km_otp_data_test` | Complete |
| OTP_DEMOTION_STATE | 0x034 | ✅ Covered | `sep_km_otp_data_test` | Complete |
| OTP_CHIPLET_UID_0..31 | 0x038-0x0B4 | ✅ Covered | `sep_km_otp_data_test` | Complete |
| VUART_TX | 0x100 | ✅ Covered | `sep_km_vuart_test` | Complete |
| VUART_RX | 0x104 | ✅ Covered | `sep_km_vuart_test` | Complete |
| VUART_STATUS | 0x108 | ✅ Covered | `sep_km_vuart_test` | Complete |
| TB_RESULT | 0x110 | ✅ Covered | `sep_km_test_protocol_test` | Complete |
| TB_SIGNATURE | 0x114 | ✅ Covered | `sep_km_test_protocol_test` | Complete |
| TB_ERRCODE | 0x118 | ✅ Covered | `sep_km_test_protocol_test` | Complete |
| TB_SUBTEST | 0x11C | ✅ Covered | `sep_km_test_protocol_test` | Complete |
| TB_CMD | 0x120 | ✅ Covered | `sep_km_test_protocol_test` | Complete |
| TB_CMD_ARG | 0x124 | ✅ Covered | `sep_km_test_protocol_test` | Complete |
| TB_CMD_STATUS | 0x128 | ✅ Covered | `sep_km_test_protocol_test` | Complete |
| TB_CMD_RESULT | 0x12C | ✅ Covered | `sep_km_test_protocol_test` | Complete |
| DEBUG | 0x1FC | ✅ Covered | `sep_km_kmcsr_debug_test` | Complete |

### 1.2 KPV Registers (km_kpv.rdl)

| Register | Offset | Test Coverage | Tests | Status |
|----------|--------|---------------|-------|--------|
| KEY_ENTRY[32].WORD[16] | 0x000-0x7FF | ✅ Covered | `sep_km_kpv_entry_test`, `sep_km_wipe_state_test` | Complete |
| CTRL[32].LOCK_WRITE | 0x800-0x87C | ✅ Covered | `sep_km_kpv_lock_write_test`, `sep_km_wipe_state_test` | Complete |
| CTRL[32].LOCK_USE | 0x800-0x87C | ✅ Covered | `sep_km_kpv_lock_use_test`, `sep_km_wipe_state_test` | Complete |
| CTRL[32].UNLOCK_SEP | 0x800-0x87C | ✅ Covered | `sep_km_kpvlp_unlock_sep_test`, `sep_km_wipe_state_test` | Complete |
| CTRL[32].RSVD (bit 3) | 0x800-0x87C | ✅ Covered | (Reserved, no test needed) | N/A |
| CTRL[32].EXTEND | 0x800-0x87C | ✅ Covered | `sep_km_kpv_extend_test` | Complete |
| CTRL[32].DEST_VALID | 0x800-0x87C | ✅ Covered | `sep_km_kpv_dest_valid_test` | Complete |
| CTRL[32].LAST_DWORD | 0x800-0x87C | ✅ Covered | `sep_km_kpv_last_dword_test` | Complete |
| KPV_SCRAMBLER_KEY | 0x880 | ✅ Covered | `sep_km_kpv_scrambler_test`, `sep_km_wipe_state_test` | Complete |
| KPV_SCRAMBLER_CTRL | 0x884 | ✅ Covered | `sep_km_kpv_scrambler_test`, `sep_km_wipe_state_test` | Complete |

### 1.3 KPVLP Registers (km_kpv_kpvlp.rdl)

| Register | Offset | Test Coverage | Tests | Status |
|----------|--------|---------------|-------|--------|
| KPVLP_KEY_ENTRY[32].WORD[16] | 0x000-0x7FF | ✅ Covered | `sep_km_kpvlp_basic_write_test`, `sep_km_kpvlp_all_slots_test` | Complete |
| KPVLP_CTRL[32] | 0x800-0x87C | ✅ Covered | `sep_km_kpvlp_basic_write_test` | Complete |
| KPVLP_STATUS | 0x880 | ✅ Covered | `sep_km_kpvlp_status_test` | Complete |

### 1.4 Mailbox KM Side Registers (km_mailbox_km.rdl)

| Register | Offset | Test Coverage | Tests | Status |
|----------|--------|---------------|-------|--------|
| KM_WRITE_DATA | 0x000 | ✅ Covered | `sep_km_mailbox_resp_test` | Complete |
| KM_WRITE_SEPARATOR | 0x004 | ✅ Covered | `sep_km_mailbox_separator_test` | Complete |
| KM_READ_DATA | 0x008 | ✅ Covered | `sep_km_mailbox_cmd_test` | Complete |
| KM_STATUS | 0x00C | ✅ Covered | `sep_km_mailbox_separator_test`, `sep_km_mailbox_overflow_test`, `sep_km_mailbox_protocol_error_test` | Complete |
| KM_IRQ_STATUS | 0x010 | ✅ Covered | `sep_km_mailbox_irq_test` | Complete |
| KM_IRQ_ENABLE | 0x014 | ✅ Covered | `sep_km_mailbox_irq_test` | Complete |
| KM_CTRL | 0x018 | ✅ Covered | `sep_km_mailbox_flush_test` | Complete |

### 1.5 Mailbox SEP Side Registers (km_mailbox_sep.rdl)

| Register | Offset | Test Coverage | Tests | Status |
|----------|--------|---------------|-------|--------|
| SEP_WRITE_DATA | 0x000 | ✅ Covered | `sep_km_mailbox_cmd_test` | Complete |
| SEP_WRITE_SEPARATOR | 0x004 | ✅ Covered | `sep_km_mailbox_separator_test` | Complete |
| SEP_READ_DATA | 0x008 | ✅ Covered | `sep_km_mailbox_resp_test` | Complete |
| SEP_STATUS | 0x00C | ✅ Covered | `sep_km_mailbox_separator_test`, `sep_km_mailbox_overflow_test`, `sep_km_mailbox_protocol_error_test` | Complete |
| SEP_IRQ_STATUS | 0x010 | ✅ Covered | `sep_km_mailbox_irq_test` | Complete |
| SEP_IRQ_ENABLE | 0x014 | ✅ Covered | `sep_km_mailbox_irq_test` | Complete |
| SEP_CTRL | 0x018 | ✅ Covered | `sep_km_mailbox_flush_test` | Complete |

### 1.6 DRBG Sampler Registers (km_drbg_sampler.rdl)

| Register | Offset | Test Coverage | Tests | Status |
|----------|--------|---------------|-------|--------|
| DATA | 0x000 | ✅ Covered | `sep_km_drbg_basic_test`, `sep_km_drbg_tstrb_assembly_test` | Complete |
| CFG | 0x004 | ✅ Covered | `sep_km_drbg_prefetch_test`, `sep_km_drbg_timeout_test` | Complete |
| STATUS | 0x008 | ✅ Covered | `sep_km_drbg_basic_test`, `sep_km_drbg_timeout_test`, `sep_km_drbg_counter_clear_test` | Complete |
| PREFETCH_DATA | 0x00C | ✅ Covered | `sep_km_drbg_prefetch_test`, `sep_km_drbg_tstrb_assembly_test` | Complete |

### 1.7 Crypto Engine Ports (key_manager.rdl)

| Port | Address | Test Coverage | Tests | Status |
|------|---------|---------------|-------|--------|
| OTBN | 0x0001_8000 | ✅ Covered | `sep_km_key_transfer_otbn_test` | Complete |
| AES | 0x0001_9000 | ✅ Covered | `sep_km_key_transfer_aes_test` | Complete |
| KMAC | 0x0001_A000 | ✅ Covered | `sep_km_key_transfer_kmac_test` | Complete |
| HMAC | 0x0001_B000 | ✅ Covered | `sep_km_key_transfer_hmac_test` | Complete |

> **Verification Method (updated v6.2)**: Key transfer verification uses a concurrent **AXI-Lite bus monitor** (`km_key_transfer_with_bus_chk` in `sep_km_uvm_base_test_seq`). The monitor samples the AW-channel (write address) and W-channel (write data) on the target engine's `km_axil_req_t` bus, decodes (aw.addr, w.data) pairs into KEY_SHARE0 and KEY_SHARE1 word writes, and verifies `SHARE0[w] ^ SHARE1[w] == key_data[w]` for all transferred words. The B-channel is monitored for SLVERR/DECERR. Non-target engine buses are checked for AW isolation. This provides bus-level end-to-end data integrity without requiring HDL backdoor access to the accelerator storage registers.
>
> **Known artifact**: KM firmware uses C bitfield access for KEY_CTRL writes (`KEY_CTRL_REG.f.key_valid = N`), which causes GCC to emit a Read-Modify-Write sequence. This generates a spurious AR transaction on the write-only AXI-Lite port. The AR touches only the non-sensitive KEY_CTRL register — key share data is never read back. This has no functional or security impact; the bus monitor does not assert on AR transactions.

### 1.8 Crypto Engine Key Storage Registers

| Register | Engine | Test Coverage | Tests | Status |
|----------|--------|---------------|-------|--------|
| KEY_SHARE0 | OTBN | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_otbn_test` | Complete |
| KEY_SHARE1 | OTBN | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_otbn_test` | Complete |
| KEY_CTRL | OTBN | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_otbn_test` | Complete |
| KEY_SHARE0 | AES | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_aes_test` | Complete |
| KEY_SHARE1 | AES | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_aes_test` | Complete |
| KEY_CTRL | AES | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_aes_test` | Complete |
| KEY_SHARE0 | KMAC | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_kmac_test` | Complete |
| KEY_SHARE1 | KMAC | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_kmac_test` | Complete |
| KEY_CTRL | KMAC | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_kmac_test` | Complete |
| KEY_SHARE0 | HMAC | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_hmac_test` | Complete |
| KEY_SHARE1 | HMAC | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_hmac_test` | Complete |
| KEY_CTRL | HMAC | ✅ Covered | `sep_km_crypto_key_storage_test`, `sep_km_key_transfer_hmac_test` | Complete |

### 1.9 Module-Level Inputs/Outputs

| Signal | Direction | Test Coverage | Tests | Status |
|--------|-----------|---------------|-------|--------|
| wipe_state | Input | ✅ Covered | `sep_km_wipe_state_test`, `sep_km_wipe_irq_test` | Complete |
| otp_data_i | Input | ✅ Covered | `sep_km_otp_data_test` | Complete |
| otp_data_wr_i | Input | ✅ Covered | `sep_km_otp_data_test`, `sep_km_otp_single_shot_test` | Complete |
| cpu_trap_o | Output | ✅ Covered | `sep_km_error_output_test` | Complete |
| recoverable_err_o | Output | ✅ Covered | `sep_km_error_output_test`, `sep_km_kmcsr_recoverable_err_test` | Complete |

### 1.10 SEP Interconnect and System Context (New in v3.2)

| Item | Source | Status | Notes |
|------|--------|--------|-------|
| SEP-facing address (0x1092_0000, 4 KB) | SEP Memory Map | ✅ Documented | Added to Design Spec v3.1 and Registers v3.1 |
| Crypto Periph master access (CPU LSU, CPU DBG, DMA, System I/F) | SEP Interconnect Table | ✅ Documented | Added to Design Spec v3.1 and Test Plan v3.2 |
| Dual-bus crypto architecture (KM masters secret key bus) | Crypto Subsystem Architecture | ✅ Documented | Added to Design Spec v3.1 and Test Plan v3.2 |
| KPV concurrent access restriction (KM port vs KPVLP) | KPV Specification | ✅ Documented | Added to Design Spec v3.1 and Test Plan v3.2 |
| Demote IRQ source (debug in product LC mode) | KMCSR IRQ Sources | ⏳ Deferred | Not in current RDL; planned for Phase 3 |

### 1.11 Firmware Command Set (KM Firmware Specification)

| Command | ID | Test Coverage | Tests | Status |
|---------|-----|---------------|-------|--------|
| CMD_HW_VER | 0x00 | ✅ Covered | `sep_km_cmd_hw_ver_test` | Complete |
| CMD_ROM_VER | 0x01 | ✅ Covered | `sep_km_cmd_rom_ver_test` | Complete |
| CMD_SRAM_VER | 0x02 | ✅ Covered | `sep_km_cmd_sram_ver_test` | Complete |
| CMD_STAT | 0x03 | ✅ Covered | `sep_km_cmd_stat_test` | Complete |
| CMD_RECOV_ACK | 0x04 | ✅ Covered | `sep_km_cmd_recov_ack_test`, `sep_km_recov_fault_flow_test` | Complete |
| CMD_KPVLP_SLOT_REQ | 0x20 | ✅ Covered | `sep_km_cmd_kpvlp_slot_req_test` | Complete |
| CMD_KPVLP_KEY_REGISTER | 0x21 | ✅ Covered | `sep_km_cmd_kpvlp_key_register_test` | Complete |
| CMD_KEY_GENERATE | 0x22 | ✅ Covered | `sep_km_cmd_key_generate_test`, `sep_km_key_generate_all_engines_test` | Complete |
| CMD_KEY_REVOKE | 0x23 | ✅ Covered | `sep_km_cmd_key_revoke_test` | Complete |
| CMD_KEY_TRANSFER | 0x24 | ✅ Covered | `sep_km_cmd_key_transfer_test` | Complete |
| CMD_ENGINE_SHRED | 0x25 | ✅ Covered | `sep_km_cmd_engine_shred_test`, `sep_km_engine_shred_verify_test` | Complete |
| CMD_EXEC_ROM | 0x05 | ⛔ Blocked | `sep_km_cmd_exec_rom_test` | **WIP — firmware not implemented** |
| CMD_FIRM | 0x06 | ⛔ Blocked | `sep_km_cmd_firm_test` | **WIP — firmware not implemented** |

### 1.12 Firmware Response Set

| Response | ID | Test Coverage | Tests | Status |
|----------|-----|---------------|-------|--------|
| RESP_CMD | 0x00 | ✅ Covered | All command tests | Complete |
| RESP_KM_READY | 0x55 | ✅ Covered | `sep_km_resp_km_ready_test`, `sep_km_boot_sequence_test` | Complete |
| RESP_RECOVERABLE_FAULT | 0xFE | ✅ Covered | `sep_km_resp_recoverable_fault_test`, `sep_km_recov_fault_flow_test` | Complete |
| RESP_UNRECOVERABLE_FAULT | 0xFF | ✅ Covered | `sep_km_resp_unrecoverable_fault_test`, `sep_km_unrecov_fault_flow_test` | Complete |

### 1.13 Firmware Protocol Validation

| Feature | Source | Test Coverage | Tests | Status |
|---------|--------|---------------|-------|--------|
| CRC-8/ROHC header | FW Spec | ✅ Covered | `sep_km_msg_header_crc_test` | Complete |
| CRC-32C payload | FW Spec | ✅ Covered | `sep_km_msg_payload_crc_test` | Complete |
| Sequence number | FW Spec | ✅ Covered | `sep_km_msg_seq_num_test` | Complete |
| Invalid command ID | FW Spec | ✅ Covered | `sep_km_msg_invalid_cmd_test` | Complete |
| Payload length check | FW Spec | ✅ Covered | `sep_km_msg_invalid_len_test` | Complete |
| Key handle lifecycle | FW Spec | ✅ Covered | `sep_km_key_handle_lifecycle_test` | Complete |
| Key CRC-32C integrity | FW Spec | ✅ Covered | `sep_km_key_integrity_check_test` | Complete |

### 1.14 Firmware Operational Details (New in v5.3)

| Detail | Source | Test Coverage | Tests | Status |
|--------|--------|---------------|-------|--------|
| Key share XOR masking (SHARE0=RAND, SHARE1=KEY⊕RAND) | FW Spec | ✅ Covered | `sep_km_key_transfer_{aes,hmac,kmac,otbn}_test` (bus monitor W-channel); `sep_km_cmd_key_transfer_test` (HDL backdoor, enriched) | Complete |
| DRBG-seeded random slot selection | FW Spec | ✅ Covered | `sep_km_cmd_kpvlp_slot_req_test`, `sep_km_cmd_key_generate_test` (enriched) | Complete |
| Key registration anti-tampering (pre/post copy) | FW Spec | ✅ Covered | `sep_km_cmd_kpvlp_key_register_test` (enriched) | Complete |
| KPV shred skip write-locked slots | FW Spec | ✅ Covered | Coverage point `cp_fw_shred_kpv_skip_write_locked` | Complete |
| DRBG Get Block prefetch enable/disable | FW Spec | ✅ Covered | Coverage point `cp_fw_drbg_get_block_prefetch` | Complete |
| Mailbox ISR error priority order | FW Spec | ✅ Covered | Coverage points `cp_fw_mbox_isr_error_priority`, `cp_fw_mbox_isr_direct_fifo_write` | Complete |
| SEP Reset Controller (0x10A5_0000) | OCH Spec v0.87 | ✅ Documented | Design Spec §SEP Reset Controller | N/A (system-level) |
| OTP differential encoding violation | OCH Spec v0.87 | ⏳ Deferred | N/A | Future — depends on OTP controller behavior, not KM-specific |

### 1.15 PCPI CRC Accelerator Instructions (spec 2130)

| Feature | Source | Test Coverage | Tests | Status |
|---------|--------|---------------|-------|--------|
| CRC-32C word-update instruction | specs/2130 | ✅ Covered | `sep_km_crc32c_word_update_test` | Complete |
| CRC-32C byte-update instruction | specs/2130 | ✅ Covered | `sep_km_crc32c_byte_update_test` | Complete |
| CRC-8/ROHC byte-update instruction | specs/2130 | ✅ Covered | `sep_km_crc8_rohc_update_test` | Complete |
| Mixed word + byte CRC-32C chaining | specs/2130 | ✅ Covered | `sep_km_crc_mixed_chaining_test` | Complete |
| Externally visible behavior preserved | specs/2130 | ✅ Covered | `sep_km_crc_compatibility_test` | Complete |
| Unrecognized PCPI encoding (illegal-insn) | specs/2130 | ✅ Covered | Coverage point `cp_fw_crc_unrecognized_encoding` | Complete |
| Performance improvement (≥50% cycle reduction) | specs/2130 | ✅ Covered | Coverage point `cp_fw_crc_performance` | Complete |

---

## Part 2: Feature Coverage Summary

### 2.1 Phase 1 Features (P0 - First Release Critical)

| Feature | RDL Source | Test Coverage | Priority | Status |
|---------|------------|---------------|----------|--------|
| KPVLP key provisioning | km_kpv_kpvlp.rdl | ✅ Full | P0 | Complete |
| UNLOCK_SEP flow | km_kpv.rdl | ✅ Full | P0 | Complete |
| Key transfer to AES | key_manager.rdl | ✅ Full | P0 | Complete |
| Key transfer to KMAC | key_manager.rdl | ✅ Full | P0 | Complete |
| Key transfer to OTBN | key_manager.rdl | ✅ Full | P0 | Complete |
| Key transfer to HMAC | key_manager.rdl | ✅ Full | P0 | Complete |
| Mailbox command/response | km_mailbox_*.rdl | ✅ Full | P0 | Complete |
| Reset state | All RDLs | ✅ Full | P0 | Complete |

**Phase 1 Coverage: 100%**

### 2.2 Phase 2 Features (P1/P2)

| Feature | RDL Source | Test Coverage | Priority | Status |
|---------|------------|---------------|----------|--------|
| KPV entry access (KM side) | km_kpv.rdl | ✅ Full | P1 | Complete |
| KPV control registers | km_kpv.rdl | ✅ Full | P1 | Complete |
| LOCK_WRITE protection | km_kpv.rdl | ✅ Full | P1 | Complete |
| LOCK_USE protection | km_kpv.rdl | ✅ Full | P1 | Complete |
| KPV scrambler | km_kpv.rdl | ✅ Full | P1 | Complete |
| EXTEND field | km_kpv.rdl | ✅ Full | P1 | Complete |
| DEST_VALID enforcement | km_kpv.rdl | ✅ Full | P1 | Complete |
| LAST_DWORD configuration | km_kpv.rdl | ✅ Full | P1 | Complete |
| Message separator | km_mailbox_*.rdl | ✅ Full | P1 | Complete |
| FIFO overflow/underflow | km_mailbox_*.rdl | ✅ Full | P1 | Complete |
| Mailbox flush | km_mailbox_*.rdl | ✅ Full | P1 | Complete |
| Mailbox IRQs | km_mailbox_*.rdl | ✅ Full | P1 | Complete |
| KMCSR VERSION | km_csr.rdl | ✅ Full | P1 | Complete |
| KMCSR IRQ handling | km_csr.rdl | ✅ Full | P1 | Complete |
| SOFT_RST_CODE | km_csr.rdl | ✅ Full | P1 | Complete |
| RECOVERABLE_ERR register | km_csr.rdl | ✅ Full | P1 | Complete |
| DRBG Sampler | km_drbg_sampler.rdl | ✅ Full | P1 | Complete |
| SEP OTP Data | km_csr.rdl | ✅ Full | P1 | Complete |
| Wipe State | km_kpv.rdl, km_csr.rdl | ✅ Full | P1 | Complete |
| Error condition outputs | key_manager.rdl | ✅ Full | P1 | Complete |
| SRAM scrambler | km_csr.rdl | ✅ Full | P2 | Complete |
| SRAM lock protection | km_csr.rdl | ✅ Full | P2 | Complete |
| Parity error detection | km_csr.rdl | ✅ Full | P2 | Complete |
| AXI error detection | km_csr.rdl | ✅ Full | P2 | Complete |
| Virtual UART | km_csr.rdl | ✅ Full | P2 | Complete |
| Test protocol | km_csr.rdl | ✅ Full | P2 | Complete |

**Phase 2 Coverage: 100%**

---

### 2.3 Behavioral/Security Coverage Summary

| Behavior | Source | Test Coverage | Status |
|----------|--------|---------------|--------|
| No-reset datapath reset expectations | FR-0000-149 | `sep_km_reset_test` (updated checks) | ✅ Covered |
| Mailbox malformed/illegal command rejection | Mailbox firewall behavior | `sep_km_mailbox_protocol_error_test` | ✅ Covered |
| DRBG partial-byte stream assembly (TSTRB != 0xF) | FR-0000-124 | `sep_km_drbg_tstrb_assembly_test` | ✅ Covered |
| DRBG counter clear-on-nonzero semantics | FR-0000-128 | `sep_km_drbg_counter_clear_test` | ✅ Covered |
| OTP write-strobe latch and no-reset behavior | FR-0000-138, FR-0000-140 | `sep_km_otp_single_shot_test` | ✅ Covered |
| OTP_DEMOTION_STATE two-field layout | km_csr.rdl | `sep_km_otp_data_test` | ✅ Covered (DEMOTE_1_VALUE, DEMOTE_2_VALUE) |
| Firmware command/response protocol (CRC, seq) | FW Spec | `sep_km_msg_*_test` (5 tests) | ✅ Covered |
| Key handle lifecycle (alloc→register→transfer→revoke) | FW Spec | `sep_km_key_handle_lifecycle_test` | ✅ Covered |
| Recoverable fault command filtering | FW Spec | `sep_km_recov_fault_cmd_filter_test` | ✅ Covered |
| Unrecoverable fault SRAM shred | FW Spec | `sep_km_unrecov_fault_flow_test` | ✅ Covered |

### 2.4 Firmware Feature Coverage

| Feature | Source | Test Coverage | Priority | Status |
|---------|--------|---------------|----------|--------|
| Command message format | FW Spec | ✅ Full | P2 | Complete |
| All 11 commands | FW Spec | ✅ Full | P2 | Complete |
| All 4 response types | FW Spec | ✅ Full | P2 | Complete |
| All 8 return codes | FW Spec | ✅ Full | P2 | Complete |
| Key handle/registry | FW Spec | ✅ Full | P2 | Complete |
| Boot sequence | FW Spec | ✅ Full | P2 | Complete |
| Recoverable fault flow | FW Spec | ✅ Full | P2 | Complete |
| Unrecoverable fault flow | FW Spec | ✅ Full | P2 | Complete |
| Engine shred operations | FW Spec | ✅ Full | P2 | Complete |
| SEP key loading end-to-end | FW Spec | ✅ Full | P2 | Complete |
| Key share XOR masking | FW Spec | ✅ Full | P2 | Complete |
| Random slot selection (DRBG-seeded) | FW Spec | ✅ Full | P2 | Complete |
| Key registration anti-tampering | FW Spec | ✅ Full | P2 | Complete |
| KPV shred skip write-locked | FW Spec | ✅ Full | P2 | Complete |
| Mailbox ISR error priority | FW Spec | ✅ Full | P2 | Complete |
| PCPI CRC-32C word-update | specs/2130 | ✅ Full | P2 | Complete |
| PCPI CRC-32C byte-update | specs/2130 | ✅ Full | P2 | Complete |
| PCPI CRC-8/ROHC update | specs/2130 | ✅ Full | P2 | Complete |
| PCPI CRC mixed chaining | specs/2130 | ✅ Full | P2 | Complete |
| PCPI CRC external compatibility | specs/2130 | ✅ Full | P2 | Complete |
| CMD_EXEC_ROM ROM execution mode lock | FW Spec | ⛔ Blocked | P2 | **WIP — firmware not implemented** |
| CMD_FIRM SRAM firmware loading | FW Spec | ⛔ Blocked | P2 | **WIP — firmware not implemented** |
| CMD_FIRM CRC integrity check (from SRAM readback) | FW Spec | ⛔ Blocked | P2 | **WIP — firmware not implemented** |

**Firmware Coverage: ~93%** (2 commands blocked by WIP firmware)

---

## Part 3: Deferred Items (Not in Current RDL)

The following items from the original test plan (v1.x) have been removed as they are not present in the current RDL specifications and are planned for future phases:

| Deferred Feature | Original Tests | Reason |
|-----------------|----------------|--------|
| Life Cycle State Handling | `sep_km_otp_lc_test`, `sep_km_prod_dbg*_test` | Firmware-level, not in current RDL - Phase 3 |
| Security Domains | `sep_km_security_domain_test` | Not in current RDL - Phase 3 |
| RMA Dual Token | `sep_km_rma_dual_token_test` | Not in current RDL - Phase 3 |
| KDF (Key Derivation) | `sep_km_kdf_*` | Not in current RDL - Phase 2 |
| Public Key Generation | `sep_km_pk_gen_*` | Not in current RDL - Phase 2 |
| Attestation | `sep_km_attestation_*`, `sep_km_pcrv_test` | Not in current RDL - Phase 3 |

**Note**: These features will be added to the test plan when the corresponding RDL/RTL is implemented.

---

## Part 4: Test Count Summary

### 4.1 By Priority

| Priority | Count | Description |
|----------|-------|-------------|
| P0 | 10 | Phase 1 Critical - First release requirement |
| P1 | 36 | Important functionality |
| P2 | 49 | Extended/advanced + firmware verification |
| **Total** | **95** | |

### 4.2 By Category

| Category | Count | Priority Range |
|----------|-------|----------------|
| Smoke | 1 | P0 |
| Reset | 2 | P0, P2 |
| KPVLP | 5 | P0, P1 |
| Key Transfer | 4 | P0 |
| KPV | 8 | P1 |
| Mailbox | 8 | P0, P1 |
| KMCSR | 7 | P1 |
| DRBG Sampler | 6 | P1 |
| OTP Data | 2 | P1, P2 |
| Wipe State | 2 | P1 |
| Error Outputs | 1 | P1 |
| Scrambler | 3 | P2 |
| SRAM Protection | 2 | P2 |
| Error Detection | 3 | P2 |
| Debug/Test | 3 | P2 |
| Stress/Random | 2 | P2 |
| FW Command | 12 | P2 |
| FW Response | 3 | P2 |
| FW Protocol | 5 | P2 |
| FW Key Mgmt | 3 | P2 |
| FW Fault | 3 | P2 |
| FW E2E | 3 | P2 |
| FW CRC | 5 | P2 |
| **Total** | **95** | |

---

## Part 5: Remaining Gaps and Open Questions

### 5.1 Current Gap Status

All registers and features defined in the current RDL specifications have corresponding test coverage, with the following exceptions:

- **SEP Integration Caveat**: The standalone KM component contains DRBG and wipe-state support, but the current `hw/sep/sep_crypto.sv` integration ties off the DRBG AXI-Stream input and drives `wipe_state_i` low. SEP-level verification of those paths therefore depends on future integration enablement or dedicated stubbing.

- **KMCSR AXI Accessibility Constraint** *(confirmed 2026-03-17, `sep_km_uvm_reset_test`)*:
  KMCSR registers at KM-internal `0x0000_E000` are **not reachable via any AXI VIP master**
  in the SEP testbench. All three VIP masters (`cpu_lsu_sqr`, `cpu_dbg_sqr`, `ext_axi_sqr`)
  route through the SEP AXI fabric, which maps the KM only at `0x1092_0000` (Mailbox SEP
  side + KPVLP). Reads to KM-internal addresses return `DECERR`. KMCSR test sequences must
  use UVM HDL backdoor (`uvm_hdl_read` on `field_storage` in `km_csr_reg`) or the firmware
  TB protocol (TB_CMD/TB_RESULT registers) instead. See CLAUDE.md for full paths and usage.

All other Phase 1/2 scope features have **no known functional gaps**.

### 5.2 Open Questions

| ID | Question | Impact | Status |
|----|----------|--------|--------|
| Q-001 | What is the exact mailbox command format? | Affects mailbox protocol-error vectors | **RESOLVED** — KM Firmware Spec provides complete command message format (CRC-8/ROHC header, CRC-32C payload, sequence numbers) |
| Q-002 | How to inject parity errors in simulation? | Affects parity tests | Use TB_CMD protocol |
| Q-003 | What are the supported function_id values? | Affects command tests | **RESOLVED** — KM Firmware Spec defines 12 command IDs: CMD_HW_VER (0x00), CMD_ROM_VER (0x01), CMD_SRAM_VER (0x02), CMD_STAT (0x03), CMD_RECOV_ACK (0x04), CMD_EXEC_ROM (0x05, **WIP**), CMD_FIRM (0x06, **WIP**), CMD_KPVLP_SLOT_REQ (0x20), CMD_KPVLP_KEY_REGISTER (0x21), CMD_KEY_GENERATE (0x22), CMD_KEY_REVOKE (0x23), CMD_KEY_TRANSFER (0x24), CMD_ENGINE_SHRED (0x25) |
| Q-004 | Is crypto engine key storage readable for verification? | Affects key transfer verification | Need RTL clarification |
| Q-005 | How to trigger CPU trap for cpu_trap_o test? | Affects error output test | Need firmware/testbench coordination |
| Q-007 | Can KMCSR registers be read via AXI VIP from the SEP testbench? | Affects all KMCSR test sequences | **RESOLVED** — No. `key_manager` exposes only Mailbox SEP side and KPVLP as external AXI slaves. Any read to `0x0000_E000` returns DECERR. Use `uvm_hdl_read` on `km_csr_reg.field_storage` (backdoor) or firmware TB protocol after boot. |
| Q-006 | What is the DRBG AXI-Stream interface behavior? | Affects DRBG TSTRB/stream-error tests | Need DRBG VIP implementation |
| Q-007 | How is the 4 KB window at 0x1092_0000 sub-addressed between Mailbox SEP side and KPVLP? | Affects SEP-side address constants in test sequences | Need integration specification |
| Q-008 | Can DMA or System I/F access KM's Mailbox SEP side / KPVLP? What are the access control restrictions? | Affects multi-master access tests | Need integration/security specification |
| Q-009 | When is the Demote IRQ source expected to be added to the KM RDL? | Affects IRQ coverage completeness | Planned for Phase 3 |
| Q-010 | Some DV docs still describe OTP as read-through, but the authoritative KM spec now requires strobe-based latching plus an OTP write-count register per FR-0000-138 and FR-0000-141. Do the remaining DV docs need alignment? | Doc/spec inconsistency; no RTL impact | Flagged for spec owner |
| Q-011 | When will DRBG and `wipe_state` be fully wired through `sep_crypto` so SEP-level tests can exercise the live integration path instead of the standalone KM component behavior? | Affects DRBG and wipe-state verification closure at the SEP top level | Pending integration work |
| Q-012 | What is the exact SHRED_ITER value for production firmware? Affects shred pass count verification. | Pending firmware configuration |
| Q-013 | What is the ROM firmware version (MAJOR/MINOR/PATCH) that CMD_ROM_VER should return? | Pending firmware build |
| Q-014 | OCH Spec v0.87 states "any differentially encoded register fields are checked for validity" with errors forwarded to IRQ_STATUS. Does the KM hardware perform differential encoding validation on OTP life-cycle/demotion fields, or is this purely an OTP controller responsibility? | Affects whether KM-level tests need differential encoding violation vectors | Need RTL/spec clarification |
| Q-015 | OCH Spec v0.87 describes 36 OTP registers (3 LC domains: Chiplet, Package, System) vs current RDL with 34 (1 combined LC). When will the per-domain LC registers be implemented? | Affects OTP test coverage completeness | Known discrepancy — OCH spec predates RDL consolidation |
| Q-016 | ~~CMD_FIRM_LOAD (0x05)~~ **Corrected**: The KM Firmware Spec defines CMD_EXEC_ROM (0x05) and CMD_FIRM (0x06), both WIP. `specs/main-spec-km.md` still marks dynamic SRAM firmware loading as reserved/future. Which document is authoritative? | Affects whether CMD_EXEC_ROM/CMD_FIRM tests should be exercised against current firmware | **PARTIALLY RESOLVED** — KM Firmware Spec is authoritative, but both commands are **WIP and not implemented in ROM firmware** (`rom_cmd.c` dispatches 0x05/0x06 to default `RC_INVALID_CMD`). Tests are BLOCKED until firmware is updated. |
| Q-017 | KMCSR.VERSION `field_storage` HDL backdoor: `uvm_hdl_read` on `field_storage.VERSION` fails because VERSION is a hardwired constant (no storage flops). How should tests verify KMCSR.VERSION? | Affects `sep_km_cmd_hw_ver_test` verification method | **RESOLVED** — Use RETURN_ARG from CMD_HW_VER mailbox response. Verify RETURN_ARG[23:0] is non-zero and consistent across iterations. HDL backdoor is not applicable for VERSION. |
| Q-018 | SRAM scrambling affects HDL backdoor reads: `uvm_hdl_read` on KM SRAM (`u_km_sram.gen_ram_inst[0].u_mem.mem[N]`) returns scrambled data, not plaintext firmware. How should `sep_km_cmd_firm_test` verify loaded firmware content? | Affects CMD_FIRM SRAM content verification | Open — per KM Firmware Spec, CRC is computed from SRAM readback (post-scrambling), so the firmware's own CRC check validates integrity. For DV, either (a) descramble in TB, (b) use firmware-side TB protocol to readback, or (c) rely on CRC pass + successful jump as sufficient proof. |
| Q-019 | KM HDL hierarchy paths: correct SRAM path is `SEP_IP_INT_PATH_STR.u_km_sram.gen_ram_inst[0].u_mem.mem[N]`; correct unrecoverable error signal is `SEP_DUT_PATH_STR.sep_crypto.u_key_manager.unrecoverable_err_o`. Are these paths stable across future RTL refactors? | Affects maintainability of HDL backdoor reads in test sequences | Open — consider adding RTL path defines or documentation for DV-accessible signals |

### 5.3 Firmware Dependencies

| Dependency | Impact | Mitigation |
|------------|--------|------------|
| Production firmware availability | Tests require firmware implementing the complete command set | **Partial** — ROM firmware implements 10 of 12 commands. CMD_EXEC_ROM (0x05) and CMD_FIRM (0x06) are **WIP — not implemented** |
| Mailbox command definitions | All 12 commands and message format now defined | **RESOLVED** — KM Firmware Spec (2 WIP) |
| Key transfer command format | CMD_KEY_TRANSFER uses KEY_HANDLE + DEST_ENGINE | **RESOLVED** — KM Firmware Spec |
| RECOVERABLE_ERR handling | Set by firmware ISR, cleared by CMD_RECOV_ACK | **RESOLVED** — KM Firmware Spec |

### 5.4 Testbench Dependencies

| Dependency | Impact | Mitigation |
|------------|--------|------------|
| DRBG AXI-Stream VIP | DRBG tests require AXI-Stream stimulus | Implement or integrate DRBG VIP |
| OTP interface driver | OTP tests require otp_data_i/otp_data_wr_i stimulus | Implement OTP interface driver |
| Wipe state driver | Wipe tests require wipe_state stimulus | Implement wipe state driver |

---

## Part 6: Verification Strategy Alignment

### 6.1 Designer Recommendations

| Recommendation | Implementation in Test Plan |
|----------------|----------------------------|
| Use production firmware | ✅ Tests assume production firmware running |
| Focus on key loading and transfer for Phase 1 | ✅ P0 tests focus exclusively on KPVLP→KPV→crypto flow |
| Don't need to verify every KM piece for first drop | ✅ Advanced features (scrambler, parity) are P2 |
| Eventually want custom firmware capability | ✅ Test protocol registers support firmware-testbench communication |

### 6.2 Verification Environment

```
+------------------+     +------------------+     +------------------+
|   SEP VIP        |     |  Key Manager     |     | Crypto Engine    |
|  (AXI4-Lite)     |     |    (DUT)         |     |   Monitors       |
+--------+---------+     +--------+---------+     +--------+---------+
         |                        |                        |
         v                        v                        v
    +---------+              +---------+              +---------+
    | KPVLP   |              | KM CPU  |              | OTBN    |
    | Mailbox |              | ROM     |              | AES     |
    | SEP     |              | SRAM    |              | KMAC    |
    +---------+              | KPV     |              | HMAC    |
                             | KMCSR   |              +---------+
                             | DRBG    |
                             | Mailbox |
                             +---------+
                                  ^
                                  |
                         +-------+-------+
                         |               |
                    +---------+     +---------+
                    | OTP VIP |     | DRBG VIP|
                    +---------+     +---------+
                         ^               ^
                         |               |
                    otp_data_i      AXI-Stream
                    otp_data_wr_i
                    wipe_state
```

---

## Part 7: Conclusion

### 7.1 Coverage Summary

| Scope | Coverage |
|-------|----------|
| Phase 1 (P0) Features | **100%** |
| Phase 2 (P1/P2) Features | **100%** |
| All RDL-defined registers | **100%** |
| Behavioral/Security coverage (in current scope) | **100%** |

### 7.2 Test Plan Status

- **Total Tests**: 94
- **Phase 1 Critical (P0)**: 10 tests
- **Important (P1)**: 36 tests
- **Extended/Firmware (P2)**: 48 tests

### 7.3 Key Changes in v3.0/v3.1

1. **DRBG Sampler**: Added full coverage for km_drbg_sampler.rdl (4 tests)
2. **SEP OTP Data**: Added coverage for OTP data registers (1 test)
3. **Wipe State**: Added coverage for wipe state functionality (2 tests)
4. **KPV Scrambler**: Added coverage for KPV-specific scrambler (1 test)
5. **RECOVERABLE_ERR**: Added coverage for recoverable error register and output (1 test)
6. **Error Outputs**: Added coverage for cpu_trap_o and recoverable_err_o (1 test)
7. **Removed CLEAR test**: CLEAR bit no longer exists in KPV CTRL (now Reserved)
8. **Updated traceability**: Added DRBG Sampler and module-level I/O sections
9. **Updated testbench dependencies**: Added DRBG VIP, OTP driver, wipe state driver requirements
10. **Behavioral coverage closure**: Added mailbox protocol-error, DRBG TSTRB assembly, DRBG counter clear semantics, OTP single-shot strobe, and no-reset reset-check alignment

### 7.4 Key Changes in v3.2

1. **SEP-facing address**: Documented KM at 0x1092_0000 (4 KB) in SEP memory map
2. **SEP Interconnect**: Documented Crypto Periph master access (CPU LSU, CPU DBG, DMA, System I/F)
3. **Dual-bus crypto architecture**: Documented KM as sole master on secret key bus
4. **KPV concurrent access restriction**: Documented that KPV does not arbitrate concurrent KM port / KPVLP access
5. **Demote IRQ**: Noted as deferred (Phase 3, not in current RDL)
6. **New open questions**: Q-007 (sub-addressing), Q-008 (multi-master access control), Q-009 (Demote IRQ timeline)

### 7.5 Key Changes in v4.0

1. **VERSION register**: Updated for semantic versioning layout (RSVD/MAJOR/MINOR/PATCH, no ID field)
2. **OTP registers**: Consolidated to single OTP_LIFE_CYCLE (0x030), removed per-level registers and OTP_WRITE_COUNT; updated offsets for OTP_DEMOTION_STATE (0x034) and OTP_CHIPLET_UID (0x038-0x0B4)
3. **Mailbox WRITE_SEPARATOR**: Added KM_WRITE_SEPARATOR (0x004) and SEP_WRITE_SEPARATOR (0x004) registers with WRITE_SPACE_AVAIL IRQ; shifted all subsequent mailbox register offsets by +4
4. **Crypto engine key storage**: Added Section 1.8 with register traceability for KEY_SHARE0, KEY_SHARE1, KEY_CTRL across OTBN, AES, KMAC, HMAC
5. **SEP address update**: Updated SEP-facing address from 0x1093_0000 to 0x1092_0000
6. **Test count**: Updated from 59 to 61 (P1: 33→35)

### 7.6 Key Changes in v4.1

1. **OTP_DEMOTION_STATE**: Updated from single 2-bit VALUE field to two separate 2-bit fields (DEMOTE_1_VALUE[1:0], DEMOTE_2_VALUE[3:2]) per latest km_csr.rdl
2. **HMAC Key Storage NOT instantiated**: hmac_wrapper_key_reg is NOT in key_manager.rdl top-level address map; HMAC uses software-writable KEY registers. HMAC crypto engine key storage registers marked N/A in traceability. sep_km_key_transfer_hmac_test updated to verify DECERR at unmapped 0x0001_B000
3. **Spec/doc inconsistency flagged**: some DV docs still describe OTP read-through behavior, but the authoritative KM spec uses strobe-based latching and OTP write-count semantics (Q-010)
4. **Coverage points**: OTP Data expanded from 4 to 6 points (split demotion into two fields). Total: 116 (was 114)
5. **New open questions**: Q-010 (OTP spec stale text), Q-011 (HMAC sideload future plans)

### 7.7 Key Changes in v4.2

1. **HMAC correction**: Updated the analysis to match the current codebase, where HMAC key storage is instantiated in `key_manager.rdl`, wired through firmware, and connected in `sep_crypto`
2. **SEP integration caveat**: Replaced the old HMAC architecture question with the current integration limitation that DRBG and `wipe_state` are still tied off in `hw/sep/sep_crypto.sv`

### 7.8 Key Changes in v5.0

1. **KM Firmware Specification**: Added complete traceability for firmware command set (10 commands), response set (4 responses), message protocol (CRC-8/ROHC, CRC-32C, sequence numbers)
2. **Open questions resolved**: Q-001 (mailbox command format) and Q-003 (function_id values) resolved by firmware spec
3. **New firmware test coverage**: 28 new tests covering commands, responses, protocol validation, key management, fault flows, and end-to-end scenarios
4. **Coverage points**: 62 new firmware coverage points (total: 178; later expanded to 191 with RETURN_ARG verification in v5.2)
5. **Firmware dependencies**: 4 of 4 firmware dependencies resolved
6. **Test count**: Updated from 61 to 89 tests
7. **New open questions**: Q-012 (SHRED_ITER value), Q-013 (ROM firmware version)

### 7.9 Key Changes in v5.3

1. **Firmware operational details**: Added Section 1.14 with traceability for 8 newly documented firmware behaviors from full KM Firmware Spec review and OCH Spec v0.87 cross-reference
2. **Key share XOR masking**: Documented and added coverage for SHARE0=RAND, SHARE1=KEY⊕RAND with interleaved random write order
3. **Random slot selection**: Documented DRBG-seeded random base slot selection for CMD_KPVLP_SLOT_REQ and CMD_KEY_GENERATE
4. **Anti-tampering detection**: Documented key registration pre/post copy comparison to detect concurrent KPVLP writes during CMD_KPVLP_KEY_REGISTER
5. **KPV shred skip behavior**: Documented that KPV shred skips write-locked slots
6. **Mailbox ISR error priority**: Documented strict priority order (overflow > underflow > flushed_by_sep) with direct FIFO write bypass for fault responses
7. **SEP Reset Controller**: Added reference to SW_RESET_N register at 0x10A5_0000 (from OCH Spec v0.87)
8. **OTP differential encoding**: Flagged OCH Spec v0.87 requirement for differential encoding violation detection as deferred (Q-014)
9. **OTP register count discrepancy**: Flagged OCH Spec v0.87 showing 36 registers (3 LC domains) vs current RDL with 34 (1 LC) as Q-015
10. **Coverage points**: 10 new points (total: 201, was 191). Test count unchanged: 89 (existing tests enriched)

### 7.10 Key Changes in v6.0

1. **PCPI CRC Accelerator Instructions**: Added Section 1.15 traceability for three hardware CRC instructions (CRC-32C word update, CRC-32C byte update, CRC-8/ROHC byte update) per spec 2130
2. **New test category**: FW CRC (5 tests): word-update, byte-update, CRC-8/ROHC, mixed chaining, compatibility
3. **Firmware feature coverage**: 5 new PCPI CRC entries in Section 2.4
4. **Coverage points**: 15 new points (total: 216, was 201)
5. **Test count**: Updated from 89 to 94 (P2: 43→48)
6. **References**: Added specs/main-spec-km.md and specs/2130-add-pcpi-crc-instructions/spec.md

### 7.11 Key Changes in v6.1

1. **CMD_FIRM_LOAD (0x05)**: Added to firmware command set traceability (Section 1.11). New command for dynamic SRAM firmware loading: 3-word payload (IMAGE_BASE_ADDRESS, IMAGE_SIZE32, JUMP_ADDRESS), reads firmware image from next mailbox frame, performs CRC-32C integrity check, jumps to loaded firmware on success, triggers unrecoverable fault on CRC mismatch, no standard RESP_CMD
2. **New test**: `sep_km_cmd_firm_load_test` (FW Command, P2)
3. **Firmware feature coverage**: 2 new entries in Section 2.4 (CMD_FIRM_LOAD SRAM loading, CMD_FIRM_LOAD CRC integrity)
4. **Coverage points**: 3 new points (total: 219, was 216). Firmware Command: 18→21
5. **Test count**: Updated from 94 to 95 (P2: 48→49, FW Command: 11→12)
6. **Updated recov fault filter**: CMD_FIRM_LOAD added to rejected command list (11 command IDs, was 10)
7. **Spec discrepancy (Q-016)**: CMD_FIRM_LOAD is detailed in the KM Firmware Spec but `specs/main-spec-km.md` still marks dynamic SRAM firmware loading as reserved/future. KM Firmware Spec treated as authoritative.

### 7.12 Key Changes in v7.0

1. **CMD_FIRM_LOAD → CMD_EXEC_ROM + CMD_FIRM**: Replaced single CMD_FIRM_LOAD (0x05) with two commands per KM Firmware Spec:
   - **CMD_EXEC_ROM (0x05)**: 1-word payload (MODE[1:0]), sets ROM execution mode, prevents SRAM load until reset — **WIP**
   - **CMD_FIRM (0x06)**: 3-word payload (MODE[1:0], IMAGE_BASE_ADDRESS[13:0], JUMP_ADDRESS[31:0]) — **WIP**
2. **Key payload changes for CMD_FIRM**: No IMAGE_SIZE32 field (reads mailbox until separator), CRC-32C computed from SRAM readback (post-scrambling, not mailbox data)
3. **Tests**: Replaced `sep_km_cmd_firm_load_test` with `sep_km_cmd_exec_rom_test` + `sep_km_cmd_firm_test` — both BLOCKED (firmware not implemented)
4. **Firmware status**: `rom_cmd.c` dispatches command IDs 0x05 and 0x06 to default `RC_INVALID_CMD`. Both commands are WIP in KM Firmware Spec.
5. **HDL backdoor findings** (from `sep_km_uvm_cmd_firm_load_test` failure analysis):
   - KMCSR.VERSION: hardwired constant, no `field_storage` flops — use RETURN_ARG instead (Q-017, resolved)
   - KM SRAM path: `SEP_IP_INT_PATH_STR.u_km_sram.gen_ram_inst[0].u_mem.mem[N]` (not `key_manager.km_sram.mem`)
   - Unrecoverable error signal: `u_key_manager.unrecoverable_err_o` (not `cpu_trap_o`)
   - SRAM reads return scrambled data — affects content verification (Q-018, open)
6. **Updated counts**: 12 command IDs (was 11), 96 tests (was 95), 222 coverage points (was 219)
7. **Firmware coverage**: Reduced from 100% to ~93% due to 2 blocked WIP commands
8. **Recoverable fault filter**: Updated to 12 rejected command IDs (CMD_EXEC_ROM + CMD_FIRM added)

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-01-21 | - | Initial gap analysis |
| 1.1 | 2026-01-22 | - | Updated for spec v0.5 |
| 2.0 | 2026-02-02 | - | Major revision aligned with Test Plan v2.0: RDL-based coverage, removed Phase 2/3 features not in current RDL, 100% coverage of current scope |
| 3.0 | 2026-02-11 | - | Major update aligned with Test Plan v3.0: Added DRBG Sampler (Section 1.6), SEP OTP Data, Wipe State, KPV Scrambler, RECOVERABLE_ERR, Error Outputs coverage. Updated test counts (45→55). Added testbench dependencies for DRBG VIP, OTP driver, wipe state driver. |
| 3.1 | 2026-02-11 | - | Updated for Test Plan v3.1: added behavioral/security coverage tracking, mailbox protocol-error coverage, DRBG TSTRB/counter-clear coverage, OTP single-shot coverage, and updated counts (55→59). |
| 3.2 | 2026-02-23 | - | Added SEP interconnect and system context section (1.9): SEP-facing address (0x1093_0000), Crypto Periph master access, dual-bus architecture, KPV concurrent access restriction, Demote IRQ (deferred). Added open questions Q-007 through Q-009. |
| 4.0 | 2026-03-03 | - | Major update aligned with Test Plan v4.0. VERSION: updated for semantic versioning (no ID). OTP: consolidated registers (1 life cycle, no write count, new offsets). Mailbox: added WRITE_SEPARATOR registers and WRITE_SPACE_AVAIL IRQ. Added crypto engine key storage register traceability. Updated SEP address to 0x1092_0000. Test count: 61. |
| 4.1 | 2026-03-07 | - | OTP_DEMOTION_STATE: two 2-bit fields (DEMOTE_1_VALUE, DEMOTE_2_VALUE). HMAC: marked NOT instantiated in key_manager.rdl — key storage registers N/A, test updated for DECERR. Flagged spec inconsistency (Q-010). Coverage: 116 points. Added Q-010, Q-011. |
| 4.2 | 2026-03-09 | - | Corrected the gap analysis to match the current implementation: HMAC key storage is instantiated and covered like the other wrapper ports. Replaced the old HMAC architecture question with the current SEP integration caveat that DRBG and `wipe_state` are still tied off in `sep_crypto`. |
| 5.0 | 2026-03-16 | - | Major update for KM Firmware Specification. Added firmware command/response/protocol traceability (Sections 1.11-1.13). Resolved Q-001 and Q-003. Added firmware feature coverage (Section 2.4). Updated test counts (61→89), coverage points (116→178). Resolved 4 firmware dependencies. Added Q-012, Q-013. |
| 5.1 | 2026-03-16 | - | Deprioritized all 28 firmware verification tests from P0/P1 to P2. Rationale: production firmware is the test vehicle, not the DUT; firmware-level verification is lowest priority. Updated priority breakdowns: P0: 10, P1: 36, P2: 43. Total unchanged: 89. |
| 5.2 | 2026-03-18 | - | Aligned with Design Spec v5.1 (KM Firmware document integration). Per-command RESP_CMD RETURN_ARG bit layouts now documented; test plan updated to verify RETURN_ARG values for error codes and success responses. Coverage points expanded: 13 new RETURN_ARG verification points (5 error + 8 success). Total coverage: 191 (was 178). No new tests required; existing test procedures enriched. |
| 5.3 | 2026-03-19 | - | Updated from full KM Firmware Spec review and OCH Spec v0.87 cross-reference. Added Section 1.14 (Firmware Operational Details): key share XOR masking, DRBG-seeded random slot selection, key registration anti-tampering, KPV shred skip write-locked, DRBG Get Block prefetch, mailbox ISR error priority, SEP Reset Controller reference, OTP differential encoding (deferred). Added 5 firmware features to Section 2.4. Added Q-014 (differential encoding validation) and Q-015 (OTP LC register count discrepancy OCH vs RDL). Coverage points: 201 (was 191). Test count: 89 (unchanged — existing tests enriched). |
| 6.0 | 2026-03-26 | - | Added PCPI CRC Accelerator Instructions traceability (Section 1.15, 7 features from specs/2130). Added 5 PCPI CRC entries to Section 2.4 firmware feature coverage. Added FW CRC test category (5 tests). Updated Part 4 counts: P2 43→48, Total 89→94. Added Section 7.10 key changes. Coverage points: 216 (was 201). |
| 6.1 | 2026-03-26 | - | Added CMD_FIRM_LOAD (0x05) to firmware command set (11 commands, was 10). New test: `sep_km_cmd_firm_load_test` (P2). 2 firmware feature entries, 3 coverage points (total: 219). Updated Part 4 counts: P2 48→49, FW Command 11→12, Total 94→95. Added Q-016 (CMD_FIRM_LOAD spec discrepancy with main-spec-km.md). Added Section 7.11 key changes. Source: Key Manager (KM) Firmware Specification. |
| 6.2 | 2026-04-01 | - | Updated Section 1.7 (Crypto Engine Ports): added verification method note documenting AXI-Lite bus monitor (`km_key_transfer_with_bus_chk`) for W-channel key data capture and `SHARE0^SHARE1` reconstruction. Added known-artifact note (firmware C bitfield RMW on write-only port — non-critical). Updated Section 1.14 key share XOR masking row: P0 key transfer tests now primary coverage via bus monitor; HDL backdoor retained as secondary in P2 firmware test. |
| 7.0 | 2026-04-10 | - | **Critical correction** per KM Firmware Spec review and `sep_km_uvm_cmd_firm_load_test` failure analysis. (1) Replaced CMD_FIRM_LOAD (0x05) with CMD_EXEC_ROM (0x05) and CMD_FIRM (0x06) in Section 1.11 — both marked **⛔ Blocked** (WIP, firmware not implemented). (2) Updated firmware feature coverage (Section 2.4): 3 WIP entries replacing 2 — coverage reduced from 100% to ~93%. (3) Updated Q-003 (12 command IDs, was 11). (4) Updated Q-016 with resolution: both commands WIP in ROM firmware, `rom_cmd.c` dispatches to `RC_INVALID_CMD`. (5) Added Q-017 (KMCSR.VERSION HDL backdoor — resolved: use RETURN_ARG), Q-018 (SRAM scrambling affects HDL reads — open), Q-019 (HDL path stability — open). (6) Updated firmware dependency: 10 of 12 commands implemented. (7) Coverage points: 222 (was 219). Tests: 96 (was 95). |
