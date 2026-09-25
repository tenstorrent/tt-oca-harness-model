// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#define CSRNG_HW_INTERFACE_TYPES_DEFINED 1

#include <systemc>
#include <cstdint>

class csrng_app_if : public sc_core::sc_interface {
public:
    virtual void send_command(const uint32_t* words, uint32_t count,
                              uint32_t& ack_status) = 0;
    virtual bool is_ready() = 0;
    virtual uint32_t get_ack_status() = 0;
};

class csrng_genbits_if : public sc_core::sc_interface {
public:
    virtual void receive_genbits(uint32_t words[4], bool& fips_compliant) = 0;
    virtual bool has_data() = 0;
    virtual void provide_genbits(const uint32_t words[4], bool fips_compliant) = 0;
};
