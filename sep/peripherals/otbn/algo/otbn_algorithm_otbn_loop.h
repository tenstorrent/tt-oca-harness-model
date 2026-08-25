// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

/**
 * @file otbn_algorithm_otbn_loop.h
 * @brief OTBN Loop/Loopi Example Algorithm Header
 *
 * Software model of the simple nested loop OTBN program:
 *
 *   x2 = 0;
 *   x3 = 3;
 *   for (int i = 0; i < 4; ++i) {
 *     x2 += 10;
 *     for (int j = 0; j < x3; ++j) {
 *       x2 += 1;
 *     }
 *   }
 *
 * The final value is x2 = 52 and the expected instruction count, matching the
 * OpenTitan `loops.s` INSN_CNT test, is 28.
 *
 * To keep the interface simple and focused on INSN_CNT (like the hardware
 * test), this algorithm:
 *   - Computes the same x2 result in C++
 *   - Stores the 32-bit value into DMEM word 0 (bytes 0..3, little-endian)
 *   - Reports a fixed instruction count of 28 via get_instruction_count()
 */


#include "otbn.h"
#include <cstdint>
#include <cstddef>
#include <iostream>

/**
 * @brief OTBN Loop/Loopi Example Algorithm (`otbn_loop`)
 */
class otbn_algorithm_otbn_loop : public otbn_algorithm {
private:
    uint64_t instruction_count;

public:
    otbn_algorithm_otbn_loop(size_t dmem_size);

    otbn_algorithm::status_t execute(char* dmem) override;
    uint64_t get_instruction_count() override;
    uint64_t get_cycle_count() override;
    void reset() override;
    void message_objects(std::ostream& debug, std::ostream& info) override;
};

