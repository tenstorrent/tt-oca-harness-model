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
 * CHECKSUM:5c61789c0c08db2f0f5993808b06b0f23ef1db2d
 ***************************************************************************/
 
#pragma once
#include "cdns_extension.h"
#include "cdns_defines.h"
#include "cdns_xspi_ctrl_regBase.h"
#include <vector>

namespace mylibrary {

/**
 * \copydoc cdns_xspi_ctrl_regBase
 */
class cdns_xspi_ctrl_reg : public cdns_xspi_ctrl_regBase {
  public:
    SC_HAS_PROCESS(cdns_xspi_ctrl_reg);

    cdns_xspi_ctrl_reg(sc_core::sc_module_name name);
    
    struct stig_instruction {
     uint8_t  opcode;
     uint8_t  instr_type;
     bool  instr_link;
     uint8_t  data_bytes;
     uint8_t  bank_num;
     uint64_t address;
     uint32_t write_data;

      // For data_phase decode only
     bool d_status_source;
     uint32_t d_data_bytes;
     bool d_DIR;     

  };
  
  private:
    friend class cdns_xspi_ctrl_regCovermodel;

    // STIG Engine Thread
    void stig_engine_thread();
    sc_core::sc_event cmd_trigger_event;

    // STIG instruction handlers
    void handle_stig_read(const stig_instruction& inst, const stig_instruction* data_phase);
    void handle_stig_write(const stig_instruction& inst);
    void handle_stig_control_command(const stig_instruction& inst, const stig_instruction* data_phase);
    void handle_stig_suspend_resume(const stig_instruction& inst);
    void handle_stig_read_sfdp(const stig_instruction& inst, const stig_instruction* data_phase);

    uint64_t extract_address(uint32_t instr_0, uint32_t instr_1, uint32_t instr_2);
    uint32_t extract_number_of_data_bytes(uint32_t instr1, uint32_t instr2);
    stig_instruction decode_instruction();
    bool execute_stig(const stig_instruction& cmd, const stig_instruction* data_phase);
    void set_busy();
    void clear_status();
    void finish();
    void set_error(const char* msg);

    void start_discovery(const uint8_t& discovery_num_lines, const uint8_t& discovery_abnum, const uint8_t& discovery_bank, const uint8_t& discovery_cmd_type,
                         const uint8_t& discovery_dummy_cnt, const uint8_t& discovery_seq_crc_en, const uint8_t& discovery_seq_crc_variant,
                         const uint8_t& discovery_seq_crc_oe, const uint8_t& discovery_seq_crc_chunk_size, const uint8_t& discovery_seq_crc_ual_chunk_en,
                         const uint8_t& discovery_extop_en);
    void configure_registers_from_sfdp(const std::vector<unsigned char>& basic_table);
    void configure_global_seq_cfg_profile1(const uint8_t& discovery_abnum, const uint8_t& discovery_cmd_type, 
                                            const uint8_t& discovery_dummy_cnt, const uint8_t& discovery_seq_crc_en,
                                            const uint8_t& discovery_seq_crc_variant, const uint8_t& discovery_seq_crc_oe,
                                            const uint8_t& discovery_seq_crc_chunk_size, const uint8_t& discovery_seq_crc_ual_chunk_en,
                                            bool full_discovery, bool sfdp_header_swapped);
    void configure_rst_seq_cfg_profile1(const uint8_t& discovery_abnum, bool full_discovery, bool sfdp_soft_reset_f0_supported);
    void configure_ers_seq_cfg_profile1(const uint8_t& discovery_abnum, bool full_discovery, bool sfdp_erase_opcode_available, uint8_t sfdp_erase_opcode);
    void configure_prog_seq_cfg_profile1(const uint8_t& discovery_num_lines, const uint8_t& discovery_abnum, bool full_discovery, const std::vector<unsigned char>& basic_table);
    void configure_read_seq_cfg_profile1(const uint8_t& discovery_num_lines, const uint8_t& discovery_abnum, const uint8_t& discovery_dummy_cnt, 
                                         const uint8_t& discovery_cmd_type, const uint8_t& discovery_extop_en, bool full_discovery, 
                                         const std::vector<unsigned char>& basic_table, bool sfdp_read_opcode_available, uint8_t sfdp_read_opcode, uint8_t sfdp_read_dummy_cycles);
    void configure_stat_seq_cfg_profile1(const uint8_t& discovery_num_lines, const uint8_t& discovery_abnum, const uint8_t& discovery_dummy_cnt,
                                         const uint8_t& discovery_cmd_type, const uint8_t& discovery_extop_en, bool full_discovery,
                                         const std::vector<unsigned char>& basic_table);
    void dump_registers(const std::string& label);

    void handle_direct_mode(tlm::tlm_generic_payload& trans);
    void handle_stig_mode(tlm::tlm_generic_payload& trans);
    void handle_acmd_mode(tlm::tlm_generic_payload& trans);

    virtual bool handle_write_cmd_reg0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_cmd_reg1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_cmd_reg2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_cmd_reg3(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_cmd_reg4(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_cmd_reg5(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_cmd_status_ptr(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_intr_enable(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_trd_comp_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_trd_error_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_trd_error_intr_en(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_long_polling(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_short_polling(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_ctrl_config(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_dma_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_discovery_control(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_xip_mode_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_global_seq_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_global_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_direct_access_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_direct_access_rmp(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_direct_access_rmp_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_rst_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_rst_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_ers_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_ers_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_ers_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_ers_seq_cfg_2_ersa_seq_p1_cmd_val(const unsigned int& value, sc_core::sc_time& time);
    virtual bool handle_write_prog_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_prog_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_prog_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_read_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_read_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_read_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_we_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_3(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_4(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_5(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_7(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_8(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_9(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_stat_seq_cfg_10(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_wp_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_reset_pin_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_clock_mode_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_jedec_rst_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_dev_delay_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_rst_recovery_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_dev_active_max_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_hf_offset_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_dll_phy_update_cnt(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_dll_phy_ctrl(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_dq_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_dqs_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_gate_lpbk_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_dll_master_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_dll_slave_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_ie_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_static_togg_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_wr_deskew_pd_ctrl_0_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_tsel_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_gpio_ctrl_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    virtual bool handle_write_phy_gpio_ctrl_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time);
    
    // Override b_transport functions for target interfaces
    virtual void b_transport_axi_slave(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    virtual void b_transport_por_input(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

  private:
        
};

}  // end of namespace mylibrary
