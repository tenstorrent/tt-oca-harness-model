// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aon_timer_basetest.h
 * @brief AON Timer base test class providing register map constants and TLM infrastructure.
 *
 * This header defines aon_timer_basetest, the generated regmodel test base class.
 * It provides:
 *   - A 32-bit TLM simple_initiator_socket for driving register transactions to the DUT.
 *   - Register_offset enum: byte offsets for all 14 AON Timer registers.
 *   - Register_Read_Access enum: read masks (0x0 for WO registers, 0xffffffff for readable).
 *   - Register_Write_Access enum: write masks for all registers.
 *   - Register_Reset_Val enum: expected reset values for all registers.
 *   - Register_Property_t struct: aggregates offset, masks, reset, and name per register.
 *
 * The derived class aon_timer_test (aon_timer_test.h) inherits from this class and
 * adds concrete register read/write helper methods.
 */

#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

/**
 * @class aon_timer_basetest
 * @brief Generated base test class for AON Timer register verification.
 *
 * Provides the TLM initiator socket and all register property constants required
 * by test cases targeting the aon_timer model. Instantiate aon_timer_test (which
 * derives from this class) rather than this class directly.
 *
 * The initiator_socket must be bound to the aon_timer target_socket before
 * simulation starts. All register accesses are performed via TLM-2.0 blocking
 * transport (b_transport) through this socket.
 */
class aon_timer_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<aon_timer_basetest, 32> initiator_socket; ///< TLM-2.0 32-bit initiator socket for register read/write transactions.

    /**
     * @brief Byte offsets for each AON Timer register relative to base address 0x0.
     *
     * Use these constants as the address argument when constructing TLM transactions
     * targeting specific registers on the DUT.
     */
    enum Register_offset
    {
      ALERT_TEST_OFFSET      = (0x00 + 0x00), ///< ALERT_TEST byte offset (WO, reset=0x0).
      WKUP_CTRL_OFFSET       = (0x04 + 0x00), ///< WKUP_CTRL byte offset (RW, reset=0x0).
      WKUP_THOLD_HI_OFFSET   = (0x08 + 0x00), ///< WKUP_THOLD_HI byte offset (RW, reset=0x0).
      WKUP_THOLD_LO_OFFSET   = (0x0C + 0x00), ///< WKUP_THOLD_LO byte offset (RW, reset=0x0).
      WKUP_COUNT_HI_OFFSET   = (0x10 + 0x00), ///< WKUP_COUNT_HI byte offset (RW, reset=0x0).
      WKUP_COUNT_LO_OFFSET   = (0x14 + 0x00), ///< WKUP_COUNT_LO byte offset (RW, reset=0x0).
      WDOG_REGWEN_OFFSET     = (0x18 + 0x00), ///< WDOG_REGWEN byte offset (RW0C, reset=0x1).
      WDOG_CTRL_OFFSET       = (0x1C + 0x00), ///< WDOG_CTRL byte offset (RW, reset=0x0).
      WDOG_BARK_THOLD_OFFSET = (0x20 + 0x00), ///< WDOG_BARK_THOLD byte offset (RW, reset=0x0).
      WDOG_BITE_THOLD_OFFSET = (0x24 + 0x00), ///< WDOG_BITE_THOLD byte offset (RW, reset=0x0).
      WDOG_COUNT_OFFSET      = (0x28 + 0x00), ///< WDOG_COUNT byte offset (RW, reset=0x0).
      INTR_STATE_OFFSET      = (0x2C + 0x00), ///< INTR_STATE byte offset (RW1C, reset=0x0).
      INTR_TEST_OFFSET       = (0x30 + 0x00), ///< INTR_TEST byte offset (WO, reset=0x0).
      WKUP_CAUSE_OFFSET      = (0x34 + 0x00)  ///< WKUP_CAUSE byte offset (RW0C, reset=0x0).
    };

    /**
     * @brief Read masks for each register (0x0 = write-only register; 0xffffffff = fully readable).
     *
     * ALERT_TEST and INTR_TEST are write-only with no storage; reads return 0x0.
     * All other registers are readable and return the current stored or live value.
     */
    enum Register_Read_Access
    {
      ALERT_TEST_READ      = (0x0),        ///< ALERT_TEST is WO; reads return 0x0.
      WKUP_CTRL_READ       = (0xffffffff), ///< WKUP_CTRL fully readable (bits[12:0] active).
      WKUP_THOLD_HI_READ   = (0xffffffff), ///< WKUP_THOLD_HI fully readable.
      WKUP_THOLD_LO_READ   = (0xffffffff), ///< WKUP_THOLD_LO fully readable.
      WKUP_COUNT_HI_READ   = (0xffffffff), ///< WKUP_COUNT_HI fully readable (volatile counter).
      WKUP_COUNT_LO_READ   = (0xffffffff), ///< WKUP_COUNT_LO fully readable (volatile counter).
      WDOG_REGWEN_READ     = (0xffffffff), ///< WDOG_REGWEN fully readable (bit[0] = lock state).
      WDOG_CTRL_READ       = (0xffffffff), ///< WDOG_CTRL fully readable (bits[1:0] active).
      WDOG_BARK_THOLD_READ = (0xffffffff), ///< WDOG_BARK_THOLD fully readable.
      WDOG_BITE_THOLD_READ = (0xffffffff), ///< WDOG_BITE_THOLD fully readable.
      WDOG_COUNT_READ      = (0xffffffff), ///< WDOG_COUNT fully readable (volatile counter).
      INTR_STATE_READ      = (0xffffffff), ///< INTR_STATE fully readable (bits[1:0] = interrupt pending flags).
      INTR_TEST_READ       = (0x0),        ///< INTR_TEST is WO with no storage; reads return 0x0.
      WKUP_CAUSE_READ      = (0xffffffff)  ///< WKUP_CAUSE fully readable (bit[0] = wakeup request active).
    };

    /**
     * @brief Write masks for each register (all 0xffffffff - writable bits gated at register behavior level).
     *
     * All registers accept 32-bit writes at the TLM transport level. Reserved bits
     * and write-access restrictions (WO, RW0C, RW1C, WDOG_REGWEN gating) are
     * enforced by register callbacks in the functional model, not by these masks.
     */
    enum Register_Write_Access
    {
      ALERT_TEST_WRITE      = (0xffffffff), ///< ALERT_TEST write mask.
      WKUP_CTRL_WRITE       = (0xffffffff), ///< WKUP_CTRL write mask.
      WKUP_THOLD_HI_WRITE   = (0xffffffff), ///< WKUP_THOLD_HI write mask.
      WKUP_THOLD_LO_WRITE   = (0xffffffff), ///< WKUP_THOLD_LO write mask.
      WKUP_COUNT_HI_WRITE   = (0xffffffff), ///< WKUP_COUNT_HI write mask.
      WKUP_COUNT_LO_WRITE   = (0xffffffff), ///< WKUP_COUNT_LO write mask.
      WDOG_REGWEN_WRITE     = (0xffffffff), ///< WDOG_REGWEN write mask.
      WDOG_CTRL_WRITE       = (0xffffffff), ///< WDOG_CTRL write mask.
      WDOG_BARK_THOLD_WRITE = (0xffffffff), ///< WDOG_BARK_THOLD write mask.
      WDOG_BITE_THOLD_WRITE = (0xffffffff), ///< WDOG_BITE_THOLD write mask.
      WDOG_COUNT_WRITE      = (0xffffffff), ///< WDOG_COUNT write mask.
      INTR_STATE_WRITE      = (0xffffffff), ///< INTR_STATE write mask.
      INTR_TEST_WRITE       = (0xffffffff), ///< INTR_TEST write mask.
      WKUP_CAUSE_WRITE      = (0xffffffff)  ///< WKUP_CAUSE write mask.
    };

    /**
     * @brief Expected reset values for all 14 AON Timer registers.
     *
     * These constants reflect the "Reset Value" column from the CSV register map.
     * WDOG_REGWEN resets to 0x1 (unlocked); all other registers reset to 0x0.
     * Use these constants in reset verification test cases.
     */
    enum Register_Reset_Val
    {
      ALERT_TEST_RESET      = (0x0), ///< ALERT_TEST reset value = 0x0.
      WKUP_CTRL_RESET       = (0x0), ///< WKUP_CTRL reset value = 0x0 (disabled, prescaler=0).
      WKUP_THOLD_HI_RESET   = (0x0), ///< WKUP_THOLD_HI reset value = 0x0.
      WKUP_THOLD_LO_RESET   = (0x0), ///< WKUP_THOLD_LO reset value = 0x0.
      WKUP_COUNT_HI_RESET   = (0x0), ///< WKUP_COUNT_HI reset value = 0x0.
      WKUP_COUNT_LO_RESET   = (0x0), ///< WKUP_COUNT_LO reset value = 0x0.
      WDOG_REGWEN_RESET     = (0x1), ///< WDOG_REGWEN reset value = 0x1 (watchdog config unlocked at reset).
      WDOG_CTRL_RESET       = (0x0), ///< WDOG_CTRL reset value = 0x0 (disabled).
      WDOG_BARK_THOLD_RESET = (0x0), ///< WDOG_BARK_THOLD reset value = 0x0.
      WDOG_BITE_THOLD_RESET = (0x0), ///< WDOG_BITE_THOLD reset value = 0x0.
      WDOG_COUNT_RESET      = (0x0), ///< WDOG_COUNT reset value = 0x0.
      INTR_STATE_RESET      = (0x0), ///< INTR_STATE reset value = 0x0 (no interrupts pending).
      INTR_TEST_RESET       = (0x0), ///< INTR_TEST reset value = 0x0.
      WKUP_CAUSE_RESET      = (0x0)  ///< WKUP_CAUSE reset value = 0x0 (no wakeup request active).
    };

    /**
     * @brief Aggregated register property descriptor for use in register map iteration.
     *
     * Bundles the offset, read mask, write mask, reset value, and name of a single
     * register into one structure. An array of these (reg_map[14] in aon_timer_basetest.cpp)
     * enables table-driven register sanity checks and reset value verification.
     */
    struct Register_Property_t
    {
      unsigned int reg_offset;  ///< Byte offset of the register from base address 0x0.
      unsigned int read_mask;   ///< Read mask; 0x0 for write-only registers.
      unsigned int write_mask;  ///< Write mask.
      unsigned int reg_reset;   ///< Expected reset value from hardware specification.
      std::string  reg_name;    ///< Human-readable register name for logging.
    };

    /**
     * @brief Construct the aon_timer_basetest module.
     * @param name SystemC hierarchical module name.
     */
    aon_timer_basetest(sc_module_name name) : sc_module(name)
    {

    }

};