/**
 * @file spi_controller_interface.h
 * @brief SPI Controller transaction-level interface definitions
 *
 * This header defines the TLM 2.0 transaction-level interfaces for SPI Controller:
 * - SPI transfer mode enumerations (Standard/Dual/Quad)
 * - SPI transfer direction enumerations (TX/RX/Bidirectional)
 * - SPI configuration structures
 * - spi_if - Master transaction interface
 */

#pragma once
#include <systemc.h>

/**
 * @enum spi_speed_e
 * @brief Enumeration for SPI transfer modes
 */
enum class spi_speed_e {
    STANDARD = 0,  ///< Single-bit data transfer
    DUAL = 1,      ///< 2-bit parallel data transfer
    QUAD = 2       ///< 4-bit parallel data transfer
};

/**
 * @enum spi_direction_e
 * @brief Enumeration for SPI transfer directions
 */
enum class spi_direction_e {
    DUMMY = 0,       ///< No data transfer, clock only
    RX_ONLY = 1,     ///< Receive only
    TX_ONLY = 2,     ///< Transmit only
    BIDIR = 3        ///< Bidirectional (full-duplex)
};

/**
 * @struct spi_config_t
 * @brief SPI configuration structure
 */
struct spi_config_t {
    uint16_t clkdiv;        ///< Clock divider (16-bit)
    uint8_t csnidle;        ///< CS idle time
    uint8_t csntrail;       ///< CS trail time
    uint8_t csnlead;        ///< CS lead time
};

/**
 * @struct spi_segment_t
 * @brief SPI segment command structure
 */
struct spi_segment_t {
    uint16_t len;                   ///< Segment length in bytes (0-511)
    spi_direction_e direction;       ///< Transfer direction
    spi_speed_e speed;               ///< Transfer speed mode
    bool csaat;                      ///< Chip Select Active After Transfer
    uint8_t csid;                    ///< Chip select ID
};

/**
 * @class spi_if
 * @brief SPI Master Transaction Interface
 */
///
/// This interface abstracts the physical SPI signals (SCK, CSB, SD) into
/// transaction-level method calls for Standard/Dual/Quad SPI operations.
class spi_if : public sc_interface
{
public:
    /**
     * @brief Execute SPI transaction segment
     * @param segment Command segment descriptor (length, direction, speed, CSAAT, CSID)
     * @param config SPI configuration (clock, timing, polarity, phase)
     * @param tx_data Transmit data buffer (for TX_ONLY and BIDIR)
     * @param rx_data Receive data buffer (for RX_ONLY and BIDIR)
     * @return true on success, false on error
     */
    virtual bool spi_transaction(const spi_segment_t& segment,
                                  const spi_config_t& config,
                                  const uint8_t* tx_data,
                                  uint8_t* rx_data) = 0;

    // virtual bool is_ready() const = 0;

    // virtual bool is_active() const = 0;
};
