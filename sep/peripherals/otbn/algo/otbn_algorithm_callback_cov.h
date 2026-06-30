#pragma once

/**
 * @file otbn_algorithm_callback_cov.h
 * @brief Minimal algorithm that exercises CSR/WDR register callbacks during execute().
 *
 * Used only for unit-test coverage of otbn_ip::{csr,wdr}_{read,write}_handler().
 */

#include "otbn.h"
#include <cstdint>
#include <iostream>

class otbn_algorithm_callback_cov : public otbn_algorithm {
private:
    uint64_t instruction_count;

public:
    explicit otbn_algorithm_callback_cov(size_t dmem_size);

    otbn_algorithm::status_t execute(char* dmem) override;
    uint64_t get_instruction_count() override;
    uint64_t get_cycle_count() override;
    void reset() override;
    void message_objects(std::ostream& debug, std::ostream& info) override;
};
