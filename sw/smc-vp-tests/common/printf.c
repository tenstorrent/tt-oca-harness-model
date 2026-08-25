/* SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
 * sw/smc-vp-tests/common/printf.c
 *
 * Minimal bare-metal printf for smc-vp firmware tests.  Output goes to
 * UART0 (0xC0006100) THR with LSR THRE polling.  UART0 is initialized in
 * start.S before main() is called.
 *
 * Supported specifiers (mirrors sw/sep-vp-tests/common/printf.c):
 *   %s   string
 *   %u   unsigned decimal
 *   %x   hex (8 digits, no prefix)
 *   %0Nx hex with N digits (e.g. %08x)
 *   %%   literal '%'
 *
 * NOTE: %d is NOT supported — use %u for unsigned, %x for hex.
 */
#include "smc_common.h"

static void uart_putc(char c)
{
    /* Poll LSR THRE until the TX holding register can accept a byte. */
    while ((REG_READ(SMC_UART0_BASE + UART_LSR) & UART_LSR_THRE) == 0) { }
    REG_WRITE(SMC_UART0_BASE + UART_RBR_THR_DLL, (uint32_t)(unsigned char)c);
}

static void putstr(const char *s)
{
    while (*s) uart_putc(*s++);
}

static void putu(uint32_t v, unsigned base, unsigned width)
{
    char buf[32];
    int i = 0;
    if (v == 0) {
        buf[i++] = '0';
    } else {
        while (v && i < (int)sizeof(buf)) {
            unsigned d = v % base;
            buf[i++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
            v /= base;
        }
    }
    while (i < (int)width) buf[i++] = '0';   /* zero-pad to width */
    while (i) uart_putc(buf[--i]);
}

int printf(const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { uart_putc(*p); continue; }
        p++;
        unsigned width = 0;
        if (*p == '0') {          /* %0Nx form */
            p++;
            while (*p >= '0' && *p <= '9') width = width * 10 + (unsigned)(*p - '0'), p++;
        }
        switch (*p) {
        case 's': putstr(__builtin_va_arg(ap, const char *)); break;
        case 'u': putu(__builtin_va_arg(ap, uint32_t), 10, width); break;
        case 'x': putu(__builtin_va_arg(ap, uint32_t), 16, width ? width : 8); break;
        case '%': uart_putc('%'); break;
        case '\0': p--; break;    /* trailing '%' */
        default:
            uart_putc('%'); uart_putc(*p); break;
        }
    }
    __builtin_va_end(ap);
    return 0;
}
