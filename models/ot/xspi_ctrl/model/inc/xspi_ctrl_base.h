#pragma once
#include "xspi_ctrl_register.h"
#include <string.h>

class xspi_ctrl_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;
    xspi_ctrl_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       cmd_reg0(std::string(name) + ".cmd_reg0", memory, (0x000 + 0x00)/sizeof(unsigned int)), 
       cmd_reg1(std::string(name) + ".cmd_reg1", memory, (0x004 + 0x00)/sizeof(unsigned int)), 
       cmd_reg2(std::string(name) + ".cmd_reg2", memory, (0x008 + 0x00)/sizeof(unsigned int)), 
       cmd_reg3(std::string(name) + ".cmd_reg3", memory, (0x00C + 0x00)/sizeof(unsigned int)), 
       cmd_reg4(std::string(name) + ".cmd_reg4", memory, (0x010 + 0x00)/sizeof(unsigned int)), 
       cmd_reg5(std::string(name) + ".cmd_reg5", memory, (0x014 + 0x00)/sizeof(unsigned int)), 
       cmd_status_ptr(std::string(name) + ".cmd_status_ptr", memory, (0x040 + 0x00)/sizeof(unsigned int)), 
       cmd_status(std::string(name) + ".cmd_status", memory, (0x044 + 0x00)/sizeof(unsigned int)), 
       ctrl_status(std::string(name) + ".ctrl_status", memory, (0x100 + 0x00)/sizeof(unsigned int)), 
       trd_status(std::string(name) + ".trd_status", memory, (0x104 + 0x00)/sizeof(unsigned int)), 
       intr_status(std::string(name) + ".intr_status", memory, (0x110 + 0x00)/sizeof(unsigned int)), 
       intr_enable(std::string(name) + ".intr_enable", memory, (0x114 + 0x00)/sizeof(unsigned int)), 
       trd_comp_intr_status(std::string(name) + ".trd_comp_intr_status", memory, (0x120 + 0x00)/sizeof(unsigned int)), 
       trd_error_intr_status(std::string(name) + ".trd_error_intr_status", memory, (0x130 + 0x00)/sizeof(unsigned int)), 
       trd_error_intr_en(std::string(name) + ".trd_error_intr_en", memory, (0x134 + 0x00)/sizeof(unsigned int)), 
       dma_target_error_l(std::string(name) + ".dma_target_error_l", memory, (0x150 + 0x00)/sizeof(unsigned int)), 
       dma_target_error_h(std::string(name) + ".dma_target_error_h", memory, (0x154 + 0x00)/sizeof(unsigned int)), 
       boot_status(std::string(name) + ".boot_status", memory, (0x158 + 0x00)/sizeof(unsigned int)), 
       long_polling(std::string(name) + ".long_polling", memory, (0x208 + 0x00)/sizeof(unsigned int)), 
       short_polling(std::string(name) + ".short_polling", memory, (0x20C + 0x00)/sizeof(unsigned int)), 
       ctrl_config(std::string(name) + ".ctrl_config", memory, (0x230 + 0x00)/sizeof(unsigned int)), 
       dma_settings(std::string(name) + ".dma_settings", memory, (0x23C + 0x00)/sizeof(unsigned int)), 
       sdma_size(std::string(name) + ".sdma_size", memory, (0x240 + 0x00)/sizeof(unsigned int)), 
       sdma_trd_info(std::string(name) + ".sdma_trd_info", memory, (0x244 + 0x00)/sizeof(unsigned int)), 
       sdma_addr0(std::string(name) + ".sdma_addr0", memory, (0x24C + 0x00)/sizeof(unsigned int)), 
       sdma_addr1(std::string(name) + ".sdma_addr1", memory, (0x250 + 0x00)/sizeof(unsigned int)), 
       discovery_control(std::string(name) + ".discovery_control", memory, (0x260 + 0x00)/sizeof(unsigned int)), 
       xip_mode_cfg(std::string(name) + ".xip_mode_cfg", memory, (0x388 + 0x00)/sizeof(unsigned int)), 
       global_seq_cfg(std::string(name) + ".global_seq_cfg", memory, (0x390 + 0x00)/sizeof(unsigned int)), 
       global_seq_cfg_1(std::string(name) + ".global_seq_cfg_1", memory, (0x394 + 0x00)/sizeof(unsigned int)), 
       direct_access_cfg(std::string(name) + ".direct_access_cfg", memory, (0x398 + 0x00)/sizeof(unsigned int)), 
       direct_access_rmp(std::string(name) + ".direct_access_rmp", memory, (0x39C + 0x00)/sizeof(unsigned int)), 
       direct_access_rmp_1(std::string(name) + ".direct_access_rmp_1", memory, (0x3A0 + 0x00)/sizeof(unsigned int)), 
       rst_seq_cfg_0(std::string(name) + ".rst_seq_cfg_0", memory, (0x400 + 0x00)/sizeof(unsigned int)), 
       rst_seq_cfg_1(std::string(name) + ".rst_seq_cfg_1", memory, (0x404 + 0x00)/sizeof(unsigned int)), 
       ers_seq_cfg_0(std::string(name) + ".ers_seq_cfg_0", memory, (0x410 + 0x00)/sizeof(unsigned int)), 
       ers_seq_cfg_1(std::string(name) + ".ers_seq_cfg_1", memory, (0x414 + 0x00)/sizeof(unsigned int)), 
       ers_seq_cfg_2(std::string(name) + ".ers_seq_cfg_2", memory, (0x418 + 0x00)/sizeof(unsigned int)), 
       prog_seq_cfg_0(std::string(name) + ".prog_seq_cfg_0", memory, (0x420 + 0x00)/sizeof(unsigned int)), 
       prog_seq_cfg_1(std::string(name) + ".prog_seq_cfg_1", memory, (0x424 + 0x00)/sizeof(unsigned int)), 
       prog_seq_cfg_2(std::string(name) + ".prog_seq_cfg_2", memory, (0x428 + 0x00)/sizeof(unsigned int)), 
       read_seq_cfg_0(std::string(name) + ".read_seq_cfg_0", memory, (0x430 + 0x00)/sizeof(unsigned int)), 
       read_seq_cfg_1(std::string(name) + ".read_seq_cfg_1", memory, (0x434 + 0x00)/sizeof(unsigned int)), 
       read_seq_cfg_2(std::string(name) + ".read_seq_cfg_2", memory, (0x438 + 0x00)/sizeof(unsigned int)), 
       we_seq_cfg_0(std::string(name) + ".we_seq_cfg_0", memory, (0x440 + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_0(std::string(name) + ".stat_seq_cfg_0", memory, (0x450 + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_1(std::string(name) + ".stat_seq_cfg_1", memory, (0x454 + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_2(std::string(name) + ".stat_seq_cfg_2", memory, (0x458 + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_3(std::string(name) + ".stat_seq_cfg_3", memory, (0x45C + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_4(std::string(name) + ".stat_seq_cfg_4", memory, (0x460 + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_5(std::string(name) + ".stat_seq_cfg_5", memory, (0x464 + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_7(std::string(name) + ".stat_seq_cfg_7", memory, (0x46C + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_8(std::string(name) + ".stat_seq_cfg_8", memory, (0x470 + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_9(std::string(name) + ".stat_seq_cfg_9", memory, (0x474 + 0x00)/sizeof(unsigned int)), 
       stat_seq_cfg_10(std::string(name) + ".stat_seq_cfg_10", memory, (0x478 + 0x00)/sizeof(unsigned int)), 
       xspi_ctrl_version(std::string(name) + ".xspi_ctrl_version", memory, (0xF00 + 0x00)/sizeof(unsigned int)), 
       ctrl_features_reg(std::string(name) + ".ctrl_features_reg", memory, (0xF04 + 0x00)/sizeof(unsigned int)), 
       wp_settings(std::string(name) + ".wp_settings", memory, (0x1000 + 0x00)/sizeof(unsigned int)), 
       reset_pin_settings(std::string(name) + ".reset_pin_settings", memory, (0x1004 + 0x00)/sizeof(unsigned int)), 
       clock_mode_settings(std::string(name) + ".clock_mode_settings", memory, (0x1008 + 0x00)/sizeof(unsigned int)), 
       jedec_rst_timing_reg(std::string(name) + ".jedec_rst_timing_reg", memory, (0x100C + 0x00)/sizeof(unsigned int)), 
       dev_delay_reg(std::string(name) + ".dev_delay_reg", memory, (0x1010 + 0x00)/sizeof(unsigned int)), 
       rst_recovery_reg(std::string(name) + ".rst_recovery_reg", memory, (0x1014 + 0x00)/sizeof(unsigned int)), 
       dev_active_max_reg(std::string(name) + ".dev_active_max_reg", memory, (0x1018 + 0x00)/sizeof(unsigned int)), 
       hf_offset_reg(std::string(name) + ".hf_offset_reg", memory, (0x1020 + 0x00)/sizeof(unsigned int)), 
       dll_phy_update_cnt(std::string(name) + ".dll_phy_update_cnt", memory, (0x1030 + 0x00)/sizeof(unsigned int)), 
       dll_phy_ctrl(std::string(name) + ".dll_phy_ctrl", memory, (0x1034 + 0x00)/sizeof(unsigned int)), 
       phy_dq_timing_reg(std::string(name) + ".phy_dq_timing_reg", memory, (0x2000 + 0x00)/sizeof(unsigned int)), 
       phy_dqs_timing_reg(std::string(name) + ".phy_dqs_timing_reg", memory, (0x2004 + 0x00)/sizeof(unsigned int)), 
       phy_gate_lpbk_ctrl_reg(std::string(name) + ".phy_gate_lpbk_ctrl_reg", memory, (0x2008 + 0x00)/sizeof(unsigned int)), 
       phy_dll_master_ctrl_reg(std::string(name) + ".phy_dll_master_ctrl_reg", memory, (0x200C + 0x00)/sizeof(unsigned int)), 
       phy_dll_slave_ctrl_reg(std::string(name) + ".phy_dll_slave_ctrl_reg", memory, (0x2010 + 0x00)/sizeof(unsigned int)), 
       phy_ie_timing_reg(std::string(name) + ".phy_ie_timing_reg", memory, (0x2014 + 0x00)/sizeof(unsigned int)), 
       phy_obs_reg_0(std::string(name) + ".phy_obs_reg_0", memory, (0x2018 + 0x00)/sizeof(unsigned int)), 
       phy_dll_obs_reg_0(std::string(name) + ".phy_dll_obs_reg_0", memory, (0x201C + 0x00)/sizeof(unsigned int)), 
       phy_dll_obs_reg_1(std::string(name) + ".phy_dll_obs_reg_1", memory, (0x2020 + 0x00)/sizeof(unsigned int)), 
       phy_dll_obs_reg_2(std::string(name) + ".phy_dll_obs_reg_2", memory, (0x2024 + 0x00)/sizeof(unsigned int)), 
       phy_static_togg_reg(std::string(name) + ".phy_static_togg_reg", memory, (0x2028 + 0x00)/sizeof(unsigned int)), 
       phy_wr_deskew_reg(std::string(name) + ".phy_wr_deskew_reg", memory, (0x202C + 0x00)/sizeof(unsigned int)), 
       phy_wr_rd_deskew_cmd_reg(std::string(name) + ".phy_wr_rd_deskew_cmd_reg", memory, (0x2030 + 0x00)/sizeof(unsigned int)), 
       phy_wr_deskew_pd_ctrl_0_reg(std::string(name) + ".phy_wr_deskew_pd_ctrl_0_reg", memory, (0x2034 + 0x00)/sizeof(unsigned int)), 
       phy_wr_deskew_pd_ctrl_1_reg(std::string(name) + ".phy_wr_deskew_pd_ctrl_1_reg", memory, (0x2038 + 0x00)/sizeof(unsigned int)), 
       phy_rd_deskew_reg(std::string(name) + ".phy_rd_deskew_reg", memory, (0x203C + 0x00)/sizeof(unsigned int)), 
       phy_version_reg(std::string(name) + ".phy_version_reg", memory, (0x2070 + 0x00)/sizeof(unsigned int)), 
       phy_features_reg(std::string(name) + ".phy_features_reg", memory, (0x2074 + 0x00)/sizeof(unsigned int)), 
       phy_ctrl_reg(std::string(name) + ".phy_ctrl_reg", memory, (0x2080 + 0x00)/sizeof(unsigned int)), 
       phy_tsel_reg(std::string(name) + ".phy_tsel_reg", memory, (0x2084 + 0x00)/sizeof(unsigned int)), 
       phy_gpio_ctrl_0(std::string(name) + ".phy_gpio_ctrl_0", memory, (0x2088 + 0x00)/sizeof(unsigned int)), 
       phy_gpio_ctrl_1(std::string(name) + ".phy_gpio_ctrl_1", memory, (0x208C + 0x00)/sizeof(unsigned int)), 
       phy_gpio_status_0(std::string(name) + ".phy_gpio_status_0", memory, (0x2090 + 0x00)/sizeof(unsigned int)), 
       phy_gpio_status_1(std::string(name) + ".phy_gpio_status_1", memory, (0x2094 + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      csml_memory<32> memory;
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      
      xspi_ctrl::cmd_reg0_type<32> cmd_reg0;
      
      xspi_ctrl::cmd_reg1_type<32> cmd_reg1;
      
      xspi_ctrl::cmd_reg2_type<32> cmd_reg2;
      
      xspi_ctrl::cmd_reg3_type<32> cmd_reg3;
      
      xspi_ctrl::cmd_reg4_type<32> cmd_reg4;
      
      xspi_ctrl::cmd_reg5_type<32> cmd_reg5;
      
      xspi_ctrl::cmd_status_ptr_type<32> cmd_status_ptr;
      
      xspi_ctrl::cmd_status_type<32> cmd_status;
      
      xspi_ctrl::ctrl_status_type<32> ctrl_status;
      
      xspi_ctrl::trd_status_type<32> trd_status;
      
      xspi_ctrl::intr_status_type<32> intr_status;
      
      xspi_ctrl::intr_enable_type<32> intr_enable;
      
      xspi_ctrl::trd_comp_intr_status_type<32> trd_comp_intr_status;
      
      xspi_ctrl::trd_error_intr_status_type<32> trd_error_intr_status;
      
      xspi_ctrl::trd_error_intr_en_type<32> trd_error_intr_en;
      
      xspi_ctrl::dma_target_error_l_type<32> dma_target_error_l;
      
      xspi_ctrl::dma_target_error_h_type<32> dma_target_error_h;
      
      xspi_ctrl::boot_status_type<32> boot_status;
      
      xspi_ctrl::long_polling_type<32> long_polling;
      
      xspi_ctrl::short_polling_type<32> short_polling;
      
      xspi_ctrl::ctrl_config_type<32> ctrl_config;
      
      xspi_ctrl::dma_settings_type<32> dma_settings;
      
      xspi_ctrl::sdma_size_type<32> sdma_size;
      
      xspi_ctrl::sdma_trd_info_type<32> sdma_trd_info;
      
      xspi_ctrl::sdma_addr0_type<32> sdma_addr0;
      
      xspi_ctrl::sdma_addr1_type<32> sdma_addr1;
      
      xspi_ctrl::discovery_control_type<32> discovery_control;
      
      xspi_ctrl::xip_mode_cfg_type<32> xip_mode_cfg;
      
      xspi_ctrl::global_seq_cfg_type<32> global_seq_cfg;
      
      xspi_ctrl::global_seq_cfg_1_type<32> global_seq_cfg_1;
      
      xspi_ctrl::direct_access_cfg_type<32> direct_access_cfg;
      
      xspi_ctrl::direct_access_rmp_type<32> direct_access_rmp;
      
      xspi_ctrl::direct_access_rmp_1_type<32> direct_access_rmp_1;
      
      xspi_ctrl::rst_seq_cfg_0_type<32> rst_seq_cfg_0;
      
      xspi_ctrl::rst_seq_cfg_1_type<32> rst_seq_cfg_1;
      
      xspi_ctrl::ers_seq_cfg_0_type<32> ers_seq_cfg_0;
      
      xspi_ctrl::ers_seq_cfg_1_type<32> ers_seq_cfg_1;
      
      xspi_ctrl::ers_seq_cfg_2_type<32> ers_seq_cfg_2;
      
      xspi_ctrl::prog_seq_cfg_0_type<32> prog_seq_cfg_0;
      
      xspi_ctrl::prog_seq_cfg_1_type<32> prog_seq_cfg_1;
      
      xspi_ctrl::prog_seq_cfg_2_type<32> prog_seq_cfg_2;
      
      xspi_ctrl::read_seq_cfg_0_type<32> read_seq_cfg_0;
      
      xspi_ctrl::read_seq_cfg_1_type<32> read_seq_cfg_1;
      
      xspi_ctrl::read_seq_cfg_2_type<32> read_seq_cfg_2;
      
      xspi_ctrl::we_seq_cfg_0_type<32> we_seq_cfg_0;
      
      xspi_ctrl::stat_seq_cfg_0_type<32> stat_seq_cfg_0;
      
      xspi_ctrl::stat_seq_cfg_1_type<32> stat_seq_cfg_1;
      
      xspi_ctrl::stat_seq_cfg_2_type<32> stat_seq_cfg_2;
      
      xspi_ctrl::stat_seq_cfg_3_type<32> stat_seq_cfg_3;
      
      xspi_ctrl::stat_seq_cfg_4_type<32> stat_seq_cfg_4;
      
      xspi_ctrl::stat_seq_cfg_5_type<32> stat_seq_cfg_5;
      
      xspi_ctrl::stat_seq_cfg_7_type<32> stat_seq_cfg_7;
      
      xspi_ctrl::stat_seq_cfg_8_type<32> stat_seq_cfg_8;
      
      xspi_ctrl::stat_seq_cfg_9_type<32> stat_seq_cfg_9;
      
      xspi_ctrl::stat_seq_cfg_10_type<32> stat_seq_cfg_10;
      
      xspi_ctrl::xspi_ctrl_version_type<32> xspi_ctrl_version;
      
      xspi_ctrl::ctrl_features_reg_type<32> ctrl_features_reg;
      
      xspi_ctrl::wp_settings_type<32> wp_settings;
      
      xspi_ctrl::reset_pin_settings_type<32> reset_pin_settings;
      
      xspi_ctrl::clock_mode_settings_type<32> clock_mode_settings;
      
      xspi_ctrl::jedec_rst_timing_reg_type<32> jedec_rst_timing_reg;
      
      xspi_ctrl::dev_delay_reg_type<32> dev_delay_reg;
      
      xspi_ctrl::rst_recovery_reg_type<32> rst_recovery_reg;
      
      xspi_ctrl::dev_active_max_reg_type<32> dev_active_max_reg;
      
      xspi_ctrl::hf_offset_reg_type<32> hf_offset_reg;
      
      xspi_ctrl::dll_phy_update_cnt_type<32> dll_phy_update_cnt;
      
      xspi_ctrl::dll_phy_ctrl_type<32> dll_phy_ctrl;
      
      xspi_ctrl::phy_dq_timing_reg_type<32> phy_dq_timing_reg;
      
      xspi_ctrl::phy_dqs_timing_reg_type<32> phy_dqs_timing_reg;
      
      xspi_ctrl::phy_gate_lpbk_ctrl_reg_type<32> phy_gate_lpbk_ctrl_reg;
      
      xspi_ctrl::phy_dll_master_ctrl_reg_type<32> phy_dll_master_ctrl_reg;
      
      xspi_ctrl::phy_dll_slave_ctrl_reg_type<32> phy_dll_slave_ctrl_reg;
      
      xspi_ctrl::phy_ie_timing_reg_type<32> phy_ie_timing_reg;
      
      xspi_ctrl::phy_obs_reg_0_type<32> phy_obs_reg_0;
      
      xspi_ctrl::phy_dll_obs_reg_0_type<32> phy_dll_obs_reg_0;
      
      xspi_ctrl::phy_dll_obs_reg_1_type<32> phy_dll_obs_reg_1;
      
      xspi_ctrl::phy_dll_obs_reg_2_type<32> phy_dll_obs_reg_2;
      
      xspi_ctrl::phy_static_togg_reg_type<32> phy_static_togg_reg;
      
      xspi_ctrl::phy_wr_deskew_reg_type<32> phy_wr_deskew_reg;
      
      xspi_ctrl::phy_wr_rd_deskew_cmd_reg_type<32> phy_wr_rd_deskew_cmd_reg;
      
      xspi_ctrl::phy_wr_deskew_pd_ctrl_0_reg_type<32> phy_wr_deskew_pd_ctrl_0_reg;
      
      xspi_ctrl::phy_wr_deskew_pd_ctrl_1_reg_type<32> phy_wr_deskew_pd_ctrl_1_reg;
      
      xspi_ctrl::phy_rd_deskew_reg_type<32> phy_rd_deskew_reg;
      
      xspi_ctrl::phy_version_reg_type<32> phy_version_reg;
      
      xspi_ctrl::phy_features_reg_type<32> phy_features_reg;
      
      xspi_ctrl::phy_ctrl_reg_type<32> phy_ctrl_reg;
      
      xspi_ctrl::phy_tsel_reg_type<32> phy_tsel_reg;
      
      xspi_ctrl::phy_gpio_ctrl_0_type<32> phy_gpio_ctrl_0;
      
      xspi_ctrl::phy_gpio_ctrl_1_type<32> phy_gpio_ctrl_1;
      
      xspi_ctrl::phy_gpio_status_0_type<32> phy_gpio_status_0;
      
      xspi_ctrl::phy_gpio_status_1_type<32> phy_gpio_status_1;
      
      void reset_all_registers();
};
