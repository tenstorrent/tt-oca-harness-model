# Zephyr on the SMC virtual platform

Out-of-tree Zephyr port for `smc-vp`.  The kernel runs on hart 0 in M-mode
(same ELF load path as `sw/smc-vp-tests/`).  UART0 is the console.

`device list` only shows peripherals that have a DTS node **and** a Zephyr
driver (today: PLIC + UART0).  Everything else on the VP is still there —
talk to it with MMIO, or add a driver later.

## One-time setup

Needs Python ≥ 3.10, `west`, a RISC-V GCC (`riscv64-elf-` or
`riscv64-unknown-elf-`), and a built `smc-vp`.

```bash
cd sw/zephyr-smc
./zephyr_smc.sh setup          # clone Zephyr v4.3.0 + west update (slow once)
```

## Build and run

```bash
./zephyr_smc.sh build hello    # samples/hello_world
./zephyr_smc.sh run hello      # live UART0 for 500 ms

./zephyr_smc.sh build shell    # samples/subsys/shell/shell_module (polling UART)
./zephyr_smc.sh run shell      # type at uart:~$  (Ctrl-C to stop)

./zephyr_smc.sh build poke     # apps/mmio_poke — unlisted-IP MMIO reachability
./zephyr_smc.sh run poke
./zephyr_smc.sh test poke      # same, and require a RESULT: PASS line
./zephyr_smc.sh ci             # setup + test hello + test poke (GitHub Actions)
```

CI (`smc-vp` / `smc-vp-rhel8`) runs `./zephyr_smc.sh ci` after the bare-metal
suite.  The west workspace is cached; `hello` must print `Hello World` and
`poke` must print `RESULT: PASS`.

Equivalent raw command (after a build):

```bash
vp/build_smc/bin/smc-vp \
    sw/zephyr-smc/config/smc_zephyr.ini \
    sw/zephyr-smc/build/hello_world/zephyr/zephyr.elf \
    200 --uart-live
```

Use `config/smc_zephyr.ini`, not `smc_platform_vp.ini`.  The default VP ini
freezes CLINT `mtime`; Zephyr then never gets a tick.

## Poke unlisted IPs (`apps/mmio_poke`)

`device list` will not name UART1–3, I2C, I3C, DMA, WDT, AVSbus, scratchpad,
bootrom, reset, or cpu_ctrl.  `mmio_poke` reaches them the same way the
bare-metal suite does: `REG_READ` / `REG_WRITE` at the bases in
`sw/smc-vp-tests/common/smc_common.h`.

It is a **reachability** test (identity / reset / write-read), not a
replacement for `smc-dma-test`, `smc-i2c-loopback-test`, and friends.  It
does not unlock the WDT or reprogram UART0.

From the Zephyr shell you can do the same by hand:

```text
uart:~$ devmem 0xc000b000          # UART1
uart:~$ devmem 0xc0038000          # DMA CONFIG
uart:~$ devmem 0xc0005000          # I2C0
```

## Can platform-level tests run on Zephyr?

Yes, incrementally.  Do **not** replace `sw/smc-vp-tests/`.  That suite is
the DV contract (no OS, `start.S`, custom `.ini` files, busy-wait).  Zephyr
is the path for SMC **management firmware**.

| Already true | What you add per test |
|--------------|------------------------|
| Same `smc-vp <ini> <elf>` load | A Zephyr app under `apps/` (or a `tests/` sibling) |
| Same address map (`smc_common.h`) | Port the `main.c` body; keep `REG_READ`/`REG_WRITE` |
| Same fabric / models | `printk` instead of bare-metal `printf` |
| Live UART + `RESULT: PASS` grep | `./zephyr_smc.sh test <name>` |

Constraints when porting a bare-metal test:

1. **Polling MMIO first.**  IRQ-driven tests (PLIC claim, UART ETBEI) need
   the PLIC path fixed; the board forces a polling serial console for that
   reason.
2. **Busy-waits still work** (each MMIO advances TLM time).  Prefer
   `k_busy_wait()` / `k_sleep()` when you want Zephyr time.
3. **Custom CCI ini** (BEU inject, memory_zeroer, telemetry, OCTS secondary)
   — pass that ini to `smc-vp` instead of `config/smc_zephyr.ini`, but keep
   `dut.clint.tick_period_ns` non-zero so the kernel ticks.
4. **Do not enable a WDT reset** from a Zephyr thread unless the test owns
   the reset story.
5. **Optional later hook:** `run_smc_vp_tests.sh` could grow a `--zephyr`
   mode that calls `./zephyr_smc.sh test …`.  Keep the suites listed
   separately until several ports exist.

Suggested order if you extend this: scratchpad / UART1 SCR → I3C version →
DMA copy (`smc-dma-test` body) → I2C/I3C loopback → tests that need a
custom ini.
