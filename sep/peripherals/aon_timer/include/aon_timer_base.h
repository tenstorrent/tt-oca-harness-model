// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file aon_timer_base.h
 * @brief AON Timer base model class providing the register infrastructure.
 *
 * This header defines aon_timer_base, the generated regmodel base class that
 * instantiates all 14 AON Timer memory-mapped registers and binds them to a
 * shared regmodel::Memory backing store connected to a TLM-2.0 32-bit target socket.
 * The derived class aon_timer (in aon_timer.h) inherits from this base and
 * implements functional callbacks.
 */

#pragma once
#include "aon_timer_register.h"
#include <string.h>

/**
 * @class aon_timer_base
 * @brief Generated base class for the AON Timer TLM-2.0 register model.
 *
 * Provides the complete set of 14 AON Timer registers (ALERT_TEST through
 * WKUP_CAUSE) backed by a single regmodel::Memory<32> instance and exposed via a
 * 32-bit TLM simple_target_socket. This class is intended to be subclassed
 * by aon_timer, which adds functional behavior through register callbacks.
 *
 * Register address map summary (base + 0x0):
 *   - ALERT_TEST      0x00  WO   reset=0x0
 *   - WKUP_CTRL       0x04  RW   reset=0x0
 *   - WKUP_THOLD_HI   0x08  RW   reset=0x0
 *   - WKUP_THOLD_LO   0x0C  RW   reset=0x0
 *   - WKUP_COUNT_HI   0x10  RW   reset=0x0
 *   - WKUP_COUNT_LO   0x14  RW   reset=0x0
 *   - WDOG_REGWEN     0x18  RW0C reset=0x1
 *   - WDOG_CTRL       0x1C  RW   reset=0x0
 *   - WDOG_BARK_THOLD 0x20  RW   reset=0x0
 *   - WDOG_BITE_THOLD 0x24  RW   reset=0x0
 *   - WDOG_COUNT      0x28  RW   reset=0x0
 *   - INTR_STATE      0x2C  RW1C reset=0x0
 *   - INTR_TEST       0x30  WO   reset=0x0
 *   - WKUP_CAUSE      0x34  RW0C reset=0x0
 */
class aon_timer_base : public sc_module
{
  public:
    typedef typename regmodel::Reg<32>::DT DT; ///< 32-bit data type alias for register access.

    /**
     * @brief Construct the aon_timer_base, instantiate all registers, and bind the TLM socket.
     * @param name        SystemC hierarchical module name.
     * @param memory_size Size in bytes of the memory region covering all registers.
     *
     * Initializes the regmodel::Memory backing store and all 14 register instances at their
     * respective byte offsets. Binds the memory to the TLM target socket so that
     * incoming TLM transactions are dispatched to the correct register.
     */
    aon_timer_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       ALERT_TEST(std::string(name) + ".ALERT_TEST", memory, (0x00 + 0x00)/sizeof(unsigned int)),
       WKUP_CTRL(std::string(name) + ".WKUP_CTRL", memory, (0x04 + 0x00)/sizeof(unsigned int)),
       WKUP_THOLD_HI(std::string(name) + ".WKUP_THOLD_HI", memory, (0x08 + 0x00)/sizeof(unsigned int)),
       WKUP_THOLD_LO(std::string(name) + ".WKUP_THOLD_LO", memory, (0x0C + 0x00)/sizeof(unsigned int)),
       WKUP_COUNT_HI(std::string(name) + ".WKUP_COUNT_HI", memory, (0x10 + 0x00)/sizeof(unsigned int)),
       WKUP_COUNT_LO(std::string(name) + ".WKUP_COUNT_LO", memory, (0x14 + 0x00)/sizeof(unsigned int)),
       WDOG_REGWEN(std::string(name) + ".WDOG_REGWEN", memory, (0x18 + 0x00)/sizeof(unsigned int)),
       WDOG_CTRL(std::string(name) + ".WDOG_CTRL", memory, (0x1C + 0x00)/sizeof(unsigned int)),
       WDOG_BARK_THOLD(std::string(name) + ".WDOG_BARK_THOLD", memory, (0x20 + 0x00)/sizeof(unsigned int)),
       WDOG_BITE_THOLD(std::string(name) + ".WDOG_BITE_THOLD", memory, (0x24 + 0x00)/sizeof(unsigned int)),
       WDOG_COUNT(std::string(name) + ".WDOG_COUNT", memory, (0x28 + 0x00)/sizeof(unsigned int)),
       INTR_STATE(std::string(name) + ".INTR_STATE", memory, (0x2C + 0x00)/sizeof(unsigned int)),
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x30 + 0x00)/sizeof(unsigned int)),
       WKUP_CAUSE(std::string(name) + ".WKUP_CAUSE", memory, (0x34 + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      regmodel::Memory<32> memory;                                               ///< Shared 32-bit memory backing store for all registers.
      tlm_utils::simple_target_socket<regmodel::Memory<32>, 32> target_socket;  ///< TLM-2.0 32-bit target socket; receives all TL-UL register transactions.

      aon_timer::ALERT_TEST_type<32>      ALERT_TEST;      ///< Alert test register (0x00, WO, reset=0x0).
      aon_timer::WKUP_CTRL_type<32>       WKUP_CTRL;       ///< Wakeup timer control register (0x04, RW, reset=0x0).
      aon_timer::WKUP_THOLD_HI_type<32>   WKUP_THOLD_HI;   ///< Wakeup timer threshold upper 32 bits (0x08, RW, reset=0x0).
      aon_timer::WKUP_THOLD_LO_type<32>   WKUP_THOLD_LO;   ///< Wakeup timer threshold lower 32 bits (0x0C, RW, reset=0x0).
      aon_timer::WKUP_COUNT_HI_type<32>   WKUP_COUNT_HI;   ///< Wakeup timer counter upper 32 bits (0x10, RW, reset=0x0).
      aon_timer::WKUP_COUNT_LO_type<32>   WKUP_COUNT_LO;   ///< Wakeup timer counter lower 32 bits (0x14, RW, reset=0x0).
      aon_timer::WDOG_REGWEN_type<32>     WDOG_REGWEN;     ///< Watchdog write-enable lock register (0x18, RW0C, reset=0x1).
      aon_timer::WDOG_CTRL_type<32>       WDOG_CTRL;       ///< Watchdog timer control register (0x1C, RW, reset=0x0).
      aon_timer::WDOG_BARK_THOLD_type<32> WDOG_BARK_THOLD; ///< Watchdog bark threshold register (0x20, RW, reset=0x0).
      aon_timer::WDOG_BITE_THOLD_type<32> WDOG_BITE_THOLD; ///< Watchdog bite threshold register (0x24, RW, reset=0x0).
      aon_timer::WDOG_COUNT_type<32>      WDOG_COUNT;      ///< Watchdog timer counter register (0x28, RW, reset=0x0).
      aon_timer::INTR_STATE_type<32>      INTR_STATE;      ///< Interrupt state register (0x2C, RW1C, reset=0x0).
      aon_timer::INTR_TEST_type<32>       INTR_TEST;       ///< Interrupt test register (0x30, WO, reset=0x0).
      aon_timer::WKUP_CAUSE_type<32>      WKUP_CAUSE;      ///< Wakeup request status register (0x34, RW0C, reset=0x0).

      /**
       * @brief Reset all 14 AON Timer registers to their hardware reset values.
       *
       * Calls reset() on each register instance in address order. After this
       * call, all registers reflect the values specified in the CSV reset column:
       * WDOG_REGWEN resets to 0x1 (unlocked); all other registers reset to 0x0.
       * This method should be called in response to rst_n or rst_aon_n assertion.
       */
      void reset_all_registers();
};
