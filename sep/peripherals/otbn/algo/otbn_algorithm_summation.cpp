// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn_algorithm_summation.cpp
 * @brief Simple Summation Algorithm Implementation
 */

#include "otbn_algorithm_summation.h"

otbn_algorithm_summation::otbn_algorithm_summation(size_t dmem_size)
    : otbn_algorithm(dmem_size, false), instruction_count(100) {}

otbn_algorithm::status_t otbn_algorithm_summation::execute(char* dmem) {
    // Cast to uint8_t* for byte-level operations
    uint8_t* dmem_bytes = reinterpret_cast<uint8_t*>(dmem);

    // Read N (number of bytes to sum) from DMEM[0]
    uint32_t N = dmem_bytes[0];

    // Validate N
    if (N == 0) {
        CSML_ERROR(0, logger) << "[OTBN Summation] ERROR: N=0, nothing to sum";
        return ERROR;
    }

    if (N + 2 > m_dmem_size) {
        CSML_ERROR(0, logger) << "[OTBN Summation] ERROR: N=" << N << " exceeds DMEM size (" << m_dmem_size << " bytes)";
        return ERROR;
    }

    // Sum bytes from DMEM[1] to DMEM[N]
    uint64_t sum = 0;
    for (uint32_t i = 1; i <= N; i++) {
        sum += dmem_bytes[i];
    }

    // Determine result length (number of bytes needed to represent sum)
    uint32_t result_length = 0;
    uint64_t temp_sum = sum;
    if (temp_sum == 0) {
        result_length = 1;  // Zero requires 1 byte
    } else {
        // Count number of bytes needed
        while (temp_sum > 0) {
            result_length++;
            temp_sum >>= 8;
        }
    }

    // Write result length to DMEM[N+1]
    dmem_bytes[N + 1] = result_length & 0xFF;

    // Write result (little-endian) starting at DMEM[N+2]
    for (uint32_t i = 0; i < result_length; i++) {
        dmem_bytes[N + 2 + i] = (sum >> (i * 8)) & 0xFF;
    }

    // Debug output
    CSML_INFO(1, logger) << "[OTBN Summation] N=" << N << ", Sum=" << sum << ", Result length=" << result_length << " bytes";

    // Update instruction count based on operation complexity
    instruction_count = 100 + (N * 10);  // Base + per-byte cost

    return otbn_algorithm::SUCCESS;
}

uint64_t otbn_algorithm_summation::get_instruction_count() {
    return instruction_count;
}

uint64_t otbn_algorithm_summation::get_cycle_count() {
    return 10;
}

void otbn_algorithm_summation::reset() {
    instruction_count = 100;
}

void otbn_algorithm_summation::message_objects(std::ostream &debug, std::ostream &info) {
    CSML_INFO(1, logger) << "[OTBN Summation] Debug stream configured";
}

