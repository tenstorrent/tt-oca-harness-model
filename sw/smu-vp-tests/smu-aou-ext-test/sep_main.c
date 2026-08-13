/* SPDX-License-Identifier: Apache-2.0
 * SEP half of smu_sep_ext_axi: SEP has no smn_outbound master in the VP,
 * so this side waits for the SMC AOU/ext_out doorbell on the dedicated
 * SEP->SMC window (proves the on-die path still works after AOU traffic).
 */
#include <stdint.h>

extern int printf(const char *format, ...);

#define SMC_DONE_CELL       0x40063000u
#define DONE_TOKEN          0xA0A00001u
#define SPIN_LIMIT          50000000u
#define REG32(addr)         (*(volatile uint32_t *)(uintptr_t)(addr))

int main(void)
{
    printf("\n=== SMU AOU ext-axi test (SEP side) ===\n");

    uint32_t spins = 0;
    while (REG32(SMC_DONE_CELL) != DONE_TOKEN) {
        if (++spins > SPIN_LIMIT) {
            printf("FAIL: timeout waiting for SMC AOU-ext done token\n");
            return 1;
        }
    }
    printf("SMC AOU->ext_out complete (doorbell 0x%08x)\n", DONE_TOKEN);
    printf("PASS: SMU AOU ext-axi test (SEP side)\n");
    return 0;
}
