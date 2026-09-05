// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn_algorithm_rsa_2048_key_enabled.cpp
 * @brief RSA-2048 Algorithm Implementation with Key Enabled
 */

#include "otbn_algorithm_rsa_2048_key_enabled.h"
#include <openssl/bn.h>
#include <openssl/err.h>
#include <cstring>

otbn_algorithm_rsa_2048_key_enabled::otbn_algorithm_rsa_2048_key_enabled(size_t dmem_size)
    : otbn_algorithm(dmem_size, true), instruction_count(18889021) {}

otbn_algorithm::status_t otbn_algorithm_rsa_2048_key_enabled::execute(char* dmem) {
    REG_INFO(1, logger) << "[OTBN RSA-2048] Starting execution";
    
    // Check if key is required and validate key registration
    if (m_is_key_required) {
        REG_INFO(1, logger) << "[OTBN RSA-2048] Key required, checking key registration status";
        
        // Check if keys are registered
        if (!m_key_status_cb()) {
            REG_ERROR(0, logger) << "[OTBN RSA-2048] ERROR: Keys not registered (KEY_INVALID)";
            
            // Set KEY_INVALID error bit
            if (m_err_bits_write_cb) {
                m_err_bits_write_cb(0x20);  // KEY_INVALID = bit 5 = 0x20
            }
            
            return otbn_algorithm::ERROR;
        }
    }
    // Verify DMEM size is sufficient for RSA-2048 operations
    if (m_dmem_size < 1024) {
        REG_ERROR(0, logger) << "[OTBN RSA-2048] ERROR: Insufficient DMEM size (" << m_dmem_size << " bytes, need >= 1024)";
        return ERROR;
    }

    // Cast to uint8_t* for byte-level operations
    uint8_t* dmem_bytes = reinterpret_cast<uint8_t*>(dmem);

    // Create OpenSSL BIGNUM context
    BN_CTX* ctx = BN_CTX_new();
    if (!ctx) {
        REG_ERROR(0, logger) << "[OTBN RSA-2048] ERROR: Failed to create BN_CTX";
        return ERROR;
    }

    BIGNUM* base = BN_new();
    BIGNUM* exponent = BN_new();
    BIGNUM* modulus = BN_new();
    BIGNUM* result = BN_new();

    if (!base || !exponent || !modulus || !result) {
        REG_ERROR(0, logger) << "[OTBN RSA-2048] ERROR: Failed to allocate BIGNUMs";
        BN_free(base); BN_free(exponent); BN_free(modulus); BN_free(result);
        BN_CTX_free(ctx);
        return ERROR;
    }

    // Read inputs from DMEM (big-endian format per OTBN spec)
    // base: DMEM[0x000-0x0FF]
    // exponent: DMEM[0x100-0x1FF]
    // modulus: DMEM[0x200-0x2FF]
    BN_bin2bn(&dmem_bytes[0x000], 256, base);
    BN_bin2bn(&dmem_bytes[0x100], 256, exponent);
    BN_bin2bn(&dmem_bytes[0x200], 256, modulus);

    // Validate inputs
    if (BN_is_zero(modulus)) {
        REG_ERROR(0, logger) << "[OTBN RSA-2048] ERROR: Modulus is zero";
        BN_free(base); BN_free(exponent); BN_free(modulus); BN_free(result);
        BN_CTX_free(ctx);
        return ERROR;
    }

    // Check if base >= modulus (invalid for RSA)
    if (BN_cmp(base, modulus) >= 0) {
        REG_WARN(1, logger) << "[OTBN RSA-2048] WARNING: Base >= Modulus, reducing base";
        BN_mod(base, base, modulus, ctx);
    }

    // Perform RSA modular exponentiation: result = base^exponent mod modulus
    int bn_result = BN_mod_exp(result, base, exponent, modulus, ctx);

    if (bn_result != 1) {
        REG_ERROR(0, logger) << "[OTBN RSA-2048] ERROR: BN_mod_exp failed";
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        REG_ERROR(0, logger) << "[OTBN RSA-2048] OpenSSL Error: " << err_buf;

        BN_free(base); BN_free(exponent); BN_free(modulus); BN_free(result);
        BN_CTX_free(ctx);
        return ERROR;
    }

    // Write result back to DMEM at offset 0x300
    // Ensure result is exactly 256 bytes (pad with zeros if necessary)
    int result_len = BN_num_bytes(result);
    if (result_len > 256) {
        REG_ERROR(0, logger) << "[OTBN RSA-2048] ERROR: Result too large (" << result_len << " bytes)";
        BN_free(base); BN_free(exponent); BN_free(modulus); BN_free(result);
        BN_CTX_free(ctx);
        return ERROR;
    }

    // Zero out result area first
    memset(&dmem_bytes[0x300], 0, 256);

    // Write result (right-aligned, big-endian)
    BN_bn2bin(result, &dmem_bytes[0x300 + (256 - result_len)]);

    // Debug output
    REG_INFO(1, logger) << "[OTBN RSA-2048] Execution successful";


    // Cleanup
    BN_free(base);
    BN_free(exponent);
    BN_free(modulus);
    BN_free(result);
    BN_CTX_free(ctx);

    return otbn_algorithm::SUCCESS;
}

uint64_t otbn_algorithm_rsa_2048_key_enabled::get_instruction_count() {
    // Based on OTBN benchmarks for RSA-2048 decryption
    // This represents the approximate cycle count for the operation
    return instruction_count;
}

uint64_t otbn_algorithm_rsa_2048_key_enabled::get_cycle_count() {
    return 10;
}

void otbn_algorithm_rsa_2048_key_enabled::reset() {
    // Reset instruction count to default
    instruction_count = 18889021;
}

void otbn_algorithm_rsa_2048_key_enabled::message_objects(std::ostream &debug, std::ostream &info) {
    debug << "[OTBN RSA-2048 Key Enabled] Debug stream configured" << std::endl;
    info << "[OTBN RSA-2048 Key Enabled] Info stream configured" << std::endl;
}

