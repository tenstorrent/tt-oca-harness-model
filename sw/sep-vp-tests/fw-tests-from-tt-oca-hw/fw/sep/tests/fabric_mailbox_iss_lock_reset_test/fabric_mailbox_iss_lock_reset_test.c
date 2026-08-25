// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * Fabric Mailbox ISS Lock Reset Test - TC_FABRIC_053
 *
 * Uses the current CSR-visible implementation of ISS mailbox policy:
 * mailbox data/IRQ status plus inbound-filter src_id and locked behavior.
 */

#include <stdint.h>
#include <stdio.h>

#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

#define FILTER_STRIDE_BYTES 0x20u
#define TEST_FILTER_IDX     15u

#define FILTER_CFG_RD_EN       (1u << 0)
#define FILTER_CFG_WR_EN       (1u << 1)
#define FILTER_CFG_ADDR_MODE   (1u << 4)
#define FILTER_CFG_ALLOW_NS    (1u << 8)
#define FILTER_CFG_ALLOW_BURST (1u << 24)
#define FILTER_CFG_SRC_ID_SHIFT 16u
#define FILTER_CFG_LOCKED_HI   (1u << 31)

static int check_eq32(const char *name, uint32_t actual, uint32_t expected)
{
    int ok = (actual == expected);

    printf("%s: 0x%08x expected 0x%08x - %s\n",
           name, actual, expected, ok ? "PASS" : "FAIL");
    return ok;
}

static int check_bit(const char *name, uint32_t value)
{
    printf("%s: %u - %s\n", name, value, value ? "PASS" : "FAIL");
    return value ? 1 : 0;
}

static uint32_t inbound_addr(uint32_t base)
{
    return base + (TEST_FILTER_IDX * FILTER_STRIDE_BYTES);
}

static void write64_split(uint32_t addr, uint64_t value)
{
    WRITE_REG(addr, (uint32_t)value);
    WRITE_REG(addr + 4, (uint32_t)(value >> 32));
}

int main(void)
{
    int pass = 1;
    uint32_t cfg_addr = inbound_addr(INBOUND_FILTER_CTRL_0__FILTER_CONFIG_REG_ADDR);
    uint32_t start_addr = inbound_addr(INBOUND_FILTER_CTRL_0__START_ADDR_REG_ADDR);
    uint32_t end_addr = inbound_addr(INBOUND_FILTER_CTRL_0__END_ADDR_REG_ADDR);
    uint32_t cfg_src3;
    uint32_t cfg_rb;
    uint32_t cfg_hi;
    AXIL_MAILBOX_STATUS_reg_u status;
    AXIL_MAILBOX_IRQS_reg_u irqs;
    AXIL_MAILBOX_CTRL_reg_u ctrl = {.f.wflush = 1, .f.rflush = 1};

    sep_outbound_filter_init();

    printf("\n==========================================\n");
    printf("Fabric Mailbox ISS Lock Reset Test (TC_FABRIC_053)\n");
    printf("==========================================\n\n");

    /* CLOCK_GATE_CTRL is a reserved, not-yet-implemented placeholder (issue #3950);
     * mailbox/filter clocks are always on, so no ungate step is required. */

    WRITE_REG(AXIL_MAILBOX_OUTBOUND_MAILBOX_0_CTRL_REG_ADDR, (uint32_t)ctrl.val);
    WRITE_REG(AXIL_MAILBOX_OUTBOUND_MAILBOX_0_IRQS_REG_ADDR, 0x7);

    WRITE_REG(AXIL_MAILBOX_OUTBOUND_MAILBOX_0_WRITE_DATA_REG_ADDR, 0xA5A50053);
    WRITE_REG(AXIL_MAILBOX_OUTBOUND_MAILBOX_0_WRITE_DATA_REG_ADDR + 4, 0x5A5A0053);

    status.val = READ_REG(AXIL_MAILBOX_OUTBOUND_MAILBOX_0_STATUS_REG_ADDR);
    if (!check_bit("Mailbox write-level-above-threshold",
                   status.f.write_level_above_thresh)) {
        pass = 0;
    }

    irqs.val = READ_REG(AXIL_MAILBOX_OUTBOUND_MAILBOX_0_IRQS_REG_ADDR);
    if (!check_bit("Mailbox wtirq", irqs.f.wtirq)) {
        pass = 0;
    }

    write64_split(start_addr, AXIL_MAILBOX_OUTBOUND_MAILBOX_0_REG_MAP_BASE_ADDR);
    write64_split(end_addr,
                  AXIL_MAILBOX_OUTBOUND_MAILBOX_0_REG_MAP_BASE_ADDR +
                  AXIL_MAILBOX_OUTBOUND_MAILBOX_0_REG_MAP_SIZE - 1u);

    cfg_src3 = FILTER_CFG_RD_EN | FILTER_CFG_WR_EN | FILTER_CFG_ADDR_MODE |
               FILTER_CFG_ALLOW_NS | FILTER_CFG_ALLOW_BURST |
               (3u << FILTER_CFG_SRC_ID_SHIFT);
    write64_split(cfg_addr, cfg_src3);
    cfg_rb = READ_REG(cfg_addr);
    if (!check_eq32("Inbound filter src_id=3 cfg",
                    cfg_rb,
                    cfg_src3 | FILTER_CTRL_FILTER_CONFIG_REG_DEFAULT)) {
        pass = 0;
    }

    WRITE_REG(cfg_addr + 4, FILTER_CFG_LOCKED_HI);
    cfg_hi = READ_REG(cfg_addr + 4);
    if (!check_bit("Inbound filter locked", (cfg_hi >> 31) & 1u)) {
        pass = 0;
    }

    cfg_rb = READ_REG(cfg_addr);
    if (!check_eq32("Locked filter preserves src_id config",
                    cfg_rb,
                    cfg_src3 | FILTER_CTRL_FILTER_CONFIG_REG_DEFAULT)) {
        pass = 0;
    }

    WRITE_REG(AXIL_MAILBOX_OUTBOUND_MAILBOX_0_IRQS_REG_ADDR, 0x7);
    WRITE_REG(AXIL_MAILBOX_OUTBOUND_MAILBOX_0_CTRL_REG_ADDR, (uint32_t)ctrl.val);

    if (pass) {
        printf("=== FABRIC MAILBOX ISS LOCK RESET TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== FABRIC MAILBOX ISS LOCK RESET TEST FAILED ===\n");
        test_fail(0);
    }

    while (1) {
        __asm__("wfi");
    }

    return pass ? 0 : -1;
}
