/* SPDX-License-Identifier: Apache-2.0
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
 * sw/smc-vp-tests/smc-map-coherence-test/main.c
 *
 * Cumulative map-coherence ELF (D4=H1): prove modeled IPs sit at the
 * Phase-1 RTL bases from smc_common.h, and that named stubs expose their
 * identity tokens, so a wrong IP (or no IP) on a slot fails.
 *
 * IMPORTANT — every check must use a NON-ZERO discriminator.  An address
 * that no longer decodes to its model falls through to the platform's
 * catch-all stub, which is write-ignore / read-as-zero.  A check of the
 * form `expect_eq(REG_READ(base + off), 0)` therefore passes just as
 * happily when the IP has vanished from the map, which is exactly the
 * regression this ELF exists to catch.  For registers whose reset value
 * is 0, write a token first and read it back.
 *
 * Covers realignment Phases 1-3.  The one remaining deliberate deviation from
 * smc_local_xbar_pkg.sv is the VP-only AOU park at 0xC000_C000 (D1=A), which
 * has no RTL slot; everything else checked here is at its RTL base.
 */
#include "smc_common.h"

extern int printf(const char *fmt, ...);

/* Pre-realignment bases, kept local so a regression that resurrects one is
 * caught even though the macros are gone from smc_common.h.
 *
 * The old OCTS base (0xC000_E000) is deliberately NOT probed: Phase 2 shrank
 * periph_main to the RTL end 0xC000_B800, so that address no longer decodes
 * anywhere and the fabric answers TLM_ADDRESS_ERROR.  The cluster turns that
 * into a load fault and common/start.S traps forever, which would hang this
 * ELF rather than fail it.  The positive OCTS check at 0xC000_A000 already
 * proves the timer moved. */
#define OLD_AVSBUS_BASE     0xC0008000ULL
#define OLD_AOU_BASE        0xC0004000ULL

#define AOU_IP_VERSION      0x00u
#define AOU_IP_VERSION_EXP  0x00010000u

#define PLL_AG_MUX_WMASK    0x3F3FFFFFu
#define TEL_INTR_MASK       (TEL_INTR_MISSING_LAST | TEL_INTR_BUFFER_THRESHOLD)

static int g_pass = 1;

static void expect_eq(const char *name, uint32_t got, uint32_t want)
{
    if (got != want) {
        printf("  FAIL: %s got=0x%x want=0x%x\n", name, got, want);
        g_pass = 0;
    } else {
        printf("  OK: %s=0x%x\n", name, got);
    }
}

/* Negative check: `got` must differ from the signature of the IP that used
 * to own (or must not own) this address. */
static void expect_ne(const char *name, uint32_t got, uint32_t forbidden)
{
    if (got == forbidden) {
        printf("  FAIL: %s reads forbidden signature 0x%x\n", name, forbidden);
        g_pass = 0;
    } else {
        printf("  OK: %s not 0x%x (read 0x%x)\n", name, forbidden, got);
    }
}

/* Write a token and read it back — the only way to distinguish a live model
 * from the read-as-zero catch-all stub for a reset-to-zero register. */
static void expect_rw(const char *name, uint64_t addr, uint32_t token,
                      uint32_t expect_back)
{
    REG_WRITE(addr, token);
    expect_eq(name, REG_READ(addr), expect_back);
}

int main(void)
{
    printf("\n=== SMC map-coherence test (Phases 1-3) ===\n\n");

    /* ---- Modeled windows -------------------------------------------- */
    printf("UART0-3 SCR (16550 @ wrap +0x100, D3=U1):\n");
    expect_rw("UART0 SCR", SMC_UART0_BASE + UART_SCR, 0xA0u, 0xA0u);
    expect_rw("UART1 SCR", SMC_UART1_BASE + UART_SCR, 0xA1u, 0xA1u);
    expect_rw("UART2 SCR", SMC_UART2_BASE + UART_SCR, 0xA2u, 0xA2u);
    expect_rw("UART3 SCR", SMC_UART3_BASE + UART_SCR, 0xA3u, 0xA3u);

    printf("AVSBus @ 0xC000_4000:\n");
    expect_eq("AVS CFG_0 reset", REG_READ(SMC_AVSBUS_BASE + AVS_CFG_0),
              AVS_CFG_0_RESET);
    expect_rw("AVS CFG_0 RW", SMC_AVSBUS_BASE + AVS_CFG_0,
              0x000300FFu, 0x000300FFu);

    printf("OCTS system timer @ 0xC000_A000:\n");
    expect_eq("OCTS CTRL reset", REG_READ(SMC_OCTS_TIMER_BASE + OCTS_CTRL),
              OCTS_CTRL_RESET);
    expect_rw("OCTS PRESET_LO", SMC_OCTS_TIMER_BASE + OCTS_TIMER_PRESET_LO,
              0xC0A57E57u, 0xC0A57E57u);

    printf("Telemetry0 @ 0xC000_9000:\n");
    expect_rw("TEL0 INTR_ENABLE", SMC_TELEMETRY0_BASE + TEL_INTR_ENABLE,
              0xFFFFFFFFu, TEL_INTR_MASK);
    REG_WRITE(SMC_TELEMETRY0_BASE + TEL_INTR_ENABLE, 0u);

    printf("AOU @ VP park 0xC000_C000 (D1=A):\n");
    expect_eq("AOU IP_VERSION", REG_READ(SMC_AOU_BASE + AOU_IP_VERSION),
              AOU_IP_VERSION_EXP);

    printf("PLL / PVT in smc_external:\n");
    /* AG_MUX_SELECT masks writes to 0x3F3FFFFF — a signature the stub cannot
     * imitate (it would read back 0). */
    expect_rw("PLL AG_MUX_SELECT", PLL_CNTL_BASE + PLL_CNTL_AG_MUX_SELECT,
              0xFFFFFFFFu, PLL_AG_MUX_WMASK);
    REG_WRITE(PLL_CNTL_BASE + PLL_CNTL_AG_MUX_SELECT, 0u);
    /* PVT PROCESS_CTRL resets to 0, so a readback of 0 proves nothing; use
     * the plain-RW ref-clock period register instead. */
    expect_rw("PVT REF_CLK_PERIOD_LO", SMC_PVT_WRAP_BASE + PVT_REF_CLK_PERIOD_LO,
              0x5EED1234u, 0x5EED1234u);
    REG_WRITE(SMC_PVT_WRAP_BASE + PVT_REF_CLK_PERIOD_LO, 0u);

    /* Cluster-internal blocks (Phase 3).  These live above the fabric's 16 MB
     * local-alias window, so reaching them at all also proves the harts are
     * using the direct cluster path rather than fabric.mmio_in. */
    printf("Cluster-internal PLIC / CLINT / BEU at RTL bases:\n");
    expect_rw("PLIC source-1 priority", SMC_PLIC_BASE + PLIC_PRIORITY(1),
              0x7u, 0x7u);
    REG_WRITE(SMC_PLIC_BASE + PLIC_PRIORITY(1), 0u);
    /* Large mtimecmp so the write cannot fire a timer interrupt mid-test. */
    expect_rw("CLINT hart0 mtimecmp_lo", SMC_CLINT_BASE + CLINT_MTIMECMP(0),
              0xC1177E57u, 0xC1177E57u);
    expect_ne("BEU0 ENABLE reset is live",
              (uint32_t)REG_READ64(SMC_BEU_BASE_N(0) + BEU_ENABLE), 0u);

    /* cpu_ctrl.rdl register file, reached as cluster.ctrl on the front port.
     * SCRATCH[16] occupies 0x080-0x0FF and 0x100 starts the read-only WB_PC
     * array, so an offset table that confuses the two fails one of the checks
     * below whichever way it drifted. */
    printf("cpu_ctrl register file @ 0xC003_9000:\n");
    expect_rw("cpu_ctrl SCRATCH0", SMC_CPU_CTRL_BASE + CPU_CTRL_SCRATCH(0),
              0x11223344u, 0x11223344u);
    expect_rw("cpu_ctrl SCRATCH15", SMC_CPU_CTRL_BASE + CPU_CTRL_SCRATCH(15),
              0x0F150F15u, 0x0F150F15u);
    REG_WRITE(SMC_CPU_CTRL_BASE + CPU_CTRL_SCRATCH(0), 0u);
    REG_WRITE(SMC_CPU_CTRL_BASE + CPU_CTRL_SCRATCH(15), 0u);
    REG_WRITE(SMC_CPU_CTRL_BASE + CPU_CTRL_WB_PC(0, 0), 0x11223344u);
    expect_ne("cpu_ctrl WB_PC_CORE0 is read-only, not SCRATCH",
              REG_READ(SMC_CPU_CTRL_BASE + CPU_CTRL_WB_PC(0, 0)), 0x11223344u);

    /* ---- Named stubs: identity token at offset 0 --------------------- */
    printf("Named stubs at RTL bases:\n");
    expect_eq("GPIO stub", REG_READ(SMC_GPIO_INTF_BASE), SMC_STUB_MAGIC_GPIO);
    expect_eq("MISC stub", REG_READ(SMC_MISC_WRAP_BASE), SMC_STUB_MAGIC_MISC);
    expect_eq("EFUSE_MAP stub", REG_READ(SMC_EFUSE_MAP_BASE),
              SMC_STUB_MAGIC_EFUSE_MAP);
    expect_eq("EFUSE_CTRL stub", REG_READ(SMC_EFUSE_CTRL_BASE),
              SMC_STUB_MAGIC_EFUSE_CTL);
    expect_eq("DTP stub", REG_READ(SMC_DTP_CTRL_BASE), SMC_STUB_MAGIC_DTP);

    /* ---- Negative: vacated slots must not answer as their old owner --- */
    printf("Vacated pre-realignment slots:\n");
    expect_ne("old AVSBus slot 0xC000_8000",
              REG_READ(OLD_AVSBUS_BASE + AVS_CFG_0), AVS_CFG_0_RESET);
    expect_ne("old AOU slot 0xC000_4000",
              REG_READ(OLD_AOU_BASE + AOU_IP_VERSION), AOU_IP_VERSION_EXP);

    /* D3=U1 guard: the 16550 lives at wrap +0x100, so a UART-style scratch
     * write at wrap +0x00 must not stick.  If it does, someone re-bound the
     * 16550 at offset 0 and printf would target the log-engine ctrl word. */
    REG_WRITE(SMC_UART_WRAP_BASE + UART_SCR, 0x5Au);
    expect_ne("uart_wrap +0x00 is not the 16550",
              REG_READ(SMC_UART_WRAP_BASE + UART_SCR) & 0xFFu, 0x5Au);

    if (g_pass) {
        printf("\nPASS: SMC map-coherence (Phases 1-3)\n\n");
    } else {
        printf("\nFAIL: SMC map-coherence (Phases 1-3)\n\n");
    }
    return g_pass ? 0 : 1;
}
