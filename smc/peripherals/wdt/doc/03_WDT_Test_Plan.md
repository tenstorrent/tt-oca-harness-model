# SMC WDT Test Plan

| # | Case | Expected |
|---|------|----------|
| 1 | Reset defaults | CTRL/COUNT=0, CMP=`0x1000`, KEY locked, outputs low |
| 2 | KEY unlock/lock | Magic unlocks; next write re-locks |
| 3 | Locked writes | CMP unchanged when locked |
| 4 | Compare / IRQ | Always-on + tick to CMP → `irq_o` |
| 5 | IP clear | Unlocked CTRL write clears IP when not elapsed |
| 6 | Sticky rst | `wdogrsten` + elapsed → `rst_sticky_o` |
| 7 | Feed | Magic feed clears count + sticky |
| 8 | Scale | `count>>scale` compared to CMP |
| 9 | zerocmp | Count clears on elapsed |
| 10 | awake vs always | Awake gated by `core_rst_i`; always overrides |
| 11 | SCALED_COUNT write | Locks without changing count |
| 12 | Negative TLM | OOW address / bad size → error |
| 13 | Module reset | Clears sticky, IP, count, CMP restore |
