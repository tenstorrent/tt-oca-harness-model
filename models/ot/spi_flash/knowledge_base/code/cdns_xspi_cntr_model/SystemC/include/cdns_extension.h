/*
 * cdns_extension.h
 *
 *  Created on: Jan 21, 2026
 *      Author: ctr-sdangi
 */

#ifndef SYSTEMC_INCLUDE_CDNS_EXTENSION_H_
#define SYSTEMC_INCLUDE_CDNS_EXTENSION_H_



#include <systemc.h>
#include <tlm.h>
#include <vector>

#pragma once

class axi_trans : public tlm::tlm_extension<axi_trans> {
public:
    ~axi_trans() {
        wstrb.resize(0);
        user_data.resize(0);
    }

    // AXI ID signals
    uint32_t awid;      // Write Address ID
    uint32_t arid;      // Read Address ID
    uint32_t bid;       // Write Response ID
    uint32_t rid;       // Read Data ID

    // AXI Burst information
    typedef enum {
        AXI_BURST_FIXED = 0,   // Fixed address
        AXI_BURST_INCR = 1,     // Incrementing address
        AXI_BURST_WRAP = 2,     // Wrapping address
        AXI_BURST_RESERVED = 3
    } burst_type_t;

    typedef enum {
        AXI_SIZE_1BYTE = 0,
        AXI_SIZE_2BYTE = 1,
        AXI_SIZE_4BYTE = 2,
        AXI_SIZE_8BYTE = 3,
        AXI_SIZE_16BYTE = 4,
        AXI_SIZE_32BYTE = 5,
        AXI_SIZE_64BYTE = 6,
        AXI_SIZE_128BYTE = 7
    } burst_size_t;

    burst_type_t awburst;   // Write Address Burst Type
    burst_type_t arburst;   // Read Address Burst Type
    burst_size_t awsize;    // Write Address Burst Size
    burst_size_t arsize;    // Read Address Burst Size
    uint8_t awlen;          // Write Address Burst Length (AXI4: 0-255, AXI3: 0-15)
    uint8_t arlen;          // Read Address Burst Length (AXI4: 0-255, AXI3: 0-15)

    // AXI Lock signals
    typedef enum {
        AXI_LOCK_NORMAL = 0,
        AXI_LOCK_EXCLUSIVE = 1
    } lock_type_t;

    lock_type_t awlock;     // Write Address Lock Type
    lock_type_t arlock;     // Read Address Lock Type

    // AXI Cache signals (4 bits)
    uint8_t awcache;        // Write Address Cache Type
    uint8_t arcache;        // Read Address Cache Type

    // AXI Protection signals (3 bits)
    uint8_t awprot;         // Write Address Protection Type
    uint8_t arprot;         // Read Address Protection Type

    // AXI QoS signals (4 bits)
    uint8_t awqos;          // Write Address QoS
    uint8_t arqos;          // Read Address QoS

    // AXI Region signals (4 bits)
    uint8_t awregion;       // Write Address Region
    uint8_t arregion;       // Read Address Region

    // AXI Response signals
    typedef enum {
        AXI_RESP_OKAY = 0,      // Normal access success
        AXI_RESP_EXOKAY = 1,    // Exclusive access okay
        AXI_RESP_SLVERR = 2,    // Slave error
        AXI_RESP_DECERR = 3     // Decode error
    } resp_type_t;

    resp_type_t bresp;      // Write Response
    resp_type_t rresp;      // Read Response

    // AXI Write Data signals
    std::vector<uint8_t> wstrb;  // Write Strobe (one bit per byte)
    uint8_t wlast;               // Write Last
    uint8_t rlast;               // Read Last

    // AXI User signals (optional, configurable width)
    std::vector<uint8_t> awuser;  // Write Address User
    std::vector<uint8_t> wuser;   // Write Data User
    std::vector<uint8_t> buser;   // Write Response User
    std::vector<uint8_t> aruser;  // Read Address User
    std::vector<uint8_t> ruser;   // Read Data User

    // Generic user data storage
    std::vector<uint8_t> user_data;

    // Transaction type
    enum class trans_type {
        WRITE,
        READ
    };

    trans_type type;        // Transaction type

    // Clone method for deep copy
    virtual tlm_extension_base* clone() const override {
        // TODO: Implement proper logging for clone method not implemented
        return nullptr;
    }

    // Copy method
    virtual void copy_from(const tlm_extension_base& ext) override {
        // TODO: Implement proper logging for copy_from method not implemented
    }

    // Constructor with default initialization
    axi_trans() :
        awid(0),
        arid(0),
        bid(0),
        rid(0),
        awburst(AXI_BURST_INCR),
        arburst(AXI_BURST_INCR),
        awsize(AXI_SIZE_4BYTE),
        arsize(AXI_SIZE_4BYTE),
        awlen(0),
        arlen(0),
        awlock(AXI_LOCK_NORMAL),
        arlock(AXI_LOCK_NORMAL),
        awcache(0),
        arcache(0),
        awprot(0),
        arprot(0),
        awqos(0),
        arqos(0),
        awregion(0),
        arregion(0),
        bresp(AXI_RESP_OKAY),
        rresp(AXI_RESP_OKAY),
        wlast(0),
        rlast(0),
        type(trans_type::WRITE)
    {
    }

    // TODO: Add other AXI-specific fields based on user need and/or protocol version (AXI3/AXI4/AXI5)
};

class xspi_PoR_trans : public tlm::tlm_extension<xspi_PoR_trans> {
public:
    ~xspi_PoR_trans() {
    }

    // Boot Engine Signals
    uint8_t boot_en;           // Input: Enables the boot sequence
    uint8_t boot_comp;         // Output: Set after automatic boot process sequence has finished
    uint8_t boot_error;        // Output: Set when boot sequence is interrupted by errors

    // Device Discovery Engine Signals
    uint8_t discovery_inhibit; // Input: Disables automatic device discovery
    uint8_t discovery_num_lines; // Input: 4 bits - Bootstrap port to configure xSPI Protocol mode
                                // 0x0: Auto, 0x1: 1-1-1, 0x2: 2-2-2, 0x4: 4-4-4,
                                // 0x8: 8-8-8, 0xC: 8-8-8 Legacy HyperFlash/xSPI Profile 2.0,
                                // 0xE: 1-1-1 Legacy SPI-NAND
    uint8_t discovery_extop_val;  // Input: Extended opcode value (0: repetition, 1: negation)
    uint8_t discovery_extop_en;   // Input: Extended opcode enable (0: disabled, 1: enabled)
    uint8_t discovery_cmd_type;   // Input: 2 bits - Command type
                                  // 0: DDR mode disabled, 1: DDR mode enabled, 2: DTR mode enabled (QUAD only)
    uint8_t discovery_dummy_cnt;  // Input: Dummy clock cycles (0: 8 cycles, 1: 20 cycles)
    uint8_t discovery_abnum;      // Input: Addressing mode (0: 3-byte, 1: 4-byte)
    uint8_t discovery_bank;       // Input: 3 bits - Bank selection (0x0-0x7 for banks 0-7)
    uint8_t discovery_seq_crc_en;         // Input: Update seq_crc_en parameter
    uint8_t discovery_seq_crc_variant;    // Input: Update seq_crc_variant parameter
    uint8_t discovery_seq_crc_oe;         // Input: Update seq_crc_oe parameter
    uint8_t discovery_seq_crc_chunk_size; // Input: 3 bits - Update seq_crc_chunk_size parameter
    uint8_t discovery_seq_crc_ual_chunk_en; // Input: Update seq_crc_ual_chunk_en parameter

    // Initialization Signals
    uint32_t init_rb_valid_time;         // Input: 32 bits - Time from PoR when device becomes accessible
    uint32_t init_phy_dq_timing_reg;     // Input: 32 bits - PHY DQ timing register
    uint32_t init_phy_dqs_timing_reg;    // Input: 32 bits - PHY DQS timing register
    uint32_t init_phy_gate_lpbk_ctrl_reg; // Input: 32 bits - PHY gate loopback control register
    uint32_t init_phy_dll_master_ctrl_reg; // Input: 32 bits - PHY DLL master control register
    uint32_t init_dqs_last_data_drop_en; // Input: 32 bits - DQS last data drop enable
    uint32_t init_sdr_edge_active;       // Input: 32 bits - SDR edge active
    uint8_t init_wp_enable;              // Input: Initialize write protect enable bit
    uint8_t init_sw_ctrled_hw_rst_option; // Input: Initialize reset_pin_settings register option
    uint8_t init_rst_dq3_enable;         // Input: Initialize reset_pin_settings register for DQ3
    uint8_t init_comp;                   // Output: Initialization complete flag
    uint8_t init_fail;                   // Output: 2 bits - Initialization fail flag
                                         // 2'b00: xSPI or SPI-NAND device detected
                                         // 2'b01: Initialization failed
                                         // 2'b10: Legacy SPI device detected
                                         // 2'b11: n/a

    // Discovery Protocol Mode Enum
    typedef enum {
        DISCOVERY_MODE_AUTO = 0x0,
        DISCOVERY_MODE_1_1_1 = 0x1,
        DISCOVERY_MODE_2_2_2 = 0x2,
        DISCOVERY_MODE_4_4_4 = 0x4,
        DISCOVERY_MODE_8_8_8 = 0x8,
        DISCOVERY_MODE_8_8_8_LEGACY = 0xC,
        DISCOVERY_MODE_1_1_1_SPI_NAND = 0xE
    } discovery_mode_t;

    // Discovery Command Type Enum
    typedef enum {
        DISCOVERY_CMD_DDR_DISABLED = 0,
        DISCOVERY_CMD_DDR_ENABLED = 1,
        DISCOVERY_CMD_DTR_ENABLED = 2
    } discovery_cmd_type_t;

    // Initialization Fail Status Enum
    typedef enum {
        INIT_FAIL_XSPI_OR_SPI_NAND = 0,  // xSPI or SPI-NAND device detected
        INIT_FAIL_FAILED = 1,             // Initialization failed
        INIT_FAIL_LEGACY_SPI = 2,         // Legacy SPI device detected
        INIT_FAIL_RESERVED = 3            // n/a
    } init_fail_status_t;

    // Clone method for deep copy
    virtual tlm_extension_base* clone() const override {
        // TODO: Implement proper logging for clone method not implemented
        return nullptr;
    }

    // Copy method
    virtual void copy_from(const tlm_extension_base& ext) override {
        // TODO: Implement proper logging for copy_from method not implemented
    }

    // Constructor with default initialization
    xspi_PoR_trans() :
        boot_en(0),
        boot_comp(0),
        boot_error(0),
        discovery_inhibit(0),
        discovery_num_lines(0),
        discovery_extop_val(0),
        discovery_extop_en(0),
        discovery_cmd_type(0),
        discovery_dummy_cnt(0),
        discovery_abnum(0),
        discovery_bank(0),
        discovery_seq_crc_en(0),
        discovery_seq_crc_variant(0),
        discovery_seq_crc_oe(0),
        discovery_seq_crc_chunk_size(0),
        discovery_seq_crc_ual_chunk_en(0),
        init_rb_valid_time(0),
        init_phy_dq_timing_reg(0),
        init_phy_dqs_timing_reg(0),
        init_phy_gate_lpbk_ctrl_reg(0),
        init_phy_dll_master_ctrl_reg(0),
        init_dqs_last_data_drop_en(0),
        init_sdr_edge_active(0),
        init_wp_enable(0),
        init_sw_ctrled_hw_rst_option(0),
        init_rst_dq3_enable(0),
        init_comp(0),
        init_fail(0)
    {
    }

    // TODO: Add other xSPI controller-specific fields based on user need and/or protocol requirements
};






class xspi_target_trans : public tlm::tlm_extension<xspi_target_trans> {
public:
    ~xspi_target_trans() {
    }

    // Target Signals
    uint8_t xspi_target_opcode; // Input: XSPI target opcode

    // Clone method for deep copy
    virtual tlm_extension_base* clone() const override {
        // TODO: Implement proper logging for clone method not implemented
        return nullptr;
    }

    // Copy method
    virtual void copy_from(const tlm_extension_base& ext) override {
        // TODO: Implement proper logging for copy_from method not implemented
    }
};



#endif /* SYSTEMC_INCLUDE_CDNS_EXTENSION_H_ */
