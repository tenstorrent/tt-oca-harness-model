/**
 * @file spi_flash.cpp
 * @brief SystemC wrapper for the SPI Flash device model
 *
 * Ported from:
 *   knowledge_base/code/xspi_target/SystemC/src/xspi_target.cc
 *   knowledge_base/code/cdns_xspi_cntr_model/Tests/Unittests/xspi_target_sc_wrapper.cc
 *
 * Key change: uses spi_if (not TLM b_transport); multi-segment TX
 * accumulation replaces single-call opcode extraction.
 */

#include "spi_flash.h"

#include <iostream>
#include <iomanip>
#include <cassert>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

spi_flash::spi_flash(sc_module_name name, uint32_t size_bytes, int log_verbosity)
    : sc_module(name)
    , spi_target("spi_target")
    , rst_ni("rst_ni")
    , verbosity("verbosity", log_verbosity)
    , m_model(size_bytes)
{
    logger.setMaxVerbosity(verbosity.get_param_value());

    // Bind the export to this module's spi_if implementation
    spi_target(*this);

    // Reset sensitivity: assert model reset whenever rst_ni is de-asserted
    SC_METHOD(reset_method);
    sensitive << rst_ni;
    dont_initialize();

    std::cout << "[spi_flash] " << name
              << ": created, size=" << (size_bytes / (1024u * 1024u)) << " MB\n";
}

// ============================================================================
// RESET METHOD
// ============================================================================

void spi_flash::reset_method()
{
    if (!rst_ni.read()) {
        m_model.reset();
        m_tx_accum.clear();
        std::cout << "[spi_flash] Reset asserted — device state cleared\n";
    }
}

// ============================================================================
// OPCODE → ADDRESS LENGTH TABLE
// ============================================================================

int spi_flash::addr_bytes_for_opcode(uint8_t opcode) const
{
    using namespace spi_flash_opcodes;

    // Explicit 4-byte address instructions
    switch (opcode) {
        case READ_4B:
        case PROGRAM_4BYTE:
        case ERASE_64KB_4B:
            return 4;

        // Legacy instructions (address size depends on EN4B state)
        case READ:
        case READ_FAST:
        case READ_FAST_ALT:
        case PROGRAM:
        case ERASE_4KB:
        case ERASE_32KB:
        case ERASE_64KB:
            return m_model.is_4byte_address_mode() ? 4 : 3;

        // Explicit 3-byte address instructions (e.g. SFDP is always 3 bytes)
        case READ_SFDP:
            return 3;

        // No-address commands (control / suspend / resume)
        case WRITE_ENABLE:
        case WRITE_DISABLE:
        case READ_STATUS:
        case SUSPEND_75:
        case SUSPEND_B0:
        case RESUME_30:
        case RESUME_7A:
        case RESUME_D0:
        case EN4B:
        case EX4B:
        case RESET_ENABLE:
        case RESET_EXECUTE:
        case CHIP_ERASE:
            return 0;

        default:
            // Safe fallback: assume 3-byte if unknown, but flag it
            return 3;
    }
}

// ============================================================================
// SPI TRANSACTION (multi-segment state machine)
// ============================================================================

bool spi_flash::spi_transaction(const spi_segment_t& segment,
                                 const spi_config_t& /*config*/,
                                 const uint8_t*       tx_data,
                                 uint8_t*             rx_data)
{
    // ------------------------------------------------------------------
    // Step 1 — Accumulate TX bytes from this segment
    // ------------------------------------------------------------------
    if (segment.direction == spi_direction_e::TX_ONLY ||
        segment.direction == spi_direction_e::BIDIR)
    {
        if (tx_data != nullptr) {
            m_tx_accum.insert(m_tx_accum.end(),
                              tx_data,
                              tx_data + segment.len);
        }
    }
    // DUMMY and RX_ONLY segments carry no TX bytes — nothing to accumulate.

    // ------------------------------------------------------------------
    // Step 2 — If CS stays asserted, wait for the next segment
    // ------------------------------------------------------------------
    if (segment.csaat) {
        return true;   // transaction not yet complete
    }

    // ------------------------------------------------------------------
    // Step 3 — Last segment: parse the accumulated TX bytes
    // ------------------------------------------------------------------
    if (m_tx_accum.empty()) {
        // Edge case: pure RX-only single-segment — no opcode to parse.
        // This should not happen in normal SPI flash usage.
        std::cerr << "[spi_flash] spi_transaction: csaat=false but no TX bytes "
                     "accumulated — ignoring\n";
        return false;
    }

    uint8_t opcode = m_tx_accum[0];
    int     addr_len = addr_bytes_for_opcode(opcode);

    // Parse address (big-endian, MSB first)
    uint32_t address = 0;
    for (int i = 0; i < addr_len; ++i) {
        size_t idx = static_cast<size_t>(1 + i);
        if (idx < m_tx_accum.size()) {
            address = (address << 8u) | m_tx_accum[idx];
        }
    }

    // Any bytes after opcode + address are write (program) data
    size_t hdr_len = static_cast<size_t>(1 + addr_len);
    std::vector<uint8_t> write_data;
    if (hdr_len < m_tx_accum.size()) {
        write_data.assign(m_tx_accum.begin() + static_cast<ptrdiff_t>(hdr_len),
                          m_tx_accum.end());
    }

    // ------------------------------------------------------------------
    // Step 4 — Prepare receive buffer for read commands
    // ------------------------------------------------------------------
    std::vector<uint8_t> rx_vec;
    if ((segment.direction == spi_direction_e::RX_ONLY ||
         segment.direction == spi_direction_e::BIDIR) &&
        segment.len > 0)
    {
        rx_vec.resize(segment.len, 0x00);
    }

    // ------------------------------------------------------------------
    // Step 5 — Dispatch to the pure-C++ flash model
    // ------------------------------------------------------------------
    bool ok = m_model.process_command(opcode, address, rx_vec, write_data);

    // ------------------------------------------------------------------
    // Step 6 — Copy read data back to the caller's buffer
    // ------------------------------------------------------------------
    if (rx_data != nullptr && !rx_vec.empty()) {
        for (size_t i = 0; i < rx_vec.size() && i < segment.len; ++i)
            rx_data[i] = rx_vec[i];
    }

    // ------------------------------------------------------------------
    // Step 7 — Clear accumulator ready for next command
    // ------------------------------------------------------------------
    m_tx_accum.clear();

    return ok;
}
