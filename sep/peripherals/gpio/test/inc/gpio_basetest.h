/**
 * GPIO Base Test Class (RDL-Based Single-Pin)
 *
 * Provides register definitions and TLM infrastructure for testing
 * the RDL-based single-pin GPIO model.
 *
 * Register Map (20 bytes / 3 registers):
 *   0x00: DATA_CTRL     - Data and interface control
 *   0x08: ACCESS_FILTER - Security access filtering
 *   0x10: CONTROL       - PAD configuration and strap
 */

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class gpio_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<gpio_basetest, 32> initiator_socket;

    /**
     * RDL Register Offsets
     */
    enum Register_offset
    {
      DATA_CTRL_OFFSET     = 0x00,  // Data and interface control register
      ACCESS_FILTER_OFFSET = 0x08,  // Security access filtering register
      CONTROL_OFFSET       = 0x10   // PAD configuration and strap register
    };

    /**
     * Register Read Masks
     * Defines which bits can be read from each register
     */
    enum Register_Read_Access
    {
      // DATA_CTRL: pad2core[31], lsio_enable[25], RW fields [21:16, 5:4, 0]
      DATA_CTRL_READ       = 0x823F0031,

      // ACCESS_FILTER: All RW fields [18:16, 10:8, 1:0]
      ACCESS_FILTER_READ   = 0x00070703,

      // CONTROL: strap_value[23], strap_valid[22], RW fields [15, 10, 8, 7, 2:0]
      CONTROL_READ         = 0x00C08587
    };

    /**
     * Register Write Masks
     * Defines which bits can be written to each register
     */
    enum Register_Write_Access
    {
      // DATA_CTRL: Only RW fields (not pad2core[31], lsio_enable[25])
      DATA_CTRL_WRITE      = 0x003F0031,

      // ACCESS_FILTER: All fields are RW
      ACCESS_FILTER_WRITE  = 0x00070703,

      // CONTROL: Only RW fields (not strap_value[23], strap_valid[22])
      CONTROL_WRITE        = 0x00008587
    };

    /**
     * Register Reset Values
     * Default values after reset
     */
    enum Register_Reset_Val
    {
      // DATA_CTRL: All fields reset to 0
      DATA_CTRL_RESET      = 0x00000000,

      // ACCESS_FILTER: arprot_requirement=0x1, awprot_requirement=0x1
      ACCESS_FILTER_RESET  = 0x00010100,

      // CONTROL: drive_strength=0x2 (medium drive)
      CONTROL_RESET        = 0x00000002
    };

    /**
     * Register Property Structure
     * Used to define register characteristics in a table
     */
    struct Register_Property_t
    {
      unsigned int reg_offset;
      unsigned int read_mask;
      unsigned int write_mask;
      unsigned int reg_reset;
      std::string reg_name;
    };

    gpio_basetest(sc_module_name name) : sc_module(name)
    {
    }
};
