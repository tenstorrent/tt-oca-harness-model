/**
 * @file otbn_algorithm_rnd_test.cpp
 * @brief RND Test Algorithm Implementation
 */

#include "otbn_algorithm_rnd_test.h"

otbn_algorithm_rnd_test::otbn_algorithm_rnd_test(size_t dmem_size)
    : otbn_algorithm(dmem_size, false), instruction_count(50) {}

otbn_algorithm::status_t otbn_algorithm_rnd_test::execute(char* dmem) {
    uint32_t rnd_data[8];

    CSML_INFO(1, logger) << "[OTBN RND Test] Requesting RND data...";

    if (m_rnd_read_cb) {
        otbn_algorithm::status_t status = m_rnd_read_cb(rnd_data);
        if (status != otbn_algorithm::SUCCESS) {
            CSML_ERROR(0, logger) << "[OTBN RND Test] RND read failed";
            return otbn_algorithm::ERROR;
        }
    } else {
        CSML_ERROR(0, logger) << "[OTBN RND Test] No RND callback registered";
        return otbn_algorithm::ERROR;
    }

    CSML_INFO(1, logger) << "[OTBN RND Test] RND data received. Writing to DMEM.";

    // Write RND data to DMEM[0..7] (little-endian words)
    for (int i = 0; i < 8; i++) {
        // Store as-is (host will read as little-endian words)
        // Note: dmem is byte array, but we cast to uint32_t* above which assumes host endianness
        // For simplicity in this test, we just copy words.
        // But wait, dmem passed to execute is raw bytes.
        // Let's write bytes carefully.
        uint32_t val = rnd_data[i];
        dmem[i*4 + 0] = (val >> 0) & 0xFF;
        dmem[i*4 + 1] = (val >> 8) & 0xFF;
        dmem[i*4 + 2] = (val >> 16) & 0xFF;
        dmem[i*4 + 3] = (val >> 24) & 0xFF;
    }

    return otbn_algorithm::SUCCESS;
}

uint64_t otbn_algorithm_rnd_test::get_instruction_count() {
    return instruction_count;
}

uint64_t otbn_algorithm_rnd_test::get_cycle_count() {
    return 10;
}

void otbn_algorithm_rnd_test::reset() {
    instruction_count = 500;
}

void otbn_algorithm_rnd_test::message_objects(std::ostream &debug, std::ostream &info) {
    CSML_INFO(1, logger) << "[OTBN RND Test] Debug stream configured";
}

