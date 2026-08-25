// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_scratch_cold.h"
#include <fstream>
#include <iostream>

#ifndef SEP_SCRATCH_COLD_STATUS_VALUES_PATH
#define SEP_SCRATCH_COLD_STATUS_VALUES_PATH ""
#endif

sep_scratch_cold_ip::sep_scratch_cold_ip(sc_module_name n)
    : sep_scratch_cold_base(n, "sep_scratch_cold", 8 * sizeof(unsigned long long))
    , sim_out_enable("sim_out.enable", true)
    , sep_status_enable("sep_status.enable", true)
    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
{
    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    reset_all_registers();

    // All SCRATCH[0..7] registers act as plain read/write storage via the CSML
    // memory block. The callbacks below add behaviour on top of that storage —
    // they fire post-write (value is already stored before the callback runs).

    // ---- Virtual console: SCRATCH[2] (offset 0x10) → SIM_OUT ----
    // Decodes the rom_virt_console.h protocol: opcode in bits[3:1], payload in
    // bits[31:8]. Line-buffered so partial ASCII sequences print as whole lines.
    vconsole_decoder_.set_enabled(sim_out_enable.get_param_value());
    vconsole_decoder_.set_emit([](const std::string& line) {
        std::cout << line << '\n';
    });
    memory.register_post_write_callback(
        [this]() -> bool {
            DT val = memory.memory_block[SCRATCH[2].offset];
            vconsole_decoder_.on_word(static_cast<uint32_t>(val & 0xFFFFFFFFU));
            return true;
        },
        SCRATCH[2].offset);

    // ---- Status reporting: SCRATCH[1] (offset 0x08) → SEP_STATUS ----
    // Firmware writes every status word to COLD_SCRATCH_1 unconditionally
    // (via STATUS_OUT in errors.h) before pushing to the SMC ring buffer.
    // Tapping here captures all types including DEBUG, which the ring buffer drops.
    status_decoder_.set_enabled(sep_status_enable.get_param_value());
    status_decoder_.set_emit([](const std::string& line) {
        std::cout << line << '\n';
    });

    // Load SEP_MSG_* names from the vendored status_values.h snapshot (see
    // include/status_values.h) so [SEP_STATUS]
    // lines show real names instead of SEP_MSG_UNKNOWN. Path is baked in by
    // CMake (SEP_SCRATCH_COLD_STATUS_VALUES_PATH), not resolved at runtime
    // relative to the process's working directory.
    {
        std::ifstream names_file(SEP_SCRATCH_COLD_STATUS_VALUES_PATH);
        if (names_file.is_open()) {
            status_decoder_.set_names(sep_status_report::StatusDecoder::parse_tsv(names_file));
            CSML_INFO(1, logger) << "Loaded " << status_decoder_.name_count()
                                  << " status names from "
                                  << SEP_SCRATCH_COLD_STATUS_VALUES_PATH << std::endl;
        } else {
            CSML_WARN(0, logger) << "Could not open status names header: " << SEP_SCRATCH_COLD_STATUS_VALUES_PATH << std::endl;
        }
    }
    memory.register_post_write_callback(
        [this]() -> bool {
            DT val = memory.memory_block[SCRATCH[1].offset];
            status_decoder_.on_word(static_cast<uint32_t>(val & 0xFFFFFFFFU));
            return true;
        },
        SCRATCH[1].offset);

    // ---- VP testbench acks (simulate CocoTB handshakes for standalone VP runs) ----
    // In CocoTB-based DV, firmware writes a sentinel to one scratch register and
    // polls an adjacent register waiting for CocoTB to ack. Without CocoTB running,
    // the firmware hangs indefinitely. These callbacks mimic the CocoTB response
    // immediately so standalone VP runs of those tests complete without hanging.

    // global_alias_remap_sanity: FW writes 0x12345678 to SCRATCH[0] ("remap set up,
    // verify it"); CocoTB/VP acks 0x87654321 to SCRATCH[1] ("verified, proceed").
    memory.register_post_write_callback(
        [this]() -> bool {
            if (memory.memory_block[SCRATCH[0].offset] == 0x12345678ULL)
                memory.memory_block[SCRATCH[1].offset] = 0x87654321ULL;
            return true;
        },
        SCRATCH[0].offset);

    // ap_stee_output_remap_test: FW writes 0x815 to SCRATCH[4] ("ready to test");
    // CocoTB/VP acks 0x777 to SCRATCH[5] ("go ahead").
    memory.register_post_write_callback(
        [this]() -> bool {
            if (memory.memory_block[SCRATCH[4].offset] == 0x815ULL)
                memory.memory_block[SCRATCH[5].offset] = 0x777ULL;
            return true;
        },
        SCRATCH[4].offset);

    // sep_aes_large_payload_test (STRESS-002): FW writes FW_READY_MAGIC (0xA1E50006)
    // to SCRATCH[6] requesting test parameters; CocoTB/VP responds with
    // {blocks[31:16], seed[15:0]} = 0x00100001 (16 blocks, seed=1) to SCRATCH[7].
    memory.register_post_write_callback(
        [this]() -> bool {
            if (memory.memory_block[SCRATCH[6].offset] == 0xA1E50006ULL)
                memory.memory_block[SCRATCH[7].offset] = 0x00100001ULL;
            return true;
        },
        SCRATCH[6].offset);
}
