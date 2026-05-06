/***************************************************************************
 * Copyright 1996-2024 Synopsys, Inc.
 *
 * This Synopsys software and all associated documentation are proprietary
 * to Synopsys, Inc. and may only be used pursuant to the terms and
 * conditions of a written license agreement with Synopsys, Inc.
 * All other use, reproduction, modification, or distribution of the
 * Synopsys software or the associated documentation is strictly prohibited.
 ***************************************************************************/
 

/***************************************************************************
 * Generated snippet, used for detecting user edits.
 * CHECKSUM:faf4f7e37c014617374360b02c0af27fbaa4c34d
 ***************************************************************************/
 

 
 // Warning: This is an auto-generated file. All changes to this file will be overwritten.


#include <systemc>
#include <scmlinc/scml_property.h>
#include "scml2_objects.h"
#include "scml2_protocol_engines/tlm2_ft_target_port/include/tlm2_ft_target_port_pe.h"
#include "scml2_protocol_engines/reset/include/reset_engine.h"
#include "scml2_protocol_engines/interrupt/include/interrupt_engine.h"
#include <tlm.h>
#include <scml2.h>


#include "cdns_xspi_ctrl_regBase.h"

namespace mylibrary {

#ifdef _WIN32
#pragma optimize("g",off)
#else
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif

cdns_xspi_ctrl_regBase::cmd_reg0_type::~cmd_reg0_type() {};

cdns_xspi_ctrl_regBase::cmd_reg0_type::cmd_reg0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cmd0(sc_core::sc_gen_unique_name("cmd0", true), *this, 0, 32),
    cmd0_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::cmd_reg1_type::~cmd_reg1_type() {};

cdns_xspi_ctrl_regBase::cmd_reg1_type::cmd_reg1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cmd1(sc_core::sc_gen_unique_name("cmd1", true), *this, 0, 32),
    cmd1_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::cmd_reg2_type::~cmd_reg2_type() {};

cdns_xspi_ctrl_regBase::cmd_reg2_type::cmd_reg2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cmd2(sc_core::sc_gen_unique_name("cmd2", true), *this, 0, 32),
    cmd2_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::cmd_reg3_type::~cmd_reg3_type() {};

cdns_xspi_ctrl_regBase::cmd_reg3_type::cmd_reg3_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cmd3(sc_core::sc_gen_unique_name("cmd3", true), *this, 0, 32),
    cmd3_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::cmd_reg4_type::~cmd_reg4_type() {};

cdns_xspi_ctrl_regBase::cmd_reg4_type::cmd_reg4_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cmd4(sc_core::sc_gen_unique_name("cmd4", true), *this, 0, 32),
    cmd4_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::cmd_reg5_type::~cmd_reg5_type() {};

cdns_xspi_ctrl_regBase::cmd_reg5_type::cmd_reg5_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cmd5(sc_core::sc_gen_unique_name("cmd5", true), *this, 0, 32),
    cmd5_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::cmd_status_ptr_type::~cmd_status_ptr_type() {};

cdns_xspi_ctrl_regBase::cmd_status_ptr_type::cmd_status_ptr_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    thrd_status_sel(sc_core::sc_gen_unique_name("thrd_status_sel", true), *this, 0, 3),
    thrd_status_sel_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::cmd_status_type::~cmd_status_type() {};

cdns_xspi_ctrl_regBase::cmd_status_type::cmd_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cmd_status(sc_core::sc_gen_unique_name("cmd_status", true), *this, 0, 32),
    cmd_status_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::ctrl_status_type::~ctrl_status_type() {};

cdns_xspi_ctrl_regBase::ctrl_status_type::ctrl_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    sdma_busy(sc_core::sc_gen_unique_name("sdma_busy", true), *this, 0, 1),
    sdma_busy_reset_value(0x0),
    mdma_busy(sc_core::sc_gen_unique_name("mdma_busy", true), *this, 1, 1),
    mdma_busy_reset_value(0x0),
    acmd_eng_busy(sc_core::sc_gen_unique_name("acmd_eng_busy", true), *this, 2, 1),
    acmd_eng_busy_reset_value(0x0),
    gcmd_eng_busy(sc_core::sc_gen_unique_name("gcmd_eng_busy", true), *this, 3, 1),
    gcmd_eng_busy_reset_value(0x0),
    gcmd_eng_mc_busy(sc_core::sc_gen_unique_name("gcmd_eng_mc_busy", true), *this, 4, 1),
    gcmd_eng_mc_busy_reset_value(0x0),
    discovery_busy(sc_core::sc_gen_unique_name("discovery_busy", true), *this, 6, 1),
    discovery_busy_reset_value(0x0),
    ctrl_busy(sc_core::sc_gen_unique_name("ctrl_busy", true), *this, 7, 1),
    ctrl_busy_reset_value(0x0),
    init_fail(sc_core::sc_gen_unique_name("init_fail", true), *this, 8, 2),
    init_fail_reset_value(0x0),
    init_comp(sc_core::sc_gen_unique_name("init_comp", true), *this, 16, 1),
    init_comp_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::trd_status_type::~trd_status_type() {};

cdns_xspi_ctrl_regBase::trd_status_type::trd_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    trd_busy(sc_core::sc_gen_unique_name("trd_busy", true), *this, 0, 8),
    trd_busy_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::intr_status_type::~intr_status_type() {};

cdns_xspi_ctrl_regBase::intr_status_type::intr_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    gp_open_drain_0(sc_core::sc_gen_unique_name("gp_open_drain_0", true), *this, 12, 1),
    gp_open_drain_0_reset_value(0x0),
    gp_open_drain_1(sc_core::sc_gen_unique_name("gp_open_drain_1", true), *this, 13, 1),
    gp_open_drain_1_reset_value(0x0),
    gp_open_drain_2(sc_core::sc_gen_unique_name("gp_open_drain_2", true), *this, 14, 1),
    gp_open_drain_2_reset_value(0x0),
    gp_open_drain_3(sc_core::sc_gen_unique_name("gp_open_drain_3", true), *this, 15, 1),
    gp_open_drain_3_reset_value(0x0),
    ctrl_idle(sc_core::sc_gen_unique_name("ctrl_idle", true), *this, 16, 1),
    ctrl_idle_reset_value(0x0),
    cdma_terr(sc_core::sc_gen_unique_name("cdma_terr", true), *this, 17, 1),
    cdma_terr_reset_value(0x0),
    ddma_terr(sc_core::sc_gen_unique_name("ddma_terr", true), *this, 18, 1),
    ddma_terr_reset_value(0x0),
    cmd_ignored(sc_core::sc_gen_unique_name("cmd_ignored", true), *this, 20, 1),
    cmd_ignored_reset_value(0x0),
    sdma_trigg(sc_core::sc_gen_unique_name("sdma_trigg", true), *this, 21, 1),
    sdma_trigg_reset_value(0x0),
    sdma_err(sc_core::sc_gen_unique_name("sdma_err", true), *this, 22, 1),
    sdma_err_reset_value(0x0),
    stig_done(sc_core::sc_gen_unique_name("stig_done", true), *this, 23, 1),
    stig_done_reset_value(0x0),
    dir_crc_err(sc_core::sc_gen_unique_name("dir_crc_err", true), *this, 24, 1),
    dir_crc_err_reset_value(0x0),
    dir_dqs_err(sc_core::sc_gen_unique_name("dir_dqs_err", true), *this, 25, 1),
    dir_dqs_err_reset_value(0x0),
    dir_cmd_err(sc_core::sc_gen_unique_name("dir_cmd_err", true), *this, 26, 1),
    dir_cmd_err_reset_value(0x0),
    dir_ecc_corr_err(sc_core::sc_gen_unique_name("dir_ecc_corr_err", true), *this, 27, 1),
    dir_ecc_corr_err_reset_value(0x0),
    dir_dev_err(sc_core::sc_gen_unique_name("dir_dev_err", true), *this, 28, 1),
    dir_dev_err_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::intr_enable_type::~intr_enable_type() {};

cdns_xspi_ctrl_regBase::intr_enable_type::intr_enable_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    gp_open_drain_0_en(sc_core::sc_gen_unique_name("gp_open_drain_0_en", true), *this, 12, 1),
    gp_open_drain_0_en_reset_value(0x0),
    gp_open_drain_1_en(sc_core::sc_gen_unique_name("gp_open_drain_1_en", true), *this, 13, 1),
    gp_open_drain_1_en_reset_value(0x0),
    gp_open_drain_2_en(sc_core::sc_gen_unique_name("gp_open_drain_2_en", true), *this, 14, 1),
    gp_open_drain_2_en_reset_value(0x0),
    gp_open_drain_3_en(sc_core::sc_gen_unique_name("gp_open_drain_3_en", true), *this, 15, 1),
    gp_open_drain_3_en_reset_value(0x0),
    ctrl_idle_en(sc_core::sc_gen_unique_name("ctrl_idle_en", true), *this, 16, 1),
    ctrl_idle_en_reset_value(0x0),
    cdma_terr_en(sc_core::sc_gen_unique_name("cdma_terr_en", true), *this, 17, 1),
    cdma_terr_en_reset_value(0x0),
    ddma_terr_en(sc_core::sc_gen_unique_name("ddma_terr_en", true), *this, 18, 1),
    ddma_terr_en_reset_value(0x0),
    cmd_ignored_en(sc_core::sc_gen_unique_name("cmd_ignored_en", true), *this, 20, 1),
    cmd_ignored_en_reset_value(0x0),
    sdma_trigg_en(sc_core::sc_gen_unique_name("sdma_trigg_en", true), *this, 21, 1),
    sdma_trigg_en_reset_value(0x0),
    sdma_err_en(sc_core::sc_gen_unique_name("sdma_err_en", true), *this, 22, 1),
    sdma_err_en_reset_value(0x0),
    stig_done_en(sc_core::sc_gen_unique_name("stig_done_en", true), *this, 23, 1),
    stig_done_en_reset_value(0x0),
    dir_crc_err_en(sc_core::sc_gen_unique_name("dir_crc_err_en", true), *this, 24, 1),
    dir_crc_err_en_reset_value(0x0),
    dir_dqs_err_en(sc_core::sc_gen_unique_name("dir_dqs_err_en", true), *this, 25, 1),
    dir_dqs_err_en_reset_value(0x0),
    dir_cmd_err_en(sc_core::sc_gen_unique_name("dir_cmd_err_en", true), *this, 26, 1),
    dir_cmd_err_en_reset_value(0x0),
    dir_ecc_corr_err_en(sc_core::sc_gen_unique_name("dir_ecc_corr_err_en", true), *this, 27, 1),
    dir_ecc_corr_err_en_reset_value(0x0),
    dir_dev_err_en(sc_core::sc_gen_unique_name("dir_dev_err_en", true), *this, 28, 1),
    dir_dev_err_en_reset_value(0x0),
    intr_en(sc_core::sc_gen_unique_name("intr_en", true), *this, 31, 1),
    intr_en_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::trd_comp_intr_status_type::~trd_comp_intr_status_type() {};

cdns_xspi_ctrl_regBase::trd_comp_intr_status_type::trd_comp_intr_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    trd0_comp(sc_core::sc_gen_unique_name("trd0_comp", true), *this, 0, 1),
    trd0_comp_reset_value(0x0),
    trd1_comp(sc_core::sc_gen_unique_name("trd1_comp", true), *this, 1, 1),
    trd1_comp_reset_value(0x0),
    trd2_comp(sc_core::sc_gen_unique_name("trd2_comp", true), *this, 2, 1),
    trd2_comp_reset_value(0x0),
    trd3_comp(sc_core::sc_gen_unique_name("trd3_comp", true), *this, 3, 1),
    trd3_comp_reset_value(0x0),
    trd4_comp(sc_core::sc_gen_unique_name("trd4_comp", true), *this, 4, 1),
    trd4_comp_reset_value(0x0),
    trd5_comp(sc_core::sc_gen_unique_name("trd5_comp", true), *this, 5, 1),
    trd5_comp_reset_value(0x0),
    trd6_comp(sc_core::sc_gen_unique_name("trd6_comp", true), *this, 6, 1),
    trd6_comp_reset_value(0x0),
    trd7_comp(sc_core::sc_gen_unique_name("trd7_comp", true), *this, 7, 1),
    trd7_comp_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::trd_error_intr_status_type::~trd_error_intr_status_type() {};

cdns_xspi_ctrl_regBase::trd_error_intr_status_type::trd_error_intr_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    trd0_error_stat(sc_core::sc_gen_unique_name("trd0_error_stat", true), *this, 0, 1),
    trd0_error_stat_reset_value(0x0),
    trd1_error_stat(sc_core::sc_gen_unique_name("trd1_error_stat", true), *this, 1, 1),
    trd1_error_stat_reset_value(0x0),
    trd2_error_stat(sc_core::sc_gen_unique_name("trd2_error_stat", true), *this, 2, 1),
    trd2_error_stat_reset_value(0x0),
    trd3_error_stat(sc_core::sc_gen_unique_name("trd3_error_stat", true), *this, 3, 1),
    trd3_error_stat_reset_value(0x0),
    trd4_error_stat(sc_core::sc_gen_unique_name("trd4_error_stat", true), *this, 4, 1),
    trd4_error_stat_reset_value(0x0),
    trd5_error_stat(sc_core::sc_gen_unique_name("trd5_error_stat", true), *this, 5, 1),
    trd5_error_stat_reset_value(0x0),
    trd6_error_stat(sc_core::sc_gen_unique_name("trd6_error_stat", true), *this, 6, 1),
    trd6_error_stat_reset_value(0x0),
    trd7_error_stat(sc_core::sc_gen_unique_name("trd7_error_stat", true), *this, 7, 1),
    trd7_error_stat_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::trd_error_intr_en_type::~trd_error_intr_en_type() {};

cdns_xspi_ctrl_regBase::trd_error_intr_en_type::trd_error_intr_en_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    trd_error_intr_en(sc_core::sc_gen_unique_name("trd_error_intr_en", true), *this, 0, 8),
    trd_error_intr_en_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::dma_target_error_l_type::~dma_target_error_l_type() {};

cdns_xspi_ctrl_regBase::dma_target_error_l_type::dma_target_error_l_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    target_err_l(sc_core::sc_gen_unique_name("target_err_l", true), *this, 0, 32),
    target_err_l_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::dma_target_error_h_type::~dma_target_error_h_type() {};

cdns_xspi_ctrl_regBase::dma_target_error_h_type::dma_target_error_h_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    target_err_h(sc_core::sc_gen_unique_name("target_err_h", true), *this, 0, 32),
    target_err_h_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::boot_status_type::~boot_status_type() {};

cdns_xspi_ctrl_regBase::boot_status_type::boot_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    boot_dqs_err(sc_core::sc_gen_unique_name("boot_dqs_err", true), *this, 0, 1),
    boot_dqs_err_reset_value(0x0),
    boot_crc_err(sc_core::sc_gen_unique_name("boot_crc_err", true), *this, 1, 1),
    boot_crc_err_reset_value(0x0),
    boot_bus_err(sc_core::sc_gen_unique_name("boot_bus_err", true), *this, 2, 1),
    boot_bus_err_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::long_polling_type::~long_polling_type() {};

cdns_xspi_ctrl_regBase::long_polling_type::long_polling_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    long_polling(sc_core::sc_gen_unique_name("long_polling", true), *this, 0, 16),
    long_polling_reset_value(0x3e8) {}

cdns_xspi_ctrl_regBase::short_polling_type::~short_polling_type() {};

cdns_xspi_ctrl_regBase::short_polling_type::short_polling_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    short_polling(sc_core::sc_gen_unique_name("short_polling", true), *this, 0, 16),
    short_polling_reset_value(0x1f4) {}

cdns_xspi_ctrl_regBase::ctrl_config_type::~ctrl_config_type() {};

cdns_xspi_ctrl_regBase::ctrl_config_type::ctrl_config_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cont_on_err(sc_core::sc_gen_unique_name("cont_on_err", true), *this, 3, 1),
    cont_on_err_reset_value(0x0),
    work_mode(sc_core::sc_gen_unique_name("work_mode", true), *this, 5, 2),
    work_mode_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::dma_settings_type::~dma_settings_type() {};

cdns_xspi_ctrl_regBase::dma_settings_type::dma_settings_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    burst_sel(sc_core::sc_gen_unique_name("burst_sel", true), *this, 0, 8),
    burst_sel_reset_value(0x0),
    OTE(sc_core::sc_gen_unique_name("OTE", true), *this, 16, 1),
    OTE_reset_value(0x1),
    sdma_err_rsp(sc_core::sc_gen_unique_name("sdma_err_rsp", true), *this, 17, 1),
    sdma_err_rsp_reset_value(0x0),
    word_size(sc_core::sc_gen_unique_name("word_size", true), *this, 18, 2),
    word_size_reset_value(0x3) {}

cdns_xspi_ctrl_regBase::sdma_size_type::~sdma_size_type() {};

cdns_xspi_ctrl_regBase::sdma_size_type::sdma_size_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    sdma_size(sc_core::sc_gen_unique_name("sdma_size", true), *this, 0, 32),
    sdma_size_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::sdma_trd_info_type::~sdma_trd_info_type() {};

cdns_xspi_ctrl_regBase::sdma_trd_info_type::sdma_trd_info_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    sdma_trd(sc_core::sc_gen_unique_name("sdma_trd", true), *this, 0, 3),
    sdma_trd_reset_value(0x0),
    sdma_dir(sc_core::sc_gen_unique_name("sdma_dir", true), *this, 8, 1),
    sdma_dir_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::sdma_addr0_type::~sdma_addr0_type() {};

cdns_xspi_ctrl_regBase::sdma_addr0_type::sdma_addr0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    sdma_addr_l(sc_core::sc_gen_unique_name("sdma_addr_l", true), *this, 0, 32),
    sdma_addr_l_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::sdma_addr1_type::~sdma_addr1_type() {};

cdns_xspi_ctrl_regBase::sdma_addr1_type::sdma_addr1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    sdma_addr_h(sc_core::sc_gen_unique_name("sdma_addr_h", true), *this, 0, 32),
    sdma_addr_h_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::discovery_control_type::~discovery_control_type() {};

cdns_xspi_ctrl_regBase::discovery_control_type::discovery_control_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    discovery_req(sc_core::sc_gen_unique_name("discovery_req", true), *this, 0, 1),
    discovery_req_reset_value(0x0),
    discovery_req_type(sc_core::sc_gen_unique_name("discovery_req_type", true), *this, 1, 1),
    discovery_req_type_reset_value(0x0),
    discovery_comp(sc_core::sc_gen_unique_name("discovery_comp", true), *this, 2, 1),
    discovery_comp_reset_value(0x0),
    discovery_fail(sc_core::sc_gen_unique_name("discovery_fail", true), *this, 3, 2),
    discovery_fail_reset_value(0x0),
    discovery_inhibit(sc_core::sc_gen_unique_name("discovery_inhibit", true), *this, 5, 1),
    discovery_inhibit_reset_value(0x0),
    discovery_extop_val(sc_core::sc_gen_unique_name("discovery_extop_val", true), *this, 6, 1),
    discovery_extop_val_reset_value(0x0),
    discovery_extop_en(sc_core::sc_gen_unique_name("discovery_extop_en", true), *this, 7, 1),
    discovery_extop_en_reset_value(0x0),
    discovery_cmd_type(sc_core::sc_gen_unique_name("discovery_cmd_type", true), *this, 8, 2),
    discovery_cmd_type_reset_value(0x0),
    discovery_dummy_cnt(sc_core::sc_gen_unique_name("discovery_dummy_cnt", true), *this, 10, 1),
    discovery_dummy_cnt_reset_value(0x0),
    discovery_abnum(sc_core::sc_gen_unique_name("discovery_abnum", true), *this, 11, 1),
    discovery_abnum_reset_value(0x0),
    discovery_num_lines(sc_core::sc_gen_unique_name("discovery_num_lines", true), *this, 12, 4),
    discovery_num_lines_reset_value(0x0),
    discovery_bank(sc_core::sc_gen_unique_name("discovery_bank", true), *this, 16, 3),
    discovery_bank_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::xip_mode_cfg_type::~xip_mode_cfg_type() {};

cdns_xspi_ctrl_regBase::xip_mode_cfg_type::xip_mode_cfg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    xip_en(sc_core::sc_gen_unique_name("xip_en", true), *this, 0, 8),
    xip_en_reset_value(0x0),
    xip_en_mb_val(sc_core::sc_gen_unique_name("xip_en_mb_val", true), *this, 8, 8),
    xip_en_mb_val_reset_value(0x0),
    xip_dis_mb_val(sc_core::sc_gen_unique_name("xip_dis_mb_val", true), *this, 16, 8),
    xip_dis_mb_val_reset_value(0xff) {}

cdns_xspi_ctrl_regBase::global_seq_cfg_type::~global_seq_cfg_type() {};

cdns_xspi_ctrl_regBase::global_seq_cfg_type::global_seq_cfg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    seq_page_size_rd(sc_core::sc_gen_unique_name("seq_page_size_rd", true), *this, 0, 4),
    seq_page_size_rd_reset_value(0xf),
    seq_page_size_pgm(sc_core::sc_gen_unique_name("seq_page_size_pgm", true), *this, 4, 4),
    seq_page_size_pgm_reset_value(0x8),
    seq_crc_en(sc_core::sc_gen_unique_name("seq_crc_en", true), *this, 8, 1),
    seq_crc_en_reset_value(0x0),
    seq_crc_variant(sc_core::sc_gen_unique_name("seq_crc_variant", true), *this, 9, 1),
    seq_crc_variant_reset_value(0x0),
    seq_crc_oe(sc_core::sc_gen_unique_name("seq_crc_oe", true), *this, 10, 1),
    seq_crc_oe_reset_value(0x0),
    seq_crc_chunk_size(sc_core::sc_gen_unique_name("seq_crc_chunk_size", true), *this, 12, 3),
    seq_crc_chunk_size_reset_value(0x2),
    seq_crc_ual_chunk_en(sc_core::sc_gen_unique_name("seq_crc_ual_chunk_en", true), *this, 16, 1),
    seq_crc_ual_chunk_en_reset_value(0x0),
    seq_crc_ual_chunk_chk(sc_core::sc_gen_unique_name("seq_crc_ual_chunk_chk", true), *this, 17, 1),
    seq_crc_ual_chunk_chk_reset_value(0x0),
    seq_tcms_en(sc_core::sc_gen_unique_name("seq_tcms_en", true), *this, 18, 1),
    seq_tcms_en_reset_value(0x0),
    seq_data_swap(sc_core::sc_gen_unique_name("seq_data_swap", true), *this, 20, 1),
    seq_data_swap_reset_value(0x0),
    seq_data_per_addr(sc_core::sc_gen_unique_name("seq_data_per_addr", true), *this, 21, 1),
    seq_data_per_addr_reset_value(0x0),
    seq_type(sc_core::sc_gen_unique_name("seq_type", true), *this, 23, 2),
    seq_type_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::global_seq_cfg_1_type::~global_seq_cfg_1_type() {};

cdns_xspi_ctrl_regBase::global_seq_cfg_1_type::global_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    seq_page_size_ext(sc_core::sc_gen_unique_name("seq_page_size_ext", true), *this, 0, 9),
    seq_page_size_ext_reset_value(0x0),
    seq_page_ca_size(sc_core::sc_gen_unique_name("seq_page_ca_size", true), *this, 16, 1),
    seq_page_ca_size_reset_value(0x0),
    seq_page_per_block(sc_core::sc_gen_unique_name("seq_page_per_block", true), *this, 24, 3),
    seq_page_per_block_reset_value(0x0),
    seq_plane_cnt(sc_core::sc_gen_unique_name("seq_plane_cnt", true), *this, 28, 2),
    seq_plane_cnt_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::direct_access_cfg_type::~direct_access_cfg_type() {};

cdns_xspi_ctrl_regBase::direct_access_cfg_type::direct_access_cfg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    dac_bank_num(sc_core::sc_gen_unique_name("dac_bank_num", true), *this, 0, 3),
    dac_bank_num_reset_value(0x0),
    rwds_cap_en(sc_core::sc_gen_unique_name("rwds_cap_en", true), *this, 4, 1),
    rwds_cap_en_reset_value(0x0),
    mode_bit_xip_en(sc_core::sc_gen_unique_name("mode_bit_xip_en", true), *this, 8, 1),
    mode_bit_xip_en_reset_value(0x0),
    mode_bit_xip_dis(sc_core::sc_gen_unique_name("mode_bit_xip_dis", true), *this, 9, 1),
    mode_bit_xip_dis_reset_value(0x0),
    rmp_addr_en(sc_core::sc_gen_unique_name("rmp_addr_en", true), *this, 12, 1),
    rmp_addr_en_reset_value(0x0),
    dac_addr_mask(sc_core::sc_gen_unique_name("dac_addr_mask", true), *this, 16, 13),
    dac_addr_mask_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::direct_access_rmp_type::~direct_access_rmp_type() {};

cdns_xspi_ctrl_regBase::direct_access_rmp_type::direct_access_rmp_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    rmp_addr_val(sc_core::sc_gen_unique_name("rmp_addr_val", true), *this, 0, 32),
    rmp_addr_val_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::direct_access_rmp_1_type::~direct_access_rmp_1_type() {};

cdns_xspi_ctrl_regBase::direct_access_rmp_1_type::direct_access_rmp_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    rmp_addr_val_1(sc_core::sc_gen_unique_name("rmp_addr_val_1", true), *this, 0, 32),
    rmp_addr_val_1_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::rst_seq_cfg_0_type::~rst_seq_cfg_0_type() {};

cdns_xspi_ctrl_regBase::rst_seq_cfg_0_type::rst_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    rst_seq_p1_cmd0_val(sc_core::sc_gen_unique_name("rst_seq_p1_cmd0_val", true), *this, 0, 8),
    rst_seq_p1_cmd0_val_reset_value(0x66),
    rst_seq_p1_cmd1_val(sc_core::sc_gen_unique_name("rst_seq_p1_cmd1_val", true), *this, 8, 8),
    rst_seq_p1_cmd1_val_reset_value(0x99),
    rst_seq_p1_cmd0_en(sc_core::sc_gen_unique_name("rst_seq_p1_cmd0_en", true), *this, 16, 1),
    rst_seq_p1_cmd0_en_reset_value(0x1),
    rst_seq_p1_data_ios(sc_core::sc_gen_unique_name("rst_seq_p1_data_ios", true), *this, 18, 2),
    rst_seq_p1_data_ios_reset_value(0x0),
    rst_seq_p1_data_edge(sc_core::sc_gen_unique_name("rst_seq_p1_data_edge", true), *this, 21, 1),
    rst_seq_p1_data_edge_reset_value(0x0),
    rst_seq_p1_data_en(sc_core::sc_gen_unique_name("rst_seq_p1_data_en", true), *this, 22, 1),
    rst_seq_p1_data_en_reset_value(0x0),
    rst_seq_p1_cmd_ios(sc_core::sc_gen_unique_name("rst_seq_p1_cmd_ios", true), *this, 24, 2),
    rst_seq_p1_cmd_ios_reset_value(0x0),
    rst_seq_p1_cmd_edge(sc_core::sc_gen_unique_name("rst_seq_p1_cmd_edge", true), *this, 28, 1),
    rst_seq_p1_cmd_edge_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::rst_seq_cfg_1_type::~rst_seq_cfg_1_type() {};

cdns_xspi_ctrl_regBase::rst_seq_cfg_1_type::rst_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    rst_seq_p1_cmd0_ext_en(sc_core::sc_gen_unique_name("rst_seq_p1_cmd0_ext_en", true), *this, 0, 1),
    rst_seq_p1_cmd0_ext_en_reset_value(0x0),
    rst_seq_p1_cmd1_ext_en(sc_core::sc_gen_unique_name("rst_seq_p1_cmd1_ext_en", true), *this, 1, 1),
    rst_seq_p1_cmd1_ext_en_reset_value(0x0),
    rst_seq_p1_cmd0_ext_val(sc_core::sc_gen_unique_name("rst_seq_p1_cmd0_ext_val", true), *this, 8, 8),
    rst_seq_p1_cmd0_ext_val_reset_value(0x99),
    rst_seq_p1_cmd1_ext_val(sc_core::sc_gen_unique_name("rst_seq_p1_cmd1_ext_val", true), *this, 16, 8),
    rst_seq_p1_cmd1_ext_val_reset_value(0x66),
    rst_seq_p1_data_val(sc_core::sc_gen_unique_name("rst_seq_p1_data_val", true), *this, 24, 8),
    rst_seq_p1_data_val_reset_value(0xd0) {}

cdns_xspi_ctrl_regBase::ers_seq_cfg_0_type::~ers_seq_cfg_0_type() {};

cdns_xspi_ctrl_regBase::ers_seq_cfg_0_type::ers_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    erss_seq_p1_cmd_val(sc_core::sc_gen_unique_name("erss_seq_p1_cmd_val", true), *this, 0, 8),
    erss_seq_p1_cmd_val_reset_value(0x20),
    erss_seq_p1_cmd_ios(sc_core::sc_gen_unique_name("erss_seq_p1_cmd_ios", true), *this, 8, 2),
    erss_seq_p1_cmd_ios_reset_value(0x0),
    erss_seq_p1_cmd_edge(sc_core::sc_gen_unique_name("erss_seq_p1_cmd_edge", true), *this, 11, 1),
    erss_seq_p1_cmd_edge_reset_value(0x0),
    erss_seq_p1_addr_cnt(sc_core::sc_gen_unique_name("erss_seq_p1_addr_cnt", true), *this, 12, 3),
    erss_seq_p1_addr_cnt_reset_value(0x3),
    erss_seq_p1_cmd_ext_en(sc_core::sc_gen_unique_name("erss_seq_p1_cmd_ext_en", true), *this, 15, 1),
    erss_seq_p1_cmd_ext_en_reset_value(0x0),
    erss_seq_p1_cmd_ext_val(sc_core::sc_gen_unique_name("erss_seq_p1_cmd_ext_val", true), *this, 16, 8),
    erss_seq_p1_cmd_ext_val_reset_value(0xdf),
    erss_seq_p1_addr_ios(sc_core::sc_gen_unique_name("erss_seq_p1_addr_ios", true), *this, 24, 2),
    erss_seq_p1_addr_ios_reset_value(0x0),
    erss_seq_p1_addr_edge(sc_core::sc_gen_unique_name("erss_seq_p1_addr_edge", true), *this, 28, 1),
    erss_seq_p1_addr_edge_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::ers_seq_cfg_1_type::~ers_seq_cfg_1_type() {};

cdns_xspi_ctrl_regBase::ers_seq_cfg_1_type::ers_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    erss_seq_p1_sect_size(sc_core::sc_gen_unique_name("erss_seq_p1_sect_size", true), *this, 0, 5),
    erss_seq_p1_sect_size_reset_value(0xc) {}

cdns_xspi_ctrl_regBase::ers_seq_cfg_2_type::~ers_seq_cfg_2_type() {};

cdns_xspi_ctrl_regBase::ers_seq_cfg_2_type::ers_seq_cfg_2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    ersa_seq_p1_cmd_val(sc_core::sc_gen_unique_name("ersa_seq_p1_cmd_val", true), *this, 0, 8),
    ersa_seq_p1_cmd_val_reset_value(0x60),
    ersa_seq_p1_cmd_ios(sc_core::sc_gen_unique_name("ersa_seq_p1_cmd_ios", true), *this, 8, 2),
    ersa_seq_p1_cmd_ios_reset_value(0x0),
    ersa_seq_p1_cmd_edge(sc_core::sc_gen_unique_name("ersa_seq_p1_cmd_edge", true), *this, 11, 1),
    ersa_seq_p1_cmd_edge_reset_value(0x0),
    ersa_seq_p1_cmd_ext_en(sc_core::sc_gen_unique_name("ersa_seq_p1_cmd_ext_en", true), *this, 15, 1),
    ersa_seq_p1_cmd_ext_en_reset_value(0x0),
    ersa_seq_p1_cmd_ext_val(sc_core::sc_gen_unique_name("ersa_seq_p1_cmd_ext_val", true), *this, 16, 8),
    ersa_seq_p1_cmd_ext_val_reset_value(0x9f) {}

cdns_xspi_ctrl_regBase::prog_seq_cfg_0_type::~prog_seq_cfg_0_type() {};

cdns_xspi_ctrl_regBase::prog_seq_cfg_0_type::prog_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    prog_seq_p1_cmd_val(sc_core::sc_gen_unique_name("prog_seq_p1_cmd_val", true), *this, 0, 8),
    prog_seq_p1_cmd_val_reset_value(0x2),
    prog_seq_p1_cmd_ios(sc_core::sc_gen_unique_name("prog_seq_p1_cmd_ios", true), *this, 8, 2),
    prog_seq_p1_cmd_ios_reset_value(0x0),
    prog_seq_p1_cmd_edge(sc_core::sc_gen_unique_name("prog_seq_p1_cmd_edge", true), *this, 11, 1),
    prog_seq_p1_cmd_edge_reset_value(0x0),
    prog_seq_p1_addr_cnt(sc_core::sc_gen_unique_name("prog_seq_p1_addr_cnt", true), *this, 12, 3),
    prog_seq_p1_addr_cnt_reset_value(0x3),
    prog_seq_p1_addr_ios(sc_core::sc_gen_unique_name("prog_seq_p1_addr_ios", true), *this, 16, 2),
    prog_seq_p1_addr_ios_reset_value(0x0),
    prog_seq_p1_addr_edge(sc_core::sc_gen_unique_name("prog_seq_p1_addr_edge", true), *this, 19, 1),
    prog_seq_p1_addr_edge_reset_value(0x0),
    prog_seq_p1_data_ios(sc_core::sc_gen_unique_name("prog_seq_p1_data_ios", true), *this, 20, 2),
    prog_seq_p1_data_ios_reset_value(0x0),
    prog_seq_p1_data_edge(sc_core::sc_gen_unique_name("prog_seq_p1_data_edge", true), *this, 23, 1),
    prog_seq_p1_data_edge_reset_value(0x0),
    prog_seq_p1_dummy_cnt(sc_core::sc_gen_unique_name("prog_seq_p1_dummy_cnt", true), *this, 24, 6),
    prog_seq_p1_dummy_cnt_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::prog_seq_cfg_1_type::~prog_seq_cfg_1_type() {};

cdns_xspi_ctrl_regBase::prog_seq_cfg_1_type::prog_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    prog_seq_p1_cmd_ext_en(sc_core::sc_gen_unique_name("prog_seq_p1_cmd_ext_en", true), *this, 0, 1),
    prog_seq_p1_cmd_ext_en_reset_value(0x0),
    prog_seq_p1_cmd_ext_val(sc_core::sc_gen_unique_name("prog_seq_p1_cmd_ext_val", true), *this, 8, 8),
    prog_seq_p1_cmd_ext_val_reset_value(0xfd) {}

cdns_xspi_ctrl_regBase::prog_seq_cfg_2_type::~prog_seq_cfg_2_type() {};

cdns_xspi_ctrl_regBase::prog_seq_cfg_2_type::prog_seq_cfg_2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    prog_seq_p2_target(sc_core::sc_gen_unique_name("prog_seq_p2_target", true), *this, 0, 1),
    prog_seq_p2_target_reset_value(0x0),
    prog_seq_p2_burst_type(sc_core::sc_gen_unique_name("prog_seq_p2_burst_type", true), *this, 1, 1),
    prog_seq_p2_burst_type_reset_value(0x1),
    prog_seq_p2_mask_cmd_mod(sc_core::sc_gen_unique_name("prog_seq_p2_mask_cmd_mod", true), *this, 2, 1),
    prog_seq_p2_mask_cmd_mod_reset_value(0x0),
    prog_seq_p2_latency_cnt(sc_core::sc_gen_unique_name("prog_seq_p2_latency_cnt", true), *this, 8, 6),
    prog_seq_p2_latency_cnt_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::read_seq_cfg_0_type::~read_seq_cfg_0_type() {};

cdns_xspi_ctrl_regBase::read_seq_cfg_0_type::read_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    read_seq_p1_cmd_val(sc_core::sc_gen_unique_name("read_seq_p1_cmd_val", true), *this, 0, 8),
    read_seq_p1_cmd_val_reset_value(0x3),
    read_seq_p1_cmd_ios(sc_core::sc_gen_unique_name("read_seq_p1_cmd_ios", true), *this, 8, 2),
    read_seq_p1_cmd_ios_reset_value(0x0),
    read_seq_p1_cmd_edge(sc_core::sc_gen_unique_name("read_seq_p1_cmd_edge", true), *this, 11, 1),
    read_seq_p1_cmd_edge_reset_value(0x0),
    read_seq_p1_addr_cnt(sc_core::sc_gen_unique_name("read_seq_p1_addr_cnt", true), *this, 12, 3),
    read_seq_p1_addr_cnt_reset_value(0x3),
    read_seq_p1_addr_ios(sc_core::sc_gen_unique_name("read_seq_p1_addr_ios", true), *this, 16, 2),
    read_seq_p1_addr_ios_reset_value(0x0),
    read_seq_p1_addr_edge(sc_core::sc_gen_unique_name("read_seq_p1_addr_edge", true), *this, 19, 1),
    read_seq_p1_addr_edge_reset_value(0x0),
    read_seq_p1_data_ios(sc_core::sc_gen_unique_name("read_seq_p1_data_ios", true), *this, 20, 2),
    read_seq_p1_data_ios_reset_value(0x0),
    read_seq_p1_data_edge(sc_core::sc_gen_unique_name("read_seq_p1_data_edge", true), *this, 23, 1),
    read_seq_p1_data_edge_reset_value(0x0),
    read_seq_p1_dummy_cnt(sc_core::sc_gen_unique_name("read_seq_p1_dummy_cnt", true), *this, 24, 6),
    read_seq_p1_dummy_cnt_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::read_seq_cfg_1_type::~read_seq_cfg_1_type() {};

cdns_xspi_ctrl_regBase::read_seq_cfg_1_type::read_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    read_seq_p1_cmd_ext_en(sc_core::sc_gen_unique_name("read_seq_p1_cmd_ext_en", true), *this, 0, 1),
    read_seq_p1_cmd_ext_en_reset_value(0x0),
    read_seq_p1_cache_random_read_en(sc_core::sc_gen_unique_name("read_seq_p1_cache_random_read_en", true), *this, 4, 1),
    read_seq_p1_cache_random_read_en_reset_value(0x0),
    read_seq_p1_cmd_ext_val(sc_core::sc_gen_unique_name("read_seq_p1_cmd_ext_val", true), *this, 8, 8),
    read_seq_p1_cmd_ext_val_reset_value(0xfc),
    read_seq_p1_mb_dummy_cnt(sc_core::sc_gen_unique_name("read_seq_p1_mb_dummy_cnt", true), *this, 24, 6),
    read_seq_p1_mb_dummy_cnt_reset_value(0x0),
    read_seq_p1_mb_en(sc_core::sc_gen_unique_name("read_seq_p1_mb_en", true), *this, 31, 1),
    read_seq_p1_mb_en_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::read_seq_cfg_2_type::~read_seq_cfg_2_type() {};

cdns_xspi_ctrl_regBase::read_seq_cfg_2_type::read_seq_cfg_2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    read_seq_p2_target(sc_core::sc_gen_unique_name("read_seq_p2_target", true), *this, 0, 1),
    read_seq_p2_target_reset_value(0x0),
    read_seq_p2_burst_type(sc_core::sc_gen_unique_name("read_seq_p2_burst_type", true), *this, 1, 1),
    read_seq_p2_burst_type_reset_value(0x1),
    read_seq_p2_mask_cmd_mod(sc_core::sc_gen_unique_name("read_seq_p2_mask_cmd_mod", true), *this, 2, 1),
    read_seq_p2_mask_cmd_mod_reset_value(0x0),
    read_seq_p2_hf_bound_en(sc_core::sc_gen_unique_name("read_seq_p2_hf_bound_en", true), *this, 3, 1),
    read_seq_p2_hf_bound_en_reset_value(0x1),
    read_seq_p2_latency_cnt(sc_core::sc_gen_unique_name("read_seq_p2_latency_cnt", true), *this, 8, 6),
    read_seq_p2_latency_cnt_reset_value(0xf) {}

cdns_xspi_ctrl_regBase::we_seq_cfg_0_type::~we_seq_cfg_0_type() {};

cdns_xspi_ctrl_regBase::we_seq_cfg_0_type::we_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    we_seq_p1_cmd_val(sc_core::sc_gen_unique_name("we_seq_p1_cmd_val", true), *this, 0, 8),
    we_seq_p1_cmd_val_reset_value(0x6),
    we_seq_p1_cmd_ios(sc_core::sc_gen_unique_name("we_seq_p1_cmd_ios", true), *this, 8, 2),
    we_seq_p1_cmd_ios_reset_value(0x0),
    we_seq_p1_cmd_edge(sc_core::sc_gen_unique_name("we_seq_p1_cmd_edge", true), *this, 11, 1),
    we_seq_p1_cmd_edge_reset_value(0x0),
    we_seq_p1_cmd_ext_en(sc_core::sc_gen_unique_name("we_seq_p1_cmd_ext_en", true), *this, 15, 1),
    we_seq_p1_cmd_ext_en_reset_value(0x0),
    we_seq_p1_cmd_ext_val(sc_core::sc_gen_unique_name("we_seq_p1_cmd_ext_val", true), *this, 16, 8),
    we_seq_p1_cmd_ext_val_reset_value(0xf9),
    we_seq_p1_en(sc_core::sc_gen_unique_name("we_seq_p1_en", true), *this, 24, 1),
    we_seq_p1_en_reset_value(0x1) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_0_type::~stat_seq_cfg_0_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_0_type::stat_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_p1_cmd_ios(sc_core::sc_gen_unique_name("stat_seq_p1_cmd_ios", true), *this, 0, 2),
    stat_seq_p1_cmd_ios_reset_value(0x0),
    stat_seq_p1_cmd_edge(sc_core::sc_gen_unique_name("stat_seq_p1_cmd_edge", true), *this, 4, 1),
    stat_seq_p1_cmd_edge_reset_value(0x0),
    stat_seq_p1_cmd_ext_en(sc_core::sc_gen_unique_name("stat_seq_p1_cmd_ext_en", true), *this, 5, 1),
    stat_seq_p1_cmd_ext_en_reset_value(0x0),
    stat_seq_p1_addr_cnt(sc_core::sc_gen_unique_name("stat_seq_p1_addr_cnt", true), *this, 8, 2),
    stat_seq_p1_addr_cnt_reset_value(0x0),
    stat_seq_p1_addr_ios(sc_core::sc_gen_unique_name("stat_seq_p1_addr_ios", true), *this, 10, 2),
    stat_seq_p1_addr_ios_reset_value(0x0),
    stat_seq_p1_addr_edge(sc_core::sc_gen_unique_name("stat_seq_p1_addr_edge", true), *this, 12, 1),
    stat_seq_p1_addr_edge_reset_value(0x0),
    stat_seq_p1_data_ios(sc_core::sc_gen_unique_name("stat_seq_p1_data_ios", true), *this, 20, 2),
    stat_seq_p1_data_ios_reset_value(0x0),
    stat_seq_p1_data_edge(sc_core::sc_gen_unique_name("stat_seq_p1_data_edge", true), *this, 22, 1),
    stat_seq_p1_data_edge_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_1_type::~stat_seq_cfg_1_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_1_type::stat_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_p1_dev_rdy_dummy_cnt(sc_core::sc_gen_unique_name("stat_seq_p1_dev_rdy_dummy_cnt", true), *this, 0, 6),
    stat_seq_p1_dev_rdy_dummy_cnt_reset_value(0x0),
    stat_seq_p1_dev_rdy_addr_en(sc_core::sc_gen_unique_name("stat_seq_p1_dev_rdy_addr_en", true), *this, 6, 1),
    stat_seq_p1_dev_rdy_addr_en_reset_value(0x0),
    stat_seq_p1_prog_fail_dummy_cnt(sc_core::sc_gen_unique_name("stat_seq_p1_prog_fail_dummy_cnt", true), *this, 16, 6),
    stat_seq_p1_prog_fail_dummy_cnt_reset_value(0x0),
    stat_seq_p1_prog_fail_addr_en(sc_core::sc_gen_unique_name("stat_seq_p1_prog_fail_addr_en", true), *this, 22, 1),
    stat_seq_p1_prog_fail_addr_en_reset_value(0x0),
    stat_seq_p1_ers_fail_dummy_cnt(sc_core::sc_gen_unique_name("stat_seq_p1_ers_fail_dummy_cnt", true), *this, 24, 6),
    stat_seq_p1_ers_fail_dummy_cnt_reset_value(0x0),
    stat_seq_p1_ers_fail_addr_en(sc_core::sc_gen_unique_name("stat_seq_p1_ers_fail_addr_en", true), *this, 30, 1),
    stat_seq_p1_ers_fail_addr_en_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_2_type::~stat_seq_cfg_2_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_2_type::stat_seq_cfg_2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_p1_dev_rdy_cmd_val(sc_core::sc_gen_unique_name("stat_seq_p1_dev_rdy_cmd_val", true), *this, 0, 8),
    stat_seq_p1_dev_rdy_cmd_val_reset_value(0x5),
    stat_seq_p1_ers_fail_cmd_val(sc_core::sc_gen_unique_name("stat_seq_p1_ers_fail_cmd_val", true), *this, 8, 8),
    stat_seq_p1_ers_fail_cmd_val_reset_value(0x5),
    stat_seq_p1_prog_fail_cmd_val(sc_core::sc_gen_unique_name("stat_seq_p1_prog_fail_cmd_val", true), *this, 24, 8),
    stat_seq_p1_prog_fail_cmd_val_reset_value(0x5) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_3_type::~stat_seq_cfg_3_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_3_type::stat_seq_cfg_3_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_p1_dev_rdy_cmd_ext_val(sc_core::sc_gen_unique_name("stat_seq_p1_dev_rdy_cmd_ext_val", true), *this, 0, 8),
    stat_seq_p1_dev_rdy_cmd_ext_val_reset_value(0xfa),
    stat_seq_p1_ers_fail_cmd_ext_val(sc_core::sc_gen_unique_name("stat_seq_p1_ers_fail_cmd_ext_val", true), *this, 8, 8),
    stat_seq_p1_ers_fail_cmd_ext_val_reset_value(0xfa),
    stat_seq_p1_prog_fail_cmd_ext_val(sc_core::sc_gen_unique_name("stat_seq_p1_prog_fail_cmd_ext_val", true), *this, 24, 8),
    stat_seq_p1_prog_fail_cmd_ext_val_reset_value(0xfa) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_4_type::~stat_seq_cfg_4_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_4_type::stat_seq_cfg_4_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_p2_mask_cmd_mod(sc_core::sc_gen_unique_name("stat_seq_p2_mask_cmd_mod", true), *this, 2, 1),
    stat_seq_p2_mask_cmd_mod_reset_value(0x0),
    stat_seq_p2_latency_cnt(sc_core::sc_gen_unique_name("stat_seq_p2_latency_cnt", true), *this, 8, 6),
    stat_seq_p2_latency_cnt_reset_value(0xf) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_5_type::~stat_seq_cfg_5_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_5_type::stat_seq_cfg_5_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_dev_rdy_idx(sc_core::sc_gen_unique_name("stat_seq_dev_rdy_idx", true), *this, 0, 4),
    stat_seq_dev_rdy_idx_reset_value(0x0),
    stat_seq_dev_rdy_val(sc_core::sc_gen_unique_name("stat_seq_dev_rdy_val", true), *this, 4, 1),
    stat_seq_dev_rdy_val_reset_value(0x0),
    stat_seq_dev_rdy_size(sc_core::sc_gen_unique_name("stat_seq_dev_rdy_size", true), *this, 5, 1),
    stat_seq_dev_rdy_size_reset_value(0x0),
    stat_seq_dev_rdy_en(sc_core::sc_gen_unique_name("stat_seq_dev_rdy_en", true), *this, 6, 1),
    stat_seq_dev_rdy_en_reset_value(0x1),
    stat_seq_ers_fail_idx(sc_core::sc_gen_unique_name("stat_seq_ers_fail_idx", true), *this, 8, 4),
    stat_seq_ers_fail_idx_reset_value(0x0),
    stat_seq_ers_fail_val(sc_core::sc_gen_unique_name("stat_seq_ers_fail_val", true), *this, 12, 1),
    stat_seq_ers_fail_val_reset_value(0x0),
    stat_seq_ers_fail_size(sc_core::sc_gen_unique_name("stat_seq_ers_fail_size", true), *this, 13, 1),
    stat_seq_ers_fail_size_reset_value(0x0),
    stat_seq_ers_fail_en(sc_core::sc_gen_unique_name("stat_seq_ers_fail_en", true), *this, 14, 1),
    stat_seq_ers_fail_en_reset_value(0x0),
    stat_seq_prog_fail_idx(sc_core::sc_gen_unique_name("stat_seq_prog_fail_idx", true), *this, 24, 4),
    stat_seq_prog_fail_idx_reset_value(0x0),
    stat_seq_prog_fail_val(sc_core::sc_gen_unique_name("stat_seq_prog_fail_val", true), *this, 28, 1),
    stat_seq_prog_fail_val_reset_value(0x0),
    stat_seq_prog_fail_size(sc_core::sc_gen_unique_name("stat_seq_prog_fail_size", true), *this, 29, 1),
    stat_seq_prog_fail_size_reset_value(0x0),
    stat_seq_prog_fail_en(sc_core::sc_gen_unique_name("stat_seq_prog_fail_en", true), *this, 30, 1),
    stat_seq_prog_fail_en_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_7_type::~stat_seq_cfg_7_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_7_type::stat_seq_cfg_7_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_dev_rdy_addr(sc_core::sc_gen_unique_name("stat_seq_dev_rdy_addr", true), *this, 0, 32),
    stat_seq_dev_rdy_addr_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_8_type::~stat_seq_cfg_8_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_8_type::stat_seq_cfg_8_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_prog_fail_addr(sc_core::sc_gen_unique_name("stat_seq_prog_fail_addr", true), *this, 0, 32),
    stat_seq_prog_fail_addr_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_9_type::~stat_seq_cfg_9_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_9_type::stat_seq_cfg_9_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_ers_fail_addr(sc_core::sc_gen_unique_name("stat_seq_ers_fail_addr", true), *this, 0, 32),
    stat_seq_ers_fail_addr_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::stat_seq_cfg_10_type::~stat_seq_cfg_10_type() {};

cdns_xspi_ctrl_regBase::stat_seq_cfg_10_type::stat_seq_cfg_10_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    stat_seq_ecc_fail_mask(sc_core::sc_gen_unique_name("stat_seq_ecc_fail_mask", true), *this, 0, 8),
    stat_seq_ecc_fail_mask_reset_value(0x0),
    stat_seq_ecc_fail_val(sc_core::sc_gen_unique_name("stat_seq_ecc_fail_val", true), *this, 8, 8),
    stat_seq_ecc_fail_val_reset_value(0x0),
    stat_seq_ecc_corr_val(sc_core::sc_gen_unique_name("stat_seq_ecc_corr_val", true), *this, 16, 8),
    stat_seq_ecc_corr_val_reset_value(0x0),
    stat_seq_crdy_idx(sc_core::sc_gen_unique_name("stat_seq_crdy_idx", true), *this, 24, 3),
    stat_seq_crdy_idx_reset_value(0x0),
    stat_seq_crdy_val(sc_core::sc_gen_unique_name("stat_seq_crdy_val", true), *this, 27, 1),
    stat_seq_crdy_val_reset_value(0x0),
    stat_seq_ecc_fail_en(sc_core::sc_gen_unique_name("stat_seq_ecc_fail_en", true), *this, 31, 1),
    stat_seq_ecc_fail_en_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::xspi_ctrl_version_type::~xspi_ctrl_version_type() {};

cdns_xspi_ctrl_regBase::xspi_ctrl_version_type::xspi_ctrl_version_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    xspi_ctrl_rev(sc_core::sc_gen_unique_name("xspi_ctrl_rev", true), *this, 0, 8),
    xspi_ctrl_rev_reset_value(0x6),
    xspi_ctrl_fix(sc_core::sc_gen_unique_name("xspi_ctrl_fix", true), *this, 8, 8),
    xspi_ctrl_fix_reset_value(0x2),
    xspi_ctrl_magic_number(sc_core::sc_gen_unique_name("xspi_ctrl_magic_number", true), *this, 16, 16),
    xspi_ctrl_magic_number_reset_value(0x6522) {}

cdns_xspi_ctrl_regBase::ctrl_features_reg_type::~ctrl_features_reg_type() {};

cdns_xspi_ctrl_regBase::ctrl_features_reg_type::ctrl_features_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    n_threads(sc_core::sc_gen_unique_name("n_threads", true), *this, 0, 4),
    n_threads_reset_value(0x3),
    asf_available(sc_core::sc_gen_unique_name("asf_available", true), *this, 12, 1),
    asf_available_reset_value(0x0),
    boot_available(sc_core::sc_gen_unique_name("boot_available", true), *this, 16, 1),
    boot_available_reset_value(0x1),
    dma_intf(sc_core::sc_gen_unique_name("dma_intf", true), *this, 18, 2),
    dma_intf_reset_value(0x0),
    dma_addr_width(sc_core::sc_gen_unique_name("dma_addr_width", true), *this, 20, 1),
    dma_addr_width_reset_value(0x1),
    dma_data_width(sc_core::sc_gen_unique_name("dma_data_width", true), *this, 21, 1),
    dma_data_width_reset_value(0x1),
    sfr_intf(sc_core::sc_gen_unique_name("sfr_intf", true), *this, 22, 2),
    sfr_intf_reset_value(0x1),
    n_banks(sc_core::sc_gen_unique_name("n_banks", true), *this, 24, 2),
    n_banks_reset_value(0x3) {}

cdns_xspi_ctrl_regBase::wp_settings_type::~wp_settings_type() {};

cdns_xspi_ctrl_regBase::wp_settings_type::wp_settings_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    wp(sc_core::sc_gen_unique_name("wp", true), *this, 0, 1),
    wp_reset_value(0x1),
    wp_enable(sc_core::sc_gen_unique_name("wp_enable", true), *this, 1, 1),
    wp_enable_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::reset_pin_settings_type::~reset_pin_settings_type() {};

cdns_xspi_ctrl_regBase::reset_pin_settings_type::reset_pin_settings_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    sw_ctrled_hw_rst(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst", true), *this, 0, 1),
    sw_ctrled_hw_rst_reset_value(0x1),
    rst_dq3_enable(sc_core::sc_gen_unique_name("rst_dq3_enable", true), *this, 1, 1),
    rst_dq3_enable_reset_value(0x0),
    sw_ctrled_hw_rst_option(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_option", true), *this, 4, 1),
    sw_ctrled_hw_rst_option_reset_value(0x0),
    sw_ctrled_hw_rst_bank0(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_bank0", true), *this, 8, 1),
    sw_ctrled_hw_rst_bank0_reset_value(0x0),
    sw_ctrled_hw_rst_bank1(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_bank1", true), *this, 9, 1),
    sw_ctrled_hw_rst_bank1_reset_value(0x0),
    sw_ctrled_hw_rst_bank2(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_bank2", true), *this, 10, 1),
    sw_ctrled_hw_rst_bank2_reset_value(0x0),
    sw_ctrled_hw_rst_bank3(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_bank3", true), *this, 11, 1),
    sw_ctrled_hw_rst_bank3_reset_value(0x0),
    sw_ctrled_hw_rst_bank4(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_bank4", true), *this, 12, 1),
    sw_ctrled_hw_rst_bank4_reset_value(0x0),
    sw_ctrled_hw_rst_bank5(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_bank5", true), *this, 13, 1),
    sw_ctrled_hw_rst_bank5_reset_value(0x0),
    sw_ctrled_hw_rst_bank6(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_bank6", true), *this, 14, 1),
    sw_ctrled_hw_rst_bank6_reset_value(0x0),
    sw_ctrled_hw_rst_bank7(sc_core::sc_gen_unique_name("sw_ctrled_hw_rst_bank7", true), *this, 15, 1),
    sw_ctrled_hw_rst_bank7_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::clock_mode_settings_type::~clock_mode_settings_type() {};

cdns_xspi_ctrl_regBase::clock_mode_settings_type::clock_mode_settings_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    spi_clock_mode(sc_core::sc_gen_unique_name("spi_clock_mode", true), *this, 0, 1),
    spi_clock_mode_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::jedec_rst_timing_reg_type::~jedec_rst_timing_reg_type() {};

cdns_xspi_ctrl_regBase::jedec_rst_timing_reg_type::jedec_rst_timing_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    tCSH_delay(sc_core::sc_gen_unique_name("tCSH_delay", true), *this, 0, 8),
    tCSH_delay_reset_value(0x80),
    tCSL_delay(sc_core::sc_gen_unique_name("tCSL_delay", true), *this, 8, 8),
    tCSL_delay_reset_value(0x80) {}

cdns_xspi_ctrl_regBase::dev_delay_reg_type::~dev_delay_reg_type() {};

cdns_xspi_ctrl_regBase::dev_delay_reg_type::dev_delay_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    cssot_delay(sc_core::sc_gen_unique_name("cssot_delay", true), *this, 0, 8),
    cssot_delay_reset_value(0x0),
    cseot_delay(sc_core::sc_gen_unique_name("cseot_delay", true), *this, 8, 8),
    cseot_delay_reset_value(0x1),
    csda_min_delay(sc_core::sc_gen_unique_name("csda_min_delay", true), *this, 24, 8),
    csda_min_delay_reset_value(0x1) {}

cdns_xspi_ctrl_regBase::rst_recovery_reg_type::~rst_recovery_reg_type() {};

cdns_xspi_ctrl_regBase::rst_recovery_reg_type::rst_recovery_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    rst_recovery(sc_core::sc_gen_unique_name("rst_recovery", true), *this, 0, 32),
    rst_recovery_reset_value(0xa) {}

cdns_xspi_ctrl_regBase::dev_active_max_reg_type::~dev_active_max_reg_type() {};

cdns_xspi_ctrl_regBase::dev_active_max_reg_type::dev_active_max_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    dev_active_max(sc_core::sc_gen_unique_name("dev_active_max", true), *this, 0, 32),
    dev_active_max_reset_value(0x80) {}

cdns_xspi_ctrl_regBase::hf_offset_reg_type::~hf_offset_reg_type() {};

cdns_xspi_ctrl_regBase::hf_offset_reg_type::hf_offset_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    hf_offset_index(sc_core::sc_gen_unique_name("hf_offset_index", true), *this, 0, 6),
    hf_offset_index_reset_value(0x3),
    hf_offset_size(sc_core::sc_gen_unique_name("hf_offset_size", true), *this, 8, 6),
    hf_offset_size_reset_value(0xd) {}

cdns_xspi_ctrl_regBase::dll_phy_update_cnt_type::~dll_phy_update_cnt_type() {};

cdns_xspi_ctrl_regBase::dll_phy_update_cnt_type::dll_phy_update_cnt_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    resync_cnt(sc_core::sc_gen_unique_name("resync_cnt", true), *this, 0, 32),
    resync_cnt_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::dll_phy_ctrl_type::~dll_phy_ctrl_type() {};

cdns_xspi_ctrl_regBase::dll_phy_ctrl_type::dll_phy_ctrl_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    resync_idle_cnt(sc_core::sc_gen_unique_name("resync_idle_cnt", true), *this, 0, 8),
    resync_idle_cnt_reset_value(0x7),
    resync_high_wait_cnt(sc_core::sc_gen_unique_name("resync_high_wait_cnt", true), *this, 8, 4),
    resync_high_wait_cnt_reset_value(0x7),
    extended_rd_mode(sc_core::sc_gen_unique_name("extended_rd_mode", true), *this, 16, 1),
    extended_rd_mode_reset_value(0x1),
    extended_wr_mode(sc_core::sc_gen_unique_name("extended_wr_mode", true), *this, 17, 1),
    extended_wr_mode_reset_value(0x1),
    dqs_last_data_drop_en(sc_core::sc_gen_unique_name("dqs_last_data_drop_en", true), *this, 20, 1),
    dqs_last_data_drop_en_reset_value(0x0),
    sdr_edge_active(sc_core::sc_gen_unique_name("sdr_edge_active", true), *this, 21, 1),
    sdr_edge_active_reset_value(0x0),
    dll_rst_n(sc_core::sc_gen_unique_name("dll_rst_n", true), *this, 24, 1),
    dll_rst_n_reset_value(0x1),
    dfi_ctrlupd_req(sc_core::sc_gen_unique_name("dfi_ctrlupd_req", true), *this, 25, 1),
    dfi_ctrlupd_req_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_dq_timing_reg_type::~phy_dq_timing_reg_type() {};

cdns_xspi_ctrl_regBase::phy_dq_timing_reg_type::phy_dq_timing_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    data_select_oe_end(sc_core::sc_gen_unique_name("data_select_oe_end", true), *this, 0, 3),
    data_select_oe_end_reset_value(0x2),
    data_select_oe_start(sc_core::sc_gen_unique_name("data_select_oe_start", true), *this, 4, 3),
    data_select_oe_start_reset_value(0x0),
    data_select_tsel_end(sc_core::sc_gen_unique_name("data_select_tsel_end", true), *this, 8, 4),
    data_select_tsel_end_reset_value(0x0),
    data_select_tsel_start(sc_core::sc_gen_unique_name("data_select_tsel_start", true), *this, 12, 4),
    data_select_tsel_start_reset_value(0x0),
    data_clkperiod_delay(sc_core::sc_gen_unique_name("data_clkperiod_delay", true), *this, 16, 1),
    data_clkperiod_delay_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_dqs_timing_reg_type::~phy_dqs_timing_reg_type() {};

cdns_xspi_ctrl_regBase::phy_dqs_timing_reg_type::phy_dqs_timing_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    dqs_select_tsel_end(sc_core::sc_gen_unique_name("dqs_select_tsel_end", true), *this, 8, 4),
    dqs_select_tsel_end_reset_value(0x0),
    dqs_select_tsel_start(sc_core::sc_gen_unique_name("dqs_select_tsel_start", true), *this, 12, 4),
    dqs_select_tsel_start_reset_value(0x0),
    phony_dqs_sel(sc_core::sc_gen_unique_name("phony_dqs_sel", true), *this, 16, 1),
    phony_dqs_sel_reset_value(0x0),
    use_phony_dqs(sc_core::sc_gen_unique_name("use_phony_dqs", true), *this, 20, 1),
    use_phony_dqs_reset_value(0x1),
    use_lpbk_dqs(sc_core::sc_gen_unique_name("use_lpbk_dqs", true), *this, 21, 1),
    use_lpbk_dqs_reset_value(0x0),
    use_ext_lpbk_dqs(sc_core::sc_gen_unique_name("use_ext_lpbk_dqs", true), *this, 22, 1),
    use_ext_lpbk_dqs_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_gate_lpbk_ctrl_reg_type::~phy_gate_lpbk_ctrl_reg_type() {};

cdns_xspi_ctrl_regBase::phy_gate_lpbk_ctrl_reg_type::phy_gate_lpbk_ctrl_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    gate_cfg(sc_core::sc_gen_unique_name("gate_cfg", true), *this, 0, 4),
    gate_cfg_reset_value(0x0),
    gate_cfg_close(sc_core::sc_gen_unique_name("gate_cfg_close", true), *this, 4, 2),
    gate_cfg_close_reset_value(0x0),
    gate_cfg_always_on(sc_core::sc_gen_unique_name("gate_cfg_always_on", true), *this, 6, 1),
    gate_cfg_always_on_reset_value(0x0),
    lpbk_en(sc_core::sc_gen_unique_name("lpbk_en", true), *this, 8, 1),
    lpbk_en_reset_value(0x0),
    lpbk_internal(sc_core::sc_gen_unique_name("lpbk_internal", true), *this, 9, 1),
    lpbk_internal_reset_value(0x0),
    loopback_control(sc_core::sc_gen_unique_name("loopback_control", true), *this, 10, 2),
    loopback_control_reset_value(0x0),
    lpbk_fail_muxsel(sc_core::sc_gen_unique_name("lpbk_fail_muxsel", true), *this, 12, 1),
    lpbk_fail_muxsel_reset_value(0x0),
    lpbk_err_check_timing(sc_core::sc_gen_unique_name("lpbk_err_check_timing", true), *this, 13, 3),
    lpbk_err_check_timing_reset_value(0x0),
    rd_del_sel_empty(sc_core::sc_gen_unique_name("rd_del_sel_empty", true), *this, 16, 1),
    rd_del_sel_empty_reset_value(0x0),
    underrun_suppress(sc_core::sc_gen_unique_name("underrun_suppress", true), *this, 18, 1),
    underrun_suppress_reset_value(0x0),
    rd_del_sel(sc_core::sc_gen_unique_name("rd_del_sel", true), *this, 19, 5),
    rd_del_sel_reset_value(0x1b),
    sync_method(sc_core::sc_gen_unique_name("sync_method", true), *this, 31, 1),
    sync_method_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_dll_master_ctrl_reg_type::~phy_dll_master_ctrl_reg_type() {};

cdns_xspi_ctrl_regBase::phy_dll_master_ctrl_reg_type::phy_dll_master_ctrl_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    param_dll_start_point(sc_core::sc_gen_unique_name("param_dll_start_point", true), *this, 0, 8),
    param_dll_start_point_reset_value(0x0),
    param_dll_lock_num(sc_core::sc_gen_unique_name("param_dll_lock_num", true), *this, 16, 3),
    param_dll_lock_num_reset_value(0x0),
    param_phase_detect_sel(sc_core::sc_gen_unique_name("param_phase_detect_sel", true), *this, 20, 3),
    param_phase_detect_sel_reset_value(0x0),
    param_dll_bypass_mode(sc_core::sc_gen_unique_name("param_dll_bypass_mode", true), *this, 23, 1),
    param_dll_bypass_mode_reset_value(0x1) {}

cdns_xspi_ctrl_regBase::phy_dll_slave_ctrl_reg_type::~phy_dll_slave_ctrl_reg_type() {};

cdns_xspi_ctrl_regBase::phy_dll_slave_ctrl_reg_type::phy_dll_slave_ctrl_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    read_dqs_delay(sc_core::sc_gen_unique_name("read_dqs_delay", true), *this, 0, 8),
    read_dqs_delay_reset_value(0x0),
    clk_wr_delay(sc_core::sc_gen_unique_name("clk_wr_delay", true), *this, 8, 8),
    clk_wr_delay_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_ie_timing_reg_type::~phy_ie_timing_reg_type() {};

cdns_xspi_ctrl_regBase::phy_ie_timing_reg_type::phy_ie_timing_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    rddata_en_ie_dly(sc_core::sc_gen_unique_name("rddata_en_ie_dly", true), *this, 0, 4),
    rddata_en_ie_dly_reset_value(0x0),
    dqs_ie_stop(sc_core::sc_gen_unique_name("dqs_ie_stop", true), *this, 4, 3),
    dqs_ie_stop_reset_value(0x0),
    dqs_ie_start(sc_core::sc_gen_unique_name("dqs_ie_start", true), *this, 8, 3),
    dqs_ie_start_reset_value(0x0),
    dq_ie_stop(sc_core::sc_gen_unique_name("dq_ie_stop", true), *this, 12, 3),
    dq_ie_stop_reset_value(0x0),
    dq_ie_start(sc_core::sc_gen_unique_name("dq_ie_start", true), *this, 16, 3),
    dq_ie_start_reset_value(0x0),
    ie_always_on(sc_core::sc_gen_unique_name("ie_always_on", true), *this, 20, 1),
    ie_always_on_reset_value(0x1) {}

cdns_xspi_ctrl_regBase::phy_obs_reg_0_type::~phy_obs_reg_0_type() {};

cdns_xspi_ctrl_regBase::phy_obs_reg_0_type::phy_obs_reg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    lpbk_status(sc_core::sc_gen_unique_name("lpbk_status", true), *this, 0, 2),
    lpbk_status_reset_value(0x0),
    lpbk_dq_data(sc_core::sc_gen_unique_name("lpbk_dq_data", true), *this, 8, 16),
    lpbk_dq_data_reset_value(0x0),
    dqs_underrun(sc_core::sc_gen_unique_name("dqs_underrun", true), *this, 24, 1),
    dqs_underrun_reset_value(0x0),
    dqs_overflow(sc_core::sc_gen_unique_name("dqs_overflow", true), *this, 25, 1),
    dqs_overflow_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_dll_obs_reg_0_type::~phy_dll_obs_reg_0_type() {};

cdns_xspi_ctrl_regBase::phy_dll_obs_reg_0_type::phy_dll_obs_reg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    dll_lock(sc_core::sc_gen_unique_name("dll_lock", true), *this, 0, 1),
    dll_lock_reset_value(0x0),
    dll_locked_mode(sc_core::sc_gen_unique_name("dll_locked_mode", true), *this, 1, 2),
    dll_locked_mode_reset_value(0x0),
    dll_unlock_cnt(sc_core::sc_gen_unique_name("dll_unlock_cnt", true), *this, 3, 5),
    dll_unlock_cnt_reset_value(0x0),
    dll_lock_value(sc_core::sc_gen_unique_name("dll_lock_value", true), *this, 8, 8),
    dll_lock_value_reset_value(0x0),
    lock_dec_dbg(sc_core::sc_gen_unique_name("lock_dec_dbg", true), *this, 16, 8),
    lock_dec_dbg_reset_value(0x0),
    lock_inc_dbg(sc_core::sc_gen_unique_name("lock_inc_dbg", true), *this, 24, 8),
    lock_inc_dbg_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_dll_obs_reg_1_type::~phy_dll_obs_reg_1_type() {};

cdns_xspi_ctrl_regBase::phy_dll_obs_reg_1_type::phy_dll_obs_reg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    decoder_out_rd(sc_core::sc_gen_unique_name("decoder_out_rd", true), *this, 0, 8),
    decoder_out_rd_reset_value(0x0),
    decoder_out_wr(sc_core::sc_gen_unique_name("decoder_out_wr", true), *this, 16, 8),
    decoder_out_wr_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_static_togg_reg_type::~phy_static_togg_reg_type() {};

cdns_xspi_ctrl_regBase::phy_static_togg_reg_type::phy_static_togg_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    static_tog_clk_div(sc_core::sc_gen_unique_name("static_tog_clk_div", true), *this, 0, 16),
    static_tog_clk_div_reset_value(0x0),
    static_togg_global_enable(sc_core::sc_gen_unique_name("static_togg_global_enable", true), *this, 16, 1),
    static_togg_global_enable_reset_value(0x0),
    static_togg_enable(sc_core::sc_gen_unique_name("static_togg_enable", true), *this, 20, 3),
    static_togg_enable_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_wr_deskew_pd_ctrl_0_reg_type::~phy_wr_deskew_pd_ctrl_0_reg_type() {};

cdns_xspi_ctrl_regBase::phy_wr_deskew_pd_ctrl_0_reg_type::phy_wr_deskew_pd_ctrl_0_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    dq_phase_detect_sel(sc_core::sc_gen_unique_name("dq_phase_detect_sel", true), *this, 0, 3),
    dq_phase_detect_sel_reset_value(0x0),
    dq_sw_half_cycle_shift(sc_core::sc_gen_unique_name("dq_sw_half_cycle_shift", true), *this, 4, 1),
    dq_sw_half_cycle_shift_reset_value(0x0),
    dq_en_sw_half_cycle(sc_core::sc_gen_unique_name("dq_en_sw_half_cycle", true), *this, 5, 1),
    dq_en_sw_half_cycle_reset_value(0x0),
    dq_sw_dq_phase_bypass(sc_core::sc_gen_unique_name("dq_sw_dq_phase_bypass", true), *this, 6, 1),
    dq_sw_dq_phase_bypass_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_version_reg_type::~phy_version_reg_type() {};

cdns_xspi_ctrl_regBase::phy_version_reg_type::phy_version_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    phy_rev(sc_core::sc_gen_unique_name("phy_rev", true), *this, 0, 8),
    phy_rev_reset_value(0x7),
    phy_fix(sc_core::sc_gen_unique_name("phy_fix", true), *this, 8, 8),
    phy_fix_reset_value(0x1),
    combo_phy_magic_number(sc_core::sc_gen_unique_name("combo_phy_magic_number", true), *this, 16, 16),
    combo_phy_magic_number_reset_value(0x6182) {}

cdns_xspi_ctrl_regBase::phy_features_reg_type::~phy_features_reg_type() {};

cdns_xspi_ctrl_regBase::phy_features_reg_type::phy_features_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    onfi_40(sc_core::sc_gen_unique_name("onfi_40", true), *this, 0, 1),
    onfi_40_reset_value(0x0),
    onfi_41(sc_core::sc_gen_unique_name("onfi_41", true), *this, 1, 1),
    onfi_41_reset_value(0x0),
    sdr_16bit(sc_core::sc_gen_unique_name("sdr_16bit", true), *this, 2, 1),
    sdr_16bit_reset_value(0x0),
    xspi(sc_core::sc_gen_unique_name("xspi", true), *this, 3, 1),
    xspi_reset_value(0x1),
    sd_emmc(sc_core::sc_gen_unique_name("sd_emmc", true), *this, 4, 1),
    sd_emmc_reset_value(0x0),
    bank_num(sc_core::sc_gen_unique_name("bank_num", true), *this, 5, 2),
    bank_num_reset_value(0x3),
    dll_tap_num(sc_core::sc_gen_unique_name("dll_tap_num", true), *this, 7, 1),
    dll_tap_num_reset_value(0x1),
    aging(sc_core::sc_gen_unique_name("aging", true), *this, 8, 1),
    aging_reset_value(0x1),
    dfi_clock_ratio(sc_core::sc_gen_unique_name("dfi_clock_ratio", true), *this, 9, 1),
    dfi_clock_ratio_reset_value(0x0),
    per_bit_deskew(sc_core::sc_gen_unique_name("per_bit_deskew", true), *this, 10, 1),
    per_bit_deskew_reset_value(0x0),
    reg_intf(sc_core::sc_gen_unique_name("reg_intf", true), *this, 11, 1),
    reg_intf_reset_value(0x1),
    ext_lpbk_dqs(sc_core::sc_gen_unique_name("ext_lpbk_dqs", true), *this, 12, 1),
    ext_lpbk_dqs_reset_value(0x1),
    jtag_sup(sc_core::sc_gen_unique_name("jtag_sup", true), *this, 13, 1),
    jtag_sup_reset_value(0x0),
    pll_sup(sc_core::sc_gen_unique_name("pll_sup", true), *this, 14, 1),
    pll_sup_reset_value(0x0),
    asf_sup(sc_core::sc_gen_unique_name("asf_sup", true), *this, 15, 1),
    asf_sup_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_ctrl_reg_type::~phy_ctrl_reg_type() {};

cdns_xspi_ctrl_regBase::phy_ctrl_reg_type::phy_ctrl_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    ctrl_clkperiod_delay(sc_core::sc_gen_unique_name("ctrl_clkperiod_delay", true), *this, 0, 1),
    ctrl_clkperiod_delay_reset_value(0x0),
    phony_dqs_timing(sc_core::sc_gen_unique_name("phony_dqs_timing", true), *this, 4, 5),
    phony_dqs_timing_reset_value(0x18) {}

cdns_xspi_ctrl_regBase::phy_tsel_reg_type::~phy_tsel_reg_type() {};

cdns_xspi_ctrl_regBase::phy_tsel_reg_type::phy_tsel_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    tsel_rd_value_dqs(sc_core::sc_gen_unique_name("tsel_rd_value_dqs", true), *this, 8, 4),
    tsel_rd_value_dqs_reset_value(0x0),
    tsel_off_value_dqs(sc_core::sc_gen_unique_name("tsel_off_value_dqs", true), *this, 12, 4),
    tsel_off_value_dqs_reset_value(0x0),
    tsel_rd_value_data(sc_core::sc_gen_unique_name("tsel_rd_value_data", true), *this, 16, 4),
    tsel_rd_value_data_reset_value(0x0),
    tsel_off_value_data(sc_core::sc_gen_unique_name("tsel_off_value_data", true), *this, 20, 4),
    tsel_off_value_data_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_gpio_ctrl_0_type::~phy_gpio_ctrl_0_type() {};

cdns_xspi_ctrl_regBase::phy_gpio_ctrl_0_type::phy_gpio_ctrl_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    phy_gpio_ctrl_0_value(sc_core::sc_gen_unique_name("phy_gpio_ctrl_0_value", true), *this, 0, 32),
    phy_gpio_ctrl_0_value_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_gpio_ctrl_1_type::~phy_gpio_ctrl_1_type() {};

cdns_xspi_ctrl_regBase::phy_gpio_ctrl_1_type::phy_gpio_ctrl_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    phy_gpio_ctrl_1_value(sc_core::sc_gen_unique_name("phy_gpio_ctrl_1_value", true), *this, 0, 32),
    phy_gpio_ctrl_1_value_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_gpio_status_0_type::~phy_gpio_status_0_type() {};

cdns_xspi_ctrl_regBase::phy_gpio_status_0_type::phy_gpio_status_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    phy_gpio_status_0_value(sc_core::sc_gen_unique_name("phy_gpio_status_0_value", true), *this, 0, 32),
    phy_gpio_status_0_value_reset_value(0x0) {}

cdns_xspi_ctrl_regBase::phy_gpio_status_1_type::~phy_gpio_status_1_type() {};

cdns_xspi_ctrl_regBase::phy_gpio_status_1_type::phy_gpio_status_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    phy_gpio_status_1_value(sc_core::sc_gen_unique_name("phy_gpio_status_1_value", true), *this, 0, 32),
    phy_gpio_status_1_value_reset_value(0x0) {}


#ifdef _WIN32
#pragma optimize("",on)
#else
#pragma GCC pop_options
#endif


}  // end of namespace mylibrary
