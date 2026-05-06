/******************************************************************************
 * @file testbench.h
 * @brief xspi_ctrl SystemC testbench header
 *
 * This file defines the top-level testbench SC_MODULE that instantiates the
 * xspi_ctrl model (DUT) and test harness (xspi_ctrl_test), performs complete
 * port binding between the two, and manages test execution.
 *
 * Port binding overview:
 *  - xspi_ctrl_test::initiator_socket → xspi_ctrl::t_reg_socket
 *    (CPU register access via scml2 target socket)
 *  - testbench::t_axi_slave_initiator  → xspi_ctrl::t_axi_slave_socket
 *    (AXI slave Direct/XIP memory interface)
 *  - testbench::por_initiator          → xspi_ctrl::PoR_input_signals
 *    (Power-on reset bootstrap)
 *  - xspi_ctrl::xspi_bus_socket[i]    → xspi_target_sc_module::target_socket
 *    (Per-CS flash bus stub targets)
 *  - xspi_ctrl::i_dma_socket          → testbench::dma_target_socket
 *    (AXI master DMA stub target)
 *  - xspi_ctrl::reset_in              ↔ rst_n_sig
 *  - xspi_ctrl::int_out               ↔ int_out_sig
 *
 * Reference:
 *   - docs/sections/xspi_ctrl-port-interfaces.md
 *   - kmac/test/inc/testbench.h (structural reference)
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#pragma once

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "../../model/inc/xspi_ctrl.h"
#include "xspi_target_sc_wrapper.h"
#include "xspi_ctrl_test.h"
#include "csml_logger.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdint>

/******************************************************************************
 * @class testbench
 * @brief Top-level xspi_ctrl testbench
 *
 * Instantiates the xspi_ctrl model and xspi_ctrl_test harness, performs full
 * port binding for all seven DUT interfaces, and drives the basic test
 * execution sequence (reset, register R/W, RO, reset verification).
 *
 * The testbench instantiates one stub flash target module per DUT CS line
 * (default four targets to match the reference DUT) plus one stub DMA
 * target socket. Stub handlers respond with TLM_OK_RESPONSE.
 ******************************************************************************/
class testbench : public sc_module
{
public:
    SC_HAS_PROCESS(testbench);

    /**
     * @brief Testbench constructor
     *
     * Instantiates DUT and test harness, allocates stub sockets, performs all
     * port bindings, initializes signals, and registers the run_tests
     * SC_THREAD.
     *
     * @param name        SystemC module name
     * @param num_targets Number of flash chip-select targets for the DUT
     *                    (must match xspi_ctrl NUM_TARGETS). The reference
     *                    simulation uses 4 to exercise multi-bank Direct mode.
     */
    testbench(sc_module_name name, int num_targets = 4);

    /// @brief Destructor — releases heap-allocated DUT, test, and socket objects
    ~testbench();

    /**
     * @brief Main test execution SC_THREAD
     *
     * Entry point for all test cases. Registered as SC_THREAD in the
     * constructor. Performs the following sequence:
     *  1. Apply reset to DUT
     *  2. Register Read/Write (RW) test
     *  3. Register Read-Only (RO) write-protection test
     *  4. Port binding verification (successful elaboration)
     *  5. Reset functionality test (write → reset → verify reset values)
     *  6. Print test summary
     *  7. Stop simulation
     */
    void run_tests();

    // =========================================================================
    // Test Result Reporting Helpers
    // =========================================================================

    /**
     * @brief Report test start with formatted banner
     * @param test_name Name of the test case being started
     */
    void report_test_start(const std::string& test_name);

    /**
     * @brief Record and report a passing test result
     * @param test_name Name of the test case that passed
     */
    void report_test_pass(const std::string& test_name);

    /**
     * @brief Record and report a failing test result
     * @param test_name Name of the test case that failed
     * @param reason    Optional failure reason string
     */
    void report_test_fail(const std::string& test_name,
                          const std::string& reason = "");

    /**
     * @brief Record test result and print pass/fail banner
     * @param test_name Name of the test case
     * @param passed    True if the test passed, false if it failed
     */
    void report_test_result(const char* test_name, bool passed);

    /**
     * @brief Print comprehensive test summary (totals, success rate, failures)
     */
    void report_test_summary();

private:
    // =========================================================================
    // Component Instances
    // =========================================================================

    /// @brief xspi_ctrl DUT model instance (xspi_ctrl_ip to avoid namespace clash)
    xspi_ctrl_ip* dut;

    /// @brief xspi_ctrl test harness instance
    xspi_ctrl_test* test;

    // =========================================================================
    // Stub Sockets for DUT Initiator Ports
    // =========================================================================

    /**
     * @brief TLM initiator socket for t_axi_slave_socket binding.
     *
     * Drives Direct-mode AXI slave transactions from the testbench to the
     * DUT's t_axi_slave_socket. Used in XIP and Direct-mode test cases.
     * Connected: testbench t_axi_slave_initiator → dut t_axi_slave_socket.
     */
    tlm_utils::simple_initiator_socket<testbench, 64> t_axi_slave_initiator;

    /**
     * @brief TLM initiator socket for PoR_input_signals binding.
     *
     * Sends the power-on reset xspi_PoR_trans transaction from the testbench
     * to the DUT's PoR_input_signals socket at simulation start.
     * Connected: testbench por_initiator → dut PoR_input_signals.
     */
    tlm_utils::simple_initiator_socket<testbench, 64> por_initiator;

    /**
     * @brief TLM xSPI flash target (wrapper + xspi_target_model) per CS.
     *
     * One module per flash chip-select: dut->xspi_bus_socket[i] binds to
     * xspi_flash_target[i]->target_socket.
     * flash_trans_prelude supplies test-harness side effects (transaction count,
     * extension capture, synthetic SFDP, error injection) before the device model.
     */
    std::vector<xspi_target_sc_module*> xspi_flash_target;

    /**
     * @brief Stub target socket for i_dma_socket binding.
     *
     * Bound to the DUT's i_dma_socket initiator socket. Stub handler returns
     * TLM_OK_RESPONSE to allow DUT AXI master DMA transactions to complete
     * without a physical memory model.
     */
    tlm_utils::simple_target_socket<testbench, 64> dma_target_socket;

    // =========================================================================
    // Interconnect Signals
    // =========================================================================

    /**
     * @brief Active-low reset signal.
     *
     * Driven by the testbench apply_reset() helper. Connected to
     * dut::reset_in. Initial value: true (reset deasserted).
     */
    sc_signal<bool> rst_n_sig;

    /**
     * @brief Interrupt output signal.
     *
     * Driven by dut::int_out. Monitored by testbench interrupt test cases.
     * Initial value: false (no interrupt pending).
     */
    sc_signal<bool> int_out_sig;

    // =========================================================================
    // Test Statistics
    // =========================================================================

    /// @brief Total number of tests executed
    int m_tests_run;

    /// @brief Number of tests that passed
    int m_tests_passed;

    /// @brief Number of tests that failed
    int m_tests_failed;

    /// @brief List of names of failed tests for summary reporting
    std::vector<std::string> m_failed_tests;

    /// @brief Number of flash targets (copy of constructor parameter)
    int m_num_targets;

    /**
     * @brief DMA stub error injection counter.
     *
     * When non-zero, the b_transport_dma_stub handler returns
     * TLM_GENERIC_ERROR_RESPONSE instead of TLM_OK_RESPONSE for the next
     * m_dma_error_count transactions. Decremented after each error response.
     * Used by FUNC_XSPI_003 tests to trigger the ACMD DMA bus error path
     * (which sets intr_status.cdma_terr and trd_error_intr_status).
     */
    int m_dma_error_count;

    /**
     * @brief Flash stub error injection counter.
     *
     * When non-zero, the flash TLM path returns
     * TLM_GENERIC_ERROR_RESPONSE instead of TLM_OK_RESPONSE for the next
     * m_flash_error_count transactions. Decremented after each error response.
     * Used by FUNC_XSPI_010 TC_XSPI_BOOT_002 to trigger the boot DQS error
     * path (which sets boot_status.boot_dqs_err and boot_error=1 in the
     * xspi_PoR_trans extension returned to the caller).
     *
     * Must be reset to 0 by the test after the PoR transaction completes to
     * prevent contaminating subsequent tests.
     */
    int m_flash_error_count;

    /**
     * @brief Flash stub transaction counter.
     *
     * Incremented on each DUT-to-flash b_transport. Reset to
     * zero by FUNC_XSPI_007 tests before a PoR transaction to measure how
     * many READ_SFDP transactions the DUT issues on xspi_bus_socket[].
     * Used by TC_XSPI_POR_002 (confirm zero for inhibited path) and
     * TC_XSPI_POR_003 (confirm non-zero for active discovery path).
     */
    int m_flash_stub_tx_count;

    /**
     * @brief Per-xspi_bus_socket flash transaction counts (index = CS / bank).
     *
     * Incremented in flash_trans_prelude() for the target that received the
     * b_transport. Used to confirm traffic did not use the wrong CS (e.g. bank=2
     * must not increment index 0).
     */
    std::vector<int> m_flash_tx_per_target;

    /**
     * @brief SFDP-mode flag for flash stub (prelude short-circuit).
     *
     * When true, flash_trans_prelude() copies m_flash_sfdp_buf[] into the READ
     * data and does not forward to xspi_target_model. TC_XSPI_POR_005 does not
     * use this; it programs sfdp_rom on xspi_target_model via flash_model() so
     * READ_SFDP is handled by the connected flash TLM target.
     */
    bool m_flash_sfdp_mode;

    /**
     * @brief Buffer for m_flash_sfdp_mode injection (prelude only).
     */
    uint8_t m_flash_sfdp_buf[256];


      /**
     * When true, the next flash READ in b_transport_flash_stub() reads
     * ctrl_status(0x100) while the DUT is still in Direct-mode flash dispatch
     * (ctrl_busy asserted). TC_XSPI_DM_005. Cleared after one probe.
     */
    bool m_probe_ctrl_busy_on_next_flash_read;
    bool m_ctrl_busy_seen_during_flash_read;
      
    /**
     * @brief Most recently captured cdns_extension from the flash stub.
     *
     * Set by flash_trans_prelude() each time the DUT issues a transaction
     * on xspi_bus_socket[]. FUNC_XSPI_008 test cases read this field after
     * issuing an AXI slave transaction to verify that b_transport_axi_slave()
     * correctly populated and forwarded the cdns_extension to the flash bus.
     *
     * Fields verified by Direct-mode tests:
     *   m_last_flash_ext.opcode     — READ_ZERO_LATENCY (0x03) or PAGE_PROGRAM (0x02)
     *   m_last_flash_ext.bank_num   — matches dac_bank_num configuration
     *   m_last_flash_ext.address    — matches expected flash address (post-remap)
     *   m_last_flash_ext.instr_type — XSPI_INSTR_READ(1)/WRITE(2)/GENERIC(0)
     *   m_last_flash_ext.data_bytes — matches AXI payload data length
     *
     * Reset by tests calling m_last_flash_ext = cdns_extension() before issuing
     * stimulus.
     *
     * Reference: docs/xspi_ctrl-functionality-testcases.md FUNC_XSPI_008.
     */
    cdns_extension m_last_flash_ext;

    /**
     * @brief DMA descriptor buffer for ACMD test descriptor injection.
     *
     * FUNC_XSPI_012 ACMD tests must supply 64-byte descriptor structures that
     * the model fetches via i_dma_socket READ transactions. Without this buffer
     * the DMA stub would return zero-filled data, causing the model to decode
     * cmd_type=0x0000 (NOP) and skip all flash dispatch paths.
     *
     * When m_dma_buf_armed is true, b_transport_dma_stub() matches the
     * incoming READ address against m_dma_buf_addr. If the address falls within
     * [m_dma_buf_addr, m_dma_buf_addr + m_dma_buf_size) the stub copies bytes
     * from m_dma_buf[] at the appropriate offset into the transaction data
     * pointer instead of zero-filling. Writes targeting the same range are
     * reflected back into m_dma_buf[] so that status-writeback verification
     * works without a real memory model.
     *
     * Usage:
     *   1. Populate m_dma_buf with the 64-byte (or longer for chaining) descriptor.
     *   2. Set m_dma_buf_addr to the base address of the descriptor.
     *   3. Set m_dma_buf_size to the number of valid bytes in m_dma_buf.
     *   4. Set m_dma_buf_armed = true.
     *   5. Issue the ACMD trigger.
     *   6. After the trigger returns, read status bytes back from m_dma_buf[40..43].
     *   7. Reset m_dma_buf_armed = false before the next test.
     *
     * Buffer capacity: 512 bytes — sufficient for two chained 64-byte
     * descriptors plus padding.
     */
    static constexpr std::size_t DMA_BUF_CAPACITY = 512u;
    uint8_t  m_dma_buf[DMA_BUF_CAPACITY];   ///< Descriptor content buffer
    uint64_t m_dma_buf_addr;                 ///< Base address the buffer is mapped to
    std::size_t m_dma_buf_size;              ///< Number of valid bytes in m_dma_buf
    bool     m_dma_buf_armed;                ///< True when descriptor buffer is active

    struct dma_trace_entry_t {
        uint64_t addr;
        uint32_t len;
        bool     is_read;
    };
    std::vector<dma_trace_entry_t> m_dma_trace;
    bool                           m_dma_trace_enabled;
    

      struct flash_trace_entry_t {
        uint8_t  opcode;
        uint64_t address;
        uint32_t data_bytes;
        uint8_t  bank_num;
    };
    std::vector<flash_trace_entry_t> m_flash_trace;
    bool    m_flash_trace_enabled;

    /**
     * @brief Socket index of the most recently captured flash transaction.
     *
     * Records which xspi_bus_socket[] element was used for the last transaction
     * captured by flash_trans_prelude(). Used by TC_XSPI_DM_003 to verify
     * that dac_bank_num correctly routes to the expected socket index.
     *
     * Reset to -1 by tests before issuing stimulus (indicates no transaction
     * seen yet). Set to the socket index (0–NUM_TARGETS-1) by the stub handler.
     *
     * The prelude callback is registered with the target index; the model
     * also populates cdns_extension.bank_num on the transaction.
     */
    int m_last_flash_bank;

    /**
     * @brief Set in flash_trans_prelude() when a READ_SFDP (opcode 0x5A) is
     *        observed with SFDP ROM address 0 and bank 0.
     *
     * Cleared by the test before arming SFDP discovery. Used by
     * TC_XSPI_POR_001_sfdp_discovery_full to assert the header read was issued
     * on xspi_bus_socket[0] (m_last_flash_ext would otherwise reflect only the
     * last table read, not address 0).
     */
    bool m_seen_read_sfdp_addr0;

    // =========================================================================
    // Test Case Methods
    // =========================================================================

    /**
     * @brief RW register test: write, read back, assert match
     *
     * Writes a pattern to ctrl_config (a known RW register), reads back the
     * value, and asserts that the read-back matches the written value. Tests
     * the basic read/write register access path through t_reg_socket.
     *
     * Pass criterion: read_value == write_pattern (within writable bit mask).
     */
    void test_rw_register_access();

    /**
     * @brief RO register test: write, read back, assert mismatch
     *
     * Writes a non-zero pattern to xspi_ctrl_version (a known RO register),
     * reads back the value, and asserts that the read-back retains the
     * hardware reset value (write was silently discarded). Tests the
     * write-ignore enforcement path.
     *
     * Pass criterion: read_value == xspi_ctrl_version_RESET (write ignored).
     */
    void test_ro_register_protection();

    /**
     * @brief Port binding verification test
     *
     * Verifies that all seven DUT port interfaces are successfully bound by
     * confirming that the simulation reached this point without elaboration
     * errors. Also confirms that writing to and reading from a register via
     * the bound initiator_socket returns TLM_OK_RESPONSE.
     *
     * Pass criterion: elaboration succeeded and socket responds correctly.
     */
    void test_port_binding_verification();

    /**
     * @brief Reset functionality test: write values, toggle reset, verify
     *
     * Writes non-reset values to several RW registers (ctrl_config,
     * long_polling, wp_settings), then asserts reset_in (active-low) and
     * deasserts it, then reads back the registers and verifies they returned
     * to their documented hardware reset values.
     *
     * Pass criterion: all registers read back at reset values after reset.
     */
    void test_reset_functionality();


    /**
     * @brief TC_XSPI_REG_013: Reserved bits ignore writes (RES0 after 0xFFFFFFFF)
     *
     * For registers with a partial write mask, writes all ones and asserts
     * bits outside the writable mask read back as zero (hardware RES0).
     *
     * Pass criterion: (read_value & ~write_mask) == 0 for each covered reg.
     */
    void test_reserved_bits_write_ignore();


    // =========================================================================
    // FUNC_XSPI_001 Test Case Methods
    // Implemented in: xspi_ctrl_func001_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_REG_001: Reset values of ctrl_cmd_stat_a registers
     *
     * Reads all 18 registers in the ctrl_cmd_stat_a group (0x000–0x158) after
     * reset_in de-assertion and asserts each equals its documented RESET value.
     * All registers in this group have reset value 0x00000000.
     *
     * Pass criterion: every register == its Register_Reset_Val enum entry.
     */
    void tc_xspi_reg_001_reset_values_ctrl_cmd_stat();

    /**
     * @brief TC_XSPI_REG_002: Reset values of ctrl_cfg_common_a registers
     *
     * Reads all 9 registers in ctrl_cfg_common_a (0x208–0x260) after reset and
     * asserts each matches its RESET value. Specifically validates non-zero
     * resets: long_polling=0x3E8, short_polling=0x1F4, dma_settings=0x000D0000.
     *
     * Pass criterion: every register == its Register_Reset_Val enum entry.
     */
    void tc_xspi_reg_002_reset_values_ctrl_cfg_common();

    /**
     * @brief TC_XSPI_REG_003: ctrl_consts_a constant RO values; immunity to reset_in
     *
     * Verifies xspi_ctrl_version (0xF00) reads 0x65220206 (magic 0x6522, fix
     * 0x02, rev 0x06) and ctrl_features_reg (0xF04) reads 0x03710003 (n_banks=3/8
     * banks, sfr_intf=1/APB, dma_data_width=1/64-bit, dma_addr_width=1/64-bit,
     * boot_available=1, n_threads=3/8 threads) after reset, and that those
     * scml-initialized read-only values are unchanged by a second \c reset_in
     * (unaffected by PoR-style reset handling in the rest of the register file).
     *
     * Pass criterion: values match the specification before and after the second
     * reset, and (for verbosity) per-field sub-checks for version and feature bits.
     */
    void tc_xspi_reg_003_reset_values_ctrl_consts();

    /**
     * @brief TC_XSPI_REG_004: RO write-ignore on ctrl_status, trd_status,
     *        dma_target_error_l, dma_target_error_h
     *
     * Writes 0xFFFFFFFF to four status/error registers (write_mask = 0x0) and
     * reads back to confirm the value remains 0x00000000. Validates that the
     * scml2 set_write_ignore_restriction prevents modification of RO registers.
     *
     * Pass criterion: all four registers read 0x00000000 after rogue write.
     */
    void tc_xspi_reg_004_ro_write_ignore_ctrl_status();

    /**
     * @brief TC_XSPI_REG_005: RO write-ignore on xspi_ctrl_version and
     *        ctrl_features_reg
     *
     * Writes 0xDEADBEEF to both capability registers in ctrl_consts_a and reads
     * back to confirm each retains its RESET value (write was ignored by scml2).
     *
     * Pass criterion: both registers read their RESET values after rogue write.
     */
    void tc_xspi_reg_005_ro_write_ignore_ctrl_consts();

    /**
     * @brief TC_XSPI_REG_009: RW retention for long_polling and short_polling
     *
     * Verifies reset values (0x3E8, 0x1F4), then writes new values (0x64, 0x32)
     * and reads back to confirm retention within the 16-bit writable mask.
     *
     * Pass criterion: reset values correct; new values retained after write.
     */
    void tc_xspi_reg_009_rw_long_short_polling();

    /**
     * @brief TC_XSPI_REG_010: xip_mode_cfg (0x388) reset default and RW retention
     *
     * Confirms reset value 0x00FF0000 (xip_dis_mb_val=0xFF in bits[23:16]),
     * then writes 0x00A5B400 and verifies retention within the 24-bit mask.
     *
     * Pass criterion: reset value correct; write value retained.
     */
    void tc_xspi_reg_010_xip_mode_cfg_reset_default();

    /**
     * @brief TC_XSPI_REG_011: PHY register store-and-acknowledge behavior
     *
     * Writes distinctive patterns to phy_dq_timing_reg (0x2000),
     * phy_ctrl_reg (0x2080), and phy_tsel_reg (0x2084). Reads back and asserts
     * each returned the written value exactly. Validates pure storage behavior
     * with no flash side effects (LT model assumption for PHY registers).
     *
     * Pass criterion: all three registers read back the written pattern.
     */
    void tc_xspi_reg_011_phy_reg_store_only();

    /**
     * @brief TC_XSPI_REG_012: RO enforcement on phy_gpio_status_0 and
     *        phy_gpio_status_1
     *
     * Writes 0xFFFFFFFF to both GPIO status registers (write_mask = 0x0)
     * and confirms reads return 0x00000000 (reset value, write ignored).
     *
     * Pass criterion: both registers read 0x00000000 after rogue write.
     */
    void tc_xspi_reg_012_phy_gpio_status_ro();

    /**
     * @brief run_func001_tests: top-level entry point for FUNC_XSPI_001 suite
     *
     * Orchestrates all 9 test cases for FUNC_XSPI_001 in document order.
     * Called from run_tests() after the mandatory baseline tests complete.
     * Begins with apply_reset() to guarantee clean entry state for the suite.
     */
    
    void run_func001_tests();

    // =========================================================================
    // FUNC_XSPI_002 Test Case Methods
    // Implemented in: xspi_ctrl_func002_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_BUS_001: Direct mode — cmd_reg0 write is a no-op.
     *
     * Verifies that in Direct mode (work_mode=2'b00) writing cmd_reg0 causes no
     * interrupt, no ctrl_busy/trd_busy assertion, and no cmd_ignored.  The
     * Direct-mode flash path is exclusively through t_axi_slave_socket.
     *
     * Pass criterion: intr_status == 0, ctrl_status == 0, trd_status == 0.
     */
    void tc_xspi_bus_001_direct_mode_cmd_reg0_noop();

    /**
     * @brief TC_XSPI_BUS_002: STIG mode — cmd_reg0 fires STIG engine.
     *
     * Sets work_mode=2'b01 (STIG), writes cmd_reg0, waits for the STIG SC_THREAD to
     * complete the flash bus dispatch.  Verifies intr_status.stig_done (bit 23)
     * is set and ctrl_busy / gcmd_eng_busy are cleared.
     *
     * Pass criterion: bit 23 of intr_status set; bits 3,7 of ctrl_status clear.
     */
    void tc_xspi_bus_002_stig_mode_engine_dispatch();

    /**
     * @brief TC_XSPI_BUS_003: STIG busy guard — second trigger sets cmd_ignored.
     *
     * Writes cmd_reg0 twice in rapid succession in STIG mode; the second write
     * finds gcmd_eng_busy already set and must raise intr_status.cmd_ignored
     * (bit 20) without launching a second STIG transaction.
     *
     * Pass criterion: intr_status.cmd_ignored (bit 20) is set.
     */
    void tc_xspi_bus_003_stig_busy_guard_cmd_ignored();

    /**
     * @brief TC_XSPI_BUS_004: PIO mode CHIP_ERASE — busy/idle lifecycle.
     *
     * Issues PIO CHIP_ERASE (CMD_TYPE=0x1001) on thread 0, bank 0, INT=0.
     * Verifies ctrl_busy and trd_busy[0] are cleared after synchronous dispatch
     * and cmd_ignored is not set.
     *
     * Pass criterion: ctrl_status.ctrl_busy=0, trd_status[0]=0, cmd_ignored=0.
     */
    void tc_xspi_bus_004_pio_chip_erase_dispatch();

    /**
     * @brief TC_XSPI_BUS_005: PIO INT flag — trd_comp_intr_status set on completion.
     *
     * Issues PIO CHIP_ERASE with INT=1 (bit 18 of cmd_reg0) on thread 0.
     * Verifies trd_comp_intr_status bit 0 is set after dispatch and trd_busy[0]
     * is cleared.
     *
     * Pass criterion: trd_comp_intr_status bit 0 set; trd_status[0] cleared.
     */
    void tc_xspi_bus_005_pio_int_flag_completion_interrupt();

    /**
     * @brief TC_XSPI_BUS_006: PIO SECTOR_ERASE — address/count registers decoded.
     *
     * Stages cmd_reg1 (flash address) and cmd_reg4 (SECT_CNT), issues PIO
     * SECTOR_ERASE (CMD_TYPE=0x1000).  Verifies ctrl_busy and trd_busy[0]
     * cleared and cmd_ignored not set (dispatch succeeded).
     *
     * Pass criterion: ctrl_busy=0, trd_busy[0]=0, cmd_ignored=0.
     */
    void tc_xspi_bus_006_pio_sector_erase_address_decode();

    /**
     * @brief TC_XSPI_BUS_007: ACMD mode — descriptor fetch and completion interrupt.
     *
     * Sets work_mode=2'b11 (ACMD global), stages cmd_reg2/cmd_reg3 descriptor address
     * (64-byte aligned), writes cmd_reg0 to trigger cdma_handle_trigger().
     * Verifies trd_comp_intr_status bit 0 set, ctrl_busy and acmd_eng_busy
     * cleared, cmd_ignored not set.
     *
     * Pass criterion: trd_comp[0] set; busy bits clear; cmd_ignored=0.
     */
    void tc_xspi_bus_007_acmd_mode_descriptor_completion();

    /**
     * @brief TC_XSPI_BUS_008: wp_settings — shadow variable update.
     *
     * Verifies handle_write_wp_settings() stores the written value and updates
     * the wp_pin_level / wp_enabled shadow variables.  Checks reset value
     * read-back, clear (0x00), and set (0x03) round-trips.
     *
     * Pass criterion: all three read-back values match within writable mask.
     */
    void tc_xspi_bus_008_wp_settings_shadow_update();

    /**
     * @brief TC_XSPI_BUS_009: reset_pin_settings — shadow variable update.
     *
     * Verifies handle_write_reset_pin_settings() updates the hw_rst_level
     * shadow variable.  Checks reset value read-back and clear (0x00) round-trip.
     *
     * Pass criterion: reset value correct; clear value retained.
     */
    void tc_xspi_bus_009_reset_pin_settings_shadow_update();

    /**
     * @brief TC_XSPI_BUS_010: clock_mode_settings — shadow variable update.
     *
     * Verifies handle_write_clock_mode_settings() updates the spi_clk_mode
     * shadow variable (bit[0]: 0=Mode 0, 1=Mode 3).  Checks reset, write
     * Mode 3 (0x01), and revert to Mode 0 (0x00) round-trips.
     *
     * Pass criterion: all three read-back values match within writable mask.
     */
    void tc_xspi_bus_010_clock_mode_settings_shadow_update();

    void tc_xspi_reg_013_ro_write_ignore_all_pure_ro();

    void tc_xspi_rw_all_registers_from_regmap();

    /**
     * @brief run_func002_tests: top-level entry point for FUNC_XSPI_002 suite.
     *
     * Orchestrates all 10 test cases for FUNC_XSPI_002 (Bus Transaction Engine
     * and Mode Dispatch) in document order.  Called from run_tests() after
     * run_func001_tests() completes.
     */
    
    void run_func002_tests();

    // =========================================================================
    // FUNC_XSPI_003 Test Case Methods
    // Implemented in: xspi_ctrl_func003_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_INT_PATH2A_001: Path 2a — trd_comp_intr_status asserts int_out.
     *
     * Issues a PIO CHIP_ERASE on thread 0 with INT=1 (bit 18 of cmd_reg0).
     * Verifies trd_comp_intr_status[0]=1 after dispatch and int_out=true
     * (Path 2a: intr_en + trd_comp_intr_status non-zero).
     *
     * Pass criterion: trd_comp_intr_status[0]=1; int_out_sig=true.
     */
    void tc_xspi_int_path2a_001_trd_comp_asserts_int_out();

    /**
     * @brief TC_XSPI_INT_W1C_001: W1C clear of trd_comp_intr_status deasserts int_out.
     *
     * With trd_comp_intr_status[0]=1 and int_out=true (from Path 2a),
     * writes 0x00000001 (W1C mask) to trd_comp_intr_status. Verifies that
     * trd_comp_intr_status becomes 0x00000000 and int_out deasserts.
     *
     * Pass criterion: trd_comp_intr_status=0x00; int_out_sig=false.
     */
    void tc_xspi_int_w1c_001_trd_comp_w1c_deasserts_int_out();

    /**
     * @brief TC_XSPI_INT_W1C_002: W1C write-0-no-effect on trd_comp_intr_status.
     *
     * With trd_comp_intr_status[0]=1 (set by PIO INT=1), writes 0x00000000
     * to trd_comp_intr_status. Verifies the bit is NOT cleared (write-0 has
     * no effect in W1C semantics). int_out must remain asserted.
     *
     * Pass criterion: trd_comp_intr_status[0]=1 unchanged; int_out=true.
     */
    void tc_xspi_int_w1c_002_trd_comp_write_zero_no_effect();

    /**
     * @brief TC_XSPI_INT_PATH1_001: Path 1 — intr_enable masks intr_status → int_out.
     *
     * Triggers STIG busy guard to set intr_status.cmd_ignored (bit 20) via
     * two rapid cmd_reg0 writes in STIG mode. With intr_enable=0x00000000
     * (all disabled), verifies int_out=false despite intr_status non-zero.
     * Then writes intr_enable to enable cmd_ignored (bit 20) with global
     * gate (bit 31). Verifies int_out=true (Path 1 active).
     *
     * Pass criterion: int_out=false before enable write; int_out=true after.
     */
    void tc_xspi_int_path1_001_intr_enable_gates_intr_status();

    /**
     * @brief TC_XSPI_INT_PATH1_W1C_001: W1C clear of intr_status deasserts int_out.
     *
     * With intr_status.cmd_ignored set (bit 20) and int_out=true, writes
     * 0x00100000 to intr_status (W1C mask for bit 20). Verifies intr_status
     * bit 20 clears and int_out deasserts (Path 1 inactive).
     *
     * Pass criterion: intr_status[20]=0; int_out=false.
     */
    void tc_xspi_int_path1_w1c_001_intr_status_w1c_deasserts();

    /**
     * @brief TC_XSPI_INT_PATH2B_001: Path 2b — trd_error_intr_en gates trd_error → int_out.
     *
     * Triggers ACMD DMA error (by forcing DMA stub to return TLM_ERROR_RESPONSE
     * on the descriptor fetch), which causes the model to set
     * intr_status.cdma_terr (bit 17). Then directly manipulates
     * trd_error_intr_status and trd_error_intr_en to verify Path 2b
     * enable/disable masking. With trd_error_intr_en[0]=0 verifies int_out
     * is not asserted; with trd_error_intr_en[0]=1 verifies int_out asserts.
     *
     * Pass criterion: int_out=false when trd_error_intr_en[0]=0;
     *                 int_out=true  when trd_error_intr_en[0]=1.
     *
     * Note: trd_error_intr_status[0] is set via ACMD DMA error injection.
     * The testbench DMA stub is transiently configured to return error.
     */
    void tc_xspi_int_path2b_001_trd_error_intr_en_masking();

    /**
     * @brief TC_XSPI_INT_CMD_STATUS_PTR_001: cmd_status_ptr selection.
     *
     * Writes cmd_status_ptr = 0 (select thread 0), then reads cmd_status.
     * After a PIO INT=1 dispatch on thread 0, cmd_status bit 0 must read 1
     * (comp flag for thread 0). Then writes cmd_status_ptr = 1 (select
     * thread 1), reads cmd_status — bit 0 must read 0 (thread 1 not done).
     *
     * Pass criterion: cmd_status[0]=1 for thread 0; cmd_status[0]=0 for
     *                 thread 1 (no completion event on thread 1).
     */
    void tc_xspi_int_cmd_status_ptr_001_thread_selection();

    /**
     * @brief TC_XSPI_INT_CMD_STATUS_PTR_002: cmd_status_ptr bit mask [2:0].
     *
     * Verifies that writing cmd_status_ptr with values > 7 are masked to
     * bits[2:0] by the write_bit_mask=0x7 restriction. Writes
     * cmd_status_ptr = 0x0000001F (should mask to 0x7) and reads back
     * cmd_status_ptr; verifies stored value = 0x7 (write_mask applied).
     *
     * Pass criterion: cmd_status_ptr read-back = 0x00000007.
     */
    void tc_xspi_int_cmd_status_ptr_002_bit_mask_enforcement();

    /**
     * @brief TC_XSPI_INT_RST_001: reset clears all interrupt status registers.
     *
     * With trd_comp_intr_status and intr_status both non-zero and int_out
     * asserted, applies reset_in. Verifies all three interrupt status
     * registers return to 0x00000000 and int_out deasserts.
     *
     * Pass criterion: trd_comp_intr_status=0; intr_status=0; int_out=false.
     */
    void tc_xspi_int_rst_001_reset_clears_interrupt_state();

    /**
     * @brief run_func003_tests: top-level entry point for FUNC_XSPI_003 suite.
     *
     * Orchestrates all 9 test cases for FUNC_XSPI_003 (Interrupt Architecture
     * and Status Management) in document order. Called from run_tests() after
     * run_func002_tests() completes.
     */
    void run_func003_tests();

    // =========================================================================
    // FUNC_XSPI_004 Test Case Methods
    // Implemented in: xspi_ctrl_func004_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_DMA_001: dma_settings reset value = 0x000D0000.
     *
     * Reads dma_settings (offset 0x23C) after reset_in de-assertion and
     * asserts it equals the documented reset value 0x000D0000.
     * Encodes: burst_sel=0x00, OTE=1 (bit 16), word_size=0b11 (bits[19:18]).
     *
     * Pass criterion: dma_settings == 0x000D0000.
     */
    void tc_xspi_dma_001_dma_settings_reset_value();

    /**
     * @brief TC_XSPI_DMA_002: dma_settings RW retention within write_bit_mask.
     *
     * Writes 0x000A00AA to dma_settings and reads back, verifying the value
     * is stored within write_bit_mask=0x000F00FF. Reserved bits outside the
     * mask are silently discarded by the scml2 framework.
     *
     * Pass criterion: (read_val & write_mask) == (write_val & write_mask).
     */
    void tc_xspi_dma_002_dma_settings_rw_retention();

    /**
     * @brief TC_XSPI_DMA_003: burst_sel (bits[7:0]) field write and read-back.
     *
     * Verifies reset value (burst_sel=0x00), writes burst_sel=0x0F, confirms
     * retention, then restores burst_sel=0x00 and confirms read-back.
     *
     * Pass criterion: three sequential read-backs match expected burst_sel.
     */
    void tc_xspi_dma_003_burst_sel_field_write();

    /**
     * @brief TC_XSPI_DMA_004: OTE (bit 16) and word_size (bits[19:18]) field
     *        write and read-back.
     *
     * Verifies reset defaults (OTE=1, word_size=0b11), writes OTE=0 and
     * word_size=0b10 (0x00080000), confirms retention, then restores to
     * 0x000D0000 and confirms.
     *
     * Pass criterion: three sequential read-backs match expected field values.
     */
    void tc_xspi_dma_004_ote_word_size_field_write();

    /**
     * @brief TC_XSPI_DMA_005: dma_settings restore to 0x000D0000 after reset.
     *
     * Writes a non-reset value to dma_settings (0x000A0055), applies
     * reset_in, reads back dma_settings, and asserts it returned to 0x000D0000.
     *
     * Pass criterion: dma_settings == 0x000D0000 after reset.
     */
    void tc_xspi_dma_005_dma_settings_reset_restore();

    /**
     * @brief TC_XSPI_AXI_SLAVE_001: t_axi_slave_socket READ → TLM_OK_RESPONSE.
     *
     * Dispatches a 64-bit TLM_READ_COMMAND via t_axi_slave_initiator and
     * asserts the DUT's b_transport_axi_slave() responds with TLM_OK_RESPONSE.
     *
     * Pass criterion: payload.get_response_status() == TLM_OK_RESPONSE.
     */
    void tc_xspi_axi_slave_001_read_response();

    /**
     * @brief TC_XSPI_AXI_SLAVE_002: t_axi_slave_socket WRITE → TLM_OK_RESPONSE.
     *
     * Dispatches a 64-bit TLM_WRITE_COMMAND via t_axi_slave_initiator and
     * asserts the DUT's b_transport_axi_slave() responds with TLM_OK_RESPONSE.
     *
     * Pass criterion: payload.get_response_status() == TLM_OK_RESPONSE.
     */
    void tc_xspi_axi_slave_002_write_response();

    /**
     * @brief TC_XSPI_POR_001: PoR_input_signals → TLM_OK_RESPONSE.
     *
     * Dispatches a bare TLM payload via por_initiator and asserts the DUT's
     * b_transport_por() handler returns TLM_OK_RESPONSE. No xspi_PoR_trans
     * extension is required at FUNC_XSPI_004 scope.
     *
     * Pass criterion: payload.get_response_status() == TLM_OK_RESPONSE.
     */
    void tc_xspi_por_001_por_response();

    /**
     * @brief TC_XSPI_DMA_ERR_001: ACMD DMA error injection — cdma_terr set,
     *        busy flags cleared, int_out asserts.
     *
     * Sets m_dma_error_count=1, enables cdma_terr in intr_enable (bit 17)
     * with global gate (bit 31), triggers ACMD on thread 0 (DMA stub returns
     * TLM_GENERIC_ERROR_RESPONSE on the descriptor fetch). Verifies:
     *   - intr_status.cdma_terr (bit 17) is set
     *   - int_out_sig == true (Path 1 active via cdma_terr + intr_enable)
     *   - trd_status[0] == 0 (thread cleared by error handler)
     *   - ctrl_status bits 7,2 == 0 (ctrl_busy, acmd_eng_busy cleared)
     *
     * Note: dma_target_error_l/h are NOT populated by cdma_handle_trigger()
     * at FUNC_XSPI_004 scope; address capture only occurs in the dma_read()/
     * dma_write() helpers used by PIO/ACMD data DMA (FUNC_XSPI_011/012).
     *
     * Pass criterion: all four assertions hold.
     */
    void tc_xspi_dma_err_001_cdma_terr_and_address_capture();

    /**
     * @brief TC_XSPI_DMA_ERR_002: dma_target_error_l/h RO write-ignore.
     *
     * Attempts to write 0xFFFFFFFF to both dma_target_error_l (0x150) and
     * dma_target_error_h (0x154). Reads back and asserts neither register
     * reads 0xFFFFFFFF (write_bit_mask=0x0 ensures all writes are discarded).
     *
     * Pass criterion: both registers unchanged after rogue write.
     */
    void tc_xspi_dma_err_002_dma_target_error_ro_enforcement();

    /**
     * @brief run_func004_tests: top-level entry point for FUNC_XSPI_004 suite.
     *
     * Orchestrates all 10 test cases for FUNC_XSPI_004 (DMA Interface and
     * AXI Transaction Management) in document order. Called from run_tests()
     * after run_func003_tests() completes.
     */
    void run_func004_tests();

    // =========================================================================
    // FUNC_XSPI_005 Test Case Methods
    // Implemented in: xspi_ctrl_func005_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_CFG_SEQ_001: Reset values of cmn_seq_regs_a registers.
     *
     * Reads all six registers in cmn_seq_regs_a (0x388–0x3A0) after reset
     * and asserts each equals its hardware reset value. Key non-zero resets:
     *   xip_mode_cfg=0x00FF0000, global_seq_cfg=0x0000208F.
     *
     * Pass criterion: all six registers match their Register_Reset_Val entries.
     */
    void tc_xspi_cfg_seq_001_reset_values_cmn_seq_regs();

    /**
     * @brief TC_XSPI_CFG_SEQ_002: Reset values of dev_seq_regs_a registers.
     *
     * Reads all 22 registers in dev_seq_regs_a (0x400–0x478) after reset and
     * asserts each equals its hardware reset value. Verifies non-zero resets
     * encoding xSPI NOR Profile 1 default opcodes (READ=0x03, WREN=0x06, etc).
     *
     * Pass criterion: all 22 registers match their Register_Reset_Val entries.
     */
    void tc_xspi_cfg_seq_002_reset_values_dev_seq_regs();

    /**
     * @brief TC_XSPI_CFG_SEQ_003: long_polling / short_polling shadow callback
     *        wiring verification.
     *
     * Verifies handle_write_long_polling() and handle_write_short_polling()
     * are wired: confirms non-zero reset values (0x3E8, 0x1F4), then writes
     * 0x012C and 0x0096 and verifies retention within 16-bit mask.
     *
     * Pass criterion: reset values correct; new values retained after write.
     */
    void tc_xspi_cfg_seq_003_polling_shadow_callback_wiring();

    /**
     * @brief TC_XSPI_CFG_SEQ_004: global_seq_cfg (0x390) write and read-back.
     *
     * Verifies handle_write_global_seq_cfg() callback wiring. Confirms reset
     * value 0x0000208F, writes 0x01800090, and verifies retention within the
     * full 32-bit write mask.
     *
     * Pass criterion: reset value correct; write value retained.
     */
    void tc_xspi_cfg_seq_004_global_seq_cfg_write_readback();

    /**
     * @brief TC_XSPI_CFG_SEQ_005: global_seq_cfg_1 (0x394) write and read-back.
     *
     * Verifies handle_write_global_seq_cfg_1() callback wiring. Confirms reset
     * value 0x00000000, writes 0x00000042 (nand_spare_area=66), and verifies
     * retention within the full 32-bit write mask.
     *
     * Pass criterion: reset value correct; write value retained.
     */
    void tc_xspi_cfg_seq_005_global_seq_cfg_1_write_readback();

    /**
     * @brief TC_XSPI_CFG_SEQ_006: xip_mode_cfg (0x388) write and read-back.
     *
     * Verifies handle_write_xip_mode_cfg() callback wiring. Confirms reset
     * value 0x00FF0000, then performs two write/readback rounds to verify
     * both xip_dis_mb_val and xip_en (bank bitmask) fields are stored.
     *
     * Pass criterion: both write patterns retained within 24-bit write mask.
     */
    void tc_xspi_cfg_seq_006_xip_mode_cfg_write_readback();

    /**
     * @brief TC_XSPI_CFG_SEQ_007: direct_access_cfg (0x398) write and read-back.
     *
     * Verifies handle_write_direct_access_cfg() callback wiring. Confirms reset
     * value 0x00000000, writes 0x00001001 (dac_bank_num=1, rmp_addr_en=1),
     * and verifies retention within the full write mask.
     *
     * Pass criterion: reset value correct; write value retained.
     */
    void tc_xspi_cfg_seq_007_direct_access_cfg_write_readback();

    /**
     * @brief TC_XSPI_CFG_SEQ_008: direct_access_rmp / direct_access_rmp_1
     *        (0x39C / 0x3A0) write and read-back.
     *
     * Verifies both halves of the 64-bit remap_offset are populated by writing
     * 0x00001000 to direct_access_rmp and 0x00000002 to direct_access_rmp_1,
     * then reading both back.
     *
     * Pass criterion: both registers retain written values after write.
     */
    void tc_xspi_cfg_seq_008_direct_access_rmp_write_readback();

    /**
     * @brief TC_XSPI_CFG_SEQ_009: All 22 device sequence registers write and
     *        read-back.
     *
     * Verifies all 22 CSML write callbacks for dev_seq_regs_a by writing
     * unique marker patterns to each register and confirming readback.
     *
     * Pass criterion: all 22 registers retain their distinctive write patterns.
     */
    void tc_xspi_cfg_seq_009_dev_seq_regs_write_readback();

    /**
     * @brief TC_XSPI_CFG_SEQ_010: Reset restores all sequence configuration
     *        registers to hardware reset values.
     *
     * Writes non-reset patterns to all 28 sequence config registers (cmn and
     * dev groups), applies reset_in, and verifies every register returned to
     * its documented hardware reset value. Validates the seq_cfg shadow reset
     * handler and scml2 register bank reset mechanisms.
     *
     * Pass criterion: all 28 registers read their RESET values after reset.
     */
    void tc_xspi_cfg_seq_010_reset_restores_seq_config_registers();

    /**
     * @brief run_func005_tests: top-level entry point for FUNC_XSPI_005 suite.
     *
     * Orchestrates all 10 test cases for FUNC_XSPI_005 (Sequence Configuration
     * Register Management) in document order. Called from run_tests() after
     * run_func004_tests() completes.
     */
    void run_func005_tests();

    // =========================================================================
    // FUNC_XSPI_006 Test Case Methods
    // Implemented in: xspi_ctrl_func006_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_MDR_001: STIG mode — cmd_reg0 written value ignored
     *
     * Verifies that in STIG mode the 32-bit value written to cmd_reg0 is never
     * decoded as a bitfield. Any value written is a pure trigger; the STIG
     * engine reads the instruction exclusively from cmd_reg1–cmd_reg4.
     * Pre-stages a valid STIG WREN, writes cmd_reg0=0xDEADBEEF, waits for
     * the SC_THREAD to complete, and verifies cmd_status.COMPLETE=1 and
     * ctrl_status.gcmd_eng_busy=0.
     *
     * Pass criterion: cmd_status[15]=1; ctrl_status[3]=0.
     */
    void tc_xspi_mdr_001_cmd_reg0_stig_value_ignored();

    /**
     * @brief TC_XSPI_STIG_008: STIG staging order — trigger before staging
     *        uses stale zeroed values
     *
     * Verifies that writing cmd_reg0 (trigger) before staging cmd_reg1–cmd_reg4
     * causes the STIG engine to execute with the current (stale, zeroed) register
     * values rather than the values written afterward. Both wrong-order and
     * correct-order phases must complete (cmd_status.COMPLETE=1) to confirm
     * the engine always fires; the ordering only determines which staging values
     * are captured.
     *
     * Pass criterion: Both phases result in cmd_status.COMPLETE=1.
     */
    void tc_xspi_stig_008_trigger_staging_order();

    /**
     * @brief TC_XSPI_MDR_002: PIO mode — cmd_reg0 field decode verified
     *
     * Verifies PIO-specific cmd_reg0 field decoding by writing 0x45221000 in
     * PIO mode: bits[31:30]=0b01 (PIO selector), bits[26:24]=0b101 (TRD_NUM=5),
     * bits[22:20]=0b010 (BANK=2), bit[18]=1 (INT=1), bits[15:0]=0x1000
     * (SECTOR_ERASE). Verifies trd_comp_intr_status[5]=1 (TRD_NUM=5 + INT=1
     * decoded), trd_status[5]=0 (thread completed), and cmd_ignored=0.
     *
     * Pass criterion: trd_comp_intr_status[5]=1; trd_status[5]=0; cmd_ignored=0.
     */
    void tc_xspi_mdr_002_cmd_reg0_pio_trd_num_bank_cmd_type();

    /** @brief TC_XSPI_MDR_002_cmd_reg0_pio_fields_decoded */
    void tc_xspi_mdr_002_cmd_reg0_pio_fields_decoded();

    /**
     * @brief TC_XSPI_MDR_003: ACMD mode — cmd_reg0 only TRD_NUM decoded
     *
     * Verifies that in ACMD mode cmd_reg0 only decodes bits[31:30]=0b00 (ACMD
     * selector) and bits[26:24] (TRD_NUM). Writes cmd_reg0=0x0300FFFF
     * (TRD_NUM=3, lower 24 bits=garbage). Confirms trd_comp[3]=1 (thread 3
     * dispatched, not thread 0xF or any garbage-decoded index), no other
     * completion bits set, and cmd_ignored=0.
     *
     * Pass criterion: trd_comp[3]=1; no other trd_comp bits set; cmd_ignored=0;
     *                 trd_error[3]=0.
     */
    void tc_xspi_mdr_003_cmd_reg0_acmd_only_trd_num();

    /**
     * @brief TC_XSPI_MDR_004: PIO SECTOR_ERASE — SECT_CNT in cmd_reg4
     *
     * Verifies that cmd_reg4 encodes SECT_CNT for PIO SECTOR_ERASE and that
     * the PIO engine processes the field correctly. Phase A: cmd_reg4=0x3
     * (SECT_CNT=3, erase 4 sectors); Phase B: cmd_reg4=0x0 (SECT_CNT=0,
     * erase 1 sector). Both phases must complete without error.
     *
     * Pass criterion: trd_status[0]=0 and trd_error[0]=0 for both phases.
     */
    void tc_xspi_mdr_004_cmd_reg4_sect_cnt_sector_erase();

    /**
     * @brief TC_XSPI_MDR_006: cmd_reg2/cmd_reg3 dual role
     *
     * Verifies the dual role of cmd_reg2/cmd_reg3: (a) In PIO READ mode they
     * form the 64-bit system memory address pointer for DMA write; (b) In ACMD
     * mode they form the 64-bit descriptor address pointer for the descriptor
     * fetch. Both phases must complete without error to confirm mode-correct
     * consumption of these shared registers.
     *
     * Pass criterion: Phase A (PIO READ) trd_status[0]=0, trd_error[0]=0;
     *                 Phase B (ACMD) trd_comp[0]=1, trd_error[0]=0.
     */
    void tc_xspi_mdr_006_cmd_reg2_reg3_pio_sys_ptr_vs_acmd_desc_ptr();

    /**
     * @brief TC_XSPI_MDR_007: cmd_reg5 consumed only by PIO
     *
     * Verifies that cmd_reg5 (xSPI address upper 32 bits) is only consumed by
     * the PIO engine. Phase A: PIO READ with cmd_reg5=0x00000002 completes
     * cleanly (address extension consumed). Phase B: STIG with cmd_reg5 written
     * to non-zero value still completes cleanly (cmd_reg5 ignored by STIG).
     *
     * Pass criterion: Phase A trd_status[0]=0, trd_error[0]=0;
     *                 Phase B cmd_status.COMPLETE=1, gcmd_eng_busy=0.
     */
    void tc_xspi_mdr_007_cmd_reg5_pio_only();

    /**
     * @brief TC_XSPI_MDR_008: Staging order — cmd_reg0 must be written last
     *
     * Verifies that writing cmd_reg0 (the trigger) dispatches with the register
     * values current at that instant. Phase A (wrong order): cmd_reg0 written
     * before cmd_reg1; dispatch uses stale cmd_reg1=0 from reset. Phase B
     * (correct order): cmd_reg1 staged first, then cmd_reg0; dispatch uses the
     * intended cmd_reg1 value. Both phases complete without error.
     *
     * Pass criterion: Both phases result in trd_status[0]=0, trd_error[0]=0,
     *                 cmd_ignored=0.
     */
    void tc_xspi_mdr_008_staging_order_cmd_reg0_last();

    /**
     * @brief run_func006_tests: top-level entry point for FUNC_XSPI_006 suite.
     *
     * Orchestrates all 8 test cases for FUNC_XSPI_006 (Operating Mode Control
     * and Command Dispatch) in document order. Called from run_tests() after
     * run_func005_tests() completes.
     */
    void run_func006_tests();

    // =========================================================================
    // FUNC_XSPI_007 Test Case Methods
    // Implemented in: xspi_ctrl_func007_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_POR_001: Basic PoR flow — init_comp set after xspi_PoR_trans
     *
     * Delivers xspi_PoR_trans with discovery_inhibit=1 (fast inhibited path).
     * Verifies:
     *   - por_ext.init_comp == 1 (extension output field)
     *   - ctrl_status bit 16 == 1 (init_comp hardware register)
     *   - ctrl_status bits[9:8] == 0b00 (no init_fail)
     *   - discovery_control bit 2 == 1 (discovery_comp)
     *   - discovery_control bits[4:3] == 0b00 (discovery_fail=0 for inhibited)
     *
     * Pass criterion: all five conditions hold.
     */
    void tc_xspi_por_001_por_init_comp_basic();

    /**
     * @brief TC_XSPI_POR_001_sfdp_discovery_full: PoR with full SFDP, 1-1-1 SDR
     *
     * Arms synthetic JESD216A SFDP (16 DWORDs basic table), delivers
     * xspi_PoR_trans with discovery_inhibit=0 and 1-1-1/ bank0 / 3B addr /
     * 8 dummy, verifies READ_SFDP @0 on bank 0, then discovery_comp,
     * init_comp, and SFDP-populated read/we/ers sequence registers.
     */
    void tc_xspi_por_001_sfdp_discovery_full();

    /**
     * @brief TC_XSPI_POR_002: discovery_inhibit=1 — no READ_SFDP issued
     *
     * Sends PoR with discovery_inhibit=1 and monitors m_flash_stub_tx_count.
     * Verifies the transaction counter remains 0 (no flash bus activity) and
     * that discovery_control.discovery_comp and the discovery_inhibit mirror
     * bit are set.
     *
     * Pass criterion: m_flash_stub_tx_count==0; discovery_comp=1; inhibit_mirror=1.
     */
    void tc_xspi_por_002_discovery_inhibit_skips_sfdp();

    /**
     * @brief TC_XSPI_POR_003: discovery_bank=2 routes READ_SFDP (0x5A) to
     *        xspi_bus_socket[2] only
     *
     * Requires NUM_TARGETS>=4. discovery_inhibit=0, discovery_bank=2. Verifies
     * m_flash_tx_per_target[0] unchanged, [2] increments, m_last_flash_ext
     * opcode 0x5A and bank_num 2, cmd_ignored=0, discovery_comp=1.
     */
    void tc_xspi_por_003_discovery_bank_selection();

    /**
     * @brief TC_XSPI_POR_004: Invalid SFDP signature → discovery_fail + init_fail
     *
     * Sends PoR with discovery_inhibit=0 and auto-mode (all 13 variations).
     * Flash stub returns zeros → no valid signature → FAIL outcome.
     * Verifies discovery_control bits[4:3]==0b01 and ctrl_status bits[9:8]==0b01.
     *
     * Pass criterion: discovery_fail=0b01; init_fail=0b01; init_comp=1.
     */
    void tc_xspi_por_004_discovery_fail_sets_init_fail();

    /**
     * @brief TC_XSPI_POR_005: JESD216A SFDP — seven sequence regs == model encoding
     *
     * Preloads xspi_target SFDP ROM; 4-4-4 SDR PoR. Read-backs of
     * global_seq_cfg, rst_seq_cfg_0, ers_seq_cfg_0, prog_seq_cfg_0,
     * read_seq_cfg_0, we_seq_cfg_0, stat_seq_cfg_0 match configure_registers_from_sfdp.
     */
    void tc_xspi_por_005_sfdp_sequence_reg_auto_populate();

    /**
     * @brief TC_XSPI_ERR_005: Pre-init write blocking
     *
     * Writes to ctrl_config before issuing xspi_PoR_trans (while
     * m_init_comp_done=false). Verifies the write is silently discarded:
     * ctrl_config reads back as 0x00000000 (reset value). After PoR, a
     * second write to the same register must be accepted.
     *
     * Pass criterion: pre-PoR ctrl_config==0x00000000; post-PoR write accepted.
     */
    void tc_xspi_err_005_write_before_init_comp_discarded();

    /**
     * @brief run_func007_tests: top-level entry point for FUNC_XSPI_007 suite.
     *
     * Orchestrates all 6 test cases for FUNC_XSPI_007 (Power-on Reset and
     * SFDP Discovery Engine) in document order. Called from run_tests() after
     * run_func006_tests() completes.
     */
    void run_func007_tests();

    // =========================================================================
    // FUNC_XSPI_008 Test Case Methods
    // Implemented in: xspi_ctrl_func008_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_DM_001: AXI READ → READ_ZERO_LATENCY (opcode 0x03)
     *
     * Sets work_mode=Direct (2'b00), issues a 4-byte AXI READ at address
     * 0x00001000 via t_axi_slave_initiator, and verifies that:
     *   - A cdns_extension transaction arrived on xspi_bus_socket[0]
     *   - ext.opcode     == 0x03 (READ_ZERO_LATENCY)
     *   - ext.instr_type == XSPI_INSTR_READ (1)
     *   - ext.bank_num   == 0 (dac_bank_num default)
     *   - ext.address    == 0x00001000
     *   - ctrl_status.ctrl_busy was set during dispatch and cleared after
     *
     * Pass criterion: all five assertions hold.
     */
    void tc_xspi_dm_001_axi_read_to_flash_read();

    /**
     * @brief TC_XSPI_DM_002: AXI WRITE → WREN (0x06) + PAGE_PROGRAM (0x02)
     *
     * Sets work_mode=Direct, issues a 4-byte AXI WRITE at address 0x00002000
     * via t_axi_slave_initiator, and verifies that:
     *   - Two flash transactions arrived on xspi_bus_socket[0]
     *     (WREN first, then PAGE_PROGRAM)
     *   - The last captured ext.opcode == 0x02 (PAGE_PROGRAM)
     *   - ext.instr_type == XSPI_INSTR_WRITE (2)
     *   - ext.address    == 0x00002000
     *   - m_flash_stub_tx_count == 2 (WREN + PAGE_PROGRAM)
     *   - ctrl_status.ctrl_busy cleared after dispatch
     *
     * Pass criterion: all assertions hold.
     */
    void tc_xspi_dm_002_axi_write_to_wren_page_program();

    /**
     * @brief TC_XSPI_DM_003: Bank selection via dac_bank_num=2
     *
     * Configures dac_bank_num=2 in direct_access_cfg (requires NUM_TARGETS>=4),
     * issues an AXI READ at address 0x00001000, and verifies that
     * ext.bank_num == 2 in the captured cdns_extension (routed to socket[2]).
     *
     * Pass criterion: m_last_flash_ext.bank_num == 2; m_flash_stub_tx_count >= 1.
     */
    void tc_xspi_dm_003_bank_selection_dac_bank_num();

    /**
     * @brief TC_XSPI_DM_004: 64-bit address remapping via rmp_addr_en
     *
     * Programs N=0x00001000 into direct_access_rmp and sets rmp_addr_en=1.
     * Issues an AXI READ at address 0x00005000 and verifies that the flash
     * transaction carries address 0x00004000 (= 0x00005000 - 0x00001000).
     *
     * Pass criterion: m_last_flash_ext.address == 0x00004000.
     */
    void tc_xspi_dm_004_address_remapping_rmp_addr_en();

    /**
     * @brief TC_XSPI_DM_005: ctrl_status.ctrl_busy lifecycle in Direct mode
     *
     * After a Direct-mode READ dispatch, verifies that ctrl_status.ctrl_busy
     * (bit 7) is cleared (= 0) after the synchronous b_transport call returns.
     * Verifies the pre-dispatch state (busy asserted by reading ctrl_status
     * inside the flash TLM path is not observable in LT; busy is cleared
     * before the handler returns and the testbench reads ctrl_status).
     *
     * Pass criterion: ctrl_status.ctrl_busy == 0 after dispatch completes.
     */
    void tc_xspi_dm_005_direct_mode_ctrl_status_busy();

    /**
     * @brief TC_XSPI_CFG_001: NUM_TARGETS boundary — out-of-range bank
     *
     * With the configured NUM_TARGETS (4 in the reference build), programs
     * dac_bank_num=4 (out of range for banks 0–3) and issues an AXI READ. Verifies:
     *   - cmd_ignored (bit 20) in intr_status is set (dispatch rejected)
     *   - No transaction on xspi_bus_socket (m_flash_stub_tx_count unchanged)
     *   - TLM_OK_RESPONSE is still returned on t_axi_slave_socket
     *
     * Pass criterion: intr_status bit 20 set; tx_count unchanged; OK response.
     */
    void tc_xspi_cfg_001_num_targets_1_boundary();

    /**
     * @brief TC_XSPI_CFG_002: NUM_TARGETS socket routing and out-of-range guard
     *
     * Phases: read n_banks from ctrl_features; in-range bank=0 to socket[0];
     * out-of-range bank=4 (>= NUM_TARGETS=4) must set cmd_ignored and issue no
     * additional flash transaction.
     */
    void tc_xspi_cfg_002_num_targets_routing();

    /**
     * @brief TC_XSPI_CFG_001_num_targets_4 — four CS build: n_banks, sockets,
     *        in-range direct transactions (0–3), bank 5 out-of-range
     *
     * With NUM_TARGETS=4: ctrl_features n_banks field; four xspi_bus_socket
     * entries; direct-mode AXI READ for dac_bank_num 0–3 routes to the matching
     * stub; dac_bank_num=5 sets cmd_ignored with no flash traffic on any CS.
     */
    void tc_xspi_cfg_002_num_targets_4();

    /**
     * @brief run_func008_tests: top-level entry point for FUNC_XSPI_008 suite.
     *
     * Orchestrates FUNC_XSPI_008 (Direct Mode Flash Forwarding) tests in
     * document order. Called from run_tests() after run_func007_tests() completes.
     */
    void run_func008_tests();

    // =========================================================================
    // FUNC_XSPI_009 Test Case Methods
    // STIG Mode Command Execution
    // =========================================================================

    /**
     * @brief TC_XSPI_STIG_001: STIG READ handler — opcode and address dispatch
     *
     * Verifies that a STIG READ instruction (instr_type=1) correctly encodes
     * the opcode in cmd_reg1[31:24], the flash address in cmd_reg3[23:0] and
     * cmd_reg2[31:8], and DATA_CNT in cmd_reg3[31:24]. After trigger and
     * engine completion, cmd_status.COMPLETE (bit 15) must be set.
     *
     * Pass criterion: cmd_status.COMPLETE=1 after STIG READ dispatch.
     */
    void tc_xspi_stig_001_read_handler();

    /**
     * @brief TC_XSPI_STIG_002: STIG WRITE handler — write data dispatch
     *
     * Verifies that a STIG WRITE instruction (instr_type=2) correctly issues a
     * write flash transaction with write_data_byte from cmd_reg4[23:16] and
     * address/DATA_CNT from cmd_reg3. After engine completion,
     * cmd_status.COMPLETE (bit 15) must be set and intr_status.stig_done set.
     *
     * Pass criterion: cmd_status.COMPLETE=1; intr_status.stig_done=1.
     */
    void tc_xspi_stig_002_write_handler();

    /**
     * @brief TC_XSPI_STIG_003: STIG control command — WREN (no data)
     *
     * Verifies that a control-only STIG instruction (instr_type=0, opcode=0x06
     * WREN, no address, no data) dispatches correctly with data_bytes=0, and
     * that cmd_status.COMPLETE is set at completion.
     *
     * Pass criterion: cmd_status.COMPLETE=1; intr_status.stig_done=1.
     */
    void tc_xspi_stig_003_control_wren();

    /**
     * @brief TC_XSPI_STIG_004: STIG READ_STATUS_REG (RDSR) — INSTR_LINK + glued 1B
     *
     * Programs phase-1 READ + CMD=0x05 with INSTR_LINK, then phase-2 INSTR=127
     * (1 byte, read). Verifies one flash bus READ on CS0, status byte via STIG
     * SDMA AXI window, cmd_status.COMPLETE=1, intr_status.stig_done=1.
     *
     * Pass criterion: bus opcode 0x05, data_bytes=1, status byte 0x00 (default
     * stub SR1), cmd_status bit15 + stig_done set.
     */
    void tc_xspi_stig_004_control_rdsr();

    /**
     * @brief TC_XSPI_STIG_005: STIG READ_SFDP handler (instr_type=96/0x60)
     *
     * Verifies that a STIG READ_SFDP instruction (opcode=0x5A, instr_type=96,
     * address=0x000000, 4-byte read) dispatches as a READ and that
     * cmd_status.COMPLETE is set at completion.
     *
     * Pass criterion: cmd_status.COMPLETE=1; intr_status.stig_done=1.
     */
    void tc_xspi_stig_005_read_sfdp_handler();

    /**
     * @brief TC_XSPI_STIG_006: INSTR_LINK two-phase chaining
     *
     * Verifies the INSTR_LINK mechanism: setting bit 28 of cmd_reg4 in the
     * first trigger causes gcmd_eng_mc_busy (ctrl_status bit 4) to be set and
     * intr_status.stig_done to fire, but cmd_status.COMPLETE is NOT yet set.
     * A second trigger then dispatches the glued data phase, clears
     * gcmd_eng_mc_busy, sets cmd_status.COMPLETE=1, and fires stig_done again.
     *
     * Pass criterion: After first trigger: gcmd_eng_mc_busy=1, COMPLETE=0.
     *                 After second trigger: gcmd_eng_mc_busy=0, COMPLETE=1.
     */
    void tc_xspi_stig_006_instr_link_two_phase_chain();

    /**
     * @brief stig_write_read_data_integrity: STIG glued page program and read, verify
     *        flash memory (Profile 1 / Variant 1 encoding; SDMA is LT placeholder).
     */
    void stig_write_read_test();

    /**
     * @brief TC_XSPI_STIG_007: cmd_status.COMPLETE set and gcmd_eng_busy=0
     *        at completion
     *
     * Verifies the full completion state after any STIG operation:
     * (a) cmd_status.COMPLETE (bit 15) is set, and
     * (b) ctrl_status.gcmd_eng_busy (bit 3) is cleared.
     * Uses a simple WREN control command as the trigger.
     *
     * Pass criterion: cmd_status[15]=1; ctrl_status[3]=0 after STIG completion.
     */
    void tc_xspi_stig_007_completion_sets_cmd_status();

    /**
     * @brief TC_XSPI_STIG_009: Suspend/resume opcode dispatch via stat_seq_cfg_8
     *
     * Verifies that erase-suspend and erase-resume opcodes stored in
     * stat_seq_cfg_8 (bits[31:24] and bits[23:16]) can be issued as STIG
     * control commands. The test writes known opcodes to stat_seq_cfg_8,
     * reads them back, then issues STIG operations using those opcodes and
     * confirms engine completion (cmd_status.COMPLETE=1, stig_done=1).
     *
     * Pass criterion: STIG complete for suspend opcode; STIG complete for
     *                 resume opcode; both cmd_status.COMPLETE=1.
     */
    void tc_xspi_stig_009_suspend_resume_handler();

    /**
     * @brief run_func009_tests: top-level entry point for FUNC_XSPI_009 suite.
     *
     * Orchestrates all 8 test cases for FUNC_XSPI_009 (STIG Mode Command
     * Execution) in document order. Called from run_tests() after
     * run_func008_tests() completes.
     */
    void run_func009_tests();

    // =========================================================================
    // FUNC_XSPI_010 Test Case Methods
    // Implemented in: xspi_ctrl_func010_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_BOOT_001: Boot engine success path
     *
     * Preloads bank-0 NOR (flash_model) with a 32-byte LE boot config at 0x0
     * (main_data_size, image_offset, host_addr, SPI-NAND page size @ 0x18) and a
     * 32-byte image at image_offset; arms m_dma_buf at host_addr to capture
     * i_dma_socket writes. Delivers xspi_PoR_trans with discovery_inhibit=1 and
     * boot_en=1. Verifies i_dma_socket payload matches flash image bytes,
     * flash[0..31] still matches the programmed config record, por_ext.boot_comp==1,
     * boot_error==0, ctrl_status.init_comp, ctrl_features_reg.boot_available,
     * and boot_status error bits [2:0] clear.
     *
     * Pass criterion: all checks hold.
     */
    void tc_xspi_boot_001_boot_success_path();

    /**
     * @brief TC_XSPI_BOOT_002: Boot DQS error path
     *
     * Arms m_flash_error_count=1 so that the flash stub returns
     * TLM_GENERIC_ERROR_RESPONSE on the first transaction (the 32-byte
     * config-record READ at flash address 0x0).  Delivers xspi_PoR_trans
     * with boot_en=1 and discovery_inhibit=1.  Verifies:
     *   (a) por_ext.boot_error == 1 after b_transport returns
     *   (b) por_ext.boot_comp == 0
     *   (c) boot_status (0x158) bit 0 (boot_dqs_err) == 1
     *
     * Pass criterion: all three conditions hold.
     */
    void tc_xspi_boot_002_boot_error_dqs_error();

    /**
     * @brief TC_XSPI_BOOT_003: Boot AXI bus error path
     *
     * Programs the flash stub to supply a 32-byte config record with
     * main_data_size=256, image_offset=0x1000, host_addr=0x80000000 via
     * m_flash_sfdp_buf (repurposed for boot record injection).  Arms
     * m_dma_error_count=1 so the DMA stub returns TLM_GENERIC_ERROR_RESPONSE
     * on the first i_dma_socket WRITE (the system memory boot image write).
     * Delivers xspi_PoR_trans with boot_en=1 and discovery_inhibit=1.
     * Verifies:
     *   (a) por_ext.boot_error == 1
     *   (b) por_ext.boot_comp == 0
     *   (c) boot_status (0x158) bit 2 (boot_bus_err) == 1
     *
     * Pass criterion: all three conditions hold.
     */
    void tc_xspi_boot_003_boot_error_bus_error();

    /**
     * @brief TC_XSPI_BOOT_004: boot_available=0 suppresses boot engine
     *
     * Verifies the behavioral side of boot suppression accessible without a
     * separately parameterized DUT: reads ctrl_features_reg and confirms
     * boot_available bit [16] matches the elaboration-time value.  Then
     * delivers xspi_PoR_trans with boot_en=0 and verifies that no boot DMA
     * activity occurs (m_flash_stub_tx_count unchanged, m_dma_error_count
     * not triggered, boot_comp=0, boot_error=0 returned by the model).
     * Also confirms that boot_status (0x158) remains 0x00000000.
     *
     * Note: Full boot_available=0 parameter verification requires a second
     * DUT instance. This test exercises the boot_en=0 suppression path and
     * ctrl_features_reg.boot_available read-only enforcement, which together
     * cover the observable behavior of TC_XSPI_BOOT_004 within the single-DUT
     * testbench configuration (NUM_TARGETS=1, boot_available=true).
     *
     * Pass criterion: boot_comp=0, boot_error=0, boot_status=0,
     *                 ctrl_features_reg boot_available bit readable.
     */
    void tc_xspi_boot_004_boot_not_available();

    /**
     * @brief TC_XSPI_BOOT_005: Register write blocking during boot execution
     *
     * Verifies that software writes to writable registers during the boot
     * initialization window (m_init_comp_done=false) are silently discarded.
     * Delivers xspi_PoR_trans with boot_en=1 and discovery_inhibit=1.  Before
     * sending the PoR, establishes that ctrl_config (0x230) is at reset value.
     * After PoR returns (boot complete), reads ctrl_config and verifies it
     * still equals the reset value, confirming the blocking mechanism.
     *
     * Implementation note: In the LT testbench, b_transport() is synchronous
     * and the register write can only be issued before or after the PoR
     * transaction.  The test issues a write BEFORE PoR (pre-init blocking,
     * same as TC_XSPI_ERR_005 path), verifies it is discarded after PoR
     * completes, then issues a valid write AFTER PoR completes and verifies
     * it is accepted.  This demonstrates the init_comp_done gate.
     *
     * Pass criterion: pre-init write to ctrl_config discarded; post-init write
     *                 to ctrl_config accepted; boot_comp=1.
     */
    void tc_xspi_boot_005_boot_register_write_blocking();

    /**
     * @brief TC_XSPI_CFG_004: boot_available=0 parameter suppresses boot
     *
     * Reads ctrl_features_reg (0xF04) and extracts bit 16 (boot_available).
     * With the current DUT (boot_available=1 by default), confirms the field
     * reads 1 and documents that the test validates the register's read-only
     * enforcement: attempts to write ctrl_features_reg with bit 16=0 must be
     * silently ignored (write_bit_mask=0x0 for this RO register).  Re-reads
     * and confirms boot_available bit is unchanged.
     *
     * Also delivers xspi_PoR_trans with boot_en=0 to confirm that when
     * boot_en=0, the boot engine is not triggered regardless of boot_available,
     * and boot_comp=0, boot_error=0.
     *
     * Note: The exact TC_XSPI_CFG_004 scenario (boot_available=0 parameter
     * suppressing boot_en=1) would require a separately parameterized DUT.
     * This test covers: (a) ctrl_features_reg.boot_available RO enforcement,
     * (b) boot_en=0 suppression path, and (c) boot_comp/boot_error both 0.
     *
     * Pass criterion: ctrl_features_reg.boot_available is RO (write ignored);
     *                 boot_en=0 yields boot_comp=0, boot_error=0.
     */
    void tc_xspi_cfg_004_boot_available_0_suppresses_boot();

    /**
     * @brief run_func010_tests: top-level entry point for FUNC_XSPI_010 suite.
     *
     * Orchestrates all 6 test cases for FUNC_XSPI_010 (Boot Mode Autonomous
     * DMA Engine) in document order:
     *   TC_XSPI_BOOT_001, TC_XSPI_BOOT_002, TC_XSPI_BOOT_003,
     *   TC_XSPI_BOOT_004, TC_XSPI_BOOT_005, TC_XSPI_CFG_004.
     * Called from run_tests() after run_func009_tests() completes.
     */
    void run_func010_tests();

    // =========================================================================
    // FUNC_XSPI_011 Test Case Methods
    // Implemented in: xspi_ctrl_func011_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_PIO_001 — PIO READ: flash read via xspi_bus_socket,
     *        64-byte DMA write via i_dma_socket; cdns_extension encoding verified.
     *
     * Verifies the complete PIO READ path (CMD_TYPE=0x2200):
     *   Phase A: flash READ on xspi_bus_socket[0] with opcode=0x03.
     *   Phase B: DMA WRITE of received data to system memory via i_dma_socket.
     * Checks trd_busy[0] lifecycle, cmd_status.COMPLETE, and cdns_extension fields.
     *
     * Pass criterion: flash READ issued; DMA WRITE issued; trd_busy[0]=0;
     *                 cmd_status[0].COMPLETE=1.
     */
    void tc_xspi_pio_001_read_cmd_type();

    /**
     * @brief TC_XSPI_PIO_002 — PIO PROGRAM: WREN + PAGE_PROGRAM on flash;
     *        data fetch from i_dma_socket.
     *
     * Verifies PIO PROGRAM path (CMD_TYPE=0x2100):
     *   Phase A: DMA READ from system memory via i_dma_socket.
     *   Phase B: WREN (opcode=0x06) on xspi_bus_socket[0].
     *   Phase C: PAGE_PROGRAM (opcode=0x02) with data on xspi_bus_socket[0].
     * Checks completion status and transaction count.
     *
     * Pass criterion: >=2 flash transactions (WREN + PAGE_PROGRAM); DMA READ
     *                 issued; trd_busy[0]=0; cmd_status[0].COMPLETE=1.
     */
    void tc_xspi_pio_002_program_cmd_type();

    /**
     * @brief TC_XSPI_PIO_003 — PIO SECTOR_ERASE: WREN + erase for SECT_CNT+1
     *        sectors; opcode from ers_seq_cfg_0.
     *
     * Verifies PIO SECTOR_ERASE path (CMD_TYPE=0x1000) with SECT_CNT=1
     * (2 sectors). Confirms 2*(WREN+ERASE) transactions on xspi_bus_socket[0].
     * Checks trd_busy lifecycle and cmd_status.COMPLETE.
     *
     * Pass criterion: flash_stub_tx_count >= 4 (2 WREN + 2 ERASE);
     *                 trd_busy[0]=0; cmd_status[0].COMPLETE=1.
     */
    void tc_xspi_pio_003_sector_erase_cmd_type();

    /**
     * @brief TC_XSPI_PIO_004 — PIO CHIP_ERASE: WREN + full-chip erase from
     *        ers_seq_cfg_2.
     *
     * Verifies PIO CHIP_ERASE path (CMD_TYPE=0x1001). Confirms WREN + erase
     * (opcode=0xDC on the bus in LT) appear on xspi_bus_socket[0]. Checks trd_busy lifecycle.
     *
     * Pass criterion: >=2 flash transactions (WREN + CHIP_ERASE);
     *                 trd_busy[0]=0; cmd_status[0].COMPLETE=1.
     */
    void tc_xspi_pio_004_chip_erase_cmd_type();

    /**
     * @brief TC_XSPI_PIO_005 — PIO SOFT_RESET: RESET_ENABLE (0x66) + RESET
     *        (0x99) from rst_seq_cfg_0/1.
     *
     * Verifies PIO SOFT_RESET path (CMD_TYPE=0x1100). Confirms reset command
     * opcodes (0xFF software-reset or 0x66+0x99 RESET_ENABLE sequence) on
     * xspi_bus_socket[0]. Checks trd_busy lifecycle.
     *
     * Pass criterion: >=1 flash transaction; trd_busy[0]=0;
     *                 cmd_status[0].COMPLETE=1.
     */
    void tc_xspi_pio_005_soft_reset_cmd_type();

    /**
     * @brief TC_XSPI_PIO_006 — PIO JEDEC_RESET: JEDEC hardware reset sequence;
     *        only cmd_reg0 consumed.
     *
     * Verifies PIO JEDEC_RESET path (CMD_TYPE=0x1101). cmd_reg1–cmd_reg5
     * are not consumed. Confirms flash transaction on xspi_bus_socket[0] with
     * JEDEC reset opcode. Checks trd_busy lifecycle.
     *
     * Pass criterion: >=1 flash transaction; trd_busy[0]=0;
     *                 cmd_status[0].COMPLETE=1; cmd_ignored not set.
     */
    void tc_xspi_pio_006_jedec_reset_cmd_type();

    /**
     * @brief TC_XSPI_PIO_007 — Multi-thread concurrent: two PIO threads (0,1)
     *        issued back-to-back; both complete independently.
     *
     * Issues PIO CHIP_ERASE on thread 0 then CHIP_ERASE on thread 1 in rapid
     * succession. In the LT synchronous model each completes inline, so both
     * trd_busy bits are clear after both cmd_reg0 writes. Verifies independent
     * thread numbering via trd_comp_intr_status bits 0 and 1 (both INT=1).
     *
     * Pass criterion: trd_comp_intr_status bits 0 and 1 both set;
     *                 trd_busy[0]=0 and trd_busy[1]=0; cmd_ignored=0.
     */
    void tc_xspi_pio_007_multi_thread_concurrent();

    /**
     * @brief TC_XSPI_PIO_008 — INT=1 on thread 2 after PIO READ (0x2200):
     *        trd_comp_intr_status[2], int_out, then W1C clear.
     *
     * Sets intr_enable[31], issues PIO READ on thread 2 with cmd_reg0 INT=1,
     * verifies trd_comp_intr_status bit 2 and int_out, then W1C bit 2 and
     * checks flag clear and int_out de-asserted.
     *
     * Pass criterion: trd_comp_intr_status[2]=1; int_out high; trd_busy[2]=0;
     *                 after W1C: trd_comp[2]=0; int_out low.
     */
    void tc_xspi_pio_008_thread_int_flag_completion_interrupt();

    /**
     * @brief TC_XSPI_PIO_009 — cmd_status_ptr / cmd_status indirect read (PIO):
     *        complete PIO READ on thread 3 (INT=1), then ptr=3 / ptr=0 reads.
     *
     * Writes cmd_status_ptr=0x3, reads cmd_status (bit 0 = completion for
     * selected thread) — expect set for thread 3. Writes ptr=0, reads
     * cmd_status — expect COMPLETE clear for thread 0.
     *
     * Pass criterion: after READ on trd3: cmd_status@sel=3 has bit0=1;
     *                 @sel=0: bit0=0.
     */
    void tc_xspi_pio_009_cmd_status_thread_selection();

    /**
     * @brief TC_XSPI_MDR_005 — cmd_reg4 DATA_CNT for READ and PROGRAM:
     *        byte count equals DATA_CNT+1.
     *
     * (a) Sets cmd_reg4=0xFF (DATA_CNT=255, actual=256 bytes). Issues PIO READ.
     *     Verifies m_last_flash_ext.data_bytes == 256.
     * (b) Sets cmd_reg4=0x00 (DATA_CNT=0, actual=1 byte). Issues PIO PROGRAM.
     *     Verifies flash_stub_tx_count >= 2 (WREN + PAGE_PROGRAM with 1 byte).
     *
     * Pass criterion: (a) data_bytes==256; (b) flash transactions issued with
     *                 1-byte payload; both trd_busy[0]=0.
     */
    void tc_xspi_mdr_005_cmd_reg4_data_cnt_read_program();

    /**
     * @brief TC_XSPI_XIP_005 — PIO MB_XIP_DIS exits XIP on target bank.
     *
     * Enters XIP on bank 0 via xip_mode_cfg.xip_en=1 (bit 0), then issues
     * PIO READ with MB_XIP_DIS=1 (cmd_reg0 bit 17). Verifies the XIP-disable
     * mode byte path is exercised (xip_mode_cfg.xip_en bit 0 cleared) and
     * the READ completes without error.
     *
     * Pass criterion: xip_mode_cfg.xip_en[0]=0 after PIO READ with
     *                 MB_XIP_DIS=1; trd_busy[0]=0; cmd_ignored=0.
     */
    void tc_xspi_xip_005_pio_xip_dis_exit();

    /**
     * @brief TC_XSPI_ERR_002 — PIO busy-thread silent ignore: second trigger
     *        on same TRD_NUM while busy is silently dropped (no CMD_IGNORED).
     *
     * In the LT synchronous model PIO runs inline so the thread is never
     * observed as busy from a second cmd_reg0 write. This test verifies the
     * silent-ignore property by confirming: (a) normal completion on the first
     * trigger; (b) no CMD_IGNORED bit set; (c) intr_status unchanged.
     * The test issues two consecutive triggers (thread 0) and confirms only
     * one effective execution (flash_stub_tx_count == original + count from
     * one CHIP_ERASE).
     *
     * Pass criterion: cmd_ignored=0; trd_busy[0]=0; cmd_status.COMPLETE=1
     *                 for thread 0.
     */
    void tc_xspi_err_002_pio_busy_thread_ignored();

    /**
     * @brief TC_XSPI_ERR_004 — PIO DMA bus error sets intr_status.ddma_terr
     *        and trd_error_intr_status[0].
     *
     * Arms m_dma_error_count=1 so the DMA stub returns TLM_GENERIC_ERROR_RESPONSE
     * on the data-path write. Issues PIO READ on thread 0. Verifies:
     *   - intr_status.ddma_terr (bit 18) is set.
     *   - dma_target_error_l (0x150) captures the failing system address.
     *   - trd_error_intr_status (0x130) bit 0 is set.
     *
     * Pass criterion: ddma_terr=1; dma_target_error_l == expected address;
     *                 trd_error_intr_status[0]=1.
     */
    void tc_xspi_err_004_pio_dma_bus_error_ddma_terr();

    /**
     * @brief TC_XSPI_ERR_006 — reset_in aborts in-progress PIO thread:
     *        trd_status and ctrl_status cleared to zero after reset.
     *
     * In the LT model PIO runs synchronously, so direct busy observation during
     * execution is not possible. This test verifies the reset postcondition:
     *   1. Issue PIO READ on thread 0 (completes synchronously).
     *   2. Assert reset_in.
     *   3. Verify trd_status=0, ctrl_status=0, trd_comp_intr_status=0,
     *      trd_error_intr_status=0, intr_status=0, int_out=false.
     *
     * Pass criterion: all status registers == 0 after reset; int_out=false.
     */
    void tc_xspi_err_006_reset_aborts_in_progress_thread();

    /**
     * @brief TC_XSPI_CFG_003 — n_threads=2 parameter: only threads 0–1 valid;
     *        thread 2 treated as out-of-range.
     *
     * With n_threads=1 encoding (2 threads, ctrl_features_reg.n_threads=1):
     *   (a) Issue PIO READ on thread 0 — must succeed (trd_busy[0]=0 after).
     *   (b) Issue PIO READ on thread 2 — should be silently ignored or
     *       handled as out-of-range (no harmful side effect; cmd_ignored=0
     *       per PIO silent-ignore rule).
     * Verifies ctrl_features_reg.n_threads encoding and per-thread isolation.
     *
     * Pass criterion: (a) thread 0 completes; (b) thread 2 trigger does not
     *                 corrupt thread 0 state; ctrl_features_reg.n_threads==1.
     */
    void tc_xspi_cfg_003_n_threads_2();

    /**
     * @brief TC_XSPI_REG_007 — W1C trd_comp_intr_status: write-1-to-clear
     *        semantics; cleared bits deassert int_out.
     *
     * Sets trd_comp_intr_status[0] via a PIO CHIP_ERASE with INT=1 on thread 0.
     * Verifies bit 0 is set and int_out is asserted. Then writes 0x00000001 to
     * trd_comp_intr_status (W1C). Verifies bit 0 is cleared and int_out is
     * deasserted. Also verifies write-0-no-effect: writes 0x00000000 and
     * confirms no change in state.
     *
     * Pass criterion: bit 0 set after PIO; int_out=true; bit 0 cleared after
     *                 W1C write; int_out=false; write-0 has no effect.
     */
    void tc_xspi_reg_007_trd_comp_intr_status_w1c();

    /**
     * @brief TC_XSPI_INT_001 — trd_comp interrupt assertion: intr_enable[31]
     *        (intr_en)=1, PIO READ on thread 0 with cmd_reg0[18](INT)=1;
     *        trd_comp_intr_status[0]=1; int_out high; W1C clear; int_out low.
     *
     * Pass criterion: trd_comp[0] set then cleared after W1C; int_out high
     *                 after completion then low after clear (Section 7.2.3;
     *                 Table 4.10 INT; intr_enable 0x114, trd_comp 0x120).
     */
    void tc_xspi_int_001_trd_comp_int_assertion();

    void tc_xspi_stig_010_single_phase_inline_write();

    /**
     * @brief run_func011_tests: top-level entry point for FUNC_XSPI_011 suite.
     *
     * Orchestrates all 17 test cases for FUNC_XSPI_011 (PIO Mode Multi-Thread
     * DMA Execution) in document order:
     *   TC_XSPI_PIO_001–009 (primary PIO tests),
     *   TC_XSPI_MDR_005, TC_XSPI_XIP_005,
     *   TC_XSPI_ERR_002, TC_XSPI_ERR_004, TC_XSPI_ERR_006,
     *   TC_XSPI_CFG_003, TC_XSPI_REG_007, TC_XSPI_INT_001.
     * Called from run_tests() after run_func010_tests() completes.
     */
    void run_func011_tests();

    // =========================================================================
    // FUNC_XSPI_012 Test Case Methods
    // Implemented in: xspi_ctrl_func012_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_ACMD_001: ACMD single READ descriptor execution.
     *
     * Sets ctrl_config.work_mode=ACMD (2'b11). Builds a 64-byte aligned
     * descriptor at DMA address 0x00010000 with:
     *   next_pointer=0, sys_mem_pointer=0x00080000, xspi_pointer=0x00200000,
     *   cmd_type=0x2200 (READ), cmd_counter=0xFF (256 bytes), flags=INT=0,CONT=0.
     * Writes cmd_reg2=0x00010000, cmd_reg3=0x00000000, triggers ACMD TRD=0.
     * Verifies: i_dma_socket fetches 64-byte descriptor; flash READ at 0x00200000
     * for 256 bytes; DMA WRITE to 0x00080000; status writeback at offset +40
     * with COMPLETE bit set; trd_status[0]=0 after completion.
     *
     * Pass criterion: flash_stub_tx_count>=1; opcode=0x03; trd_status[0]=0;
     *                 m_dma_buf[40] bit 15 set (COMPLETE in writeback word).
     */
    void tc_xspi_acmd_001_single_read_descriptor();

    /**
     * @brief TC_XSPI_ACMD_002: ACMD single PROGRAM descriptor execution.
     *
     * Builds descriptor with cmd_type=0x2100 (PROGRAM), sys_mem=0x00020000,
     * xspi_ptr=0x00400000, counter=0xFF (256 bytes), CONT=0, INT=0.
     * Triggers thread 0. Verifies: DMA READ from 0x00020000; WREN + PAGE_PROGRAM
     * at 0x00400000; status writeback at descriptor+40 with COMPLETE=1.
     *
     * Pass criterion: flash_stub_tx_count>=2 (WREN+PAGE_PROGRAM); opcode=0x02;
     *                 trd_status[0]=0; COMPLETE bit in writeback word set.
     */
    void tc_xspi_acmd_002_single_program_descriptor();

    /**
     * @brief TC_XSPI_ACMD_003: ACMD descriptor chaining via CONT flag.
     *
     * Creates two 64-byte aligned descriptors:
     *   Descriptor A at 0x00020000: next_pointer=0x00020040, CONT=1, INT=0,
     *                               cmd_type=0x2200 (READ), counter=0xFF.
     *   Descriptor B at 0x00020040: next_pointer=0, CONT=0, INT=1,
     *                               cmd_type=0x2200 (READ), counter=0xFF.
     * Triggers thread 0. Verifies: both descriptors execute (at least 2 flash
     * READ transactions); status writeback on both descriptors; final INT=1
     * causes trd_comp_intr_status[0]=1 after chain end.
     *
     * Pass criterion: flash_stub_tx_count>=2; trd_comp_intr_status[0]=1;
     *                 both writeback words show COMPLETE bit set.
     */
    void tc_xspi_acmd_003_cont_flag_chained_descriptors();

    /**
     * @brief TC_XSPI_ACMD_004: ACMD INT flag generates completion interrupt.
     *
     * Creates a single READ descriptor with INT=1 (cmd_flags bit 8), CONT=0.
     * Triggers thread 0. Verifies: trd_comp_intr_status(0x120) bit 0=1; int_out
     * asserted. Then W1C-clears trd_comp_intr_status by writing 0x00000001
     * and verifies int_out deasserts.
     *
     * Pass criterion: trd_comp_intr_status[0]=1; int_out=true after execution;
     *                 int_out=false after W1C clear.
     */
    void tc_xspi_acmd_004_int_flag_interrupt_on_chain_end();

    /**
     * @brief TC_XSPI_ACMD_005: MB_XIP_EN on READ descriptor sets mode byte.
     *
     * Builds READ descriptor with MB_XIP_EN=1 (cmd_flags bit 6). Triggers.
     * Verifies that m_last_flash_ext.write_data==1 (XIP-entry signal conveyed
     * to the flash stub via ext.write_data per cdma_handle_trigger() READ path).
     *
     * Pass criterion: m_last_flash_ext.write_data==xip_en_mb_val (0xA5, pre-programmed);
     *                 trd_status[0]=0; trd_error_intr_status[0]=0 (no error).
     */
    void tc_xspi_acmd_005_mb_xip_en_on_read_descriptor();

    /**
     * @brief TC_XSPI_ACMD_006: MB_XIP_EN on non-READ generates DSC_ERROR.
     *
     * Builds PROGRAM descriptor (cmd_type=0x2100) with MB_XIP_EN=1 (cmd_flags
     * bit 6). Triggers thread 0. Verifies: no flash transaction issued;
     * status writeback at descriptor+40 has DSC_ERROR bit (bit 0) set;
     * trd_error_intr_status[0]=1; trd_status[0]=0.
     *
     * Pass criterion: flash_stub_tx_count remains 0 (no flash op);
     *                 writeback DSC_ERROR bit set; trd_error_intr_status[0]=1.
     */
    void tc_xspi_acmd_006_mb_xip_en_on_non_read_generates_dsc_error();

    /**
     * @brief TC_XSPI_ACMD_007: Descriptor status writeback verification.
     *
     * Phase A (success): issues READ descriptor and verifies writeback at
     * descriptor_base+0x28 has COMPLETE (bit 15) set, FAIL (bit 14)=0.
     * Phase B (failure): arms m_dma_error_count=1 on the data-path DMA write,
     * issues another READ descriptor, verifies writeback has BUS_ERROR (bit 1)
     * and FAIL (bit 14) and COMPLETE (bit 15) set; verifies
     * dma_target_error_l (0x150) is non-zero (failing address captured).
     *
     * Pass criterion: Phase A writeback = COMPLETE only; Phase B writeback has
     *                 BUS_ERROR|FAIL|COMPLETE; dma_target_error_l != 0.
     */
    void tc_xspi_acmd_007_descriptor_status_writeback();

    /**
     * @brief TC_XSPI_ACMD_008: ACMD ERASE_SECTORS descriptor.
     *
     * Builds descriptor with cmd_type=0x1000 (ERASE_SECTORS), xspi_ptr=0x00100000,
     * counter=0x002 (erase 3 sectors). No sys_mem_pointer needed. Triggers thread 0.
     * Verifies: xspi_bus_socket[0] receives WREN (0x06) + ERASE_64KB (0xD8);
     * m_last_flash_ext.write_data==3 (sector count); status writeback COMPLETE=1;
     * trd_status[0]=0.
     *
     * Pass criterion: flash_stub_tx_count>=2; opcode=0xD8; write_data==3;
     *                 trd_status[0]=0; COMPLETE bit in writeback word set.
     */
    void tc_xspi_acmd_008_acmd_erase_sectors_descriptor();

    /**
     * @brief TC_XSPI_MDR_003: ACMD mode — cmd_reg0 only TRD_NUM decoded.
     *
     * In ACMD mode, writes cmd_reg0=0x0300FFFF (TRD_NUM=3, lower 24 bits=garbage).
     * Verifies thread 3 executes the descriptor (trd_status[3] observed as 0
     * after completion since LT model runs inline), threads 0..2 unaffected.
     * Confirms cmd_ignored=0 (valid trigger, not rejected).
     * Uses a zeroed descriptor (NOP), so no flash transaction required: just
     * confirms the correct thread was dispatched.
     *
     * Pass criterion: trd_error_intr_status[3]=0; intr_status.cmd_ignored=0;
     *                 trd_status[3]=0 after trigger.
     */

    /**
     * @brief TC_XSPI_ACMD_009: ACMD CDMA PROGRAM→READ chain (AXI master DMA→flash).
     *
     * Two 64-byte descriptors: PROGRAM (CONT,DMA_SEL) then READ (INT,DMA_SEL)
     * at the same flash address; 64-byte transfers. Verifies i_dma_socket trace,
     * flash WREN+PAGE+READ, descriptor status COMPLETE/FAIL, trd_comp, and that
     * read-back data in the DMA buffer matches the programmed source pattern.
     */
    void tc_xspi_acmd_009_program_read_chain_dma_flash_verify();


    /**
     * @brief TC_XSPI_ERR_001: ACMD misaligned descriptor address rejected.
     *
     * Sets cmd_reg2=0x00010010 (address % 64 = 16, not aligned). Triggers ACMD
     * thread 0. Verifies: NO descriptor fetch on i_dma_socket
     * (m_dma_fetch_count remains 0); trd_error_intr_status[0]=1; trd_status[0]=0
     * (thread immediately idle); int_out driven per enable/mask state.
     *
     * Pass criterion: trd_error_intr_status[0]=1; trd_status[0]=0;
     *                 flash_stub_tx_count unchanged (no flash op).
     */
    void tc_xspi_err_001_acmd_misaligned_descriptor();

    /**
     * @brief TC_XSPI_ERR_003: ACMD DMA bus error on descriptor fetch.
     *
     * Arms m_dma_error_count=1 so DMA stub returns TLM_GENERIC_ERROR_RESPONSE
     * on the first transaction (descriptor fetch). Triggers ACMD thread 0.
     * Verifies: intr_status.cdma_terr (bit 17) set; dma_target_error_l (0x150)
     * captures the failing descriptor address (0x00010000); trd_error_intr_status
     * bit 0 set; trd_status[0]=0.
     *
     * Pass criterion: cdma_terr=1; dma_target_error_l==0x00010000;
     *                 trd_error_intr_status[0]=1; trd_status[0]=0.
     */
    void tc_xspi_err_003_acmd_dma_bus_error_cdma_terr();

    /**
     * @brief TC_XSPI_INT_002: Thread error interrupt on ACMD misalignment.
     *
     * Enables trd_error_intr_en[1] (bit 1). Triggers ACMD thread 1 with a
     * misaligned descriptor address 0x00010010 (not 64-byte aligned). Verifies
     * trd_error_intr_status[1]=1; int_out asserted immediately (no descriptor
     * fetch). Verifies no DMA transaction occurred.
     *
     * Pass criterion: trd_error_intr_status[1]=1; int_out=true; flash_stub
     *                 tx_count unchanged.
     */
    void tc_xspi_int_002_trd_error_int_assertion();

    /**
     * @brief TC_XSPI_INT_003: int_out de-assertion after W1C of both error regs.
     *
     * Sets trd_comp_intr_status[0]=1 via ACMD READ with INT=1, and
     * trd_error_intr_status[1]=1 via misaligned ACMD thread 1, enabling both
     * thread interrupt enable bits so int_out is high. Writes W1C 0x00000001 to
     * trd_comp_intr_status — int_out remains high (error still set). Writes W1C
     * 0x00000002 to trd_error_intr_status — int_out goes low only after BOTH
     * sources are cleared.
     *
     * Pass criterion: int_out=false only after clearing both registers; W1C-only
     *                 partial clear keeps int_out=true.
     */
    void tc_xspi_int_003_int_out_deassertion_after_w1c();

    /**
     * @brief TC_XSPI_INT_004: intr_enable masks intr_status → int_out.
     *
     * Forces intr_status.cmd_ignored (bit 20) by triggering a busy ACMD thread.
     * With intr_enable=0x00000000 verifies int_out is NOT asserted despite
     * intr_status non-zero. Then enables the cmd_ignored bit in intr_enable;
     * verifies int_out asserts immediately.
     *
     * Pass criterion: int_out=false with intr_enable=0; int_out=true after
     *                 enabling cmd_ignored bit in intr_enable.
     */
    void tc_xspi_int_004_intr_enable_mask_gates_int_out();

    /**
     * @brief TC_XSPI_INT_005: cmd_ignored set on second ACMD trigger to busy thread.
     *
     * In ACMD mode (LT model runs inline so thread is never observed busy from
     * a second trigger). Verifies that after the first successful ACMD READ,
     * a second trigger with identical parameters completes again (LT), and
     * cmd_ignored is 0 (thread not busy in LT mode). Then manually sets trd_busy
     * by verifying the busy-thread guard: uses the fact that intr_status.cmd_ignored
     * is set when trd_busy is already set. Uses intr_status read to confirm.
     *
     * Note: In LT the model executes ACMD synchronously. The busy guard fires
     * only if trd_status.trd_busy[TRD_NUM] is already set when cmd_reg0 is
     * written. In LT the thread completes before the second write can be issued.
     * This test verifies that two sequential triggers both complete without error
     * and cmd_ignored remains 0 (confirming the silent-ignore semantics).
     *
     * Pass criterion: both triggers complete; cmd_ignored=0; trd_status[0]=0
     *                 after both triggers; no error.
     */
    void tc_xspi_int_005_cmd_ignored_bit_set();

    /**
     * @brief TC_XSPI_INT_006: trd_error_intr_en masking of int_out.
     *
     * Forces thread-2 trd_error via injected DMA bus error on descriptor fetch.
     * With trd_error_intr_en[2]=0, verifies int_out remains LOW despite
     * trd_error_intr_status[2]=1. Then writes trd_error_intr_en with bit 2=1;
     * verifies int_out asserts immediately due to already-set bit 2.
     *
     * Pass criterion: int_out=false when trd_error_intr_en[2]=0;
     *                 int_out=true after enabling bit 2 in trd_error_intr_en.
     */
    void tc_xspi_int_006_trd_error_intr_en_masking();

    /**
     * @brief TC_XSPI_INT_007: reset_in clears all interrupt state and int_out.
     *
     * With trd_comp_intr_status and trd_error_intr_status both non-zero and
     * int_out high, asserts reset_in. Verifies all three interrupt status
     * registers (trd_comp_intr_status, trd_error_intr_status, intr_status)
     * return to 0x00000000 and int_out deasserts.
     *
     * Pass criterion: trd_comp_intr_status=0; trd_error_intr_status=0;
     *                 intr_status=0; int_out=false after reset.
     */
    void tc_xspi_int_007_reset_clears_int_out();

    /**
     * @brief TC_XSPI_CFG_005: dma_addr_width=0 uses only 32-bit descriptor address.
     *
     * Reads ctrl_features_reg (0xF04) to confirm dma_addr_width field value.
     * In ACMD mode, sets cmd_reg2=0x00030000 and cmd_reg3=0x00000000. Triggers
     * thread 0. Verifies descriptor is fetched from 0x00030000 (cmd_reg3 unused
     * when dma_addr_width==32, i.e., the default DUT configuration with
     * dma_addr_width=32 encoded as ctrl_features_reg bits[9:8]==0b00).
     * Confirms correct fetch by arming m_dma_buf at 0x00030000.
     *
     * Pass criterion: descriptor fetched from 0x00030000; trd_status[0]=0;
     *                 no trd_error set; ctrl_features_reg.dma_addr_width readable.
     */
    void tc_xspi_cfg_005_dma_addr_width_32bit();

    /**
     * @brief TC_XSPI_REG_006: W1C behavior of intr_status forced by ACMD CMD_IGNORED.
     *
     * Forces intr_status.cmd_ignored (bit 20) by triggering ACMD on an already-busy
     * thread (manually set trd_status[0]=busy via first ACMD trigger, then
     * immediately trigger again — in LT this is not possible since thread
     * completes inline). Uses the busy-trigger simulation: issue first trigger
     * to complete normally, then trigger a second ACMD with m_dma_error on the
     * descriptor fetch + re-trigger same thread. Instead: directly tests W1C
     * by using intr_status.cmd_ignored bit set via the ACMD busy guard. The
     * practical approach is to set intr_status.cmd_ignored via a CMD_IGNORED
     * condition (out-of-range bank in ACMD mode) and then perform W1C.
     * Writes 0xFFFFFFFF to intr_status and reads back to confirm all bits cleared.
     *
     * Pass criterion: intr_status.cmd_ignored set before W1C; intr_status==0
     *                 after writing 0xFFFFFFFF; int_out deasserts.
     */
    void tc_xspi_reg_006_w1c_intr_status_clear();

    /**
     * @brief TC_XSPI_REG_008: W1C behavior of trd_error_intr_status via ACMD
     *        misaligned descriptor.
     *
     * Triggers a thread error on thread 1 by issuing ACMD with descriptor
     * address 0x00010010 (not 64-byte aligned). Reads trd_error_intr_status
     * and confirms bit 1 is set. Writes 0x00000002 (W1C bit 1); reads back
     * and confirms trd_error_intr_status==0x00000000.
     *
     * Pass criterion: trd_error_intr_status[1]=1 before W1C;
     *                 trd_error_intr_status==0 after W1C write.
     */
    void tc_xspi_reg_008_w1c_trd_error_intr_status();

    /**
     * @brief run_func012_tests: top-level entry point for FUNC_XSPI_012 suite.
     *
     * Orchestrates all 21 test cases for FUNC_XSPI_012 (ACMD/CDMA Mode
     * Descriptor-Based DMA Engine) in document order:
     *   TC_XSPI_ACMD_001–008 (primary ACMD tests),
     *   TC_XSPI_MDR_003, TC_XSPI_MDR_006,
     *   TC_XSPI_ERR_001, TC_XSPI_ERR_003,
     *   TC_XSPI_INT_002–007,
     *   TC_XSPI_CFG_005, TC_XSPI_REG_006, TC_XSPI_REG_008.
     * Called from run_tests() after run_func011_tests() completes.
     */
    void run_func012_tests();

    // =========================================================================
    // FUNC_XSPI_013 Test Case Methods
    // Implemented in: xspi_ctrl_func013_test.cpp
    // =========================================================================

    /**
     * @brief TC_XSPI_XIP_001: XIP entry via Direct-mode READ — xip_en_mb_val=0xA5
     *        inserted as mode byte in cdns_extension.write_data on the first READ
     *        after arming mode_bit_xip_en; second READ has no mode byte.
     *
     * Pass criterion: first READ write_data==0xA5; xip_mode_cfg[0]==1;
     *                 direct_access_cfg bit8 self-cleared; second READ write_data==0x00.
     */
    void tc_xspi_xip_001_entry_mode_byte_insertion();

    /**
     * @brief TC_XSPI_XIP_002: Direct-mode XIP exit via mode_bit_xip_dis (bit 9):
     *        xip_dis_mb_val=0xFF inserted on exit READ; xip_en[0] cleared;
     *        direct_access_cfg bit9 self-cleared; subsequent READ has no mode byte.
     *
     * Pass criterion: exit READ write_data==0xFF; xip_mode_cfg[0]==0;
     *                 direct_access_cfg[9]==0; post-exit READ write_data==0x00.
     */
    void tc_xspi_xip_002_exit_via_mode_bit_xip_dis();

    /**
     * @brief TC_XSPI_XIP_003: Non-READ (AXI WRITE) to XIP-active bank 0 in
     *        Direct mode: intr_status.dir_cmd_err (bit 26) set; no flash
     *        WRITE transaction dispatched; AXI WRITE returns TLM_OK_RESPONSE.
     *
     * Pass criterion: dir_cmd_err set; flash_stub_tx_count==0 for the rejected
     *                 WRITE; AXI response == TLM_OK_RESPONSE.
     */
    void tc_xspi_xip_003_non_read_while_xip_active();

    /**
     * @brief TC_XSPI_XIP_004: Per-bank XIP isolation — writing xip_mode_cfg
     *        to activate XIP on bank 1 only must not affect bank 0 Direct-mode
     *        READ transactions (no dir_cmd_err, no mode byte, flash transaction
     *        dispatched normally).
     *
     * Pass criterion: bank 0 READ: resp==TLM_OK_RESPONSE; tx_count==1;
     *                 write_data==0x00; bank_num==0; dir_cmd_err==0; cmd_ignored==0.
     */
    void tc_xspi_xip_004_per_bank_xip_isolation();

    /**
     * @brief Multi-Target Flash_Program_and_Verify_direct — multi-bank data
     *        integrity in Direct mode (banks 0–3): program unique 32-bit patterns
     *        per bank via dac_bank_num + AXI WRITE, then READ back and compare;
     *        poll init_comp, ctrl_idle before configuration and before each
     *        direct_access_cfg bank select.
     */
    void tc_multi_target_flash_program_verify_direct();

    /**
     * @brief run_func013_tests: top-level entry point for FUNC_XSPI_013 suite.
     *
     * Orchestrates XIP_001–003 plus Multi-Target Flash_Program_and_Verify_direct
     * (XIP_004/REG_010 may be commented per build). XIP_005 (PIO MB_XIP_DIS) is
     * run from run_func011_tests(); ACMD_005/006 from run_func012_tests().
     * Called from run_tests() after run_func012_tests() completes.
     */
    void run_func013_tests();

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * @brief Bind all ports between DUT and testbench components
     *
     * Performs the complete port binding sequence:
     *  1. test->initiator_socket → dut->t_reg_socket
     *  2. t_axi_slave_initiator  → dut->t_axi_slave_socket
     *  3. por_initiator          → dut->PoR_input_signals
     *  4. dut->xspi_bus_socket[i] → xspi_flash_target[i]->target_socket  (each CS)
     *  5. dut->i_dma_socket      → dma_target_socket
     *  6. dut->reset_in          ↔ rst_n_sig
     *  7. dut->int_out           ↔ int_out_sig
     */
    void bind_ports();

    /**
     * @brief Apply active-low reset to the DUT
     *
     * Asserts reset_in (drives rst_n_sig to false), waits 10 ns for reset
     * propagation, then deasserts reset_in (drives rst_n_sig to true) and
     * waits a further 30 ns for internal initialization to complete.
     */
    void apply_reset();

    /**
     * @brief Pre-device-model hook for the xSPI flash TLM path
     *
     * Receives xSPI bus transactions from the DUT's xspi_bus_socket[]
     * initiator sockets. Returns TLM_OK_RESPONSE without modifying data.
     * Logs the transaction at verbosity level 3.
     *
     * @param trans         TLM generic payload from DUT flash initiator socket
     * @param delay         Local time offset (forwarded unchanged)
     * @param target_index  Which xspi_bus_socket (0 .. m_num_targets-1) received the b_transport
     */
    bool flash_trans_prelude(tlm::tlm_generic_payload& trans,
                            sc_core::sc_time& delay, int target_index);

    /**
     * @brief b_transport stub handler for dma_target_socket
     *
     * Receives AXI master DMA transactions from the DUT's i_dma_socket.
     * Returns TLM_OK_RESPONSE without modifying data. Logs the transaction
     * at verbosity level 3.
     *
     * @param trans TLM generic payload from DUT DMA initiator socket
     * @param delay Local time offset (forwarded unchanged)
     */
    void b_transport_dma_stub(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay);

    /// @brief Logger instance for structured CSML logging
    mutable CsmlLogger logger;
};
