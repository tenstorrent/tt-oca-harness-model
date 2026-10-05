// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*******************************************************************************
 * Common helpers for SEP eFuse firmware tests.
 ******************************************************************************/

#ifndef EFUSE_FW_TEST_COMMON_H
#define EFUSE_FW_TEST_COMMON_H

#include <stdint.h>
#include <stdio.h>

#include "och_sep_common.h"
#include "och_sep_top_reg.h"

#define EFUSE_FW_POLL_TIMEOUT 1000000

#define EFUSE_TOKEN_MATCH    0x15u
#define EFUSE_TOKEN_MISMATCH 0x2Au
#define EFUSE_TOKEN_ERROR    0x3Fu

#define EFUSE_FW_DENY_WORD 0xBADCAB1Eu

#define EFUSE_FW_FIELD_CLASS_KEY       7u
#define EFUSE_FW_FIELD_CHIPLET_UID     11u
#define EFUSE_FW_WRITE_LOCK_BIT(field) ((field) * 2u)
#define EFUSE_FW_READ_LOCK_BIT(field)  (((field) * 2u) + 1u)

/*
 * OTP bit offset of an eFuse field: the register header gives each field a byte
 * offset in the eFuse MAP window, and the array is addressed by bit. Literals are
 * not used: these offsets are cumulative, and a stale literal does not fail to compile.
 */
#define EFUSE_FW_BIT0(field_offset) ((uint32_t)(field_offset) * 8u)

#define EFUSE_FW_CLASS_KEY_BIT0   EFUSE_FW_BIT0(SEP_EFUSE_MAP_CLASS_KEY_REG_OFFSET)
#define EFUSE_FW_BL1_VERSION_BIT0 EFUSE_FW_BIT0(SEP_EFUSE_MAP_BL1_VERSION_REG_OFFSET)
#define EFUSE_FW_CHIPLET_UID_BIT0 EFUSE_FW_BIT0(SEP_EFUSE_MAP_CHIPLET_UID_REG_OFFSET)

typedef struct {
    uint32_t completed;
    uint32_t read_status;
    uint32_t req_error;
    uint32_t data_word;
    uint32_t bit_value;
} efuse_fw_read_result_t;

static inline int efuse_wait_sense_done(void)
{
    for (int i = 0; i < EFUSE_FW_POLL_TIMEOUT; i++) {
        uint32_t status = READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_INTERFACE_CTRL_STATUS_REG_ADDR);
        if ((status & EFUSE_INTERFACE_CTRL_EFUSE_INTERFACE_CTRL_STATUS_EFUSE_SENSE_DONE_MASK) != 0) {
            return 0;
        }
    }
    printf("ERROR: timed out waiting for eFuse sense done\n");
    return -1;
}

static inline void efuse_clear_req_error(void)
{
    WRITE_REG(EFUSE_INTERFACE_CTRL_EFUSE_INTERFACE_CTRL_STATUS_REG_ADDR,
              EFUSE_INTERFACE_CTRL_EFUSE_INTERFACE_CTRL_STATUS_EFUSE_REQ_ERROR_CLEAR_MASK);
}

static inline int efuse_config_program_clock(uint32_t divider)
{
    uint32_t cfg = 0;
    cfg |= (divider & 0xffu) << EFUSE_SHIM_CTRL_SAMSUNG_EFUSE_CTRL_STATUS_CLOCK_DIVIDER_VALUE_SHIFT;
    cfg |= 0x80u << EFUSE_SHIM_CTRL_SAMSUNG_EFUSE_CTRL_STATUS_CLOCK_DUTYCYCLE_SHIFT;
    cfg |= EFUSE_SHIM_CTRL_SAMSUNG_EFUSE_CTRL_STATUS_CLOCK_DIV_ENABLE_MASK;

    WRITE_REG(SEP_EXTERNAL_EFUSE_SHIM_CTRL_EFUSE_CTRL_STATUS_REG_ADDR, cfg);
    WRITE_REG(SEP_EXTERNAL_EFUSE_SHIM_CTRL_EFUSE_CTRL_STATUS_REG_ADDR,
              cfg | EFUSE_SHIM_CTRL_SAMSUNG_EFUSE_CTRL_STATUS_CLOCK_DIV_SET_MASK);

    uint32_t rb = READ_REG(SEP_EXTERNAL_EFUSE_SHIM_CTRL_EFUSE_CTRL_STATUS_REG_ADDR);
    if ((rb & EFUSE_SHIM_CTRL_SAMSUNG_EFUSE_CTRL_STATUS_CLOCK_DIVIDER_VALUE_MASK) !=
        (divider & EFUSE_SHIM_CTRL_SAMSUNG_EFUSE_CTRL_STATUS_CLOCK_DIVIDER_VALUE_MASK)) {
        printf("ERROR: eFuse clock divider mismatch wrote=0x%02x read=0x%02x\n",
               divider & 0xffu, rb & 0xffu);
        return -1;
    }
    return 0;
}

static inline int efuse_program_bit(uint32_t bit_addr)
{
    uint32_t ctrl = 0;
    ctrl |= (bit_addr << EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_ADDR_SHIFT) &
            EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_ADDR_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_DATA_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_PROGRAM_GO_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_PROGRAM_READ_BACK_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_ENABLE_MASK;

    WRITE_REG(EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_REG_ADDR, ctrl);

    uint32_t rb = 0;
    for (int i = 0; i < EFUSE_FW_POLL_TIMEOUT; i++) {
        rb = READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_REG_ADDR);
        if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_DONE_MASK) != 0) {
            break;
        }
    }

    WRITE_REG(EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_REG_ADDR, 0);

    if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_DONE_MASK) == 0) {
        printf("ERROR: OTP program timeout bit=%u\n", bit_addr);
        return -1;
    }
    if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_STATUS_MASK) != 0) {
        printf("ERROR: OTP program status error bit=%u ctrl=0x%08x\n", bit_addr, rb);
        return -1;
    }
    return 0;
}

static inline int efuse_program_bit_expect(uint32_t bit_addr, uint32_t expect_success)
{
    uint32_t ctrl = 0;
    uint32_t rb = 0;

    ctrl |= (bit_addr << EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_ADDR_SHIFT) &
            EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_ADDR_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_DATA_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_PROGRAM_GO_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_EFUSE_PROGRAM_READ_BACK_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_ENABLE_MASK;

    WRITE_REG(EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_REG_ADDR, ctrl);

    for (int i = 0; i < EFUSE_FW_POLL_TIMEOUT; i++) {
        rb = READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_REG_ADDR);
        if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_DONE_MASK) != 0) {
            break;
        }
    }

    WRITE_REG(EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_REG_ADDR, 0);

    if (expect_success) {
        if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_DONE_MASK) == 0) {
            printf("ERROR: OTP program timeout bit=%u\n", bit_addr);
            return -1;
        }
        if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_STATUS_MASK) != 0) {
            printf("ERROR: OTP program status error bit=%u ctrl=0x%08x\n", bit_addr, rb);
            return -1;
        }
        return 0;
    }

    if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_DONE_MASK) == 0) {
        printf("OTP program correctly did not complete bit=%u\n", bit_addr);
        efuse_clear_req_error();
        return 0;
    }
    if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_PROGRAM_CTRL_PROGRAM_STATUS_MASK) == 0) {
        printf("ERROR: OTP program unexpectedly succeeded bit=%u\n", bit_addr);
        return -1;
    }

    printf("OTP program correctly denied bit=%u\n", bit_addr);
    efuse_clear_req_error();
    return 0;
}

static inline int efuse_read_word_raw(uint32_t bit_addr, efuse_fw_read_result_t *result)
{
    uint32_t ctrl = 0;
    uint32_t rb = 0;

    result->completed = 0;
    result->read_status = 0;
    result->req_error = 0;
    result->data_word = 0;
    result->bit_value = 0;

    ctrl |= (bit_addr << EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_EFUSE_ADDR_SHIFT) &
            EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_EFUSE_ADDR_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_EFUSE_READ_GO_MASK;
    ctrl |= EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_READ_ENABLE_MASK;

    WRITE_REG(EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR, ctrl);
    WRITE_REG(EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR,
              ctrl & ~EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_EFUSE_READ_GO_MASK);

    for (int i = 0; i < EFUSE_FW_POLL_TIMEOUT; i++) {
        rb = READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_REG_ADDR);
        if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_READ_DONE_MASK) != 0) {
            break;
        }
    }

    if ((rb & EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_READ_DONE_MASK) == 0) {
        printf("ERROR: OTP read timeout bit=%u\n", bit_addr);
        return -1;
    }

    result->completed = 1;
    result->read_status =
        (rb & EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_READ_STATUS_MASK) >>
        EFUSE_INTERFACE_CTRL_EFUSE_READ_CTRL_READ_STATUS_SHIFT;
    result->data_word = READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_READ_INTERFACE_READ_DATA_REG_ADDR);
    result->bit_value = (result->data_word >> (bit_addr & 31u)) & 1u;
    result->req_error =
        (READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_INTERFACE_CTRL_STATUS_REG_ADDR) &
         EFUSE_INTERFACE_CTRL_EFUSE_INTERFACE_CTRL_STATUS_EFUSE_REQ_ERROR_MASK) >>
        EFUSE_INTERFACE_CTRL_EFUSE_INTERFACE_CTRL_STATUS_EFUSE_REQ_ERROR_SHIFT;

    return 0;
}

static inline int efuse_read_word(uint32_t bit_addr, uint32_t *data_word)
{
    efuse_fw_read_result_t result;

    if (efuse_read_word_raw(bit_addr, &result) != 0) {
        return -1;
    }
    if (result.read_status != 0 || result.req_error != 0) {
        printf("ERROR: OTP read status error bit=%u read_status=%u req_error=%u data=0x%08x\n",
               bit_addr, result.read_status, result.req_error, result.data_word);
        return -1;
    }

    *data_word = result.data_word;
    return 0;
}

static inline int efuse_read_bit(uint32_t bit_addr, uint32_t *bit_value)
{
    uint32_t data_word = 0;
    if (efuse_read_word(bit_addr, &data_word) != 0) {
        return -1;
    }
    *bit_value = (data_word >> (bit_addr & 31u)) & 1u;
    return 0;
}

static inline void efuse_write_8_words(uint32_t base_addr, const uint32_t words[8])
{
    for (int i = 0; i < 8; i++) {
        WRITE_REG(base_addr + (uint32_t)(i * 4), words[i]);
    }
}

static inline int efuse_set_shadow_lock_bit(uint32_t lock_bit)
{
    if (lock_bit >= 32u) {
        printf("ERROR: helper only supports lower LOCKS word, bit=%u\n", lock_bit);
        return -1;
    }

    WRITE_REG(SEP_EFUSE_MAP_LOCKS_REG_ADDR, 1u << lock_bit);
    uint32_t locks = READ_REG(SEP_EFUSE_MAP_LOCKS_REG_ADDR);
    if ((locks & (1u << lock_bit)) == 0) {
        printf("ERROR: LOCKS bit %u did not set, locks=0x%08x\n", lock_bit, locks);
        return -1;
    }
    return 0;
}

static inline int efuse_shadow_rw32(uint32_t addr, uint32_t pattern, const char *name)
{
    uint32_t original = READ_REG(addr);
    uint32_t rb = 0;

    if (pattern == original) {
        pattern = ~pattern;
    }

    WRITE_REG(addr, pattern);
    rb = READ_REG(addr);
    if (rb != pattern) {
        printf("ERROR: %s shadow readback mismatch expected=0x%08x got=0x%08x\n",
               name, pattern, rb);
        WRITE_REG(addr, original);
        return -1;
    }

    WRITE_REG(addr, original);
    rb = READ_REG(addr);
    if (rb != original) {
        printf("ERROR: %s shadow restore mismatch expected=0x%08x got=0x%08x\n",
               name, original, rb);
        return -1;
    }

    return 0;
}

static inline void efuse_fw_delay(unsigned int iterations)
{
    for (volatile unsigned int i = 0; i < iterations; i++) {
        __asm__ volatile("nop");
    }
}

static inline int efuse_token_poll(uint32_t match_addr, uint32_t expected)
{
    uint32_t status = EFUSE_TOKEN_ERROR;
    for (int i = 0; i < EFUSE_FW_POLL_TIMEOUT; i++) {
        status = READ_REG(match_addr) & EFUSE_MMR_TOKEN_MATCH_TOKEN_MATCH_STATUS_MASK;
        if (status != 0) {
            break;
        }
    }

    if (status != expected) {
        printf("ERROR: token status expected=0x%02x got=0x%02x\n", expected, status);
        return -1;
    }
    return 0;
}

static inline int efuse_token_trigger_and_poll(uint32_t eop_mask, uint32_t match_addr,
                                               uint32_t expected)
{
    WRITE_REG(EFUSE_MMR_TOKEN_EOP_REG_ADDR, eop_mask);

    /*
     * The UVM sequence waits 10 us after TOKEN_EOP. A short CPU-side delay
     * prevents sampling a previous token result before SHA256/comparison reruns.
     */
    efuse_fw_delay(20000);
    return efuse_token_poll(match_addr, expected);
}

#endif // EFUSE_FW_TEST_COMMON_H
