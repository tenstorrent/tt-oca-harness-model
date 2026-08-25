// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

// Copyright 2025 - SEP Platform OpenTitan Compatibility Layer
// HMAC test utilities compatibility


#include "base_compat.h"
#include "dif_hmac_sep.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Push a message to HMAC FIFO with polling
 */
static inline status_t hmac_testutils_push_message(const dif_hmac_t *hmac,
                                                    const char *data,
                                                    size_t len) {
    size_t total_sent = 0;
    
    while (total_sent < len) {
        size_t sent = 0;
        dif_result_t result = dif_hmac_fifo_push(hmac, data + total_sent,
                                                 len - total_sent, &sent);

        // Account for any bytes that were actually written to the FIFO before
        // dif_hmac_fifo_push returned (true for both kDifOk and kDifUnavailable).
        // Without this, a FIFO-full retry re-pushes the same bytes and the HMAC
        // counts them multiple times in MSG_LENGTH.
        total_sent += sent;

        if (result == kDifOk || result == kDifUnavailable) {
            continue;
        }
        return kStatusError;
    }
    
    return kStatusOk;
}

/**
 * Poll until HMAC FIFO is empty
 * NOTE: SEP platform HMAC may not update fifo_depth correctly,
 * so we use the fifo_empty status bit instead
 */
extern void uart_print(const char *str);
extern void uart_print_hex(uint32_t value, int digits);

static inline status_t hmac_testutils_fifo_empty_polled(const dif_hmac_t *hmac) {
    uint32_t timeout = 100000;  // Timeout counter
    
    
    
    uart_print("[DEBUG] Waiting for FIFO to empty...\n");
    
    // Poll until FIFO empty bit is set or timeout
    do {
        uint32_t status = mmio_region_read32(hmac->base_addr, HMAC_STATUS_REG_OFFSET);
        uint32_t fifo_empty = (status >> HMAC_STATUS_FIFO_EMPTY_BIT) & 0x1;
        
        if (fifo_empty) {
            uart_print("[DEBUG] FIFO is empty\n");
            return kStatusOk;
        }
        
        if (--timeout == 0) {
            uart_print("[ERROR] FIFO empty timeout\n");
            uart_print("[DEBUG] STATUS = 0x");
            uart_print_hex(status, 8);
            uart_print("\n");
            return kStatusError;
        }
    } while (1);
    
    return kStatusOk;
}

/**
 * Check message length matches expected value
 */
static inline status_t hmac_testutils_check_message_length(const dif_hmac_t *hmac,
                                                            uint64_t expected_bits) {
    uint64_t actual_bits;
    
    dif_result_t result = dif_hmac_get_message_length(hmac, &actual_bits);
    uart_print("[DIF] dif_hmac_get_message_length actual result: ");
    uart_print_hex((uint32_t)(actual_bits >> 32), 8);
        uart_print("\n");    
    uart_print_hex(actual_bits & 0xFFFFFFFF, 8);
        uart_print("\n");
        
        uart_print_hex((uint32_t)(expected_bits >> 32), 8);
        uart_print("\n");
        uart_print_hex((uint32_t)(expected_bits & 0xFFFFFFFF), 8);
    if (result != kDifOk) {
        uart_print("[ERROR] dif_hmac_get_message_length failed\n");
        return kStatusError;
    }
    
    if (actual_bits != expected_bits) {
        uart_print("[ERROR] Message length mismatch: expected ");
        return kStatusError;
    }
    uart_print("[DEBUG] Message length matches expected: ");
    return kStatusOk;
}

/**
 * Finish HMAC and check digest with polling
 */
static inline status_t hmac_testutils_finish_and_check_polled(
    const dif_hmac_t *hmac,
    const dif_hmac_digest_t *expected_digest) {
    
    dif_hmac_digest_t actual_digest;
    dif_result_t result;
    uint32_t timeout = 1000000;  // Timeout counter
    
    // Poll until HMAC is done or timeout
    do {
        result = dif_hmac_finish(hmac, true, &actual_digest);
        
        if (--timeout == 0) {
            // Timeout - HMAC peripheral likely not responding
            extern void uart_print(const char *str);
            uart_print("[ERROR] HMAC finish timeout - peripheral not responding?\n");
            return kStatusError;
        }
    } while (result == kDifUnavailable);
    
    if (result != kDifOk) {
        return kStatusError;
    }
    
    // Compare digest
    for (int i = 0; i < 8; i++) {
        if (actual_digest.digest[i] != expected_digest->digest[i]) {
            extern void uart_print(const char *str);
            extern void uart_print_hex(uint32_t value, int digits);
            uart_print("[ERROR] Digest mismatch at index ");
            uart_print_hex(i, 1);
            uart_print("\n");
            return kStatusError;
        }
    }
    
    return kStatusOk;
}

#ifdef __cplusplus
}
#endif

