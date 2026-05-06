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

// Constructor: Initialize all SFDP structures and memory
xspi_target_model::xspi_target_model() {
    // Initialize memory with 0xFF (default flash memory value)
    // Default size: 16 MB
    memory.resize(16 * 1024 * 1024, 0xFF);
    
    // Initialize device state
    write_enabled = false;
    operation_suspended = false;
    status_register = 0x00; // Ready, not busy, WEL=0
    // Note: SR1 bit 1 (WEL) is already 0, matching write_enabled=false
    
    // Initialize SFDP structures with defaults
    // SFDP Header is already initialized in its constructor
    // Parameter Header is already initialized in its constructor
    // Basic Table is already initialized in its constructor
    
    // Build standard SFDP layout in ROM
    sfdp_rom.build_standard_layout();
    
    // Backdoor access: Load memory from file if it exists
    load_memory_from_file();
}

// Helper function to ensure data directory exists
static void ensure_data_directory() {
    struct stat info;
    if (stat("data", &info) != 0) {
        // Directory doesn't exist, create it
        mkdir("data", 0755);
    }
}

// Backdoor: Load memory from file
bool xspi_target_model::load_memory_from_file() {
    std::ifstream file(BACKDOOR_FILE_PATH, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        // File doesn't exist - this is OK, use default 0xFF values
        std::cout << "[Backdoor] File " << BACKDOOR_FILE_PATH 
                  << " not found. Using default 0xFF memory initialization." << std::endl;
        return false;
    }
    
    // Get file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size <= 0) {
        std::cerr << "[Backdoor] Error: File " << BACKDOOR_FILE_PATH << " is empty" << std::endl;
        file.close();
        return false;
    }
    
    // Resize memory if file is larger than current memory size
    if (static_cast<size_t>(size) > memory.size()) {
        std::cout << "[Backdoor] Resizing memory from " << memory.size() 
                  << " to " << size << " bytes" << std::endl;
        memory.resize(size, 0xFF);
    }
    
    // Read file content into memory
    if (!file.read(reinterpret_cast<char*>(memory.data()), size)) {
        std::cerr << "[Backdoor] Error: Failed to read from file " << BACKDOOR_FILE_PATH << std::endl;
        file.close();
        return false;
    }
    
    file.close();
    
    std::cout << "[Backdoor] Successfully loaded " << size << " bytes from " 
              << BACKDOOR_FILE_PATH << std::endl;
    return true;
}

// Backdoor: Save memory to file
bool xspi_target_model::save_memory_to_file() const {
    // Ensure data directory exists before writing
    ensure_data_directory();
    
    std::ofstream file(BACKDOOR_FILE_PATH, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Backdoor] Error: Failed to open file " << BACKDOOR_FILE_PATH 
                  << " for writing" << std::endl;
        return false;
    }
    
    if (!file.write(reinterpret_cast<const char*>(memory.data()), memory.size())) {
        std::cerr << "[Backdoor] Error: Failed to write to file " << BACKDOOR_FILE_PATH << std::endl;
        file.close();
        return false;
    }
    
    file.close();
    
    std::cout << "[Backdoor] Successfully saved " << memory.size() << " bytes to " 
              << BACKDOOR_FILE_PATH << std::endl;
    return true;
}

// Read byte from memory at address
uint8_t xspi_target_model::read_byte(uint32_t address) const {
    if (address < memory.size()) {
        return memory[address];
    }
    return 0xFF; // Out of bounds returns 0xFF (default flash value)
}

// Write byte to memory at address
void xspi_target_model::write_byte(uint32_t address, uint8_t value) {
    if (address < memory.size()) {
        memory[address] = value;
    }
    // Silently ignore out-of-bounds writes
}

// ============================================================================
// HELPER FUNCTIONS FOR COMMAND PROCESSING
// ============================================================================

void xspi_target_model::handle_read_command(uint32_t address, std::vector<uint8_t>& rx_buffer) {
    std::cout << "[Target] Read @ Addr: 0x" << std::hex << address 
              << ", Length: " << std::dec << rx_buffer.size() << std::endl;
    
    for (size_t i = 0; i < rx_buffer.size(); ++i) {
        uint32_t addr = address + i;
        if (addr < memory.size()) {
            rx_buffer[i] = memory[addr];
        } else {
            rx_buffer[i] = 0xFF; // Out of bounds returns 0xFF
        }
    }
}

void xspi_target_model::handle_read_sfdp(uint32_t address, std::vector<uint8_t>& rx_buffer) {
    std::cout << "[Target] Read SFDP (0x5A) @ Addr: 0x" << std::hex << address << std::dec << std::endl;
    for (size_t i = 0; i < rx_buffer.size(); ++i) {
        rx_buffer[i] = sfdp_rom.read_byte(address + i);
    }
}

    void xspi_target_model::handle_program_command(uint32_t address, const std::vector<uint8_t>& tx_buffer, bool is_4byte) {
    if (!write_enabled) {
        std::cout << "[Target] Program failed: Write not enabled (WREN required)" << std::endl;
        return;
    }
    
    if (operation_suspended) {
        std::cout << "[Target] Program failed: Operation suspended" << std::endl;
        return;
    }
    
    std::cout << "[Target] Program " << (is_4byte ? "(4-byte)" : "(3-byte)") 
              << " @ Addr: 0x" << std::hex << address 
              << ", Length: " << std::dec << tx_buffer.size() << std::endl;
    
    // LT Model: Directly write to memory (no timing simulation)
    for (size_t i = 0; i < tx_buffer.size(); ++i) {
        uint32_t addr = address + i;
        if (addr < memory.size()) {
            // Flash memory: can only change 1->0, not 0->1
            memory[addr] &= tx_buffer[i];
        }
    }
    
    // Clear write enable after program
    write_enabled = false;
    update_sr1_wel();  // Sync SR1 bit 1 (WEL)
}

void xspi_target_model::handle_erase_command(uint32_t address, bool is_4byte) {
    if (!write_enabled) {
        std::cout << "[Target] Erase failed: Write not enabled (WREN required)" << std::endl;
        return;
    }
    
    if (operation_suspended) {
        std::cout << "[Target] Erase failed: Operation suspended" << std::endl;
        return;
    }
    
    // Erase 64KB block (0x10000 bytes)
    const uint32_t ERASE_SIZE = 64 * 1024;
    uint32_t block_start = address & ~(ERASE_SIZE - 1); // Align to 64KB boundary
    
    std::cout << "[Target] Erase 64KB " << (is_4byte ? "(4-byte)" : "(3-byte)") 
              << " @ Block: 0x" << std::hex << block_start << std::dec << std::endl;
    
    // LT Model: Directly erase memory (set to 0xFF)
    for (uint32_t i = 0; i < ERASE_SIZE; ++i) {
        uint32_t addr = block_start + i;
        if (addr < memory.size()) {
            memory[addr] = 0xFF;
        }
    }
    
    // Clear write enable after erase
    write_enabled = false;
    update_sr1_wel();  // Sync SR1 bit 1 (WEL)
}

void xspi_target_model::update_sr1_wel() {
    // Synchronize SR1 bit 1 (WEL) with write_enabled state
    if (write_enabled) {
        status_register |= (1 << 1);  // Set bit 1 (WEL)
    } else {
        status_register &= ~(1 << 1); // Clear bit 1 (WEL)
    }
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

void xspi_target_model::process_command(uint8_t opcode, 
                                        uint32_t address, 
                                        std::vector<uint8_t>& rx_buffer,
                                        const std::vector<uint8_t>& tx_buffer) {
    switch (opcode) {
        // Read Commands (grouped)
        case xspi_opcodes::READ_ZERO_LATENCY:
        case xspi_opcodes::READ_FAST:
        case xspi_opcodes::READ_FAST_ALT:
            handle_read_command(address, rx_buffer);
            break;
            
        case xspi_opcodes::READ_SFDP:
            handle_read_sfdp(address, rx_buffer);
            break;
            
        // Program Commands (grouped)
        case xspi_opcodes::PROGRAM:
            handle_program_command(address, tx_buffer, false);
            break;
        case xspi_opcodes::PROGRAM_4BYTE:
            handle_program_command(address, tx_buffer, true);
            break;
            
        // Erase Commands (grouped)
        case xspi_opcodes::ERASE_64KB:
            handle_erase_command(address, false);
            break;
        case xspi_opcodes::ERASE_64KB_4BYTE:
            handle_erase_command(address, true);
            break;
            
        // Control Commands
        case xspi_opcodes::WRITE_ENABLE:
        case xspi_opcodes::WRITE_DISABLE:
        case xspi_opcodes::READ_STATUS_REG:
            handle_control_command(opcode, rx_buffer);
            break;
            
        // Suspend/Resume Commands (grouped)
        case xspi_opcodes::SUSPEND_75:
        case xspi_opcodes::SUSPEND_B0:
        case xspi_opcodes::RESUME_30:
        case xspi_opcodes::RESUME_7A:
        case xspi_opcodes::RESUME_D0:
            handle_suspend_resume(opcode);
            break;
            
        default:
            std::cerr << "[Target] Unknown opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)opcode << std::dec << std::endl;
            break;
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


// ============================================================================
// MAIN FUNCTION
// ============================================================================
int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << " XSPI Target Model Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Create XSPI target model instance
    xspi_target_model target;
    
    // Print complete SFDP structure tree (using SFDP utility functions)
    print_sfdp_tree(*target.get_sfdp_header(), 
                    *target.get_parameter_header(),
                    *target.get_basic_table());
    
    // Test 1: Access SFDP structures
    std::cout << "[Test 1] Accessing SFDP Structures" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    sfdp_header_t* header = target.get_sfdp_header();
    std::cout << "  SFDP Signature: 0x" << std::hex << header->signature << std::dec << std::endl;
    std::cout << "  Major Rev: " << (int)header->major_rev << std::endl;
    std::cout << "  Minor Rev: " << (int)header->minor_rev << std::endl;
    std::cout << "  NPH: " << (int)header->nph << std::endl;
    
    // Test 2: Access Basic Table
    std::cout << "\n[Test 2] Accessing Basic Flash Parameter Table" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    jedec_basic_table_t* table = target.get_basic_table();
    uint64_t density = table->get_density();
    std::cout << "  Density: " << density << " bits (" 
              << (density / 1024 / 1024) << " Mbits)" << std::endl;
    
    // Test 3: Modify and update
    std::cout << "\n[Test 3] Modifying SFDP Parameters" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    uint64_t new_density = 128 * 1024 * 1024; // 128 Mbit
    table->set_density(new_density);
    std::cout << "  Set density to: " << new_density << " bits" << std::endl;
    
    // Update SFDP ROM with new values
    target.update_sfdp_rom();
    std::cout << "  Updated SFDP ROM with new values" << std::endl;
    
    // Test 4: Read from SFDP ROM
    std::cout << "\n[Test 4] Reading from SFDP ROM" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Read SFDP Header (8 bytes at 0x00)
    std::vector<uint8_t> header_buffer(8);
    target.process_command(0x5A, 0x00, header_buffer);
    
    std::cout << "  SFDP Header bytes (0x00): ";
    for (size_t i = 0; i < header_buffer.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)header_buffer[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Read DWORD 2 (density) from basic table at offset 0x84 (0x80 + 4 bytes for DWORD 2)
    std::vector<uint8_t> density_buffer(4);
    target.process_command(0x5A, 0x84, density_buffer);
    
    std::cout << "  DWORD 2 bytes (0x84): ";
    for (size_t i = 0; i < density_buffer.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)density_buffer[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Parse density from little-endian bytes
    uint32_t dword2_value = (static_cast<uint32_t>(density_buffer[0]) << 0) |
                            (static_cast<uint32_t>(density_buffer[1]) << 8) |
                            (static_cast<uint32_t>(density_buffer[2]) << 16) |
                            (static_cast<uint32_t>(density_buffer[3]) << 24);
    
    // Decode density using same logic as dword_2_t::get_density()
    uint64_t read_density;
    if (dword2_value & (1<<31)) {
        // >= 4Gb (Bit 31=1)
        read_density = 1ULL << (dword2_value & 0x7FFFFFFF);
    } else {
        // < 2Gb (Bit 31=0)
        read_density = static_cast<uint64_t>(dword2_value) + 1;
    }
    
    std::cout << "  Read Density:         " << read_density << " bits (" 
              << (read_density / 1024 / 1024) << " Mbits)" << std::endl;
    std::cout << "  Expected Density:     " << new_density << " bits (" 
              << (new_density / 1024 / 1024) << " Mbits)" << std::endl;
    
    if (read_density == new_density) {
        std::cout << "  -> PASS: Density matches!" << std::endl;
    } else {
        std::cout << "  -> FAIL: Density mismatch!" << std::endl;
    }
    
    // Test 5: Memory access
    std::cout << "\n[Test 5] Memory Access" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    uint8_t byte_at_0 = target.read_byte(0);
    std::cout << "  Memory[0]: 0x" << std::hex << (int)byte_at_0 << std::dec << std::endl;
    
    target.write_byte(0x1000, 0xAA);
    uint8_t byte_at_1000 = target.read_byte(0x1000);
    std::cout << "  Wrote 0xAA to memory[0x1000]" << std::endl;
    std::cout << "  Memory[0x1000]: 0x" << std::hex << (int)byte_at_1000 << std::dec << std::endl;
    
    // Test 6: Backdoor file operations
    std::cout << "\n[Test 6] Backdoor File Operations" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    bool saved = target.save_memory_to_file();
    if (saved) {
        std::cout << "  Memory saved to flash_memory.bin" << std::endl;
    } else {
        std::cout << "  Failed to save memory to file" << std::endl;
    }
    
    // Test 7: Profile 1 Read Commands
    std::cout << "\n[Test 7] Profile 1 Read Commands" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Test Read Zero Latency (0x03)
    std::vector<uint8_t> read_buffer(16);
    target.process_command(xspi_opcodes::READ_ZERO_LATENCY, 0x1000, read_buffer);
    std::cout << "  Read Zero Latency (0x03) @ 0x1000: ";
    for (size_t i = 0; i < 4; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)read_buffer[i] << " ";
    }
    std::cout << std::dec << "..." << std::endl;
    
    // Test Read Fast (0x0B)
    target.process_command(xspi_opcodes::READ_FAST, 0x2000, read_buffer);
    std::cout << "  Read Fast (0x0B) @ 0x2000: ";
    for (size_t i = 0; i < 4; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)read_buffer[i] << " ";
    }
    std::cout << std::dec << "..." << std::endl;
    
    // Test Read Fast Alt (0xEE)
    target.process_command(xspi_opcodes::READ_FAST_ALT, 0x3000, read_buffer);
    std::cout << "  Read Fast Alt (0xEE) @ 0x3000: ";
    for (size_t i = 0; i < 4; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)read_buffer[i] << " ";
    }
    std::cout << std::dec << "..." << std::endl;
    
    // Test 8: Profile 1 Control Commands
    std::cout << "\n[Test 8] Profile 1 Control Commands" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Test Write Enable (0x06)
    std::vector<uint8_t> status_buffer(1);
    target.process_command(xspi_opcodes::WRITE_ENABLE, 0, status_buffer);
    
    // Test Read Status Register (0x05)
    target.process_command(xspi_opcodes::READ_STATUS_REG, 0, status_buffer);
    std::cout << "  Status Register: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)status_buffer[0] << std::dec << std::endl;
    
    // Test Write Disable (0x04)
    target.process_command(xspi_opcodes::WRITE_DISABLE, 0, status_buffer);
    
    // Test 9: Profile 1 Program Commands
    std::cout << "\n[Test 9] Profile 1 Program Commands" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Enable write first
    target.process_command(xspi_opcodes::WRITE_ENABLE, 0, status_buffer);
    
    // Test Program (0x02) - 3-byte address
    std::vector<uint8_t> program_data = {0xAA, 0xBB, 0xCC, 0xDD};
    target.process_command(xspi_opcodes::PROGRAM, 0x5000, status_buffer, program_data);
    std::cout << "  Programmed 4 bytes @ 0x5000 using opcode 0x02" << std::endl;
    
    // Verify by reading back
    target.process_command(xspi_opcodes::READ_ZERO_LATENCY, 0x5000, read_buffer);
    std::cout << "  Read back: ";
    for (size_t i = 0; i < 4; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)read_buffer[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Test Program 4-byte (0x12)
    target.process_command(xspi_opcodes::WRITE_ENABLE, 0, status_buffer);
    target.process_command(xspi_opcodes::PROGRAM_4BYTE, 0x6000, status_buffer, program_data);
    std::cout << "  Programmed 4 bytes @ 0x6000 using opcode 0x12 (4-byte)" << std::endl;
    
    // Test program without WREN (should fail)
    target.process_command(xspi_opcodes::WRITE_DISABLE, 0, status_buffer);
    target.process_command(xspi_opcodes::PROGRAM, 0x7000, status_buffer, program_data);
    
    // Test 10: Profile 1 Erase Commands
    std::cout << "\n[Test 10] Profile 1 Erase Commands" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Write some data to a 64KB block first
    target.process_command(xspi_opcodes::WRITE_ENABLE, 0, status_buffer);
    std::vector<uint8_t> test_data(256, 0x55);
    target.process_command(xspi_opcodes::PROGRAM, 0x10000, status_buffer, test_data);
    std::cout << "  Wrote test data to 0x10000 before erase" << std::endl;
    
    // Test Erase 64KB (0xD8) - 3-byte address
    target.process_command(xspi_opcodes::WRITE_ENABLE, 0, status_buffer);
    target.process_command(xspi_opcodes::ERASE_64KB, 0x10000, status_buffer);
    std::cout << "  Erased 64KB block starting @ 0x10000 using opcode 0xD8" << std::endl;
    
    // Verify erase (should be 0xFF)
    target.process_command(xspi_opcodes::READ_ZERO_LATENCY, 0x10000, read_buffer);
    std::cout << "  Read after erase: ";
    for (size_t i = 0; i < 4; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)read_buffer[i] << " ";
    }
    std::cout << std::dec << " (should be 0xFF)" << std::endl;
    
    // Test Erase 64KB 4-byte (0xDC)
    target.process_command(xspi_opcodes::WRITE_ENABLE, 0, status_buffer);
    target.process_command(xspi_opcodes::ERASE_64KB_4BYTE, 0x20000, status_buffer);
    std::cout << "  Erased 64KB block starting @ 0x20000 using opcode 0xDC (4-byte)" << std::endl;
    
    // Test erase without WREN (should fail)
    target.process_command(xspi_opcodes::WRITE_DISABLE, 0, status_buffer);
    target.process_command(xspi_opcodes::ERASE_64KB, 0x30000, status_buffer);
    
    // Test 11: Profile 1 Suspend/Resume Commands
    std::cout << "\n[Test 11] Profile 1 Suspend/Resume Commands" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Test Suspend (0x75)
    target.process_command(xspi_opcodes::SUSPEND_75, 0, status_buffer);
    
    // Try to program while suspended (should fail)
    target.process_command(xspi_opcodes::WRITE_ENABLE, 0, status_buffer);
    target.process_command(xspi_opcodes::PROGRAM, 0x8000, status_buffer, program_data);
    
    // Test Resume (0x7A)
    target.process_command(xspi_opcodes::RESUME_7A, 0, status_buffer);
    std::cout << "  Resumed operation" << std::endl;
    
    // Now program should work
    target.process_command(xspi_opcodes::WRITE_ENABLE, 0, status_buffer);
    target.process_command(xspi_opcodes::PROGRAM, 0x8000, status_buffer, program_data);
    std::cout << "  Programmed after resume" << std::endl;
    
    // Test other suspend opcodes
    target.process_command(xspi_opcodes::SUSPEND_B0, 0, status_buffer);
    target.process_command(xspi_opcodes::RESUME_30, 0, status_buffer);
    target.process_command(xspi_opcodes::RESUME_D0, 0, status_buffer);
    std::cout << "  Tested all suspend/resume opcodes" << std::endl;
    
    // Test 12: Unknown Opcode
    std::cout << "\n[Test 12] Unknown Opcode Handling" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    target.process_command(0xFF, 0, status_buffer);
    std::cout << "  Unknown opcode 0xFF handled" << std::endl;
    
    // Test 13: Step-by-Step SFDP Discovery
    std::cout << "\n[Test 13] Step-by-Step SFDP Discovery" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Helper function to read DWORD (little-endian) from byte vector
    auto read_dword_le = [](const std::vector<uint8_t>& data, size_t offset) -> uint32_t {
        if (offset + 4 > data.size()) return 0;
        return static_cast<uint32_t>(data[offset + 0]) |
               (static_cast<uint32_t>(data[offset + 1]) << 8) |
               (static_cast<uint32_t>(data[offset + 2]) << 16) |
               (static_cast<uint32_t>(data[offset + 3]) << 24);
    };
    
    // Step 1: Read and verify SFDP header signature (first 4 bytes)
    std::cout << "\nStep 1: Reading SFDP Header Signature..." << std::endl;
    std::vector<uint8_t> header_sig_buffer(4);
    target.process_command(xspi_opcodes::READ_SFDP, 0x00, header_sig_buffer);
    
    if (header_sig_buffer.size() < 4) {
        std::cout << "  ERROR: Failed to read signature (got " << header_sig_buffer.size() << " bytes)" << std::endl;
    } else {
        uint32_t signature = read_dword_le(header_sig_buffer, 0);
        bool valid = (signature == SFDP_SIGNATURE);
        std::cout << "  SFDP Signature: 0x" << std::hex << std::setw(8) << std::setfill('0') 
                  << signature << " (\"PDFS\") - " << (valid ? "✓ Valid" : "✗ Invalid") << std::dec << std::endl;
        
        if (!valid) {
            std::cout << "  ERROR: Invalid SFDP signature!" << std::endl;
        }
    }
    
    // Step 2: Read complete SFDP header (8 bytes)
    std::cout << "\nStep 2: Reading Complete SFDP Header..." << std::endl;
    std::vector<uint8_t> sfdp_header_buffer(8);
    target.process_command(xspi_opcodes::READ_SFDP, 0x00, sfdp_header_buffer);
    
    if (sfdp_header_buffer.size() < 8) {
        std::cout << "  ERROR: Failed to read complete header (got " << sfdp_header_buffer.size() << " bytes)" << std::endl;
    } else {
        uint32_t signature = read_dword_le(sfdp_header_buffer, 0);
        uint8_t minor_rev = sfdp_header_buffer[4];
        uint8_t major_rev = sfdp_header_buffer[5];
        uint8_t nph = sfdp_header_buffer[6];
        uint8_t unused = sfdp_header_buffer[7];
        
        std::cout << "  Signature:       0x" << std::hex << std::setw(8) << std::setfill('0') 
                  << signature << std::dec << std::endl;
        std::cout << "  Major Revision:  " << (int)major_rev << std::endl;
        std::cout << "  Minor Revision:  " << (int)minor_rev << std::endl;
        std::cout << "  NPH:             " << (int)nph << " (Number of Parameter Headers: " 
                  << ((int)nph + 1) << ")" << std::endl;
        std::cout << "  Unused:          0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)unused << std::dec << std::endl;
        
        // Step 3: Loop through parameter headers
        std::cout << "\nStep 3: Reading Parameter Headers (NPH=" << (int)nph 
                  << ", reading " << ((int)nph + 1) << " header(s))..." << std::endl;
        
        uint8_t num_headers = nph + 1;
        sfdp_header_t parsed_header;
        parsed_header.signature = signature;
        parsed_header.minor_rev = minor_rev;
        parsed_header.major_rev = major_rev;
        parsed_header.nph = nph;
        parsed_header.unused = unused;
        
        sfdp_parameter_header_t first_p_header;
        
        for (uint8_t i = 0; i < num_headers; ++i) {
            uint32_t ph_offset = 0x08 + (i * 8);
            std::vector<uint8_t> ph_buffer(8);
            target.process_command(xspi_opcodes::READ_SFDP, ph_offset, ph_buffer);
            
            if (ph_buffer.size() < 8) {
                std::cout << "  ERROR: Failed to read parameter header #" << (int)i 
                          << " (got " << ph_buffer.size() << " bytes)" << std::endl;
                continue;
            }
            
            // Parse parameter header
            uint8_t id_lsb = ph_buffer[0];
            uint8_t ph_minor_rev = ph_buffer[1];
            uint8_t ph_major_rev = ph_buffer[2];
            uint8_t length_dwords = ph_buffer[3];
            uint32_t ptp = static_cast<uint32_t>(ph_buffer[4]) |
                          (static_cast<uint32_t>(ph_buffer[5]) << 8) |
                          (static_cast<uint32_t>(ph_buffer[6]) << 16);
            
            std::cout << "\n  Parameter Header #" << (int)i << ":" << std::endl;
            std::cout << "    Parameter ID LSB:    0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)id_lsb << std::dec << std::endl;
            std::cout << "    Major Revision:      0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)ph_major_rev << std::dec << std::endl;
            std::cout << "    Minor Revision:      0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)ph_minor_rev << std::dec << std::endl;
            std::cout << "    Length (DWORDs):     " << std::dec << (int)length_dwords << std::endl;
            std::cout << "    Parameter Table Ptr: 0x" << std::hex << std::setw(6) << std::setfill('0') 
                      << ptp << std::dec << std::endl;
            
            // Step 4: Verify first parameter header (JEDEC Basic Table)
            if (i == 0) {
                std::cout << "\nStep 4: Verifying First Parameter Header (JEDEC Basic)..." << std::endl;
                bool id_ok = (id_lsb == 0x00);
                bool major_ok = (ph_major_rev == 0x01);
                bool minor_ok = (ph_minor_rev == 0x05);
                bool length_ok = (length_dwords == 16);
                
                std::cout << "    Parameter ID LSB:    0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)id_lsb << (id_ok ? " ✓" : " ✗") << std::dec << std::endl;
                std::cout << "    Major Revision:      0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)ph_major_rev << (major_ok ? " ✓" : " ✗") << std::dec << std::endl;
                std::cout << "    Minor Revision:      0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)ph_minor_rev << (minor_ok ? " ✓" : " ✗") << std::dec << std::endl;
                std::cout << "    Length (DWORDs):     " << std::dec << (int)length_dwords 
                          << (length_ok ? " ✓" : " ✗") << std::endl;
                std::cout << "    Parameter Table Ptr: 0x" << std::hex << std::setw(6) << std::setfill('0') 
                          << ptp << std::dec << std::endl;
                
                if (id_ok && major_ok && minor_ok && length_ok) {
                    first_p_header.id_lsb = id_lsb;
                    first_p_header.minor_rev = ph_minor_rev;
                    first_p_header.major_rev = ph_major_rev;
                    first_p_header.length_dwords = length_dwords;
                    first_p_header.ptp = ptp;
                    
                    // Step 5: Read parameter table data
                    std::cout << "\nStep 5: Reading Parameter Table..." << std::endl;
                    uint32_t length_bytes = length_dwords * 4;
                    std::cout << "  Reading " << length_bytes << " bytes from address 0x" 
                              << std::hex << std::setw(6) << std::setfill('0') << ptp << std::dec << "..." << std::endl;
                    
                    std::vector<uint8_t> table_buffer(length_bytes);
                    target.process_command(xspi_opcodes::READ_SFDP, ptp, table_buffer);
                    
                    if (table_buffer.size() < length_bytes) {
                        std::cout << "  ERROR: Failed to read parameter table (got " << table_buffer.size() 
                                  << " bytes, expected " << length_bytes << ")" << std::endl;
                    } else {
                        std::cout << "  Successfully read " << table_buffer.size() << " bytes" << std::endl;
                        
                        // Step 6: Parse and print full SFDP tree
                        std::cout << "\nStep 6: Parsing and Printing Full SFDP Tree..." << std::endl;
                        
                        // Parse basic table from the read data
                        jedec_basic_table_t parsed_table;
                        if (table_buffer.size() >= (16 * 4)) {
                            // Parse each DWORD
                            parsed_table.get_dword1().from_dword(read_dword_le(table_buffer, 0 * 4));
                            parsed_table.get_dword2().from_dword(read_dword_le(table_buffer, 1 * 4));
                            parsed_table.get_dword3().from_dword(read_dword_le(table_buffer, 2 * 4));
                            parsed_table.get_dword4().from_dword(read_dword_le(table_buffer, 3 * 4));
                            parsed_table.get_dword5().from_dword(read_dword_le(table_buffer, 4 * 4));
                            parsed_table.get_dword6().from_dword(read_dword_le(table_buffer, 5 * 4));
                            parsed_table.get_dword7().from_dword(read_dword_le(table_buffer, 6 * 4));
                            parsed_table.get_dword8().from_dword(read_dword_le(table_buffer, 7 * 4));
                            parsed_table.get_dword9().from_dword(read_dword_le(table_buffer, 8 * 4));
                            parsed_table.get_dword10().from_dword(read_dword_le(table_buffer, 9 * 4));
                            parsed_table.get_dword11().from_dword(read_dword_le(table_buffer, 10 * 4));
                            parsed_table.get_dword12().from_dword(read_dword_le(table_buffer, 11 * 4));
                            parsed_table.get_dword13().from_dword(read_dword_le(table_buffer, 12 * 4));
                            parsed_table.get_dword14().from_dword(read_dword_le(table_buffer, 13 * 4));
                            parsed_table.get_dword15().from_dword(read_dword_le(table_buffer, 14 * 4));
                            parsed_table.get_dword16().from_dword(read_dword_le(table_buffer, 15 * 4));
                            
                            // Print the full SFDP tree
                            print_sfdp_tree(parsed_header, first_p_header, parsed_table);
                        } else {
                            std::cout << "  ERROR: Not enough data to parse basic table" << std::endl;
                        }
                    }
                } else {
                    std::cout << "  ERROR: First parameter header verification failed!" << std::endl;
                }
            }
        }
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << " All Tests Completed" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return 0;
}

