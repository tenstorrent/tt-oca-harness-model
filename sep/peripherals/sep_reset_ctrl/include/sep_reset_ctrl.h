// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file sep_reset_ctrl.h
 * @brief SEP Software Reset Controller - regmodel implementation
 * 
 * Migrated from knowledge-base design: controls software reset signals
 * for various SEP peripherals (KM, OTBN, AES, HMAC, KMAC).
 * 
 * Register: SW_RESET_N (64-bit) at offset 0x0
 * - Bit 0: km_sw_rst_n   (reset=0, KM starts in reset)
 * - Bit 1: otbn_sw_rst_n (reset=1, OTBN starts released)
 * - Bit 2: aes_sw_rst_n  (reset=1, AES starts released)
 * - Bit 3: hmac_sw_rst_n (reset=1, HMAC starts released) 
 * - Bit 4: kmac_sw_rst_n (reset=1, KMAC starts released)
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
    
    // Reset output ports (matches knowledge-base design)
    sc_core::sc_in<bool>  global_rst_ni{"global_rst_ni"};
    sc_core::sc_out<bool> km_rst_ni{"km_rst_ni"};
    sc_core::sc_out<bool> otbn_rst_n{"otbn_rst_n"};
    sc_core::sc_out<bool> aes_rst_ni{"aes_rst_ni"};
    sc_core::sc_out<bool> hmac_rst_ni{"hmac_rst_ni"};
    sc_core::sc_out<bool> kmac_rst_ni{"kmac_rst_ni"};
    
    regmodel::Param<int> verbosity;
    RegLogger      logger;

    // Constructor
    explicit sep_reset_ctrl_ip(sc_module_name n);
    
    // Reset function
    void reset();

private:
    // Internal event to trigger reset output updates
    sc_core::sc_event sw_reset_changed_;
    
    // Current SW_RESET_N value for reset output generation
    uint64_t sw_reset_current_value_;
    
    // SC_METHOD to update reset outputs when SW_RESET_N register changes
    void update_rst_outputs();

    // SC_METHOD sensitive to global_rst_ni — calls reset() so SW_RESET_N's
    // software-visible value reverts to its documented default (0x1E) on a
    // global reset, instead of surviving with whatever firmware last wrote.
    void reset_handler();

    // Write callback for SW_RESET_N register - AES pattern
    bool handle_sw_reset_n_write(uint64_t value, uint64_t mask);
};
