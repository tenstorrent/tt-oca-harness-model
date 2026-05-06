#pragma once

/*
 * UART I/O function declarations
 */


#include <stdint.h>

void uart_print(const char *str);
void uart_print_dec(uint32_t value);
void uart_print_hex(uint32_t value, int digits);

