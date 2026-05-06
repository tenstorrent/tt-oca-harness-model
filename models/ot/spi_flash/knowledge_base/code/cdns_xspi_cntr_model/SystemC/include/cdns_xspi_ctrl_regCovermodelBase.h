/***************************************************************************
 * Copyright 1996-2025 Synopsys, Inc.
 *
 * This Synopsys software and all associated documentation are proprietary
 * to Synopsys, Inc. and may only be used pursuant to the terms and
 * conditions of a written license agreement with Synopsys, Inc.
 * All other use, reproduction, modification, or distribution of the
 * Synopsys software or the associated documentation is strictly prohibited.
 ***************************************************************************/
 

/***************************************************************************
 * Generated snippet, used for detecting user edits.
 * CHECKSUM:2b6b999eb1cfc353b9c4d9b4bd37dc3288e796c4
 ***************************************************************************/
 
#pragma once

#include "scml2_coverage.h"
#include "cdns_xspi_ctrl_reg.h"
#include "../../SystemC/include/cdns_extension.h"
#include "../../SystemC/include/sfdp.h"

namespace mylibrary {
#ifdef _WIN32
#pragma optimize("g",off)
#else
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif


class cdns_xspi_ctrl_regCovermodelBase
#ifdef SNPS_SLS_VP_COVERAGE
  : public scml2::cov::covergroup
#endif
{
public:
 cdns_xspi_ctrl_regCovermodelBase(const std::string& test_name, cdns_xspi_ctrl_reg& t)
 #ifdef SNPS_SLS_VP_COVERAGE
  : scml2::cov::covergroup("cdns_xspi_ctrl_reg", test_name, &t)
  , NUM_TARGETS(t.NUM_TARGETS, "NUM_TARGETS")
  , int_out(t.int_out, "int_out")
  , reset_in(t.reset_in, "reset_in")
  , t_reg_socket(t.t_reg_socket, "t_reg_socket")
  , ctrl_cmd_stat_a(t.ctrl_cmd_stat_a, "ctrl_cmd_stat_a")
  , cmd_reg0(t.cmd_reg0, "ctrl_cmd_stat_a.cmd_reg0", this)
  , cmd_reg1(t.cmd_reg1, "ctrl_cmd_stat_a.cmd_reg1", this)
  , cmd_reg2(t.cmd_reg2, "ctrl_cmd_stat_a.cmd_reg2", this)
  , cmd_reg3(t.cmd_reg3, "ctrl_cmd_stat_a.cmd_reg3", this)
  , cmd_reg4(t.cmd_reg4, "ctrl_cmd_stat_a.cmd_reg4", this)
  , cmd_reg5(t.cmd_reg5, "ctrl_cmd_stat_a.cmd_reg5", this)
  , cmd_status_ptr(t.cmd_status_ptr, "ctrl_cmd_stat_a.cmd_status_ptr", this)
  , cmd_status(t.cmd_status, "ctrl_cmd_stat_a.cmd_status", this)
  , ctrl_status(t.ctrl_status, "ctrl_cmd_stat_a.ctrl_status", this)
  , trd_status(t.trd_status, "ctrl_cmd_stat_a.trd_status", this)
  , intr_status(t.intr_status, "ctrl_cmd_stat_a.intr_status", this)
  , intr_enable(t.intr_enable, "ctrl_cmd_stat_a.intr_enable", this)
  , trd_comp_intr_status(t.trd_comp_intr_status, "ctrl_cmd_stat_a.trd_comp_intr_status", this)
  , trd_error_intr_status(t.trd_error_intr_status, "ctrl_cmd_stat_a.trd_error_intr_status", this)
  , trd_error_intr_en(t.trd_error_intr_en, "ctrl_cmd_stat_a.trd_error_intr_en", this)
  , dma_target_error_l(t.dma_target_error_l, "ctrl_cmd_stat_a.dma_target_error_l", this)
  , dma_target_error_h(t.dma_target_error_h, "ctrl_cmd_stat_a.dma_target_error_h", this)
  , boot_status(t.boot_status, "ctrl_cmd_stat_a.boot_status", this)
  , ctrl_cfg_common_a(t.ctrl_cfg_common_a, "ctrl_cfg_common_a")
  , long_polling(t.long_polling, "ctrl_cfg_common_a.long_polling", this)
  , short_polling(t.short_polling, "ctrl_cfg_common_a.short_polling", this)
  , ctrl_config(t.ctrl_config, "ctrl_cfg_common_a.ctrl_config", this)
  , dma_settings(t.dma_settings, "ctrl_cfg_common_a.dma_settings", this)
  , sdma_size(t.sdma_size, "ctrl_cfg_common_a.sdma_size", this)
  , sdma_trd_info(t.sdma_trd_info, "ctrl_cfg_common_a.sdma_trd_info", this)
  , sdma_addr0(t.sdma_addr0, "ctrl_cfg_common_a.sdma_addr0", this)
  , sdma_addr1(t.sdma_addr1, "ctrl_cfg_common_a.sdma_addr1", this)
  , discovery_control(t.discovery_control, "ctrl_cfg_common_a.discovery_control", this)
  , cmn_seq_regs_a(t.cmn_seq_regs_a, "cmn_seq_regs_a")
  , xip_mode_cfg(t.xip_mode_cfg, "cmn_seq_regs_a.xip_mode_cfg", this)
  , global_seq_cfg(t.global_seq_cfg, "cmn_seq_regs_a.global_seq_cfg", this)
  , global_seq_cfg_1(t.global_seq_cfg_1, "cmn_seq_regs_a.global_seq_cfg_1", this)
  , direct_access_cfg(t.direct_access_cfg, "cmn_seq_regs_a.direct_access_cfg", this)
  , direct_access_rmp(t.direct_access_rmp, "cmn_seq_regs_a.direct_access_rmp", this)
  , direct_access_rmp_1(t.direct_access_rmp_1, "cmn_seq_regs_a.direct_access_rmp_1", this)
  , dev_seq_regs_a(t.dev_seq_regs_a, "dev_seq_regs_a")
  , rst_seq_cfg_0(t.rst_seq_cfg_0, "dev_seq_regs_a.rst_seq_cfg_0", this)
  , rst_seq_cfg_1(t.rst_seq_cfg_1, "dev_seq_regs_a.rst_seq_cfg_1", this)
  , ers_seq_cfg_0(t.ers_seq_cfg_0, "dev_seq_regs_a.ers_seq_cfg_0", this)
  , ers_seq_cfg_1(t.ers_seq_cfg_1, "dev_seq_regs_a.ers_seq_cfg_1", this)
  , ers_seq_cfg_2(t.ers_seq_cfg_2, "dev_seq_regs_a.ers_seq_cfg_2", this)
  , prog_seq_cfg_0(t.prog_seq_cfg_0, "dev_seq_regs_a.prog_seq_cfg_0", this)
  , prog_seq_cfg_1(t.prog_seq_cfg_1, "dev_seq_regs_a.prog_seq_cfg_1", this)
  , prog_seq_cfg_2(t.prog_seq_cfg_2, "dev_seq_regs_a.prog_seq_cfg_2", this)
  , read_seq_cfg_0(t.read_seq_cfg_0, "dev_seq_regs_a.read_seq_cfg_0", this)
  , read_seq_cfg_1(t.read_seq_cfg_1, "dev_seq_regs_a.read_seq_cfg_1", this)
  , read_seq_cfg_2(t.read_seq_cfg_2, "dev_seq_regs_a.read_seq_cfg_2", this)
  , we_seq_cfg_0(t.we_seq_cfg_0, "dev_seq_regs_a.we_seq_cfg_0", this)
  , stat_seq_cfg_0(t.stat_seq_cfg_0, "dev_seq_regs_a.stat_seq_cfg_0", this)
  , stat_seq_cfg_1(t.stat_seq_cfg_1, "dev_seq_regs_a.stat_seq_cfg_1", this)
  , stat_seq_cfg_2(t.stat_seq_cfg_2, "dev_seq_regs_a.stat_seq_cfg_2", this)
  , stat_seq_cfg_3(t.stat_seq_cfg_3, "dev_seq_regs_a.stat_seq_cfg_3", this)
  , stat_seq_cfg_4(t.stat_seq_cfg_4, "dev_seq_regs_a.stat_seq_cfg_4", this)
  , stat_seq_cfg_5(t.stat_seq_cfg_5, "dev_seq_regs_a.stat_seq_cfg_5", this)
  , stat_seq_cfg_7(t.stat_seq_cfg_7, "dev_seq_regs_a.stat_seq_cfg_7", this)
  , stat_seq_cfg_8(t.stat_seq_cfg_8, "dev_seq_regs_a.stat_seq_cfg_8", this)
  , stat_seq_cfg_9(t.stat_seq_cfg_9, "dev_seq_regs_a.stat_seq_cfg_9", this)
  , stat_seq_cfg_10(t.stat_seq_cfg_10, "dev_seq_regs_a.stat_seq_cfg_10", this)
  , ctrl_consts_a(t.ctrl_consts_a, "ctrl_consts_a")
  , xspi_ctrl_version(t.xspi_ctrl_version, "ctrl_consts_a.xspi_ctrl_version", this)
  , ctrl_features_reg(t.ctrl_features_reg, "ctrl_consts_a.ctrl_features_reg", this)
  , rf_minictrl_regs_a(t.rf_minictrl_regs_a, "rf_minictrl_regs_a")
  , wp_settings(t.wp_settings, "rf_minictrl_regs_a.wp_settings", this)
  , reset_pin_settings(t.reset_pin_settings, "rf_minictrl_regs_a.reset_pin_settings", this)
  , clock_mode_settings(t.clock_mode_settings, "rf_minictrl_regs_a.clock_mode_settings", this)
  , jedec_rst_timing_reg(t.jedec_rst_timing_reg, "rf_minictrl_regs_a.jedec_rst_timing_reg", this)
  , dev_delay_reg(t.dev_delay_reg, "rf_minictrl_regs_a.dev_delay_reg", this)
  , rst_recovery_reg(t.rst_recovery_reg, "rf_minictrl_regs_a.rst_recovery_reg", this)
  , dev_active_max_reg(t.dev_active_max_reg, "rf_minictrl_regs_a.dev_active_max_reg", this)
  , hf_offset_reg(t.hf_offset_reg, "rf_minictrl_regs_a.hf_offset_reg", this)
  , dll_phy_update_cnt(t.dll_phy_update_cnt, "rf_minictrl_regs_a.dll_phy_update_cnt", this)
  , dll_phy_ctrl(t.dll_phy_ctrl, "rf_minictrl_regs_a.dll_phy_ctrl", this)
  , dataslice_Rfile_a(t.dataslice_Rfile_a, "dataslice_Rfile_a")
  , phy_dq_timing_reg(t.phy_dq_timing_reg, "dataslice_Rfile_a.phy_dq_timing_reg", this)
  , phy_dqs_timing_reg(t.phy_dqs_timing_reg, "dataslice_Rfile_a.phy_dqs_timing_reg", this)
  , phy_gate_lpbk_ctrl_reg(t.phy_gate_lpbk_ctrl_reg, "dataslice_Rfile_a.phy_gate_lpbk_ctrl_reg", this)
  , phy_dll_master_ctrl_reg(t.phy_dll_master_ctrl_reg, "dataslice_Rfile_a.phy_dll_master_ctrl_reg", this)
  , phy_dll_slave_ctrl_reg(t.phy_dll_slave_ctrl_reg, "dataslice_Rfile_a.phy_dll_slave_ctrl_reg", this)
  , phy_ie_timing_reg(t.phy_ie_timing_reg, "dataslice_Rfile_a.phy_ie_timing_reg", this)
  , phy_obs_reg_0(t.phy_obs_reg_0, "dataslice_Rfile_a.phy_obs_reg_0", this)
  , phy_dll_obs_reg_0(t.phy_dll_obs_reg_0, "dataslice_Rfile_a.phy_dll_obs_reg_0", this)
  , phy_dll_obs_reg_1(t.phy_dll_obs_reg_1, "dataslice_Rfile_a.phy_dll_obs_reg_1", this)
  , phy_static_togg_reg(t.phy_static_togg_reg, "dataslice_Rfile_a.phy_static_togg_reg", this)
  , phy_wr_deskew_pd_ctrl_0_reg(t.phy_wr_deskew_pd_ctrl_0_reg, "dataslice_Rfile_a.phy_wr_deskew_pd_ctrl_0_reg", this)
  , phy_version_reg(t.phy_version_reg, "dataslice_Rfile_a.phy_version_reg", this)
  , phy_features_reg(t.phy_features_reg, "dataslice_Rfile_a.phy_features_reg", this)
  , ctb_Rfile_a(t.ctb_Rfile_a, "ctb_Rfile_a")
  , phy_ctrl_reg(t.phy_ctrl_reg, "ctb_Rfile_a.phy_ctrl_reg", this)
  , phy_tsel_reg(t.phy_tsel_reg, "ctb_Rfile_a.phy_tsel_reg", this)
  , phy_gpio_ctrl_0(t.phy_gpio_ctrl_0, "ctb_Rfile_a.phy_gpio_ctrl_0", this)
  , phy_gpio_ctrl_1(t.phy_gpio_ctrl_1, "ctb_Rfile_a.phy_gpio_ctrl_1", this)
  , phy_gpio_status_0(t.phy_gpio_status_0, "ctb_Rfile_a.phy_gpio_status_0", this)
  , phy_gpio_status_1(t.phy_gpio_status_1, "ctb_Rfile_a.phy_gpio_status_1", this)
 #endif
 {
 #ifdef SNPS_SLS_VP_COVERAGE
  ctrl_cmd_stat_a.disable();
  cmd_reg0.configure_default_bins(0);
  cmd_reg1.configure_default_bins(0);
  cmd_reg2.configure_default_bins(0);
  cmd_reg3.configure_default_bins(0);
  cmd_reg4.configure_default_bins(0);
  cmd_reg5.configure_default_bins(0);
  cmd_status_ptr.configure_default_bins(0);
  cmd_status.access(scml2::cov::WRITES);
  cmd_status.configure_default_bins(0);
  ctrl_status.configure_default_bins(0);
  trd_status.access(scml2::cov::WRITES);
  trd_status.configure_default_bins(0);
  intr_status.configure_default_bins(0);
  intr_enable.configure_default_bins(0);
  trd_comp_intr_status.configure_default_bins(0);
  trd_error_intr_status.configure_default_bins(0);
  trd_error_intr_en.configure_default_bins(0);
  dma_target_error_l.access(scml2::cov::WRITES);
  dma_target_error_l.configure_default_bins(0);
  dma_target_error_h.access(scml2::cov::WRITES);
  dma_target_error_h.configure_default_bins(0);
  boot_status.access(scml2::cov::WRITES);
  boot_status.configure_default_bins(0);
  ctrl_cfg_common_a.disable();
  long_polling.configure_default_bins(0);
  short_polling.configure_default_bins(0);
  ctrl_config.configure_default_bins(0);
  dma_settings.configure_default_bins(0);
  sdma_size.access(scml2::cov::WRITES);
  sdma_size.configure_default_bins(0);
  sdma_trd_info.access(scml2::cov::WRITES);
  sdma_trd_info.configure_default_bins(0);
  sdma_addr0.access(scml2::cov::WRITES);
  sdma_addr0.configure_default_bins(0);
  sdma_addr1.access(scml2::cov::WRITES);
  sdma_addr1.configure_default_bins(0);
  discovery_control.configure_default_bins(0);
  cmn_seq_regs_a.disable();
  xip_mode_cfg.configure_default_bins(0);
  global_seq_cfg.configure_default_bins(0);
  global_seq_cfg_1.configure_default_bins(0);
  direct_access_cfg.configure_default_bins(0);
  direct_access_rmp.configure_default_bins(0);
  direct_access_rmp_1.configure_default_bins(0);
  dev_seq_regs_a.disable();
  rst_seq_cfg_0.configure_default_bins(0);
  rst_seq_cfg_1.configure_default_bins(0);
  ers_seq_cfg_0.configure_default_bins(0);
  ers_seq_cfg_1.configure_default_bins(0);
  ers_seq_cfg_2.configure_default_bins(0);
  prog_seq_cfg_0.configure_default_bins(0);
  prog_seq_cfg_1.configure_default_bins(0);
  prog_seq_cfg_2.configure_default_bins(0);
  read_seq_cfg_0.configure_default_bins(0);
  read_seq_cfg_1.configure_default_bins(0);
  read_seq_cfg_2.configure_default_bins(0);
  we_seq_cfg_0.configure_default_bins(0);
  stat_seq_cfg_0.configure_default_bins(0);
  stat_seq_cfg_1.configure_default_bins(0);
  stat_seq_cfg_2.configure_default_bins(0);
  stat_seq_cfg_3.configure_default_bins(0);
  stat_seq_cfg_4.configure_default_bins(0);
  stat_seq_cfg_5.configure_default_bins(0);
  stat_seq_cfg_7.configure_default_bins(0);
  stat_seq_cfg_8.configure_default_bins(0);
  stat_seq_cfg_9.configure_default_bins(0);
  stat_seq_cfg_10.configure_default_bins(0);
  ctrl_consts_a.disable();
  xspi_ctrl_version.access(scml2::cov::WRITES);
  xspi_ctrl_version.configure_default_bins(0);
  ctrl_features_reg.access(scml2::cov::WRITES);
  ctrl_features_reg.configure_default_bins(0);
  rf_minictrl_regs_a.disable();
  wp_settings.configure_default_bins(0);
  reset_pin_settings.configure_default_bins(0);
  clock_mode_settings.configure_default_bins(0);
  jedec_rst_timing_reg.configure_default_bins(0);
  dev_delay_reg.configure_default_bins(0);
  rst_recovery_reg.configure_default_bins(0);
  dev_active_max_reg.configure_default_bins(0);
  hf_offset_reg.configure_default_bins(0);
  dll_phy_update_cnt.configure_default_bins(0);
  dll_phy_ctrl.configure_default_bins(0);
  dataslice_Rfile_a.disable();
  phy_dq_timing_reg.configure_default_bins(0);
  phy_dqs_timing_reg.configure_default_bins(0);
  phy_gate_lpbk_ctrl_reg.configure_default_bins(0);
  phy_dll_master_ctrl_reg.configure_default_bins(0);
  phy_dll_slave_ctrl_reg.configure_default_bins(0);
  phy_ie_timing_reg.configure_default_bins(0);
  phy_obs_reg_0.access(scml2::cov::WRITES);
  phy_obs_reg_0.configure_default_bins(0);
  phy_dll_obs_reg_0.access(scml2::cov::WRITES);
  phy_dll_obs_reg_0.configure_default_bins(0);
  phy_dll_obs_reg_1.access(scml2::cov::WRITES);
  phy_dll_obs_reg_1.configure_default_bins(0);
  phy_static_togg_reg.configure_default_bins(0);
  phy_wr_deskew_pd_ctrl_0_reg.configure_default_bins(0);
  phy_version_reg.access(scml2::cov::WRITES);
  phy_version_reg.configure_default_bins(0);
  phy_features_reg.access(scml2::cov::WRITES);
  phy_features_reg.configure_default_bins(0);
  ctb_Rfile_a.disable();
  phy_ctrl_reg.configure_default_bins(0);
  phy_tsel_reg.configure_default_bins(0);
  phy_gpio_ctrl_0.configure_default_bins(0);
  phy_gpio_ctrl_1.configure_default_bins(0);
  phy_gpio_status_0.access(scml2::cov::WRITES);
  phy_gpio_status_0.configure_default_bins(0);
  phy_gpio_status_1.access(scml2::cov::WRITES);
  phy_gpio_status_1.configure_default_bins(0);

  this->NUM_TARGETS.disable();
 #endif
 }
 #ifdef SNPS_SLS_VP_COVERAGE
protected:
 scml2::cov::scml_property<int> NUM_TARGETS;
 scml2::cov::sc_out<bool> int_out;
 scml2::cov::sc_in<bool> reset_in;
 scml2::cov::socket<scml2::ft_target_socket<32> > t_reg_socket;
 scml2::cov::memory<unsigned int> ctrl_cmd_stat_a;
 struct cmd_reg0_type : public scml2::cov::reg<unsigned int> 
 {
   cmd_reg0_type(cdns_xspi_ctrl_reg::cmd_reg0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cmd0(_reg.cmd0, name + ".cmd0")
  {
   disable();
   cmd0.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cmd0.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cmd0.access(at);
  }
  scml2::cov::bitfield<unsigned int> cmd0;
 };
  cmd_reg0_type cmd_reg0;
 struct cmd_reg1_type : public scml2::cov::reg<unsigned int> 
 {
   cmd_reg1_type(cdns_xspi_ctrl_reg::cmd_reg1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cmd1(_reg.cmd1, name + ".cmd1")
  {
   disable();
   cmd1.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cmd1.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cmd1.access(at);
  }
  scml2::cov::bitfield<unsigned int> cmd1;
 };
  cmd_reg1_type cmd_reg1;
 struct cmd_reg2_type : public scml2::cov::reg<unsigned int> 
 {
   cmd_reg2_type(cdns_xspi_ctrl_reg::cmd_reg2_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cmd2(_reg.cmd2, name + ".cmd2")
  {
   disable();
   cmd2.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cmd2.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cmd2.access(at);
  }
  scml2::cov::bitfield<unsigned int> cmd2;
 };
  cmd_reg2_type cmd_reg2;
 struct cmd_reg3_type : public scml2::cov::reg<unsigned int> 
 {
   cmd_reg3_type(cdns_xspi_ctrl_reg::cmd_reg3_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cmd3(_reg.cmd3, name + ".cmd3")
  {
   disable();
   cmd3.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cmd3.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cmd3.access(at);
  }
  scml2::cov::bitfield<unsigned int> cmd3;
 };
  cmd_reg3_type cmd_reg3;
 struct cmd_reg4_type : public scml2::cov::reg<unsigned int> 
 {
   cmd_reg4_type(cdns_xspi_ctrl_reg::cmd_reg4_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cmd4(_reg.cmd4, name + ".cmd4")
  {
   disable();
   cmd4.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cmd4.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cmd4.access(at);
  }
  scml2::cov::bitfield<unsigned int> cmd4;
 };
  cmd_reg4_type cmd_reg4;
 struct cmd_reg5_type : public scml2::cov::reg<unsigned int> 
 {
   cmd_reg5_type(cdns_xspi_ctrl_reg::cmd_reg5_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cmd5(_reg.cmd5, name + ".cmd5")
  {
   disable();
   cmd5.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cmd5.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cmd5.access(at);
  }
  scml2::cov::bitfield<unsigned int> cmd5;
 };
  cmd_reg5_type cmd_reg5;
 struct cmd_status_ptr_type : public scml2::cov::reg<unsigned int> 
 {
   cmd_status_ptr_type(cdns_xspi_ctrl_reg::cmd_status_ptr_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , thrd_status_sel(_reg.thrd_status_sel, name + ".thrd_status_sel")
  {
   disable();
   thrd_status_sel.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->thrd_status_sel.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->thrd_status_sel.access(at);
  }
  scml2::cov::bitfield<unsigned int> thrd_status_sel;
 };
  cmd_status_ptr_type cmd_status_ptr;
 struct cmd_status_type : public scml2::cov::reg<unsigned int> 
 {
   cmd_status_type(cdns_xspi_ctrl_reg::cmd_status_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cmd_status(_reg.cmd_status, name + ".cmd_status")
  {
   disable();
   cmd_status.access(scml2::cov::WRITES);
   cmd_status.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cmd_status.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cmd_status.access(at);
  }
  scml2::cov::bitfield<unsigned int> cmd_status;
 };
  cmd_status_type cmd_status;
 struct ctrl_status_type : public scml2::cov::reg<unsigned int> 
 {
   ctrl_status_type(cdns_xspi_ctrl_reg::ctrl_status_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , sdma_busy(_reg.sdma_busy, name + ".sdma_busy")
   , mdma_busy(_reg.mdma_busy, name + ".mdma_busy")
   , acmd_eng_busy(_reg.acmd_eng_busy, name + ".acmd_eng_busy")
   , gcmd_eng_busy(_reg.gcmd_eng_busy, name + ".gcmd_eng_busy")
   , gcmd_eng_mc_busy(_reg.gcmd_eng_mc_busy, name + ".gcmd_eng_mc_busy")
   , discovery_busy(_reg.discovery_busy, name + ".discovery_busy")
   , ctrl_busy(_reg.ctrl_busy, name + ".ctrl_busy")
   , init_fail(_reg.init_fail, name + ".init_fail")
   , init_comp(_reg.init_comp, name + ".init_comp")
  {
   disable();
   sdma_busy.access(scml2::cov::WRITES);
   sdma_busy.configure_default_bins(0);
   mdma_busy.access(scml2::cov::WRITES);
   mdma_busy.configure_default_bins(0);
   acmd_eng_busy.access(scml2::cov::WRITES);
   acmd_eng_busy.configure_default_bins(0);
   gcmd_eng_busy.access(scml2::cov::WRITES);
   gcmd_eng_busy.configure_default_bins(0);
   gcmd_eng_mc_busy.access(scml2::cov::WRITES);
   gcmd_eng_mc_busy.configure_default_bins(0);
   discovery_busy.access(scml2::cov::WRITES);
   discovery_busy.configure_default_bins(0);
   ctrl_busy.access(scml2::cov::WRITES);
   ctrl_busy.configure_default_bins(0);
   init_fail.access(scml2::cov::WRITES);
   init_fail.configure_default_bins(0);
   init_comp.access(scml2::cov::WRITES);
   init_comp.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->sdma_busy.disable();
   this->mdma_busy.disable();
   this->acmd_eng_busy.disable();
   this->gcmd_eng_busy.disable();
   this->gcmd_eng_mc_busy.disable();
   this->discovery_busy.disable();
   this->ctrl_busy.disable();
   this->init_fail.disable();
   this->init_comp.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->sdma_busy.access(at);
   this->mdma_busy.access(at);
   this->acmd_eng_busy.access(at);
   this->gcmd_eng_busy.access(at);
   this->gcmd_eng_mc_busy.access(at);
   this->discovery_busy.access(at);
   this->ctrl_busy.access(at);
   this->init_fail.access(at);
   this->init_comp.access(at);
  }
  scml2::cov::bitfield<unsigned int> sdma_busy;
  scml2::cov::bitfield<unsigned int> mdma_busy;
  scml2::cov::bitfield<unsigned int> acmd_eng_busy;
  scml2::cov::bitfield<unsigned int> gcmd_eng_busy;
  scml2::cov::bitfield<unsigned int> gcmd_eng_mc_busy;
  scml2::cov::bitfield<unsigned int> discovery_busy;
  scml2::cov::bitfield<unsigned int> ctrl_busy;
  scml2::cov::bitfield<unsigned int> init_fail;
  scml2::cov::bitfield<unsigned int> init_comp;
 };
  ctrl_status_type ctrl_status;
 struct trd_status_type : public scml2::cov::reg<unsigned int> 
 {
   trd_status_type(cdns_xspi_ctrl_reg::trd_status_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , trd_busy(_reg.trd_busy, name + ".trd_busy")
  {
   disable();
   trd_busy.access(scml2::cov::WRITES);
   trd_busy.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->trd_busy.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->trd_busy.access(at);
  }
  scml2::cov::bitfield<unsigned int> trd_busy;
 };
  trd_status_type trd_status;
 struct intr_status_type : public scml2::cov::reg<unsigned int> 
 {
   intr_status_type(cdns_xspi_ctrl_reg::intr_status_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , gp_open_drain_0(_reg.gp_open_drain_0, name + ".gp_open_drain_0")
   , gp_open_drain_1(_reg.gp_open_drain_1, name + ".gp_open_drain_1")
   , gp_open_drain_2(_reg.gp_open_drain_2, name + ".gp_open_drain_2")
   , gp_open_drain_3(_reg.gp_open_drain_3, name + ".gp_open_drain_3")
   , ctrl_idle(_reg.ctrl_idle, name + ".ctrl_idle")
   , cdma_terr(_reg.cdma_terr, name + ".cdma_terr")
   , ddma_terr(_reg.ddma_terr, name + ".ddma_terr")
   , cmd_ignored(_reg.cmd_ignored, name + ".cmd_ignored")
   , sdma_trigg(_reg.sdma_trigg, name + ".sdma_trigg")
   , sdma_err(_reg.sdma_err, name + ".sdma_err")
   , stig_done(_reg.stig_done, name + ".stig_done")
   , dir_crc_err(_reg.dir_crc_err, name + ".dir_crc_err")
   , dir_dqs_err(_reg.dir_dqs_err, name + ".dir_dqs_err")
   , dir_cmd_err(_reg.dir_cmd_err, name + ".dir_cmd_err")
   , dir_ecc_corr_err(_reg.dir_ecc_corr_err, name + ".dir_ecc_corr_err")
   , dir_dev_err(_reg.dir_dev_err, name + ".dir_dev_err")
  {
   disable();
   gp_open_drain_0.configure_default_bins(0);
   gp_open_drain_1.configure_default_bins(0);
   gp_open_drain_2.configure_default_bins(0);
   gp_open_drain_3.configure_default_bins(0);
   ctrl_idle.configure_default_bins(0);
   cdma_terr.configure_default_bins(0);
   ddma_terr.configure_default_bins(0);
   cmd_ignored.configure_default_bins(0);
   sdma_trigg.configure_default_bins(0);
   sdma_err.configure_default_bins(0);
   stig_done.configure_default_bins(0);
   dir_crc_err.configure_default_bins(0);
   dir_dqs_err.configure_default_bins(0);
   dir_cmd_err.configure_default_bins(0);
   dir_ecc_corr_err.configure_default_bins(0);
   dir_dev_err.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->gp_open_drain_0.disable();
   this->gp_open_drain_1.disable();
   this->gp_open_drain_2.disable();
   this->gp_open_drain_3.disable();
   this->ctrl_idle.disable();
   this->cdma_terr.disable();
   this->ddma_terr.disable();
   this->cmd_ignored.disable();
   this->sdma_trigg.disable();
   this->sdma_err.disable();
   this->stig_done.disable();
   this->dir_crc_err.disable();
   this->dir_dqs_err.disable();
   this->dir_cmd_err.disable();
   this->dir_ecc_corr_err.disable();
   this->dir_dev_err.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->gp_open_drain_0.access(at);
   this->gp_open_drain_1.access(at);
   this->gp_open_drain_2.access(at);
   this->gp_open_drain_3.access(at);
   this->ctrl_idle.access(at);
   this->cdma_terr.access(at);
   this->ddma_terr.access(at);
   this->cmd_ignored.access(at);
   this->sdma_trigg.access(at);
   this->sdma_err.access(at);
   this->stig_done.access(at);
   this->dir_crc_err.access(at);
   this->dir_dqs_err.access(at);
   this->dir_cmd_err.access(at);
   this->dir_ecc_corr_err.access(at);
   this->dir_dev_err.access(at);
  }
  scml2::cov::bitfield<unsigned int> gp_open_drain_0;
  scml2::cov::bitfield<unsigned int> gp_open_drain_1;
  scml2::cov::bitfield<unsigned int> gp_open_drain_2;
  scml2::cov::bitfield<unsigned int> gp_open_drain_3;
  scml2::cov::bitfield<unsigned int> ctrl_idle;
  scml2::cov::bitfield<unsigned int> cdma_terr;
  scml2::cov::bitfield<unsigned int> ddma_terr;
  scml2::cov::bitfield<unsigned int> cmd_ignored;
  scml2::cov::bitfield<unsigned int> sdma_trigg;
  scml2::cov::bitfield<unsigned int> sdma_err;
  scml2::cov::bitfield<unsigned int> stig_done;
  scml2::cov::bitfield<unsigned int> dir_crc_err;
  scml2::cov::bitfield<unsigned int> dir_dqs_err;
  scml2::cov::bitfield<unsigned int> dir_cmd_err;
  scml2::cov::bitfield<unsigned int> dir_ecc_corr_err;
  scml2::cov::bitfield<unsigned int> dir_dev_err;
 };
  intr_status_type intr_status;
 struct intr_enable_type : public scml2::cov::reg<unsigned int> 
 {
   intr_enable_type(cdns_xspi_ctrl_reg::intr_enable_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , gp_open_drain_0_en(_reg.gp_open_drain_0_en, name + ".gp_open_drain_0_en")
   , gp_open_drain_1_en(_reg.gp_open_drain_1_en, name + ".gp_open_drain_1_en")
   , gp_open_drain_2_en(_reg.gp_open_drain_2_en, name + ".gp_open_drain_2_en")
   , gp_open_drain_3_en(_reg.gp_open_drain_3_en, name + ".gp_open_drain_3_en")
   , ctrl_idle_en(_reg.ctrl_idle_en, name + ".ctrl_idle_en")
   , cdma_terr_en(_reg.cdma_terr_en, name + ".cdma_terr_en")
   , ddma_terr_en(_reg.ddma_terr_en, name + ".ddma_terr_en")
   , cmd_ignored_en(_reg.cmd_ignored_en, name + ".cmd_ignored_en")
   , sdma_trigg_en(_reg.sdma_trigg_en, name + ".sdma_trigg_en")
   , sdma_err_en(_reg.sdma_err_en, name + ".sdma_err_en")
   , stig_done_en(_reg.stig_done_en, name + ".stig_done_en")
   , dir_crc_err_en(_reg.dir_crc_err_en, name + ".dir_crc_err_en")
   , dir_dqs_err_en(_reg.dir_dqs_err_en, name + ".dir_dqs_err_en")
   , dir_cmd_err_en(_reg.dir_cmd_err_en, name + ".dir_cmd_err_en")
   , dir_ecc_corr_err_en(_reg.dir_ecc_corr_err_en, name + ".dir_ecc_corr_err_en")
   , dir_dev_err_en(_reg.dir_dev_err_en, name + ".dir_dev_err_en")
   , intr_en(_reg.intr_en, name + ".intr_en")
  {
   disable();
   gp_open_drain_0_en.configure_default_bins(0);
   gp_open_drain_1_en.configure_default_bins(0);
   gp_open_drain_2_en.configure_default_bins(0);
   gp_open_drain_3_en.configure_default_bins(0);
   ctrl_idle_en.configure_default_bins(0);
   cdma_terr_en.configure_default_bins(0);
   ddma_terr_en.configure_default_bins(0);
   cmd_ignored_en.configure_default_bins(0);
   sdma_trigg_en.configure_default_bins(0);
   sdma_err_en.configure_default_bins(0);
   stig_done_en.configure_default_bins(0);
   dir_crc_err_en.configure_default_bins(0);
   dir_dqs_err_en.configure_default_bins(0);
   dir_cmd_err_en.configure_default_bins(0);
   dir_ecc_corr_err_en.configure_default_bins(0);
   dir_dev_err_en.configure_default_bins(0);
   intr_en.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->gp_open_drain_0_en.disable();
   this->gp_open_drain_1_en.disable();
   this->gp_open_drain_2_en.disable();
   this->gp_open_drain_3_en.disable();
   this->ctrl_idle_en.disable();
   this->cdma_terr_en.disable();
   this->ddma_terr_en.disable();
   this->cmd_ignored_en.disable();
   this->sdma_trigg_en.disable();
   this->sdma_err_en.disable();
   this->stig_done_en.disable();
   this->dir_crc_err_en.disable();
   this->dir_dqs_err_en.disable();
   this->dir_cmd_err_en.disable();
   this->dir_ecc_corr_err_en.disable();
   this->dir_dev_err_en.disable();
   this->intr_en.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->gp_open_drain_0_en.access(at);
   this->gp_open_drain_1_en.access(at);
   this->gp_open_drain_2_en.access(at);
   this->gp_open_drain_3_en.access(at);
   this->ctrl_idle_en.access(at);
   this->cdma_terr_en.access(at);
   this->ddma_terr_en.access(at);
   this->cmd_ignored_en.access(at);
   this->sdma_trigg_en.access(at);
   this->sdma_err_en.access(at);
   this->stig_done_en.access(at);
   this->dir_crc_err_en.access(at);
   this->dir_dqs_err_en.access(at);
   this->dir_cmd_err_en.access(at);
   this->dir_ecc_corr_err_en.access(at);
   this->dir_dev_err_en.access(at);
   this->intr_en.access(at);
  }
  scml2::cov::bitfield<unsigned int> gp_open_drain_0_en;
  scml2::cov::bitfield<unsigned int> gp_open_drain_1_en;
  scml2::cov::bitfield<unsigned int> gp_open_drain_2_en;
  scml2::cov::bitfield<unsigned int> gp_open_drain_3_en;
  scml2::cov::bitfield<unsigned int> ctrl_idle_en;
  scml2::cov::bitfield<unsigned int> cdma_terr_en;
  scml2::cov::bitfield<unsigned int> ddma_terr_en;
  scml2::cov::bitfield<unsigned int> cmd_ignored_en;
  scml2::cov::bitfield<unsigned int> sdma_trigg_en;
  scml2::cov::bitfield<unsigned int> sdma_err_en;
  scml2::cov::bitfield<unsigned int> stig_done_en;
  scml2::cov::bitfield<unsigned int> dir_crc_err_en;
  scml2::cov::bitfield<unsigned int> dir_dqs_err_en;
  scml2::cov::bitfield<unsigned int> dir_cmd_err_en;
  scml2::cov::bitfield<unsigned int> dir_ecc_corr_err_en;
  scml2::cov::bitfield<unsigned int> dir_dev_err_en;
  scml2::cov::bitfield<unsigned int> intr_en;
 };
  intr_enable_type intr_enable;
 struct trd_comp_intr_status_type : public scml2::cov::reg<unsigned int> 
 {
   trd_comp_intr_status_type(cdns_xspi_ctrl_reg::trd_comp_intr_status_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , trd0_comp(_reg.trd0_comp, name + ".trd0_comp")
   , trd1_comp(_reg.trd1_comp, name + ".trd1_comp")
   , trd2_comp(_reg.trd2_comp, name + ".trd2_comp")
   , trd3_comp(_reg.trd3_comp, name + ".trd3_comp")
   , trd4_comp(_reg.trd4_comp, name + ".trd4_comp")
   , trd5_comp(_reg.trd5_comp, name + ".trd5_comp")
   , trd6_comp(_reg.trd6_comp, name + ".trd6_comp")
   , trd7_comp(_reg.trd7_comp, name + ".trd7_comp")
  {
   disable();
   trd0_comp.configure_default_bins(0);
   trd1_comp.configure_default_bins(0);
   trd2_comp.configure_default_bins(0);
   trd3_comp.configure_default_bins(0);
   trd4_comp.configure_default_bins(0);
   trd5_comp.configure_default_bins(0);
   trd6_comp.configure_default_bins(0);
   trd7_comp.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->trd0_comp.disable();
   this->trd1_comp.disable();
   this->trd2_comp.disable();
   this->trd3_comp.disable();
   this->trd4_comp.disable();
   this->trd5_comp.disable();
   this->trd6_comp.disable();
   this->trd7_comp.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->trd0_comp.access(at);
   this->trd1_comp.access(at);
   this->trd2_comp.access(at);
   this->trd3_comp.access(at);
   this->trd4_comp.access(at);
   this->trd5_comp.access(at);
   this->trd6_comp.access(at);
   this->trd7_comp.access(at);
  }
  scml2::cov::bitfield<unsigned int> trd0_comp;
  scml2::cov::bitfield<unsigned int> trd1_comp;
  scml2::cov::bitfield<unsigned int> trd2_comp;
  scml2::cov::bitfield<unsigned int> trd3_comp;
  scml2::cov::bitfield<unsigned int> trd4_comp;
  scml2::cov::bitfield<unsigned int> trd5_comp;
  scml2::cov::bitfield<unsigned int> trd6_comp;
  scml2::cov::bitfield<unsigned int> trd7_comp;
 };
  trd_comp_intr_status_type trd_comp_intr_status;
 struct trd_error_intr_status_type : public scml2::cov::reg<unsigned int> 
 {
   trd_error_intr_status_type(cdns_xspi_ctrl_reg::trd_error_intr_status_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , trd0_error_stat(_reg.trd0_error_stat, name + ".trd0_error_stat")
   , trd1_error_stat(_reg.trd1_error_stat, name + ".trd1_error_stat")
   , trd2_error_stat(_reg.trd2_error_stat, name + ".trd2_error_stat")
   , trd3_error_stat(_reg.trd3_error_stat, name + ".trd3_error_stat")
   , trd4_error_stat(_reg.trd4_error_stat, name + ".trd4_error_stat")
   , trd5_error_stat(_reg.trd5_error_stat, name + ".trd5_error_stat")
   , trd6_error_stat(_reg.trd6_error_stat, name + ".trd6_error_stat")
   , trd7_error_stat(_reg.trd7_error_stat, name + ".trd7_error_stat")
  {
   disable();
   trd0_error_stat.configure_default_bins(0);
   trd1_error_stat.configure_default_bins(0);
   trd2_error_stat.configure_default_bins(0);
   trd3_error_stat.configure_default_bins(0);
   trd4_error_stat.configure_default_bins(0);
   trd5_error_stat.configure_default_bins(0);
   trd6_error_stat.configure_default_bins(0);
   trd7_error_stat.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->trd0_error_stat.disable();
   this->trd1_error_stat.disable();
   this->trd2_error_stat.disable();
   this->trd3_error_stat.disable();
   this->trd4_error_stat.disable();
   this->trd5_error_stat.disable();
   this->trd6_error_stat.disable();
   this->trd7_error_stat.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->trd0_error_stat.access(at);
   this->trd1_error_stat.access(at);
   this->trd2_error_stat.access(at);
   this->trd3_error_stat.access(at);
   this->trd4_error_stat.access(at);
   this->trd5_error_stat.access(at);
   this->trd6_error_stat.access(at);
   this->trd7_error_stat.access(at);
  }
  scml2::cov::bitfield<unsigned int> trd0_error_stat;
  scml2::cov::bitfield<unsigned int> trd1_error_stat;
  scml2::cov::bitfield<unsigned int> trd2_error_stat;
  scml2::cov::bitfield<unsigned int> trd3_error_stat;
  scml2::cov::bitfield<unsigned int> trd4_error_stat;
  scml2::cov::bitfield<unsigned int> trd5_error_stat;
  scml2::cov::bitfield<unsigned int> trd6_error_stat;
  scml2::cov::bitfield<unsigned int> trd7_error_stat;
 };
  trd_error_intr_status_type trd_error_intr_status;
 struct trd_error_intr_en_type : public scml2::cov::reg<unsigned int> 
 {
   trd_error_intr_en_type(cdns_xspi_ctrl_reg::trd_error_intr_en_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , trd_error_intr_en(_reg.trd_error_intr_en, name + ".trd_error_intr_en")
  {
   disable();
   trd_error_intr_en.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->trd_error_intr_en.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->trd_error_intr_en.access(at);
  }
  scml2::cov::bitfield<unsigned int> trd_error_intr_en;
 };
  trd_error_intr_en_type trd_error_intr_en;
 struct dma_target_error_l_type : public scml2::cov::reg<unsigned int> 
 {
   dma_target_error_l_type(cdns_xspi_ctrl_reg::dma_target_error_l_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , target_err_l(_reg.target_err_l, name + ".target_err_l")
  {
   disable();
   target_err_l.access(scml2::cov::WRITES);
   target_err_l.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->target_err_l.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->target_err_l.access(at);
  }
  scml2::cov::bitfield<unsigned int> target_err_l;
 };
  dma_target_error_l_type dma_target_error_l;
 struct dma_target_error_h_type : public scml2::cov::reg<unsigned int> 
 {
   dma_target_error_h_type(cdns_xspi_ctrl_reg::dma_target_error_h_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , target_err_h(_reg.target_err_h, name + ".target_err_h")
  {
   disable();
   target_err_h.access(scml2::cov::WRITES);
   target_err_h.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->target_err_h.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->target_err_h.access(at);
  }
  scml2::cov::bitfield<unsigned int> target_err_h;
 };
  dma_target_error_h_type dma_target_error_h;
 struct boot_status_type : public scml2::cov::reg<unsigned int> 
 {
   boot_status_type(cdns_xspi_ctrl_reg::boot_status_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , boot_dqs_err(_reg.boot_dqs_err, name + ".boot_dqs_err")
   , boot_crc_err(_reg.boot_crc_err, name + ".boot_crc_err")
   , boot_bus_err(_reg.boot_bus_err, name + ".boot_bus_err")
  {
   disable();
   boot_dqs_err.access(scml2::cov::WRITES);
   boot_dqs_err.configure_default_bins(0);
   boot_crc_err.access(scml2::cov::WRITES);
   boot_crc_err.configure_default_bins(0);
   boot_bus_err.access(scml2::cov::WRITES);
   boot_bus_err.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->boot_dqs_err.disable();
   this->boot_crc_err.disable();
   this->boot_bus_err.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->boot_dqs_err.access(at);
   this->boot_crc_err.access(at);
   this->boot_bus_err.access(at);
  }
  scml2::cov::bitfield<unsigned int> boot_dqs_err;
  scml2::cov::bitfield<unsigned int> boot_crc_err;
  scml2::cov::bitfield<unsigned int> boot_bus_err;
 };
  boot_status_type boot_status;
 scml2::cov::memory<unsigned int> ctrl_cfg_common_a;
 struct long_polling_type : public scml2::cov::reg<unsigned int> 
 {
   long_polling_type(cdns_xspi_ctrl_reg::long_polling_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , long_polling(_reg.long_polling, name + ".long_polling")
  {
   disable();
   long_polling.configure_default_bins(1000);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->long_polling.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->long_polling.access(at);
  }
  scml2::cov::bitfield<unsigned int> long_polling;
 };
  long_polling_type long_polling;
 struct short_polling_type : public scml2::cov::reg<unsigned int> 
 {
   short_polling_type(cdns_xspi_ctrl_reg::short_polling_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , short_polling(_reg.short_polling, name + ".short_polling")
  {
   disable();
   short_polling.configure_default_bins(500);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->short_polling.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->short_polling.access(at);
  }
  scml2::cov::bitfield<unsigned int> short_polling;
 };
  short_polling_type short_polling;
 struct ctrl_config_type : public scml2::cov::reg<unsigned int> 
 {
   ctrl_config_type(cdns_xspi_ctrl_reg::ctrl_config_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cont_on_err(_reg.cont_on_err, name + ".cont_on_err")
   , work_mode(_reg.work_mode, name + ".work_mode")
  {
   disable();
   cont_on_err.configure_default_bins(0);
   work_mode.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cont_on_err.disable();
   this->work_mode.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cont_on_err.access(at);
   this->work_mode.access(at);
  }
  scml2::cov::bitfield<unsigned int> cont_on_err;
  scml2::cov::bitfield<unsigned int> work_mode;
 };
  ctrl_config_type ctrl_config;
 struct dma_settings_type : public scml2::cov::reg<unsigned int> 
 {
   dma_settings_type(cdns_xspi_ctrl_reg::dma_settings_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , burst_sel(_reg.burst_sel, name + ".burst_sel")
   , OTE(_reg.OTE, name + ".OTE")
   , sdma_err_rsp(_reg.sdma_err_rsp, name + ".sdma_err_rsp")
   , word_size(_reg.word_size, name + ".word_size")
  {
   disable();
   burst_sel.configure_default_bins(0);
   OTE.configure_default_bins(1);
   sdma_err_rsp.configure_default_bins(0);
   word_size.configure_default_bins(3);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->burst_sel.disable();
   this->OTE.disable();
   this->sdma_err_rsp.disable();
   this->word_size.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->burst_sel.access(at);
   this->OTE.access(at);
   this->sdma_err_rsp.access(at);
   this->word_size.access(at);
  }
  scml2::cov::bitfield<unsigned int> burst_sel;
  scml2::cov::bitfield<unsigned int> OTE;
  scml2::cov::bitfield<unsigned int> sdma_err_rsp;
  scml2::cov::bitfield<unsigned int> word_size;
 };
  dma_settings_type dma_settings;
 struct sdma_size_type : public scml2::cov::reg<unsigned int> 
 {
   sdma_size_type(cdns_xspi_ctrl_reg::sdma_size_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , sdma_size(_reg.sdma_size, name + ".sdma_size")
  {
   disable();
   sdma_size.access(scml2::cov::WRITES);
   sdma_size.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->sdma_size.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->sdma_size.access(at);
  }
  scml2::cov::bitfield<unsigned int> sdma_size;
 };
  sdma_size_type sdma_size;
 struct sdma_trd_info_type : public scml2::cov::reg<unsigned int> 
 {
   sdma_trd_info_type(cdns_xspi_ctrl_reg::sdma_trd_info_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , sdma_trd(_reg.sdma_trd, name + ".sdma_trd")
   , sdma_dir(_reg.sdma_dir, name + ".sdma_dir")
  {
   disable();
   sdma_trd.access(scml2::cov::WRITES);
   sdma_trd.configure_default_bins(0);
   sdma_dir.access(scml2::cov::WRITES);
   sdma_dir.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->sdma_trd.disable();
   this->sdma_dir.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->sdma_trd.access(at);
   this->sdma_dir.access(at);
  }
  scml2::cov::bitfield<unsigned int> sdma_trd;
  scml2::cov::bitfield<unsigned int> sdma_dir;
 };
  sdma_trd_info_type sdma_trd_info;
 struct sdma_addr0_type : public scml2::cov::reg<unsigned int> 
 {
   sdma_addr0_type(cdns_xspi_ctrl_reg::sdma_addr0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , sdma_addr_l(_reg.sdma_addr_l, name + ".sdma_addr_l")
  {
   disable();
   sdma_addr_l.access(scml2::cov::WRITES);
   sdma_addr_l.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->sdma_addr_l.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->sdma_addr_l.access(at);
  }
  scml2::cov::bitfield<unsigned int> sdma_addr_l;
 };
  sdma_addr0_type sdma_addr0;
 struct sdma_addr1_type : public scml2::cov::reg<unsigned int> 
 {
   sdma_addr1_type(cdns_xspi_ctrl_reg::sdma_addr1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , sdma_addr_h(_reg.sdma_addr_h, name + ".sdma_addr_h")
  {
   disable();
   sdma_addr_h.access(scml2::cov::WRITES);
   sdma_addr_h.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->sdma_addr_h.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->sdma_addr_h.access(at);
  }
  scml2::cov::bitfield<unsigned int> sdma_addr_h;
 };
  sdma_addr1_type sdma_addr1;
 struct discovery_control_type : public scml2::cov::reg<unsigned int> 
 {
   discovery_control_type(cdns_xspi_ctrl_reg::discovery_control_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , discovery_req(_reg.discovery_req, name + ".discovery_req")
   , discovery_req_type(_reg.discovery_req_type, name + ".discovery_req_type")
   , discovery_comp(_reg.discovery_comp, name + ".discovery_comp")
   , discovery_fail(_reg.discovery_fail, name + ".discovery_fail")
   , discovery_inhibit(_reg.discovery_inhibit, name + ".discovery_inhibit")
   , discovery_extop_val(_reg.discovery_extop_val, name + ".discovery_extop_val")
   , discovery_extop_en(_reg.discovery_extop_en, name + ".discovery_extop_en")
   , discovery_cmd_type(_reg.discovery_cmd_type, name + ".discovery_cmd_type")
   , discovery_dummy_cnt(_reg.discovery_dummy_cnt, name + ".discovery_dummy_cnt")
   , discovery_abnum(_reg.discovery_abnum, name + ".discovery_abnum")
   , discovery_num_lines(_reg.discovery_num_lines, name + ".discovery_num_lines")
   , discovery_bank(_reg.discovery_bank, name + ".discovery_bank")
  {
   disable();
   discovery_req.configure_default_bins(0);
   discovery_req_type.configure_default_bins(0);
   discovery_comp.access(scml2::cov::WRITES);
   discovery_comp.configure_default_bins(0);
   discovery_fail.access(scml2::cov::WRITES);
   discovery_fail.configure_default_bins(0);
   discovery_inhibit.access(scml2::cov::WRITES);
   discovery_inhibit.configure_default_bins(0);
   discovery_extop_val.configure_default_bins(0);
   discovery_extop_en.configure_default_bins(0);
   discovery_cmd_type.configure_default_bins(0);
   discovery_dummy_cnt.configure_default_bins(0);
   discovery_abnum.configure_default_bins(0);
   discovery_num_lines.configure_default_bins(0);
   discovery_bank.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->discovery_req.disable();
   this->discovery_req_type.disable();
   this->discovery_comp.disable();
   this->discovery_fail.disable();
   this->discovery_inhibit.disable();
   this->discovery_extop_val.disable();
   this->discovery_extop_en.disable();
   this->discovery_cmd_type.disable();
   this->discovery_dummy_cnt.disable();
   this->discovery_abnum.disable();
   this->discovery_num_lines.disable();
   this->discovery_bank.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->discovery_req.access(at);
   this->discovery_req_type.access(at);
   this->discovery_comp.access(at);
   this->discovery_fail.access(at);
   this->discovery_inhibit.access(at);
   this->discovery_extop_val.access(at);
   this->discovery_extop_en.access(at);
   this->discovery_cmd_type.access(at);
   this->discovery_dummy_cnt.access(at);
   this->discovery_abnum.access(at);
   this->discovery_num_lines.access(at);
   this->discovery_bank.access(at);
  }
  scml2::cov::bitfield<unsigned int> discovery_req;
  scml2::cov::bitfield<unsigned int> discovery_req_type;
  scml2::cov::bitfield<unsigned int> discovery_comp;
  scml2::cov::bitfield<unsigned int> discovery_fail;
  scml2::cov::bitfield<unsigned int> discovery_inhibit;
  scml2::cov::bitfield<unsigned int> discovery_extop_val;
  scml2::cov::bitfield<unsigned int> discovery_extop_en;
  scml2::cov::bitfield<unsigned int> discovery_cmd_type;
  scml2::cov::bitfield<unsigned int> discovery_dummy_cnt;
  scml2::cov::bitfield<unsigned int> discovery_abnum;
  scml2::cov::bitfield<unsigned int> discovery_num_lines;
  scml2::cov::bitfield<unsigned int> discovery_bank;
 };
  discovery_control_type discovery_control;
 scml2::cov::memory<unsigned int> cmn_seq_regs_a;
 struct xip_mode_cfg_type : public scml2::cov::reg<unsigned int> 
 {
   xip_mode_cfg_type(cdns_xspi_ctrl_reg::xip_mode_cfg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , xip_en(_reg.xip_en, name + ".xip_en")
   , xip_en_mb_val(_reg.xip_en_mb_val, name + ".xip_en_mb_val")
   , xip_dis_mb_val(_reg.xip_dis_mb_val, name + ".xip_dis_mb_val")
  {
   disable();
   xip_en.configure_default_bins(0);
   xip_en_mb_val.configure_default_bins(0);
   xip_dis_mb_val.configure_default_bins(255);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->xip_en.disable();
   this->xip_en_mb_val.disable();
   this->xip_dis_mb_val.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->xip_en.access(at);
   this->xip_en_mb_val.access(at);
   this->xip_dis_mb_val.access(at);
  }
  scml2::cov::bitfield<unsigned int> xip_en;
  scml2::cov::bitfield<unsigned int> xip_en_mb_val;
  scml2::cov::bitfield<unsigned int> xip_dis_mb_val;
 };
  xip_mode_cfg_type xip_mode_cfg;
 struct global_seq_cfg_type : public scml2::cov::reg<unsigned int> 
 {
   global_seq_cfg_type(cdns_xspi_ctrl_reg::global_seq_cfg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , seq_page_size_rd(_reg.seq_page_size_rd, name + ".seq_page_size_rd")
   , seq_page_size_pgm(_reg.seq_page_size_pgm, name + ".seq_page_size_pgm")
   , seq_crc_en(_reg.seq_crc_en, name + ".seq_crc_en")
   , seq_crc_variant(_reg.seq_crc_variant, name + ".seq_crc_variant")
   , seq_crc_oe(_reg.seq_crc_oe, name + ".seq_crc_oe")
   , seq_crc_chunk_size(_reg.seq_crc_chunk_size, name + ".seq_crc_chunk_size")
   , seq_crc_ual_chunk_en(_reg.seq_crc_ual_chunk_en, name + ".seq_crc_ual_chunk_en")
   , seq_crc_ual_chunk_chk(_reg.seq_crc_ual_chunk_chk, name + ".seq_crc_ual_chunk_chk")
   , seq_tcms_en(_reg.seq_tcms_en, name + ".seq_tcms_en")
   , seq_data_swap(_reg.seq_data_swap, name + ".seq_data_swap")
   , seq_data_per_addr(_reg.seq_data_per_addr, name + ".seq_data_per_addr")
   , seq_type(_reg.seq_type, name + ".seq_type")
  {
   disable();
   seq_page_size_rd.configure_default_bins(15);
   seq_page_size_pgm.configure_default_bins(8);
   seq_crc_en.configure_default_bins(0);
   seq_crc_variant.configure_default_bins(0);
   seq_crc_oe.configure_default_bins(0);
   seq_crc_chunk_size.configure_default_bins(2);
   seq_crc_ual_chunk_en.configure_default_bins(0);
   seq_crc_ual_chunk_chk.configure_default_bins(0);
   seq_tcms_en.configure_default_bins(0);
   seq_data_swap.configure_default_bins(0);
   seq_data_per_addr.configure_default_bins(0);
   seq_type.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->seq_page_size_rd.disable();
   this->seq_page_size_pgm.disable();
   this->seq_crc_en.disable();
   this->seq_crc_variant.disable();
   this->seq_crc_oe.disable();
   this->seq_crc_chunk_size.disable();
   this->seq_crc_ual_chunk_en.disable();
   this->seq_crc_ual_chunk_chk.disable();
   this->seq_tcms_en.disable();
   this->seq_data_swap.disable();
   this->seq_data_per_addr.disable();
   this->seq_type.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->seq_page_size_rd.access(at);
   this->seq_page_size_pgm.access(at);
   this->seq_crc_en.access(at);
   this->seq_crc_variant.access(at);
   this->seq_crc_oe.access(at);
   this->seq_crc_chunk_size.access(at);
   this->seq_crc_ual_chunk_en.access(at);
   this->seq_crc_ual_chunk_chk.access(at);
   this->seq_tcms_en.access(at);
   this->seq_data_swap.access(at);
   this->seq_data_per_addr.access(at);
   this->seq_type.access(at);
  }
  scml2::cov::bitfield<unsigned int> seq_page_size_rd;
  scml2::cov::bitfield<unsigned int> seq_page_size_pgm;
  scml2::cov::bitfield<unsigned int> seq_crc_en;
  scml2::cov::bitfield<unsigned int> seq_crc_variant;
  scml2::cov::bitfield<unsigned int> seq_crc_oe;
  scml2::cov::bitfield<unsigned int> seq_crc_chunk_size;
  scml2::cov::bitfield<unsigned int> seq_crc_ual_chunk_en;
  scml2::cov::bitfield<unsigned int> seq_crc_ual_chunk_chk;
  scml2::cov::bitfield<unsigned int> seq_tcms_en;
  scml2::cov::bitfield<unsigned int> seq_data_swap;
  scml2::cov::bitfield<unsigned int> seq_data_per_addr;
  scml2::cov::bitfield<unsigned int> seq_type;
 };
  global_seq_cfg_type global_seq_cfg;
 struct global_seq_cfg_1_type : public scml2::cov::reg<unsigned int> 
 {
   global_seq_cfg_1_type(cdns_xspi_ctrl_reg::global_seq_cfg_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , seq_page_size_ext(_reg.seq_page_size_ext, name + ".seq_page_size_ext")
   , seq_page_ca_size(_reg.seq_page_ca_size, name + ".seq_page_ca_size")
   , seq_page_per_block(_reg.seq_page_per_block, name + ".seq_page_per_block")
   , seq_plane_cnt(_reg.seq_plane_cnt, name + ".seq_plane_cnt")
  {
   disable();
   seq_page_size_ext.configure_default_bins(0);
   seq_page_ca_size.configure_default_bins(0);
   seq_page_per_block.configure_default_bins(0);
   seq_plane_cnt.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->seq_page_size_ext.disable();
   this->seq_page_ca_size.disable();
   this->seq_page_per_block.disable();
   this->seq_plane_cnt.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->seq_page_size_ext.access(at);
   this->seq_page_ca_size.access(at);
   this->seq_page_per_block.access(at);
   this->seq_plane_cnt.access(at);
  }
  scml2::cov::bitfield<unsigned int> seq_page_size_ext;
  scml2::cov::bitfield<unsigned int> seq_page_ca_size;
  scml2::cov::bitfield<unsigned int> seq_page_per_block;
  scml2::cov::bitfield<unsigned int> seq_plane_cnt;
 };
  global_seq_cfg_1_type global_seq_cfg_1;
 struct direct_access_cfg_type : public scml2::cov::reg<unsigned int> 
 {
   direct_access_cfg_type(cdns_xspi_ctrl_reg::direct_access_cfg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , dac_bank_num(_reg.dac_bank_num, name + ".dac_bank_num")
   , rwds_cap_en(_reg.rwds_cap_en, name + ".rwds_cap_en")
   , mode_bit_xip_en(_reg.mode_bit_xip_en, name + ".mode_bit_xip_en")
   , mode_bit_xip_dis(_reg.mode_bit_xip_dis, name + ".mode_bit_xip_dis")
   , rmp_addr_en(_reg.rmp_addr_en, name + ".rmp_addr_en")
   , dac_addr_mask(_reg.dac_addr_mask, name + ".dac_addr_mask")
  {
   disable();
   dac_bank_num.configure_default_bins(0);
   rwds_cap_en.configure_default_bins(0);
   mode_bit_xip_en.configure_default_bins(0);
   mode_bit_xip_dis.configure_default_bins(0);
   rmp_addr_en.configure_default_bins(0);
   dac_addr_mask.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->dac_bank_num.disable();
   this->rwds_cap_en.disable();
   this->mode_bit_xip_en.disable();
   this->mode_bit_xip_dis.disable();
   this->rmp_addr_en.disable();
   this->dac_addr_mask.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->dac_bank_num.access(at);
   this->rwds_cap_en.access(at);
   this->mode_bit_xip_en.access(at);
   this->mode_bit_xip_dis.access(at);
   this->rmp_addr_en.access(at);
   this->dac_addr_mask.access(at);
  }
  scml2::cov::bitfield<unsigned int> dac_bank_num;
  scml2::cov::bitfield<unsigned int> rwds_cap_en;
  scml2::cov::bitfield<unsigned int> mode_bit_xip_en;
  scml2::cov::bitfield<unsigned int> mode_bit_xip_dis;
  scml2::cov::bitfield<unsigned int> rmp_addr_en;
  scml2::cov::bitfield<unsigned int> dac_addr_mask;
 };
  direct_access_cfg_type direct_access_cfg;
 struct direct_access_rmp_type : public scml2::cov::reg<unsigned int> 
 {
   direct_access_rmp_type(cdns_xspi_ctrl_reg::direct_access_rmp_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , rmp_addr_val(_reg.rmp_addr_val, name + ".rmp_addr_val")
  {
   disable();
   rmp_addr_val.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->rmp_addr_val.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->rmp_addr_val.access(at);
  }
  scml2::cov::bitfield<unsigned int> rmp_addr_val;
 };
  direct_access_rmp_type direct_access_rmp;
 struct direct_access_rmp_1_type : public scml2::cov::reg<unsigned int> 
 {
   direct_access_rmp_1_type(cdns_xspi_ctrl_reg::direct_access_rmp_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , rmp_addr_val_1(_reg.rmp_addr_val_1, name + ".rmp_addr_val_1")
  {
   disable();
   rmp_addr_val_1.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->rmp_addr_val_1.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->rmp_addr_val_1.access(at);
  }
  scml2::cov::bitfield<unsigned int> rmp_addr_val_1;
 };
  direct_access_rmp_1_type direct_access_rmp_1;
 scml2::cov::memory<unsigned int> dev_seq_regs_a;
 struct rst_seq_cfg_0_type : public scml2::cov::reg<unsigned int> 
 {
   rst_seq_cfg_0_type(cdns_xspi_ctrl_reg::rst_seq_cfg_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , rst_seq_p1_cmd0_val(_reg.rst_seq_p1_cmd0_val, name + ".rst_seq_p1_cmd0_val")
   , rst_seq_p1_cmd1_val(_reg.rst_seq_p1_cmd1_val, name + ".rst_seq_p1_cmd1_val")
   , rst_seq_p1_cmd0_en(_reg.rst_seq_p1_cmd0_en, name + ".rst_seq_p1_cmd0_en")
   , rst_seq_p1_data_ios(_reg.rst_seq_p1_data_ios, name + ".rst_seq_p1_data_ios")
   , rst_seq_p1_data_edge(_reg.rst_seq_p1_data_edge, name + ".rst_seq_p1_data_edge")
   , rst_seq_p1_data_en(_reg.rst_seq_p1_data_en, name + ".rst_seq_p1_data_en")
   , rst_seq_p1_cmd_ios(_reg.rst_seq_p1_cmd_ios, name + ".rst_seq_p1_cmd_ios")
   , rst_seq_p1_cmd_edge(_reg.rst_seq_p1_cmd_edge, name + ".rst_seq_p1_cmd_edge")
  {
   disable();
   rst_seq_p1_cmd0_val.configure_default_bins(102);
   rst_seq_p1_cmd1_val.configure_default_bins(153);
   rst_seq_p1_cmd0_en.configure_default_bins(1);
   rst_seq_p1_data_ios.configure_default_bins(0);
   rst_seq_p1_data_edge.configure_default_bins(0);
   rst_seq_p1_data_en.configure_default_bins(0);
   rst_seq_p1_cmd_ios.configure_default_bins(0);
   rst_seq_p1_cmd_edge.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->rst_seq_p1_cmd0_val.disable();
   this->rst_seq_p1_cmd1_val.disable();
   this->rst_seq_p1_cmd0_en.disable();
   this->rst_seq_p1_data_ios.disable();
   this->rst_seq_p1_data_edge.disable();
   this->rst_seq_p1_data_en.disable();
   this->rst_seq_p1_cmd_ios.disable();
   this->rst_seq_p1_cmd_edge.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->rst_seq_p1_cmd0_val.access(at);
   this->rst_seq_p1_cmd1_val.access(at);
   this->rst_seq_p1_cmd0_en.access(at);
   this->rst_seq_p1_data_ios.access(at);
   this->rst_seq_p1_data_edge.access(at);
   this->rst_seq_p1_data_en.access(at);
   this->rst_seq_p1_cmd_ios.access(at);
   this->rst_seq_p1_cmd_edge.access(at);
  }
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd0_val;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd1_val;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd0_en;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_data_ios;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_data_edge;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_data_en;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd_ios;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd_edge;
 };
  rst_seq_cfg_0_type rst_seq_cfg_0;
 struct rst_seq_cfg_1_type : public scml2::cov::reg<unsigned int> 
 {
   rst_seq_cfg_1_type(cdns_xspi_ctrl_reg::rst_seq_cfg_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , rst_seq_p1_cmd0_ext_en(_reg.rst_seq_p1_cmd0_ext_en, name + ".rst_seq_p1_cmd0_ext_en")
   , rst_seq_p1_cmd1_ext_en(_reg.rst_seq_p1_cmd1_ext_en, name + ".rst_seq_p1_cmd1_ext_en")
   , rst_seq_p1_cmd0_ext_val(_reg.rst_seq_p1_cmd0_ext_val, name + ".rst_seq_p1_cmd0_ext_val")
   , rst_seq_p1_cmd1_ext_val(_reg.rst_seq_p1_cmd1_ext_val, name + ".rst_seq_p1_cmd1_ext_val")
   , rst_seq_p1_data_val(_reg.rst_seq_p1_data_val, name + ".rst_seq_p1_data_val")
  {
   disable();
   rst_seq_p1_cmd0_ext_en.configure_default_bins(0);
   rst_seq_p1_cmd1_ext_en.configure_default_bins(0);
   rst_seq_p1_cmd0_ext_val.configure_default_bins(153);
   rst_seq_p1_cmd1_ext_val.configure_default_bins(102);
   rst_seq_p1_data_val.configure_default_bins(208);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->rst_seq_p1_cmd0_ext_en.disable();
   this->rst_seq_p1_cmd1_ext_en.disable();
   this->rst_seq_p1_cmd0_ext_val.disable();
   this->rst_seq_p1_cmd1_ext_val.disable();
   this->rst_seq_p1_data_val.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->rst_seq_p1_cmd0_ext_en.access(at);
   this->rst_seq_p1_cmd1_ext_en.access(at);
   this->rst_seq_p1_cmd0_ext_val.access(at);
   this->rst_seq_p1_cmd1_ext_val.access(at);
   this->rst_seq_p1_data_val.access(at);
  }
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd0_ext_en;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd1_ext_en;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd0_ext_val;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_cmd1_ext_val;
  scml2::cov::bitfield<unsigned int> rst_seq_p1_data_val;
 };
  rst_seq_cfg_1_type rst_seq_cfg_1;
 struct ers_seq_cfg_0_type : public scml2::cov::reg<unsigned int> 
 {
   ers_seq_cfg_0_type(cdns_xspi_ctrl_reg::ers_seq_cfg_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , erss_seq_p1_cmd_val(_reg.erss_seq_p1_cmd_val, name + ".erss_seq_p1_cmd_val")
   , erss_seq_p1_cmd_ios(_reg.erss_seq_p1_cmd_ios, name + ".erss_seq_p1_cmd_ios")
   , erss_seq_p1_cmd_edge(_reg.erss_seq_p1_cmd_edge, name + ".erss_seq_p1_cmd_edge")
   , erss_seq_p1_addr_cnt(_reg.erss_seq_p1_addr_cnt, name + ".erss_seq_p1_addr_cnt")
   , erss_seq_p1_cmd_ext_en(_reg.erss_seq_p1_cmd_ext_en, name + ".erss_seq_p1_cmd_ext_en")
   , erss_seq_p1_cmd_ext_val(_reg.erss_seq_p1_cmd_ext_val, name + ".erss_seq_p1_cmd_ext_val")
   , erss_seq_p1_addr_ios(_reg.erss_seq_p1_addr_ios, name + ".erss_seq_p1_addr_ios")
   , erss_seq_p1_addr_edge(_reg.erss_seq_p1_addr_edge, name + ".erss_seq_p1_addr_edge")
  {
   disable();
   erss_seq_p1_cmd_val.configure_default_bins(32);
   erss_seq_p1_cmd_ios.configure_default_bins(0);
   erss_seq_p1_cmd_edge.configure_default_bins(0);
   erss_seq_p1_addr_cnt.configure_default_bins(3);
   erss_seq_p1_cmd_ext_en.configure_default_bins(0);
   erss_seq_p1_cmd_ext_val.configure_default_bins(223);
   erss_seq_p1_addr_ios.configure_default_bins(0);
   erss_seq_p1_addr_edge.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->erss_seq_p1_cmd_val.disable();
   this->erss_seq_p1_cmd_ios.disable();
   this->erss_seq_p1_cmd_edge.disable();
   this->erss_seq_p1_addr_cnt.disable();
   this->erss_seq_p1_cmd_ext_en.disable();
   this->erss_seq_p1_cmd_ext_val.disable();
   this->erss_seq_p1_addr_ios.disable();
   this->erss_seq_p1_addr_edge.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->erss_seq_p1_cmd_val.access(at);
   this->erss_seq_p1_cmd_ios.access(at);
   this->erss_seq_p1_cmd_edge.access(at);
   this->erss_seq_p1_addr_cnt.access(at);
   this->erss_seq_p1_cmd_ext_en.access(at);
   this->erss_seq_p1_cmd_ext_val.access(at);
   this->erss_seq_p1_addr_ios.access(at);
   this->erss_seq_p1_addr_edge.access(at);
  }
  scml2::cov::bitfield<unsigned int> erss_seq_p1_cmd_val;
  scml2::cov::bitfield<unsigned int> erss_seq_p1_cmd_ios;
  scml2::cov::bitfield<unsigned int> erss_seq_p1_cmd_edge;
  scml2::cov::bitfield<unsigned int> erss_seq_p1_addr_cnt;
  scml2::cov::bitfield<unsigned int> erss_seq_p1_cmd_ext_en;
  scml2::cov::bitfield<unsigned int> erss_seq_p1_cmd_ext_val;
  scml2::cov::bitfield<unsigned int> erss_seq_p1_addr_ios;
  scml2::cov::bitfield<unsigned int> erss_seq_p1_addr_edge;
 };
  ers_seq_cfg_0_type ers_seq_cfg_0;
 struct ers_seq_cfg_1_type : public scml2::cov::reg<unsigned int> 
 {
   ers_seq_cfg_1_type(cdns_xspi_ctrl_reg::ers_seq_cfg_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , erss_seq_p1_sect_size(_reg.erss_seq_p1_sect_size, name + ".erss_seq_p1_sect_size")
  {
   disable();
   erss_seq_p1_sect_size.configure_default_bins(12);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->erss_seq_p1_sect_size.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->erss_seq_p1_sect_size.access(at);
  }
  scml2::cov::bitfield<unsigned int> erss_seq_p1_sect_size;
 };
  ers_seq_cfg_1_type ers_seq_cfg_1;
 struct ers_seq_cfg_2_type : public scml2::cov::reg<unsigned int> 
 {
   ers_seq_cfg_2_type(cdns_xspi_ctrl_reg::ers_seq_cfg_2_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , ersa_seq_p1_cmd_val(_reg.ersa_seq_p1_cmd_val, name + ".ersa_seq_p1_cmd_val")
   , ersa_seq_p1_cmd_ios(_reg.ersa_seq_p1_cmd_ios, name + ".ersa_seq_p1_cmd_ios")
   , ersa_seq_p1_cmd_edge(_reg.ersa_seq_p1_cmd_edge, name + ".ersa_seq_p1_cmd_edge")
   , ersa_seq_p1_cmd_ext_en(_reg.ersa_seq_p1_cmd_ext_en, name + ".ersa_seq_p1_cmd_ext_en")
   , ersa_seq_p1_cmd_ext_val(_reg.ersa_seq_p1_cmd_ext_val, name + ".ersa_seq_p1_cmd_ext_val")
  {
   disable();
   ersa_seq_p1_cmd_val.configure_default_bins(96);
   ersa_seq_p1_cmd_ios.configure_default_bins(0);
   ersa_seq_p1_cmd_edge.configure_default_bins(0);
   ersa_seq_p1_cmd_ext_en.configure_default_bins(0);
   ersa_seq_p1_cmd_ext_val.configure_default_bins(159);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->ersa_seq_p1_cmd_val.disable();
   this->ersa_seq_p1_cmd_ios.disable();
   this->ersa_seq_p1_cmd_edge.disable();
   this->ersa_seq_p1_cmd_ext_en.disable();
   this->ersa_seq_p1_cmd_ext_val.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->ersa_seq_p1_cmd_val.access(at);
   this->ersa_seq_p1_cmd_ios.access(at);
   this->ersa_seq_p1_cmd_edge.access(at);
   this->ersa_seq_p1_cmd_ext_en.access(at);
   this->ersa_seq_p1_cmd_ext_val.access(at);
  }
  scml2::cov::bitfield<unsigned int> ersa_seq_p1_cmd_val;
  scml2::cov::bitfield<unsigned int> ersa_seq_p1_cmd_ios;
  scml2::cov::bitfield<unsigned int> ersa_seq_p1_cmd_edge;
  scml2::cov::bitfield<unsigned int> ersa_seq_p1_cmd_ext_en;
  scml2::cov::bitfield<unsigned int> ersa_seq_p1_cmd_ext_val;
 };
  ers_seq_cfg_2_type ers_seq_cfg_2;
 struct prog_seq_cfg_0_type : public scml2::cov::reg<unsigned int> 
 {
   prog_seq_cfg_0_type(cdns_xspi_ctrl_reg::prog_seq_cfg_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , prog_seq_p1_cmd_val(_reg.prog_seq_p1_cmd_val, name + ".prog_seq_p1_cmd_val")
   , prog_seq_p1_cmd_ios(_reg.prog_seq_p1_cmd_ios, name + ".prog_seq_p1_cmd_ios")
   , prog_seq_p1_cmd_edge(_reg.prog_seq_p1_cmd_edge, name + ".prog_seq_p1_cmd_edge")
   , prog_seq_p1_addr_cnt(_reg.prog_seq_p1_addr_cnt, name + ".prog_seq_p1_addr_cnt")
   , prog_seq_p1_addr_ios(_reg.prog_seq_p1_addr_ios, name + ".prog_seq_p1_addr_ios")
   , prog_seq_p1_addr_edge(_reg.prog_seq_p1_addr_edge, name + ".prog_seq_p1_addr_edge")
   , prog_seq_p1_data_ios(_reg.prog_seq_p1_data_ios, name + ".prog_seq_p1_data_ios")
   , prog_seq_p1_data_edge(_reg.prog_seq_p1_data_edge, name + ".prog_seq_p1_data_edge")
   , prog_seq_p1_dummy_cnt(_reg.prog_seq_p1_dummy_cnt, name + ".prog_seq_p1_dummy_cnt")
  {
   disable();
   prog_seq_p1_cmd_val.configure_default_bins(2);
   prog_seq_p1_cmd_ios.configure_default_bins(0);
   prog_seq_p1_cmd_edge.configure_default_bins(0);
   prog_seq_p1_addr_cnt.configure_default_bins(3);
   prog_seq_p1_addr_ios.configure_default_bins(0);
   prog_seq_p1_addr_edge.configure_default_bins(0);
   prog_seq_p1_data_ios.configure_default_bins(0);
   prog_seq_p1_data_edge.configure_default_bins(0);
   prog_seq_p1_dummy_cnt.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->prog_seq_p1_cmd_val.disable();
   this->prog_seq_p1_cmd_ios.disable();
   this->prog_seq_p1_cmd_edge.disable();
   this->prog_seq_p1_addr_cnt.disable();
   this->prog_seq_p1_addr_ios.disable();
   this->prog_seq_p1_addr_edge.disable();
   this->prog_seq_p1_data_ios.disable();
   this->prog_seq_p1_data_edge.disable();
   this->prog_seq_p1_dummy_cnt.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->prog_seq_p1_cmd_val.access(at);
   this->prog_seq_p1_cmd_ios.access(at);
   this->prog_seq_p1_cmd_edge.access(at);
   this->prog_seq_p1_addr_cnt.access(at);
   this->prog_seq_p1_addr_ios.access(at);
   this->prog_seq_p1_addr_edge.access(at);
   this->prog_seq_p1_data_ios.access(at);
   this->prog_seq_p1_data_edge.access(at);
   this->prog_seq_p1_dummy_cnt.access(at);
  }
  scml2::cov::bitfield<unsigned int> prog_seq_p1_cmd_val;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_cmd_ios;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_cmd_edge;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_addr_cnt;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_addr_ios;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_addr_edge;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_data_ios;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_data_edge;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_dummy_cnt;
 };
  prog_seq_cfg_0_type prog_seq_cfg_0;
 struct prog_seq_cfg_1_type : public scml2::cov::reg<unsigned int> 
 {
   prog_seq_cfg_1_type(cdns_xspi_ctrl_reg::prog_seq_cfg_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , prog_seq_p1_cmd_ext_en(_reg.prog_seq_p1_cmd_ext_en, name + ".prog_seq_p1_cmd_ext_en")
   , prog_seq_p1_cmd_ext_val(_reg.prog_seq_p1_cmd_ext_val, name + ".prog_seq_p1_cmd_ext_val")
  {
   disable();
   prog_seq_p1_cmd_ext_en.configure_default_bins(0);
   prog_seq_p1_cmd_ext_val.configure_default_bins(253);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->prog_seq_p1_cmd_ext_en.disable();
   this->prog_seq_p1_cmd_ext_val.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->prog_seq_p1_cmd_ext_en.access(at);
   this->prog_seq_p1_cmd_ext_val.access(at);
  }
  scml2::cov::bitfield<unsigned int> prog_seq_p1_cmd_ext_en;
  scml2::cov::bitfield<unsigned int> prog_seq_p1_cmd_ext_val;
 };
  prog_seq_cfg_1_type prog_seq_cfg_1;
 struct prog_seq_cfg_2_type : public scml2::cov::reg<unsigned int> 
 {
   prog_seq_cfg_2_type(cdns_xspi_ctrl_reg::prog_seq_cfg_2_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , prog_seq_p2_target(_reg.prog_seq_p2_target, name + ".prog_seq_p2_target")
   , prog_seq_p2_burst_type(_reg.prog_seq_p2_burst_type, name + ".prog_seq_p2_burst_type")
   , prog_seq_p2_mask_cmd_mod(_reg.prog_seq_p2_mask_cmd_mod, name + ".prog_seq_p2_mask_cmd_mod")
   , prog_seq_p2_latency_cnt(_reg.prog_seq_p2_latency_cnt, name + ".prog_seq_p2_latency_cnt")
  {
   disable();
   prog_seq_p2_target.configure_default_bins(0);
   prog_seq_p2_burst_type.configure_default_bins(1);
   prog_seq_p2_mask_cmd_mod.configure_default_bins(0);
   prog_seq_p2_latency_cnt.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->prog_seq_p2_target.disable();
   this->prog_seq_p2_burst_type.disable();
   this->prog_seq_p2_mask_cmd_mod.disable();
   this->prog_seq_p2_latency_cnt.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->prog_seq_p2_target.access(at);
   this->prog_seq_p2_burst_type.access(at);
   this->prog_seq_p2_mask_cmd_mod.access(at);
   this->prog_seq_p2_latency_cnt.access(at);
  }
  scml2::cov::bitfield<unsigned int> prog_seq_p2_target;
  scml2::cov::bitfield<unsigned int> prog_seq_p2_burst_type;
  scml2::cov::bitfield<unsigned int> prog_seq_p2_mask_cmd_mod;
  scml2::cov::bitfield<unsigned int> prog_seq_p2_latency_cnt;
 };
  prog_seq_cfg_2_type prog_seq_cfg_2;
 struct read_seq_cfg_0_type : public scml2::cov::reg<unsigned int> 
 {
   read_seq_cfg_0_type(cdns_xspi_ctrl_reg::read_seq_cfg_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , read_seq_p1_cmd_val(_reg.read_seq_p1_cmd_val, name + ".read_seq_p1_cmd_val")
   , read_seq_p1_cmd_ios(_reg.read_seq_p1_cmd_ios, name + ".read_seq_p1_cmd_ios")
   , read_seq_p1_cmd_edge(_reg.read_seq_p1_cmd_edge, name + ".read_seq_p1_cmd_edge")
   , read_seq_p1_addr_cnt(_reg.read_seq_p1_addr_cnt, name + ".read_seq_p1_addr_cnt")
   , read_seq_p1_addr_ios(_reg.read_seq_p1_addr_ios, name + ".read_seq_p1_addr_ios")
   , read_seq_p1_addr_edge(_reg.read_seq_p1_addr_edge, name + ".read_seq_p1_addr_edge")
   , read_seq_p1_data_ios(_reg.read_seq_p1_data_ios, name + ".read_seq_p1_data_ios")
   , read_seq_p1_data_edge(_reg.read_seq_p1_data_edge, name + ".read_seq_p1_data_edge")
   , read_seq_p1_dummy_cnt(_reg.read_seq_p1_dummy_cnt, name + ".read_seq_p1_dummy_cnt")
  {
   disable();
   read_seq_p1_cmd_val.configure_default_bins(3);
   read_seq_p1_cmd_ios.configure_default_bins(0);
   read_seq_p1_cmd_edge.configure_default_bins(0);
   read_seq_p1_addr_cnt.configure_default_bins(3);
   read_seq_p1_addr_ios.configure_default_bins(0);
   read_seq_p1_addr_edge.configure_default_bins(0);
   read_seq_p1_data_ios.configure_default_bins(0);
   read_seq_p1_data_edge.configure_default_bins(0);
   read_seq_p1_dummy_cnt.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->read_seq_p1_cmd_val.disable();
   this->read_seq_p1_cmd_ios.disable();
   this->read_seq_p1_cmd_edge.disable();
   this->read_seq_p1_addr_cnt.disable();
   this->read_seq_p1_addr_ios.disable();
   this->read_seq_p1_addr_edge.disable();
   this->read_seq_p1_data_ios.disable();
   this->read_seq_p1_data_edge.disable();
   this->read_seq_p1_dummy_cnt.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->read_seq_p1_cmd_val.access(at);
   this->read_seq_p1_cmd_ios.access(at);
   this->read_seq_p1_cmd_edge.access(at);
   this->read_seq_p1_addr_cnt.access(at);
   this->read_seq_p1_addr_ios.access(at);
   this->read_seq_p1_addr_edge.access(at);
   this->read_seq_p1_data_ios.access(at);
   this->read_seq_p1_data_edge.access(at);
   this->read_seq_p1_dummy_cnt.access(at);
  }
  scml2::cov::bitfield<unsigned int> read_seq_p1_cmd_val;
  scml2::cov::bitfield<unsigned int> read_seq_p1_cmd_ios;
  scml2::cov::bitfield<unsigned int> read_seq_p1_cmd_edge;
  scml2::cov::bitfield<unsigned int> read_seq_p1_addr_cnt;
  scml2::cov::bitfield<unsigned int> read_seq_p1_addr_ios;
  scml2::cov::bitfield<unsigned int> read_seq_p1_addr_edge;
  scml2::cov::bitfield<unsigned int> read_seq_p1_data_ios;
  scml2::cov::bitfield<unsigned int> read_seq_p1_data_edge;
  scml2::cov::bitfield<unsigned int> read_seq_p1_dummy_cnt;
 };
  read_seq_cfg_0_type read_seq_cfg_0;
 struct read_seq_cfg_1_type : public scml2::cov::reg<unsigned int> 
 {
   read_seq_cfg_1_type(cdns_xspi_ctrl_reg::read_seq_cfg_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , read_seq_p1_cmd_ext_en(_reg.read_seq_p1_cmd_ext_en, name + ".read_seq_p1_cmd_ext_en")
   , read_seq_p1_cache_random_read_en(_reg.read_seq_p1_cache_random_read_en, name + ".read_seq_p1_cache_random_read_en")
   , read_seq_p1_cmd_ext_val(_reg.read_seq_p1_cmd_ext_val, name + ".read_seq_p1_cmd_ext_val")
   , read_seq_p1_mb_dummy_cnt(_reg.read_seq_p1_mb_dummy_cnt, name + ".read_seq_p1_mb_dummy_cnt")
   , read_seq_p1_mb_en(_reg.read_seq_p1_mb_en, name + ".read_seq_p1_mb_en")
  {
   disable();
   read_seq_p1_cmd_ext_en.configure_default_bins(0);
   read_seq_p1_cache_random_read_en.configure_default_bins(0);
   read_seq_p1_cmd_ext_val.configure_default_bins(252);
   read_seq_p1_mb_dummy_cnt.configure_default_bins(0);
   read_seq_p1_mb_en.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->read_seq_p1_cmd_ext_en.disable();
   this->read_seq_p1_cache_random_read_en.disable();
   this->read_seq_p1_cmd_ext_val.disable();
   this->read_seq_p1_mb_dummy_cnt.disable();
   this->read_seq_p1_mb_en.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->read_seq_p1_cmd_ext_en.access(at);
   this->read_seq_p1_cache_random_read_en.access(at);
   this->read_seq_p1_cmd_ext_val.access(at);
   this->read_seq_p1_mb_dummy_cnt.access(at);
   this->read_seq_p1_mb_en.access(at);
  }
  scml2::cov::bitfield<unsigned int> read_seq_p1_cmd_ext_en;
  scml2::cov::bitfield<unsigned int> read_seq_p1_cache_random_read_en;
  scml2::cov::bitfield<unsigned int> read_seq_p1_cmd_ext_val;
  scml2::cov::bitfield<unsigned int> read_seq_p1_mb_dummy_cnt;
  scml2::cov::bitfield<unsigned int> read_seq_p1_mb_en;
 };
  read_seq_cfg_1_type read_seq_cfg_1;
 struct read_seq_cfg_2_type : public scml2::cov::reg<unsigned int> 
 {
   read_seq_cfg_2_type(cdns_xspi_ctrl_reg::read_seq_cfg_2_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , read_seq_p2_target(_reg.read_seq_p2_target, name + ".read_seq_p2_target")
   , read_seq_p2_burst_type(_reg.read_seq_p2_burst_type, name + ".read_seq_p2_burst_type")
   , read_seq_p2_mask_cmd_mod(_reg.read_seq_p2_mask_cmd_mod, name + ".read_seq_p2_mask_cmd_mod")
   , read_seq_p2_hf_bound_en(_reg.read_seq_p2_hf_bound_en, name + ".read_seq_p2_hf_bound_en")
   , read_seq_p2_latency_cnt(_reg.read_seq_p2_latency_cnt, name + ".read_seq_p2_latency_cnt")
  {
   disable();
   read_seq_p2_target.configure_default_bins(0);
   read_seq_p2_burst_type.configure_default_bins(1);
   read_seq_p2_mask_cmd_mod.configure_default_bins(0);
   read_seq_p2_hf_bound_en.configure_default_bins(1);
   read_seq_p2_latency_cnt.configure_default_bins(15);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->read_seq_p2_target.disable();
   this->read_seq_p2_burst_type.disable();
   this->read_seq_p2_mask_cmd_mod.disable();
   this->read_seq_p2_hf_bound_en.disable();
   this->read_seq_p2_latency_cnt.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->read_seq_p2_target.access(at);
   this->read_seq_p2_burst_type.access(at);
   this->read_seq_p2_mask_cmd_mod.access(at);
   this->read_seq_p2_hf_bound_en.access(at);
   this->read_seq_p2_latency_cnt.access(at);
  }
  scml2::cov::bitfield<unsigned int> read_seq_p2_target;
  scml2::cov::bitfield<unsigned int> read_seq_p2_burst_type;
  scml2::cov::bitfield<unsigned int> read_seq_p2_mask_cmd_mod;
  scml2::cov::bitfield<unsigned int> read_seq_p2_hf_bound_en;
  scml2::cov::bitfield<unsigned int> read_seq_p2_latency_cnt;
 };
  read_seq_cfg_2_type read_seq_cfg_2;
 struct we_seq_cfg_0_type : public scml2::cov::reg<unsigned int> 
 {
   we_seq_cfg_0_type(cdns_xspi_ctrl_reg::we_seq_cfg_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , we_seq_p1_cmd_val(_reg.we_seq_p1_cmd_val, name + ".we_seq_p1_cmd_val")
   , we_seq_p1_cmd_ios(_reg.we_seq_p1_cmd_ios, name + ".we_seq_p1_cmd_ios")
   , we_seq_p1_cmd_edge(_reg.we_seq_p1_cmd_edge, name + ".we_seq_p1_cmd_edge")
   , we_seq_p1_cmd_ext_en(_reg.we_seq_p1_cmd_ext_en, name + ".we_seq_p1_cmd_ext_en")
   , we_seq_p1_cmd_ext_val(_reg.we_seq_p1_cmd_ext_val, name + ".we_seq_p1_cmd_ext_val")
   , we_seq_p1_en(_reg.we_seq_p1_en, name + ".we_seq_p1_en")
  {
   disable();
   we_seq_p1_cmd_val.configure_default_bins(6);
   we_seq_p1_cmd_ios.configure_default_bins(0);
   we_seq_p1_cmd_edge.configure_default_bins(0);
   we_seq_p1_cmd_ext_en.configure_default_bins(0);
   we_seq_p1_cmd_ext_val.configure_default_bins(249);
   we_seq_p1_en.configure_default_bins(1);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->we_seq_p1_cmd_val.disable();
   this->we_seq_p1_cmd_ios.disable();
   this->we_seq_p1_cmd_edge.disable();
   this->we_seq_p1_cmd_ext_en.disable();
   this->we_seq_p1_cmd_ext_val.disable();
   this->we_seq_p1_en.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->we_seq_p1_cmd_val.access(at);
   this->we_seq_p1_cmd_ios.access(at);
   this->we_seq_p1_cmd_edge.access(at);
   this->we_seq_p1_cmd_ext_en.access(at);
   this->we_seq_p1_cmd_ext_val.access(at);
   this->we_seq_p1_en.access(at);
  }
  scml2::cov::bitfield<unsigned int> we_seq_p1_cmd_val;
  scml2::cov::bitfield<unsigned int> we_seq_p1_cmd_ios;
  scml2::cov::bitfield<unsigned int> we_seq_p1_cmd_edge;
  scml2::cov::bitfield<unsigned int> we_seq_p1_cmd_ext_en;
  scml2::cov::bitfield<unsigned int> we_seq_p1_cmd_ext_val;
  scml2::cov::bitfield<unsigned int> we_seq_p1_en;
 };
  we_seq_cfg_0_type we_seq_cfg_0;
 struct stat_seq_cfg_0_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_0_type(cdns_xspi_ctrl_reg::stat_seq_cfg_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_p1_cmd_ios(_reg.stat_seq_p1_cmd_ios, name + ".stat_seq_p1_cmd_ios")
   , stat_seq_p1_cmd_edge(_reg.stat_seq_p1_cmd_edge, name + ".stat_seq_p1_cmd_edge")
   , stat_seq_p1_cmd_ext_en(_reg.stat_seq_p1_cmd_ext_en, name + ".stat_seq_p1_cmd_ext_en")
   , stat_seq_p1_addr_cnt(_reg.stat_seq_p1_addr_cnt, name + ".stat_seq_p1_addr_cnt")
   , stat_seq_p1_addr_ios(_reg.stat_seq_p1_addr_ios, name + ".stat_seq_p1_addr_ios")
   , stat_seq_p1_addr_edge(_reg.stat_seq_p1_addr_edge, name + ".stat_seq_p1_addr_edge")
   , stat_seq_p1_data_ios(_reg.stat_seq_p1_data_ios, name + ".stat_seq_p1_data_ios")
   , stat_seq_p1_data_edge(_reg.stat_seq_p1_data_edge, name + ".stat_seq_p1_data_edge")
  {
   disable();
   stat_seq_p1_cmd_ios.configure_default_bins(0);
   stat_seq_p1_cmd_edge.configure_default_bins(0);
   stat_seq_p1_cmd_ext_en.configure_default_bins(0);
   stat_seq_p1_addr_cnt.configure_default_bins(0);
   stat_seq_p1_addr_ios.configure_default_bins(0);
   stat_seq_p1_addr_edge.configure_default_bins(0);
   stat_seq_p1_data_ios.configure_default_bins(0);
   stat_seq_p1_data_edge.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_p1_cmd_ios.disable();
   this->stat_seq_p1_cmd_edge.disable();
   this->stat_seq_p1_cmd_ext_en.disable();
   this->stat_seq_p1_addr_cnt.disable();
   this->stat_seq_p1_addr_ios.disable();
   this->stat_seq_p1_addr_edge.disable();
   this->stat_seq_p1_data_ios.disable();
   this->stat_seq_p1_data_edge.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_p1_cmd_ios.access(at);
   this->stat_seq_p1_cmd_edge.access(at);
   this->stat_seq_p1_cmd_ext_en.access(at);
   this->stat_seq_p1_addr_cnt.access(at);
   this->stat_seq_p1_addr_ios.access(at);
   this->stat_seq_p1_addr_edge.access(at);
   this->stat_seq_p1_data_ios.access(at);
   this->stat_seq_p1_data_edge.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_p1_cmd_ios;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_cmd_edge;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_cmd_ext_en;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_addr_cnt;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_addr_ios;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_addr_edge;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_data_ios;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_data_edge;
 };
  stat_seq_cfg_0_type stat_seq_cfg_0;
 struct stat_seq_cfg_1_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_1_type(cdns_xspi_ctrl_reg::stat_seq_cfg_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_p1_dev_rdy_dummy_cnt(_reg.stat_seq_p1_dev_rdy_dummy_cnt, name + ".stat_seq_p1_dev_rdy_dummy_cnt")
   , stat_seq_p1_dev_rdy_addr_en(_reg.stat_seq_p1_dev_rdy_addr_en, name + ".stat_seq_p1_dev_rdy_addr_en")
   , stat_seq_p1_prog_fail_dummy_cnt(_reg.stat_seq_p1_prog_fail_dummy_cnt, name + ".stat_seq_p1_prog_fail_dummy_cnt")
   , stat_seq_p1_prog_fail_addr_en(_reg.stat_seq_p1_prog_fail_addr_en, name + ".stat_seq_p1_prog_fail_addr_en")
   , stat_seq_p1_ers_fail_dummy_cnt(_reg.stat_seq_p1_ers_fail_dummy_cnt, name + ".stat_seq_p1_ers_fail_dummy_cnt")
   , stat_seq_p1_ers_fail_addr_en(_reg.stat_seq_p1_ers_fail_addr_en, name + ".stat_seq_p1_ers_fail_addr_en")
  {
   disable();
   stat_seq_p1_dev_rdy_dummy_cnt.configure_default_bins(0);
   stat_seq_p1_dev_rdy_addr_en.configure_default_bins(0);
   stat_seq_p1_prog_fail_dummy_cnt.configure_default_bins(0);
   stat_seq_p1_prog_fail_addr_en.configure_default_bins(0);
   stat_seq_p1_ers_fail_dummy_cnt.configure_default_bins(0);
   stat_seq_p1_ers_fail_addr_en.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_p1_dev_rdy_dummy_cnt.disable();
   this->stat_seq_p1_dev_rdy_addr_en.disable();
   this->stat_seq_p1_prog_fail_dummy_cnt.disable();
   this->stat_seq_p1_prog_fail_addr_en.disable();
   this->stat_seq_p1_ers_fail_dummy_cnt.disable();
   this->stat_seq_p1_ers_fail_addr_en.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_p1_dev_rdy_dummy_cnt.access(at);
   this->stat_seq_p1_dev_rdy_addr_en.access(at);
   this->stat_seq_p1_prog_fail_dummy_cnt.access(at);
   this->stat_seq_p1_prog_fail_addr_en.access(at);
   this->stat_seq_p1_ers_fail_dummy_cnt.access(at);
   this->stat_seq_p1_ers_fail_addr_en.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_p1_dev_rdy_dummy_cnt;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_dev_rdy_addr_en;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_prog_fail_dummy_cnt;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_prog_fail_addr_en;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_ers_fail_dummy_cnt;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_ers_fail_addr_en;
 };
  stat_seq_cfg_1_type stat_seq_cfg_1;
 struct stat_seq_cfg_2_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_2_type(cdns_xspi_ctrl_reg::stat_seq_cfg_2_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_p1_dev_rdy_cmd_val(_reg.stat_seq_p1_dev_rdy_cmd_val, name + ".stat_seq_p1_dev_rdy_cmd_val")
   , stat_seq_p1_ers_fail_cmd_val(_reg.stat_seq_p1_ers_fail_cmd_val, name + ".stat_seq_p1_ers_fail_cmd_val")
   , stat_seq_p1_prog_fail_cmd_val(_reg.stat_seq_p1_prog_fail_cmd_val, name + ".stat_seq_p1_prog_fail_cmd_val")
  {
   disable();
   stat_seq_p1_dev_rdy_cmd_val.configure_default_bins(5);
   stat_seq_p1_ers_fail_cmd_val.configure_default_bins(5);
   stat_seq_p1_prog_fail_cmd_val.configure_default_bins(5);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_p1_dev_rdy_cmd_val.disable();
   this->stat_seq_p1_ers_fail_cmd_val.disable();
   this->stat_seq_p1_prog_fail_cmd_val.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_p1_dev_rdy_cmd_val.access(at);
   this->stat_seq_p1_ers_fail_cmd_val.access(at);
   this->stat_seq_p1_prog_fail_cmd_val.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_p1_dev_rdy_cmd_val;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_ers_fail_cmd_val;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_prog_fail_cmd_val;
 };
  stat_seq_cfg_2_type stat_seq_cfg_2;
 struct stat_seq_cfg_3_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_3_type(cdns_xspi_ctrl_reg::stat_seq_cfg_3_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_p1_dev_rdy_cmd_ext_val(_reg.stat_seq_p1_dev_rdy_cmd_ext_val, name + ".stat_seq_p1_dev_rdy_cmd_ext_val")
   , stat_seq_p1_ers_fail_cmd_ext_val(_reg.stat_seq_p1_ers_fail_cmd_ext_val, name + ".stat_seq_p1_ers_fail_cmd_ext_val")
   , stat_seq_p1_prog_fail_cmd_ext_val(_reg.stat_seq_p1_prog_fail_cmd_ext_val, name + ".stat_seq_p1_prog_fail_cmd_ext_val")
  {
   disable();
   stat_seq_p1_dev_rdy_cmd_ext_val.configure_default_bins(250);
   stat_seq_p1_ers_fail_cmd_ext_val.configure_default_bins(250);
   stat_seq_p1_prog_fail_cmd_ext_val.configure_default_bins(250);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_p1_dev_rdy_cmd_ext_val.disable();
   this->stat_seq_p1_ers_fail_cmd_ext_val.disable();
   this->stat_seq_p1_prog_fail_cmd_ext_val.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_p1_dev_rdy_cmd_ext_val.access(at);
   this->stat_seq_p1_ers_fail_cmd_ext_val.access(at);
   this->stat_seq_p1_prog_fail_cmd_ext_val.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_p1_dev_rdy_cmd_ext_val;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_ers_fail_cmd_ext_val;
  scml2::cov::bitfield<unsigned int> stat_seq_p1_prog_fail_cmd_ext_val;
 };
  stat_seq_cfg_3_type stat_seq_cfg_3;
 struct stat_seq_cfg_4_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_4_type(cdns_xspi_ctrl_reg::stat_seq_cfg_4_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_p2_mask_cmd_mod(_reg.stat_seq_p2_mask_cmd_mod, name + ".stat_seq_p2_mask_cmd_mod")
   , stat_seq_p2_latency_cnt(_reg.stat_seq_p2_latency_cnt, name + ".stat_seq_p2_latency_cnt")
  {
   disable();
   stat_seq_p2_mask_cmd_mod.configure_default_bins(0);
   stat_seq_p2_latency_cnt.configure_default_bins(15);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_p2_mask_cmd_mod.disable();
   this->stat_seq_p2_latency_cnt.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_p2_mask_cmd_mod.access(at);
   this->stat_seq_p2_latency_cnt.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_p2_mask_cmd_mod;
  scml2::cov::bitfield<unsigned int> stat_seq_p2_latency_cnt;
 };
  stat_seq_cfg_4_type stat_seq_cfg_4;
 struct stat_seq_cfg_5_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_5_type(cdns_xspi_ctrl_reg::stat_seq_cfg_5_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_dev_rdy_idx(_reg.stat_seq_dev_rdy_idx, name + ".stat_seq_dev_rdy_idx")
   , stat_seq_dev_rdy_val(_reg.stat_seq_dev_rdy_val, name + ".stat_seq_dev_rdy_val")
   , stat_seq_dev_rdy_size(_reg.stat_seq_dev_rdy_size, name + ".stat_seq_dev_rdy_size")
   , stat_seq_dev_rdy_en(_reg.stat_seq_dev_rdy_en, name + ".stat_seq_dev_rdy_en")
   , stat_seq_ers_fail_idx(_reg.stat_seq_ers_fail_idx, name + ".stat_seq_ers_fail_idx")
   , stat_seq_ers_fail_val(_reg.stat_seq_ers_fail_val, name + ".stat_seq_ers_fail_val")
   , stat_seq_ers_fail_size(_reg.stat_seq_ers_fail_size, name + ".stat_seq_ers_fail_size")
   , stat_seq_ers_fail_en(_reg.stat_seq_ers_fail_en, name + ".stat_seq_ers_fail_en")
   , stat_seq_prog_fail_idx(_reg.stat_seq_prog_fail_idx, name + ".stat_seq_prog_fail_idx")
   , stat_seq_prog_fail_val(_reg.stat_seq_prog_fail_val, name + ".stat_seq_prog_fail_val")
   , stat_seq_prog_fail_size(_reg.stat_seq_prog_fail_size, name + ".stat_seq_prog_fail_size")
   , stat_seq_prog_fail_en(_reg.stat_seq_prog_fail_en, name + ".stat_seq_prog_fail_en")
  {
   disable();
   stat_seq_dev_rdy_idx.configure_default_bins(0);
   stat_seq_dev_rdy_val.configure_default_bins(0);
   stat_seq_dev_rdy_size.configure_default_bins(0);
   stat_seq_dev_rdy_en.configure_default_bins(1);
   stat_seq_ers_fail_idx.configure_default_bins(0);
   stat_seq_ers_fail_val.configure_default_bins(0);
   stat_seq_ers_fail_size.configure_default_bins(0);
   stat_seq_ers_fail_en.configure_default_bins(0);
   stat_seq_prog_fail_idx.configure_default_bins(0);
   stat_seq_prog_fail_val.configure_default_bins(0);
   stat_seq_prog_fail_size.configure_default_bins(0);
   stat_seq_prog_fail_en.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_dev_rdy_idx.disable();
   this->stat_seq_dev_rdy_val.disable();
   this->stat_seq_dev_rdy_size.disable();
   this->stat_seq_dev_rdy_en.disable();
   this->stat_seq_ers_fail_idx.disable();
   this->stat_seq_ers_fail_val.disable();
   this->stat_seq_ers_fail_size.disable();
   this->stat_seq_ers_fail_en.disable();
   this->stat_seq_prog_fail_idx.disable();
   this->stat_seq_prog_fail_val.disable();
   this->stat_seq_prog_fail_size.disable();
   this->stat_seq_prog_fail_en.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_dev_rdy_idx.access(at);
   this->stat_seq_dev_rdy_val.access(at);
   this->stat_seq_dev_rdy_size.access(at);
   this->stat_seq_dev_rdy_en.access(at);
   this->stat_seq_ers_fail_idx.access(at);
   this->stat_seq_ers_fail_val.access(at);
   this->stat_seq_ers_fail_size.access(at);
   this->stat_seq_ers_fail_en.access(at);
   this->stat_seq_prog_fail_idx.access(at);
   this->stat_seq_prog_fail_val.access(at);
   this->stat_seq_prog_fail_size.access(at);
   this->stat_seq_prog_fail_en.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_dev_rdy_idx;
  scml2::cov::bitfield<unsigned int> stat_seq_dev_rdy_val;
  scml2::cov::bitfield<unsigned int> stat_seq_dev_rdy_size;
  scml2::cov::bitfield<unsigned int> stat_seq_dev_rdy_en;
  scml2::cov::bitfield<unsigned int> stat_seq_ers_fail_idx;
  scml2::cov::bitfield<unsigned int> stat_seq_ers_fail_val;
  scml2::cov::bitfield<unsigned int> stat_seq_ers_fail_size;
  scml2::cov::bitfield<unsigned int> stat_seq_ers_fail_en;
  scml2::cov::bitfield<unsigned int> stat_seq_prog_fail_idx;
  scml2::cov::bitfield<unsigned int> stat_seq_prog_fail_val;
  scml2::cov::bitfield<unsigned int> stat_seq_prog_fail_size;
  scml2::cov::bitfield<unsigned int> stat_seq_prog_fail_en;
 };
  stat_seq_cfg_5_type stat_seq_cfg_5;
 struct stat_seq_cfg_7_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_7_type(cdns_xspi_ctrl_reg::stat_seq_cfg_7_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_dev_rdy_addr(_reg.stat_seq_dev_rdy_addr, name + ".stat_seq_dev_rdy_addr")
  {
   disable();
   stat_seq_dev_rdy_addr.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_dev_rdy_addr.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_dev_rdy_addr.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_dev_rdy_addr;
 };
  stat_seq_cfg_7_type stat_seq_cfg_7;
 struct stat_seq_cfg_8_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_8_type(cdns_xspi_ctrl_reg::stat_seq_cfg_8_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_prog_fail_addr(_reg.stat_seq_prog_fail_addr, name + ".stat_seq_prog_fail_addr")
  {
   disable();
   stat_seq_prog_fail_addr.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_prog_fail_addr.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_prog_fail_addr.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_prog_fail_addr;
 };
  stat_seq_cfg_8_type stat_seq_cfg_8;
 struct stat_seq_cfg_9_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_9_type(cdns_xspi_ctrl_reg::stat_seq_cfg_9_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_ers_fail_addr(_reg.stat_seq_ers_fail_addr, name + ".stat_seq_ers_fail_addr")
  {
   disable();
   stat_seq_ers_fail_addr.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_ers_fail_addr.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_ers_fail_addr.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_ers_fail_addr;
 };
  stat_seq_cfg_9_type stat_seq_cfg_9;
 struct stat_seq_cfg_10_type : public scml2::cov::reg<unsigned int> 
 {
   stat_seq_cfg_10_type(cdns_xspi_ctrl_reg::stat_seq_cfg_10_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , stat_seq_ecc_fail_mask(_reg.stat_seq_ecc_fail_mask, name + ".stat_seq_ecc_fail_mask")
   , stat_seq_ecc_fail_val(_reg.stat_seq_ecc_fail_val, name + ".stat_seq_ecc_fail_val")
   , stat_seq_ecc_corr_val(_reg.stat_seq_ecc_corr_val, name + ".stat_seq_ecc_corr_val")
   , stat_seq_crdy_idx(_reg.stat_seq_crdy_idx, name + ".stat_seq_crdy_idx")
   , stat_seq_crdy_val(_reg.stat_seq_crdy_val, name + ".stat_seq_crdy_val")
   , stat_seq_ecc_fail_en(_reg.stat_seq_ecc_fail_en, name + ".stat_seq_ecc_fail_en")
  {
   disable();
   stat_seq_ecc_fail_mask.configure_default_bins(0);
   stat_seq_ecc_fail_val.configure_default_bins(0);
   stat_seq_ecc_corr_val.configure_default_bins(0);
   stat_seq_crdy_idx.configure_default_bins(0);
   stat_seq_crdy_val.configure_default_bins(0);
   stat_seq_ecc_fail_en.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->stat_seq_ecc_fail_mask.disable();
   this->stat_seq_ecc_fail_val.disable();
   this->stat_seq_ecc_corr_val.disable();
   this->stat_seq_crdy_idx.disable();
   this->stat_seq_crdy_val.disable();
   this->stat_seq_ecc_fail_en.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->stat_seq_ecc_fail_mask.access(at);
   this->stat_seq_ecc_fail_val.access(at);
   this->stat_seq_ecc_corr_val.access(at);
   this->stat_seq_crdy_idx.access(at);
   this->stat_seq_crdy_val.access(at);
   this->stat_seq_ecc_fail_en.access(at);
  }
  scml2::cov::bitfield<unsigned int> stat_seq_ecc_fail_mask;
  scml2::cov::bitfield<unsigned int> stat_seq_ecc_fail_val;
  scml2::cov::bitfield<unsigned int> stat_seq_ecc_corr_val;
  scml2::cov::bitfield<unsigned int> stat_seq_crdy_idx;
  scml2::cov::bitfield<unsigned int> stat_seq_crdy_val;
  scml2::cov::bitfield<unsigned int> stat_seq_ecc_fail_en;
 };
  stat_seq_cfg_10_type stat_seq_cfg_10;
 scml2::cov::memory<unsigned int> ctrl_consts_a;
 struct xspi_ctrl_version_type : public scml2::cov::reg<unsigned int> 
 {
   xspi_ctrl_version_type(cdns_xspi_ctrl_reg::xspi_ctrl_version_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , xspi_ctrl_rev(_reg.xspi_ctrl_rev, name + ".xspi_ctrl_rev")
   , xspi_ctrl_fix(_reg.xspi_ctrl_fix, name + ".xspi_ctrl_fix")
   , xspi_ctrl_magic_number(_reg.xspi_ctrl_magic_number, name + ".xspi_ctrl_magic_number")
  {
   disable();
   xspi_ctrl_rev.access(scml2::cov::WRITES);
   xspi_ctrl_rev.configure_default_bins(6);
   xspi_ctrl_fix.access(scml2::cov::WRITES);
   xspi_ctrl_fix.configure_default_bins(2);
   xspi_ctrl_magic_number.access(scml2::cov::WRITES);
   xspi_ctrl_magic_number.configure_default_bins(25890);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->xspi_ctrl_rev.disable();
   this->xspi_ctrl_fix.disable();
   this->xspi_ctrl_magic_number.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->xspi_ctrl_rev.access(at);
   this->xspi_ctrl_fix.access(at);
   this->xspi_ctrl_magic_number.access(at);
  }
  scml2::cov::bitfield<unsigned int> xspi_ctrl_rev;
  scml2::cov::bitfield<unsigned int> xspi_ctrl_fix;
  scml2::cov::bitfield<unsigned int> xspi_ctrl_magic_number;
 };
  xspi_ctrl_version_type xspi_ctrl_version;
 struct ctrl_features_reg_type : public scml2::cov::reg<unsigned int> 
 {
   ctrl_features_reg_type(cdns_xspi_ctrl_reg::ctrl_features_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , n_threads(_reg.n_threads, name + ".n_threads")
   , asf_available(_reg.asf_available, name + ".asf_available")
   , boot_available(_reg.boot_available, name + ".boot_available")
   , dma_intf(_reg.dma_intf, name + ".dma_intf")
   , dma_addr_width(_reg.dma_addr_width, name + ".dma_addr_width")
   , dma_data_width(_reg.dma_data_width, name + ".dma_data_width")
   , sfr_intf(_reg.sfr_intf, name + ".sfr_intf")
   , n_banks(_reg.n_banks, name + ".n_banks")
  {
   disable();
   n_threads.access(scml2::cov::WRITES);
   n_threads.configure_default_bins(3);
   asf_available.access(scml2::cov::WRITES);
   asf_available.configure_default_bins(0);
   boot_available.access(scml2::cov::WRITES);
   boot_available.configure_default_bins(1);
   dma_intf.access(scml2::cov::WRITES);
   dma_intf.configure_default_bins(0);
   dma_addr_width.access(scml2::cov::WRITES);
   dma_addr_width.configure_default_bins(1);
   dma_data_width.access(scml2::cov::WRITES);
   dma_data_width.configure_default_bins(1);
   sfr_intf.access(scml2::cov::WRITES);
   sfr_intf.configure_default_bins(1);
   n_banks.access(scml2::cov::WRITES);
   n_banks.configure_default_bins(3);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->n_threads.disable();
   this->asf_available.disable();
   this->boot_available.disable();
   this->dma_intf.disable();
   this->dma_addr_width.disable();
   this->dma_data_width.disable();
   this->sfr_intf.disable();
   this->n_banks.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->n_threads.access(at);
   this->asf_available.access(at);
   this->boot_available.access(at);
   this->dma_intf.access(at);
   this->dma_addr_width.access(at);
   this->dma_data_width.access(at);
   this->sfr_intf.access(at);
   this->n_banks.access(at);
  }
  scml2::cov::bitfield<unsigned int> n_threads;
  scml2::cov::bitfield<unsigned int> asf_available;
  scml2::cov::bitfield<unsigned int> boot_available;
  scml2::cov::bitfield<unsigned int> dma_intf;
  scml2::cov::bitfield<unsigned int> dma_addr_width;
  scml2::cov::bitfield<unsigned int> dma_data_width;
  scml2::cov::bitfield<unsigned int> sfr_intf;
  scml2::cov::bitfield<unsigned int> n_banks;
 };
  ctrl_features_reg_type ctrl_features_reg;
 scml2::cov::memory<unsigned int> rf_minictrl_regs_a;
 struct wp_settings_type : public scml2::cov::reg<unsigned int> 
 {
   wp_settings_type(cdns_xspi_ctrl_reg::wp_settings_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , wp(_reg.wp, name + ".wp")
   , wp_enable(_reg.wp_enable, name + ".wp_enable")
  {
   disable();
   wp.configure_default_bins(1);
   wp_enable.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->wp.disable();
   this->wp_enable.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->wp.access(at);
   this->wp_enable.access(at);
  }
  scml2::cov::bitfield<unsigned int> wp;
  scml2::cov::bitfield<unsigned int> wp_enable;
 };
  wp_settings_type wp_settings;
 struct reset_pin_settings_type : public scml2::cov::reg<unsigned int> 
 {
   reset_pin_settings_type(cdns_xspi_ctrl_reg::reset_pin_settings_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , sw_ctrled_hw_rst(_reg.sw_ctrled_hw_rst, name + ".sw_ctrled_hw_rst")
   , rst_dq3_enable(_reg.rst_dq3_enable, name + ".rst_dq3_enable")
   , sw_ctrled_hw_rst_option(_reg.sw_ctrled_hw_rst_option, name + ".sw_ctrled_hw_rst_option")
   , sw_ctrled_hw_rst_bank0(_reg.sw_ctrled_hw_rst_bank0, name + ".sw_ctrled_hw_rst_bank0")
   , sw_ctrled_hw_rst_bank1(_reg.sw_ctrled_hw_rst_bank1, name + ".sw_ctrled_hw_rst_bank1")
   , sw_ctrled_hw_rst_bank2(_reg.sw_ctrled_hw_rst_bank2, name + ".sw_ctrled_hw_rst_bank2")
   , sw_ctrled_hw_rst_bank3(_reg.sw_ctrled_hw_rst_bank3, name + ".sw_ctrled_hw_rst_bank3")
   , sw_ctrled_hw_rst_bank4(_reg.sw_ctrled_hw_rst_bank4, name + ".sw_ctrled_hw_rst_bank4")
   , sw_ctrled_hw_rst_bank5(_reg.sw_ctrled_hw_rst_bank5, name + ".sw_ctrled_hw_rst_bank5")
   , sw_ctrled_hw_rst_bank6(_reg.sw_ctrled_hw_rst_bank6, name + ".sw_ctrled_hw_rst_bank6")
   , sw_ctrled_hw_rst_bank7(_reg.sw_ctrled_hw_rst_bank7, name + ".sw_ctrled_hw_rst_bank7")
  {
   disable();
   sw_ctrled_hw_rst.configure_default_bins(1);
   rst_dq3_enable.configure_default_bins(0);
   sw_ctrled_hw_rst_option.configure_default_bins(0);
   sw_ctrled_hw_rst_bank0.configure_default_bins(0);
   sw_ctrled_hw_rst_bank1.configure_default_bins(0);
   sw_ctrled_hw_rst_bank2.configure_default_bins(0);
   sw_ctrled_hw_rst_bank3.configure_default_bins(0);
   sw_ctrled_hw_rst_bank4.configure_default_bins(0);
   sw_ctrled_hw_rst_bank5.configure_default_bins(0);
   sw_ctrled_hw_rst_bank6.configure_default_bins(0);
   sw_ctrled_hw_rst_bank7.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->sw_ctrled_hw_rst.disable();
   this->rst_dq3_enable.disable();
   this->sw_ctrled_hw_rst_option.disable();
   this->sw_ctrled_hw_rst_bank0.disable();
   this->sw_ctrled_hw_rst_bank1.disable();
   this->sw_ctrled_hw_rst_bank2.disable();
   this->sw_ctrled_hw_rst_bank3.disable();
   this->sw_ctrled_hw_rst_bank4.disable();
   this->sw_ctrled_hw_rst_bank5.disable();
   this->sw_ctrled_hw_rst_bank6.disable();
   this->sw_ctrled_hw_rst_bank7.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->sw_ctrled_hw_rst.access(at);
   this->rst_dq3_enable.access(at);
   this->sw_ctrled_hw_rst_option.access(at);
   this->sw_ctrled_hw_rst_bank0.access(at);
   this->sw_ctrled_hw_rst_bank1.access(at);
   this->sw_ctrled_hw_rst_bank2.access(at);
   this->sw_ctrled_hw_rst_bank3.access(at);
   this->sw_ctrled_hw_rst_bank4.access(at);
   this->sw_ctrled_hw_rst_bank5.access(at);
   this->sw_ctrled_hw_rst_bank6.access(at);
   this->sw_ctrled_hw_rst_bank7.access(at);
  }
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst;
  scml2::cov::bitfield<unsigned int> rst_dq3_enable;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_option;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_bank0;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_bank1;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_bank2;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_bank3;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_bank4;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_bank5;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_bank6;
  scml2::cov::bitfield<unsigned int> sw_ctrled_hw_rst_bank7;
 };
  reset_pin_settings_type reset_pin_settings;
 struct clock_mode_settings_type : public scml2::cov::reg<unsigned int> 
 {
   clock_mode_settings_type(cdns_xspi_ctrl_reg::clock_mode_settings_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , spi_clock_mode(_reg.spi_clock_mode, name + ".spi_clock_mode")
  {
   disable();
   spi_clock_mode.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->spi_clock_mode.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->spi_clock_mode.access(at);
  }
  scml2::cov::bitfield<unsigned int> spi_clock_mode;
 };
  clock_mode_settings_type clock_mode_settings;
 struct jedec_rst_timing_reg_type : public scml2::cov::reg<unsigned int> 
 {
   jedec_rst_timing_reg_type(cdns_xspi_ctrl_reg::jedec_rst_timing_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , tCSH_delay(_reg.tCSH_delay, name + ".tCSH_delay")
   , tCSL_delay(_reg.tCSL_delay, name + ".tCSL_delay")
  {
   disable();
   tCSH_delay.configure_default_bins(128);
   tCSL_delay.configure_default_bins(128);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->tCSH_delay.disable();
   this->tCSL_delay.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->tCSH_delay.access(at);
   this->tCSL_delay.access(at);
  }
  scml2::cov::bitfield<unsigned int> tCSH_delay;
  scml2::cov::bitfield<unsigned int> tCSL_delay;
 };
  jedec_rst_timing_reg_type jedec_rst_timing_reg;
 struct dev_delay_reg_type : public scml2::cov::reg<unsigned int> 
 {
   dev_delay_reg_type(cdns_xspi_ctrl_reg::dev_delay_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , cssot_delay(_reg.cssot_delay, name + ".cssot_delay")
   , cseot_delay(_reg.cseot_delay, name + ".cseot_delay")
   , csda_min_delay(_reg.csda_min_delay, name + ".csda_min_delay")
  {
   disable();
   cssot_delay.configure_default_bins(0);
   cseot_delay.configure_default_bins(1);
   csda_min_delay.configure_default_bins(1);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->cssot_delay.disable();
   this->cseot_delay.disable();
   this->csda_min_delay.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->cssot_delay.access(at);
   this->cseot_delay.access(at);
   this->csda_min_delay.access(at);
  }
  scml2::cov::bitfield<unsigned int> cssot_delay;
  scml2::cov::bitfield<unsigned int> cseot_delay;
  scml2::cov::bitfield<unsigned int> csda_min_delay;
 };
  dev_delay_reg_type dev_delay_reg;
 struct rst_recovery_reg_type : public scml2::cov::reg<unsigned int> 
 {
   rst_recovery_reg_type(cdns_xspi_ctrl_reg::rst_recovery_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , rst_recovery(_reg.rst_recovery, name + ".rst_recovery")
  {
   disable();
   rst_recovery.configure_default_bins(10);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->rst_recovery.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->rst_recovery.access(at);
  }
  scml2::cov::bitfield<unsigned int> rst_recovery;
 };
  rst_recovery_reg_type rst_recovery_reg;
 struct dev_active_max_reg_type : public scml2::cov::reg<unsigned int> 
 {
   dev_active_max_reg_type(cdns_xspi_ctrl_reg::dev_active_max_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , dev_active_max(_reg.dev_active_max, name + ".dev_active_max")
  {
   disable();
   dev_active_max.configure_default_bins(128);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->dev_active_max.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->dev_active_max.access(at);
  }
  scml2::cov::bitfield<unsigned int> dev_active_max;
 };
  dev_active_max_reg_type dev_active_max_reg;
 struct hf_offset_reg_type : public scml2::cov::reg<unsigned int> 
 {
   hf_offset_reg_type(cdns_xspi_ctrl_reg::hf_offset_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , hf_offset_index(_reg.hf_offset_index, name + ".hf_offset_index")
   , hf_offset_size(_reg.hf_offset_size, name + ".hf_offset_size")
  {
   disable();
   hf_offset_index.configure_default_bins(3);
   hf_offset_size.configure_default_bins(13);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->hf_offset_index.disable();
   this->hf_offset_size.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->hf_offset_index.access(at);
   this->hf_offset_size.access(at);
  }
  scml2::cov::bitfield<unsigned int> hf_offset_index;
  scml2::cov::bitfield<unsigned int> hf_offset_size;
 };
  hf_offset_reg_type hf_offset_reg;
 struct dll_phy_update_cnt_type : public scml2::cov::reg<unsigned int> 
 {
   dll_phy_update_cnt_type(cdns_xspi_ctrl_reg::dll_phy_update_cnt_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , resync_cnt(_reg.resync_cnt, name + ".resync_cnt")
  {
   disable();
   resync_cnt.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->resync_cnt.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->resync_cnt.access(at);
  }
  scml2::cov::bitfield<unsigned int> resync_cnt;
 };
  dll_phy_update_cnt_type dll_phy_update_cnt;
 struct dll_phy_ctrl_type : public scml2::cov::reg<unsigned int> 
 {
   dll_phy_ctrl_type(cdns_xspi_ctrl_reg::dll_phy_ctrl_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , resync_idle_cnt(_reg.resync_idle_cnt, name + ".resync_idle_cnt")
   , resync_high_wait_cnt(_reg.resync_high_wait_cnt, name + ".resync_high_wait_cnt")
   , extended_rd_mode(_reg.extended_rd_mode, name + ".extended_rd_mode")
   , extended_wr_mode(_reg.extended_wr_mode, name + ".extended_wr_mode")
   , dqs_last_data_drop_en(_reg.dqs_last_data_drop_en, name + ".dqs_last_data_drop_en")
   , sdr_edge_active(_reg.sdr_edge_active, name + ".sdr_edge_active")
   , dll_rst_n(_reg.dll_rst_n, name + ".dll_rst_n")
   , dfi_ctrlupd_req(_reg.dfi_ctrlupd_req, name + ".dfi_ctrlupd_req")
  {
   disable();
   resync_idle_cnt.configure_default_bins(7);
   resync_high_wait_cnt.configure_default_bins(7);
   extended_rd_mode.configure_default_bins(1);
   extended_wr_mode.configure_default_bins(1);
   dqs_last_data_drop_en.configure_default_bins(0);
   sdr_edge_active.configure_default_bins(0);
   dll_rst_n.configure_default_bins(1);
   dfi_ctrlupd_req.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->resync_idle_cnt.disable();
   this->resync_high_wait_cnt.disable();
   this->extended_rd_mode.disable();
   this->extended_wr_mode.disable();
   this->dqs_last_data_drop_en.disable();
   this->sdr_edge_active.disable();
   this->dll_rst_n.disable();
   this->dfi_ctrlupd_req.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->resync_idle_cnt.access(at);
   this->resync_high_wait_cnt.access(at);
   this->extended_rd_mode.access(at);
   this->extended_wr_mode.access(at);
   this->dqs_last_data_drop_en.access(at);
   this->sdr_edge_active.access(at);
   this->dll_rst_n.access(at);
   this->dfi_ctrlupd_req.access(at);
  }
  scml2::cov::bitfield<unsigned int> resync_idle_cnt;
  scml2::cov::bitfield<unsigned int> resync_high_wait_cnt;
  scml2::cov::bitfield<unsigned int> extended_rd_mode;
  scml2::cov::bitfield<unsigned int> extended_wr_mode;
  scml2::cov::bitfield<unsigned int> dqs_last_data_drop_en;
  scml2::cov::bitfield<unsigned int> sdr_edge_active;
  scml2::cov::bitfield<unsigned int> dll_rst_n;
  scml2::cov::bitfield<unsigned int> dfi_ctrlupd_req;
 };
  dll_phy_ctrl_type dll_phy_ctrl;
 scml2::cov::memory<unsigned int> dataslice_Rfile_a;
 struct phy_dq_timing_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_dq_timing_reg_type(cdns_xspi_ctrl_reg::phy_dq_timing_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , data_select_oe_end(_reg.data_select_oe_end, name + ".data_select_oe_end")
   , data_select_oe_start(_reg.data_select_oe_start, name + ".data_select_oe_start")
   , data_select_tsel_end(_reg.data_select_tsel_end, name + ".data_select_tsel_end")
   , data_select_tsel_start(_reg.data_select_tsel_start, name + ".data_select_tsel_start")
   , data_clkperiod_delay(_reg.data_clkperiod_delay, name + ".data_clkperiod_delay")
  {
   disable();
   data_select_oe_end.configure_default_bins(2);
   data_select_oe_start.configure_default_bins(0);
   data_select_tsel_end.configure_default_bins(0);
   data_select_tsel_start.configure_default_bins(0);
   data_clkperiod_delay.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->data_select_oe_end.disable();
   this->data_select_oe_start.disable();
   this->data_select_tsel_end.disable();
   this->data_select_tsel_start.disable();
   this->data_clkperiod_delay.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->data_select_oe_end.access(at);
   this->data_select_oe_start.access(at);
   this->data_select_tsel_end.access(at);
   this->data_select_tsel_start.access(at);
   this->data_clkperiod_delay.access(at);
  }
  scml2::cov::bitfield<unsigned int> data_select_oe_end;
  scml2::cov::bitfield<unsigned int> data_select_oe_start;
  scml2::cov::bitfield<unsigned int> data_select_tsel_end;
  scml2::cov::bitfield<unsigned int> data_select_tsel_start;
  scml2::cov::bitfield<unsigned int> data_clkperiod_delay;
 };
  phy_dq_timing_reg_type phy_dq_timing_reg;
 struct phy_dqs_timing_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_dqs_timing_reg_type(cdns_xspi_ctrl_reg::phy_dqs_timing_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , dqs_select_tsel_end(_reg.dqs_select_tsel_end, name + ".dqs_select_tsel_end")
   , dqs_select_tsel_start(_reg.dqs_select_tsel_start, name + ".dqs_select_tsel_start")
   , phony_dqs_sel(_reg.phony_dqs_sel, name + ".phony_dqs_sel")
   , use_phony_dqs(_reg.use_phony_dqs, name + ".use_phony_dqs")
   , use_lpbk_dqs(_reg.use_lpbk_dqs, name + ".use_lpbk_dqs")
   , use_ext_lpbk_dqs(_reg.use_ext_lpbk_dqs, name + ".use_ext_lpbk_dqs")
  {
   disable();
   dqs_select_tsel_end.configure_default_bins(0);
   dqs_select_tsel_start.configure_default_bins(0);
   phony_dqs_sel.configure_default_bins(0);
   use_phony_dqs.configure_default_bins(1);
   use_lpbk_dqs.configure_default_bins(0);
   use_ext_lpbk_dqs.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->dqs_select_tsel_end.disable();
   this->dqs_select_tsel_start.disable();
   this->phony_dqs_sel.disable();
   this->use_phony_dqs.disable();
   this->use_lpbk_dqs.disable();
   this->use_ext_lpbk_dqs.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->dqs_select_tsel_end.access(at);
   this->dqs_select_tsel_start.access(at);
   this->phony_dqs_sel.access(at);
   this->use_phony_dqs.access(at);
   this->use_lpbk_dqs.access(at);
   this->use_ext_lpbk_dqs.access(at);
  }
  scml2::cov::bitfield<unsigned int> dqs_select_tsel_end;
  scml2::cov::bitfield<unsigned int> dqs_select_tsel_start;
  scml2::cov::bitfield<unsigned int> phony_dqs_sel;
  scml2::cov::bitfield<unsigned int> use_phony_dqs;
  scml2::cov::bitfield<unsigned int> use_lpbk_dqs;
  scml2::cov::bitfield<unsigned int> use_ext_lpbk_dqs;
 };
  phy_dqs_timing_reg_type phy_dqs_timing_reg;
 struct phy_gate_lpbk_ctrl_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_gate_lpbk_ctrl_reg_type(cdns_xspi_ctrl_reg::phy_gate_lpbk_ctrl_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , gate_cfg(_reg.gate_cfg, name + ".gate_cfg")
   , gate_cfg_close(_reg.gate_cfg_close, name + ".gate_cfg_close")
   , gate_cfg_always_on(_reg.gate_cfg_always_on, name + ".gate_cfg_always_on")
   , lpbk_en(_reg.lpbk_en, name + ".lpbk_en")
   , lpbk_internal(_reg.lpbk_internal, name + ".lpbk_internal")
   , loopback_control(_reg.loopback_control, name + ".loopback_control")
   , lpbk_fail_muxsel(_reg.lpbk_fail_muxsel, name + ".lpbk_fail_muxsel")
   , lpbk_err_check_timing(_reg.lpbk_err_check_timing, name + ".lpbk_err_check_timing")
   , rd_del_sel_empty(_reg.rd_del_sel_empty, name + ".rd_del_sel_empty")
   , underrun_suppress(_reg.underrun_suppress, name + ".underrun_suppress")
   , rd_del_sel(_reg.rd_del_sel, name + ".rd_del_sel")
   , sync_method(_reg.sync_method, name + ".sync_method")
  {
   disable();
   gate_cfg.configure_default_bins(0);
   gate_cfg_close.configure_default_bins(0);
   gate_cfg_always_on.configure_default_bins(0);
   lpbk_en.configure_default_bins(0);
   lpbk_internal.configure_default_bins(0);
   loopback_control.configure_default_bins(0);
   lpbk_fail_muxsel.configure_default_bins(0);
   lpbk_err_check_timing.configure_default_bins(0);
   rd_del_sel_empty.configure_default_bins(0);
   underrun_suppress.configure_default_bins(0);
   rd_del_sel.configure_default_bins(27);
   sync_method.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->gate_cfg.disable();
   this->gate_cfg_close.disable();
   this->gate_cfg_always_on.disable();
   this->lpbk_en.disable();
   this->lpbk_internal.disable();
   this->loopback_control.disable();
   this->lpbk_fail_muxsel.disable();
   this->lpbk_err_check_timing.disable();
   this->rd_del_sel_empty.disable();
   this->underrun_suppress.disable();
   this->rd_del_sel.disable();
   this->sync_method.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->gate_cfg.access(at);
   this->gate_cfg_close.access(at);
   this->gate_cfg_always_on.access(at);
   this->lpbk_en.access(at);
   this->lpbk_internal.access(at);
   this->loopback_control.access(at);
   this->lpbk_fail_muxsel.access(at);
   this->lpbk_err_check_timing.access(at);
   this->rd_del_sel_empty.access(at);
   this->underrun_suppress.access(at);
   this->rd_del_sel.access(at);
   this->sync_method.access(at);
  }
  scml2::cov::bitfield<unsigned int> gate_cfg;
  scml2::cov::bitfield<unsigned int> gate_cfg_close;
  scml2::cov::bitfield<unsigned int> gate_cfg_always_on;
  scml2::cov::bitfield<unsigned int> lpbk_en;
  scml2::cov::bitfield<unsigned int> lpbk_internal;
  scml2::cov::bitfield<unsigned int> loopback_control;
  scml2::cov::bitfield<unsigned int> lpbk_fail_muxsel;
  scml2::cov::bitfield<unsigned int> lpbk_err_check_timing;
  scml2::cov::bitfield<unsigned int> rd_del_sel_empty;
  scml2::cov::bitfield<unsigned int> underrun_suppress;
  scml2::cov::bitfield<unsigned int> rd_del_sel;
  scml2::cov::bitfield<unsigned int> sync_method;
 };
  phy_gate_lpbk_ctrl_reg_type phy_gate_lpbk_ctrl_reg;
 struct phy_dll_master_ctrl_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_dll_master_ctrl_reg_type(cdns_xspi_ctrl_reg::phy_dll_master_ctrl_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , param_dll_start_point(_reg.param_dll_start_point, name + ".param_dll_start_point")
   , param_dll_lock_num(_reg.param_dll_lock_num, name + ".param_dll_lock_num")
   , param_phase_detect_sel(_reg.param_phase_detect_sel, name + ".param_phase_detect_sel")
   , param_dll_bypass_mode(_reg.param_dll_bypass_mode, name + ".param_dll_bypass_mode")
  {
   disable();
   param_dll_start_point.configure_default_bins(0);
   param_dll_lock_num.configure_default_bins(0);
   param_phase_detect_sel.configure_default_bins(0);
   param_dll_bypass_mode.configure_default_bins(1);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->param_dll_start_point.disable();
   this->param_dll_lock_num.disable();
   this->param_phase_detect_sel.disable();
   this->param_dll_bypass_mode.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->param_dll_start_point.access(at);
   this->param_dll_lock_num.access(at);
   this->param_phase_detect_sel.access(at);
   this->param_dll_bypass_mode.access(at);
  }
  scml2::cov::bitfield<unsigned int> param_dll_start_point;
  scml2::cov::bitfield<unsigned int> param_dll_lock_num;
  scml2::cov::bitfield<unsigned int> param_phase_detect_sel;
  scml2::cov::bitfield<unsigned int> param_dll_bypass_mode;
 };
  phy_dll_master_ctrl_reg_type phy_dll_master_ctrl_reg;
 struct phy_dll_slave_ctrl_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_dll_slave_ctrl_reg_type(cdns_xspi_ctrl_reg::phy_dll_slave_ctrl_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , read_dqs_delay(_reg.read_dqs_delay, name + ".read_dqs_delay")
   , clk_wr_delay(_reg.clk_wr_delay, name + ".clk_wr_delay")
  {
   disable();
   read_dqs_delay.configure_default_bins(0);
   clk_wr_delay.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->read_dqs_delay.disable();
   this->clk_wr_delay.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->read_dqs_delay.access(at);
   this->clk_wr_delay.access(at);
  }
  scml2::cov::bitfield<unsigned int> read_dqs_delay;
  scml2::cov::bitfield<unsigned int> clk_wr_delay;
 };
  phy_dll_slave_ctrl_reg_type phy_dll_slave_ctrl_reg;
 struct phy_ie_timing_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_ie_timing_reg_type(cdns_xspi_ctrl_reg::phy_ie_timing_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , rddata_en_ie_dly(_reg.rddata_en_ie_dly, name + ".rddata_en_ie_dly")
   , dqs_ie_stop(_reg.dqs_ie_stop, name + ".dqs_ie_stop")
   , dqs_ie_start(_reg.dqs_ie_start, name + ".dqs_ie_start")
   , dq_ie_stop(_reg.dq_ie_stop, name + ".dq_ie_stop")
   , dq_ie_start(_reg.dq_ie_start, name + ".dq_ie_start")
   , ie_always_on(_reg.ie_always_on, name + ".ie_always_on")
  {
   disable();
   rddata_en_ie_dly.configure_default_bins(0);
   dqs_ie_stop.configure_default_bins(0);
   dqs_ie_start.configure_default_bins(0);
   dq_ie_stop.configure_default_bins(0);
   dq_ie_start.configure_default_bins(0);
   ie_always_on.configure_default_bins(1);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->rddata_en_ie_dly.disable();
   this->dqs_ie_stop.disable();
   this->dqs_ie_start.disable();
   this->dq_ie_stop.disable();
   this->dq_ie_start.disable();
   this->ie_always_on.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->rddata_en_ie_dly.access(at);
   this->dqs_ie_stop.access(at);
   this->dqs_ie_start.access(at);
   this->dq_ie_stop.access(at);
   this->dq_ie_start.access(at);
   this->ie_always_on.access(at);
  }
  scml2::cov::bitfield<unsigned int> rddata_en_ie_dly;
  scml2::cov::bitfield<unsigned int> dqs_ie_stop;
  scml2::cov::bitfield<unsigned int> dqs_ie_start;
  scml2::cov::bitfield<unsigned int> dq_ie_stop;
  scml2::cov::bitfield<unsigned int> dq_ie_start;
  scml2::cov::bitfield<unsigned int> ie_always_on;
 };
  phy_ie_timing_reg_type phy_ie_timing_reg;
 struct phy_obs_reg_0_type : public scml2::cov::reg<unsigned int> 
 {
   phy_obs_reg_0_type(cdns_xspi_ctrl_reg::phy_obs_reg_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , lpbk_status(_reg.lpbk_status, name + ".lpbk_status")
   , lpbk_dq_data(_reg.lpbk_dq_data, name + ".lpbk_dq_data")
   , dqs_underrun(_reg.dqs_underrun, name + ".dqs_underrun")
   , dqs_overflow(_reg.dqs_overflow, name + ".dqs_overflow")
  {
   disable();
   lpbk_status.access(scml2::cov::WRITES);
   lpbk_status.configure_default_bins(0);
   lpbk_dq_data.access(scml2::cov::WRITES);
   lpbk_dq_data.configure_default_bins(0);
   dqs_underrun.access(scml2::cov::WRITES);
   dqs_underrun.configure_default_bins(0);
   dqs_overflow.access(scml2::cov::WRITES);
   dqs_overflow.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->lpbk_status.disable();
   this->lpbk_dq_data.disable();
   this->dqs_underrun.disable();
   this->dqs_overflow.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->lpbk_status.access(at);
   this->lpbk_dq_data.access(at);
   this->dqs_underrun.access(at);
   this->dqs_overflow.access(at);
  }
  scml2::cov::bitfield<unsigned int> lpbk_status;
  scml2::cov::bitfield<unsigned int> lpbk_dq_data;
  scml2::cov::bitfield<unsigned int> dqs_underrun;
  scml2::cov::bitfield<unsigned int> dqs_overflow;
 };
  phy_obs_reg_0_type phy_obs_reg_0;
 struct phy_dll_obs_reg_0_type : public scml2::cov::reg<unsigned int> 
 {
   phy_dll_obs_reg_0_type(cdns_xspi_ctrl_reg::phy_dll_obs_reg_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , dll_lock(_reg.dll_lock, name + ".dll_lock")
   , dll_locked_mode(_reg.dll_locked_mode, name + ".dll_locked_mode")
   , dll_unlock_cnt(_reg.dll_unlock_cnt, name + ".dll_unlock_cnt")
   , dll_lock_value(_reg.dll_lock_value, name + ".dll_lock_value")
   , lock_dec_dbg(_reg.lock_dec_dbg, name + ".lock_dec_dbg")
   , lock_inc_dbg(_reg.lock_inc_dbg, name + ".lock_inc_dbg")
  {
   disable();
   dll_lock.access(scml2::cov::WRITES);
   dll_lock.configure_default_bins(0);
   dll_locked_mode.access(scml2::cov::WRITES);
   dll_locked_mode.configure_default_bins(0);
   dll_unlock_cnt.access(scml2::cov::WRITES);
   dll_unlock_cnt.configure_default_bins(0);
   dll_lock_value.access(scml2::cov::WRITES);
   dll_lock_value.configure_default_bins(0);
   lock_dec_dbg.access(scml2::cov::WRITES);
   lock_dec_dbg.configure_default_bins(0);
   lock_inc_dbg.access(scml2::cov::WRITES);
   lock_inc_dbg.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->dll_lock.disable();
   this->dll_locked_mode.disable();
   this->dll_unlock_cnt.disable();
   this->dll_lock_value.disable();
   this->lock_dec_dbg.disable();
   this->lock_inc_dbg.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->dll_lock.access(at);
   this->dll_locked_mode.access(at);
   this->dll_unlock_cnt.access(at);
   this->dll_lock_value.access(at);
   this->lock_dec_dbg.access(at);
   this->lock_inc_dbg.access(at);
  }
  scml2::cov::bitfield<unsigned int> dll_lock;
  scml2::cov::bitfield<unsigned int> dll_locked_mode;
  scml2::cov::bitfield<unsigned int> dll_unlock_cnt;
  scml2::cov::bitfield<unsigned int> dll_lock_value;
  scml2::cov::bitfield<unsigned int> lock_dec_dbg;
  scml2::cov::bitfield<unsigned int> lock_inc_dbg;
 };
  phy_dll_obs_reg_0_type phy_dll_obs_reg_0;
 struct phy_dll_obs_reg_1_type : public scml2::cov::reg<unsigned int> 
 {
   phy_dll_obs_reg_1_type(cdns_xspi_ctrl_reg::phy_dll_obs_reg_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , decoder_out_rd(_reg.decoder_out_rd, name + ".decoder_out_rd")
   , decoder_out_wr(_reg.decoder_out_wr, name + ".decoder_out_wr")
  {
   disable();
   decoder_out_rd.access(scml2::cov::WRITES);
   decoder_out_rd.configure_default_bins(0);
   decoder_out_wr.access(scml2::cov::WRITES);
   decoder_out_wr.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->decoder_out_rd.disable();
   this->decoder_out_wr.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->decoder_out_rd.access(at);
   this->decoder_out_wr.access(at);
  }
  scml2::cov::bitfield<unsigned int> decoder_out_rd;
  scml2::cov::bitfield<unsigned int> decoder_out_wr;
 };
  phy_dll_obs_reg_1_type phy_dll_obs_reg_1;
 struct phy_static_togg_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_static_togg_reg_type(cdns_xspi_ctrl_reg::phy_static_togg_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , static_tog_clk_div(_reg.static_tog_clk_div, name + ".static_tog_clk_div")
   , static_togg_global_enable(_reg.static_togg_global_enable, name + ".static_togg_global_enable")
   , static_togg_enable(_reg.static_togg_enable, name + ".static_togg_enable")
  {
   disable();
   static_tog_clk_div.configure_default_bins(0);
   static_togg_global_enable.configure_default_bins(0);
   static_togg_enable.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->static_tog_clk_div.disable();
   this->static_togg_global_enable.disable();
   this->static_togg_enable.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->static_tog_clk_div.access(at);
   this->static_togg_global_enable.access(at);
   this->static_togg_enable.access(at);
  }
  scml2::cov::bitfield<unsigned int> static_tog_clk_div;
  scml2::cov::bitfield<unsigned int> static_togg_global_enable;
  scml2::cov::bitfield<unsigned int> static_togg_enable;
 };
  phy_static_togg_reg_type phy_static_togg_reg;
 struct phy_wr_deskew_pd_ctrl_0_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_wr_deskew_pd_ctrl_0_reg_type(cdns_xspi_ctrl_reg::phy_wr_deskew_pd_ctrl_0_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , dq_phase_detect_sel(_reg.dq_phase_detect_sel, name + ".dq_phase_detect_sel")
   , dq_sw_half_cycle_shift(_reg.dq_sw_half_cycle_shift, name + ".dq_sw_half_cycle_shift")
   , dq_en_sw_half_cycle(_reg.dq_en_sw_half_cycle, name + ".dq_en_sw_half_cycle")
   , dq_sw_dq_phase_bypass(_reg.dq_sw_dq_phase_bypass, name + ".dq_sw_dq_phase_bypass")
  {
   disable();
   dq_phase_detect_sel.configure_default_bins(0);
   dq_sw_half_cycle_shift.configure_default_bins(0);
   dq_en_sw_half_cycle.configure_default_bins(0);
   dq_sw_dq_phase_bypass.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->dq_phase_detect_sel.disable();
   this->dq_sw_half_cycle_shift.disable();
   this->dq_en_sw_half_cycle.disable();
   this->dq_sw_dq_phase_bypass.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->dq_phase_detect_sel.access(at);
   this->dq_sw_half_cycle_shift.access(at);
   this->dq_en_sw_half_cycle.access(at);
   this->dq_sw_dq_phase_bypass.access(at);
  }
  scml2::cov::bitfield<unsigned int> dq_phase_detect_sel;
  scml2::cov::bitfield<unsigned int> dq_sw_half_cycle_shift;
  scml2::cov::bitfield<unsigned int> dq_en_sw_half_cycle;
  scml2::cov::bitfield<unsigned int> dq_sw_dq_phase_bypass;
 };
  phy_wr_deskew_pd_ctrl_0_reg_type phy_wr_deskew_pd_ctrl_0_reg;
 struct phy_version_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_version_reg_type(cdns_xspi_ctrl_reg::phy_version_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , phy_rev(_reg.phy_rev, name + ".phy_rev")
   , phy_fix(_reg.phy_fix, name + ".phy_fix")
   , combo_phy_magic_number(_reg.combo_phy_magic_number, name + ".combo_phy_magic_number")
  {
   disable();
   phy_rev.access(scml2::cov::WRITES);
   phy_rev.configure_default_bins(7);
   phy_fix.access(scml2::cov::WRITES);
   phy_fix.configure_default_bins(1);
   combo_phy_magic_number.access(scml2::cov::WRITES);
   combo_phy_magic_number.configure_default_bins(24962);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->phy_rev.disable();
   this->phy_fix.disable();
   this->combo_phy_magic_number.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->phy_rev.access(at);
   this->phy_fix.access(at);
   this->combo_phy_magic_number.access(at);
  }
  scml2::cov::bitfield<unsigned int> phy_rev;
  scml2::cov::bitfield<unsigned int> phy_fix;
  scml2::cov::bitfield<unsigned int> combo_phy_magic_number;
 };
  phy_version_reg_type phy_version_reg;
 struct phy_features_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_features_reg_type(cdns_xspi_ctrl_reg::phy_features_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , onfi_40(_reg.onfi_40, name + ".onfi_40")
   , onfi_41(_reg.onfi_41, name + ".onfi_41")
   , sdr_16bit(_reg.sdr_16bit, name + ".sdr_16bit")
   , xspi(_reg.xspi, name + ".xspi")
   , sd_emmc(_reg.sd_emmc, name + ".sd_emmc")
   , bank_num(_reg.bank_num, name + ".bank_num")
   , dll_tap_num(_reg.dll_tap_num, name + ".dll_tap_num")
   , aging(_reg.aging, name + ".aging")
   , dfi_clock_ratio(_reg.dfi_clock_ratio, name + ".dfi_clock_ratio")
   , per_bit_deskew(_reg.per_bit_deskew, name + ".per_bit_deskew")
   , reg_intf(_reg.reg_intf, name + ".reg_intf")
   , ext_lpbk_dqs(_reg.ext_lpbk_dqs, name + ".ext_lpbk_dqs")
   , jtag_sup(_reg.jtag_sup, name + ".jtag_sup")
   , pll_sup(_reg.pll_sup, name + ".pll_sup")
   , asf_sup(_reg.asf_sup, name + ".asf_sup")
  {
   disable();
   onfi_40.access(scml2::cov::WRITES);
   onfi_40.configure_default_bins(0);
   onfi_41.access(scml2::cov::WRITES);
   onfi_41.configure_default_bins(0);
   sdr_16bit.access(scml2::cov::WRITES);
   sdr_16bit.configure_default_bins(0);
   xspi.access(scml2::cov::WRITES);
   xspi.configure_default_bins(1);
   sd_emmc.access(scml2::cov::WRITES);
   sd_emmc.configure_default_bins(0);
   bank_num.access(scml2::cov::WRITES);
   bank_num.configure_default_bins(3);
   dll_tap_num.access(scml2::cov::WRITES);
   dll_tap_num.configure_default_bins(1);
   aging.access(scml2::cov::WRITES);
   aging.configure_default_bins(1);
   dfi_clock_ratio.access(scml2::cov::WRITES);
   dfi_clock_ratio.configure_default_bins(0);
   per_bit_deskew.access(scml2::cov::WRITES);
   per_bit_deskew.configure_default_bins(0);
   reg_intf.access(scml2::cov::WRITES);
   reg_intf.configure_default_bins(1);
   ext_lpbk_dqs.access(scml2::cov::WRITES);
   ext_lpbk_dqs.configure_default_bins(1);
   jtag_sup.access(scml2::cov::WRITES);
   jtag_sup.configure_default_bins(0);
   pll_sup.access(scml2::cov::WRITES);
   pll_sup.configure_default_bins(0);
   asf_sup.access(scml2::cov::WRITES);
   asf_sup.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->onfi_40.disable();
   this->onfi_41.disable();
   this->sdr_16bit.disable();
   this->xspi.disable();
   this->sd_emmc.disable();
   this->bank_num.disable();
   this->dll_tap_num.disable();
   this->aging.disable();
   this->dfi_clock_ratio.disable();
   this->per_bit_deskew.disable();
   this->reg_intf.disable();
   this->ext_lpbk_dqs.disable();
   this->jtag_sup.disable();
   this->pll_sup.disable();
   this->asf_sup.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->onfi_40.access(at);
   this->onfi_41.access(at);
   this->sdr_16bit.access(at);
   this->xspi.access(at);
   this->sd_emmc.access(at);
   this->bank_num.access(at);
   this->dll_tap_num.access(at);
   this->aging.access(at);
   this->dfi_clock_ratio.access(at);
   this->per_bit_deskew.access(at);
   this->reg_intf.access(at);
   this->ext_lpbk_dqs.access(at);
   this->jtag_sup.access(at);
   this->pll_sup.access(at);
   this->asf_sup.access(at);
  }
  scml2::cov::bitfield<unsigned int> onfi_40;
  scml2::cov::bitfield<unsigned int> onfi_41;
  scml2::cov::bitfield<unsigned int> sdr_16bit;
  scml2::cov::bitfield<unsigned int> xspi;
  scml2::cov::bitfield<unsigned int> sd_emmc;
  scml2::cov::bitfield<unsigned int> bank_num;
  scml2::cov::bitfield<unsigned int> dll_tap_num;
  scml2::cov::bitfield<unsigned int> aging;
  scml2::cov::bitfield<unsigned int> dfi_clock_ratio;
  scml2::cov::bitfield<unsigned int> per_bit_deskew;
  scml2::cov::bitfield<unsigned int> reg_intf;
  scml2::cov::bitfield<unsigned int> ext_lpbk_dqs;
  scml2::cov::bitfield<unsigned int> jtag_sup;
  scml2::cov::bitfield<unsigned int> pll_sup;
  scml2::cov::bitfield<unsigned int> asf_sup;
 };
  phy_features_reg_type phy_features_reg;
 scml2::cov::memory<unsigned int> ctb_Rfile_a;
 struct phy_ctrl_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_ctrl_reg_type(cdns_xspi_ctrl_reg::phy_ctrl_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , ctrl_clkperiod_delay(_reg.ctrl_clkperiod_delay, name + ".ctrl_clkperiod_delay")
   , phony_dqs_timing(_reg.phony_dqs_timing, name + ".phony_dqs_timing")
  {
   disable();
   ctrl_clkperiod_delay.configure_default_bins(0);
   phony_dqs_timing.configure_default_bins(24);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->ctrl_clkperiod_delay.disable();
   this->phony_dqs_timing.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->ctrl_clkperiod_delay.access(at);
   this->phony_dqs_timing.access(at);
  }
  scml2::cov::bitfield<unsigned int> ctrl_clkperiod_delay;
  scml2::cov::bitfield<unsigned int> phony_dqs_timing;
 };
  phy_ctrl_reg_type phy_ctrl_reg;
 struct phy_tsel_reg_type : public scml2::cov::reg<unsigned int> 
 {
   phy_tsel_reg_type(cdns_xspi_ctrl_reg::phy_tsel_reg_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , tsel_rd_value_dqs(_reg.tsel_rd_value_dqs, name + ".tsel_rd_value_dqs")
   , tsel_off_value_dqs(_reg.tsel_off_value_dqs, name + ".tsel_off_value_dqs")
   , tsel_rd_value_data(_reg.tsel_rd_value_data, name + ".tsel_rd_value_data")
   , tsel_off_value_data(_reg.tsel_off_value_data, name + ".tsel_off_value_data")
  {
   disable();
   tsel_rd_value_dqs.configure_default_bins(0);
   tsel_off_value_dqs.configure_default_bins(0);
   tsel_rd_value_data.configure_default_bins(0);
   tsel_off_value_data.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->tsel_rd_value_dqs.disable();
   this->tsel_off_value_dqs.disable();
   this->tsel_rd_value_data.disable();
   this->tsel_off_value_data.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->tsel_rd_value_dqs.access(at);
   this->tsel_off_value_dqs.access(at);
   this->tsel_rd_value_data.access(at);
   this->tsel_off_value_data.access(at);
  }
  scml2::cov::bitfield<unsigned int> tsel_rd_value_dqs;
  scml2::cov::bitfield<unsigned int> tsel_off_value_dqs;
  scml2::cov::bitfield<unsigned int> tsel_rd_value_data;
  scml2::cov::bitfield<unsigned int> tsel_off_value_data;
 };
  phy_tsel_reg_type phy_tsel_reg;
 struct phy_gpio_ctrl_0_type : public scml2::cov::reg<unsigned int> 
 {
   phy_gpio_ctrl_0_type(cdns_xspi_ctrl_reg::phy_gpio_ctrl_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , phy_gpio_ctrl_0_value(_reg.phy_gpio_ctrl_0_value, name + ".phy_gpio_ctrl_0_value")
  {
   disable();
   phy_gpio_ctrl_0_value.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->phy_gpio_ctrl_0_value.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->phy_gpio_ctrl_0_value.access(at);
  }
  scml2::cov::bitfield<unsigned int> phy_gpio_ctrl_0_value;
 };
  phy_gpio_ctrl_0_type phy_gpio_ctrl_0;
 struct phy_gpio_ctrl_1_type : public scml2::cov::reg<unsigned int> 
 {
   phy_gpio_ctrl_1_type(cdns_xspi_ctrl_reg::phy_gpio_ctrl_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , phy_gpio_ctrl_1_value(_reg.phy_gpio_ctrl_1_value, name + ".phy_gpio_ctrl_1_value")
  {
   disable();
   phy_gpio_ctrl_1_value.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->phy_gpio_ctrl_1_value.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->phy_gpio_ctrl_1_value.access(at);
  }
  scml2::cov::bitfield<unsigned int> phy_gpio_ctrl_1_value;
 };
  phy_gpio_ctrl_1_type phy_gpio_ctrl_1;
 struct phy_gpio_status_0_type : public scml2::cov::reg<unsigned int> 
 {
   phy_gpio_status_0_type(cdns_xspi_ctrl_reg::phy_gpio_status_0_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , phy_gpio_status_0_value(_reg.phy_gpio_status_0_value, name + ".phy_gpio_status_0_value")
  {
   disable();
   phy_gpio_status_0_value.access(scml2::cov::WRITES);
   phy_gpio_status_0_value.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->phy_gpio_status_0_value.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->phy_gpio_status_0_value.access(at);
  }
  scml2::cov::bitfield<unsigned int> phy_gpio_status_0_value;
 };
  phy_gpio_status_0_type phy_gpio_status_0;
 struct phy_gpio_status_1_type : public scml2::cov::reg<unsigned int> 
 {
   phy_gpio_status_1_type(cdns_xspi_ctrl_reg::phy_gpio_status_1_type& _reg, const std::string& name, cdns_xspi_ctrl_regCovermodelBase * model = 0)
    : scml2::cov::reg<unsigned int>(_reg, name)
   , phy_gpio_status_1_value(_reg.phy_gpio_status_1_value, name + ".phy_gpio_status_1_value")
  {
   disable();
   phy_gpio_status_1_value.access(scml2::cov::WRITES);
   phy_gpio_status_1_value.configure_default_bins(0);
  }
  virtual void disable_all()
  {
   scml2::cov::reg<unsigned int>::disable_all();
   this->phy_gpio_status_1_value.disable();
  }
  virtual void access_all(scml2::cov::access_type at)
  {
   scml2::cov::reg<unsigned int>::access_all(at);
   this->phy_gpio_status_1_value.access(at);
  }
  scml2::cov::bitfield<unsigned int> phy_gpio_status_1_value;
 };
  phy_gpio_status_1_type phy_gpio_status_1;
 #endif
};
#ifdef _WIN32
#pragma optimize("",on)
#else
#pragma GCC pop_options
#endif


}  // end of namespace mylibrary

