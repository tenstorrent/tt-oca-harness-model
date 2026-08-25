// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

/**
 * @file otbn_algorithm_rnd_test.h
 * @brief RND Test Algorithm Header
 *
 * Algorithm for testing RND register blocking behavior.
 * Requests 256 bits (8 words) of RND data from EDN and writes result to DMEM.
 *
 * DMEM Layout:
 *   DMEM[0..7]: Result (8 words = 256 bits)
 */


#include "otbn.h"
#include <cstdint>
#include <cstddef>
#include <iostream>

/**
 * @brief RND Test Algorithm
 */
class otbn_algorithm_rnd_test : public otbn_algorithm {
private:
    uint64_t instruction_count;

public:
    otbn_algorithm_rnd_test(size_t dmem_size);

    otbn_algorithm::status_t execute(char* dmem) override;
    uint64_t get_instruction_count() override;
    uint64_t get_cycle_count() override;
    void reset() override;
    void message_objects(std::ostream &debug, std::ostream &info) override;
};

