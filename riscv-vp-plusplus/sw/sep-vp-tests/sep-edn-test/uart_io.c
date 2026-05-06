/*
 * Simple UART-based print functions
 * No newlib dependency - direct UART hardware access
 */

#include <stdint.h>

// UART register addresses
#define UART_BASE 0x44000000
#define UART_THR  (*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_LSR  (*(volatile uint32_t *)(UART_BASE + 0x14))
#define LSR_THRE  (1 << 5)

// Send single character (no wait - VP handles buffering)
static void uart_putc(char c) {
    UART_THR = (uint32_t)c;
    // Small delay to allow UART to process
    for (volatile int i = 0; i < 10; i++);
}

// Print string
void uart_print(const char *str) {
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

