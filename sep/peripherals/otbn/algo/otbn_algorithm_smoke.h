#pragma once

/**
 * @file otbn_algorithm_smoke.h
 * @brief OTBN Smoke/Integration Algorithm Header
 *
 * Models the OTBN smoke program (otbn_smoke.s / otbn_sep_integration.s),
 * used by otbn_smoke_test, otbn_sep_integration_test, and otbn_fw_control_test.
 *
 * The program reads three inputs from DMEM before execution:
 *   DMEM word 0 (offset 0x00): input_outer_inc
 *   DMEM word 1 (offset 0x04): input_inner_count
 *   DMEM word 2 (offset 0x08): input_inner_inc
 *
 * Computes:
 *   result = 4 * (outer_inc + inner_count * inner_inc)
 *
 * Writes result to DMEM word 3 (offset 0x0C).
 * Reports INSN_CNT = 39 (preamble la/lw × 3 + loopi/loop body + epilogue sw/ecall).
 */

#include "otbn.h"
#include <cstdint>
#include <cstddef>
#include <iostream>

class otbn_algorithm_smoke : public otbn_algorithm {
private:
    uint64_t instruction_count;

public:
    otbn_algorithm_smoke(size_t dmem_size);

    otbn_algorithm::status_t execute(char* dmem) override;
    uint64_t get_instruction_count() override;
    uint64_t get_cycle_count() override;
    void reset() override;
    void message_objects(std::ostream& debug, std::ostream& info) override;
};
