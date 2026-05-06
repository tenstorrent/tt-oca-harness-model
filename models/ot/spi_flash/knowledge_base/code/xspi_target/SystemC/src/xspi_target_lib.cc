/*
 * xspi_target_lib.cc
 *
 *  Created on: Feb 3, 2026
 *      Author: ctr-sdangi
 */


 #include "xspi_target.h"
 #include <fstream>
 #include <iostream>
 #include <cstring>
 #include <iomanip>
 #include <vector>
 #include <sys/stat.h>
 #include <sys/types.h>
 
 // ============================================================================
 // XSPI TARGET MODEL IMPLEMENTATION
 // ============================================================================
 // Size matches Base: MEM_SIZE (0x2000000) / sizeof(unsigned int) words
 static constexpr size_t MEM_SIZE_WORDS = 0x2000000u / sizeof(unsigned int);
 static constexpr size_t MEM_SIZE_BYTES = MEM_SIZE_WORDS * sizeof(unsigned int);

 // Constructor: Initialize SFDP structures and device state; set_mem() must be called from xspi_target
 xspi_target_model::xspi_target_model() : m_mem(nullptr) {
     write_enabled = false;
     operation_suspended = false;
     status_register = 0x00;
     sfdp_rom.build_standard_layout();
 }
 


// Helper function to ensure data directory exists
static void ensure_data_directory() {
    struct stat info;
    if (stat("data", &info) != 0) {
        // Directory doesn't exist, create it
        mkdir("data", 0755);
    }
}

// Backdoor: Load memory from file into scml2::memory (when m_mem set)
bool xspi_target_model::load_memory_from_file() {
    if (m_mem == nullptr) return false;
    std::ifstream file(BACKDOOR_FILE_PATH, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "[Backdoor] File " << BACKDOOR_FILE_PATH << " not found." << std::endl;
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) { file.close(); return false; }
    if (static_cast<size_t>(size) > MEM_SIZE_BYTES) size = static_cast<std::streamsize>(MEM_SIZE_BYTES);
    std::vector<char> buf(static_cast<size_t>(size));
    if (!file.read(buf.data(), size)) { file.close(); return false; }
    file.close();
    for (size_t i = 0; i < static_cast<size_t>(size); ++i)
        write_byte(static_cast<uint32_t>(i), static_cast<uint8_t>(buf[i]));
    std::cout << "[Backdoor] Loaded " << size << " bytes from " << BACKDOOR_FILE_PATH << std::endl;
    return true;
}

// Backdoor: Save scml2::memory to file (when m_mem set)
bool xspi_target_model::save_memory_to_file() const {
    if (m_mem == nullptr) return false;
    ensure_data_directory();
    std::ofstream file(BACKDOOR_FILE_PATH, std::ios::binary);
    if (!file.is_open()) return false;
    std::vector<char> buf(MEM_SIZE_BYTES);
    for (size_t i = 0; i < MEM_SIZE_BYTES; ++i)
        buf[i] = static_cast<char>(read_byte(static_cast<uint32_t>(i)));
    bool ok = !!file.write(buf.data(), MEM_SIZE_BYTES);
    file.close();
    if (ok) std::cout << "[Backdoor] Saved " << MEM_SIZE_BYTES << " bytes to " << BACKDOOR_FILE_PATH << std::endl;
    return ok;
}

// Read byte from scml2::memory at address (word indexing)
uint8_t xspi_target_model::read_byte(uint32_t address) const {
    if (m_mem == nullptr) return 0xFF;
    size_t word_index = address >> 2;
    size_t byte_off = address & 3u;
    if (word_index < MEM_SIZE_WORDS) {
        unsigned int w = (*m_mem)[word_index];
        return static_cast<uint8_t>((w >> (byte_off * 8)) & 0xFFu);
    }
    return 0xFF;
}

// Write byte to scml2::memory at address (word indexing)
void xspi_target_model::write_byte(uint32_t address, uint8_t value) {
    if (m_mem == nullptr) return;
    size_t word_index = address >> 2;
    size_t byte_off = address & 3u;
    if (word_index < MEM_SIZE_WORDS) {
        unsigned int w = (*m_mem)[word_index];
        w &= ~(0xFFu << (byte_off * 8));
        w |= (static_cast<unsigned int>(value) << (byte_off * 8));
        (*m_mem)[word_index] = w;
    }
}

// ============================================================================
// HELPER FUNCTIONS FOR COMMAND PROCESSING
// ============================================================================

void xspi_target_model::handle_read_command(uint32_t address, std::vector<uint8_t>& rx_buffer) {
    std::cout << "[Target] Read @ Addr: 0x" << std::hex << address
              << ", Length: " << std::dec << rx_buffer.size() << std::endl;
    for (size_t i = 0; i < rx_buffer.size(); ++i)
        rx_buffer[i] = read_byte(static_cast<uint32_t>(address + i));
}

void xspi_target_model::handle_read_sfdp(uint32_t address, std::vector<uint8_t>& rx_buffer) {
    std::cout << "[Target] Read SFDP (0x5A) @ Addr: 0x" << std::hex << address << std::dec << std::endl;
    for (size_t i = 0; i < rx_buffer.size(); ++i) {
        rx_buffer[i] = sfdp_rom.read_byte(address + i);
    }
}

bool xspi_target_model::handle_program_command(uint32_t address, const std::vector<uint8_t>& tx_buffer, bool is_4byte) {
    
    if (!write_enabled) {
        std::cout << "[Target] Program failed: Write not enabled (WREN required)" << std::endl;
        return false;
    }

    if (operation_suspended) {
        std::cout << "[Target] Program failed: Operation suspended" << std::endl;
        return false;
    }

    std::cout << "[Target] Program " << (is_4byte ? "(4-byte)" : "(3-byte)")
              << " @ Addr: 0x" << std::hex << address
              << ", Length: " << std::dec << tx_buffer.size() << std::endl;

    // LT Model: write to scml2::memory; flash: can only change 1->0
    for (size_t i = 0; i < tx_buffer.size(); ++i) {
        uint32_t addr = static_cast<uint32_t>(address + i);
        write_byte(addr, read_byte(addr) & tx_buffer[i]);
    }

    // Clear write enable after program
    write_enabled = false;
    update_sr1_wel();  // Sync SR1 bit 1 (WEL)
    return true;
}

bool xspi_target_model::handle_erase_command(uint32_t address, bool is_4byte) {
    if (!write_enabled) {
        std::cout << "[Target] Erase failed: Write not enabled (WREN required)" << std::endl;
        return false;
    }

    if (operation_suspended) {
        std::cout << "[Target] Erase failed: Operation suspended" << std::endl;
        return false;
    }

    // Erase 64KB block (0x10000 bytes)
    const uint32_t ERASE_SIZE = 64 * 1024;
    uint32_t block_start = address & ~(ERASE_SIZE - 1); // Align to 64KB boundary

    std::cout << "[Target] Erase 64KB " << (is_4byte ? "(4-byte)" : "(3-byte)")
              << " @ Block: 0x" << std::hex << block_start << std::dec << std::endl;

    // LT Model: erase in scml2::memory (set to 0xFF)
    for (uint32_t i = 0; i < ERASE_SIZE; ++i)
        write_byte(block_start + i, 0xFF);

    // Clear write enable after erase
    write_enabled = false;
    update_sr1_wel();  // Sync SR1 bit 1 (WEL)
    return true;
}

void xspi_target_model::update_sr1_wel() {
    // Synchronize SR1 bit 1 (WEL) with write_enabled state
    if (write_enabled) {
        status_register |= (1 << 1);  // Set bit 1 (WEL)
    } else {
        status_register &= ~(1 << 1); // Clear bit 1 (WEL)
    }
}

// ============================================================================
// RESET FUNCTIONALITY
// ============================================================================

void xspi_target_model::reset() {
    // Reset device state to power-on defaults
    write_enabled = false;
    operation_suspended = false;
    status_register = 0x00; // Ready, not busy, WEL=0

    // Note: Memory and SFDP structures are NOT reset (preserves flash content)
    std::cout << "[Target] Device reset: State cleared, memory preserved" << std::endl;
}

void xspi_target_model::handle_control_command(uint8_t opcode, std::vector<uint8_t>& rx_buffer) {
    switch (opcode) {
        case xspi_opcodes::WRITE_ENABLE:
            write_enabled = true;
            update_sr1_wel();  // Sync SR1 bit 1 (WEL)
            std::cout << "[Target] Write Enable (WREN)" << std::endl;
            break;

        case xspi_opcodes::WRITE_DISABLE:
            write_enabled = false;
            update_sr1_wel();  // Sync SR1 bit 1 (WEL)
            std::cout << "[Target] Write Disable" << std::endl;
            break;

        case xspi_opcodes::READ_STATUS_REG:
            if (rx_buffer.size() > 0) {
                rx_buffer[0] = status_register;
                std::cout << "[Target] Read Status Register: 0x" << std::hex
                          << std::setw(2) << std::setfill('0') << (int)status_register << std::dec << std::endl;
            }
            break;

        default:
            std::cerr << "[Target] Unknown control opcode: 0x" << std::hex << (int)opcode << std::dec << std::endl;
            break;
    }
}

void xspi_target_model::handle_suspend_resume(uint8_t opcode) {
    switch (opcode) {
        case xspi_opcodes::SUSPEND_75:
        case xspi_opcodes::SUSPEND_B0:
            operation_suspended = true;
            std::cout << "[Target] Suspend (0x" << std::hex << (int)opcode << std::dec << ")" << std::endl;
            break;

        case xspi_opcodes::RESUME_30:
        case xspi_opcodes::RESUME_7A:
        case xspi_opcodes::RESUME_D0:
            operation_suspended = false;
            std::cout << "[Target] Resume (0x" << std::hex << (int)opcode << std::dec << ")" << std::endl;
            break;

        default:
            std::cerr << "[Target] Unknown suspend/resume opcode: 0x" << std::hex << (int)opcode << std::dec << std::endl;
            break;
    }
}

// ============================================================================
// MAIN COMMAND PROCESSOR
// ============================================================================

bool xspi_target_model::process_command(uint8_t opcode,
                                         uint32_t address,
                                         std::vector<uint8_t>& rx_buffer,
                                         const std::vector<uint8_t>& tx_buffer) {
    switch (opcode) {
        // Read Commands (grouped) - always succeed
        case xspi_opcodes::READ_ZERO_LATENCY:
        case xspi_opcodes::READ_FAST:
        case xspi_opcodes::READ_FAST_ALT:
            handle_read_command(address, rx_buffer);
            return true;

        case xspi_opcodes::READ_SFDP:
            handle_read_sfdp(address, rx_buffer);
            return true;

        // Program Commands (grouped) - return handler result
        case xspi_opcodes::PROGRAM:
            return handle_program_command(address, tx_buffer, false);
        case xspi_opcodes::PROGRAM_4BYTE:
            return handle_program_command(address, tx_buffer, true);

        // Erase Commands (grouped) - return handler result
        case xspi_opcodes::ERASE_64KB:
            return handle_erase_command(address, false);
        case xspi_opcodes::ERASE_64KB_4BYTE:
            return handle_erase_command(address, true);

        // Control Commands - always succeed
        case xspi_opcodes::WRITE_ENABLE:
        case xspi_opcodes::WRITE_DISABLE:
        case xspi_opcodes::READ_STATUS_REG:
            handle_control_command(opcode, rx_buffer);
            return true;

        // Suspend/Resume Commands (grouped) - always succeed
        case xspi_opcodes::SUSPEND_75:
        case xspi_opcodes::SUSPEND_B0:
        case xspi_opcodes::RESUME_30:
        case xspi_opcodes::RESUME_7A:
        case xspi_opcodes::RESUME_D0:
            handle_suspend_resume(opcode);
            return true;

        default:
            std::cerr << "[Target] Unknown opcode: 0x" << std::hex << std::setw(2) << std::setfill('0')
                      << (int)opcode << std::dec << std::endl;
            return false;
    }
}

// Update SFDP ROM with current structures
void xspi_target_model::update_sfdp_rom() {
    // Rebuild the SFDP layout with current structure values
    sfdp_rom.load_structure(0x00, sfdp_header.to_bytes());

    parameter_header.set_pointer(0x80);
    sfdp_rom.load_structure(0x08, parameter_header.to_bytes());

    sfdp_rom.load_structure(0x80, basic_table.to_bytes());
}
