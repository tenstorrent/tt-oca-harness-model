/**
 * @file spi_flash_model.cpp
 * @brief SPI Flash device model implementation
 *
 * Ported from: knowledge_base/code/xspi_target/SystemC/src/xspi_target_lib.cc
 * Key change: scml2::memory replaced with std::vector<uint8_t>
 */

#include "spi_flash_model.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

spi_flash_model::spi_flash_model(uint32_t size_bytes)
    : m_mem(size_bytes, 0xFF)
    , m_write_enabled(false)
    , m_op_suspended(false)
    , m_status_reg(0x00)
    , m_4byte_address_mode(false)
    , m_reset_enabled(false)
{
    // Build SFDP ROM — populate with default parameters that describe this model:
    //   density  = size_bytes * 8 bits
    //   erase    = 64 KB block (0xD8 / 0xDC)
    //   page     = 256 bytes
    //   address  = 3-byte only for <16 MB, 3-or-4-byte otherwise
    m_basic_table.set_density(static_cast<uint64_t>(size_bytes) * 8u);
    m_basic_table.set_address_bytes(
        size_bytes > (16u * 1024u * 1024u) ? ADDR_3_OR_4_BYTE : ADDR_3_BYTE_ONLY);
    m_basic_table.get_dword8().set_type1_size_pow2(16);           // 2^16 = 64 KB
    m_basic_table.get_dword8().set_type1_opcode(spi_flash_opcodes::ERASE_64KB);
    m_basic_table.get_dword11().set_page_size_pow2(8);            // 2^8 = 256 bytes
    m_basic_table.set_soft_reset_support(static_cast<uint8_t>(RESET_66H_99H));

    m_sfdp_rom.build_default(m_sfdp_header, m_param_header, m_basic_table);
}

// ============================================================================
// RESET
// ============================================================================

void spi_flash_model::reset()
{
    m_write_enabled = false;
    m_op_suspended  = false;
    m_status_reg    = 0x00;
    m_4byte_address_mode = false;
    m_reset_enabled = false;
    std::cout << "[spi_flash] Reset: device state cleared, memory preserved\n";
}

// ============================================================================
// MEMORY HELPERS
// ============================================================================

uint8_t spi_flash_model::read_byte(uint32_t address) const
{
    if (address < m_mem.size()) return m_mem[address];
    return 0xFF;
}

void spi_flash_model::write_byte(uint32_t address, uint8_t value)
{
    if (address < m_mem.size()) m_mem[address] = value;
}

void spi_flash_model::sync_sr1_wel()
{
    if (m_write_enabled)
        m_status_reg |=  (1u << 1);
    else
        m_status_reg &= ~(1u << 1);
}

// ============================================================================
// SFDP ROM UPDATE
// ============================================================================

void spi_flash_model::update_sfdp_rom()
{
    m_param_header.set_pointer(0x80);
    m_sfdp_rom.build_default(m_sfdp_header, m_param_header, m_basic_table);
}

// ============================================================================
// BACKDOOR FILE I/O
// ============================================================================

bool spi_flash_model::load_memory_from_file()
{
    std::ifstream file(BACKDOOR_FILE_PATH, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "[spi_flash] Backdoor: " << BACKDOOR_FILE_PATH
                  << " not found — using blank (0xFF) memory\n";
        return false;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_size <= 0) { file.close(); return false; }

    size_t load_size = std::min(static_cast<size_t>(file_size), m_mem.size());
    if (!file.read(reinterpret_cast<char*>(m_mem.data()),
                   static_cast<std::streamsize>(load_size))) {
        file.close();
        return false;
    }
    file.close();

    std::cout << "[spi_flash] Backdoor: loaded " << load_size
              << " bytes from " << BACKDOOR_FILE_PATH << "\n";
    return true;
}

bool spi_flash_model::save_memory_to_file() const
{
    // Ensure data/ directory exists
    struct stat info;
    if (stat("data", &info) != 0) mkdir("data", 0755);

    std::ofstream file(BACKDOOR_FILE_PATH, std::ios::binary);
    if (!file.is_open()) return false;

    bool ok = !!file.write(reinterpret_cast<const char*>(m_mem.data()),
                           static_cast<std::streamsize>(m_mem.size()));
    file.close();
    if (ok)
        std::cout << "[spi_flash] Backdoor: saved " << m_mem.size()
                  << " bytes to " << BACKDOOR_FILE_PATH << "\n";
    return ok;
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void spi_flash_model::handle_read(uint32_t address, std::vector<uint8_t>& rx_buffer)
{
    std::cout << "[spi_flash] Read @ 0x" << std::hex << address
              << " len=" << std::dec << rx_buffer.size() << "\n";
    for (size_t i = 0; i < rx_buffer.size(); ++i)
        rx_buffer[i] = read_byte(static_cast<uint32_t>(address + i));
}

void spi_flash_model::handle_read_sfdp(uint32_t address, std::vector<uint8_t>& rx_buffer)
{
    std::cout << "[spi_flash] READ_SFDP @ 0x" << std::hex << address << std::dec << "\n";
    for (size_t i = 0; i < rx_buffer.size(); ++i)
        rx_buffer[i] = m_sfdp_rom.read_byte(static_cast<uint32_t>(address + i));
}

bool spi_flash_model::handle_program(uint32_t address,
                                     const std::vector<uint8_t>& tx_buffer,
                                     bool addr_4byte)
{
    if (!m_write_enabled) {
        std::cout << "[spi_flash] Program FAILED: WEL not set\n";
        return false;
    }
    if (m_op_suspended) {
        std::cout << "[spi_flash] Program FAILED: operation suspended\n";
        return false;
    }

    std::cout << "[spi_flash] Program (" << (addr_4byte ? "4B" : "3B")
              << ") @ 0x" << std::hex << address
              << " len=" << std::dec << tx_buffer.size() << "\n";

    // Flash semantics: program can only clear bits (AND operation)
    for (size_t i = 0; i < tx_buffer.size(); ++i) {
        uint32_t addr = static_cast<uint32_t>(address + i);
        write_byte(addr, read_byte(addr) & tx_buffer[i]);
    }

    m_write_enabled = false;
    sync_sr1_wel();
    return true;
}

bool spi_flash_model::handle_erase(uint32_t address, bool addr_4byte, uint32_t erase_size)
{
    if (!m_write_enabled) {
        std::cout << "[spi_flash] Erase FAILED: WEL not set\n";
        return false;
    }
    if (m_op_suspended) {
        std::cout << "[spi_flash] Erase FAILED: operation suspended\n";
        return false;
    }

    uint32_t block_start = address & ~(erase_size - 1u);

    std::cout << "[spi_flash] Erase (" << (erase_size / 1024) << "KB, " << (addr_4byte ? "4B" : "3B")
              << ") block @ 0x" << std::hex << block_start << std::dec << "\n";

    for (uint32_t i = 0; i < erase_size; ++i)
        write_byte(block_start + i, 0xFF);

    m_write_enabled = false;
    sync_sr1_wel();
    return true;
}

void spi_flash_model::handle_chip_erase()
{
    if (!m_write_enabled) {
        std::cout << "[spi_flash] Chip Erase FAILED: WEL not set\n";
        return;
    }
    if (m_op_suspended) {
        std::cout << "[spi_flash] Chip Erase FAILED: operation suspended\n";
        return;
    }

    std::cout << "[spi_flash] Chip Erase: erasing entire flash\n";
    std::fill(m_mem.begin(), m_mem.end(), 0xFF);

    m_write_enabled = false;
    sync_sr1_wel();
}

void spi_flash_model::handle_control(uint8_t opcode, std::vector<uint8_t>& rx_buffer)
{
    using namespace spi_flash_opcodes;
    switch (opcode) {
        case WRITE_ENABLE:
            m_write_enabled = true;
            sync_sr1_wel();
            std::cout << "[spi_flash] WREN: write enabled\n";
            break;

        case WRITE_DISABLE:
            m_write_enabled = false;
            sync_sr1_wel();
            std::cout << "[spi_flash] WRDI: write disabled\n";
            break;

        case READ_STATUS:
            if (!rx_buffer.empty()) {
                rx_buffer[0] = m_status_reg;
                std::cout << "[spi_flash] RDSR: SR1=0x"
                          << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(m_status_reg) << std::dec << "\n";
            }
            break;

        case EN4B:
            m_4byte_address_mode = true;
            std::cout << "[spi_flash] EN4B: 4-byte address mode activated\n";
            break;

        case EX4B:
            m_4byte_address_mode = false;
            std::cout << "[spi_flash] EX4B: 4-byte address mode deactivated\n";
            break;

        case RESET_ENABLE:
            m_reset_enabled = true;
            std::cout << "[spi_flash] RESET_ENABLE: reset sequence armed\n";
            break;

        case RESET_EXECUTE:
            if (m_reset_enabled) {
                std::cout << "[spi_flash] RESET_EXECUTE: executing software reset\n";
                reset();
                // Notice reset() clears m_reset_enabled
            } else {
                std::cout << "[spi_flash] RESET_EXECUTE ignored: not armed via 0x66\n";
            }
            break;

        default:
            std::cerr << "[spi_flash] Unknown control opcode: 0x"
                      << std::hex << static_cast<int>(opcode) << std::dec << "\n";
            break;
    }
}

void spi_flash_model::handle_suspend_resume(uint8_t opcode)
{
    using namespace spi_flash_opcodes;
    switch (opcode) {
        case SUSPEND_75:
        case SUSPEND_B0:
            m_op_suspended = true;
            std::cout << "[spi_flash] Suspend (0x"
                      << std::hex << static_cast<int>(opcode) << std::dec << ")\n";
            break;

        case RESUME_30:
        case RESUME_7A:
        case RESUME_D0:
            m_op_suspended = false;
            std::cout << "[spi_flash] Resume (0x"
                      << std::hex << static_cast<int>(opcode) << std::dec << ")\n";
            break;

        default:
            std::cerr << "[spi_flash] Unknown suspend/resume opcode: 0x"
                      << std::hex << static_cast<int>(opcode) << std::dec << "\n";
            break;
    }
}

// ============================================================================
// MAIN COMMAND DISPATCHER
// ============================================================================

bool spi_flash_model::process_command(uint8_t opcode,
                                      uint32_t address,
                                      std::vector<uint8_t>& rx_buffer,
                                      const std::vector<uint8_t>& tx_buffer)
{
    using namespace spi_flash_opcodes;

    switch (opcode) {
        // --- Read ---
        case READ:
        case READ_4B:
        case READ_FAST:
        case READ_FAST_ALT:
            handle_read(address, rx_buffer);
            return true;

        case READ_SFDP:
            handle_read_sfdp(address, rx_buffer);
            return true;

        // --- Program ---
        case PROGRAM:
            return handle_program(address, tx_buffer, false);
        case PROGRAM_4BYTE:
            return handle_program(address, tx_buffer, true);

        // --- Erase ---
        case ERASE_4KB:
            return handle_erase(address, false, 4096u);
        case ERASE_32KB:
            return handle_erase(address, false, 32768u);
        case ERASE_64KB:
            return handle_erase(address, false, ERASE_BLOCK_SIZE);
        case ERASE_64KB_4B:
            return handle_erase(address, true, ERASE_BLOCK_SIZE);
        case CHIP_ERASE:
            handle_chip_erase();
            return true;

        // --- Control ---
        case WRITE_ENABLE:
        case WRITE_DISABLE:
        case READ_STATUS:
        case EN4B:
        case EX4B:
        case RESET_ENABLE:
        case RESET_EXECUTE:
            handle_control(opcode, rx_buffer);
            return true;

        // --- Suspend / Resume ---
        case SUSPEND_75:
        case SUSPEND_B0:
        case RESUME_30:
        case RESUME_7A:
        case RESUME_D0:
            handle_suspend_resume(opcode);
            return true;

        default:
            std::cerr << "[spi_flash] Unknown opcode: 0x"
                      << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(opcode) << std::dec << "\n";
            return false;
    }
}
