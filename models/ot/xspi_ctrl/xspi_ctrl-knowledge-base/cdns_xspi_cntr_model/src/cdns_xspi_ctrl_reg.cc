
/***************************************************************************
 * Generated snippet, used for detecting user edits.
 * CHECKSUM:8ed63e719ec69cd59bb857680acff1d05ee1b4b7
 ***************************************************************************/
 
#include "cdns_xspi_ctrl_reg.h"
#include <iomanip>
#include <vector>
#include <string>
#include "sfdp.h"

// EXPLICIT FORWARD DECLARATION to fix linkage issue
//extern uint32_t read_le32(const std::vector<uint8_t>& buf, size_t offset);
namespace mylibrary {

// SFDP API type and function declarations from global namespace
using ::sfdp_header_t;
using ::sfdp_parameter_header_t;
using ::jedec_basic_table_t;
using ::sfdp_addr_mode_e;
using ::ADDR_3_BYTE_ONLY;
using ::ADDR_4_BYTE_ONLY;
using ::parse_sfdp_from_bytes;
using ::addr_mode_to_string;
using ::read_le32;  // ADD THIS - now using global namespace function

cdns_xspi_ctrl_reg::cdns_xspi_ctrl_reg(sc_core::sc_module_name name) : cdns_xspi_ctrl_regBase(name)  {
    SC_THREAD(stig_engine_thread);  
}

void cdns_xspi_ctrl_reg::b_transport_por_input(tlm::tlm_generic_payload& trans,  sc_core::sc_time& delay){

	std::cout <<"\nMODEL: CDNS_XSPI_CTRL_REG: Received PoR input signal" << std::endl;

	    //get the PoR extension from the trans
	    xspi_PoR_trans* por_trans = trans.get_extension<xspi_PoR_trans>();
	    if (por_trans == nullptr) {
	        SC_REPORT_ERROR("XSPI_CTRL_REG", "PoR extension not found in trans");
	        return;
	    }

	    uint8_t discovery_num_lines = por_trans->discovery_num_lines;
	    uint8_t discovery_abnum = por_trans->discovery_abnum;
	    uint8_t discovery_bank = por_trans->discovery_bank;
	    uint8_t discovery_cmd_type = por_trans->discovery_cmd_type;
	    uint8_t discovery_dummy_cnt = por_trans->discovery_dummy_cnt;
	    uint8_t discovery_extop_val = por_trans->discovery_extop_val;
	    uint8_t discovery_extop_en = por_trans->discovery_extop_en;
	    uint8_t discovery_seq_crc_en = por_trans->discovery_seq_crc_en;
	    uint8_t discovery_seq_crc_variant = por_trans->discovery_seq_crc_variant;
	    uint8_t discovery_seq_crc_oe = por_trans->discovery_seq_crc_oe;
	    uint8_t discovery_seq_crc_chunk_size = por_trans->discovery_seq_crc_chunk_size;
	    uint8_t discovery_seq_crc_ual_chunk_en = por_trans->discovery_seq_crc_ual_chunk_en;

	    if(por_trans->discovery_inhibit == 0) {
	      std::cout << "Starting discovery process" << std::endl;
	      
	      // Dump registers before discovery
	      dump_registers("BEFORE DEVICE DISCOVERY");
	      
	      //start the discovery proces
	      start_discovery(discovery_num_lines, discovery_abnum, discovery_bank, discovery_cmd_type,
	                      discovery_dummy_cnt, discovery_seq_crc_en, discovery_seq_crc_variant,
	                      discovery_seq_crc_oe, discovery_seq_crc_chunk_size, discovery_seq_crc_ual_chunk_en,
	                      discovery_extop_en);

	      // Dump registers after discovery
	      dump_registers("AFTER DEVICE DISCOVERY");

	      //set the discovery_comp to 1. //where it should be set?//Register or in the extension signal?
	    } else {
	      std::cout << "Discovery inhibited" << std::endl;
	    }

	    std::cout <<"Settting CTRL_Status"<<std::endl;
	    ctrl_status.init_comp = 0x1;
	    ctrl_status.init_fail = 0x2;
}


// Helper function to read SFDP data at a specific address
template<typename SocketType>
static bool read_sfdp_data(SocketType& socket, uint32_t address, size_t length, std::vector<uint8_t>& data) {

    std::cout << "Reading SFDP data at address: " << address << " with length: " << length << std::endl;
    tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
    xspi_target_trans* target_trans = new xspi_target_trans();
    target_trans->xspi_target_opcode = 0x5A; // SFDP Read command
    trans->set_address(address);
    trans->set_command(tlm::TLM_READ_COMMAND);

    // Allocate data buffer for read operation
    uint8_t* data_buf = new uint8_t[length];
    trans->set_data_ptr(data_buf);
    trans->set_data_length(length);
    trans->set_extension(target_trans);

    // Send the transaction to the target
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    std::cout << "Sending transaction to target" << std::endl;
    socket->b_transport(*trans, delay);

    // Get the response data
    std::cout << "Getting response data" << std::endl;
    uint8_t* data_ptr = trans->get_data_ptr();
    bool success = false;
    if (data_ptr != nullptr && trans->get_data_length() >= length) {
        data.assign(data_ptr, data_ptr + length);
        success = true;
    }

    // Clear data pointer before destroying transaction so the payload destructor
    // does not free/delete our buffer (we use new[]; some TLM impls call free() -> invalid pointer).
    trans->set_data_ptr(nullptr);
    delete trans;  // Extensions are auto-deleted with the payload
    delete[] data_buf;  // We allocated it; we always free it

    return success;
}

// Helper function to print data in hex format
static void print_hex_data(const std::vector<uint8_t>& data, const std::string& label, size_t bytes_per_line = 16) {
    std::cout << label << " (" << data.size() << " bytes):" << std::endl;
    for (size_t i = 0; i < data.size(); i += bytes_per_line) {
        std::cout << "  ";
        for (size_t j = 0; j < bytes_per_line && (i + j) < data.size(); j++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i + j] << " ";
        }
        std::cout << std::dec << std::endl;
    }

}

// REMOVED: Duplicate read_le32() function - now using ::read_le32() from sfdp_utils.cpp

// Function to configure registers based on SFDP Basic Parameter Table
void cdns_xspi_ctrl_reg::configure_registers_from_sfdp(const std::vector<uint8_t>& basic_table) {
    if (basic_table.size() < 64) {  // Need at least 16 DWORDs (64 bytes)
        std::cout << "ERROR: Basic parameter table too small (" << basic_table.size() << " bytes)" << std::endl;
        return;
    }

    std::cout << "\nConfiguring registers from SFDP Basic Parameter Table using SFDP API..." << std::endl;

    // Construct full SFDP structure for parse_sfdp_from_bytes
    std::vector<uint8_t> full_sfdp_data;
    full_sfdp_data.reserve(16 + basic_table.size());
    
    // Create minimal SFDP header (8 bytes)
    sfdp_header_t temp_header;
    auto header_bytes = temp_header.to_bytes();
    full_sfdp_data.insert(full_sfdp_data.end(), header_bytes.begin(), header_bytes.end());
    
    // Create minimal parameter header (8 bytes) pointing to offset 0x10 (16)
    sfdp_parameter_header_t temp_param;
    temp_param.set_pointer(0x000010);
    auto param_bytes = temp_param.to_bytes();
    full_sfdp_data.insert(full_sfdp_data.end(), param_bytes.begin(), param_bytes.end());
    
    // Add the basic table data
    full_sfdp_data.insert(full_sfdp_data.end(), basic_table.begin(), basic_table.end());
    
    // Use parse_sfdp_from_bytes from sfdp_utils.cpp
    sfdp_header_t sfdp_header;
    sfdp_parameter_header_t param_header;
    jedec_basic_table_t sfdp_table;
    
    if (!parse_sfdp_from_bytes(full_sfdp_data, sfdp_header, param_header, sfdp_table)) {
        std::cout << "ERROR: Failed to parse Basic Parameter Table via parse_sfdp_from_bytes()" << std::endl;
        return;
    }

    // Extract parameters using SFDP API methods (no manual bit shifting!)
    bool write_enable_06h = sfdp_table.get_dword1().get_write_enable_opcode_select();
    sfdp_addr_mode_e address_mode = sfdp_table.get_dword1().get_address_bytes();

    // Extract read parameters using API methods
    uint8_t read_1_1_4_opcode = sfdp_table.get_dword3().get_1_1_4_opcode();
    uint8_t read_1_1_4_wait_states = sfdp_table.get_dword3().get_1_1_4_wait_states();

    uint8_t read_1_1_2_opcode = sfdp_table.get_dword4().get_1_1_2_opcode();
    uint8_t read_1_1_2_wait_states = sfdp_table.get_dword4().get_1_1_2_wait_states();

    // Extract erase opcodes using API methods
    uint8_t erase_type1_opcode = sfdp_table.get_dword8().get_erase_type1_opcode();
    uint8_t erase_type2_opcode = sfdp_table.get_dword8().get_erase_type2_opcode();

    // Determine address byte count using API (no manual bit shifting!)
    uint8_t addr_byte_count = (address_mode == ADDR_4_BYTE_ONLY) ? 4 : 3;

    // Extract opcodes from SFDP using API
    uint8_t write_enable_opcode = write_enable_06h ? 0x06 : 0x50;
    
    // Read opcode from SFDP - only use if present
    uint8_t read_opcode = 0;
    uint8_t read_dummy_cycles = 0;
    bool read_opcode_from_sfdp = false;
    
    if (read_1_1_2_opcode != 0xFF && read_1_1_2_opcode != 0x00) {
        read_opcode = read_1_1_2_opcode;
        read_dummy_cycles = read_1_1_2_wait_states;
        read_opcode_from_sfdp = true;
    } else if (read_1_1_4_opcode != 0xFF && read_1_1_4_opcode != 0x00) {
        read_opcode = read_1_1_4_opcode;
        read_dummy_cycles = read_1_1_4_wait_states;
        read_opcode_from_sfdp = true;
    }

    // Erase opcode from SFDP - only use if present
    uint8_t erase_opcode = 0;
    bool erase_opcode_from_sfdp = false;
    if (erase_type2_opcode != 0xFF && erase_type2_opcode != 0x00) {
        erase_opcode = erase_type2_opcode;
        erase_opcode_from_sfdp = true;
    } else if (erase_type1_opcode != 0xFF && erase_type1_opcode != 0x00) {
        erase_opcode = erase_type1_opcode;
        erase_opcode_from_sfdp = true;
    }

    std::cout << "\nExtracted SFDP Parameters using API:" << std::endl;
    std::cout << "  Write Enable Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)write_enable_opcode << std::dec << " (from SFDP DWORD 1 API)" << std::endl;
    if (read_opcode_from_sfdp) {
        std::cout << "  Read Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)read_opcode << std::dec << " (from SFDP API)" << std::endl;
        std::cout << "  Read Dummy Cycles: " << std::dec << (int)read_dummy_cycles << " (from SFDP API)" << std::endl;
    } else {
        std::cout << "  Read Opcode: Not found in SFDP - leaving at default" << std::endl;
    }
    if (erase_opcode_from_sfdp) {
        std::cout << "  Erase Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)erase_opcode << std::dec << " (from SFDP API)" << std::endl;
    } else {
        std::cout << "  Erase Opcode: Not found in SFDP - leaving at default" << std::endl;
    }
    std::cout << "  Address Byte Count: " << std::dec << (int)addr_byte_count 
              << " (from SFDP DWORD 1 API)" << std::endl;

    // Configure Write Enable Sequence Register
    std::cout << "\nConfiguring we_seq_cfg_0..." << std::endl;
    we_seq_cfg_0.we_seq_p1_cmd_val = write_enable_opcode;
    we_seq_cfg_0.we_seq_p1_cmd_ios = 0x0;  // 1 line, serial
    we_seq_cfg_0.we_seq_p1_cmd_edge = 0x0; // SDR
    we_seq_cfg_0.we_seq_p1_cmd_ext_en = 0x0; // disabled
    we_seq_cfg_0.we_seq_p1_cmd_ext_val = 0x0;
    we_seq_cfg_0.we_seq_p1_en = 0x1; // Enable write enable sequence
    std::cout << "  we_seq_cfg_0 configured:" << std::endl;
    std::cout << "    cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)we_seq_cfg_0.we_seq_p1_cmd_val << std::dec << std::endl;

    // Configure Program Sequence Register 0
    std::cout << "\nConfiguring prog_seq_cfg_0 (address byte count from SFDP API)..." << std::endl;
    prog_seq_cfg_0.prog_seq_p1_addr_cnt = addr_byte_count;  // From SFDP API
    std::cout << "  prog_seq_cfg_0.prog_seq_p1_addr_cnt = " << std::dec << (int)prog_seq_cfg_0.prog_seq_p1_addr_cnt 
              << " (from SFDP API)" << std::endl;
    std::cout << "  Note: prog_seq_p1_cmd_val and prog_seq_p1_dummy_cnt will be configured in Profile 1 configuration step (Table 4.42)" << std::endl;

    // Configure Program Sequence Register 1
    std::cout << "\n  prog_seq_cfg_1: Left at default values (not specified in SFDP)" << std::endl;

    // Configure Read Sequence Register 0
    std::cout << "\nConfiguring read_seq_cfg_0 (address byte count from SFDP API)..." << std::endl;
    if (read_opcode_from_sfdp) {
        read_seq_cfg_0.read_seq_p1_addr_cnt = addr_byte_count;  // From SFDP API
        std::cout << "  read_seq_cfg_0.read_seq_p1_addr_cnt = " << std::dec << (int)read_seq_cfg_0.read_seq_p1_addr_cnt 
                  << " (from SFDP API)" << std::endl;
        std::cout << "  Note: read_seq_p1_cmd_val and read_seq_p1_dummy_cnt will be configured in Profile 1 configuration step (Table 4.45)" << std::endl;
    } else {
        std::cout << "  read_seq_cfg_0: Address byte count not configured (read opcode not found in SFDP)" << std::endl;
    }

    // Configure Read Sequence Register 1
    std::cout << "\n  read_seq_cfg_1: Mode byte parameters will be configured in Profile 1 configuration step (Table 4.46)" << std::endl;

    // Note: Erase sequence registers will be configured in configure_ers_seq_cfg_profile1()
    std::cout << "\nErase sequence registers will be configured in Profile 1 configuration step (Table 4.37)" << std::endl;

    // Configure Status Check Sequence Registers
    std::cout << "\n  Status sequence registers will be configured in Profile 1 configuration step (Table 4.50)" << std::endl;

    std::cout << "\nRegister configuration complete using SFDP API!" << std::endl;
}

// Function to configure global_seq_cfg and global_seq_cfg_1 registers for Profile 1
void cdns_xspi_ctrl_reg::configure_global_seq_cfg_profile1(const uint8_t& discovery_abnum, const uint8_t& discovery_cmd_type, 
                                                             const uint8_t& discovery_dummy_cnt, const uint8_t& discovery_seq_crc_en,
                                                             const uint8_t& discovery_seq_crc_variant, const uint8_t& discovery_seq_crc_oe,
                                                             const uint8_t& discovery_seq_crc_chunk_size, const uint8_t& discovery_seq_crc_ual_chunk_en,
                                                             bool full_discovery, bool sfdp_header_swapped) {
    std::cout << "\n=== Configuring global_seq_cfg and global_seq_cfg_1 for Profile 1 ===" << std::endl;
    std::cout << "Note: Updating fields as explicitly specified in Table 4.36 for Profile 1." << std::endl;

    // Configure global_seq_cfg register (0x390)
    std::cout << "\nConfiguring global_seq_cfg..." << std::endl;
    
    global_seq_cfg.seq_type = 0x0;
    std::cout << "  seq_type = 0x0 (Profile 1 - Legacy SPI/xSPI Profile 1.0, per Table 4.36)" << std::endl;
    
    global_seq_cfg.seq_data_per_addr = 0x0;
    std::cout << "  seq_data_per_addr = 0x0 (for Legacy SPI/xSPI Profile 1.0, per Table 4.36)" << std::endl;
    
    if (full_discovery) {
        global_seq_cfg.seq_page_size_pgm = 8;
        std::cout << "  seq_page_size_pgm: Set to 8 (should be read from SFDP/Parameter Page in full discovery, but not available in Basic Parameter Table)" << std::endl;
    } else {
        global_seq_cfg.seq_page_size_pgm = 8;
        std::cout << "  seq_page_size_pgm = 8 (for Legacy SPI/xSPI Profile 1.0 - not full discovery, per Table 4.36)" << std::endl;
    }
    
    global_seq_cfg.seq_page_size_rd = 15;
    std::cout << "  seq_page_size_rd = 15 (for Profile 1, per Table 4.36)" << std::endl;
    
    if (full_discovery) {
        global_seq_cfg.seq_data_swap = sfdp_header_swapped ? 1 : 0;
        std::cout << "  seq_data_swap = " << (int)global_seq_cfg.seq_data_swap 
                  << " (from SFDP header swap detection in Octal DDR mode - full discovery)" << std::endl;
    } else {
        bool octal_ddr_mode = (discovery_cmd_type == 1);
        bool four_addr_bytes = (discovery_abnum == 1);
        bool twenty_dummy_cycles = (discovery_dummy_cnt == 1);
        global_seq_cfg.seq_data_swap = (octal_ddr_mode && four_addr_bytes && twenty_dummy_cycles) ? 1 : 0;
        std::cout << "  seq_data_swap = " << (int)global_seq_cfg.seq_data_swap 
                  << " (xSPI Profile 1.0: Octal-DDR=" << (octal_ddr_mode ? "yes" : "no")
                  << ", 4 addr bytes=" << (four_addr_bytes ? "yes" : "no")
                  << ", 20 dummy cycles=" << (twenty_dummy_cycles ? "yes" : "no") << " - not full discovery)" << std::endl;
    }
    
    global_seq_cfg.seq_crc_en = discovery_seq_crc_en;
    std::cout << "  seq_crc_en = " << (int)global_seq_cfg.seq_crc_en 
              << " (from discovery_seq_crc_en bootstrap signal)" << std::endl;
    
    global_seq_cfg.seq_crc_variant = discovery_seq_crc_variant;
    std::cout << "  seq_crc_variant = " << (int)global_seq_cfg.seq_crc_variant 
              << " (from discovery_seq_crc_variant bootstrap signal)" << std::endl;
    
    global_seq_cfg.seq_crc_oe = discovery_seq_crc_oe;
    std::cout << "  seq_crc_oe = " << (int)global_seq_cfg.seq_crc_oe 
              << " (from discovery_seq_crc_oe bootstrap signal)" << std::endl;
    
    global_seq_cfg.seq_crc_chunk_size = discovery_seq_crc_chunk_size;
    std::cout << "  seq_crc_chunk_size = " << (int)global_seq_cfg.seq_crc_chunk_size 
              << " (from discovery_seq_crc_chunk_size bootstrap signal)" << std::endl;
    
    global_seq_cfg.seq_crc_ual_chunk_en = discovery_seq_crc_ual_chunk_en;
    std::cout << "  seq_crc_ual_chunk_en = " << (int)global_seq_cfg.seq_crc_ual_chunk_en 
              << " (from discovery_seq_crc_ual_chunk_en bootstrap signal)" << std::endl;
    
    std::cout << "  seq_crc_ual_chunk_chk: Left at default value (discovery_seq_crc_ual_chunk_chk signal not in extension)" << std::endl;
    
    global_seq_cfg.seq_tcms_en = 0;
    std::cout << "  seq_tcms_en = 0 (fixed for Profile 1, per Table 4.36)" << std::endl;

    // Configure global_seq_cfg_1 register (0x394)
    std::cout << "\nConfiguring global_seq_cfg_1..." << std::endl;
    
    if (discovery_dummy_cnt == 0) {
        global_seq_cfg_1.seq_page_size_ext = 64;
        std::cout << "  seq_page_size_ext = 64 (derived from discovery_dummy_cnt = 0)" << std::endl;
    } else {
        global_seq_cfg_1.seq_page_size_ext = 128;
        std::cout << "  seq_page_size_ext = 128 (derived from discovery_dummy_cnt != 0)" << std::endl;
    }
    
    global_seq_cfg_1.seq_page_ca_size = discovery_abnum;
    std::cout << "  seq_page_ca_size = " << (int)global_seq_cfg_1.seq_page_ca_size 
              << " (from discovery_abnum: " << (discovery_abnum == 0 ? "3-byte" : "4-byte") << " addressing)" << std::endl;
    
    global_seq_cfg_1.seq_plane_cnt = discovery_cmd_type;
    std::cout << "  seq_plane_cnt = " << (int)global_seq_cfg_1.seq_plane_cnt 
              << " (from discovery_cmd_type)" << std::endl;
    
    global_seq_cfg_1.seq_page_per_block = 6;
    std::cout << "  seq_page_per_block = 6 (fixed: 64 pages per block = 2^6, per Table 4.36)" << std::endl;
    
    std::cout << "\n=== global_seq_cfg and global_seq_cfg_1 configuration complete for Profile 1 ===" << std::endl;
}

// Function to configure rst_seq_cfg_0 and rst_seq_cfg_1 registers for Profile 1
void cdns_xspi_ctrl_reg::configure_rst_seq_cfg_profile1(const uint8_t& discovery_abnum, bool full_discovery, bool sfdp_soft_reset_f0_supported) {
    std::cout << "\n=== Configuring rst_seq_cfg_0 and rst_seq_cfg_1 for Profile 1 ===" << std::endl;
    std::cout << "Note: Updating fields as explicitly specified in Table 4.37 for Profile 1." << std::endl;

    std::cout << "\nConfiguring rst_seq_cfg_0..." << std::endl;
    
    rst_seq_cfg_0.rst_seq_p1_cmd0_val = 0x66;
    std::cout << "  rst_seq_p1_cmd0_val = 0x66 (always, per Table 4.37)" << std::endl;
    
    if (full_discovery) {
        rst_seq_cfg_0.rst_seq_p1_cmd0_en = sfdp_soft_reset_f0_supported ? 0 : 1;
        std::cout << "  rst_seq_p1_cmd0_en = " << (int)rst_seq_cfg_0.rst_seq_p1_cmd0_en 
                  << " (full discovery: 0 if Soft Reset 0xF0 supported, otherwise 1, per Table 4.37)" << std::endl;
    } else {
        rst_seq_cfg_0.rst_seq_p1_cmd0_en = 1;
        std::cout << "  rst_seq_p1_cmd0_en = 1 (not full discovery: always 1, per Table 4.37)" << std::endl;
    }
    
    if (full_discovery) {
        rst_seq_cfg_0.rst_seq_p1_cmd1_val = sfdp_soft_reset_f0_supported ? 0xF0 : 0x99;
        std::cout << "  rst_seq_p1_cmd1_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)rst_seq_cfg_0.rst_seq_p1_cmd1_val << std::dec 
                  << " (full discovery: 0xF0 if Soft Reset supported, otherwise 0x99, per Table 4.37)" << std::endl;
    } else {
        rst_seq_cfg_0.rst_seq_p1_cmd1_val = 0x99;
        std::cout << "  rst_seq_p1_cmd1_val = 0x99 (not full discovery: always 0x99, per Table 4.37)" << std::endl;
    }
    
    rst_seq_cfg_1.rst_seq_p1_data_val = 0xD0;
    std::cout << "  rst_seq_p1_data_val = 0xD0 (always, per Table 4.37)" << std::endl;
    
    rst_seq_cfg_0.rst_seq_p1_data_en = 0;
    std::cout << "  rst_seq_p1_data_en = 0 (always, per Table 4.37)" << std::endl;
    
    std::cout << "\n=== rst_seq_cfg_0 and rst_seq_cfg_1 configuration complete for Profile 1 ===" << std::endl;
}

// Function to configure ers_seq_cfg_0, ers_seq_cfg_1, and ers_seq_cfg_2 registers for Profile 1
void cdns_xspi_ctrl_reg::configure_ers_seq_cfg_profile1(const uint8_t& discovery_abnum, bool full_discovery, bool sfdp_erase_opcode_available, uint8_t sfdp_erase_opcode) {
    std::cout << "\n=== Configuring ers_seq_cfg_0, ers_seq_cfg_1, and ers_seq_cfg_2 for Profile 1 ===" << std::endl;
    std::cout << "Note: Updating fields as explicitly specified in Table 4.37 for Profile 1." << std::endl;

    std::cout << "\nConfiguring ers_seq_cfg_0..." << std::endl;
    
    if (full_discovery && sfdp_erase_opcode_available) {
        ers_seq_cfg_0.erss_seq_p1_cmd_val = sfdp_erase_opcode;
        std::cout << "  erss_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)ers_seq_cfg_0.erss_seq_p1_cmd_val << std::dec 
                  << " (from SFDP database, per Table 4.37 - full discovery)" << std::endl;
    } else {
        if (discovery_abnum == 0) {
            ers_seq_cfg_0.erss_seq_p1_cmd_val = 0x20;
            std::cout << "  erss_seq_p1_cmd_val = 0x20 (discovery_abnum = 0, 3-byte addressing, per Table 4.37 - not full discovery)" << std::endl;
        } else {
            ers_seq_cfg_0.erss_seq_p1_cmd_val = 0x21;
            std::cout << "  erss_seq_p1_cmd_val = 0x21 (discovery_abnum = 1, 4-byte addressing, per Table 4.37 - not full discovery)" << std::endl;
        }
    }

    std::cout << "\nConfiguring ers_seq_cfg_1..." << std::endl;
    
    ers_seq_cfg_1.erss_seq_p1_sect_size = 12;
    if (full_discovery) {
        std::cout << "  erss_seq_p1_sect_size = 12 (4KB = 2^12, default - SFDP does not provide sector size directly, per Table 4.37 - full discovery)" << std::endl;
    } else {
        std::cout << "  erss_seq_p1_sect_size = 12 (4KB = 2^12, always for not full discovery, per Table 4.37)" << std::endl;
    }

    std::cout << "\nConfiguring ers_seq_cfg_2..." << std::endl;
    
    ers_seq_cfg_2.ersa_seq_p1_cmd_val = 0xC7;
    std::cout << "  ersa_seq_p1_cmd_val = 0xC7 (always, per Table 4.37)" << std::endl;
    
    std::cout << "\n=== ers_seq_cfg_0, ers_seq_cfg_1, and ers_seq_cfg_2 configuration complete for Profile 1 ===" << std::endl;
}

// Function to configure prog_seq_cfg_0 and prog_seq_cfg_1 registers for Profile 1
void cdns_xspi_ctrl_reg::configure_prog_seq_cfg_profile1(const uint8_t& discovery_num_lines, const uint8_t& discovery_abnum, bool full_discovery, const std::vector<uint8_t>& basic_table) {
    std::cout << "\n=== Configuring prog_seq_cfg_0 and prog_seq_cfg_1 for Profile 1 ===" << std::endl;
    std::cout << "Note: Updating fields as explicitly specified in Table 4.42 for Profile 1 using SFDP API." << std::endl;

    bool is_single_mode = (discovery_num_lines == 0x1);
    bool is_dual_quad_octal_mode = (discovery_num_lines == 0x2 || discovery_num_lines == 0x4 || discovery_num_lines == 0x8);
    
    std::cout << "\nConfiguring prog_seq_cfg_0..." << std::endl;
    std::cout << "  Mode: " << (is_single_mode ? "SINGLE" : (is_dual_quad_octal_mode ? "DUAL/QUAD/OCTAL" : "AUTO/OTHER")) << std::endl;
    
    uint8_t prog_cmd_val = 0x02;
    
    if (is_single_mode || is_dual_quad_octal_mode) {
        if (full_discovery && basic_table.size() >= 64) {
            // Construct full SFDP structure for parse_sfdp_from_bytes
            std::vector<uint8_t> full_sfdp_data;
            full_sfdp_data.reserve(16 + basic_table.size());
            
            sfdp_header_t temp_header;
            auto header_bytes = temp_header.to_bytes();
            full_sfdp_data.insert(full_sfdp_data.end(), header_bytes.begin(), header_bytes.end());
            
            sfdp_parameter_header_t temp_param;
            temp_param.set_pointer(0x000010);
            auto param_bytes = temp_param.to_bytes();
            full_sfdp_data.insert(full_sfdp_data.end(), param_bytes.begin(), param_bytes.end());
            
            full_sfdp_data.insert(full_sfdp_data.end(), basic_table.begin(), basic_table.end());
            
            // Use parse_sfdp_from_bytes from sfdp_utils.cpp
            sfdp_header_t sfdp_header;
            sfdp_parameter_header_t param_header;
            jedec_basic_table_t sfdp_table;
            
            if (!parse_sfdp_from_bytes(full_sfdp_data, sfdp_header, param_header, sfdp_table)) {
                std::cout << "WARNING: parse_sfdp_from_bytes() failed while configuring prog_seq (falling back to defaults)" << std::endl;
            }
            
            uint8_t prog_1_1_4_opcode = (basic_table.size() > 44) ? basic_table[44] : 0xFF;
            uint8_t prog_1_4_4_opcode = (basic_table.size() > 45) ? basic_table[45] : 0xFF;
            
            if (prog_1_4_4_opcode != 0xFF && prog_1_4_4_opcode != 0x00) {
                prog_cmd_val = prog_1_4_4_opcode;
                std::cout << "  prog_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)prog_cmd_val << std::dec 
                          << " (1-4-4 via SFDP DWORD 11, per Table 4.42 - full discovery)" << std::endl;
            }
            else if (prog_1_1_4_opcode != 0xFF && prog_1_1_4_opcode != 0x00) {
                prog_cmd_val = prog_1_1_4_opcode;
                std::cout << "  prog_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)prog_cmd_val << std::dec 
                          << " (1-1-4 via SFDP DWORD 11, per Table 4.42 - full discovery)" << std::endl;
            }
            else {
                prog_cmd_val = 0x02;
                std::cout << "  prog_seq_p1_cmd_val = 0x02 (legacy - SFDP DWORD 11 does not advertise program opcodes, per JEDEC JESD216A and Table 4.42 - full discovery)" << std::endl;
            }
        } else {
            prog_cmd_val = 0x02;
            std::cout << "  prog_seq_p1_cmd_val = 0x02 (legacy Page Program - " 
                      << (is_single_mode ? "SINGLE" : "DUAL/QUAD/OCTAL") 
                      << " mode, not full discovery, per JEDEC JESD216A)" << std::endl;
        }
    } else {
        prog_cmd_val = 0x02;
        std::cout << "  prog_seq_p1_cmd_val = 0x02 (legacy Page Program - AUTO/OTHER mode, per JEDEC JESD216A)" << std::endl;
    }
    
    prog_seq_cfg_0.prog_seq_p1_cmd_val = prog_cmd_val;
    
    prog_seq_cfg_0.prog_seq_p1_dummy_cnt = 0;
    std::cout << "  prog_seq_p1_dummy_cnt = 0 (always, per Table 4.42)" << std::endl;
    
    std::cout << "\n=== prog_seq_cfg_0 configuration complete for Profile 1 ===" << std::endl;
}

// Function to configure read_seq_cfg_0, read_seq_cfg_1, and xip_mode_cfg registers for Profile 1
void cdns_xspi_ctrl_reg::configure_read_seq_cfg_profile1(const uint8_t& discovery_num_lines, const uint8_t& discovery_abnum, const uint8_t& discovery_dummy_cnt, 
                                                         const uint8_t& discovery_cmd_type, const uint8_t& discovery_extop_en, bool full_discovery, 
                                                         const std::vector<uint8_t>& basic_table, bool sfdp_read_opcode_available, uint8_t sfdp_read_opcode, uint8_t sfdp_read_dummy_cycles) {
    std::cout << "\n=== Configuring read_seq_cfg_0, read_seq_cfg_1, and xip_mode_cfg for Profile 1 ===" << std::endl;
    std::cout << "Note: Updating fields as explicitly specified in Table 4.45 and Table 4.46 for Profile 1 using SFDP API." << std::endl;

    bool is_dual_mode = (discovery_num_lines == 0x2);
    bool is_quad_mode = (discovery_num_lines == 0x4);
    bool is_octal_mode = (discovery_num_lines == 0x8);
    
    std::cout << "\nConfiguring read_seq_cfg_0..." << std::endl;
    std::cout << "  Mode: " << (is_dual_mode ? "DUAL" : (is_quad_mode ? "QUAD" : (is_octal_mode ? "OCTAL" : "SINGLE/OTHER"))) << std::endl;
    
    sfdp_addr_mode_e address_mode = ADDR_3_BYTE_ONLY;
    if (full_discovery && basic_table.size() >= 64) {
        // Construct full SFDP structure for parse_sfdp_from_bytes
        std::vector<uint8_t> full_sfdp_data;
        full_sfdp_data.reserve(16 + basic_table.size());
        
        sfdp_header_t temp_header;
        auto header_bytes = temp_header.to_bytes();
        full_sfdp_data.insert(full_sfdp_data.end(), header_bytes.begin(), header_bytes.end());
        
        sfdp_parameter_header_t temp_param;
        temp_param.set_pointer(0x000010);
        auto param_bytes = temp_param.to_bytes();
        full_sfdp_data.insert(full_sfdp_data.end(), param_bytes.begin(), param_bytes.end());
        
        full_sfdp_data.insert(full_sfdp_data.end(), basic_table.begin(), basic_table.end());
        
        // Use parse_sfdp_from_bytes from sfdp_utils.cpp
        sfdp_header_t sfdp_header;
        sfdp_parameter_header_t param_header;
        jedec_basic_table_t sfdp_table;
        
        if (parse_sfdp_from_bytes(full_sfdp_data, sfdp_header, param_header, sfdp_table)) {
            address_mode = sfdp_table.get_dword1().get_address_bytes();
        } else {
            std::cout << "WARNING: parse_sfdp_from_bytes() failed while configuring read_seq (using default address mode)" << std::endl;
        }
    }
    
    if (is_dual_mode) {
        if (full_discovery && sfdp_read_opcode_available) {
            read_seq_cfg_0.read_seq_p1_dummy_cnt = sfdp_read_dummy_cycles;
            std::cout << "  read_seq_p1_dummy_cnt = " << (int)read_seq_cfg_0.read_seq_p1_dummy_cnt 
                      << " (from SFDP database via API, per Table 4.45 - full discovery)" << std::endl;
        } else {
            read_seq_cfg_0.read_seq_p1_dummy_cnt = (discovery_dummy_cnt == 0) ? 8 : 10;
            std::cout << "  read_seq_p1_dummy_cnt = " << (int)read_seq_cfg_0.read_seq_p1_dummy_cnt 
                      << " (discovery_dummy_cnt = " << (int)discovery_dummy_cnt << ", per Table 4.45 - not full discovery)" << std::endl;
        }
    } else if (is_quad_mode) {
        if (full_discovery && sfdp_read_opcode_available) {
            read_seq_cfg_0.read_seq_p1_cmd_val = sfdp_read_opcode;
            std::cout << "  read_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)read_seq_cfg_0.read_seq_p1_cmd_val << std::dec 
                      << " (from SFDP database via API, per Table 4.45 - full discovery)" << std::endl;
        } else {
            read_seq_cfg_0.read_seq_p1_cmd_val = (discovery_abnum == 0) ? 0x0B : 0x0C;
            std::cout << "  read_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)read_seq_cfg_0.read_seq_p1_cmd_val << std::dec 
                      << " (discovery_abnum = " << (int)discovery_abnum << ", per Table 4.45 - not full discovery)" << std::endl;
        }
        
        if (full_discovery && sfdp_read_opcode_available) {
            read_seq_cfg_0.read_seq_p1_dummy_cnt = sfdp_read_dummy_cycles;
            std::cout << "  read_seq_p1_dummy_cnt = " << (int)read_seq_cfg_0.read_seq_p1_dummy_cnt 
                      << " (from SFDP database via API, per Table 4.45 - full discovery)" << std::endl;
        } else {
            read_seq_cfg_0.read_seq_p1_dummy_cnt = 8;
            std::cout << "  read_seq_p1_dummy_cnt = 8 (default for QUAD mode, per Table 4.45)" << std::endl;
        }
    } else if (is_octal_mode) {
        if (full_discovery && basic_table.size() >= 64) {
            bool supports_4byte_addr = (address_mode == ADDR_4_BYTE_ONLY);
            
            if (sfdp_read_opcode_available && (sfdp_read_opcode == 0x0C || supports_4byte_addr)) {
                read_seq_cfg_0.read_seq_p1_cmd_val = 0x0C;
                std::cout << "  read_seq_p1_cmd_val = 0x0C (SFDP API indicates support, per Table 4.45 - full discovery)" << std::endl;
            } else {
                read_seq_cfg_0.read_seq_p1_cmd_val = 0x0B;
                std::cout << "  read_seq_p1_cmd_val = 0x0B (SFDP API does not indicate 0x0C support, per Table 4.45 - full discovery)" << std::endl;
            }
        } else {
            bool cmd_type_bit0 = (discovery_cmd_type & 0x1) == 0;
            
            if (discovery_abnum == 1 && discovery_dummy_cnt == 1 && cmd_type_bit0 && discovery_extop_en == 1) {
                read_seq_cfg_0.read_seq_p1_cmd_val = 0xEC;
                std::cout << "  read_seq_p1_cmd_val = 0xEC (discovery_abnum=1, discovery_dummy_cnt=1, cmd_type[0]=0, extop_en=1, per Table 4.45 - not full discovery)" << std::endl;
            } else if (discovery_abnum == 1 && discovery_dummy_cnt == 1 && !cmd_type_bit0) {
                read_seq_cfg_0.read_seq_p1_cmd_val = 0xEE;
                std::cout << "  read_seq_p1_cmd_val = 0xEE (discovery_abnum=1, discovery_dummy_cnt=1, cmd_type[0]=1, per Table 4.45 - not full discovery)" << std::endl;
            } else if (discovery_abnum == 0) {
                read_seq_cfg_0.read_seq_p1_cmd_val = 0x0B;
                std::cout << "  read_seq_p1_cmd_val = 0x0B (discovery_abnum = 0, per Table 4.45 - not full discovery)" << std::endl;
            } else {
                read_seq_cfg_0.read_seq_p1_cmd_val = 0x0C;
                std::cout << "  read_seq_p1_cmd_val = 0x0C (discovery_abnum = 1, per Table 4.45 - not full discovery)" << std::endl;
            }
        }
        
        read_seq_cfg_0.read_seq_p1_dummy_cnt = (discovery_dummy_cnt == 0) ? 16 : 20;
        if (full_discovery && sfdp_read_opcode_available) {
            std::cout << "  read_seq_p1_dummy_cnt = " << (int)read_seq_cfg_0.read_seq_p1_dummy_cnt 
                      << " (" << (discovery_dummy_cnt == 0 ? "8" : "20") << " cycles used for detection, per Table 4.45 - full discovery)" << std::endl;
        } else {
            std::cout << "  read_seq_p1_dummy_cnt = " << (int)read_seq_cfg_0.read_seq_p1_dummy_cnt 
                      << " (discovery_dummy_cnt = " << (int)discovery_dummy_cnt << ", per Table 4.45 - not full discovery)" << std::endl;
        }
    } else {
        if (full_discovery && sfdp_read_opcode_available) {
            read_seq_cfg_0.read_seq_p1_cmd_val = sfdp_read_opcode;
            read_seq_cfg_0.read_seq_p1_dummy_cnt = sfdp_read_dummy_cycles;
            std::cout << "  read_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)read_seq_cfg_0.read_seq_p1_cmd_val << std::dec << " (from SFDP API, SINGLE/OTHER mode)" << std::endl;
            std::cout << "  read_seq_p1_dummy_cnt = " << (int)read_seq_cfg_0.read_seq_p1_dummy_cnt 
                      << " (from SFDP API, SINGLE/OTHER mode)" << std::endl;
        } else {
            std::cout << "  read_seq_cfg_0: Left at default values (SINGLE/OTHER mode, no SFDP data)" << std::endl;
        }
    }

    std::cout << "\nConfiguring read_seq_cfg_1 (mode byte parameters)..." << std::endl;
    
    uint8_t addr_ios = 0;
    if (is_dual_mode) {
        addr_ios = 1;
    } else if (is_quad_mode) {
        addr_ios = 2;
    } else if (is_octal_mode) {
        addr_ios = 3;
    }
    
    uint8_t addr_edge = (discovery_cmd_type == 1) ? 1 : 0;
    
    uint8_t mb_factor;
    if (addr_ios == 3 && addr_edge == 1) {
        mb_factor = 1;
    } else {
        int exponent = 3 - addr_ios - addr_edge;
        if (exponent >= 0 && exponent <= 3) {
            mb_factor = 1 << exponent;
        } else {
            mb_factor = 1;
        }
    }
    
    if (read_seq_cfg_0.read_seq_p1_dummy_cnt >= mb_factor) {
        read_seq_cfg_1.read_seq_p1_mb_dummy_cnt = read_seq_cfg_0.read_seq_p1_dummy_cnt - mb_factor;
    } else {
        read_seq_cfg_1.read_seq_p1_mb_dummy_cnt = 0;
    }
    std::cout << "  read_seq_p1_mb_dummy_cnt = " << (int)read_seq_cfg_1.read_seq_p1_mb_dummy_cnt 
              << " (calculated: read_seq_p1_dummy_cnt=" << (int)read_seq_cfg_0.read_seq_p1_dummy_cnt 
              << " - mb_factor=" << (int)mb_factor << " [addr_ios=" << (int)addr_ios 
              << ", addr_edge=" << (int)addr_edge << "], per Table 4.46)" << std::endl;
    
    read_seq_cfg_1.read_seq_p1_mb_en = 0;
    if (full_discovery) {
        std::cout << "  read_seq_p1_mb_en = 0 (SFDP API: Basic Parameter Table does not indicate mode byte support, per Table 4.46 - full discovery)" << std::endl;
    } else {
        std::cout << "  read_seq_p1_mb_en = 0 (always 0 for not full discovery, per Table 4.46)" << std::endl;
    }

    std::cout << "\nConfiguring xip_mode_cfg..." << std::endl;
    
    xip_mode_cfg.xip_dis_mb_val = 0xFF;
    std::cout << "  xip_dis_mb_val = 0xFF (always, per Table 4.46)" << std::endl;
    
    std::cout << "\n=== read_seq_cfg_0, read_seq_cfg_1, and xip_mode_cfg configuration complete for Profile 1 ===" << std::endl;
}

// Function to configure stat_seq_cfg_0, stat_seq_cfg_1, and stat_seq_cfg_2 registers for Profile 1
void cdns_xspi_ctrl_reg::configure_stat_seq_cfg_profile1(const uint8_t& discovery_num_lines, const uint8_t& discovery_abnum, const uint8_t& discovery_dummy_cnt,
                                                         const uint8_t& discovery_cmd_type, const uint8_t& discovery_extop_en, bool full_discovery,
                                                         const std::vector<uint8_t>& basic_table) {
    std::cout << "\n=== Configuring stat_seq_cfg_0, stat_seq_cfg_1, and stat_seq_cfg_2 for Profile 1 ===" << std::endl;
    std::cout << "Note: Updating fields as explicitly specified in Table 4.50 for Profile 1 using SFDP API." << std::endl;

    bool poll_status_legacy = false;
    
    if (full_discovery && basic_table.size() >= 64) {
        // Construct full SFDP structure for parse_sfdp_from_bytes
        std::vector<uint8_t> full_sfdp_data;
        full_sfdp_data.reserve(16 + basic_table.size());
        
        sfdp_header_t temp_header;
        auto header_bytes = temp_header.to_bytes();
        full_sfdp_data.insert(full_sfdp_data.end(), header_bytes.begin(), header_bytes.end());
        
        sfdp_parameter_header_t temp_param;
        temp_param.set_pointer(0x000010);
        auto param_bytes = temp_param.to_bytes();
        full_sfdp_data.insert(full_sfdp_data.end(), param_bytes.begin(), param_bytes.end());
        
        full_sfdp_data.insert(full_sfdp_data.end(), basic_table.begin(), basic_table.end());
        
        // Use parse_sfdp_from_bytes from sfdp_utils.cpp
        sfdp_header_t sfdp_header;
        sfdp_parameter_header_t param_header;
        jedec_basic_table_t sfdp_table;
        
        if (parse_sfdp_from_bytes(full_sfdp_data, sfdp_header, param_header, sfdp_table)) {
            poll_status_legacy = sfdp_table.get_dword14().get_poll_status_legacy();
        } else {
            std::cout << "WARNING: parse_sfdp_from_bytes() failed while configuring stat_seq (using default)" << std::endl;
        }
    }
    
    std::cout << "\nConfiguring stat_seq_cfg_0..." << std::endl;
    
    stat_seq_cfg_0.stat_seq_p1_addr_cnt = 0;
    if (full_discovery && basic_table.size() >= 64) {
        if (poll_status_legacy) {
            std::cout << "  stat_seq_p1_addr_cnt = 0 (legacy polling supported - NO address phase required, per JEDEC JESD216A and Table 4.50 - full discovery)" << std::endl;
        } else {
            std::cout << "  stat_seq_p1_addr_cnt = 0 (default - SFDP API, per Table 4.50 - full discovery)" << std::endl;
        }
    } else {
        std::cout << "  stat_seq_p1_addr_cnt = 0 (legacy - no address phase in status reads, per JEDEC default)" << std::endl;
    }

    std::cout << "\nConfiguring stat_seq_cfg_1..." << std::endl;
    
    stat_seq_cfg_1.stat_seq_p1_dev_rdy_addr_en = 0;
    stat_seq_cfg_1.stat_seq_p1_dev_rdy_dummy_cnt = 0;
    stat_seq_cfg_1.stat_seq_p1_prog_fail_addr_en = 0;
    stat_seq_cfg_1.stat_seq_p1_prog_fail_dummy_cnt = 0;
    stat_seq_cfg_1.stat_seq_p1_ers_fail_addr_en = 0;
    stat_seq_cfg_1.stat_seq_p1_ers_fail_dummy_cnt = 0;
    
    if (full_discovery && basic_table.size() >= 64) {
        if (poll_status_legacy) {
            std::cout << "  All stat_seq_p1 fields = 0 (legacy polling - NO address phase/dummy cycles, per JEDEC JESD216A and Table 4.50 - full discovery)" << std::endl;
        } else {
            std::cout << "  All stat_seq_p1 fields = 0 (default - SFDP API, per Table 4.50 - full discovery)" << std::endl;
        }
    } else {
        std::cout << "  All stat_seq_p1 fields = 0 (legacy - no address phase/dummy cycles, per JEDEC default)" << std::endl;
    }

    std::cout << "\nConfiguring stat_seq_cfg_2..." << std::endl;
    
    stat_seq_cfg_2.stat_seq_p1_dev_rdy_cmd_val = 0x05;
    stat_seq_cfg_2.stat_seq_p1_prog_fail_cmd_val = 0x05;
    stat_seq_cfg_2.stat_seq_p1_ers_fail_cmd_val = 0x05;
    
    if (full_discovery && basic_table.size() >= 64) {
        if (poll_status_legacy) {
            std::cout << "  All command values = 0x05 (SFDP API indicates legacy status polling support, per Table 4.50 - full discovery)" << std::endl;
        } else {
            std::cout << "  All command values = 0x05 (default - SFDP API, per Table 4.50 - full discovery)" << std::endl;
        }
    } else {
        std::cout << "  All command values = 0x05 (always 0x05 for not full discovery, per Table 4.50)" << std::endl;
    }
    
    std::cout << "\n=== stat_seq_cfg_0, stat_seq_cfg_1, and stat_seq_cfg_2 configuration complete for Profile 1 ===" << std::endl;
}

void cdns_xspi_ctrl_reg::start_discovery(const uint8_t& discovery_num_lines, const uint8_t& discovery_abnum, const uint8_t& discovery_bank, const uint8_t& discovery_cmd_type,
                                          const uint8_t& discovery_dummy_cnt, const uint8_t& discovery_seq_crc_en, const uint8_t& discovery_seq_crc_variant,
                                          const uint8_t& discovery_seq_crc_oe, const uint8_t& discovery_seq_crc_chunk_size, const uint8_t& discovery_seq_crc_ual_chunk_en,
                                          const uint8_t& discovery_extop_en){
    std::cout << "\n=== Starting SFDP Discovery Process ===" << std::endl;

    if(discovery_bank >= xspi_bus_socket.size()) {
      SC_REPORT_ERROR("XSPI_CTRL_REG", "Discovery bank out of range");
      return;
    }

    // Step 1: Read SFDP Header (8 bytes at offset 0x00)
    std::cout << "\n--- Step 1: Reading SFDP Header ---" << std::endl;
    std::vector<uint8_t> header_data;
    if (!read_sfdp_data(xspi_bus_socket[discovery_bank], 0x00, 8, header_data)) {
        std::cout << "ERROR: Failed to read SFDP header" << std::endl;
        return;
    }

    print_hex_data(header_data, "SFDP Header");

    // Parse SFDP header using ::read_le32() from global namespace
    uint32_t signature = ::read_le32(header_data, 0);
    uint8_t minor_rev = header_data[4];
    uint8_t major_rev = header_data[5];
    uint8_t nph = header_data[6];
    uint8_t unused = header_data[7];

    std::cout << "\nSFDP Header Details:" << std::endl;
    std::cout << "  Signature: 0x" << std::hex << std::setw(8) << std::setfill('0') << signature;
    if (signature == 0x50444653) {
        std::cout << " (SFDP - Valid)" << std::endl;
    } else {
        std::cout << " (Invalid - Expected 0x50444653)" << std::endl;
    }
    std::cout << "  Major Revision: " << std::dec << (int)major_rev << std::endl;
    std::cout << "  Minor Revision: " << std::dec << (int)minor_rev << std::endl;
    std::cout << "  Number of Parameter Headers: " << std::dec << (int)(nph + 1) << " (NPH=" << (int)nph << ")" << std::endl;
    std::cout << "  Unused: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)unused << std::dec << std::endl;

    // Step 2: Read Parameter Headers
    std::cout << "\n--- Step 2: Reading Parameter Headers ---" << std::endl;
    uint8_t num_param_headers = nph + 1;
    std::vector<std::vector<uint8_t>> param_headers;
    std::vector<uint32_t> param_table_pointers;

    for (uint8_t i = 0; i < num_param_headers; i++) {
        uint32_t ph_offset = 0x08 + (i * 8);
        std::vector<uint8_t> ph_data;
        
        std::cout << "\nReading Parameter Header #" << (int)i << " at offset 0x" << std::hex << ph_offset << std::dec << "..." << std::endl;
        
        if (!read_sfdp_data(xspi_bus_socket[discovery_bank], ph_offset, 8, ph_data)) {
            std::cout << "ERROR: Failed to read Parameter Header #" << (int)i << std::endl;
            continue;
        }

        print_hex_data(ph_data, "Parameter Header #" + std::to_string(i));

        uint8_t id_lsb = ph_data[0];
        uint8_t ph_minor_rev = ph_data[1];
        uint8_t ph_major_rev = ph_data[2];
        uint8_t length_dwords = ph_data[3];
        uint32_t ptp = static_cast<uint32_t>(ph_data[4]) |
                      (static_cast<uint32_t>(ph_data[5]) << 8) |
                      (static_cast<uint32_t>(ph_data[6]) << 16);
        uint8_t reserved = ph_data[7];

        std::cout << "  Parameter ID LSB:    0x" << std::hex << std::setw(2) << std::setfill('0') << (int)id_lsb << std::dec << std::endl;
        std::cout << "  Major Revision:      0x" << std::hex << std::setw(2) << std::setfill('0') << (int)ph_major_rev << std::dec << std::endl;
        std::cout << "  Minor Revision:      0x" << std::hex << std::setw(2) << std::setfill('0') << (int)ph_minor_rev << std::dec << std::endl;
        std::cout << "  Length (DWORDs):     " << std::dec << (int)length_dwords << std::endl;
        std::cout << "  Parameter Table Ptr: 0x" << std::hex << std::setw(6) << std::setfill('0') << ptp << std::dec << std::endl;
        std::cout << "  Reserved:            0x" << std::hex << std::setw(2) << std::setfill('0') << (int)reserved << std::dec << std::endl;

        param_headers.push_back(ph_data);
        param_table_pointers.push_back(ptp);
    }

    // Step 3: Read Parameter Tables
    std::cout << "\n--- Step 3: Reading Parameter Tables and Configuring Registers ---" << std::endl;
    std::vector<uint8_t> basic_table_data;
    bool basic_table_found = false;

    for (size_t i = 0; i < param_table_pointers.size(); i++) {
        uint32_t ptp = param_table_pointers[i];
        uint8_t id_lsb = param_headers[i][0];
        uint8_t length_dwords = param_headers[i][3];
        size_t table_size = length_dwords * 4;

        std::cout << "\nReading Parameter Table #" << i << " (ID=0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)id_lsb << std::dec << ") at address 0x" << std::hex << ptp 
                  << " (size: " << std::dec << table_size << " bytes, " << (int)length_dwords << " DWORDs)..." << std::endl;

        std::vector<uint8_t> table_data;
        if (!read_sfdp_data(xspi_bus_socket[discovery_bank], ptp, table_size, table_data)) {
            std::cout << "ERROR: Failed to read Parameter Table #" << i << std::endl;
            continue;
        }

        print_hex_data(table_data, "Parameter Table #" + std::to_string(i));

        std::cout << "\n  DWORDs:" << std::endl;
        for (size_t dword_idx = 0; dword_idx < length_dwords; dword_idx++) {
            uint32_t dword = ::read_le32(table_data, dword_idx * 4);  // Use global namespace function
            std::cout << "    DWORD[" << std::dec << dword_idx << "]: 0x" 
                      << std::hex << std::setw(8) << std::setfill('0') << dword << std::dec << std::endl;
        }

        if (id_lsb == 0x00 && length_dwords >= 16) {
            basic_table_data = table_data;
            basic_table_found = true;
        }
    }

    // Step 4: Parse Basic Parameter Table using parse_sfdp_from_bytes
    bool full_discovery = false;
    bool sfdp_header_swapped = false;
    bool sfdp_soft_reset_f0_supported = false;
    bool sfdp_erase_opcode_available = false;
    uint8_t sfdp_erase_opcode = 0;
    bool sfdp_read_opcode_available = false;
    uint8_t sfdp_read_opcode = 0;
    uint8_t sfdp_read_dummy_cycles = 0;
    
    if (basic_table_found) {
        std::cout << "\n--- Step 4: Configuring Registers from SFDP Basic Parameter Table ---" << std::endl;
        
        // Construct full SFDP data for parse_sfdp_from_bytes
        std::vector<uint8_t> all_sfdp_data;
        all_sfdp_data.insert(all_sfdp_data.end(), header_data.begin(), header_data.end());
        all_sfdp_data.insert(all_sfdp_data.end(), param_headers[0].begin(), param_headers[0].end());
        all_sfdp_data.insert(all_sfdp_data.end(), basic_table_data.begin(), basic_table_data.end());
        
        sfdp_header_t sfdp_header;
        sfdp_parameter_header_t param_header;
        jedec_basic_table_t sfdp_table;
        
        // Use parse_sfdp_from_bytes from sfdp_utils.cpp
        if (parse_sfdp_from_bytes(all_sfdp_data, sfdp_header, param_header, sfdp_table)) {
            std::cout << "Successfully parsed SFDP data using parse_sfdp_from_bytes API" << std::endl;
            
            sfdp_addr_mode_e address_mode = sfdp_table.get_address_bytes();
            uint8_t addr_byte_count = (address_mode == ADDR_4_BYTE_ONLY) ? 4 : 3;
            
            bool write_enable_06h = sfdp_table.get_dword1().get_write_enable_opcode_select();
            uint8_t write_enable_opcode = write_enable_06h ? 0x06 : 0x50;
            
            bool fast_read_1_1_4_support = sfdp_table.get_dword1().get_fast_read_1_1_4_support();
            bool fast_read_1_4_4_support = sfdp_table.get_dword1().get_fast_read_1_4_4_support();
            
            uint8_t erase_type1_opcode = sfdp_table.get_dword8().get_erase_type1_opcode();
            uint8_t erase_type2_opcode = sfdp_table.get_dword8().get_erase_type2_opcode();
            uint8_t erase_type3_opcode = sfdp_table.get_dword9().get_type3_opcode();
            uint8_t erase_type4_opcode = sfdp_table.get_dword9().get_type4_opcode();
            
            // Check erase opcodes in priority order
            if (erase_type2_opcode != 0xFF && erase_type2_opcode != 0x00) {
                sfdp_erase_opcode = erase_type2_opcode;
                sfdp_erase_opcode_available = true;
            } else if (erase_type1_opcode != 0xFF && erase_type1_opcode != 0x00) {
                sfdp_erase_opcode = erase_type1_opcode;
                sfdp_erase_opcode_available = true;
            } else if (erase_type3_opcode != 0xFF && erase_type3_opcode != 0x00) {
                sfdp_erase_opcode = erase_type3_opcode;
                sfdp_erase_opcode_available = true;
            } else if (erase_type4_opcode != 0xFF && erase_type4_opcode != 0x00) {
                sfdp_erase_opcode = erase_type4_opcode;
                sfdp_erase_opcode_available = true;
            }
            
            // Check read opcodes
            if (fast_read_1_1_4_support) {
                sfdp_read_opcode = sfdp_table.get_read_opcode_1_1_4();
                if (sfdp_read_opcode != 0xFF && sfdp_read_opcode != 0x00) {
                    sfdp_read_opcode_available = true;
                    sfdp_read_dummy_cycles = sfdp_table.get_dword3().get_1_1_4_wait_states();
                }
            }
            if (!sfdp_read_opcode_available && fast_read_1_4_4_support) {
                sfdp_read_opcode = sfdp_table.get_dword3().get_1_4_4_opcode();
                if (sfdp_read_opcode != 0xFF && sfdp_read_opcode != 0x00) {
                    sfdp_read_opcode_available = true;
                    sfdp_read_dummy_cycles = sfdp_table.get_dword3().get_1_4_4_wait_states();
                }
            }
            
            uint8_t soft_reset_support = sfdp_table.get_dword16().get_soft_reset_support();
            sfdp_soft_reset_f0_supported = (soft_reset_support & 0x10) != 0;
            
            std::cout << "\nExtracted SFDP Parameters using API:" << std::endl;
            std::cout << "  Address Mode: " << addr_mode_to_string(address_mode) << " (" << (int)addr_byte_count << " bytes)" << std::endl;
            std::cout << "  Write Enable Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)write_enable_opcode << std::dec << std::endl;
            if (sfdp_erase_opcode_available) {
                std::cout << "  Erase Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)sfdp_erase_opcode << std::dec << std::endl;
            }
            if (sfdp_read_opcode_available) {
                std::cout << "  Read Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)sfdp_read_opcode << std::dec << std::endl;
            }
            std::cout << "  Soft Reset F0h Support: " << (sfdp_soft_reset_f0_supported ? "Yes" : "No") << std::endl;
            
            full_discovery = true;
            sfdp_header_swapped = false;
        } else {
            std::cout << "WARNING: Failed to parse SFDP using parse_sfdp_from_bytes API. Registers not configured." << std::endl;
            full_discovery = false;
        }
    } else {
        std::cout << "\nWARNING: JEDEC Basic Flash Parameter Table (ID=0x00) not found. Registers not configured." << std::endl;
        full_discovery = false;
    }

    // Steps 5-10: Configure registers
    std::cout << "\n--- Step 5: Configuring global_seq_cfg and global_seq_cfg_1 for Profile 1 ---" << std::endl;
    configure_global_seq_cfg_profile1(discovery_abnum, discovery_cmd_type, discovery_dummy_cnt,
                                      discovery_seq_crc_en, discovery_seq_crc_variant, discovery_seq_crc_oe,
                                      discovery_seq_crc_chunk_size, discovery_seq_crc_ual_chunk_en,
                                      full_discovery, sfdp_header_swapped);

    std::cout << "\n--- Step 6: Configuring rst_seq_cfg_0 and rst_seq_cfg_1 for Profile 1 ---" << std::endl;
    configure_rst_seq_cfg_profile1(discovery_abnum, full_discovery, sfdp_soft_reset_f0_supported);

    std::cout << "\n--- Step 7: Configuring ers_seq_cfg_0, ers_seq_cfg_1, and ers_seq_cfg_2 for Profile 1 ---" << std::endl;
    configure_ers_seq_cfg_profile1(discovery_abnum, full_discovery, sfdp_erase_opcode_available, sfdp_erase_opcode);

    std::cout << "\n--- Step 8: Configuring prog_seq_cfg_0 for Profile 1 ---" << std::endl;
    if (basic_table_found) {
        configure_prog_seq_cfg_profile1(discovery_num_lines, discovery_abnum, full_discovery, basic_table_data);
    } else {
        configure_prog_seq_cfg_profile1(discovery_num_lines, discovery_abnum, false, std::vector<uint8_t>());
    }

    std::cout << "\n--- Step 9: Configuring read_seq_cfg_0, read_seq_cfg_1, and xip_mode_cfg for Profile 1 ---" << std::endl;
    if (basic_table_found) {
        configure_read_seq_cfg_profile1(discovery_num_lines, discovery_abnum, discovery_dummy_cnt, discovery_cmd_type,discovery_extop_en, full_discovery, basic_table_data,
                                    sfdp_read_opcode_available, sfdp_read_opcode, sfdp_read_dummy_cycles);
    } else {
        configure_read_seq_cfg_profile1(discovery_num_lines, discovery_abnum, discovery_dummy_cnt, discovery_cmd_type,
                                        discovery_extop_en, false, std::vector<uint8_t>(),
                                        false, 0, 0);
    }

    std::cout << "\n--- Step 10: Configuring stat_seq_cfg_0, stat_seq_cfg_1, and stat_seq_cfg_2 for Profile 1 ---" << std::endl;
    if (basic_table_found) {
        configure_stat_seq_cfg_profile1(discovery_num_lines, discovery_abnum, discovery_dummy_cnt, discovery_cmd_type,
                                        discovery_extop_en, full_discovery, basic_table_data);
    } else {
        configure_stat_seq_cfg_profile1(discovery_num_lines, discovery_abnum, discovery_dummy_cnt, discovery_cmd_type,
                                        discovery_extop_en, false, std::vector<uint8_t>());
    }

    std::cout << "\n=== SFDP Discovery Process Complete ===" << std::endl;
}

// Function to dump all register values for debugging
void cdns_xspi_ctrl_reg::dump_registers(const std::string& label) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "REGISTER DUMP: " << label << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "\n--- Global Sequence Configuration Registers ---" << std::endl;
    std::cout << "global_seq_cfg (0x390): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)global_seq_cfg << std::dec << std::endl;
    std::cout << "  seq_type = " << (int)global_seq_cfg.seq_type << std::endl;
    std::cout << "  seq_data_per_addr = " << (int)global_seq_cfg.seq_data_per_addr << std::endl;
    std::cout << "  seq_page_size_pgm = " << (int)global_seq_cfg.seq_page_size_pgm << std::endl;
    std::cout << "  seq_page_size_rd = " << (int)global_seq_cfg.seq_page_size_rd << std::endl;
    std::cout << "  seq_data_swap = " << (int)global_seq_cfg.seq_data_swap << std::endl;
    std::cout << "  seq_crc_en = " << (int)global_seq_cfg.seq_crc_en << std::endl;
    std::cout << "  seq_crc_variant = " << (int)global_seq_cfg.seq_crc_variant << std::endl;
    std::cout << "  seq_crc_oe = " << (int)global_seq_cfg.seq_crc_oe << std::endl;
    std::cout << "  seq_crc_chunk_size = " << (int)global_seq_cfg.seq_crc_chunk_size << std::endl;
    std::cout << "  seq_crc_ual_chunk_en = " << (int)global_seq_cfg.seq_crc_ual_chunk_en << std::endl;
    std::cout << "  seq_tcms_en = " << (int)global_seq_cfg.seq_tcms_en << std::endl;

    std::cout << "global_seq_cfg_1 (0x394): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)global_seq_cfg_1 << std::dec << std::endl;
    std::cout << "  seq_page_size_ext = " << (int)global_seq_cfg_1.seq_page_size_ext << std::endl;
    std::cout << "  seq_page_ca_size = " << (int)global_seq_cfg_1.seq_page_ca_size << std::endl;
    std::cout << "  seq_plane_cnt = " << (int)global_seq_cfg_1.seq_plane_cnt << std::endl;
    std::cout << "  seq_page_per_block = " << (int)global_seq_cfg_1.seq_page_per_block << std::endl;

    std::cout << "\n--- Reset Sequence Configuration Registers ---" << std::endl;
    std::cout << "rst_seq_cfg_0 (0x400): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)rst_seq_cfg_0 << std::dec << std::endl;
    std::cout << "  rst_seq_p1_cmd0_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)rst_seq_cfg_0.rst_seq_p1_cmd0_val << std::dec << std::endl;
    std::cout << "  rst_seq_p1_cmd0_en = " << (int)rst_seq_cfg_0.rst_seq_p1_cmd0_en << std::endl;
    std::cout << "  rst_seq_p1_cmd1_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)rst_seq_cfg_0.rst_seq_p1_cmd1_val << std::dec << std::endl;
    std::cout << "  rst_seq_p1_data_en = " << (int)rst_seq_cfg_0.rst_seq_p1_data_en << std::endl;

    std::cout << "rst_seq_cfg_1 (0x404): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)rst_seq_cfg_1 << std::dec << std::endl;
    std::cout << "  rst_seq_p1_data_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)rst_seq_cfg_1.rst_seq_p1_data_val << std::dec << std::endl;

    std::cout << "\n--- Erase Sequence Configuration Registers ---" << std::endl;
    std::cout << "ers_seq_cfg_0 (0x410): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)ers_seq_cfg_0 << std::dec << std::endl;
    std::cout << "  erss_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)ers_seq_cfg_0.erss_seq_p1_cmd_val << std::dec << std::endl;

    std::cout << "ers_seq_cfg_1 (0x414): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)ers_seq_cfg_1 << std::dec << std::endl;
    std::cout << "  erss_seq_p1_sect_size = " << (int)ers_seq_cfg_1.erss_seq_p1_sect_size << std::endl;

    std::cout << "ers_seq_cfg_2 (0x418): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)ers_seq_cfg_2 << std::dec << std::endl;
    std::cout << "  ersa_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)ers_seq_cfg_2.ersa_seq_p1_cmd_val << std::dec << std::endl;

    std::cout << "\n--- Program Sequence Configuration Registers ---" << std::endl;
    std::cout << "prog_seq_cfg_0 (0x420): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)prog_seq_cfg_0 << std::dec << std::endl;
    std::cout << "  prog_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)prog_seq_cfg_0.prog_seq_p1_cmd_val << std::dec << std::endl;
    std::cout << "  prog_seq_p1_addr_cnt = " << (int)prog_seq_cfg_0.prog_seq_p1_addr_cnt << std::endl;
    std::cout << "  prog_seq_p1_dummy_cnt = " << (int)prog_seq_cfg_0.prog_seq_p1_dummy_cnt << std::endl;

    std::cout << "\n--- Read Sequence Configuration Registers ---" << std::endl;
    std::cout << "read_seq_cfg_0 (0x430): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)read_seq_cfg_0 << std::dec << std::endl;
    std::cout << "  read_seq_p1_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)read_seq_cfg_0.read_seq_p1_cmd_val << std::dec << std::endl;
    std::cout << "  read_seq_p1_addr_cnt = " << (int)read_seq_cfg_0.read_seq_p1_addr_cnt << std::endl;
    std::cout << "  read_seq_p1_dummy_cnt = " << (int)read_seq_cfg_0.read_seq_p1_dummy_cnt << std::endl;

    std::cout << "read_seq_cfg_1 (0x434): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)read_seq_cfg_1 << std::dec << std::endl;
    std::cout << "  read_seq_p1_mb_dummy_cnt = " << (int)read_seq_cfg_1.read_seq_p1_mb_dummy_cnt << std::endl;
    std::cout << "  read_seq_p1_mb_en = " << (int)read_seq_cfg_1.read_seq_p1_mb_en << std::endl;

    std::cout << "\n--- Status Sequence Configuration Registers ---" << std::endl;
    std::cout << "stat_seq_cfg_0 (0x450): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)stat_seq_cfg_0 << std::dec << std::endl;
    std::cout << "  stat_seq_p1_addr_cnt = " << (int)stat_seq_cfg_0.stat_seq_p1_addr_cnt << std::endl;

    std::cout << "stat_seq_cfg_1 (0x454): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)stat_seq_cfg_1 << std::dec << std::endl;
    std::cout << "  stat_seq_p1_dev_rdy_dummy_cnt = " << (int)stat_seq_cfg_1.stat_seq_p1_dev_rdy_dummy_cnt << std::endl;
    std::cout << "  stat_seq_p1_dev_rdy_addr_en = " << (int)stat_seq_cfg_1.stat_seq_p1_dev_rdy_addr_en << std::endl;
    std::cout << "  stat_seq_p1_prog_fail_dummy_cnt = " << (int)stat_seq_cfg_1.stat_seq_p1_prog_fail_dummy_cnt << std::endl;
    std::cout << "  stat_seq_p1_prog_fail_addr_en = " << (int)stat_seq_cfg_1.stat_seq_p1_prog_fail_addr_en << std::endl;
    std::cout << "  stat_seq_p1_ers_fail_dummy_cnt = " << (int)stat_seq_cfg_1.stat_seq_p1_ers_fail_dummy_cnt << std::endl;
    std::cout << "  stat_seq_p1_ers_fail_addr_en = " << (int)stat_seq_cfg_1.stat_seq_p1_ers_fail_addr_en << std::endl;

    std::cout << "stat_seq_cfg_2 (0x458): 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << (unsigned int)stat_seq_cfg_2 << std::dec << std::endl;
    std::cout << "  stat_seq_p1_dev_rdy_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)stat_seq_cfg_2.stat_seq_p1_dev_rdy_cmd_val << std::dec << std::endl;
    std::cout << "  stat_seq_p1_prog_fail_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)stat_seq_cfg_2.stat_seq_p1_prog_fail_cmd_val << std::dec << std::endl;
    std::cout << "  stat_seq_p1_ers_fail_cmd_val = 0x" << std::hex << std::setw(2) << std::setfill('0')
              << (int)stat_seq_cfg_2.stat_seq_p1_ers_fail_cmd_val << std::dec << std::endl;
              std::cout << "\n--- XIP Mode Configuration Register ---" << std::endl;
    std::cout << "xip_mode_cfg (0x388): 0x" << std::hex << std::setw(8) << std::setfill('0') 
            << (unsigned int)xip_mode_cfg << std::dec << std::endl;
    std::cout << "  xip_dis_mb_val = 0x" << std::hex << std::setw(2) << std::setfill('0') 
            << (int)xip_mode_cfg.xip_dis_mb_val << std::dec << std::endl;

    std::cout << "\n" << std::string(80, '=') << std::endl;
}

void cdns_xspi_ctrl_reg::b_transport_axi_slave(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay){


    switch(static_cast<unsigned int>(ctrl_config.work_mode)) {
        case static_cast<unsigned int>(xspi_word_mode::DIRECT_MODE):
            handle_direct_mode(trans);
            break;
        case static_cast<unsigned int>(xspi_word_mode::STIG_MODE):
            handle_stig_mode(trans);
            break;
        case static_cast<unsigned int>(xspi_word_mode::ACMD_MODE):
            handle_acmd_mode(trans);
            break;
        default:
            //print error message
            std::cout << "Error: Invalid work mode" << std::endl;
            std::cout << "Work mode: " << (int)ctrl_config.work_mode << std::endl;
            break;
    }
}

void cdns_xspi_ctrl_reg::handle_direct_mode(tlm::tlm_generic_payload& trans) {
    //print message
    std::cout << "Handling direct mode" << std::endl;

    uint8_t discovery_bank_num = direct_access_cfg.dac_bank_num;

    // Get or create xspi_target_trans extension
    xspi_target_trans* target_trans = trans.get_extension<xspi_target_trans>();
    if (target_trans == nullptr) {
        target_trans = new xspi_target_trans();
        trans.set_extension(target_trans);
    }

    // Extract address from transaction
    uint32_t address = static_cast<uint32_t>(trans.get_address());

    // Check if read or write command
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        // Set read opcode (using READ_ZERO_LATENCY as default)
        target_trans->xspi_target_opcode = 0x03; // READ_ZERO_LATENCY
        trans.set_extension(target_trans); 
        // Alternative options: 0x0B (READ_FAST) or 0xEE (READ_FAST_ALT)

        // Ensure data buffer is allocated for read
        if (trans.get_data_ptr() == nullptr || trans.get_data_length() == 0) {
            std::cerr << "[Direct Mode] Error: Read transaction has no data buffer" << std::endl;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        // Forward transaction to target via socket (using bank 0 as default)
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        if (xspi_bus_socket.size() > discovery_bank_num) {
            xspi_bus_socket[discovery_bank_num]->b_transport(trans, delay);
        } else {
            std::cerr << "[Direct Mode] Error: No xspi_bus_socket available" << std::endl;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        // Data should already be in the payload after b_transport
        // The target model will have copied the read data into trans.get_data_ptr()
        if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
            std::cout << "[Direct Mode] Read completed successfully, " 
                      << trans.get_data_length() << " bytes read" << std::endl;
        }

    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {

        target_trans->xspi_target_opcode = 0x06; // WRITE_ENABLE
        trans.set_extension(target_trans); 
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        if (xspi_bus_socket.size() > discovery_bank_num) {
            xspi_bus_socket[discovery_bank_num]->b_transport(trans, delay);
        } else {
            std::cerr << "[Direct Mode] Error: No xspi_bus_socket available" << std::endl;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        
        // Set write opcode (PROGRAM)
        target_trans->xspi_target_opcode = 0x02; // PROGRAM
        trans.set_extension(target_trans); 

        // Extract write data from payload
        if (trans.get_data_ptr() == nullptr || trans.get_data_length() == 0) {
            std::cerr << "[Direct Mode] Error: Write transaction has no data" << std::endl;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        // Forward transaction to target via socket (using bank 0 as default)
        
        if (xspi_bus_socket.size() > discovery_bank_num) {
            xspi_bus_socket[discovery_bank_num]->b_transport(trans, delay);
        } else {
            std::cerr << "[Direct Mode] Error: No xspi_bus_socket available" << std::endl;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        // Data was already in the payload, target model processes it
        if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
            std::cout << "[Direct Mode] Write (PROGRAM) completed successfully, " 
                      << trans.get_data_length() << " bytes written" << std::endl;
        }

    } else {
        std::cerr << "[Direct Mode] Error: Invalid command type" << std::endl;
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    }
}

// STIG 2-byte Write Handler (Opcode 0x02)
// Called from STIG engine thread
void cdns_xspi_ctrl_reg::handle_stig_write(const cdns_xspi_ctrl_reg::stig_instruction& inst) {
    std::cout << "\n  [HANDLER] STIG 2-Byte WRITE" << std::endl;
    std::cout << "    Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)inst.opcode << std::dec << std::endl;
    std::cout << "    Address: 0x" << std::hex << std::setw(12) << std::setfill('0') << inst.address << std::dec << std::endl;
    std::cout << "    Write Data: 0x" << std::hex << std::setw(4) << std::setfill('0') << inst.write_data << std::dec << std::endl;
    std::cout << "    Bank Num: " << (int)inst.bank_num << std::endl;

    // Create and send TLM transactions to xSPI device
    if (xspi_bus_socket.size() > 0) {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        
        // Step 1: WRITE_ENABLE (0x06) - Required before programming
        tlm::tlm_generic_payload* trans_we = new tlm::tlm_generic_payload();
        xspi_target_trans* target_trans_we = new xspi_target_trans();
        
        target_trans_we->xspi_target_opcode = 0x06;  // WRITE_ENABLE opcode
        trans_we->set_address(0);
        trans_we->set_command(tlm::TLM_WRITE_COMMAND);
        trans_we->set_data_length(0);
        trans_we->set_extension(target_trans_we);
        
        xspi_bus_socket[inst.bank_num]->b_transport(*trans_we, delay);
        std::cout << "  Step 1: WRITE_ENABLE (0x06) sent" << std::endl;
        
        // Cleanup Step 1
        delete trans_we;
        
        // Step 2: PROGRAM (0x02) - Write data to flash
        tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
        xspi_target_trans* target_trans = new xspi_target_trans();
        
        target_trans->xspi_target_opcode = inst.opcode;
        trans->set_address(inst.address);
        trans->set_command(tlm::TLM_WRITE_COMMAND);
        
        // Allocate data buffer
        uint8_t* data_buf = new uint8_t[2];
        data_buf[0] = (uint8_t)(inst.write_data & 0xFF);
        data_buf[1] = (uint8_t)((inst.write_data >> 8) & 0xFF);
        
        trans->set_data_ptr(data_buf);
        trans->set_data_length(2);
        trans->set_extension(target_trans);
        
        // Send transaction
        xspi_bus_socket[inst.bank_num]->b_transport(*trans, delay);
        std::cout << "   Step 2: PROGRAM (0x02) transaction sent" << std::endl;
        
        // Cleanup Step 2
        uint8_t* current_data_ptr = trans->get_data_ptr();
        delete trans;
        if (current_data_ptr == data_buf) {
            delete[] data_buf;
        }
    } else {
        SC_REPORT_ERROR("XSPI_CTRL_REG", "No xspi_bus_socket available for 2-byte write");
        return;
    }
    
    std::cout << "  [HANDLER] WRITE handler complete" << std::endl;
}

// STIG 2-byte Read Handler - Instruction 1 (Opcode 0x03)
// Address phase - executes the READ command and stores data temporarily
void cdns_xspi_ctrl_reg::handle_stig_read(const cdns_xspi_ctrl_reg::stig_instruction& inst, const cdns_xspi_ctrl_reg::stig_instruction* data_phase) {
    std::cout << "\n=== STIG 2-Byte READ - Instruction 1/2 (Address Phase) ===" << std::endl;
    
    // Set gcmd_eng_mc_busy to indicate glued instruction in progress
    this->ctrl_status.gcmd_eng_mc_busy = 1;
    std::cout << "  [SET] ctrl_status.gcmd_eng_mc_busy = 1 (glued instruction active)" << std::endl;
    
    std::cout << "  Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)inst.opcode << std::dec << std::endl;
    std::cout << "  Address: 0x" << std::hex << std::setw(12) << std::setfill('0') << inst.address << std::dec << std::endl;
    std::cout << "  Bank Num: " << (int)inst.bank_num << std::endl;
    std::cout << "  Number of Data bytes: "<< (int)data_phase->d_data_bytes << std::endl;
    
    // Create and send TLM read transaction to xSPI device
    if (xspi_bus_socket.size() > 0) {
        tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
        xspi_target_trans* target_trans = new xspi_target_trans();
        
        target_trans->xspi_target_opcode = inst.opcode;
        trans->set_address(inst.address);
        trans->set_command(tlm::TLM_READ_COMMAND);
        
        // Allocate buffer for 2-byte read data
        uint8_t* data_buf = new uint8_t[data_phase->d_data_bytes];
        trans->set_data_ptr(data_buf);
        trans->set_data_length(data_phase->d_data_bytes); // Use data_bytes from data_phase for flexibility
        trans->set_extension(target_trans);
        
        // Send read transaction to device
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        xspi_bus_socket[inst.bank_num]->b_transport(*trans, delay);
        std::cout << "  Transaction: " << (int)data_phase->d_data_bytes << "-byte READ sent to xSPI device (bank " << (int)inst.bank_num << ")" << std::endl;

        // Retrieve and store read data (will be used in Instruction 2)
        uint8_t* read_ptr = trans->get_data_ptr();
        if (read_ptr != nullptr) {
            uint16_t read_data = read_ptr[0] | (read_ptr[1] << 8);
        }
        
        // STIG 2-byte Read Handler - Instruction 2 (Glued Data Variant)
        // Final glued instruction that returns read data to cmd_status
        std::cout << "\n=== STIG 2-Byte READ - Instruction 2/2 (Glued Data Variant) ===" << std::endl;
        std::cout << " Instruction_type: " << (int)data_phase->instr_type << std::endl;
        std::cout << " Status source: " << (int)data_phase->d_status_source << std::endl;

        if(data_phase->d_data_bytes == 2 && read_ptr != nullptr && data_phase != nullptr && data_phase->d_status_source == 1)
        // STATUS_SOURCE bit[24] indicates data phase instruction
        {
            uint16_t read_data = read_ptr[0] | (read_ptr[1] << 8);
            // Store in upper 16 bits of cmd_status register per spec: DATA_FROM_DEV = bits [31:16]
            this->cmd_status = (this->cmd_status & 0x0000FFFF) | ((uint32_t)read_data << 16);
            std::cout << "  Read Data: 0x" << std::hex << std::setw(4) << std::setfill('0') << read_data << std::dec << std::endl;
            std::cout << "  Data buffered in cmd_status register (DATA_FROM_DEV at bits [31:16])" << std::endl;
        } else {
            std::cout << "  Read Data: " << data_phase->d_data_bytes << " bytes read, not stored in cmd_status (only supports 2 bytes)" << std::endl;
        }
        
        // Cleanup
        uint8_t* current_data_ptr = trans->get_data_ptr();
        delete trans;
        if (current_data_ptr == data_buf) {
            delete[] data_buf;
        }
    } else {
        SC_REPORT_ERROR("XSPI_CTRL_REG", "No xspi_bus_socket available for 2-byte read");
        this->ctrl_status.gcmd_eng_mc_busy = 0;  // Clear busy on error
        return;
    }
    std::cout << "  STIG 2-byte READ transaction COMPLETE" << std::endl;
}

void cdns_xspi_ctrl_reg::handle_stig_control_command(const cdns_xspi_ctrl_reg::stig_instruction& inst, const cdns_xspi_ctrl_reg::stig_instruction* data_phase) {
    std::cout << "\n[HANDLER] STIG Control Command" << std::endl; 
    std::cout << "Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)inst.opcode << std::dec << std::endl;
    
    if(inst.opcode == 0x04 || inst.opcode == 0x06) // enable WEL = 0x06, disable WEL = 0x04  
    {        std::cout << "This is a WRITE_ENABLE control command (WEL " << ((inst.opcode == 0x06) ? "ENABLED" : "DISABLED") << ")" << std::endl;
        // Create and send TLM transactions to xSPI device
        if (xspi_bus_socket.size() > 0) {
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        
            tlm::tlm_generic_payload* trans_we = new tlm::tlm_generic_payload();
            xspi_target_trans* target_trans_we = new xspi_target_trans();
        
            target_trans_we->xspi_target_opcode = inst.opcode;  // WRITE_ENABLE or WRITE_DISABLE opcode
            trans_we->set_address(0);
            trans_we->set_command(tlm::TLM_WRITE_COMMAND);
            trans_we->set_data_length(0);
            trans_we->set_extension(target_trans_we);
            
            xspi_bus_socket[inst.bank_num]->b_transport(*trans_we, delay);
            // Check response
            if (trans_we->is_response_error())
            {
                SC_REPORT_ERROR("XSPI_CTRL_REG",
                                "WEL transaction failed");
            }

            // Cleanup
            //delete trans_we;
            delete target_trans_we;

        } else {
            SC_REPORT_ERROR("XSPI_CTRL_REG", "No xspi_bus_socket available for STIG control command");
            return;
        }
    } else if(inst.opcode == 0x05) // Read status register command
    {
        std::cout << "This is a READ_STATUS control command" << std::endl;
        // For simplicity, we won't implement the full read status logic here, but in a real implementation,
        // this would involve sending a read status command to the xSPI device and returning the result to the STIG engine.

    } else
    {
        std::cout << "This is an unrecognized control command, no action taken" << std::endl;
        return;
    }    
    std::cout << "[HANDLER] Control command handler complete" << std::endl;
}

void cdns_xspi_ctrl_reg::handle_stig_suspend_resume(const cdns_xspi_ctrl_reg::stig_instruction& inst)
{
    std::cout << "\n[HANDLER] STIG Suspend/Resume Command" << std::endl;
    std::cout << "Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)inst.opcode << std::dec << std::endl;

    if (inst.opcode == 0x75 || inst.opcode == 0xB0)
    {
        std::cout << "This is a SUSPEND command ("
                  << ((inst.opcode == 0x75) ? "SUSPEND_75" : "SUSPEND_B0")
                  << ")" << std::endl;
    }
    else if (inst.opcode == 0x30 || inst.opcode == 0x7A || inst.opcode == 0xD0)
    {
        std::cout << "This is a RESUME command ("
                  << ((inst.opcode == 0x30) ? "RESUME_30" :
                     (inst.opcode == 0x7A) ? "RESUME_7A" : "RESUME_D0")
                  << ")" << std::endl;
    }
    else
    {
        std::cout << "Unrecognized suspend/resume command" << std::endl;
        return;
    }

    if (xspi_bus_socket.size() > 0)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
        xspi_target_trans* target_trans = new xspi_target_trans();

        target_trans->xspi_target_opcode = inst.opcode;    

        trans->set_address(0);                   // No address phase
        trans->set_command(tlm::TLM_WRITE_COMMAND);
        trans->set_data_length(0);               // No data phase
        trans->set_streaming_width(0);
        trans->set_byte_enable_ptr(nullptr);
        trans->set_dmi_allowed(false);
        trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        trans->set_extension(target_trans);

        // Send to correct bank
        xspi_bus_socket[inst.bank_num]->b_transport(*trans, delay);

        // Check response
        if (trans->is_response_error())
        {
            SC_REPORT_ERROR("XSPI_CTRL_REG",
                            "Suspend/Resume transaction failed");
        }

        // Cleanup
        delete target_trans;
    }
    else
    {
        SC_REPORT_ERROR("XSPI_CTRL_REG",
                        "No xspi_bus_socket available for Suspend/Resume");
        return;
    }

    std::cout << "[HANDLER] Suspend/Resume handler complete" << std::endl;
}

void cdns_xspi_ctrl_reg::handle_stig_read_sfdp(const cdns_xspi_ctrl_reg::stig_instruction& inst, const cdns_xspi_ctrl_reg::stig_instruction* data_phase) {
    std::cout << "\n=== STIG READ_SFDP(16 bytes) - Instruction 1/2 (Address Phase) ===" << std::endl;
    
    // Set gcmd_eng_mc_busy to indicate glued instruction in progress
    this->ctrl_status.gcmd_eng_mc_busy = 1;
    std::cout << "  [SET] ctrl_status.gcmd_eng_mc_busy = 1 (glued instruction active)" << std::endl;
    
    std::cout << "  Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)inst.opcode << std::dec << std::endl;
    std::cout << "  Address: 0x" << std::hex << std::setw(12) << std::setfill('0') << inst.address << std::dec << std::endl;
    std::cout << "  Bank Num: " << (int)inst.bank_num << std::endl;
    std::cout << "  Data bytes: "<< (int)data_phase->d_data_bytes << std::endl;
    
    // Create and send TLM read transaction to xSPI device
    if (xspi_bus_socket.size() > 0) {
        tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload();
        xspi_target_trans* target_trans = new xspi_target_trans();
        
        target_trans->xspi_target_opcode = inst.opcode;
        trans->set_address(inst.address);
        trans->set_command(tlm::TLM_READ_COMMAND);
        
        // Allocate buffer for 2-byte read data
        uint8_t* data_buf = new uint8_t[data_phase->d_data_bytes];
        trans->set_data_ptr(data_buf);
        trans->set_data_length(data_phase->d_data_bytes); // Use data_bytes from instruction for flexibility
        trans->set_extension(target_trans);
        
        // Send read transaction to device
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        xspi_bus_socket[inst.bank_num]->b_transport(*trans, delay);
        std::cout << "  Transaction: " << (int)data_phase->d_data_bytes << "-byte READ sent to xSPI device (bank " << (int)inst.bank_num << ")" << std::endl;

        // Retrieve and store read data (will be used in Instruction 2)
        uint8_t* read_ptr = trans->get_data_ptr();
        if (read_ptr != nullptr) {
            uint16_t read_data = read_ptr[0] | (read_ptr[1] << 8);
        }
        
        // STIG 2-byte Read Handler - Instruction 2 (Glued Data Variant)
        // Final glued instruction that returns read data to cmd_status
        std::cout << "\n=== STIG READ_SFDP(16 bytes) - Instruction 2/2 (Glued Data Variant) ===" << std::endl;
        std::cout << " Instruction_type: " << (int)data_phase->instr_type << std::endl;
        std::cout << " Status source: " << (int)data_phase->d_status_source << std::endl;

        if(inst.data_bytes == 2 && read_ptr != nullptr && data_phase != nullptr && data_phase->d_status_source == 1)
        // STATUS_SOURCE bit[24] indicates data phase instruction
        {
            uint16_t read_data = read_ptr[0] | (read_ptr[1] << 8);
            // Store in upper 16 bits of cmd_status register per spec: DATA_FROM_DEV = bits [31:16]
            this->cmd_status = (this->cmd_status & 0x0000FFFF) | ((uint32_t)read_data << 16);
            std::cout << "  Read Data: 0x" << std::hex << std::setw(4) << std::setfill('0') << read_data << std::dec << std::endl;
            std::cout << "  Data buffered in cmd_status register (DATA_FROM_DEV at bits [31:16])" << std::endl;
        } else {
            std::cout << "  Read Data: " << inst.data_bytes << " bytes read, not stored in cmd_status (only supports 2 bytes)" << std::endl;
            return;
        }
        
        // Cleanup
        uint8_t* current_data_ptr = trans->get_data_ptr();
        delete trans;
        if (current_data_ptr == data_buf) {
            delete[] data_buf;
        }
    } else {
        SC_REPORT_ERROR("XSPI_CTRL_REG", "No xspi_bus_socket available for 2-byte read");
        this->ctrl_status.gcmd_eng_mc_busy = 0;  // Clear busy on error
        return;
    }
    std::cout << " STIG READ_SFDP transaction COMPLETE " << std::endl;
}

 // Extract address
uint64_t cdns_xspi_ctrl_reg::extract_address(uint32_t instr_0, uint32_t instr_1, uint32_t instr_2) {
        uint8_t addr0 = (instr_0 >> 24) & 0xFF;
        uint8_t addr1 = instr_1 & 0xFF;
        uint8_t addr2 = (instr_1 >> 8) & 0xFF;
        uint8_t addr3 = (instr_1 >> 16) & 0xFF;
        uint8_t addr4 = (instr_1 >> 24) & 0xFF;
        uint8_t addr5 = instr_2 & 0xFF;
        uint64_t address = ((uint64_t)addr5 << 40) | ((uint64_t)addr4 << 32) | 
                          ((uint32_t)addr3 << 24) | ((uint32_t)addr2 << 16) | 
                          ((uint32_t)addr1 << 8) | addr0;

        return address;
}
        
uint32_t cdns_xspi_ctrl_reg::extract_number_of_data_bytes(uint32_t instr1, uint32_t instr2) {
        uint16_t no_of_data_bytes_0 = (instr1 >> 16) & 0xFFFF;  // DATA_BYTES at bits [63:48]
        uint16_t no_of_data_bytes_1 = instr2 & 0xFFFF; // DATA_BYTES at bits [79:64]
        
        uint32_t no_of_data_bytes = ((uint32_t)no_of_data_bytes_1 << 16) | no_of_data_bytes_0;

        return no_of_data_bytes;
}

cdns_xspi_ctrl_reg::stig_instruction cdns_xspi_ctrl_reg::decode_instruction()
{
   stig_instruction inst{};

   uint32_t instr0 = cmd_reg1;
   uint32_t instr1 = cmd_reg2;
   uint32_t instr2 = cmd_reg3;
   uint32_t instr3 = cmd_reg4;

   inst.instr_type    = instr0 & 0x7F;          // Instruction Type at bits [6:0]

   if(inst.instr_type == 0x7F)
   {
        // This is a glued data phase instruction
        inst.d_status_source = (instr0 >> 24) & 0x1;   // STATUS_SOURCE at bit [24] only in glued instruction
        inst.d_data_bytes = extract_number_of_data_bytes(instr1, instr2);  // [79:48] DATA_BYTES at bits only in glued instruction
        inst.d_DIR = (instr3 >> 5) & 0x1;              // DIR at bit [28] only in glued instruction    
        
        return inst;    
   } else
   {
        // This is a command phase instruction
        inst.opcode        = (instr2 >> 16) & 0xFF;  // CMD at bits [87:80]
        inst.instr_link    = (instr3 >> 30) & 0x1;
        inst.data_bytes    = (instr2 >> 24) & 0x3;
        inst.write_data    = (instr0 >> 8) & 0xFFFF;   // DATA0 at bits [15:8] and DATA1 at bits [23:16]
        inst.bank_num      = (instr3 >> 13) & 0x3;   // BANK_NUM at bits [110:108]
        inst.address = extract_address(instr0, instr1, instr2);

        return inst;
   }
}

bool cdns_xspi_ctrl_reg::execute_stig(
       const stig_instruction& cmd,
       const stig_instruction* data_phase)
{
   switch (cmd.opcode)
   {
        case 0x02:   // WRITE
           handle_stig_write(cmd);
           return true;

        case 0x03:   // READ
           handle_stig_read(cmd, data_phase);
           return true;

        case 0x06:  // WRITE_ENABLE
        case 0x04:  // WRITE_DISABLE
        case 0x05:  // READ_STATUS_REG
            handle_stig_control_command(cmd, data_phase);
            return true;

        case 0x75:  // SUSPEND_75
        case 0xB0:  // SUSPEND_B0
        case 0x30:  // RESUME_30
        case 0x7A:  // RESUME_7A
        case 0xD0:  // RESUME_D0
            handle_stig_suspend_resume(cmd);
            return true;

        // case 0x9F: // READ JEDEC_ID 

        case 0x5A:  // READ_SFDP
            handle_stig_read_sfdp(cmd, data_phase);
            return true;

       default:
           std::cout << "[ERROR] Unsupported opcode 0x"
                     << std::hex << (int)cmd.opcode << std::dec << std::endl;
           return false;
   }
}

void cdns_xspi_ctrl_reg::set_busy()
{
   ctrl_status.ctrl_busy = 1;
   ctrl_status.gcmd_eng_busy = 1;
   ctrl_status.gcmd_eng_mc_busy = 1;
}

void cdns_xspi_ctrl_reg::clear_status()
{
    cmd_status = 0;
    intr_status = {};
}

void cdns_xspi_ctrl_reg::finish()
{
   cmd_status |= (1 << 15);  // COMPLETE

   ctrl_status.ctrl_busy = 0;
   ctrl_status.gcmd_eng_busy = 0;
   ctrl_status.gcmd_eng_mc_busy = 0;

   if ((intr_enable >> 31) & 1 &&
       (intr_enable >> 23) & 1)
       intr_status.stig_done = 1;
}

void cdns_xspi_ctrl_reg::set_error(const char* msg)
{
   std::cout << "[STIG ERROR] " << msg << std::endl;
   cmd_status |= (1 << 14);
}


void cdns_xspi_ctrl_reg::stig_engine_thread()
{
   while (true)
   {
       wait(cmd_trigger_event);   // Wait for host trigger

       set_busy();
       clear_status();

       // Decode first instruction
       stig_instruction inst1 = decode_instruction();

       stig_instruction inst2{};
       bool glued = false;

       // Handle glued sequence
       if (inst1.instr_link)
       {
           glued = true;

           wait(cmd_trigger_event);   // wait for second trigger

           inst2 = decode_instruction();

           if (inst2.instr_type != 0x7F)  // Data phase must have instr_type = 0x7F
           {
               set_error("Invalid glue sequence");
               finish();
               continue;
           }
       }

        // Execute
        bool success = execute_stig(inst1, glued ? &inst2 : nullptr);

        if (!success)
            set_error("Execution failed");

        finish();
   }
}

void cdns_xspi_ctrl_reg::handle_stig_mode(tlm::tlm_generic_payload& trans) {
    //print message
    std::cout << "Handling stig mode" << std::endl;
}

void cdns_xspi_ctrl_reg::handle_acmd_mode(tlm::tlm_generic_payload& trans) {
    //print message
    std::cout << "Handling acmd mode" << std::endl;
}

bool cdns_xspi_ctrl_reg::handle_write_cmd_reg0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
    this->cmd_reg0 = ((this->cmd_reg0 & ~byteEnables) | (value & byteEnables));
    if (byteEnables & 0x1) {
        cmd_trigger_event.notify(SC_ZERO_TIME);
    }
    
    return true;
}


bool cdns_xspi_ctrl_reg::handle_write_cmd_reg1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->cmd_reg1 = ((this->cmd_reg1 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_cmd_reg2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->cmd_reg2 = ((this->cmd_reg2 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_cmd_reg3(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->cmd_reg3 = ((this->cmd_reg3 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_cmd_reg4(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->cmd_reg4 = ((this->cmd_reg4 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_cmd_reg5(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->cmd_reg5 = ((this->cmd_reg5 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_cmd_status_ptr(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->cmd_status_ptr = ((this->cmd_status_ptr & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
bool result = this->intr_status.put_with_triggering_bitfield_callbacks(value, byteEnables, time);
return result;
}
bool cdns_xspi_ctrl_reg::handle_write_intr_enable(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->intr_enable = ((this->intr_enable & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_trd_comp_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
bool result = this->trd_comp_intr_status.put_with_triggering_bitfield_callbacks(value, byteEnables, time);
return result;
}
bool cdns_xspi_ctrl_reg::handle_write_trd_error_intr_status(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
bool result = this->trd_error_intr_status.put_with_triggering_bitfield_callbacks(value, byteEnables, time);
return result;
}
bool cdns_xspi_ctrl_reg::handle_write_trd_error_intr_en(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->trd_error_intr_en = ((this->trd_error_intr_en & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_long_polling(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->long_polling = ((this->long_polling & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_short_polling(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->short_polling = ((this->short_polling & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_ctrl_config(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->ctrl_config = ((this->ctrl_config & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_dma_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->dma_settings = ((this->dma_settings & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_discovery_control(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->discovery_control = ((this->discovery_control & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_xip_mode_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->xip_mode_cfg = ((this->xip_mode_cfg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_global_seq_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->global_seq_cfg = ((this->global_seq_cfg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_global_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->global_seq_cfg_1 = ((this->global_seq_cfg_1 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_direct_access_cfg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->direct_access_cfg = ((this->direct_access_cfg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_direct_access_rmp(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->direct_access_rmp = ((this->direct_access_rmp & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_direct_access_rmp_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->direct_access_rmp_1 = ((this->direct_access_rmp_1 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_rst_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->rst_seq_cfg_0 = ((this->rst_seq_cfg_0 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_rst_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->rst_seq_cfg_1 = ((this->rst_seq_cfg_1 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_ers_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->ers_seq_cfg_0 = ((this->ers_seq_cfg_0 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_ers_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->ers_seq_cfg_1 = ((this->ers_seq_cfg_1 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_ers_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
bool result = this->ers_seq_cfg_2.put_with_triggering_bitfield_callbacks(value, byteEnables, time);
return result;
}
bool cdns_xspi_ctrl_reg::handle_write_ers_seq_cfg_2_ersa_seq_p1_cmd_val(const unsigned int& value, sc_core::sc_time& time) {
this->ers_seq_cfg_2.ersa_seq_p1_cmd_val = value;
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_prog_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->prog_seq_cfg_0 = ((this->prog_seq_cfg_0 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_prog_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->prog_seq_cfg_1 = ((this->prog_seq_cfg_1 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_prog_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->prog_seq_cfg_2 = ((this->prog_seq_cfg_2 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_read_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->read_seq_cfg_0 = ((this->read_seq_cfg_0 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_read_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->read_seq_cfg_1 = ((this->read_seq_cfg_1 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_read_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->read_seq_cfg_2 = ((this->read_seq_cfg_2 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_we_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->we_seq_cfg_0 = ((this->we_seq_cfg_0 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_0 = ((this->stat_seq_cfg_0 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_1 = ((this->stat_seq_cfg_1 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_2(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_2 = ((this->stat_seq_cfg_2 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_3(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_3 = ((this->stat_seq_cfg_3 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_4(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_4 = ((this->stat_seq_cfg_4 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_5(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_5 = ((this->stat_seq_cfg_5 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_7(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_7 = ((this->stat_seq_cfg_7 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_8(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_8 = ((this->stat_seq_cfg_8 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_9(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_9 = ((this->stat_seq_cfg_9 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_stat_seq_cfg_10(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->stat_seq_cfg_10 = ((this->stat_seq_cfg_10 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_wp_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->wp_settings = ((this->wp_settings & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_reset_pin_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->reset_pin_settings = ((this->reset_pin_settings & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_clock_mode_settings(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->clock_mode_settings = ((this->clock_mode_settings & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_jedec_rst_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->jedec_rst_timing_reg = ((this->jedec_rst_timing_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_dev_delay_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->dev_delay_reg = ((this->dev_delay_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_rst_recovery_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->rst_recovery_reg = ((this->rst_recovery_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_dev_active_max_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->dev_active_max_reg = ((this->dev_active_max_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_hf_offset_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->hf_offset_reg = ((this->hf_offset_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_dll_phy_update_cnt(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->dll_phy_update_cnt = ((this->dll_phy_update_cnt & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_dll_phy_ctrl(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->dll_phy_ctrl = ((this->dll_phy_ctrl & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_dq_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_dq_timing_reg = ((this->phy_dq_timing_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_dqs_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_dqs_timing_reg = ((this->phy_dqs_timing_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_gate_lpbk_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_gate_lpbk_ctrl_reg = ((this->phy_gate_lpbk_ctrl_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_dll_master_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_dll_master_ctrl_reg = ((this->phy_dll_master_ctrl_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_dll_slave_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_dll_slave_ctrl_reg = ((this->phy_dll_slave_ctrl_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_ie_timing_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_ie_timing_reg = ((this->phy_ie_timing_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_static_togg_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_static_togg_reg = ((this->phy_static_togg_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_wr_deskew_pd_ctrl_0_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_wr_deskew_pd_ctrl_0_reg = ((this->phy_wr_deskew_pd_ctrl_0_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_ctrl_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_ctrl_reg = ((this->phy_ctrl_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_tsel_reg(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_tsel_reg = ((this->phy_tsel_reg & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_gpio_ctrl_0(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_gpio_ctrl_0 = ((this->phy_gpio_ctrl_0 & ~byteEnables) | (value & byteEnables));
return true;
}
bool cdns_xspi_ctrl_reg::handle_write_phy_gpio_ctrl_1(const unsigned int& value, const unsigned int& byteEnables, sc_core::sc_time& time) {
this->phy_gpio_ctrl_1 = ((this->phy_gpio_ctrl_1 & ~byteEnables) | (value & byteEnables));
return true;
}
}  // end of namespace mylibrary

