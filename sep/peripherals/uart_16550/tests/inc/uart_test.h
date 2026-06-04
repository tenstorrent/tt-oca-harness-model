
#pragma once
#include "uart_basetest.h"
#include "uart.h"
#include "terminal_if.h"
#include <vector>
#include <deque>

class UART_test : public UART_basetest, public terminal_if
{
public:
    SC_HAS_PROCESS(UART_test);
    UART_test(sc_module_name name);
    ~UART_test() {}

    sc_out<bool> reset;        // TB drives reset
    sc_in<bool> INTR;          // TB reads interrupt

    // Terminal interface communication
    sc_port<terminal_if> terminal_port;     // Port to send data to UART (RX injection)
    sc_export<terminal_if> terminal_export; // Export for receiving data from UART (TX capture)

    // Terminal interface implementation
    void uart_to_terminal(uint8_t data) override;
    void terminal_to_uart(uint8_t data) override;

    // void register_read_8(unsigned int offset, uint8_t &read_value);
    // void register_write_8(unsigned int offset, uint8_t write_value);

    void register_read_32(unsigned int offset, uint32_t &read_value);
    void register_write_32(unsigned int offset, uint32_t write_value);

    // Helper to inject a byte via rx_valid/rx_data
    void drive_rx(uint8_t value);

    // Main test process
    void run_test();
    void monitor_intr();

    // TX verification
    void verify_tx_results();

    // RX verification helper
    void on_rx_read(uint8_t v);
    
    // Helper to verify FIFO trigger tests (interrupt count + data check)
    void verify_fifo_trigger_test(const std::vector<uint8_t>& expected_data);

private:
    int results_count{0};
    std::vector<uint8_t> expected_tx_;
    std::vector<uint8_t> observed_tx_;

    // Test counters
    int total_test_cases = 0;
    int passed_test_cases = 0;
    std::deque<uint8_t> expected_rx_;

    // Interrupt tracking counters
    uint32_t intr_rda_count_{0};      // Received Data Available
    uint32_t intr_thre_count_{0};     // Transmitter Holding Register Empty
    uint32_t intr_rls_count_{0};      // Receiver Line Status
    uint32_t intr_ms_count_{0};       // Modem Status
    uint32_t intr_ct_count_{0};       // Character Timeout
    uint32_t intr_fifo_err_count_{0}; // FIFO Error
    
    // LSR value from last RLS interrupt (before it's cleared by reading)
    uint32_t last_lsr_value_{0};

public:
    // Helper to clear interrupt counters
    void clear_intr_counters() {
        intr_rda_count_ = 0;
        intr_thre_count_ = 0;
        intr_rls_count_ = 0;
        intr_ms_count_ = 0;
        intr_ct_count_ = 0;
        intr_fifo_err_count_ = 0;
        last_lsr_value_ = 0;
    }

    // Getters for interrupt counters
    uint32_t get_intr_rda_count() const { return intr_rda_count_; }
    uint32_t get_intr_thre_count() const { return intr_thre_count_; }
    uint32_t get_intr_rls_count() const { return intr_rls_count_; }
    uint32_t get_intr_ms_count() const { return intr_ms_count_; }
    uint32_t get_intr_ct_count() const { return intr_ct_count_; }
    uint32_t get_intr_fifo_err_count() const { return intr_fifo_err_count_; }
    uint32_t get_last_lsr_value() const { return last_lsr_value_; }
};
