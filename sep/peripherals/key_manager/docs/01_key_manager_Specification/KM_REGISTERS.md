# SEP Key Manager (KM) Register Reference

---

## Overview

This document provides the register reference for the SEP Key Manager (KM), derived from the RDL specifications.

**RDL Sources**:

- `key_manager.rdl` - Top-level address map
- `km_csr.rdl` - Control and Status Registers
- `km_kpv.rdl` - Key and Policy Vault (KM port)
- `km_kpv_kpvlp.rdl` - KPV Load Port (SEP port)
- `km_mailbox_km.rdl` - Mailbox KM side
- `km_mailbox_sep.rdl` - Mailbox SEP side
- `km_drbg_sampler.rdl` - DRBG Sampler
- `aes_wrapper_key.rdl` - AES key storage registers
- `otbn_wrapper_key.rdl` - OTBN key storage registers
- `kmac_wrapper_key.rdl` - KMAC key storage registers
- `hmac_wrapper_key.rdl` - HMAC key storage registers

---

## SEP-Facing Address

From the SEP CPU's perspective, the Key Manager's SEP-facing interfaces (Mailbox SEP side and KPVLP) are mapped at `0x1092_0000` to `0x1092_0FFF` (4 KB) in the SEP memory map. The KM falls under the **Crypto Periph** slave in the SEP AXI crossbar.

---

## KM Internal Memory Map Overview

The KM subsystem has its own 32-bit internal address space (accessed by the KM CPU, not the SEP CPU). All register offsets in this document are relative to this internal address space.

| Address Range | Size | Unit | Description |
|:--------------|:-----|:-----|:------------|
| 0x0000_0000 - 0x0000_1FFF | 8 KB | KM ROM | Key Manager Program ROM (Direct Memory) |
| 0x0000_2000 - 0x0000_3FFF | 8 KB | - | *Reserved* |
| 0x0000_4000 - 0x0000_7FFF | 16 KB | KM SRAM | Key Manager Program/Data SRAM (Direct Memory) |
| 0x0000_8000 - 0x0000_CFFF | 20 KB | - | *Reserved* |
| 0x0000_D000 - 0x0000_DFFF | 4 KB | KPV | Key and Policy Vault (AXI-Lite) |
| 0x0000_E000 - 0x0000_EFFF | 4 KB | KMCSR | Control, Status, Interrupt Registers (AXI-Lite) |
| 0x0000_F000 - 0x0000_FFFF | 4 KB | DRBG Sampler | DRBG random data, config, status (AXI-Lite) |
| 0x0001_0000 - 0x0001_0FFF | 4 KB | Mailbox KM | Mailbox KM Side Interface (AXI-Lite) |
| 0x0001_1000 - 0x0001_7FFF | 28 KB | - | *Reserved* |
| 0x0001_8000 - 0x0001_8FFF | 4 KB | OTBN | OTBN Crypto Accelerator Port (AXI-Lite Pass-through) |
| 0x0001_9000 - 0x0001_9FFF | 4 KB | AES | AES Crypto Accelerator Port (AXI-Lite Pass-through) |
| 0x0001_A000 - 0x0001_AFFF | 4 KB | KMAC | KMAC Crypto Accelerator Port (AXI-Lite Pass-through) |
| 0x0001_B000 - 0x0001_BFFF | 4 KB | HMAC | HMAC Crypto Accelerator Port (AXI-Lite Pass-through) |

---

## 1. Key and Policy Vault (KPV) - KM Port

**Base Address**: 0x0000_D000
**Size**: 4 KB
**Access**: KM CPU via AXI-Lite crossbar

### 1.1 Key Entry Registers (0x000 - 0x7FF)

32 key entries, each 512 bits (16 × 32-bit words).

| Offset | Register | Description |
|--------|----------|-------------|
| 0x000 - 0x03C | KEY_ENTRY[0].WORD[0:15] | Key Entry 0 (512 bits) |
| 0x040 - 0x07C | KEY_ENTRY[1].WORD[0:15] | Key Entry 1 (512 bits) |
| ... | ... | ... |
| 0x7C0 - 0x7FC | KEY_ENTRY[31].WORD[0:15] | Key Entry 31 (512 bits) |

**KEY_WORD Register**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | RW | *None* | 32-bit key data word. **No reset** (powers up random for security, FR-0000-149). Write protected by LOCK_WRITE (swwel). Key entry reads when LOCK_USE=1 return SLVERR and data=0. |

### 1.2 Control Registers (0x800 - 0x87C)

32 control registers, one per key slot.

| Offset | Register | Description |
|--------|----------|-------------|
| 0x800 | CTRL[0] | Control for Key Entry 0 |
| 0x804 | CTRL[1] | Control for Key Entry 1 |
| ... | ... | ... |
| 0x87C | CTRL[31] | Control for Key Entry 31 |

**CTRL Register Fields**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | LOCK_WRITE | RW (woset) | 0x0 | Prevents KM write to key entry data and control fields (Extend, Dest_valid, Last_dword) until reset. Lower 3 bits (Lock_write, Lock_use, Unlock_sep) remain writable. CTRL writes when locked return OKAY (no SLVERR). **Write-1-only-set**. HW can clear on wipe (hwclr). |
| 1 | LOCK_USE | RW (woset) | 0x0 | Prevents KM read of key entry data until reset. Reads return SLVERR and data=0. **Write-1-only-set**. HW can clear on wipe (hwclr). |
| 2 | UNLOCK_SEP | RW (woset) | 0x0 | Allows KPVLP to write this slot when LOCK_WRITE=0. **Write-1-only-set**. HW can clear on wipe (hwclr). |
| 3 | RSVD | RO | 0x0 | Reserved |
| 6:4 | EXTEND | RW | 0x0 | Zero-indexed number of additional slots for wide keys. Firmware enforced. Protected by LOCK_WRITE (swwel). |
| 8:7 | RSVD | RO | 0x0 | Reserved |
| 16:9 | DEST_VALID | RW | 0x0 | Which crypto block may consume this key (bit mask). Firmware enforced. Protected by LOCK_WRITE (swwel). |
| 20:17 | LAST_DWORD | RW | 0x0 | Last valid key word index [1,15]. Hardware returns 0 for reads beyond this. Protected by LOCK_WRITE (swwel). |
| 31:21 | RSVD | RO | 0x0 | Reserved |

**DEST_VALID Bit Encoding** (firmware-defined):

| Bit | Destination |
|-----|-------------|
| 9 | HMAC |
| 10 | KMAC |
| 11 | AES |
| 12 | OTBN |
| 13 | ABR ML-DSA seed |
| 14 | ABR ML-KEM D |
| 15 | ABR ML-KEM Z |
| 16 | ABR ML-KEM MSG |

### 1.3 KPV Scrambler Registers (0x880 - 0x884)

KPV-specific scrambler for key entry data. Accessible only via KM port (not KPVLP).

**KPV_SCRAMBLER_KEY (0x880)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | KEY | RW | *None* | 32-bit KPV scrambler key. **No reset** (powers up random for security, FR-0000-149). When LOCK=1: writes ignored (swwel). HW clears to 0 on wipe (hwclr). |

**KPV_SCRAMBLER_CTRL (0x884)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | ENABLE | RW | 0x0 | Enable KPV key entry scrambling. Locked when LOCK=1 (swwel). HW clears on wipe (hwclr). |
| 1 | LOCK | RW (woset) | 0x0 | Lock KPV scrambler key and enable. Write-one-only (0→1). HW clears on wipe (hwclr). |
| 31:2 | RSVD | RO | 0x0 | Reserved |

---

## 2. KPV Load Port (KPVLP) - SEP Port

**Base Address**: Integration-defined (not in KM memory map)
**Size**: ~4 KB
**Access**: SEP host via dedicated AXI-Lite interface

### 2.1 Key Entry Registers (0x000 - 0x7FF)

32 key entries, **write-only** from SEP side.

| Offset | Register | Description |
|--------|----------|-------------|
| 0x000 - 0x03C | KPVLP_KEY_ENTRY[0].WORD[0:15] | Key Entry 0 (write-only) |
| ... | ... | ... |
| 0x7C0 - 0x7FC | KPVLP_KEY_ENTRY[31].WORD[0:15] | Key Entry 31 (write-only) |

**KPVLP_KEY_WORD Register**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | WO | *None* | 32-bit key data word (write-only, reads not supported). No reset (FR-0000-149). |

**Access Rules**:

- Write allowed only when UNLOCK_SEP=1 AND LOCK_WRITE=0
- Write rejected (SLVERR) otherwise

### 2.2 Control Registers (0x800 - 0x87C)

32 control registers, **write-only** with limited writable fields.

**KPVLP_CTRL Register** (writable fields only):

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 6:4 | EXTEND | WO | 0x0 | Zero-indexed number of additional slots for wide keys |
| 8:7 | RSVD | WO | 0x0 | Reserved (writes ignored) |
| 16:9 | DEST_VALID | WO | 0x0 | Which crypto block may consume this key |
| 20:17 | LAST_DWORD | WO | 0x0 | Last valid key word index [1,15] |
| 31:21 | RSVD | WO | 0x0 | Reserved (writes ignored) |

**Note**: LOCK_WRITE, LOCK_USE, UNLOCK_SEP bits are NOT writable from KPVLP. Writes to those bits return OKAY but do not update the values (FR-0000-110).

### 2.3 STATUS Register (0x880)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | UNLOCK_SEP | RO | 0x0 | Bit N = 1 if slot N has UNLOCK_SEP set (SEP may write when LOCK_WRITE=0) |

---

## 3. Key Manager Control and Status Registers (KMCSR)

**Base Address**: 0x0000_E000
**Size**: 4 KB
**Access**: KM CPU via AXI-Lite

### 3.1 VERSION (0x000)

Semantic version register. Version 1.0.0. Writes to VERSION return SLVERR.

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 7:0 | PATCH | RO | 0x00 | Patch version number |
| 15:8 | MINOR | RO | 0x00 | Minor version number |
| 23:16 | MAJOR | RO | 0x01 | Major version number |
| 31:24 | RSVD | RO | 0x00 | Reserved; must read as zero |

### 3.2 CTRL (0x004)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | RSVD | RO | 0x0 | Reserved for future use |

### 3.3 SOFT_RST_CODE (0x008)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | CODE | RW | 0x0 | Write 0x53525354 ('SRST') to trigger soft reset. Any other value has no effect. |

### 3.4 IRQ_STATUS (0x00C)

Interrupt status register. Sticky bits cleared by writing 1 (W1C).

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | ROM_PARITY_ERR | RW1C | 0x0 | ROM parity error detected. Sticky, write 1 to clear. |
| 1 | SRAM_PARITY_ERR | RW1C | 0x0 | SRAM parity error detected. Sticky, write 1 to clear. |
| 2 | ROM_WRITE_ERR | RW1C | 0x0 | ROM write attempt detected. Sticky, write 1 to clear. |
| 3 | SRAM_WRITE_LOCK_ERR | RW1C | 0x0 | SRAM write to locked region detected. Sticky, write 1 to clear. |
| 4 | AXI_SLVERR | RW1C | 0x0 | AXI SLVERR response detected. Sticky, write 1 to clear. |
| 5 | AXI_DECERR | RW1C | 0x0 | AXI DECERR response detected. Sticky, write 1 to clear. |
| 6 | DRBG_ERR | RW1C | 0x0 | DRBG Sampler error (timeout or AXI-Stream error). Sticky, write 1 to clear. |
| 7 | WIPE_STATE | RW1C | 0x0 | Wipe state event (rising edge of wipe_state input). Sticky, write 1 to clear. |
| 31:8 | RSVD | RO | 0x0 | Reserved |

### 3.5 IRQ_ENABLE (0x010)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | ROM_PARITY_EN | RW | 0x0 | Enable ROM parity error interrupt |
| 1 | SRAM_PARITY_EN | RW | 0x0 | Enable SRAM parity error interrupt |
| 2 | ROM_WRITE_EN | RW | 0x0 | Enable ROM write error interrupt |
| 3 | SRAM_WRITE_LOCK_EN | RW | 0x0 | Enable SRAM write-lock violation interrupt |
| 4 | AXI_SLVERR_EN | RW | 0x0 | Enable AXI SLVERR error interrupt |
| 5 | AXI_DECERR_EN | RW | 0x0 | Enable AXI DECERR error interrupt |
| 6 | DRBG_ERR_EN | RW | 0x0 | Enable DRBG Sampler error interrupt |
| 7 | WIPE_STATE_EN | RW | 0x0 | Enable wipe state interrupt |
| 31:8 | RSVD | RO | 0x0 | Reserved |

### 3.6 SCRAMBLER_KEY (0x014) - No reset (FR-0000-149)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | KEY | RW | *None* | 32-bit SRAM scrambler key. **No reset** (powers up random for security). When LOCK=1: writes ignored, reads return 0. |

### 3.7 SCRAMBLER_CTRL (0x018)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | ENABLE | RW | 0x0 | Enable SRAM scrambling. Locked when LOCK=1. |
| 1 | LOCK | RW | 0x0 | Lock scrambler key and enable. Write-once (0→1 only). |
| 31:2 | RSVD | RO | 0x0 | Reserved |

### 3.8 SRAM_LOCK (0x01C)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | LOCK_BITS | RW (woset) | 0x0 | One bit per 512-byte SRAM region (32 regions). **Write-1-only-set**: writing 1 sets bit, writing 0 has no effect. Region 0 = 0x4000-0x41FF, Region 31 = 0x7E00-0x7FFF. |

### 3.9 IRQ_SET (0x020)

Software interrupt trigger. Write 1 to trigger corresponding interrupt (for ISR testing).

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | ROM_PARITY_ERR_SET | WO | 0x0 | Write 1 to set IRQ_STATUS.ROM_PARITY_ERR |
| 1 | SRAM_PARITY_ERR_SET | WO | 0x0 | Write 1 to set IRQ_STATUS.SRAM_PARITY_ERR |
| 2 | ROM_WRITE_ERR_SET | WO | 0x0 | Write 1 to set IRQ_STATUS.ROM_WRITE_ERR |
| 3 | SRAM_WRITE_LOCK_ERR_SET | WO | 0x0 | Write 1 to set IRQ_STATUS.SRAM_WRITE_LOCK_ERR |
| 4 | AXI_SLVERR_SET | WO | 0x0 | Write 1 to set IRQ_STATUS.AXI_SLVERR |
| 5 | AXI_DECERR_SET | WO | 0x0 | Write 1 to set IRQ_STATUS.AXI_DECERR |
| 6 | DRBG_ERR_SET | WO | 0x0 | Write 1 to set IRQ_STATUS.DRBG_ERR |
| 7 | WIPE_STATE_SET | WO | 0x0 | Write 1 to set IRQ_STATUS.WIPE_STATE |
| 31:8 | RSVD | WO | 0x0 | Reserved |

### 3.10 SRAM_WRITE_LOCK_VIOLATION (0x024)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | VIOLATION_BITS | RW1C | 0x0 | Bit N = 1 if region N had write attempt while locked. Sticky, write 1 to clear each bit. |

### 3.11 RECOVERABLE_ERR (0x028)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | RECOVERABLE_ERR | RW | 0x0 | Set by firmware ISR after recovering from a recoverable fault. Cleared by firmware when SEP sends clear command. Drives recoverable error event output. |
| 31:1 | RSVD | RO | 0x0 | Reserved |

### 3.12 OTP Data Registers (0x030 - 0x0B4)

Read-through from `otp_data_i` port. All registers always reflect current port signals (no latching). No reset values. CPU writes return SLVERR. Total: 34 registers.

**OTP_LIFE_CYCLE (0x030)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 7:0 | VALUE | RO | *None* | 8-bit life cycle state. Read-through from OTP port. No reset. |
| 31:8 | RSVD | RO | 0x0 | Reserved |

**OTP_DEMOTION_STATE (0x034)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 1:0 | DEMOTE_1_VALUE | RO | *None* | 2-bit demotion state 1. Read-through from OTP port. No reset. Differentially encoded. |
| 3:2 | DEMOTE_2_VALUE | RO | *None* | 2-bit demotion state 2. Read-through from OTP port. No reset. Differentially encoded. |
| 31:4 | RSVD | RO | 0x0 | Reserved |

**OTP_CHIPLET_UID_0..31 (0x038 - 0x0B4)**:

32 consecutive registers, one byte of Device UID each.

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 7:0 | VALUE | RO | *None* | 8-bit UID byte. Read-through from OTP port. No reset. |
| 31:8 | RSVD | RO | 0x0 | Reserved |

### 3.13 Virtual UART Registers (0x100 - 0x108)

For firmware-testbench communication during simulation.

**VUART_TX (0x100)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 7:0 | TX_BYTE | RW | 0x00 | Byte to transmit to testbench |
| 30:8 | RSVD | RO | 0x0 | Reserved |
| 31 | DATA_VALID | RW | 0x0 | Data valid strobe. Set by firmware, cleared by hardware after one cycle. |

**VUART_RX (0x104)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 7:0 | RX_BYTE | RO | 0x00 | Received byte from testbench |
| 30:8 | RSVD | RO | 0x0 | Reserved |
| 31 | DATA_VALID | RO | 0x0 | RX data valid. Set by testbench, cleared by firmware read. |

**VUART_STATUS (0x108)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | TX_READY | RO | 0x1 | TX ready to accept data (always 1 in simulation) |
| 1 | RX_VALID | RO | 0x0 | RX has valid data available |
| 2 | PRINT_ENABLE | RO | 0x0 | Enable VUART printing (set by testbench) |
| 31:3 | RSVD | RO | 0x0 | Reserved |

### 3.14 Test Protocol Registers (0x110 - 0x12C)

For firmware test result reporting.

**TB_RESULT (0x110)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | RESULT | RW | 0x0 | Test result: 0=fail, 1=pass |

**TB_SIGNATURE (0x114)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | SIGNATURE | RW | 0x0 | Completion signature: 0x600D600D (pass) or 0xBADBADBA (fail) |

**TB_ERRCODE (0x118)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | ERRCODE | RW | 0x0 | Optional error code for debugging |

**TB_SUBTEST (0x11C)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | SUBTEST | RW | 0x0 | Current subtest number |

**TB_CMD (0x120)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | CMD | RW | 0x0 | Command code (0=NOP, 1=ROM_PARITY_EN, 2=ROM_PARITY_DIS, etc.) |

**TB_CMD_ARG (0x124)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | ARG | RW | 0x0 | Command argument value |

**TB_CMD_STATUS (0x128)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | STATUS | RW | 0x0 | Command status: 0=IDLE, 1=ACK, 0xFFFFFFFF=ERR |

**TB_CMD_RESULT (0x12C)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | RESULT | RO | 0x0 | Command result value (written by testbench) |

### 3.15 DEBUG (0x1FC)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | MAGIC | RO | 0xCAFEBEEF | Magic constant for verification |

---

## 4. DRBG Sampler

**Base Address**: 0x0000_F000
**Size**: 4 KB
**Access**: KM CPU via AXI-Lite

### 4.1 DATA (0x000) - No reset (FR-0000-149)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | RO | *None* | 32-bit random data from DRBG. Read triggers fetch. **No reset** (powers up random). Writes return SLVERR. |

### 4.2 CFG (0x004)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | PREFETCH | RW | 0x0 | Enable prefetch. When 0, prefetch data register is cleared. |
| 15:1 | RSVD | RW | 0x0 | Reserved |
| 31:16 | TIMEOUT | RW | 0x0100 | Cycles to wait for DRBG on active CPU read. 0 = disabled. Default 256. |

### 4.3 STATUS (0x008)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | DRBG_READY | RO | 0x0 | TVALID asserted from DRBG |
| 1 | PREFETCHED | RO | 0x0 | Prefetched data available |
| 2 | TIMEOUT_ERR | RW1C | 0x0 | DRBG read timed out. Sticky, write 1 to clear. |
| 3 | STREAM_ERR | RW1C | 0x0 | DRBG AXI-Stream error (e.g. TVALID deasserted before TREADY). Sticky, write 1 to clear. |
| 7:4 | RSVD | RO | 0x0 | Reserved |
| 15:8 | COUNT_BAD | RO | 0x0 | Failed transactions. Saturates at 0xFF. Write non-zero to clear. |
| 31:16 | COUNT_GOOD | RO | 0x0 | Successful words transferred. Saturates at 0xFFFF. Write non-zero to clear. |

### 4.4 PREFETCH_DATA (0x00C) - No reset (FR-0000-149)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | RO | *None* | Prefetched 32-bit random data (debug). **No reset**. Cleared when PREFETCH=0. |

---

## 5. Mailbox KM Side

**Base Address**: 0x0001_0000
**Size**: 4 KB (decode window)
**Access**: KM CPU via AXI-Lite

### 5.1 KM_WRITE_DATA (0x000)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | WO | 0x0 | Write data to outbound FIFO (KM→SEP) |

### 5.2 KM_WRITE_SEPARATOR (0x004)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | SET | RW | 0x0 | Write 1 to set message separator on next outbound write. Cleared by hardware when that write completes. |
| 31:1 | RSVD | RO | 0x0 | Reserved |

### 5.3 KM_READ_DATA (0x008)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | RO | 0x0 | Read data from inbound FIFO (SEP→KM) |

### 5.4 KM_STATUS (0x00C)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | INBOUND_EMPTY | RO | 0x1 | Inbound FIFO is empty |
| 1 | INBOUND_FULL | RO | 0x0 | Inbound FIFO is full |
| 2 | OUTBOUND_EMPTY | RO | 0x1 | Outbound FIFO is empty |
| 3 | OUTBOUND_FULL | RO | 0x0 | Outbound FIFO is full |
| 11:4 | INBOUND_DEPTH | RO | 0x0 | Inbound FIFO fill level |
| 19:12 | OUTBOUND_DEPTH | RO | 0x0 | Outbound FIFO fill level |
| 20 | INBOUND_OVERFLOW | RW1C | 0x0 | Inbound FIFO overflow. Sticky, W1C. |
| 21 | OUTBOUND_OVERFLOW | RW1C | 0x0 | Outbound FIFO overflow. Sticky, W1C. |
| 22 | INBOUND_UNDERFLOW | RW1C | 0x0 | Inbound FIFO underflow. Sticky, W1C. |
| 23 | OUTBOUND_UNDERFLOW | RW1C | 0x0 | Outbound FIFO underflow. Sticky, W1C. |
| 24 | INBOUND_SEPARATOR | RO | 0x0 | Last inbound word had separator bit set |
| 25 | OUTBOUND_SEPARATOR | RO | 0x0 | Last outbound word had separator bit set |
| 31:26 | RSVD | RO | 0x0 | Reserved |

### 5.5 KM_IRQ_STATUS (0x010)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | INBOUND_READ_DATA_AVAIL | RO | 0x0 | Inbound FIFO has data for KM to read (level-sensitive) |
| 1 | OUTBOUND_WRITE_SPACE_AVAIL | RO | 0x0 | Outbound FIFO has space for KM to write (level-sensitive) |
| 2 | OUTBOUND_OVERFLOW | RW1C | 0x0 | Outbound FIFO overflow. Sticky, W1C. |
| 3 | INBOUND_UNDERFLOW | RW1C | 0x0 | Inbound FIFO underflow. Sticky, W1C. |
| 4 | FLUSHED_BY_SEP | RW1C | 0x0 | SEP performed mailbox flush. Sticky, W1C. |
| 31:5 | RSVD | RO | 0x0 | Reserved |

### 5.6 KM_IRQ_ENABLE (0x014)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | INBOUND_READ_DATA_AVAIL_EN | RW | 0x0 | Enable inbound read data available IRQ |
| 1 | OUTBOUND_WRITE_SPACE_AVAIL_EN | RW | 0x0 | Enable outbound write space available IRQ |
| 2 | OUTBOUND_OVERFLOW_EN | RW | 0x0 | Enable outbound overflow IRQ |
| 3 | INBOUND_UNDERFLOW_EN | RW | 0x0 | Enable inbound underflow IRQ |
| 4 | FLUSHED_BY_SEP_EN | RW | 0x0 | Enable flush notification IRQ |
| 31:5 | RSVD | RO | 0x0 | Reserved |

### 5.7 KM_CTRL (0x018)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | OUTBOUND_OVERFLOW_RESP | RW | 0x0 | Overflow response: 0=SLVERR, 1=OKAY |
| 1 | INBOUND_UNDERFLOW_RESP | RW | 0x0 | Underflow response: 0=SLVERR, 1=OKAY |
| 2 | FLUSH | RW | 0x0 | Write 1 to flush all FIFOs. HW clears when complete. |
| 31:3 | RSVD | RO | 0x0 | Reserved |

---

## 6. Mailbox SEP Side

**Base Address**: Integration-defined (not in KM memory map)
**Size**: 4 KB (decode window)
**Access**: SEP host via dedicated AXI-Lite interface

### 6.1 SEP_WRITE_DATA (0x000)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | WO | 0x0 | Write data to inbound FIFO (SEP→KM) |

### 6.2 SEP_WRITE_SEPARATOR (0x004)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | SET | RW | 0x0 | Write 1 to set message separator on next inbound write. Cleared by hardware when that write completes. |
| 31:1 | RSVD | RO | 0x0 | Reserved |

### 6.3 SEP_READ_DATA (0x008)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | RO | 0x0 | Read data from outbound FIFO (KM→SEP) |

### 6.4 SEP_STATUS (0x00C)

Same fields as KM_STATUS, but from SEP perspective:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | INBOUND_EMPTY | RO | 0x1 | Inbound FIFO is empty |
| 1 | INBOUND_FULL | RO | 0x0 | Inbound FIFO is full |
| 2 | OUTBOUND_EMPTY | RO | 0x1 | Outbound FIFO is empty |
| 3 | OUTBOUND_FULL | RO | 0x0 | Outbound FIFO is full |
| 11:4 | INBOUND_DEPTH | RO | 0x0 | Inbound FIFO fill level |
| 19:12 | OUTBOUND_DEPTH | RO | 0x0 | Outbound FIFO fill level |
| 20 | INBOUND_OVERFLOW | RW1C | 0x0 | Inbound FIFO overflow. Sticky, W1C. |
| 21 | OUTBOUND_OVERFLOW | RW1C | 0x0 | Outbound FIFO overflow. Sticky, W1C. |
| 22 | INBOUND_UNDERFLOW | RW1C | 0x0 | Inbound FIFO underflow. Sticky, W1C. |
| 23 | OUTBOUND_UNDERFLOW | RW1C | 0x0 | Outbound FIFO underflow. Sticky, W1C. |
| 24 | INBOUND_SEPARATOR | RO | 0x0 | Last inbound word had separator bit |
| 25 | OUTBOUND_SEPARATOR | RO | 0x0 | Last outbound word had separator bit |
| 31:26 | RSVD | RO | 0x0 | Reserved |

### 6.5 SEP_IRQ_STATUS (0x010)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | OUTBOUND_READ_DATA_AVAIL | RO | 0x0 | Outbound FIFO has data (KM→SEP messages) |
| 1 | INBOUND_WRITE_SPACE_AVAIL | RO | 0x0 | Inbound FIFO has space for SEP to write (level-sensitive) |
| 2 | INBOUND_OVERFLOW | RW1C | 0x0 | Inbound FIFO overflow. Sticky, W1C. |
| 3 | OUTBOUND_UNDERFLOW | RW1C | 0x0 | Outbound FIFO underflow. Sticky, W1C. |
| 4 | FLUSHED_BY_KM | RW1C | 0x0 | KM performed mailbox flush. Sticky, W1C. |
| 31:5 | RSVD | RO | 0x0 | Reserved |

### 6.6 SEP_IRQ_ENABLE (0x014)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | OUTBOUND_READ_DATA_AVAIL_EN | RW | 0x0 | Enable outbound read data available IRQ |
| 1 | INBOUND_WRITE_SPACE_AVAIL_EN | RW | 0x0 | Enable inbound write space available IRQ |
| 2 | INBOUND_OVERFLOW_EN | RW | 0x0 | Enable inbound overflow IRQ |
| 3 | OUTBOUND_UNDERFLOW_EN | RW | 0x0 | Enable outbound underflow IRQ |
| 4 | FLUSHED_BY_KM_EN | RW | 0x0 | Enable flush notification IRQ |
| 31:5 | RSVD | RO | 0x0 | Reserved |

### 6.7 SEP_CTRL (0x018)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | INBOUND_OVERFLOW_RESP | RW | 0x0 | Overflow response: 0=SLVERR, 1=OKAY |
| 1 | OUTBOUND_UNDERFLOW_RESP | RW | 0x0 | Underflow response: 0=SLVERR, 1=OKAY |
| 2 | FLUSH | RW | 0x0 | Write 1 to flush all FIFOs. HW clears when complete. |
| 31:3 | RSVD | RO | 0x0 | Reserved |

---

## 7. Crypto Engine Key Storage Registers

The KM transfers keys to cryptographic accelerators via dedicated AXI4-Lite key storage register blocks on the private secret key bus. Each engine has KEY_SHARE0, KEY_SHARE1 (write-only, no reset), and KEY_CTRL (KEY_VALID flag).

**RDL Sources**: `aes_wrapper_key.rdl`, `otbn_wrapper_key.rdl`, `kmac_wrapper_key.rdl`, `hmac_wrapper_key.rdl`

### 7.1 OTBN Key Storage (0x0001_8000)

**Size**: 4 KB
**Access**: KM CPU only (private key bus)

**KEY_SHARE0[0..11] (0x000 - 0x02C)**: 384-bit key share 0 (12 × 32-bit words)
**KEY_SHARE1[0..11] (0x030 - 0x05C)**: 384-bit key share 1 (12 × 32-bit words)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | WO | *None* | 32-bit key data word. Write-only; reads return 0. **No reset** (powers up undefined for security). |

**KEY_CTRL (0x060)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | KEY_VALID | RW | 0x0 | When 1, asserts sideload valid to OTBN core. Write 0 to invalidate. |
| 31:1 | RSVD | RO | 0x0 | Reserved |

### 7.2 AES Key Storage (0x0001_9000)

**Size**: 4 KB
**Access**: KM CPU only (private key bus)

**KEY_SHARE0[0..7] (0x000 - 0x01C)**: 256-bit key share 0 (8 × 32-bit words)
**KEY_SHARE1[0..7] (0x020 - 0x03C)**: 256-bit key share 1 (8 × 32-bit words)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | WO | *None* | 32-bit key data word. Write-only; reads return 0. **No reset**. |

**KEY_CTRL (0x040)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | KEY_VALID | RW | 0x0 | When 1, asserts sideload valid to AES core. Write 0 to invalidate. |
| 31:1 | RSVD | RO | 0x0 | Reserved |

### 7.3 KMAC Key Storage (0x0001_A000)

**Size**: 4 KB
**Access**: KM CPU only (private key bus)

**KEY_SHARE0[0..7] (0x000 - 0x01C)**: 256-bit key share 0 (8 × 32-bit words)
**KEY_SHARE1[0..7] (0x020 - 0x03C)**: 256-bit key share 1 (8 × 32-bit words)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | WO | *None* | 32-bit key data word. Write-only; reads return 0. **No reset**. |

**KEY_CTRL (0x040)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | KEY_VALID | RW | 0x0 | When 1, asserts sideload valid to KMAC core. Write 0 to invalidate. |
| 31:1 | RSVD | RO | 0x0 | Reserved |

### 7.4 HMAC Key Storage (0x0001_B000)

**Size**: 4 KB
**Access**: KM CPU only (private key bus)

**KEY_SHARE0[0..7] (0x000 - 0x01C)**: 256-bit key share 0 (8 × 32-bit words)
**KEY_SHARE1[0..7] (0x020 - 0x03C)**: 256-bit key share 1 (8 × 32-bit words)

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 31:0 | DATA | WO | *None* | 32-bit key data word. Write-only; reads return 0. **No reset**. |

**KEY_CTRL (0x040)**:

| Bits | Field | Access | Reset | Description |
|------|-------|--------|-------|-------------|
| 0 | KEY_VALID | RW | 0x0 | When 1, asserts sideload valid to HMAC core. Write 0 to invalidate. |
| 31:1 | RSVD | RO | 0x0 | Reserved |

---

## Access Type Legend

| Access | Description |
|--------|-------------|
| RO | Read-only |
| RW | Read-write |
| WO | Write-only |
| RW1C | Read, write-1-to-clear |
| woset | Write-1-only-set (writing 1 sets bit, writing 0 has no effect) |
| swwel | Software write enable lock (HW signal gates SW writes) |
| hwclr | Hardware can clear all bits (used for wipe) |
| *None* | No reset value (powers up random for security) |

---

## 8. PCPI CRC Accelerator (Non-CSR)

The KM PicoRV32 CPU includes a PCPI CRC accelerator that provides three hardware CRC instructions (CRC-32C word update, CRC-32C byte update, CRC-8/ROHC byte update). This feature uses the PicoRV32 PCPI custom instruction interface and does **not** introduce any new memory-mapped registers or CSRs. The CRC accelerator is entirely instruction-level; see `KM_DESIGN_SPEC.md` §PCPI CRC Accelerator Instructions for encoding details and behavioral properties.

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-01-21 | - | Initial register documentation |
| 1.1 | 2026-01-22 | - | Updated for spec v0.5 |
| 2.0 | 2026-02-02 | - | Complete rewrite based on RDL specifications. Updated memory map, added all KMCSR registers, mailbox registers, KPVLP registers. Removed Phase 2/3 registers not in current RDL. |
| 3.0 | 2026-02-11 | - | Major update aligned with latest RDL. Added: DRBG Sampler block (Section 4), KPV Scrambler registers (KPV_SCRAMBLER_KEY/CTRL), RECOVERABLE_ERR (0x028), OTP Data registers (0x030-0x0C0), WIPE_STATE and DRBG_ERR in IRQ_STATUS/ENABLE/SET. Updated: KPV CTRL (CLEAR bit removed → RSVD, added hwclr/swwel annotations), SCRAMBLER_KEY no-reset, memory map (DRBG at 0xF000, Mailbox 4KB). Added FR-0000-149 no-reset annotations throughout. |
| 3.1 | 2026-02-23 | - | Added SEP-facing address (0x1093_0000, 4 KB) and clarified that internal memory map is for KM CPU address space. |
| 4.0 | 2026-03-03 | - | Major update aligned with latest RDL. VERSION: new semantic versioning layout (RSVD/MAJOR/MINOR/PATCH, version 1.0.0, writes return SLVERR). OTP: consolidated to 1 life cycle register, removed OTP_WRITE_COUNT, read-through from OTP port (34 regs). Mailbox: added WRITE_SEPARATOR register (both sides), WRITE_SPACE_AVAIL IRQ source, shifted offsets, removed separator from CTRL. Added Section 7: Crypto Engine Key Storage Registers (2-share key architecture). Updated SEP-facing address to 0x1092_0000. |
| 4.1 | 2026-03-07 | - | OTP_DEMOTION_STATE: updated from single 2-bit VALUE field to two 2-bit fields (DEMOTE_1_VALUE[1:0], DEMOTE_2_VALUE[3:2]) per latest km_csr.rdl. HMAC Key Storage (Section 7.4): marked as NOT instantiated — hmac_wrapper_key_reg.rdl exists but is not in top-level address map; HMAC uses software-writable KEY registers. Added hmac_wrapper_key_reg.rdl to RDL Sources. Updated memory map for HMAC entry. |
| 4.2 | 2026-03-09 | - | Corrected the register reference to match the current implementation: HMAC key storage is instantiated at `0x0001_B000`, DEST_VALID uses the firmware bit ordering (HMAC, KMAC, AES, OTBN), and KMCSR IRQ_STATUS/IRQ_ENABLE/IRQ_SET no longer list a mailbox bit because mailbox inbound is delivered as a separate direct CPU IRQ. |
| 5.0 | 2026-03-16 | - | No register changes. Version bump for consistency with Design Spec v5.0 and Test Plan v5.0 (KM Firmware Specification integration). All register definitions remain current with RDL. |
| 6.0 | 2026-03-26 | - | No register changes. Added Section 8 noting the PCPI CRC Accelerator (non-CSR feature using PicoRV32 custom instructions). Version bump for consistency with Design Spec v6.0. |
