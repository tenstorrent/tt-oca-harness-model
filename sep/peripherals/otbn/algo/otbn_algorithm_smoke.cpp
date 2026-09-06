// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file otbn_algorithm_smoke.cpp
 * @brief OTBN Smoke/Integration Algorithm Implementation
 *
 * Models otbn_smoke.s / otbn_sep_integration.s (identical programs).
 * Used by: otbn_smoke_test, otbn_sep_integration_test, otbn_fw_control_test.
 *
 * DMEM layout (set by SEP firmware before CMD=EXECUTE):
 *   word 0 (0x00): input_outer_inc
 *   word 1 (0x04): input_inner_count
 *   word 2 (0x08): input_inner_inc
 *   word 3 (0x0C): result  (written by this algorithm after execution)
 *
 * INSN_CNT = 39:
 *   addi(1) + la/lw x3(6) + loopi(1) + outer×[add+loop+inner×add+nop](6×4=24)
 *   + la/sw/ecall(4) = 39
 */

#include "otbn_algorithm_smoke.h"

otbn_algorithm_smoke::otbn_algorithm_smoke(size_t dmem_size)
    : otbn_algorithm(dmem_size, false),
      instruction_count(39) {}

otbn_algorithm::status_t otbn_algorithm_smoke::execute(char* dmem) {
    uint32_t* dw = reinterpret_cast<uint32_t*>(dmem);

    uint32_t outer_inc   = dw[0];  // input_outer_inc   @ DMEM offset 0x00
    uint32_t inner_count = dw[1];  // input_inner_count @ DMEM offset 0x04
    uint32_t inner_inc   = dw[2];  // input_inner_inc   @ DMEM offset 0x08

    int32_t x2 = 0;
    for (int i = 0; i < 4; ++i) {
        x2 += static_cast<int32_t>(outer_inc);
        for (uint32_t j = 0; j < inner_count; ++j)
            x2 += static_cast<int32_t>(inner_inc);
    }

    dw[3] = static_cast<uint32_t>(x2);  // result @ DMEM offset 0x0C

    instruction_count = 39;

    REG_INFO(1, logger) << "[OTBN smoke] Completed: outer_inc=" << outer_inc << " inner_count=" << inner_count << " inner_inc=" << inner_inc << " result=" << x2 << " INSN_CNT=" << instruction_count;

    return otbn_algorithm::SUCCESS;
}

uint64_t otbn_algorithm_smoke::get_instruction_count() {
    return instruction_count;
}

uint64_t otbn_algorithm_smoke::get_cycle_count() {
    return 10;
}

void otbn_algorithm_smoke::reset() {
    instruction_count = 39;
}

void otbn_algorithm_smoke::message_objects(std::ostream& debug, std::ostream& info) {
    debug << "[OTBN smoke] Debug stream configured" << std::endl;
    info  << "[OTBN smoke] Info stream configured"  << std::endl;
}
