# SMC WDT Specification (SystemC)

## Purpose

Model one per-core SiFive TLWDT (Chipyard `WatchdogTimer`) for firmware
bring-up. Matches programmer-visible behaviour of `wdt.rdl`.

## Address map

| Absolute base | Size | Instance |
|---------------|------|----------|
| `0xC000_0000 + N×0x400` | 1 KiB | Core N (0..3) |

Fabric routes `0xC000_0000..0xC000_0FFF` via `to_front_port`.

## Registers

| Offset | Name | Reset | Notes |
|--------|------|-------|-------|
| `0x00` | CTRL | 0 | scale, rsten, zerocmp, always, awake, ip |
| `0x08` | COUNT | 0 | 31-bit |
| `0x10` | SCALED_COUNT | 0 | RO `count>>scale`; write locks |
| `0x18` | FEED | 0 | `0xD09F00D` pets |
| `0x1C` | KEY | 0 | `0x51F15E` unlocks once |
| `0x20` | CMP | `0x1000` | vs scaled count |

## Sidebands

- `irq_o` — pending interrupt (`wdogip0`)
- `rst_sticky_o` — sticky when `wdogrsten` and compare elapsed; cleared by feed or module reset
- Stage-2 escalation is **not** in this module (see `cpu_ctrl` / cluster)
