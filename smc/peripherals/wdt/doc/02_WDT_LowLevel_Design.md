# SMC WDT Low-Level Design (SystemC)

## Module

`smc::wdt` — `sc_module` with CCI (`tick_period_ns`, `access_delay_ns`),
`reg_socket`, `rst_n_i`, `core_rst_i`, `irq_o`, `rst_sticky_o`.

## Processes

| Process | Sensitivity | Role |
|---------|-------------|------|
| `reset_proc` | `rst_n_i` | Clear state, re-arm tick |
| `tick_method` | `tick_event_` | Increment count when enabled |
| `output_method` | `recompute_event_` | Sole driver of `irq_o` / `rst_sticky_o` |

## Count enable

```
enabled = wdogenalways || (wdogcoreawake && !core_rst_i)
```

## Elapsed

```
elapsed = (count >> scale)[15:0] >= CMP
```

On elapsed: set IP; if `wdogrsten` latch sticky; if `wdogzerocmp` clear count.

## KEY / FEED

- KEY `0x51F15E` unlocks for exactly one subsequent config write (or FEED).
- FEED `0xD09F00D` clears count and sticky; re-locks.

## Integration

```
fabric.to_front_port → decode → wdt[N].reg_socket
wdt[N].rst_sticky_o → cluster.wdt_timeout_cluster_i[N]
wdt[N].irq_o        → plic.src_in[…]
cluster.wdt_second_timeout_o → reset_unit.smc_wdt_second_timeout_i
```
