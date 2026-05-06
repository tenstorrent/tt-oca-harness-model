/******************************************************************************
 * @file kmac_interface.h
 * @brief Abstract interface definitions for KMAC SystemC TLM custom ports
 *
 * This file defines the pure virtual interface classes for KMAC's custom
 * port interfaces: KeyMgr sideload, Application interfaces (KeyMgr, LC_CTRL,
 * ROM_CTRL). These abstracts enable
 * separation between model and test harness components.
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include <systemc.h>
#include <cstdint>
#include <cstddef>

/******************************************************************************
 * @class kmac_keymgr_if
 * @brief Abstract interface for Key Manager sideload key
 *
 * This interface provides hardware key sideload path from Key Manager to KMAC,
 * bypassing MMIO register access for enhanced security. Keys are provided in
 * two-share masked form (256 bits max per share).
 ******************************************************************************/
class kmac_keymgr_if : public sc_interface
{
public:
    /**
     * @brief Check if sideloaded key is valid and ready for use
     * @return true if key is valid, false otherwise
     */
    virtual bool is_key_valid() const = 0;

    /**
     * @brief Get key share 0 data
     * @param[out] key Pointer to buffer for key data (256 bits max)
     * @param[out] len_bytes Number of valid key bytes (up to 32 bytes)
     */
    virtual void get_key_share0(uint32_t* key, size_t& len_bytes) const = 0;

    /**
     * @brief Get key share 1 data
     * @param[out] key Pointer to buffer for key data (256 bits max)
     * @param[out] len_bytes Number of valid key bytes (up to 32 bytes)
     */
    virtual void get_key_share1(uint32_t* key, size_t& len_bytes) const = 0;
};

/******************************************************************************
 * @class kmac_app_if
 * @brief Abstract interface for hardware application request/response
 *
 * This interface supports hardware application modules (KeyMgr, LC_CTRL,
 * ROM_CTRL) to request hash operations and retrieve digest results. Each
 * application has pre-configured algorithm (KMAC or cSHAKE) and prefix values.
 ******************************************************************************/
class kmac_app_if : public sc_interface
{
public:
    /**
     * @brief Application initiates hash operation with data beat
     * @param data 64-bit data word
     * @param strobe Byte enable mask (bit 0 = byte 0 valid, etc.)
     * @param last True if this is the final data beat
     */
    virtual void app_request(uint64_t data, uint8_t strobe, bool last) = 0;

    /**
     * @brief Check if hash operation is complete
     * @return true if digest is ready, false if still processing
     */
    virtual bool is_done() const = 0;

    /**
     * @brief Get digest result in two-share form
     * @param[out] share0 Pointer to buffer for digest share 0 (256 bits = 8 words)
     * @param[out] share1 Pointer to buffer for digest share 1 (256 bits = 8 words)
     * @note If EnMasking=false, share1 will be all zeros
     */
    virtual void get_digest(uint32_t* share0, uint32_t* share1) const = 0;

    /**
     * @brief Check if error occurred during operation
     * @return true if error detected, false otherwise
     */
    virtual bool has_error() const = 0;
};

