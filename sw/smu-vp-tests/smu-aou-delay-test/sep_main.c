/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */
/*
 * SEP half of smu-aou-delay-test.  The check runs on the SMC; the SEP only
 * has to boot and print its banner so the runner sees both halves.
 */
extern int printf(const char *format, ...);

int main(void)
{
    printf("\n=== SMU AOU delay test (SEP side) ===\n");
    printf("PASS: SMU AOU delay test (SEP side)\n");
    return 0;
}
