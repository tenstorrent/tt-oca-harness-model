// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * Simple UART output for OpenTitan compatibility layer
 * Provides basic print functionality for LOG_INFO and debugging
 */

#include <stdint.h>

// VP "magic" stdout port — the SEP VP prints any byte written here directly
// to the simulator terminal. Use this so test output appears in `make sim`
// without needing tmux/nc on the UART TCP socket.
#define STDOUT_ADDR (*(volatile uint32_t *)0x80000000)

// Send single character
static void uart_putc(char c) {
    STDOUT_ADDR = (uint32_t)c;
}

// Print string
void uart_print(const char *str) {
    if (!str) return;
    
    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');
        }
        uart_putc(*str++);
    }
}

// Print decimal number
void uart_print_dec(uint32_t value) {
    char buf[12];
    int i = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

// Print hex number with specified digits
void uart_print_hex(uint32_t value, int digits) {
    const char hex[] = "0123456789abcdef";

    uart_print("0x");
    for (int i = (digits - 1) * 4; i >= 0; i -= 4) {
        uart_putc(hex[(value >> i) & 0xF]);
    }
}
