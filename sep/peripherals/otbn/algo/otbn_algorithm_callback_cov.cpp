/**
 * @file otbn_algorithm_callback_cov.cpp
 * @brief Exercises algorithm CSR/WDR callbacks for coverage of otbn_ip handlers.
 */

#include "otbn_algorithm_callback_cov.h"

otbn_algorithm_callback_cov::otbn_algorithm_callback_cov(size_t dmem_size)
    : otbn_algorithm(dmem_size, false), instruction_count(8) {}

otbn_algorithm::status_t otbn_algorithm_callback_cov::execute(char* dmem) {
    CSML_INFO(1, logger) << "[OTBN callback_cov] Exercising CSR/WDR callbacks";

    uint32_t csr_val = 0;
    if (m_csr_read_cb) {
        m_csr_read_cb(0x10, &csr_val);
    }
    if (m_csr_write_cb) {
        m_csr_write_cb(0x10, 0xDEADBEEFu);
    }

    uint64_t wdr_buf[4] = {
        0x1111111111111111ULL, 0x2222222222222222ULL,
        0x3333333333333333ULL, 0x4444444444444444ULL};
    uint64_t wdr_out[4] = {0};

    if (m_wdr_write_cb) {
        m_wdr_write_cb(0, wdr_buf);
        m_wdr_write_cb(32, wdr_buf);
    }

    if (m_wdr_read_cb) {
        m_wdr_read_cb(32, wdr_out);

        if (m_key_status_cb && !m_key_status_cb()) {
            m_wdr_read_cb(20, wdr_out);  // WDR_KEY_S0_L without sideload key
        }

        m_wdr_read_cb(0, wdr_out);

        if (m_key_status_cb && m_key_status_cb()) {
            m_wdr_read_cb(20, wdr_out);  // WDR_KEY_S0_L with key programmed
        }
    }

    auto* dw = reinterpret_cast<uint32_t*>(dmem);
    dw[0] = 0xC0DEC0DEu;

    instruction_count = 8;
    return otbn_algorithm::SUCCESS;
}

uint64_t otbn_algorithm_callback_cov::get_instruction_count() {
    return instruction_count;
}

uint64_t otbn_algorithm_callback_cov::get_cycle_count() {
    return 8;
}

void otbn_algorithm_callback_cov::reset() {
    instruction_count = 8;
}

void otbn_algorithm_callback_cov::message_objects(std::ostream& debug, std::ostream& info) {
    debug << "[OTBN callback_cov] Debug stream configured" << std::endl;
    info << "[OTBN callback_cov] Info stream configured" << std::endl;
}
