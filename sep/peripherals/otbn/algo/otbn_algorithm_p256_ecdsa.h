// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

/**
 * @file otbn_algorithm_p256_ecdsa.h
 * @brief P256 ECDSA Signature Verification Algorithm Header
 *
 * Implements P256 ECDSA signature verification using OpenSSL.
 * Behavioral model that reads test vectors from DMEM and writes results back.
 *
 * DMEM Layout (matching otbn_p256_verify_test.c):
 *   0x500: mode (operation mode, 0x1f8 = VERIFY)
 *   0x504: ok (verification result, 0x739 = success)
 *   0x520: msg (message digest, 8 words)
 *   0x540: r (signature R component, 8 words)
 *   0x560: s (signature S component, 8 words)
 *   0x580: x (public key x-coordinate, 8 words)
 *   0x5a0: y (public key y-coordinate, 8 words)
 *   0x640: x_r (recovered x-coordinate, 8 words)
 */


#include "otbn.h"
#include <cstdint>
#include <cstddef>
#include <iostream>

/**
 * @brief P256 ECDSA Signature Verification Algorithm
 */
class otbn_algorithm_p256_ecdsa : public otbn_algorithm {
private:
    uint64_t instruction_count;

    // DMEM offsets (from otbn_p256_verify_test.c)
    static constexpr uint32_t MODE_OFFSET  = 0x500;
    static constexpr uint32_t OK_OFFSET    = 0x504;
    static constexpr uint32_t MSG_OFFSET   = 0x520;
    static constexpr uint32_t R_OFFSET     = 0x540;
    static constexpr uint32_t S_OFFSET     = 0x560;
    static constexpr uint32_t X_OFFSET     = 0x580;
    static constexpr uint32_t Y_OFFSET     = 0x5a0;
    static constexpr uint32_t X_R_OFFSET   = 0x640;

    // P256 parameters
    static constexpr int P256_WORDS = 8;  // 256 bits = 8 x 32-bit words

    // OpenTitan hardened boolean encoding
    static constexpr uint32_t HARDENED_BOOL_TRUE = 0x00000739;
    static constexpr uint32_t HARDENED_BOOL_FALSE = 0x00001d4e;

    /**
     * Read 256-bit value from DMEM (little-endian words) and convert to big-endian bytes
     */
    void read_p256_value(const char* dmem, uint32_t offset, unsigned char* output);

    /**
     * Write 256-bit value to DMEM (convert from big-endian bytes to little-endian words)
     */
    void write_p256_value(char* dmem, uint32_t offset, const unsigned char* input);

public:
    otbn_algorithm_p256_ecdsa(size_t dmem_size);

    otbn_algorithm::status_t execute(char* dmem) override;
    uint64_t get_instruction_count() override;
    uint64_t get_cycle_count() override;
    void reset() override;
    void message_objects(std::ostream &debug, std::ostream &info) override;
};

