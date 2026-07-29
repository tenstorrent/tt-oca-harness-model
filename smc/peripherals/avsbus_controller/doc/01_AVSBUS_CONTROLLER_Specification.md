# AVSBus Controller — Specification

## 1. Purpose

The AVSBus Controller is the SMC's Adaptive Voltage Scaling Bus master. It
implements AVSBus **1.3.1** for a **single** target voltage regulator, exposing
a 32-bit AXI4-Lite register window and one aggregated interrupt.

SMC placement: base `0xC000_8000`, 4 KiB window, peripheral interrupt bit 22
(PLIC source 278).

## 2. Software-visible behaviour

### 2.1 Register map

| Offset | Name | Access | Reset | Description |
|--------|------|--------|-------|-------------|
| 0x00 | AVS_CMD | W | — | Push command into command FIFO |
| 0x04 | AVS_READBACK | R | 0 | Pop response from readback FIFO |
| 0x08 | AVS_DEBUG_READBACK | R | 0xFFFFFFFF / 0xDEADBEEF empty | Peek TOF (no pop) |
| 0x0C | AVS_LATEST_SLAVE_SUBFRAME | R | 0x0000FFFF | Latest slave frame (bypass FIFO) |
| 0x20 | AVS_NORMAL_STATUS | R | live | Idle / FIFO / retry status |
| 0x24 | AVS_SLAVE_STATUS | R | 0 | Latest ACK + status response |
| 0x28 | AVS_FIFOS_STATUS | R | 0x08000800 | Occupancy / vacancy |
| 0x30 | AVS_INTERRUPT | R | 0 | Sticky interrupt status |
| 0x34 | AVS_INTERRUPT_MASK | RW | 0x1FF | `1` = disable source |
| 0x38 | AVS_INTERRUPT_CLEAR | W | 0 | Write-1-to-clear |
| 0x50 | AVS_CFG_0 | RW | 0x00051000 | Max retries + resync interval |
| 0x54 | AVS_CFG_1 | RW | 0x00000003 | Clock select / divider / force resync |
| 0x58 | AVS_CONFIG | RW | 0x1 | `AVS_GPIO_ENABLE` |

### 2.2 Command / response

Writing `AVS_CMD` enqueues `{R_OR_W, CMD_GRP, CMD_CODE, RAIL_SEL, CMD_DATA}`.
When the bus is free and the readback FIFO has space, the controller launches
the head command. The slave response is enqueued into the readback FIFO and
mirrored in `AVS_LATEST_SLAVE_SUBFRAME`.

SlaveAck `0x1` / `0x2` trigger hardware retries up to `AVS_CFG_0.MAX_RETRIES`.
Ack `0x3` (bad data) does not retry.

### 2.3 Interrupts

Nine sticky sources (bits 0–8). Aggregate `irq = OR(status & ~mask)`. Clear
only via `AVS_INTERRUPT_CLEAR`.

## 3. Out of scope (this model)

- Bit-serial AVS wire timing / CRC on the wire path (CRC-3 helper is provided)
- Real clock mux / divider duty-cycle generation
- Multi-target AVSBus 2.x (that is a different IP / RDL)
