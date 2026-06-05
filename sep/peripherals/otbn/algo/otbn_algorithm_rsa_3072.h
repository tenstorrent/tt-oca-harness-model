#pragma once

/**
 * @file otbn_algorithm_rsa_3072.h
 * @brief RSA-3072 PKCS#1 v1.5 verify algorithm (modular exponentiation, e=65537)
 *
 * DMEM Layout (rsa_3072_app OTBN program, LSWord-first storage):
 *   0x000-0x17F (384 bytes / 96 words): modulus n       (word[0] = LSWord)
 *   0x600-0x77F (384 bytes / 96 words): inout signature (word[0] = LSWord)
 *
 * Computes: inout = inout^65537 mod n
 * After execution, inout[0..7] holds the SHA-256 hash from the PKCS#1 v1.5 block.
 */

#include "otbn.h"
#include <cstdint>
#include <cstddef>
#include <iostream>

class otbn_algorithm_rsa_3072 : public otbn_algorithm {
private:
    uint64_t instruction_count;

public:
    explicit otbn_algorithm_rsa_3072(size_t dmem_size);

    otbn_algorithm::status_t execute(char* dmem) override;
    uint64_t get_instruction_count() override;
    uint64_t get_cycle_count() override;
    void reset() override;
    void message_objects(std::ostream& debug, std::ostream& info) override;
};
