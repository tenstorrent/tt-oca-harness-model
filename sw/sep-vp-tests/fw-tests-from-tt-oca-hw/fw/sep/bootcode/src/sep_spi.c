/*
 * OCH SEP ROM - Cadence xSPI init implementation
 *
 * This is the Cadence xSPI initialization flow for this ROM.
 *
 * Key implementation notes:
 * - Uses absolute register addresses from `och_sep_top_reg.h` directly.
 * - Uses virtual console output via SEP scratch register for debug visibility.
 * - Uses ROM MMIO helpers (`mmio_read32/mmio_write32`) instead of READ_EXT/WRITE_EXT.
 *
 * NOTE:
 * - This file is built for a freestanding ROM environment (no libc).
 * - The xSPI flash is memory-mapped at XIP_REGION (0x3000_0000) in the OCH address map.
 */

#include <stdbool.h>
#include <stdint.h>

#include "errors.h"
#include "rom_mmio.h"

// Generated absolute register map for OCH SEP.
#include "och_sep_top_reg.h"

#include "sep_spi.h"
#include "spi_tlv.h"

#ifndef BIT
#define BIT(n) (1u << (n))
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define pr_debug(...) ((void)0)
#define pr_info(...) ((void)0)
#define pr_error(...) ((void)0)

// SPI error codes now come from status_values.h (SEP_MSG_SPI_*) via errors.h.

enum side {
    PRIMARY = 0,
    BACKUP
};

// Location in flash that the TLV can be found.
static uint32_t flash_address[2] = {
    (uint32_t)SEP_AXI_EXTENSION_XIP_REGION_MEM_BASE_ADDR,
    (uint32_t)SEP_AXI_EXTENSION_XIP_REGION_MEM_BASE_ADDR + 0x40000u,
};

static bool primary_tlv_failed;
static uint16_t sysclk_freq_mhz;

// boot flow state queried main loop
bool spi_primary_tlv_failed(void) { return primary_tlv_failed; }

// main loop uses this to inform SPI code the sysclk
void spi_set_sysclk(uint16_t mhz) { sysclk_freq_mhz = mhz; }

#if defined(TEST_BUILD)
// DV-only frequency-robust SPI clocking. Returns the core frequency (MHz) from
// the sensed eFuse smu_pll_sysclk field, or 0 when blank. In the SEP UVM model
// core_clk is plain-generated (e.g. 800 MHz) even when the bl0_pll_clk strap is
// deasserted and ROM pll_init() selected refclk, so pll_init()'s 100 MHz value
// does not match the active clock and the SPI divider would be wrong; directed
// eFuse preloads carry the real core frequency here. Compiled only into
// TEST_BUILD images: the release ROM keeps pll_init()'s result, where
// smu_pll_sysclk is the *configured* PLL frequency and is not necessarily the
// active SPI clock when refclk is selected.
static uint16_t spi_fuse_sysclk_mhz(void) {
    SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_reg_u spi_ctrl;
    spi_ctrl.val = mmio_read32(SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR);
    return (uint16_t)spi_ctrl.f.smu_pll_sysclk;
}
#endif /* TEST_BUILD */

// SPI parameters that are common for all configurations.
// Partial discovery: tell the controller the flash is 8-lane DDR (Octal DDR)
// instead of using full discovery (num_lines=0, value 0x480) which sends probe
// commands that the simulation flash models (N25Q/MT35X) do not recognize.
// Value 0x490 matches the register default and the spi_sanity_cadence test:
//   discovery_num_lines=8 (bits [4:1]), discovery_cmd_type=1/DDR (bits [8:7]).
#define SPI_DISCOVERY_CTRL 0x00000490u
#define SPI_DQ_TIMING 0x00000101u
#define SPI_DLL_MASTER 0x003400e4u

// Defaults for the SPI boot flow to use (25MHz, partial discovery).
static const struct spi_param_tlv spi_initial_params[] = {
    // DQS part (preferred first).
    // dll_slave=0x3305 matches the spi_sanity_cadence 25 MHz validated params.
    [0] = {
        .discovery_ctrl = SPI_DISCOVERY_CTRL,
        .dq_timing = SPI_DQ_TIMING,
        .dqs_timing = 0x00000404u,
        .gate_lpbk = 0x00200030u,
        .dll_slave = 0x00003305u,
        .dll_master = SPI_DLL_MASTER,
        .misc = 0x00000001u,
        .rb_valid_time = 0x00002710u,
    },
    // Non-DQS part.
    [1] = {
        .discovery_ctrl = SPI_DISCOVERY_CTRL,
        .dq_timing = SPI_DQ_TIMING,
        .dqs_timing = 0x00300404u,
        .gate_lpbk = 0x00200030u,
        .dll_slave = 0x0000332du,
        .dll_master = SPI_DLL_MASTER,
        .misc = 0x00000000u,
        .rb_valid_time = 0x00002710u,
    },
    // Low board delay configuration.
    [2] = {
        .discovery_ctrl = SPI_DISCOVERY_CTRL,
        .dq_timing = SPI_DQ_TIMING,
        .dqs_timing = 0x00300404u,
        .gate_lpbk = 0x00200030u,
        .dll_slave = 0x00003300u,
        .dll_master = SPI_DLL_MASTER,
        .misc = 0x00000000u,
        .rb_valid_time = 0x00002710u,
    },
    // Repeats (fallback when entries overridden by fuses).
    [3] = {
        .discovery_ctrl = SPI_DISCOVERY_CTRL,
        .dq_timing = SPI_DQ_TIMING,
        .dqs_timing = 0x00000404u,
        .gate_lpbk = 0x00200030u,
        .dll_slave = 0x00003305u,
        .dll_master = SPI_DLL_MASTER,
        .misc = 0x00000001u,
        .rb_valid_time = 0x00002710u,
    },
    [4] = {
        .discovery_ctrl = SPI_DISCOVERY_CTRL,
        .dq_timing = SPI_DQ_TIMING,
        .dqs_timing = 0x00300404u,
        .gate_lpbk = 0x00200030u,
        .dll_slave = 0x0000332du,
        .dll_master = SPI_DLL_MASTER,
        .misc = 0x00000000u,
        .rb_valid_time = 0x00002710u,
    },
    [5] = {
        .discovery_ctrl = SPI_DISCOVERY_CTRL,
        .dq_timing = SPI_DQ_TIMING,
        .dqs_timing = 0x00300404u,
        .gate_lpbk = 0x00200030u,
        .dll_slave = 0x00003300u,
        .dll_master = SPI_DLL_MASTER,
        .misc = 0x00000000u,
        .rb_valid_time = 0x00002710u,
    },
};

#define NUM_FUSE_BANKS 3
struct fuse_map {
    uint32_t fuse_base[NUM_FUSE_BANKS];
    uint32_t fuse_offset;
    uint32_t reg;
};

// Ordering must match bits in SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN.spi_control_field_en.
static const struct fuse_map fuse_map[8] = {
    {.fuse_base = {SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR, SEP_EFUSE_MAP_RESERVED_0_REG_ADDR,
                   SEP_EFUSE_MAP_RESERVED_1_REG_ADDR},
     .fuse_offset =
         SEP_EFUSE_MAP_SPI_DISCOVERY_CTRL_REG_ADDR - SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR,
     .reg = SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR},
    {.fuse_base = {SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR, SEP_EFUSE_MAP_RESERVED_0_REG_ADDR,
                   SEP_EFUSE_MAP_RESERVED_1_REG_ADDR},
     .fuse_offset =
         SEP_EFUSE_MAP_SPI_PHY_DQ_TIMING_REG_ADDR - SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR,
     .reg = SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_ADDR},
    {.fuse_base = {SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR, SEP_EFUSE_MAP_RESERVED_0_REG_ADDR,
                   SEP_EFUSE_MAP_RESERVED_1_REG_ADDR},
     .fuse_offset =
         SEP_EFUSE_MAP_SPI_PHY_DQS_TIMING_REG_ADDR - SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR,
     .reg = SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_ADDR},
    {.fuse_base = {SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR, SEP_EFUSE_MAP_RESERVED_0_REG_ADDR,
                   SEP_EFUSE_MAP_RESERVED_1_REG_ADDR},
     .fuse_offset =
         SEP_EFUSE_MAP_SPI_PHY_GATE_LPBK_REG_ADDR - SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR,
     .reg = SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_ADDR},
    {.fuse_base = {SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR, SEP_EFUSE_MAP_RESERVED_0_REG_ADDR,
                   SEP_EFUSE_MAP_RESERVED_1_REG_ADDR},
     .fuse_offset =
         SEP_EFUSE_MAP_SPI_PHY_DLL_SLAVE_REG_ADDR - SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR,
     .reg = SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_ADDR},
    {.fuse_base = {SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR, SEP_EFUSE_MAP_RESERVED_0_REG_ADDR,
                   SEP_EFUSE_MAP_RESERVED_1_REG_ADDR},
     .fuse_offset =
         SEP_EFUSE_MAP_SPI_PHY_DLL_MASTER_REG_ADDR - SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR,
     .reg = SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_ADDR},
    {.fuse_base = {SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR, SEP_EFUSE_MAP_RESERVED_0_REG_ADDR,
                   SEP_EFUSE_MAP_RESERVED_1_REG_ADDR},
     .fuse_offset =
         SEP_EFUSE_MAP_SPI_PHY_MISC_REG_ADDR - SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR,
     .reg = SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_MISC_REG_ADDR},
    {.fuse_base = {SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR, SEP_EFUSE_MAP_RESERVED_0_REG_ADDR,
                   SEP_EFUSE_MAP_RESERVED_1_REG_ADDR},
     .fuse_offset =
         SEP_EFUSE_MAP_SPI_RB_VALID_TIME_REG_ADDR - SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR,
     .reg = SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_RB_VALID_TIME_REG_ADDR},
};

// Configure SPI mux: select Cadence xSPI controller and clear cs_force_high.
//
// The OCH SEP has a SPI mux (OCH_SEP_SPI_MUX_CTRL) that selects between the
// Cadence xSPI and OpenTitan SPI Host controllers.  The default register value
// has cs_force_high=1, which forces all CS# pins high (deasserted) for safety
// during power-up.  This must be cleared before the xSPI controller can
// communicate with the flash.
static void spi_configure_mux(void) {
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };

    // Select Cadence xSPI controller (0 = Cadence, 1 = OpenTitan).
    spi_mux.f.spi_sel = 0;

    // Clear cs_force_high to allow normal CS# operation.
    spi_mux.f.cs_force_high = 0;

    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
    simputs("SPI_MUX: sel=0 cs_force_high=0\n");
}

static void sep_spi_delay(unsigned int spi_cycles) {
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u dummy_spi_ctrl;

    // Add 2 cycles to account for async start vs SPI clock edge.
    spi_cycles += 2;

    for (; spi_cycles > 0; spi_cycles--) {
        // Volatile read so should not be optimised away.
        dummy_spi_ctrl.val = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR);
    }
}

static uint32_t spi_read_fuse_ctrl(int index) {
    switch (index) {
    case 0:
        return mmio_read32(SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_REG_ADDR);
    case 1:
        return mmio_read32(SEP_EFUSE_MAP_RESERVED_0_REG_ADDR);
    case 2:
        return mmio_read32(SEP_EFUSE_MAP_RESERVED_1_REG_ADDR);
    default:
        return 0;
    }
}

static bool spi_check_fuses(int index) {
    SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_reg_u spi_fuse;

    if (index >= NUM_FUSE_BANKS) {
        return false;
    }

    spi_fuse.val = spi_read_fuse_ctrl(index);
    simputshex32("FUSE INDEX: ", (uint32_t)index);
    simputshex32("SPI_CTRL_FIELD_EN: ", (uint32_t)spi_fuse.f.spi_control_field_en);

    if (!spi_fuse.f.spi_control_field_en) {
        simputs("FUSE: No SPI config fuses enabled\n");
        return false;
    }

    return true;
}

static void set_fuse_parameter(int index) {
    SEP_EFUSE_MAP_SEP_SPI_CTRL_FIELD_EN_reg_u spi_fuse;

    if (index >= NUM_FUSE_BANKS) {
        return;
    }
    spi_fuse.val = spi_read_fuse_ctrl(index);

    for (unsigned i = 0; i < ARRAY_SIZE(fuse_map); i++) {
        const struct fuse_map *map = &fuse_map[i];
        uint32_t val;

        if (spi_fuse.f.spi_control_field_en & BIT(i)) {
            val = mmio_read32(map->fuse_base[index] + map->fuse_offset);
            mmio_write32(map->reg, val);
        }
    }
}

// The spi_clock argument is either the constant 25MHz default, or a non-zero value from the TLV.
static uint8_t spi_calculate_divisor(uint32_t pll_mhz, uint32_t spi_clock_mhz) {
    if (spi_clock_mhz == 0) {
        simputs("SPI: spi_clock is zero, forcing to default 25MHz\n");
        spi_clock_mhz = 25;
    }
    return (uint8_t)(pll_mhz / spi_clock_mhz);
}

static void program_spi_clk_div(uint8_t divider) {
    OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_reg_u spi_clk_div_ctrl = {
        .val = OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_DEFAULT,
    };

    spi_clk_div_ctrl.f.clock_divider_value = divider;
    spi_clk_div_ctrl.f.clock_div_set = 1;
    spi_clk_div_ctrl.f.clock_dutycycle = 128;
    spi_clk_div_ctrl.f.clock_div_enable = 1;

    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR, spi_clk_div_ctrl.val);
}

static void spi_controller_enable(void) {
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u spi_ctrl;

    spi_ctrl.val = OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_DEFAULT;
    spi_ctrl.f.spi_enable = 1;
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, spi_ctrl.val);

    // Give SPI_ENABLE time to settle before releasing reset.
    // Use a simple volatile counter (matching spi_sanity_cadence test exactly)
    // instead of sep_spi_delay() which reads SPI_CTRL during the delay.
    // Reading SPI_CTRL while the controller is in a transitional state
    // (enabled but still in reset) could potentially cause wrapper side-effects.
    {
        volatile int delay;
        for (delay = 0; delay < 100; delay++) {}
    }

    spi_ctrl.f.spi_reset_n_n0_scan = 1;
    spi_ctrl.f.spi_ctrl_reg_reset_n_n0_scan = 1;
    spi_ctrl.f.spi_phy_reg_reset_n_n0_scan = 1;
    spi_ctrl.f.spi_phy_reset_n_n0_scan = 1;
    spi_ctrl.f.spi_axi_reset_n_n0_scan = 1;
    spi_ctrl.f.spi_reg_reset_n_n0_scan = 1;
    spi_ctrl.f.spi_xspi_reg_reset_n_n0_scan = 1;
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, spi_ctrl.val);

    simputshex32("EN_CTRL=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR));
}

static void spi_controller_disable(void) {
    OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_reg_u spi_ctrl;

    spi_ctrl.val = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR);
    spi_ctrl.f.spi_reset_n_n0_scan = 0;
    spi_ctrl.f.spi_ctrl_reg_reset_n_n0_scan = 0;
    spi_ctrl.f.spi_phy_reg_reset_n_n0_scan = 0;
    spi_ctrl.f.spi_phy_reset_n_n0_scan = 0;
    spi_ctrl.f.spi_axi_reset_n_n0_scan = 0;
    spi_ctrl.f.spi_reg_reset_n_n0_scan = 0;
    spi_ctrl.f.spi_xspi_reg_reset_n_n0_scan = 0;
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, spi_ctrl.val);

    sep_spi_delay(5);

    spi_ctrl.f.spi_enable = 0;
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR, spi_ctrl.val);

    sep_spi_delay(6);
}

static bool spi_init_poll(void) {
    CTRL_CMD_STAT_CTRL_STATUS_reg_u spi_status;
    do {
        spi_status.val = mmio_read32(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_CTRL_STATUS_REG_ADDR);
    } while (spi_status.f.init_comp == 0);

    simputshex32("SPI_STATUS=", spi_status.val);

    if (spi_status.f.init_fail) {
        // Dump Cadence core diagnostic registers on failure.
        simputshex32("SPI_BOOT_STATUS=",
            mmio_read32(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_BOOT_STATUS_REG_ADDR));
        simputshex32("SPI_INTR_STATUS=",
            mmio_read32(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CMD_STAT_A_INTR_STATUS_REG_ADDR));
        simputshex32("SPI_CORE_DISCOVERY=",
            mmio_read32(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CFG_COMMON_A_DISCOVERY_CONTROL_REG_ADDR));
        simputshex32("SPI_CTRL_VAL=",
            mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR));
        simputshex32("SPI_CLK_DIV_VAL=",
            mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR));
        // Cadence core version — confirms core is accessible (expect 0x65220206).
        simputshex32("SPI_VERSION=",
            mmio_read32(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_CTRL_CONSTS_A_XSPI_CTRL_VERSION_REG_ADDR));
        // DLL PHY control — effective DLL configuration after init (default 0x01030707).
        simputshex32("SPI_DLL_PHY=",
            mmio_read32(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_RF_MINICTRL_REGS_A_DLL_PHY_CTRL_REG_ADDR));
        // Clock mode settings — internal clock configuration.
        simputshex32("SPI_CLK_MODE=",
            mmio_read32(SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_RF_MINICTRL_REGS_A_CLOCK_MODE_SETTINGS_REG_ADDR));
    }

    switch (spi_status.f.init_fail) {
    case 0x3: // Invalid
        return false;
    case 0x2: // Legacy SPI device detected
        return true;
    case 0x1: // Initialization has failed
        return false;
    case 0x0: // xSPI device detected
        return true;
    }
    return false;
}

static void spi_param_apply(const struct spi_param_tlv *params) {
    // Write PHY registers first, discovery control last.
    // This matches the spi_sanity_cadence test order.
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_ADDR, params->dqs_timing);
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_ADDR, params->dq_timing);
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_ADDR, params->dll_master);
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_ADDR, params->dll_slave);
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_ADDR, params->gate_lpbk);
    // Skip MISC and RB_VALID_TIME: the spi_sanity_cadence test does NOT write
    // these registers and relies on their HW defaults (0x1 and 0x2710).
    // Writing them explicitly may trigger wrapper side-effects.
    mmio_write32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR, params->discovery_ctrl);

    // Readback verification: confirm the values we just wrote actually stuck
    // in the INIT wrapper registers.
    simputshex32("PA_DQS=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_ADDR));
    simputshex32("PA_DQ=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_ADDR));
    simputshex32("PA_DM=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_ADDR));
    simputshex32("PA_DS=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_ADDR));
    simputshex32("PA_GL=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_ADDR));
    simputshex32("PA_DC=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR));
}

static bool spi_param_validate(struct spi_param_tlv *params) {
    simputshex32("TLV_TYPE=", (uint32_t)params->type);
    simputshex32("TLV_LEN=", (uint32_t)params->length);
    simputshex32("TLV_FLAGS=", (uint32_t)params->flags);
    if (params->type != SPI_PARAM_TLV_TYPE) {
        return false;
    }

    if (params->length < (sizeof(struct spi_param_tlv) - 4u)) { // Exclude type+length
        return false;
    }

    return true;
}

static void spi_param_load(struct spi_param_tlv *params, uint32_t addr) {
    simputshex32("TLV_LOAD_ADDR=", addr);
    volatile uint32_t *dst = (uint32_t *)params;
    uint32_t *src = (uint32_t *)addr;
    uint32_t *end = (uint32_t *)(addr + sizeof(struct spi_param_tlv));

    while (src < end) {
        *dst++ = *src++;
    }
    // Dump first 3 words of loaded TLV for debug.
    uint32_t *d = (uint32_t *)params;
    simputshex32("TLV_W0=", d[0]);
    simputshex32("TLV_W1=", d[1]);
    simputshex32("TLV_W2=", d[2]);
}

static void spi_param_save(struct spi_param_tlv *params) {
    params->type = SPI_PARAM_TLV_TYPE;
    params->length = sizeof(struct spi_param_tlv) - 4u;
    params->flags = 0;
    params->spi_freq = 0;
    params->unused1 = 0;
    params->unused2 = 0;

    params->discovery_ctrl = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR);
    params->dq_timing = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_ADDR);
    params->dqs_timing = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_ADDR);
    params->gate_lpbk = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_ADDR);
    params->dll_slave = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_ADDR);
    params->dll_master = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_ADDR);
    params->misc = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_MISC_REG_ADDR);
    params->rb_valid_time = mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_RB_VALID_TIME_REG_ADDR);
}

// Phase 1: Try each param set, init controller, then load+validate TLV.
// TLV loading is inside the retry loop, so an invalid TLV causes the next
// param set to be tried just like a controller init failure. Returns 0 only
// when both controller init and TLV validation succeed.
static uint32_t spi_init_phase1(enum side side, struct spi_param_tlv *params) {
    const uint8_t div = spi_calculate_divisor(sysclk_freq_mhz, 25);
    simputshex32("SPI_SYSCLK=", (uint32_t)sysclk_freq_mhz);
    simputshex32("SPI_DIV=", (uint32_t)div);

    for (unsigned i = 0; i < ARRAY_SIZE(spi_initial_params); i++) {
        simputshex32("SPI_TRY=", i);

        spi_controller_disable();

        // Program clock divider BEFORE writing PHY INIT registers.
        // The Cadence wrapper uses the divided SPI clock domain for
        // INIT register synchronisation.
        program_spi_clk_div(div);

        spi_param_apply(&spi_initial_params[i]);

        if (spi_check_fuses((int)i)) {
            set_fuse_parameter((int)i);
        }

        spi_controller_enable();

        if (!spi_init_poll()) {
            continue;
        }

        simputsdec24("SPI flash detected using param set ", i);

        // Controller init succeeded — load and validate TLV from flash.
        spi_param_load(params, flash_address[side]);

        if (spi_param_validate(params) == false) {
            simputs("SPI_TLV_INVALID\n");
            continue;
        }

        simputs("SPI_TLV_VALID\n");
        return 0;
    }

    return SEP_MSG_SPI_NOT_DETECTED_DEFAULT;
}

// Phase 2: Apply TLV params (already validated by Phase 1) to re-init
// the controller at optimal settings.
// Receives validated params, applies them, and falls back to saved defaults
// on failure.
static uint32_t spi_init_phase2(enum side side, struct spi_param_tlv *params) {
    struct spi_param_tlv old_params;
    // Zero manually — freestanding, no memset.
    {
        uint8_t *p = (uint8_t *)&old_params;
        for (unsigned i = 0; i < sizeof(old_params); i++) p[i] = 0;
    }

    if (params->flags & SPI_PARAM_KEEP_DEFAULT_INIT) {
        simputs("SPI: KEEP_DEFAULT_INIT\n");
        return 0;
    }

    // Save working Phase-1 config for later fallback.
    spi_param_save(&old_params);

    spi_controller_disable();
    spi_param_apply(params);

    if (params->spi_freq) {
        program_spi_clk_div(spi_calculate_divisor(sysclk_freq_mhz, params->spi_freq));
    }

    spi_controller_enable();

    if (spi_init_poll() == false) {
        if (side == PRIMARY) {
            return SEP_MSG_SPI_PRIMARY_TLV_FAILED;
        }

        // Backup: restore Phase-1 defaults and retry.
        spi_controller_disable();
        spi_param_apply(&old_params);
        program_spi_clk_div(spi_calculate_divisor(sysclk_freq_mhz, 25));
        spi_controller_enable();

        if (spi_init_poll() == false) {
            return SEP_MSG_SPI_NOT_DETECTED_SLOW;
        }
    }

    return 0;
}

// Internal: full SPI init sequence for a given side (PRIMARY or BACKUP).
static uint32_t __spi_init(enum side side) {
    struct spi_param_tlv params;
    // Zero manually — freestanding, no memset.
    uint8_t *p = (uint8_t *)&params;
    for (unsigned i = 0; i < sizeof(params); i++) p[i] = 0;
    uint32_t error_code;

    // OCH-specific: configure SPI mux (select Cadence xSPI, clear cs_force_high).
    spi_configure_mux();

    error_code = spi_init_phase1(side, &params);
    if (error_code)
        return error_code;

    return spi_init_phase2(side, &params);
}

void spi_set_rotate(bool rotate) {
    if (rotate) {
        uint32_t tmp = flash_address[0];
        flash_address[0] = flash_address[1];
        flash_address[1] = tmp;
    }
}

uint32_t spi_init(void) {
    primary_tlv_failed = false;

#if defined(TEST_BUILD)
    // DV-only: prefer the tb-deposited core frequency (see spi_fuse_sysclk_mhz)
    // so SCLK = core/div stays constant across the 100/800 MHz core clocks the
    // UVM model generates. No-op when the fuse is blank; excluded from release
    // images so real boot keeps using pll_init()'s active-clock result.
    {
        const uint16_t fuse_mhz = spi_fuse_sysclk_mhz();
        if (fuse_mhz != 0u) {
            sysclk_freq_mhz = fuse_mhz;
            simputshex32("SPI_SYSCLK_FUSE=", (uint32_t)fuse_mhz);
        }
    }
#endif /* TEST_BUILD */

    // Dump HW defaults before any writes.
    simputs("SPI_HW_DEFAULTS:\n");
    simputshex32("  CTRL=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CTRL_REG_ADDR));
    simputshex32("  CLK=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_CLK_DIV_CTRL_REG_ADDR));
    simputshex32("  DQS=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQS_TIMING_REG_ADDR));
    simputshex32("  DQ=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DQ_TIMING_REG_ADDR));
    simputshex32("  DM=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_MASTER_CTRL_REG_ADDR));
    simputshex32("  DS=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_DLL_SLAVE_CTRL_REG_ADDR));
    simputshex32("  GL=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_GATE_LPBK_CTRL_REG_ADDR));
    simputshex32("  DC=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_SPI_DISCOVERY_CTRL_REG_ADDR));
    simputshex32("  MI=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_PHY_MISC_REG_ADDR));
    simputshex32("  RB=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_CDNS_SPI_CTRL_INIT_RB_VALID_TIME_REG_ADDR));
    simputshex32("  MUX=", mmio_read32(SEP_AXI_EXTENSION_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR));

    uint32_t err = __spi_init(PRIMARY);
    if (err) {
        primary_tlv_failed = true;
    }
    return err;
}

uint32_t spi_reinit(void) {
    return __spi_init(BACKUP);
}
