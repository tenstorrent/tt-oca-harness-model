# SEP eFuse SystemC TLM High-Level Design Specification

**Document Version:** 1.0
**Date:** 2026-04-27
**IP Module:** SEP eFuse Controller (sep_efuse)
**Modeling Approach:** SystemC TLM2.0, fuse-shadow register model with WOSET semantics

---

## Executive Summary

This document specifies the SystemC Transaction-Level Model (TLM) for the SEP eFuse Controller (`sep_efuse`). The model provides **read-only OTP shadow register access** and **write-set-only firmware update semantics** for fuse-backed security state.

### Key Architectural Characteristics

- **Fuse-shadow abstraction** — the model does not simulate the physical OTP array or the analog sense amplifiers. All fuse content is loaded into CSML shadow registers at `end_of_elaboration()` from CCI parameters (ini file).
- **WOSET callbacks** — eleven registers implement Write-One-Set semantics: firmware can set additional bits at runtime, but can never clear bits already set. This models the one-way nature of fuse programming.
- **Three address spaces** — the module exposes one memory window of `0x570` bytes covering: fuse shadow registers (0x000–0x3FF), OTP interface control stubs (0x400–0x41F), and eFuse MMR token comparison registers (0x500–0x56F).
- **Zero-recompile configuration** — all fuse content (22 scalar + 12 array fields) is driven by CCI parameters. Any fuse scenario can be tested by changing the ini file.
- **OTP data accessors for keymgr_tt** — `get_lc_state()` and `get_chiplet_uid()` provide stable pointers to fuse state consumed by `keymgr_tt` at `start_of_simulation`.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Features](#2-features)
3. [Port Interfaces](#3-port-interfaces)
4. [Memory-Mapped Registers](#4-memory-mapped-registers)
5. [Register Callbacks](#5-register-callbacks)
6. [Internal Architecture](#6-internal-architecture)
7. [Modeling Assumptions](#7-modeling-assumptions)
8. [Conclusion](#8-conclusion)

---

## 1. Introduction

### 1.1 SEP eFuse Overview

The SEP eFuse block contains the One-Time Programmable (OTP) fuse map for the SEP chiplet. After manufacture, the fuses are read by the OTP sense amplifiers and their values are latched into shadow registers accessible to SEP firmware over a 32-bit MMIO bus.

The fuse map covers:

- **Life-cycle state** (`LC_STATE`) — differentially encoded 4-bit value indicating chip lifecycle (unprovisioned, TEST_DEV, PROD, RMA, PROD_END)
- **Security control** (`SBOOT_DIS`, `TRANSIENT_RMA_EN`) — secure boot disable and transient RMA mode
- **Feature disable vectors** (`SIP_DIS_LO/HI`, `SYS_DIS_LO/HI`) — per-feature disable bitmaps consumed by `lc_ctrl`
- **Token material** (`RMA_SIP_TOKEN`, `RMA_CHIPLET_TOKEN`, `CLASS_KEY`) — 256-bit values used for RMA token matching and class-key derivation
- **Key revocation** (`CHIPLET_PUBK_REVOKE`) — bitmap marking which public-key slots are revoked
- **Anti-rollback versions** (`BL1_VERSION`, `BL2_VERSION`) — one-hot encoded boot-loader version fields; firmware can advance by setting more bits
- **Device identities** (`CHIPLET_UID`, `SIP_UID`, `SYS_UID`) — 256-bit unique identifiers
- **Public key hashes** (`SIP_PUBK`, `SYS_PUBK`, `PUBLIC_KEY_0`, `PUBLIC_KEY_1`) — 256-bit key digests
- **Platform configuration** (`STATUS_RPT`, `SEP_ROM_CTRL`, `SEP_SPI_CTRL_FIELD_EN`, SPI PHY fields) — ROM/SPI boot configuration

The eFuse interface control block (`EFUSE_INTERFACE_CTRL`) provides raw OTP access (read/program). The eFuse MMR block (`EFUSE_MMR`) hosts token comparison registers where firmware submits token candidates and reads SHA-256 comparison results.

### 1.2 Purpose of This Document

This high-level design specification defines the SystemC TLM implementation for virtual platform integration. It documents:

- Features included and excluded from the TLM model
- External port interface
- Complete memory-mapped register specifications across three address spaces
- WOSET callback implementations
- OTP data accessor methods
- Modeling assumptions and abstractions

### 1.3 Modeling Goals

1. **Enable SEP firmware development** — provide fuse shadow registers accessible via MMIO, matching the hardware register map
2. **Support boot-sequence simulation** — deliver `lc_state`, `sboot_dis`, `chiplet_uid`, `sip_pubk`, `bl1_version` and other boot-critical fields at elaboration
3. **Model firmware-driven fuse advancing** — WOSET registers (`LC_STATE`, `SIP_DIS`, `BL1_VERSION`, etc.) allow firmware to set additional bits at runtime, matching the one-way fuse programming model
4. **Zero-recompile scenario testing** — all 34 fuse parameters configurable via ini file

### 1.4 Target Audience

- SystemC model developers maintaining or extending `sep_efuse`
- VP architects integrating `sep_efuse` into the SEP subsystem
- SEP firmware engineers developing fuse-reading and boot-flow code
- Verification engineers creating or extending the test plan

---

## 2. Features

### 2.1 Is (MUST Model)

#### 2.1.1 Fuse Shadow Registers

All fuse content is loaded at `end_of_elaboration()` from CCI parameters into CSML shadow registers via `load_fuses()`. Once loaded, the values are accessible to SEP firmware via MMIO reads. The three register access categories:

**Read-Only (RO)** — loaded at elaboration; firmware writes are silently ignored:
`SBOOT_DIS`, `TRANSIENT_RMA_EN`, `RMA_SIP_TOKEN[8]`, `RMA_CHIPLET_TOKEN[8]`, `CLASS_KEY[8]`, `CHIPLET_UID[8]`, `SIP_PUBK[8]`, `SIP_UID[8]`, `SYS_PUBK[8]`, `SYS_UID[8]`, `STATUS_RPT`, `SEP_ROM_CTRL`, `SEP_SPI_CTRL_FIELD_EN`, `SPI_DISCOVERY_CTRL`, SPI PHY registers (×7), `PUBLIC_KEY_0[8]`, `PUBLIC_KEY_1[8]`, all RESERVED regions.

**Write-Set-Only (WOSET)** — loaded at elaboration; firmware can set additional bits via MMIO write:
`LOCKS_LO`, `LOCKS_HI`, `LC_STATE`, `SIP_DIS_LO`, `SIP_DIS_HI`, `SYS_DIS_LO`, `SYS_DIS_HI`, `CHIPLET_PUBK_REVOKE`, `BL1_VERSION[8]`, `BL2_VERSION[8]`.

**Read-Write (RW)** — firmware can freely read/write:
`EFUSE_PROGRAM_CTRL`, `EFUSE_READ_CTRL`, `EFUSE_READ_REQ_TIMEOUT`, `EFUSE_PROGRAM_REQ_TIMEOUT`, `RMA_SIP_TOKEN_I[8]`, `RMA_CHIPLET_TOKEN_I[8]`, `SEC_DISABLE_TOKEN_I[8]`, `TOKEN_MATCH` registers (preset via CCI param for VP testing).

#### 2.1.2 WOSET Callback Logic

Eleven registers implement Write-One-Set semantics via registered write callbacks. For each WOSET register, a private shadow value (e.g. `m_lc_state_val`) maintains the accumulated result. On every firmware write:

```
shadow_val |= written_value        // OR in the new bits
register   = shadow_val            // write accumulated value to CSML register
```

This ensures:
- Bits already set (from fuse load or prior writes) cannot be cleared
- Multiple firmware writes accumulate without losing prior state

#### 2.1.3 CCI Parameter-Driven Fuse Content

All 34 fuse fields are public `csml_param` members on `sep_efuse_model`. Scalar fields use `csml_param<uint32_t>`; 256-bit array fields use `csml_param<std::vector<uint32_t>>` with JSON array format in the ini file (`[w0, w1, w2, w3, w4, w5, w6, w7]`).

| Category | Parameters |
|----------|-----------|
| Life-cycle | `lc_state` |
| Security control | `sboot_dis`, `transient_rma_en` |
| Feature disable | `sip_dis_lo`, `sip_dis_hi`, `sys_dis_lo`, `sys_dis_hi` |
| Token material (256-bit) | `rma_sip_token`, `rma_chiplet_token`, `class_key` |
| Revocation / version | `chiplet_pubk_revoke`, `bl1_version`, `bl2_version` |
| Device identity (256-bit) | `chiplet_uid`, `sip_uid`, `sys_uid` |
| Public key hashes (256-bit) | `sip_pubk`, `sys_pubk`, `public_key_0`, `public_key_1` |
| Platform config | `status_rpt`, `sep_rom_ctrl`, `sep_spi_ctrl_field_en`, `spi_discovery_ctrl`, `spi_phy_dq_timing`, `spi_phy_dqs_timing`, `spi_phy_gate_lpbk`, `spi_phy_dll_slave`, `spi_phy_dll_master`, `spi_phy_misc`, `spi_rb_valid_time` |
| Token match presets | `rma_sip_token_match`, `rma_chiplet_token_match`, `sec_disable_token_match` |

#### 2.1.4 OTP Data Accessors

Two public accessors provide data to other VP modules:

```cpp
uint32_t        get_lc_state()    const;   // returns m_lc_state_val
const uint32_t* get_chiplet_uid() const;   // returns m_chiplet_uid_cache[8]
```

`m_chiplet_uid_cache[8]` is a private `uint32_t[8]` array populated at `end_of_elaboration()` from the `chiplet_uid` vector param. It provides a stable pointer (unlike `get_param_value()` which returns a temporary vector). Both accessors are safe to call after `end_of_elaboration()`.

These are consumed by `keymgr_tt` at `start_of_simulation()`:

```cpp
km_otp_data_t otp;
otp.lc_state = sep_efuse->get_lc_state();
std::memcpy(otp.chiplet_uid, sep_efuse->get_chiplet_uid(), 8 * sizeof(uint32_t));
keymgr->set_otp_data(otp);
```

#### 2.1.5 EFUSE_INTERFACE_CTRL_STATUS — efuse_sense_done

`EFUSE_INTERFACE_CTRL_STATUS` (offset 0x400) resets to `0x00000001` — bit[0] `efuse_sense_done` is permanently asserted in the VP model. This reflects the RTL behavior: after power-on fuse sensing completes, this bit stays high. The register has `write_mask=0x0` (fully read-only from software's perspective).

### 2.2 Is Not (EXCLUDE)

#### 2.2.1 Physical OTP Array

- Analog sense amplifier timing — not modeled; `efuse_sense_done=1` immediately at power-on
- Bit-level OTP programming via `EFUSE_PROGRAM_CTRL` — write control registers are RW stubs; no actual fuse blowing logic
- ECC / parity error injection — `STATUS_RPT` is loaded from a CCI param; no error generation
- OTP read-back via `EFUSE_PROGRAM_INTERFACE_RD_DATA` / `EFUSE_READ_INTERFACE_RD_DATA` — always return 0x0

#### 2.2.2 SHA-256 Token Matching

- Hardware SHA-256 computation for RMA/secure-disable token comparison — not implemented
- `TOKEN_EOP` go-bits (`rma_sip_token_go`, `rma_chiplet_token_go`, `secure_disable_token_go`) — singlepulse fields; hardware clears them and triggers comparison. In the VP, no comparison is performed; `TOKEN_MATCH` values are preset via CCI params before simulation starts
- The `RMA_SIP_TOKEN_I`, `RMA_CHIPLET_TOKEN_I`, `SEC_DISABLE_TOKEN_I` input registers are RW (firmware can write token candidates), but no hardware reaction follows

#### 2.2.3 Timing

- Sense amplifier latency — not modeled
- OTP read/program cycle timing — not modeled
- All register operations complete at `SC_ZERO_TIME`

---

## 3. Port Interfaces

| Category | Port Name | Type | Direction | Description |
|----------|-----------|------|-----------|-------------|
| **Register Bus** | `target_socket` (via `sep_efuse_base`) | `tlm_utils::simple_target_socket<..., 32>` | Target | 32-bit TLM target for MMIO register access. Memory window: `0x570` bytes covering all three address spaces. |

### 3.1 Interface Notes

1. **No OTP hardware bus** — raw fuse values are loaded from CCI params; there is no physical OTP bus port.
2. **No reset port** — initialization happens in `end_of_elaboration()`. Cold reset is not modeled at the TLM level.
3. **`get_lc_state()` and `get_chiplet_uid()` are C++ method calls** — not TLM sockets. Consumed by `och_sep_ss` at `start_of_simulation`.
4. **Physical base address** — the SEP eFuse address `0x1093_0000–0x1093_056F` is configured in the VP platform (`Args.hpp`); the model itself does not encode an absolute address.

---

## 4. Memory-Mapped Registers

The `sep_efuse` module exposes three contiguous sub-regions within one `0x570`-byte memory window.

### 4.1 Shadow Register Space (0x000–0x3FF)

#### 4.1.1 Lock Registers

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **LOCKS_LO** | 0x000 | 0x00000000 | RW WOSET | Write-lock and read-lock pairs for lower shadow registers. Bits[31:0]: each pair controls one shadow register. Bit[0] = LC_STATE write-lock, bit[1] = LC_STATE read-lock, … (see below). |
| **LOCKS_HI** | 0x004 | 0x00000000 | RW WOSET | Write-lock and read-lock pairs for upper shadow registers. |

**LOCKS_LO bit assignments:**

| Bits | Field | Bit | Field |
|------|-------|-----|-------|
| [0] | LC_STATE_WRITE_LOCK | [1] | LC_STATE_READ_LOCK |
| [2] | SBOOT_DIS_WRITE_LOCK | [3] | SBOOT_DIS_READ_LOCK |
| [4] | TRANSIENT_RMA_EN_WRITE_LOCK | [5] | TRANSIENT_RMA_EN_READ_LOCK |
| [6] | SIP_DIS_WRITE_LOCK | [7] | SIP_DIS_READ_LOCK |
| [8] | SYS_DIS_WRITE_LOCK | [9] | SYS_DIS_READ_LOCK |
| [10] | RMA_SIP_TOKEN_WRITE_LOCK | [11] | RMA_SIP_TOKEN_READ_LOCK |
| [12] | RMA_CHIPLET_TOKEN_WRITE_LOCK | [13] | RMA_CHIPLET_TOKEN_READ_LOCK |
| [14] | CLASS_KEY_WRITE_LOCK | [15] | CLASS_KEY_READ_LOCK |
| [16] | CHIPLET_PUBK_SEL_WRITE_LOCK | [17] | CHIPLET_PUBK_SEL_READ_LOCK |
| [18] | BL1_VERSION_WRITE_LOCK | [19] | BL1_VERSION_READ_LOCK |
| [20] | BL2_VERSION_WRITE_LOCK | [21] | BL2_VERSION_READ_LOCK |
| [22] | CHIPLET_UID_WRITE_LOCK | [23] | CHIPLET_UID_READ_LOCK |
| [24] | SIP_PUBK_WRITE_LOCK | [25] | SIP_PUBK_READ_LOCK |
| [26] | SIP_UID_WRITE_LOCK | [27] | SIP_UID_READ_LOCK |
| [28] | SYS_PUBK_WRITE_LOCK | [29] | SYS_PUBK_READ_LOCK |
| [30] | SYS_UID_WRITE_LOCK | [31] | SYS_UID_READ_LOCK |

> **Note:** `LOCKS_LO`/`LOCKS_HI` in the VP model implement WOSET semantics and accumulate bits, but do not enforce the corresponding register locks. The lock bits are present for firmware observability only; the model does not gate shadow register reads/writes based on lock state.

#### 4.1.2 Life-Cycle State

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **LC_STATE** | 0x008 | 0x000000F0 | RW WOSET | Differentially encoded lifecycle state: bits[7:0] hold `{~raw[3:0], raw[3:0]}`. Hardware reset = 0xF0 (unprovisioned). Overridden at elaboration by `load_fuses()` with the `lc_state` CCI param. Bit[0]=lc_state[0], …, Bit[3]=lc_state[3], Bit[4]=~lc_state[0], …, Bit[7]=~lc_state[3]. Bits[31:8]=reserved. |

#### 4.1.3 Security Control (Read-Only)

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **SBOOT_DIS** | 0x00C | 0x00000000 | RO | Secure boot disable. Bit[0]=disable_secure_boot. `write_mask=0x0`. |
| **TRANSIENT_RMA_EN** | 0x010 | 0x00000000 | RO | Transient RMA enable. Bit[0]=transient_rma_en. `write_mask=0x0`. |

#### 4.1.4 Feature Disable Vectors (WOSET)

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **SIP_DIS_LO** | 0x014 | 0x00000000 | RW WOSET | SIP feature/debug disable bits [31:0]. Bit[0]=sep_debug, [1]=soc_debug, [2]=ap_debug, [3]=ap_trace, [4]=sip_debug. Consumed by `lc_ctrl`. |
| **SIP_DIS_HI** | 0x018 | 0x00000000 | RW WOSET | SIP test/func disable bits [63:32]. Bit[0]=fuse_test, [1]=sep_stest, [2]=sep_dtest. |
| **SYS_DIS_LO** | 0x01C | 0x00000000 | RW WOSET | SYS feature/debug disable bits [31:0]. Same bit layout as SIP_DIS_LO. |
| **SYS_DIS_HI** | 0x020 | 0x00000000 | RW WOSET | SYS test/func disable bits [63:32]. Same bit layout as SIP_DIS_HI. |

#### 4.1.5 Token Material (Read-Only, 256-bit arrays)

| Register | Offset | Reset | Access | Array | Description |
|----------|--------|-------|--------|-------|-------------|
| **RMA_SIP_TOKEN[0:7]** | 0x024–0x043 | 0x0 each | RO | 8 × 32-bit | SHA-256 digest of the RMA SIP token. Used by hardware token comparator. |
| **RMA_CHIPLET_TOKEN[0:7]** | 0x044–0x063 | 0x0 each | RO | 8 × 32-bit | SHA-256 digest of the RMA chiplet token. |
| **CLASS_KEY[0:7]** | 0x064–0x083 | 0x0 each | RO | 8 × 32-bit | Class key material for key derivation. |

#### 4.1.6 Revocation and Version (WOSET)

| Register | Offset | Reset | Access | Array | Description |
|----------|--------|-------|--------|-------|-------------|
| **CHIPLET_PUBK_REVOKE** | 0x084 | 0x00000000 | RW WOSET | N/A | Public key revocation bitmap. Bit N=1 revokes public key slot N. |
| **BL1_VERSION[0:7]** | 0x088–0x0A7 | 0x0 each | RW WOSET | 8 × 32-bit | BL1 anti-rollback version. One-hot encoded: firmware sets the next bit to advance version. |
| **BL2_VERSION[0:7]** | 0x0A8–0x0C7 | 0x0 each | RW WOSET | 8 × 32-bit | BL2 anti-rollback version. One-hot encoded. |

#### 4.1.7 Device Identity (Read-Only, 256-bit arrays)

| Register | Offset | Reset | Access | Array | Description |
|----------|--------|-------|--------|-------|-------------|
| **CHIPLET_UID[0:7]** | 0x0C8–0x0E7 | 0x0 each | RO | 8 × 32-bit | Chiplet unique identifier. Also exposed via `get_chiplet_uid()` for `keymgr_tt`. |
| **SIP_PUBK[0:7]** | 0x0E8–0x107 | 0x0 each | RO | 8 × 32-bit | SIP public key digest. |
| **SIP_UID[0:7]** | 0x108–0x127 | 0x0 each | RO | 8 × 32-bit | SIP unique identifier. |
| **SYS_PUBK[0:7]** | 0x128–0x147 | 0x0 each | RO | 8 × 32-bit | System public key digest. |
| **SYS_UID[0:7]** | 0x148–0x167 | 0x0 each | RO | 8 × 32-bit | System unique identifier. |

#### 4.1.8 Platform Configuration (Read-Only)

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **STATUS_RPT** | 0x168 | 0x00000000 | RO | ECC/parity sense report. Bits[1:0]: 0=pass, 1=single-bit error, 2=multi-bit error. |
| **SEP_ROM_CTRL** | 0x16C | 0x00000000 | RO | SEP ROM endianness and byte-swap control. Bit[0]=rom_endianness_ctrl, bits[5:1]=rom_swap_ctrl. |
| **SEP_SPI_CTRL_FIELD_EN** | 0x170 | 0x00000000 | RO | SPI controller field enable. Bits[7:0]=spi_control_field_en, bits[18:8]=smu_pll_sysclk. |
| **SPI_DISCOVERY_CTRL** | 0x174 | 0x00000000 | RO | SPI discovery control (ro_stub). |
| **SPI_PHY_DQ_TIMING** | 0x178 | 0x00000000 | RO | SPI PHY DQ timing (ro_stub). |
| **SPI_PHY_DQS_TIMING** | 0x17C | 0x00000000 | RO | SPI PHY DQS timing (ro_stub). |
| **SPI_PHY_GATE_LPBK** | 0x180 | 0x00000000 | RO | SPI PHY gate loopback (ro_stub). |
| **SPI_PHY_DLL_SLAVE** | 0x184 | 0x00000000 | RO | SPI PHY DLL slave (ro_stub). |
| **SPI_PHY_DLL_MASTER** | 0x188 | 0x00000000 | RO | SPI PHY DLL master (ro_stub). |
| **SPI_PHY_MISC** | 0x18C | 0x00000000 | RO | SPI PHY miscellaneous (ro_stub). |
| **SPI_RB_VALID_TIME** | 0x190 | 0x00000000 | RO | SPI read-back valid time (ro_stub). |

#### 4.1.9 Public Key Hashes and Reserved (Read-Only)

| Register | Offset | Reset | Access | Array | Description |
|----------|--------|-------|--------|-------|-------------|
| **PUBLIC_KEY_0[0:7]** | 0x194–0x1B3 | 0x0 each | RO | 8 × 32-bit | Public key hash 0 (ro_stub). |
| **PUBLIC_KEY_1[0:7]** | 0x1B4–0x1D3 | 0x0 each | RO | 8 × 32-bit | Public key hash 1 (ro_stub). |
| **RESERVED_0–7[0:15]** | 0x1D4–0x3D3 | 0x0 each | RO | 8 banks × 16 × 32-bit | Eight reserved 64-byte regions (ro_stubs, always 0). |
| **RESERVED_LAST_256[0:7]** | 0x3D4–0x3F3 | 0x0 each | RO | 8 × 32-bit | Reserved (ro_stub). |
| **RESERVED_LAST_64_LO/HI** | 0x3F4, 0x3F8 | 0x0 | RO | — | Reserved (ro_stubs). |
| **RESERVED_LAST_32** | 0x3FC | 0x0 | RO | — | Reserved (ro_stub). |

### 4.2 eFuse Interface Control Registers (0x400–0x41B)

These registers provide a raw OTP access interface. In the VP, they are RW stubs — no actual fuse programming or readback occurs.

| Register | Offset | Reset | Access | Description |
|----------|--------|-------|--------|-------------|
| **EFUSE_INTERFACE_CTRL_STATUS** | 0x400 | 0x00000001 | RO | Bit[0]=efuse_sense_done (permanently 1 in VP). `write_mask=0x0`. |
| **EFUSE_PROGRAM_CTRL** | 0x404 | 0x00000000 | RW | OTP programming control. Bits[15:0]=efuse_addr, [16]=efuse_data, [17]=efuse_program_go (singlepulse), [27]=program_enable. |
| **EFUSE_READ_CTRL** | 0x408 | 0x00000000 | RW | OTP readback control. Bits[15:0]=efuse_addr, [16]=efuse_read_go (singlepulse), [28]=read_enable. |
| **EFUSE_PROGRAM_INTERFACE_RD_DATA** | 0x40C | 0x00000000 | RO (hw=w) | Program interface read data. Always 0x0 in VP. |
| **EFUSE_READ_INTERFACE_RD_DATA** | 0x410 | 0x00000000 | RO (hw=w) | Read interface data. Always 0x0 in VP. |
| **EFUSE_READ_REQ_TIMEOUT** | 0x414 | 0x00800000 | RW | Read request timeout config. Bits[27:0]=timeout_cycles (default 0x800000), [28]=timeout_enable. |
| **EFUSE_PROGRAM_REQ_TIMEOUT** | 0x418 | 0x00800000 | RW | Program request timeout config. Same layout as EFUSE_READ_REQ_TIMEOUT. |

### 4.3 eFuse MMR Registers (0x500–0x56C)

These registers handle token submission and match-result readback. In the VP, SHA-256 is not computed; `TOKEN_MATCH` registers are preset via CCI params for VP test scenarios.

| Register | Offset | Reset | Access | Array | Description |
|----------|--------|-------|--------|-------|-------------|
| **RMA_SIP_TOKEN_I[0:7]** | 0x500–0x51F | 0x0 | RW | 8 × 32-bit | RMA SIP token input (firmware writes 256-bit token candidate before asserting TOKEN_EOP.rma_sip_token_go). |
| **RMA_CHIPLET_TOKEN_I[0:7]** | 0x520–0x53F | 0x0 | RW | 8 × 32-bit | RMA chiplet token input. |
| **SEC_DISABLE_TOKEN_I[0:7]** | 0x540–0x55F | 0x0 | RW | 8 × 32-bit | Secure-disable token input. |
| **TOKEN_EOP** | 0x560 | 0x00000000 | WO | N/A | Token hash trigger. Bit[0]=rma_sip_token_go, Bit[8]=rma_chiplet_token_go, Bit[16]=secure_disable_token_go. Singlepulse; reads return 0. In VP, write has no effect (no SHA-256). |
| **RMA_SIP_TOKEN_MATCH** | 0x564 | 0x00000000 | RW (VP) / RO (RTL) | N/A | RMA SIP token match result. Bits[5:0]: 0x15=match, 0x2A=mismatch, 0x3F=error. Preset via `rma_sip_token_match` CCI param. |
| **RMA_CHIPLET_TOKEN_MATCH** | 0x568 | 0x00000000 | RW (VP) / RO (RTL) | N/A | RMA chiplet token match result. Same encoding as RMA_SIP_TOKEN_MATCH. |
| **SEC_DISABLE_TOKEN_MATCH** | 0x56C | 0x00000000 | RW (VP) / RO (RTL) | N/A | Secure-disable token match result. Same encoding. |

---

## 5. Register Callbacks

All WOSET register callbacks are registered in `sep_efuse_model::register_callbacks()` called from the constructor.

### 5.1 WOSET Callbacks

Each WOSET register has a dedicated write callback that OR's the written value into a private shadow:

| Callback | Shadow Variable | Register(s) |
|----------|-----------------|-------------|
| `handle_write_LOCKS_LO` | `m_locks_lo_val` | `LOCKS_LO` |
| `handle_write_LOCKS_HI` | `m_locks_hi_val` | `LOCKS_HI` |
| `handle_write_LC_STATE` | `m_lc_state_val` | `LC_STATE` |
| `handle_write_SIP_DIS_LO` | `m_sip_dis_lo_val` | `SIP_DIS_LO` |
| `handle_write_SIP_DIS_HI` | `m_sip_dis_hi_val` | `SIP_DIS_HI` |
| `handle_write_SYS_DIS_LO` | `m_sys_dis_lo_val` | `SYS_DIS_LO` |
| `handle_write_SYS_DIS_HI` | `m_sys_dis_hi_val` | `SYS_DIS_HI` |
| `handle_write_CHIPLET_PUBK_REVOKE` | `m_chiplet_pubk_revoke_val` | `CHIPLET_PUBK_REVOKE` |
| `handle_write_BL1_VERSION(idx, val)` | `m_bl1_version_val[idx]` | `BL1_VERSION[0:7]` |
| `handle_write_BL2_VERSION(idx, val)` | `m_bl2_version_val[idx]` | `BL2_VERSION[0:7]` |

Each callback body follows the same pattern:

```cpp
bool handle_write_X(uint32_t value) {
    m_x_val |= value;        // accumulate set bits
    X = m_x_val;             // write back accumulated value
    return true;
}
```

Array callbacks (`BL1_VERSION`, `BL2_VERSION`) receive the word index `idx` in addition to the value.

### 5.2 Callback Registration for Array Registers

For the 8-word array WOSET registers, a loop registers one callback per word:

```cpp
for (int i = 0; i < 8; i++) {
    memory.register_write_callback(
        [this, i](uint32_t v){ return handle_write_BL1_VERSION(i, v); },
        BL1_VERSION[i].offset);
}
```

---

## 6. Internal Architecture

### 6.1 Class Hierarchy

```
sep_efuse_model (sc_module, extends sep_efuse_base)
├── sep_efuse_base      (CSML register instances + target_socket)
│   ├── Shadow regs     @ 0x000–0x3FF   (RO + WOSET)
│   ├── Interface ctrl  @ 0x400–0x41B   (RW stubs)
│   └── MMR             @ 0x500–0x56C   (token input + match result)
├── CsmlLogger          logger
├── csml_param<int>     verbosity
├── csml_param<uint32_t> [22 scalar params]
├── csml_param<std::vector<uint32_t>> [12 array params]
└── (private)
    ├── uint32_t m_chiplet_uid_cache[8]   — stable pointer for get_chiplet_uid()
    ├── uint32_t m_locks_lo_val, m_locks_hi_val
    ├── uint32_t m_lc_state_val
    ├── uint32_t m_sip_dis_lo_val, m_sip_dis_hi_val
    ├── uint32_t m_sys_dis_lo_val, m_sys_dis_hi_val
    ├── uint32_t m_chiplet_pubk_revoke_val
    ├── uint32_t m_bl1_version_val[8]
    └── uint32_t m_bl2_version_val[8]
```

### 6.2 Initialization Flow

```
sep_efuse_model constructor:
  → Initialize all 34 csml_params with defaults (0 for scalars, k_zero8 for arrays)
  → reset_all_registers()  — set all CSML registers to their hardware reset values
  → register_callbacks()   — register WOSET write callbacks for 10 register groups

end_of_elaboration():
  → load_fuses()
      → For each param: call get_param_value() and store in shadow variable
      → Assign shadow variable to corresponding CSML register (overrides reset value)
      → Populate m_chiplet_uid_cache[] from chiplet_uid vector param
```

### 6.3 `load_fuses()` Flow (abridged)

```
m_lc_state_val = lc_state.get_param_value()
LC_STATE = m_lc_state_val

SBOOT_DIS = sboot_dis.get_param_value()
TRANSIENT_RMA_EN = transient_rma_en.get_param_value()

m_sip_dis_lo_val = sip_dis_lo.get_param_value()
SIP_DIS_LO = m_sip_dis_lo_val
... (similar for all dis, token, key, uid fields)

auto chiplet_uid_v = chiplet_uid.get_param_value()
for i in 0..7:
    m_chiplet_uid_cache[i] = (i < chiplet_uid_v.size()) ? chiplet_uid_v[i] : 0
    CHIPLET_UID[i] = m_chiplet_uid_cache[i]
```

---

## 7. Modeling Assumptions

### 7.1 Fuse Loading

1. **`end_of_elaboration()` timing** — CCI params are set by `load_config_file()` before `sc_start()`, so all param values are stable at `end_of_elaboration()`. `load_fuses()` reads them once at this point.

2. **Hardware reset value of `LC_STATE` is 0xF0 (unprovisioned)** — this is the reset value in `lc_ctrl_register.h`. `load_fuses()` overrides it with the `lc_state` CCI param. After elaboration, `LC_STATE` holds the ini-configured value, not 0xF0.

3. **WOSET shadow variables initialised before `load_fuses()`** — the constructor initialises all `m_*_val` members to 0. `load_fuses()` then assigns the param value to them. Subsequent firmware writes accumulate on top of the fuse-loaded value.

### 7.2 WOSET Semantics

4. **WOSET is firmware-level, not hardware-level** — the RTL has dedicated fuse-write sequencers with OTP programming voltage. In the VP, WOSET is purely a software constraint modeled by the callback. Firmware can "program" additional bits but cannot physically blow fuses via this model.

5. **Lock bits are informational** — `LOCKS_LO`/`LOCKS_HI` accumulate as WOSET, but the VP model does not enforce lock semantics by gating other register writes. The bits are present for firmware observability (to read back what was locked).

6. **Array WOSET word independence** — `BL1_VERSION[i]` and `BL2_VERSION[i]` are independently WOSET per word index. Firmware advancing the anti-rollback counter sets words sequentially.

### 7.3 Token Matching

7. **No SHA-256** — token comparison results are preset via CCI params before simulation. Firmware that reads `RMA_SIP_TOKEN_MATCH` will see the preset value, not a hardware-computed result.

8. **`TOKEN_EOP` go-bits are no-ops** — writing a go-bit generates no side-effect in the VP; the match result is already in the register from the ini-file preset.

### 7.4 OTP Interface Control Stubs

9. **`EFUSE_PROGRAM_CTRL` and `EFUSE_READ_CTRL` are RW stubs** — firmware can write/read them freely (needed for driver compatibility), but no OTP hardware action occurs.

10. **`EFUSE_PROGRAM/READ_INTERFACE_RD_DATA` always return 0x0** — raw bit-level readback is not modeled.

---

## 8. Conclusion

### 8.1 Summary

The `sep_efuse` SystemC TLM model provides a complete MMIO-accessible fuse shadow register block. Key aspects:

1. **Three-region memory map** — 0x570-byte window covering shadow registers (0x000), OTP interface control (0x400), and token MMR (0x500).
2. **34 CCI parameters** — cover all 22 scalar and 12 array (256-bit) fuse fields; no recompile needed for different fuse scenarios.
3. **WOSET semantics** — 10 write callbacks implement accumulating-OR behavior for all programmable fields.
4. **OTP data accessors** — `get_lc_state()` and `get_chiplet_uid()` provide data to `keymgr_tt` at VP integration time.
5. **efuse_sense_done always 1** — VP delivers fuse data immediately at elaboration; no analog sense timing.

### 8.2 Known Gaps

| Gap | Risk | Mitigation |
|-----|------|------------|
| No SHA-256 token comparison | Low — preset via CCI param | `TOKEN_MATCH` preset via `rma_sip_token_match` etc. params |
| Lock bits not enforced | Low — informational only | WOSET lock bits written by firmware; no gating of other registers |
| OTP raw interface returns zeros | Low — VP use case | Firmware using raw OTP access (non-shadow path) will not get correct data |
| `LC_STATE` differential encoding not applied | Low | `lc_state` param is raw 4-bit value; the 8-bit differential encoding in the reset value (0xF0) is overridden by load_fuses() |

---

**End of High-Level Design Specification**
