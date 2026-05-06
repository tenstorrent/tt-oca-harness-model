/**
 * @file aon_timer_test.h
 * @brief AON Timer test class providing concrete register read/write access methods
 *        and complementary sc_export/sc_signal port infrastructure.
 *
 * Derives from aon_timer_basetest and implements:
 *   - register_read_32 / register_write_32 for 32-bit register access
 *   - register_read_8  / register_write_8  for byte-granularity register access
 *   - sc_signal instances for every port declared on the aon_timer model
 *
 * The signal members serve as the binding intermediaries between the aon_timer DUT
 * ports and this test module. The testbench binds DUT ports to these signals and
 * reads/writes them directly to drive or observe the DUT side-band ports.
 *
 * Test cases instantiate this class to perform register reads and writes against
 * the aon_timer DUT via TLM-2.0 blocking transport (b_transport) through the
 * inherited initiator_socket.
 *
 * Usage:
 * @code
 *   // In testbench constructor:
 *   aon_timer_test tester("tester");
 *   dut.target_socket.bind(tester.initiator_socket); // or vice-versa
 *   dut.rst_n.bind(tester.rst_n_sig);
 *   ...
 *   // In test case:
 *   uint32_t val;
 *   tester.read_register_32(aon_timer_basetest::WDOG_REGWEN_OFFSET, val);
 *   // val should equal 0x01 after reset
 * @endcode
 */

#pragma once
#include "aon_timer_basetest.h"
#include "aon_timer_interface.h"

/**
 * @class aon_timer_test
 * @brief Concrete AON Timer test class with register access methods and port signals.
 *
 * Provides register_read_32, register_write_32, register_read_8, and register_write_8
 * helper methods that issue TLM-2.0 blocking transport transactions to the aon_timer DUT.
 * Also declares sc_signal members for all aon_timer side-band ports so that the
 * testbench can bind DUT ports to these signals and then drive/observe them
 * programmatically during test execution.
 *
 * Inherits aon_timer_if to satisfy the abstract register access contract and enable
 * use of the test class in a generic register access context.
 */
class aon_timer_test : public aon_timer_basetest, public aon_timer_if
{
public:
   // =========================================================================
   // Interconnect Signals - driven by testbench to bind DUT ports
   // =========================================================================

   /// @brief Signal bound to dut.rst_n - drive low to assert SYS reset, high to de-assert.
   sc_signal<bool> rst_n_sig;

   /// @brief Signal bound to dut.rst_aon_n - drive low to assert AON reset, high to de-assert.
   sc_signal<bool> rst_aon_n_sig;

   /// @brief Signal bound to dut.sleep_mode - drive high to indicate SoC sleep state.
   sc_signal<bool> sleep_mode_sig;

   /// @brief Signal bound to dut.lc_escalate_en - drive high to trigger lifecycle escalation.
   sc_signal<bool> lc_escalate_en_sig;

   /// @brief Signal bound to dut.clk_aon_freq - set to the desired AON clock frequency in Hz.
   sc_signal<double> clk_aon_freq_sig;

   /// @brief Signal bound to dut.clk_sys_freq - set to the desired SYS clock frequency in Hz.
   sc_signal<double> clk_sys_freq_sig;

   /// @brief Signal bound to dut.intr_wkup_timer_expired - observe wakeup interrupt assertion.
   sc_signal<bool> intr_wkup_timer_expired_sig;

   /// @brief Signal bound to dut.intr_wdog_timer_bark - observe watchdog bark interrupt assertion.
   sc_signal<bool> intr_wdog_timer_bark_sig;

   /// @brief Signal bound to dut.nmi_wdog_timer_bark - observe watchdog NMI interrupt assertion.
   sc_signal<bool> nmi_wdog_timer_bark_sig;

   /// @brief Signal bound to dut.wkup_req - observe wakeup request assertion to power manager.
   sc_signal<bool> wkup_req_sig;

   /// @brief Signal bound to dut.aon_timer_rst_req - observe reset request assertion to power manager.
   sc_signal<bool> aon_timer_rst_req_sig;

   /// @brief Signal bound to dut.fatal_fault - observe fatal TL-UL integrity alert assertion.
   sc_signal<bool> fatal_fault_sig;

   /// @brief Signal bound to dut.racl_policies - drive RACL policy vector for RACL testing.
   sc_signal<uint32_t> racl_policies_sig;

   /// @brief Signal bound to dut.racl_error - observe RACL violation output.
   sc_signal<bool> racl_error_sig;

   /**
    * @brief Construct the aon_timer_test module.
    * @param name SystemC hierarchical module name.
    *
    * Initializes all sc_signal members with descriptive names and calls the
    * aon_timer_basetest constructor. All signals default to their initial SystemC
    * values (false for bool, 0.0 for double, 0 for uint32_t).
    */
   aon_timer_test(sc_module_name name)
      : aon_timer_basetest(name),
        rst_n_sig("rst_n_sig"),
        rst_aon_n_sig("rst_aon_n_sig"),
        sleep_mode_sig("sleep_mode_sig"),
        lc_escalate_en_sig("lc_escalate_en_sig"),
        clk_aon_freq_sig("clk_aon_freq_sig"),
        clk_sys_freq_sig("clk_sys_freq_sig"),
        intr_wkup_timer_expired_sig("intr_wkup_timer_expired_sig"),
        intr_wdog_timer_bark_sig("intr_wdog_timer_bark_sig"),
        nmi_wdog_timer_bark_sig("nmi_wdog_timer_bark_sig"),
        wkup_req_sig("wkup_req_sig"),
        aon_timer_rst_req_sig("aon_timer_rst_req_sig"),
        fatal_fault_sig("fatal_fault_sig"),
        racl_policies_sig("racl_policies_sig"),
        racl_error_sig("racl_error_sig")
   {
   }

   // =========================================================================
   // 32-bit Register Access Methods
   // =========================================================================

   /**
    * @brief Perform a 32-bit register write via TLM-2.0 blocking transport.
    * @param offset      Byte offset of the target register (use Register_offset enum values).
    * @param write_value The 32-bit value to write to the register.
    *
    * Constructs a TLM generic payload with:
    *   - command   = TLM_WRITE_COMMAND
    *   - address   = offset
    *   - data_ptr  = &write_value
    *   - data_length = 4 bytes
    *   - byte_enable_ptr = nullptr (all bytes enabled)
    *   - streaming_width = 4 bytes
    *
    * Issues the transaction via initiator_socket->b_transport(). The transaction
    * uses a zero sc_time offset; temporal decoupling is managed by the DUT model.
    * The response status is checked via SC_REPORT_ERROR if not TLM_OK_RESPONSE.
    */
   void write_register_32(unsigned int offset, uint32_t write_value) override;

   /**
    * @brief Perform a 32-bit register read via TLM-2.0 blocking transport.
    * @param offset     Byte offset of the target register (use Register_offset enum values).
    * @param read_value Reference to a uint32_t that will receive the read value.
    *
    * Constructs a TLM generic payload with:
    *   - command   = TLM_READ_COMMAND
    *   - address   = offset
    *   - data_ptr  = &read_value
    *   - data_length = 4 bytes
    *   - byte_enable_ptr = nullptr (all bytes enabled)
    *   - streaming_width = 4 bytes
    *
    * Issues the transaction via initiator_socket->b_transport(). For write-only
    * registers (ALERT_TEST, INTR_TEST), read_value is set to 0x0.
    */
   void read_register_32(unsigned int offset, uint32_t& read_value) override;

   // =========================================================================
   // 8-bit Register Access Methods (from base interface)
   // =========================================================================

   /**
    * @brief Perform an 8-bit register read via TLM-2.0 blocking transport.
    * @param offset     Byte offset of the target register (use Register_offset enum values).
    * @param read_value Reference to a uint8_t that will receive the read byte value.
    *
    * Issues a TLM_READ_COMMAND transaction with 1-byte data length to the DUT's
    * target socket. The transaction response is checked internally.
    */
   void read_register_8(unsigned int offset, uint8_t& read_value) override;

   /**
    * @brief Perform an 8-bit register write via TLM-2.0 blocking transport.
    * @param offset      Byte offset of the target register (use Register_offset enum values).
    * @param write_value The byte value to write to the register.
    *
    * Issues a TLM_WRITE_COMMAND transaction with 1-byte data length to the DUT's
    * target socket. The transaction response is checked internally.
    */
   void write_register_8(unsigned int offset, uint8_t write_value) override;

   /**
    * @brief Destructor.
    */
   ~aon_timer_test() override {}
};
