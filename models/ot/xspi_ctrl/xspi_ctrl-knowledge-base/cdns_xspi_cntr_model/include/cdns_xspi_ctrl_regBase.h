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
 * CHECKSUM:df1295313121ef27044372d24248adf897d508a0
 ***************************************************************************/
 

 
 // Warning: This is an auto-generated file. All changes to this file will be overwritten.


#pragma once

#include <systemc>
#include <scmlinc/scml_property.h>
#include "scml2_objects.h"
#include "scml2_protocol_engines/tlm2_ft_target_port/include/tlm2_ft_target_port_pe.h"
#include "scml2_protocol_engines/reset/include/reset_engine.h"
#include "scml2_protocol_engines/interrupt/include/interrupt_engine.h"
#include <tlm.h>
#include <scml2.h>


namespace mylibrary {


class cdns_xspi_ctrl_regCovermodelBase;


class cdns_xspi_ctrl_regCovermodel;


class cdns_xspi_ctrl_reg;

/**
 * This empty model example contains no predefined interfaces or registers.
It allows full flexibility in defining arbitrary components as it generates a minimal SystemC skeleton.
 * 
 * \version 1.0
 *
 * \par Company
 *   example.org
 */
#ifdef _WIN32
#pragma optimize("g",off)
#else
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif


class cdns_xspi_ctrl_regBase : public sc_core::sc_module {
    typedef cdns_xspi_ctrl_regBase ModelBaseType;
  public:
    SC_HAS_PROCESS(cdns_xspi_ctrl_regBase);

    cdns_xspi_ctrl_regBase(sc_core::sc_module_name name) 
        : sc_core::sc_module(name)
        , NUM_TARGETS("NUM_TARGETS", 1)
        , t_reg_socket("t_reg_socket")
        , reset_in("reset_in")
        , int_out("int_out")
        , t_axi_slave_socket("t_axi_slave_socket")
        , i_dma_socket("i_dma_socket")
        , PoR_input_signals("PoR_input_signals")
        , xspi_bus_socket("xspi_bus_socket", NUM_TARGETS)
        , t_reg_socket_router("t_reg_socket_router")
        , t_reg_socket_protocol_engine("t_reg_socket_protocol_engine", t_reg_socket)
        , reset_in_protocol_engine("reset_in_protocol_engine", reset_in)
        , int_out_protocol_engine("int_out_protocol_engine", int_out)
        , ctrl_cmd_stat_a("ctrl_cmd_stat_a", (0x15c) / scml2::sizeOf<unsigned int >())
        , cmd_reg0(sc_core::sc_gen_unique_name("cmd_reg0", true), ctrl_cmd_stat_a, (0x0) / scml2::sizeOf<unsigned int >())
        , cmd_reg1(sc_core::sc_gen_unique_name("cmd_reg1", true), ctrl_cmd_stat_a, (0x4) / scml2::sizeOf<unsigned int >())
        , cmd_reg2(sc_core::sc_gen_unique_name("cmd_reg2", true), ctrl_cmd_stat_a, (0x8) / scml2::sizeOf<unsigned int >())
        , cmd_reg3(sc_core::sc_gen_unique_name("cmd_reg3", true), ctrl_cmd_stat_a, (0xc) / scml2::sizeOf<unsigned int >())
        , cmd_reg4(sc_core::sc_gen_unique_name("cmd_reg4", true), ctrl_cmd_stat_a, (0x10) / scml2::sizeOf<unsigned int >())
        , cmd_reg5(sc_core::sc_gen_unique_name("cmd_reg5", true), ctrl_cmd_stat_a, (0x14) / scml2::sizeOf<unsigned int >())
        , cmd_status_ptr(sc_core::sc_gen_unique_name("cmd_status_ptr", true), ctrl_cmd_stat_a, (0x40) / scml2::sizeOf<unsigned int >())
        , cmd_status(sc_core::sc_gen_unique_name("cmd_status", true), ctrl_cmd_stat_a, (0x44) / scml2::sizeOf<unsigned int >())
        , ctrl_status(sc_core::sc_gen_unique_name("ctrl_status", true), ctrl_cmd_stat_a, (0x100) / scml2::sizeOf<unsigned int >())
        , trd_status(sc_core::sc_gen_unique_name("trd_status", true), ctrl_cmd_stat_a, (0x104) / scml2::sizeOf<unsigned int >())
        , intr_status(sc_core::sc_gen_unique_name("intr_status", true), ctrl_cmd_stat_a, (0x110) / scml2::sizeOf<unsigned int >())
        , intr_enable(sc_core::sc_gen_unique_name("intr_enable", true), ctrl_cmd_stat_a, (0x114) / scml2::sizeOf<unsigned int >())
        , trd_comp_intr_status(sc_core::sc_gen_unique_name("trd_comp_intr_status", true), ctrl_cmd_stat_a, (0x120) / scml2::sizeOf<unsigned int >())
        , trd_error_intr_status(sc_core::sc_gen_unique_name("trd_error_intr_status", true), ctrl_cmd_stat_a, (0x130) / scml2::sizeOf<unsigned int >())
        , trd_error_intr_en(sc_core::sc_gen_unique_name("trd_error_intr_en", true), ctrl_cmd_stat_a, (0x134) / scml2::sizeOf<unsigned int >())
        , dma_target_error_l(sc_core::sc_gen_unique_name("dma_target_error_l", true), ctrl_cmd_stat_a, (0x150) / scml2::sizeOf<unsigned int >())
        , dma_target_error_h(sc_core::sc_gen_unique_name("dma_target_error_h", true), ctrl_cmd_stat_a, (0x154) / scml2::sizeOf<unsigned int >())
        , boot_status(sc_core::sc_gen_unique_name("boot_status", true), ctrl_cmd_stat_a, (0x158) / scml2::sizeOf<unsigned int >())
        , ctrl_cfg_common_a("ctrl_cfg_common_a", (0x64) / scml2::sizeOf<unsigned int >())
        , long_polling(sc_core::sc_gen_unique_name("long_polling", true), ctrl_cfg_common_a, (0x8) / scml2::sizeOf<unsigned int >())
        , short_polling(sc_core::sc_gen_unique_name("short_polling", true), ctrl_cfg_common_a, (0xc) / scml2::sizeOf<unsigned int >())
        , ctrl_config(sc_core::sc_gen_unique_name("ctrl_config", true), ctrl_cfg_common_a, (0x30) / scml2::sizeOf<unsigned int >())
        , dma_settings(sc_core::sc_gen_unique_name("dma_settings", true), ctrl_cfg_common_a, (0x3c) / scml2::sizeOf<unsigned int >())
        , sdma_size(sc_core::sc_gen_unique_name("sdma_size", true), ctrl_cfg_common_a, (0x40) / scml2::sizeOf<unsigned int >())
        , sdma_trd_info(sc_core::sc_gen_unique_name("sdma_trd_info", true), ctrl_cfg_common_a, (0x44) / scml2::sizeOf<unsigned int >())
        , sdma_addr0(sc_core::sc_gen_unique_name("sdma_addr0", true), ctrl_cfg_common_a, (0x4c) / scml2::sizeOf<unsigned int >())
        , sdma_addr1(sc_core::sc_gen_unique_name("sdma_addr1", true), ctrl_cfg_common_a, (0x50) / scml2::sizeOf<unsigned int >())
        , discovery_control(sc_core::sc_gen_unique_name("discovery_control", true), ctrl_cfg_common_a, (0x60) / scml2::sizeOf<unsigned int >())
        , cmn_seq_regs_a("cmn_seq_regs_a", (0x24) / scml2::sizeOf<unsigned int >())
        , xip_mode_cfg(sc_core::sc_gen_unique_name("xip_mode_cfg", true), cmn_seq_regs_a, (0x8) / scml2::sizeOf<unsigned int >())
        , global_seq_cfg(sc_core::sc_gen_unique_name("global_seq_cfg", true), cmn_seq_regs_a, (0x10) / scml2::sizeOf<unsigned int >())
        , global_seq_cfg_1(sc_core::sc_gen_unique_name("global_seq_cfg_1", true), cmn_seq_regs_a, (0x14) / scml2::sizeOf<unsigned int >())
        , direct_access_cfg(sc_core::sc_gen_unique_name("direct_access_cfg", true), cmn_seq_regs_a, (0x18) / scml2::sizeOf<unsigned int >())
        , direct_access_rmp(sc_core::sc_gen_unique_name("direct_access_rmp", true), cmn_seq_regs_a, (0x1c) / scml2::sizeOf<unsigned int >())
        , direct_access_rmp_1(sc_core::sc_gen_unique_name("direct_access_rmp_1", true), cmn_seq_regs_a, (0x20) / scml2::sizeOf<unsigned int >())
        , dev_seq_regs_a("dev_seq_regs_a", (0x7c) / scml2::sizeOf<unsigned int >())
        , rst_seq_cfg_0(sc_core::sc_gen_unique_name("rst_seq_cfg_0", true), dev_seq_regs_a, (0x0) / scml2::sizeOf<unsigned int >())
        , rst_seq_cfg_1(sc_core::sc_gen_unique_name("rst_seq_cfg_1", true), dev_seq_regs_a, (0x4) / scml2::sizeOf<unsigned int >())
        , ers_seq_cfg_0(sc_core::sc_gen_unique_name("ers_seq_cfg_0", true), dev_seq_regs_a, (0x10) / scml2::sizeOf<unsigned int >())
        , ers_seq_cfg_1(sc_core::sc_gen_unique_name("ers_seq_cfg_1", true), dev_seq_regs_a, (0x14) / scml2::sizeOf<unsigned int >())
        , ers_seq_cfg_2(sc_core::sc_gen_unique_name("ers_seq_cfg_2", true), dev_seq_regs_a, (0x18) / scml2::sizeOf<unsigned int >())
        , prog_seq_cfg_0(sc_core::sc_gen_unique_name("prog_seq_cfg_0", true), dev_seq_regs_a, (0x20) / scml2::sizeOf<unsigned int >())
        , prog_seq_cfg_1(sc_core::sc_gen_unique_name("prog_seq_cfg_1", true), dev_seq_regs_a, (0x24) / scml2::sizeOf<unsigned int >())
        , prog_seq_cfg_2(sc_core::sc_gen_unique_name("prog_seq_cfg_2", true), dev_seq_regs_a, (0x28) / scml2::sizeOf<unsigned int >())
        , read_seq_cfg_0(sc_core::sc_gen_unique_name("read_seq_cfg_0", true), dev_seq_regs_a, (0x30) / scml2::sizeOf<unsigned int >())
        , read_seq_cfg_1(sc_core::sc_gen_unique_name("read_seq_cfg_1", true), dev_seq_regs_a, (0x34) / scml2::sizeOf<unsigned int >())
        , read_seq_cfg_2(sc_core::sc_gen_unique_name("read_seq_cfg_2", true), dev_seq_regs_a, (0x38) / scml2::sizeOf<unsigned int >())
        , we_seq_cfg_0(sc_core::sc_gen_unique_name("we_seq_cfg_0", true), dev_seq_regs_a, (0x40) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_0(sc_core::sc_gen_unique_name("stat_seq_cfg_0", true), dev_seq_regs_a, (0x50) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_1(sc_core::sc_gen_unique_name("stat_seq_cfg_1", true), dev_seq_regs_a, (0x54) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_2(sc_core::sc_gen_unique_name("stat_seq_cfg_2", true), dev_seq_regs_a, (0x58) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_3(sc_core::sc_gen_unique_name("stat_seq_cfg_3", true), dev_seq_regs_a, (0x5c) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_4(sc_core::sc_gen_unique_name("stat_seq_cfg_4", true), dev_seq_regs_a, (0x60) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_5(sc_core::sc_gen_unique_name("stat_seq_cfg_5", true), dev_seq_regs_a, (0x64) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_7(sc_core::sc_gen_unique_name("stat_seq_cfg_7", true), dev_seq_regs_a, (0x6c) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_8(sc_core::sc_gen_unique_name("stat_seq_cfg_8", true), dev_seq_regs_a, (0x70) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_9(sc_core::sc_gen_unique_name("stat_seq_cfg_9", true), dev_seq_regs_a, (0x74) / scml2::sizeOf<unsigned int >())
        , stat_seq_cfg_10(sc_core::sc_gen_unique_name("stat_seq_cfg_10", true), dev_seq_regs_a, (0x78) / scml2::sizeOf<unsigned int >())
        , ctrl_consts_a("ctrl_consts_a", (0x8) / scml2::sizeOf<unsigned int >())
        , xspi_ctrl_version(sc_core::sc_gen_unique_name("xspi_ctrl_version", true), ctrl_consts_a, (0x0) / scml2::sizeOf<unsigned int >())
        , ctrl_features_reg(sc_core::sc_gen_unique_name("ctrl_features_reg", true), ctrl_consts_a, (0x4) / scml2::sizeOf<unsigned int >())
        , rf_minictrl_regs_a("rf_minictrl_regs_a", (0x38) / scml2::sizeOf<unsigned int >())
        , wp_settings(sc_core::sc_gen_unique_name("wp_settings", true), rf_minictrl_regs_a, (0x0) / scml2::sizeOf<unsigned int >())
        , reset_pin_settings(sc_core::sc_gen_unique_name("reset_pin_settings", true), rf_minictrl_regs_a, (0x4) / scml2::sizeOf<unsigned int >())
        , clock_mode_settings(sc_core::sc_gen_unique_name("clock_mode_settings", true), rf_minictrl_regs_a, (0x8) / scml2::sizeOf<unsigned int >())
        , jedec_rst_timing_reg(sc_core::sc_gen_unique_name("jedec_rst_timing_reg", true), rf_minictrl_regs_a, (0xc) / scml2::sizeOf<unsigned int >())
        , dev_delay_reg(sc_core::sc_gen_unique_name("dev_delay_reg", true), rf_minictrl_regs_a, (0x10) / scml2::sizeOf<unsigned int >())
        , rst_recovery_reg(sc_core::sc_gen_unique_name("rst_recovery_reg", true), rf_minictrl_regs_a, (0x14) / scml2::sizeOf<unsigned int >())
        , dev_active_max_reg(sc_core::sc_gen_unique_name("dev_active_max_reg", true), rf_minictrl_regs_a, (0x18) / scml2::sizeOf<unsigned int >())
        , hf_offset_reg(sc_core::sc_gen_unique_name("hf_offset_reg", true), rf_minictrl_regs_a, (0x20) / scml2::sizeOf<unsigned int >())
        , dll_phy_update_cnt(sc_core::sc_gen_unique_name("dll_phy_update_cnt", true), rf_minictrl_regs_a, (0x30) / scml2::sizeOf<unsigned int >())
        , dll_phy_ctrl(sc_core::sc_gen_unique_name("dll_phy_ctrl", true), rf_minictrl_regs_a, (0x34) / scml2::sizeOf<unsigned int >())
        , dataslice_Rfile_a("dataslice_Rfile_a", (0x78) / scml2::sizeOf<unsigned int >())
        , phy_dq_timing_reg(sc_core::sc_gen_unique_name("phy_dq_timing_reg", true), dataslice_Rfile_a, (0x0) / scml2::sizeOf<unsigned int >())
        , phy_dqs_timing_reg(sc_core::sc_gen_unique_name("phy_dqs_timing_reg", true), dataslice_Rfile_a, (0x4) / scml2::sizeOf<unsigned int >())
        , phy_gate_lpbk_ctrl_reg(sc_core::sc_gen_unique_name("phy_gate_lpbk_ctrl_reg", true), dataslice_Rfile_a, (0x8) / scml2::sizeOf<unsigned int >())
        , phy_dll_master_ctrl_reg(sc_core::sc_gen_unique_name("phy_dll_master_ctrl_reg", true), dataslice_Rfile_a, (0xc) / scml2::sizeOf<unsigned int >())
        , phy_dll_slave_ctrl_reg(sc_core::sc_gen_unique_name("phy_dll_slave_ctrl_reg", true), dataslice_Rfile_a, (0x10) / scml2::sizeOf<unsigned int >())
        , phy_ie_timing_reg(sc_core::sc_gen_unique_name("phy_ie_timing_reg", true), dataslice_Rfile_a, (0x14) / scml2::sizeOf<unsigned int >())
        , phy_obs_reg_0(sc_core::sc_gen_unique_name("phy_obs_reg_0", true), dataslice_Rfile_a, (0x18) / scml2::sizeOf<unsigned int >())
        , phy_dll_obs_reg_0(sc_core::sc_gen_unique_name("phy_dll_obs_reg_0", true), dataslice_Rfile_a, (0x1c) / scml2::sizeOf<unsigned int >())
        , phy_dll_obs_reg_1(sc_core::sc_gen_unique_name("phy_dll_obs_reg_1", true), dataslice_Rfile_a, (0x20) / scml2::sizeOf<unsigned int >())
        , phy_static_togg_reg(sc_core::sc_gen_unique_name("phy_static_togg_reg", true), dataslice_Rfile_a, (0x28) / scml2::sizeOf<unsigned int >())
        , phy_wr_deskew_pd_ctrl_0_reg(sc_core::sc_gen_unique_name("phy_wr_deskew_pd_ctrl_0_reg", true), dataslice_Rfile_a, (0x34) / scml2::sizeOf<unsigned int >())
        , phy_version_reg(sc_core::sc_gen_unique_name("phy_version_reg", true), dataslice_Rfile_a, (0x70) / scml2::sizeOf<unsigned int >())
        , phy_features_reg(sc_core::sc_gen_unique_name("phy_features_reg", true), dataslice_Rfile_a, (0x74) / scml2::sizeOf<unsigned int >())
        , ctb_Rfile_a("ctb_Rfile_a", (0x18) / scml2::sizeOf<unsigned int >())
        , phy_ctrl_reg(sc_core::sc_gen_unique_name("phy_ctrl_reg", true), ctb_Rfile_a, (0x0) / scml2::sizeOf<unsigned int >())
        , phy_tsel_reg(sc_core::sc_gen_unique_name("phy_tsel_reg", true), ctb_Rfile_a, (0x4) / scml2::sizeOf<unsigned int >())
        , phy_gpio_ctrl_0(sc_core::sc_gen_unique_name("phy_gpio_ctrl_0", true), ctb_Rfile_a, (0x8) / scml2::sizeOf<unsigned int >())
        , phy_gpio_ctrl_1(sc_core::sc_gen_unique_name("phy_gpio_ctrl_1", true), ctb_Rfile_a, (0xc) / scml2::sizeOf<unsigned int >())
        , phy_gpio_status_0(sc_core::sc_gen_unique_name("phy_gpio_status_0", true), ctb_Rfile_a, (0x10) / scml2::sizeOf<unsigned int >())
        , phy_gpio_status_1(sc_core::sc_gen_unique_name("phy_gpio_status_1", true), ctb_Rfile_a, (0x14) / scml2::sizeOf<unsigned int >()) {
      t_reg_socket_router.mappings.resize(8);
      t_reg_socket_protocol_engine.intercepts.resize(0);
      t_reg_socket_router.bound_on = t_reg_socket_protocol_engine;
      t_reg_socket_router.mappings[0].base = 0x380;
      t_reg_socket_router.mappings[0].size = cmn_seq_regs_a.get_size()*cmn_seq_regs_a.get_width();
      t_reg_socket_router.mappings[0].destination = cmn_seq_regs_a;
      /* t_reg_socket_router.mappings[0].offset = ; */
      t_reg_socket_router.mappings[0].type = scml2::objects::RW;
      t_reg_socket_router.mappings[0].active = true;
      /* t_reg_socket_router.mappings[0].name = ; */
      t_reg_socket_router.mappings[1].base = 0x2080;
      t_reg_socket_router.mappings[1].size = ctb_Rfile_a.get_size()*ctb_Rfile_a.get_width();
      t_reg_socket_router.mappings[1].destination = ctb_Rfile_a;
      /* t_reg_socket_router.mappings[1].offset = ; */
      t_reg_socket_router.mappings[1].type = scml2::objects::RW;
      t_reg_socket_router.mappings[1].active = true;
      /* t_reg_socket_router.mappings[1].name = ; */
      t_reg_socket_router.mappings[2].base = 0x200;
      t_reg_socket_router.mappings[2].size = ctrl_cfg_common_a.get_size()*ctrl_cfg_common_a.get_width();
      t_reg_socket_router.mappings[2].destination = ctrl_cfg_common_a;
      /* t_reg_socket_router.mappings[2].offset = ; */
      t_reg_socket_router.mappings[2].type = scml2::objects::RW;
      t_reg_socket_router.mappings[2].active = true;
      /* t_reg_socket_router.mappings[2].name = ; */
      t_reg_socket_router.mappings[3].base = 0x0;
      t_reg_socket_router.mappings[3].size = ctrl_cmd_stat_a.get_size()*ctrl_cmd_stat_a.get_width();
      t_reg_socket_router.mappings[3].destination = ctrl_cmd_stat_a;
      /* t_reg_socket_router.mappings[3].offset = ; */
      t_reg_socket_router.mappings[3].type = scml2::objects::RW;
      t_reg_socket_router.mappings[3].active = true;
      /* t_reg_socket_router.mappings[3].name = ; */
      t_reg_socket_router.mappings[4].base = 0xf00;
      t_reg_socket_router.mappings[4].size = ctrl_consts_a.get_size()*ctrl_consts_a.get_width();
      t_reg_socket_router.mappings[4].destination = ctrl_consts_a;
      /* t_reg_socket_router.mappings[4].offset = ; */
      t_reg_socket_router.mappings[4].type = scml2::objects::RW;
      t_reg_socket_router.mappings[4].active = true;
      /* t_reg_socket_router.mappings[4].name = ; */
      t_reg_socket_router.mappings[5].base = 0x2000;
      t_reg_socket_router.mappings[5].size = dataslice_Rfile_a.get_size()*dataslice_Rfile_a.get_width();
      t_reg_socket_router.mappings[5].destination = dataslice_Rfile_a;
      /* t_reg_socket_router.mappings[5].offset = ; */
      t_reg_socket_router.mappings[5].type = scml2::objects::RW;
      t_reg_socket_router.mappings[5].active = true;
      /* t_reg_socket_router.mappings[5].name = ; */
      t_reg_socket_router.mappings[6].base = 0x400;
      t_reg_socket_router.mappings[6].size = dev_seq_regs_a.get_size()*dev_seq_regs_a.get_width();
      t_reg_socket_router.mappings[6].destination = dev_seq_regs_a;
      /* t_reg_socket_router.mappings[6].offset = ; */
      t_reg_socket_router.mappings[6].type = scml2::objects::RW;
      t_reg_socket_router.mappings[6].active = true;
      /* t_reg_socket_router.mappings[6].name = ; */
      t_reg_socket_router.mappings[7].base = 0x1000;
      t_reg_socket_router.mappings[7].size = rf_minictrl_regs_a.get_size()*rf_minictrl_regs_a.get_width();
      t_reg_socket_router.mappings[7].destination = rf_minictrl_regs_a;
      /* t_reg_socket_router.mappings[7].offset = ; */
      t_reg_socket_router.mappings[7].type = scml2::objects::RW;
      t_reg_socket_router.mappings[7].active = true;
      /* t_reg_socket_router.mappings[7].name = ; */
      t_reg_socket_router.finalize_construction();
      t_reg_socket_protocol_engine.abstraction = scml2::LT;
      #ifdef SYNOPSYS_SYSTEMC
      t_reg_socket_protocol_engine.consume_annotated_time = false;
      #endif
      t_reg_socket_protocol_engine.finalize_construction();
      reset_in_protocol_engine.active_level = false;
      reset_in_protocol_engine.finalize_construction();
      int_out_protocol_engine.active_level = false;
      /* int_out_protocol_engine.enable = ; */
      int_out_protocol_engine.finalize_construction();


      t_axi_slave_socket.register_b_transport(this, &cdns_xspi_ctrl_regBase::b_transport_axi_slave);

           // Register b_transport for PoR input signals
      PoR_input_signals.register_b_transport(this, &cdns_xspi_ctrl_regBase::b_transport_por_input);


      scml2::set_write_callback<unsigned int, scml2::reg>(cmd_reg0, SCML2_CALLBACK(handle_write_cmd_reg0), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(cmd_reg1, SCML2_CALLBACK(handle_write_cmd_reg1), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(cmd_reg2, SCML2_CALLBACK(handle_write_cmd_reg2), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(cmd_reg3, SCML2_CALLBACK(handle_write_cmd_reg3), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(cmd_reg4, SCML2_CALLBACK(handle_write_cmd_reg4), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(cmd_reg5, SCML2_CALLBACK(handle_write_cmd_reg5), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(cmd_status_ptr, SCML2_CALLBACK(handle_write_cmd_status_ptr), scml2::AUTO_SYNCING);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(cmd_status);
      scml2::set_write_ignore_restriction(cmd_status.cmd_status);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(ctrl_status);
      scml2::set_write_ignore_restriction(ctrl_status.sdma_busy);
      scml2::set_write_ignore_restriction(ctrl_status.mdma_busy);
      scml2::set_write_ignore_restriction(ctrl_status.acmd_eng_busy);
      scml2::set_write_ignore_restriction(ctrl_status.gcmd_eng_busy);
      scml2::set_write_ignore_restriction(ctrl_status.gcmd_eng_mc_busy);
      scml2::set_write_ignore_restriction(ctrl_status.discovery_busy);
      scml2::set_write_ignore_restriction(ctrl_status.ctrl_busy);
      scml2::set_write_ignore_restriction(ctrl_status.init_fail);
      scml2::set_write_ignore_restriction(ctrl_status.init_comp);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(trd_status);
      scml2::set_write_ignore_restriction(trd_status.trd_busy);
      scml2::set_write_callback<unsigned int, scml2::reg>(intr_status, SCML2_CALLBACK(handle_write_intr_status), scml2::AUTO_SYNCING);
      scml2::set_clear_on_write_1(intr_status.gp_open_drain_0);
      scml2::set_clear_on_write_1(intr_status.gp_open_drain_1);
      scml2::set_clear_on_write_1(intr_status.gp_open_drain_2);
      scml2::set_clear_on_write_1(intr_status.gp_open_drain_3);
      scml2::set_clear_on_write_1(intr_status.ctrl_idle);
      scml2::set_clear_on_write_1(intr_status.cdma_terr);
      scml2::set_clear_on_write_1(intr_status.ddma_terr);
      scml2::set_clear_on_write_1(intr_status.cmd_ignored);
      scml2::set_clear_on_write_1(intr_status.sdma_trigg);
      scml2::set_clear_on_write_1(intr_status.sdma_err);
      scml2::set_clear_on_write_1(intr_status.stig_done);
      scml2::set_clear_on_write_1(intr_status.dir_crc_err);
      scml2::set_clear_on_write_1(intr_status.dir_dqs_err);
      scml2::set_clear_on_write_1(intr_status.dir_cmd_err);
      scml2::set_clear_on_write_1(intr_status.dir_ecc_corr_err);
      scml2::set_clear_on_write_1(intr_status.dir_dev_err);
      scml2::set_write_callback<unsigned int, scml2::reg>(intr_enable, SCML2_CALLBACK(handle_write_intr_enable), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(trd_comp_intr_status, SCML2_CALLBACK(handle_write_trd_comp_intr_status), scml2::AUTO_SYNCING);
      scml2::set_clear_on_write_1(trd_comp_intr_status.trd0_comp);
      scml2::set_clear_on_write_1(trd_comp_intr_status.trd1_comp);
      scml2::set_clear_on_write_1(trd_comp_intr_status.trd2_comp);
      scml2::set_clear_on_write_1(trd_comp_intr_status.trd3_comp);
      scml2::set_clear_on_write_1(trd_comp_intr_status.trd4_comp);
      scml2::set_clear_on_write_1(trd_comp_intr_status.trd5_comp);
      scml2::set_clear_on_write_1(trd_comp_intr_status.trd6_comp);
      scml2::set_clear_on_write_1(trd_comp_intr_status.trd7_comp);
      scml2::set_write_callback<unsigned int, scml2::reg>(trd_error_intr_status, SCML2_CALLBACK(handle_write_trd_error_intr_status), scml2::AUTO_SYNCING);
      scml2::set_clear_on_write_1(trd_error_intr_status.trd0_error_stat);
      scml2::set_clear_on_write_1(trd_error_intr_status.trd1_error_stat);
      scml2::set_clear_on_write_1(trd_error_intr_status.trd2_error_stat);
      scml2::set_clear_on_write_1(trd_error_intr_status.trd3_error_stat);
      scml2::set_clear_on_write_1(trd_error_intr_status.trd4_error_stat);
      scml2::set_clear_on_write_1(trd_error_intr_status.trd5_error_stat);
      scml2::set_clear_on_write_1(trd_error_intr_status.trd6_error_stat);
      scml2::set_clear_on_write_1(trd_error_intr_status.trd7_error_stat);
      scml2::set_write_callback<unsigned int, scml2::reg>(trd_error_intr_en, SCML2_CALLBACK(handle_write_trd_error_intr_en), scml2::AUTO_SYNCING);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(dma_target_error_l);
      scml2::set_write_ignore_restriction(dma_target_error_l.target_err_l);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(dma_target_error_h);
      scml2::set_write_ignore_restriction(dma_target_error_h.target_err_h);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(boot_status);
      scml2::set_write_ignore_restriction(boot_status.boot_dqs_err);
      scml2::set_write_ignore_restriction(boot_status.boot_crc_err);
      scml2::set_write_ignore_restriction(boot_status.boot_bus_err);
      scml2::set_write_callback<unsigned int, scml2::reg>(long_polling, SCML2_CALLBACK(handle_write_long_polling), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(short_polling, SCML2_CALLBACK(handle_write_short_polling), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(ctrl_config, SCML2_CALLBACK(handle_write_ctrl_config), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(dma_settings, SCML2_CALLBACK(handle_write_dma_settings), scml2::AUTO_SYNCING);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(sdma_size);
      scml2::set_write_ignore_restriction(sdma_size.sdma_size);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(sdma_trd_info);
      scml2::set_write_ignore_restriction(sdma_trd_info.sdma_trd);
      scml2::set_write_ignore_restriction(sdma_trd_info.sdma_dir);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(sdma_addr0);
      scml2::set_write_ignore_restriction(sdma_addr0.sdma_addr_l);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(sdma_addr1);
      scml2::set_write_ignore_restriction(sdma_addr1.sdma_addr_h);
      scml2::set_write_callback<unsigned int, scml2::reg>(discovery_control, SCML2_CALLBACK(handle_write_discovery_control), scml2::AUTO_SYNCING);
      scml2::set_write_ignore_restriction(discovery_control.discovery_comp);
      scml2::set_write_ignore_restriction(discovery_control.discovery_fail);
      scml2::set_write_ignore_restriction(discovery_control.discovery_inhibit);
      scml2::set_write_callback<unsigned int, scml2::reg>(xip_mode_cfg, SCML2_CALLBACK(handle_write_xip_mode_cfg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(global_seq_cfg, SCML2_CALLBACK(handle_write_global_seq_cfg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(global_seq_cfg_1, SCML2_CALLBACK(handle_write_global_seq_cfg_1), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(direct_access_cfg, SCML2_CALLBACK(handle_write_direct_access_cfg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(direct_access_rmp, SCML2_CALLBACK(handle_write_direct_access_rmp), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(direct_access_rmp_1, SCML2_CALLBACK(handle_write_direct_access_rmp_1), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(rst_seq_cfg_0, SCML2_CALLBACK(handle_write_rst_seq_cfg_0), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(rst_seq_cfg_1, SCML2_CALLBACK(handle_write_rst_seq_cfg_1), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(ers_seq_cfg_0, SCML2_CALLBACK(handle_write_ers_seq_cfg_0), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(ers_seq_cfg_1, SCML2_CALLBACK(handle_write_ers_seq_cfg_1), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(ers_seq_cfg_2, SCML2_CALLBACK(handle_write_ers_seq_cfg_2), scml2::AUTO_SYNCING);
      scml2::set_write_callback(ers_seq_cfg_2.ersa_seq_p1_cmd_val, SCML2_CALLBACK(handle_write_ers_seq_cfg_2_ersa_seq_p1_cmd_val), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(prog_seq_cfg_0, SCML2_CALLBACK(handle_write_prog_seq_cfg_0), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(prog_seq_cfg_1, SCML2_CALLBACK(handle_write_prog_seq_cfg_1), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(prog_seq_cfg_2, SCML2_CALLBACK(handle_write_prog_seq_cfg_2), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(read_seq_cfg_0, SCML2_CALLBACK(handle_write_read_seq_cfg_0), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(read_seq_cfg_1, SCML2_CALLBACK(handle_write_read_seq_cfg_1), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(read_seq_cfg_2, SCML2_CALLBACK(handle_write_read_seq_cfg_2), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(we_seq_cfg_0, SCML2_CALLBACK(handle_write_we_seq_cfg_0), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_0, SCML2_CALLBACK(handle_write_stat_seq_cfg_0), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_1, SCML2_CALLBACK(handle_write_stat_seq_cfg_1), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_2, SCML2_CALLBACK(handle_write_stat_seq_cfg_2), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_3, SCML2_CALLBACK(handle_write_stat_seq_cfg_3), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_4, SCML2_CALLBACK(handle_write_stat_seq_cfg_4), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_5, SCML2_CALLBACK(handle_write_stat_seq_cfg_5), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_7, SCML2_CALLBACK(handle_write_stat_seq_cfg_7), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_8, SCML2_CALLBACK(handle_write_stat_seq_cfg_8), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_9, SCML2_CALLBACK(handle_write_stat_seq_cfg_9), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(stat_seq_cfg_10, SCML2_CALLBACK(handle_write_stat_seq_cfg_10), scml2::AUTO_SYNCING);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(xspi_ctrl_version);
      scml2::set_write_ignore_restriction(xspi_ctrl_version.xspi_ctrl_rev);
      scml2::set_write_ignore_restriction(xspi_ctrl_version.xspi_ctrl_fix);
      scml2::set_write_ignore_restriction(xspi_ctrl_version.xspi_ctrl_magic_number);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(ctrl_features_reg);
      scml2::set_write_ignore_restriction(ctrl_features_reg.n_threads);
      scml2::set_write_ignore_restriction(ctrl_features_reg.asf_available);
      scml2::set_write_ignore_restriction(ctrl_features_reg.boot_available);
      scml2::set_write_ignore_restriction(ctrl_features_reg.dma_intf);
      scml2::set_write_ignore_restriction(ctrl_features_reg.dma_addr_width);
      scml2::set_write_ignore_restriction(ctrl_features_reg.dma_data_width);
      scml2::set_write_ignore_restriction(ctrl_features_reg.sfr_intf);
      scml2::set_write_ignore_restriction(ctrl_features_reg.n_banks);
      scml2::set_write_callback<unsigned int, scml2::reg>(wp_settings, SCML2_CALLBACK(handle_write_wp_settings), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(reset_pin_settings, SCML2_CALLBACK(handle_write_reset_pin_settings), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(clock_mode_settings, SCML2_CALLBACK(handle_write_clock_mode_settings), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(jedec_rst_timing_reg, SCML2_CALLBACK(handle_write_jedec_rst_timing_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(dev_delay_reg, SCML2_CALLBACK(handle_write_dev_delay_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(rst_recovery_reg, SCML2_CALLBACK(handle_write_rst_recovery_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(dev_active_max_reg, SCML2_CALLBACK(handle_write_dev_active_max_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(hf_offset_reg, SCML2_CALLBACK(handle_write_hf_offset_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(dll_phy_update_cnt, SCML2_CALLBACK(handle_write_dll_phy_update_cnt), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(dll_phy_ctrl, SCML2_CALLBACK(handle_write_dll_phy_ctrl), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_dq_timing_reg, SCML2_CALLBACK(handle_write_phy_dq_timing_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_dqs_timing_reg, SCML2_CALLBACK(handle_write_phy_dqs_timing_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_gate_lpbk_ctrl_reg, SCML2_CALLBACK(handle_write_phy_gate_lpbk_ctrl_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_dll_master_ctrl_reg, SCML2_CALLBACK(handle_write_phy_dll_master_ctrl_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_dll_slave_ctrl_reg, SCML2_CALLBACK(handle_write_phy_dll_slave_ctrl_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_ie_timing_reg, SCML2_CALLBACK(handle_write_phy_ie_timing_reg), scml2::AUTO_SYNCING);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(phy_obs_reg_0);
      scml2::set_write_ignore_restriction(phy_obs_reg_0.lpbk_status);
      scml2::set_write_ignore_restriction(phy_obs_reg_0.lpbk_dq_data);
      scml2::set_write_ignore_restriction(phy_obs_reg_0.dqs_underrun);
      scml2::set_write_ignore_restriction(phy_obs_reg_0.dqs_overflow);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(phy_dll_obs_reg_0);
      scml2::set_write_ignore_restriction(phy_dll_obs_reg_0.dll_lock);
      scml2::set_write_ignore_restriction(phy_dll_obs_reg_0.dll_locked_mode);
      scml2::set_write_ignore_restriction(phy_dll_obs_reg_0.dll_unlock_cnt);
      scml2::set_write_ignore_restriction(phy_dll_obs_reg_0.dll_lock_value);
      scml2::set_write_ignore_restriction(phy_dll_obs_reg_0.lock_dec_dbg);
      scml2::set_write_ignore_restriction(phy_dll_obs_reg_0.lock_inc_dbg);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(phy_dll_obs_reg_1);
      scml2::set_write_ignore_restriction(phy_dll_obs_reg_1.decoder_out_rd);
      scml2::set_write_ignore_restriction(phy_dll_obs_reg_1.decoder_out_wr);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_static_togg_reg, SCML2_CALLBACK(handle_write_phy_static_togg_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_wr_deskew_pd_ctrl_0_reg, SCML2_CALLBACK(handle_write_phy_wr_deskew_pd_ctrl_0_reg), scml2::AUTO_SYNCING);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(phy_version_reg);
      scml2::set_write_ignore_restriction(phy_version_reg.phy_rev);
      scml2::set_write_ignore_restriction(phy_version_reg.phy_fix);
      scml2::set_write_ignore_restriction(phy_version_reg.combo_phy_magic_number);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(phy_features_reg);
      scml2::set_write_ignore_restriction(phy_features_reg.onfi_40);
      scml2::set_write_ignore_restriction(phy_features_reg.onfi_41);
      scml2::set_write_ignore_restriction(phy_features_reg.sdr_16bit);
      scml2::set_write_ignore_restriction(phy_features_reg.xspi);
      scml2::set_write_ignore_restriction(phy_features_reg.sd_emmc);
      scml2::set_write_ignore_restriction(phy_features_reg.bank_num);
      scml2::set_write_ignore_restriction(phy_features_reg.dll_tap_num);
      scml2::set_write_ignore_restriction(phy_features_reg.aging);
      scml2::set_write_ignore_restriction(phy_features_reg.dfi_clock_ratio);
      scml2::set_write_ignore_restriction(phy_features_reg.per_bit_deskew);
      scml2::set_write_ignore_restriction(phy_features_reg.reg_intf);
      scml2::set_write_ignore_restriction(phy_features_reg.ext_lpbk_dqs);
      scml2::set_write_ignore_restriction(phy_features_reg.jtag_sup);
      scml2::set_write_ignore_restriction(phy_features_reg.pll_sup);
      scml2::set_write_ignore_restriction(phy_features_reg.asf_sup);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_ctrl_reg, SCML2_CALLBACK(handle_write_phy_ctrl_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_tsel_reg, SCML2_CALLBACK(handle_write_phy_tsel_reg), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_gpio_ctrl_0, SCML2_CALLBACK(handle_write_phy_gpio_ctrl_0), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(phy_gpio_ctrl_1, SCML2_CALLBACK(handle_write_phy_gpio_ctrl_1), scml2::AUTO_SYNCING);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(phy_gpio_status_0);
      scml2::set_write_ignore_restriction(phy_gpio_status_0.phy_gpio_status_0_value);
      scml2::set_write_ignore_restriction<unsigned int, scml2::reg>(phy_gpio_status_1);
      scml2::set_write_ignore_restriction(phy_gpio_status_1.phy_gpio_status_1_value);
    }
    
// LCOV_EXCL_START
    virtual ~cdns_xspi_ctrl_regBase() {
      
    }
// LCOV_EXCL_STOP    

  protected:
    /**
     * This method resets all interfaces, registers, and bitfields to their reset value.
     * It is typically called from end_of_elaboration(), and connected to the reset port.
     */
    
    virtual void reset_model() {
      reset_model_if_available(t_reg_socket_router);
      cmd_reg0.cmd0 = cmd_reg0.cmd0_reset_value;
      cmd_reg1.cmd1 = cmd_reg1.cmd1_reset_value;
      cmd_reg2.cmd2 = cmd_reg2.cmd2_reset_value;
      cmd_reg3.cmd3 = cmd_reg3.cmd3_reset_value;
      cmd_reg4.cmd4 = cmd_reg4.cmd4_reset_value;
      cmd_reg5.cmd5 = cmd_reg5.cmd5_reset_value;
      cmd_status_ptr.thrd_status_sel = cmd_status_ptr.thrd_status_sel_reset_value;
      cmd_status.cmd_status = cmd_status.cmd_status_reset_value;
      ctrl_status.sdma_busy = ctrl_status.sdma_busy_reset_value;
      ctrl_status.mdma_busy = ctrl_status.mdma_busy_reset_value;
      ctrl_status.acmd_eng_busy = ctrl_status.acmd_eng_busy_reset_value;
      ctrl_status.gcmd_eng_busy = ctrl_status.gcmd_eng_busy_reset_value;
      ctrl_status.gcmd_eng_mc_busy = ctrl_status.gcmd_eng_mc_busy_reset_value;
      ctrl_status.discovery_busy = ctrl_status.discovery_busy_reset_value;
      ctrl_status.ctrl_busy = ctrl_status.ctrl_busy_reset_value;
      ctrl_status.init_fail = ctrl_status.init_fail_reset_value;
      ctrl_status.init_comp = ctrl_status.init_comp_reset_value;
      trd_status.trd_busy = trd_status.trd_busy_reset_value;
      intr_status.gp_open_drain_0 = intr_status.gp_open_drain_0_reset_value;
      intr_status.gp_open_drain_1 = intr_status.gp_open_drain_1_reset_value;
      intr_status.gp_open_drain_2 = intr_status.gp_open_drain_2_reset_value;
      intr_status.gp_open_drain_3 = intr_status.gp_open_drain_3_reset_value;
      intr_status.ctrl_idle = intr_status.ctrl_idle_reset_value;
      intr_status.cdma_terr = intr_status.cdma_terr_reset_value;
      intr_status.ddma_terr = intr_status.ddma_terr_reset_value;
      intr_status.cmd_ignored = intr_status.cmd_ignored_reset_value;
      intr_status.sdma_trigg = intr_status.sdma_trigg_reset_value;
      intr_status.sdma_err = intr_status.sdma_err_reset_value;
      intr_status.stig_done = intr_status.stig_done_reset_value;
      intr_status.dir_crc_err = intr_status.dir_crc_err_reset_value;
      intr_status.dir_dqs_err = intr_status.dir_dqs_err_reset_value;
      intr_status.dir_cmd_err = intr_status.dir_cmd_err_reset_value;
      intr_status.dir_ecc_corr_err = intr_status.dir_ecc_corr_err_reset_value;
      intr_status.dir_dev_err = intr_status.dir_dev_err_reset_value;
      intr_enable.gp_open_drain_0_en = intr_enable.gp_open_drain_0_en_reset_value;
      intr_enable.gp_open_drain_1_en = intr_enable.gp_open_drain_1_en_reset_value;
      intr_enable.gp_open_drain_2_en = intr_enable.gp_open_drain_2_en_reset_value;
      intr_enable.gp_open_drain_3_en = intr_enable.gp_open_drain_3_en_reset_value;
      intr_enable.ctrl_idle_en = intr_enable.ctrl_idle_en_reset_value;
      intr_enable.cdma_terr_en = intr_enable.cdma_terr_en_reset_value;
      intr_enable.ddma_terr_en = intr_enable.ddma_terr_en_reset_value;
      intr_enable.cmd_ignored_en = intr_enable.cmd_ignored_en_reset_value;
      intr_enable.sdma_trigg_en = intr_enable.sdma_trigg_en_reset_value;
      intr_enable.sdma_err_en = intr_enable.sdma_err_en_reset_value;
      intr_enable.stig_done_en = intr_enable.stig_done_en_reset_value;
      intr_enable.dir_crc_err_en = intr_enable.dir_crc_err_en_reset_value;
      intr_enable.dir_dqs_err_en = intr_enable.dir_dqs_err_en_reset_value;
      intr_enable.dir_cmd_err_en = intr_enable.dir_cmd_err_en_reset_value;
      intr_enable.dir_ecc_corr_err_en = intr_enable.dir_ecc_corr_err_en_reset_value;
      intr_enable.dir_dev_err_en = intr_enable.dir_dev_err_en_reset_value;
      intr_enable.intr_en = intr_enable.intr_en_reset_value;
      trd_comp_intr_status.trd0_comp = trd_comp_intr_status.trd0_comp_reset_value;
      trd_comp_intr_status.trd1_comp = trd_comp_intr_status.trd1_comp_reset_value;
      trd_comp_intr_status.trd2_comp = trd_comp_intr_status.trd2_comp_reset_value;
      trd_comp_intr_status.trd3_comp = trd_comp_intr_status.trd3_comp_reset_value;
      trd_comp_intr_status.trd4_comp = trd_comp_intr_status.trd4_comp_reset_value;
      trd_comp_intr_status.trd5_comp = trd_comp_intr_status.trd5_comp_reset_value;
      trd_comp_intr_status.trd6_comp = trd_comp_intr_status.trd6_comp_reset_value;
      trd_comp_intr_status.trd7_comp = trd_comp_intr_status.trd7_comp_reset_value;
      trd_error_intr_status.trd0_error_stat = trd_error_intr_status.trd0_error_stat_reset_value;
      trd_error_intr_status.trd1_error_stat = trd_error_intr_status.trd1_error_stat_reset_value;
      trd_error_intr_status.trd2_error_stat = trd_error_intr_status.trd2_error_stat_reset_value;
      trd_error_intr_status.trd3_error_stat = trd_error_intr_status.trd3_error_stat_reset_value;
      trd_error_intr_status.trd4_error_stat = trd_error_intr_status.trd4_error_stat_reset_value;
      trd_error_intr_status.trd5_error_stat = trd_error_intr_status.trd5_error_stat_reset_value;
      trd_error_intr_status.trd6_error_stat = trd_error_intr_status.trd6_error_stat_reset_value;
      trd_error_intr_status.trd7_error_stat = trd_error_intr_status.trd7_error_stat_reset_value;
      trd_error_intr_en.trd_error_intr_en = trd_error_intr_en.trd_error_intr_en_reset_value;
      dma_target_error_l.target_err_l = dma_target_error_l.target_err_l_reset_value;
      dma_target_error_h.target_err_h = dma_target_error_h.target_err_h_reset_value;
      boot_status.boot_dqs_err = boot_status.boot_dqs_err_reset_value;
      boot_status.boot_crc_err = boot_status.boot_crc_err_reset_value;
      boot_status.boot_bus_err = boot_status.boot_bus_err_reset_value;
      long_polling.long_polling = long_polling.long_polling_reset_value;
      short_polling.short_polling = short_polling.short_polling_reset_value;
      ctrl_config.cont_on_err = ctrl_config.cont_on_err_reset_value;
      ctrl_config.work_mode = ctrl_config.work_mode_reset_value;
      dma_settings.burst_sel = dma_settings.burst_sel_reset_value;
      dma_settings.OTE = dma_settings.OTE_reset_value;
      dma_settings.sdma_err_rsp = dma_settings.sdma_err_rsp_reset_value;
      dma_settings.word_size = dma_settings.word_size_reset_value;
      sdma_size.sdma_size = sdma_size.sdma_size_reset_value;
      sdma_trd_info.sdma_trd = sdma_trd_info.sdma_trd_reset_value;
      sdma_trd_info.sdma_dir = sdma_trd_info.sdma_dir_reset_value;
      sdma_addr0.sdma_addr_l = sdma_addr0.sdma_addr_l_reset_value;
      sdma_addr1.sdma_addr_h = sdma_addr1.sdma_addr_h_reset_value;
      discovery_control.discovery_req = discovery_control.discovery_req_reset_value;
      discovery_control.discovery_req_type = discovery_control.discovery_req_type_reset_value;
      discovery_control.discovery_comp = discovery_control.discovery_comp_reset_value;
      discovery_control.discovery_fail = discovery_control.discovery_fail_reset_value;
      discovery_control.discovery_inhibit = discovery_control.discovery_inhibit_reset_value;
      discovery_control.discovery_extop_val = discovery_control.discovery_extop_val_reset_value;
      discovery_control.discovery_extop_en = discovery_control.discovery_extop_en_reset_value;
      discovery_control.discovery_cmd_type = discovery_control.discovery_cmd_type_reset_value;
      discovery_control.discovery_dummy_cnt = discovery_control.discovery_dummy_cnt_reset_value;
      discovery_control.discovery_abnum = discovery_control.discovery_abnum_reset_value;
      discovery_control.discovery_num_lines = discovery_control.discovery_num_lines_reset_value;
      discovery_control.discovery_bank = discovery_control.discovery_bank_reset_value;
      xip_mode_cfg.xip_en = xip_mode_cfg.xip_en_reset_value;
      xip_mode_cfg.xip_en_mb_val = xip_mode_cfg.xip_en_mb_val_reset_value;
      xip_mode_cfg.xip_dis_mb_val = xip_mode_cfg.xip_dis_mb_val_reset_value;
      global_seq_cfg.seq_page_size_rd = global_seq_cfg.seq_page_size_rd_reset_value;
      global_seq_cfg.seq_page_size_pgm = global_seq_cfg.seq_page_size_pgm_reset_value;
      global_seq_cfg.seq_crc_en = global_seq_cfg.seq_crc_en_reset_value;
      global_seq_cfg.seq_crc_variant = global_seq_cfg.seq_crc_variant_reset_value;
      global_seq_cfg.seq_crc_oe = global_seq_cfg.seq_crc_oe_reset_value;
      global_seq_cfg.seq_crc_chunk_size = global_seq_cfg.seq_crc_chunk_size_reset_value;
      global_seq_cfg.seq_crc_ual_chunk_en = global_seq_cfg.seq_crc_ual_chunk_en_reset_value;
      global_seq_cfg.seq_crc_ual_chunk_chk = global_seq_cfg.seq_crc_ual_chunk_chk_reset_value;
      global_seq_cfg.seq_tcms_en = global_seq_cfg.seq_tcms_en_reset_value;
      global_seq_cfg.seq_data_swap = global_seq_cfg.seq_data_swap_reset_value;
      global_seq_cfg.seq_data_per_addr = global_seq_cfg.seq_data_per_addr_reset_value;
      global_seq_cfg.seq_type = global_seq_cfg.seq_type_reset_value;
      global_seq_cfg_1.seq_page_size_ext = global_seq_cfg_1.seq_page_size_ext_reset_value;
      global_seq_cfg_1.seq_page_ca_size = global_seq_cfg_1.seq_page_ca_size_reset_value;
      global_seq_cfg_1.seq_page_per_block = global_seq_cfg_1.seq_page_per_block_reset_value;
      global_seq_cfg_1.seq_plane_cnt = global_seq_cfg_1.seq_plane_cnt_reset_value;
      direct_access_cfg.dac_bank_num = direct_access_cfg.dac_bank_num_reset_value;
      direct_access_cfg.rwds_cap_en = direct_access_cfg.rwds_cap_en_reset_value;
      direct_access_cfg.mode_bit_xip_en = direct_access_cfg.mode_bit_xip_en_reset_value;
      direct_access_cfg.mode_bit_xip_dis = direct_access_cfg.mode_bit_xip_dis_reset_value;
      direct_access_cfg.rmp_addr_en = direct_access_cfg.rmp_addr_en_reset_value;
      direct_access_cfg.dac_addr_mask = direct_access_cfg.dac_addr_mask_reset_value;
      direct_access_rmp.rmp_addr_val = direct_access_rmp.rmp_addr_val_reset_value;
      direct_access_rmp_1.rmp_addr_val_1 = direct_access_rmp_1.rmp_addr_val_1_reset_value;
      rst_seq_cfg_0.rst_seq_p1_cmd0_val = rst_seq_cfg_0.rst_seq_p1_cmd0_val_reset_value;
      rst_seq_cfg_0.rst_seq_p1_cmd1_val = rst_seq_cfg_0.rst_seq_p1_cmd1_val_reset_value;
      rst_seq_cfg_0.rst_seq_p1_cmd0_en = rst_seq_cfg_0.rst_seq_p1_cmd0_en_reset_value;
      rst_seq_cfg_0.rst_seq_p1_data_ios = rst_seq_cfg_0.rst_seq_p1_data_ios_reset_value;
      rst_seq_cfg_0.rst_seq_p1_data_edge = rst_seq_cfg_0.rst_seq_p1_data_edge_reset_value;
      rst_seq_cfg_0.rst_seq_p1_data_en = rst_seq_cfg_0.rst_seq_p1_data_en_reset_value;
      rst_seq_cfg_0.rst_seq_p1_cmd_ios = rst_seq_cfg_0.rst_seq_p1_cmd_ios_reset_value;
      rst_seq_cfg_0.rst_seq_p1_cmd_edge = rst_seq_cfg_0.rst_seq_p1_cmd_edge_reset_value;
      rst_seq_cfg_1.rst_seq_p1_cmd0_ext_en = rst_seq_cfg_1.rst_seq_p1_cmd0_ext_en_reset_value;
      rst_seq_cfg_1.rst_seq_p1_cmd1_ext_en = rst_seq_cfg_1.rst_seq_p1_cmd1_ext_en_reset_value;
      rst_seq_cfg_1.rst_seq_p1_cmd0_ext_val = rst_seq_cfg_1.rst_seq_p1_cmd0_ext_val_reset_value;
      rst_seq_cfg_1.rst_seq_p1_cmd1_ext_val = rst_seq_cfg_1.rst_seq_p1_cmd1_ext_val_reset_value;
      rst_seq_cfg_1.rst_seq_p1_data_val = rst_seq_cfg_1.rst_seq_p1_data_val_reset_value;
      ers_seq_cfg_0.erss_seq_p1_cmd_val = ers_seq_cfg_0.erss_seq_p1_cmd_val_reset_value;
      ers_seq_cfg_0.erss_seq_p1_cmd_ios = ers_seq_cfg_0.erss_seq_p1_cmd_ios_reset_value;
      ers_seq_cfg_0.erss_seq_p1_cmd_edge = ers_seq_cfg_0.erss_seq_p1_cmd_edge_reset_value;
      ers_seq_cfg_0.erss_seq_p1_addr_cnt = ers_seq_cfg_0.erss_seq_p1_addr_cnt_reset_value;
      ers_seq_cfg_0.erss_seq_p1_cmd_ext_en = ers_seq_cfg_0.erss_seq_p1_cmd_ext_en_reset_value;
      ers_seq_cfg_0.erss_seq_p1_cmd_ext_val = ers_seq_cfg_0.erss_seq_p1_cmd_ext_val_reset_value;
      ers_seq_cfg_0.erss_seq_p1_addr_ios = ers_seq_cfg_0.erss_seq_p1_addr_ios_reset_value;
      ers_seq_cfg_0.erss_seq_p1_addr_edge = ers_seq_cfg_0.erss_seq_p1_addr_edge_reset_value;
      ers_seq_cfg_1.erss_seq_p1_sect_size = ers_seq_cfg_1.erss_seq_p1_sect_size_reset_value;
      ers_seq_cfg_2.ersa_seq_p1_cmd_val = ers_seq_cfg_2.ersa_seq_p1_cmd_val_reset_value;
      ers_seq_cfg_2.ersa_seq_p1_cmd_ios = ers_seq_cfg_2.ersa_seq_p1_cmd_ios_reset_value;
      ers_seq_cfg_2.ersa_seq_p1_cmd_edge = ers_seq_cfg_2.ersa_seq_p1_cmd_edge_reset_value;
      ers_seq_cfg_2.ersa_seq_p1_cmd_ext_en = ers_seq_cfg_2.ersa_seq_p1_cmd_ext_en_reset_value;
      ers_seq_cfg_2.ersa_seq_p1_cmd_ext_val = ers_seq_cfg_2.ersa_seq_p1_cmd_ext_val_reset_value;
      prog_seq_cfg_0.prog_seq_p1_cmd_val = prog_seq_cfg_0.prog_seq_p1_cmd_val_reset_value;
      prog_seq_cfg_0.prog_seq_p1_cmd_ios = prog_seq_cfg_0.prog_seq_p1_cmd_ios_reset_value;
      prog_seq_cfg_0.prog_seq_p1_cmd_edge = prog_seq_cfg_0.prog_seq_p1_cmd_edge_reset_value;
      prog_seq_cfg_0.prog_seq_p1_addr_cnt = prog_seq_cfg_0.prog_seq_p1_addr_cnt_reset_value;
      prog_seq_cfg_0.prog_seq_p1_addr_ios = prog_seq_cfg_0.prog_seq_p1_addr_ios_reset_value;
      prog_seq_cfg_0.prog_seq_p1_addr_edge = prog_seq_cfg_0.prog_seq_p1_addr_edge_reset_value;
      prog_seq_cfg_0.prog_seq_p1_data_ios = prog_seq_cfg_0.prog_seq_p1_data_ios_reset_value;
      prog_seq_cfg_0.prog_seq_p1_data_edge = prog_seq_cfg_0.prog_seq_p1_data_edge_reset_value;
      prog_seq_cfg_0.prog_seq_p1_dummy_cnt = prog_seq_cfg_0.prog_seq_p1_dummy_cnt_reset_value;
      prog_seq_cfg_1.prog_seq_p1_cmd_ext_en = prog_seq_cfg_1.prog_seq_p1_cmd_ext_en_reset_value;
      prog_seq_cfg_1.prog_seq_p1_cmd_ext_val = prog_seq_cfg_1.prog_seq_p1_cmd_ext_val_reset_value;
      prog_seq_cfg_2.prog_seq_p2_target = prog_seq_cfg_2.prog_seq_p2_target_reset_value;
      prog_seq_cfg_2.prog_seq_p2_burst_type = prog_seq_cfg_2.prog_seq_p2_burst_type_reset_value;
      prog_seq_cfg_2.prog_seq_p2_mask_cmd_mod = prog_seq_cfg_2.prog_seq_p2_mask_cmd_mod_reset_value;
      prog_seq_cfg_2.prog_seq_p2_latency_cnt = prog_seq_cfg_2.prog_seq_p2_latency_cnt_reset_value;
      read_seq_cfg_0.read_seq_p1_cmd_val = read_seq_cfg_0.read_seq_p1_cmd_val_reset_value;
      read_seq_cfg_0.read_seq_p1_cmd_ios = read_seq_cfg_0.read_seq_p1_cmd_ios_reset_value;
      read_seq_cfg_0.read_seq_p1_cmd_edge = read_seq_cfg_0.read_seq_p1_cmd_edge_reset_value;
      read_seq_cfg_0.read_seq_p1_addr_cnt = read_seq_cfg_0.read_seq_p1_addr_cnt_reset_value;
      read_seq_cfg_0.read_seq_p1_addr_ios = read_seq_cfg_0.read_seq_p1_addr_ios_reset_value;
      read_seq_cfg_0.read_seq_p1_addr_edge = read_seq_cfg_0.read_seq_p1_addr_edge_reset_value;
      read_seq_cfg_0.read_seq_p1_data_ios = read_seq_cfg_0.read_seq_p1_data_ios_reset_value;
      read_seq_cfg_0.read_seq_p1_data_edge = read_seq_cfg_0.read_seq_p1_data_edge_reset_value;
      read_seq_cfg_0.read_seq_p1_dummy_cnt = read_seq_cfg_0.read_seq_p1_dummy_cnt_reset_value;
      read_seq_cfg_1.read_seq_p1_cmd_ext_en = read_seq_cfg_1.read_seq_p1_cmd_ext_en_reset_value;
      read_seq_cfg_1.read_seq_p1_cache_random_read_en = read_seq_cfg_1.read_seq_p1_cache_random_read_en_reset_value;
      read_seq_cfg_1.read_seq_p1_cmd_ext_val = read_seq_cfg_1.read_seq_p1_cmd_ext_val_reset_value;
      read_seq_cfg_1.read_seq_p1_mb_dummy_cnt = read_seq_cfg_1.read_seq_p1_mb_dummy_cnt_reset_value;
      read_seq_cfg_1.read_seq_p1_mb_en = read_seq_cfg_1.read_seq_p1_mb_en_reset_value;
      read_seq_cfg_2.read_seq_p2_target = read_seq_cfg_2.read_seq_p2_target_reset_value;
      read_seq_cfg_2.read_seq_p2_burst_type = read_seq_cfg_2.read_seq_p2_burst_type_reset_value;
      read_seq_cfg_2.read_seq_p2_mask_cmd_mod = read_seq_cfg_2.read_seq_p2_mask_cmd_mod_reset_value;
      read_seq_cfg_2.read_seq_p2_hf_bound_en = read_seq_cfg_2.read_seq_p2_hf_bound_en_reset_value;
      read_seq_cfg_2.read_seq_p2_latency_cnt = read_seq_cfg_2.read_seq_p2_latency_cnt_reset_value;
      we_seq_cfg_0.we_seq_p1_cmd_val = we_seq_cfg_0.we_seq_p1_cmd_val_reset_value;
      we_seq_cfg_0.we_seq_p1_cmd_ios = we_seq_cfg_0.we_seq_p1_cmd_ios_reset_value;
      we_seq_cfg_0.we_seq_p1_cmd_edge = we_seq_cfg_0.we_seq_p1_cmd_edge_reset_value;
      we_seq_cfg_0.we_seq_p1_cmd_ext_en = we_seq_cfg_0.we_seq_p1_cmd_ext_en_reset_value;
      we_seq_cfg_0.we_seq_p1_cmd_ext_val = we_seq_cfg_0.we_seq_p1_cmd_ext_val_reset_value;
      we_seq_cfg_0.we_seq_p1_en = we_seq_cfg_0.we_seq_p1_en_reset_value;
      stat_seq_cfg_0.stat_seq_p1_cmd_ios = stat_seq_cfg_0.stat_seq_p1_cmd_ios_reset_value;
      stat_seq_cfg_0.stat_seq_p1_cmd_edge = stat_seq_cfg_0.stat_seq_p1_cmd_edge_reset_value;
      stat_seq_cfg_0.stat_seq_p1_cmd_ext_en = stat_seq_cfg_0.stat_seq_p1_cmd_ext_en_reset_value;
      stat_seq_cfg_0.stat_seq_p1_addr_cnt = stat_seq_cfg_0.stat_seq_p1_addr_cnt_reset_value;
      stat_seq_cfg_0.stat_seq_p1_addr_ios = stat_seq_cfg_0.stat_seq_p1_addr_ios_reset_value;
      stat_seq_cfg_0.stat_seq_p1_addr_edge = stat_seq_cfg_0.stat_seq_p1_addr_edge_reset_value;
      stat_seq_cfg_0.stat_seq_p1_data_ios = stat_seq_cfg_0.stat_seq_p1_data_ios_reset_value;
      stat_seq_cfg_0.stat_seq_p1_data_edge = stat_seq_cfg_0.stat_seq_p1_data_edge_reset_value;
      stat_seq_cfg_1.stat_seq_p1_dev_rdy_dummy_cnt = stat_seq_cfg_1.stat_seq_p1_dev_rdy_dummy_cnt_reset_value;
      stat_seq_cfg_1.stat_seq_p1_dev_rdy_addr_en = stat_seq_cfg_1.stat_seq_p1_dev_rdy_addr_en_reset_value;
      stat_seq_cfg_1.stat_seq_p1_prog_fail_dummy_cnt = stat_seq_cfg_1.stat_seq_p1_prog_fail_dummy_cnt_reset_value;
      stat_seq_cfg_1.stat_seq_p1_prog_fail_addr_en = stat_seq_cfg_1.stat_seq_p1_prog_fail_addr_en_reset_value;
      stat_seq_cfg_1.stat_seq_p1_ers_fail_dummy_cnt = stat_seq_cfg_1.stat_seq_p1_ers_fail_dummy_cnt_reset_value;
      stat_seq_cfg_1.stat_seq_p1_ers_fail_addr_en = stat_seq_cfg_1.stat_seq_p1_ers_fail_addr_en_reset_value;
      stat_seq_cfg_2.stat_seq_p1_dev_rdy_cmd_val = stat_seq_cfg_2.stat_seq_p1_dev_rdy_cmd_val_reset_value;
      stat_seq_cfg_2.stat_seq_p1_ers_fail_cmd_val = stat_seq_cfg_2.stat_seq_p1_ers_fail_cmd_val_reset_value;
      stat_seq_cfg_2.stat_seq_p1_prog_fail_cmd_val = stat_seq_cfg_2.stat_seq_p1_prog_fail_cmd_val_reset_value;
      stat_seq_cfg_3.stat_seq_p1_dev_rdy_cmd_ext_val = stat_seq_cfg_3.stat_seq_p1_dev_rdy_cmd_ext_val_reset_value;
      stat_seq_cfg_3.stat_seq_p1_ers_fail_cmd_ext_val = stat_seq_cfg_3.stat_seq_p1_ers_fail_cmd_ext_val_reset_value;
      stat_seq_cfg_3.stat_seq_p1_prog_fail_cmd_ext_val = stat_seq_cfg_3.stat_seq_p1_prog_fail_cmd_ext_val_reset_value;
      stat_seq_cfg_4.stat_seq_p2_mask_cmd_mod = stat_seq_cfg_4.stat_seq_p2_mask_cmd_mod_reset_value;
      stat_seq_cfg_4.stat_seq_p2_latency_cnt = stat_seq_cfg_4.stat_seq_p2_latency_cnt_reset_value;
      stat_seq_cfg_5.stat_seq_dev_rdy_idx = stat_seq_cfg_5.stat_seq_dev_rdy_idx_reset_value;
      stat_seq_cfg_5.stat_seq_dev_rdy_val = stat_seq_cfg_5.stat_seq_dev_rdy_val_reset_value;
      stat_seq_cfg_5.stat_seq_dev_rdy_size = stat_seq_cfg_5.stat_seq_dev_rdy_size_reset_value;
      stat_seq_cfg_5.stat_seq_dev_rdy_en = stat_seq_cfg_5.stat_seq_dev_rdy_en_reset_value;
      stat_seq_cfg_5.stat_seq_ers_fail_idx = stat_seq_cfg_5.stat_seq_ers_fail_idx_reset_value;
      stat_seq_cfg_5.stat_seq_ers_fail_val = stat_seq_cfg_5.stat_seq_ers_fail_val_reset_value;
      stat_seq_cfg_5.stat_seq_ers_fail_size = stat_seq_cfg_5.stat_seq_ers_fail_size_reset_value;
      stat_seq_cfg_5.stat_seq_ers_fail_en = stat_seq_cfg_5.stat_seq_ers_fail_en_reset_value;
      stat_seq_cfg_5.stat_seq_prog_fail_idx = stat_seq_cfg_5.stat_seq_prog_fail_idx_reset_value;
      stat_seq_cfg_5.stat_seq_prog_fail_val = stat_seq_cfg_5.stat_seq_prog_fail_val_reset_value;
      stat_seq_cfg_5.stat_seq_prog_fail_size = stat_seq_cfg_5.stat_seq_prog_fail_size_reset_value;
      stat_seq_cfg_5.stat_seq_prog_fail_en = stat_seq_cfg_5.stat_seq_prog_fail_en_reset_value;
      stat_seq_cfg_7.stat_seq_dev_rdy_addr = stat_seq_cfg_7.stat_seq_dev_rdy_addr_reset_value;
      stat_seq_cfg_8.stat_seq_prog_fail_addr = stat_seq_cfg_8.stat_seq_prog_fail_addr_reset_value;
      stat_seq_cfg_9.stat_seq_ers_fail_addr = stat_seq_cfg_9.stat_seq_ers_fail_addr_reset_value;
      stat_seq_cfg_10.stat_seq_ecc_fail_mask = stat_seq_cfg_10.stat_seq_ecc_fail_mask_reset_value;
      stat_seq_cfg_10.stat_seq_ecc_fail_val = stat_seq_cfg_10.stat_seq_ecc_fail_val_reset_value;
      stat_seq_cfg_10.stat_seq_ecc_corr_val = stat_seq_cfg_10.stat_seq_ecc_corr_val_reset_value;
      stat_seq_cfg_10.stat_seq_crdy_idx = stat_seq_cfg_10.stat_seq_crdy_idx_reset_value;
      stat_seq_cfg_10.stat_seq_crdy_val = stat_seq_cfg_10.stat_seq_crdy_val_reset_value;
      stat_seq_cfg_10.stat_seq_ecc_fail_en = stat_seq_cfg_10.stat_seq_ecc_fail_en_reset_value;
      xspi_ctrl_version.xspi_ctrl_rev = xspi_ctrl_version.xspi_ctrl_rev_reset_value;
      xspi_ctrl_version.xspi_ctrl_fix = xspi_ctrl_version.xspi_ctrl_fix_reset_value;
      xspi_ctrl_version.xspi_ctrl_magic_number = xspi_ctrl_version.xspi_ctrl_magic_number_reset_value;
      ctrl_features_reg.n_threads = ctrl_features_reg.n_threads_reset_value;
      ctrl_features_reg.asf_available = ctrl_features_reg.asf_available_reset_value;
      ctrl_features_reg.boot_available = ctrl_features_reg.boot_available_reset_value;
      ctrl_features_reg.dma_intf = ctrl_features_reg.dma_intf_reset_value;
      ctrl_features_reg.dma_addr_width = ctrl_features_reg.dma_addr_width_reset_value;
      ctrl_features_reg.dma_data_width = ctrl_features_reg.dma_data_width_reset_value;
      ctrl_features_reg.sfr_intf = ctrl_features_reg.sfr_intf_reset_value;
      ctrl_features_reg.n_banks = ctrl_features_reg.n_banks_reset_value;
      wp_settings.wp = wp_settings.wp_reset_value;
      wp_settings.wp_enable = wp_settings.wp_enable_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst = reset_pin_settings.sw_ctrled_hw_rst_reset_value;
      reset_pin_settings.rst_dq3_enable = reset_pin_settings.rst_dq3_enable_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_option = reset_pin_settings.sw_ctrled_hw_rst_option_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_bank0 = reset_pin_settings.sw_ctrled_hw_rst_bank0_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_bank1 = reset_pin_settings.sw_ctrled_hw_rst_bank1_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_bank2 = reset_pin_settings.sw_ctrled_hw_rst_bank2_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_bank3 = reset_pin_settings.sw_ctrled_hw_rst_bank3_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_bank4 = reset_pin_settings.sw_ctrled_hw_rst_bank4_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_bank5 = reset_pin_settings.sw_ctrled_hw_rst_bank5_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_bank6 = reset_pin_settings.sw_ctrled_hw_rst_bank6_reset_value;
      reset_pin_settings.sw_ctrled_hw_rst_bank7 = reset_pin_settings.sw_ctrled_hw_rst_bank7_reset_value;
      clock_mode_settings.spi_clock_mode = clock_mode_settings.spi_clock_mode_reset_value;
      jedec_rst_timing_reg.tCSH_delay = jedec_rst_timing_reg.tCSH_delay_reset_value;
      jedec_rst_timing_reg.tCSL_delay = jedec_rst_timing_reg.tCSL_delay_reset_value;
      dev_delay_reg.cssot_delay = dev_delay_reg.cssot_delay_reset_value;
      dev_delay_reg.cseot_delay = dev_delay_reg.cseot_delay_reset_value;
      dev_delay_reg.csda_min_delay = dev_delay_reg.csda_min_delay_reset_value;
      rst_recovery_reg.rst_recovery = rst_recovery_reg.rst_recovery_reset_value;
      dev_active_max_reg.dev_active_max = dev_active_max_reg.dev_active_max_reset_value;
      hf_offset_reg.hf_offset_index = hf_offset_reg.hf_offset_index_reset_value;
      hf_offset_reg.hf_offset_size = hf_offset_reg.hf_offset_size_reset_value;
      dll_phy_update_cnt.resync_cnt = dll_phy_update_cnt.resync_cnt_reset_value;
      dll_phy_ctrl.resync_idle_cnt = dll_phy_ctrl.resync_idle_cnt_reset_value;
      dll_phy_ctrl.resync_high_wait_cnt = dll_phy_ctrl.resync_high_wait_cnt_reset_value;
      dll_phy_ctrl.extended_rd_mode = dll_phy_ctrl.extended_rd_mode_reset_value;
      dll_phy_ctrl.extended_wr_mode = dll_phy_ctrl.extended_wr_mode_reset_value;
      dll_phy_ctrl.dqs_last_data_drop_en = dll_phy_ctrl.dqs_last_data_drop_en_reset_value;
      dll_phy_ctrl.sdr_edge_active = dll_phy_ctrl.sdr_edge_active_reset_value;
      dll_phy_ctrl.dll_rst_n = dll_phy_ctrl.dll_rst_n_reset_value;
      dll_phy_ctrl.dfi_ctrlupd_req = dll_phy_ctrl.dfi_ctrlupd_req_reset_value;
      phy_dq_timing_reg.data_select_oe_end = phy_dq_timing_reg.data_select_oe_end_reset_value;
      phy_dq_timing_reg.data_select_oe_start = phy_dq_timing_reg.data_select_oe_start_reset_value;
      phy_dq_timing_reg.data_select_tsel_end = phy_dq_timing_reg.data_select_tsel_end_reset_value;
      phy_dq_timing_reg.data_select_tsel_start = phy_dq_timing_reg.data_select_tsel_start_reset_value;
      phy_dq_timing_reg.data_clkperiod_delay = phy_dq_timing_reg.data_clkperiod_delay_reset_value;
      phy_dqs_timing_reg.dqs_select_tsel_end = phy_dqs_timing_reg.dqs_select_tsel_end_reset_value;
      phy_dqs_timing_reg.dqs_select_tsel_start = phy_dqs_timing_reg.dqs_select_tsel_start_reset_value;
      phy_dqs_timing_reg.phony_dqs_sel = phy_dqs_timing_reg.phony_dqs_sel_reset_value;
      phy_dqs_timing_reg.use_phony_dqs = phy_dqs_timing_reg.use_phony_dqs_reset_value;
      phy_dqs_timing_reg.use_lpbk_dqs = phy_dqs_timing_reg.use_lpbk_dqs_reset_value;
      phy_dqs_timing_reg.use_ext_lpbk_dqs = phy_dqs_timing_reg.use_ext_lpbk_dqs_reset_value;
      phy_gate_lpbk_ctrl_reg.gate_cfg = phy_gate_lpbk_ctrl_reg.gate_cfg_reset_value;
      phy_gate_lpbk_ctrl_reg.gate_cfg_close = phy_gate_lpbk_ctrl_reg.gate_cfg_close_reset_value;
      phy_gate_lpbk_ctrl_reg.gate_cfg_always_on = phy_gate_lpbk_ctrl_reg.gate_cfg_always_on_reset_value;
      phy_gate_lpbk_ctrl_reg.lpbk_en = phy_gate_lpbk_ctrl_reg.lpbk_en_reset_value;
      phy_gate_lpbk_ctrl_reg.lpbk_internal = phy_gate_lpbk_ctrl_reg.lpbk_internal_reset_value;
      phy_gate_lpbk_ctrl_reg.loopback_control = phy_gate_lpbk_ctrl_reg.loopback_control_reset_value;
      phy_gate_lpbk_ctrl_reg.lpbk_fail_muxsel = phy_gate_lpbk_ctrl_reg.lpbk_fail_muxsel_reset_value;
      phy_gate_lpbk_ctrl_reg.lpbk_err_check_timing = phy_gate_lpbk_ctrl_reg.lpbk_err_check_timing_reset_value;
      phy_gate_lpbk_ctrl_reg.rd_del_sel_empty = phy_gate_lpbk_ctrl_reg.rd_del_sel_empty_reset_value;
      phy_gate_lpbk_ctrl_reg.underrun_suppress = phy_gate_lpbk_ctrl_reg.underrun_suppress_reset_value;
      phy_gate_lpbk_ctrl_reg.rd_del_sel = phy_gate_lpbk_ctrl_reg.rd_del_sel_reset_value;
      phy_gate_lpbk_ctrl_reg.sync_method = phy_gate_lpbk_ctrl_reg.sync_method_reset_value;
      phy_dll_master_ctrl_reg.param_dll_start_point = phy_dll_master_ctrl_reg.param_dll_start_point_reset_value;
      phy_dll_master_ctrl_reg.param_dll_lock_num = phy_dll_master_ctrl_reg.param_dll_lock_num_reset_value;
      phy_dll_master_ctrl_reg.param_phase_detect_sel = phy_dll_master_ctrl_reg.param_phase_detect_sel_reset_value;
      phy_dll_master_ctrl_reg.param_dll_bypass_mode = phy_dll_master_ctrl_reg.param_dll_bypass_mode_reset_value;
      phy_dll_slave_ctrl_reg.read_dqs_delay = phy_dll_slave_ctrl_reg.read_dqs_delay_reset_value;
      phy_dll_slave_ctrl_reg.clk_wr_delay = phy_dll_slave_ctrl_reg.clk_wr_delay_reset_value;
      phy_ie_timing_reg.rddata_en_ie_dly = phy_ie_timing_reg.rddata_en_ie_dly_reset_value;
      phy_ie_timing_reg.dqs_ie_stop = phy_ie_timing_reg.dqs_ie_stop_reset_value;
      phy_ie_timing_reg.dqs_ie_start = phy_ie_timing_reg.dqs_ie_start_reset_value;
      phy_ie_timing_reg.dq_ie_stop = phy_ie_timing_reg.dq_ie_stop_reset_value;
      phy_ie_timing_reg.dq_ie_start = phy_ie_timing_reg.dq_ie_start_reset_value;
      phy_ie_timing_reg.ie_always_on = phy_ie_timing_reg.ie_always_on_reset_value;
      phy_obs_reg_0.lpbk_status = phy_obs_reg_0.lpbk_status_reset_value;
      phy_obs_reg_0.lpbk_dq_data = phy_obs_reg_0.lpbk_dq_data_reset_value;
      phy_obs_reg_0.dqs_underrun = phy_obs_reg_0.dqs_underrun_reset_value;
      phy_obs_reg_0.dqs_overflow = phy_obs_reg_0.dqs_overflow_reset_value;
      phy_dll_obs_reg_0.dll_lock = phy_dll_obs_reg_0.dll_lock_reset_value;
      phy_dll_obs_reg_0.dll_locked_mode = phy_dll_obs_reg_0.dll_locked_mode_reset_value;
      phy_dll_obs_reg_0.dll_unlock_cnt = phy_dll_obs_reg_0.dll_unlock_cnt_reset_value;
      phy_dll_obs_reg_0.dll_lock_value = phy_dll_obs_reg_0.dll_lock_value_reset_value;
      phy_dll_obs_reg_0.lock_dec_dbg = phy_dll_obs_reg_0.lock_dec_dbg_reset_value;
      phy_dll_obs_reg_0.lock_inc_dbg = phy_dll_obs_reg_0.lock_inc_dbg_reset_value;
      phy_dll_obs_reg_1.decoder_out_rd = phy_dll_obs_reg_1.decoder_out_rd_reset_value;
      phy_dll_obs_reg_1.decoder_out_wr = phy_dll_obs_reg_1.decoder_out_wr_reset_value;
      phy_static_togg_reg.static_tog_clk_div = phy_static_togg_reg.static_tog_clk_div_reset_value;
      phy_static_togg_reg.static_togg_global_enable = phy_static_togg_reg.static_togg_global_enable_reset_value;
      phy_static_togg_reg.static_togg_enable = phy_static_togg_reg.static_togg_enable_reset_value;
      phy_wr_deskew_pd_ctrl_0_reg.dq_phase_detect_sel = phy_wr_deskew_pd_ctrl_0_reg.dq_phase_detect_sel_reset_value;
      phy_wr_deskew_pd_ctrl_0_reg.dq_sw_half_cycle_shift = phy_wr_deskew_pd_ctrl_0_reg.dq_sw_half_cycle_shift_reset_value;
      phy_wr_deskew_pd_ctrl_0_reg.dq_en_sw_half_cycle = phy_wr_deskew_pd_ctrl_0_reg.dq_en_sw_half_cycle_reset_value;
      phy_wr_deskew_pd_ctrl_0_reg.dq_sw_dq_phase_bypass = phy_wr_deskew_pd_ctrl_0_reg.dq_sw_dq_phase_bypass_reset_value;
      phy_version_reg.phy_rev = phy_version_reg.phy_rev_reset_value;
      phy_version_reg.phy_fix = phy_version_reg.phy_fix_reset_value;
      phy_version_reg.combo_phy_magic_number = phy_version_reg.combo_phy_magic_number_reset_value;
      phy_features_reg.onfi_40 = phy_features_reg.onfi_40_reset_value;
      phy_features_reg.onfi_41 = phy_features_reg.onfi_41_reset_value;
      phy_features_reg.sdr_16bit = phy_features_reg.sdr_16bit_reset_value;
      phy_features_reg.xspi = phy_features_reg.xspi_reset_value;
      phy_features_reg.sd_emmc = phy_features_reg.sd_emmc_reset_value;
      phy_features_reg.bank_num = phy_features_reg.bank_num_reset_value;
      phy_features_reg.dll_tap_num = phy_features_reg.dll_tap_num_reset_value;
      phy_features_reg.aging = phy_features_reg.aging_reset_value;
      phy_features_reg.dfi_clock_ratio = phy_features_reg.dfi_clock_ratio_reset_value;
      phy_features_reg.per_bit_deskew = phy_features_reg.per_bit_deskew_reset_value;
      phy_features_reg.reg_intf = phy_features_reg.reg_intf_reset_value;
      phy_features_reg.ext_lpbk_dqs = phy_features_reg.ext_lpbk_dqs_reset_value;
      phy_features_reg.jtag_sup = phy_features_reg.jtag_sup_reset_value;
      phy_features_reg.pll_sup = phy_features_reg.pll_sup_reset_value;
      phy_features_reg.asf_sup = phy_features_reg.asf_sup_reset_value;
      phy_ctrl_reg.ctrl_clkperiod_delay = phy_ctrl_reg.ctrl_clkperiod_delay_reset_value;
      phy_ctrl_reg.phony_dqs_timing = phy_ctrl_reg.phony_dqs_timing_reset_value;
      phy_tsel_reg.tsel_rd_value_dqs = phy_tsel_reg.tsel_rd_value_dqs_reset_value;
      phy_tsel_reg.tsel_off_value_dqs = phy_tsel_reg.tsel_off_value_dqs_reset_value;
      phy_tsel_reg.tsel_rd_value_data = phy_tsel_reg.tsel_rd_value_data_reset_value;
      phy_tsel_reg.tsel_off_value_data = phy_tsel_reg.tsel_off_value_data_reset_value;
      phy_gpio_ctrl_0.phy_gpio_ctrl_0_value = phy_gpio_ctrl_0.phy_gpio_ctrl_0_value_reset_value;
      phy_gpio_ctrl_1.phy_gpio_ctrl_1_value = phy_gpio_ctrl_1.phy_gpio_ctrl_1_value_reset_value;
      phy_gpio_status_0.phy_gpio_status_0_value = phy_gpio_status_0.phy_gpio_status_0_value_reset_value;
      phy_gpio_status_1.phy_gpio_status_1_value = phy_gpio_status_1.phy_gpio_status_1_value_reset_value;
    }
    
    virtual void end_of_elaboration() {
      sc_core::sc_module::end_of_elaboration();
      reset_model();
    }
    
    ModelBaseType& model() {
      return *this;
    }
  
    virtual bool handle_write_cmd_reg0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_cmd_reg1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_cmd_reg2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_cmd_reg3(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_cmd_reg4(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_cmd_reg5(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_cmd_status_ptr(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_intr_enable(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_trd_comp_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_trd_error_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_trd_error_intr_en(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_long_polling(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_short_polling(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_ctrl_config(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_dma_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_discovery_control(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_xip_mode_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_global_seq_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_global_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_direct_access_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_direct_access_rmp(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_direct_access_rmp_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_rst_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_rst_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_ers_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_ers_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_ers_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_ers_seq_cfg_2_ersa_seq_p1_cmd_val(const unsigned int& value, sc_core::sc_time& time) = 0;
    virtual bool handle_write_prog_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_prog_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_prog_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_read_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_read_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_read_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_we_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_3(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_4(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_5(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_7(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_8(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_9(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_stat_seq_cfg_10(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_wp_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_reset_pin_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_clock_mode_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_jedec_rst_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_dev_delay_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_rst_recovery_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_dev_active_max_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_hf_offset_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_dll_phy_update_cnt(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_dll_phy_ctrl(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_dq_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_dqs_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_gate_lpbk_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_dll_master_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_dll_slave_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_ie_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_static_togg_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_wr_deskew_pd_ctrl_0_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_tsel_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_gpio_ctrl_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual bool handle_write_phy_gpio_ctrl_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) = 0;
    virtual void b_transport_axi_slave(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) = 0;
    virtual void b_transport_por_input(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) = 0;

  protected:
    scml_property<int> NUM_TARGETS;

  public:
    scml2::ft_target_socket<32> t_reg_socket;
    sc_core::sc_in<bool> reset_in;
    sc_core::sc_out<bool> int_out;
    tlm_utils::simple_target_socket<SC_CURRENT_USER_MODULE, 64> t_axi_slave_socket;
    tlm_utils::simple_initiator_socket<SC_CURRENT_USER_MODULE, 64> i_dma_socket;
    tlm_utils::simple_target_socket<SC_CURRENT_USER_MODULE, 64> PoR_input_signals;
    scml2::vector< tlm_utils::simple_initiator_socket<SC_CURRENT_USER_MODULE, 64> > xspi_bus_socket;

  protected:
    friend class cdns_xspi_ctrl_regCovermodelBase;
    friend class cdns_xspi_ctrl_regCovermodel;
    friend class cdns_xspi_ctrl_reg;

    struct cmd_reg0_type : public scml2::reg< unsigned int > {
      ~cmd_reg0_type();
      cmd_reg0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command 0 register field. */
      scml2::bitfield< unsigned int > cmd0;
      unsigned int cmd0_reset_value;
    };
    struct cmd_reg1_type : public scml2::reg< unsigned int > {
      ~cmd_reg1_type();
      cmd_reg1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command 1 register field. */
      scml2::bitfield< unsigned int > cmd1;
      unsigned int cmd1_reset_value;
    };
    struct cmd_reg2_type : public scml2::reg< unsigned int > {
      ~cmd_reg2_type();
      cmd_reg2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command 2 register field. */
      scml2::bitfield< unsigned int > cmd2;
      unsigned int cmd2_reset_value;
    };
    struct cmd_reg3_type : public scml2::reg< unsigned int > {
      ~cmd_reg3_type();
      cmd_reg3_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command 3 register field. */
      scml2::bitfield< unsigned int > cmd3;
      unsigned int cmd3_reset_value;
    };
    struct cmd_reg4_type : public scml2::reg< unsigned int > {
      ~cmd_reg4_type();
      cmd_reg4_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command 4 register field. */
      scml2::bitfield< unsigned int > cmd4;
      unsigned int cmd4_reset_value;
    };
    struct cmd_reg5_type : public scml2::reg< unsigned int > {
      ~cmd_reg5_type();
      cmd_reg5_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command 5 register field. */
      scml2::bitfield< unsigned int > cmd5;
      unsigned int cmd5_reset_value;
    };
    struct cmd_status_ptr_type : public scml2::reg< unsigned int > {
      ~cmd_status_ptr_type();
      cmd_status_ptr_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Thread ID whose status will be available in the cmd_status register. */
      scml2::bitfield< unsigned int > thrd_status_sel;
      unsigned int thrd_status_sel_reset_value;
    };
    struct cmd_status_type : public scml2::reg< unsigned int > {
      ~cmd_status_type();
      cmd_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Command status register field. When operating in ACMD mode, this field reports the descriptor status associated with the selected thread (the thread to which this status belongs is selected via the cmd_status_ptr register). 
       * Refer to the UserGuide, section 'Status Checking' within the section 'Operating in Auto Command (ACMD) work mode' for a full bitwise definition.
       * When operating in STIG mode mode, this register reports the STIG completion status. Refer to the UserGuide, section 'Checking STIG Completion Status' for a full bitwise definition.
       */
      scml2::bitfield< unsigned int > cmd_status;
      unsigned int cmd_status_reset_value;
    };
    struct ctrl_status_type : public scml2::reg< unsigned int > {
      ~ctrl_status_type();
      ctrl_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* When set, the AXI Slave interface is busy. */
      scml2::bitfield< unsigned int > sdma_busy;
      unsigned int sdma_busy_reset_value;
      /* When set, the AXI Master interface is busy.. */
      scml2::bitfield< unsigned int > mdma_busy;
      unsigned int mdma_busy_reset_value;
      /* When set, the Auto Command Engine is busy (ACMD mode only). */
      scml2::bitfield< unsigned int > acmd_eng_busy;
      unsigned int acmd_eng_busy_reset_value;
      /* When operating in DIRECT work mode, this bit will be set high when the DIRECT CMD Generator is busy. Note that no new request on the AXI slave interface will be accepted while this is high When operating in STIG work mode, this bit will be set while the STIG engine is busy. */
      scml2::bitfield< unsigned int > gcmd_eng_busy;
      unsigned int gcmd_eng_busy_reset_value;
      /* This bit is relevant for STIG mode only. This bit indicates when the controller is waiting for next/last instruction in glued instruction chain or it is executing requested sequence on the xSPI i/f. */
      scml2::bitfield< unsigned int > gcmd_eng_mc_busy;
      unsigned int gcmd_eng_mc_busy_reset_value;
      /* When set, Device Discovery is in progress. When Device Discovery is inhibited then this bit is also set during the PHY initialization procedure. */
      scml2::bitfield< unsigned int > discovery_busy;
      unsigned int discovery_busy_reset_value;
      /**
       * This bit indicates if controller is in the busy state or not.
       * 1: Controller is busy.
       * 0: Controller is idle.
       * Note that this bit is also routed to the controller interface via the ctrl_busy pin.
       */
      scml2::bitfield< unsigned int > ctrl_busy;
      unsigned int ctrl_busy_reset_value;
      /**
       * Initialization process status:
       * 2'b00: xSPI device detected,
       * 2'b01: Initialization has failed,
       * 2'b10: Legacy SPI device detected,
       * 2'b11: n/a.
       */
      scml2::bitfield< unsigned int > init_fail;
      unsigned int init_fail_reset_value;
      /* This bit is set when the Cadence xSPI Controller has completed its reset and initialization process. */
      scml2::bitfield< unsigned int > init_comp;
      unsigned int init_comp_reset_value;
    };
    struct trd_status_type : public scml2::reg< unsigned int > {
      ~trd_status_type();
      trd_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* This bit indicates the Auto Command Engine thread busy status. When set, the corresponding thread is currently busy. */
      scml2::bitfield< unsigned int > trd_busy;
      unsigned int trd_busy_reset_value;
    };
    struct intr_status_type : public scml2::reg< unsigned int > {
      ~intr_status_type();
      intr_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* This simply reflects that a HIGH-to-LOW or LOW-to-HIGH transition detected on the xspi_dfi_gp_open_drain[0] input pin. */
      scml2::bitfield< unsigned int > gp_open_drain_0;
      unsigned int gp_open_drain_0_reset_value;
      /* This simply reflects that a HIGH-to-LOW or LOW-to-HIGH transition detected on the xspi_dfi_gp_open_drain[1] input pin. */
      scml2::bitfield< unsigned int > gp_open_drain_1;
      unsigned int gp_open_drain_1_reset_value;
      /* This simply reflects that a HIGH-to-LOW or LOW-to-HIGH transition detected on the xspi_dfi_gp_open_drain[2] input pin. */
      scml2::bitfield< unsigned int > gp_open_drain_2;
      unsigned int gp_open_drain_2_reset_value;
      /* This simply reflects that a HIGH-to-LOW or LOW-to-HIGH transition was detected on the xspi_dfi_gp_open_drain[3] input pin. */
      scml2::bitfield< unsigned int > gp_open_drain_3;
      unsigned int gp_open_drain_3_reset_value;
      /* This is general status and indicates that the xSPI controller has returned to an IDLE state. */
      scml2::bitfield< unsigned int > ctrl_idle;
      unsigned int ctrl_idle_reset_value;
      /* Command DMA Target error. ACMD mode only. This bit will be set if a system bus error was detected on the AXI Master Interface during descriptor reads or writes */
      scml2::bitfield< unsigned int > cdma_terr;
      unsigned int cdma_terr_reset_value;
      /* Master Data DMA Target error. ACMD mode only. This bit will be set if a system bus error was detected on the AXI Master Interface during data reads or writes */
      scml2::bitfield< unsigned int > ddma_terr;
      unsigned int ddma_terr_reset_value;
      /**
       * ACMD work mode: The controller detected that a command that was sent by the host was to an already busy thread and ignored it.
       * STIG work mode: The controller detected that a command was sent when the STIG engine was already busy and ignored it.
       */
      scml2::bitfield< unsigned int > cmd_ignored;
      unsigned int cmd_ignored_reset_value;
      /* This bit is set when the trigger condition for the Slave DMA is met. It is relevant to STIG and ACMD mode only and used to instruct the host when an access on the AXI slave interface is to be performed. This is explained in more detail in the Operating in ACMD or STIG mode sections of the UserGuide */
      scml2::bitfield< unsigned int > sdma_trigg;
      unsigned int sdma_trigg_reset_value;
      /* This bit is set when an illegal access to the Slave DMA interface is detected. */
      scml2::bitfield< unsigned int > sdma_err;
      unsigned int sdma_err_reset_value;
      /* This bit is set when the last instruction in glued chain has completed. */
      scml2::bitfield< unsigned int > stig_done;
      unsigned int stig_done_reset_value;
      /* This bit is set when CRC checking is enabled and a CRC error after a read or status checking command in DIRECT work mode has been detected. */
      scml2::bitfield< unsigned int > dir_crc_err;
      unsigned int dir_crc_err_reset_value;
      /* This bit is set when an incorrect number of DQS pulses were detected during a DIRECT mode read or status check. Essentially this status is passed from the soft PHY and could mean that the PHY is configured badly such that it expects DQS strobes but never received them or that the rd_del_sel value is incorrect and has caused data corruption and pointer misalignment in the PHY.  To resolve this issue the PHY must be reset to clear the dqs_underrun and dqs_overflow flags and reset the read data pointers. */
      scml2::bitfield< unsigned int > dir_dqs_err;
      unsigned int dir_dqs_err_reset_value;
      /* This bit is typically set when the host has attempted to do something that is unsupported or incorrect. An example could be that the host has triggered a write when the device is operating in XIP mode. The full list of error conditions is described in the Error/Event Handling section of the UserGuide. */
      scml2::bitfield< unsigned int > dir_cmd_err;
      unsigned int dir_cmd_err_reset_value;
      /* For IP6522A, this bit is set when a correctable ECC error occurs in DIRECT mode. */
      scml2::bitfield< unsigned int > dir_ecc_corr_err;
      unsigned int dir_ecc_corr_err_reset_value;
      /* Set when a program operation in DIRECT mode failed. That is, the device returned the program fail bit when the status was checked. For IP6522A, this bit is also set when uncorrectable ECC error occurs in DIRECT mode. */
      scml2::bitfield< unsigned int > dir_dev_err;
      unsigned int dir_dev_err_reset_value;
    };
    struct intr_enable_type : public scml2::reg< unsigned int > {
      ~intr_enable_type();
      intr_enable_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Interrupt enable for detecting the HIGH-to-LOW or LOW-to-HIGH transition on the xspi_dfi_gp_open_drain[0] input pin. */
      scml2::bitfield< unsigned int > gp_open_drain_0_en;
      unsigned int gp_open_drain_0_en_reset_value;
      /* Interrupt enable for detecting the HIGH-to-LOW or LOW-to-HIGH transition on the xspi_dfi_gp_open_drain[1] input pin. */
      scml2::bitfield< unsigned int > gp_open_drain_1_en;
      unsigned int gp_open_drain_1_en_reset_value;
      /* Interrupt enable for detecting the HIGH-to-LOW or LOW-to-HIGH transition on the xspi_dfi_gp_open_drain[2] input pin. */
      scml2::bitfield< unsigned int > gp_open_drain_2_en;
      unsigned int gp_open_drain_2_en_reset_value;
      /* Interrupt enable for detecting the HIGH-to-LOW or LOW-to-HIGH transition on the xspi_dfi_gp_open_drain[3] input pin. */
      scml2::bitfield< unsigned int > gp_open_drain_3_en;
      unsigned int gp_open_drain_3_en_reset_value;
      /* Interrupt enable for detecting that Controller has returned to the IDLE state. */
      scml2::bitfield< unsigned int > ctrl_idle_en;
      unsigned int ctrl_idle_en_reset_value;
      /* Interrupt enable for detecting Auto CMD Engine target error. */
      scml2::bitfield< unsigned int > cdma_terr_en;
      unsigned int cdma_terr_en_reset_value;
      /* Interrupt enable for detecting Data DMA Master target error. */
      scml2::bitfield< unsigned int > ddma_terr_en;
      unsigned int ddma_terr_en_reset_value;
      /* Interrupt enable for detecting of ignored command. */
      scml2::bitfield< unsigned int > cmd_ignored_en;
      unsigned int cmd_ignored_en_reset_value;
      /* Enables interrupt when the trigger condition for the Slave DMA is met. */
      scml2::bitfield< unsigned int > sdma_trigg_en;
      unsigned int sdma_trigg_en_reset_value;
      /* Enables interrupt when an illegal access to the Slave DMA interface is detected. */
      scml2::bitfield< unsigned int > sdma_err_en;
      unsigned int sdma_err_en_reset_value;
      /* Enables interrupt when an instruction in glued chain is completed. */
      scml2::bitfield< unsigned int > stig_done_en;
      unsigned int stig_done_en_reset_value;
      /* Enables interrupt when the controller returns CRC error after read or status checking command in DIRECT work mode. */
      scml2::bitfield< unsigned int > dir_crc_err_en;
      unsigned int dir_crc_err_en_reset_value;
      /* Enables interrupt when the controller returns DQS error after read or status checking command in DIRECT work mode. */
      scml2::bitfield< unsigned int > dir_dqs_err_en;
      unsigned int dir_dqs_err_en_reset_value;
      /* Enables interrupt when an invalid command sequence has been detected in DIRECT work mode. */
      scml2::bitfield< unsigned int > dir_cmd_err_en;
      unsigned int dir_cmd_err_en_reset_value;
      /* Enables interrupt when a correctable ECC error occurred in DIRECT work mode. */
      scml2::bitfield< unsigned int > dir_ecc_corr_err_en;
      unsigned int dir_ecc_corr_err_en_reset_value;
      /* Enables interrupt when an uncorrectable ECC or program fail error occurred in DIRECT work mode. */
      scml2::bitfield< unsigned int > dir_dev_err_en;
      unsigned int dir_dev_err_en_reset_value;
      /* Global Interrupts enable flag. */
      scml2::bitfield< unsigned int > intr_en;
      unsigned int intr_en_reset_value;
    };
    struct trd_comp_intr_status_type : public scml2::reg< unsigned int > {
      ~trd_comp_intr_status_type();
      trd_comp_intr_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Thread 0 operation complete flag. */
      scml2::bitfield< unsigned int > trd0_comp;
      unsigned int trd0_comp_reset_value;
      /* Thread 1 operation complete flag. */
      scml2::bitfield< unsigned int > trd1_comp;
      unsigned int trd1_comp_reset_value;
      /* Thread 2 operation complete flag. */
      scml2::bitfield< unsigned int > trd2_comp;
      unsigned int trd2_comp_reset_value;
      /* Thread 3 operation complete flag. */
      scml2::bitfield< unsigned int > trd3_comp;
      unsigned int trd3_comp_reset_value;
      /* Thread 4 operation complete flag. */
      scml2::bitfield< unsigned int > trd4_comp;
      unsigned int trd4_comp_reset_value;
      /* Thread 5 operation complete flag. */
      scml2::bitfield< unsigned int > trd5_comp;
      unsigned int trd5_comp_reset_value;
      /* Thread 6 operation complete flag. */
      scml2::bitfield< unsigned int > trd6_comp;
      unsigned int trd6_comp_reset_value;
      /* Thread 7 operation complete flag. */
      scml2::bitfield< unsigned int > trd7_comp;
      unsigned int trd7_comp_reset_value;
    };
    struct trd_error_intr_status_type : public scml2::reg< unsigned int > {
      ~trd_error_intr_status_type();
      trd_error_intr_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Thread 0 error. */
      scml2::bitfield< unsigned int > trd0_error_stat;
      unsigned int trd0_error_stat_reset_value;
      /* Thread 1 error. */
      scml2::bitfield< unsigned int > trd1_error_stat;
      unsigned int trd1_error_stat_reset_value;
      /* Thread 2 error. */
      scml2::bitfield< unsigned int > trd2_error_stat;
      unsigned int trd2_error_stat_reset_value;
      /* Thread 3 error. */
      scml2::bitfield< unsigned int > trd3_error_stat;
      unsigned int trd3_error_stat_reset_value;
      /* Thread 4 error. */
      scml2::bitfield< unsigned int > trd4_error_stat;
      unsigned int trd4_error_stat_reset_value;
      /* Thread 5 error. */
      scml2::bitfield< unsigned int > trd5_error_stat;
      unsigned int trd5_error_stat_reset_value;
      /* Thread 6 error. */
      scml2::bitfield< unsigned int > trd6_error_stat;
      unsigned int trd6_error_stat_reset_value;
      /* Thread 7 error. */
      scml2::bitfield< unsigned int > trd7_error_stat;
      unsigned int trd7_error_stat_reset_value;
    };
    struct trd_error_intr_en_type : public scml2::reg< unsigned int > {
      ~trd_error_intr_en_type();
      trd_error_intr_en_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Interrupt enable for detecting thread error. */
      scml2::bitfield< unsigned int > trd_error_intr_en;
      unsigned int trd_error_intr_en_reset_value;
    };
    struct dma_target_error_l_type : public scml2::reg< unsigned int > {
      ~dma_target_error_l_type();
      dma_target_error_l_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Address of the first request on the
       * master interface that returned error
       * response.
       */
      scml2::bitfield< unsigned int > target_err_l;
      unsigned int target_err_l_reset_value;
    };
    struct dma_target_error_h_type : public scml2::reg< unsigned int > {
      ~dma_target_error_h_type();
      dma_target_error_h_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Address of the first request on the
       * master interface that returned error
       * response.
       */
      scml2::bitfield< unsigned int > target_err_h;
      unsigned int target_err_h_reset_value;
    };
    struct boot_status_type : public scml2::reg< unsigned int > {
      ~boot_status_type();
      boot_status_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * This field describes dqs status during boot process. When set, the boot process failed due to the
       * dqs error on the xspi interface. Allowed values are:
       * [list]
       * [*] 0 - no error detected,
       * [*] 1 - error detected.
       * [/list]
       */
      scml2::bitfield< unsigned int > boot_dqs_err;
      unsigned int boot_dqs_err_reset_value;
      /**
       * This field describes crc status during boot process. When set, the boot process failed due to the
       * crc error on the xspi interface. Allowed values are:
       * [list]
       * [*] 0 - no error detected,
       * [*] 1 - error detected.
       * [/list]
       */
      scml2::bitfield< unsigned int > boot_crc_err;
      unsigned int boot_crc_err_reset_value;
      /**
       * This field describes bus status during boot process. When set, the boot process failed due to the
       * bus interface receiving an error response from the
       * target. Allowed values are:
       * [list]
       * [*] 0 - no error detected,
       * [*] 1 - error detected.
       * [/list]
       */
      scml2::bitfield< unsigned int > boot_bus_err;
      unsigned int boot_bus_err_reset_value;
    };
    struct long_polling_type : public scml2::reg< unsigned int > {
      ~long_polling_type();
      long_polling_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Number of system clock cycles after an erase/write operation has been issued before the controller starts to check device status (ready/busy and fail/pass).
       * First status checking polling will happen after at least this many number of system clock cycles.
       * Next status checking will happen every short_polling cycles.
       * The long polling value should be significantly larger the short polling value.
       */
      scml2::bitfield< unsigned int > long_polling;
      unsigned int long_polling_reset_value;
    };
    struct short_polling_type : public scml2::reg< unsigned int > {
      ~short_polling_type();
      short_polling_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Minimum number of system clocks after long polling delay before the controller starts to poll for status if the controller was busy during the first status poll attempt. 
       * The long polling value should be significantly larger the short polling value.
       */
      scml2::bitfield< unsigned int > short_polling;
      unsigned int short_polling_reset_value;
    };
    struct ctrl_config_type : public scml2::reg< unsigned int > {
      ~ctrl_config_type();
      ctrl_config_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * This control bit is relevant only for ACMD work mode.
       * If this bit is cleared and an error occurs, the controller will drop
       * execution of all further operations programmed in a single thread
       * at the page (for read or program commands)
       * or sector (for the sector erase command) boundary.
       * It applys to both Command DMA and PIO work modes.
       * When this bit is set execution will continue.
       */
      scml2::bitfield< unsigned int > cont_on_err;
      unsigned int cont_on_err_reset_value;
      /**
       * Field selecting controllers work mode. Allowed values are:
       * [list]
       * [*] 00 - DIRECT mode,
       * [*] 01 - STIG mode,
       * [*] 11 - ACMD mode.
       * [/list]
       */
      scml2::bitfield< unsigned int > work_mode;
      unsigned int work_mode_reset_value;
    };
    struct dma_settings_type : public scml2::reg< unsigned int > {
      ~dma_settings_type();
      dma_settings_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Controls the maximum AXI burst size used by the master interface. The maximum burst size can be
       * calculated as burst_sel+1.  Note that this field should be changed
       * only if controller is in an IDLE state.
       */
      scml2::bitfield< unsigned int > burst_sel;
      unsigned int burst_sel_reset_value;
      /**
       * Outstanding transaction enable. This only applies to the AXI
       * master interface. The AXI slave interface will ignore this bit
       * and will accept all incoming transactions.
       */
      scml2::bitfield< unsigned int > OTE;
      unsigned int OTE_reset_value;
      /**
       * AXI error responses can only occur in ACMD or STIG work modes. If this bit is set then an AXI ERROR response will be returned if one of the following is true ..
       *   1. The host attempts to access the slave interface before it is permitted (refer to the required steps in STIG and ACMD sections of this document).
       *   2. The host issues an unsupported burst type. The controller only supports incremental bursts (non-wrapping) bursts currently.
       * 
       * If this bit is not set, an OK response is returned.
       */
      scml2::bitfield< unsigned int > sdma_err_rsp;
      unsigned int sdma_err_rsp_reset_value;
      /**
       * This field selects the AXI transaction size used by the master interface to transfer data. Field encoding is as following:
       * [list]
       * [*] 2'b00 - Byte,
       * [*] 2'b01 - 16-bit word,
       * [*] 2'b10 - 32-bit word,
       * [*] 2'b11 - 64-bit word.
       * [/list]
       */
      scml2::bitfield< unsigned int > word_size;
      unsigned int word_size_reset_value;
    };
    struct sdma_size_type : public scml2::reg< unsigned int > {
      ~sdma_size_type();
      sdma_size_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Transferred data block size in bytes for the Slave DMA module. Data size is rounded up to the data bus word size. */
      scml2::bitfield< unsigned int > sdma_size;
      unsigned int sdma_size_reset_value;
    };
    struct sdma_trd_info_type : public scml2::reg< unsigned int > {
      ~sdma_trd_info_type();
      sdma_trd_info_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Thread number associated with transferred data block for the Slave DMA module. */
      scml2::bitfield< unsigned int > sdma_trd;
      unsigned int sdma_trd_reset_value;
      /* Transfer direction related to current Slave DMA transfer (0-read; 1-write). */
      scml2::bitfield< unsigned int > sdma_dir;
      unsigned int sdma_dir_reset_value;
    };
    struct sdma_addr0_type : public scml2::reg< unsigned int > {
      ~sdma_addr0_type();
      sdma_addr0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* The SDMA destination/source address - lower part. */
      scml2::bitfield< unsigned int > sdma_addr_l;
      unsigned int sdma_addr_l_reset_value;
    };
    struct sdma_addr1_type : public scml2::reg< unsigned int > {
      ~sdma_addr1_type();
      sdma_addr1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* The SDMA destination/source address - higher part. */
      scml2::bitfield< unsigned int > sdma_addr_h;
      unsigned int sdma_addr_h_reset_value;
    };
    struct discovery_control_type : public scml2::reg< unsigned int > {
      ~discovery_control_type();
      discovery_control_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Discovery request signal.
       * Writing 1 triggers Device Discovery operation.
       * This bit is cleared by hardware when DD operation completes.
       */
      scml2::bitfield< unsigned int > discovery_req;
      unsigned int discovery_req_reset_value;
      /**
       * Discovery request type:
       * 0: perform full discovery process (try to detect device);
       * 1: configure registers only to selected mode (not full discovery process).
       */
      scml2::bitfield< unsigned int > discovery_req_type;
      unsigned int discovery_req_type_reset_value;
      /**
       * Status of the last Discovery operation.
       * This bit is 1 when Device Discovery
       * operation has finished. The result can be read from discovery_fail field.
       */
      scml2::bitfield< unsigned int > discovery_comp;
      unsigned int discovery_comp_reset_value;
      /**
       * Result of the last Discovery operation.
       * Valid if discovery_comp is 1.
       * 2'b00: xSPI or SPI NAND device detected,
       * 2'b01: failed,
       * 2'b10: Legacy SPI device detected,
       * 2'b11: n/a.
       */
      scml2::bitfield< unsigned int > discovery_fail;
      unsigned int discovery_fail_reset_value;
      /**
       * Discovery inhibit status.
       * This is a status bit to inform whether Device Discovery is inhibited at power-on.
       * 1 - discovery inhibited, 0 - discovery allowed.
       */
      scml2::bitfield< unsigned int > discovery_inhibit;
      unsigned int discovery_inhibit_reset_value;
      /**
       * Discovery extended op-code value.
       * 1: Extended op-code is 8'h5A; 0: Extended op-code is 8'hA5.
       * This field is updated after initialization process.
       */
      scml2::bitfield< unsigned int > discovery_extop_val;
      unsigned int discovery_extop_val_reset_value;
      /**
       * Discovery extended op-code enable.
       * 1: Extended op-code enabled; 0: Extended op-code disabled.
       * This field is updated after initialization process.
       */
      scml2::bitfield< unsigned int > discovery_extop_en;
      unsigned int discovery_extop_en_reset_value;
      /**
       * Discovery command type mode enable.
       * 1: DDR mode enabled; 0: SDR mode enabled; 2: DTR mode enabled (QUAD mode only).
       *  This field is updated after initialization process.
       */
      scml2::bitfield< unsigned int > discovery_cmd_type;
      unsigned int discovery_cmd_type_reset_value;
      /**
       * Discovery number of dummy clock cycles.
       * 0 - 8 dummy clock cycles; 1 - 20 dummy clock cycles.
       *  This field is updated after initialization process.
       */
      scml2::bitfield< unsigned int > discovery_dummy_cnt;
      unsigned int discovery_dummy_cnt_reset_value;
      /**
       * Discovery 4-bit addressing enable.
       * 0: 3-bit addressing; 1: 4-bit addressing.
       *  This field is updated after initialization process.
       */
      scml2::bitfield< unsigned int > discovery_abnum;
      unsigned int discovery_abnum_reset_value;
      /**
       * Discovery mode. This is a 4-bit value.
       * Writing a value selects number of xSPI I/Os used by Device Discovery.
       * 0x0: Auto;
       * 0x1: 1 line;
       * 0x2: 2 lines;
       * 0x4: 4 lines;
       * 0x8: 8 lines;
       * 0xC: 8 lines for Legacy Hyper Flash and xSPI Profile 2.0;
       * 0xE: 1 line for Legacy SPI NAND;
       * Other values are reserved.
       *  This field is updated after initialization process.
       */
      scml2::bitfield< unsigned int > discovery_num_lines;
      unsigned int discovery_num_lines_reset_value;
      /**
       * Discovery bank select. This is a 3-bit value.
       * Writing value of 0x0-0x7 selects a bank.
       *  This field is updated after initialization process.
       */
      scml2::bitfield< unsigned int > discovery_bank;
      unsigned int discovery_bank_reset_value;
    };
    struct xip_mode_cfg_type : public scml2::reg< unsigned int > {
      ~xip_mode_cfg_type();
      xip_mode_cfg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * XIP mode enable for selected memory bank.
       * If XIP mode is enabled only READ sequences are valid.
       * Invoking any other command sequence will be ignored
       * and CMD_ERROR/DSC_ERROR/dir_cmd_err will be rise.
       */
      scml2::bitfield< unsigned int > xip_en;
      unsigned int xip_en_reset_value;
      /* Value of mode-bits required to enable XIP mode. */
      scml2::bitfield< unsigned int > xip_en_mb_val;
      unsigned int xip_en_mb_val_reset_value;
      /* Value of mode-bits required to disable XIP mode. */
      scml2::bitfield< unsigned int > xip_dis_mb_val;
      unsigned int xip_dis_mb_val_reset_value;
    };
    struct global_seq_cfg_type : public scml2::reg< unsigned int > {
      ~global_seq_cfg_type();
      global_seq_cfg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Determines page size of device being used for READ operations.
       * Number of bytes in page = 2^page_size.
       * Allowed values are:
       * [list]
       * [*] 4'b0000 - 1B,
       * [*] 4'b0001 - 2B,
       * [*] ...,
       * [*] 4'b1000 - 256B,
       * [*] 4'b1001 - 512B,
       * [*] 4'b1010 - 1024B,
       * [*] 4'b1011 - 2048B,
       * [*] 4'b1100 - 4096B,
       * [*] 4'b1101 - n/a,
       * [*] ...,
       * [*] 4'b1111 - unlimited (controller will send all data with single xSPI command).
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_page_size_rd;
      unsigned int seq_page_size_rd_reset_value;
      /**
       * Determines page size of device being used for PROGRAM operations.
       * Number of bytes in page = 2^page_size.
       * Allowed values are:
       * [list]
       * [*] 4'b0000 - 1B,
       * [*] 4'b0001 - 2B,
       * [*] ...,
       * [*] 4'b1000 - 256B,
       * [*] 4'b1001 - 512B,
       * [*] 4'b1010 - 1024B,
       * [*] 4'b1011 - 2048B,
       * [*] 4'b1100 - 4096B,
       * [*] 4'b1101 - n/a,
       * [*] ...,
       * [*] 4'b1111 - n/a.
       * [/list]
       * This field is not used in DIRECT mode for PROFILE 2 - HR or when SPI NAND device is selected.
       */
      scml2::bitfield< unsigned int > seq_page_size_pgm;
      unsigned int seq_page_size_pgm_reset_value;
      /**
       * Setting this bit enables dynamic CRC calculation based on all previous bytes in the
       * current sequence and puts this value on xSPI Flash Interface.
       * Not required by Legacy Hyper Flash and xSPI Profile 2.0 Devices but
       * can be useful for external Flash Monitor to control data integrity.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Disable,
       * [*] 1'b1 - Enable.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_crc_en;
      unsigned int seq_crc_en_reset_value;
      /**
       * Selecting of CRC variant. Allowed values are:
       * [list]
       * [*] 1'b0 - CRC is calculated for all bytes of address transfer phase only and
       * put on the bus after address transfer phase.
       * [*] 1'b1 - CRC is calculated for all bytes in sequence and put on the bus after all bytes
       * in sequence.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_crc_variant;
      unsigned int seq_crc_variant_reset_value;
      /**
       * This field determines if the controller expects the xSPI device to toggle
       * CRC data on both SPI clock edges in CRC->CRC# sequence.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Disable,
       * [*] 1'b1 - Enable.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_crc_oe;
      unsigned int seq_crc_oe_reset_value;
      /**
       * This field indicates the number of bytes after which CRC occurs.
       * Allowed values are:
       * [list]
       * [*] 3'b000 - n/a,
       * [*] 3'b001 - 8B,
       * [*] 3'b010 - 16B,
       * [*] 3'b011 - 32B,
       * [*] 3'b100 - 64B,
       * [*] 3'b101 - 128B,
       * [*] 3'b110 - 256B,
       * [*] 3'b111 - 512B.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_crc_chunk_size;
      unsigned int seq_crc_chunk_size_reset_value;
      /**
       * Setting this bit enables taking into consideration the command address to
       * determine after how many bytes CRC data slice is expected to
       * be returned by the Flash Device.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Disable,
       * [*] 1'b1 - Enable.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_crc_ual_chunk_en;
      unsigned int seq_crc_ual_chunk_en_reset_value;
      /**
       * Setting this bit enables the checking of CRC unaligned chunk from the Flash
       * Device. It can be set high only if seq_crc_ual_chunk_en = 1.
       * It must be set low otherwise.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Disable,
       * [*] 1'b1 - Enable.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_crc_ual_chunk_chk;
      unsigned int seq_crc_ual_chunk_chk_reset_value;
      /**
       * Setting this bit enables tCMS timing in PROFILE 1. Refer to the dev_active_max register at offset 0x1018 for further details.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Disable,
       * [*] 1'b1 - Enable.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_tcms_en;
      unsigned int seq_tcms_en_reset_value;
      /**
       * Enables reversed byte order. This bit can be set only when data phase reflects Octal DDR mode.
       * In other modes this bit must be set to low.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Disable,
       * [*] 1'b1 - Enable.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_data_swap;
      unsigned int seq_data_swap_reset_value;
      /**
       * Selects data organization of the xSPI memory.
       * Please note that xSPI address/pointer specified during sending
       * CDMA/PIO and
       * DIRECT
       * command is always byte-aligned
       * (i.e. if this field is set, xSPI address must be even).
       * [list]
       * [*] 1'b0 - 1B per single memory address (no translation of xSPI address),
       * [*] 1'b1 - 2B per single memory address (translation of xSPI address from byte address to word address will be performed automatically).
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_data_per_addr;
      unsigned int seq_data_per_addr_reset_value;
      /**
       * Sequence type (common for all sequences):
       * [list]
       * [*] 0 - PROFILE 1,
       * [*] 1 - PROFILE 2 - HF (HyperFlash),
       * [*] 2 - PROFILE 2 - HR (HyperRAM),
       * [*] 3 - SPI NAND.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_type;
      unsigned int seq_type_reset_value;
    };
    struct global_seq_cfg_1_type : public scml2::reg< unsigned int > {
      ~global_seq_cfg_1_type();
      global_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Determines the extended page size area (spare area size) for SPI NAND devices.
       * The number of data bytes transmitted to/from each page will be extended by a value of this
       * field.
       * This field is not used in DIRECT mode.
       */
      scml2::bitfield< unsigned int > seq_page_size_ext;
      unsigned int seq_page_size_ext_reset_value;
      /**
       * Width of the Column Address for SPI NAND devices. Value of this field is used to calculate
       * the next page address in case when data size specified in sequence exceed the current page capacity.
       * [list]
       * [*] 1'd0 - 12 bit address width,
       * [*] 1'd1 - 13 bit address width
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_page_ca_size;
      unsigned int seq_page_ca_size_reset_value;
      /**
       * Number of pages per blocks for SPI NAND device (encoded as 2^N):
       * [list]
       * [*] 0 - 1 page per block,
       * [*] 1 - 2 pages per block,
       * [*] ...
       * [*] 6 - 64 pages per block,
       * [*] 7 - 128 pages per block.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_page_per_block;
      unsigned int seq_page_per_block_reset_value;
      /**
       * Number of planes in SPI NAND device (encoded as 2^N):
       * [list]
       * [*] 0 - single plane,
       * [*] 1 - two planes,
       * [*] 2 - four planes,
       * [*] 3 - reserved.
       * [/list]
       */
      scml2::bitfield< unsigned int > seq_plane_cnt;
      unsigned int seq_plane_cnt_reset_value;
    };
    struct direct_access_cfg_type : public scml2::reg< unsigned int > {
      ~direct_access_cfg_type();
      direct_access_cfg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* This field selects the bank (the attached memory device) targeted by the controller while operating in DIRECT mode. */
      scml2::bitfield< unsigned int > dac_bank_num;
      unsigned int dac_bank_num_reset_value;
      /**
       * When enabled, the controller will support byte masking in the 
       * connected device by automatically translating the incoming AXI 
       * slave write strobes as needed.  If the connected device does not 
       * support RWDS byte masking, this bit must be set low. Note that 
       * when low, the controller assumes all bytes on the AXI write 
       * channel have an equivalent AXI write strobe set to '1'.  
       * Also note that when this bit is set low, the user cannot 
       * send byte writes to an octal DDR device or 
       * to a device that is 16bit addressed. 
       * 
       * NOTE this feature is NOT supported with the IP variant IP6523 when 
       * purchased without the soft PHY.
       * It is only available for the IP6523-with-soft-phy variant and the 
       * IP6522 controller variants.
       */
      scml2::bitfield< unsigned int > rwds_cap_en;
      unsigned int rwds_cap_en_reset_value;
      /**
       * This bit is used to trigger entry to XIP mode within the device. Essentially, if set to 1, the controller will send mode bits as specified in the
       * xip_en_mb_val on the next READ transaction.
       * This will cause switching both device and controller
       * into XIP work mode.
       */
      scml2::bitfield< unsigned int > mode_bit_xip_en;
      unsigned int mode_bit_xip_en_reset_value;
      /**
       * This bit is used to trigger exit from XIP mode within the device. Essentially, if set to 1,  controller will send mode bits specified in the
       * xip_dis_mb_val on the next READ transaction.
       * This will cause disabling XIP work mode for both
       * device and controller.
       */
      scml2::bitfield< unsigned int > mode_bit_xip_dis;
      unsigned int mode_bit_xip_dis_reset_value;
      /**
       * Enables Slave Data Interface address remapping.
       * When set to 1, the incoming AXI Slave address will be adapted
       * and sent to the Flash device as (address - N), where N is the value stored
       * in the remap address register (direct_access_rmp at offset 0x1c).
       */
      scml2::bitfield< unsigned int > rmp_addr_en;
      unsigned int rmp_addr_en_reset_value;
      /* This mask is used for masking bits [44:32] of the system address for read/write transfers for PROFILE 2. */
      scml2::bitfield< unsigned int > dac_addr_mask;
      unsigned int dac_addr_mask_reset_value;
    };
    struct direct_access_rmp_type : public scml2::reg< unsigned int > {
      ~direct_access_rmp_type();
      direct_access_rmp_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Remapping of incoming address on the AXI Slave Interface to a different address used by the Flash device.
       * Value of this register must be aligned to 8 bytes.
       */
      scml2::bitfield< unsigned int > rmp_addr_val;
      unsigned int rmp_addr_val_reset_value;
    };
    struct direct_access_rmp_1_type : public scml2::reg< unsigned int > {
      ~direct_access_rmp_1_type();
      direct_access_rmp_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Remapping of incoming address on Slave Data Interface to a different address used by the Flash device. */
      scml2::bitfield< unsigned int > rmp_addr_val_1;
      unsigned int rmp_addr_val_1_reset_value;
    };
    struct rst_seq_cfg_0_type : public scml2::reg< unsigned int > {
      ~rst_seq_cfg_0_type();
      rst_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command mnemonic value for CMD0 phase. */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd0_val;
      unsigned int rst_seq_p1_cmd0_val_reset_value;
      /* Command mnemonic value for CMD1 phase. */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd1_val;
      unsigned int rst_seq_p1_cmd1_val_reset_value;
      /* Enable bit for CMD0 phase. */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd0_en;
      unsigned int rst_seq_p1_cmd0_en_reset_value;
      /**
       * Number of lines used to send data phase
       * (Confirmation Byte In) followig CMD1 phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > rst_seq_p1_data_ios;
      unsigned int rst_seq_p1_data_ios_reset_value;
      /**
       * Selecting between SDR/DDR mode for
       * data phase (Confirmation Byte In) following CMD1 phase.
       */
      scml2::bitfield< unsigned int > rst_seq_p1_data_edge;
      unsigned int rst_seq_p1_data_edge_reset_value;
      /* Enable sending data phase (Confirmation Byte In) following CMD1 phase. */
      scml2::bitfield< unsigned int > rst_seq_p1_data_en;
      unsigned int rst_seq_p1_data_en_reset_value;
      /**
       * Number of data lines used to send commands for
       * both command phases.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd_ios;
      unsigned int rst_seq_p1_cmd_ios_reset_value;
      /**
       * Selecting between SDR/DDR mode for both
       * command phases.
       */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd_edge;
      unsigned int rst_seq_p1_cmd_edge_reset_value;
    };
    struct rst_seq_cfg_1_type : public scml2::reg< unsigned int > {
      ~rst_seq_cfg_1_type();
      rst_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command extension enable for CMD0 phase. */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd0_ext_en;
      unsigned int rst_seq_p1_cmd0_ext_en_reset_value;
      /* Command extension enable for CMD1 phase. */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd1_ext_en;
      unsigned int rst_seq_p1_cmd1_ext_en_reset_value;
      /* Command extension value of CMD0 phase (if enabled). */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd0_ext_val;
      unsigned int rst_seq_p1_cmd0_ext_val_reset_value;
      /* Command extension value of CMD1 phase (if enabled). */
      scml2::bitfield< unsigned int > rst_seq_p1_cmd1_ext_val;
      unsigned int rst_seq_p1_cmd1_ext_val_reset_value;
      /* Value of Confirmation Byte In (if enabled) following CMD1 phase. */
      scml2::bitfield< unsigned int > rst_seq_p1_data_val;
      unsigned int rst_seq_p1_data_val_reset_value;
    };
    struct ers_seq_cfg_0_type : public scml2::reg< unsigned int > {
      ~ers_seq_cfg_0_type();
      ers_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command mnemonic value. */
      scml2::bitfield< unsigned int > erss_seq_p1_cmd_val;
      unsigned int erss_seq_p1_cmd_val_reset_value;
      /**
       * Number of data lines used to send command phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > erss_seq_p1_cmd_ios;
      unsigned int erss_seq_p1_cmd_ios_reset_value;
      /* Selecting between SDR/DDR mode for command phase. */
      scml2::bitfield< unsigned int > erss_seq_p1_cmd_edge;
      unsigned int erss_seq_p1_cmd_edge_reset_value;
      /* Number of address bytes. */
      scml2::bitfield< unsigned int > erss_seq_p1_addr_cnt;
      unsigned int erss_seq_p1_addr_cnt_reset_value;
      /* Command extension enable. */
      scml2::bitfield< unsigned int > erss_seq_p1_cmd_ext_en;
      unsigned int erss_seq_p1_cmd_ext_en_reset_value;
      /* Command extension value if enabled. */
      scml2::bitfield< unsigned int > erss_seq_p1_cmd_ext_val;
      unsigned int erss_seq_p1_cmd_ext_val_reset_value;
      /**
       * Number of data lines used to send address phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > erss_seq_p1_addr_ios;
      unsigned int erss_seq_p1_addr_ios_reset_value;
      /* Selecting between SDR/DDR mode for address phase. */
      scml2::bitfield< unsigned int > erss_seq_p1_addr_edge;
      unsigned int erss_seq_p1_addr_edge_reset_value;
    };
    struct ers_seq_cfg_1_type : public scml2::reg< unsigned int > {
      ~ers_seq_cfg_1_type();
      ers_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Sector size. Value encoded as 2^erss_seq_p1_sect_size:
       * [list]
       * [*] 8'h00 - 1B,
       * [*] 8'h01 - 2B,
       * [*] 8'h02 - 4B,
       * [*] ...,
       * [*] 8'h0f - 32kB,
       * [*] 8'h10 - 64kB,
       * [*] ...,
       * [*] 8'h1f - (2^31)B.
       * [/list]
       */
      scml2::bitfield< unsigned int > erss_seq_p1_sect_size;
      unsigned int erss_seq_p1_sect_size_reset_value;
    };
    struct ers_seq_cfg_2_type : public scml2::reg< unsigned int > {
      ~ers_seq_cfg_2_type();
      ers_seq_cfg_2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command mnemonic value. */
      scml2::bitfield< unsigned int > ersa_seq_p1_cmd_val;
      unsigned int ersa_seq_p1_cmd_val_reset_value;
      /**
       * Number of data lines used to send command phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > ersa_seq_p1_cmd_ios;
      unsigned int ersa_seq_p1_cmd_ios_reset_value;
      /* Selecting between SDR/DDR mode for command phase. */
      scml2::bitfield< unsigned int > ersa_seq_p1_cmd_edge;
      unsigned int ersa_seq_p1_cmd_edge_reset_value;
      /* Command extension enable. */
      scml2::bitfield< unsigned int > ersa_seq_p1_cmd_ext_en;
      unsigned int ersa_seq_p1_cmd_ext_en_reset_value;
      /* Command extension value. */
      scml2::bitfield< unsigned int > ersa_seq_p1_cmd_ext_val;
      unsigned int ersa_seq_p1_cmd_ext_val_reset_value;
    };
    struct prog_seq_cfg_0_type : public scml2::reg< unsigned int > {
      ~prog_seq_cfg_0_type();
      prog_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command mnemonic value. */
      scml2::bitfield< unsigned int > prog_seq_p1_cmd_val;
      unsigned int prog_seq_p1_cmd_val_reset_value;
      /**
       * Number of data lines used to send command phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > prog_seq_p1_cmd_ios;
      unsigned int prog_seq_p1_cmd_ios_reset_value;
      /* Selecting between SDR/DDR mode for command phase. */
      scml2::bitfield< unsigned int > prog_seq_p1_cmd_edge;
      unsigned int prog_seq_p1_cmd_edge_reset_value;
      /* Number of address bytes. */
      scml2::bitfield< unsigned int > prog_seq_p1_addr_cnt;
      unsigned int prog_seq_p1_addr_cnt_reset_value;
      /**
       * Number of data lines used to send address phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > prog_seq_p1_addr_ios;
      unsigned int prog_seq_p1_addr_ios_reset_value;
      /* Selecting between SDR/DDR mode for address phase. */
      scml2::bitfield< unsigned int > prog_seq_p1_addr_edge;
      unsigned int prog_seq_p1_addr_edge_reset_value;
      /**
       * Number of data lines used to send data phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > prog_seq_p1_data_ios;
      unsigned int prog_seq_p1_data_ios_reset_value;
      /* Selecting between SDR/DDR mode for data phase. */
      scml2::bitfield< unsigned int > prog_seq_p1_data_edge;
      unsigned int prog_seq_p1_data_edge_reset_value;
      /* Number of dummy cycles in PROFILE 1. If 0 - dummy cycles disabled. */
      scml2::bitfield< unsigned int > prog_seq_p1_dummy_cnt;
      unsigned int prog_seq_p1_dummy_cnt_reset_value;
    };
    struct prog_seq_cfg_1_type : public scml2::reg< unsigned int > {
      ~prog_seq_cfg_1_type();
      prog_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command extension enable. */
      scml2::bitfield< unsigned int > prog_seq_p1_cmd_ext_en;
      unsigned int prog_seq_p1_cmd_ext_en_reset_value;
      /* Command extension value. */
      scml2::bitfield< unsigned int > prog_seq_p1_cmd_ext_val;
      unsigned int prog_seq_p1_cmd_ext_val_reset_value;
    };
    struct prog_seq_cfg_2_type : public scml2::reg< unsigned int > {
      ~prog_seq_cfg_2_type();
      prog_seq_cfg_2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Target space - corresponds to 46th Command/Address (CA) bit assignment.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Memory space,
       * [*] 1'b1 - Register space.
       * [/list]
       */
      scml2::bitfield< unsigned int > prog_seq_p2_target;
      unsigned int prog_seq_p2_target_reset_value;
      /**
       * Burst type - corresponds to 45th Command/Address (CA) bit assignment.
       * Although the user can set wrapped burst here, the controller itself does not currently support wrapping bursts and therefore should always have this set to '1'. values are:
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Wrapped burst,
       * [*] 1'b1 - Linear burst.
       * [/list]
       */
      scml2::bitfield< unsigned int > prog_seq_p2_burst_type;
      unsigned int prog_seq_p2_burst_type_reset_value;
      /**
       * Determines PROFILE 2 Command extension variant.
       * This influences bits [44:40]
       * of Command/Address. If this bit is set to 1
       * CA[44:40] will be set to all ones.
       * In DIRECT work mode if this bit is set to 0 bits [44:40]
       * of Command/Address will be
       * set to (sAWADDR[45:41] logically ANDed with dac_addr_mask[12:8]).
       * In ACMD work mode if this bit is set to 0 bits [44:40]
       * of Command/Address will be
       * set to to 0;
       */
      scml2::bitfield< unsigned int > prog_seq_p2_mask_cmd_mod;
      unsigned int prog_seq_p2_mask_cmd_mod_reset_value;
      /**
       * Number of latency cycles for PROFILE 2 - HR only.
       * Setting this bit to 0 will disable latency cycles.
       * This value should be set to 'N-1', where 'N' is the number
       * of latency clock cycles expected by the memory device.
       */
      scml2::bitfield< unsigned int > prog_seq_p2_latency_cnt;
      unsigned int prog_seq_p2_latency_cnt_reset_value;
    };
    struct read_seq_cfg_0_type : public scml2::reg< unsigned int > {
      ~read_seq_cfg_0_type();
      read_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command mnemonic value. */
      scml2::bitfield< unsigned int > read_seq_p1_cmd_val;
      unsigned int read_seq_p1_cmd_val_reset_value;
      /**
       * Number of data lines used to send command phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > read_seq_p1_cmd_ios;
      unsigned int read_seq_p1_cmd_ios_reset_value;
      /* Selecting between SDR/DDR mode for command phase. */
      scml2::bitfield< unsigned int > read_seq_p1_cmd_edge;
      unsigned int read_seq_p1_cmd_edge_reset_value;
      /* Number of address bytes. */
      scml2::bitfield< unsigned int > read_seq_p1_addr_cnt;
      unsigned int read_seq_p1_addr_cnt_reset_value;
      /**
       * Number of data lines used to send address phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > read_seq_p1_addr_ios;
      unsigned int read_seq_p1_addr_ios_reset_value;
      /* Selecting between SDR/DDR mode for address phase. */
      scml2::bitfield< unsigned int > read_seq_p1_addr_edge;
      unsigned int read_seq_p1_addr_edge_reset_value;
      /**
       * Number of data lines used to send data phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > read_seq_p1_data_ios;
      unsigned int read_seq_p1_data_ios_reset_value;
      /* Selecting between SDR/DDR mode for data phase. */
      scml2::bitfield< unsigned int > read_seq_p1_data_edge;
      unsigned int read_seq_p1_data_edge_reset_value;
      /**
       * Number of dummy cycles. If 0 - dummy cycles disabled.
       * This field is used when sending mode-bits is disabled.
       * Otherwise the read_seq_p1_mb_dummy_cnt should be used.
       */
      scml2::bitfield< unsigned int > read_seq_p1_dummy_cnt;
      unsigned int read_seq_p1_dummy_cnt_reset_value;
    };
    struct read_seq_cfg_1_type : public scml2::reg< unsigned int > {
      ~read_seq_cfg_1_type();
      read_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command extension enable. */
      scml2::bitfield< unsigned int > read_seq_p1_cmd_ext_en;
      unsigned int read_seq_p1_cmd_ext_en_reset_value;
      /**
       * This bit changes behavior of the Read sequence to utilize
       * the Read Page Cache Random/Read Page Cache Random Last commands.
       * This field is not used in DIRECT mode.
       */
      scml2::bitfield< unsigned int > read_seq_p1_cache_random_read_en;
      unsigned int read_seq_p1_cache_random_read_en_reset_value;
      /* Command extension value. */
      scml2::bitfield< unsigned int > read_seq_p1_cmd_ext_val;
      unsigned int read_seq_p1_cmd_ext_val_reset_value;
      /**
       * Number of dummy cycles. If 0 - dummy cycles are disabled.
       * This field is used when sending mode-bits is enabled.
       */
      scml2::bitfield< unsigned int > read_seq_p1_mb_dummy_cnt;
      unsigned int read_seq_p1_mb_dummy_cnt_reset_value;
      /* Set to 1'b1 to ensure the mode bits as defined in the xip_dis_mb_val field are sent following the address bytes. */
      scml2::bitfield< unsigned int > read_seq_p1_mb_en;
      unsigned int read_seq_p1_mb_en_reset_value;
    };
    struct read_seq_cfg_2_type : public scml2::reg< unsigned int > {
      ~read_seq_cfg_2_type();
      read_seq_cfg_2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Target space - corresponds to 46th Command/Address (CA) bit assignment.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Memory space,
       * [*] 1'b1 - Register space.
       * [/list]
       */
      scml2::bitfield< unsigned int > read_seq_p2_target;
      unsigned int read_seq_p2_target_reset_value;
      /**
       * Burst type - corresponds to 45th Command/Address (CA) bit assignment.
       * Although the user can set wrapped burst here, the controller itself does not currently support wrapping bursts and therefore should always have this set to '1'. values are:
       * [list]
       * [*] 1'b0 - Wrapped burst,
       * [*] 1'b1 - Linear burst.
       * [/list]
       */
      scml2::bitfield< unsigned int > read_seq_p2_burst_type;
      unsigned int read_seq_p2_burst_type_reset_value;
      /**
       * Determines PROFILE 2 Command extension variant.
       * This bit influences bits [44:40] 
       * of Command/Address. If this bit is set to 1
       * bits [44:40] of Command/Address will be set to 1.
       * In DIRECT work mode if this bit is set to 0 bits [44:40] of Command/Address will be
       * set to (sARADDR[45:41] logically ANDed with dac_addr_mask[12:8]).
       * In ACMD work mode if this bit is set to 0 bits [44:40] of Command/Address will be
       * set to to 0;
       */
      scml2::bitfield< unsigned int > read_seq_p2_mask_cmd_mod;
      unsigned int read_seq_p2_mask_cmd_mod_reset_value;
      /**
       * This field is used by the controller to calculate read transaction crossing page boundary.
       * It is valid only when PROFILE 2 - HF is selected.
       * Allowed values are:
       * [list]
       * [*] 1'b0 - Disable,
       * [*] 1'b1 - Enable.
       * [/list]
       */
      scml2::bitfield< unsigned int > read_seq_p2_hf_bound_en;
      unsigned int read_seq_p2_hf_bound_en_reset_value;
      /**
       * Number of latency cycles. Setting this bit to 0 disables latency clock cycles.
       * This value should be set to 'N-1', where 'N' is the number
       * of latency clock cycles expected by the memory device.
       */
      scml2::bitfield< unsigned int > read_seq_p2_latency_cnt;
      unsigned int read_seq_p2_latency_cnt_reset_value;
    };
    struct we_seq_cfg_0_type : public scml2::reg< unsigned int > {
      ~we_seq_cfg_0_type();
      we_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Command mnemonic value. */
      scml2::bitfield< unsigned int > we_seq_p1_cmd_val;
      unsigned int we_seq_p1_cmd_val_reset_value;
      /**
       * Number of data lines used to send command phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > we_seq_p1_cmd_ios;
      unsigned int we_seq_p1_cmd_ios_reset_value;
      /* Selecting between SDR/DDR mode for command phase. */
      scml2::bitfield< unsigned int > we_seq_p1_cmd_edge;
      unsigned int we_seq_p1_cmd_edge_reset_value;
      /* Command extension enable. */
      scml2::bitfield< unsigned int > we_seq_p1_cmd_ext_en;
      unsigned int we_seq_p1_cmd_ext_en_reset_value;
      /* Command extension value. */
      scml2::bitfield< unsigned int > we_seq_p1_cmd_ext_val;
      unsigned int we_seq_p1_cmd_ext_val_reset_value;
      /* Enables sending WEL command. */
      scml2::bitfield< unsigned int > we_seq_p1_en;
      unsigned int we_seq_p1_en_reset_value;
    };
    struct stat_seq_cfg_0_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_0_type();
      stat_seq_cfg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Number of data lines used to send command.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > stat_seq_p1_cmd_ios;
      unsigned int stat_seq_p1_cmd_ios_reset_value;
      /* Selecting between SDR/DDR mode for command phase. */
      scml2::bitfield< unsigned int > stat_seq_p1_cmd_edge;
      unsigned int stat_seq_p1_cmd_edge_reset_value;
      /* Command extension enable. */
      scml2::bitfield< unsigned int > stat_seq_p1_cmd_ext_en;
      unsigned int stat_seq_p1_cmd_ext_en_reset_value;
      /**
       * Number of address bytes for all status sequences.
       * Field encoding is as following:
       * [list]
       * [*] 2'b00 - One address byte,
       * [*] 2'b01 - Two address bytes,
       * [*] 2'b10 - Three address bytes,
       * [*] 2'b11 - Four address bytes.
       * [/list]
       */
      scml2::bitfield< unsigned int > stat_seq_p1_addr_cnt;
      unsigned int stat_seq_p1_addr_cnt_reset_value;
      /**
       * Number of data lines used to send address phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > stat_seq_p1_addr_ios;
      unsigned int stat_seq_p1_addr_ios_reset_value;
      /* Selecting between SDR/DDR mode for address phase. */
      scml2::bitfield< unsigned int > stat_seq_p1_addr_edge;
      unsigned int stat_seq_p1_addr_edge_reset_value;
      /**
       * Number of data lines used to send data phase.
       * [list]
       * [*] 2'b00 - One data line used (i.e. serial).
       * [*] 2'b01 - Two data lines used used.
       * [*] 2'b10 - Four data lines used used.
       * [*] 2'b11 - Eight data lines used used.
       * [/list]
       */
      scml2::bitfield< unsigned int > stat_seq_p1_data_ios;
      unsigned int stat_seq_p1_data_ios_reset_value;
      /* Selecting between SDR/DDR mode for data phase. */
      scml2::bitfield< unsigned int > stat_seq_p1_data_edge;
      unsigned int stat_seq_p1_data_edge_reset_value;
    };
    struct stat_seq_cfg_1_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_1_type();
      stat_seq_cfg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * This register defines the number of dummy clock cycles needed to check ready/busy status
       * after PROGRAM/ERASE or SOFT RESET/READ (only for SPI NAND) operation.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_dev_rdy_dummy_cnt;
      unsigned int stat_seq_p1_dev_rdy_dummy_cnt_reset_value;
      /**
       * Enables address phase for checking ready/busy status
       * after PROGRAM/ERASE and SOFT RESET/READ (only for SPI NAND) operation.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_dev_rdy_addr_en;
      unsigned int stat_seq_p1_dev_rdy_addr_en_reset_value;
      /**
       * Number of dummy clock cycles used to check fail status
       * after PROGRAM operation.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_prog_fail_dummy_cnt;
      unsigned int stat_seq_p1_prog_fail_dummy_cnt_reset_value;
      /**
       * Enables address phase for checking fail status
       * after PROGRAM operation.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_prog_fail_addr_en;
      unsigned int stat_seq_p1_prog_fail_addr_en_reset_value;
      /**
       * Number of dummy clock cycles used to check fail status
       * after ERASE operation.
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_ers_fail_dummy_cnt;
      unsigned int stat_seq_p1_ers_fail_dummy_cnt_reset_value;
      /**
       * Enables address phase for checking fail status
       * after ERASE operation.
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_ers_fail_addr_en;
      unsigned int stat_seq_p1_ers_fail_addr_en_reset_value;
    };
    struct stat_seq_cfg_2_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_2_type();
      stat_seq_cfg_2_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Command mnemonic value for command used to
       * check ready/busy status
       * after PROGRAM/ERASE and SOFT RESET/READ (only for SPI NAND) operation.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_dev_rdy_cmd_val;
      unsigned int stat_seq_p1_dev_rdy_cmd_val_reset_value;
      /**
       * Command mnemonic value for command used to check fail status
       * after ERASE operation.
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_ers_fail_cmd_val;
      unsigned int stat_seq_p1_ers_fail_cmd_val_reset_value;
      /**
       * Command mnemonic value for command used to check fail status
       * after PROGRAM operation.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_prog_fail_cmd_val;
      unsigned int stat_seq_p1_prog_fail_cmd_val_reset_value;
    };
    struct stat_seq_cfg_3_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_3_type();
      stat_seq_cfg_3_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Command extension value used to check ready/busy status
       * after PROGRAM/ERASE and SOFT RESET/READ (only for SPI NAND) operation.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_dev_rdy_cmd_ext_val;
      unsigned int stat_seq_p1_dev_rdy_cmd_ext_val_reset_value;
      /**
       * Command extension value used to check fail status
       * after ERASE operation.
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_ers_fail_cmd_ext_val;
      unsigned int stat_seq_p1_ers_fail_cmd_ext_val_reset_value;
      /**
       * Command extension value used to check fail status
       * after PROGRAM operation.
       */
      scml2::bitfield< unsigned int > stat_seq_p1_prog_fail_cmd_ext_val;
      unsigned int stat_seq_p1_prog_fail_cmd_ext_val_reset_value;
    };
    struct stat_seq_cfg_4_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_4_type();
      stat_seq_cfg_4_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Determines PROFILE 2 Command extension variant.
       * Value of this bits influences the [44:40] bits
       * of Command/Address. If this bit is set to 1
       * bits [44:40] of Command/Address will be set to 1. Otherwise bits [44:40] of Command/Address will be set to 0.
       */
      scml2::bitfield< unsigned int > stat_seq_p2_mask_cmd_mod;
      unsigned int stat_seq_p2_mask_cmd_mod_reset_value;
      /**
       * Number of latency cycles between CA and STATUS reading.
       * This value should be set to 'N-1', where 'N' is the number
       * of latency clock cycles expected by the memory device.
       */
      scml2::bitfield< unsigned int > stat_seq_p2_latency_cnt;
      unsigned int stat_seq_p2_latency_cnt_reset_value;
    };
    struct stat_seq_cfg_5_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_5_type();
      stat_seq_cfg_5_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * This field determines which bit of the status word contains
       * the ready/busy information which will be polled after
       * a PROGRAM/ERASE or SOFT RESET/READ (only for SPI NAND) operation.
       */
      scml2::bitfield< unsigned int > stat_seq_dev_rdy_idx;
      unsigned int stat_seq_dev_rdy_idx_reset_value;
      /**
       * Value which will be compared with selected status bit
       * in order to detect that the device is ready
       * after a PROGRAM/ERASE or SOFT RESET/READ (only for SPI NAND) operation.
       */
      scml2::bitfield< unsigned int > stat_seq_dev_rdy_val;
      unsigned int stat_seq_dev_rdy_val_reset_value;
      /* Size of status word (0-1B, 1-2B). */
      scml2::bitfield< unsigned int > stat_seq_dev_rdy_size;
      unsigned int stat_seq_dev_rdy_size_reset_value;
      /**
       * Enables checking RDY/BUSY status
       * after PROGRAM/ERASE operations.
       */
      scml2::bitfield< unsigned int > stat_seq_dev_rdy_en;
      unsigned int stat_seq_dev_rdy_en_reset_value;
      /**
       * This field determines which bit of the status word contains
       * the fail information after the ERASE command.
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_ers_fail_idx;
      unsigned int stat_seq_ers_fail_idx_reset_value;
      /**
       * Value which will be compared with selected status bit
       * in order to detect that the device is in the ready state after
       * ERASE operation.
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_ers_fail_val;
      unsigned int stat_seq_ers_fail_val_reset_value;
      /**
       * Size of status word (0-1B, 1-2B).
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_ers_fail_size;
      unsigned int stat_seq_ers_fail_size_reset_value;
      /**
       * Enables checking fail status
       * after an ERASE operation.
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_ers_fail_en;
      unsigned int stat_seq_ers_fail_en_reset_value;
      /**
       * This field determines which bit of the status word contains
       * the fail information after the PROGRAM command.
       */
      scml2::bitfield< unsigned int > stat_seq_prog_fail_idx;
      unsigned int stat_seq_prog_fail_idx_reset_value;
      /**
       * Value which will be compared with selected status bit
       * in order to detect that the device is in fail state after
       * PROGRAM operation.
       */
      scml2::bitfield< unsigned int > stat_seq_prog_fail_val;
      unsigned int stat_seq_prog_fail_val_reset_value;
      /* Size of status word (0-1B, 1-2B). */
      scml2::bitfield< unsigned int > stat_seq_prog_fail_size;
      unsigned int stat_seq_prog_fail_size_reset_value;
      /**
       * Enables checking of the fail status
       * after PROGRAM operation.
       */
      scml2::bitfield< unsigned int > stat_seq_prog_fail_en;
      unsigned int stat_seq_prog_fail_en_reset_value;
    };
    struct stat_seq_cfg_7_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_7_type();
      stat_seq_cfg_7_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Value of address used to check rdy/busy status
       * after PROGRAM/ERASE and SOFT RESET/READ (only for SPI NAND) operation.
       */
      scml2::bitfield< unsigned int > stat_seq_dev_rdy_addr;
      unsigned int stat_seq_dev_rdy_addr_reset_value;
    };
    struct stat_seq_cfg_8_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_8_type();
      stat_seq_cfg_8_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Value of address used to check fail status
       * after PROGRAM operation.
       */
      scml2::bitfield< unsigned int > stat_seq_prog_fail_addr;
      unsigned int stat_seq_prog_fail_addr_reset_value;
    };
    struct stat_seq_cfg_9_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_9_type();
      stat_seq_cfg_9_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Value of address used to check fail status
       * after ERASE operation.
       */
      scml2::bitfield< unsigned int > stat_seq_ers_fail_addr;
      unsigned int stat_seq_ers_fail_addr_reset_value;
    };
    struct stat_seq_cfg_10_type : public scml2::reg< unsigned int > {
      ~stat_seq_cfg_10_type();
      stat_seq_cfg_10_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Mask used to select which bits of status word carries
       * the ECC status.
       */
      scml2::bitfield< unsigned int > stat_seq_ecc_fail_mask;
      unsigned int stat_seq_ecc_fail_mask_reset_value;
      /**
       * Value which will be compared with status word
       * masked by the stat_seq_ecc_fail_mask field
       * in order to detect if the device returned an
       * uncorrectable ECC error
       * during SPI NAND Page Read operation.
       */
      scml2::bitfield< unsigned int > stat_seq_ecc_fail_val;
      unsigned int stat_seq_ecc_fail_val_reset_value;
      /**
       * Value which will be compared with status word
       * masked by the stat_seq_ecc_fail_mask field
       * in order to detect if the device returned a
       * correctable ECC error
       * during SPI NAND Page Read operation.
       * This can be used to detect a single range of correctable
       * errors returned by the XSPI device.
       */
      scml2::bitfield< unsigned int > stat_seq_ecc_corr_val;
      unsigned int stat_seq_ecc_corr_val_reset_value;
      /**
       * This field determines which bit of the status word contains
       * the Cache Read Busy (CRBSY) bit information for the SPI NAND
       * Read Page Cache Random operation.
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_crdy_idx;
      unsigned int stat_seq_crdy_idx_reset_value;
      /**
       * Value which will be compared with selected status bit
       * in order to detect is the device in ready state after
       * the Read Page Cache Random operation (CRBSY bit).
       * This field is not used in DIRECT work mode.
       */
      scml2::bitfield< unsigned int > stat_seq_crdy_val;
      unsigned int stat_seq_crdy_val_reset_value;
      /**
       * Enables checking ECC status
       * after READ PAGE operation.
       */
      scml2::bitfield< unsigned int > stat_seq_ecc_fail_en;
      unsigned int stat_seq_ecc_fail_en_reset_value;
    };
    struct xspi_ctrl_version_type : public scml2::reg< unsigned int > {
      ~xspi_ctrl_version_type();
      xspi_ctrl_version_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Controller revision number. */
      scml2::bitfield< unsigned int > xspi_ctrl_rev;
      unsigned int xspi_ctrl_rev_reset_value;
      /* Fixed number (minor revision number). */
      scml2::bitfield< unsigned int > xspi_ctrl_fix;
      unsigned int xspi_ctrl_fix_reset_value;
      /**
       * Controller's 'Magic Number'.
       * It is a unique number characteristic to the
       * Cadence's xSPI Controller.
       */
      scml2::bitfield< unsigned int > xspi_ctrl_magic_number;
      unsigned int xspi_ctrl_magic_number_reset_value;
    };
    struct ctrl_features_reg_type : public scml2::reg< unsigned int > {
      ~ctrl_features_reg_type();
      ctrl_features_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Number of threads available in the controller. The following decoding is used:[list]
       * [*]0 - One thread
       * [*]1 - Two threads
       * [*]2 - Four threads
       * [*]3 - Eight threads
       * [*]4-15 - Reserved[/list]
       */
      scml2::bitfield< unsigned int > n_threads;
      unsigned int n_threads_reset_value;
      /* ASF features present. */
      scml2::bitfield< unsigned int > asf_available;
      unsigned int asf_available_reset_value;
      /* Boot feature present. */
      scml2::bitfield< unsigned int > boot_available;
      unsigned int boot_available_reset_value;
      /* DMA interface type (0-AXI4 other values reserved). */
      scml2::bitfield< unsigned int > dma_intf;
      unsigned int dma_intf_reset_value;
      /**
       * Slave
       * and Master
       * DMA address width:
       * [list]
       * [*]0 - 32bit,
       * [*]1 - 64bit.
       * [/list]
       */
      scml2::bitfield< unsigned int > dma_addr_width;
      unsigned int dma_addr_width_reset_value;
      /**
       * Slave
       * and Master
       * DMA data width:
       * [list]
       * [*]0 - 32bit,
       * [*]1 - 64bit.
       * [/list]
       */
      scml2::bitfield< unsigned int > dma_data_width;
      unsigned int dma_data_width_reset_value;
      /* SFR interface type  1-APB, other values reserved. */
      scml2::bitfield< unsigned int > sfr_intf;
      unsigned int sfr_intf_reset_value;
      /**
       * Maximum number of banks supported by hardware. This is an encoded value.[list]
       * [*]0 - One bank,
       * [*]1 - Two banks,
       * [*]2 - Four banks,
       * [*]3 - Eight banks.[/list]
       */
      scml2::bitfield< unsigned int > n_banks;
      unsigned int n_banks_reset_value;
    };
    struct wp_settings_type : public scml2::reg< unsigned int > {
      ~wp_settings_type();
      wp_settings_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Write protect signal for all devices. Value of this register is directly
       * routed to the DQ2 output signal. The value can be changed only when xSPI Flash interface is in
       * an idle state. Value of the write protection signal
       * is overwritten in case DQ2 is a valid transaction pin. The controller does not
       * check the write protect setup/hold timings - this must be ensured by the host.
       * Controller does not drive Write Protect value during read data phase of any active transfer.
       * Write Protect on DQ2 functionality is only supported by Flash Devices and if the controller is configured in single
       * and dual SPI Modes.
       */
      scml2::bitfield< unsigned int > wp;
      unsigned int wp_reset_value;
      /**
       * Enables passing Write protect signal to the device (by switching direction
       * of DQ2 pad).
       */
      scml2::bitfield< unsigned int > wp_enable;
      unsigned int wp_enable_reset_value;
    };
    struct reset_pin_settings_type : public scml2::reg< unsigned int > {
      ~reset_pin_settings_type();
      reset_pin_settings_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Software Controlled Hardware Reset Signal. Value of this field is
       * directly routed to the DQ3 or RESET# output signal (depending on sw_ctrled_hw_rst_option).
       * The value can be changed only when xSPI Flash interface is in an idle state. Value of the Software Controlled Hardware Reset Signal
       * is overwritten in case DQ3 is valid transaction pin. The controller does
       * not check the device hardware reset setup/hold timings - this must be ensured by the host.
       * The host is also responsible for triggering a suitable RESET method by selecting corresponding
       * Bank Number and RESET method (as defined in this register).
       * The controller does not drive RESET# value during read data phase of any active transfer
       * (as transfer direction switches in this transfer part).
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst;
      unsigned int sw_ctrled_hw_rst_reset_value;
      /* Enables passing RESET to the DQ3 port of the device (by switching direction of DQ3 pad). */
      scml2::bitfield< unsigned int > rst_dq3_enable;
      unsigned int rst_dq3_enable_reset_value;
      /**
       * Defines Hardware RESET options as follows:[list]
       * [*]0 - Device RESET# pin will be used for toggling Device Hardware Reset functionality.
       * [*]1 - Device DQ3 pin will be used for toggling Device Hardware Reset functionality.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_option;
      unsigned int sw_ctrled_hw_rst_option_reset_value;
      /**
       * Activates software controlled hardware reset signal on bank 0 (i.e. device connected to chip select bit 0).
       * Applicable only for devices that support #RESET pin :[list]
       *   [*]0 - CS[0] device is disabled for Software Controlled Hardware Reset trigger.
       *   [*]1 - CS[0] device is enabled for Software Controlled Hardware Reset trigger.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_bank0;
      unsigned int sw_ctrled_hw_rst_bank0_reset_value;
      /**
       * Activates software controlled hardware reset signal on bank 1 (i.e. device connected to chip select bit 1).
       * Applicable only for devices that support #RESET pin :[list]
       *   [*]0 - CS[1] device is disabled for Software Controlled Hardware Reset trigger.
       *   [*]1 - CS[1] device is enabled for Software Controlled Hardware Reset trigger.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_bank1;
      unsigned int sw_ctrled_hw_rst_bank1_reset_value;
      /**
       * Activates software controlled hardware reset signal on bank 2 (i.e. device connected to chip select bit 2).
       * Applicable only for devices that support #RESET pin :[list]
       *   [*]0 - CS[2] device is disabled for Software Controlled Hardware Reset trigger.
       *   [*]1 - CS[2] device is enabled for Software Controlled Hardware Reset trigger.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_bank2;
      unsigned int sw_ctrled_hw_rst_bank2_reset_value;
      /**
       * Activates software controlled hardware reset signal on bank 3 (i.e. device connected to chip select bit 3).
       * Applicable only for devices that support #RESET pin :[list]
       *   [*]0 - CS[3] device is disabled for Software Controlled Hardware Reset trigger.
       *   [*]1 - CS[3] device is enabled for Software Controlled Hardware Reset trigger.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_bank3;
      unsigned int sw_ctrled_hw_rst_bank3_reset_value;
      /**
       * Activates software controlled hardware reset signal on bank 4 (i.e. device connected to chip select bit 4).
       * Applicable only for devices that support #RESET pin :[list]
       *   [*]0 - CS[4] device is disabled for Software Controlled Hardware Reset trigger.
       *   [*]1 - CS[4] device is enabled for Software Controlled Hardware Reset trigger.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_bank4;
      unsigned int sw_ctrled_hw_rst_bank4_reset_value;
      /**
       * Activates software controlled hardware reset signal on bank 5 (i.e. device connected to chip select bit 5).
       * Applicable only for devices that support #RESET pin :[list]
       *   [*]0 - CS[5] device is disabled for Software Controlled Hardware Reset trigger.
       *   [*]1 - CS[5] device is enabled for Software Controlled Hardware Reset trigger.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_bank5;
      unsigned int sw_ctrled_hw_rst_bank5_reset_value;
      /**
       * Activates software controlled hardware reset signal on bank 6 (i.e. device connected to chip select bit 6).
       * Applicable only for devices that support #RESET pin :[list]
       *   [*]0 - CS[6] device is disabled for Software Controlled Hardware Reset trigger.
       *   [*]1 - CS[6] device is enabled for Software Controlled Hardware Reset trigger.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_bank6;
      unsigned int sw_ctrled_hw_rst_bank6_reset_value;
      /**
       * Activates software controlled hardware reset signal on bank 7 (i.e. device connected to chip select bit 7).
       * Applicable only for devices that support #RESET pin :[list]
       *   [*]0 - CS[7] device is disabled for Software Controlled Hardware Reset trigger.
       *   [*]1 - CS[7] device is enabled for Software Controlled Hardware Reset trigger.[/list]
       */
      scml2::bitfield< unsigned int > sw_ctrled_hw_rst_bank7;
      unsigned int sw_ctrled_hw_rst_bank7_reset_value;
    };
    struct clock_mode_settings_type : public scml2::reg< unsigned int > {
      ~clock_mode_settings_type();
      clock_mode_settings_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Defines SPI Clock Mode.
       * For DDR transfers this bit should always be set low to meet
       * DDR Flash timings.
       * For SDR transfers, allowable values are as follows:[list]
       *   [*]0 - SPI MODE 0 (clock is low when SPI bus is in idle),
       *   [*]1 - SPI MODE 3 (clock is high when SPI bus is in idle).[/list]
       */
      scml2::bitfield< unsigned int > spi_clock_mode;
      unsigned int spi_clock_mode_reset_value;
    };
    struct jedec_rst_timing_reg_type : public scml2::reg< unsigned int > {
      ~jedec_rst_timing_reg_type();
      jedec_rst_timing_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Defines how many xspi_clk cycles constitute tCSH timing of JEDEC Reset Instruction. */
      scml2::bitfield< unsigned int > tCSH_delay;
      unsigned int tCSH_delay_reset_value;
      /* Defines how many xspi_clk cycles constitute tCSL timing of JEDEC Reset Instruction. */
      scml2::bitfield< unsigned int > tCSL_delay;
      unsigned int tCSL_delay_reset_value;
    };
    struct dev_delay_reg_type : public scml2::reg< unsigned int > {
      ~dev_delay_reg_type();
      dev_delay_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * CSSOT - Chip Select Start Of Transfer.
       * It allows to improve CS de-assertion device timing to first active clock edge.
       */
      scml2::bitfield< unsigned int > cssot_delay;
      unsigned int cssot_delay_reset_value;
      /**
       * CSEOT - Chip Select End Of Transfer.
       * It allows to improve last active clock edge to CS de-assertion device timing.
       */
      scml2::bitfield< unsigned int > cseot_delay;
      unsigned int cseot_delay_reset_value;
      /* CSDA_MIN -Minimum Chip Select de-assertion timing. */
      scml2::bitfield< unsigned int > csda_min_delay;
      unsigned int csda_min_delay_reset_value;
    };
    struct rst_recovery_reg_type : public scml2::reg< unsigned int > {
      ~rst_recovery_reg_type();
      rst_recovery_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * This field defines additional delay for CS de-assertion to accommodate Device Reset Recovery
       * timing.
       */
      scml2::bitfield< unsigned int > rst_recovery;
      unsigned int rst_recovery_reset_value;
    };
    struct dev_active_max_reg_type : public scml2::reg< unsigned int > {
      ~dev_active_max_reg_type();
      dev_active_max_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * The value in this field is only valid if bit 18 of the global_seq_cfg register at offset 0x390 is set.  When 
       * using the STIG work mode, this is also only valid when the TCMS_EN bit of the STIG instruction is set.
       * This bit should also only be enabled only while working with RAM
       * devices that require timing constraint for Chip Select low
       * pulse width (the most common name is tCMS or tCEM in device
       * specification).
       */
      scml2::bitfield< unsigned int > dev_active_max;
      unsigned int dev_active_max_reset_value;
    };
    struct hf_offset_reg_type : public scml2::reg< unsigned int > {
      ~hf_offset_reg_type();
      hf_offset_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Starting index of reserved area in command format. */
      scml2::bitfield< unsigned int > hf_offset_index;
      unsigned int hf_offset_index_reset_value;
      /* Offset size of reserved area in command format. */
      scml2::bitfield< unsigned int > hf_offset_size;
      unsigned int hf_offset_size_reset_value;
    };
    struct dll_phy_update_cnt_type : public scml2::reg< unsigned int > {
      ~dll_phy_update_cnt_type();
      dll_phy_update_cnt_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * This field defines the time interval (in terms of
       * xspi_clk cycles) to send an update
       * (assert dfi_ctrlupd_req high) to the PHY to re-synchronize
       * the slave DLL values with that of the master DLL and to
       * also re-synchronize the read and write FIFO pointers in
       * the read path. If the value in this field is zero, the
       * controller will not further DLL update requests to the
       * PHY. dfi_ctrlupd_req signal can be controlled directly by
       * the host using the dfi_ctrlupd_req field in the
       * dll_phy_ctrl register.
       * NOTE: While this feature is enabled access to the PHY
       * registers shall not be performed.
       */
      scml2::bitfield< unsigned int > resync_cnt;
      unsigned int resync_cnt_reset_value;
    };
    struct dll_phy_ctrl_type : public scml2::reg< unsigned int > {
      ~dll_phy_ctrl_type();
      dll_phy_ctrl_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * This field defines the wait time (in terms of
       * xspi_clk cycles) between the
       * de-assertion of the DLL update request (dfi_ctrlupd_req)
       * and resuming traffic to the PHY.
       */
      scml2::bitfield< unsigned int > resync_idle_cnt;
      unsigned int resync_idle_cnt_reset_value;
      /* This field defines the number of xspi_clk cycles for which the DLL update request (dfi_ctrlupd_req) has to be asserted to resynchronize the DLLs and read and write FIFO pointers. */
      scml2::bitfield< unsigned int > resync_high_wait_cnt;
      unsigned int resync_high_wait_cnt_reset_value;
      /* This PHY register field is not applicable for xSPI Flash Controller. */
      scml2::bitfield< unsigned int > extended_rd_mode;
      unsigned int extended_rd_mode_reset_value;
      /* This PHY register field is not applicable for xSPI Flash Controller. */
      scml2::bitfield< unsigned int > extended_wr_mode;
      unsigned int extended_wr_mode_reset_value;
      /**
       * This bit should be set when the
       * Flash Device being used issues data on negative edge of
       * Flash clock and returns them with DQS and
       * the PHY is configured to sample data in DQS Mode.
       * In this case, number of DQS edges equals to number of
       * requested data + 1. If this bit is set, the controller
       * internally requests this redundant data at the end of the transfer
       * cleaning up the PHY fifo.
       */
      scml2::bitfield< unsigned int > dqs_last_data_drop_en;
      unsigned int dqs_last_data_drop_en_reset_value;
      /**
       * The PHY samples data on both edges of sampling clock.
       * In SDR Mode, only one sample is needed.
       * If this bit is low, the controller propagates data from positive edge of PHY sampling clock.
       * If this bit is high, the controller propagates data from negative edge of PHY sampling clock.
       * In DDR Mode, this bit should be set low.
       */
      scml2::bitfield< unsigned int > sdr_edge_active;
      unsigned int sdr_edge_active_reset_value;
      /* Signal to reset the DLLs of the PHY and start searching for lock again. */
      scml2::bitfield< unsigned int > dll_rst_n;
      unsigned int dll_rst_n_reset_value;
      /* Signal to re-synchronize the DLLs and read and write FIFO pointers. To send the update request to the PHY, the host must first set this field high then wait until this bit will be set low. This signal should not be used when automatic resync is enabled that is:. 'dll_phy_update_cnt' is not zero. */
      scml2::bitfield< unsigned int > dfi_ctrlupd_req;
      unsigned int dfi_ctrlupd_req_reset_value;
    };
    struct phy_dq_timing_reg_type : public scml2::reg< unsigned int > {
      ~phy_dq_timing_reg_type();
      phy_dq_timing_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Adjusts the ending point of the DQ pad output enable window. Lower numbers pull the falling edge earlier in time and larger numbers
       * cause the falling edge to be delayed. Each bit changes the output enable time by a 1/2 cycle resolution.
       */
      scml2::bitfield< unsigned int > data_select_oe_end;
      unsigned int data_select_oe_end_reset_value;
      /* Adjusts the starting point of the DQ pad output enable window. Lower numbers pull the rising edge earlier in time and larger numbers cause the rising edge to be delayed. Each bit changes the output enable time by a 1/2 cycle resolution. */
      scml2::bitfield< unsigned int > data_select_oe_start;
      unsigned int data_select_oe_start_reset_value;
      /* Defines the DQ pad dynamic termination select disable time. Larger values increase the delay to when tsel turns off. Each bit changes the output enable time by a 1/2 cycle resolution. */
      scml2::bitfield< unsigned int > data_select_tsel_end;
      unsigned int data_select_tsel_end_reset_value;
      /* Defines the DQ pad dynamic termination select enable time. Larger values add greater delay to when tsel turns on. Each bit changes the output enable time by a 1/2 cycle resolution. */
      scml2::bitfield< unsigned int > data_select_tsel_start;
      unsigned int data_select_tsel_start_reset_value;
      /* Defines additional latency on the write datapath. It also adds a clock cycle delay for the data OE path which is equivalent of adding 2 to the data_select_oe_end and data_select_oe_start. */
      scml2::bitfield< unsigned int > data_clkperiod_delay;
      unsigned int data_clkperiod_delay_reset_value;
    };
    struct phy_dqs_timing_reg_type : public scml2::reg< unsigned int > {
      ~phy_dqs_timing_reg_type();
      phy_dqs_timing_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Defines the DQ pad dynamic termination select disable time. Larger values increase the delay to when tsel turns off. Each bit changes the output enable time by a 1/2 cycle resolution. */
      scml2::bitfield< unsigned int > dqs_select_tsel_end;
      unsigned int dqs_select_tsel_end_reset_value;
      /* Defines the DQ pad dynamic termination select enable time. Larger values add greater delay to when tsel turns on. Each bit changes the output enable time by a 1/2 cycle resolution. */
      scml2::bitfield< unsigned int > dqs_select_tsel_start;
      unsigned int dqs_select_tsel_start_reset_value;
      /* If this bit is cleared the phony_dqs is synchronous with rising edge of the clk_phy before sending to the entry flops. If this bit is set high the phony_dqs is synchronous with falling edge of clk_phy before sending to the entry flops. */
      scml2::bitfield< unsigned int > phony_dqs_sel;
      unsigned int phony_dqs_sel_reset_value;
      /**
       * This bit is used in conjunction with bits 22 and 21 to control how read data is sampled by the PHY.
       * This bit selects whether the read data sent by the memory device will be sampled by DQS supplied by the memory device, or by a signal locally generated within the PHY.[list] [*]0 - Use DQS from device for data capture.[*]1 - Use internally generated DQS for data capture.[/list]
       */
      scml2::bitfield< unsigned int > use_phony_dqs;
      unsigned int use_phony_dqs_reset_value;
      /**
       * This bit is used in conjunction with bits 22 and 20 to control how read data is sampled by the PHY.
       *                     This bit is only valid when bit 20 is set to '1', meaning it is only relevant when read data is not being sampled by DQS from the memory device. 
       *                     If bit 20 is set to '0' this bit should also be set to '0'.
       *                     When using the PHY with the xSPI controller, this bit must be set to the same value as bit 20.
       *                     This bit selects which internal source will be used by the PHY to sample the read data. This is internally generated and is passed out of the PHY via rebar_opad
       *                     If bit 22 of this register is '0', then it will be internally looped back within the rebar pad. If bit 22 of this register is '1', the rebar_opad will be passed
       *                     through the rebar pad and it will be the responsibility of the integrator to loop that back to the PHY into the lpbk_dqs pin. Refer to section 2 'Read Sampling lpbk_dqs'.
       *                     . [list] [*]0 - Use phony DQS for data capture.[*]1 - Use lpbk_dqs for data capture. Mandatory setting for SD/eMMC.
       * [/list]
       */
      scml2::bitfield< unsigned int > use_lpbk_dqs;
      unsigned int use_lpbk_dqs_reset_value;
      /**
       * This bit is used in conjunction with bits 21 and 20 to control how read data is sampled by the PHY
       * This bit is only valid when bit 20 = '1' and bit 21 = '1' and selects if the internally generated clock source should be 
       * looped back within the rebar pad (The PHY will then sample the read data using mem_rebar_ipad), or if the integrator should take the
       * signal from the rebar pin, loop it back on the board back to the PHY using the lpbk_dqs pin.  The PHY will then sample the read data using lpbk_dqs
       * [list] [*]0 - use internal lpbk_dqs (mem_rebar_ipad) for data capture.[*]1 -  use external lpbk_dqs (lpbk_dqs connected to the lpbk_dqs_IO PAD) for data capture. Mandatory setting for SD/eMMC[/list]
       */
      scml2::bitfield< unsigned int > use_ext_lpbk_dqs;
      unsigned int use_ext_lpbk_dqs_reset_value;
    };
    struct phy_gate_lpbk_ctrl_reg_type : public scml2::reg< unsigned int > {
      ~phy_gate_lpbk_ctrl_reg_type();
      phy_gate_lpbk_ctrl_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Coarse adjust of gate open time. This value is the number of cycles to delay the dfi_rddata_en signal prior to opening the gate in full cycle increments. Decreasing this value pulls the gate earlier in time. This field should be programmed such that the gate signal lands in the valid DQS gate window. */
      scml2::bitfield< unsigned int > gate_cfg;
      unsigned int gate_cfg_reset_value;
      /* Normally the gate is closing when all bits of dfi_cebar are high or when dfi_rd_pre_post_amble and rebar_dfi are high. This parameter allows to extend the closing of the DQS gate. Recommended value is zero. */
      scml2::bitfield< unsigned int > gate_cfg_close;
      unsigned int gate_cfg_close_reset_value;
      /* This parameter cause the gate to be always on. */
      scml2::bitfield< unsigned int > gate_cfg_always_on;
      unsigned int gate_cfg_always_on_reset_value;
      /* Controls internal write multiplexer. [list][*]0 = Normal Operation. [*]1 = Enable loopback.[/list] */
      scml2::bitfield< unsigned int > lpbk_en;
      unsigned int lpbk_en_reset_value;
      /* Controls the loopback read multiplexer. [list][*]0 = External Loopback. [*]1 = Internal loopback.[/list] */
      scml2::bitfield< unsigned int > lpbk_internal;
      unsigned int lpbk_internal_reset_value;
      /* Loopback control. [list][*]0 = Normal Operation Mode. [*]1 = lpbk_start; Enables loopback write mode. [*]2 = lpbk_stop; Stop loopback to check error register. [*]3 = clear; Clear loopback registers.[/list] */
      scml2::bitfield< unsigned int > loopback_control;
      unsigned int loopback_control_reset_value;
      /* Selects data output type for phy_obs_reg_0[23:8]. [list][*]0 = Return the expected data. [*]1 = Return the actual data.[/list] */
      scml2::bitfield< unsigned int > lpbk_fail_muxsel;
      unsigned int lpbk_fail_muxsel_reset_value;
      /* Sets the cycle delay between the LFSR and loopback error check logic to ensure that the LFSR sourced data and data being looped back arrive at the same clock cycle for comparison. This value is related to the rd_del_sel field, and is equal to 7 - rd_del_sel. */
      scml2::bitfield< unsigned int > lpbk_err_check_timing;
      unsigned int lpbk_err_check_timing_reset_value;
      /**
       * Defines the read data delay for the empty signal generated based on the incoming DQS strobes. For zero delay the data are passed from entry flops to the iodatain* flops one clock cycle after the !empty signals is asserted.
       * Normally the zero value of this field is sufficient as the signal is generated based on the gray pointer synchronized with two stage synchronizer on clk_phy clock domain which gives minimum two clock cycle path from entry flop to the iodatain flop.
       * Increasing the value of this field delays the moment of passing the data from entry flops to the iodatain flops. Increased value gives even more time to propagate the data but the bigger value the bigger probability to overflow the FIFO. Recommended value is zero.
       */
      scml2::bitfield< unsigned int > rd_del_sel_empty;
      unsigned int rd_del_sel_empty_reset_value;
      /**
       * This field turns off the generation of the underrun signal when 'sync_method' is set high. 
       * Recommended value is zero.
       */
      scml2::bitfield< unsigned int > underrun_suppress;
      unsigned int underrun_suppress_reset_value;
      /**
       * Defines the read data delay. Holds the number of cycles to delay the dfi_rddata_en signal prior to enabling the read FIFO. After this delay, the read pointers begin incrementing the read FIFO.
       * If 'sync_method' is set high the value of this field must take into account the synchronization time of the pointers in the entry FIFO (adding three clock cycles should be sufficient).
       */
      scml2::bitfield< unsigned int > rd_del_sel;
      unsigned int rd_del_sel_reset_value;
      /**
       * Defines the method of transfering the data from DQS domain flops to the clk_phy clock domain.
       * [list]
       *    [*]if set low the read pointer advances based upon a programmable delay of the dfi_rddata_en pulse from the DFI interface.
       *    [*]if set high the read pointer advances based upon a programmable delay of the empty signal.
       * Recommended setting for SD/eMMC controller.
       * [/list]
       */
      scml2::bitfield< unsigned int > sync_method;
      unsigned int sync_method_reset_value;
    };
    struct phy_dll_master_ctrl_reg_type : public scml2::reg< unsigned int > {
      ~phy_dll_master_ctrl_reg_type();
      phy_dll_master_ctrl_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* This value is the initial delay value for the DLL. This value is also used as the increment value if the initial value is less than a half-clock cycle. This field should be set such that it is not greater than 7/8ths of a clock period given the worst case element delay. For example, if the frequency is 200MHz (5ns cycle time) with a worst case element 80ps delay, this field should be set to = 5 * (7/8) / .080 = 54 elements. This calculation helps determine the start point which achieves the fastest lock. However, a small value such as 0x04 may be used instead to ensure that the DLL does not lock on a harmonic. Note that with a small value like this, the initial lock time will be longer. Value smaller than 0x04 may cause no lock by DLL. */
      scml2::bitfield< unsigned int > param_dll_start_point;
      unsigned int param_dll_start_point_reset_value;
      /* Holds the number of consecutive increment or decrement indications that will trigger an unlock condition and increment the dll_unlock_cnt field (bits [7:3]) and either the lock_dec_dbg (bits [23:16]) or lock_inc_dbg (bits [31:24]) fields of the phy_dll_obs_reg_0 parameter. */
      scml2::bitfield< unsigned int > param_dll_lock_num;
      unsigned int param_dll_lock_num_reset_value;
      /**
       * Selects the number of delay elements to be inserted between the phase detect flip-flops.
       * Defaults to 0x0 although the recommended value is 2 elements but if a lock condition is
       * not detected, the user should increase the number of delay elements.
       * [list]
       * [*]'b000 - One delay element.
       * [*]'b001 - Two delay element.
       * [*]'b010 - Three delay element.
       * [*]'b011 - Four delay element.
       * [*]'b100 - Five delay element.
       * [*]'b101 - Six delay element.
       * [*]'b110 - Seven delay element.
       * [*]'b111 - Eight delay element.
       * [/list]
       */
      scml2::bitfield< unsigned int > param_phase_detect_sel;
      unsigned int param_phase_detect_sel_reset_value;
      /* DLL bypass mode control. Controls the bypass mode of the master and slave DLLs. The param_dll_bypass_mode is intended to be used only for debug. [list][*]0 - Normal operational mode. DLL functioning in normal mode of operation where the slave delay line settings are used as fractional delay of the master delay line encoder reading of the number of delays in one cycle. [*]1 - Bypass mode on. Delays are defined in phy_dll_slave_ctrl_reg. Master DLL is disabled with only 1 delay element in its delay line. The slave slave delay lines decode delays in absolute delay elements rather than as fractional delays. The dll_lock field (bit [0]) of the phy_dll_obs_reg_0 parameter will be forced high. [/list] */
      scml2::bitfield< unsigned int > param_dll_bypass_mode;
      unsigned int param_dll_bypass_mode_reset_value;
    };
    struct phy_dll_slave_ctrl_reg_type : public scml2::reg< unsigned int > {
      ~phy_dll_slave_ctrl_reg_type();
      phy_dll_slave_ctrl_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Controls the read DQS delay which adjusts the timing in 1/256th of the clock period when in normal DLL locked mode. In bypass mode, this field directly programs the number of delay elements. */
      scml2::bitfield< unsigned int > read_dqs_delay;
      unsigned int read_dqs_delay_reset_value;
      /* Controls the clk_wr delay line which adjusts the write DQ bit timing in 1/256th steps of the clock period in normal DLL locked mode. In bypass mode, this field directly programs the number of delay elements. */
      scml2::bitfield< unsigned int > clk_wr_delay;
      unsigned int clk_wr_delay_reset_value;
    };
    struct phy_ie_timing_reg_type : public scml2::reg< unsigned int > {
      ~phy_ie_timing_reg_type();
      phy_ie_timing_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Specifies the number of clocks of delay for the dfi_rddata_en signal to line it up with the true (normal) DFI read data position. The MC must deliver an early version of the read data enable to allow time for the input pads to turn on and this field allows the PHY to create the original timing. */
      scml2::bitfield< unsigned int > rddata_en_ie_dly;
      unsigned int rddata_en_ie_dly_reset_value;
      /* Define the stop position for the DQS input enable. */
      scml2::bitfield< unsigned int > dqs_ie_stop;
      unsigned int dqs_ie_stop_reset_value;
      /* Define the start position for the DQS input enable. */
      scml2::bitfield< unsigned int > dqs_ie_start;
      unsigned int dqs_ie_start_reset_value;
      /* Define the stop position for the DQ input enable. */
      scml2::bitfield< unsigned int > dq_ie_stop;
      unsigned int dq_ie_stop_reset_value;
      /* Define the start position for the DQ input enable. */
      scml2::bitfield< unsigned int > dq_ie_start;
      unsigned int dq_ie_start_reset_value;
      /* Forces the input enable(s) to be on always. */
      scml2::bitfield< unsigned int > ie_always_on;
      unsigned int ie_always_on_reset_value;
    };
    struct phy_obs_reg_0_type : public scml2::reg< unsigned int > {
      ~phy_obs_reg_0_type();
      phy_obs_reg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Loopback Status [list] [*] Bit0 - lpbk start; Defines the status of the loopback mode. 0 = Not in loopback mode; 1 = In loopback mode. [*] Bit1 - lpbk status; Defines the status of the loopback mode. 0 = Last Loopback test had no errors; 1 = Last loopback test contained data errors. [/list] */
      scml2::bitfield< unsigned int > lpbk_status;
      unsigned int lpbk_status_reset_value;
      /* If errors are encountered in loopback test this field reports the actual data or the expected data, depending on the setting of the phy_gate_lpbk_ctrl_reg [12] parameter bit. This field is not clear by the clear state of the loopback. If there are no errors in loopback test the value is zero (or value from previous state). */
      scml2::bitfield< unsigned int > lpbk_dq_data;
      unsigned int lpbk_dq_data_reset_value;
      /**
       * Status signal to indicate that the logic gate had to
       * be forced closed. It indicates that either the DQS
       * strobe did not appear during read or rd_del_sel
       * signal value is too low and dfi_rddata are corrupted.
       * The dll_rst_n or rst_n clears this
       * flag.
       */
      scml2::bitfield< unsigned int > dqs_underrun;
      unsigned int dqs_underrun_reset_value;
      /**
       * Status signal to indicate that the logic gate was closed too late
       * ie. the number of DQS strobes exceed the capacity of the entry FIFO.
       * It indicates that rd_del_sel signal value is too high and dfi_rddata are corrupted.
       *  It is possible that overflow status is asserted with underrun status - in such case the overflow takes the precedence.
       * The dll_rst_n or rst_n clears this flag.
       */
      scml2::bitfield< unsigned int > dqs_overflow;
      unsigned int dqs_overflow_reset_value;
    };
    struct phy_dll_obs_reg_0_type : public scml2::reg< unsigned int > {
      ~phy_dll_obs_reg_0_type();
      phy_dll_obs_reg_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Indicates status of DLL. It indicates the DLL locking when the DLL lock logic found (not inc AND not dec) OR (an inc then dec) OR (a dec then inc). When param_dll_start_point is set smaller than half clock period the first found (a dec then inc) isn't the really DLL locking point but dll_lock is asserted. [list][*]0 - DLL has not locked. [*]1 - DLL is locked. [/list] */
      scml2::bitfield< unsigned int > dll_lock;
      unsigned int dll_lock_reset_value;
      /**
       * Indicates status of DLL. Defines the mode in which the DLL has achieved the lock.
       * [list]
       *   [*]'b00 -  Full clock mode. The master delay line was long enough to lock on one full
       *     clock cycle of delay. In this mode, the dll_lock_value field (bits [15:8]) of this
       *     parameter indicates the number of delays in full clock cycles.
       *   [*]'b01 - Reserved.
       *   [*]'b10 - Half clock mode. The master delay line was not long enough to lock one full
       *     cycle of delay but could lock on a half-cycle of delay. In this mode, the
       *     dll_lock_value field (bits [15:8]) of this parameter indicates the number
       *     of delays in one half clock cycles.
       *   [*]'b11 -  Saturation mode. The master delay line was not long enough to lock on a full or
       *     a half-clock cycle. In this mode, the encoder value is fixed at the maximum delay line
       *     setting and the master DLL will be disabled. The slave delay lines continue to use the
       *     fractional delays based upon the fixed saturation value of the delay line.
       * [/list]
       */
      scml2::bitfield< unsigned int > dll_locked_mode;
      unsigned int dll_locked_mode_reset_value;
      /* Reports the number of times that the master DLL consecutive increment or decrement value programmed into the param_dll_lock_num field (bits [18:16]) of the phy_dll_master_ctrl_reg register has been triggered. The dll_unlock_cnt will saturate at a value of 0x1f. Asserting the dll_rst_n signal will reset this counter to 0. */
      scml2::bitfield< unsigned int > dll_unlock_cnt;
      unsigned int dll_unlock_cnt_reset_value;
      /* Reports the number of delay elements that the DLL has determined for lock in either full clock or half clock mode. In full clock mode, this value equals the number of delay elements in one cycle. In half clock mode, this value equals the number of delay elements in one half clock cycle. In saturation mode, this value equals the maximum number of delay elements. The slaves use this value to set up their delays for the clk_wr and read DQS signals. This value is valid only when locking mechanism is done. */
      scml2::bitfield< unsigned int > dll_lock_value;
      unsigned int dll_lock_value_reset_value;
      /* Holds the state of the cumulative dll_lock_dec register when the dll_unlock_cnt field(bits [7:3]) of this parameter was triggered to decrement or was last saturated at a value of 0x1f. */
      scml2::bitfield< unsigned int > lock_dec_dbg;
      unsigned int lock_dec_dbg_reset_value;
      /* Holds the state of the cumulative dll_lock_inc register when the dll_unlock_cnt field(bits [7:3]) of this parameter was triggered to increment or was last saturated at a value of 0x1f. */
      scml2::bitfield< unsigned int > lock_inc_dbg;
      unsigned int lock_inc_dbg_reset_value;
    };
    struct phy_dll_obs_reg_1_type : public scml2::reg< unsigned int > {
      ~phy_dll_obs_reg_1_type();
      phy_dll_obs_reg_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Holds the encoded value for the read delay line for this slice. */
      scml2::bitfield< unsigned int > decoder_out_rd;
      unsigned int decoder_out_rd_reset_value;
      /* Holds the encoded value for the clk_wr delay line for this slice. */
      scml2::bitfield< unsigned int > decoder_out_wr;
      unsigned int decoder_out_wr_reset_value;
    };
    struct phy_static_togg_reg_type : public scml2::reg< unsigned int > {
      ~phy_static_togg_reg_type();
      phy_static_togg_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Clock divider to create toggle signal. */
      scml2::bitfield< unsigned int > static_tog_clk_div;
      unsigned int static_tog_clk_div_reset_value;
      /* Global control to enable the toggle signal during static activity. */
      scml2::bitfield< unsigned int > static_togg_global_enable;
      unsigned int static_togg_global_enable_reset_value;
      /**
       * Control to enable the toggle signal during static activity.
       * When low the feature is disabled.
       * [list][*]bit 0 - master delay line enable.
       *       [*]bit 1 - read path delay line enable.
       *       [*]bit 2 - write path delay line enable.
       *  [/list]
       */
      scml2::bitfield< unsigned int > static_togg_enable;
      unsigned int static_togg_enable_reset_value;
    };
    struct phy_wr_deskew_pd_ctrl_0_reg_type : public scml2::reg< unsigned int > {
      ~phy_wr_deskew_pd_ctrl_0_reg_type();
      phy_wr_deskew_pd_ctrl_0_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * DLL Phase Detect Selector for DQ generation to handle the clock domain crossing between the clock and clk_wr signal. Selects the number
       * of delay elements to be inserted between the phase detect flip-flops. Defaults to 0x0.
       * [list]
       * [*]'b000 - One delay element.
       * [*]'b001 - Two delay element.
       * [*]'b010 - Three delay element.
       * [*]'b011 - Four delay element.
       * [*]'b100 - Five delay element.
       * [*]'b101 - Six delay element.
       * [*]'b110 - Seven delay element.
       * [*]'b111 - Eight delay element.
       * [/list]
       */
      scml2::bitfield< unsigned int > dq_phase_detect_sel;
      unsigned int dq_phase_detect_sel_reset_value;
      /**
       * [list]
       * [*]'b0 - No effect.
       * [*]'b1 - Adds a half clock delay to the write data path.[/list]
       */
      scml2::bitfield< unsigned int > dq_sw_half_cycle_shift;
      unsigned int dq_sw_half_cycle_shift_reset_value;
      /**
       * Enables the software
       *      half cycle shift. This determines if write data is
       *      transferred to the clk_wr domain on the
       *      positive or negative edge of the PHY clock.
       *      This field is valid when dq_sw_dq_phase_bypass is low.
       * [list]
       *    [*]'b0 - Hardware automatically controls any
       *      shifting needed for the write level delay line.
       *    [*]'b1 - The setting in the dq0_sw_half_cycle_shift
       *      field this reg defines the shift.
       *      Note: If the user chooses to control the half
       *      cycle shift manually, it is important that the
       *      dq_sw_half_cycle_shift field (bit [4]) of the
       *      phy_wr_deskew_pd_ctrl_reg parameter be
       *      cleared to 'b0 if the delay is less than a 1/2
       *      cycle and set to 'b1 if the delay is greater than
       *      a 1/2 cycle. It is recommended to allow the
       *      hardware to control this automatically.
       * [/list]
       */
      scml2::bitfield< unsigned int > dq_en_sw_half_cycle;
      unsigned int dq_en_sw_half_cycle_reset_value;
      /**
       * [list]
       *    [*]'b0 - Use phase detect circuit to determine
       *       the half_cycle_shift.
       *    [*]'b1 - Use the clk_wr_delay delay line setting to
       *       determine the half_cycle_shift. A delay line
       *       setting of 0x00-0x7f means half_cycle_shift
       *       = 0 and a delay line setting of 0x80-0xff
       *       means half_cycle_shift = 1.
       * [/list]
       */
      scml2::bitfield< unsigned int > dq_sw_dq_phase_bypass;
      unsigned int dq_sw_dq_phase_bypass_reset_value;
    };
    struct phy_version_reg_type : public scml2::reg< unsigned int > {
      ~phy_version_reg_type();
      phy_version_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* PHY revision number. */
      scml2::bitfield< unsigned int > phy_rev;
      unsigned int phy_rev_reset_value;
      /* Fixed number (minor revision number). */
      scml2::bitfield< unsigned int > phy_fix;
      unsigned int phy_fix_reset_value;
      /* Magic number. */
      scml2::bitfield< unsigned int > combo_phy_magic_number;
      unsigned int combo_phy_magic_number_reset_value;
    };
    struct phy_features_reg_type : public scml2::reg< unsigned int > {
      ~phy_features_reg_type();
      phy_features_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Support for ONFI4.0 - NAND Flash. */
      scml2::bitfield< unsigned int > onfi_40;
      unsigned int onfi_40_reset_value;
      /* Support for ONFI4.1 - NAND Flash. */
      scml2::bitfield< unsigned int > onfi_41;
      unsigned int onfi_41_reset_value;
      /* Support for 16bit in ONFI SDR work mode. */
      scml2::bitfield< unsigned int > sdr_16bit;
      unsigned int sdr_16bit_reset_value;
      /* Support for XSPI. */
      scml2::bitfield< unsigned int > xspi;
      unsigned int xspi_reset_value;
      /* Support for SD/eMMC. */
      scml2::bitfield< unsigned int > sd_emmc;
      unsigned int sd_emmc_reset_value;
      /* Maximum number of banks supported by hardware. This is an encoded value. [list][*]0 - One bank. [*]1 - Two banks. [*]2 - Four banks. [*]3 - Eight banks.[/list] */
      scml2::bitfield< unsigned int > bank_num;
      unsigned int bank_num_reset_value;
      /* Number of taps in delay line. This is an encoded value. [list][*]0 - 128. [*]1 - 256. [/list] */
      scml2::bitfield< unsigned int > dll_tap_num;
      unsigned int dll_tap_num_reset_value;
      /* Support for aging in delay lines. */
      scml2::bitfield< unsigned int > aging;
      unsigned int aging_reset_value;
      /* Support for clock ratio on DFI interface. This is an encoded value. [list][*]0 - 1:1 [*]1 - 1:2 [/list] */
      scml2::bitfield< unsigned int > dfi_clock_ratio;
      unsigned int dfi_clock_ratio_reset_value;
      /* Support for per-bit deskew. */
      scml2::bitfield< unsigned int > per_bit_deskew;
      unsigned int per_bit_deskew_reset_value;
      /* SFR interface type.  This is an encoded value. [list][*]0 - DFI. [*]1 - APB.[/list] */
      scml2::bitfield< unsigned int > reg_intf;
      unsigned int reg_intf_reset_value;
      /* Support for external LPBK_DQS io pad. */
      scml2::bitfield< unsigned int > ext_lpbk_dqs;
      unsigned int ext_lpbk_dqs_reset_value;
      /* Support for JTAG muxes. */
      scml2::bitfield< unsigned int > jtag_sup;
      unsigned int jtag_sup_reset_value;
      /* Support for PLL. */
      scml2::bitfield< unsigned int > pll_sup;
      unsigned int pll_sup_reset_value;
      /* Support for Automotive Safety Feature. */
      scml2::bitfield< unsigned int > asf_sup;
      unsigned int asf_sup_reset_value;
    };
    struct phy_ctrl_reg_type : public scml2::reg< unsigned int > {
      ~phy_ctrl_reg_type();
      phy_ctrl_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Defines additional latency on the control signals WE/RE/CE/WP. */
      scml2::bitfield< unsigned int > ctrl_clkperiod_delay;
      unsigned int ctrl_clkperiod_delay_reset_value;
      /* The value of the field should be Zero for xSPI. Please refer to the controller User guide 'Extended_read_mode' */
      scml2::bitfield< unsigned int > phony_dqs_timing;
      unsigned int phony_dqs_timing_reset_value;
    };
    struct phy_tsel_reg_type : public scml2::reg< unsigned int > {
      ~phy_tsel_reg_type();
      phy_tsel_reg_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Termination select read value for the data strobe. */
      scml2::bitfield< unsigned int > tsel_rd_value_dqs;
      unsigned int tsel_rd_value_dqs_reset_value;
      /* Termination select off value for the data strobe. */
      scml2::bitfield< unsigned int > tsel_off_value_dqs;
      unsigned int tsel_off_value_dqs_reset_value;
      /* Termination select read value for the data. */
      scml2::bitfield< unsigned int > tsel_rd_value_data;
      unsigned int tsel_rd_value_data_reset_value;
      /* Termination select off value for the data. */
      scml2::bitfield< unsigned int > tsel_off_value_data;
      unsigned int tsel_off_value_data_reset_value;
    };
    struct phy_gpio_ctrl_0_type : public scml2::reg< unsigned int > {
      ~phy_gpio_ctrl_0_type();
      phy_gpio_ctrl_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* General purpose register field. The [31:0] vector is brought to the PHY I/Os. User may choose to use these pins to control any static settings that may be required for the connected I/O pads. */
      scml2::bitfield< unsigned int > phy_gpio_ctrl_0_value;
      unsigned int phy_gpio_ctrl_0_value_reset_value;
    };
    struct phy_gpio_ctrl_1_type : public scml2::reg< unsigned int > {
      ~phy_gpio_ctrl_1_type();
      phy_gpio_ctrl_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* General purpose register field. The [31:0] vector is brought to the PHY IOs. User may choose to use these pins to control any static settings that may be required for the connected IO pads. */
      scml2::bitfield< unsigned int > phy_gpio_ctrl_1_value;
      unsigned int phy_gpio_ctrl_1_value_reset_value;
    };
    struct phy_gpio_status_0_type : public scml2::reg< unsigned int > {
      ~phy_gpio_status_0_type();
      phy_gpio_status_0_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* This register is a general purpose register. A [31:0] vector is brought from the PHY IOs to this register. User may choose to use this as a status register. */
      scml2::bitfield< unsigned int > phy_gpio_status_0_value;
      unsigned int phy_gpio_status_0_value_reset_value;
    };
    struct phy_gpio_status_1_type : public scml2::reg< unsigned int > {
      ~phy_gpio_status_1_type();
      phy_gpio_status_1_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* This register is a general purpose register. A [31:0] vector is brought from the PHY I/Os to this register. User may choose to use this as a status register. */
      scml2::bitfield< unsigned int > phy_gpio_status_1_value;
      unsigned int phy_gpio_status_1_value_reset_value;
    };
    scml2::objects::router<unsigned int> t_reg_socket_router;
    scml2::tlm2_ft_target_port_pe<32> t_reg_socket_protocol_engine;
    scml2::reset_slave_engine reset_in_protocol_engine;
    scml2::interrupt_master_engine int_out_protocol_engine;
    scml2::memory< unsigned int > ctrl_cmd_stat_a;
    /**
     * Command register 0. Writing data to this register will initiate a new transaction of the xSPI Flash Controller in CDMA/PIO and STIG work mode.
     * Fields encoding of those registers depend on selected work mode. Refer to the CDMA/PIO or STIG workmode section in the userguide.
     */
    cmd_reg0_type cmd_reg0;
    /* Command register 1. This register is only used in CDMA/PIO(IP6522 only) and STIG work mode, and the definition changes depending on the workmode. Refer to the applicable section in the UserGuide. */
    cmd_reg1_type cmd_reg1;
    /* Command register 2. This register is only used in CDMA/PIO(IP6522 only) and STIG work mode, and the definition changes depending on the workmode. Refer to the applicable section in the UserGuide. */
    cmd_reg2_type cmd_reg2;
    /* Command register 3. This register is only used in CDMA/PIO(IP6522 only) and STIG work mode, and the definition changes depending on the workmode. Refer to the applicable section in the UserGuide. */
    cmd_reg3_type cmd_reg3;
    /* Command register 4. This register is only used in CDMA/PIO(IP6522 only) and STIG work mode, and the definition changes depending on the workmode. Refer to the applicable section in the UserGuide. */
    cmd_reg4_type cmd_reg4;
    /* Command register 5. This register is only used in CDMA/PIO(IP6522 only) and STIG work mode, and the definition changes depending on the workmode. Refer to the applicable section in the UserGuide. */
    cmd_reg5_type cmd_reg5;
    /* This register is relevant for ACMD mode only and selects which thread will be used for outputting the command status in the cmd_status register. */
    cmd_status_ptr_type cmd_status_ptr;
    /**
     * This status register reports the Command Status
     * for the selected thread in ACMD work mode and
     * for STIG work mode when an xSPI flash transaction has completed.
     */
    cmd_status_type cmd_status;
    /* General Controller Status Register. */
    ctrl_status_type ctrl_status;
    /* Auto Command Engine Thread Status (ACMD mode only). */
    trd_status_type trd_status;
    /* Interrupt status register. */
    intr_status_type intr_status;
    /* Interrupt enable register. If the selected bit of this register is set, the rising edge of the corresponding bit in intr_status will trigger an interrupt. */
    intr_enable_type intr_enable;
    /**
     * Each bit of this field corresponds to an Auto Command Engine thread and holds the descriptor status for that selected thread.
     * It is set only when INT bit of descriptor is set.
     */
    trd_comp_intr_status_type trd_comp_intr_status;
    /**
     * This register can be read to determine if a particular thread (in the Auto Command engine) has encountered an error condition.
     * To get more information on the error, s/w needs to read the status field of the descriptor(CDMA mode) or appropriate status register(PIO mode).
     */
    trd_error_intr_status_type trd_error_intr_status;
    /**
     * Interrupt enable register. If selected bit of this register is set,
     * the rising edge of corresponding bit in trd_error_intr_status will cause setting of the external interrupt line.
     */
    trd_error_intr_en_type trd_error_intr_en;
    /**
     * Master data interface error address [31:0]. This register
     * holds the lower 32 bits of the address of the AXI request on the system master data interface
     * that caused the cdma_terr or ddma_terr bits in the
     * intr_status register to be set. This register will be overwritten if further error
     * responses are detected.
     */
    dma_target_error_l_type dma_target_error_l;
    /**
     * Master data interface error address [63:32]. This register
     * holds the upper 32 bits of the address of the AXI request on the system master data interface
     * that caused the cdma_terr or ddma_terr bits in the
     * intr_status register to be set. This register will be overwritten if further error
     * responses are detected.
     */
    dma_target_error_h_type dma_target_error_h;
    /* This register provides status of the most recent boot operation, triggered by the automated boot controller. */
    boot_status_type boot_status;
    scml2::memory< unsigned int > ctrl_cfg_common_a;
    /* Wait count value for long polling. */
    long_polling_type long_polling;
    /* Status monitor cycle count value. */
    short_polling_type short_polling;
    /* Device control register. */
    ctrl_config_type ctrl_config;
    /* AXI Interface settings register. It is common register for both the Master and Slave interface. */
    dma_settings_type dma_settings;
    /* Transferred data block size for the Slave DMA module. */
    sdma_size_type sdma_size;
    /* Information for current Slave DMA transaction related with execution thread. */
    sdma_trd_info_type sdma_trd_info;
    /**
     * This register stores the buffer address in the host memory that will be used as a sink/source for the SDMA transfer.
     * The SDMA address is based on the Memory Pointer field that was programed by the host as part of the CDMA/PIO command.
     * A single CDMA/PIO command can trigger multiple transfers on the slave interface, so the SDMA address value will be automatically incremented and updated before
     * each SDMA transfer.
     */
    sdma_addr0_type sdma_addr0;
    /**
     * This register stores the buffer address in the host memory that will be used as a sink/source for the SDMA transfer.
     * The SDMA address is based on the Memory Pointer field that was programed by the host as part of the CDMA/PIO command.
     * A single CDMA/PIO command can trigger multiple transfers on the slave interface, so the SDMA address value will be automatically incremented and updated before
     * each SDMA transfer.
     */
    sdma_addr1_type sdma_addr1;
    /* Device Discovery control register. */
    discovery_control_type discovery_control;
    scml2::memory< unsigned int > cmn_seq_regs_a;
    /**
     * Register designated to configure controller in XIP work mode
     * in
     * CDMA, PIO and
     * DIRECT work mode.
     */
    xip_mode_cfg_type xip_mode_cfg;
    /**
     * Register to configure common values for sequences in
     * CDMA, PIO and
     * DIRECT work mode.
     */
    global_seq_cfg_type global_seq_cfg;
    /**
     * Register to configure common values for sequences in
     * CDMA, PIO and
     * DIRECT work mode.
     */
    global_seq_cfg_1_type global_seq_cfg_1;
    /* Register to hold specific configuration required while operating in the DIRECT work mode. */
    direct_access_cfg_type direct_access_cfg;
    /**
     * When rmp_addr_en of the direct config register is set to 1, the incoming AXI Slave address will be adapted
     * and sent to the Flash device as (address - N), where N[31:0] is the value stored
     * in this register
     */
    direct_access_rmp_type direct_access_rmp;
    /**
     * When rmp_addr_en of the direct config register is set to 1, the incoming AXI Slave address will be adapted
     * and sent to the Flash device as (address - N), where N[63:32] is the value stored
     * in this register
     */
    direct_access_rmp_1_type direct_access_rmp_1;
    scml2::memory< unsigned int > dev_seq_regs_a;
    /* Register to configure RESET sequence for PROFILE 1 and SPI NAND in ACMD work mode. */
    rst_seq_cfg_0_type rst_seq_cfg_0;
    /* Register to configure RESET sequence for PROFILE 1 and SPI NAND in ACMD work mode. */
    rst_seq_cfg_1_type rst_seq_cfg_1;
    /* Register to configure ERASE_SECTOR sequence for PROFILE 1 and SPI NAND in ACMD work mode. */
    ers_seq_cfg_0_type ers_seq_cfg_0;
    /* Register to configure ERASE_SECTOR sequence for PROFILE 1 and SPI NAND in ACMD work mode. */
    ers_seq_cfg_1_type ers_seq_cfg_1;
    /* Register to configure ERASE_ALL sequence for PROFILE 1 in ACMD work mode. */
    ers_seq_cfg_2_type ers_seq_cfg_2;
    /**
     * Register to configure PROGRAM sequence for PROFILE 1 and SPI NAND in
     * ACMD and DIRECT work modes.
     */
    prog_seq_cfg_0_type prog_seq_cfg_0;
    /**
     * Register to configure PROGRAM sequence for PROFILE 1 and SPI NAND in
     * ACMD and DIRECT work modes.
     */
    prog_seq_cfg_1_type prog_seq_cfg_1;
    /**
     * Register to configure PROGRAM sequence for PROFILE 2 in
     * ACMD and DIRECT work modes.
     */
    prog_seq_cfg_2_type prog_seq_cfg_2;
    /**
     * Register to configure READ sequence for PROFILE 1 and SPI NAND in
     * ACMD and DIRECT work modes.
     */
    read_seq_cfg_0_type read_seq_cfg_0;
    /**
     * Register to configure READ sequence for PROFILE 1 and SPI NAND in
     * ACMD and DIRECT work modes.
     */
    read_seq_cfg_1_type read_seq_cfg_1;
    /**
     * Register to configure READ sequence for PROFILE 2 in
     * ACMD and DIRECT work modes.
     */
    read_seq_cfg_2_type read_seq_cfg_2;
    /**
     * Register to configure Write Enable Latch (WEL) sequence for PROFILE 1 and SPI NAND in
     * ACMD and DIRECT work modes.
     */
    we_seq_cfg_0_type we_seq_cfg_0;
    /**
     * Register to configure status checking sequence for PROFILE 1 and SPI NAND in
     * ACMD and DIRECT work modes.
     */
    stat_seq_cfg_0_type stat_seq_cfg_0;
    /**
     * Register to configure status checking sequence for PROFILE 1 and SPI NAND
     * ACMD and DIRECT work modes.
     */
    stat_seq_cfg_1_type stat_seq_cfg_1;
    /**
     * Register to configure status checking sequence for PROFILE 1 and SPI NAND in
     * ACMD and DIRECT work modes.
     */
    stat_seq_cfg_2_type stat_seq_cfg_2;
    /**
     * Register to configure status checking sequence for PROFILE 1 and SPI NAND in
     * ACMD and DIRECT work modes.
     */
    stat_seq_cfg_3_type stat_seq_cfg_3;
    /**
     * Register to configure status checking sequence for PROFILE 2 - HF in
     * ACMD and DIRECT work modes.
     */
    stat_seq_cfg_4_type stat_seq_cfg_4;
    /**
     * Register to configure status checking sequence for PROFILE 1, SPI NAND and PROFILE 2 - HF in
     * ACMD and DIRECT work modes.
     */
    stat_seq_cfg_5_type stat_seq_cfg_5;
    /**
     * Register to configure status checking sequence for PROFILE 1, SPI NAND and PROFILE 2 - HF
     * in ACMD and DIRECT work modes.
     */
    stat_seq_cfg_7_type stat_seq_cfg_7;
    /* Register to configure status checking sequence for PROFILE 1, SPI NAND and PROFILE 2 - HF in ACMD and DIRECT work modes. */
    stat_seq_cfg_8_type stat_seq_cfg_8;
    /* Register to configure status checking sequence for PROFILE 1, SPI NAND and PROFILE 2 - HF in ACMD work mode. */
    stat_seq_cfg_9_type stat_seq_cfg_9;
    /**
     * Register to configure status checking sequence for SPI NAND devices in
     * ACMD and DIRECT work modes.
     */
    stat_seq_cfg_10_type stat_seq_cfg_10;
    scml2::memory< unsigned int > ctrl_consts_a;
    /* Register contains release identification number. */
    xspi_ctrl_version_type xspi_ctrl_version;
    /* Shows available hardware features of the controller */
    ctrl_features_reg_type ctrl_features_reg;
    scml2::memory< unsigned int > rf_minictrl_regs_a;
    /* Write Protect. */
    wp_settings_type wp_settings;
    /* Software Controlled Hardware RESET. */
    reset_pin_settings_type reset_pin_settings;
    /* This register defines the CPOL/CPHA settings for legacy SPI mode (defaulting to mode 0). */
    clock_mode_settings_type clock_mode_settings;
    /* This register is used to introduce relative device selection delays applicable for JEDEC Reset Instruction. */
    jedec_rst_timing_reg_type jedec_rst_timing_reg;
    /* This register is used to introduce relative device selection delays with respect to generated xSPI Flash Interface. */
    dev_delay_reg_type dev_delay_reg;
    /* This register is used to introduce relative reset recovery delay with respect to generated xSPI Flash Interface. */
    rst_recovery_reg_type rst_recovery_reg;
    /**
     * This register is used to introduce a maximum number of xspi_clk cycles through which
     * CS# will be kept active (low) on Memory interface. It is sometimes referred to as tCMS or tCEM timing and is
     * generally only relevant for RAM devices that require to be periodically refreshed. The refresh is typically
     * handled internal to the device, but to fit this refesh operation in along with normal controller requests, it may require
     * that the controller limits its access time. The user can program this register to force the controller to obey that 
     * requirement.
     */
    dev_active_max_reg_type dev_active_max_reg;
    /* This register is only applicable for Legacy Hyper Flash or xSPI Profile 2.0 devices and can be used to configure where the reserved area of the Command-Address(CA) field is. */
    hf_offset_reg_type hf_offset_reg;
    /* Configuration of the resynchronization of slave DLL of PHY. */
    dll_phy_update_cnt_type dll_phy_update_cnt;
    /**
     * Configuration of the resynchronization of slave DLL of PHY.
     * When the PHY is used with the Cadence xSPI controller, this
     * register is automatically updated by  Device Discovery
     *  during initialization.
     */
    dll_phy_ctrl_type dll_phy_ctrl;
    scml2::memory< unsigned int > dataslice_Rfile_a;
    /* This register controls the DQ related timing. */
    phy_dq_timing_reg_type phy_dq_timing_reg;
    /* This register controls the DQS related timing. */
    phy_dqs_timing_reg_type phy_dqs_timing_reg;
    /* This register controls the gate and loopback control related timing. */
    phy_gate_lpbk_ctrl_reg_type phy_gate_lpbk_ctrl_reg;
    /* This register holds the control for the Master DLL logic. */
    phy_dll_master_ctrl_reg_type phy_dll_master_ctrl_reg;
    /* This register holds the control for the slave DLL logic. */
    phy_dll_slave_ctrl_reg_type phy_dll_slave_ctrl_reg;
    /* This register controls the DQS related timing. */
    phy_ie_timing_reg_type phy_ie_timing_reg;
    /* This register holds the following observable points in the PHY. */
    phy_obs_reg_0_type phy_obs_reg_0;
    /* This register holds the following observable points in the PHY. */
    phy_dll_obs_reg_0_type phy_dll_obs_reg_0;
    /* This register holds the following observable points in the PHY. */
    phy_dll_obs_reg_1_type phy_dll_obs_reg_1;
    /* This register controls the static aging feature of the PHY. */
    phy_static_togg_reg_type phy_static_togg_reg;
    /* This register holds the values of phase detect block for each DQ bit on the write path. */
    phy_wr_deskew_pd_ctrl_0_reg_type phy_wr_deskew_pd_ctrl_0_reg;
    /* This register contains release identification number. */
    phy_version_reg_type phy_version_reg;
    /* This register shows available hardware features. */
    phy_features_reg_type phy_features_reg;
    scml2::memory< unsigned int > ctb_Rfile_a;
    /* This register handles the global control settings for the PHY. */
    phy_ctrl_reg_type phy_ctrl_reg;
    /* This register handles the global control settings for the termination selects for reads. */
    phy_tsel_reg_type phy_tsel_reg;
    /* This register is a general purpose register. The [31:0] vector is brought to the PHY I/Os. User may choose to use these pins to control any static settings that may be required for the connected I/O pads. */
    phy_gpio_ctrl_0_type phy_gpio_ctrl_0;
    /* This register is a general purpose register. The [31:0] vector is brought to the PHY I/Os. User may choose to use these pins to control any static settings that may be required for the connected IO pads. */
    phy_gpio_ctrl_1_type phy_gpio_ctrl_1;
    /* This register is a general purpose register. A [31:0] vector is brought from the PHY IOs to this register. User may choose to use this as a status register. */
    phy_gpio_status_0_type phy_gpio_status_0;
    /* This register is a general purpose register. A [31:0] vector is brought from the PHY IOs to this register. User may choose to use this as a status register. */
    phy_gpio_status_1_type phy_gpio_status_1;
private:
};
#ifdef _WIN32
#pragma optimize("",on)
#else
#pragma GCC pop_options
#endif


}  // end of namespace mylibrary
