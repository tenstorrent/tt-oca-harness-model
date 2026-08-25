// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file spi_flash_sfdp_utils.cpp
 * @brief SFDP parse and print utility implementations
 *
 */

#include "spi_flash_sfdp_utils.h"

#include <iostream>
#include <iomanip>
#include <fstream>

// ============================================================================
// STRING CONVERSION
// ============================================================================

std::string addr_mode_to_string(uint8_t mode)
{
    switch (mode) {
        case ADDR_3_BYTE_ONLY: return "3-Byte Only";
        case ADDR_3_OR_4_BYTE: return "3-Byte or 4-Byte";
        case ADDR_4_BYTE_ONLY: return "4-Byte Only";
        default:               return "Unknown";
    }
}

std::string qer_to_string(uint8_t qer)
{
    switch (qer) {
        case QER_NONE_OR_HOLD:    return "None or HOLD";
        case QER_BIT1_SR2_REG:    return "Bit 1 of SR2";
        case QER_BIT6_SR1_REG:    return "Bit 6 of SR1";
        case QER_BIT7_SR2_OP3E:   return "Bit 7 of SR2 (via 3Eh)";
        case QER_BIT1_SR2_NO_CLR: return "Bit 1 of SR2 (no clear)";
        case QER_BIT1_SR2_OP35:   return "Bit 1 of SR2 (via 35h)";
        default:                  return "Unknown";
    }
}

// ============================================================================
// PARSER HELPERS (file-local)
// ============================================================================

uint32_t read_le32(const std::vector<uint8_t>& buf, size_t offset)
{
    if (offset + 4 > buf.size()) return 0xFFFFFFFFu;
    return (static_cast<uint32_t>(buf[offset + 0]) <<  0) |
           (static_cast<uint32_t>(buf[offset + 1]) <<  8) |
           (static_cast<uint32_t>(buf[offset + 2]) << 16) |
           (static_cast<uint32_t>(buf[offset + 3]) << 24);
}

static bool parse_sfdp_header(const std::vector<uint8_t>& data,
                               size_t offset, sfdp_header_t& hdr)
{
    if (offset + 8 > data.size()) return false;
    hdr.signature = read_le32(data, offset);
    hdr.minor_rev = data[offset + 4];
    hdr.major_rev = data[offset + 5];
    hdr.nph       = data[offset + 6];
    hdr.unused    = data[offset + 7];
    return hdr.signature == SFDP_SIGNATURE;
}

static bool parse_parameter_header(const std::vector<uint8_t>& data,
                                    size_t offset, sfdp_parameter_header_t& ph)
{
    if (offset + 8 > data.size()) return false;
    ph.id_lsb        = data[offset + 0];
    ph.minor_rev     = data[offset + 1];
    ph.major_rev     = data[offset + 2];
    ph.length_dwords = data[offset + 3];
    ph.ptp = (static_cast<uint32_t>(data[offset + 4]) <<  0) |
             (static_cast<uint32_t>(data[offset + 5]) <<  8) |
             (static_cast<uint32_t>(data[offset + 6]) << 16);
    return true;
}

static bool parse_basic_table(const std::vector<uint8_t>& data,
                               size_t offset, jedec_basic_table_t& tbl)
{
    if (offset + 64 > data.size()) return false;
    tbl.get_dword1().from_dword(read_le32(data, offset +  0));
    tbl.get_dword2().from_dword(read_le32(data, offset +  4));
    tbl.get_dword3().from_dword(read_le32(data, offset +  8));
    tbl.get_dword4().from_dword(read_le32(data, offset + 12));
    tbl.get_dword5().from_dword(read_le32(data, offset + 16));
    tbl.get_dword6().from_dword(read_le32(data, offset + 20));
    tbl.get_dword7().from_dword(read_le32(data, offset + 24));
    tbl.get_dword8().from_dword(read_le32(data, offset + 28));
    tbl.get_dword9().from_dword(read_le32(data, offset + 32));
    tbl.get_dword10().from_dword(read_le32(data, offset + 36));
    tbl.get_dword11().from_dword(read_le32(data, offset + 40));
    tbl.get_dword12().from_dword(read_le32(data, offset + 44));
    tbl.get_dword13().from_dword(read_le32(data, offset + 48));
    tbl.get_dword14().from_dword(read_le32(data, offset + 52));
    tbl.get_dword15().from_dword(read_le32(data, offset + 56));
    tbl.get_dword16().from_dword(read_le32(data, offset + 60));
    return true;
}

// ============================================================================
// PUBLIC PARSER FUNCTIONS
// ============================================================================

bool parse_sfdp_from_bytes(const std::vector<uint8_t>& data,
                           sfdp_header_t&           header,
                           sfdp_parameter_header_t& p_header,
                           jedec_basic_table_t&     table)
{
    if (!parse_sfdp_header(data, 0x00, header))     return false;
    if (!parse_parameter_header(data, 0x08, p_header)) return false;
    return parse_basic_table(data, p_header.ptp, table);
}

bool parse_sfdp_from_file(const std::string&        filename,
                          sfdp_header_t&            header,
                          sfdp_parameter_header_t&  p_header,
                          jedec_basic_table_t&      table)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize sz = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    if (!file.read(reinterpret_cast<char*>(data.data()), sz)) return false;
    file.close();
    return parse_sfdp_from_bytes(data, header, p_header, table);
}

// ============================================================================
// PRETTY-PRINT
// ============================================================================

void print_sfdp_tree(const sfdp_header_t&           header,
                     const sfdp_parameter_header_t& p_header,
                     const jedec_basic_table_t&     table)
{
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    SFDP STRUCTURE TREE                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // ---- SFDP Header --------------------------------------------------------
    std::cout << "SFDP Header (Address: 0x00)\n";
    std::cout << "├── Signature:           0x"
              << std::hex << std::setw(8) << std::setfill('0') << header.signature
              << " (\"SFDP\")\n" << std::dec;
    std::cout << "├── Major Revision:      " << static_cast<int>(header.major_rev) << "\n";
    std::cout << "├── Minor Revision:      " << static_cast<int>(header.minor_rev) << "\n";
    std::cout << "├── NPH:                 " << static_cast<int>(header.nph)
              << " (" << (static_cast<int>(header.nph) + 1) << " parameter headers)\n";
    std::cout << "└── Unused:              0x"
              << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(header.unused) << "\n\n" << std::dec;

    // ---- Parameter Header ---------------------------------------------------
    std::cout << "Parameter Header (Address: 0x08)\n";
    std::cout << "├── Parameter ID LSB:    0x"
              << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(p_header.id_lsb) << std::dec << " (JEDEC Basic Table)\n";
    std::cout << "├── Parameter ID MSB:    0xFF (JEDEC)\n";
    std::cout << "├── Minor Revision:      " << static_cast<int>(p_header.minor_rev) << "\n";
    std::cout << "├── Major Revision:      " << static_cast<int>(p_header.major_rev) << "\n";
    std::cout << "├── Length (DWORDs):     " << static_cast<int>(p_header.length_dwords) << "\n";
    std::cout << "└── Table Pointer:       0x"
              << std::hex << std::setw(6) << std::setfill('0') << p_header.ptp << "\n\n" << std::dec;

    // ---- Basic Flash Parameter Table ----------------------------------------
    std::cout << "Basic Flash Parameter Table (Address: 0x"
              << std::hex << p_header.ptp << std::dec << ")\n";

    // DWORD 1
    uint64_t density = table.get_density();
    std::cout << "├── DWORD 1: Architecture & Fast Read Support\n";
    std::cout << "│   ├── Density:            " << density << " bits ("
              << (density / 1024 / 1024) << " Mbits, "
              << (density / 8 / 1024 / 1024) << " MB)\n";
    std::cout << "│   ├── Address Mode:        " << addr_mode_to_string(table.get_address_bytes()) << "\n";
    std::cout << "│   ├── Erase Size Support:  " << static_cast<int>(table.get_dword1().get_erase_size()) << "\n";
    std::cout << "│   ├── Write Granularity:   "
              << (table.get_dword1().get_write_granularity() ? "64 bytes" : "1 byte") << "\n";
    std::cout << "│   ├── Volatile BP:         "
              << (table.get_dword1().get_volatile_status_register() ? "Yes" : "No") << "\n";
    std::cout << "│   ├── Write Enable Opcode: "
              << (table.get_dword1().get_write_enable_opcode_select() ? "06h" : "50h") << "\n";
    std::cout << "│   ├── 4KB Erase Opcode:    0x"
              << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword1().get_erase_4kb_instruction()) << std::dec << "\n";
    std::cout << "│   ├── DTR Support:         "
              << (table.get_dword1().get_dtr_clocking_support() ? "Yes" : "No") << "\n";
    bool f114 = table.get_dword1().get_fast_read_1_1_4_support();
    std::cout << "│   ├── Fast Read 1-1-4:     " << (f114 ? "Supported" : "Not Supported");
    if (f114) std::cout << " (Opcode: 0x" << std::hex << static_cast<int>(table.get_read_opcode_1_1_4()) << std::dec << ")";
    std::cout << "\n";
    std::cout << "│   ├── Fast Read 1-4-4:     "
              << (table.get_dword1().get_fast_read_1_4_4_support() ? "Supported" : "Not Supported") << "\n";
    std::cout << "│   ├── Fast Read 1-2-2:     "
              << (table.get_dword1().get_fast_read_1_2_2_support() ? "Supported" : "Not Supported") << "\n";
    std::cout << "│   └── Fast Read 1-1-2:     "
              << (table.get_dword1().get_fast_read_1_1_2_support() ? "Supported" : "Not Supported") << "\n";

    // DWORD 2
    std::cout << "├── DWORD 2: Flash Memory Density\n";
    std::cout << "│   └── Density:            " << table.get_dword2().get_density()
              << " bits (" << (table.get_dword2().get_density() / 1024 / 1024) << " Mbits)\n";

    // DWORD 3
    std::cout << "├── DWORD 3: (1-4-4) and (1-1-4) Fast Read Parameters\n";
    std::cout << "│   ├── (1-4-4) Wait States: " << static_cast<int>(table.get_dword3().get_1_4_4_wait_states()) << "\n";
    std::cout << "│   ├── (1-4-4) Mode Clocks: " << static_cast<int>(table.get_dword3().get_1_4_4_mode_clocks()) << "\n";
    std::cout << "│   ├── (1-4-4) Opcode:      0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword3().get_1_4_4_opcode()) << std::dec << "\n";
    std::cout << "│   ├── (1-1-4) Wait States: " << static_cast<int>(table.get_dword3().get_1_1_4_wait_states()) << "\n";
    std::cout << "│   ├── (1-1-4) Mode Clocks: " << static_cast<int>(table.get_dword3().get_1_1_4_mode_clocks()) << "\n";
    std::cout << "│   └── (1-1-4) Opcode:      0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword3().get_1_1_4_opcode()) << std::dec << "\n";

    // DWORD 4
    std::cout << "├── DWORD 4: (1-1-2) and (1-2-2) Fast Read Parameters\n";
    std::cout << "│   ├── (1-1-2) Wait States: " << static_cast<int>(table.get_dword4().get_1_1_2_wait_states()) << "\n";
    std::cout << "│   ├── (1-1-2) Mode Clocks: " << static_cast<int>(table.get_dword4().get_1_1_2_mode_clocks()) << "\n";
    std::cout << "│   ├── (1-1-2) Opcode:      0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword4().get_1_1_2_opcode()) << std::dec << "\n";
    std::cout << "│   ├── (1-2-2) Wait States: " << static_cast<int>(table.get_dword4().get_1_2_2_wait_states()) << "\n";
    std::cout << "│   ├── (1-2-2) Mode Clocks: " << static_cast<int>(table.get_dword4().get_1_2_2_mode_clocks()) << "\n";
    std::cout << "│   └── (1-2-2) Opcode:      0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword4().get_1_2_2_opcode()) << std::dec << "\n";

    // DWORD 5
    std::cout << "├── DWORD 5: (2-2-2) and (4-4-4) Support Flags\n";
    std::cout << "│   ├── (2-2-2) Support:     "
              << (table.get_dword5().get_2_2_2_support() ? "Yes" : "No") << "\n";
    std::cout << "│   └── (4-4-4) Support:     "
              << (table.get_dword5().get_4_4_4_support() ? "Yes" : "No") << "\n";

    // DWORD 6
    std::cout << "├── DWORD 6: (2-2-2) Fast Read Parameters\n";
    std::cout << "│   ├── (2-2-2) Wait States: " << static_cast<int>(table.get_dword6().get_2_2_2_wait_states()) << "\n";
    std::cout << "│   ├── (2-2-2) Mode Clocks: " << static_cast<int>(table.get_dword6().get_2_2_2_mode_clocks()) << "\n";
    std::cout << "│   └── (2-2-2) Opcode:      0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword6().get_2_2_2_opcode()) << std::dec << "\n";

    // DWORD 7
    std::cout << "├── DWORD 7: (4-4-4) Fast Read Parameters\n";
    std::cout << "│   ├── (4-4-4) Wait States: " << static_cast<int>(table.get_dword7().get_4_4_4_wait_states()) << "\n";
    std::cout << "│   ├── (4-4-4) Mode Clocks: " << static_cast<int>(table.get_dword7().get_4_4_4_mode_clocks()) << "\n";
    std::cout << "│   └── (4-4-4) Opcode:      0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword7().get_4_4_4_opcode()) << std::dec << "\n";

    // DWORD 8
    std::cout << "├── DWORD 8: Erase Types 1 & 2\n";
    {
        uint8_t sz, op;
        table.get_sector_erase_type1(sz, op);
        uint32_t bytes = (sz < 32u) ? (1u << sz) : 0u;
        std::cout << "│   ├── Erase Type 1:    " << bytes << " bytes ("
                  << (bytes / 1024) << " KB) - Opcode: 0x"
                  << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(op) << std::dec << "\n";
        uint8_t sz2 = table.get_dword8().get_erase_type2_size();
        uint8_t op2 = table.get_dword8().get_erase_type2_opcode();
        if (sz2 > 0 && sz2 < 32) {
            uint32_t bytes2 = 1u << sz2;
            std::cout << "│   └── Erase Type 2:    " << bytes2 << " bytes ("
                      << (bytes2 / 1024) << " KB) - Opcode: 0x"
                      << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(op2) << std::dec << "\n";
        } else {
            std::cout << "│   └── Erase Type 2:    Not Defined\n";
        }
    }

    // DWORD 9
    std::cout << "├── DWORD 9: Erase Types 3 & 4\n";
    {
        auto print_erase_type = [&](const char* last, uint8_t sz, uint8_t op) {
            if (sz > 0 && sz < 32) {
                uint32_t bytes = 1u << sz;
                std::cout << last << bytes << " bytes (" << (bytes / 1024) << " KB) - Opcode: 0x"
                          << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(op) << std::dec << "\n";
            } else {
                std::cout << last << "Not Defined\n";
            }
        };
        print_erase_type("│   ├── Erase Type 3:    ",
                         table.get_dword9().get_type3_size_pow2(),
                         table.get_dword9().get_type3_opcode());
        print_erase_type("│   └── Erase Type 4:    ",
                         table.get_dword9().get_type4_size_pow2(),
                         table.get_dword9().get_type4_opcode());
    }

    // DWORD 10
    std::cout << "├── DWORD 10: Erase Timings\n";
    {
        const char* u[] = {"1ms", "16ms", "128ms", "1s"};
        uint8_t mult = table.get_dword10().get_multiplier();
        std::cout << "│   ├── Multiplier:       " << static_cast<int>(mult) << "\n";
        for (int i = 1; i <= 4; ++i) {
            uint8_t cnt, unit;
            table.get_dword10().get_erase_timing(i, cnt, unit);
            std::cout << "│   ├── Erase Type " << i << " Time: ("
                      << static_cast<int>(cnt + 1) << " x " << u[unit & 3] << ")\n";
        }
        std::cout << "│   └── (typical values)\n";
    }

    // DWORD 11
    std::cout << "├── DWORD 11: Program Timings & Page Size\n";
    {
        uint8_t page_n = table.get_dword11().get_page_size_pow2();
        std::cout << "│   ├── Page Size:        " << (1u << page_n) << " bytes\n";
        uint8_t cnt; bool u64;
        table.get_dword11().get_page_prog(cnt, u64);
        std::cout << "│   ├── Page Program:     (" << static_cast<int>(cnt + 1) << " x "
                  << (u64 ? "64us" : "8us") << ")\n";
        uint8_t ce_cnt, ce_unit;
        table.get_dword11().get_chip_erase_time(ce_cnt, ce_unit);
        const char* cu[] = {"16ms", "256ms", "4s", "64s"};
        std::cout << "│   └── Chip Erase:       (" << static_cast<int>(ce_cnt + 1)
                  << " x " << cu[ce_unit & 3] << ")\n";
    }

    // DWORD 12
    std::cout << "├── DWORD 12: Suspend/Resume Limits\n";
    std::cout << "│   └── Suspend Supported:  "
              << (table.get_dword12().get_suspend_supported() ? "Yes" : "No") << "\n";

    // DWORD 13
    std::cout << "├── DWORD 13: Suspend/Resume Instructions\n";
    std::cout << "│   ├── Suspend:            0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword13().get_suspend_op()) << std::dec << "\n";
    std::cout << "│   └── Resume:             0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword13().get_resume_op()) << std::dec << "\n";

    // DWORD 14
    std::cout << "├── DWORD 14: Deep Powerdown\n";
    std::cout << "│   ├── DPD Supported:      "
              << (table.get_dword14().get_dpd_supported() ? "Yes" : "No") << "\n";
    std::cout << "│   ├── Enter DPD Opcode:   0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword14().get_enter_dpd_op()) << std::dec << "\n";
    std::cout << "│   └── Exit DPD Opcode:    0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword14().get_exit_dpd_op()) << std::dec << "\n";

    // DWORD 15
    std::cout << "├── DWORD 15: Quad Enable & Advanced Features\n";
    std::cout << "│   ├── Quad Enable:        " << qer_to_string((table.get_dword15().value >> 20) & 0x7u) << "\n";
    std::cout << "│   ├── 0-4-4 Mode:         "
              << (table.get_dword15().get_0_4_4_mode_support() ? "Supported" : "Not Supported") << "\n";
    std::cout << "│   ├── 4-4-4 Enable Seq:   0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword15().get_4_4_4_enable_sequence()) << std::dec << "\n";
    std::cout << "│   └── 4-4-4 Disable Seq:  0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword15().get_4_4_4_disable_sequence()) << std::dec << "\n";

    // DWORD 16
    std::cout << "└── DWORD 16: 4-Byte Addressing & Soft Reset\n";
    {
        uint8_t sr = table.get_dword16().get_soft_reset_support();
        std::cout << "    ├── Soft Reset:         ";
        if (sr == 0)             std::cout << "Not Supported";
        else {
            if (sr & 0x10) std::cout << "F0h ";
            if (sr & 0x08) std::cout << "66h/99h ";
        }
        std::cout << "\n";
    }
    std::cout << "    ├── 4-Byte Entry:       0x"
              << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(table.get_dword16().get_4byte_addr_entry_method()) << std::dec << "\n";
    std::cout << "    └── 4-Byte Exit:        0x"
              << std::hex << std::setw(3) << std::setfill('0')
              << static_cast<int>(table.get_dword16().get_4byte_addr_exit_method()) << std::dec << "\n";

    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout <<   "║                     END OF SFDP TREE                      ║\n";
    std::cout <<   "╚════════════════════════════════════════════════════════════╝\n\n";
}
