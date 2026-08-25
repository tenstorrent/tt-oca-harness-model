// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

/**
 * @file spi_flash.h
 * @brief SystemC wrapper for the SPI Flash device model
 *
 * This module wraps the pure-C++ spi_flash_model with a SystemC interface
 * so it can be connected directly to the OpenTitan SPI Controller model.
 *
 * Connection:
 *   spi_controller_ip::spi_master (sc_port<spi_if>)
 *       bound to
 *   spi_flash::spi_target (sc_export<spi_if>)
 *
 * The flash is NOT memory-mapped to the CPU bus — the CPU communicates with
 * it exclusively through the SPI Controller.  Consequently this module has
 * no TLM target socket and no CSML registers.
 *
 * Multi-segment transaction protocol
 * ------------------------------------
 * A typical SPI read consists of two segments issued by the controller:
 *   Segment 1  TX_ONLY  csaat=true  : [opcode, addr_msb, …, addr_lsb]
 *   Segment 2  RX_ONLY  csaat=false : [data bytes]
 *
 * A write (program / erase) may arrive as a single TX_ONLY segment or as
 * two TX segments (header then data), both with csaat=true except the last.
 * Control commands (WREN, WRDI, READ_STATUS) are a single segment with
 * csaat=false.
 *
 * This wrapper accumulates all TX bytes in m_tx_accum across csaat=true
 * segments.  spi_transaction() then classifies each segment and routes it:
 *   - a memory-read RX segment is served immediately from the running read
 *     address (reads stream data on every RX segment, so a chained multi-segment
 *     read is served in order rather than collapsing to its final segment);
 *   - any other command accumulates until csaat=false, then parses the bytes
 *     (opcode + optional address + optional write data) and dispatches once.
 *
 * LT model: all operations complete instantly (sc_time = SC_ZERO_TIME).
 */


#include "spi_flash_model.h"
#include "spi_controller_interface.h"
#include "csml_logger.h"
#include "csml_parameter.h"

#include <systemc.h>
#include <vector>
#include <cstdint>

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

/**
 * @class spi_flash
 * @brief SystemC TLM-LT SPI NOR flash device
 *
 * Inherits sc_module for SystemC integration and implements spi_if so that
 * it can be bound to the SPI controller's sc_port<spi_if> spi_master port.
 */
class spi_flash : public sc_module, public spi_if
{
public:
    // ----------------------------------------------------------------
    // Ports / exports
    // ----------------------------------------------------------------

    /** Export implementing spi_if — bind this to spi_controller::spi_master */
    sc_export<spi_if> spi_target;

    /** Active-low reset driven by the platform reset network */
    sc_in<bool> rst_ni;

    // ----------------------------------------------------------------
    // Constructor
    // ----------------------------------------------------------------

    SC_HAS_PROCESS(spi_flash);

    /**
     * @brief Constructor
     * @param name       SystemC module name
     * @param size_bytes Flash capacity in bytes (default 32 MB)
     */
    explicit spi_flash(sc_module_name name,
                       uint32_t size_bytes = spi_flash_model::DEFAULT_FLASH_SIZE,
                       int log_verbosity = CSML_DEFAULT_VERBOSITY);

    ~spi_flash() = default;

    // ----------------------------------------------------------------
    // spi_if implementation
    // ----------------------------------------------------------------

    /**
     * @brief Execute one SPI segment
     *
     * Called by the SPI controller once per segment.  TX bytes are
     * accumulated until csaat=false, at which point the full command
     * is parsed and dispatched to the flash model.
     *
     * @param segment  Segment descriptor (length, direction, speed, csaat, csid)
     * @param config   SPI bus configuration (clock divider, CS timing)
     * @param tx_data  Transmit buffer (non-null for TX_ONLY / BIDIR)
     * @param rx_data  Receive  buffer (non-null for RX_ONLY / BIDIR)
     * @return true on success, false on protocol error
     */
    bool spi_transaction(const spi_segment_t& segment,
                         const spi_config_t&  config,
                         const uint8_t*       tx_data,
                         uint8_t*             rx_data) override;

    // ----------------------------------------------------------------
    // Backdoor / inspection helpers
    // ----------------------------------------------------------------

    /** Direct access to the underlying flash model (testbench use only) */
    spi_flash_model*       get_model()       { return &m_model; }
    const spi_flash_model* get_model() const { return &m_model; }

    CsmlLogger          logger;             ///< CSML logger for diagnostics
    csml_param<int>     verbosity;  ///< Logging verbosity (runtime-overridable via ini)

private:
    // ----------------------------------------------------------------
    // Internal state
    // ----------------------------------------------------------------

    spi_flash_model      m_model;      ///< Pure-C++ flash behaviour
    std::vector<uint8_t> m_tx_accum;   ///< TX byte accumulator (across CSAAT segments)

    // Streaming-read state. A memory read is framed as one opcode+address TX
    // segment followed by one or more CSAAT-chained RX segments, each served in
    // order from an advancing address. Because the RX segments add no TX bytes,
    // the accumulated header (hence the base address) is stable for the whole
    // read, so a single "bytes served so far" counter is all the state needed:
    // each segment reads from base + m_read_bytes. Reset to 0 when CS is released
    // (or on rst_ni), readying the accumulator for the next command.
    uint32_t m_read_bytes = 0;         ///< Bytes already served in the in-progress read

    // ----------------------------------------------------------------
    // SystemC process
    // ----------------------------------------------------------------

    /** SC_METHOD: asserts reset when rst_ni goes low */
    void reset_method();

    // ----------------------------------------------------------------
    // Helper
    // ----------------------------------------------------------------

    /**
     * @brief Return address field length for a given SPI flash opcode
     * @return 4 for 4-byte-address opcodes, 3 for 3-byte-address opcodes,
     *         0 for no-address opcodes (control / suspend / resume)
     */
    int addr_bytes_for_opcode(uint8_t opcode) const;

    /**
     * @brief True if @p opcode is a memory-read opcode that streams data on every
     *        RX segment (READ / fast / dual / quad / 4-byte / SFDP). READ_STATUS is
     *        deliberately excluded — it dispatches once via dispatch_command().
     */
    bool is_memory_read_opcode(uint8_t opcode) const;

    /** Parse the big-endian address that follows the opcode in m_tx_accum. */
    uint32_t parse_address(uint8_t opcode) const;

    /** Copy up to @p len bytes of read data back to the caller (no-op if null). */
    void copy_rx(uint8_t* rx_data, const std::vector<uint8_t>& rx_vec, uint32_t len) const;

    /**
     * @brief Serve one RX segment of a memory read from the running address
     *        (base + bytes already served), advancing the counter. Resets the
     *        read state and clears the accumulator on the final (csaat=0) segment.
     */
    bool serve_read_segment(const spi_segment_t& segment, uint8_t* rx_data);

    /**
     * @brief Dispatch a completed non-streaming command (write / erase / control /
     *        status, or a bare RX with no opcode) once, from the accumulated bytes.
     */
    bool dispatch_command(const spi_segment_t& segment, uint8_t* rx_data);
};

