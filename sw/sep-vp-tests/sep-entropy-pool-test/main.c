// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

#include <stdint.h>

extern int printf(const char*, ...);

#define REG32(a) (*(volatile uint32_t*)(uintptr_t)(a))

#define EDN_BASE             0x10915800u
#define EDN_CTRL             (EDN_BASE + 0x14u)
#define EDN_SW_CMD_REQ       (EDN_BASE + 0x20u)
#define EDN_SW_CMD_STS       (EDN_BASE + 0x24u)

#define POOL_BASE            0x10950000u
#define POOL_STATUS          (POOL_BASE + 0x00u)
#define POOL_IRQ_CAUSE       (POOL_BASE + 0x08u)
#define POOL_DATA            (POOL_BASE + 0x10u)

#define RESET_SW_RESET_N     0x10803000u
#define TRNG_RST_N           (1u << 5)
#define EDN_ENABLE_SW_MODE   0x9996u
#define EDN_CMD_ACK          (1u << 2)

static unsigned failures;

static void check(int condition, const char* name)
{
    printf("  %s: %s\n", name, condition ? "PASS" : "FAIL");
    if (!condition)
        ++failures;
}

static int issue_edn_command(uint32_t command)
{
    REG32(EDN_SW_CMD_REQ) = command;
    for (unsigned timeout = 0; timeout < 200000u; ++timeout) {
        uint32_t status = REG32(EDN_SW_CMD_STS);
        if ((status & EDN_CMD_ACK) != 0u)
            return ((status >> 3) & 7u) == 0u;
    }
    return 0;
}

static unsigned wait_for_pool(void)
{
    for (unsigned timeout = 0; timeout < 400000u; ++timeout) {
        unsigned level = REG32(POOL_STATUS) & 0x3fu;
        if (level != 0u)
            return level;
    }
    return 0;
}

int main(void)
{
    printf("\n=== SEP Entropy Pool Firmware Test ===\n");

    check((REG32(POOL_STATUS) & 0x1ffu) == (1u << 6),
          "reset status is empty and low");
    check((REG32(POOL_IRQ_CAUSE) & 7u) == 1u,
          "pool-low IRQ cause is live");

    REG32(EDN_CTRL) = EDN_ENABLE_SW_MODE;
    check(REG32(EDN_CTRL) == EDN_ENABLE_SW_MODE, "EDN software mode enabled");
    check(issue_edn_command(0x1u), "EDN instantiate accepted");

    /* Generate two 128-bit blocks: acmd=3, glen=2. */
    check(issue_edn_command((2u << 12) | 3u), "EDN generate accepted");
    unsigned level = wait_for_pool();
    check(level == 4u, "32-to-64 pool packing level");

    for (unsigned i = 0; i < level; ++i)
        (void)REG32(POOL_DATA); /* Any DATA read pops one 64-bit entry. */
    check((REG32(POOL_STATUS) & 0x3fu) == 0u, "firmware drains pool");

    check(issue_edn_command((1u << 12) | 3u), "second generate accepted");
    check(wait_for_pool() == 2u, "pool refills before reset");

    uint32_t reset = REG32(RESET_SW_RESET_N);
    REG32(RESET_SW_RESET_N) = reset & ~TRNG_RST_N;
    (void)REG32(RESET_SW_RESET_N);
    uint32_t reset_status = 0;
    for (unsigned timeout = 0; timeout < 100000u; ++timeout) {
        reset_status = REG32(POOL_STATUS) & 0x1ffu;
        if (reset_status == (1u << 6))
            break;
    }
    check(reset_status == (1u << 6),
          "TRNG reset scrubs pool immediately");
    REG32(RESET_SW_RESET_N) = reset | TRNG_RST_N;

    if (failures == 0)
        printf("All entropy checks PASSED\n");
    else
        printf("*** Entropy pool checks FAILED: %u ***\n", failures);
    return failures ? 1 : 0;
}
