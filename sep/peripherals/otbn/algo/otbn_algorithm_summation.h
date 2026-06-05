#pragma once

/**
 * @file otbn_algorithm_summation.h
 * @brief Simple Summation Algorithm Header
 *
 * Simple reference algorithm that sums N bytes from DMEM.
 * Demonstrates basic DMEM access pattern for algorithm development.
 *
 * DMEM Layout Convention (per otbn_plan.md):
 *   DMEM[0]:       N (number of bytes to sum, 1 byte)
 *   DMEM[1..N]:    Input bytes to be summed
 *   DMEM[N+1]:     Result length in bytes
 *   DMEM[N+2..]:   Result (sum of input bytes)
 *
 * Example:
 *   Input:  DMEM[0] = 3, DMEM[1] = 5, DMEM[2] = 10, DMEM[3] = 7
 *   Output: DMEM[4] = 1 (result length), DMEM[5] = 22 (5+10+7=22)
 */


#include "otbn.h"
#include <cstdint>
#include <cstddef>
#include <iostream>

/**
 * @brief Simple Summation Algorithm (Reference Implementation)
 */
class otbn_algorithm_summation : public otbn_algorithm {
private:
    uint64_t instruction_count;  // Estimated instruction count

public:
    /**
     * Constructor
     * @param dmem_size Size of DMEM in bytes
     */
    otbn_algorithm_summation(size_t dmem_size);

    otbn_algorithm::status_t execute(char* dmem) override;
    uint64_t get_instruction_count() override;
    uint64_t get_cycle_count() override;
    void reset() override;
    void message_objects(std::ostream &debug, std::ostream &info) override;
};

