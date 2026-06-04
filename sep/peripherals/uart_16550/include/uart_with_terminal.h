#pragma once

/**
 * @file uart_with_terminal.h
 * @brief UART wrapper with integrated terminal
 *
 * This wrapper module encapsulates the UART IP and terminal UI,
 * providing a self-contained UART peripheral that can be directly
 * integrated into a VP platform without external terminal management.
 */


#include <systemc.h>
#include "uart.h"
#include "uart_terminal_ui.h"

/**
 * @brief UART wrapper module with integrated terminal
 *
 * This module instantiates both the UART IP core and the terminal UI,
 * connecting them internally. The platform only needs to connect the
 * TLM target socket and basic control signals.
 */
SC_MODULE(UART_with_terminal) {
    // UART IP core instance
    UART_IP uart_inst;

    // Terminal UI instance (internal - not exposed)
    uart_terminal_ui terminal_ui;

    // Public interface - forward declarations to expose UART IP's interfaces
    // Note: These are direct references, not copies
    decltype(uart_inst.target_socket)& target_socket;
    decltype(uart_inst.reset)& reset;
    decltype(uart_inst.INTR)& INTR;

    /**
     * @brief Constructor
     *
     * @param name Module name
     * @param terminal_port TCP port for terminal (default 8888)
     * @param auto_spawn_client Auto-spawn Python terminal client (default false)
     * @param log_verbosity Logger verbosity level (0=minimal, 1=info, 2=normal, 3=detailed, default: 2)
     */
    UART_with_terminal(sc_module_name name, int terminal_port = 8888, bool auto_spawn_client = false, int log_verbosity = CSML_DEFAULT_VERBOSITY)
        : sc_module(name),
          uart_inst("uart_inst", 56, log_verbosity),
          terminal_ui("terminal_ui", terminal_port, auto_spawn_client),
          target_socket(uart_inst.target_socket),
          reset(uart_inst.reset),
          INTR(uart_inst.INTR)
    {
        // Connect UART and terminal internally via terminal_if interface
        uart_inst.terminal_port(terminal_ui.uart_export);    // UART sends to terminal's export
        terminal_ui.uart_port(uart_inst.terminal_export);    // Terminal sends to UART's export
    }
};

