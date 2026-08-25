// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

/**
 * @file otbn_algorithm_rsa_2048.h
 * @brief RSA-2048 Algorithm Implementation Header
 *
 * Implements RSA-2048 modular exponentiation using OpenSSL's BIGNUM library.
 * Performs: result = base^exponent mod modulus
 *
 * DMEM Layout Convention:
 *   0x000-0x0FF (256 bytes): base/message (input for encryption/signing)
 *   0x100-0x1FF (256 bytes): exponent (public/private key exponent)
 *   0x200-0x2FF (256 bytes): modulus (RSA modulus N)
 *   0x300-0x3FF (256 bytes): result (output - encrypted/signed data)
 *
 * All multi-byte values are in big-endian format per OTBN specification.
 */


#include "otbn.h"
#include <cstdint>
#include <cstddef>
#include <iostream>

/**
 * @brief RSA-2048 Algorithm Implementation
 */
class otbn_algorithm_rsa_2048 : public otbn_algorithm {
private:
    uint64_t instruction_count;  // Estimated instruction count for RSA-2048

public:
    /**
     * Constructor
     * @param dmem_size Size of DMEM in bytes (must be >= 1024 for RSA-2048)
     */
    otbn_algorithm_rsa_2048(size_t dmem_size);

    otbn_algorithm::status_t execute(char* dmem) override;
    uint64_t get_instruction_count() override;
    uint64_t get_cycle_count() override;
    void reset() override;
    void message_objects(std::ostream &debug, std::ostream &info) override;
};

