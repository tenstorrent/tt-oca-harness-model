// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * Simple printf implementation for bare-metal RISC-V
 * Supports only the formats used in hmac_test: %s, %u, %08x
 */

#include <stdint.h>
#include <stdarg.h>

// Stdout device address (direct console output)
#define STDOUT_ADDR (*(volatile uint32_t *)0x80000000)

// Send single character to stdout
static void putchar(char c) {
    STDOUT_ADDR = (uint32_t)c;
}

// Print string
static void puts(const char *str) {
    while (*str) {
        if (*str == '\n') {
            putchar('\r');
        }
        putchar(*str++);
    }
}

// Print unsigned decimal
static void print_dec(uint32_t value) {
    char buf[12];
    int i = 0;

    if (value == 0) {
        putchar('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        putchar(buf[--i]);
    }
}

// Print hex with specified digits
static void print_hex(uint32_t value, int digits) {
    const char hex[] = "0123456789abcdef";

    for (int i = (digits - 1) * 4; i >= 0; i -= 4) {
        putchar(hex[(value >> i) & 0xF]);
    }
}

// Simple printf implementation
int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    const char *p = format;
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == 's') {
                // String
                const char *str = va_arg(args, const char *);
                puts(str);
            } else if (*p == 'u') {
                // Unsigned decimal
                uint32_t val = va_arg(args, uint32_t);
                print_dec(val);
            } else if (*p == '0') {
                int digits = 0;
                const char *q = p + 1;
                while (*q >= '0' && *q <= '9') {
                    digits = digits * 10 + (*q - '0');
                    q++;
                }
                if (*q == 'x' && digits > 0) {
                    uint32_t val = va_arg(args, uint32_t);
                    print_hex(val, digits);
                    p = q; // points to 'x'; p++ below advances past it
                }
            } else if (*p == 'x') {
                // Hex without leading zeros
                uint32_t val = va_arg(args, uint32_t);
                print_hex(val, 8);
            } else if (*p == '%') {
                putchar('%');
            }
            p++;
        } else {
            if (*p == '\n') {
                putchar('\r');
            }
            putchar(*p++);
        }
    }

    va_end(args);
    return 0;
}

// strcmp implementation
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}
