#include "uart.h"
#include <iostream>
#include <iomanip>
#include "uart_test.h"
#include "uart_log_adapter.h"

/**
 * @brief Terminal interface implementation: UART to Terminal
 * 
 * This method is called by the UART IP core when it transmits a byte.
 * It replaces the capture_tx thread.
 * 
 * @param data Byte transmitted by UART
 */
void UART_test::uart_to_terminal(uint8_t data)
{
    observed_tx_.push_back(data);
}

/**
 * @brief Terminal interface implementation: Terminal to UART
 * 
 * This method is called by the terminal (testbench acting as terminal)
 * when it receives data. This is a placeholder since the testbench
 * uses terminal_port to send data, not receive via this method.
 * 
 * @param data Byte received (not used in TB)
 */
void UART_test::terminal_to_uart(uint8_t data)
{
    // This should not be called on TB side
    // TB sends via terminal_port->terminal_to_uart()
    UART_INFO("Warning: terminal_to_uart called on UART_test (should not happen)");
}


void UART_test::verify_tx_results()
{
    bool ok = true;
    if (observed_tx_.size() != expected_tx_.size()) {
        UART_ERROR("TX size mismatch: expected " << expected_tx_.size()
                  << " observed " << observed_tx_.size());
        ok = false;
    }
    size_t n = std::min(observed_tx_.size(), expected_tx_.size());
    for (size_t i = 0; i < n; ++i) {
        if (observed_tx_[i] != expected_tx_[i]) {
            UART_ERROR("  TX mismatch at " << i << ": expected 0x"
                      << std::hex << (int)expected_tx_[i] << " got 0x"
                      << (int)observed_tx_[i] << std::dec);
            ok = false;
        }
    }
    if (ok) {
        UART_INFO("TX verification PASSED (" << expected_tx_.size() << " bytes)");
        passed_test_cases++;
    }
    total_test_cases++;
}

UART_test::UART_test(sc_module_name name) : UART_basetest(name)
{
    // Bind export to this module
    terminal_export(*this);
    
	SC_THREAD(run_test);
	SC_METHOD(monitor_intr);
	sensitive << INTR.pos();
	dont_initialize();
}

void UART_test::monitor_intr()
{
	//uint32_t lsr_tmp = 0;

	UART_INFO(sc_time_stamp() << " Interrupt: INTR line asserted");
	uint32_t iir = 0;
	register_read_32(UART_basetest::IIR_OFFSET, iir);

	if ((iir & 0x01) == 0) { // interrupt pending
		uint8_t intr_id =
		    (iir >> 1) & 0x07; // Bits [3:1] contain interrupt ID

		// Read ITR to detect test-forced interrupts
		uint32_t itr_val = 0;
		register_read_32(UART_basetest::ITR_OFFSET, itr_val);

		switch (intr_id) {
		case 0x03: // Receiver Line Status
			intr_rls_count_++;
			UART_INFO(sc_time_stamp() << " Interrupt: Line Status");
			// Clear ITR.TLSI if this was a test interrupt
			if (itr_val & 0x04) {
				register_write_32(UART_basetest::ITR_OFFSET, itr_val & ~0x04);
			}
			break;
		case 0x02: // Received Data Available
			intr_rda_count_++;
			UART_INFO(sc_time_stamp() << " Interrupt: RX Data Available");
			// Note: Not reading RBR here - let individual test cases read and verify as needed
			// If this was a test interrupt, clear TRBFI
			if (itr_val & 0x01) {
				register_write_32(UART_basetest::ITR_OFFSET, itr_val & ~0x01);
			}
			break;
		case 0x06: // Character Timeout in FIFO mode
			intr_ct_count_++;
			UART_INFO(sc_time_stamp() << " Interrupt: Character Timeout");
			// If this was a test interrupt, clear TRTI
			if (itr_val & 0x20) {
				register_write_32(UART_basetest::ITR_OFFSET, itr_val & ~0x20);
			}
			break;
		case 0x01: { // THR Empty
			intr_thre_count_++;
			UART_INFO(sc_time_stamp() << " Interrupt: THR Empty");
			// If this was a test interrupt, clear TTBEI
			if (itr_val & 0x02) {
				register_write_32(UART_basetest::ITR_OFFSET, itr_val & ~0x02);
			}
			break;
		}
		case 0x00: // Modem Status
			intr_ms_count_++;
			UART_INFO(sc_time_stamp() << " Interrupt: Modem Status");
			// If this was a test interrupt, clear TDSSI
			if (itr_val & 0x08) {
				register_write_32(UART_basetest::ITR_OFFSET, itr_val & ~0x08);
			}
			break;
		case 0x07: // FIFO Error (New case)
			intr_fifo_err_count_++;
			UART_INFO(sc_time_stamp() << " Interrupt: FIFO Error");
			// Clear ITR.TFEI if this was a test interrupt
			if (itr_val & 0x10) {
				register_write_32(UART_basetest::ITR_OFFSET, itr_val & ~0x10);
			}
			break;
		default:
			UART_INFO(sc_time_stamp() << " Interrupt: Unknown (IIR=0x" << std::hex
				  << (int)iir << ")");
		}
	}
}

void UART_test::verify_fifo_trigger_test(const std::vector<uint8_t>& expected_data)
{
    bool intr_ok = (get_intr_rda_count() >= 1); 
    
    std::vector<uint8_t> received_data;
    uint32_t lsr, data;
    // Read all available data
    while (true) {
        register_read_32(UART_basetest::LSR_OFFSET, lsr);
        if (lsr & 0x01) {
            register_read_32(UART_basetest::RBR_OFFSET, data);
            received_data.push_back(static_cast<uint8_t>(data));
        } else {
            break;
        }
    }
    
    bool data_ok = true;
    if (received_data.size() != expected_data.size()) {
        data_ok = false;
        UART_ERROR("  Data size mismatch: expected " << expected_data.size() 
                  << ", got " << received_data.size());
    } else {
        for (size_t i = 0; i < expected_data.size(); ++i) {
            if (received_data[i] != expected_data[i]) {
                data_ok = false;
                UART_ERROR("  Data mismatch at " << i << ": expected 0x" << std::hex << (int)expected_data[i]
                          << ", got 0x" << (int)received_data[i] << std::dec);
                break;
            }
        }
    }
    
    if (intr_ok && data_ok) {
        UART_INFO("FIFO Trigger Test PASSED (RDA count=" << get_intr_rda_count() 
                  << ", Data=" << received_data.size() << " bytes verified)");
        passed_test_cases++;
    } else {
        UART_ERROR("FIFO Trigger Test FAILED (RDA_intr=" << (intr_ok ? "OK" : "FAIL")
                  << " count=" << get_intr_rda_count()
                  << ", Data=" << (data_ok ? "OK" : "FAIL") << ")");
    }
    total_test_cases++;
}

void UART_test::run_test()
{
	uint32_t data;

	// Ensure UART is out of reset initially (active-low)
	reset.write(true);
	wait(10, SC_NS);

	// === RESET FEATURE TEST ===
	UART_INFO("\n==== CASE 0: RESET FEATURE TEST ====");

	// Step 1: Write to some registers and populate FIFOs before reset
	UART_INFO("Setting up state before reset...");
	
	// Enable FIFO and set trigger level
	uint32_t fcr_val = 0b10000111; // FIFO Enable, RX/TX Reset, Trigger Level=2
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);
	
	// Enable interrupts
	register_write_32(UART_basetest::IER_OFFSET, 0x0F); // Enable all interrupts
	wait(10, SC_NS);
	
	// Set LCR to non-zero value
	register_write_32(UART_basetest::LCR_OFFSET, 0x03); // 8-bit word length
	wait(10, SC_NS);
	
	// Inject some RX data to populate FIFO
	drive_rx(0xAA);
	drive_rx(0xBB);
	wait(10, SC_NS);
	
	// Write some TX data
	register_write_32(UART_basetest::THR_OFFSET, 0xCC);
	wait(10, SC_NS);

	// Step 2: Apply reset (active-low)
	UART_INFO("Applying reset...");
	reset.write(false);
	wait(10, SC_NS);
	reset.write(true); // De-assert
	wait(10, SC_NS);

	// Step 3: Verify reset values of readable registers
	UART_INFO("Verifying register reset values...");
	bool reset_test_passed = true;
	
	// Check IER (should be 0x00)
	uint32_t ier_val;
	register_read_32(UART_basetest::IER_OFFSET, ier_val);
	if (ier_val != 0x00) {
		UART_ERROR("  IER reset FAILED: expected 0x00, got 0x" << std::hex << ier_val << std::dec);
		reset_test_passed = false;
	}
	
	// Check LCR (should be 0x00)
	uint32_t lcr_val;
	register_read_32(UART_basetest::LCR_OFFSET, lcr_val);
	if (lcr_val != 0x00) {
		UART_ERROR("  LCR reset FAILED: expected 0x00, got 0x" << std::hex << lcr_val << std::dec);
		reset_test_passed = false;
	}
	
	// Check MCR (should be 0x00)
	uint32_t mcr_val;
	register_read_32(UART_basetest::MCR_OFFSET, mcr_val);
	if (mcr_val != 0x00) {
		UART_ERROR("  MCR reset FAILED: expected 0x00, got 0x" << std::hex << mcr_val << std::dec);
		reset_test_passed = false;
	}
	
	// Check LSR BEFORE reading IIR (IIR read clears THRE bit)
	// LSR should have THRE=1, TEMT=1 (bits 5 and 6 set)
	uint32_t lsr_val;
	register_read_32(UART_basetest::LSR_OFFSET, lsr_val);
	if ((lsr_val & 0x60) != 0x60) {
		UART_ERROR("  LSR reset FAILED: THRE and TEMT should be set, got 0x" << std::hex << lsr_val << std::dec);
		reset_test_passed = false;
	}
	// DR bit should be 0 (no data in RX FIFO)
	if (lsr_val & 0x01) {
		UART_ERROR("  LSR reset FAILED: DR should be 0 (RX FIFO empty), got 0x" << std::hex << lsr_val << std::dec);
		reset_test_passed = false;
	}
	
	// Check IIR AFTER LSR (should have INTERRUPT_PENDING=1, i.e., bit 0 = 1)
	// Note: Reading IIR clears THRE bit in LSR, so we read it last
	uint32_t iir_val;
	register_read_32(UART_basetest::IIR_OFFSET, iir_val);
	if ((iir_val & 0x01) != 0x01) {
		UART_ERROR("  IIR reset FAILED: INTERRUPT_PENDING should be 1, got 0x" << std::hex << iir_val << std::dec);
		reset_test_passed = false;
	}
	
	// Step 4: Verify FIFOs are cleared by trying to read RX data
	UART_INFO("Verifying FIFOs are cleared...");
	register_read_32(UART_basetest::LSR_OFFSET, lsr_val);
	if (lsr_val & 0x01) {
		UART_ERROR("  FIFO clear FAILED: RX FIFO should be empty after reset");
		reset_test_passed = false;
	}
	
	// Step 5: Verify INTR signal is deasserted
	if (INTR.read()) {
		UART_ERROR("  INTR deassert FAILED: INTR should be low after reset");
		reset_test_passed = false;
	}
	
	if (reset_test_passed) {
		UART_INFO("Reset Test PASSED (all registers and FIFOs reset correctly)");
		passed_test_cases++;
	} else {
		UART_ERROR("Reset Test FAILED (see errors above)");
	}
	total_test_cases++;


	// === CASE 1: Set DLAB = 1 and access DLL ===
	UART_INFO("\n==== CASE 1: DLL ACCESS (DLAB = 1) ====");
	lcr_val = 0x80; // DLAB = 1 (bit 7)
	register_write_32(UART_basetest::LCR_OFFSET, lcr_val);
	wait(10, SC_NS);

	// Write to DLL
	uint32_t dll_write_val = 0xAB;
	UART_INFO("Writing value: 0x" << std::hex << (int)dll_write_val << " to the DLL register.......\n");
	register_write_32(UART_basetest::DLL_OFFSET, dll_write_val);
	wait(10, SC_NS);

	// Read back DLL
	uint32_t dll_read_val = 0;
	register_read_32(UART_basetest::DLL_OFFSET, dll_read_val);

	if(dll_read_val == dll_write_val)
	{
		UART_INFO("DLL Access Test PASSED");
		passed_test_cases++;
	}
	else
	{
		UART_ERROR("DLL Access Test FAILED\n" << "Expected: 0x" << std::hex << (int)dll_write_val << "\n" << "Observed: 0x" << std::hex << (int)dll_read_val << "\n");
	}
	total_test_cases++;


		// === CASE 2: Set DLAB = 1 and access DLM ===
	UART_INFO("\n==== CASE 2: DLM ACCESS (DLAB = 1) ====");

	// Write to DLM
	uint32_t dlm_write_val = 0xCD;
	UART_INFO("Writing value: 0x" << std::hex << (int)dlm_write_val << " to the DLM register.......\n");
	register_write_32(UART_basetest::DLM_OFFSET, dlm_write_val);
	wait(10, SC_NS);

	// Read back DLM
	uint32_t dlm_read_val = 0;
	register_read_32(UART_basetest::DLM_OFFSET, dlm_read_val);

	if(dlm_read_val == dlm_write_val)
	{
		UART_INFO("DLM Access Test PASSED");
		passed_test_cases++;
	}
	else
	{
		UART_ERROR("DLM Access Test FAILED\n" << "Expected: 0x" << std::hex << (int)dlm_write_val << "\n" << "Observed: 0x" << std::hex << (int)dlm_read_val << "\n");
	}
	total_test_cases++;


	// === CASE 3: Set DLAB = 0 and test TX via THR ===
	UART_INFO("\n==== CASE 3: TX TEST (DLAB = 0) ====");
	lcr_val = 0x00; // Clear DLAB
	register_write_32(UART_basetest::LCR_OFFSET, lcr_val);
	wait(10, SC_NS);

	fcr_val = 0x0;
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Clear previous TX observations
	observed_tx_.clear();
	expected_tx_.clear();

	// Write to THR
	uint8_t tx_val = '-';
	register_write_32(UART_basetest::THR_OFFSET, tx_val);

	// Wait for UART to process and transmit
	wait(2, SC_NS);  // Give enough time for TX to complete

	// Verify results
	verify_tx_results();
		

	// Read back DLL again after writing to THR, to cross check alaising
	// === CASE 4: Set DLAB = 1 and access DLL ===
	UART_INFO("\n==== CASE 4: DLL ACCESS (DLAB = 1) ====");
	lcr_val = 0x80; // DLAB = 1 (bit 7)
	register_write_32(UART_basetest::LCR_OFFSET, lcr_val);
	wait(10, SC_NS);
	uint32_t dll_read_val2 = 0;
	register_read_32(UART_basetest::DLL_OFFSET, dll_read_val2);

	if(dll_read_val2 == dll_write_val)
	{
		UART_INFO("DLL Access Test PASSED");
		passed_test_cases++;
	}
	else
	{
		UART_ERROR("DLL Access Test FAILED\n" << "Expected: 0x" << std::hex << (int)dll_write_val << "\n" << "Observed: 0x" << std::hex << (int)dll_read_val2 << "\n");
	}
	total_test_cases++;

	// === CASE 5: Set DLAB = 1 and access DLM ===
	UART_INFO("\n==== CASE 5: DLM ACCESS (DLAB = 1) ====");
	
	uint32_t dlm_read_val2 = 0;
	register_read_32(UART_basetest::DLM_OFFSET, dlm_read_val2);

	if(dlm_read_val2 == dlm_write_val)
	{
		UART_INFO("DLM Access Test PASSED");
		passed_test_cases++;
	}
	else
	{
		UART_ERROR("DLM Access Test FAILED\n" << "Expected: 0x" << std::hex << (int)dlm_write_val << "\n" << "Observed: 0x" << std::hex << (int)dlm_read_val2 << "\n");
	}
	total_test_cases++;

	// === CASE 6: RX TEST (polling) ===
	UART_INFO("\n==== CASE 6: RX TEST (polling) ====");

	// Inject data into UART RX
	uint8_t rx_test_val = 0x33;
	drive_rx(rx_test_val);

	// Poll LSR and read RBR if data ready
	lcr_val = 0x00; // Clear DLAB
	register_write_32(UART_basetest::LCR_OFFSET, lcr_val);
	wait(10, SC_NS);
	uint32_t lsr;
	bool rx_poll_success = false;
	
	while (true) {
		register_read_32(UART_basetest::LSR_OFFSET, lsr);
		if (lsr & 0x01) { // DR bit
			register_read_32(UART_basetest::RBR_OFFSET, data);
			UART_INFO(" UART RX POLL: 0x" << std::hex << (int)data << std::dec);
			if (data == rx_test_val) {
				rx_poll_success = true;
			}
		} else {
			break;
		}
		wait(5, SC_NS);
	}
	
	if (rx_poll_success) {
		UART_INFO("RX Polling Test PASSED");
		passed_test_cases++;
	} else {
		UART_ERROR("RX Polling Test FAILED (expected 0x" << std::hex << (int)rx_test_val 
		          << ", got 0x" << (int)data << std::dec << ")");
	}
	total_test_cases++;

	// === CASE 7: FIFO Tx FEATURE TEST by transmitting 8 bytes===
	UART_INFO("\n==== CASE 7: FIFO Tx FEATURE TEST by transmitting 8 bytes====");

	// Enable FIFO (bit0=1), clear RX/TX (bits1=2=1), set trigger level=8
	fcr_val = 0b10000111; // FIFO_Enable=1, RX_Reset=1, TX_Reset=1, Trigger_Level=2
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Clear previous TX observations
	observed_tx_.clear();
	expected_tx_.clear();

	// Transmit multiple bytes (FIFO TX)
	UART_INFO("Writing multiple bytes to THR (FIFO TX)...");
	for (int i = 0; i < 8; ++i) {
		uint8_t tx_byte = 'A' + i;
		register_write_32(UART_basetest::THR_OFFSET, tx_byte);
		wait(2, SC_NS);
	}

	// Verify results
	verify_tx_results();


	// === CASE 8: FIFO Tx FEATURE TEST by transmitting 1 byte====
	UART_INFO("\n==== CASE 8: FIFO Tx FEATURE TEST by transmitting 1 byte====");

	// FIFO already enabled in previous test

	// Clear previous TX observations
	observed_tx_.clear();
	expected_tx_.clear();

	// Transmit single byte (FIFO TX)
	UART_INFO("Writing single byte to THR (FIFO TX)...");
	uint8_t tx_byte = 'Z';
	register_write_32(UART_basetest::THR_OFFSET, tx_byte);

	// Wait for UART to transmit (1 byte: 1μs + 100ns, use 3μs for safety)
	wait(2, SC_NS);

	// Verify results
	verify_tx_results();


	// === CASE 9: RDA in RCVR FIFO with trigger level 1 INTERRUPT (single byte) ===
	UART_INFO("\n==== CASE 9: Receiver Data Available in RCVR FIFO with trigger level 1 INTERRUPT (single byte) ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	ier_val = 0x01; // Enable Received Data Available Interrupt
	register_write_32(UART_basetest::IER_OFFSET, ier_val);
	wait(10, SC_NS);

	// Set FIFO with trigger level = 1 (0x0 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x0);  // Set RCVR_TRIGGER_MS2B = 0x0
	fcr_val = 0b00000111; // FIFO Enable, RX/TX Reset, Trigger Level=0 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject a single RX byte
	std::vector<uint8_t> case9_data = {0x77};
	drive_rx(case9_data[0]);
	
	verify_fifo_trigger_test(case9_data);

	// === CASE 10: RDA in RCVR FIFO with trigger level 4 INTERRUPT (4 bytes) ===
	UART_INFO("\n==== CASE 10: Receiver Data Available in RCVR FIFO with trigger level 4 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 4 (0x1 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x0);  // Set RCVR_TRIGGER_MS2B = 0x0
	fcr_val = 0b01000111; // FIFO Enable, RX/TX Reset, Trigger Level=1 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 4 RX bytes
	std::vector<uint8_t> case10_data;
	for (int i = 0; i < 4; ++i) {
		uint8_t val = 0x80 + i;
		case10_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case10_data);

	// === CASE 11: RDA in RCVR FIFO with trigger level 8 INTERRUPT (8 bytes) ===
	UART_INFO("\n==== CASE 11: Receiver Data Available in RCVR FIFO with trigger level 8 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 8 (0x2 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x0);  // Set RCVR_TRIGGER_MS2B = 0x0
	fcr_val = 0b10000111; // FIFO Enable, RX/TX Reset, Trigger Level=2 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 8 RX bytes
	std::vector<uint8_t> case11_data;
	for (int i = 0; i < 8; ++i) {
		uint8_t val = 0x80 + i;
		case11_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case11_data);

	// === CASE 12: RDA in RCVR FIFO with trigger level 14 INTERRUPT (14 bytes) ===
	UART_INFO("\n==== CASE 12: Receiver Data Available in RCVR FIFO with trigger level 14 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 14 (0x3 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x0);  // Set RCVR_TRIGGER_MS2B = 0x0
	fcr_val = 0b11000111; // FIFO Enable, RX/TX Reset, Trigger Level=3 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 14 RX bytes
	std::vector<uint8_t> case12_data;
	for (int i = 0; i < 14; ++i) {
		uint8_t val = 0x80 + i;
		case12_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case12_data);

	// === CASE 13: RDA in RCVR FIFO with trigger level 32 INTERRUPT (32 bytes) ===
	UART_INFO("\n==== CASE 13: Receiver Data Available in RCVR FIFO with trigger level 32 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 32 (0x4 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x1);  // Set RCVR_TRIGGER_MS2B = 0x1
	fcr_val = 0b00000111; // FIFO Enable, RX/TX Reset, Trigger Level=0 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 32 RX bytes
	std::vector<uint8_t> case13_data;
	for (int i = 0; i < 32; ++i) {
		uint8_t val = 0x90 + (i % 16);
		case13_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case13_data);

	// === CASE 14: RDA in RCVR FIFO with trigger level 64 INTERRUPT (64 bytes) ===
	UART_INFO("\n==== CASE 14: Receiver Data Available in RCVR FIFO with trigger level 64 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 64 (0x5 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x1);  // Set RCVR_TRIGGER_MS2B = 0x1
	fcr_val = 0b01000111; // FIFO Enable, RX/TX Reset, Trigger Level=1 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 64 RX bytes
	std::vector<uint8_t> case14_data;
	for (int i = 0; i < 64; ++i) {
		uint8_t val = 0xA0 + (i % 16);
		case14_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case14_data);

	// === CASE 15: RDA in RCVR FIFO with trigger level 128 INTERRUPT (128 bytes) ===
	UART_INFO("\n==== CASE 15: Receiver Data Available in RCVR FIFO with trigger level 128 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 128 (0x6 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x1);  // Set RCVR_TRIGGER_MS2B = 0x1
	fcr_val = 0b10000111; // FIFO Enable, RX/TX Reset, Trigger Level=2 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 128 RX bytes
	std::vector<uint8_t> case15_data;
	for (int i = 0; i < 128; ++i) {
		uint8_t val = 0xB0 + (i % 16);
		case15_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case15_data);

	// === CASE 16: RDA in RCVR FIFO with trigger level 256 INTERRUPT (256 bytes) ===
	UART_INFO("\n==== CASE 16: Receiver Data Available in RCVR FIFO with trigger level 256 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 256 (0x7 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x1);  // Set RCVR_TRIGGER_MS2B = 0x1
	fcr_val = 0b11000111; // FIFO Enable, RX/TX Reset, Trigger Level=3 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 256 RX bytes
	std::vector<uint8_t> case16_data;
	for (int i = 0; i < 256; ++i) {
		uint8_t val = 0xC0 + (i % 16);
		case16_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case16_data);

	// === CASE 17: RDA in RCVR FIFO with trigger level 512 INTERRUPT (512 bytes) ===
	UART_INFO("\n==== CASE 17: Receiver Data Available in RCVR FIFO with trigger level 512 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 512 (0x8 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x2);  // Set RCVR_TRIGGER_MS2B = 0x2
	fcr_val = 0b00000111; // FIFO Enable, RX/TX Reset, Trigger Level=0 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 512 RX bytes
	std::vector<uint8_t> case17_data;
	for (int i = 0; i < 512; ++i) {
		uint8_t val = 0xD0 + (i % 16);
		case17_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case17_data);

	// === CASE 18: RDA in RCVR FIFO with trigger level 1024 INTERRUPT (1024 bytes) ===
	UART_INFO("\n==== CASE 18: Receiver Data Available in RCVR FIFO with trigger level 1024 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 1024 (0x9 in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x2);  // Set RCVR_TRIGGER_MS2B = 0x2
	fcr_val = 0b01000111; // FIFO Enable, RX/TX Reset, Trigger Level=1 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 1024 RX bytes
	std::vector<uint8_t> case18_data;
	for (int i = 0; i < 1024; ++i) {
		uint8_t val = 0xE0 + (i % 16);
		case18_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case18_data);

	// === CASE 19: RDA in RCVR FIFO with trigger level 2048 INTERRUPT (2048 bytes) ===
	UART_INFO("\n==== CASE 19: Receiver Data Available in RCVR FIFO with trigger level 2048 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 2048 (0xA in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x2);  // Set RCVR_TRIGGER_MS2B = 0x2
	fcr_val = 0b10000111; // FIFO Enable, RX/TX Reset, Trigger Level=2 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 2048 RX bytes
	std::vector<uint8_t> case19_data;
	for (int i = 0; i < 2048; ++i) {
		uint8_t val = 0xF0 + (i % 16);
		case19_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case19_data);

	// === CASE 20: RDA in RCVR FIFO with trigger level 4096 INTERRUPT (4096 bytes) ===
	UART_INFO("\n==== CASE 20: Receiver Data Available in RCVR FIFO with trigger level 4096 bytes INTERRUPT ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Set FIFO trigger level = 4096 (0xB in combined trigger)
	register_write_32(UART_basetest::ECR_OFFSET, 0x2);  // Set RCVR_TRIGGER_MS2B = 0x2
	fcr_val = 0b11000111; // FIFO Enable, RX/TX Reset, Trigger Level=3 (LSB)
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 4096 RX bytes
	std::vector<uint8_t> case20_data;
	for (int i = 0; i < 4096; ++i) {
		uint8_t val = 0x00 + (i % 16);
		case20_data.push_back(val);
		drive_rx(val);
	}
	
	verify_fifo_trigger_test(case20_data);


	// === CASE 21: RDA in RBR INTERRUPT (non-FIFO mode) ===
	UART_INFO("\n==== CASE 21: Receiver Data Available in RBR register INTERRUPT (non-FIFO mode)====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // RDA Interrupt
	wait(10, SC_NS);

	// Disable FIFO
	fcr_val = 0x0;
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Inject 1 RX byte
	uint8_t val = 0x80;
	drive_rx(val);
	
	if (get_intr_rda_count() >= 1) {
		uint32_t lsr_val, rbr_val;
		register_read_32(UART_basetest::LSR_OFFSET, lsr_val);

		if (lsr_val & 0x01) { // DR bit set
			register_read_32(UART_basetest::RBR_OFFSET, rbr_val);

			if ((uint8_t)rbr_val == val) {
				UART_INFO("RDA(non-FIFO) Interrupt Test PASSED (RDA interrupt and RBR data matched: 0x80)");
				passed_test_cases++;
			} else {
				UART_INFO("RDA(non-FIFO) Interrupt Test FAILED (Data mismatch: expected 0x80, got 0x"
						<< std::hex << (int)rbr_val << std::dec << ")");
			}
		} else {
			UART_INFO("RDA(non-FIFO) Interrupt Test FAILED (RDA interrupt but DR bit not set)");
		}
	} else {
		UART_INFO("RDA(non-FIFO) Interrupt Test FAILED (RDA interrupt not triggered)");
	}
	total_test_cases++;
	
	// === CASE 22: THRE INTERRUPT ===
	UART_INFO("\n==== CASE 22: Transmitter Holding Register Empty(THRE) INTERRUPT(Non-Fifo Mode) ====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable THRE interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x02); // THRE Interrupt

	// Disable FIFO
	fcr_val = 0x0;
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);

	// Write to THR
	tx_val = 'A';
	register_write_32(UART_basetest::THR_OFFSET, tx_val);
	
	// Wait for byte transmission (1μs) + interrupt processing
	wait(2, SC_NS);

	// Verify THRE interrupt occurred
	if (get_intr_thre_count() >= 1) {
		UART_INFO("THRE Interrupt Test PASSED (count=" << get_intr_thre_count() << ")");
		passed_test_cases++;
	} else {
		UART_INFO("THRE Interrupt Test FAILED (expected >=1, got " << get_intr_thre_count() << ")");
	}
	total_test_cases++;


	// === CASE 23: THRE INTERRUPT(FIFO Mode) ===
	UART_INFO("\n==== CASE 23: Transmitter Holding Register Empty(THRE) INTERRUPT  in FIFO MODE====");

	// Clear interrupt counters
	clear_intr_counters();

	// Enable THRE interrupt
	register_write_32(UART_basetest::IER_OFFSET,
			 0x02); // THRE Interrupt
	wait(10, SC_NS);

	fcr_val = 0x07;
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);

	// Write to THR
	tx_val = 'A';
	register_write_32(UART_basetest::THR_OFFSET, tx_val);
	wait(2, SC_NS); // allow INTR to be generated and monitor_intr() to process

	// Verify THRE interrupt occurred
	if (get_intr_thre_count() >= 1) { 
		UART_INFO("THRE Interrupt (FIFO) Test PASSED (count=" << get_intr_thre_count() << ")");
		passed_test_cases++;
	} else {
		UART_INFO("THRE Interrupt (FIFO) Test FAILED (expected >=1, got " << get_intr_thre_count() << ")");
	}
	total_test_cases++;


	// === CASE 24: System LOOPBACK(internal) polling ===
	UART_INFO("\n==== CASE 24: Test System LOOPBACK(internal) feature with polling ====");
	// Disable RDA/THRE interrupts
	ier_val = 0x00;
	register_write_32(UART_basetest::IER_OFFSET, ier_val);

	// Enable loopback
	register_write_32(UART_basetest::MCR_OFFSET, 0x10); // Loopback bit set

	// Write THR
	uint32_t loopback_tx_val = 0x5A;
	register_write_32(UART_basetest::THR_OFFSET, loopback_tx_val);
	wait(10, SC_NS);

	// Poll RBR and verify
	register_read_32(UART_basetest::LSR_OFFSET, lsr);
	if (lsr & 0x01) { // DR bit
		register_read_32(UART_basetest::RBR_OFFSET, data);
		UART_INFO(sc_time_stamp()
			  << " LOOPBACK POLL MODE: " << std::hex << (int)data << std::dec);
		if (data == loopback_tx_val) {
			UART_INFO("Loopback Polling Test PASSED");
			passed_test_cases++;
		} else {
			UART_INFO("Loopback Polling Test FAILED (expected 0x" << std::hex 
			          << loopback_tx_val << ", got 0x" << data << std::dec << ")");
		}
	} else {
		UART_INFO("Loopback Polling Test FAILED (no data received, DR=0)");
	}
	total_test_cases++;
	wait(5, SC_NS);

// === CASE 25: System LOOPBACK(internal) INTERRUPT(non-FIFO mode) ===
	UART_INFO("\n==== CASE 25: Test System LOOPBACK(internal) feature with Interrupt(non-FIFO mode) ====");
	
	// Clear interrupt counters
	clear_intr_counters();
	
	// Enable RDA interrupt
	ier_val = 0x01;
	register_write_32(UART_basetest::IER_OFFSET, ier_val);
	
	register_write_32(UART_basetest::ECR_OFFSET, 0x0);  //clear ECR
	register_write_32(UART_basetest::FCR_OFFSET, 0x0);
	wait(10, SC_NS);
	// Enable loopback
	register_write_32(UART_basetest::MCR_OFFSET, 0x10); // Loopback bit set

	// Write THR
	uint32_t loopback_tx_25 = 0xAA;
	register_write_32(UART_basetest::THR_OFFSET, loopback_tx_25);
	wait(10, SC_NS);
	
	// Verify RDA interrupt occurred and read data
	if (get_intr_rda_count() >= 1) {
		// Read the loopback data from RBR
		register_read_32(UART_basetest::LSR_OFFSET, lsr);
		if (lsr & 0x01) { // DR bit
			register_read_32(UART_basetest::RBR_OFFSET, data);
			UART_INFO("  Loopback data read: 0x" << std::hex << (int)data << std::dec);
			if (data == loopback_tx_25) {
				UART_INFO("Loopback Interrupt (non-FIFO) Test PASSED (RDA count=" << get_intr_rda_count() << ", data verified)");
				passed_test_cases++;
			} else {
				UART_INFO("Loopback Interrupt (non-FIFO) Test FAILED (data mismatch: expected 0x" 
				          << std::hex << loopback_tx_25 << ", got 0x" << data << std::dec << ")");
			}
		} else {
			UART_INFO("Loopback Interrupt (non-FIFO) Test FAILED (RDA fired but DR=0)");
		}
	} else {
		UART_INFO("Loopback Interrupt (non-FIFO) Test FAILED (expected >=1 RDA, got " << get_intr_rda_count() << ")");
	}	
	total_test_cases++;

	// === CASE 26: System LOOPBACK(internal) INTERRUPT(FIFO mode) ===
	UART_INFO("\n==== CASE 26: Test System LOOPBACK(internal) feature with Interrupt(FIFO mode) ====");
	
	// Clear interrupt counters
	clear_intr_counters();
	
	// Enable RDA interrupt
	ier_val = 0x01;
	register_write_32(UART_basetest::IER_OFFSET, ier_val);
	
	register_write_32(UART_basetest::ECR_OFFSET, 0x0);  //clear ECR
	register_write_32(UART_basetest::FCR_OFFSET, 0x01);
	wait(10, SC_NS);
	// Enable loopback
	register_write_32(UART_basetest::MCR_OFFSET, 0x10); // Loopback bit set

	// Write THR
	uint32_t loopback_tx_26 = 0xAA;
	register_write_32(UART_basetest::THR_OFFSET, loopback_tx_26);
	wait(10, SC_NS);
	
	// Verify RDA interrupt occurred and read data
	if (get_intr_rda_count() >= 1) {
		// Read the loopback data from RBR
		register_read_32(UART_basetest::LSR_OFFSET, lsr);
		if (lsr & 0x01) { // DR bit
			register_read_32(UART_basetest::RBR_OFFSET, data);
			UART_INFO("  Loopback data read: 0x" << std::hex << (int)data << std::dec);
			if (data == loopback_tx_26) {
				UART_INFO("Loopback Interrupt (FIFO) Test PASSED (RDA count=" << get_intr_rda_count() << ", data verified)");
				passed_test_cases++;
			} else {
				UART_INFO("Loopback Interrupt (FIFO) Test FAILED (data mismatch: expected 0x" 
				          << std::hex << loopback_tx_26 << ", got 0x" << data << std::dec << ")");
			}
		} else {
			UART_INFO("Loopback Interrupt (FIFO) Test FAILED (RDA fired but DR=0)");
		}
	} else {
		UART_INFO("Loopback Interrupt (FIFO) Test FAILED (expected >=1 RDA, got " << get_intr_rda_count() << ")");
	}
	total_test_cases++;

	
	// === CASE 27: ITR Test - TRBFI (RX Data Ready) ===
	UART_INFO("\n==== CASE 27: ITR Test - TRBFI (RX Data Ready) ====\n");
	clear_intr_counters();
	register_write_32(UART_basetest::ITR_OFFSET, 0x00);
	wait(10, SC_NS);
	register_write_32(UART_basetest::ITR_OFFSET, 0x01);
	wait(20, SC_NS);
	if (get_intr_rda_count() >= 1) {
		UART_INFO("ITR TRBFI Test PASSED (RDA count=" << get_intr_rda_count() << ")");
		passed_test_cases++;
	} else {
		UART_INFO("ITR TRBFI Test FAILED (expected >=1 RDA, got " << get_intr_rda_count() << ")");
	}
	total_test_cases++;

	// === CASE 28: ITR Test - TTBEI (THR Empty) ===
	UART_INFO("\n==== CASE 28: ITR Test - TTBEI (THR Empty) ====\n");
	clear_intr_counters();
	register_write_32(UART_basetest::ITR_OFFSET, 0x02);
	wait(20, SC_NS);
	if (get_intr_thre_count() >= 1) {
		UART_INFO("ITR TTBEI Test PASSED (THRE count=" << get_intr_thre_count() << ")");
		passed_test_cases++;
	} else {
		UART_INFO("ITR TTBEI Test FAILED (expected >=1 THRE, got " << get_intr_thre_count() << ")");
	}
	total_test_cases++;

	// === CASE 29: ITR Test - TLSI (Line Status) ===
	UART_INFO("\n==== CASE 29: ITR Test - TLSI (Line Status) ====\n");
	clear_intr_counters();
	register_write_32(UART_basetest::ITR_OFFSET, 0x04);
	wait(20, SC_NS);
	if (get_intr_rls_count() >= 1) {
		UART_INFO("ITR TLSI Test PASSED (RLS count=" << get_intr_rls_count() << ")");
		passed_test_cases++;
	} else {
		UART_INFO("ITR TLSI Test FAILED (expected >=1 RLS, got " << get_intr_rls_count() << ")");
	}
	total_test_cases++;

	// === CASE 30: ITR Test - TDSSI (Modem Status) ===
	UART_INFO("\n==== CASE 30: ITR Test - TDSSI (Modem Status) ====\n");
	clear_intr_counters();
	register_write_32(UART_basetest::ITR_OFFSET, 0x08);
	wait(20, SC_NS);
	if (get_intr_ms_count() >= 1) {
		UART_INFO("ITR TDSSI Test PASSED (MS count=" << get_intr_ms_count() << ")");
		passed_test_cases++;
	} else {
		UART_INFO("ITR TDSSI Test FAILED (expected >=1 MS, got " << get_intr_ms_count() << ")");
	}
	total_test_cases++;

	// === CASE 31: ITR Test - TFEI (FIFO Error) ===
	UART_INFO("\n==== CASE 31: ITR Test - TFEI (FIFO Error) ====\n");
	clear_intr_counters();
	register_write_32(UART_basetest::ITR_OFFSET, 0x10);
	wait(20, SC_NS);
	if (get_intr_fifo_err_count() >= 1) {
		UART_INFO("ITR TFEI Test PASSED (FIFO_ERR count=" << get_intr_fifo_err_count() << ")");
		passed_test_cases++;
	} else {
		UART_INFO("ITR TFEI Test FAILED (expected >=1 FIFO_ERR, got " << get_intr_fifo_err_count() << ")");
	}
	total_test_cases++;

	// === CASE 32: ITR Test - TRTI (Character Timeout) ===
	UART_INFO("\n==== CASE 32: ITR Test - TRTI (Character Timeout) ====\n");
	clear_intr_counters();
	register_write_32(UART_basetest::ITR_OFFSET, 0x20);
	wait(20, SC_NS);
	if (get_intr_ct_count() >= 1) {
		UART_INFO("ITR TRTI Test PASSED (CT count=" << get_intr_ct_count() << ")");
		passed_test_cases++;
	} else {
		UART_INFO("ITR TRTI Test FAILED (expected >=1 CT, got " << get_intr_ct_count() << ")");
	}
	total_test_cases++;
	

	// === CASE 33: Line Loopback (MCR.LINE_LOOPBACK=1, LOOP=0) ===
	UART_INFO("\n==== CASE 33: Line Loopback (external line mirrored internally) ====");

	// Clear counters and observations
	clear_intr_counters();
	observed_tx_.clear();
	expected_tx_.clear();

	// Enable RDA interrupt
	register_write_32(UART_basetest::IER_OFFSET, 0x01);
	// Non-FIFO or FIFO trigger=1 to get immediate RDA
	register_write_32(UART_basetest::ECR_OFFSET, 0x0);
	register_write_32(UART_basetest::FCR_OFFSET, 0x07);

	// Ensure only LINE_LOOPBACK
	register_write_32(UART_basetest::MCR_OFFSET, 0x20);
	
	// Write a byte; model should transmit on TX AND reflect to RX
	uint32_t line_loopback_val = 'Y';
	register_write_32(UART_basetest::THR_OFFSET, line_loopback_val);
	
	// Wait for transmission (1μs) + loopback processing
	wait(2, SC_NS);
	
	// Verify TX transmission occurred
	bool tx_ok = false;
	if (observed_tx_.size() >= 1 && observed_tx_[0] == line_loopback_val) {
		tx_ok = true;
	}
	
	// Verify RX loopback occurred
	bool rx_ok = false;
	if (get_intr_rda_count() >= 1) {
		register_read_32(UART_basetest::LSR_OFFSET, lsr);
		if (lsr & 0x01) {
			register_read_32(UART_basetest::RBR_OFFSET, data);
			UART_INFO("  Line loopback RX data: 0x" << std::hex << (int)data << std::dec);
			if (data == line_loopback_val) {
				rx_ok = true;
			}
		}
	}
	
	if (tx_ok && rx_ok) {
		UART_INFO("Line Loopback Test PASSED (TX transmitted and RX looped back)");
		passed_test_cases++;
	} else {
		UART_INFO("Line Loopback Test FAILED (TX=" << (tx_ok ? "OK" : "FAIL") 
		          << ", RX=" << (rx_ok ? "OK" : "FAIL") << ")");
	}
	total_test_cases++;
	
	// Clear
	register_write_32(UART_basetest::MCR_OFFSET, 0x00);

	// === CASE 34: Both LOOP and LINE_LOOPBACK set (precedence check) ===
	UART_INFO("\n==== CASE 34: Both LOOP and LINE_LOOPBACK set (expect line-loopback behavior, no double RX) ====");
	
	// Clear counters and observations
	clear_intr_counters();
	observed_tx_.clear();
	expected_tx_.clear();
	
	// Set both LOOP and LINE_LOOPBACK (LINE_LOOPBACK should take precedence)
	register_write_32(UART_basetest::MCR_OFFSET, 0x30); // both bits
	
	// Transmit two bytes
	register_write_32(UART_basetest::THR_OFFSET, 'A');
	wait(2, SC_NS);
	register_write_32(UART_basetest::THR_OFFSET, 'B');
	wait(2, SC_NS);
	
	// Verify TX transmission occurred (line loopback should transmit)
	bool tx_ok_34 = (observed_tx_.size() == 2 && observed_tx_[0] == 'A' && observed_tx_[1] == 'B');
	
	// Verify RX loopback occurred (should get at least 1 RDA interrupt, possibly 2 depending on FIFO trigger)
	// The key is: no double RX (not 4 interrupts from both loopback modes)
	bool rx_count_ok_34 = (get_intr_rda_count() >= 1 && get_intr_rda_count() <= 2);
	
	// Read and verify the looped-back data are same as transmitted data
	bool rx_data_ok_34 = false;
	uint8_t rx_bytes[2] = {0, 0};
	int rx_idx = 0;
	for (int i = 0; i < 2; i++) {
		register_read_32(UART_basetest::LSR_OFFSET, lsr);
		if (lsr & 0x01) { // Data Ready(DR bit)
			register_read_32(UART_basetest::RBR_OFFSET, data);
			if (rx_idx < 2) {
				rx_bytes[rx_idx++] = data;
			}
		}
	}
	rx_data_ok_34 = (rx_bytes[0] == 'A' && rx_bytes[1] == 'B');
	
	if (tx_ok_34 && rx_count_ok_34 && rx_data_ok_34) {
		UART_INFO("Both Loopback Modes Test PASSED (line-loopback precedence confirmed, no double RX)");
		passed_test_cases++;
	} else {
		UART_INFO("Both Loopback Modes Test FAILED (TX=" << (tx_ok_34 ? "OK" : "FAIL")
		          << ", RX_count=" << (rx_count_ok_34 ? "OK" : "FAIL") << " (got " << get_intr_rda_count() << ")"
		          << ", RX_data=" << (rx_data_ok_34 ? "OK" : "FAIL") << ")");
	}
	total_test_cases++;
	
	// Clear
	register_write_32(UART_basetest::MCR_OFFSET, 0x00);
	wait(10, SC_NS);

	// Disable interrupts for safety before finishing
	register_write_32(UART_basetest::IER_OFFSET, 0x00);
	wait(10, SC_NS);


	// === CASE 35: OE in non-FIFO mode, interrupts disabled ===
	UART_INFO("\n==== CASE 35: OE in non-FIFO mode (INT disabled) ====\n");
	
	// Clear interrupt counters
	clear_intr_counters();
	
	// Enable ELSI (Error Line Status Interrupt) to detect OE
	register_write_32(UART_basetest::IER_OFFSET, 0x04);
	wait(5, SC_NS);
	// Disable FIFO
	register_write_32(UART_basetest::FCR_OFFSET, 0x00);
	wait(5, SC_NS);
	
	// Inject first byte -> DR=1
	drive_rx(0x11);
	// Inject second byte before reading -> OE should set and RBR overwritten
	drive_rx(0x22);
	
	// Verify OE occurred
	uint32_t lsr_oe;
	register_read_32(UART_basetest::LSR_OFFSET, lsr_oe);
	bool oe_set = (lsr_oe & 0x02); // OE bit is bit 1
	
	// Verify RBR contains the second byte (first was overwritten)
	uint32_t rbr_val;
	register_read_32(UART_basetest::RBR_OFFSET, rbr_val);
	bool rbr_correct = (rbr_val == 0x22);
	
	// Verify RLS interrupt fired
	bool rls_intr = (get_intr_rls_count() >= 1);
	
	if (oe_set && rbr_correct && rls_intr) {
		UART_INFO("OE Non-FIFO Test PASSED (OE set, RBR=0x22, RLS interrupt fired)");
		passed_test_cases++;
	} else {
		UART_INFO("OE Non-FIFO Test FAILED (OE=" << (oe_set ? "OK" : "FAIL")
		          << ", RBR=" << (rbr_correct ? "OK" : "FAIL") << " (got 0x" << std::hex << rbr_val << std::dec << ")"
		          << ", RLS_intr=" << (rls_intr ? "OK" : "FAIL") << ")");
	}
	total_test_cases++;

	//=== CASE 36: OE in FIFO mode, write 4097 bytes (1 over capacity) ===
	UART_INFO("\n==== CASE 36: OE in FIFO mode (4097 bytes) ====\n");
	
	// Clear interrupt counters
	clear_intr_counters();
	
	// Enable ELSI to detect OE
	register_write_32(UART_basetest::IER_OFFSET, 0x04);
	wait(5, SC_NS);
	// Enable FIFO, set trigger low (1-byte) for quick RDA, extended trigger MSBs=0
	register_write_32(UART_basetest::ECR_OFFSET, 0x0);
	register_write_32(UART_basetest::FCR_OFFSET, 0x07); // enable + resets + trigger=1
	wait(10, SC_NS);
	
	// Fill FIFO: depth assumed 4096, write 4097th to cause OE
	for (int i = 0; i < 4097; ++i) {
		drive_rx(static_cast<uint8_t>(i & 0xFF));
	}
	wait(50, SC_NS);
	
	// Verify OE occurred
	uint32_t lsr_fifo_oe;
	register_read_32(UART_basetest::LSR_OFFSET, lsr_fifo_oe);
	bool oe_fifo_set = (lsr_fifo_oe & 0x02); // OE bit is bit 1
	
	// Verify RLS interrupt fired
	bool rls_fifo_intr = (get_intr_rls_count() >= 1);
	
	// Count how many bytes are in FIFO (should be 4096, the 4097th was lost)
	int fifo_count = 0;
	while (true) {
		uint32_t lsr_check;
		register_read_32(UART_basetest::LSR_OFFSET, lsr_check);
		if (lsr_check & 0x01) { // DR bit
			uint32_t dummy;
			register_read_32(UART_basetest::RBR_OFFSET, dummy);
			fifo_count++;
		} else {
			break;
		}
	}
	bool fifo_count_ok = (fifo_count == 4096);
	
	if (oe_fifo_set && rls_fifo_intr && fifo_count_ok) {
		UART_INFO("OE FIFO Test PASSED (OE set, RLS interrupt fired, FIFO had 4096 bytes)");
		passed_test_cases++;
	} else {
		UART_INFO("OE FIFO Test FAILED (OE=" << (oe_fifo_set ? "OK" : "FAIL")
		          << ", RLS_intr=" << (rls_fifo_intr ? "OK" : "FAIL")
		          << ", FIFO_count=" << (fifo_count_ok ? "OK" : "FAIL") << " (got " << fifo_count << "))");
	}
	total_test_cases++;
	
	// === CASE 37: Read from Empty RCVR FIFO in FIFO Mode ===
	UART_INFO("\n\n==== CASE 37: Read from Empty RCVR FIFO in FIFO Mode ====\n");
	
	// Clear interrupt counters
	clear_intr_counters();
	
	// Enable FIFO mode and reset FIFOs to ensure they're empty
	fcr_val = 0x07; // FIFO Enable, RX/TX Reset, Trigger Level=0
	register_write_32(UART_basetest::FCR_OFFSET, fcr_val);
	wait(10, SC_NS);
	
	// Enable ERBFI interrupt to exercise the interrupt update path
	register_write_32(UART_basetest::IER_OFFSET, 0x01); // Enable RDA interrupt
	wait(10, SC_NS);
	
	// Verify LSR.DR is 0 (FIFO should be empty)
	uint32_t lsr_before;
	register_read_32(UART_basetest::LSR_OFFSET, lsr_before);
	bool dr_clear_before = ((lsr_before & 0x01) == 0);
	
	// Attempt to read from RBR (which will read from empty RCVR FIFO)
	uint32_t rbr_empty_val;
	register_read_32(UART_basetest::RBR_OFFSET, rbr_empty_val);
	
	// Verify the read value is 0
	bool value_is_zero = (rbr_empty_val == 0);
	
	// Verify LSR.DR is still 0 after the read
	uint32_t lsr_after;
	register_read_32(UART_basetest::LSR_OFFSET, lsr_after);
	bool dr_clear_after = ((lsr_after & 0x01) == 0);
	
	// Verify no RDA interrupt was triggered (FIFO is empty, so no data available)
	bool no_rda_interrupt = (get_intr_rda_count() == 0);
	
	if (dr_clear_before && value_is_zero && dr_clear_after && no_rda_interrupt) {
		UART_INFO("Empty FIFO Read Test PASSED (value=0, LSR.DR=0, no RDA interrupt)");
		passed_test_cases++;
	} else {
		UART_ERROR("Empty FIFO Read Test FAILED (DR_before=" << (dr_clear_before ? "OK" : "FAIL")
		          << ", value=" << (value_is_zero ? "OK" : "FAIL") << " (got 0x" << std::hex << rbr_empty_val << std::dec << ")"
		          << ", DR_after=" << (dr_clear_after ? "OK" : "FAIL")
		          << ", no_RDA_intr=" << (no_rda_interrupt ? "OK" : "FAIL") << " (count=" << get_intr_rda_count() << "))");
	}
	total_test_cases++;
	
	// Print final test report
	UART_INFO("\n=============================================");
	UART_INFO("TEST REPORT");
	UART_INFO("=============================================");
	UART_INFO("Total Test Cases: " << total_test_cases);
	UART_INFO("Passed Test Cases: " << passed_test_cases);
	UART_INFO("Failed Test Cases: " << (total_test_cases - passed_test_cases));
	float pass_percentage = (total_test_cases > 0) ? ((float)passed_test_cases / total_test_cases * 100.0) : 0.0;
	UART_INFO("Pass Percentage: " << std::fixed << std::setprecision(2) << pass_percentage << "%");
	UART_INFO("=============================================");
	
	UART_INFO("==== TEST DONE ====");
	sc_stop();
}

void UART_test::drive_rx(uint8_t value)
{
    // Send data to UART via interface
    terminal_port->terminal_to_uart(value);  
}

// ------------------------------------------------------------
// Helper functions for register read/write
// ------------------------------------------------------------
void UART_test::register_read_32(unsigned int offset, uint32_t &read_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&read_value));
    trans.set_data_length(4);  // 4 bytes for 32-bit
   // trans.set_streaming_width(4);

    initiator_socket->b_transport(trans, delay);
}

void UART_test::register_write_32(unsigned int offset, uint32_t write_value)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&write_value));
    trans.set_data_length(4);  // 4 bytes for 32-bit
   // trans.set_streaming_width(4);

    initiator_socket->b_transport(trans, delay);

    // Track expected TX sequence when writing THR (only if DLAB=0)
    if (offset == UART_basetest::THR_OFFSET) {
        uint32_t lcr_val = 0;
        register_read_32(UART_basetest::LCR_OFFSET, lcr_val);
        if (((lcr_val >> 7) & 0x1) == 0) { // DLAB==0
            expected_tx_.push_back(static_cast<uint8_t>(write_value & 0xFF));
        }
    }
}