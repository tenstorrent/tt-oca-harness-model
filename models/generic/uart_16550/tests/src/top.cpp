/**
 * @file top.cpp
 * @brief Top-level SystemC module for UART IP simulation with terminal interface
 * 
 * This file contains the top-level SystemC module that instantiates and connects
 * the UART IP core with a terminal UI for interactive testing.
 */
#include <systemc.h>
#include "uart_test.h"
#include "uart.h"
#include "csml_logger.h"
#include <cstdlib> // for quick_exit

// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif




/**
 * @brief Top-level SystemC module for UART simulation
 * 
 * This module serves as the top-level container that instantiates and connects:
 * - UART module (contains UART IP core)
 * - UART testbench (UART_test)
 * 
 * It handles all the signal connections between these components and provides
 * the main simulation environment.
 */
SC_MODULE(Top) {
    UART_test tb;                    ///< UART testbench instance
    UART_IP uart_inst;               ///< UART IP instance


    // Reset and INTR signals
    sc_signal<bool> reset_sig;    ///< Reset signal
    sc_signal<bool, SC_MANY_WRITERS> INTR_sig;    ///< INTR signal

    SC_CTOR(Top)
        : tb("tb"), uart_inst("uart_inst")
    {
        // --- Socket binding ---
        tb.initiator_socket.bind(uart_inst.target_socket);

        // --- UART native interface binding ---
        // Bind UART port to TB export (for TX capture)
        uart_inst.terminal_port(tb.terminal_export);
        
        // Bind TB port to UART export (for RX injection)
        tb.terminal_port(uart_inst.terminal_export);



        // --- Reset / INTR binding ---
        tb.reset(reset_sig);
        uart_inst.reset(reset_sig);

        tb.INTR(INTR_sig);
        uart_inst.INTR(INTR_sig);
    }
};

/**
 * @brief Main entry point for the UART simulation
 * 
 * This function:
 * 1. Instantiates the top-level module
 * 2. Starts the SystemC simulation
 * 3. Prints simulation status
 * 
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @return int Program exit status
 */
int sc_main(int argc, char* argv[]) {
    // Create logger for sc_main
    CsmlLogger main_logger;
    main_logger.setMaxVerbosity(CSML_DEFAULT_VERBOSITY);
    main_logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    main_logger.setFunctionTrace(false);

    CSML_INFO(1, main_logger) << "Starting UART IP Testbench..." << std::endl;

    // Initialize CCI broker and optionally load INI config file.
    load_config_file(argc > 1 ? argv[1] : nullptr);

    Top top("top");

    sc_start();

    CSML_INFO(1, main_logger) << "\nSimulation completed." << std::endl;

#ifdef ACCELLERA_CCI_STD
#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    std::quick_exit(0);
#endif

    return 0;
}
