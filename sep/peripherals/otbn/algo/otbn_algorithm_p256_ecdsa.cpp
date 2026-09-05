// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn_algorithm_p256_ecdsa.cpp
 * @brief P256 ECDSA Signature Verification Algorithm Implementation
 */

#include "otbn_algorithm_p256_ecdsa.h"
// Suppress OpenSSL 3.0 deprecation warnings for EC API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#pragma GCC diagnostic pop

void otbn_algorithm_p256_ecdsa::read_p256_value(const char* dmem, uint32_t offset, unsigned char* output) {
    const uint32_t* dmem_words = reinterpret_cast<const uint32_t*>(dmem);
    uint32_t word_offset = offset / 4;

    // OTBN stores in little-endian word order, but each word is little-endian
    // Convert to big-endian byte array for OpenSSL
    for (int i = 0; i < P256_WORDS; i++) {
        uint32_t word = dmem_words[word_offset + i];
        // Store in big-endian: word[0] from DMEM goes to last bytes in output
        int byte_pos = (7 - i) * 4;  // Reverse word order
        output[byte_pos + 3] = (word >> 0) & 0xFF;
        output[byte_pos + 2] = (word >> 8) & 0xFF;
        output[byte_pos + 1] = (word >> 16) & 0xFF;
        output[byte_pos + 0] = (word >> 24) & 0xFF;
    }
}

void otbn_algorithm_p256_ecdsa::write_p256_value(char* dmem, uint32_t offset, const unsigned char* input) {
    uint32_t* dmem_words = reinterpret_cast<uint32_t*>(dmem);
    uint32_t word_offset = offset / 4;

    // Convert from big-endian byte array to little-endian word order
    for (int i = 0; i < P256_WORDS; i++) {
        int byte_pos = (7 - i) * 4;  // Reverse word order
        uint32_t word = ((uint32_t)input[byte_pos + 3] << 0) |
                       ((uint32_t)input[byte_pos + 2] << 8) |
                       ((uint32_t)input[byte_pos + 1] << 16) |
                       ((uint32_t)input[byte_pos + 0] << 24);
        dmem_words[word_offset + i] = word;
    }
}

otbn_algorithm_p256_ecdsa::otbn_algorithm_p256_ecdsa(size_t dmem_size)
    : otbn_algorithm(dmem_size, false), instruction_count(100000) {}

otbn_algorithm::status_t otbn_algorithm_p256_ecdsa::execute(char* dmem) {
    REG_INFO(1, logger) << "[OTBN P256 ECDSA] Starting signature verification";

    // Read inputs from DMEM
    unsigned char msg[32], r[32], s[32], pubkey_x[32], pubkey_y[32];
    read_p256_value(dmem, MSG_OFFSET, msg);
    read_p256_value(dmem, R_OFFSET, r);
    read_p256_value(dmem, S_OFFSET, s);
    read_p256_value(dmem, X_OFFSET, pubkey_x);
    read_p256_value(dmem, Y_OFFSET, pubkey_y);

    // Suppress OpenSSL 3.0 deprecation warnings for EC_KEY usage
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    // Create EC_KEY and set public key
    EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!key) {
        REG_ERROR(0, logger) << "[OTBN P256 ECDSA] Failed to create EC_KEY";
            return otbn_algorithm::ERROR;
    }

    // Create public key point from x, y coordinates
    const EC_GROUP* group = EC_KEY_get0_group(key);
    EC_POINT* pub_point = EC_POINT_new(group);
    BIGNUM* bn_x = BN_bin2bn(pubkey_x, 32, NULL);
    BIGNUM* bn_y = BN_bin2bn(pubkey_y, 32, NULL);

    if (!EC_POINT_set_affine_coordinates(group, pub_point, bn_x, bn_y, NULL)) {
        REG_ERROR(0, logger) << "[OTBN P256 ECDSA] Failed to set public key point";
        BN_free(bn_x);
        BN_free(bn_y);
        EC_POINT_free(pub_point);
        EC_KEY_free(key);
            return otbn_algorithm::ERROR;
    }

    if (!EC_KEY_set_public_key(key, pub_point)) {
        REG_ERROR(0, logger) << "[OTBN P256 ECDSA] Failed to set public key";
        BN_free(bn_x);
        BN_free(bn_y);
        EC_POINT_free(pub_point);
        EC_KEY_free(key);
            return otbn_algorithm::ERROR;
    }

    // Create ECDSA signature from r, s
    ECDSA_SIG* sig = ECDSA_SIG_new();
    BIGNUM* bn_r = BN_bin2bn(r, 32, NULL);
    BIGNUM* bn_s = BN_bin2bn(s, 32, NULL);
    ECDSA_SIG_set0(sig, bn_r, bn_s);

    // Verify signature
    int verify_result = ECDSA_do_verify(msg, 32, sig, key);

    REG_INFO(1, logger) << "[OTBN P256 ECDSA] Verification result: " << verify_result;

    // Write results to DMEM
    uint32_t* dmem_words = reinterpret_cast<uint32_t*>(dmem);

    if (verify_result == 1) {
        // Signature valid
        dmem_words[OK_OFFSET / 4] = HARDENED_BOOL_TRUE;
        // Write recovered x_r (should equal r)
        write_p256_value(dmem, X_R_OFFSET, r);
        REG_INFO(1, logger) << "[OTBN P256 ECDSA] Signature VALID";
    } else {
        // Signature invalid
        dmem_words[OK_OFFSET / 4] = HARDENED_BOOL_FALSE;
        REG_INFO(1, logger) << "[OTBN P256 ECDSA] Signature INVALID";
    }

    // Cleanup
    ECDSA_SIG_free(sig);
    BN_free(bn_x);
    BN_free(bn_y);
    EC_POINT_free(pub_point);
    EC_KEY_free(key);

    #pragma GCC diagnostic pop

    return otbn_algorithm::SUCCESS;
}

uint64_t otbn_algorithm_p256_ecdsa::get_instruction_count() {
    return instruction_count;
}

uint64_t otbn_algorithm_p256_ecdsa::get_cycle_count() {
    return instruction_count;
}

void otbn_algorithm_p256_ecdsa::reset() {
    instruction_count = 100000;
}

void otbn_algorithm_p256_ecdsa::message_objects(std::ostream &debug, std::ostream &info) {
    REG_INFO(1, logger) << "[OTBN P256 ECDSA] Debug stream configured";
}

