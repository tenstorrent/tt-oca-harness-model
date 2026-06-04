/**
 * @file terminal_if.h
 * @brief SystemC interface for UART-Terminal communication
 * 
 * This file defines the interface for bidirectional communication between
 * the UART IP core and the Terminal UI module. It replaces the signal-based
 * communication with direct method calls.
 */

#pragma once

#include <systemc.h>

/**
 * @brief Interface for UART-Terminal communication
 * 
 * This interface provides two pure virtual methods for bidirectional
 * communication between UART and Terminal modules:
 * 
 * - uart_to_terminal(): Called by UART when transmitting data to terminal
 * - terminal_to_uart(): Called by Terminal when receiving data from client
 * 
 * Each module implements the method it needs to receive data and calls
 * the method when sending data through sc_port/sc_export mechanism.
 */
class terminal_if : virtual public sc_interface
{
public:
    /**
     * @brief Send data from UART to Terminal
     * 
     * This method is called by the UART IP core when it needs to transmit
     * a byte to the terminal. The Terminal module implements this method
     * to receive and forward the data to the connected client.
     * 
     * @param data Byte to be transmitted to the terminal
     */
    virtual void uart_to_terminal(uint8_t data) = 0;

    /**
     * @brief Send data from Terminal to UART
     * 
     * This method is called by the Terminal UI when it receives a byte
     * from the connected client. The UART IP core implements this method
     * to receive and process the incoming data.
     * 
     * @param data Byte received from the terminal client
     */
    virtual void terminal_to_uart(uint8_t data) = 0;
};
