/* SPDX-License-Identifier: Apache-2.0
 * SEP half of smu_sep_ext_axi: SEP reaches AOU two ways, matching OCH
 * (tt-oca-harness smu.sv + aou on smu_axi_in/out):
 *
 *   1. CSRs via the dedicated SEP->SMC window (0x4000_C000 == SMC 0xC000_C000).
 *   2. Catch-all AXI via smn_outbound -> xbar.sep_out -> ext_out -> AOU
 *      (after programming the outbound filter; BlockByDefault=1).
 *
 * SMC activates AOU and doorbells this side before the data-path check.
 */
#include <stdint.h>

extern int printf(const char *format, ...);

#define SMC_DONE_CELL       0x40063000u
#define DONE_TOKEN          0xA0A00001u
#define SPIN_LIMIT          50000000u
#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

/* AOU CSRs through the SEP SMC window (local 0xC000_C000). */
#define SEP_AOU_BASE        0x4000C000u
#define AOU_IP_VERSION      0x00u
#define AOU_IP_VERSION_EXP  0x00010000u

/* Outbound filter 0 (0x10A20000, 0x20-byte stride). Secure SEP CPU traffic
 * needs allow_ns=0; allow_burst widens the range to the enclosing 4 KiB page. */
#define OUTBOUND_FILTER0    0x10A20000u
#define FILTER_CONFIG       0x0000000100000013ULL
#define FILTER_START        0x00000000A0001000ULL
#define FILTER_END          0x00000000A0001FFFULL

#define EXTAXI_SEP_OUT_ADDR 0xA0001008u
#define EXTAXI_SEP_OUT_DATA 0x5E9A0A01u

static void wr64(uint32_t addr, uint64_t v)
{
    REG32(addr)     = (uint32_t)v;
    REG32(addr + 4) = (uint32_t)(v >> 32);
}

int main(void)
{
    printf("\n=== SMU AOU ext-axi test (SEP side) ===\n");

    const uint32_t ver = REG32(SEP_AOU_BASE + AOU_IP_VERSION);
    if (ver != AOU_IP_VERSION_EXP) {
        printf("FAIL: AOU ip_version via SEP->SMC window 0x%08x want 0x%08x\n",
               ver, AOU_IP_VERSION_EXP);
        return 1;
    }
    printf("AOU ip_version 0x%08x via SEP SMC window\n", ver);

    uint32_t spins = 0;
    while (REG32(SMC_DONE_CELL) != DONE_TOKEN) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SMC AOU-ext done token\n");
            return 1;
        }
    }
    printf("SMC AOU ENABLED (doorbell 0x%08x)\n", DONE_TOKEN);

    wr64(OUTBOUND_FILTER0 + 0x08, FILTER_START);
    wr64(OUTBOUND_FILTER0 + 0x10, FILTER_END);
    wr64(OUTBOUND_FILTER0 + 0x00, FILTER_CONFIG);

    REG32(EXTAXI_SEP_OUT_ADDR) = EXTAXI_SEP_OUT_DATA;
    const uint32_t got = REG32(EXTAXI_SEP_OUT_ADDR);
    if (got != EXTAXI_SEP_OUT_DATA) {
        printf("FAIL: SEP->ext_out via AOU readback 0x%08x want 0x%08x\n",
               got, EXTAXI_SEP_OUT_DATA);
        return 1;
    }
    printf("SEP->ext_out via AOU: 0x%08x\n", got);
    printf("PASS: SMU AOU ext-axi test (SEP side)\n");
    return 0;
}
