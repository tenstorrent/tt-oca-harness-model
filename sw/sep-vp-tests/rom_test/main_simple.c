// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include <stdint.h>

#define ROM_BASE 0x10000000
#define ROM_SIZE 0x00010000 // 64KB

// Simple test that just reads from ROM and exits
// No UART needed - just verify no bus errors occur
int main() {
    volatile uint32_t *rom_ptr = (volatile uint32_t *)ROM_BASE;
    uint32_t val;
    
    // Test 1: Read from beginning of ROM
    val = rom_ptr[0];
    
    // Test 2: Read from end of ROM
    val = rom_ptr[(ROM_SIZE / 4) - 1];
    
    // Test 3: Read from middle
    val = rom_ptr[1024];
    
    // If we get here without a bus error, test passed
    // Exit via syscall
    return 0;
}
