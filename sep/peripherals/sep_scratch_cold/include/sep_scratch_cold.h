// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "sep_scratch_cold_base.h"
#include "virt_console_decoder.h"
#include "sep_status_decoder.h"
#include "reg_param.h"
#include "reg_logger.h"
#include <cstdint>

#ifndef REG_DEFAULT_VERBOSITY
#define REG_DEFAULT_VERBOSITY 2
#endif

class sep_scratch_cold_ip : public sep_scratch_cold_base
{
public:
    SC_HAS_PROCESS(sep_scratch_cold_ip);

    /// Active-low cold (power-on) reset. Watchdog warm reset does not drive it.
    /// In RTL the cold bank takes `arst_n(rst_ni)` while the warm bank takes
    /// `arst_n(rst_ni && rst_warm_ni)` (sep_system_csr.sv), so this bank retains
    /// its contents across a watchdog reset.
    sc_core::sc_in<bool> cold_rst_ni{"cold_rst_ni"};

    explicit sep_scratch_cold_ip(sc_module_name n);

    // CCI parameters — controllable via accellera_config.ini
    regmodel::Param<bool> sim_out_enable;
    regmodel::Param<bool> sep_status_enable;
    regmodel::Param<int>  verbosity;
    RegLogger             logger;

private:
    void cold_reset_handler();

    virt_console::VirtConsoleDecoder vconsole_decoder_;
    sep_status_report::StatusDecoder     status_decoder_;
};
