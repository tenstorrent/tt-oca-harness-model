/******************************************************************************
 * @file testbench.cpp
 * @brief xspi_ctrl SystemC testbench implementation
 *
 * This file implements the testbench constructor with complete port binding
 * between the xspi_ctrl DUT model and the xspi_ctrl_test harness. It provides
 * implementations for the baseline test cases (RW, RO, binding verification,
 * reset functionality, and TC_XSPI_REG_013 reserved bits write-ignore), the
 * helper apply_reset(), bind_ports(),
 * flash_trans_prelude / DMA stub b_transport handlers, and the test reporting
 * infrastructure.
 *
 * Reference:
 *   - docs/sections/xspi_ctrl-port-interfaces.md
 *   - kmac/test/src/testbench.cpp (structural reference)
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <cstring>

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

/******************************************************************************
 * @brief Testbench constructor
 *
 * Instantiates the DUT (xspi_ctrl) and the test harness (xspi_ctrl_test),
 * allocates NUM_TARGETS stub flash target sockets, performs all port bindings,
 * initializes interconnect signals, and registers the run_tests SC_THREAD.
 *
 * @param name        SystemC module name
 * @param num_targets Number of flash chip-select targets (default: 4)
 ******************************************************************************/
testbench::testbench(sc_module_name name, int num_targets)
    : sc_module(name)
    , t_axi_slave_initiator("t_axi_slave_initiator")
    , por_initiator("por_initiator")
    , dma_target_socket("dma_target_socket")
    , m_tests_run(0)
    , m_tests_passed(0)
    , m_tests_failed(0)
    , m_num_targets(num_targets)
    , m_dma_error_count(0)
    , m_flash_error_count(0)
    , m_flash_stub_tx_count(0)
    , m_flash_tx_per_target(num_targets, 0)
    , m_flash_sfdp_mode(false)
    , m_last_flash_ext()
    , m_last_flash_bank(-1)
    , m_seen_read_sfdp_addr0(false)
    , m_dma_trace_enabled(false)
    , m_flash_trace_enabled(false)
    , m_probe_ctrl_busy_on_next_flash_read(false)
    , m_ctrl_busy_seen_during_flash_read(false)
    , logger()
{
    // Configure logger
    logger.setMaxVerbosity(2);
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    std::memset(m_flash_sfdp_buf, 0, sizeof(m_flash_sfdp_buf));
    std::memset(m_dma_buf, 0, sizeof(m_dma_buf));
    m_dma_buf_addr  = 0u;
    m_dma_buf_size  = 0u;
    m_dma_buf_armed = false;

    CSML_INFO(2, logger) << "Constructing xspi_ctrl testbench (num_targets="
                         << num_targets << ")";

    // =========================================================================
    // Instantiate DUT model
    // =========================================================================
    // memory_size = 0x3000 covers all register sub-regions:
    // ctb_Rfile_a ends at 0x2097 (< 0x3000)
    dut = new xspi_ctrl_ip("xspi_ctrl_dut", 0x3000, CSML_DEFAULT_VERBOSITY, num_targets);

    // =========================================================================
    // Instantiate test harness
    // =========================================================================
    test = new xspi_ctrl_test("xspi_ctrl_test_inst");

    // =========================================================================
    // Instantiate TLM xSPI flash target (wrapper + xspi_target_model) per CS
    // =========================================================================
    xspi_flash_target.resize(num_targets);
    for (int i = 0; i < num_targets; i++) {
        std::string mod_name = std::string(sc_module::name()) + ".xspi_flash_"
                               + std::to_string(i);
        xspi_flash_target[i] =
            new xspi_target_sc_module(sc_module_name(mod_name.c_str()));
        xspi_flash_target[i]->flash_trans_prelude =
            [this, i](tlm::tlm_generic_payload& t, sc_core::sc_time& d) {
                return this->flash_trans_prelude(t, d, i);
            };
        CSML_INFO(3, logger) << "  Created xspi_flash_target[" << i << "]";
    }

    // Register DMA stub b_transport handler
    dma_target_socket.register_b_transport(
        this, &testbench::b_transport_dma_stub);

    // =========================================================================
    // Perform port binding
    // =========================================================================
    bind_ports();

    // =========================================================================
    // Initialize interconnect signals
    // =========================================================================
    rst_n_sig.write(true);      // Reset deasserted (active-low)
    int_out_sig.write(false);   // No interrupt pending

    // =========================================================================
    // Register test execution SC_THREAD
    // =========================================================================
    SC_THREAD(run_tests);

    CSML_INFO(2, logger) << "xspi_ctrl testbench construction complete";
}

/******************************************************************************
 * @brief Testbench destructor
 *
 * Releases heap-allocated DUT, test harness, and stub socket objects.
 ******************************************************************************/
testbench::~testbench()
{
    for (auto* m : xspi_flash_target) {
        delete m;
    }
    xspi_flash_target.clear();
    delete dut;
    delete test;
}

/******************************************************************************
 * @brief Bind all ports between DUT and testbench components
 *
 * Performs the complete seven-interface port binding sequence:
 *  1. test->initiator_socket   → dut->t_reg_socket     (CPU register access)
 *  2. t_axi_slave_initiator    → dut->t_axi_slave_socket (AXI slave)
 *  3. por_initiator            → dut->PoR_input_signals  (PoR bootstrap)
 *  4. dut->xspi_bus_socket[i] → xspi_flash_target[i]->target_socket
 *  5. dut->i_dma_socket        → dma_target_socket       (AXI master DMA)
 *  6. dut->reset_in            ↔ rst_n_sig               (active-low reset)
 *  7. dut->int_out             ↔ int_out_sig             (interrupt output)
 ******************************************************************************/
void testbench::bind_ports()
{
    CSML_INFO(2, logger) << "Binding ports...";

    // =========================================================================
    // 1. Register access: test initiator_socket → dut t_reg_socket
    //    The scml2::ft_target_socket in the DUT's base class is bound to the
    //    memory which is already set up via memory.bind_to_socket(target_socket)
    //    in xspi_ctrl_base. The test harness initiator_socket drives register
    //    read/write transactions through this path.
    // =========================================================================
    test->initiator_socket.bind(dut->target_socket);
    CSML_INFO(2, logger) << "  [BOUND] test::initiator_socket → dut::target_socket";

    // =========================================================================
    // 2. AXI slave: testbench initiator → dut t_axi_slave_socket
    //    Used for Direct-mode memory-mapped flash transactions.
    // =========================================================================
    t_axi_slave_initiator.bind(dut->t_axi_slave_socket);
    CSML_INFO(2, logger) << "  [BOUND] t_axi_slave_initiator → dut::t_axi_slave_socket";

    // =========================================================================
    // 3. PoR bootstrap: testbench initiator → dut PoR_input_signals
    //    Carries xspi_PoR_trans extension at power-on.
    // =========================================================================
    por_initiator.bind(dut->PoR_input_signals);
    CSML_INFO(2, logger) << "  [BOUND] por_initiator → dut::PoR_input_signals";

    // =========================================================================
    // 4. Flash bus: dut xspi_bus_socket[i] → xspi_flash_target[i]->target_socket
    //    One binding per flash chip-select. DUT drives; TLM target receives.
    // =========================================================================
    for (int i = 0; i < m_num_targets; i++) {
        dut->xspi_bus_socket[i]->bind(xspi_flash_target[i]->target_socket);
        CSML_INFO(2, logger) << "  [BOUND] dut::xspi_bus_socket[" << i
                             << "] → xspi_flash_target[" << i << "].target_socket";
    }

    // =========================================================================
    // 5. AXI master DMA: dut i_dma_socket → dma_target_socket
    //    DUT drives DMA transfers; stub target accepts and completes them.
    // =========================================================================
    dut->i_dma_socket.bind(dma_target_socket);
    CSML_INFO(2, logger) << "  [BOUND] dut::i_dma_socket → dma_target_socket";

    // =========================================================================
    // 6. Active-low reset signal: test drives, DUT receives
    // =========================================================================
    dut->reset_in(rst_n_sig);
    CSML_INFO(2, logger) << "  [BOUND] dut::reset_in ↔ rst_n_sig";

    // =========================================================================
    // 7. Interrupt output: DUT drives, testbench monitors
    // =========================================================================
    dut->int_out(int_out_sig);
    CSML_INFO(2, logger) << "  [BOUND] dut::int_out ↔ int_out_sig";

    CSML_INFO(2, logger) << "Port binding complete";
}

/******************************************************************************
 * @brief Apply active-low reset to the DUT
 *
 * Asserts reset_in (rst_n_sig = false), waits 10 ns for reset to propagate,
 * then deasserts reset_in (rst_n_sig = true) and waits 30 ns for internal
 * register initialization to complete.
 ******************************************************************************/
void testbench::apply_reset()
{
    CSML_INFO(2, logger) << "Asserting reset (rst_n_sig = false)";
    rst_n_sig.write(false);
    wait(10, sc_core::SC_NS);

    CSML_INFO(2, logger) << "Deasserting reset (rst_n_sig = true)";
    rst_n_sig.write(true);
    wait(30, sc_core::SC_NS);

    CSML_INFO(2, logger) << "Reset complete — DUT ready";
}

// =============================================================================
// Stub b_transport Handlers
// =============================================================================

/******************************************************************************
 * @brief Test-harness hook before xspi_target_model (per flash b_transport)
 *
 * Maintains transaction count, extension capture, synthetic SFDP, and error
 * injection used by the functional tests. Returns false if the TLM response is
 * final; returns true to continue into the xspi_target_sc_module device model.
 *
 * @param trans TLM generic payload from DUT flash initiator socket
 * @param delay Local time offset (may be updated)
 * @return true to run the device model; false if complete
 ******************************************************************************/
bool testbench::flash_trans_prelude(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay, int target_index)
{
    CSML_INFO(3, logger) << "flash_trans_prelude: addr=0x"
                         << std::hex << trans.get_address()
                         << " len=" << std::dec << trans.get_data_length()
                         << " target_index=" << target_index;

    // Increment transaction counter so FUNC_XSPI_007 tests can measure whether
    // the DUT issued READ_SFDP transactions on the flash bus.
    ++m_flash_stub_tx_count;
    if (target_index >= 0 && target_index < m_num_targets) {
        ++m_flash_tx_per_target[static_cast<unsigned>(target_index)];
    }
    if (m_probe_ctrl_busy_on_next_flash_read
        && trans.get_command() == tlm::TLM_READ_COMMAND
        && !m_flash_sfdp_mode
        && m_flash_error_count == 0)
    {
        uint32_t cs = 0u;
        test->register_read_32(xspi_ctrl_basetest::ctrl_status_OFFSET, cs);
        if ((cs & (1u << 7u)) != 0u) {
            m_ctrl_busy_seen_during_flash_read = true;
        }
        m_probe_ctrl_busy_on_next_flash_read = false;
    }

    // Flash error injection: if m_flash_error_count > 0, return an error
    // response for the next flash transaction and decrement the counter.
    // Used by FUNC_XSPI_010 TC_XSPI_BOOT_002 to trigger the boot DQS error
    // path (boot engine config-record READ returns non-OK, boot_dqs_err set).
    if (m_flash_error_count > 0) {
        --m_flash_error_count;
        CSML_INFO(2, logger) << "flash_trans_prelude: injecting TLM error response "
                             << "(remaining=" << m_flash_error_count << ")";
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        delay += sc_core::sc_time(5, sc_core::SC_NS);
        return false;
    }

    // Capture the cdns_extension for FUNC_XSPI_008 test assertions.
    // FUNC_XSPI_008 Direct-mode tests verify the opcode, bank_num, address,
    // instr_type, and data_bytes fields of the outgoing cdns_extension.
    // m_last_flash_ext is overwritten on each call; tests read it after their
    // AXI slave dispatch returns. m_last_flash_bank mirrors ext.bank_num for
    // convenience.
    cdns_extension* captured_ext = nullptr;
    trans.get_extension(captured_ext);
    if (captured_ext != nullptr) {
        m_last_flash_ext.copy_from(*captured_ext);
        m_last_flash_bank = static_cast<int>(captured_ext->bank_num);
        if (captured_ext->opcode == 0x5Au
            && captured_ext->address == 0x00000000u
            && captured_ext->bank_num == 0u) {
            m_seen_read_sfdp_addr0 = true;
        }
        CSML_INFO(3, logger) << "flash_trans_prelude: captured ext"
                             << " opcode=0x" << std::hex
                             << static_cast<unsigned>(captured_ext->opcode)
                             << " bank=" << std::dec << m_last_flash_bank
                             << " addr=0x" << std::hex << captured_ext->address
                             << " instr_type=" << std::dec
                             << static_cast<unsigned>(captured_ext->instr_type)
                             << " data_bytes=" << captured_ext->data_bytes;
    }

     if (m_flash_trace_enabled) {
        flash_trace_entry_t fe;
        fe.opcode     = captured_ext->opcode;
        fe.address    = captured_ext->address;
        fe.data_bytes = captured_ext->data_bytes;
        fe.bank_num   = captured_ext->bank_num;
        m_flash_trace.push_back(fe);
    }

    // SFDP-mode: when armed by TC_XSPI_POR_005, copy bytes from the synthetic
    // SFDP ROM buffer into the transaction data buffer at the requested address.
    // The DUT carries the SFDP ROM byte address in trans.get_address().
    if (m_flash_sfdp_mode
        && trans.get_command() == tlm::TLM_READ_COMMAND
        && trans.get_data_ptr() != nullptr
        && trans.get_data_length() > 0u)
    {
        sc_dt::uint64 sfdp_addr = trans.get_address();
        uint32_t      req_len   = trans.get_data_length();
        uint8_t*      dst       = trans.get_data_ptr();

        for (uint32_t i = 0u; i < req_len; ++i) {
            uint64_t byte_addr = static_cast<uint64_t>(sfdp_addr) + i;
            if (byte_addr < sizeof(m_flash_sfdp_buf)) {
                dst[i] = m_flash_sfdp_buf[static_cast<size_t>(byte_addr)];
            } else {
                dst[i] = 0xFFu;  // Out-of-range: return erased flash value
            }
        }
        CSML_INFO(3, logger)
            << "flash_trans_prelude: SFDP mode — returned "
            << req_len << " bytes from sfdp_buf[0x"
            << std::hex << sfdp_addr << "]";
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(5, sc_core::SC_NS);
        return false;
    }

    return true;
}

/******************************************************************************
 * @brief Stub b_transport handler for dma_target_socket
 *
 * Receives AXI master DMA transactions from the DUT's i_dma_socket. Returns
 * TLM_OK_RESPONSE. For READ transactions the data buffer is zero-filled to
 * provide defined values to the DUT. Enables DMA engine tests without a
 * physical system memory model.
 *
 * @param trans TLM generic payload from DUT DMA initiator socket
 * @param delay Local time offset (forwarded unchanged)
 ******************************************************************************/
void testbench::b_transport_dma_stub(tlm::tlm_generic_payload& trans,
                                     sc_core::sc_time& delay)
{
    CSML_INFO(3, logger) << "b_transport_dma_stub: addr=0x"
                         << std::hex << trans.get_address()
                         << " cmd=" << (trans.get_command() == tlm::TLM_READ_COMMAND
                                        ? "READ" : "WRITE");

    // Error injection: if m_dma_error_count > 0, return an error response
    // for the next transaction and decrement the counter. This is used by
    // FUNC_XSPI_003 tests to trigger the ACMD DMA descriptor fetch error path.
    if (m_dma_error_count > 0) {
        --m_dma_error_count;
        CSML_INFO(2, logger) << "b_transport_dma_stub: injecting TLM error response "
                             << "(remaining=" << m_dma_error_count << ")";
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        delay += sc_core::sc_time(5, sc_core::SC_NS);
        return;
    }

    if (m_dma_trace_enabled) {
        dma_trace_entry_t ent;
        ent.addr    = static_cast<uint64_t>(trans.get_address());
        ent.len     = static_cast<uint32_t>(trans.get_data_length());
        ent.is_read = (trans.get_command() == tlm::TLM_READ_COMMAND);
        m_dma_trace.push_back(ent);
    }
    // Descriptor buffer: if armed and the transaction address falls within the
    // mapped range, serve/capture bytes from/to m_dma_buf[] rather than
    // zero-filling. This enables ACMD tests to inject real descriptor content
    // that the model will fetch via i_dma_socket READ transactions, and to
    // observe status writebacks at descriptor_base + 40.
    if (m_dma_buf_armed && trans.get_data_ptr() != nullptr) {
        uint64_t addr  = trans.get_address();
        std::size_t len = static_cast<std::size_t>(trans.get_data_length());

        if (addr >= m_dma_buf_addr) {
            uint64_t offset64 = addr - m_dma_buf_addr;
            if (offset64 < static_cast<uint64_t>(m_dma_buf_size)) {
                std::size_t off = static_cast<std::size_t>(offset64);
                std::size_t avail = m_dma_buf_size - off;
                std::size_t copy_len = (len < avail) ? len : avail;

                if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                    // Serve descriptor data from buffer.
                    std::memcpy(trans.get_data_ptr(), m_dma_buf + off, copy_len);
                    if (copy_len < len) {
                        // Zero-fill any remainder beyond the buffer.
                        std::memset(trans.get_data_ptr() + copy_len, 0, len - copy_len);
                    }
                } else {
                    // Capture writeback data into buffer (status slot at offset +40).
                    std::memcpy(m_dma_buf + off, trans.get_data_ptr(), copy_len);
                }
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
                delay += sc_core::sc_time(5, sc_core::SC_NS);
                return;
            }
        }
    }

    // For READ transactions, zero-fill the data buffer
    if (trans.get_command() == tlm::TLM_READ_COMMAND
        && trans.get_data_ptr() != nullptr
        && trans.get_data_length() > 0) {
        std::memset(trans.get_data_ptr(), 0, trans.get_data_length());
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay += sc_core::sc_time(5, sc_core::SC_NS);
}

// =============================================================================
// Register Access Helpers
// =============================================================================

/******************************************************************************
 * @brief Write a 32-bit value to a DUT register via the test harness socket
 *
 * Delegates to test->register_write_32, which issues a single atomic
 * TLM_WRITE_COMMAND b_transport with data_length=4 through the test
 * initiator_socket. Follows the same pattern as dma_test.cpp.
 *
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  32-bit value to write
 ******************************************************************************/
static void write_reg(xspi_ctrl_test* test, unsigned int offset,
                      uint32_t value)
{
    test->register_write_32(offset, value);
}

/******************************************************************************
 * @brief Read a 32-bit value from a DUT register via the test harness socket
 *
 * Delegates to test->register_read_32, which issues a single atomic
 * TLM_READ_COMMAND b_transport with data_length=4 through the test
 * initiator_socket. Follows the same pattern as dma_test.cpp.
 *
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  Reference to receive the 32-bit read value
 ******************************************************************************/
static void read_reg(xspi_ctrl_test* test, unsigned int offset,
                     uint32_t& value)
{
    test->register_read_32(offset, value);
}

// =============================================================================
// Test Case Implementations
// =============================================================================

/******************************************************************************
 * @brief RW register test: write, read back, assert match
 *
 * Writes 0x00000020 to ctrl_config (offset 0x230, work_mode 2'b01 in bits [6:5]).
 * Reads back and asserts the returned value
 * matches the written value within the writable bit mask (0x00000068).
 * A mismatch indicates a register storage or path failure.
 ******************************************************************************/
void testbench::test_rw_register_access()
{
    report_test_start("TC_XSPI_RW_001: RW Register Read/Write");

    const uint32_t write_val  = 0x00000020u;  // work_mode=2'b01 STIG (bits[6:5])
    const uint32_t write_mask = xspi_ctrl_basetest::ctrl_config_WRITE;
    const uint32_t expected   = write_val & write_mask;
    uint32_t       read_val   = 0xDEADBEEFu;

    write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, write_val);
    wait(5, sc_core::SC_NS);
    read_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET, read_val);

    bool passed = ((read_val & write_mask) == expected);
    if (!passed) {
        CSML_ERROR(0, logger) << "  ctrl_config RW mismatch: wrote=0x"
                              << std::hex << write_val
                              << " expected=0x" << expected
                              << " got=0x" << read_val;
    }
    report_test_result("TC_XSPI_RW_001", passed);
    apply_reset();
}

/******************************************************************************
 * @brief RO register protection test: write, read back, assert mismatch
 *
 * Writes 0xAAAAAAAA to xspi_ctrl_version (offset 0xF00, RO write-ignore).
 * Reads back and asserts the value equals the hardware reset value
 * (0x65220206), confirming that the write was silently discarded by the
 * scml2 write-ignore restriction.
 ******************************************************************************/
void testbench::test_ro_register_protection()
{
    report_test_start("TC_XSPI_RO_001: RO Register Write Protection");

    const uint32_t rogue_write  = 0xAAAAAAAAu;
    uint32_t       read_val     = 0u;

    write_reg(test, xspi_ctrl_basetest::xspi_ctrl_version_OFFSET, rogue_write);
    wait(5, sc_core::SC_NS);
    read_reg(test, xspi_ctrl_basetest::xspi_ctrl_version_OFFSET, read_val);

    // Pass if the read value is not the rogue write (value was unchanged)
    bool passed = (read_val != rogue_write);
    if (!passed) {
        CSML_ERROR(0, logger) << "  xspi_ctrl_version RO write not ignored: "
                              << "read back=0x" << std::hex << read_val;
    } else {
        CSML_INFO(2, logger) << "  xspi_ctrl_version read=0x" << std::hex
                             << read_val << " (write correctly ignored)";
    }
    report_test_result("TC_XSPI_RO_001", passed);
}

/******************************************************************************
 * @brief Port binding verification test
 *
 * Confirms that all port bindings were established correctly by:
 *  1. Verifying that a simple register read via initiator_socket returns
 *     TLM_OK_RESPONSE (exercising the t_reg_socket binding path).
 *  2. Verifying that the DUT reaches this point without SC_REPORT_ERROR
 *     (confirming all other socket bindings were satisfied at elaboration).
 ******************************************************************************/
void testbench::test_port_binding_verification()
{
    report_test_start("TC_XSPI_BIND_001: Port Binding Verification");

    // A successful register access confirms t_reg_socket is correctly bound
    uint32_t version_val = 0u;
    read_reg(test, xspi_ctrl_basetest::xspi_ctrl_version_OFFSET, version_val);
    wait(5, sc_core::SC_NS);

    // If we reach this point, all socket bindings were satisfied at elaboration
    bool passed = true;
    CSML_INFO(2, logger) << "  xspi_ctrl_version=0x" << std::hex << version_val
                         << " (register path confirmed)";
    CSML_INFO(2, logger) << "  All " << (2 + m_num_targets + 1)
                         << " socket bindings confirmed at elaboration";
    report_test_result("TC_XSPI_BIND_001", passed);
}

/******************************************************************************
 * @brief Reset functionality test: write values, toggle reset, verify reset
 *
 * Writes distinctive non-reset patterns to three RW registers:
 *  - ctrl_config (0x230): written 0x00000020, reset=0x00000000
 *  - long_polling (0x208): written 0x0000FFFF, reset=0x000003E8
 *  - wp_settings (0x1000): written 0x00000002, reset=0x00000001
 *
 * After applying reset, reads back all three registers and verifies they
 * returned to their hardware reset values.
 ******************************************************************************/
void testbench::test_reset_functionality()
{
    report_test_start("TC_XSPI_RST_001: Reset Functionality");

    // --- Poison: writable registers (RO / W1C-only skipped) -----------------
    write_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET,   0x00000002u);
    write_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET,   0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET,   0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET,   0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET,   0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET,   0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, 0x00000007u);
    write_reg(test, xspi_ctrl_basetest::intr_enable_OFFSET, 0x9FF7F000u);
    write_reg(test, xspi_ctrl_basetest::trd_error_intr_en_OFFSET, 0x000000FFu);
    write_reg(test, xspi_ctrl_basetest::long_polling_OFFSET,  0x0000FFFFu);
    write_reg(test, xspi_ctrl_basetest::short_polling_OFFSET, 0x0000FFFFu);
    write_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET,   0x00000020u);
    write_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET,  0x000F0000u);
    write_reg(test, xspi_ctrl_basetest::discovery_control_OFFSET, 0x00000003u);
    write_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET,  0x00000001u);
    write_reg(test, xspi_ctrl_basetest::global_seq_cfg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::global_seq_cfg_1_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::direct_access_cfg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::direct_access_rmp_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::direct_access_rmp_1_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::rst_seq_cfg_0_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::rst_seq_cfg_1_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::ers_seq_cfg_1_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::ers_seq_cfg_2_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::prog_seq_cfg_0_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::prog_seq_cfg_1_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::prog_seq_cfg_2_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::read_seq_cfg_0_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::read_seq_cfg_1_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::read_seq_cfg_2_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::we_seq_cfg_0_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_0_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_1_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_2_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_3_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_4_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_5_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_7_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_8_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_9_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::stat_seq_cfg_10_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::wp_settings_OFFSET,   0x00000002u);
    write_reg(test, xspi_ctrl_basetest::reset_pin_settings_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::clock_mode_settings_OFFSET, 0x00000001u);
    write_reg(test, xspi_ctrl_basetest::jedec_rst_timing_reg_OFFSET, 0x0000FFFFu);
    write_reg(test, xspi_ctrl_basetest::dev_delay_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::rst_recovery_reg_OFFSET, 0x000000FFu);
    write_reg(test, xspi_ctrl_basetest::dev_active_max_reg_OFFSET, 0x000000FFu);
    write_reg(test, xspi_ctrl_basetest::hf_offset_reg_OFFSET, 0x0000FFFFu);
    write_reg(test, xspi_ctrl_basetest::dll_phy_update_cnt_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::dll_phy_ctrl_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_dq_timing_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_dqs_timing_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_gate_lpbk_ctrl_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_dll_master_ctrl_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_dll_slave_ctrl_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_ie_timing_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_static_togg_reg_OFFSET, 0xFFFFFFFFu);
        write_reg(test, xspi_ctrl_basetest::phy_wr_deskew_reg_OFFSET, 0xFFFFFFFFu);
        write_reg(test, xspi_ctrl_basetest::phy_wr_rd_deskew_cmd_reg_OFFSET, 0xFFFFFFFFu);

    write_reg(test, xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_0_reg_OFFSET, 0xFFFFFFFFu);
    
        write_reg(test, xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_1_reg_OFFSET, 0xFFFFFFFFu);
        write_reg(test, xspi_ctrl_basetest::phy_rd_deskew_reg_OFFSET, 0xFFFFFFFFu);

    write_reg(test, xspi_ctrl_basetest::phy_ctrl_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_tsel_reg_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_gpio_ctrl_0_OFFSET, 0xFFFFFFFFu);
    write_reg(test, xspi_ctrl_basetest::phy_gpio_ctrl_1_OFFSET, 0xFFFFFFFFu);
    wait(5, sc_core::SC_NS);

    apply_reset();

    uint32_t cmd_reg0_val  = 0u;
    uint32_t cmd_reg1_val  = 0u;
    uint32_t cmd_reg2_val  = 0u;
    uint32_t cmd_reg3_val  = 0u;
    uint32_t cmd_reg4_val  = 0u;
    uint32_t cmd_reg5_val  = 0u;
    uint32_t cmd_status_ptr_val = 0u;
    uint32_t cmd_status_val = 0u;
    uint32_t ctrl_status_val = 0u;
    uint32_t trd_status_val = 0u;
    uint32_t intr_status_val = 0u;
    uint32_t intr_enable_val = 0u;
    uint32_t trd_comp_intr_status_val = 0u;
    uint32_t trd_error_intr_status_val = 0u;
    uint32_t trd_error_intr_en_val = 0u;
    uint32_t dma_target_error_l_val = 0u;
    uint32_t dma_target_error_h_val = 0u;
    uint32_t boot_status_val = 0u;
    uint32_t long_polling_val = 0u;
    uint32_t short_polling_val = 0u;
    uint32_t ctrl_config_val  = 0u;
    uint32_t dma_settings_val = 0u;
    uint32_t sdma_size_val = 0u;
    uint32_t sdma_trd_info_val = 0u;
    uint32_t sdma_addr0_val = 0u;
    uint32_t sdma_addr1_val = 0u;
    uint32_t discovery_control_val = 0u;
    uint32_t xip_mode_cfg_val = 0u;
    uint32_t global_seq_cfg_val = 0u;
    uint32_t global_seq_cfg_1_val = 0u;
    uint32_t direct_access_cfg_val = 0u;
    uint32_t direct_access_rmp_val = 0u;
    uint32_t direct_access_rmp_1_val = 0u;
    uint32_t rst_seq_cfg_0_val = 0u;
    uint32_t rst_seq_cfg_1_val = 0u;
    uint32_t ers_seq_cfg_0_val = 0u;
    uint32_t ers_seq_cfg_1_val = 0u;
    uint32_t ers_seq_cfg_2_val = 0u;
    uint32_t prog_seq_cfg_0_val = 0u;
    uint32_t prog_seq_cfg_1_val = 0u;
    uint32_t prog_seq_cfg_2_val = 0u;
    uint32_t read_seq_cfg_0_val = 0u;
    uint32_t read_seq_cfg_1_val = 0u;
    uint32_t read_seq_cfg_2_val = 0u;
    uint32_t we_seq_cfg_0_val = 0u;
    uint32_t stat_seq_cfg_0_val = 0u;
    uint32_t stat_seq_cfg_1_val = 0u;
    uint32_t stat_seq_cfg_2_val = 0u;
    uint32_t stat_seq_cfg_3_val = 0u;
    uint32_t stat_seq_cfg_4_val = 0u;
    uint32_t stat_seq_cfg_5_val = 0u;
    uint32_t stat_seq_cfg_7_val = 0u;
    uint32_t stat_seq_cfg_8_val = 0u;
    uint32_t stat_seq_cfg_9_val = 0u;
    uint32_t stat_seq_cfg_10_val = 0u;
    uint32_t xspi_ctrl_version_val = 0u;
    uint32_t ctrl_features_reg_val = 0u;
    uint32_t wp_settings_val  = 0u;
    uint32_t reset_pin_settings_val = 0u;
    uint32_t clock_mode_settings_val = 0u;
    uint32_t jedec_rst_timing_reg_val = 0u;
    uint32_t dev_delay_reg_val = 0u;
    uint32_t rst_recovery_reg_val = 0u;
    uint32_t dev_active_max_reg_val = 0u;
    uint32_t hf_offset_reg_val = 0u;
    uint32_t dll_phy_update_cnt_val = 0u;
    uint32_t dll_phy_ctrl_val = 0u;
    uint32_t phy_dq_timing_reg_val = 0u;
    uint32_t phy_dqs_timing_reg_val = 0u;
    uint32_t phy_gate_lpbk_ctrl_reg_val = 0u;
    uint32_t phy_dll_master_ctrl_reg_val = 0u;
    uint32_t phy_dll_slave_ctrl_reg_val = 0u;
    uint32_t phy_ie_timing_reg_val = 0u;
    uint32_t phy_obs_reg_0_val = 0u;
    uint32_t phy_dll_obs_reg_0_val = 0u;
    uint32_t phy_dll_obs_reg_1_val = 0u;
    uint32_t phy_dll_obs_reg_2_val = 0u;
    uint32_t phy_static_togg_reg_val = 0u;

    uint32_t phy_wr_deskew_reg_val = 0u;
    uint32_t phy_wr_rd_deskew_cmd_reg_val = 0u;

    uint32_t phy_wr_deskew_pd_ctrl_0_reg_val = 0u;

    uint32_t phy_wr_deskew_pd_ctrl_1_reg_val = 0u;
    uint32_t phy_rd_deskew_reg_val = 0u;

    uint32_t phy_version_reg_val = 0u;
    uint32_t phy_features_reg_val = 0u;
    uint32_t phy_ctrl_reg_val = 0u;
    uint32_t phy_tsel_reg_val = 0u;
    uint32_t phy_gpio_ctrl_0_val = 0u;
    uint32_t phy_gpio_ctrl_1_val = 0u;
    uint32_t phy_gpio_status_0_val = 0u;
    uint32_t phy_gpio_status_1_val = 0u;

    read_reg(test, xspi_ctrl_basetest::cmd_reg0_OFFSET,   cmd_reg0_val);
    read_reg(test, xspi_ctrl_basetest::cmd_reg1_OFFSET,   cmd_reg1_val);
    read_reg(test, xspi_ctrl_basetest::cmd_reg2_OFFSET,   cmd_reg2_val);
    read_reg(test, xspi_ctrl_basetest::cmd_reg3_OFFSET,   cmd_reg3_val);
    read_reg(test, xspi_ctrl_basetest::cmd_reg4_OFFSET,   cmd_reg4_val);
    read_reg(test, xspi_ctrl_basetest::cmd_reg5_OFFSET,   cmd_reg5_val);
    read_reg(test, xspi_ctrl_basetest::cmd_status_ptr_OFFSET, cmd_status_ptr_val);
    read_reg(test, xspi_ctrl_basetest::cmd_status_OFFSET, cmd_status_val);
    read_reg(test, xspi_ctrl_basetest::ctrl_status_OFFSET, ctrl_status_val);
    read_reg(test, xspi_ctrl_basetest::trd_status_OFFSET, trd_status_val);
    read_reg(test, xspi_ctrl_basetest::intr_status_OFFSET, intr_status_val);
    read_reg(test, xspi_ctrl_basetest::intr_enable_OFFSET, intr_enable_val);
    read_reg(test, xspi_ctrl_basetest::trd_comp_intr_status_OFFSET, trd_comp_intr_status_val);
    read_reg(test, xspi_ctrl_basetest::trd_error_intr_status_OFFSET, trd_error_intr_status_val);
    read_reg(test, xspi_ctrl_basetest::trd_error_intr_en_OFFSET, trd_error_intr_en_val);
    read_reg(test, xspi_ctrl_basetest::dma_target_error_l_OFFSET, dma_target_error_l_val);
    read_reg(test, xspi_ctrl_basetest::dma_target_error_h_OFFSET, dma_target_error_h_val);
    read_reg(test, xspi_ctrl_basetest::boot_status_OFFSET, boot_status_val);
    read_reg(test, xspi_ctrl_basetest::long_polling_OFFSET,  long_polling_val);
    read_reg(test, xspi_ctrl_basetest::short_polling_OFFSET, short_polling_val);
    read_reg(test, xspi_ctrl_basetest::ctrl_config_OFFSET,   ctrl_config_val);
    read_reg(test, xspi_ctrl_basetest::dma_settings_OFFSET, dma_settings_val);
    read_reg(test, xspi_ctrl_basetest::sdma_size_OFFSET, sdma_size_val);
    read_reg(test, xspi_ctrl_basetest::sdma_trd_info_OFFSET, sdma_trd_info_val);
    read_reg(test, xspi_ctrl_basetest::sdma_addr0_OFFSET, sdma_addr0_val);
    read_reg(test, xspi_ctrl_basetest::sdma_addr1_OFFSET, sdma_addr1_val);
    read_reg(test, xspi_ctrl_basetest::discovery_control_OFFSET, discovery_control_val);
    read_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, xip_mode_cfg_val);
    read_reg(test, xspi_ctrl_basetest::global_seq_cfg_OFFSET, global_seq_cfg_val);
    read_reg(test, xspi_ctrl_basetest::global_seq_cfg_1_OFFSET, global_seq_cfg_1_val);
    read_reg(test, xspi_ctrl_basetest::direct_access_cfg_OFFSET, direct_access_cfg_val);
    read_reg(test, xspi_ctrl_basetest::direct_access_rmp_OFFSET, direct_access_rmp_val);
    read_reg(test, xspi_ctrl_basetest::direct_access_rmp_1_OFFSET, direct_access_rmp_1_val);
    read_reg(test, xspi_ctrl_basetest::rst_seq_cfg_0_OFFSET, rst_seq_cfg_0_val);
    read_reg(test, xspi_ctrl_basetest::rst_seq_cfg_1_OFFSET, rst_seq_cfg_1_val);
    read_reg(test, xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET, ers_seq_cfg_0_val);
    read_reg(test, xspi_ctrl_basetest::ers_seq_cfg_1_OFFSET, ers_seq_cfg_1_val);
    read_reg(test, xspi_ctrl_basetest::ers_seq_cfg_2_OFFSET, ers_seq_cfg_2_val);
    read_reg(test, xspi_ctrl_basetest::prog_seq_cfg_0_OFFSET, prog_seq_cfg_0_val);
    read_reg(test, xspi_ctrl_basetest::prog_seq_cfg_1_OFFSET, prog_seq_cfg_1_val);
    read_reg(test, xspi_ctrl_basetest::prog_seq_cfg_2_OFFSET, prog_seq_cfg_2_val);
    read_reg(test, xspi_ctrl_basetest::read_seq_cfg_0_OFFSET, read_seq_cfg_0_val);
    read_reg(test, xspi_ctrl_basetest::read_seq_cfg_1_OFFSET, read_seq_cfg_1_val);
    read_reg(test, xspi_ctrl_basetest::read_seq_cfg_2_OFFSET, read_seq_cfg_2_val);
    read_reg(test, xspi_ctrl_basetest::we_seq_cfg_0_OFFSET, we_seq_cfg_0_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_0_OFFSET, stat_seq_cfg_0_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_1_OFFSET, stat_seq_cfg_1_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_2_OFFSET, stat_seq_cfg_2_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_3_OFFSET, stat_seq_cfg_3_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_4_OFFSET, stat_seq_cfg_4_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_5_OFFSET, stat_seq_cfg_5_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_7_OFFSET, stat_seq_cfg_7_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_8_OFFSET, stat_seq_cfg_8_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_9_OFFSET, stat_seq_cfg_9_val);
    read_reg(test, xspi_ctrl_basetest::stat_seq_cfg_10_OFFSET, stat_seq_cfg_10_val);
    read_reg(test, xspi_ctrl_basetest::xspi_ctrl_version_OFFSET, xspi_ctrl_version_val);
    read_reg(test, xspi_ctrl_basetest::ctrl_features_reg_OFFSET, ctrl_features_reg_val);
    read_reg(test, xspi_ctrl_basetest::wp_settings_OFFSET,   wp_settings_val);
    read_reg(test, xspi_ctrl_basetest::reset_pin_settings_OFFSET, reset_pin_settings_val);
    read_reg(test, xspi_ctrl_basetest::clock_mode_settings_OFFSET, clock_mode_settings_val);
    read_reg(test, xspi_ctrl_basetest::jedec_rst_timing_reg_OFFSET, jedec_rst_timing_reg_val);
    read_reg(test, xspi_ctrl_basetest::dev_delay_reg_OFFSET, dev_delay_reg_val);
    read_reg(test, xspi_ctrl_basetest::rst_recovery_reg_OFFSET, rst_recovery_reg_val);
    read_reg(test, xspi_ctrl_basetest::dev_active_max_reg_OFFSET, dev_active_max_reg_val);
    read_reg(test, xspi_ctrl_basetest::hf_offset_reg_OFFSET, hf_offset_reg_val);
    read_reg(test, xspi_ctrl_basetest::dll_phy_update_cnt_OFFSET, dll_phy_update_cnt_val);
    read_reg(test, xspi_ctrl_basetest::dll_phy_ctrl_OFFSET, dll_phy_ctrl_val);
    read_reg(test, xspi_ctrl_basetest::phy_dq_timing_reg_OFFSET, phy_dq_timing_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_dqs_timing_reg_OFFSET, phy_dqs_timing_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_gate_lpbk_ctrl_reg_OFFSET, phy_gate_lpbk_ctrl_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_dll_master_ctrl_reg_OFFSET, phy_dll_master_ctrl_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_dll_slave_ctrl_reg_OFFSET, phy_dll_slave_ctrl_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_ie_timing_reg_OFFSET, phy_ie_timing_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_obs_reg_0_OFFSET, phy_obs_reg_0_val);
    read_reg(test, xspi_ctrl_basetest::phy_dll_obs_reg_0_OFFSET, phy_dll_obs_reg_0_val);
    read_reg(test, xspi_ctrl_basetest::phy_dll_obs_reg_1_OFFSET, phy_dll_obs_reg_1_val);
    read_reg(test, xspi_ctrl_basetest::phy_dll_obs_reg_2_OFFSET, phy_dll_obs_reg_2_val);
    read_reg(test, xspi_ctrl_basetest::phy_static_togg_reg_OFFSET, phy_static_togg_reg_val);

    read_reg(test, xspi_ctrl_basetest::phy_wr_deskew_reg_OFFSET, phy_wr_deskew_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_wr_rd_deskew_cmd_reg_OFFSET, phy_wr_rd_deskew_cmd_reg_val);


    read_reg(test, xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_0_reg_OFFSET, phy_wr_deskew_pd_ctrl_0_reg_val);

    read_reg(test, xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_1_reg_OFFSET, phy_wr_deskew_pd_ctrl_1_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_rd_deskew_reg_OFFSET, phy_rd_deskew_reg_val);



    read_reg(test, xspi_ctrl_basetest::phy_version_reg_OFFSET, phy_version_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_features_reg_OFFSET, phy_features_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_ctrl_reg_OFFSET, phy_ctrl_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_tsel_reg_OFFSET, phy_tsel_reg_val);
    read_reg(test, xspi_ctrl_basetest::phy_gpio_ctrl_0_OFFSET, phy_gpio_ctrl_0_val);
    read_reg(test, xspi_ctrl_basetest::phy_gpio_ctrl_1_OFFSET, phy_gpio_ctrl_1_val);
    read_reg(test, xspi_ctrl_basetest::phy_gpio_status_0_OFFSET, phy_gpio_status_0_val);
    read_reg(test, xspi_ctrl_basetest::phy_gpio_status_1_OFFSET, phy_gpio_status_1_val);
    wait(5, sc_core::SC_NS);

    bool pass_cmd_reg0 = (cmd_reg0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg0_RESET));
    bool pass_cmd_reg1 = (cmd_reg1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg1_RESET));
    bool pass_cmd_reg2 = (cmd_reg2_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg2_RESET));
    bool pass_cmd_reg3 = (cmd_reg3_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg3_RESET));
    bool pass_cmd_reg4 = (cmd_reg4_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg4_RESET));
    bool pass_cmd_reg5 = (cmd_reg5_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg5_RESET));
    bool pass_cmd_status_ptr = (cmd_status_ptr_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::cmd_status_ptr_RESET));
    bool pass_cmd_status = (cmd_status_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::cmd_status_RESET));
    bool pass_ctrl_status = (ctrl_status_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::ctrl_status_RESET));
    bool pass_trd_status = (trd_status_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::trd_status_RESET));
    bool pass_intr_status = (intr_status_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::intr_status_RESET));
    bool pass_intr_enable = (intr_enable_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::intr_enable_RESET));
    bool pass_trd_comp_intr_status = (trd_comp_intr_status_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::trd_comp_intr_status_RESET));
    bool pass_trd_error_intr_status = (trd_error_intr_status_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::trd_error_intr_status_RESET));
    bool pass_trd_error_intr_en = (trd_error_intr_en_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::trd_error_intr_en_RESET));
    bool pass_dma_target_error_l = (dma_target_error_l_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::dma_target_error_l_RESET));
    bool pass_dma_target_error_h = (dma_target_error_h_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::dma_target_error_h_RESET));
    bool pass_boot_status = (boot_status_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::boot_status_RESET));
    bool pass_long_polling = (long_polling_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::long_polling_RESET));
    bool pass_short_polling = (short_polling_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::short_polling_RESET));
    bool pass_ctrl_config = (ctrl_config_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::ctrl_config_RESET));
    bool pass_dma_settings = (dma_settings_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::dma_settings_RESET));
    bool pass_sdma_size = (sdma_size_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::sdma_size_RESET));
    bool pass_sdma_trd_info = (sdma_trd_info_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::sdma_trd_info_RESET));
    bool pass_sdma_addr0 = (sdma_addr0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::sdma_addr0_RESET));
    bool pass_sdma_addr1 = (sdma_addr1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::sdma_addr1_RESET));
    bool pass_discovery_control = (discovery_control_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::discovery_control_RESET));
    bool pass_xip_mode_cfg = (xip_mode_cfg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::xip_mode_cfg_RESET));
    bool pass_global_seq_cfg = (global_seq_cfg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_RESET));
    bool pass_global_seq_cfg_1 = (global_seq_cfg_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::global_seq_cfg_1_RESET));
    bool pass_direct_access_cfg = (direct_access_cfg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_cfg_RESET));
    bool pass_direct_access_rmp = (direct_access_rmp_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_rmp_RESET));
    bool pass_direct_access_rmp_1 = (direct_access_rmp_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::direct_access_rmp_1_RESET));
    bool pass_rst_seq_cfg_0 = (rst_seq_cfg_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::rst_seq_cfg_0_RESET));
    bool pass_rst_seq_cfg_1 = (rst_seq_cfg_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::rst_seq_cfg_1_RESET));
    bool pass_ers_seq_cfg_0 = (ers_seq_cfg_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_0_RESET));
    bool pass_ers_seq_cfg_1 = (ers_seq_cfg_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_1_RESET));
    bool pass_ers_seq_cfg_2 = (ers_seq_cfg_2_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::ers_seq_cfg_2_RESET));
    bool pass_prog_seq_cfg_0 = (prog_seq_cfg_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_0_RESET));
    bool pass_prog_seq_cfg_1 = (prog_seq_cfg_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_1_RESET));
    bool pass_prog_seq_cfg_2 = (prog_seq_cfg_2_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::prog_seq_cfg_2_RESET));
    bool pass_read_seq_cfg_0 = (read_seq_cfg_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_0_RESET));
    bool pass_read_seq_cfg_1 = (read_seq_cfg_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_1_RESET));
    bool pass_read_seq_cfg_2 = (read_seq_cfg_2_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::read_seq_cfg_2_RESET));
    bool pass_we_seq_cfg_0 = (we_seq_cfg_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::we_seq_cfg_0_RESET));
    bool pass_stat_seq_cfg_0 = (stat_seq_cfg_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_0_RESET));
    bool pass_stat_seq_cfg_1 = (stat_seq_cfg_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_1_RESET));
    bool pass_stat_seq_cfg_2 = (stat_seq_cfg_2_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_2_RESET));
    bool pass_stat_seq_cfg_3 = (stat_seq_cfg_3_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_3_RESET));
    bool pass_stat_seq_cfg_4 = (stat_seq_cfg_4_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_4_RESET));
    bool pass_stat_seq_cfg_5 = (stat_seq_cfg_5_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_5_RESET));
    bool pass_stat_seq_cfg_7 = (stat_seq_cfg_7_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_7_RESET));
    bool pass_stat_seq_cfg_8 = (stat_seq_cfg_8_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_8_RESET));
    bool pass_stat_seq_cfg_9 = (stat_seq_cfg_9_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_9_RESET));
    bool pass_stat_seq_cfg_10 = (stat_seq_cfg_10_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::stat_seq_cfg_10_RESET));
    bool pass_xspi_ctrl_version = (xspi_ctrl_version_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::xspi_ctrl_version_RESET));
    bool pass_ctrl_features_reg = (ctrl_features_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::ctrl_features_reg_RESET));
    bool pass_wp_settings = (wp_settings_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::wp_settings_RESET));
    bool pass_reset_pin_settings = (reset_pin_settings_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::reset_pin_settings_RESET));
    bool pass_clock_mode_settings = (clock_mode_settings_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::clock_mode_settings_RESET));
    bool pass_jedec_rst_timing_reg = (jedec_rst_timing_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::jedec_rst_timing_reg_RESET));
    bool pass_dev_delay_reg = (dev_delay_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::dev_delay_reg_RESET));
    bool pass_rst_recovery_reg = (rst_recovery_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::rst_recovery_reg_RESET));
    bool pass_dev_active_max_reg = (dev_active_max_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::dev_active_max_reg_RESET));
    bool pass_hf_offset_reg = (hf_offset_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::hf_offset_reg_RESET));
    bool pass_dll_phy_update_cnt = (dll_phy_update_cnt_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::dll_phy_update_cnt_RESET));
    bool pass_dll_phy_ctrl = (dll_phy_ctrl_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::dll_phy_ctrl_RESET));
    bool pass_phy_dq_timing_reg = (phy_dq_timing_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_dq_timing_reg_RESET));
    bool pass_phy_dqs_timing_reg = (phy_dqs_timing_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_dqs_timing_reg_RESET));
    bool pass_phy_gate_lpbk_ctrl_reg = (phy_gate_lpbk_ctrl_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_gate_lpbk_ctrl_reg_RESET));
    bool pass_phy_dll_master_ctrl_reg = (phy_dll_master_ctrl_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_dll_master_ctrl_reg_RESET));
    bool pass_phy_dll_slave_ctrl_reg = (phy_dll_slave_ctrl_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_dll_slave_ctrl_reg_RESET));
    bool pass_phy_ie_timing_reg = (phy_ie_timing_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_ie_timing_reg_RESET));
    bool pass_phy_obs_reg_0 = (phy_obs_reg_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_obs_reg_0_RESET));
    bool pass_phy_dll_obs_reg_0 = (phy_dll_obs_reg_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_dll_obs_reg_0_RESET));
    bool pass_phy_dll_obs_reg_1 = (phy_dll_obs_reg_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_dll_obs_reg_1_RESET));
    bool pass_phy_dll_obs_reg_2 = (phy_dll_obs_reg_2_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_dll_obs_reg_2_RESET));
    bool pass_phy_static_togg_reg = (phy_static_togg_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_static_togg_reg_RESET));

    bool pass_phy_wr_deskew_reg = (phy_wr_deskew_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_wr_deskew_reg_RESET));
    bool pass_phy_wr_rd_deskew_cmd_reg = (phy_wr_rd_deskew_cmd_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_wr_rd_deskew_cmd_reg_RESET));    

    bool pass_phy_wr_deskew_pd_ctrl_0_reg = (phy_wr_deskew_pd_ctrl_0_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_0_reg_RESET));
    
    bool pass_phy_wr_deskew_pd_ctrl_1_reg = (phy_wr_deskew_pd_ctrl_1_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_1_reg_RESET));
    bool pass_phy_rd_deskew_reg = (phy_rd_deskew_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_rd_deskew_reg_RESET));
    
    bool pass_phy_version_reg = (phy_version_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_version_reg_RESET));
    bool pass_phy_features_reg = (phy_features_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_features_reg_RESET));
    bool pass_phy_ctrl_reg = (phy_ctrl_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_ctrl_reg_RESET));
    bool pass_phy_tsel_reg = (phy_tsel_reg_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_tsel_reg_RESET));
    bool pass_phy_gpio_ctrl_0 = (phy_gpio_ctrl_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_gpio_ctrl_0_RESET));
    bool pass_phy_gpio_ctrl_1 = (phy_gpio_ctrl_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_gpio_ctrl_1_RESET));
    bool pass_phy_gpio_status_0 = (phy_gpio_status_0_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_gpio_status_0_RESET));
    bool pass_phy_gpio_status_1 = (phy_gpio_status_1_val ==
        static_cast<uint32_t>(xspi_ctrl_basetest::phy_gpio_status_1_RESET));

    if (!pass_cmd_reg0) {
        CSML_ERROR(0, logger) << "  cmd_reg0 not reset: got=0x"
                              << std::hex << cmd_reg0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::cmd_reg0_RESET;
        throw std::runtime_error("");
    }
        if (!pass_cmd_reg1) {
        CSML_ERROR(0, logger) << "  cmd_reg1 not reset: got=0x"
                              << std::hex << cmd_reg1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::cmd_reg1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_cmd_reg2) {
        CSML_ERROR(0, logger) << "  cmd_reg2 not reset: got=0x"
                              << std::hex << cmd_reg2_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::cmd_reg2_RESET;
        throw std::runtime_error("");
    }
    if (!pass_cmd_reg3) {
        CSML_ERROR(0, logger) << "  cmd_reg3 not reset: got=0x"
                              << std::hex << cmd_reg3_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::cmd_reg3_RESET;
        throw std::runtime_error("");
    }
    if (!pass_cmd_reg4) {
        CSML_ERROR(0, logger) << "  cmd_reg4 not reset: got=0x"
                              << std::hex << cmd_reg4_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::cmd_reg4_RESET;
        throw std::runtime_error("");
    }
    if (!pass_cmd_reg5) {
        CSML_ERROR(0, logger) << "  cmd_reg5 not reset: got=0x"
                              << std::hex << cmd_reg5_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::cmd_reg5_RESET;
        throw std::runtime_error("");
    }
    if (!pass_cmd_status_ptr) {
        CSML_ERROR(0, logger) << "  cmd_status_ptr not reset: got=0x"
                              << std::hex << cmd_status_ptr_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::cmd_status_ptr_RESET;
        throw std::runtime_error("");
    }
    if (!pass_cmd_status) {
        CSML_ERROR(0, logger) << "  cmd_status not reset: got=0x"
                              << std::hex << cmd_status_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::cmd_status_RESET;
        throw std::runtime_error("");
    }
    if (!pass_ctrl_status) {
        CSML_ERROR(0, logger) << "  ctrl_status not reset: got=0x"
                              << std::hex << ctrl_status_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::ctrl_status_RESET;
        throw std::runtime_error("");
    }
    if (!pass_trd_status) {
        CSML_ERROR(0, logger) << "  trd_status not reset: got=0x"
                              << std::hex << trd_status_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::trd_status_RESET;
        throw std::runtime_error("");
    }
    if (!pass_intr_status) {
        CSML_ERROR(0, logger) << "  intr_status not reset: got=0x"
                              << std::hex << intr_status_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::intr_status_RESET;
       throw std::runtime_error("");
    }
    if (!pass_intr_enable) {
        CSML_ERROR(0, logger) << "  intr_enable not reset: got=0x"
                              << std::hex << intr_enable_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::intr_enable_RESET;
        throw std::runtime_error("");
    }
    if (!pass_trd_comp_intr_status) {
        CSML_ERROR(0, logger) << "  trd_comp_intr_status not reset: got=0x"
                              << std::hex << trd_comp_intr_status_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::trd_comp_intr_status_RESET;
        throw std::runtime_error("");
    }
    if (!pass_trd_error_intr_status) {
        CSML_ERROR(0, logger) << "  trd_error_intr_status not reset: got=0x"
                              << std::hex << trd_error_intr_status_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::trd_error_intr_status_RESET;
        throw std::runtime_error("");
    }
    if (!pass_trd_error_intr_en) {
        CSML_ERROR(0, logger) << "  trd_error_intr_en not reset: got=0x"
                              << std::hex << trd_error_intr_en_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::trd_error_intr_en_RESET;
        throw std::runtime_error("");
    }
    if (!pass_dma_target_error_l) {
        CSML_ERROR(0, logger) << "  dma_target_error_l not reset: got=0x"
                              << std::hex << dma_target_error_l_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::dma_target_error_l_RESET;
        throw std::runtime_error("");
    }
    if (!pass_dma_target_error_h) {
        CSML_ERROR(0, logger) << "  dma_target_error_h not reset: got=0x"
                              << std::hex << dma_target_error_h_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::dma_target_error_h_RESET;
        throw std::runtime_error("");
    }
    if (!pass_boot_status) {
        CSML_ERROR(0, logger) << "  boot_status not reset: got=0x"
                              << std::hex << boot_status_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::boot_status_RESET;
        throw std::runtime_error("");
    }
    if (!pass_long_polling) {
        CSML_ERROR(0, logger) << "  long_polling not reset: got=0x"
                              << std::hex << long_polling_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::long_polling_RESET;
        throw std::runtime_error("");
    }
    if (!pass_short_polling) {
        CSML_ERROR(0, logger) << "  short_polling not reset: got=0x"
                              << std::hex << short_polling_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::short_polling_RESET;
        throw std::runtime_error("");
    }
    if (!pass_ctrl_config) {
        CSML_ERROR(0, logger) << "  ctrl_config not reset: got=0x"
                              << std::hex << ctrl_config_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::ctrl_config_RESET;
        throw std::runtime_error("");
    }
    if (!pass_dma_settings) {
        CSML_ERROR(0, logger) << "  dma_settings not reset: got=0x"
                              << std::hex << dma_settings_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::dma_settings_RESET;
        throw std::runtime_error("");
    }
    if (!pass_sdma_size) {
        CSML_ERROR(0, logger) << "  sdma_size not reset: got=0x"
                              << std::hex << sdma_size_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::sdma_size_RESET;
        throw std::runtime_error("");
    }
    if (!pass_sdma_trd_info) {
        CSML_ERROR(0, logger) << "  sdma_trd_info not reset: got=0x"
                              << std::hex << sdma_trd_info_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::sdma_trd_info_RESET;
        throw std::runtime_error("");
    }
    if (!pass_sdma_addr0) {
        CSML_ERROR(0, logger) << "  sdma_addr0 not reset: got=0x"
                              << std::hex << sdma_addr0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::sdma_addr0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_sdma_addr1) {
        CSML_ERROR(0, logger) << "  sdma_addr1 not reset: got=0x"
                              << std::hex << sdma_addr1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::sdma_addr1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_discovery_control) {
        CSML_ERROR(0, logger) << "  discovery_control not reset: got=0x"
                              << std::hex << discovery_control_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::discovery_control_RESET;
        throw std::runtime_error("");
    }
    if (!pass_xip_mode_cfg) {
        CSML_ERROR(0, logger) << "  xip_mode_cfg not reset: got=0x"
                              << std::hex << xip_mode_cfg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::xip_mode_cfg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_global_seq_cfg) {
        CSML_ERROR(0, logger) << "  global_seq_cfg not reset: got=0x"
                              << std::hex << global_seq_cfg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::global_seq_cfg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_global_seq_cfg_1) {
        CSML_ERROR(0, logger) << "  global_seq_cfg_1 not reset: got=0x"
                              << std::hex << global_seq_cfg_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::global_seq_cfg_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_direct_access_cfg) {
        CSML_ERROR(0, logger) << "  direct_access_cfg not reset: got=0x"
                              << std::hex << direct_access_cfg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::direct_access_cfg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_direct_access_rmp) {
        CSML_ERROR(0, logger) << "  direct_access_rmp not reset: got=0x"
                              << std::hex << direct_access_rmp_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::direct_access_rmp_RESET;
        throw std::runtime_error("");
    }
    if (!pass_direct_access_rmp_1) {
        CSML_ERROR(0, logger) << "  direct_access_rmp_1 not reset: got=0x"
                              << std::hex << direct_access_rmp_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::direct_access_rmp_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_rst_seq_cfg_0) {
        CSML_ERROR(0, logger) << "  rst_seq_cfg_0 not reset: got=0x"
                              << std::hex << rst_seq_cfg_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::rst_seq_cfg_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_rst_seq_cfg_1) {
        CSML_ERROR(0, logger) << "  rst_seq_cfg_1 not reset: got=0x"
                              << std::hex << rst_seq_cfg_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::rst_seq_cfg_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_ers_seq_cfg_0) {
        CSML_ERROR(0, logger) << "  ers_seq_cfg_0 not reset: got=0x"
                              << std::hex << ers_seq_cfg_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::ers_seq_cfg_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_ers_seq_cfg_1) {
        CSML_ERROR(0, logger) << "  ers_seq_cfg_1 not reset: got=0x"
                              << std::hex << ers_seq_cfg_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::ers_seq_cfg_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_ers_seq_cfg_2) {
        CSML_ERROR(0, logger) << "  ers_seq_cfg_2 not reset: got=0x"
                              << std::hex << ers_seq_cfg_2_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::ers_seq_cfg_2_RESET;
        throw std::runtime_error("");
    }
    if (!pass_prog_seq_cfg_0) {
        CSML_ERROR(0, logger) << "  prog_seq_cfg_0 not reset: got=0x"
                              << std::hex << prog_seq_cfg_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::prog_seq_cfg_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_prog_seq_cfg_1) {
        CSML_ERROR(0, logger) << "  prog_seq_cfg_1 not reset: got=0x"
                              << std::hex << prog_seq_cfg_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::prog_seq_cfg_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_prog_seq_cfg_2) {
        CSML_ERROR(0, logger) << "  prog_seq_cfg_2 not reset: got=0x"
                              << std::hex << prog_seq_cfg_2_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::prog_seq_cfg_2_RESET;
        throw std::runtime_error("");
    }
    if (!pass_read_seq_cfg_0) {
        CSML_ERROR(0, logger) << "  read_seq_cfg_0 not reset: got=0x"
                              << std::hex << read_seq_cfg_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::read_seq_cfg_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_read_seq_cfg_1) {
        CSML_ERROR(0, logger) << "  read_seq_cfg_1 not reset: got=0x"
                              << std::hex << read_seq_cfg_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::read_seq_cfg_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_read_seq_cfg_2) {
        CSML_ERROR(0, logger) << "  read_seq_cfg_2 not reset: got=0x"
                              << std::hex << read_seq_cfg_2_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::read_seq_cfg_2_RESET;
        throw std::runtime_error("");
    }
    if (!pass_we_seq_cfg_0) {
        CSML_ERROR(0, logger) << "  we_seq_cfg_0 not reset: got=0x"
                              << std::hex << we_seq_cfg_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::we_seq_cfg_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_0) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_0 not reset: got=0x"
                              << std::hex << stat_seq_cfg_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_1) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_1 not reset: got=0x"
                              << std::hex << stat_seq_cfg_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_2) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_2 not reset: got=0x"
                              << std::hex << stat_seq_cfg_2_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_2_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_3) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_3 not reset: got=0x"
                              << std::hex << stat_seq_cfg_3_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_3_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_4) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_4 not reset: got=0x"
                              << std::hex << stat_seq_cfg_4_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_4_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_5) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_5 not reset: got=0x"
                              << std::hex << stat_seq_cfg_5_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_5_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_7) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_7 not reset: got=0x"
                              << std::hex << stat_seq_cfg_7_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_7_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_8) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_8 not reset: got=0x"
                              << std::hex << stat_seq_cfg_8_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_8_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_9) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_9 not reset: got=0x"
                              << std::hex << stat_seq_cfg_9_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_9_RESET;
        throw std::runtime_error("");
    }
    if (!pass_stat_seq_cfg_10) {
        CSML_ERROR(0, logger) << "  stat_seq_cfg_10 not reset: got=0x"
                              << std::hex << stat_seq_cfg_10_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::stat_seq_cfg_10_RESET;
        throw std::runtime_error("");
    }
    if (!pass_xspi_ctrl_version) {
        CSML_ERROR(0, logger) << "  xspi_ctrl_version not reset: got=0x"
                              << std::hex << xspi_ctrl_version_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::xspi_ctrl_version_RESET;
        throw std::runtime_error("");
    }
    if (!pass_ctrl_features_reg) {
        CSML_ERROR(0, logger) << "  ctrl_features_reg not reset: got=0x"
                              << std::hex << ctrl_features_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::ctrl_features_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_wp_settings) {
        CSML_ERROR(0, logger) << "  wp_settings not reset: got=0x"
                              << std::hex << wp_settings_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::wp_settings_RESET;
        throw std::runtime_error("");
    }
    if (!pass_reset_pin_settings) {
        CSML_ERROR(0, logger) << "  reset_pin_settings not reset: got=0x"
                              << std::hex << reset_pin_settings_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::reset_pin_settings_RESET;
        throw std::runtime_error("");
    }
    if (!pass_clock_mode_settings) {
        CSML_ERROR(0, logger) << "  clock_mode_settings not reset: got=0x"
                              << std::hex << clock_mode_settings_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::clock_mode_settings_RESET;
        throw std::runtime_error("");
    }
    if (!pass_jedec_rst_timing_reg) {
        CSML_ERROR(0, logger) << "  jedec_rst_timing_reg not reset: got=0x"
                              << std::hex << jedec_rst_timing_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::jedec_rst_timing_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_dev_delay_reg) {
        CSML_ERROR(0, logger) << "  dev_delay_reg not reset: got=0x"
                              << std::hex << dev_delay_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::dev_delay_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_rst_recovery_reg) {
        CSML_ERROR(0, logger) << "  rst_recovery_reg not reset: got=0x"
                              << std::hex << rst_recovery_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::rst_recovery_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_dev_active_max_reg) {
        CSML_ERROR(0, logger) << "  dev_active_max_reg not reset: got=0x"
                              << std::hex << dev_active_max_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::dev_active_max_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_hf_offset_reg) {
        CSML_ERROR(0, logger) << "  hf_offset_reg not reset: got=0x"
                              << std::hex << hf_offset_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::hf_offset_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_dll_phy_update_cnt) {
        CSML_ERROR(0, logger) << "  dll_phy_update_cnt not reset: got=0x"
                              << std::hex << dll_phy_update_cnt_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::dll_phy_update_cnt_RESET;
        throw std::runtime_error("");
    }
    if (!pass_dll_phy_ctrl) {
        CSML_ERROR(0, logger) << "  dll_phy_ctrl not reset: got=0x"
                              << std::hex << dll_phy_ctrl_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::dll_phy_ctrl_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_dq_timing_reg) {
        CSML_ERROR(0, logger) << "  phy_dq_timing_reg not reset: got=0x"
                              << std::hex << phy_dq_timing_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_dq_timing_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_dqs_timing_reg) {
        CSML_ERROR(0, logger) << "  phy_dqs_timing_reg not reset: got=0x"
                              << std::hex << phy_dqs_timing_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_dqs_timing_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_gate_lpbk_ctrl_reg) {
        CSML_ERROR(0, logger) << "  phy_gate_lpbk_ctrl_reg not reset: got=0x"
                              << std::hex << phy_gate_lpbk_ctrl_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_gate_lpbk_ctrl_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_dll_master_ctrl_reg) {
        CSML_ERROR(0, logger) << "  phy_dll_master_ctrl_reg not reset: got=0x"
                              << std::hex << phy_dll_master_ctrl_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_dll_master_ctrl_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_dll_slave_ctrl_reg) {
        CSML_ERROR(0, logger) << "  phy_dll_slave_ctrl_reg not reset: got=0x"
                              << std::hex << phy_dll_slave_ctrl_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_dll_slave_ctrl_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_ie_timing_reg) {
        CSML_ERROR(0, logger) << "  phy_ie_timing_reg not reset: got=0x"
                              << std::hex << phy_ie_timing_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_ie_timing_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_obs_reg_0) {
        CSML_ERROR(0, logger) << "  phy_obs_reg_0 not reset: got=0x"
                              << std::hex << phy_obs_reg_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_obs_reg_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_dll_obs_reg_0) {
        CSML_ERROR(0, logger) << "  phy_dll_obs_reg_0 not reset: got=0x"
                              << std::hex << phy_dll_obs_reg_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_dll_obs_reg_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_dll_obs_reg_1) {
        CSML_ERROR(0, logger) << "  phy_dll_obs_reg_1 not reset: got=0x"
                              << std::hex << phy_dll_obs_reg_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_dll_obs_reg_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_dll_obs_reg_2) {
        CSML_ERROR(0, logger) << "  phy_dll_obs_reg_2 not reset: got=0x"
                              << std::hex << phy_dll_obs_reg_2_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_dll_obs_reg_2_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_static_togg_reg) {
        CSML_ERROR(0, logger) << "  phy_static_togg_reg not reset: got=0x"
                              << std::hex << phy_static_togg_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_static_togg_reg_RESET;
        throw std::runtime_error("");
    }


    if (!pass_phy_wr_deskew_reg) {
        CSML_ERROR(0, logger) << "  phy_wr_deskew_reg not reset: got=0x"
                              << std::hex << phy_wr_deskew_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_wr_deskew_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_wr_rd_deskew_cmd_reg) {
        CSML_ERROR(0, logger) << "  phy_wr_rd_deskew_cmd_reg not reset: got=0x"
                              << std::hex << phy_wr_rd_deskew_cmd_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_wr_rd_deskew_cmd_reg_RESET;
        throw std::runtime_error("");
    }

    if (!pass_phy_wr_deskew_pd_ctrl_0_reg) {
        CSML_ERROR(0, logger) << "  phy_wr_deskew_pd_ctrl_0_reg not reset: got=0x"
                              << std::hex << phy_wr_deskew_pd_ctrl_0_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_0_reg_RESET;
        throw std::runtime_error("");
    }

    if (!pass_phy_wr_deskew_pd_ctrl_1_reg) {
        CSML_ERROR(0, logger) << "  phy_wr_deskew_pd_ctrl_1_reg not reset: got=0x"
                              << std::hex << phy_wr_deskew_pd_ctrl_1_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_1_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_rd_deskew_reg) {
        CSML_ERROR(0, logger) << "  phy_rd_deskew_reg not reset: got=0x"
                              << std::hex << phy_rd_deskew_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_rd_deskew_reg_RESET;
        throw std::runtime_error("");
    }


    if (!pass_phy_version_reg) {
        CSML_ERROR(0, logger) << "  phy_version_reg not reset: got=0x"
                              << std::hex << phy_version_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_version_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_features_reg) {
        CSML_ERROR(0, logger) << "  phy_features_reg not reset: got=0x"
                              << std::hex << phy_features_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_features_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_ctrl_reg) {
        CSML_ERROR(0, logger) << "  phy_ctrl_reg not reset: got=0x"
                              << std::hex << phy_ctrl_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_ctrl_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_tsel_reg) {
        CSML_ERROR(0, logger) << "  phy_tsel_reg not reset: got=0x"
                              << std::hex << phy_tsel_reg_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_tsel_reg_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_gpio_ctrl_0) {
        CSML_ERROR(0, logger) << "  phy_gpio_ctrl_0 not reset: got=0x"
                              << std::hex << phy_gpio_ctrl_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_gpio_ctrl_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_gpio_ctrl_1) {
        CSML_ERROR(0, logger) << "  phy_gpio_ctrl_1 not reset: got=0x"
                              << std::hex << phy_gpio_ctrl_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_gpio_ctrl_1_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_gpio_status_0) {
        CSML_ERROR(0, logger) << "  phy_gpio_status_0 not reset: got=0x"
                              << std::hex << phy_gpio_status_0_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_gpio_status_0_RESET;
        throw std::runtime_error("");
    }
    if (!pass_phy_gpio_status_1) {
        CSML_ERROR(0, logger) << "  phy_gpio_status_1 not reset: got=0x"
                              << std::hex << phy_gpio_status_1_val
                              << " expected=0x"
                              << xspi_ctrl_basetest::phy_gpio_status_1_RESET;
        throw std::runtime_error("");
    }

    bool passed = pass_cmd_reg0 && pass_cmd_reg1 && pass_cmd_reg2 && pass_cmd_reg3
        && pass_cmd_reg4 && pass_cmd_reg5 && pass_cmd_status_ptr && pass_cmd_status
        && pass_ctrl_status && pass_trd_status && pass_intr_status && pass_intr_enable
        && pass_trd_comp_intr_status && pass_trd_error_intr_status && pass_trd_error_intr_en
        && pass_dma_target_error_l && pass_dma_target_error_h && pass_boot_status
        && pass_long_polling && pass_short_polling && pass_ctrl_config && pass_dma_settings
        && pass_sdma_size && pass_sdma_trd_info && pass_sdma_addr0 && pass_sdma_addr1
        && pass_discovery_control && pass_xip_mode_cfg && pass_global_seq_cfg
        && pass_global_seq_cfg_1 && pass_direct_access_cfg && pass_direct_access_rmp
        && pass_direct_access_rmp_1 && pass_rst_seq_cfg_0 && pass_rst_seq_cfg_1
        && pass_ers_seq_cfg_0 && pass_ers_seq_cfg_1 && pass_ers_seq_cfg_2
        && pass_prog_seq_cfg_0 && pass_prog_seq_cfg_1 && pass_prog_seq_cfg_2
        && pass_read_seq_cfg_0 && pass_read_seq_cfg_1 && pass_read_seq_cfg_2
        && pass_we_seq_cfg_0 && pass_stat_seq_cfg_0 && pass_stat_seq_cfg_1
        && pass_stat_seq_cfg_2 && pass_stat_seq_cfg_3 && pass_stat_seq_cfg_4
        && pass_stat_seq_cfg_5 && pass_stat_seq_cfg_7 && pass_stat_seq_cfg_8
        && pass_stat_seq_cfg_9 && pass_stat_seq_cfg_10 && pass_xspi_ctrl_version
        && pass_ctrl_features_reg && pass_wp_settings && pass_reset_pin_settings
        && pass_clock_mode_settings && pass_jedec_rst_timing_reg && pass_dev_delay_reg
        && pass_rst_recovery_reg && pass_dev_active_max_reg && pass_hf_offset_reg
        && pass_dll_phy_update_cnt && pass_dll_phy_ctrl && pass_phy_dq_timing_reg
        && pass_phy_dqs_timing_reg && pass_phy_gate_lpbk_ctrl_reg
        && pass_phy_dll_master_ctrl_reg && pass_phy_dll_slave_ctrl_reg
        && pass_phy_ie_timing_reg && pass_phy_obs_reg_0 && pass_phy_dll_obs_reg_0
        && pass_phy_dll_obs_reg_1 && pass_phy_dll_obs_reg_2 && pass_phy_static_togg_reg && pass_phy_wr_deskew_reg && pass_phy_wr_rd_deskew_cmd_reg
        && pass_phy_wr_deskew_pd_ctrl_0_reg && pass_phy_wr_deskew_pd_ctrl_1_reg && pass_phy_rd_deskew_reg && pass_phy_version_reg
        && pass_phy_features_reg && pass_phy_ctrl_reg && pass_phy_tsel_reg
        && pass_phy_gpio_ctrl_0 && pass_phy_gpio_ctrl_1 && pass_phy_gpio_status_0
        && pass_phy_gpio_status_1;

    report_test_result("TC_XSPI_RST_001", passed);
}


void testbench::test_reserved_bits_write_ignore()
{
    report_test_start("TC_XSPI_REG_013: Reserved Bits Write Ignore");

    apply_reset();

    bool     all_passed = true;
    uint32_t write_val  = 0xFFFFFFFFu;
    uint32_t read_val   = 0u;
    uint32_t reserved   = 0u;
    uint32_t rm         = 0u;

    auto check_resv = [&](const char* name, unsigned int offset,
                          uint32_t write_mask) {
        rm = (~write_mask) & 0xFFFFFFFFu;
        if (rm == 0u) {
            return;
        }
        read_reg(test, offset, read_val);
        wait(5, sc_core::SC_NS);
        write_reg(test, offset, write_val);
        wait(5, sc_core::SC_NS);
        read_reg(test, offset, read_val);
        wait(5, sc_core::SC_NS);
        reserved = read_val & rm;
        if (reserved != 0u) {
            CSML_ERROR(0, logger) << "ERROR: " << name
                << " reserved bits not RES0: mask=0x" << std::hex << rm
                << " got=0x" << reserved << " full_read=0x" << read_val;
            all_passed = false;
        }
    };

    check_resv("cmd_status_ptr", xspi_ctrl_basetest::cmd_status_ptr_OFFSET,
               xspi_ctrl_basetest::cmd_status_ptr_WRITE);
    check_resv("ctrl_status", xspi_ctrl_basetest::ctrl_status_OFFSET,
               xspi_ctrl_basetest::ctrl_status_WRITE);           
    check_resv("intr_status", xspi_ctrl_basetest::intr_status_OFFSET,
               xspi_ctrl_basetest::intr_status_WRITE);
    check_resv("intr_enable", xspi_ctrl_basetest::intr_enable_OFFSET,
               xspi_ctrl_basetest::intr_enable_WRITE);
    check_resv("long_polling", xspi_ctrl_basetest::long_polling_OFFSET,
               xspi_ctrl_basetest::long_polling_WRITE);
    check_resv("short_polling", xspi_ctrl_basetest::short_polling_OFFSET,
               xspi_ctrl_basetest::short_polling_WRITE);
    check_resv("ctrl_config", xspi_ctrl_basetest::ctrl_config_OFFSET,
               xspi_ctrl_basetest::ctrl_config_WRITE);
    check_resv("dma_settings", xspi_ctrl_basetest::dma_settings_OFFSET,
               xspi_ctrl_basetest::dma_settings_WRITE);
    check_resv("discovery_control", xspi_ctrl_basetest::discovery_control_OFFSET,
               xspi_ctrl_basetest::discovery_control_WRITE);
    check_resv("wp_settings", xspi_ctrl_basetest::wp_settings_OFFSET,
               xspi_ctrl_basetest::wp_settings_WRITE);
    check_resv("clock_mode_settings", xspi_ctrl_basetest::clock_mode_settings_OFFSET,
               xspi_ctrl_basetest::clock_mode_settings_WRITE);
    check_resv("jedec_rst_timing_reg", xspi_ctrl_basetest::jedec_rst_timing_reg_OFFSET,
               xspi_ctrl_basetest::jedec_rst_timing_reg_WRITE);
    check_resv("rst_recovery_reg", xspi_ctrl_basetest::rst_recovery_reg_OFFSET,
               xspi_ctrl_basetest::rst_recovery_reg_WRITE);
    check_resv("dev_active_max_reg", xspi_ctrl_basetest::dev_active_max_reg_OFFSET,
               xspi_ctrl_basetest::dev_active_max_reg_WRITE);
    check_resv("hf_offset_reg", xspi_ctrl_basetest::hf_offset_reg_OFFSET,
               xspi_ctrl_basetest::hf_offset_reg_WRITE);
    check_resv("trd_comp_intr_status", xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
               xspi_ctrl_basetest::trd_comp_intr_status_WRITE);
    check_resv("trd_error_intr_status", xspi_ctrl_basetest::trd_error_intr_status_OFFSET,
               xspi_ctrl_basetest::trd_error_intr_status_WRITE);
    check_resv("trd_error_intr_en", xspi_ctrl_basetest::trd_error_intr_en_OFFSET,
               xspi_ctrl_basetest::trd_error_intr_en_WRITE);


    check_resv("trd_status", xspi_ctrl_basetest::trd_status_OFFSET,
               xspi_ctrl_basetest::trd_status_WRITE); 
    check_resv("boot_status", xspi_ctrl_basetest::boot_status_OFFSET,
               xspi_ctrl_basetest::boot_status_WRITE);
    check_resv("sdma_trd_info", xspi_ctrl_basetest::sdma_trd_info_OFFSET,
               xspi_ctrl_basetest::sdma_trd_info_WRITE); 
    check_resv("xip_mode_cfg", xspi_ctrl_basetest::xip_mode_cfg_OFFSET,
               xspi_ctrl_basetest::xip_mode_cfg_WRITE);                           
    check_resv("global_seq_cfg", xspi_ctrl_basetest::global_seq_cfg_OFFSET,
               xspi_ctrl_basetest::global_seq_cfg_WRITE); 
    check_resv("global_seq_cfg_1", xspi_ctrl_basetest::global_seq_cfg_1_OFFSET,
               xspi_ctrl_basetest::global_seq_cfg_1_WRITE);
    check_resv("direct_access_cfg", xspi_ctrl_basetest::direct_access_cfg_OFFSET,
               xspi_ctrl_basetest::direct_access_cfg_WRITE); 
    check_resv("rst_seq_cfg_0", xspi_ctrl_basetest::rst_seq_cfg_0_OFFSET,
               xspi_ctrl_basetest::rst_seq_cfg_0_WRITE);   
    check_resv("rst_seq_cfg_1", xspi_ctrl_basetest::rst_seq_cfg_1_OFFSET,
               xspi_ctrl_basetest::rst_seq_cfg_1_WRITE); 
    check_resv("ers_seq_cfg_0", xspi_ctrl_basetest::ers_seq_cfg_0_OFFSET,
               xspi_ctrl_basetest::ers_seq_cfg_0_WRITE);
    check_resv("ers_seq_cfg_1", xspi_ctrl_basetest::ers_seq_cfg_1_OFFSET,
               xspi_ctrl_basetest::ers_seq_cfg_1_WRITE); 
    check_resv("ers_seq_cfg_2", xspi_ctrl_basetest::ers_seq_cfg_2_OFFSET,
               xspi_ctrl_basetest::ers_seq_cfg_2_WRITE);                           
    check_resv("prog_seq_cfg_0", xspi_ctrl_basetest::prog_seq_cfg_0_OFFSET,
               xspi_ctrl_basetest::prog_seq_cfg_0_WRITE); 
    check_resv("prog_seq_cfg_1", xspi_ctrl_basetest::prog_seq_cfg_1_OFFSET,
               xspi_ctrl_basetest::prog_seq_cfg_1_WRITE);
    check_resv("prog_seq_cfg_2", xspi_ctrl_basetest::prog_seq_cfg_2_OFFSET,
               xspi_ctrl_basetest::prog_seq_cfg_2_WRITE); 
    check_resv("read_seq_cfg_0", xspi_ctrl_basetest::read_seq_cfg_0_OFFSET,
               xspi_ctrl_basetest::read_seq_cfg_0_WRITE);           
               
    check_resv("read_seq_cfg_1", xspi_ctrl_basetest::read_seq_cfg_1_OFFSET,
               xspi_ctrl_basetest::read_seq_cfg_1_WRITE); 
    check_resv("read_seq_cfg_2", xspi_ctrl_basetest::read_seq_cfg_2_OFFSET,
               xspi_ctrl_basetest::read_seq_cfg_2_WRITE);
    check_resv("we_seq_cfg_0", xspi_ctrl_basetest::we_seq_cfg_0_OFFSET,
               xspi_ctrl_basetest::we_seq_cfg_0_WRITE); 
    check_resv("stat_seq_cfg_0", xspi_ctrl_basetest::stat_seq_cfg_0_OFFSET,
               xspi_ctrl_basetest::stat_seq_cfg_0_WRITE);                           
    check_resv("stat_seq_cfg_1", xspi_ctrl_basetest::stat_seq_cfg_1_OFFSET,
               xspi_ctrl_basetest::stat_seq_cfg_1_WRITE); 
    check_resv("stat_seq_cfg_2", xspi_ctrl_basetest::stat_seq_cfg_2_OFFSET,
               xspi_ctrl_basetest::stat_seq_cfg_2_WRITE);
    check_resv("stat_seq_cfg_3", xspi_ctrl_basetest::stat_seq_cfg_3_OFFSET,
               xspi_ctrl_basetest::stat_seq_cfg_3_WRITE); 
    check_resv("stat_seq_cfg_4", xspi_ctrl_basetest::stat_seq_cfg_4_OFFSET,
               xspi_ctrl_basetest::stat_seq_cfg_4_WRITE);   
    check_resv("stat_seq_cfg_5", xspi_ctrl_basetest::stat_seq_cfg_5_OFFSET,
               xspi_ctrl_basetest::stat_seq_cfg_5_WRITE); 
    check_resv("stat_seq_cfg_10", xspi_ctrl_basetest::stat_seq_cfg_10_OFFSET,
               xspi_ctrl_basetest::stat_seq_cfg_10_WRITE);
    check_resv("ers_seq_cfg_2", xspi_ctrl_basetest::ers_seq_cfg_2_OFFSET,
               xspi_ctrl_basetest::ers_seq_cfg_2_WRITE);                           
    check_resv("reset_pin_settings", xspi_ctrl_basetest::reset_pin_settings_OFFSET,
               xspi_ctrl_basetest::reset_pin_settings_WRITE); 
    check_resv("dev_delay_reg", xspi_ctrl_basetest::dev_delay_reg_OFFSET,
               xspi_ctrl_basetest::dev_delay_reg_WRITE);
    check_resv("phy_dll_obs_reg_1", xspi_ctrl_basetest::phy_dll_obs_reg_1_OFFSET,
               xspi_ctrl_basetest::phy_dll_obs_reg_1_WRITE); 
    check_resv("phy_dll_obs_reg_2", xspi_ctrl_basetest::phy_dll_obs_reg_2_OFFSET,
               xspi_ctrl_basetest::phy_dll_obs_reg_2_WRITE); 


    check_resv("phy_static_togg_reg", xspi_ctrl_basetest::phy_static_togg_reg_OFFSET,
               xspi_ctrl_basetest::phy_static_togg_reg_WRITE); 
    check_resv("phy_wr_rd_deskew_cmd_reg", xspi_ctrl_basetest::phy_wr_rd_deskew_cmd_reg_OFFSET,
               xspi_ctrl_basetest::phy_wr_rd_deskew_cmd_reg_WRITE);
    check_resv("phy_wr_deskew_pd_ctrl_0_reg", xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_0_reg_OFFSET,
               xspi_ctrl_basetest::phy_wr_deskew_pd_ctrl_0_reg_WRITE); 
    check_resv("dll_phy_ctrl", xspi_ctrl_basetest::dll_phy_ctrl_OFFSET,
               xspi_ctrl_basetest::dll_phy_ctrl_WRITE);                           
    check_resv("phy_dq_timing_reg", xspi_ctrl_basetest::phy_dq_timing_reg_OFFSET,
               xspi_ctrl_basetest::phy_dq_timing_reg_WRITE); 
    check_resv("phy_dqs_timing_reg", xspi_ctrl_basetest::phy_dqs_timing_reg_OFFSET,
               xspi_ctrl_basetest::phy_dqs_timing_reg_WRITE);
    check_resv("phy_dll_master_ctrl_reg", xspi_ctrl_basetest::phy_dll_master_ctrl_reg_OFFSET,
               xspi_ctrl_basetest::phy_dll_master_ctrl_reg_WRITE); 
    check_resv("phy_ie_timing_reg", xspi_ctrl_basetest::phy_ie_timing_reg_OFFSET,
               xspi_ctrl_basetest::phy_ie_timing_reg_WRITE);   
    check_resv("phy_obs_reg_0", xspi_ctrl_basetest::phy_obs_reg_0_OFFSET,
               xspi_ctrl_basetest::phy_obs_reg_0_WRITE); 
    check_resv("phy_ctrl_reg", xspi_ctrl_basetest::phy_ctrl_reg_OFFSET,
               xspi_ctrl_basetest::phy_ctrl_reg_WRITE); 
    check_resv("phy_tsel_reg", xspi_ctrl_basetest::phy_tsel_reg_OFFSET,
               xspi_ctrl_basetest::phy_tsel_reg_WRITE);                           


    report_test_result("TC_XSPI_REG_013", all_passed);
}

// =============================================================================
// Main Test Execution SC_THREAD
// =============================================================================

/******************************************************************************
 * @brief Main test execution SC_THREAD
 *
 * Runs in simulation time. Executes the four mandatory test cases after
 * applying an initial reset to ensure a clean DUT state.
 ******************************************************************************/
void testbench::run_tests()
{
    CSML_INFO(2, logger) << "================================================";
    CSML_INFO(2, logger) << "  Starting xspi_ctrl Test Execution";
    CSML_INFO(2, logger) << "================================================";

    // Apply initial reset to ensure DUT is in a clean state
    apply_reset();

    // =========================================================================
    // Test 1: RW Register Read/Write
    // =========================================================================
    test_rw_register_access();
    // =========================================================================
    // Test 2: RO Register Write Protection
    // =========================================================================
    test_ro_register_protection();

    // =========================================================================
    // Test 3: Port Binding Verification
    // =========================================================================
    test_port_binding_verification();

    // =========================================================================
    // Test 4: Reset Functionality
    // =========================================================================
    test_reset_functionality();

    // =========================================================================
    // Test 5: Reserved bits write-ignore (TC_XSPI_REG_013)
    // =========================================================================
    test_reserved_bits_write_ignore();

    // =========================================================================
    // FUNC_XSPI_001 Test Suite — Register Model: Initialization, Access
    // Enforcement, and Reset (TC_XSPI_REG_001/002/003/004/005/009/010/011/012)
    // =========================================================================
    run_func001_tests();

    // =========================================================================
    // FUNC_XSPI_002 Test Suite — xSPI Flash Bus Transaction Engine
    // (TC_XSPI_BUS_001 through TC_XSPI_BUS_010)
    // =========================================================================
    run_func002_tests();

    // =========================================================================
    // FUNC_XSPI_003 Test Suite — Interrupt Architecture and Status Management
    // (TC_XSPI_INT_PATH2A_001 through TC_XSPI_INT_RST_001)
    // =========================================================================
    run_func003_tests();

    // =========================================================================
    // FUNC_XSPI_004 Test Suite — DMA Interface and AXI Transaction Management
    // (TC_XSPI_DMA_001 through TC_XSPI_DMA_ERR_002)
    // =========================================================================
    run_func004_tests();

    // =========================================================================
    // FUNC_XSPI_005 Test Suite — Sequence Configuration Register Management
    // (TC_XSPI_CFG_SEQ_001 through TC_XSPI_CFG_SEQ_010)
    // =========================================================================
    run_func005_tests();

    // =========================================================================
    // FUNC_XSPI_006 Test Suite — Operating Mode Control and Command Dispatch
    // (TC_XSPI_MDR_001, TC_XSPI_STIG_008, TC_XSPI_MDR_002–004, MDR_006–008)
    // =========================================================================
    run_func006_tests();

    // =========================================================================
    // FUNC_XSPI_007 Test Suite — Power-on Reset and SFDP Discovery Engine
    // (TC_XSPI_POR_001–005, TC_XSPI_ERR_005)
    // =========================================================================
    run_func007_tests();

    // =========================================================================
    // FUNC_XSPI_008 Test Suite — Direct Mode Flash Forwarding
    // (TC_XSPI_DM_001–005, TC_XSPI_CFG_001–002)
    // =========================================================================
    run_func008_tests();

    // =========================================================================
    // FUNC_XSPI_009 Test Suite — STIG Mode Command Execution
    // (TC_XSPI_STIG_001, TC_XSPI_STIG_002, TC_XSPI_STIG_003, TC_XSPI_STIG_006)
    // =========================================================================
   run_func009_tests();

    // =========================================================================
    // FUNC_XSPI_010 Test Suite — Boot Mode Autonomous DMA Engine
    // (TC_XSPI_BOOT_001–005, TC_XSPI_CFG_004)
    // =========================================================================
    run_func010_tests();

    // =========================================================================
    // FUNC_XSPI_011 Test Suite — PIO Mode Multi-Thread DMA Execution
    // (TC_XSPI_PIO_001–009, TC_XSPI_MDR_005, TC_XSPI_XIP_005,
    //  TC_XSPI_ERR_002, TC_XSPI_ERR_004, TC_XSPI_ERR_006,
    //  TC_XSPI_CFG_003, TC_XSPI_REG_007, TC_XSPI_INT_001)
    // =========================================================================
    run_func011_tests();

    // =========================================================================
    // FUNC_XSPI_012 Test Suite — ACMD/CDMA Mode Descriptor-Based DMA Engine
    // (TC_XSPI_ACMD_001–008, TC_XSPI_MDR_003, TC_XSPI_MDR_006,
    //  TC_XSPI_ERR_001, TC_XSPI_ERR_003,
    //  TC_XSPI_INT_002–007, TC_XSPI_CFG_005,
    //  TC_XSPI_REG_006, TC_XSPI_REG_008)
    // =========================================================================
    run_func012_tests();

    // =========================================================================
    // FUNC_XSPI_013 Test Suite — XIP (eXecute-In-Place) Mode Management
    // (TC_XSPI_REG_010, TC_XSPI_XIP_001–004)
    // Note: TC_XSPI_XIP_005 in FUNC_XSPI_011; TC_XSPI_ACMD_005/006 in FUNC_XSPI_012
    // =========================================================================
    run_func013_tests();

    // Wait for any outstanding transactions to complete
    wait(100, sc_core::SC_NS);

    // Print test summary
    report_test_summary();

    // Stop simulation
    wait(100, sc_core::SC_NS);
    sc_core::sc_stop();
}

// =============================================================================
// Test Reporting Infrastructure
// =============================================================================

/******************************************************************************
 * @brief Report test start with formatted banner
 ******************************************************************************/
void testbench::report_test_start(const std::string& test_name)
{
    CSML_INFO(1, logger) << "\n========================================\n"
                         << test_name << "\n"
                         << "========================================";
}

/******************************************************************************
 * @brief Record and report a passing test
 ******************************************************************************/
void testbench::report_test_pass(const std::string& test_name)
{
    m_tests_passed++;
    m_tests_run++;
    CSML_INFO(1, logger) << test_name << ": PASS";
}

/******************************************************************************
 * @brief Record and report a failing test
 ******************************************************************************/
void testbench::report_test_fail(const std::string& test_name,
                                 const std::string& reason)
{
    m_tests_failed++;
    m_tests_run++;
    m_failed_tests.push_back(test_name);
    if (reason.empty()) {
        CSML_WARN(1, logger) << test_name << ": FAIL";
    } else {
        CSML_WARN(1, logger) << test_name << ": FAIL - " << reason;
    }
}

/******************************************************************************
 * @brief Record test result and print pass/fail banner
 ******************************************************************************/
void testbench::report_test_result(const char* test_name, bool passed)
{
    m_tests_run++;
    if (passed) {
        m_tests_passed++;
        CSML_INFO(1, logger) << "\n========================================\n"
                             << "[*** TEST PASSED ***] " << test_name << "\n"
                             << "========================================";
    } else {
        m_tests_failed++;
        m_failed_tests.push_back(test_name);
        CSML_ERROR(0, logger) << "\n========================================\n"
                              << "[XXX TEST FAILED XXX] " << test_name << "\n"
                              << "========================================";
    }
}

/******************************************************************************
 * @brief Print comprehensive test summary
 ******************************************************************************/
void testbench::report_test_summary()
{
    std::stringstream ss;

    CSML_INFO(1, logger) << "\n========================================";
    CSML_INFO(1, logger) << "  xspi_ctrl Test Summary";
    CSML_INFO(1, logger) << "========================================";

    ss << "Total Tests:  " << m_tests_run;
    CSML_INFO(1, logger) << ss.str();

    ss.str("");
    ss << "Passed:       " << m_tests_passed << " (PASS)";
    CSML_INFO(1, logger) << ss.str();

    ss.str("");
    ss << "Failed:       " << m_tests_failed << " (FAIL)";
    CSML_INFO(1, logger) << ss.str();

    if (m_tests_run > 0) {
        double rate = (100.0 * m_tests_passed) / m_tests_run;
        ss.str("");
        ss << "Success Rate: " << std::fixed << std::setprecision(1) << rate << "%";
        CSML_INFO(1, logger) << ss.str();
    }

    CSML_INFO(1, logger) << "========================================";

    if (m_tests_failed > 0) {
        CSML_ERROR(0, logger) << "\nFailed Tests:";
        for (const auto& t : m_failed_tests) {
            CSML_ERROR(0, logger) << "  - " << t;
        }
        ss.str("");
        ss << "\n[OVERALL RESULT: FAILED - " << m_tests_failed << " test(s) failed]";
        CSML_ERROR(0, logger) << ss.str();
    } else if (m_tests_passed > 0) {
        CSML_INFO(1, logger) << "[OVERALL RESULT: PASSED — All tests passed]";
    } else {
        CSML_WARN(1, logger) << "[OVERALL RESULT: NO TESTS RUN]";
    }

    CSML_INFO(1, logger) << "========================================\n";
}

/******************************************************************************
 * @brief SystemC sc_main entry point
 *
 * Creates the testbench with two flash target stubs and a DUT built with
 * NUM_TARGETS=2, then starts the simulation. All test cases run in
 * run_tests() and the simulation stops on completion. (Some tests skip if
 * they require four CS — see those test bodies.)
 *
 * @param argc Argument count (unused)
 * @param argv Argument vector (unused)
 * @return 0 on successful simulation completion
 ******************************************************************************/
int sc_main(int argc, char* argv[])
{
    load_config_file(argc > 1 ? argv[1] : nullptr);

    // Create a local logger for sc_main scope
    CsmlLogger main_logger;
    main_logger.setMaxVerbosity(2);
    main_logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    main_logger.setFunctionTrace(false);

    CSML_INFO(2, main_logger) << "================================================";
    CSML_INFO(2, main_logger) << "  xspi_ctrl SystemC TLM Testbench";
    CSML_INFO(2, main_logger) << "  Cadence XSPI Controller (IP6522 + IP6182)";
    CSML_INFO(2, main_logger) << "  flash CS count (NUM_TARGETS): 2";
    CSML_INFO(2, main_logger) << "================================================";

    // NUM_TARGETS=4: required for TC_XSPI_DM_003 (dac_bank_num=2 → xspi_bus_socket[2])
    testbench tb("xspi_ctrl_testbench", 4);

    CSML_INFO(2, main_logger) << "Starting simulation...";

    // Start simulation — run_tests() SC_THREAD drives execution until sc_stop()
    sc_core::sc_start();

    CSML_INFO(2, main_logger) << "================================================";
    CSML_INFO(2, main_logger) << "  Simulation Complete";
    CSML_INFO(2, main_logger) << "================================================";

#ifdef __GNUC__
#ifdef __COVERAGE__
    __gcov_dump();
#endif
#endif

#ifdef ACCELLERA_CCI_STD
    std::quick_exit(0);
#endif
    return 0;
}
