#pragma once
#include "sep_scratch_cold_base.h"
#include "virt_console_decoder.h"
#include "sep_status_decoder.h"
#include "csml_parameter.h"
#include "csml_logger.h"
#include <cstdint>

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

class sep_scratch_cold_ip : public sep_scratch_cold_base
{
public:
    SC_HAS_PROCESS(sep_scratch_cold_ip);

    explicit sep_scratch_cold_ip(sc_module_name n);

    // CCI parameters — controllable via accellera_config.ini
    csml_param<bool> sim_out_enable;
    csml_param<bool> sep_status_enable;
    csml_param<int>  verbosity;
    CsmlLogger       logger;

private:
    sep_virt_console::VirtConsoleDecoder vconsole_decoder_;
    sep_status_report::StatusDecoder     status_decoder_;
};
