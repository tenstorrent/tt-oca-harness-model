/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/smc-rom-boot-test/rom.ld
 *
 * Links the ROM-only boot image at the SMC boot ROM base, where the cores start when the
 * platform is given no ELF.
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    ROM (rx) : ORIGIN = 0xC0040000, LENGTH = 128K
}

SECTIONS
{
    .text : { *(.text.init) *(.text*) *(.rodata*) } > ROM
}
