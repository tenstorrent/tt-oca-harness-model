/**
 * Testbench for Single-Pin GPIO Module (RDL-Based)
 *
 * Tests a single GPIO pin instance with:
 * - Register access via TLM
 * - GPIO I/O functionality
 * - LSIO interface
 * - PAD configuration
 * - Interrupt generation
 * - Hardware strap sampling
 */

#pragma once
#include <systemc.h>
#include "gpio.h"
#include "gpio_test.h"

class testbench : public sc_module
{
public:
    // Module instances
    uint32_t m_tests_failed = 0;
    gpio_ip* dut;        // Device Under Test (GPIO model)
    gpio_test* test;     // Test module

private:
    int m_log_verbosity; // Logger verbosity level

public:
    // GPIO Pin Signals (SC_MANY_WRITERS for LT optimization)
    sc_signal<bool, SC_MANY_WRITERS> gpio_out_sig;     // DUT output
    sc_signal<bool, SC_MANY_WRITERS> gpio_oe_sig;      // DUT output enable
    sc_signal<bool> gpio_in_sig;                       // DUT input (no optimization needed)

    // LSIO Interface Signals
    sc_signal<bool> lsio_gpio_out_sig;                 // DUT input (no optimization needed)
    sc_signal<bool> lsio_gpio_oe_sig;                  // DUT input (no optimization needed)
    sc_signal<bool, SC_MANY_WRITERS> lsio_gpio_in_sig; // DUT output
    sc_signal<bool> lsio_access_sig;                   // DUT input (no optimization needed)

    // PAD Configuration Signals (SC_MANY_WRITERS for LT optimization)
    sc_signal<sc_uint<3>, SC_MANY_WRITERS> pad_drive_strength_sig;
    sc_signal<bool, SC_MANY_WRITERS> pad_pull_enable_sig;
    sc_signal<bool, SC_MANY_WRITERS> pad_pull_select_sig;
    sc_signal<bool, SC_MANY_WRITERS> pad_schmitt_enable_sig;

    // Interrupt Signal (SC_MANY_WRITERS for LT optimization)
    sc_signal<bool, SC_MANY_WRITERS> interrupt_sig;

    // Reset Signal
    sc_signal<bool> rst_n_sig;

    SC_HAS_PROCESS(testbench);

    testbench(sc_module_name name, int log_verbosity = CSML_DEFAULT_VERBOSITY) :
        sc_module(name),
        m_log_verbosity(log_verbosity),
        gpio_out_sig("gpio_out_sig"),
        gpio_oe_sig("gpio_oe_sig"),
        gpio_in_sig("gpio_in_sig"),
        lsio_gpio_out_sig("lsio_gpio_out_sig"),
        lsio_gpio_oe_sig("lsio_gpio_oe_sig"),
        lsio_gpio_in_sig("lsio_gpio_in_sig"),
        lsio_access_sig("lsio_access_sig"),
        pad_drive_strength_sig("pad_drive_strength_sig"),
        pad_pull_enable_sig("pad_pull_enable_sig"),
        pad_pull_select_sig("pad_pull_select_sig"),
        pad_schmitt_enable_sig("pad_schmitt_enable_sig"),
        interrupt_sig("interrupt_sig"),
        rst_n_sig("rst_n_sig")
    {
        // Instantiate GPIO model (DUT) - single pin; is_strap overridden at runtime via ini
        // Constructor: gpio_ip(name, is_strap, log_verbosity)
        dut = new gpio_ip("gpio_dut");

        // Instantiate test module - sync verbosity from DUT (CCI ini may override build default)
        test = new gpio_test("gpio_test", dut->verbosity.get_param_value());

        // Bind TLM ports: test initiator -> DUT target
        test->initiator_socket.bind(dut->target_socket);

        // Bind GPIO pin signals
        dut->gpio_out_o(gpio_out_sig);
        test->gpio_out_i(gpio_out_sig);

        dut->gpio_oe_o(gpio_oe_sig);
        test->gpio_oe_i(gpio_oe_sig);

        test->gpio_in_o(gpio_in_sig);
        dut->gpio_in_i(gpio_in_sig);

        // Bind LSIO interface signals
        test->lsio_gpio_out_o(lsio_gpio_out_sig);
        dut->lsio_gpio_out_i(lsio_gpio_out_sig);

        test->lsio_gpio_oe_o(lsio_gpio_oe_sig);
        dut->lsio_gpio_oe_i(lsio_gpio_oe_sig);

        dut->lsio_gpio_in_o(lsio_gpio_in_sig);
        test->lsio_gpio_in_i(lsio_gpio_in_sig);

        test->lsio_access_o(lsio_access_sig);
        dut->lsio_access_i(lsio_access_sig);

        // Bind PAD configuration signals
        dut->pad_drive_strength_o(pad_drive_strength_sig);
        test->pad_drive_strength_i(pad_drive_strength_sig);

        dut->pad_pull_enable_o(pad_pull_enable_sig);
        test->pad_pull_enable_i(pad_pull_enable_sig);

        dut->pad_pull_select_o(pad_pull_select_sig);
        test->pad_pull_select_i(pad_pull_select_sig);

        dut->pad_schmitt_enable_o(pad_schmitt_enable_sig);
        test->pad_schmitt_enable_i(pad_schmitt_enable_sig);

        // Bind interrupt signal
        dut->interrupt_o(interrupt_sig);
        test->interrupt_i(interrupt_sig);

        // Bind reset signals
        test->rst_no(rst_n_sig);
        dut->rst_ni(rst_n_sig);

        // Register test process
        SC_THREAD(run_tests);
    }

    ~testbench()
    {
        delete dut;
        delete test;
    }

    void run_tests();

private:
    // ===== Infrastructure Tests =====
    void test_read_write_registers();
    void test_read_only_registers();
    void test_port_binding();
    void test_reset_functionality();
    void test_write_only_registers();

    // ===== Feature Tests =====
    // Feature 1: GPIO I/O Operations
    void test_gpio_io_comprehensive();

    // Feature 2: Interrupt Generation
    void test_gpio_interrupts();
    void test_interrupt_falling_edge();
    void test_interrupt_level_high();
    void test_interrupt_level_low();
    void test_interrupt_enable_disable();
    void test_interrupt_type_switching();
    void test_interrupt_rapid_edges();

    // Feature 3: LSIO Interface
    void test_lsio_interface();
    void test_lsio_output_control();
    void test_lsio_priority();
    void test_lsio_disable();

    // Feature 4: PAD Configuration
    void test_pad_configuration();
    void test_pad_all_drive_strengths();
    void test_pad_pull_configurations();
    void test_pad_schmitt_trigger();
    void test_pad_config_enable_disable();

    // Feature 5: Hardware Strap Sampling
    void test_hardware_strap_sampling();
    void test_strap_sample_zero();
    void test_strap_sample_one();
    void test_non_strap_pin();
    void test_strap_multiple_resets();

    // Feature 6: GPIO I/O Modes
    void test_gpio_mode_neither_00();
    void test_gpio_mode_neither_11();
    void test_gpio_mode_transitions();
    void test_gpio_output_toggling();
    void test_gpio_input_rapid_changes();

    // Feature 7: ACCESS_FILTER (Enhanced with enforcement tests)
    void test_access_filter_basic();
    void test_access_filter_write_enforcement();
    void test_access_filter_read_enforcement();
    void test_access_filter_sep_vs_nonsep();
    void test_access_filter_mixed_prot_values();

    // Feature 8: Register Corner Cases
    void test_register_reserved_bits();
    void test_register_read_modify_write();
    void test_register_ro_field_protection();

    // Feature 9: State Transitions
    void test_state_transitions_full_cycle();
    void test_state_control_path_switching();

    // Feature 10: Timing and Stress
    void test_timing_back_to_back_writes();
    void test_stress_rapid_interrupts();

    // Feature 11: Boundary Cases & Integration
    void test_gpio_boundary_cases();
    void test_integration_full_sequence();

    // Feature 12: Critical Gap Tests (High Priority)
    void test_edge_interrupt_clear();
    void test_concurrent_interrupt_config_change();
    void test_lsio_seamless_transition();
    void test_reset_during_tx();
    void test_reset_during_interrupt();
};
