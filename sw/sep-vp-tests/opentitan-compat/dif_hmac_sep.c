// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// Copyright 2025 - SEP Platform OpenTitan Compatibility Layer
// HMAC Device Interface Functions (DIF) - Implementation

#include "dif_hmac_sep.h"
#include "base_compat.h"

// Helper function to check if FIFO is full
static bool hmac_fifo_full(const dif_hmac_t *hmac) {
    uint32_t status = mmio_region_read32(hmac->base_addr, HMAC_STATUS_REG_OFFSET);
    return (status >> HMAC_STATUS_FIFO_FULL_BIT) & 0x1;
}

// Helper function to check if HMAC is done
static bool hmac_done(const dif_hmac_t *hmac) {
    uint32_t intr_state = mmio_region_read32(hmac->base_addr, HMAC_INTR_STATE_REG_OFFSET);
    return (intr_state >> HMAC_INTR_HMAC_DONE_BIT) & 0x1;
}

dif_result_t dif_hmac_init_from_dt(dt_hmac_t dt_index, dif_hmac_t *hmac) {
    if (hmac == NULL) {
        return kDifBadArg;
    }
    
    // For SEP platform, we only have one HMAC instance at fixed address
    hmac->base_addr = mmio_region_from_addr(HMAC_BASE_ADDR);
    
    return kDifOk;
}

dif_result_t dif_hmac_mode_hmac_start(const dif_hmac_t *hmac,
                                      const uint8_t *key,
                                      const dif_hmac_transaction_t config) {
    if (hmac == NULL) {
        return kDifBadArg;
    }
    
    // Load the key if provided (256-bit key = 8 x 32-bit words)
    if (key != NULL) {
        const uint32_t *key_words = (const uint32_t *)key;
        for (int i = 0; i < 8; i++) {
            mmio_region_write32(hmac->base_addr, 
                              HMAC_KEY_0_REG_OFFSET + (i * 4),
                              key_words[i]);
        }
    }
    
    // Configure HMAC
    uint32_t cfg = 0;
    cfg |= (1 << HMAC_CFG_HMAC_EN_BIT);  // Enable HMAC mode
    cfg |= (1 << HMAC_CFG_SHA_EN_BIT);   // Enable SHA engine
    
    // Set digest size to SHA-256 (0x1 in bits [8:5])
    cfg |= (0x1 << 5);
    
    // Set key length to 256-bit (0x2 in bits [13:9])
    cfg |= (0x2 << 9);
    
    // Set endianness
    if (config.message_endianness == kDifHmacEndiannessLittle) {
        cfg |= (1 << HMAC_CFG_ENDIAN_SWAP_BIT);
    }
    if (config.digest_endianness == kDifHmacEndiannessLittle) {
        cfg |= (1 << HMAC_CFG_DIGEST_SWAP_BIT);
    }
    
    mmio_region_write32(hmac->base_addr, HMAC_CFG_REG_OFFSET, cfg);
    
    // Start HMAC operation
    mmio_region_write32(hmac->base_addr, HMAC_CMD_REG_OFFSET, HMAC_CMD_HASH_START);
    
    return kDifOk;
}

dif_result_t dif_hmac_mode_sha256_start(const dif_hmac_t *hmac,
                                        const dif_hmac_transaction_t config) {
    if (hmac == NULL) {
        return kDifBadArg;
    }
    
    // Configure SHA-256 only mode
    uint32_t cfg = 0;
    cfg |= (1 << HMAC_CFG_SHA_EN_BIT);   // Enable SHA engine
    // HMAC_EN bit is 0 for SHA-256 only mode
    
    // Set digest size to SHA-256 (0x1 in bits [8:5])
    cfg |= (0x1 << 5);
    
    // Set endianness
    if (config.message_endianness == kDifHmacEndiannessLittle) {
        cfg |= (1 << HMAC_CFG_ENDIAN_SWAP_BIT);
    }
    if (config.digest_endianness == kDifHmacEndiannessLittle) {
        cfg |= (1 << HMAC_CFG_DIGEST_SWAP_BIT);
    }
    
    mmio_region_write32(hmac->base_addr, HMAC_CFG_REG_OFFSET, cfg);
    
    // Start SHA operation
    mmio_region_write32(hmac->base_addr, HMAC_CMD_REG_OFFSET, HMAC_CMD_HASH_START);
    
    return kDifOk;
}

dif_result_t dif_hmac_fifo_push(const dif_hmac_t *hmac, const void *data,
                                size_t len, size_t *bytes_sent) {
    if (hmac == NULL || data == NULL) {
        return kDifBadArg;
    }
    
    const uint8_t *data_bytes = (const uint8_t *)data;
    size_t sent = 0;
    
    // Push data to FIFO in 32-bit words
    while (sent < len) {
        // Check if FIFO is full
        if (hmac_fifo_full(hmac)) {
            if (bytes_sent != NULL) {
                *bytes_sent = sent;
            }
            return kDifUnavailable;  // FIFO full
        }
        
        // Collect up to 4 bytes for a 32-bit word
        uint32_t word = 0;
        int bytes_in_word = 0;
        
        for (int i = 0; i < 4 && sent < len; i++) {
            word |= ((uint32_t)data_bytes[sent]) << (i * 8);
            sent++;
            bytes_in_word++;
        }
        
        // Write to FIFO
        mmio_region_write32(hmac->base_addr, HMAC_MSG_FIFO_REG_OFFSET, word);
    }
    
    if (bytes_sent != NULL) {
        *bytes_sent = sent;
    }
    
    return kDifOk;
}

dif_result_t dif_hmac_fifo_count_entries(const dif_hmac_t *hmac,
                                         uint32_t *num_entries) {
    if (hmac == NULL || num_entries == NULL) {
        return kDifBadArg;
    }
    
    uint32_t status = mmio_region_read32(hmac->base_addr, HMAC_STATUS_REG_OFFSET);
    *num_entries = (status >> HMAC_STATUS_FIFO_DEPTH_OFFSET) & 0x1F;
    
    return kDifOk;
}

dif_result_t dif_hmac_get_message_length(const dif_hmac_t *hmac,
                                         uint64_t *msg_len) {
    if (hmac == NULL || msg_len == NULL) {
        return kDifBadArg;
    }
    
    extern void uart_print(const char *str);
    extern void uart_print_hex(uint32_t value, int digits);
    
    uint32_t lower = mmio_region_read32(hmac->base_addr, HMAC_MSG_LENGTH_LOWER_REG_OFFSET);
    uint32_t upper = mmio_region_read32(hmac->base_addr, HMAC_MSG_LENGTH_UPPER_REG_OFFSET);
    
    *msg_len = ((uint64_t)upper << 32) | lower;
    
    uart_print("[DIF] Message length read: ");
    uart_print_hex((uint32_t)*msg_len, 8);
    uart_print(" bits\n");
    
    return kDifOk;
}

dif_result_t dif_hmac_process(const dif_hmac_t *hmac) {
    if (hmac == NULL) {
        return kDifBadArg;
    }
    
    // Trigger hash processing
    mmio_region_write32(hmac->base_addr, HMAC_CMD_REG_OFFSET, HMAC_CMD_HASH_PROCESS);
    
    return kDifOk;
}

dif_result_t dif_hmac_finish(const dif_hmac_t *hmac, bool disable_after_done,
                             dif_hmac_digest_t *digest) {
    if (hmac == NULL || digest == NULL) {
        return kDifBadArg;
    }
    
    // Check if HMAC is done
    if (!hmac_done(hmac)) {
        return kDifUnavailable;  // Still processing
    }
    
    // Read the digest (8 x 32-bit words = 256 bits)
    for (int i = 0; i < 8; i++) {
        digest->digest[i] = mmio_region_read32(hmac->base_addr,
                                               HMAC_DIGEST_0_REG_OFFSET + (i * 4));
    }
    
    // Clear the interrupt state
    mmio_region_write32(hmac->base_addr, HMAC_INTR_STATE_REG_OFFSET,
                       (1 << HMAC_INTR_HMAC_DONE_BIT));
    
    // Optionally disable HMAC after reading digest
    if (disable_after_done) {
        mmio_region_write32(hmac->base_addr, HMAC_CFG_REG_OFFSET, 0);
    }
    
    return kDifOk;
}

dif_result_t dif_hmac_wipe_secret(const dif_hmac_t *hmac, uint32_t entropy,
                                  dif_hmac_digest_t *digest) {
    if (hmac == NULL || digest == NULL) {
        return kDifBadArg;
    }
    
    // Write entropy to wipe secret register
    mmio_region_write32(hmac->base_addr, HMAC_WIPE_SECRET_REG_OFFSET, entropy);
    
    // Read resulting digest
    for (int i = 0; i < 8; i++) {
        digest->digest[i] = mmio_region_read32(hmac->base_addr,
                                               HMAC_DIGEST_0_REG_OFFSET + (i * 4));
    }
    
    return kDifOk;
}
