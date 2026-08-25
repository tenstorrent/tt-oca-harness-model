// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

// Copyright 2025 - SEP Platform OpenTitan Compatibility Layer
// OTTF (OpenTitan Test Framework) compatibility shims


#include "base_compat.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simple UART output function
void uart_print(const char *str);
void uart_print_hex(uint32_t value, int digits);

/**
 * LOG_INFO macro - outputs to UART
 */
#define LOG_INFO(msg, ...) \
    do { \
        uart_print("[INFO] "); \
        uart_print(msg); \
        uart_print("\n"); \
    } while (0)

/**
 * CHECK_DIF_OK - Check that a DIF function returned kDifOk
 * On error, this will print error and halt execution
 */
#define CHECK_DIF_OK(expr) \
    do { \
        dif_result_t result = (expr); \
        if (result != kDifOk) { \
            uart_print("[ERROR] CHECK_DIF_OK failed: "); \
            uart_print(#expr); \
            uart_print("\n"); \
            while (1) { /* Halt on error */ } \
        } \
    } while (0)

/**
 * CHECK_STATUS_OK - Check that a status function returned kStatusOk
 */
#define CHECK_STATUS_OK(expr) \
    do { \
        status_t result = (expr); \
        if (result != kStatusOk) { \
            uart_print("[ERROR] CHECK_STATUS_OK failed: "); \
            uart_print(#expr); \
            uart_print("\n"); \
            while (1) { /* Halt on error */ } \
        } \
    } while (0)

/**
 * OTTF_DEFINE_TEST_CONFIG - No-op for SEP platform
 * OpenTitan uses this to define test configuration
 */
#define OTTF_DEFINE_TEST_CONFIG()

/**
 * Test main wrapper
 * OpenTitan tests use test_main() which returns bool
 * We'll call this from our main() function
 */
bool test_main(void);

/**
 * Main entry point adapter
 * This allows us to use the same test_main() signature as OpenTitan
 */
static inline int ottf_main(void) {
    bool result = test_main();
    return result ? 0 : 1;
}

#ifdef __cplusplus
}
#endif

