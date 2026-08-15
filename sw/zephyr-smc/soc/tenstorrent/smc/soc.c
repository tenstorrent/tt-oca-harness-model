/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/kernel.h>

void sys_arch_reboot(int type)
{
	ARG_UNUSED(type);
	for (;;) {
		__asm__ volatile("wfi");
	}
}

/*
 * Homebrew riscv64-elf libgcc is built medlow and cannot address symbols
 * at 0x80000000 (R_RISCV_HI20 truncated against __clz_tab).  Provide the
 * two helpers the shell pulls in so we never link those libgcc objects.
 */
int __clzdi2(unsigned long long x)
{
	int n = 0;

	if (x == 0ULL) {
		return 64;
	}
	if (x <= 0x00000000FFFFFFFFULL) {
		n += 32;
		x <<= 32;
	}
	if (x <= 0x0000FFFFFFFFFFFFULL) {
		n += 16;
		x <<= 16;
	}
	if (x <= 0x00FFFFFFFFFFFFFFULL) {
		n += 8;
		x <<= 8;
	}
	if (x <= 0x0FFFFFFFFFFFFFFFULL) {
		n += 4;
		x <<= 4;
	}
	if (x <= 0x3FFFFFFFFFFFFFFFULL) {
		n += 2;
		x <<= 2;
	}
	if (x <= 0x7FFFFFFFFFFFFFFFULL) {
		n += 1;
	}
	return n;
}

int __ctzdi2(unsigned long long x)
{
	int n = 0;

	if (x == 0ULL) {
		return 64;
	}
	if ((x & 0xFFFFFFFFULL) == 0ULL) {
		n += 32;
		x >>= 32;
	}
	if ((x & 0xFFFFULL) == 0ULL) {
		n += 16;
		x >>= 16;
	}
	if ((x & 0xFFULL) == 0ULL) {
		n += 8;
		x >>= 8;
	}
	if ((x & 0xFULL) == 0ULL) {
		n += 4;
		x >>= 4;
	}
	if ((x & 0x3ULL) == 0ULL) {
		n += 2;
		x >>= 2;
	}
	if ((x & 0x1ULL) == 0ULL) {
		n += 1;
	}
	return n;
}
