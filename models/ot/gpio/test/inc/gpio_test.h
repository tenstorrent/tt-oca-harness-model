/**
 * GPIO Test Module (RDL-Based Single-Pin)
 *
 * Provides test infrastructure for single GPIO pin:
 * - TLM initiator for register access (with PROT extension support)
 * - GPIO pin drivers and monitors
 * - LSIO interface drivers
 * - PAD configuration monitors
 * - Interrupt monitoring
 * - Reset generation
 */

#pragma once
#include "gpio_basetest.h"
#include "gpio_prot_extension.h"
#include "csml_logger.h"
#include <tlm_utils/tlm_quantumkeeper.h>

class gpio_test : public gpio_basetest
{
public:
    // GPIO Pin Ports (complementary to DUT)
    sc_in<bool> gpio_out_i;    // Monitors DUT output
    sc_in<bool> gpio_oe_i;     // Monitors DUT output enable
    sc_out<bool> gpio_in_o;    // Drives DUT input

    // LSIO Interface Ports (test drives these to DUT)
    sc_out<bool> lsio_gpio_out_o;
    sc_out<bool> lsio_gpio_oe_o;
    sc_in<bool> lsio_gpio_in_i;
    sc_out<bool> lsio_access_o;

    // PAD Configuration Monitoring Ports
    sc_in<sc_uint<3>> pad_drive_strength_i;
    sc_in<bool> pad_pull_enable_i;
    sc_in<bool> pad_pull_select_i;
    sc_in<bool> pad_schmitt_enable_i;

    // Interrupt Monitoring Port
    sc_in<bool> interrupt_i;

    // Reset Generation Port
    sc_out<bool> rst_no;

    // CSML Logger instance (mutable to allow logging in const functions)
    mutable CsmlLogger logger;

    // Track last transaction status for error checking
    mutable tlm::tlm_response_status m_last_response;

    // Quantum keeper for temporal decoupling
    tlm_utils::tlm_quantumkeeper m_qk;

    gpio_test(sc_module_name name, int log_verbosity = 3) :
        gpio_basetest(name),
        gpio_out_i("gpio_out_i"),
        gpio_oe_i("gpio_oe_i"),
        gpio_in_o("gpio_in_o"),
        lsio_gpio_out_o("lsio_gpio_out_o"),
        lsio_gpio_oe_o("lsio_gpio_oe_o"),
        lsio_gpio_in_i("lsio_gpio_in_i"),
        lsio_access_o("lsio_access_o"),
        pad_drive_strength_i("pad_drive_strength_i"),
        pad_pull_enable_i("pad_pull_enable_i"),
        pad_pull_select_i("pad_pull_select_i"),
        pad_schmitt_enable_i("pad_schmitt_enable_i"),
        interrupt_i("interrupt_i"),
        rst_no("rst_no")
    {
        // Initialize logger
        logger.setMaxVerbosity(log_verbosity);
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [GPIO_TEST] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);

        // Initialize LSIO outputs to inactive state
        lsio_gpio_out_o.initialize(false);
        lsio_gpio_oe_o.initialize(false);
        lsio_access_o.initialize(false);

        // Initialize GPIO input driver to low
        gpio_in_o.initialize(false);

        // Initialize reset to inactive (high - active-low reset)
        rst_no.initialize(true);

        // Initialize quantum keeper
        m_qk.reset();
    }

    ~gpio_test()
    {
        // No dynamic memory to clean up
    }

    // Quantum keeper management
    void sync_quantum() {
        wait(m_qk.get_local_time());
        m_qk.reset();
    }

    void inc_quantum(const sc_time& delay) {
        m_qk.inc(delay);
        if (m_qk.need_sync()) {
            sync_quantum();
        }
    }

    // 32-bit register access functions (no PROT - uses default 0x0)
    void read_register_32(unsigned int offset, uint32_t &read_value);
    void write_register_32(unsigned int offset, uint32_t write_value);

    // 32-bit register access with PROT extension (for ACCESS_FILTER testing)
    void read_register_32_with_prot(unsigned int offset, uint32_t &read_value, uint8_t arprot);
    void write_register_32_with_prot(unsigned int offset, uint32_t write_value, uint8_t awprot);

    // Convenience methods for SEP vs non-SEP access
    void read_register_32_sep(unsigned int offset, uint32_t &read_value);
    void write_register_32_sep(unsigned int offset, uint32_t write_value);
    void read_register_32_nonsep(unsigned int offset, uint32_t &read_value);
    void write_register_32_nonsep(unsigned int offset, uint32_t write_value);

    // Check transaction response status
    bool last_transaction_succeeded() const;

    // Assert function for validation
    void assert_equal(uint32_t expected, uint32_t actual, const char* message);

    // Helper functions to drive/read GPIO pin
    void drive_gpio_pin(bool value);
    bool read_gpio_out();
    bool read_gpio_oe();

    // Helper functions for LSIO interface
    void drive_lsio_output(bool value);
    void drive_lsio_oe(bool value);
    void set_lsio_access(bool active);
    bool read_lsio_input();

    // Helper functions to read PAD configuration
    uint32_t read_pad_drive_strength();
    bool read_pad_pull_enable();
    bool read_pad_pull_select();
    bool read_pad_schmitt_enable();

    // Helper function to read interrupt signal
    bool read_interrupt();

    // Reset control functions
    void assert_reset();
    void deassert_reset();
};
