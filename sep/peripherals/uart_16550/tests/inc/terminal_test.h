#pragma once

#include <systemc.h>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include "uart_terminal_ui.h"
#include "terminal_if.h"

class terminal_test : public sc_module, public terminal_if {
public:
    // Terminal interface communication
    sc_port<terminal_if> terminal_port;     // Port to send data to Terminal UI
    sc_export<terminal_if> terminal_export; // Export for receiving data from Terminal UI

    // DUT instance
    uart_terminal_ui* dut;

    // Constructor
    SC_HAS_PROCESS(terminal_test);
    terminal_test(sc_module_name name);
    ~terminal_test();

    // Terminal interface implementation
    void uart_to_terminal(uint8_t data) override;
    void terminal_to_uart(uint8_t data) override;

private:
    // Test process
    void run_test();
};

