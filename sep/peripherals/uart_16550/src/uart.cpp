/**
 * @file UART.cpp
 * @brief Implementation of the UART IP module
 *
 * This file contains the implementation of the UART IP core functionality
 * including register access, data transmission, reception, Loopback modes and Interrupts.
 */

#include "uart.h"
#include <iostream>
#include "csml_logger.h"
#include "uart_log_adapter.h"
#include <iomanip>

/// Global logger instance for UART module
CsmlLogger g_uart_logger;

UART_IP::UART_IP(sc_module_name name, unsigned int memory_size, int log_verbosity)
    : UART_base(name, memory_size),
      tx_fifo(4096), rx_fifo(4096), tx_process_event("tx_process_event"),
      rx_process_event("rx_process_event"),
      m_byte_time(1, SC_NS)        // 1ns per byte (functional model approximation)
      , TimeKeeperQuantumNs("TimeKeeperQuantumNs", 0.0)
      , verbosity("verbosity", log_verbosity)
{
	// Initialize CSML logger (0=error, 1=info, 2=debug, 3=trace)
	g_uart_logger.setMaxVerbosity(verbosity.get_param_value());

	// log format
	g_uart_logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");

	// function trace
	g_uart_logger.setFunctionTrace(true);  // enables CSML_FUNC_TRACE
	// variable trace
	g_uart_logger.setVariableTrace(false); // disable variable tracing unless needed


	// reset handling: call reset_method on falling edge of reset (active low)
	SC_METHOD(reset_method);
	sensitive << reset.neg();
	dont_initialize();

	// SystemC threads to handle TX and RX operation
	SC_THREAD(tx_process);
	SC_THREAD(rx_process);

	// Bind terminal export to this module (implements terminal_if)
	terminal_export(*this);

	// Register custom callbacks for memory-mapped registers based on the offset
	register_callbacks();

	// Apply time keeper quantum from CCI configuration
	if (TimeKeeperQuantumNs.get_param_value() > 0.0) {
		sc_core::sc_time quantum(TimeKeeperQuantumNs.get_param_value(), sc_core::SC_NS);
		tlm::tlm_global_quantum::instance().set(quantum);
		m_qk.reset();
	}

	// Initialize quantum keeper
	m_qk.reset();

	UART_DEBUG("UART module initialized");
}

/**
 * @brief Registers memory access callbacks for UART registers
 * 
 * This function sets up the read and write callbacks for the UART's memory-mapped
 * registers. It handles register aliasing and shared offsets by mapping
 * different register behaviors to the same memory addresses based on the
 * current UART configuration (like DLAB bit).
 * 
 * The following register mappings are established:
 * - Offset 0x00: 
 *   - Read:  RBR (Receiver Buffer Register) when DLAB=0
 *   - Write: THR (Transmitter Holding Register) when DLAB=0
 *   - Read/Write: DLL (Divisor Latch LSB) when DLAB=1
 * 
 * - Offset 0x01:
 *   - Read/Write: IER (Interrupt Enable Register) when DLAB=0
 *   - Read/Write: DLM (Divisor Latch MSB) when DLAB=1
 * 
 * - Offset 0x02:
 *   - Read:  IIR (Interrupt Identification Register)
 *   - Write: FCR (FIFO Control Register)
 * 
 * @note The actual behavior of some registers depends on the DLAB bit in the LCR register.
 * @see UART_IP::LCR
 */
void UART_IP::register_callbacks()
{
	// Register combined read/write callbacks for shared offsets

	// --- offset 0 callbacks:
	memory.register_read_callback(
	    [this](unsigned int &value) -> bool {
        uint8_t val8 = static_cast<uint8_t>(value);
        bool result = read_callback_off_0(val8);
        value = static_cast<unsigned int>(val8);
        return result;
    },
    RBR.offset // shared offset (0x00)
	);

	memory.register_write_callback(
	    [this](unsigned int value) -> bool {
        uint8_t val8 = static_cast<uint8_t>(value);
        bool result = write_callback_off_0(val8);
        value = static_cast<unsigned int>(val8);
        return result;
	    },
	    THR.offset // same offset (0x00)
	);

	// --- offset 2 callbacks: FCR (write) / IIR (read) ---
	// write -> FCR
	memory.register_write_callback(
	    [this](unsigned int value) -> bool {
        uint8_t val8 = static_cast<uint8_t>(value);
        bool result = write_callback_off_2(val8);
        value = static_cast<unsigned int>(val8);
        return result;
	    },
	    FCR.offset // 0x08
	);

	// read -> IIR
	memory.register_read_callback(
	    [this](unsigned int &value) -> bool {
        uint8_t val8 = static_cast<uint8_t>(value);
        bool result = read_callback_off_2(val8);
        value = static_cast<unsigned int>(val8);
        return result;
    },
	    IIR.offset // 0x08
	);

	// --- offset 1 callbacks: IER (DLAB=0) / DLM (DLAB=1)
	memory.register_read_callback(
	    [this](unsigned int &value) -> bool {
        uint8_t val8 = static_cast<uint8_t>(value);
        bool result = read_callback_off_1(val8);
        value = static_cast<unsigned int>(val8);
        return result;
    },
	    IER.offset // 0x04
	);

	memory.register_write_callback(
	    [this](unsigned int value) -> bool {
        uint8_t val8 = static_cast<uint8_t>(value);
        bool result = write_callback_off_1(val8);
        value = static_cast<unsigned int>(val8);
        return result;
	    },
	    IER.offset // 0x04
	);

	// LSR read callback (offset 0x14 byte)
	memory.register_read_callback(
		[this](unsigned int &value) -> bool {
			uint8_t val8 = 0;
			bool result = read_callback_off_5(val8);
			value = static_cast<unsigned int>(val8);
			return result;
		},
		LSR.offset
	);

	// Register post-write callback for ITR to Assert the Interrupt immediately.
	memory.register_post_write_callback(
		[this]() -> bool {
			this->update_interrupt();  // immediate evaluation of test bits
			return true;
		},
		ITR.offset 
	);
}

/**
 * @brief Read callback for offset 0x00
 * 
 * This function handles the read operations to the UART's memory-mapped
 * registers at offset 0x00. It supports different behaviors based on the
 * DLAB bit in the LCR register:
 * 
 * - When DLAB=0:
 *   - RBR (Receiver Buffer Register) or RCVR FIFO when FIFO is enabled
 * 
 * - When DLAB=1:
 *   - DLL (Divisor Latch LSB)
 * 
 * @param value Reference to the value to be read from the register
 * @return true if the read was successful
 */
bool UART_IP::read_callback_off_0(uint8_t &value)
{
	value = 0;
	UART_DEBUG("Read from RBR/DLL");
	if (LCR.DLAB == 0) {
		// Read from RBR/RCVR FIFO
		if (FCR_val.FIFO_ENABLE) {
			// Read from RCVR FIFO
			if (rx_fifo.num_available() > 0) {
				UART_DEBUG("Read from RCVR FIFO");
				rx_fifo.nb_read(value);
				LSR.DR = (rx_fifo.num_available() ? 1 : 0);
				// Update interrupt if enabled
				if (IER_val.EDSSI || IER_val.ELSI || IER_val.ERBFI ||
				    IER_val.ETBEI || IER_val.EFEI)
					update_interrupt();
			} else {
				UART_DEBUG("RCVR FIFO is empty");
				value = 0;
				LSR.DR = 0;
				// Update interrupt if enabled
				if (IER_val.EDSSI || IER_val.ELSI || IER_val.ERBFI ||
				    IER_val.ETBEI || IER_val.EFEI)
					update_interrupt();
			}
		} else {
			UART_DEBUG("Read from RBR");
			// Non-FIFO: perform read from RBR (local var)
			value = RBR_val;
			LSR.DR = 0;
			// Update interrupt if enabled
			if (IER_val.EDSSI || IER_val.ELSI || IER_val.ERBFI ||
				    IER_val.ETBEI || IER_val.EFEI)
				update_interrupt();
		}
	} else {
		UART_DEBUG("Read from DLL");
		// Divisor Latch mode
		value = DLL_val;
	}
	return true;
}

/**
 * @brief Write callback for offset 0x00
 * 
 * This function handles the write operations to the UART's memory-mapped
 * registers at offset 0x00. It supports different behaviors based on the
 * DLAB bit in the LCR register:
 * 
 * - When DLAB=0:
 *   - THR (Transmitter Holding Register) or XMIT FIFO when FIFO is enabled
 * 
 * - When DLAB=1:
 *   - DLL (Divisor Latch LSB)
 * 
 * @param value The value to be written to the register
 * @return true if the write was successful
 */
bool UART_IP::write_callback_off_0(uint8_t value)
{
	UART_DEBUG("Write to THR/DLL");
	if (LCR.DLAB == 0) {
		
		// --- SYSTEM LOOPBACK(internal) FEATURE ---
		// if only LOOP is set then dont transmit, only dump internally to RBR/RCVR fifo
		if (MCR.LOOP && !MCR.LINE_LOOPBACK) {
			// Write the same data directly to RX
			UART_DEBUG("Looping back the THR content to RBR/ RCVR FIFO internally");
			write_rx(value); // Reuse existing write_rx()
			return true;
		}

		// THR write.
		UART_DEBUG("Write to THR");
		THR_val = value;

		if (FCR_val.FIFO_ENABLE)
			tx_fifo.write(value);

		LSR.THRE = 0;
		LSR.TEMT = 0;

		// Notify tx_process thread through an event (tx_process_event)to process the data
		tx_process_event.notify(SC_ZERO_TIME);
	} else {
		// Divisor Latch mode - DLL
		UART_DEBUG("Write to DLL");
		DLL_val = value;
	}
	return true;
}

/**
 * @brief Read callback for offset 0x04
 * 
 * This function handles the read operations to the UART's memory-mapped
 * registers at offset 0x04. It supports different behaviors based on the
 * DLAB bit in the LCR register:
 * 
 * - When DLAB=0:
 *   - IER (Interrupt Enable Register)
 * 
 * - When DLAB=1:
 *   - DLM (Divisor Latch MSB)
 * 
 * @param value Reference to the value to be read from the register
 * @return true if the read was successful.
 */
bool UART_IP::read_callback_off_1(uint8_t &value)
{
	if (LCR.DLAB == 0) {
		// Access IER
		UART_DEBUG("Read from IER");
		value = 0;
		value |= (IER_val.ERBFI & 0x1) << 0;
		value |= (IER_val.ETBEI & 0x1) << 1;
		value |= (IER_val.ELSI & 0x1) << 2;
		value |= (IER_val.EDSSI & 0x1) << 3;
		value |= (IER_val.EFEI & 0x1) << 4;
	} else {
		// DLAB mode - Access DLM
		UART_DEBUG("Read from DLM");
		value = DLM_val;
	}
	return true;
}

/**
 * @brief Write callback for offset 0x04
 * 
 * This function handles the write operations to the UART's memory-mapped
 * registers at offset 0x04. It supports different behaviors based on the
 * DLAB bit in the LCR register:
 * 
 * - When DLAB=0:
 *   - IER (Interrupt Enable Register)
 * 
 * - When DLAB=1:
 *   - DLM (Divisor Latch MSB)
 * 
 * @param value The value to be written to the register
 * @return true if the write was successful, false otherwise
 */
bool UART_IP::write_callback_off_1(uint8_t value)
{
	if (LCR.DLAB == 0) {
		// Access IER
		UART_DEBUG("Write to IER");
		IER_val.ERBFI = (value >> 0) & 0x1;
		IER_val.ETBEI = (value >> 1) & 0x1;
		IER_val.ELSI = (value >> 2) & 0x1;
		IER_val.EDSSI = (value >> 3) & 0x1;
		IER_val.EFEI = (value >> 4) & 0x1;
	} else {
		// Access DLM
		UART_DEBUG("Write to DLM");
		DLM_val = value;
	}
	return true;
}

/**
 * @brief Write callback for offset 0x08
 * 
 * This function handles the write operations to the UART's memory-mapped
 * registers at offset 0x08. It supports different behaviors based on the
 * read and write cycle by the CPU :
 * 
 * - When write cycle:
 *   - FCR (FIFO Control Register)
 * 
 * - When read cycle:
 *   - IIR (Interrupt Identification Register)
 * 
 * @param value The value to be written to the register
 * @return true if the write was successful
 */
bool UART_IP::write_callback_off_2(uint8_t value)
{
	UART_DEBUG("Write to FCR");
	bool was_enabled = FCR_val.FIFO_ENABLE; // before decode


	// Decode FCR bitfields from the byte and populate the register
	FCR_val.FIFO_ENABLE = (value >> 0) & 0x1;

	if(FCR_val.FIFO_ENABLE) {
		FCR_val.RCVR_FIFO_RESET = (value >> 1) & 0x1;
		FCR_val.XMIT_FIFO_RESET = (value >> 2) & 0x1;
		FCR_val.DMA_MODE_SELECT = (value >> 3) & 0x1;
		FCR_val.RCVR_TRIGGER = (value >> 6) & 0x3;
	}

	bool now_enabled = FCR_val.FIFO_ENABLE;
	if (was_enabled != now_enabled) {
		// clear RX
		uint8_t b = 0;
		while (rx_fifo.nb_read(b)) {}
		LSR.DR = 0;
		// clear TX
		while (tx_fifo.nb_read(b)) {}
		LSR.THRE = 1;
		LSR.TEMT = 1;
	}
	// FIFO reset behavior if the relevent bit is set.
	if (FCR_val.RCVR_FIFO_RESET) {
		UART_TRACE("FCR RX Reset");
		
		uint8_t tmp = 0;
		while (rx_fifo.nb_read(tmp)) {
		}
		LSR.DR = 0;

		// Clear the bit as it is self clearing.
		FCR_val.RCVR_FIFO_RESET = 0b0;
	}
	if (FCR_val.XMIT_FIFO_RESET) {
		UART_TRACE("FCR TX Reset");
		uint8_t tmp = 0;
		while (tx_fifo.nb_read(tmp)) {
		}
		LSR.THRE = 1;
		LSR.TEMT = 1;
		// Clear the bit as it is self clearing.
		FCR_val.XMIT_FIFO_RESET = 0b0;
	}

	// Update Interrupt Status.
	if (IER_val.EDSSI || IER_val.ELSI || IER_val.ERBFI ||
				IER_val.ETBEI || IER_val.EFEI)
		update_interrupt();

	UART_DEBUG("FCR write: FIFO_Enable=" << (int)FCR_val.FIFO_ENABLE);
	return true;
}

/**
 * @brief Read callback for offset 0x08
 * 
 * This function handles the read operations to the UART's memory-mapped
 * registers at offset 0x08. It supports different behaviors based on the
 * read and write cycle by the CPU :
 * 
 * - When write cycle:
 *   - FCR (FIFO Control Register)
 * 
 * - When read cycle:
 *   - IIR (Interrupt Identification Register)
 * 
 * @param value The value to be read from the register
 * @return true if the read was successful
 */
bool UART_IP::read_callback_off_2(uint8_t &value)
{
	UART_DEBUG("Read from IIR");
	// Update IIR fields based on current FIFO and interrupt status
	IIR_val.FIFOS_ENABLED = (FCR_val.FIFO_ENABLE ? 0x3 : 0x0); // bits 6–7


	// Construct IIR value based on bit positions
	value = 0;
	value |= (IIR_val.INTERRUPT_PENDING & 0x1);	   // bit 0
	value |= (IIR_val.INTERRUPT_ID & 0x7) << 1;	   // bits 1–3
	value |= (IIR_val.FIFOS_ENABLED & 0x3) << 6; // bits 6–7

	UART_DEBUG("IIR read: INT_PENDING=" << (int)IIR_val.INTERRUPT_PENDING
		  << ", INT_ID=" << (int)IIR_val.INTERRUPT_ID
		  << ", FIFO_STATUS=" << (int)IIR_val.FIFOS_ENABLED);

	// Clear THRE bit(if set) as it is cleared on IIR read
	if (LSR.THRE) {
		UART_DEBUG("Clearing THRE bit");
		LSR.THRE = 0;
		// ---THRE interrupt update(deassert after THRE is cleared) ---
		if (IER_val.ETBEI)
			update_interrupt();
	}
	return true;
}

/**
 * @brief Read callback for offset 0x05
 * 
 * This function handles the read operations to the UART's memory-mapped
 * registers at offset 0x05. It's a custom implementation instead of the default read
 * callback to handle the LSR register. this is because we have to clear the OE, PE, FE, BI
 * bits when the LSR is read which is not handled by the default read callback.
 * 
 * @param value The value to be read from the register
 * @return true if the read was successful
 */
bool UART_IP::read_callback_off_5(uint8_t &value)
{
    // Pack LSR bits per the LSR register fields
    uint8_t v = 0;
    v |= (LSR.DR   & 0x1) << 0;
    v |= (LSR.OE   & 0x1) << 1;
    v |= (LSR.PE   & 0x1) << 2;
    v |= (LSR.FE   & 0x1) << 3;
    v |= (LSR.BI   & 0x1) << 4;
    v |= (LSR.THRE & 0x1) << 5;
    v |= (LSR.TEMT & 0x1) << 6;
    v |= (LSR.ERROR_IN_RCVR_FIFO & 0x1) << 7;

    value = v;

    // 16550 semantics: OE (and typically PE/FE/BI) clear on LSR read
	// Clear the error bits
    LSR.OE = 0;

	// optional (not implemented)
    // LSR.PE = 0;
    // LSR.FE = 0;
    // LSR.BI = 0;

	// ---Line Status interrupt update(deassert after LSR is read) ---
	if(IER_val.ELSI)
		update_interrupt();

    return true;
}

/**
 * @brief Transmitter process
 * 
 * This SystemC process handles the transmitter process of the UART IP core.
 * It is executed in a loop and waits for the tx_process_event to be
 * triggered. When triggered, it writes the data from the THR register
 * to the terminal(simulating the serial out behavior) and updates the LSR register.
 * 
 * @return void
 */
void UART_IP::tx_process()
{
	while (true) {
		wait(tx_process_event);

		if(!reset.read()) {
			continue;
		}

		UART_TRACE("In tx_process");
		uint8_t byte_to_send = 0;

		// If FIFO is enabled and there are bytes available in the tx FIFO, read from the tx FIFO
		// else read from the THR register
		if (FCR_val.FIFO_ENABLE && tx_fifo.num_available() > 0) {
			tx_fifo.nb_read(byte_to_send);
			UART_DEBUG("Writing from FIFO to S_out pin");
		} else {
			byte_to_send = THR_val;
			UART_DEBUG("Writing from THR to S_out pin");
		}

		// Send the byte to the terminal(simulating the serial out behavior)
		send_byte(byte_to_send);
		//wait(m_byte_time);
		// Set the THRE bit in the LSR register
		LSR.THRE = 1;
		LSR.TEMT = 1;

		// Update Interrupt Status.
		if (IER_val.EDSSI || IER_val.ELSI || IER_val.ERBFI ||
				    IER_val.ETBEI || IER_val.EFEI)
			update_interrupt();
	}
}

/**
 * @brief Receiver process
 * 
 * This SystemC process handles the receiver process of the UART IP core.
 * It is executed in a loop and waits for the rx_process_event to be
 * triggered. When triggered, it updates the Data Ready(DR) bit of the LSR register
 * and updates the interrupt status.
 * 
 * @return void
 */
void UART_IP::rx_process()
{
	while (true) {
		wait(rx_process_event);

		if(!reset.read()) {
			continue;
		}

		UART_TRACE("In rx_process");

		if (FCR_val.FIFO_ENABLE) {
			// Update DR according to FIFO contents
			LSR.DR = (rx_fifo.num_available() ? 1 : 0);
		}

		// Update Interrupt Status.
		if (IER_val.EDSSI || IER_val.ELSI || IER_val.ERBFI ||
		    IER_val.ETBEI || IER_val.EFEI) {
			update_interrupt();
		}
	}
}

/**
 * @brief Update interrupt status
 * 
 * This function updates the interrupt status of the UART IP core based on
 * the current state of the registers and the interrupt enable register.
 * It checks for the following conditions:
 * 
 * - RX Data Ready (based on RCVR FIFO trigger level or DR bit in LSR)
 * - TX Empty (THRE bit in LSR)
 * - Line Status (Incase of Overrun error)
 * 
 * @return void
 */
void UART_IP::update_interrupt()
{
	// Priority: Line Status (0x3) > RX Data Ready (0x2) > TX Empty (0x1)
	bool intr_asserted = false;
	
	// Test interrupts (forced) — highest priority, ignore IER
	// Priority order (highest to lowest):
	// 0x7 (FIFO Error) > 0x3 (Line Status) > 0x6 (Timeout) >
	// 0x2 (RX Data Ready) > 0x1 (THR Empty) > 0x0 (Modem Status)
	if (ITR.TFEI) {                 // 0x7
		IIR_val.INTERRUPT_PENDING = 0;
		IIR_val.INTERRUPT_ID = 0x7;
		intr_asserted = true;
	} else if (ITR.TLSI) {          // 0x3
		IIR_val.INTERRUPT_PENDING = 0;
		IIR_val.INTERRUPT_ID = 0x3;
		intr_asserted = true;
	} else if (ITR.TRTI) {          // 0x6
		IIR_val.INTERRUPT_PENDING = 0;
		IIR_val.INTERRUPT_ID = 0x6;
		intr_asserted = true;
	} else if (ITR.TRBFI) {         // 0x2
		IIR_val.INTERRUPT_PENDING = 0;
		IIR_val.INTERRUPT_ID = 0x2;
		intr_asserted = true;
	} else if (ITR.TTBEI) {         // 0x1
		IIR_val.INTERRUPT_PENDING = 0;
		IIR_val.INTERRUPT_ID = 0x1;
		intr_asserted = true;
	} else if (ITR.TDSSI) {         // 0x0
		IIR_val.INTERRUPT_PENDING = 0;
		IIR_val.INTERRUPT_ID = 0x0;
		intr_asserted = true;
	} else { // else: fall through to normal interrupt logic below
		UART_DEBUG("Normal interrupt logic");
		// 1. RX Data Ready (RCVR FIFO reached trigger level or DR bit in LSR is set(non-fifo mode))
		bool rx_interrupt = false;
		unsigned int trigger_level = 1;

		//If FIFO mode - Calculate the trigger level configured in the FCR and ECR registers
		if (FCR_val.FIFO_ENABLE) {
			// Handle receiver trigger level using extended trigger level support
				// Combine FCR.Trigger_Level (LSB) and ECR.RCVR_TRIGGER_MS2B (MSB) to form 4-bit trigger level
				unsigned int trigger_level_val = (ECR.RCVR_TRIGGER_MS2B << 2) | FCR_val.RCVR_TRIGGER;

				switch (trigger_level_val) {
				case 0x0: trigger_level = 1; break;
				case 0x1: trigger_level = 4; break;
				case 0x2: trigger_level = 8; break;
				case 0x3: trigger_level = 14; break;
				case 0x4: trigger_level = 32; break;
				case 0x5: trigger_level = 64; break;
				case 0x6: trigger_level = 128; break;
				case 0x7: trigger_level = 256; break;
				case 0x8: trigger_level = 512; break;
				case 0x9: trigger_level = 1024; break;
				case 0xA: trigger_level = 2048; break;
				case 0xB: trigger_level = 4096; break;
				default: trigger_level = 1; // Fallback to 1 character if invalid
				break;
				}
			UART_DEBUG("Trigger level: " << trigger_level);
			if ((static_cast<unsigned int>(rx_fifo.num_available()) >= trigger_level)) {
					UART_DEBUG("RCVR trigger reached ("
						<< rx_fifo.num_available()
						<< " bytes), trigger "
						<< trigger_level);	
			}
			rx_interrupt =
			(static_cast<unsigned int>(rx_fifo.num_available()) >= trigger_level && IER_val.ERBFI);
			UART_TRACE("rx_interrupt(FIFO) value: " << std::boolalpha
				<< rx_interrupt);
		} else {
			//Non-fifo mode, check if DR bit in LSR is set and relevant interrupt is enabled
			rx_interrupt = (LSR.DR && IER_val.ERBFI);
			UART_TRACE("rx_interrupt value: " << std::boolalpha
				<< rx_interrupt);
		}

		// TX Holding Register Empty
		bool tx_interrupt = (LSR.THRE && IER_val.ETBEI);

		// Line Status (Only OE is implemented)
		bool line_status_int = (IER_val.ELSI && (LSR.OE));

		// --- Priority check ---
		if (line_status_int) {
		    IIR_val.INTERRUPT_PENDING = 0;
		    IIR_val.INTERRUPT_ID = 0x3;
		    intr_asserted = true;
		} else if (rx_interrupt) {
			IIR_val.INTERRUPT_PENDING = 0;
			IIR_val.INTERRUPT_ID = 0x2;
			intr_asserted = true;
		} else if (tx_interrupt) {
			IIR_val.INTERRUPT_PENDING = 0;
			IIR_val.INTERRUPT_ID = 0x1;
			intr_asserted = true;
		} else {
			IIR_val.INTERRUPT_PENDING = 1; // no interrupt
			IIR_val.INTERRUPT_ID = 0;
		}
	}
	

	// Drive INTR line
	UART_INFO("Updating INTR line value " << std::boolalpha << intr_asserted);
	INTR.write(intr_asserted);
}

/**
 * @brief Reset method
 * 
 * This function resets the UART IP core by resetting the registers and
 * FIFOs to their default values. It also drains the FIFOs and updates the
 * interrupt status.
 * 
 * @return void
 */
void UART_IP::reset_method()
{
	UART_DEBUG("Reset asserted - resetting registers and FIFOs");
	// Reset CSML registers to their configured reset values
	reset_all_registers();

	// Clear internal alias Register variables
	RBR_val = 0;
	THR_val = 0;
	DLL_val = 0;
	DLM_val = 0;

	// Reset local copies of register bitfields to defaults
	FCR_val = FCR_fields(); // zeroed (FIFO disabled)
	IER_val = IER_fields(); // zeroed (interrupts disabled)
	IIR_val = IIR_fields(); // default constructor sets FIFO_STATUS=0,
				// INT_PENDING=1;

	// Drain FIFOs
	uint8_t tmp = 0;
	while (rx_fifo.nb_read(tmp)) {
	}
	while (tx_fifo.nb_read(tmp)) {
	}

	// Ensure INTR is deasserted and IIR/interrupt state consistent
	INTR.write(false);
	rx_process_event.cancel();
	tx_process_event.cancel();

	// Re-evaluate interrupt state 
	update_interrupt();

	UART_DEBUG("Reset complete");
}

/**
 * @brief Send byte to UART
 * 
 * This function simulates sending out a serial byte by setting the tx_data
 * and tx_valid signals connected to the UART terminal emulator.
 * 
 * @param byte_to_send The byte to be sent
 */
void UART_IP::send_byte(uint8_t byte_to_send)
{
	UART_DEBUG("Sending byte (UART Tx): " << std::hex << (int)byte_to_send);
	// Send data to terminal via interface
	terminal_port->uart_to_terminal(byte_to_send);

	// Simulate transmission delay using temporal decoupling
	m_qk.inc(m_byte_time);
	if (m_qk.need_sync()) {
		m_qk.sync();
	}

	// If line loopback mode is enabled, dump the byte internally to RX(RBR/RCVR FIFO)
	if(MCR.LINE_LOOPBACK) {
		write_rx(byte_to_send);
	}
}

/**
 * @brief Write byte to receiver FIFO
 * 
 * This function simulates receiving a serial byte by writing it to the RBR or RCVR FIFO based on the FIFO mode.
 * If FIFO is enabled, it writes the byte to the RCVR FIFO. If the FIFO is full, it sets the overflow flag.
 * If FIFO is disabled, it writes the byte to the RBR register. it also triggers the rx_process by notifying the rx_process_event.
 * 
 * @param b The byte to be written
 */
void UART_IP::write_rx(uint8_t b)
{
	UART_DEBUG("Receiving byte (UART Rx): " << std::hex << (int)b);
    if (FCR_val.FIFO_ENABLE) {
        UART_DEBUG("fifo mode in write_rx");
        if (rx_fifo.num_free() > 0) {
            rx_fifo.write(b);
            UART_DEBUG("RX FIFO write: " << std::hex << (int)b);
            LSR.DR = 1;
        } else {
            LSR.OE = 1; // overflow, drop incoming byte
        }
    } else {
        UART_DEBUG("non-fifo mode in write_rx");
        if (LSR.DR) {
            // RBR not yet read; signal overrun and drop incoming byte
            LSR.OE = 1;
			RBR_val = b;
        } else {
            RBR_val = b;
            LSR.DR = 1;
        }
    }
    // Trigger RX processing and interrupts
    rx_process_event.notify(SC_ZERO_TIME);
}

/**
 * @brief Terminal interface implementation: UART to Terminal
 * 
 * This method is called when UART needs to send data to the terminal.
 * This is a placeholder that should never be called since UART uses
 * terminal_port to send data, not receive via this method.
 * 
 * @param data Byte received (not used in UART)
 */
void UART_IP::uart_to_terminal(uint8_t data)
{
	// This should not be called on UART side
	// UART sends via terminal_port->uart_to_terminal()
	UART_DEBUG("Warning: uart_to_terminal called on UART (should not happen)");
}

/**
 * @brief Terminal interface implementation: Terminal to UART
 * 
 * This method is called by the terminal when it receives data from
 * the client and needs to send it to the UART for reception.
 * 
 * @param data Byte received from terminal
 */
void UART_IP::terminal_to_uart(uint8_t data)
{
	UART_DEBUG("Receiving byte from terminal: " << std::hex << (int)data);
	// Write the received byte to RX path
	write_rx(data);
	terminal_port->uart_to_terminal(data);
	// Simulate reception delay using temporal decoupling
	m_qk.inc(m_byte_time);
	if (m_qk.need_sync()) {
		m_qk.sync();
	}
}