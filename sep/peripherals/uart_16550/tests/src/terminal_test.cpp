#include "terminal_test.h"
#include "uart_log_adapter.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>

#define PORT 8888

// Initialize logger (defined in uart.cpp but not initialized if UART_IP not instantiated)
extern CsmlLogger g_uart_logger;

// Queue for echoing characters back to terminal
std::queue<uint8_t> echo_queue;
std::mutex echo_mutex;

terminal_test::terminal_test(sc_module_name name) 
    : sc_module(name),
      terminal_port("terminal_port"),
      terminal_export("terminal_export")
{
    // Initialize CSML logger
	#ifdef DEBUG
		g_uart_logger.setMaxVerbosity(csml_severity::CSML_DEBUG);  // most verbose
	#else
		g_uart_logger.setMaxVerbosity(csml_severity::CSML_INFO);   // in release
	#endif

	// log format
	g_uart_logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");

	// function trace
	g_uart_logger.setFunctionTrace(true);  // enables CSML_FUNC_TRACE
	// variable trace
	g_uart_logger.setVariableTrace(false); // disable variable tracing unless needed

    // Bind export to this module immediately
    terminal_export(*this);

    // Instantiate DUT
    // auto_spawn_client = true for interactive mode
    dut = new uart_terminal_ui("uart_terminal_ui", PORT, false);

    // Bind ports
    dut->uart_port(terminal_export);
    terminal_port(dut->uart_export);

    SC_THREAD(run_test);
}

/**
 * @brief Terminal interface implementation: Terminal to UART
 * 
 * This method is called by the Terminal UI when it receives data from client.
 * It replaces the rx_monitor method.
 * 
 * @param data Byte received from terminal client
 */
void terminal_test::terminal_to_uart(uint8_t data) {
    std::cout << "[Testbench] RX Data: " << (char)data << std::endl;
    
    // Queue character for echo
    std::lock_guard<std::mutex> lock(echo_mutex);
    echo_queue.push(data);
}

/**
 * @brief Terminal interface implementation: UART to Terminal
 * 
 * This method is called by UART to send data to terminal.
 * This is a placeholder since the testbench uses terminal_port
 * to send data, not receive via this method.
 * 
 * @param data Byte received (not used in TB)
 */
void terminal_test::uart_to_terminal(uint8_t data) {
    // This should not be called on TB side
    // TB sends via terminal_port->uart_to_terminal()
    std::cout << "Warning: uart_to_terminal called on terminal_test (should not happen)" << std::endl;
}

terminal_test::~terminal_test() {
    delete dut;
}

void terminal_test::run_test() {
    // Initialize signals

    // Wait for simulation to start and server to initialize
    wait(1, SC_SEC);

    std::cout << "Interactive Terminal Test Started." << std::endl;
    std::cout << "An gnome-terminal window should appear." << std::endl;
    std::cout << "Type in the gnome-terminal window to see characters here." << std::endl;

    // Send Welcome Message
    const char* msg = "\r\nWelcome to UART Terminal Test!\r\nType something in the gnome-terminal window and cross check with the console output...\r\n";
    for (size_t i = 0; i < strlen(msg); ++i) {
        terminal_port->uart_to_terminal(msg[i]);
        wait(10, SC_NS); // Pulse width
        wait(10, SC_MS); // Wait between chars
    }

    // Loop indefinitely, checking for echo characters
    while (true) {
        // Check if there are characters to echo
        bool has_char = false;
        uint8_t ch = 0;
        {
            std::lock_guard<std::mutex> lock(echo_mutex);
            if (!echo_queue.empty()) {
                ch = echo_queue.front();
                echo_queue.pop();
                has_char = true;
            }
        }
        
        if (has_char) {
            // Echo character back to terminal
            terminal_port->uart_to_terminal(ch);
            wait(10, SC_NS);
        }
        
        wait(1, SC_MS); // Small delay before checking again
    }
}

int sc_main(int argc, char* argv[]) {
    terminal_test test("terminal_test");
    sc_start();
    return 0;
}
