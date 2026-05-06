/*
 * xspi_target.cpp
 *
 *  Created on: Jan 21, 2026
 *      Author: ctr-sdangi
 */




#include "xspi_target.h"
#include <iostream>
#include <cstring>
#include <iomanip>
#include <vector>

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

    // File-backed backdoor init intentionally disabled.
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
    for (size_t i = 0; i < rx_buffer.size(); ++i) {
        rx_buffer[i] = sfdp_rom.read_byte(address + i);
    }
}

bool xspi_target_model::handle_program_command(uint32_t address, const std::vector<uint8_t>& tx_buffer, bool is_4byte) {
    
    if (!write_enabled) {
        return false;
    }

    if (operation_suspended) {
        return false;
    }

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
    return true;
}

bool xspi_target_model::handle_erase_command(uint32_t address, bool is_4byte) {
    if (!write_enabled) {
        return false;
    }

    if (operation_suspended) {
        return false;
    }

    // Erase 64KB block (0x10000 bytes)
    const uint32_t ERASE_SIZE = 64 * 1024;
    uint32_t block_start = address & ~(ERASE_SIZE - 1); // Align to 64KB boundary

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
}

void xspi_target_model::handle_control_command(uint8_t opcode, std::vector<uint8_t>& rx_buffer) {
    switch (opcode) {
        case xspi_opcodes::WRITE_ENABLE:
            write_enabled = true;
            update_sr1_wel();  // Sync SR1 bit 1 (WEL)
            break;

        case xspi_opcodes::WRITE_DISABLE:
            write_enabled = false;
            update_sr1_wel();  // Sync SR1 bit 1 (WEL)
            break;

        case xspi_opcodes::READ_STATUS_REG:
            if (rx_buffer.size() > 0) {
                rx_buffer[0] = status_register;
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
            break;

        case xspi_opcodes::RESUME_30:
        case xspi_opcodes::RESUME_7A:
        case xspi_opcodes::RESUME_D0:
            operation_suspended = false;
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
