// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

// Copyright 2025 - SEP Platform OpenTitan Compatibility Layer
// Base compatibility header for OpenTitan types and macros


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Result type for DIF functions
 */
typedef enum dif_result {
    kDifOk = 0,
    kDifError = 1,
    kDifBadArg = 2,
    kDifOutOfRange = 3,
    kDifUnavailable = 4,
    kDifLocked = 5,
} dif_result_t;

/**
 * Generic status type
 */
typedef enum status {
    kStatusOk = 0,
    kStatusError = 1,
} status_t;

/**
 * MMIO region type - simplified for SEP platform
 */
typedef struct mmio_region {
    uint32_t base;
} mmio_region_t;

/**
 * Macro for marking functions that should have their return value checked
 */
#define OT_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

/**
 * Macro for non-string literals (OpenTitan uses this for test data)
 */
#define OT_NONSTRING

/**
 * Helper macros for register access
 */
#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

/**
 * Create an MMIO region from a base address
 */
static inline mmio_region_t mmio_region_from_addr(uint32_t base) {
    mmio_region_t region = { .base = base };
    return region;
}

/**
 * Read from an MMIO region at an offset
 */
static inline uint32_t mmio_region_read32(mmio_region_t region, uint32_t offset) {
    return REG_READ(region.base + offset);
}

/**
 * Write to an MMIO region at an offset
 */
static inline void mmio_region_write32(mmio_region_t region, uint32_t offset, uint32_t value) {
    REG_WRITE(region.base + offset, value);
}

#ifdef __cplusplus
}
#endif

