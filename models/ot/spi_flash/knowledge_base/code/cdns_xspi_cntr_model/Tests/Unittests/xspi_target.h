/*
 * xspi_target.h
 *
 *  Created on: Jan 21, 2026
 *      Author: ctr-sdangi
 */

#ifndef TESTS_UNITTESTS_XSPI_TARGET_H_
#define TESTS_UNITTESTS_XSPI_TARGET_H_

#include "sfdp.h"
#include <vector>
#include <cstdint>



// ============================================================================
// XSPI TARGET MODEL
// ============================================================================
/**
 * @class xspi_target_model
 * @brief XSPI Target Device Model with SFDP and Profile 1 Command Support
 *
 * This class represents an XSPI target device that:
 * - Manages SFDP structures (header, parameter header, basic table, ROM)
 * - Emulates flash memory with read/write/erase operations
 * - Processes Profile 1 commands (read, program, erase, control, suspend/resume)
 * - Enforces write protection (WREN required for program/erase)
 * - Supports backdoor file I/O for memory persistence
 *
 * @note This is an LT (Loosely Timed) model - no timing simulation is performed
 */
class xspi_target_model {
private:
    // SFDP Structures
    sfdp_header_t sfdp_header;
    sfdp_parameter_header_t parameter_header;
    jedec_basic_table_t basic_table;
    sfdp_rom_t sfdp_rom;

    // Flash Memory (backdoor access)
    std::vector<uint8_t> memory;

    // Device State
    bool write_enabled;          // Write Enable (WREN) state
    bool operation_suspended;    // Suspend state
    uint8_t status_register;     // Status Register value

    // Fixed path for backdoor file loading (in data/ folder)
    static constexpr const char* BACKDOOR_FILE_PATH = "data/flash_memory.bin";

    /**
     * @brief Handle read commands (0x03, 0x0B, 0xEE)
     * @param address Memory address to read from
     * @param rx_buffer Buffer to fill with read data
     */
    void handle_read_command(uint32_t address, std::vector<uint8_t>& rx_buffer);

    /**
     * @brief Handle SFDP read command (0x5A)
     * @param address SFDP ROM address to read from
     * @param rx_buffer Buffer to fill with SFDP data
     */
    void handle_read_sfdp(uint32_t address, std::vector<uint8_t>& rx_buffer);

    /**
     * @brief Handle program commands (0x02, 0x12)
     * @param address Memory address to program
     * @param tx_buffer Data to program
     * @param is_4byte true for 4-byte addressing, false for 3-byte
     * @return true on success, false on failure (WEL not set, suspended, etc.)
     */
    bool handle_program_command(uint32_t address, const std::vector<uint8_t>& tx_buffer, bool is_4byte);

    /**
     * @brief Handle erase commands (0xD8, 0xDC)
     * @param address Block address to erase (aligned to 64KB boundary)
     * @param is_4byte true for 4-byte addressing, false for 3-byte
     * @return true on success, false on failure (WEL not set, suspended, etc.)
     */
    bool handle_erase_command(uint32_t address, bool is_4byte);

    /**
     * @brief Handle control commands (0x04, 0x05, 0x06)
     * @param opcode Control command opcode
     * @param rx_buffer Buffer for status register read (if applicable)
     */
    void handle_control_command(uint8_t opcode, std::vector<uint8_t>& rx_buffer);

    /**
     * @brief Handle suspend/resume commands (0x75, 0xB0, 0x30, 0x7A, 0xD0)
     * @param opcode Suspend or resume opcode
     */
    void handle_suspend_resume(uint8_t opcode);

    /**
     * @brief Update SR1 WEL bit (bit 1) to match write_enabled state
     *
     * Synchronizes Status Register 1 bit 1 (Write Enable Latch) with
     * the internal write_enabled boolean state.
     */
    void update_sr1_wel();

public:
    /**
     * @brief Constructor: Initialize all SFDP structures and memory
     *
     * Initializes:
     * - 16 MB flash memory (default size, filled with 0xFF)
     * - SFDP structures with default values
     * - Device state (write_enabled=false, operation_suspended=false)
     * - Attempts to load memory from backdoor file if it exists
     */
    xspi_target_model();

    /**
     * @brief Destructor
     */
    ~xspi_target_model() = default;

    /**
     * @brief Get pointer to SFDP header
     * @return Pointer to SFDP header structure
     */
    sfdp_header_t* get_sfdp_header() { return &sfdp_header; }

    /**
     * @brief Get const pointer to SFDP header
     * @return Const pointer to SFDP header structure
     */
    const sfdp_header_t* get_sfdp_header() const { return &sfdp_header; }

    /**
     * @brief Get pointer to parameter header
     * @return Pointer to SFDP parameter header structure
     */
    sfdp_parameter_header_t* get_parameter_header() { return &parameter_header; }

    /**
     * @brief Get const pointer to parameter header
     * @return Const pointer to SFDP parameter header structure
     */
    const sfdp_parameter_header_t* get_parameter_header() const { return &parameter_header; }

    /**
     * @brief Get pointer to basic flash parameter table
     * @return Pointer to JEDEC basic table structure
     */
    jedec_basic_table_t* get_basic_table() { return &basic_table; }

    /**
     * @brief Get const pointer to basic flash parameter table
     * @return Const pointer to JEDEC basic table structure
     */
    const jedec_basic_table_t* get_basic_table() const { return &basic_table; }

    /**
     * @brief Get pointer to SFDP ROM
     * @return Pointer to SFDP ROM structure
     */
    sfdp_rom_t* get_sfdp_rom() { return &sfdp_rom; }

    /**
     * @brief Get const pointer to SFDP ROM
     * @return Const pointer to SFDP ROM structure
     */
    const sfdp_rom_t* get_sfdp_rom() const { return &sfdp_rom; }

    /**
     * @brief Get pointer to flash memory
     * @return Pointer to memory vector
     */
    std::vector<uint8_t>* get_memory() { return &memory; }

    /**
     * @brief Get const pointer to flash memory
     * @return Const pointer to memory vector
     */
    const std::vector<uint8_t>* get_memory() const { return &memory; }

    /**
     * @brief Backdoor: Load memory from file
     *
     * Loads flash memory content from data/flash_memory.bin if it exists.
     * If file doesn't exist, uses default 0xFF initialization.
     *
     * @return true if file was loaded successfully, false otherwise
     */
    bool load_memory_from_file();

    /**
     * @brief Backdoor: Save memory to file
     *
     * Saves current flash memory content to data/flash_memory.bin.
     * Creates data/ directory if it doesn't exist.
     *
     * @return true if file was saved successfully, false otherwise
     */
    bool save_memory_to_file() const;

    /**
     * @brief Read byte from memory at address
     * @param address Memory address to read from
     * @return Byte value at address (0xFF if out of bounds)
     */
    uint8_t read_byte(uint32_t address) const;

    /**
     * @brief Write byte to memory at address
     *
     * Performs flash memory AND operation (can only change 1->0).
     * Silently ignores out-of-bounds writes.
     *
     * @param address Memory address to write to
     * @param value Byte value to write
     */
    void write_byte(uint32_t address, uint8_t value);

    /**
     * @brief Process Profile 1 command
     *
     * Processes flash commands including:
     * - Read commands: 0x03, 0x0B, 0xEE, 0x5A
     * - Program commands: 0x02, 0x12
     * - Erase commands: 0xD8, 0xDC
     * - Control commands: 0x04, 0x05, 0x06
     * - Suspend/Resume: 0x75, 0xB0, 0x30, 0x7A, 0xD0
     *
     * @param opcode Command opcode (Profile 1)
     * @param address Address for the command (3-byte or 4-byte depending on opcode)
     * @param rx_buffer Buffer for read operations (output, filled by function)
     * @param tx_buffer Buffer for write operations (input, optional, used for program commands)
     * @return true on success, false on failure (e.g., WEL not set, invalid opcode)
     */
    bool process_command(uint8_t opcode,
                         uint32_t address,
                         std::vector<uint8_t>& rx_buffer,
                         const std::vector<uint8_t>& tx_buffer = {});

    /**
     * @brief Reset device to initial state
     *
     * Resets device state to power-on defaults:
     * - write_enabled = false
     * - operation_suspended = false
     * - status_register = 0x00
     *
     * @note Does NOT reset memory or SFDP structures (preserves flash content)
     */
    void reset();

    /**
     * @brief Update SFDP ROM with current structures
     *
     * Rebuilds the SFDP ROM layout using current values from:
     * - SFDP header (at offset 0x00)
     * - Parameter header (at offset 0x08)
     * - Basic flash parameter table (at offset 0x80)
     */
    void update_sfdp_rom();
};








#endif /* TESTS_UNITTESTS_XSPI_TARGET_H_ */
