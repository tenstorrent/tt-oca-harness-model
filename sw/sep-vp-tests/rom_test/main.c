// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include <stdint.h>

// Simple UART print functions

extern int printf(const char *format, ...);

#define ROM_BASE 0x10040000
#define ROM_SIZE 0x00010000 // 64KB

int main() {
    printf("\n=== ROM Access Test ===\n");

    volatile uint32_t *rom_ptr = (volatile uint32_t *)ROM_BASE;
    uint32_t val;

    // Test 1: Read from beginning of ROM
    printf("Reading from ROM base (0x10040000)...\n");
    val = rom_ptr[0];
    printf("Value: 0x");
    printf("%X",val);
    printf("\n");

    // Test 2: Read from end of ROM
    printf("Reading from ROM end (0x1000FFFC)...\n");
    val = rom_ptr[(ROM_SIZE / 4) - 1];
    printf("Value: 0x");
    printf("%X", val);
    printf("\n");

    // Test 3: Read from middle
    printf("Reading from ROM middle...\n");
    val = rom_ptr[1024];
    printf("Value: 0x");
    printf("%X",val);
    printf("\n");

    printf("ROM Read Test Passed (No Bus Error)\n");

    return 0;
}
