/**
 * @file mailbox_basetest.h
 * @brief Base test infrastructure for mailbox IP verification
 *
 * Provides register offset definitions, access masks, reset values, and TLM
 * initiator socket for mailbox IP register-level testing. Contains enumerations
 * for all 10 registers with their access properties.
 */

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

/**
 * @class mailbox_basetest
 * @brief Base test harness providing register definitions and TLM infrastructure
 *
 * Provides:
 * - Register offsets for all 10 registers (0x00-0x48)
 * - Read/write access masks for access type verification
 * - Reset values for initialization testing
 * - TLM initiator socket for register transactions
 * - Register property structure for test automation
 */
class mailbox_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<mailbox_basetest, 32> initiator_socket; ///< TLM initiator socket for register access (32-bit buswidth, 64-bit registers)
    /**
     * @enum Register_offset
     * @brief Register address offsets within mailbox register space
     */
    enum Register_offset
    {
      WRITE_DATA_OFFSET = (0x0 + 0x00), 
      READ_DATA_OFFSET = (0x8 + 0x00), 
      STATUS_OFFSET = (0x10 + 0x00), 
      ERROR_FLAGS_OFFSET = (0x18 + 0x00), 
      WIRQT_OFFSET = (0x20 + 0x00), 
      RIRQT_OFFSET = (0x28 + 0x00), 
      IRQS_OFFSET = (0x30 + 0x00), 
      IRQEN_OFFSET = (0x38 + 0x00), 
      IRQP_OFFSET = (0x40 + 0x00), 
      CTRL_OFFSET = (0x48 + 0x00)  
    };

    /**
     * @enum Register_Read_Access
     * @brief Read access masks indicating readable bits per register (0=not readable, mask=functional bits)
     */
    enum Register_Read_Access
    {
      WRITE_DATA_READ = (0x0), ///< WO register - no read access
      READ_DATA_READ = (0xffffffffffffffff), ///< RO register - full 64-bit read access (all bits functional)
      STATUS_READ = (0x000000000000000f), ///< RO register - bits [3:0] functional (empty, full, write_level_above_thresh, read_level_above_thresh)
      ERROR_FLAGS_READ = (0x0000000000000003), ///< RO register - bits [1:0] functional (read_error, write_error)
      WIRQT_READ = (0x00000000000000ff), ///< RW register - bits [7:0] functional (wirqt threshold)
      RIRQT_READ = (0x00000000000000ff), ///< RW register - bits [7:0] functional (rirqt threshold)
      IRQS_READ = (0x0000000000000007), ///< RW register - bits [2:0] functional (wtirq, rtirq, eirq)
      IRQEN_READ = (0x0000000000000007), ///< RW register - bits [2:0] functional (wtirq, rtirq, eirq enable)
      IRQP_READ = (0x0000000000000007), ///< RO register - bits [2:0] functional (wtirq, rtirq, eirq pending)
      CTRL_READ = (0x0)  ///< WO register - no read access
    };

    /**
     * @enum Register_Write_Access
     * @brief Write access masks indicating writable bits per register (0=not writable, mask=functional bits)
     */
    enum Register_Write_Access
    {
      WRITE_DATA_WRITE = (0xffffffffffffffff), ///< WO register - full 64-bit write access (all bits functional)
      READ_DATA_WRITE = (0x0), ///< RO register - no write access
      STATUS_WRITE = (0x0), ///< RO register - no write access
      ERROR_FLAGS_WRITE = (0x0), ///< RO register - no write access
      WIRQT_WRITE = (0x00000000000000ff), ///< RW register - bits [7:0] writable (wirqt threshold)
      RIRQT_WRITE = (0x00000000000000ff), ///< RW register - bits [7:0] writable (rirqt threshold)
      IRQS_WRITE = (0x0000000000000007), ///< RW register - bits [2:0] writable (W1C: wtirq, rtirq, eirq)
      IRQEN_WRITE = (0x0000000000000007), ///< RW register - bits [2:0] writable (wtirq, rtirq, eirq enable)
      IRQP_WRITE = (0x0), ///< RO register - no write access
      CTRL_WRITE = (0x0000000000000003) ///< WO register - bits [1:0] writable (wflush, rflush)
    };

    /**
     * @enum Register_Reset_Val
     * @brief Reset values for all registers as specified in RDL
     */
    enum Register_Reset_Val
    {
      WRITE_DATA_RESET = (0x0), ///< Reset: 0x0
      READ_DATA_RESET = (0x0), ///< Reset: 0x0
      STATUS_RESET = (0x1), ///< Reset: 0x1
      ERROR_FLAGS_RESET = (0x0), ///< Reset: 0x0
      WIRQT_RESET = (0x0), ///< Reset: 0x0
      RIRQT_RESET = (0x0), ///< Reset: 0x0
      IRQS_RESET = (0x0), ///< Reset: 0x0
      IRQEN_RESET = (0x0), ///< Reset: 0x0
      IRQP_RESET = (0x0), ///< Reset: 0x0
      CTRL_RESET = (0x0) ///< Reset: 0x0
    };

    /**
     * @struct Register_Property_t
     * @brief Register property structure for test automation
     *
     * Aggregates all properties for a single register to enable table-driven
     * testing of register access, reset values, and access type enforcement.
     */
    struct Register_Property_t
    {
		  unsigned int reg_offset; ///< Register offset address
		  uint64_t read_mask; ///< Read access mask (0=no read, 0xff..ff=readable)
		  uint64_t write_mask; ///< Write access mask (0=no write, 0xff..ff=writable)
		  uint64_t reg_reset; ///< Reset value from RDL specification
		  std::string reg_name; ///< Register name for reporting
    };

    /**
     * @brief Constructor for mailbox base test harness
     * @param name SystemC module hierarchical name
     */
    mailbox_basetest(sc_module_name name) : sc_module(name)
    {

    }

};