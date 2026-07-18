/**
 * @file spi_flash.cpp
 * @brief SystemC wrapper for the SPI Flash device model
 *
 * Ported from:
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
        m_read_bytes = 0;
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
        case READ_DUAL_OUT:
        case READ_QUAD_OUT:
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
// OPCODE CLASSIFICATION + SHARED PARSE / COPY HELPERS
// ============================================================================

bool spi_flash::is_memory_read_opcode(uint8_t opcode) const
{
    using namespace spi_flash_opcodes;
    switch (opcode) {
        case READ:
        case READ_4B:
        case READ_FAST:
        case READ_FAST_ALT:
        case READ_DUAL_OUT:
        case READ_QUAD_OUT:
        case READ_SFDP:
            return true;
        default:
            return false;   // incl. READ_STATUS: dispatched once, not streamed
    }
}

uint32_t spi_flash::parse_address(uint8_t opcode) const
{
    // Address bytes follow the opcode in the accumulated header, big-endian.
    const int alen = addr_bytes_for_opcode(opcode);
    uint32_t addr = 0;
    for (int i = 0; i < alen; ++i) {
        const size_t idx = static_cast<size_t>(1 + i);
        if (idx < m_tx_accum.size())
            addr = (addr << 8u) | m_tx_accum[idx];
    }
    return addr;
}

void spi_flash::copy_rx(uint8_t* rx_data,
                        const std::vector<uint8_t>& rx_vec,
                        uint32_t len) const
{
    if (rx_data == nullptr) return;
    for (size_t i = 0; i < rx_vec.size() && i < len; ++i)
        rx_data[i] = rx_vec[i];
}

// ============================================================================
// SPI TRANSACTION — classify each segment, then dispatch
// ============================================================================
//
// The controller calls this once per SPI segment; a logical flash command spans a
// sequence of segments framed by csaat ("chip select stays asserted after this
// segment"). Every segment first accumulates its TX bytes, then takes exactly one
// of two dispatch shapes:
//
//   * Memory read — returns data on EVERY RX segment, streamed from an advancing
//     address, so each RX segment dispatches its own read (serve_read_segment).
//   * Everything else (write / erase / control / status, or a bare RX with no
//     opcode) — accumulates until CS is released, then dispatches once
//     (dispatch_command).

bool spi_flash::spi_transaction(const spi_segment_t& segment,
                                 const spi_config_t& /*config*/,
                                 const uint8_t*       tx_data,
                                 uint8_t*             rx_data)
{
    // Accumulate this segment's TX bytes. The command header (opcode + address +
    // any write data) builds up across CSAAT-chained TX/BIDIR segments; RX and
    // DUMMY segments carry none.
    if ((segment.direction == spi_direction_e::TX_ONLY ||
         segment.direction == spi_direction_e::BIDIR) && tx_data != nullptr) {
        m_tx_accum.insert(m_tx_accum.end(), tx_data, tx_data + segment.len);
    }

    // A segment carries memory-read data iff it receives bytes and the accumulated
    // opcode is a streaming-read opcode. Such segments must be served now, from the
    // running read address — not deferred to CS release (which would drop every
    // intermediate chunk and truncate the read to its final segment).
    const bool receives =
        (segment.direction == spi_direction_e::RX_ONLY ||
         segment.direction == spi_direction_e::BIDIR) && segment.len > 0;
    if (receives && !m_tx_accum.empty() && is_memory_read_opcode(m_tx_accum[0])) {
        return serve_read_segment(segment, rx_data);
    }

    // Non-read command: keep accumulating until CS is released, then dispatch once.
    if (segment.csaat) {
        return true;
    }
    return dispatch_command(segment, rx_data);
}

// Serve one RX segment of a (possibly multi-segment) memory read. The base address
// is parsed from the stable accumulated header and offset by the bytes already
// served this command, so no separate "read active" flag or latched opcode is
// needed — RX segments add no TX bytes, so the header stays put for the whole read.
bool spi_flash::serve_read_segment(const spi_segment_t& segment, uint8_t* rx_data)
{
    const uint8_t  opcode = m_tx_accum[0];
    const uint32_t addr   = parse_address(opcode) + m_read_bytes;

    std::vector<uint8_t> rx_vec(segment.len, 0x00);
    std::vector<uint8_t> no_write;
    const bool ok = m_model.process_command(opcode, addr, rx_vec, no_write);
    copy_rx(rx_data, rx_vec, segment.len);
    m_read_bytes += segment.len;

    if (segment.csaat) {
        if (!ok) {
            // Error mid-read: reset state so the next command starts clean
            m_read_bytes = 0;
            m_tx_accum.clear();
        }
        return ok;                    // more RX segments of this read follow
    }
    m_read_bytes = 0;                 // CS released — ready for the next command
    m_tx_accum.clear();
    return ok;
}

// Dispatch a completed non-streaming command from the accumulated TX bytes: parse
// [opcode][address][write data], run it once, and copy back any read bytes (e.g.
// READ_STATUS). A bare RX with no accumulated opcode models an undriven MISO (0xFF).
bool spi_flash::dispatch_command(const spi_segment_t& segment, uint8_t* rx_data)
{
    if (m_tx_accum.empty()) {
        // RX-only segment with no preceding TX command: model MISO idle-high.
        // Exercised by VP DMA-handshake tests that issue a bare RX with no flash
        // command prefix (those check the handshake protocol, not data values).
        if (rx_data != nullptr && segment.len > 0) {
            std::memset(rx_data, 0xFF, segment.len);
        }
        return true;
    }

    const uint8_t  opcode  = m_tx_accum[0];
    const uint32_t address = parse_address(opcode);
    const size_t   hdr_len = static_cast<size_t>(1 + addr_bytes_for_opcode(opcode));

    // Any bytes after opcode + address are write (program) data.
    std::vector<uint8_t> write_data;
    if (hdr_len < m_tx_accum.size()) {
        write_data.assign(m_tx_accum.begin() + static_cast<ptrdiff_t>(hdr_len),
                          m_tx_accum.end());
    }

    // Size the receive buffer for commands that return data (e.g. READ_STATUS).
    std::vector<uint8_t> rx_vec;
    if ((segment.direction == spi_direction_e::RX_ONLY ||
         segment.direction == spi_direction_e::BIDIR) && segment.len > 0) {
        rx_vec.resize(segment.len, 0x00);
    }

    const bool ok = m_model.process_command(opcode, address, rx_vec, write_data);
    copy_rx(rx_data, rx_vec, segment.len);

    m_tx_accum.clear();
    return ok;
}
