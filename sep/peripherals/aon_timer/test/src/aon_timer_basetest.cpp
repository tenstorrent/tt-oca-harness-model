/**
 * @file aon_timer_basetest.cpp
 * @brief AON Timer register map property table for table-driven test verification.
 *
 * Defines the reg_map[] array of 14 Register_Property_t entries, one per AON Timer
 * register. This table is consumed by test utilities and reset-value verification
 * routines to iterate over all registers without hard-coding individual accesses.
 *
 * Each entry captures the register's byte offset, read mask, write mask, expected
 * reset value, and human-readable name. The ordering follows the hardware register
 * address map (0x00 to 0x34 in 0x4-byte steps).
 *
 * Important reset value note: WDOG_REGWEN has a non-zero reset value of 0x1,
 * indicating the watchdog configuration registers are unlocked at power-on reset.
 * All other registers reset to 0x0.
 */

#include "aon_timer_basetest.h"

/**
 * @brief Register property table for all 14 AON Timer registers.
 *
 * Indexed [0..13] in register address order. Each entry provides the
 * offset, read/write masks, reset value, and name for one register.
 * Suitable for table-driven reset verification and generic read/write tests.
 */
aon_timer_basetest::Register_Property_t reg_map[14] = {
  /* 0x00 */ {aon_timer_basetest::ALERT_TEST_OFFSET,      aon_timer_basetest::ALERT_TEST_READ,      aon_timer_basetest::ALERT_TEST_WRITE,      aon_timer_basetest::ALERT_TEST_RESET,      "ALERT_TEST"},
  /* 0x04 */ {aon_timer_basetest::WKUP_CTRL_OFFSET,       aon_timer_basetest::WKUP_CTRL_READ,       aon_timer_basetest::WKUP_CTRL_WRITE,       aon_timer_basetest::WKUP_CTRL_RESET,       "WKUP_CTRL"},
  /* 0x08 */ {aon_timer_basetest::WKUP_THOLD_HI_OFFSET,   aon_timer_basetest::WKUP_THOLD_HI_READ,   aon_timer_basetest::WKUP_THOLD_HI_WRITE,   aon_timer_basetest::WKUP_THOLD_HI_RESET,   "WKUP_THOLD_HI"},
  /* 0x0C */ {aon_timer_basetest::WKUP_THOLD_LO_OFFSET,   aon_timer_basetest::WKUP_THOLD_LO_READ,   aon_timer_basetest::WKUP_THOLD_LO_WRITE,   aon_timer_basetest::WKUP_THOLD_LO_RESET,   "WKUP_THOLD_LO"},
  /* 0x10 */ {aon_timer_basetest::WKUP_COUNT_HI_OFFSET,   aon_timer_basetest::WKUP_COUNT_HI_READ,   aon_timer_basetest::WKUP_COUNT_HI_WRITE,   aon_timer_basetest::WKUP_COUNT_HI_RESET,   "WKUP_COUNT_HI"},
  /* 0x14 */ {aon_timer_basetest::WKUP_COUNT_LO_OFFSET,   aon_timer_basetest::WKUP_COUNT_LO_READ,   aon_timer_basetest::WKUP_COUNT_LO_WRITE,   aon_timer_basetest::WKUP_COUNT_LO_RESET,   "WKUP_COUNT_LO"},
  /* 0x18 */ {aon_timer_basetest::WDOG_REGWEN_OFFSET,     aon_timer_basetest::WDOG_REGWEN_READ,     aon_timer_basetest::WDOG_REGWEN_WRITE,     aon_timer_basetest::WDOG_REGWEN_RESET,     "WDOG_REGWEN"},
  /* 0x1C */ {aon_timer_basetest::WDOG_CTRL_OFFSET,       aon_timer_basetest::WDOG_CTRL_READ,       aon_timer_basetest::WDOG_CTRL_WRITE,       aon_timer_basetest::WDOG_CTRL_RESET,       "WDOG_CTRL"},
  /* 0x20 */ {aon_timer_basetest::WDOG_BARK_THOLD_OFFSET, aon_timer_basetest::WDOG_BARK_THOLD_READ, aon_timer_basetest::WDOG_BARK_THOLD_WRITE, aon_timer_basetest::WDOG_BARK_THOLD_RESET, "WDOG_BARK_THOLD"},
  /* 0x24 */ {aon_timer_basetest::WDOG_BITE_THOLD_OFFSET, aon_timer_basetest::WDOG_BITE_THOLD_READ, aon_timer_basetest::WDOG_BITE_THOLD_WRITE, aon_timer_basetest::WDOG_BITE_THOLD_RESET, "WDOG_BITE_THOLD"},
  /* 0x28 */ {aon_timer_basetest::WDOG_COUNT_OFFSET,      aon_timer_basetest::WDOG_COUNT_READ,      aon_timer_basetest::WDOG_COUNT_WRITE,      aon_timer_basetest::WDOG_COUNT_RESET,      "WDOG_COUNT"},
  /* 0x2C */ {aon_timer_basetest::INTR_STATE_OFFSET,      aon_timer_basetest::INTR_STATE_READ,      aon_timer_basetest::INTR_STATE_WRITE,      aon_timer_basetest::INTR_STATE_RESET,      "INTR_STATE"},
  /* 0x30 */ {aon_timer_basetest::INTR_TEST_OFFSET,       aon_timer_basetest::INTR_TEST_READ,       aon_timer_basetest::INTR_TEST_WRITE,       aon_timer_basetest::INTR_TEST_RESET,       "INTR_TEST"},
  /* 0x34 */ {aon_timer_basetest::WKUP_CAUSE_OFFSET,      aon_timer_basetest::WKUP_CAUSE_READ,      aon_timer_basetest::WKUP_CAUSE_WRITE,      aon_timer_basetest::WKUP_CAUSE_RESET,      "WKUP_CAUSE"}
};