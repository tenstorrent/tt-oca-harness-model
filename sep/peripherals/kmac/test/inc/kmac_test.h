// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2021-2025 Tenstorrent USA, Inc.
/******************************************************************************
 * @file kmac_test.h
 * @brief KMAC test harness header with complementary ports
 *
 * This file defines the KMAC test harness class that provides complementary
 * port interfaces to connect with the KMAC model. Includes TLM initiator
 * socket for register access, KeyMgr sideload provider, application interface
 * stubs, and control signal drivers.
 *
 * @copyright Copyright (c) 2021-2025, Tenstorrent USA, Inc.
 ******************************************************************************/

#pragma once
#include "kmac_basetest.h"
#include "kmac_interface.h"
#include "reg_logger.h"
#include <iostream>
#include <cstring>

/******************************************************************************
 * @class kmac_keymgr_channel
 * @brief Channel implementing KeyMgr sideload interface
 *
 * Provides sideloaded key data to KMAC model for secure key delivery bypass
 * of MMIO register path.
 ******************************************************************************/
class kmac_keymgr_channel : public kmac_keymgr_if
{
public:
    kmac_keymgr_channel() : key_valid(false), share0_len(0), share1_len(0) {
        memset(key_share0, 0, sizeof(key_share0));
        memset(key_share1, 0, sizeof(key_share1));
    }

    /// @brief Set sideloaded key data (both shares)
    void set_key(const uint32_t* s0, const uint32_t* s1, size_t len_bytes) {
        if (len_bytes > sizeof(key_share0)) len_bytes = sizeof(key_share0);
        memcpy(key_share0, s0, len_bytes);
        memcpy(key_share1, s1, len_bytes);
        share0_len = len_bytes;
        share1_len = len_bytes;
        key_valid = true;
    }

    /// @brief Clear sideloaded key
    void clear_key() {
        key_valid = false;
        memset(key_share0, 0, sizeof(key_share0));
        memset(key_share1, 0, sizeof(key_share1));
    }

    // Interface implementation
    virtual bool is_key_valid() const { return key_valid; }
    virtual void get_key_share0(uint32_t* key, size_t& len_bytes) const {
        len_bytes = share0_len;
        memcpy(key, key_share0, len_bytes);
    }
    virtual void get_key_share1(uint32_t* key, size_t& len_bytes) const {
        len_bytes = share1_len;
        memcpy(key, key_share1, len_bytes);
    }

private:
    bool key_valid;
    uint32_t key_share0[8]; // 256 bits max
    uint32_t key_share1[8]; // 256 bits max
    size_t share0_len;
    size_t share1_len;
};



/******************************************************************************
 * @class kmac_test
 * @brief KMAC test harness with complementary ports
 *
 * Provides test infrastructure for KMAC model including TLM initiator socket,
 * interface channel implementations, and register access helper functions.
 ******************************************************************************/
class kmac_test : public kmac_basetest
{
public:
    SC_HAS_PROCESS(kmac_test);

    /**
     * @brief Constructor
     * @param name Module name
     * @param num_app_intf Number of application interfaces (must match model)
     */
    kmac_test(sc_module_name name, unsigned int num_app_intf = 3);

    /// @brief Destructor
    ~kmac_test();

    // =========================================================================
    // Port Interfaces (complementary to model)
    // =========================================================================

    // Note: No sc_ports needed - model's exports bind directly to our channels

    /// @brief Idle status input (monitors model's idle output)
    sc_in<bool> idle_i;

    /// @brief Life cycle escalation output (drives model's input)
    sc_out<bool> lc_escalate_en_o;

    /// @brief Active-low reset output (drives model's reset input)
    sc_out<bool> rst_no;

    /// @brief Primary clock output signal (drives model's input)
    sc_out<bool> clk_o;


    // =========================================================================
    // Interface Channels
    // =========================================================================

    /// @brief KeyMgr sideload TLM socket (pushes key to model's keymgr_tl_socket)
    tlm_utils::simple_initiator_socket<kmac_test> keymgr_socket;

    /// @brief Push sideload key to model via TLM
    void set_keymgr_key(const uint32_t* s0, const uint32_t* s1, size_t len_bytes);

    /// @brief Invalidate sideload key in model via TLM
    void clear_keymgr_key();

    /// @brief Read is rejected by the model (write-only sideload bus).
    void keymgr_read_word(uint64_t offset, uint32_t &value);

    /// @brief Application interface ports (array) - TLM pattern: port connects to model's export
    sc_port<kmac_app_if>* app_port;


    // =========================================================================
    // Register Access Helper Functions
    // =========================================================================

    /**
     * @brief Read 32-bit register
     * @param offset Register byte offset
     * @param read_value Reference to store read value
     */
    void register_read_32(unsigned int offset, uint32_t &read_value);

    /**
     * @brief Write 32-bit register
     * @param offset Register byte offset
     * @param write_value Value to write
     */
    void register_write_32(unsigned int offset, uint32_t write_value);

    /**
     * @brief Read 8-bit register
     * @param offset Register byte offset
     * @param read_value Reference to store read value
     */
    void register_read_8(unsigned int offset, uint8_t &read_value);

    /**
     * @brief Write 8-bit register
     * @param offset Register byte offset
     * @param write_value Value to write
     */
    void register_write_8(unsigned int offset, uint8_t write_value);

private:
    /// @brief Number of application interfaces
    const unsigned int NumAppIntf;

    /**
     * @brief Clock driver process
     */
    void clock_driver();

    /// @brief Logger instance for structured logging
    mutable RegLogger logger;

private:
    void keymgr_write_word(uint64_t offset, uint32_t value);
};