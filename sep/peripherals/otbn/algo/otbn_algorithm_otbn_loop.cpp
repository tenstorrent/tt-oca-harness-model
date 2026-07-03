/**
 * @file otbn_algorithm_otbn_loop.cpp
 * @brief OTBN Loop/Loopi Example Algorithm Implementation
 */

#include "otbn_algorithm_otbn_loop.h"

otbn_algorithm_otbn_loop::otbn_algorithm_otbn_loop(size_t dmem_size)
    : otbn_algorithm(dmem_size, false),
      instruction_count(28) {}

otbn_algorithm::status_t otbn_algorithm_otbn_loop::execute(char* dmem) {
    // Compute the nested loop using plain C to mirror the OTBN program.
    int32_t x2 = 0;
    int32_t x3 = 3;

    for (int i = 0; i < 4; ++i) {
        x2 += 10;
        for (int j = 0; j < x3; ++j) {
            x2 += 1;
        }
    }

    // x2 should now be 52. Store it in DMEM word 0 (little-endian).
    uint8_t* dmem_bytes = reinterpret_cast<uint8_t*>(dmem);
    dmem_bytes[0] = static_cast<uint8_t>((x2 >> 0) & 0xFF);
    dmem_bytes[1] = static_cast<uint8_t>((x2 >> 8) & 0xFF);
    dmem_bytes[2] = static_cast<uint8_t>((x2 >> 16) & 0xFF);
    dmem_bytes[3] = static_cast<uint8_t>((x2 >> 24) & 0xFF);

    // Fixed instruction count, derived from the OpenTitan INSN_CNT test.
    instruction_count = 28;

    CSML_INFO(1, logger) << "[OTBN otbn_loop] Completed nested loop: x2=" << x2 << ", INSN_CNT=" << instruction_count;

    return otbn_algorithm::SUCCESS;
}

uint64_t otbn_algorithm_otbn_loop::get_instruction_count() {
    return instruction_count;
}

uint64_t otbn_algorithm_otbn_loop::get_cycle_count() {
    // Keep a small, deterministic delay for the TLM timing model.
    return 10;
}

void otbn_algorithm_otbn_loop::reset() {
    instruction_count = 28;
}

void otbn_algorithm_otbn_loop::message_objects(std::ostream& debug, std::ostream& info) {
    debug << "[OTBN otbn_loop] Debug stream configured" << std::endl;
    info << "[OTBN otbn_loop] Info stream configured" << std::endl;
}

