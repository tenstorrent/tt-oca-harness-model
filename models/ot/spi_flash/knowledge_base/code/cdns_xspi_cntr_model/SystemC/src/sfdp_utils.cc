// #include "sfdp.h"
// #include <iostream>
// #include <iomanip>
// #include <string>
// #include <vector>
// #include <fstream>
// #include <cstring>

// // ============================================================================
// // SFDP UTILITY FUNCTIONS
// // ============================================================================
// // These functions are separated from sfdp.cpp to avoid main() conflicts
// // when used by other modules like xspi_target
// // ============================================================================

// std::string addr_mode_to_string(sfdp_addr_mode_e mode) {
//     switch(mode) {
//         case ADDR_3_BYTE_ONLY: return "3-Byte Only";
//         case ADDR_3_OR_4_BYTE: return "3-Byte or 4-Byte";
//         case ADDR_4_BYTE_ONLY: return "4-Byte Only";
//         default: return "Unknown";
//     }
// }

// std::string qer_to_string(sfdp_qer_e qer) {
//     switch(qer) {
//         case QER_NONE_OR_HOLD: return "None or HOLD";
//         case QER_BIT1_SR2_REG: return "Bit 1 of SR2";
//         case QER_BIT6_SR1_REG: return "Bit 6 of SR1";
//         case QER_BIT7_SR2_OP3E: return "Bit 7 of SR2 (via 3Eh)";
//         case QER_BIT1_SR2_NO_CLR: return "Bit 1 of SR2 (no clear)";
//         case QER_BIT1_SR2_OP35: return "Bit 1 of SR2 (via 35h)";
//         default: return "Unknown";
//     }
// }

// void print_sfdp_tree(const sfdp_header_t& header, 
//                      const sfdp_parameter_header_t& p_header,
//                      const jedec_basic_table_t& table) {
//     std::cout << "\n";
//     std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
//     std::cout << "║                    SFDP STRUCTURE TREE                    ║" << std::endl;
//     std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
//     std::cout << std::endl;
    
//     // ========================================================================
//     // SFDP HEADER
//     // ========================================================================
//     std::cout << "SFDP Header (Address: 0x00)" << std::endl;
//     std::cout << "├── Signature:           0x" << std::hex << std::setw(8) << std::setfill('0') 
//               << header.signature << " (\"PDFS\")" << std::dec << std::endl;
//     std::cout << "├── Major Revision:     " << (int)header.major_rev << std::endl;
//     std::cout << "├── Minor Revision:      " << (int)header.minor_rev << std::endl;
//     std::cout << "├── NPH:                 " << (int)header.nph << " (Number of Parameter Headers: " 
//               << ((int)header.nph + 1) << ")" << std::endl;
//     std::cout << "└── Unused:              0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)header.unused << std::dec << std::endl;
//     std::cout << std::endl;
    
//     // ========================================================================
//     // PARAMETER HEADER
//     // ========================================================================
//     std::cout << "Parameter Header (Address: 0x08)" << std::endl;
//     std::cout << "├── Parameter ID LSB:    0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)p_header.id_lsb << std::dec << " (JEDEC Basic Table)" << std::endl;
//     std::cout << "├── Parameter ID MSB:    0xFF (JEDEC)" << std::endl;
//     std::cout << "├── Minor Revision:      " << (int)p_header.minor_rev << std::endl;
//     std::cout << "├── Major Revision:      " << (int)p_header.major_rev << std::endl;
//     std::cout << "├── Length (DWORDs):     " << (int)p_header.length_dwords << std::endl;
//     std::cout << "└── Table Pointer:       0x" << std::hex << std::setw(6) << std::setfill('0') 
//               << p_header.ptp << std::dec << std::endl;
//     std::cout << std::endl;
    
//     // ========================================================================
//     // BASIC FLASH PARAMETER TABLE - ALL 16 DWORDS
//     // ========================================================================
//     std::cout << "Basic Flash Parameter Table (Address: 0x" << std::hex << p_header.ptp << std::dec << ")" << std::endl;
    
//     // DWORD 1: Architecture & Fast Read Support
//     std::cout << "├── DWORD 1: Architecture & Fast Read Support" << std::endl;
//     uint64_t density = table.get_density();
//     std::cout << "│   ├── Density:         " << density << " bits (" 
//               << (density / 1024 / 1024) << " Mbits, " 
//               << (density / 8 / 1024 / 1024) << " MB)" << std::endl;
//     std::cout << "│   ├── Address Mode:     " << addr_mode_to_string(table.get_address_bytes()) << std::endl;
//     std::cout << "│   ├── Erase Size Support: " << (int)table.get_dword1().get_erase_size() << std::endl;
//     std::cout << "│   ├── Write Granularity: " << (table.get_dword1().get_write_granularity() ? "64 bytes" : "1 byte") << std::endl;
//     std::cout << "│   ├── Volatile BP:      " << (table.get_dword1().get_volatile_status_register() ? "Yes" : "No") << std::endl;
//     std::cout << "│   ├── Write Enable:     " << (table.get_dword1().get_write_enable_opcode_select() ? "06h" : "50h") << std::endl;
//     std::cout << "│   ├── 4KB Erase Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword1().get_erase_4kb_instruction() << std::dec << std::endl;
//     std::cout << "│   ├── DTR Support:     " << (table.get_dword1().get_dtr_clocking_support() ? "Yes" : "No") << std::endl;
//     bool fast_114 = table.get_dword1().get_fast_read_1_1_4_support();
//     bool fast_144 = table.get_dword1().get_fast_read_1_4_4_support();
//     bool fast_122 = table.get_dword1().get_fast_read_1_2_2_support();
//     bool fast_112 = table.get_dword1().get_fast_read_1_1_2_support();
//     std::cout << "│   ├── Fast Read 1-1-4:  " << (fast_114 ? "Supported" : "Not Supported");
//     if (fast_114) {
//         uint8_t op = table.get_read_opcode_1_1_4();
//         std::cout << " (Opcode: 0x" << std::hex << (int)op << std::dec << ")";
//     }
//     std::cout << std::endl;
//     std::cout << "│   ├── Fast Read 1-4-4:  " << (fast_144 ? "Supported" : "Not Supported") << std::endl;
//     std::cout << "│   ├── Fast Read 1-2-2:  " << (fast_122 ? "Supported" : "Not Supported") << std::endl;
//     std::cout << "│   └── Fast Read 1-1-2:  " << (fast_112 ? "Supported" : "Not Supported") << std::endl;
    
//     // DWORD 2: Density (explicit)
//     std::cout << "├── DWORD 2: Density" << std::endl;
//     uint64_t dword2_density = table.get_dword2().get_density();
//     std::cout << "│   └── Density:         " << dword2_density << " bits (" 
//               << (dword2_density / 1024 / 1024) << " Mbits)" << std::endl;
    
//     // DWORD 3: (1-4-4) and (1-1-4) Fast Read Parameters
//     std::cout << "├── DWORD 3: (1-4-4) and (1-1-4) Fast Read Parameters" << std::endl;
//     std::cout << "│   ├── (1-4-4) Wait States: " << (int)table.get_dword3().get_1_4_4_wait_states() << std::endl;
//     std::cout << "│   ├── (1-4-4) Mode Clocks: " << (int)table.get_dword3().get_1_4_4_mode_clocks() << std::endl;
//     std::cout << "│   ├── (1-4-4) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword3().get_1_4_4_opcode() << std::dec << std::endl;
//     std::cout << "│   ├── (1-1-4) Wait States: " << (int)table.get_dword3().get_1_1_4_wait_states() << std::endl;
//     std::cout << "│   ├── (1-1-4) Mode Clocks: " << (int)table.get_dword3().get_1_1_4_mode_clocks() << std::endl;
//     std::cout << "│   └── (1-1-4) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword3().get_1_1_4_opcode() << std::dec << std::endl;
    
//     // DWORD 4: (1-1-2) and (1-2-2) Fast Read Parameters
//     std::cout << "├── DWORD 4: (1-1-2) and (1-2-2) Fast Read Parameters" << std::endl;
//     std::cout << "│   ├── (1-1-2) Wait States: " << (int)table.get_dword4().get_1_1_2_wait_states() << std::endl;
//     std::cout << "│   ├── (1-1-2) Mode Clocks: " << (int)table.get_dword4().get_1_1_2_mode_clocks() << std::endl;
//     std::cout << "│   ├── (1-1-2) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword4().get_1_1_2_opcode() << std::dec << std::endl;
//     std::cout << "│   ├── (1-2-2) Wait States: " << (int)table.get_dword4().get_1_2_2_wait_states() << std::endl;
//     std::cout << "│   ├── (1-2-2) Mode Clocks: " << (int)table.get_dword4().get_1_2_2_mode_clocks() << std::endl;
//     std::cout << "│   └── (1-2-2) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword4().get_1_2_2_opcode() << std::dec << std::endl;
    
//     // DWORD 5: (2-2-2) and (4-4-4) Support Flags
//     std::cout << "├── DWORD 5: (2-2-2) and (4-4-4) Support Flags" << std::endl;
//     std::cout << "│   ├── (2-2-2) Support:    " << (table.get_dword5().get_2_2_2_support() ? "Yes" : "No") << std::endl;
//     std::cout << "│   └── (4-4-4) Support:    " << (table.get_dword5().get_4_4_4_support() ? "Yes" : "No") << std::endl;
    
//     // DWORD 6: (2-2-2) Parameters
//     std::cout << "├── DWORD 6: (2-2-2) Parameters" << std::endl;
//     std::cout << "│   ├── (2-2-2) Wait States: " << (int)table.get_dword6().get_2_2_2_wait_states() << std::endl;
//     std::cout << "│   ├── (2-2-2) Mode Clocks: " << (int)table.get_dword6().get_2_2_2_mode_clocks() << std::endl;
//     std::cout << "│   └── (2-2-2) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword6().get_2_2_2_opcode() << std::dec << std::endl;
    
//     // DWORD 7: (4-4-4) Parameters
//     std::cout << "├── DWORD 7: (4-4-4) Parameters" << std::endl;
//     std::cout << "│   ├── (4-4-4) Wait States: " << (int)table.get_dword7().get_4_4_4_wait_states() << std::endl;
//     std::cout << "│   ├── (4-4-4) Mode Clocks: " << (int)table.get_dword7().get_4_4_4_mode_clocks() << std::endl;
//     std::cout << "│   └── (4-4-4) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword7().get_4_4_4_opcode() << std::dec << std::endl;
    
//     // DWORD 8: Erase Types 1 & 2
//     std::cout << "├── DWORD 8: Erase Types 1 & 2" << std::endl;
//     uint8_t size_pow2, opcode;
//     table.get_sector_erase_type1(size_pow2, opcode);
//     uint32_t erase_size = (1 << size_pow2);
//     std::cout << "│   ├── Erase Type 1:    " << erase_size << " bytes (" 
//               << (erase_size / 1024) << " KB) - Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)opcode << std::dec << std::endl;
//     uint8_t type2_size = table.get_dword8().get_erase_type2_size();
//     uint8_t type2_op = table.get_dword8().get_erase_type2_opcode();
//     if (type2_size > 0 && type2_size < 32) {
//         uint32_t type2_erase_size = (1 << type2_size);
//         std::cout << "│   └── Erase Type 2:    " << type2_erase_size << " bytes (" 
//                   << (type2_erase_size / 1024) << " KB) - Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//                   << (int)type2_op << std::dec << std::endl;
//     } else {
//         std::cout << "│   └── Erase Type 2:    Not Defined" << std::endl;
//     }
    
//     // DWORD 9: Erase Types 3 & 4
//     std::cout << "├── DWORD 9: Erase Types 3 & 4" << std::endl;
//     uint8_t type3_size = table.get_dword9().get_type3_size_pow2();
//     uint8_t type3_op = table.get_dword9().get_type3_opcode();
//     if (type3_size > 0 && type3_size < 32) {
//         uint32_t type3_erase_size = (1 << type3_size);
//         std::cout << "│   ├── Erase Type 3:    " << type3_erase_size << " bytes (" 
//                   << (type3_erase_size / 1024) << " KB) - Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//                   << (int)type3_op << std::dec << std::endl;
//     } else {
//         std::cout << "│   ├── Erase Type 3:    Not Defined" << std::endl;
//     }
//     uint8_t type4_size = table.get_dword9().get_type4_size_pow2();
//     uint8_t type4_op = table.get_dword9().get_type4_opcode();
//     if (type4_size > 0 && type4_size < 32) {
//         uint32_t type4_erase_size = (1 << type4_size);
//         std::cout << "│   └── Erase Type 4:    " << type4_erase_size << " bytes (" 
//                   << (type4_erase_size / 1024) << " KB) - Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//                   << (int)type4_op << std::dec << std::endl;
//     } else {
//         std::cout << "│   └── Erase Type 4:    Not Defined" << std::endl;
//     }
    
//     // DWORD 10: Erase Timings
//     std::cout << "├── DWORD 10: Erase Timings" << std::endl;
//     uint8_t erase_mult = table.get_dword10().get_multiplier();
//     std::cout << "│   ├── Multiplier:      " << (int)erase_mult << std::endl;
//     const char* erase_unit_names[] = {"1ms", "16ms", "128ms", "1s"};
//     for (int i = 1; i <= 4; i++) {
//         uint8_t count, units;
//         table.get_dword10().get_erase_timing(i, count, units);
//         uint32_t time_ms = 0;
//         if (units == 0) time_ms = (count + 1) * 1;
//         else if (units == 1) time_ms = (count + 1) * 16;
//         else if (units == 2) time_ms = (count + 1) * 128;
//         else if (units == 3) time_ms = (count + 1) * 1000;
//         time_ms *= (erase_mult + 1);
//         std::cout << "│   ├── Erase Type " << i << " Time: " << (count + 1) << " x " 
//                   << erase_unit_names[units] << " = " << time_ms << "ms (typical)" << std::endl;
//     }
//     std::cout << "│   └── (Typical times shown)" << std::endl;
    
//     // DWORD 11: Chip Erase & Program Timings
//     std::cout << "├── DWORD 11: Chip Erase & Program Timings" << std::endl;
//     uint8_t prog_mult = table.get_dword11().get_prog_multiplier();
//     uint8_t page_size_pow2 = table.get_dword11().get_page_size_pow2();
//     uint32_t page_size = (1 << page_size_pow2);
//     std::cout << "│   ├── Program Multiplier: " << (int)prog_mult << std::endl;
//     std::cout << "│   ├── Page Size:       " << page_size << " bytes" << std::endl;
//     bool unit_64us;
//     uint8_t count;
//     table.get_dword11().get_page_prog(count, unit_64us);
//     uint32_t page_prog_time = (count + 1) * (unit_64us ? 64 : 8) * (prog_mult + 1);
//     std::cout << "│   ├── Page Program:    " << (count + 1) << " x " << (unit_64us ? "64us" : "8us") 
//               << " = " << page_prog_time << "us (typical)" << std::endl;
//     bool unit_8us;
//     table.get_dword11().get_byte_prog_first(count, unit_8us);
//     uint32_t byte_prog_first = (count + 1) * (unit_8us ? 8 : 1) * (prog_mult + 1);
//     std::cout << "│   ├── Byte Prog First: " << (count + 1) << " x " << (unit_8us ? "8us" : "1us") 
//               << " = " << byte_prog_first << "us (typical)" << std::endl;
//     table.get_dword11().get_byte_prog_add(count, unit_8us);
//     uint32_t byte_prog_add = (count + 1) * (unit_8us ? 8 : 1) * (prog_mult + 1);
//     std::cout << "│   ├── Byte Prog Add:   " << (count + 1) << " x " << (unit_8us ? "8us" : "1us") 
//               << " = " << byte_prog_add << "us (typical)" << std::endl;
//     uint8_t chip_erase_count, chip_erase_units;
//     table.get_dword11().get_chip_erase_time(chip_erase_count, chip_erase_units);
//     const char* chip_erase_unit_names[] = {"16ms", "256ms", "4s", "64s"};
//     uint32_t chip_erase_time = (chip_erase_count + 1);
//     if (chip_erase_units < 4) {
//         std::cout << "│   └── Chip Erase:      " << chip_erase_time << " x " 
//                   << chip_erase_unit_names[chip_erase_units] << " (typical)" << std::endl;
//     } else {
//         std::cout << "│   └── Chip Erase:      Not Defined" << std::endl;
//     }
    
//     // DWORD 12: Suspend/Resume Limits
//     std::cout << "├── DWORD 12: Suspend/Resume Limits" << std::endl;
//     bool suspend_supported = table.get_dword12().get_suspend_supported();
//     std::cout << "│   ├── Suspend Supported: " << (suspend_supported ? "Yes" : "No") << std::endl;
//     uint8_t prog_suspend_count, prog_suspend_unit;
//     table.get_dword12().get_prog_suspend_max(prog_suspend_count, prog_suspend_unit);
//     const char* suspend_unit_names[] = {"128ns", "256ns", "512ns", "64us"};
//     if (prog_suspend_unit < 4) {
//         std::cout << "│   ├── Prog Suspend Max: " << (prog_suspend_count + 1) << " x " 
//                   << suspend_unit_names[prog_suspend_unit] << std::endl;
//     }
//     uint8_t erase_suspend_count, erase_suspend_unit;
//     table.get_dword12().get_erase_suspend_max(erase_suspend_count, erase_suspend_unit);
//     if (erase_suspend_unit < 4) {
//         std::cout << "│   └── Erase Suspend Max: " << (erase_suspend_count + 1) << " x " 
//                   << suspend_unit_names[erase_suspend_unit] << std::endl;
//     }
    
//     // DWORD 13: Suspend/Resume Instructions
//     std::cout << "├── DWORD 13: Suspend/Resume Instructions" << std::endl;
//     std::cout << "│   ├── Program Resume:   0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword13().get_prog_resume_op() << std::dec << std::endl;
//     std::cout << "│   ├── Program Suspend: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword13().get_prog_suspend_op() << std::dec << std::endl;
//     std::cout << "│   ├── Resume:         0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword13().get_resume_op() << std::dec << std::endl;
//     std::cout << "│   └── Suspend:        0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword13().get_suspend_op() << std::dec << std::endl;
    
//     // DWORD 14: Deep Powerdown & Polling
//     std::cout << "├── DWORD 14: Deep Powerdown & Polling" << std::endl;
//     bool dpd_supported = table.get_dword14().get_dpd_supported();
//     std::cout << "│   ├── DPD Supported:   " << (dpd_supported ? "Yes" : "No") << std::endl;
//     std::cout << "│   ├── Poll Status Legacy: " << (table.get_dword14().get_poll_status_legacy() ? "Yes" : "No") << std::endl;
//     std::cout << "│   ├── Poll Status Flag: " << (table.get_dword14().get_poll_status_flag() ? "Yes" : "No") << std::endl;
//     uint8_t exit_dpd_count, exit_dpd_unit;
//     table.get_dword14().get_exit_dpd_delay(exit_dpd_count, exit_dpd_unit);
//     const char* dpd_unit_names[] = {"128ns", "256ns", "512ns", "64us"};
//     if (exit_dpd_unit < 4) {
//         std::cout << "│   ├── Exit DPD Delay:   " << (exit_dpd_count + 1) << " x " 
//                   << dpd_unit_names[exit_dpd_unit] << std::endl;
//     }
//     std::cout << "│   ├── Exit DPD Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword14().get_exit_dpd_op() << std::dec << std::endl;
//     std::cout << "│   └── Enter DPD Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword14().get_enter_dpd_op() << std::dec << std::endl;
    
//     // DWORD 15: Quad Enable & Advanced Features
//     std::cout << "├── DWORD 15: Quad Enable & Advanced Features" << std::endl;
//     sfdp_qer_e qer = table.get_dword15().get_quad_enable_requirement();
//     std::cout << "│   ├── Quad Enable:     " << qer_to_string(qer) << std::endl;
//     bool mode_044 = table.get_dword15().get_0_4_4_mode_support();
//     std::cout << "│   ├── 0-4-4 Mode:      " << (mode_044 ? "Supported" : "Not Supported") << std::endl;
//     std::cout << "│   ├── 4-4-4 Enable Seq: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword15().get_4_4_4_enable_sequence() << std::dec << std::endl;
//     std::cout << "│   └── 4-4-4 Disable Seq: 0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)table.get_dword15().get_4_4_4_disable_sequence() << std::dec << std::endl;
    
//     // DWORD 16: 4-Byte Addressing & Soft Reset
//     std::cout << "└── DWORD 16: 4-Byte Addressing & Soft Reset" << std::endl;
//     uint8_t soft_reset = table.get_dword16().get_soft_reset_support();
//     std::cout << "    ├── Soft Reset:      ";
//     if (soft_reset == 0) {
//         std::cout << "Not Supported";
//     } else {
//         if (soft_reset & 0x10) std::cout << "F0h ";
//         if (soft_reset & 0x08) std::cout << "66h/99h ";
//     }
//     std::cout << std::endl;
//     uint8_t entry_method = table.get_dword16().get_4byte_addr_entry_method();
//     uint16_t exit_method = table.get_dword16().get_4byte_addr_exit_method();
//     std::cout << "    ├── 4-Byte Entry:    0x" << std::hex << std::setw(2) << std::setfill('0') 
//               << (int)entry_method << std::dec << std::endl;
//     std::cout << "    └── 4-Byte Exit:     0x" << std::hex << std::setw(3) << std::setfill('0') 
//               << exit_method << std::dec << std::endl;
    
//     std::cout << std::endl;
//     std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
//     std::cout << "║                    END OF SFDP TREE                        ║" << std::endl;
//     std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
//     std::cout << std::endl;
// }

// // ============================================================================
// // SFDP PARSER FUNCTIONS
// // ============================================================================

// // Helper function to read a 32-bit DWORD from byte vector (little-endian)
// static uint32_t read_dword_le(const std::vector<uint8_t>& data, size_t offset) {
//     if (offset + 4 > data.size()) {
//         return 0xFFFFFFFF; // Return default value if out of bounds
//     }
//     return (static_cast<uint32_t>(data[offset + 0]) << 0) |
//            (static_cast<uint32_t>(data[offset + 1]) << 8) |
//            (static_cast<uint32_t>(data[offset + 2]) << 16) |
//            (static_cast<uint32_t>(data[offset + 3]) << 24);
// }

// // Parse SFDP header from byte vector (little-endian)
// bool parse_sfdp_header(const std::vector<uint8_t>& data, size_t offset, sfdp_header_t& header) {
//     if (offset + 8 > data.size()) {
//         return false; // Not enough data
//     }
    
//     // Read signature (little-endian, 4 bytes)
//     header.signature = read_dword_le(data, offset);
    
//     // Read revision and header fields (4 bytes)
//     header.minor_rev = data[offset + 4];
//     header.major_rev = data[offset + 5];
//     header.nph = data[offset + 6];
//     header.unused = data[offset + 7];
    
//     // Validate signature
//     if (header.signature != SFDP_SIGNATURE) {
//         return false;
//     }
    
//     return true;
// }

// // Parse parameter header from byte vector (little-endian)
// bool parse_parameter_header(const std::vector<uint8_t>& data, size_t offset, sfdp_parameter_header_t& p_header) {
//     if (offset + 8 > data.size()) {
//         return false; // Not enough data
//     }
    
//     // Read parameter header fields (8 bytes)
//     p_header.id_lsb = data[offset + 0];
//     p_header.minor_rev = data[offset + 1];
//     p_header.major_rev = data[offset + 2];
//     p_header.length_dwords = data[offset + 3];
    
//     // Read parameter table pointer (24-bit, little-endian, 3 bytes)
//     p_header.ptp = (static_cast<uint32_t>(data[offset + 4]) << 0) |
//                    (static_cast<uint32_t>(data[offset + 5]) << 8) |
//                    (static_cast<uint32_t>(data[offset + 6]) << 16);
//     // Byte 7 is reserved/unused
    
//     return true;
// }

// // Parse basic flash parameter table (16 DWORDs) from byte vector (little-endian)
// bool parse_basic_table(const std::vector<uint8_t>& data, size_t offset, jedec_basic_table_t& table) {
//     if (offset + (16 * 4) > data.size()) {
//         return false; // Not enough data for 16 DWORDs
//     }
    
//     // Parse each DWORD and load into the corresponding structure
//     table.get_dword1().from_dword(read_dword_le(data, offset + 0 * 4));
//     table.get_dword2().from_dword(read_dword_le(data, offset + 1 * 4));
//     table.get_dword3().from_dword(read_dword_le(data, offset + 2 * 4));
//     table.get_dword4().from_dword(read_dword_le(data, offset + 3 * 4));
//     table.get_dword5().from_dword(read_dword_le(data, offset + 4 * 4));
//     table.get_dword6().from_dword(read_dword_le(data, offset + 5 * 4));
//     table.get_dword7().from_dword(read_dword_le(data, offset + 6 * 4));
//     table.get_dword8().from_dword(read_dword_le(data, offset + 7 * 4));
//     table.get_dword9().from_dword(read_dword_le(data, offset + 8 * 4));
//     table.get_dword10().from_dword(read_dword_le(data, offset + 9 * 4));
//     table.get_dword11().from_dword(read_dword_le(data, offset + 10 * 4));
//     table.get_dword12().from_dword(read_dword_le(data, offset + 11 * 4));
//     table.get_dword13().from_dword(read_dword_le(data, offset + 12 * 4));
//     table.get_dword14().from_dword(read_dword_le(data, offset + 13 * 4));
//     table.get_dword15().from_dword(read_dword_le(data, offset + 14 * 4));
//     table.get_dword16().from_dword(read_dword_le(data, offset + 15 * 4));
    
//     return true;
// }

// // Parse complete SFDP structure from byte vector
// // Assumes standard layout: Header@0x00, Parameter Header@0x08, Table@PTP
// bool parse_sfdp_from_bytes(const std::vector<uint8_t>& data,
//                            sfdp_header_t& header,
//                            sfdp_parameter_header_t& p_header,
//                            jedec_basic_table_t& table) {
//     // Parse SFDP header at offset 0x00
//     if (!parse_sfdp_header(data, 0x00, header)) {
//         return false;
//     }
    
//     // Parse parameter header at offset 0x08
//     if (!parse_parameter_header(data, 0x08, p_header)) {
//         return false;
//     }
    
//     // Parse basic table at the pointer specified in parameter header
//     uint32_t table_offset = p_header.ptp;
//     if (!parse_basic_table(data, table_offset, table)) {
//         return false;
//     }
    
//     return true;
// }

// // Parse SFDP data from a binary file
// bool parse_sfdp_from_file(const std::string& filename,
//                           sfdp_header_t& header,
//                           sfdp_parameter_header_t& p_header,
//                           jedec_basic_table_t& table) {
//     std::ifstream file(filename, std::ios::binary | std::ios::ate);
//     if (!file.is_open()) {
//         return false;
//     }
    
//     // Get file size
//     std::streamsize size = file.tellg();
//     file.seekg(0, std::ios::beg);
    
//     // Read entire file into vector
//     std::vector<uint8_t> data(size);
//     if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
//         return false;
//     }
    
//     file.close();
    
//     // Parse from byte vector
//     return parse_sfdp_from_bytes(data, header, p_header, table);
// }





#include "sfdp.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>

// ============================================================================
// SFDP UTILITY FUNCTIONS
// ============================================================================

// PUBLIC: Helper function to read a 32-bit DWORD from byte vector (little-endian)
uint32_t read_le32(const std::vector<uint8_t>& buf, size_t offset)
{
    if (offset + 4 > buf.size()) {
        return 0xFFFFFFFF;
    }

    return (static_cast<uint32_t>(buf[offset + 0]) << 0)  |
           (static_cast<uint32_t>(buf[offset + 1]) << 8)  |
           (static_cast<uint32_t>(buf[offset + 2]) << 16) |
           (static_cast<uint32_t>(buf[offset + 3]) << 24);
}

std::string addr_mode_to_string(sfdp_addr_mode_e mode) {
    switch(mode) {
        case ADDR_3_BYTE_ONLY: return "3-Byte Only";
        case ADDR_3_OR_4_BYTE: return "3-Byte or 4-Byte";
        case ADDR_4_BYTE_ONLY: return "4-Byte Only";
        default: return "Unknown";
    }
}

// PRIVATE: Internal utility function
static std::string qer_to_string(sfdp_qer_e qer) {
    switch(qer) {
        case QER_NONE_OR_HOLD: return "None or HOLD";
        case QER_BIT1_SR2_REG: return "Bit 1 of SR2";
        case QER_BIT6_SR1_REG: return "Bit 6 of SR1";
        case QER_BIT7_SR2_OP3E: return "Bit 7 of SR2 (via 3Eh)";
        case QER_BIT1_SR2_NO_CLR: return "Bit 1 of SR2 (no clear)";
        case QER_BIT1_SR2_OP35: return "Bit 1 of SR2 (via 35h)";
        default: return "Unknown";
    }
}

// ... rest of the file remains the same, but use read_le32 instead of read_dword_le ...

// PRIVATE: Parse SFDP header from byte vector (little-endian)
static bool parse_sfdp_header(const std::vector<uint8_t>& data, size_t offset, sfdp_header_t& header) {
    if (offset + 8 > data.size()) {
        return false; // Not enough data
    }
    
    // Read signature (little-endian, 4 bytes)
    header.signature = read_le32(data, offset);
    
    // Read revision and header fields (4 bytes)
    header.minor_rev = data[offset + 4];
    header.major_rev = data[offset + 5];
    header.nph = data[offset + 6];
    header.unused = data[offset + 7];
    
    // Validate signature
    if (header.signature != SFDP_SIGNATURE) {
        return false;
    }
    
    return true;
}

// PRIVATE: Parse parameter header from byte vector (little-endian)
static bool parse_parameter_header(const std::vector<uint8_t>& data, size_t offset, sfdp_parameter_header_t& p_header) {
    if (offset + 8 > data.size()) {
        return false; // Not enough data
    }
    
    // Read parameter header fields (8 bytes)
    p_header.id_lsb = data[offset + 0];
    p_header.minor_rev = data[offset + 1];
    p_header.major_rev = data[offset + 2];
    p_header.length_dwords = data[offset + 3];
    
    // Read parameter table pointer (24-bit, little-endian, 3 bytes)
    p_header.ptp = (static_cast<uint32_t>(data[offset + 4]) << 0) |
                   (static_cast<uint32_t>(data[offset + 5]) << 8) |
                   (static_cast<uint32_t>(data[offset + 6]) << 16);
    // Byte 7 is reserved/unused
    
    return true;
}

// PRIVATE: Parse basic flash parameter table (16 DWORDs) from byte vector (little-endian)
static bool parse_basic_table(const std::vector<uint8_t>& data, size_t offset, jedec_basic_table_t& table) {
    if (offset + (16 * 4) > data.size()) {
        return false; // Not enough data for 16 DWORDs
    }
    
    // Parse each DWORD and load into the corresponding structure
    table.get_dword1().from_dword(read_le32(data, offset + 0 * 4));
    table.get_dword2().from_dword(read_le32(data, offset + 1 * 4));
    table.get_dword3().from_dword(read_le32(data, offset + 2 * 4));
    table.get_dword4().from_dword(read_le32(data, offset + 3 * 4));
    table.get_dword5().from_dword(read_le32(data, offset + 4 * 4));
    table.get_dword6().from_dword(read_le32(data, offset + 5 * 4));
    table.get_dword7().from_dword(read_le32(data, offset + 6 * 4));
    table.get_dword8().from_dword(read_le32(data, offset + 7 * 4));
    table.get_dword9().from_dword(read_le32(data, offset + 8 * 4));
    table.get_dword10().from_dword(read_le32(data, offset + 9 * 4));
    table.get_dword11().from_dword(read_le32(data, offset + 10 * 4));
    table.get_dword12().from_dword(read_le32(data, offset + 11 * 4));
    table.get_dword13().from_dword(read_le32(data, offset + 12 * 4));
    table.get_dword14().from_dword(read_le32(data, offset + 13 * 4));
    table.get_dword15().from_dword(read_le32(data, offset + 14 * 4));
    table.get_dword16().from_dword(read_le32(data, offset + 15 * 4));
    
    return true;
}

// PUBLIC: Parse complete SFDP structure from byte vector
bool parse_sfdp_from_bytes(const std::vector<uint8_t>& data,
                           sfdp_header_t& header,
                           sfdp_parameter_header_t& p_header,
                           jedec_basic_table_t& table) {
    // Parse SFDP header at offset 0x00
    if (!parse_sfdp_header(data, 0x00, header)) {
        return false;
    }
    
    // Parse parameter header at offset 0x08
    if (!parse_parameter_header(data, 0x08, p_header)) {
        return false;
    }
    
    // Parse basic table at the pointer specified in parameter header
    uint32_t table_offset = p_header.ptp;
    if (!parse_basic_table(data, table_offset, table)) {
        return false;
    }
    
    return true;
}

// PUBLIC: Parse SFDP data from a binary file
bool parse_sfdp_from_file(const std::string& filename,
                          sfdp_header_t& header,
                          sfdp_parameter_header_t& p_header,
                          jedec_basic_table_t& table) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    
    // Get file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read entire file into vector
    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        return false;
    }
    
    file.close();
    
    // Parse from byte vector
    return parse_sfdp_from_bytes(data, header, p_header, table);
}

// PUBLIC: Print SFDP tree
void print_sfdp_tree(const sfdp_header_t& header, 
                     const sfdp_parameter_header_t& p_header,
                     const jedec_basic_table_t& table) {
     std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    SFDP STRUCTURE TREE                    ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    // ========================================================================
    // SFDP HEADER
    // ========================================================================
    std::cout << "SFDP Header (Address: 0x00)" << std::endl;
    std::cout << "├── Signature:           0x" << std::hex << std::setw(8) << std::setfill('0') 
              << header.signature << " (\"PDFS\")" << std::dec << std::endl;
    std::cout << "├── Major Revision:     " << (int)header.major_rev << std::endl;
    std::cout << "├── Minor Revision:      " << (int)header.minor_rev << std::endl;
    std::cout << "├── NPH:                 " << (int)header.nph << " (Number of Parameter Headers: " 
              << ((int)header.nph + 1) << ")" << std::endl;
    std::cout << "└── Unused:              0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)header.unused << std::dec << std::endl;
    std::cout << std::endl;
    
    // ========================================================================
    // PARAMETER HEADER
    // ========================================================================
    std::cout << "Parameter Header (Address: 0x08)" << std::endl;
    std::cout << "├── Parameter ID LSB:    0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)p_header.id_lsb << std::dec << " (JEDEC Basic Table)" << std::endl;
    std::cout << "├── Parameter ID MSB:    0xFF (JEDEC)" << std::endl;
    std::cout << "├── Minor Revision:      " << (int)p_header.minor_rev << std::endl;
    std::cout << "├── Major Revision:      " << (int)p_header.major_rev << std::endl;
    std::cout << "├── Length (DWORDs):     " << (int)p_header.length_dwords << std::endl;
    std::cout << "└── Table Pointer:       0x" << std::hex << std::setw(6) << std::setfill('0') 
              << p_header.ptp << std::dec << std::endl;
    std::cout << std::endl;
    
    // ========================================================================
    // BASIC FLASH PARAMETER TABLE - ALL 16 DWORDS
    // ========================================================================
    std::cout << "Basic Flash Parameter Table (Address: 0x" << std::hex << p_header.ptp << std::dec << ")" << std::endl;
    
    // DWORD 1: Architecture & Fast Read Support
    std::cout << "├── DWORD 1: Architecture & Fast Read Support" << std::endl;
    uint64_t density = table.get_density();
    std::cout << "│   ├── Density:         " << density << " bits (" 
              << (density / 1024 / 1024) << " Mbits, " 
              << (density / 8 / 1024 / 1024) << " MB)" << std::endl;
    std::cout << "│   ├── Address Mode:     " << addr_mode_to_string(table.get_address_bytes()) << std::endl;
    std::cout << "│   ├── Erase Size Support: " << (int)table.get_dword1().get_erase_size() << std::endl;
    std::cout << "│   ├── Write Granularity: " << (table.get_dword1().get_write_granularity() ? "64 bytes" : "1 byte") << std::endl;
    std::cout << "│   ├── Volatile BP:      " << (table.get_dword1().get_volatile_status_register() ? "Yes" : "No") << std::endl;
    std::cout << "│   ├── Write Enable:     " << (table.get_dword1().get_write_enable_opcode_select() ? "06h" : "50h") << std::endl;
    std::cout << "│   ├── 4KB Erase Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword1().get_erase_4kb_instruction() << std::dec << std::endl;
    std::cout << "│   ├── DTR Support:     " << (table.get_dword1().get_dtr_clocking_support() ? "Yes" : "No") << std::endl;
    bool fast_114 = table.get_dword1().get_fast_read_1_1_4_support();
    bool fast_144 = table.get_dword1().get_fast_read_1_4_4_support();
    bool fast_122 = table.get_dword1().get_fast_read_1_2_2_support();
    bool fast_112 = table.get_dword1().get_fast_read_1_1_2_support();
    std::cout << "│   ├── Fast Read 1-1-4:  " << (fast_114 ? "Supported" : "Not Supported");
    if (fast_114) {
        uint8_t op = table.get_read_opcode_1_1_4();
        std::cout << " (Opcode: 0x" << std::hex << (int)op << std::dec << ")";
    }
    std::cout << std::endl;
    std::cout << "│   ├── Fast Read 1-4-4:  " << (fast_144 ? "Supported" : "Not Supported") << std::endl;
    std::cout << "│   ├── Fast Read 1-2-2:  " << (fast_122 ? "Supported" : "Not Supported") << std::endl;
    std::cout << "│   └── Fast Read 1-1-2:  " << (fast_112 ? "Supported" : "Not Supported") << std::endl;
    
    // DWORD 2: Density (explicit)
    std::cout << "├── DWORD 2: Density" << std::endl;
    uint64_t dword2_density = table.get_dword2().get_density();
    std::cout << "│   └── Density:         " << dword2_density << " bits (" 
              << (dword2_density / 1024 / 1024) << " Mbits)" << std::endl;
    
    // DWORD 3: (1-4-4) and (1-1-4) Fast Read Parameters
    std::cout << "├── DWORD 3: (1-4-4) and (1-1-4) Fast Read Parameters" << std::endl;
    std::cout << "│   ├── (1-4-4) Wait States: " << (int)table.get_dword3().get_1_4_4_wait_states() << std::endl;
    std::cout << "│   ├── (1-4-4) Mode Clocks: " << (int)table.get_dword3().get_1_4_4_mode_clocks() << std::endl;
    std::cout << "│   ├── (1-4-4) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword3().get_1_4_4_opcode() << std::dec << std::endl;
    std::cout << "│   ├── (1-1-4) Wait States: " << (int)table.get_dword3().get_1_1_4_wait_states() << std::endl;
    std::cout << "│   ├── (1-1-4) Mode Clocks: " << (int)table.get_dword3().get_1_1_4_mode_clocks() << std::endl;
    std::cout << "│   └── (1-1-4) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword3().get_1_1_4_opcode() << std::dec << std::endl;
    
    // DWORD 4: (1-1-2) and (1-2-2) Fast Read Parameters
    std::cout << "├── DWORD 4: (1-1-2) and (1-2-2) Fast Read Parameters" << std::endl;
    std::cout << "│   ├── (1-1-2) Wait States: " << (int)table.get_dword4().get_1_1_2_wait_states() << std::endl;
    std::cout << "│   ├── (1-1-2) Mode Clocks: " << (int)table.get_dword4().get_1_1_2_mode_clocks() << std::endl;
    std::cout << "│   ├── (1-1-2) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword4().get_1_1_2_opcode() << std::dec << std::endl;
    std::cout << "│   ├── (1-2-2) Wait States: " << (int)table.get_dword4().get_1_2_2_wait_states() << std::endl;
    std::cout << "│   ├── (1-2-2) Mode Clocks: " << (int)table.get_dword4().get_1_2_2_mode_clocks() << std::endl;
    std::cout << "│   └── (1-2-2) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword4().get_1_2_2_opcode() << std::dec << std::endl;
    
    // DWORD 5: (2-2-2) and (4-4-4) Support Flags
    std::cout << "├── DWORD 5: (2-2-2) and (4-4-4) Support Flags" << std::endl;
    std::cout << "│   ├── (2-2-2) Support:    " << (table.get_dword5().get_2_2_2_support() ? "Yes" : "No") << std::endl;
    std::cout << "│   └── (4-4-4) Support:    " << (table.get_dword5().get_4_4_4_support() ? "Yes" : "No") << std::endl;
    
    // DWORD 6: (2-2-2) Parameters
    std::cout << "├── DWORD 6: (2-2-2) Parameters" << std::endl;
    std::cout << "│   ├── (2-2-2) Wait States: " << (int)table.get_dword6().get_2_2_2_wait_states() << std::endl;
    std::cout << "│   ├── (2-2-2) Mode Clocks: " << (int)table.get_dword6().get_2_2_2_mode_clocks() << std::endl;
    std::cout << "│   └── (2-2-2) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword6().get_2_2_2_opcode() << std::dec << std::endl;
    
    // DWORD 7: (4-4-4) Parameters
    std::cout << "├── DWORD 7: (4-4-4) Parameters" << std::endl;
    std::cout << "│   ├── (4-4-4) Wait States: " << (int)table.get_dword7().get_4_4_4_wait_states() << std::endl;
    std::cout << "│   ├── (4-4-4) Mode Clocks: " << (int)table.get_dword7().get_4_4_4_mode_clocks() << std::endl;
    std::cout << "│   └── (4-4-4) Opcode:     0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword7().get_4_4_4_opcode() << std::dec << std::endl;
    
    // DWORD 8: Erase Types 1 & 2
    std::cout << "├── DWORD 8: Erase Types 1 & 2" << std::endl;
    uint8_t size_pow2, opcode;
    table.get_sector_erase_type1(size_pow2, opcode);
    uint32_t erase_size = (1 << size_pow2);
    std::cout << "│   ├── Erase Type 1:    " << erase_size << " bytes (" 
              << (erase_size / 1024) << " KB) - Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)opcode << std::dec << std::endl;
    uint8_t type2_size = table.get_dword8().get_erase_type2_size();
    uint8_t type2_op = table.get_dword8().get_erase_type2_opcode();
    if (type2_size > 0 && type2_size < 32) {
        uint32_t type2_erase_size = (1 << type2_size);
        std::cout << "│   └── Erase Type 2:    " << type2_erase_size << " bytes (" 
                  << (type2_erase_size / 1024) << " KB) - Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)type2_op << std::dec << std::endl;
    } else {
        std::cout << "│   └── Erase Type 2:    Not Defined" << std::endl;
    }
    
    // DWORD 9: Erase Types 3 & 4
    std::cout << "├── DWORD 9: Erase Types 3 & 4" << std::endl;
    uint8_t type3_size = table.get_dword9().get_type3_size_pow2();
    uint8_t type3_op = table.get_dword9().get_type3_opcode();
    if (type3_size > 0 && type3_size < 32) {
        uint32_t type3_erase_size = (1 << type3_size);
        std::cout << "│   ├── Erase Type 3:    " << type3_erase_size << " bytes (" 
                  << (type3_erase_size / 1024) << " KB) - Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)type3_op << std::dec << std::endl;
    } else {
        std::cout << "│   ├── Erase Type 3:    Not Defined" << std::endl;
    }
    uint8_t type4_size = table.get_dword9().get_type4_size_pow2();
    uint8_t type4_op = table.get_dword9().get_type4_opcode();
    if (type4_size > 0 && type4_size < 32) {
        uint32_t type4_erase_size = (1 << type4_size);
        std::cout << "│   └── Erase Type 4:    " << type4_erase_size << " bytes (" 
                  << (type4_erase_size / 1024) << " KB) - Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)type4_op << std::dec << std::endl;
    } else {
        std::cout << "│   └── Erase Type 4:    Not Defined" << std::endl;
    }
    
    // DWORD 10: Erase Timings
    std::cout << "├── DWORD 10: Erase Timings" << std::endl;
    uint8_t erase_mult = table.get_dword10().get_multiplier();
    std::cout << "│   ├── Multiplier:      " << (int)erase_mult << std::endl;
    const char* erase_unit_names[] = {"1ms", "16ms", "128ms", "1s"};
    for (int i = 1; i <= 4; i++) {
        uint8_t count, units;
        table.get_dword10().get_erase_timing(i, count, units);
        uint32_t time_ms = 0;
        if (units == 0) time_ms = (count + 1) * 1;
        else if (units == 1) time_ms = (count + 1) * 16;
        else if (units == 2) time_ms = (count + 1) * 128;
        else if (units == 3) time_ms = (count + 1) * 1000;
        time_ms *= (erase_mult + 1);
        std::cout << "│   ├── Erase Type " << i << " Time: " << (count + 1) << " x " 
                  << erase_unit_names[units] << " = " << time_ms << "ms (typical)" << std::endl;
    }
    std::cout << "│   └── (Typical times shown)" << std::endl;
    
    // DWORD 11: Chip Erase & Program Timings
    std::cout << "├── DWORD 11: Chip Erase & Program Timings" << std::endl;
    uint8_t prog_mult = table.get_dword11().get_prog_multiplier();
    uint8_t page_size_pow2 = table.get_dword11().get_page_size_pow2();
    uint32_t page_size = (1 << page_size_pow2);
    std::cout << "│   ├── Program Multiplier: " << (int)prog_mult << std::endl;
    std::cout << "│   ├── Page Size:       " << page_size << " bytes" << std::endl;
    bool unit_64us;
    uint8_t count;
    table.get_dword11().get_page_prog(count, unit_64us);
    uint32_t page_prog_time = (count + 1) * (unit_64us ? 64 : 8) * (prog_mult + 1);
    std::cout << "│   ├── Page Program:    " << (count + 1) << " x " << (unit_64us ? "64us" : "8us") 
              << " = " << page_prog_time << "us (typical)" << std::endl;
    bool unit_8us;
    table.get_dword11().get_byte_prog_first(count, unit_8us);
    uint32_t byte_prog_first = (count + 1) * (unit_8us ? 8 : 1) * (prog_mult + 1);
    std::cout << "│   ├── Byte Prog First: " << (count + 1) << " x " << (unit_8us ? "8us" : "1us") 
              << " = " << byte_prog_first << "us (typical)" << std::endl;
    table.get_dword11().get_byte_prog_add(count, unit_8us);
    uint32_t byte_prog_add = (count + 1) * (unit_8us ? 8 : 1) * (prog_mult + 1);
    std::cout << "│   ├── Byte Prog Add:   " << (count + 1) << " x " << (unit_8us ? "8us" : "1us") 
              << " = " << byte_prog_add << "us (typical)" << std::endl;
    uint8_t chip_erase_count, chip_erase_units;
    table.get_dword11().get_chip_erase_time(chip_erase_count, chip_erase_units);
    const char* chip_erase_unit_names[] = {"16ms", "256ms", "4s", "64s"};
    uint32_t chip_erase_time = (chip_erase_count + 1);
    if (chip_erase_units < 4) {
        std::cout << "│   └── Chip Erase:      " << chip_erase_time << " x " 
                  << chip_erase_unit_names[chip_erase_units] << " (typical)" << std::endl;
    } else {
        std::cout << "│   └── Chip Erase:      Not Defined" << std::endl;
    }
    
    // DWORD 12: Suspend/Resume Limits
    std::cout << "├── DWORD 12: Suspend/Resume Limits" << std::endl;
    bool suspend_supported = table.get_dword12().get_suspend_supported();
    std::cout << "│   ├── Suspend Supported: " << (suspend_supported ? "Yes" : "No") << std::endl;
    uint8_t prog_suspend_count, prog_suspend_unit;
    table.get_dword12().get_prog_suspend_max(prog_suspend_count, prog_suspend_unit);
    const char* suspend_unit_names[] = {"128ns", "256ns", "512ns", "64us"};
    if (prog_suspend_unit < 4) {
        std::cout << "│   ├── Prog Suspend Max: " << (prog_suspend_count + 1) << " x " 
                  << suspend_unit_names[prog_suspend_unit] << std::endl;
    }
    uint8_t erase_suspend_count, erase_suspend_unit;
    table.get_dword12().get_erase_suspend_max(erase_suspend_count, erase_suspend_unit);
    if (erase_suspend_unit < 4) {
        std::cout << "│   └── Erase Suspend Max: " << (erase_suspend_count + 1) << " x " 
                  << suspend_unit_names[erase_suspend_unit] << std::endl;
    }
    
    // DWORD 13: Suspend/Resume Instructions
    std::cout << "├── DWORD 13: Suspend/Resume Instructions" << std::endl;
    std::cout << "│   ├── Program Resume:   0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword13().get_prog_resume_op() << std::dec << std::endl;
    std::cout << "│   ├── Program Suspend: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword13().get_prog_suspend_op() << std::dec << std::endl;
    std::cout << "│   ├── Resume:         0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword13().get_resume_op() << std::dec << std::endl;
    std::cout << "│   └── Suspend:        0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword13().get_suspend_op() << std::dec << std::endl;
    
    // DWORD 14: Deep Powerdown & Polling
    std::cout << "├── DWORD 14: Deep Powerdown & Polling" << std::endl;
    bool dpd_supported = table.get_dword14().get_dpd_supported();
    std::cout << "│   ├── DPD Supported:   " << (dpd_supported ? "Yes" : "No") << std::endl;
    std::cout << "│   ├── Poll Status Legacy: " << (table.get_dword14().get_poll_status_legacy() ? "Yes" : "No") << std::endl;
    std::cout << "│   ├── Poll Status Flag: " << (table.get_dword14().get_poll_status_flag() ? "Yes" : "No") << std::endl;
    uint8_t exit_dpd_count, exit_dpd_unit;
    table.get_dword14().get_exit_dpd_delay(exit_dpd_count, exit_dpd_unit);
    const char* dpd_unit_names[] = {"128ns", "256ns", "512ns", "64us"};
    if (exit_dpd_unit < 4) {
        std::cout << "│   ├── Exit DPD Delay:   " << (exit_dpd_count + 1) << " x " 
                  << dpd_unit_names[exit_dpd_unit] << std::endl;
    }
    std::cout << "│   ├── Exit DPD Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword14().get_exit_dpd_op() << std::dec << std::endl;
    std::cout << "│   └── Enter DPD Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword14().get_enter_dpd_op() << std::dec << std::endl;
    
    // DWORD 15: Quad Enable & Advanced Features
    std::cout << "├── DWORD 15: Quad Enable & Advanced Features" << std::endl;
    sfdp_qer_e qer = table.get_dword15().get_quad_enable_requirement();
    std::cout << "│   ├── Quad Enable:     " << qer_to_string(qer) << std::endl;
    bool mode_044 = table.get_dword15().get_0_4_4_mode_support();
    std::cout << "│   ├── 0-4-4 Mode:      " << (mode_044 ? "Supported" : "Not Supported") << std::endl;
    std::cout << "│   ├── 4-4-4 Enable Seq: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword15().get_4_4_4_enable_sequence() << std::dec << std::endl;
    std::cout << "│   └── 4-4-4 Disable Seq: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)table.get_dword15().get_4_4_4_disable_sequence() << std::dec << std::endl;
    
    // DWORD 16: 4-Byte Addressing & Soft Reset
    std::cout << "└── DWORD 16: 4-Byte Addressing & Soft Reset" << std::endl;
    uint8_t soft_reset = table.get_dword16().get_soft_reset_support();
    std::cout << "    ├── Soft Reset:      ";
    if (soft_reset == 0) {
        std::cout << "Not Supported";
    } else {
        if (soft_reset & 0x10) std::cout << "F0h ";
        if (soft_reset & 0x08) std::cout << "66h/99h ";
    }
    std::cout << std::endl;
    uint8_t entry_method = table.get_dword16().get_4byte_addr_entry_method();
    uint16_t exit_method = table.get_dword16().get_4byte_addr_exit_method();
    std::cout << "    ├── 4-Byte Entry:    0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)entry_method << std::dec << std::endl;
    std::cout << "    └── 4-Byte Exit:     0x" << std::hex << std::setw(3) << std::setfill('0') 
              << exit_method << std::dec << std::endl;
    
    std::cout << std::endl;
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    END OF SFDP TREE                        ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

}
