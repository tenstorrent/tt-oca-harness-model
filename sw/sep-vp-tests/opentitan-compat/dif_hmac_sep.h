// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

// Copyright 2025 - SEP Platform OpenTitan Compatibility Layer
// HMAC Device Interface Functions (DIF) - SEP Platform Implementation


#include "base_compat.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// HMAC Base Address (SEP platform mapping; see vp/src/platform/sep/Args.hpp)
// Registers: 0x10911000-0x10911FFF, MSG_FIFO window: 0x10912000-0x10912FFF
#define HMAC_BASE_ADDR 0x10911000

// HMAC Register Offsets
#define HMAC_INTR_STATE_REG_OFFSET         0x00
#define HMAC_INTR_ENABLE_REG_OFFSET        0x04
#define HMAC_INTR_TEST_REG_OFFSET          0x08
#define HMAC_ALERT_TEST_REG_OFFSET         0x0C
#define HMAC_CFG_REG_OFFSET                0x10
#define HMAC_CMD_REG_OFFSET                0x14
#define HMAC_STATUS_REG_OFFSET             0x18
#define HMAC_ERR_CODE_REG_OFFSET           0x1C
#define HMAC_WIPE_SECRET_REG_OFFSET        0x20
#define HMAC_KEY_0_REG_OFFSET              0x24
#define HMAC_KEY_1_REG_OFFSET              0x28
#define HMAC_KEY_2_REG_OFFSET              0x2C
#define HMAC_KEY_3_REG_OFFSET              0x30
#define HMAC_KEY_4_REG_OFFSET              0x34
#define HMAC_KEY_5_REG_OFFSET              0x38
#define HMAC_KEY_6_REG_OFFSET              0x3C
#define HMAC_KEY_7_REG_OFFSET              0x40
#define HMAC_DIGEST_0_REG_OFFSET           0xA4
#define HMAC_MSG_LENGTH_LOWER_REG_OFFSET   0xE4
#define HMAC_MSG_LENGTH_UPPER_REG_OFFSET   0xE8
#define HMAC_MSG_FIFO_REG_OFFSET           0x1000

// CFG register bit fields
#define HMAC_CFG_HMAC_EN_BIT               0
#define HMAC_CFG_SHA_EN_BIT                1
#define HMAC_CFG_ENDIAN_SWAP_BIT           2
#define HMAC_CFG_DIGEST_SWAP_BIT           3
#define HMAC_CFG_DIGEST_SIZE_OFFSET        8
#define HMAC_CFG_KEY_LENGTH_OFFSET         16

// CMD register values
#define HMAC_CMD_HASH_START                0x01
#define HMAC_CMD_HASH_PROCESS              0x02

// STATUS register bit fields (SEP Platform - different from OpenTitan!)
#define HMAC_STATUS_HMAC_IDLE_BIT      0  // SEP specific
#define HMAC_STATUS_FIFO_EMPTY_BIT     1  // SEP: bit 1, OpenTitan: bit 0
#define HMAC_STATUS_FIFO_FULL_BIT      2  // SEP: bit 2, OpenTitan: bit 1
#define HMAC_STATUS_FIFO_DEPTH_OFFSET  4  // Same in both

// INTR_STATE register bit fields
#define HMAC_INTR_HMAC_DONE_BIT            0
#define HMAC_INTR_FIFO_EMPTY_BIT           1
#define HMAC_INTR_HMAC_ERR_BIT             2

/**
 * Supported HMAC modes of operation
 */
typedef enum dif_hmac_mode {
    kDifHmacModeHmac = 0,
    kDifHmacModeSha256,
} dif_hmac_mode_t;

/**
 * Supported byte endianness options
 */
typedef enum dif_hmac_endianness {
    kDifHmacEndiannessBig = 0,
    kDifHmacEndiannessLittle,
} dif_hmac_endianness_t;

/**
 * HMAC key length in bits
 */
typedef enum dif_hmac_key_length {
    kDifHMACKey128 = 1,
    kDifHMACKey256 = (1 << 1),
    kDifHMACKey384 = (1 << 2),
    kDifHMACKey512 = (1 << 3),
    kDifHMACKey1024 = (1 << 4),
} dif_hmac_key_length_t;

/**
 * SHA-2 digest size
 */
typedef enum dif_sha2_digest_size {
    kDifSHA256 = (1 << 1),
    kDifSHA384 = (1 << 2),
    kDifSHA512 = (1 << 3),
} dif_sha2_digest_size_t;

/**
 * Configuration for a single HMAC transaction
 */
typedef struct dif_hmac_transaction {
    dif_hmac_endianness_t message_endianness;
    dif_hmac_endianness_t digest_endianness;
    dif_sha2_digest_size_t digest_size;
    dif_hmac_key_length_t key_length;
} dif_hmac_transaction_t;

/**
 * A typed representation of the HMAC digest
 */
typedef struct dif_hmac_digest {
    uint32_t digest[8];
} dif_hmac_digest_t;

/**
 * HMAC device handle
 */
typedef struct dif_hmac {
    mmio_region_t base_addr;
} dif_hmac_t;

/**
 * Device tree HMAC type (simplified for SEP platform)
 */
typedef uint32_t dt_hmac_t;

/**
 * Initialize HMAC from device tree index
 * For SEP platform, we only have one HMAC instance
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_init_from_dt(dt_hmac_t dt_index, dif_hmac_t *hmac);

/**
 * Start HMAC operation in HMAC mode
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_mode_hmac_start(const dif_hmac_t *hmac,
                                      const uint8_t *key,
                                      const dif_hmac_transaction_t config);

/**
 * Start HMAC operation in SHA256-only mode
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_mode_sha256_start(const dif_hmac_t *hmac,
                                        const dif_hmac_transaction_t config);

/**
 * Push data to HMAC FIFO
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_fifo_push(const dif_hmac_t *hmac, const void *data,
                                size_t len, size_t *bytes_sent);

/**
 * Get number of entries in HMAC FIFO
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_fifo_count_entries(const dif_hmac_t *hmac,
                                         uint32_t *num_entries);

/**
 * Get message length in bits
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_get_message_length(const dif_hmac_t *hmac,
                                         uint64_t *msg_len);

/**
 * Trigger HMAC processing
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_process(const dif_hmac_t *hmac);

/**
 * Finish HMAC operation and read digest
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_finish(const dif_hmac_t *hmac, bool disable_after_done,
                             dif_hmac_digest_t *digest);

/**
 * Wipe HMAC secret state
 */
OT_WARN_UNUSED_RESULT
dif_result_t dif_hmac_wipe_secret(const dif_hmac_t *hmac, uint32_t entropy,
                                  dif_hmac_digest_t *digest);

#ifdef __cplusplus
}
#endif

