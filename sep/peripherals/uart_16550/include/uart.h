/**
 * @file UART.h
 * @brief UART IP module definition and interface
 * 
 * This file contains the SystemC module definition for a UART IP core
 * that implements standard UART functionality with configurable FIFO,
 * data formats, and interrupt support.
 */

#pragma once

#include "uart_base.h"
#include "terminal_if.h"
#include "csml_parameter.h"
#include <systemc.h>
#include <tlm_utils/tlm_quantumkeeper.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Local bitfield structs for offset 2 registers
/**
 * @brief FIFO Control Register (FCR) bitfield structure
 * 
 * This structure represents the bitfields of the FIFO Control Register
 * used to control the UART's FIFO behavior.
 */
struct FCR_fields {
	uint8_t FIFO_ENABLE : 1;
	uint8_t RCVR_FIFO_RESET : 1;
	uint8_t XMIT_FIFO_RESET : 1;
	uint8_t DMA_MODE_SELECT : 1;
	uint8_t Reserved : 2;
	uint8_t RCVR_TRIGGER : 2;

	FCR_fields()
	    : FIFO_ENABLE(0), RCVR_FIFO_RESET(0), XMIT_FIFO_RESET(0), DMA_MODE_SELECT(0),
	      Reserved(0), RCVR_TRIGGER(0)
	{
	}
};

/**
 * @brief Interrupt Identification Register (IIR) bitfield structure
 * 
 * This structure represents the bitfields of the Interrupt Identification
 * Register used to identify pending interrupts and FIFO status.
 */
struct IIR_fields {
	uint8_t INTERRUPT_PENDING : 1; // bit 0
	uint8_t INTERRUPT_ID : 3;	 // bits 1–3
	uint8_t Reserved : 2;	 // bits 4–5
	uint8_t FIFOS_ENABLED : 2; // bits 6–7

	IIR_fields() : INTERRUPT_PENDING(1),INTERRUPT_ID(0), Reserved(0), FIFOS_ENABLED(0)
	{
	}
};

// IER register bitfields
/**
 * @brief Interrupt Enable Register (IER) bitfield structure
 * 
 * This structure represents the bitfields of the Interrupt Enable Register
 * used to enable/disable specific UART interrupts.
 */
struct IER_fields {
	uint8_t ERBFI : 1; // Enable Received Data Available Interrupt
	uint8_t ETBEI : 1; // Enable Transmitter Holding Register Empty
			   // Interrupt
	uint8_t ELSI : 1;  // Enable Receiver Line Status Interrupt
	uint8_t EDSSI : 1; // Enable Modem Status Interrupt (optional)
    uint8_t EFEI : 1;   // Enable FIFO Error Interrupt
	uint8_t Reserved : 3;

	IER_fields() : ERBFI(0), ETBEI(0), ELSI(0), EDSSI(0), EFEI(0), Reserved(0)
	{
	}
};

/**
 * @brief UART IP module implementation
 * 
 * This class implements a UART IP core with standard UART functionality
 * including FIFO configuration, interrupt generation and Loopback modes.
 * It inherits from UART_base.
 * 
 * @ingroup UART_IP
 */
class UART_IP : public UART_base, public terminal_if
{
      public:
	SC_HAS_PROCESS(UART_IP);
	// Terminal interface communication
	sc_port<terminal_if> terminal_port;     // Port to send data to terminal
	sc_export<terminal_if> terminal_export; // Export for receiving data from terminal
	
	// Interrupt output signal
	sc_out<bool> INTR{ "INTR" };

	// Reset input signal
	sc_in<bool> reset{ "reset" };
	
	// FIFOs
	sc_fifo<uint8_t> tx_fifo;
	sc_fifo<uint8_t> rx_fifo;

	// Events to notify processes(tx_process and rx_process)
	sc_event tx_process_event;
	sc_event rx_process_event;

	// Quantum keeper timing parameters
	sc_time m_byte_time;        // Time to transmit/receive one byte
	tlm_utils::tlm_quantumkeeper m_qk;  // Quantum keeper for temporal decoupling
	csml_param<double> TimeKeeperQuantumNs;  ///< Quantum keeper period in nanoseconds (0=disabled)

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
	csml_param<int> verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug
	// Constructor
	UART_IP(sc_module_name name, unsigned int memory_size = 56, int log_verbosity = CSML_DEFAULT_VERBOSITY);
    virtual ~UART_IP() = default;

	// Interrupt update method
    void update_interrupt();

	    	// UART interface
	void send_byte(uint8_t b);
	void write_rx(uint8_t b);

	// Terminal interface implementation
	void uart_to_terminal(uint8_t data) override;
	void terminal_to_uart(uint8_t data) override;

	// Systemc Processes
	void tx_process();
	void rx_process();
	void reset_method();
      private:
	void register_callbacks();

	// Internal storage for aliased registers
	uint8_t RBR_val = 0;
	uint8_t THR_val = 0;
	uint8_t DLL_val = 0;
	uint8_t DLM_val = 0;

	FCR_fields FCR_val;
	IIR_fields IIR_val;
	IER_fields IER_val;

	// Callback handlers offset 0x00
	bool read_callback_off_0(uint8_t &value);
	bool write_callback_off_0(uint8_t value);

    // Callback handlers offset 0x04
	bool read_callback_off_1(uint8_t &value);
	bool write_callback_off_1(uint8_t value);

	// Callback handlers offset 0x08
	bool read_callback_off_2(uint8_t &value);
	bool write_callback_off_2(uint8_t value);

	// Read callback handler offset 0x14
	bool read_callback_off_5(uint8_t &value);

};
