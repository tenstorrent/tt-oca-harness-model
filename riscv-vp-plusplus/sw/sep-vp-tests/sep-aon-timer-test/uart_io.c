/*
 * Simple stdout-based print functions
 * No newlib dependency - writes directly to SEP stdout device at 0x80000000
 */

#include <stdint.h>

// Stdout device address (direct console output, mapped in SEP VP)
#define STDOUT_ADDR (*(volatile uint32_t *)0x80000000)

// Send single character to stdout device
static void stdout_putc(char c) {
    STDOUT_ADDR = (uint32_t)c;
}

// Print string
void uart_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            stdout_putc('\r');
        }
        stdout_putc(*str++);
    }
}

// Print decimal number
void uart_print_dec(uint32_t value) {
    char buf[12];
    int i = 0;

    if (value == 0) {
        stdout_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        stdout_putc(buf[--i]);
    }
}

// Print hex number with specified digits
void uart_print_hex(uint32_t value, int digits) {
    const char hex[] = "0123456789abcdef";

    uart_print("0x");
    for (int i = (digits - 1) * 4; i >= 0; i -= 4) {
        stdout_putc(hex[(value >> i) & 0xF]);
    }
}
