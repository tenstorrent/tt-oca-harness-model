// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_reset_ctrl.h
 * @brief SEP Software Reset Controller - regmodel implementation
 *
 * Register: SW_RESET_N (64-bit) at offset 0x0
 * - Bit 0: km_sw_rst_n   (reset=0, KM starts in reset)
 * - Bit 1: otbn_sw_rst_n (reset=1, OTBN starts released)
 * - Bit 2: aes_sw_rst_n  (reset=1, AES starts released)
 * - Bit 3: hmac_sw_rst_n (reset=1, HMAC starts released)
 * - Bit 4: kmac_sw_rst_n (reset=1, KMAC starts released)
 * - Bit 5: trng_sw_rst_n (reset=1, entropy_src/CSRNG/EDN start released)
 * - Bit 6: abr_sw_rst_n  (reset=1, Adams Bridge starts released)
 *
 * Default 0x7E, writable mask 0x7F. Matches sep_reset_ctrl.rdl in tt-oca-harness.
 */

#pragma once
#include "sep_reset_ctrl_base.h"
#include "reg_param.h"
#include "reg_logger.h"
#include <systemc.h>

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif

class sep_reset_ctrl_ip : public sep_reset_ctrl_base
{
public:
    SC_HAS_PROCESS(sep_reset_ctrl_ip);

    typedef typename regmodel::Reg<64>::DT DT;

    static constexpr uint64_t SW_RESET_N_MASK  = 0x7Full;
    static constexpr uint64_t SW_RESET_N_RESET = 0x7Eull;
    static constexpr uint64_t SW_RESET_N_KM    = 1ull << 0;
    static constexpr uint64_t SW_RESET_N_OTBN  = 1ull << 1;
    static constexpr uint64_t SW_RESET_N_AES   = 1ull << 2;
    static constexpr uint64_t SW_RESET_N_HMAC  = 1ull << 3;
    static constexpr uint64_t SW_RESET_N_KMAC  = 1ull << 4;
    static constexpr uint64_t SW_RESET_N_TRNG  = 1ull << 5;
    static constexpr uint64_t SW_RESET_N_ABR   = 1ull << 6;

    sc_core::sc_in<bool>  global_rst_ni{"global_rst_ni"};
    sc_core::sc_out<bool> km_rst_ni{"km_rst_ni"};
    sc_core::sc_out<bool> otbn_rst_n{"otbn_rst_n"};
    sc_core::sc_out<bool> aes_rst_ni{"aes_rst_ni"};
    sc_core::sc_out<bool> hmac_rst_ni{"hmac_rst_ni"};
    sc_core::sc_out<bool> kmac_rst_ni{"kmac_rst_ni"};
    sc_core::sc_out<bool> trng_rst_ni{"trng_rst_ni"};
    sc_core::sc_out<bool> abr_rst_ni{"abr_rst_ni"};

    regmodel::Param<int> verbosity;
    RegLogger      logger;

    explicit sep_reset_ctrl_ip(sc_module_name n);

    void reset();

private:
    sc_core::sc_event sw_reset_changed_;

    void update_rst_outputs();

    // SC_METHOD sensitive to global_rst_ni — calls reset() so SW_RESET_N's
    // software-visible value reverts to its documented default (0x7E) on a
    // global reset, instead of surviving with whatever firmware last wrote.
    void reset_handler();

    bool handle_sw_reset_n_write(uint64_t value, uint64_t mask);
};
