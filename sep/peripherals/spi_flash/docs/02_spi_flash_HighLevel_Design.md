# SPI Flash Model High-Level Design Document

## TABLE OF CONTENTS

1. [Introduction](#1-introduction)
   - [1.1 Objective](#11-objective)
   - [1.2 Scope](#12-scope)
   - [1.3 Is list](#13-is-list)
   - [1.4 Is not list](#14-is-not-list)

2. [Functional Description](#2-functional-description)
   - [2.1 Two-layer structure](#21-two-layer-structure)
   - [2.2 Configuration parameters](#22-configuration-parameters)
   - [2.3 Port interfaces](#23-port-interfaces)
   - [2.4 Command set](#24-command-set)
   - [2.5 Device state](#25-device-state)
   - [2.6 SFDP ROM](#26-sfdp-rom)

3. [Use model](#3-use-model)
   - [3.1 Segment framing](#31-segment-framing)
   - [3.2 Backdoor image loading](#32-backdoor-image-loading)

4. [Assumptions](#4-assumptions)

## 1. Introduction

### 1.1 Objective

This document describes the SystemC model of the SPI NOR flash device attached to the SEP's
OpenTitan SPI host. The model exists so that firmware exercising the boot path — SFDP
discovery, manifest read, payload read — runs against a device that answers the way a real
NOR part does, rather than against a stub that returns whatever the test happens to want.

The reference for behaviour is the DV flash BFM in the harness,
`ocah_spi_flash.py`, together with the JESD216A SFDP standard (`docs/JESD216A.pdf`).

### 1.2 Scope

The model has no memory-mapped bus address of its own. It is reached only through the
`spi_if` interface that `spi_controller` drives, so everything here is expressed in terms of
SPI commands and segments rather than register accesses.

### 1.3 Is list

- Profile 1 SPI NOR command set: read, program, erase, control, suspend/resume
- Flash program semantics: program clears bits only (1→0), erase restores `0xFF`
- Page-bounded programming, wrapping within a 256-byte page
- 64 KB erase blocks, plus chip erase
- Write Enable Latch (WEL) enforcement on program and erase
- Status Register 1 and Status Register 2
- 3-byte and 4-byte addressing modes (EN4B / EX4B)
- JEDEC device identification (RDID) with a configurable ID
- SFDP ROM, JESD216A compliant, with a JEDEC Basic Flash Parameter Table
- Software reset sequence (arm with `0x66`, execute with `0x99`)
- Multi-segment CSAAT-chained reads with a running address
- Explicit backdoor load and save of the memory array

### 1.4 Is not list

- Timing of any kind. This is an LT model: every operation completes instantly from the
  caller's perspective. There is no program or erase duration, and consequently **no WIP
  (Work In Progress) busy bit** — a status poll never observes a busy device. Firmware that
  polls for completion sees it satisfied immediately
- Dual and quad I/O as distinct electrical modes. Their opcodes are accepted and return the
  correct data, but the model does not distinguish lane counts, and fast-read dummy cycles
  are not consumed. Both the model and the DV BFM abstract this the same way, so the two
  agree — but neither verifies it
- Block protection bits, OTP security registers, and lock/unlock commands
- Power-down and deep power-down states
- Bad-block management and wear levelling, neither of which NOR has

## 2. Functional Description

### 2.1 Two-layer structure

The model is split so that flash logic can be tested without a simulator:

| Class | Dependency | Role |
|---|---|---|
| `spi_flash_model` | Pure C++ | The device: memory array, command handling, SFDP, state |
| `spi_flash` | `sc_module`, implements `spi_if` | Segment framing, reset handling, delegation |

`spi_flash_model::process_command()` is the whole device interface — an opcode, an address, a
receive buffer and a transmit buffer. Everything above it is about turning the controller's
segment stream into those four arguments.

### 2.2 Configuration parameters

| Constant | Default | Meaning |
|---|---|---|
| `DEFAULT_FLASH_SIZE` | 16 MB | Array size, settable through the constructor |
| `ERASE_BLOCK_SIZE` | 64 KB | Erase granularity |
| `PAGE_SIZE` | 256 B | Program page; a program wraps within it |
| `DEFAULT_JEDEC_ID` | `0x20BA18` | Reported by RDID; matches the DV BFM |

These four are deliberately consistent with each other. The JEDEC capacity byte `0x18`
encodes 2²⁴ bytes, which is `DEFAULT_FLASH_SIZE`, and the SFDP table's density and addressing
mode agree with both. A part whose ID claims one size while its array is another is a
realistic way to break a bootloader, so the model does not permit the discrepancy by default.

`set_jedec_id()` overrides the ID at runtime, mirroring the BFM's `+spi_flash_jedec_id`
plusarg. There is no `.ini` key for it yet.

### 2.3 Port interfaces

| Port / Export | Type | Description |
|---|---|---|
| `spi_target` | `sc_export<spi_if>` | SPI transaction target, bound by `spi_controller` |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

Reset clears the write-enable latch, the suspend flag and the status registers. It does
**not** clear the memory array — a reset is not an erase, and a staged image survives it.

### 2.4 Command set

| Group | Opcodes |
|---|---|
| Read | `0x03` READ, `0x0B` FAST_READ, `0x13` READ_4B, `0xEE`, `0x5A` READ_SFDP |
| Program | `0x02` PAGE_PROGRAM, `0x12` PAGE_PROGRAM_4B |
| Erase | `0x20` (4 KB), `0x52` (32 KB), `0xD8` (64 KB), `0xDC` (64 KB 4B), `0x60` chip erase |
| Control | `0x04` WRDI, `0x05` RDSR1, `0x06` WREN, `0x35` RDSR2, `0x9F` RDID, `0xB7` EN4B, `0xE9` EX4B, `0x66` reset-enable, `0x99` reset-execute |
| Suspend / Resume | `0x75`, `0xB0` suspend; `0x30`, `0x7A`, `0xD0` resume |

An unknown opcode returns failure rather than being silently ignored.

Program and erase both require WEL to be set by a prior `WREN`; without it the command fails
and the array is untouched. WEL is cleared automatically once the operation completes, as on
a real device.

`RDSR2` (`0x35`) is implemented alongside `RDSR1`, because a driver that probes quad-enable
state reads it during initialisation.

### 2.5 Device state

- **WEL** — write enable latch, mirrored into SR1 bit 1
- **Suspend flag** — set by suspend, cleared by resume
- **SR1 / SR2** — status registers
- **4-byte address mode** — entered with `EN4B`, left with `EX4B`
- **Reset armed** — `0x99` is ignored unless `0x66` armed it first

### 2.6 SFDP ROM

The SFDP ROM is built from three structures — an SFDP header, a parameter header and a JEDEC
Basic Flash Parameter Table — which are individually accessible for inspection and for tests
that want to present a different part. `update_sfdp_rom()` rebuilds the flat ROM after any
such change; without that call, `READ_SFDP` still returns the old image.

## 3. Use model

### 3.1 Segment framing

The controller does not hand the flash a whole command. It drives a sequence of segments, and
the wrapper reassembles them:

- An opcode+address TX segment opens the command.
- One or more RX segments follow. While CS stays asserted (`csaat=1`), each RX segment is
  served from a **running address** that advances by the length of the preceding segment.
- A bare RX segment with no preceding opcode+address returns `0xFF`, which is what an idle
  MISO line reads as.

The running address is the part worth attention. Serving data only on the final `csaat=0`
segment truncates every multi-segment read, and because a short read often still contains the
bytes a test checks, the failure can look like data corruption rather than a framing bug.

### 3.2 Backdoor image loading

`load_memory_from_file()` and `save_memory_to_file()` both take an explicit path. The model
never searches the working directory, never has a default image, and never opens a file
unless it was told to. A run that configures no image starts from erased memory.

On a platform, two config keys name an image, both resolved relative to the directory of the
`.ini` that names them:

| Key | Format |
|---|---|
| `och_sep_ss1.spiPreload` | Verilog `$readmemh`-style text |
| `och_sep_ss1.spiBackdoorFile` | Raw binary |

`spiPreload` takes precedence if both are set. CCI requires string values to be JSON-quoted,
so the value needs surrounding quotes in the `.ini` or the file is rejected at parse time.

`BACKDOOR_FILE_PATH` (`data/flash_memory.bin`) remains as a constant, but only as a shared
naming convention between testbenches. Nothing opens it implicitly. It previously *was* an
implicit fallback, which meant a stray file in the run directory could silently change what a
test read; removing that is what makes a blank-flash assertion trustworthy.

## 4. Assumptions

1. **Instantaneous operations.** Program, erase and read all complete within the call. This is
   the single largest abstraction and the source of the missing WIP bit.
2. **No electrical layer.** Lane counts, dummy cycles and clock rates do not affect returned
   data.
3. **Memory survives reset** but not reconstruction; a new `spi_flash_model` starts erased.
4. **The array is flat and fully addressable.** Out-of-bounds reads return `0xFF` rather than
   wrapping or faulting.
5. **One command at a time.** The model has no concept of overlapping or pipelined commands,
   which matches how the controller drives it.

---

**Document Metadata:**
- IP Name: SPI NOR Flash (device model)
- Reference BFM: `ocah_spi_flash.py` in `tt-oca-harness-main`
- TLM Model Classes: `spi_flash_model` (pure C++), `spi_flash` (`sc_module`)
- Abstraction Level: Loosely Timed (LT), untimed device behaviour
- Standard: JESD216A (`docs/JESD216A.pdf`)
- RTL comparison and change history: `md_files/SPI_FLASH_RTL_VS_VP.md`
