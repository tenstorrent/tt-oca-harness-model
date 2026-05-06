/******************************************************************************
 * Copyright (c) 2025, Vayavya Labs Pvt. Ltd.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * CRNG Test Component - Enhanced Version
 * Comprehensive test infrastructure with register access and port management
 *******************************************************************************/

#pragma once

#include "crng_basetest.h"
#include <csml_logger.h>
#include <iomanip>

class crng_test : public crng_basetest {
public:
    // Complementary ports for model connection
    sc_signal<bool> interrupt_cmd_req_done;
    sc_signal<bool> interrupt_entropy_req;
    sc_signal<bool> interrupt_hw_inst_exc;
    sc_signal<bool> interrupt_fatal_err;

    // Control signals
    sc_signal<bool> otp_en_sw_app_read;
    sc_signal<bool> lc_hw_debug_en;

    // Clock and reset signals
    sc_signal<double> clk_signal;
    sc_signal<bool> rst_n_signal;

    // Entropy provider instance
    entropy_provider* m_entropy_provider;

    // Logger instance
    CsmlLogger m_logger;

    SC_HAS_PROCESS(crng_test);

    crng_test(sc_module_name name)
        : crng_basetest(name),
          interrupt_cmd_req_done("interrupt_cmd_req_done"),
          interrupt_entropy_req("interrupt_entropy_req"),
          interrupt_hw_inst_exc("interrupt_hw_inst_exc"),
          interrupt_fatal_err("interrupt_fatal_err"),
          otp_en_sw_app_read("otp_en_sw_app_read"),
          lc_hw_debug_en("lc_hw_debug_en"),
          clk_signal("clk_signal"),
          rst_n_signal("rst_n_signal"),
          m_entropy_provider(nullptr) {

        // Initialize control signals
        otp_en_sw_app_read.write(true);   // Enable internal state read
        lc_hw_debug_en.write(true);       // Enable debug access
        clk_signal.write(100e6);          // 100 MHz clock
        rst_n_signal.write(true);         // Deasserted reset

        // Configure logger
        m_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);

        // Create entropy provider
        m_entropy_provider = new entropy_provider("entropy_provider");

        CSML_INFO(0, m_logger) << "CRNG Test component initialized";
    }

    ~crng_test() {
        if (m_entropy_provider) {
            delete m_entropy_provider;
        }
    }

    // ========================================================================
    // Register Access Helper Functions (32-bit)
    // ========================================================================

    uint32_t register_read_32(unsigned int offset) {
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(10, SC_NS);
        uint32_t read_data = 0;

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        initiator_socket->b_transport(trans, delay);

        if (trans.is_response_ok()) {
            CSML_DEBUG(2, m_logger) << "Read from offset 0x" << std::hex << offset
                                    << ": 0x" << read_data << std::dec;
            return read_data;
        } else {
            CSML_ERROR(0, m_logger) << "Read failed at offset 0x" << std::hex << offset;
            return 0;
        }
    }

    void register_write_32(unsigned int offset, uint32_t write_value) {
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(10, SC_NS);

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        initiator_socket->b_transport(trans, delay);

        if (trans.is_response_ok()) {
            CSML_DEBUG(2, m_logger) << "Write to offset 0x" << std::hex << offset
                                    << ": 0x" << write_value << std::dec;
        } else {
            CSML_ERROR(0, m_logger) << "Write failed at offset 0x" << std::hex << offset;
        }
    }

    // ========================================================================
    // High-Level Register Access Methods
    // ========================================================================

    // Enable CRNG module
    void enable_crng(bool enable_sw_app = true, bool enable_fips = false) {
        uint32_t ctrl_val = 0;
        if (enable_sw_app) {
            ctrl_val |= (0x6 << 4);  // MUBI4_ENABLE for SW_APP_ENABLE
        }
        if (enable_fips) {
            ctrl_val |= (0x6 << 8);  // MUBI4_ENABLE for FIPS_FORCE_ENABLE
        }
        ctrl_val |= 0x6;  // MUBI4_ENABLE for ENABLE field

        register_write_32(CTRL_OFFSET, ctrl_val);
        wait(100, SC_NS);

        CSML_INFO(1, m_logger) << "CRNG module enabled (SW_APP=" << enable_sw_app
                               << ", FIPS=" << enable_fips << ")";
    }

    // Disable CRNG module
    void disable_crng() {
        register_write_32(CTRL_OFFSET, 0x999);  // All MUBI4_DISABLE
        wait(100, SC_NS);

        CSML_INFO(1, m_logger) << "CRNG module disabled";
    }

    // Write command request header
    void write_cmd_req(uint32_t acmd, uint32_t clen, uint32_t flag0, uint32_t glen) {
        uint32_t cmd_header = (acmd & 0xF) | ((clen & 0xF) << 4) |
                             ((flag0 & 0xF) << 8) | ((glen & 0xFFF) << 12);
        register_write_32(CMD_REQ_OFFSET, cmd_header);

        CSML_INFO(1, m_logger) << "CMD_REQ written: acmd=" << acmd << ", clen=" << clen
                               << ", flag0=" << flag0 << ", glen=" << glen;
    }

    // Write additional data for command
    void write_additional_data(const std::vector<uint32_t>& data) {
        for (uint32_t word : data) {
            register_write_32(CMD_REQ_OFFSET, word);
            wait(10, SC_NS);
        }

        CSML_INFO(1, m_logger) << "Additional data written (" << data.size() << " words)";
    }

    // Read command status
    uint32_t read_cmd_status() {
        return register_read_32(SW_CMD_STS_OFFSET) & 0x7;
    }

    // Read GENBITS valid status
    bool is_genbits_valid() {
        return (register_read_32(GENBITS_VLD_OFFSET) & 0x1) != 0;
    }

    // Read GENBITS data (128 bits as 4x32-bit words)
    void read_genbits(uint32_t data[4], bool& fips_compliant) {
        uint32_t vld = register_read_32(GENBITS_VLD_OFFSET);
        fips_compliant = (vld & 0x2) != 0;

        for (int i = 0; i < 4; i++) {
            data[i] = register_read_32(GENBITS_OFFSET);
            wait(10, SC_NS);
        }

        CSML_INFO(1, m_logger) << "GENBITS read: [0x" << std::hex << data[0] << ", 0x"
                               << data[1] << ", 0x" << data[2] << ", 0x" << data[3]
                               << "], FIPS=" << fips_compliant << std::dec;
    }

    // Read reseed counter for instance
    uint32_t read_reseed_counter(uint32_t instance) {
        uint32_t offset = RESEED_COUNTER_0_OFFSET + (instance * 4);
        return register_read_32(offset);
    }

    // Read error code
    uint32_t read_error_code() {
        return register_read_32(ERR_CODE_OFFSET);
    }

    // Read main FSM state
    uint32_t read_main_sm_state() {
        return register_read_32(MAIN_SM_STATE_OFFSET);
    }

    // Enable interrupts
    void enable_interrupts(uint32_t mask = 0xF) {
        register_write_32(INTR_ENABLE_OFFSET, mask);
        CSML_INFO(1, m_logger) << "Interrupts enabled: 0x" << std::hex << mask << std::dec;
    }

    // Read interrupt status
    uint32_t read_interrupt_status() {
        return register_read_32(INTR_STATE_OFFSET);
    }

    // Clear interrupts
    void clear_interrupts(uint32_t mask) {
        register_write_32(INTR_STATE_OFFSET, mask);
        CSML_INFO(1, m_logger) << "Interrupts cleared: 0x" << std::hex << mask << std::dec;
    }

    // ========================================================================
    // Reset Control
    // ========================================================================

    void assert_reset() {
        rst_n_signal.write(false);
        wait(100, SC_NS);
        CSML_INFO(1, m_logger) << "Reset asserted";
    }

    void deassert_reset() {
        rst_n_signal.write(true);
        wait(100, SC_NS);
        CSML_INFO(1, m_logger) << "Reset deasserted";
    }

    // ========================================================================
    // Test Verification Methods
    // ========================================================================

    bool verify_register_rw(unsigned int offset, uint32_t write_mask,
                           const std::string& reg_name) {
        // Read original value
        uint32_t original = register_read_32(offset);

        // Write test pattern
        uint32_t test_pattern = 0xA5A5A5A5 & write_mask;
        register_write_32(offset, test_pattern);
        wait(10, SC_NS);

        // Read back
        uint32_t readback = register_read_32(offset);
        bool pass = ((readback & write_mask) == test_pattern);

        if (pass) {
            CSML_INFO(0, m_logger) << "PASS: " << reg_name << " R/W test";
        } else {
            CSML_ERROR(0, m_logger) << "FAIL: " << reg_name << " R/W test - Expected: 0x"
                                    << std::hex << test_pattern << ", Got: 0x" << readback;
        }

        // Restore original value
        register_write_32(offset, original);

        return pass;
    }

    bool verify_register_ro(unsigned int offset, const std::string& reg_name) {
        // Read original value
        uint32_t original = register_read_32(offset);

        // Attempt to write
        register_write_32(offset, ~original);
        wait(10, SC_NS);

        // Read back - should be unchanged
        uint32_t readback = register_read_32(offset);
        bool pass = (readback == original);

        if (pass) {
            CSML_INFO(0, m_logger) << "PASS: " << reg_name << " read-only test";
        } else {
            CSML_ERROR(0, m_logger) << "FAIL: " << reg_name << " not read-only - "
                                    << "Original: 0x" << std::hex << original
                                    << ", After write: 0x" << readback;
        }

        return pass;
    }

    // Wait for interrupt with timeout
    bool wait_for_interrupt(sc_signal<bool>& interrupt_signal, sc_time timeout = sc_time(10, SC_MS)) {
        sc_time start_time = sc_time_stamp();
        while (!interrupt_signal.read()) {
            wait(1, SC_US);
            if ((sc_time_stamp() - start_time) > timeout) {
                CSML_ERROR(0, m_logger) << "Interrupt timeout";
                return false;
            }
        }

        CSML_INFO(1, m_logger) << "Interrupt received at " << sc_time_stamp();
        return true;
    }
};

