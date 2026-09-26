// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <systemc>
#include <cstdint>

class entropy_provider_if : public sc_core::sc_interface {
public:
    virtual bool get_seed_384(uint8_t seed[48], bool& fips_compliant) = 0;
};
