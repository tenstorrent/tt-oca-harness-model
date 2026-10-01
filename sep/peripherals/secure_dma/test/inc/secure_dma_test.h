// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file secure_dma_test.h
 * @brief DMA Controller test harness class
 *
 * This file defines the test harness for DMA Controller verification,
 * providing helper methods for register access and test execution.
 */

#pragma once
#include "secure_dma_basetest.h"
#include "reg_logger.h"
#include <memory>

/**
 * @class secure_dma_test
 * @brief DMA Controller test harness for SystemC TLM verification
 *
 * This class extends secure_dma_basetest with test-specific helper functions
 * for validating DMA Controller register behavior and functionality.
 * Provides methods for:
 * - 8-bit, 16-bit, and 32-bit register access
 * - Test setup and teardown
 * - Register value verification
 * - Memory target simulation for DMA transfers
 * - Hardware handshake signal generation
 */
class secure_dma_test : public secure_dma_basetest
{
public:
   SC_HAS_PROCESS(secure_dma_test);

   // =========================================================================
   // TLM Target Sockets for Memory Simulation
   // =========================================================================

   /// TLM target socket for OpenTitan internal memory (32-bit)
   tlm_utils::simple_target_socket<secure_dma_test, 32> ot_target_socket;

   /// TLM target socket for SoC Control Network memory (32-bit)
   tlm_utils::simple_target_socket<secure_dma_test, 32> ctn_target_socket;

   /// TLM target socket for SoC system memory (64-bit)
   tlm_utils::simple_target_socket<secure_dma_test, 64> sys_target_socket;

   // =========================================================================
   // Interrupt Input Ports (monitoring DMA outputs)
   // =========================================================================

   /// Transfer completion interrupt input (monitors DMA output)
   sc_in<bool> dma_done_intr_i;

   /// Chunk completion interrupt input (monitors DMA output)
   sc_in<bool> dma_chunk_done_intr_i;

   /// Error condition interrupt input (monitors DMA output)
   sc_in<bool> dma_error_intr_i;

   // =========================================================================
   // Alert Input Port
   // =========================================================================

   /// Fatal fault alert input (monitors DMA output)
   sc_in<bool> alert_fatal_fault_i;

   // =========================================================================
   // Hardware Handshake Output Ports (test drives these to DMA)
   // =========================================================================

   /// Hardware handshake trigger outputs to DMA [10:0]
   sc_out<bool> lsio_trigger_o[11];

   // =========================================================================
   // Clock and Reset Ports
   // =========================================================================

   /// Abstract clock output to DMA
   sc_out<sc_time> clk_o;

   /// Active-low reset output to DMA
   sc_out<bool> rst_no;

   /**
    * @brief Constructor for DMA test harness
    * @param name SystemC module name
    *
    * Initializes test harness, sets up TLM target sockets for memory
    * simulation, and initializes all ports for DMA verification.
    */
   secure_dma_test(sc_module_name name);

   /**
    * @brief Destructor for DMA test harness
    */
   ~secure_dma_test();

   /**
    * @brief Performs 32-bit register read
    * @param offset Register offset address
    * @param read_value Reference to store read value
    */
   void register_read_32(unsigned int offset, uint32_t &read_value);

   /**
    * @brief Performs 32-bit register write
    * @param offset Register offset address
    * @param write_value Value to write to register
    */
   void register_write_32(unsigned int offset, uint32_t write_value);

   /**
    * @brief Performs 16-bit register read
    * @param offset Register offset address
    * @param read_value Reference to store read value
    */
   void register_read_16(unsigned int offset, uint16_t &read_value);

   /**
    * @brief Performs 16-bit register write
    * @param offset Register offset address
    * @param write_value Value to write to register
    */
   void register_write_16(unsigned int offset, uint16_t write_value);

   /**
    * @brief Performs 8-bit register read
    * @param offset Register offset address
    * @param read_value Reference to store read value
    */
   void register_read_8(unsigned int offset, uint8_t &read_value);

   /**
    * @brief Performs 8-bit register write
    * @param offset Register offset address
    * @param write_value Value to write to register
    */
   void register_write_8(unsigned int offset, uint8_t write_value);

   /**
    * @brief Apply reset sequence to DMA
    * @param duration Reset duration in nanoseconds (default 100ns)
    *
    * Asserts active-low reset for specified duration, then de-asserts.
    */
   void apply_reset(sc_time duration = sc_time(100, SC_NS));

   /**
    * @brief Assert a hardware handshake trigger
    * @param trigger_index Index of trigger to assert (0-10)
    * @param assert_value True to assert, false to de-assert
    */
   void set_lsio_trigger(unsigned int trigger_index, bool assert_value);

   // =========================================================================
   // Memory Initialization Helpers for Test Data Setup
   // =========================================================================

   /**
    * @brief Write single byte to OT memory for test data initialization
    * @param addr Physical address in OT memory space
    * @param data Byte value to write
    *
    * Initializes OT memory with test patterns before DMA transfers.
    * Uses modulo addressing to wrap within allocated memory bounds.
    */
   void write_ot_memory_byte(uint64_t addr, uint8_t data);

  /**
   * @brief Write single byte to OT source/read memory (explicit API)
   * @param addr Physical address in OT memory space
   * @param data Byte value to write
   */
  void write_ot_memory_r_byte(uint64_t addr, uint8_t data);

  /**
   * @brief Write single byte to OT destination/write memory (explicit API)
   * @param addr Physical address in OT memory space
   * @param data Byte value to write
   */
  void write_ot_memory_w_byte(uint64_t addr, uint8_t data);

   /**
    * @brief Write block of data to OT memory
    * @param addr Starting physical address in OT memory space
    * @param data Pointer to source data buffer
    * @param length Number of bytes to write
    *
    * Bulk initialization of OT memory for test patterns.
    * Calls write_ot_memory_byte() for each byte.
    */
   void write_ot_memory_block(uint64_t addr, const unsigned char* data, size_t length);

   /**
    * @brief Read single byte from OT memory for verification
    * @param addr Physical address in OT memory space
    * @return Byte value at specified address
    *
    * Allows tests to verify memory contents after initialization or transfers.
    * Uses modulo addressing to wrap within allocated memory bounds.
    */
   uint8_t read_ot_memory_byte(uint64_t addr);

  /**
   * @brief Read single byte from OT source/read memory (explicit API)
   * @param addr Physical address in OT memory space
   * @return Byte value at specified address
   */
  uint8_t read_ot_memory_r_byte(uint64_t addr);

  /**
   * @brief Read single byte from OT destination/write memory (explicit API)
   * @param addr Physical address in OT memory space
   * @return Byte value at specified address
   */
  uint8_t read_ot_memory_w_byte(uint64_t addr);

  /**
   * @brief Write single byte to CTN source/read memory (explicit API)
   * @param addr Physical address in CTN memory space
   * @param data Byte value to write
   */
  void write_ctn_memory_r_byte(uint64_t addr, uint8_t data);

  /**
   * @brief Write single byte to CTN destination/write memory (explicit API)
   * @param addr Physical address in CTN memory space
   * @param data Byte value to write
   */
  void write_ctn_memory_w_byte(uint64_t addr, uint8_t data);

  /**
   * @brief Read single byte from CTN source/read memory (explicit API)
   * @param addr Physical address in CTN memory space
   * @return Byte value at specified address
   */
  uint8_t read_ctn_memory_r_byte(uint64_t addr);

  /**
   * @brief Read single byte from CTN destination/write memory (explicit API)
   * @param addr Physical address in CTN memory space
   * @return Byte value at specified address
   */
  uint8_t read_ctn_memory_w_byte(uint64_t addr);

  /**
   * @brief Write single byte to SYS source/read memory (explicit API)
   * @param addr Physical address in SYS memory space
   * @param data Byte value to write
   */
  void write_sys_memory_r_byte(uint64_t addr, uint8_t data);

  /**
   * @brief Write single byte to SYS destination/write memory (explicit API)
   * @param addr Physical address in SYS memory space
   * @param data Byte value to write
   */
  void write_sys_memory_w_byte(uint64_t addr, uint8_t data);

  /**
   * @brief Read single byte from SYS source/read memory (explicit API)
   * @param addr Physical address in SYS memory space
   * @return Byte value at specified address
   */
  uint8_t read_sys_memory_r_byte(uint64_t addr);

  /**
   * @brief Read single byte from SYS destination/write memory (explicit API)
   * @param addr Physical address in SYS memory space
   * @return Byte value at specified address
   */
  uint8_t read_sys_memory_w_byte(uint64_t addr);

  /**
   * @brief Write 64-bit word to SYS source/read memory (explicit API)
   * @param addr Physical address in SYS memory space
   * @param data 64-bit value to write
   */
  void write_sys_memory_r_qword(uint64_t addr, uint64_t data);

  /**
   * @brief Write 64-bit word to SYS destination/write memory (explicit API)
   * @param addr Physical address in SYS memory space
   * @param data 64-bit value to write
   */
  void write_sys_memory_w_qword(uint64_t addr, uint64_t data);

  /**
   * @brief Read 64-bit word from SYS source/read memory (explicit API)
   * @param addr Physical address in SYS memory space
   * @return 64-bit value at specified address
   */
  uint64_t read_sys_memory_r_qword(uint64_t addr);

  /**
   * @brief Read 64-bit word from SYS destination/write memory (explicit API)
   * @param addr Physical address in SYS memory space
   * @return 64-bit value at specified address
   */
  uint64_t read_sys_memory_w_qword(uint64_t addr);

  /**
   * @brief Force next OT source READ transaction to return bus error
   *
   * One-shot fault injection used by bus-error validation tests.
   */
  void inject_ot_read_bus_error_once();

  /**
   * @brief Force next OT destination WRITE transaction to return bus error
   *
   * One-shot fault injection used by bus-error validation tests.
   */
  void inject_ot_write_bus_error_once();

  /// Beats whose sep_axi_extension is missing or is not OTHERS_SOURCE_ID.
  unsigned m_sideband_errors = 0;

private:
   /// RegLogger instance for test diagnostics
   RegLogger logger;

   /// Simulated OT destination memory used by OT bus writes (1MB)
   std::vector<uint8_t> m_ot_memory_w;

   /// Simulated OT source memory used by OT bus reads (1MB)
   std::vector<uint8_t> m_ot_memory_r;

  /// One-shot injection flag for OT read bus error response.
  bool m_inject_ot_read_bus_error_once;

  /// One-shot injection flag for OT write bus error response.
  bool m_inject_ot_write_bus_error_once;

   /// Simulated CTN destination memory used by CTN bus writes (1MB)
   std::vector<uint8_t> m_ctn_memory_w;

   /// Simulated CTN source memory used by CTN bus reads (1MB)
   std::vector<uint8_t> m_ctn_memory_r;

   /// Simulated SYS destination memory used by SYS bus writes (4MB)
   std::vector<uint8_t> m_sys_memory_w;

   /// Simulated SYS source memory used by SYS bus reads (4MB)
   std::vector<uint8_t> m_sys_memory_r;

   /**
    * @brief TLM b_transport callback for OT memory
    * @param trans TLM generic payload
    * @param delay Annotated delay
    */
   void ot_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay);

   /**
    * @brief TLM b_transport callback for CTN memory
    * @param trans TLM generic payload
    * @param delay Annotated delay
    */
   void ctn_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay);

   /**
    * @brief TLM b_transport callback for system memory
    * @param trans TLM generic payload
    * @param delay Annotated delay
    */
   void sys_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay);
};