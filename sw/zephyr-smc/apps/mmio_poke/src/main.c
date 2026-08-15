/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Reachability poke of SMC peripherals that have no Zephyr driver (so they
 * do not appear in `device list`).  Uses the same MMIO map as
 * sw/smc-vp-tests/common/smc_common.h.  Safe: no WDT unlock, no UART0
 * reprogramming, and no persistent interrupt enables.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "smc_common.h"

static int g_fail;

static void expect_eq(const char *name, uint32_t got, uint32_t want)
{
	if (got != want) {
		printk("  FAIL %s: got 0x%x want 0x%x\n", name, got, want);
		g_fail = 1;
	} else {
		printk("  PASS %s = 0x%x\n", name, got);
	}
}

static void poke_scratch(void)
{
	const uintptr_t a = (uintptr_t)(SMC_SCRATCH_BASE + 0x200ULL);

	printk("scratchpad @ 0x%x\n", (uint32_t)SMC_SCRATCH_BASE);
	REG_WRITE(a, 0xA5A5A5A5u);
	expect_eq("scratch[0x200]", REG_READ(a), 0xA5A5A5A5u);
}

static void poke_uart1(void)
{
	printk("uart1 @ 0x%x (not in device list)\n", (uint32_t)SMC_UART1_BASE);
	uint32_t lsr = REG_READ(SMC_UART1_BASE + UART_LSR);
	if ((lsr & UART_LSR_THRE) == 0u) {
		printk("  FAIL uart1 LSR.THRE clear (LSR=0x%x)\n", lsr);
		g_fail = 1;
	} else {
		printk("  PASS uart1 LSR.THRE (LSR=0x%x)\n", lsr);
	}
	REG_WRITE(SMC_UART1_BASE + UART_SCR, 0x5Au);
	expect_eq("uart1 SCR", REG_READ(SMC_UART1_BASE + UART_SCR) & 0xFFu, 0x5Au);
}

static void poke_i3c(void)
{
	printk("i3c0 @ 0x%x\n", (uint32_t)SMC_I3C_BASE);
	expect_eq("i3c HCI_VERSION", REG_READ(SMC_I3C_BASE + I3C_HCI_VERSION), 0x00000120u);
}

static void poke_i2c(void)
{
	printk("i2c0 @ 0x%x\n", (uint32_t)SMC_I2C0_BASE);
	REG_WRITE(SMC_I2C0_BASE + I2C_INTR_ENABLE, 0x3u);
	expect_eq("i2c0 INTR_ENABLE", REG_READ(SMC_I2C0_BASE + I2C_INTR_ENABLE), 0x3u);
	REG_WRITE(SMC_I2C0_BASE + I2C_INTR_ENABLE, 0u);
}

static void poke_dma(void)
{
	printk("dma @ 0x%x\n", (uint32_t)SMC_DMA_BASE);
	REG_WRITE(SMC_DMA_BASE + DMA_CONFIG, 0xA5A5A5A5u);
	expect_eq("dma CONFIG", REG_READ(SMC_DMA_BASE + DMA_CONFIG), 0xA5A5A5A5u);
	REG_WRITE(SMC_DMA_BASE + DMA_CONFIG, 0u);
}

static void poke_avs(void)
{
	printk("avsbus @ 0x%x\n", (uint32_t)SMC_AVSBUS_BASE);
	expect_eq("avs CFG_0 reset", REG_READ(SMC_AVSBUS_BASE + AVS_CFG_0), AVS_CFG_0_RESET);
}

static void poke_wdt(void)
{
	printk("wdt0 @ 0x%x (read-only poke)\n", (uint32_t)SMC_WDT0_BASE);
	expect_eq("wdt0 KEY locked", REG_READ(SMC_WDT0_BASE + WDT_KEY), 0u);
}

static void poke_cpu_ctrl(void)
{
	const uintptr_t a = (uintptr_t)(SMC_CPU_CTRL_BASE + CPU_CTRL_SCRATCH(0));

	printk("cpu_ctrl @ 0x%x\n", (uint32_t)SMC_CPU_CTRL_BASE);
	REG_WRITE(a, 0x11223344u);
	expect_eq("cpu_ctrl SCRATCH0", REG_READ(a), 0x11223344u);
}

static void poke_reset(void)
{
	printk("reset_unit @ 0x%x\n", (uint32_t)SMC_RESET_BASE);
	/* Reachability: a decode miss typically returns 0xFFFFFFFF. */
	uint32_t cfg = REG_READ(SMC_RESET_BASE + RESET_SS_CONFIG);
	if (cfg == 0xFFFFFFFFu) {
		printk("  FAIL reset SS_CONFIG looks like a decode miss\n");
		g_fail = 1;
	} else {
		printk("  PASS reset SS_CONFIG = 0x%x\n", cfg);
	}
}

static void poke_bootrom(void)
{
	printk("bootrom @ 0x%x\n", (uint32_t)SMC_BOOTROM_BASE);
	expect_eq("bootrom[0] (zero-init)", REG_READ(SMC_BOOTROM_BASE), 0u);
}

int main(void)
{
	printk("\n=== Zephyr SMC MMIO poke (unlisted IPs) ===\n\n");

	poke_scratch();
	poke_uart1();
	poke_i3c();
	poke_i2c();
	poke_dma();
	poke_avs();
	poke_wdt();
	poke_cpu_ctrl();
	poke_reset();
	poke_bootrom();

	if (g_fail) {
		printk("\nRESULT: FAIL\n");
	} else {
		printk("\nRESULT: PASS\n");
	}
	return 0;
}
