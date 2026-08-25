// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// Simple UART test to verify output is working
#include <stdint.h>

#define UART_BASE 0x44000000
#define UART_THR  (*(volatile uint32_t *)(UART_BASE + 0x00))

void uart_putc(char c) {
    UART_THR = (uint32_t)c;
    for (volatile int i = 0; i < 100; i++);
}

void uart_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');
        }
        uart_putc(*str++);
    }
}

int main(void) {
    uart_print("=== UART Test Start ===\n");
    uart_print("Line 1\n");
    uart_print("Line 2\n");
    uart_print("Line 3\n");
    uart_print("=== UART Test End ===\n");
    
    while(1);  // Hang to keep VP running
    return 0;
}
