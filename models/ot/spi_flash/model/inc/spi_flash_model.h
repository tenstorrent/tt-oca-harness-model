#pragma once

/**
 * @file spi_flash_model.h
 * @brief SPI Flash device model — pure C++ command handler
 *
 * Implements flash memory emulation with:
 * - SFDP ROM (JESD216A compliant)
 * - Profile 1 command set (16 opcodes)
 * - 32 MB internal memory (std::vector<uint8_t>, initialised to 0xFF)
 * - Write-enable (WREN/WRDI) enforcement
 * - Suspend / Resume support
 * - Backdoor file I/O for memory persistence
 *
 * No SystemC dependencies — compiles and tests standalone.
 *
 * Ported from: knowledge_base/code/xspi_target/SystemC/include/xspi_target_lib.h
 *              knowledge_base/code/xspi_target/SystemC/src/xspi_target_lib.cc
 */


#include "spi_flash_sfdp.h"
#include <vector>
#include <cstdint>
#include <string>

/**
 * @class spi_flash_model
 * @brief SPI NOR flash device model
 *
 * This is an LT (Loosely Timed) model — no timing simulation is performed.
 * All operations complete instantly from the caller's perspective.
 *
 * The memory is a flat byte array initialised to 0xFF (erased state).
 * Flash write semantics are enforced: program can only clear bits (1→0),
 * erase restores a 64 KB block to 0xFF.
 */
class spi_flash_model {
public:
    /// Default flash size: 32 MB
    static constexpr uint32_t DEFAULT_FLASH_SIZE = 32u * 1024u * 1024u;

    /// Erase block size: 64 KB
    static constexpr uint32_t ERASE_BLOCK_SIZE   = 64u * 1024u;

    /// Backdoor persistence file
    static constexpr const char* BACKDOOR_FILE_PATH = "data/flash_memory.bin";

    /**
     * @brief Constructor
     * @param size_bytes Flash capacity in bytes (default 32 MB)
     */
    explicit spi_flash_model(uint32_t size_bytes = DEFAULT_FLASH_SIZE);

    ~spi_flash_model() = default;

    // ------------------------------------------------------------------
    // Primary interface — called by the SystemC wrapper
    // ------------------------------------------------------------------

    /**
     * @brief Process one decoded flash command
     *
     * Supported opcodes (Profile 1):
     *   Read:          0x03, 0x0B, 0xEE, 0x5A, 0x13
     *   Program:       0x02, 0x12
     *   Erase:         0x20, 0x52, 0x60, 0xD8, 0xDC
     *   Control:       0x04, 0x05, 0x06, 0xB7, 0xE9, 0x66, 0x99
     *   Suspend/Resume:0x75, 0xB0, 0x30, 0x7A, 0xD0
     *
     * @param opcode    Command opcode
     * @param address   Flash or SFDP address
     * @param rx_buffer Buffer filled with read data (caller pre-sizes for reads)
     * @param tx_buffer Write data payload (for program commands)
     * @return true on success, false on error (WEL not set, unknown opcode, …)
     */
    bool process_command(uint8_t opcode,
                         uint32_t address,
                         std::vector<uint8_t>& rx_buffer,
                         const std::vector<uint8_t>& tx_buffer = {});

    /**
     * @brief Reset device to power-on state
     *
     * Clears write_enabled, operation_suspended, and status_register.
     * Does NOT modify flash memory contents or SFDP structures.
     */
    void reset();

    /**
     * @brief Is device in 4-byte address mode
     * @return true if EN4B is active
     */
    bool is_4byte_address_mode() const { return m_4byte_address_mode; }

    // ------------------------------------------------------------------
    // SFDP accessors (for testbench / backdoor inspection)
    // ------------------------------------------------------------------

    sfdp_header_t*            get_sfdp_header()      { return &m_sfdp_header; }
    const sfdp_header_t*      get_sfdp_header() const{ return &m_sfdp_header; }
    sfdp_parameter_header_t*  get_param_header()     { return &m_param_header; }
    jedec_basic_table_t*      get_basic_table()      { return &m_basic_table; }
    const jedec_basic_table_t* get_basic_table() const{ return &m_basic_table; }
    sfdp_rom_t*               get_sfdp_rom()         { return &m_sfdp_rom; }

    /**
     * @brief Rebuild the SFDP ROM from current header / table structures
     *
     * Call after modifying m_sfdp_header, m_param_header, or m_basic_table
     * if you want the changes reflected in READ_SFDP responses.
     */
    void update_sfdp_rom();

    // ------------------------------------------------------------------
    // Memory backdoor (for testbench pre-loading / persistence)
    // ------------------------------------------------------------------

    /**
     * @brief Load flash memory from BACKDOOR_FILE_PATH
     * @return true if the file was found and loaded
     */
    bool load_memory_from_file();

    /**
     * @brief Save flash memory to BACKDOOR_FILE_PATH
     * @return true if the file was written successfully
     */
    bool save_memory_to_file() const;

    /**
     * @brief Direct byte read (no command overhead)
     * @return 0xFF for out-of-bounds addresses
     */
    uint8_t  read_byte(uint32_t address) const;

    /**
     * @brief Direct byte write (no command overhead, no WEL check)
     *
     * Bypasses all flash protection — intended for testbench pre-loading only.
     */
    void write_byte(uint32_t address, uint8_t value);

    /// Flash capacity in bytes
    uint32_t size() const { return static_cast<uint32_t>(m_mem.size()); }

private:
    // ------------------------------------------------------------------
    // Flash memory storage
    // ------------------------------------------------------------------
    std::vector<uint8_t> m_mem;  ///< Flat byte array, initialised to 0xFF

    // ------------------------------------------------------------------
    // SFDP structures
    // ------------------------------------------------------------------
    sfdp_header_t           m_sfdp_header;
    sfdp_parameter_header_t m_param_header;
    jedec_basic_table_t     m_basic_table;
    sfdp_rom_t              m_sfdp_rom;

    // ------------------------------------------------------------------
    // Device state
    // ------------------------------------------------------------------
    bool    m_write_enabled;       ///< Write Enable Latch (WEL)
    bool    m_op_suspended;        ///< Suspend flag
    uint8_t m_status_reg;          ///< Status Register 1 (SR1)
    bool    m_4byte_address_mode;  ///< Currently in 4-byte address mode via EN4B
    bool    m_reset_enabled;       ///< Reset sequence initiated (66h)

    // ------------------------------------------------------------------
    // Internal command handlers
    // ------------------------------------------------------------------
    void handle_read(uint32_t address, std::vector<uint8_t>& rx_buffer);
    void handle_read_sfdp(uint32_t address, std::vector<uint8_t>& rx_buffer);
    bool handle_program(uint32_t address, const std::vector<uint8_t>& tx_buffer, bool addr_4byte);
    bool handle_erase(uint32_t address, bool addr_4byte, uint32_t erase_size);
    void handle_chip_erase();
    void handle_control(uint8_t opcode, std::vector<uint8_t>& rx_buffer);
    void handle_suspend_resume(uint8_t opcode);

    /// Synchronise SR1 bit 1 (WEL) with m_write_enabled
    void sync_sr1_wel();
};

