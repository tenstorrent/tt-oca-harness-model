// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file hmac_interface.h
 * @brief Abstract interface class for HMAC custom port interfaces
 * 
 * This class defines pure virtual functions for interrupt, alert, clock, and reset signals.
 * It provides a SystemC interface abstraction for connecting the HMAC IP to external components.
 */

#pragma once
#include <systemc.h>

/**
 * @brief Abstract interface for HMAC custom port interfaces
 * 
 * This class defines pure virtual functions for interrupt, alert, clock, and reset signals.
 * Implementations of this interface provide access to HMAC IP control and status signals.
 */
class hmac_if : virtual public sc_interface
{
public:
    // Set the hmac_done interrupt signal
    // value: Interrupt state (true = asserted, false = deasserted)
    // This interrupt is asserted when HMAC/SHA-2 operation completes.
    virtual void set_intr_hmac_done(bool value) = 0;
    
    // Get the hmac_done interrupt signal state
    // Returns: Current interrupt state
    virtual bool get_intr_hmac_done() = 0;

    // Set the fifo_empty interrupt signal
    // value: Interrupt state (true = asserted, false = deasserted)
    // This interrupt is asserted when message FIFO becomes empty.
    virtual void set_intr_fifo_empty(bool value) = 0;
    
    // Get the fifo_empty interrupt signal state
    // Returns: Current interrupt state
    virtual bool get_intr_fifo_empty() = 0;

    // Set the hmac_err interrupt signal
    // value: Interrupt state (true = asserted, false = deasserted)
    // This interrupt is asserted when configuration or operational errors occur.
    virtual void set_intr_hmac_err(bool value) = 0;
    
    // Get the hmac_err interrupt signal state
    // Returns: Current interrupt state
    virtual bool get_intr_hmac_err() = 0;

    // Set the fatal_fault alert signal
    // value: Alert state (true = asserted, false = deasserted)
    // This alert is asserted on TL-UL bus integrity violations.
    virtual void set_alert_fatal_fault(bool value) = 0;
    
    // Get the fatal_fault alert signal state
    // Returns: Current alert state
    virtual bool get_alert_fatal_fault() = 0;

    // Set the functional clock frequency
    // frequency_hz: Clock frequency in Hz
    // The clock input represents the functional clock frequency for timing calculations.
    virtual void set_clk_i(double frequency_hz) = 0;
    
    // Get the functional clock frequency
    // Returns: Current clock frequency in Hz
    virtual double get_clk_i() = 0;

    // Set the reset signal state
    // value: Reset state (true = deasserted, false = asserted)
    // rst_ni is an active-low asynchronous reset input.
    virtual void set_rst_ni(bool value) = 0;
    
    // Get the reset signal state
    // Returns: Current reset state (true = deasserted, false = asserted)
    virtual bool get_rst_ni() = 0;
};
