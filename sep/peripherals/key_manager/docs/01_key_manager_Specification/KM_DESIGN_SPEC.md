# SEP Key Manager (KM) Design Specification Summary

---

## Overview

The Key Manager (KM) module provides secure key storage and transfer functionality for the SEP subsystem. It uses a CPU-based architecture (PicoRV32 RISC-V) with firmware control, enabling flexibility for future enhancements via firmware updates.

**Key Differentiator**: Unlike hardware state machine approaches, SEP KM uses a CPU-based architecture with firmware control.

## Phased Development

The KM is deployed in three phases:

| Phase | Scope | Priority | Status |
|-------|-------|----------|--------|
| **Phase 1** | External key loading (KPVLP) and transfer to crypto engines | HIGH | First release requirement |
| **Phase 2** | Internal key generation and derivation (KDF) | MEDIUM | Expected before first release |
| **Phase 3** | Expanded capabilities and hardening | LOW | Future |

**Phase 1 is the only DV requirement for the first SEP release.**

## Features (Current Implementation)

### Phase 1 Features (Implemented)

- **Key Loading** - SEP host provisions keys via KPVLP (Key and Policy Vault Load Port)
- **Key Storage** - 32 key entries × 512 bits in Key and Policy Vault (KPV)
- **Key Transfer** - Distribute keys to cryptographic accelerators (OTBN, AES, KMAC, HMAC) via the private key bus sideload interface
- **Access Control** - LOCK_WRITE, LOCK_USE, UNLOCK_SEP protection mechanisms
- **Mailbox Communication** - Command/response interface between SEP and KM
- **SRAM Scrambling** - PRESENT-inspired data and address scrambling
- **KPV Scrambling** - PRESENT-based data and address scrambling for KPV key entries (same algorithm as SRAM, 512×32 address space)
- **DRBG Sampler** - AXI-Stream bridge to DRBG for random data (scrambler key provisioning, entropy)
- **SEP OTP Data** - Read-only registers for life cycle states, demotion state, Device UID from OTP
- **Wipe State** - Emergency wipe input that zeroes all KPV contents (key data, control, scrambler)
- **Error Detection** - Parity checking for ROM/SRAM, AXI error detection
- **Error Condition Outputs** - Unrecoverable fault (CPU trap) and recoverable error outputs

### Phase 2/3 Features (Not Yet Implemented)

- Key derivation (HMAC-SHA256 KDF)
- Life cycle state handling (firmware-level)
- Public key generation (ECDSA P256/P384)
- Remote attestation support

## Architecture

### Block Diagram

```
                    +------------------+
                    |   SEP Host CPU   |
                    +--------+---------+
                             |
         +-------------------+-------------------+
         |                                       |
+--------v---------+                   +---------v--------+
|     KPVLP        |                   |   Mailbox SEP    |
| (Key Provisioning)|                   | (Commands/Resp)  |
+--------+---------+                   +---------+--------+
         |                                       |
         |        +------------------+           |
         +------->|                  |<----------+
                  |   Key Manager    |
                  |                  |
                  |   +-----------+  |
                  |   | PicoRV32  |  |    +------------------+
                  |   |   CPU     |  |--->| OTBN Key Storage |
                  |   +-----------+  |    +------------------+
                  |   | ROM (8KB) |  |    +------------------+
                  |   +-----------+  |--->| AES Key Storage  |
                  |   | SRAM(16KB)|  |    +------------------+
                  |   +-----------+  |    +------------------+
                  |   | KPV       |  |--->| KMAC Key Storage |
                  |   +-----------+  |    +------------------+
                  |   | KMCSR     |  |    +------------------+
                  |   +-----------+  |--->| HMAC Key Storage |
                  |   | DRBG Samp.|  |
                  |   +-----------+  |
                  |                  |<--- wipe_state
                  |                  |<--- otp_data_i / otp_data_wr_i
                  |                  |---> cpu_trap_o (unrecoverable)
                  |                  |---> recoverable_err_o
                  +------------------+
```

### Security Domain Separation

- **SEP host never has direct access** to KPV key data
- **KPVLP is write-only** - SEP can provision keys but cannot read them back
- **Private key bus** - Only KM CPU can write to crypto engine key storage
- **Mailbox acts as firewall** - KM only executes legal commands

### Datapath No-Reset Security Model (FR-0000-149)

For security, certain datapath registers holding secret or random data **must NOT be reset**. They power up with random/undefined values. This prevents an attacker from using reset behavior to infer key material.

Registers with **no reset**:

| Register | Location | Rationale |
|----------|----------|-----------|
| KPV KEY_ENTRY[32].WORD[16] | km_kpv | Key storage must not leak via reset |
| KPV_SCRAMBLER_KEY | km_kpv (0x880) | Scrambler key must not leak |
| SCRAMBLER_KEY | km_csr (0x014) | SRAM scrambler key must not leak |
| DRBG DATA | km_drbg_sampler (0x000) | Random data |
| DRBG PREFETCH_DATA | km_drbg_sampler (0x00C) | Random data |
| OTP_LIFE_CYCLE | km_csr (0x030) | OTP data reflects current otp_data_i port |
| OTP_DEMOTION_STATE | km_csr (0x034) | OTP data reflects current otp_data_i port |
| OTP_CHIPLET_UID_* | km_csr (0x038-0x0B4) | OTP data reflects current otp_data_i port |

## SEP Memory Map (SEP-Facing Address)

From the SEP CPU's perspective, the Key Manager's SEP-facing interfaces (Mailbox SEP side and KPVLP) are mapped at:

| Base Address | End Address | Size | Description |
|:-------------|:------------|:-----|:------------|
| 0x1092_0000 | 0x1092_0FFF | 4 KB | Key Manager (SEP-facing interfaces) |

The KM falls under the **Crypto Periph** slave in the SEP AXI crossbar. The exact sub-addressing of Mailbox SEP side and KPVLP within this window is defined by system integration.

## SEP Interconnect - KM Connections

### Crypto Periph Slave Access

The KM's SEP-facing interfaces (Mailbox SEP side and KPVLP) are part of the "Crypto Periph" slave in the SEP top-level AXI4 interconnect. The following masters can access KM through this path:

| Master | Access to KM (Crypto Periph) | Notes |
|--------|------------------------------|-------|
| CPU IFU | **No** | IFU can only access Boot ROM, Scratch SRAM, ICCM |
| CPU LSU | **Yes** | Primary path for SEP firmware to send mailbox commands and provision keys via KPVLP |
| CPU DBG | **Yes** | Debug access to KM interfaces |
| DMA | **Yes** | DMA can access crypto peripherals |
| System I/F | **Yes** | External access to KM interfaces |

### Cryptographic Subsystem Dual-Bus Architecture

The cryptographic subsystem uses two physically separate buses:

| Bus | Master | Slaves | Purpose |
|-----|--------|--------|---------|
| AXI4-Lite Data/CSR Bus | SEP Host (via bridge) | Crypto accelerator CSRs (OTBN, AES, KMAC, HMAC, etc.) | Data processing, configuration |
| AXI4-Lite Secret Key Bus | **Key Manager** | Crypto accelerator key storage ports (OTBN, AES, KMAC, HMAC) | Key transfer only |

The KM CPU is the sole master on the secret key bus. Secret key data is physically separated from the software-accessible data bus, preventing software-based key extraction attacks.

### Current SEP Integration Note

At the current `sep_crypto` integration point, the KM private key bus is connected to all four accelerator wrappers (OTBN, AES, KMAC, HMAC), and OTP data is connected into KMCSR. The standalone KM component also contains a DRBG sampler and external `wipe_state` input, but in `hw/sep/sep_crypto.sv` the DRBG AXI-Stream input is tied off and `wipe_state_i` is tied low, so those two features are present in the KM block but not yet active through the SEP top-level integration.

## KM Internal Memory Map

The KM subsystem has its own 32-bit internal address space (accessed by the KM CPU, not the SEP CPU):

| Address Range | Size | Unit | Description |
|:--------------|:-----|:-----|:------------|
| 0x0000_0000 - 0x0000_1FFF | 8 KB | KM ROM | Key Manager Program ROM |
| 0x0000_4000 - 0x0000_7FFF | 16 KB | KM SRAM | Key Manager Program/Data SRAM |
| 0x0000_D000 - 0x0000_DFFF | 4 KB | KPV | Key and Policy Vault |
| 0x0000_E000 - 0x0000_EFFF | 4 KB | KMCSR | Control, Status, Interrupt Registers |
| 0x0000_F000 - 0x0000_FFFF | 4 KB | DRBG Sampler | DRBG random data, config, status |
| 0x0001_0000 - 0x0001_0FFF | 4 KB | Mailbox KM | Mailbox KM Side Interface |
| 0x0001_8000 - 0x0001_8FFF | 4 KB | OTBN | OTBN Crypto Accelerator Port |
| 0x0001_9000 - 0x0001_9FFF | 4 KB | AES | AES Crypto Accelerator Port |
| 0x0001_A000 - 0x0001_AFFF | 4 KB | KMAC | KMAC Crypto Accelerator Port |
| 0x0001_B000 - 0x0001_BFFF | 4 KB | HMAC | HMAC Crypto Accelerator Port |

## Key and Policy Vault (KPV)

### Overview

The KPV provides secure storage for 32 key entries, each up to 512 bits (16 × 32-bit words).

### Dual-Port Access

| Port | Interface | Access | Description |
|------|-----------|--------|-------------|
| KM Port | 0x0000_D000 | RW | Full access from KM CPU |
| KPVLP (SEP Port) | Integration-defined | WO | Write-only from SEP host |

### Key Entry Structure

- **32 key slots** (entries 0-31)
- **512 bits per slot** (16 words)
- **Per-slot control register** with protection and configuration fields
- **No reset** on key entry data (powers up random for security, FR-0000-106, FR-0000-149)

### Control Register Fields

| Field | Bits | Description |
|-------|------|-------------|
| LOCK_WRITE | [0] | Prevents write to key data and CTRL fields (Extend, Dest_valid, Last_dword) (W1S). Lower 3 bits (Lock_write, Lock_use, Unlock_sep) remain writable. CTRL writes when locked return OKAY (no SLVERR). |
| LOCK_USE | [1] | Prevents read of key data; reads return SLVERR and data=0 (W1S) |
| UNLOCK_SEP | [2] | Allows KPVLP to write this slot when LOCK_WRITE=0 (W1S) |
| RSVD | [3] | Reserved |
| EXTEND | [6:4] | Additional slots for wide keys (swwel by LOCK_WRITE) |
| DEST_VALID | [16:9] | Allowed crypto engine destinations (swwel by LOCK_WRITE) |
| LAST_DWORD | [20:17] | Last valid key word index [1,15] (swwel by LOCK_WRITE). HW returns 0 for reads beyond this. |

**Note**: All lock bits (LOCK_WRITE, LOCK_USE, UNLOCK_SEP) support `hwclr` for wipe.

### KPV Scrambling

The KPV supports address and data scrambling for all key entry data, using the same PRESENT-based algorithm as SRAM scrambling but operating over a 512×32 address space (32 slots × 16 words = 512 words).

| Register | Offset (in KPV) | Description |
|----------|-----------------|-------------|
| KPV_SCRAMBLER_KEY | 0x880 | 32-bit scrambler key. No reset (security). Cleared on wipe. |
| KPV_SCRAMBLER_CTRL | 0x884 | ENABLE[0] + LOCK[1] (write-one-only). Reset to 0. Cleared on wipe. |

- Accessible only via KM port (NOT exposed via KPVLP)
- When enabled: both KM port and KPVLP see unscrambled data (transparent scramble/descramble)
- Address input is linear register index: slot × 16 + word_index (0..511)
- CTRL registers and KPVLP STATUS are NOT scrambled

### Concurrent Access Restriction

The KPV does not arbitrate concurrent access to the same key slot by the KM port and the KPVLP. Firmware must not access the same key slot via both ports concurrently; behavior is undefined if they do.

### KPVLP (SEP Side)

- **Write-only** key entry access
- **Write-only** control fields (only EXTEND, DEST_VALID, LAST_DWORD)
- **Read-only** STATUS register showing UNLOCK_SEP state for all 32 slots
- Access requires both UNLOCK_SEP=1 AND LOCK_WRITE=0
- Writes to LOCK_WRITE, LOCK_USE, UNLOCK_SEP, reserved bits return OKAY but do NOT update values (FR-0000-110)

## DRBG Sampler

### Overview

The DRBG Sampler bridges KM CPU AXI4-Lite read requests to the DRBG AXI-Stream interface. It provides random data for scrambler key provisioning and other entropy needs.

**Base Address**: 0x0000_F000 | **Size**: 4 KB

### Registers

| Offset | Register | Description |
|--------|----------|-------------|
| 0x000 | DATA | Read triggers DRBG fetch. No reset (security). Writes return SLVERR. |
| 0x004 | CFG | PREFETCH[0] enable, TIMEOUT[31:16] (default 256, 0=disabled) |
| 0x008 | STATUS | DRBG_READY[0], PREFETCHED[1], TIMEOUT_ERR[2] (W1C), STREAM_ERR[3] (W1C), COUNT_BAD[15:8] (sat 0xFF), COUNT_GOOD[31:16] (sat 0xFFFF) |
| 0x00C | PREFETCH_DATA | Read-only debug. No reset (security). Cleared when PREFETCH=0. |

### Key Behaviors

- **Bus blocking**: Read from DATA when DRBG has not asserted TVALID blocks the KM system bus until data arrives or timeout
- **Timeout**: Configurable; on timeout returns SLVERR with RDATA=0, sets TIMEOUT_ERR, sends error pulse to KMCSR (IRQ_STATUS.DRBG_ERR)
- **AXI-Stream errors**: TVALID deassert before TREADY is a protocol violation; sets STREAM_ERR, sends error pulse
- **TSTRB handling**: When TSTRB != 0xF, assembles valid bytes across multiple AXI-Stream transactions
- **Single-use guarantee**: Each random sample returned to CPU exactly once
- **Prefetch**: Optional one-word prefetch from DRBG; not subject to timeout
- **Counter clearing**: Write any non-zero value to COUNT_GOOD or COUNT_BAD field to clear that counter; writing 0 has no effect

### IRQ Integration

DRBG errors (timeout or stream error) send an aggregated pulsed interrupt to KMCSR → `IRQ_STATUS.DRBG_ERR[7]`.

## SEP OTP Data

### Overview

The SEP OTP unit provides 34 software-readable registers of data relevant to the Key Manager. These are read-only in KMCSR and reflect current values on the `otp_data_i` input port (read-through, not latched).

### OTP Data Layout (34 registers)

| Register(s) | Offset | Size | Description |
|-------------|--------|------|-------------|
| OTP_LIFE_CYCLE | 0x030 | 8-bit | Combined life cycle state (differentially encoded) |
| OTP_DEMOTION_STATE | 0x034 | 4-bit | Two 2-bit demotion fields: demote_1_value[1:0], demote_2_value[3:2] (differentially encoded) |
| OTP_CHIPLET_UID_0..31 | 0x038-0x0B4 | 32 × 8-bit | Device Unique Identifier (256 bits total) |

### Key Behaviors

- **No reset** on OTP data registers (reads return undefined until OTP port driven, FR-0000-149)
- **Read-through**: Registers always reflect current `otp_data_i` port signals (not latched)
- **CPU writes return SLVERR** (read-only from KM firmware)
- No OTP_WRITE_COUNT register (removed)
- No single-shot latch behavior (data is always live from the OTP port)

## Wipe State

### Overview

The wipe state input provides an emergency wipe mechanism that zeroes all KPV contents.

### Behavior

1. **On rising edge of `wipe_state`**:
   - `IRQ_STATUS.WIPE_STATE[8]` is set (sticky, W1C) in the same cycle
   - On the **next** clock cycle, the entire KPV is synchronously reset to zero:
     - All 512 key entry data registers (32 × 16 words)
     - All 32 KPV control registers (LOCK_WRITE, LOCK_USE, UNLOCK_SEP, EXTEND, DEST_VALID, LAST_DWORD)
     - KPV_SCRAMBLER_KEY and KPV_SCRAMBLER_CTRL (ENABLE and LOCK)
   - No scrambling performed during wipe (data already scrambled in normal operation)

2. **Concurrent access**: If wipe occurs during an in-flight KPV transaction, the transaction completes with OKAY. Read data may be pre-wipe, zero, or undefined.

3. **Minimum pulse width**: `wipe_state` must be held high for at least one complete KM clock period.

## Mailbox Communication

### Overview

The mailbox provides bidirectional communication between SEP host and KM CPU:

- **Inbound FIFO**: SEP → KM (commands), minimum depth 16 words
- **Outbound FIFO**: KM → SEP (responses), minimum depth 16 words

### KM-Side Registers (Mailbox KM, base 0x0001_0000)

| Offset | Register | Description |
|--------|----------|-------------|
| 0x000 | WRITE_DATA | Write to outbound FIFO |
| 0x004 | KM_WRITE_SEPARATOR | Write 1 to set separator on next outbound write; cleared by HW after write |
| 0x008 | READ_DATA | Read from inbound FIFO |
| 0x00C | STATUS | FIFO status (depths, empty/full flags) |
| 0x010 | IRQ_STATUS | Bit[0] INBOUND_READ_DATA_AVAIL, Bit[1] OUTBOUND_WRITE_SPACE_AVAIL, Bit[2] OUTBOUND_OVERFLOW, Bit[3] INBOUND_UNDERFLOW, Bit[4] FLUSHED_BY_SEP |
| 0x014 | IRQ_ENABLE | Per-bit interrupt enable |
| 0x018 | CTRL | Bit[0] INBOUND_FLUSH, Bit[1] ERROR_RESPONSE, Bit[2] FLUSH |

### SEP-Side Registers (Mailbox SEP, integration-defined base)

| Offset | Register | Description |
|--------|----------|-------------|
| 0x000 | WRITE_DATA | Write to inbound FIFO |
| 0x004 | SEP_WRITE_SEPARATOR | Write 1 to set separator on next inbound write; cleared by HW after write |
| 0x008 | READ_DATA | Read from outbound FIFO |
| 0x00C | STATUS | FIFO status (depths, empty/full flags) |
| 0x010 | IRQ_STATUS | Bit[0] OUTBOUND_READ_DATA_AVAIL, Bit[1] INBOUND_WRITE_SPACE_AVAIL, Bit[2] INBOUND_OVERFLOW, Bit[3] OUTBOUND_UNDERFLOW, Bit[4] FLUSHED_BY_KM |
| 0x014 | IRQ_ENABLE | Per-bit interrupt enable |
| 0x018 | CTRL | Bit[0] OUTBOUND_FLUSH, Bit[1] ERROR_RESPONSE, Bit[2] FLUSH |

### Message Framing

Messages are framed using the dedicated WRITE_SEPARATOR register:

- Write 1 to `KM_WRITE_SEPARATOR` (offset 0x004) before writing the last word of a KM→SEP message; HW clears the register after the write completes
- Write 1 to `SEP_WRITE_SEPARATOR` (offset 0x004) before writing the last word of a SEP→KM message; HW clears the register after the write completes
- Check `OUTBOUND_SEPARATOR` / `INBOUND_SEPARATOR` in read data to detect message boundaries

### Features

- Pure FIFO transport (no hardware message validation)
- FIFO overflow/underflow detection
- Configurable error response (SLVERR vs OKAY)
- Overflow drops new data, preserves existing FIFO contents (FR-0000-059)
- Flush capability (both sides)
- Cross-side flush notification
- Interrupt support for data availability and write space availability

## KMCSR (Control and Status Registers)

### VERSION Register

The KMCSR includes a VERSION register at offset 0x000 with semantic versioning per FR-0000-076:

| Field | Bits | Reset Value | Description |
|-------|------|-------------|-------------|
| RSVD | [31:24] | 0x00 | Reserved |
| MAJOR | [23:16] | 0x01 | Major version |
| MINOR | [15:8] | 0x00 | Minor version |
| PATCH | [7:0] | 0x00 | Patch version |

- Current version: **1.0.0** (MAJOR=1, MINOR=0, PATCH=0)
- Read-only: writes return **SLVERR**
- The old ID/MAJOR/MINOR format (ID[31:16]=0x4B4D) is removed

### Interrupt Sources

| Bit | Source | Description |
|-----|--------|-------------|
| [0] | ROM_PARITY_ERR | ROM parity error detected (sticky, W1C) |
| [1] | SRAM_PARITY_ERR | SRAM parity error detected (sticky, W1C) |
| [2] | ROM_WRITE_ERR | ROM write attempt detected (sticky, W1C) |
| [3] | SRAM_WRITE_LOCK_ERR | SRAM write to locked region (sticky, W1C) |
| [4] | AXI_SLVERR | AXI slave error response (sticky, W1C) |
| [5] | AXI_DECERR | AXI decode error response (sticky, W1C) |
| [6] | DRBG_ERR | DRBG Sampler error (timeout or stream error) (sticky, W1C) |
| [7] | WIPE_STATE | Wipe state event (rising edge of wipe_state input) (sticky, W1C) |

**Deferred IRQ source** (not in current RDL, planned for future phase):

| Bit | Source | Description |
|-----|--------|-------------|
| TBD | DEMOTE | Demote - debug in product LC mode (Phase 3) |

All KMCSR interrupt sources have corresponding IRQ_ENABLE and IRQ_SET bits. Mailbox inbound notification is delivered to the PicoRV32 as a separate direct mailbox IRQ rather than through `KMCSR.IRQ_STATUS`.

### Error Condition Outputs

| Signal | Source | Description |
|--------|--------|-------------|
| cpu_trap_o | PicoRV32 trap output | KM has encountered unrecoverable fault; will not respond to further commands |
| recoverable_err_o | RECOVERABLE_ERR register [0] | Recoverable fault handled by ISR; remains set until SEP commands clear |

### SRAM Protection

- **Scrambling**: PRESENT-inspired cipher for data obfuscation (SCRAMBLER_KEY has no reset)
- **Region Locking**: 32 regions × 512 bytes, write-1-only-set lock bits
- **Violation Detection**: Sticky bits indicate which regions had write violations

### Test Infrastructure

- **Virtual UART**: VUART_TX, VUART_RX, VUART_STATUS for firmware-testbench communication
- **Test Protocol**: TB_RESULT, TB_SIGNATURE, TB_ERRCODE, TB_SUBTEST, TB_CMD for test pass/fail reporting
- **Debug Register**: Magic constant 0xCAFEBEEF for connectivity verification

## Key Transfer Flow

### Phase 1 Key Loading and Transfer

1. **SEP provisions key via KPVLP**:
   - KM firmware sets UNLOCK_SEP=1 for target slot
   - SEP writes key data to KPVLP_KEY_ENTRY
   - SEP sets KPVLP_CTRL (DEST_VALID, LAST_DWORD)
   - KM firmware sets LOCK_WRITE=1 to protect key

2. **SEP requests key transfer via mailbox**:
   - SEP sends KEY_TRANSFER command
   - Command specifies source slot and target engine

3. **KM firmware transfers key**:
   - KM reads key from KPV (if LOCK_USE=0)
   - KM verifies DEST_VALID permits target engine
   - KM writes key to crypto engine port (OTBN/AES/KMAC/HMAC via private key bus)
   - KM sends success/failure response via mailbox

4. **Crypto engine uses key**:
   - Key available in engine's key storage
   - Engine performs cryptographic operations

## Crypto Engine Key Storage

### Overview

The KM transfers keys to cryptographic accelerators via dedicated key storage register blocks on the private secret key bus. Each crypto engine has a wrapper with 2-share key registers and a KEY_VALID control bit.

### Key Register Architecture

Each crypto engine wrapper provides:

| Component | Description |
|-----------|-------------|
| KEY_SHARE0 | First key share (write-only, reads return 0, no reset) |
| KEY_SHARE1 | Second key share (write-only, reads return 0, no reset) |
| KEY_CTRL | Control register with KEY_VALID flag (resets to 0) |

### Per-Engine Details

| Engine | Address | Share Size | KEY_SHARE0 Offset | KEY_SHARE1 Offset | KEY_CTRL Offset | Status |
|--------|---------|------------|-------------------|-------------------|-----------------|--------|
| OTBN | 0x0001_8000 | 384 bits (12 words) | 0x000-0x02C | 0x030-0x05C | 0x060 | Instantiated |
| AES | 0x0001_9000 | 256 bits (8 words) | 0x000-0x01C | 0x020-0x03C | 0x040 | Instantiated |
| KMAC | 0x0001_A000 | 256 bits (8 words) | 0x000-0x01C | 0x020-0x03C | 0x040 | Instantiated |
| HMAC | 0x0001_B000 | 256 bits (8 words) | 0x000-0x01C | 0x020-0x03C | 0x040 | Instantiated |

### Security Properties

- **Write-only**: Key share reads return 0 (no key readback)
- **No reset**: Key data powers up undefined (FR-0000-149)
- **KEY_VALID**: Must be set to 1 after writing key shares to make key available to crypto engine
- **2-share architecture**: Matches OpenTitan sideload interface (hw_key_req_t)

### Key Share XOR Masking (Firmware)

When transferring a key to a crypto engine, the KM firmware splits the key into two shares using XOR masking:

1. Pad the key with random data from DRBG up to the full share register size
2. Collect a random array `RAND` from DRBG (same size as padded key)
3. Compute `KEY_RAND[i] = KEY[i] XOR RAND[i]` for each word
4. Write `RAND` to `KEY_SHARE0` and `KEY_RAND` to `KEY_SHARE1`
5. Write order is pseudorandomly interleaved across both share registers (Fisher-Yates shuffle) to enhance obfuscation

The crypto engine internally XORs the two shares to reconstruct the original key. This prevents a single-trace side-channel attack from recovering the full key.

## Error Detection

### Parity Checking

- **Byte-wise odd parity** for ROM and SRAM
- Cannot be disabled
- Errors trigger IRQ (if enabled) and set sticky status

### AXI Error Detection

- **SLVERR**: Slave error response (e.g., ROM write attempt, KPV access violation)
- **DECERR**: Decode error for unmapped addresses
- Monitored on both read and write transactions from KM CPU's AXI master interface

## KM Firmware

### Firmware Overview

The KM firmware runs on the PicoRV32 CPU and is responsible for:

- Boot and secure initialization
- Mailbox command/response protocol
- Message receive/transmit buffering
- Command parsing and validation
- Interrupt and fault handling
- DRBG and firmware PRNG support
- KPV and KPVLP management
- Crypto-engine sideload drivers (HMAC, KMAC, AES, OTBN)
- CRC support functions
- Key registry and key lifecycle management
- Main event loop and runtime state management

---

### Command Message Format

All messages use this container format:

| Word | Content |
|------|---------|
| 0 | `HEADER_CRC8[31:24] | PAYLOAD_LEN[23:16] | COMMAND_ID[15:8] | CMD_SEQ_NUM[7:0]` |
| 1..N | `PAYLOAD_WORD[0..N-1]` |
| Final (if payload) | `PAYLOAD_CRC32[31:0]` |

**Field descriptions:**

| Field | Width | Description |
|-------|-------|-------------|
| CMD_SEQ_NUM | 8-bit | Sequence number, reset to 0 on init or flush, must increment per message, rolls over after max |
| COMMAND_ID | 8-bit | Command ID |
| PAYLOAD_LEN | 8-bit | Payload length in 32-bit words (0 = no payload, no CRC32) |
| HEADER_CRC8 | 8-bit | CRC-8/ROHC over 24-bit header {PAYLOAD_LEN, COMMAND_ID, CMD_SEQ_NUM} in LE byte order |
| PAYLOAD_CRC32 | 32-bit | CRC-32C (Castagnoli) over payload words in LE byte order (absent when PAYLOAD_LEN=0) |

- Max payload: 255 words (1020 bytes)

---

### Command Set

| ID | Command | Payload | Description |
|----|---------|---------|-------------|
| 0x00 | CMD_HW_VER | None | Returns HW version (MAJOR/MINOR/PATCH) |
| 0x01 | CMD_ROM_VER | None | Returns ROM FW version |
| 0x02 | CMD_SRAM_VER | None | Returns SRAM FW version (**ROM rev: always returns failure** — reserved for future SRAM FW flow) |
| 0x03 | CMD_STAT | None | Returns KM status (recov_fault bit) |
| 0x04 | CMD_RECOV_ACK | None | Acknowledges recoverable fault, clears RECOVERABLE_ERR |
| 0x05 | CMD_EXEC_ROM | MODE[1:0] | Sets KM into ROM execution mode; prevents SRAM firmware load until next reset (**WIP — not yet implemented in firmware**) |
| 0x06 | CMD_FIRM | MODE[1:0], IMAGE_BASE_ADDRESS[13:0], JUMP_ADDRESS[31:0] | Initiates SRAM firmware load: reads image from next mailbox frame until separator, computes CRC-32C from SRAM readback, jumps to loaded firmware on success (unrecoverable fault on CRC mismatch; no RESP_CMD) (**WIP — not yet implemented in firmware**) |
| 0x20 | CMD_KPVLP_SLOT_REQ | SLOT_REQ[2:0] | Requests SLOT_REQ+1 consecutive KPV slots for KPVLP |
| 0x21 | CMD_KPVLP_KEY_REGISTER | BASE_SLOT_INDEX[4:0], KEY_SIZE[6:0], DEST_VALID[7:0], KEY_CRC32[31:0] | Registers KPVLP-loaded key, returns KEY_HANDLE |
| 0x22 | CMD_KEY_GENERATE | REQ_SIZE[6:0], DEST_VALID[7:0] | Generates random key from DRBG, returns KEY_HANDLE |
| 0x23 | CMD_KEY_REVOKE | KEY_HANDLE[7:0] | Revokes key, read-locks and destroys handle |
| 0x24 | CMD_KEY_TRANSFER | KEY_HANDLE[7:0], DEST_ENGINE[7:0] | Transfers key to crypto engine(s) |
| 0x25 | CMD_ENGINE_SHRED | DEST_ENGINE[7:0] | Purges crypto engine key registers with random data |

**DEST_VALID / DEST_ENGINE bit encoding:**

| Bit | Field Name | Engine |
|-----|------------|--------|
| 0 | `hmac_sha2` | HMAC |
| 1 | `kmac_sha3` | KMAC |
| 2 | `aes` | AES |
| 3 | `otbn` | OTBN |
| 4 | `abr_mldsa_seed` | Adams Bridge ML-DSA seed |
| 5 | `abr_mlkem_d` | Adams Bridge ML-KEM seed D |
| 6 | `abr_mlkem_z` | Adams Bridge ML-KEM seed Z |
| 7 | `abr_mlkem_msg` | Adams Bridge ML-KEM message |

**CMD_KPVLP_SLOT_REQ payload word layout:**

| Bits | Field | Description |
|------|-------|-------------|
| [2:0] | SLOT_REQ | Number of consecutive slots = SLOT_REQ+1 (max 7+1=8) |
| [31:3] | RESERVED | Write as 0 |

**CMD_KPVLP_KEY_REGISTER payload word layout (4 words):**

| Word | Bits | Field | Description |
|------|------|-------|-------------|
| 0 | [4:0] | BASE_SLOT_INDEX | Base index of the KPV slot to register |
| 1 | [6:0] | KEY_SIZE | Key size in 32-bit words = KEY_SIZE+1 |
| 2 | [7:0] | DEST_VALID | Permitted destination engines (see bit table above) |
| 3 | [31:0] | KEY_CRC32 | CRC-32C over all key words written via KPVLP |

**CMD_KEY_GENERATE payload word layout (2 words):**

| Word | Bits | Field | Description |
|------|------|-------|-------------|
| 0 | [6:0] | REQ_SIZE | Requested key size in 32-bit words = REQ_SIZE+1 |
| 1 | [7:0] | DEST_VALID | Permitted destination engines (see bit table above) |

---

### Response Message Format

Same container format as commands:

| Word | Content |
|------|---------|
| 0 | `HEADER_CRC8[31:24] | PAYLOAD_LEN[23:16] | RESPONSE_ID[15:8] | RESP_SEQ_NUM[7:0]` |
| 1..N | `PAYLOAD_WORD[0..N-1]` |
| Final (if payload) | `PAYLOAD_CRC32[31:0]` |

Response sequence numbers are independent from command sequence numbers.

---

### Response Set

| ID | Response | Description |
|----|----------|-------------|
| 0x00 | RESP_CMD | General command response (sent for every direct command) |
| 0x55 | RESP_KM_READY | Unsolicited: KM completed init and entered main loop |
| 0xFE | RESP_RECOVERABLE_FAULT | Unsolicited: KM encountered recoverable fault |
| 0xFF | RESP_UNRECOVERABLE_FAULT | Unsolicited: KM encountered unrecoverable fault, CPU halted |

**RESP_CMD payload (3 or 4 words):**

| Word | Bits | Field | Description |
|------|------|-------|-------------|
| 0 | [7:0] | CMD_SEQ_NUM | Sequence number echoed from source command |
| 1 | [7:0] | COMMAND_ID | Command ID echoed from source command |
| 2 | [7:0] | RETURN_CODE | Signed 8-bit return code (see table below) |
| 3 | [31:0] | RETURN_ARG | Optional. Present only on success or select error codes (see per-command table below) |

**Return codes with RETURN_ARG values:**

| Code | Name | Description | RETURN_ARG (if present) |
|------|------|-------------|-------------------------|
| 0 | success | Command succeeded | Command-specific (see RESP_CMD RETURN_ARG Layouts below) |
| -1 | failure | General failure; no action taken | Not present |
| -2 | header_crc | Header CRC-8/ROHC invalid; no action taken | Actual CRC-8/ROHC received |
| -3 | cmd_noseq | CMD_SEQ_NUM was not the expected value; no action taken | Next expected sequence number |
| -4 | invalid_cmd | COMMAND_ID did not map to a valid command; no action taken | Not present |
| -5 | invalid_len | PAYLOAD_LEN did not match message frame size; no action taken | Actual payload length received |
| -6 | payload_crc | Payload CRC-32C invalid; no action taken | Actual CRC-32C received |
| -7 | invalid_arg | One or more payload arguments invalid; no action taken | Index of the invalid payload word (if index exceeds payload, a required argument was missing) |

---

### RESP_CMD RETURN_ARG Layouts (Success)

For `RETURN_CODE = success (0)`, each command has a specific `RETURN_ARG` bit layout:

| Command | RETURN_ARG [31:0] | Notes |
|---------|-------------------|-------|
| CMD_HW_VER | `{RSVD[31:24], HW_MAJOR_REV[23:16], HW_MINOR_REV[15:8], HW_PATCH_REV[7:0]}` | Hardware version |
| CMD_ROM_VER | `{RSVD[31:24], ROM_MAJOR_REV[23:16], ROM_MINOR_REV[15:8], ROM_PATCH_REV[7:0]}` | ROM FW version |
| CMD_SRAM_VER | `{RSVD[31:24], SRAM_MAJOR_REV[23:16], SRAM_MINOR_REV[15:8], SRAM_PATCH_REV[7:0]}` | SRAM FW version (not present in ROM rev — always failure) |
| CMD_STAT | `{RSVD[31:1], recov_fault[0]}` | KM status: bit[0]=RECOVERABLE_ERR |
| CMD_RECOV_ACK | Not present | — |
| CMD_EXEC_ROM | Not present | No RETURN_ARG; success response only (**WIP**) |
| CMD_FIRM | Not present | No RESP_CMD; on success firmware jumps to SRAM; on CRC failure triggers unrecoverable fault (**WIP**) |
| CMD_KPVLP_SLOT_REQ | `{RSVD[31:11], SLOT_GRANT[10:8], RSVD[7:5], BASE_SLOT_INDEX[4:0]}` | SLOT_GRANT always equals SLOT_REQ |
| CMD_KPVLP_KEY_REGISTER | `{RSVD[31:8], KEY_HANDLE[7:0]}` | Handle for the registered key |
| CMD_KEY_GENERATE | `{RSVD[31:24], DEST_VALID[23:16], REQ_SIZE[14:8], KEY_HANDLE[7:0]}` | Granted key parameters |
| CMD_KEY_REVOKE | `{RSVD[31:8], KEY_HANDLE[7:0]}` | Handle of the revoked key |
| CMD_KEY_TRANSFER | `{RSVD[31:16], DEST_ENGINE[15:8], KEY_HANDLE[7:0]}` | Engines to which the key was transferred |
| CMD_ENGINE_SHRED | `{RSVD[31:8], DEST_ENGINE[7:0]}` | Engines that were purged |

---

### Recoverable Fault Codes (RESP_RECOVERABLE_FAULT)

**Payload:** `{RSVD[31:8], FAULT_CODE[7:0]}` (signed 8-bit code, 1 word)

| Code | Name | Description |
|------|------|-------------|
| -1 | key_slot_crc | Key slot CRC-32C check failed; triggering operation was aborted |
| -2 | rx_buff_oflow | Message receive buffer overflow — a single incomplete message filled the buffer; KM flushed all message buffers and mailbox FIFOs |
| -3 | mbox_overflow | KM outgoing mailbox FIFO overflow; KM flushed all message buffers and mailbox FIFOs |
| -4 | mbox_underflow | KM incoming mailbox FIFO underflow; KM flushed all message buffers and mailbox FIFOs |
| -5 | flushed_by_sep | Mailbox flushed by SEP; KM flushed all message buffers (mailbox FIFOs not re-flushed by KM) |

---

### Unrecoverable Fault Codes (RESP_UNRECOVERABLE_FAULT)

**Payload:** `{RSVD[31:8], FAULT_CODE[7:0]}` (signed 8-bit code, 1 word)

| Code | Name | Description |
|------|------|-------------|
| -1 | wipe_state | Wipe state signal asserted |
| -2 | rom_parity | ROM parity error |
| -3 | sram_parity | SRAM parity error |
| -4 | rom_write | ROM write error |
| -5 | sram_write_lock | SRAM write-lock violation |
| -6 | axi_decerr | AXI DECERR |
| -7 | axi_slverr | Unhandled AXI SLVERR |
| -8 | drbg_err | DRBG read error |
| -9 | illegal_insn | Illegal instruction |
| -10 | bus_error | CPU bus error or misaligned access |
| -11 | ebreak | EBREAK instruction executed |
| -12 | spurious_irq | Unrecognised IRQ source |

---

### Key Registry

- Firmware maintains handle-to-slot mapping (key registry)
- KEY_HANDLE: unique 8-bit number (0x00 = null/reserved)
- Handles are incremented as allocated, never reused
- Registry stores: handle → {base_slot_index, key_CRC32}
- Registry stores: slot_index → handle
- Key integrity verified via CRC-32C before transfers

---

### Message Buffers

The KM firmware uses fixed-size SRAM-based circular buffers for message receive and transmit. These sit between the mailbox FIFOs and the command/response handler.

- **Buffer size:** 257 words per buffer (1 header word + 255 payload words + 1 CRC-32C word)
- **Capacity:** Each buffer can hold multiple message frames simultaneously
- **Pointers:** Buffer tracks start and end pointers per message frame stored
- **Partial reads/writes:** Interface supports reading and writing partial frames
- **Overflow/underflow protection:** Interface prevents buffer overflow and underflow
- **IRQ gating:** Mailbox IRQs must be disabled during any buffer read/write operation and restored to their previous state on completion; this prevents synchronization hazards

---

### Boot Sequence

1. Enable all fault/error interrupts
2. Initialize DRBG sampler (wait for DRBG_READY)
3. Seed firmware PRNG (xoshiro128++) from DRBG (4 words)
4. If SRAM scrambler not enabled: provision key (SHRED_ITER+1 DRBG writes), enable & lock scrambler, restart CPU (jump to 0)
5. Initialize KPV: init scrambler, enable, lock, shred all slots
6. Initialize and shred HMAC key interface
7. Initialize and shred KMAC key interface
8. Initialize and shred AES key interface
9. Initialize and shred OTBN key interface
10. Initialize message buffers
11. Clear and disable outgoing mailbox FIFO interrupt
12. Clear and enable incoming mailbox FIFO interrupt
13. Send RESP_KM_READY to SEP
14. Enter main event loop

---

### Shred Operations

- Use xoshiro128++ PRNG for random data generation
- Fisher-Yates shuffle for pseudorandom register access order
- Each shred: SHRED_ITER+1 total passes

**KPV shred:** Clear CTRL regs, then write random data to key data regs in shuffled order, reseeding PRNG each pass. Write-locked slots are skipped (both CTRL clear and key data writes).

**KPV slot shred:** Same as KPV shred but for a single slot. Returns error if the slot is write-locked.

**Engine shred:** Clear KEY_VALID, write random data to both key shares in shuffled interleaved order.

**SRAM shred (unrecoverable only):** One Weyl sequence pass + two xoshiro128++ passes, entirely in assembly (no SRAM reads).

**Note:** During wipe_state, PRNG is NOT reseeded from DRBG (avoids hang if DRBG unavailable).

#### xoshiro128++ PRNG Details

- **State:** 128-bit internal state as four 32-bit words `s[0]`, `s[1]`, `s[2]`, `s[3]`
- **Seeding:** Read 4 consecutive words from DRBG into `s[0]`–`s[3]`. If all four words are zero (absorbing fixed point), set `s[0] = 1` before use
- **Reseeding:** On-demand via function call from DRBG; reseeded between shred passes for KPV shred
- **SRAM shred constraint:** During SRAM shredding, PRNG state, write pointer, loop bound, and temporaries must reside entirely in CPU registers — the PRNG state must not be stored in SRAM at any point during the shred loop
- **Cryptographic use:** PRNG must NOT be used for generating cryptographic key material; all key material must be sourced from the DRBG directly

#### Register Access Shuffling Details

All shred and key write operations on KPV, crypto engine key registers, SRAM scrambler key, and KPV scrambler key use the Fisher-Yates shuffle to determine pseudorandom access order:

- Operates on an index array `[0, 1, ..., n-1]`, producing a uniformly random permutation
- Random index `j` in range `[0, bound)` is generated using **bit-masked rejection sampling** from a PRNG-fed 32-bit bit pool
- Bits are consumed from the pool LSB to MSB; pool is refilled from the PRNG when insufficient bits remain
- This avoids division and eliminates modulo bias

---

### DRBG Driver Patterns

- **Initialize DRBG:** Wait for STATUS.DRBG_READY before any reads
- **Get Word:** Single random word read from DATA register
- **Get Block:** Enable CFG.PREFETCH → read N words → disable CFG.PREFETCH. Prefetch is used only during block reads for performance.

---

### Mailbox ISR Error Priority

Within the mailbox interrupt handler, error conditions are checked in strict priority order:

1. **OUTBOUND_OVERFLOW** (highest) — set RECOVERABLE_ERR, flush all buffers + FIFOs, send RESP_RECOVERABLE_FAULT (`mbox_overflow`) directly via FIFO (bypass transmit buffer), reset sequence numbers
2. **INBOUND_UNDERFLOW** — same flow with `mbox_underflow` fault code
3. **FLUSHED_BY_SEP** (lowest error) — set RECOVERABLE_ERR, flush all message buffers only (do NOT re-flush FIFOs), send RESP_RECOVERABLE_FAULT (`flushed_by_sep`) directly via FIFO, reset sequence numbers

**Note:** All recoverable fault responses from the mailbox ISR are written directly to the outgoing mailbox FIFO, bypassing the message transmit buffer entirely. This ensures the fault notification reaches the SEP even if the transmit buffer is in an inconsistent state.

---

### Recoverable Fault Flow

1. KM asserts RECOVERABLE_ERR
2. KM flushes mailbox (only for mailbox/buffer-related faults)
3. KM sends RESP_RECOVERABLE_FAULT with fault code
4. KM only processes: CMD_HW_VER, CMD_ROM_VER, CMD_SRAM_VER, CMD_STAT, CMD_RECOV_ACK
5. SEP sends CMD_RECOV_ACK to clear fault
6. KM deasserts RECOVERABLE_ERR and resumes normal operation

---

### Unrecoverable Fault Flow

1. KM shreds all crypto engine key sideload registers
2. KM flushes mailbox
3. KM sends RESP_UNRECOVERABLE_FAULT with fault code
4. KM shreds entire SRAM (Weyl + 2× xoshiro128++)
5. KM CPU halts (trap), sets cpu_trap_o
6. SEP must hard-reset KM

---

### SEP Key Loading Flow

1. SEP sends CMD_KPVLP_SLOT_REQ → KM allocates slots using **DRBG-seeded random base slot selection**, shreds allocated slots, sets UNLOCK_SEP
2. SEP writes key via KPVLP interface
3. SEP sends CMD_KPVLP_KEY_REGISTER (with key params + CRC-32C) → KM makes a **local copy** of key data/control, validates CRC, write-locks slots, then **verifies KPV unchanged from local copy** (anti-tampering), returns KEY_HANDLE
4. SEP uses KEY_HANDLE for subsequent operations

**Slot Allocation Details:** The base key slot is selected randomly using DRBG data. Allocated slots must not be write-locked, read-locked, or associated with an existing key handle. Slots already allocated to the SEP are also avoided. The same random slot selection is used by CMD_KEY_GENERATE.

**Key Registration Anti-Tampering:** During CMD_KPVLP_KEY_REGISTER, the KM firmware makes a temporary local copy of the key data and control registers before write-locking. After write-locking (which revokes SEP write permission), it verifies that the KPV contents remain identical to the local copy. If any change is detected (indicating concurrent tampering by the SEP), the command returns failure.

---

### Interrupt Handler Constraints

The PicoRV32 has a minimal interrupt controller with no hardware interrupt priority and no support for nested interrupts. Firmware must manage these limitations explicitly:

- **No hardware priority:** All interrupt priority is managed in software within the main ISR
- **No nested interrupts:** A new interrupt that arrives while the ISR is executing generates a pending signal that re-executes the ISR after it returns — higher-priority interrupts cannot preempt lower-priority ones
- **Single ISR call:** All currently active interrupts are processed within a single ISR invocation
- **ISR must be short and deterministic:** Long ISR execution delays handling of subsequently arriving interrupts

### Interrupt Priority (highest to lowest)

| Priority | Source |
|----------|--------|
| 1 | EBREAK / Illegal instruction |
| 2 | Bus error / misaligned access |
| 3 | Wipe state |
| 4 | ROM parity error |
| 5 | SRAM parity error |
| 6 | ROM write error |
| 7 | SRAM write lock error |
| 8 | AXI DECERR |
| 9 | AXI SLVERR |
| 10 | DRBG error |
| 11 | Mailbox |
| 12 | Spurious IRQ (catch-all) |

---

### CRC Specifications

| Algorithm | Width | Poly | Init | RefIn | RefOut | XorOut |
|-----------|-------|------|------|-------|--------|--------|
| CRC-8/ROHC | 8 | 0x07 | 0xFF | true | true | 0x00 |
| CRC-32C (Castagnoli) | 32 | 0x82F63B78 (reflected) | 0xFFFFFFFF | true | true | 0xFFFFFFFF |

---

## PCPI CRC Accelerator Instructions

### Overview

The KM PicoRV32 CPU includes a PCPI (Pico Co-Processor Interface) CRC accelerator that provides three hardware-accelerated CRC instructions. These instructions replace the software CRC lookup-table routines used by firmware for message header validation (CRC-8/ROHC), payload integrity (CRC-32C), and key integrity checks (CRC-32C), reducing CPU cycles while preserving identical externally visible CRC results.

**Design Location**: `hw/comp/key_manager/rtl/picorv32_pcpi_crc.sv`, `hw/comp/key_manager/rtl/km_crc_engine.sv`

**Reference**: `specs/2130-add-pcpi-crc-instructions/spec.md`

### Instruction Set

All three instructions use R-type encoding under the `custom0` opcode (`7'b0001011`) with `funct7 = 7'b0101100`. The `funct3` field selects the CRC mode:

| Instruction | funct3 | rs1 (operand 1) | rs2 (operand 2) | rd (result) |
|-------------|--------|------------------|------------------|-------------|
| CRC-32C word update | `3'b000` | Current CRC-32C state | Next 32-bit input word | Updated CRC-32C state |
| CRC-32C byte update | `3'b001` | Current CRC-32C state | Next byte in bits [7:0] | Updated CRC-32C state |
| CRC-8/ROHC update | `3'b010` | Current CRC-8 state in bits [7:0] | Next byte in bits [7:0] | Updated CRC-8 state in bits [7:0] |

### Behavioral Properties

- **CRC-32C word update**: Processes the full 32-bit `rs2` operand as four bytes in little-endian order: `[7:0]`, `[15:8]`, `[23:16]`, `[31:24]`. Takes 4 cycles (one byte-update per cycle).
- **CRC-32C byte update**: Processes only `rs2[7:0]`; bits `[31:8]` are ignored. Takes 1 cycle.
- **CRC-8/ROHC update**: Processes only `rs2[7:0]`; bits `[31:8]` are ignored. Returns updated CRC in `rd[7:0]` with `rd[31:8]` zero-extended. Takes 1 cycle.
- **Chaining state**: CRC-32C instructions expose the running remainder before final XOR. Firmware seeds `0xFFFF_FFFF` and applies the final XOR (`^ 0xFFFF_FFFF`) only when the externally visible CRC value is needed.
- **CRC-8/ROHC state**: Carried in bits `[7:0]` of the operand and result, with result bits `[31:8]` zero-extended.
- **Unrecognized encodings**: Any `funct3` value other than `000`, `001`, `010` under the same `funct7`/`opcode` is not claimed by the CRC PCPI block; `pcpi_wait`, `pcpi_ready`, and `pcpi_wr` remain deasserted, preserving PicoRV32's illegal-instruction behavior.

### CRC Polynomials

| Algorithm | Reflected Polynomial | Used by |
|-----------|---------------------|---------|
| CRC-32C (Castagnoli) | `0x82F63B78` | Payload CRC, key integrity CRC |
| CRC-8/ROHC | `0xE0` (reflected form of `0x07`) | Message header CRC |

### Firmware Integration

The PCPI CRC accelerator is required in the wrapper-integrated KM PicoRV32 environment. Firmware invokes the instructions unconditionally — no runtime capability probe or software fallback path is used.

**Low-level wrappers** (1:1 mapping to hardware instructions):

| Function | Signature |
|----------|-----------|
| `rom_picorv32_crc32c_word_update` | `uint32_t (uint32_t state, uint32_t word)` |
| `rom_picorv32_crc32c_byte_update` | `uint32_t (uint32_t state, uint32_t data)` |
| `rom_picorv32_crc8_rohc_update` | `uint32_t (uint32_t state, uint32_t data)` |

**High-level APIs** (preserved signatures, unchanged externally visible results):

| Function | Signature | Internal change |
|----------|-----------|-----------------|
| `rom_crc8_rohc` | `uint8_t (const uint8_t *data, uint32_t len)` | Uses PCPI CRC-8 wrapper per byte |
| `rom_crc32c` | `uint32_t (const uint8_t *data, uint32_t len)` | Uses PCPI CRC-32C word wrapper for full words, byte wrapper for remaining 1-3 bytes |

### Hardware Architecture

The CRC accelerator consists of two modules:

1. **`picorv32_pcpi_crc.sv`** — PCPI interface module. Decodes instruction encoding, captures operands, dispatches to the CRC engine, and returns the result via `pcpi_rd_o`.
2. **`km_crc_engine.sv`** — Shared byte-update datapath. Performs reflected CRC byte updates using a configurable polynomial. CRC-32C word mode runs 4 byte-update cycles; CRC-32C byte and CRC-8 modes run 1 cycle each.

### Integration Notes

- No new top-level Key Manager ports or CSRs are introduced for this feature.
- `picorv32_wrapper.sv` enables the external PCPI path (`ENABLE_PCPI = 1'b1`) and connects it to `picorv32_pcpi_crc.sv`.
- Existing PicoRV32 multiply/divide support remains enabled and functionally unchanged.
- The chosen `funct7` (`7'b0101100`) does not overlap PicoRV32's built-in IRQ custom instructions.

---

## SEP Reset Controller

The SEP Reset Controller at `0x10A5_0000` provides software-controllable resets for the KM and crypto accelerators. The 64-bit `SW_RESET_N` register has per-IP fields (0=assert reset, 1=release):

| Bit | Field | Description |
|-----|-------|-------------|
| 0 | km_sw_rst_n | KM reset |
| 1 | otbn_sw_rst_n | OTBN reset |
| 2 | aes_sw_rst_n | AES reset |
| 3 | hmac_sw_rst_n | HMAC reset |
| 4 | kmac_sw_rst_n | KMAC reset |

On power-up, all fields default to 0 (IPs held in reset). SEP firmware writes 1 to release. Each bit is ANDed with system reset and passed through a 2-FF synchronizer. Hardware reset always takes precedence.

---

## References

- RDL Specifications: `key_manager.rdl`, `km_csr.rdl`, `km_kpv.rdl`, `km_kpv_kpvlp.rdl`, `km_mailbox_km.rdl`, `km_mailbox_sep.rdl`, `km_drbg_sampler.rdl`, `aes_wrapper_key.rdl`, `otbn_wrapper_key.rdl`, `kmac_wrapper_key.rdl`, `hmac_wrapper_key.rdl`
- Main Specification: `specs/main-spec-km.md`
- Feature Specification: `specs/2130-add-pcpi-crc-instructions/spec.md`
- Open Chiplet Harness Specification (v0.87, 3/9/2026)
- Key Manager (KM) Firmware Specification

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-01-21 | - | Initial version based on Open Chiplet Harness Specification |
| 1.1 | 2026-01-22 | - | Updated for spec v0.5 |
| 2.0 | 2026-02-02 | - | Major revision aligned with RDL specifications. Updated memory map, removed Phase 2/3 features not yet implemented, added phased development section. |
| 3.0 | 2026-02-11 | - | Major revision aligned with MVP spec and latest RDL. Added: DRBG Sampler (0xF000), KPV Scrambling (KPV_SCRAMBLER_KEY/CTRL), SEP OTP Data (36+1 registers), Wipe State (emergency wipe with KPV zeroing), RECOVERABLE_ERR register, Error Condition Outputs (cpu_trap, recoverable_err). Updated: memory map (DRBG region, mailbox 4KB), KPV CTRL (CLEAR bit removed → Reserved, hwclr on lock bits, swwel on protected fields), IRQ_STATUS (added DRBG_ERR[7] and WIPE_STATE[8]), datapath no-reset security model (FR-0000-149). |
| 3.1 | 2026-02-23 | - | Added SEP-facing address (0x1093_0000, 4 KB) and SEP interconnect context (Crypto Periph slave access). Added dual-bus crypto architecture (KM masters secret key bus). Added KPV concurrent access restriction. Noted Demote IRQ source as deferred. Renamed internal memory map section for clarity. |
| 4.0 | 2026-03-03 | - | Major update aligned with latest RDL and unified spec. VERSION register: new semantic versioning layout (RSVD/MAJOR/MINOR/PATCH), version 1.0.0, writes return SLVERR. OTP registers: consolidated to 1 life cycle register, removed OTP_WRITE_COUNT, read-through from OTP port (34 registers total). Mailbox: added WRITE_SEPARATOR register (both sides), WRITE_SPACE_AVAIL IRQ source, shifted register offsets. Added Crypto Engine Key Storage section (2-share architecture, KEY_VALID, write-only). Updated SEP-facing address to 0x1092_0000. |
| 4.1 | 2026-03-07 | - | OTP_DEMOTION_STATE: updated from single 2-bit VALUE field to two 2-bit fields (demote_1_value[1:0], demote_2_value[3:2]) per latest km_csr.rdl. HMAC Key Storage: marked as NOT instantiated in key_manager.rdl — HMAC uses software-writable KEY registers, not a sideload interface. Updated block diagram, memory map, dual-bus architecture, and key transfer sections to reflect HMAC exclusion from private key bus. |
| 4.2 | 2026-03-09 | - | Corrected the spec to match the current implementation: HMAC key storage is instantiated on the private key bus, KMCSR IRQ bits are the 8 sticky fault sources only (mailbox inbound remains a separate direct CPU IRQ), and the SEP integration note now documents that OTP is connected while DRBG and `wipe_state` are still tied off in `sep_crypto`. |
| 5.0 | 2026-03-16 | - | Major update: added complete KM Firmware specification section. Added: command set (10 commands), response set (4 responses), message format (CRC-8/ROHC header, CRC-32C payload), key registry (8-bit handle system), boot sequence (14 steps), shred operations (PRNG-based with Fisher-Yates shuffle), recoverable/unrecoverable fault flows, SEP key loading flow, interrupt priority, CRC specifications. Resolves previous open questions about mailbox command format and supported function IDs. (**Note**: command count was 10 as of v5.0; CMD_FIRM_LOAD added in v6.1, corrected to CMD_EXEC_ROM + CMD_FIRM in v7.0.) |
| 5.1 | 2026-03-18 | - | Added firmware doc details not previously captured: per-command RESP_CMD RETURN_ARG bit layouts for all 10 commands; return code argument values; fault response payload structure (FAULT_CODE field) for RESP_RECOVERABLE_FAULT and RESP_UNRECOVERABLE_FAULT; DEST_VALID/DEST_ENGINE named bit fields (hmac_sha2, kmac_sha3, aes, otbn); CMD_SRAM_VER reserved in ROM revision; message buffers section (257-word circular buffers, IRQ gating); xoshiro128++ PRNG seeding details and all-zero guard; register access shuffling via bit-masked rejection sampling; interrupt handler constraints (no HW priority, no nested interrupts). Reference: ~/Documents/OCH/Key Manager (KM) Firmware.md |
| 5.2 | 2026-03-19 | - | Added firmware operational details from full KM Firmware Spec review and OCH Spec v0.87 cross-reference. New: crypto engine key share XOR masking algorithm (RAND→SHARE0, KEY⊕RAND→SHARE1 with interleaved random write order); DRBG-seeded random base slot selection for CMD_KPVLP_SLOT_REQ and CMD_KEY_GENERATE; key registration anti-tampering check (pre/post copy comparison after write-lock); KPV shred skip behavior for write-locked slots; KPV per-slot shred; DRBG driver patterns (Get Block prefetch enable/disable); mailbox ISR error priority order (overflow > underflow > flushed_by_sep) with direct FIFO write bypass for fault responses; SEP Reset Controller (0x10A5_0000) reference with SW_RESET_N register. Updated references to include OCH Spec v0.87 and KM Firmware Spec. |
| 6.0 | 2026-03-26 | - | Added PCPI CRC Accelerator Instructions section: three hardware CRC instructions (CRC-32C word update, CRC-32C byte update, CRC-8/ROHC byte update) via PicoRV32 PCPI interface. Includes instruction encoding table, behavioral properties, CRC polynomials, firmware integration (low-level wrappers and preserved high-level APIs), hardware architecture (picorv32_pcpi_crc.sv, km_crc_engine.sv), and integration notes. Added specs/main-spec-km.md and specs/2130-add-pcpi-crc-instructions/spec.md to references. |
| 6.1 | 2026-03-26 | - | Added CMD_FIRM_LOAD (0x05) to command set (11 commands, was 10). CMD_FIRM_LOAD initiates dynamic SRAM firmware loading: 3-word payload (IMAGE_BASE_ADDRESS, IMAGE_SIZE32, JUMP_ADDRESS), reads firmware image from next mailbox frame using separator framing, performs CRC-32C integrity check (unrecoverable fault on mismatch), and jumps to loaded firmware on success — no standard RESP_CMD. CMD_FIRM_LOAD is rejected during recoverable fault (not in allowed command set). Source: Key Manager (KM) Firmware Specification. **Note**: specs/main-spec-km.md still marks dynamic SRAM firmware loading as reserved/future; the KM Firmware Spec is authoritative for this feature. |
| 7.0 | 2026-04-10 | - | **Critical correction**: Replaced CMD_FIRM_LOAD (0x05) with two separate commands per KM Firmware Spec. **CMD_EXEC_ROM (0x05)**: 1-word payload (MODE[1:0]), sets ROM execution mode, prevents SRAM firmware load until reset — **WIP, not implemented in firmware**. **CMD_FIRM (0x06)**: 3-word payload (MODE[1:0], IMAGE_BASE_ADDRESS[13:0], JUMP_ADDRESS[31:0]), initiates SRAM firmware load reading until separator (no IMAGE_SIZE32), CRC-32C computed from SRAM readback (post-scrambling) — **WIP, not implemented in firmware**. Command count: 12 (was 11). Updated RESP_CMD RETURN_ARG table. Source: ~/Documents/OCH/'Key Manager (KM) Firmware.md'. |
