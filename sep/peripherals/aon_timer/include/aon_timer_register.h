
/**
 * Copyright header goes here...
 */

/**
 * @file aon_timer_register.h
 * @brief AON Timer CSML register type definitions.
 *
 * This header defines all 14 register types for the Always-On Timer (AON Timer)
 * peripheral. Each register class is derived from csml_reg<N> and encapsulates
 * its constituent bitfields. Register types correspond directly to the hardware
 * memory-mapped register specification at the respective offsets.
 *
 * Registers covered (base address 0x0):
 *   - ALERT_TEST      (0x00, WO,  reset=0x0)
 *   - WKUP_CTRL       (0x04, RW,  reset=0x0)
 *   - WKUP_THOLD_HI   (0x08, RW,  reset=0x0)
 *   - WKUP_THOLD_LO   (0x0C, RW,  reset=0x0)
 *   - WKUP_COUNT_HI   (0x10, RW,  reset=0x0)
 *   - WKUP_COUNT_LO   (0x14, RW,  reset=0x0)
 *   - WDOG_REGWEN     (0x18, RW0C, reset=0x1)
 *   - WDOG_CTRL       (0x1C, RW,  reset=0x0)
 *   - WDOG_BARK_THOLD (0x20, RW,  reset=0x0)
 *   - WDOG_BITE_THOLD (0x24, RW,  reset=0x0)
 *   - WDOG_COUNT      (0x28, RW,  reset=0x0)
 *   - INTR_STATE      (0x2C, RW1C, reset=0x0)
 *   - INTR_TEST       (0x30, WO,  reset=0x0)
 *   - WKUP_CAUSE      (0x34, RW0C, reset=0x0)
 */

#pragma once
#include<iostream>
#include<systemc.h>
#include "csml_register.h"

/// @brief Namespace encapsulating all AON Timer register type definitions.
namespace aon_timer {

/**
 * @class ALERT_TEST_type
 * @brief Alert test register type (offset 0x00, WO, reset=0x0).
 *
 * Write-only register used to trigger a test fatal alert event for verifying
 * alert connectivity. Reads always return 0x0 (no storage). Writing 1 to
 * bit[0] (fatal_fault) immediately asserts the fatal_fault alert output for
 * one test event. Reserved bits[31:1] are ignored on write.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class ALERT_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct ALERT_TEST register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    ALERT_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      fatal_fault(reg_name + ".fatal_fault", *this, 0, 1),
      reserved0(reg_name + ".reserved0", *this, 1, 31)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> fatal_fault; ///< Bit[0]: Write 1 to trigger one fatal alert test event (WO).
    csml_bitfield<N> reserved0;   ///< Bits[31:1]: Reserved. Reads as 0; writes ignored.
};

/**
 * @class WKUP_CTRL_type
 * @brief Wakeup timer control register type (offset 0x04, RW, reset=0x0).
 *
 * Controls the 64-bit upcounting wakeup timer. Bit[0] enables or disables
 * counting. Bits[12:1] set the 12-bit prescaler: the counter increments once
 * every (prescaler + 1) AON clock ticks. Every write to this register, even if
 * the value is unchanged, unconditionally resets the internal prescaler
 * accumulator to zero (software-visible side-effect). Reserved bits[31:13] read
 * as zero and are ignored on write.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WKUP_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WKUP_CTRL register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WKUP_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      enable(reg_name + ".enable", *this, 0, 1),
      prescaler(reg_name + ".prescaler", *this, 1, 12),
      reserved0(reg_name + ".reserved0", *this, 13, 19)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> enable;    ///< Bit[0]: Set to 1 to start wakeup timer counting; 0 to stop (RW).
    csml_bitfield<N> prescaler; ///< Bits[12:1]: 12-bit prescaler; count rate = clk_aon / (prescaler+1) (RW).
    csml_bitfield<N> reserved0; ///< Bits[31:13]: Reserved. Reads as 0; writes ignored.
};

/**
 * @class WKUP_THOLD_HI_type
 * @brief Wakeup timer threshold upper register type (offset 0x08, RW, reset=0x0).
 *
 * Holds bits[63:32] of the 64-bit wakeup timer threshold. Hardware never
 * modifies this register autonomously, so sequential reads are always safe.
 * When updating the 64-bit threshold, use the safe write sequence: write LO
 * to 0xFFFFFFFF, write new HI, then write new LO to prevent a transient lower
 * threshold from triggering a spurious wakeup interrupt.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WKUP_THOLD_HI_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WKUP_THOLD_HI register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WKUP_THOLD_HI_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      threshold_hi(reg_name + ".threshold_hi", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> threshold_hi; ///< Bits[31:0]: Upper 32 bits of the 64-bit wakeup timer threshold (RW).
};

/**
 * @class WKUP_THOLD_LO_type
 * @brief Wakeup timer threshold lower register type (offset 0x0C, RW, reset=0x0).
 *
 * Holds bits[31:0] of the 64-bit wakeup timer threshold. Part of a non-atomic
 * 64-bit register pair with WKUP_THOLD_HI. When updating the threshold, write
 * this register to 0xFFFFFFFF before updating HI, then write the intended LO
 * value last, to prevent a transient lower combined threshold from causing a
 * spurious wakeup interrupt.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WKUP_THOLD_LO_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WKUP_THOLD_LO register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WKUP_THOLD_LO_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      threshold_lo(reg_name + ".threshold_lo", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> threshold_lo; ///< Bits[31:0]: Lower 32 bits of the 64-bit wakeup timer threshold (RW).
};

/**
 * @class WKUP_COUNT_HI_type
 * @brief Wakeup timer counter upper register type (offset 0x10, RW, reset=0x0).
 *
 * Holds bits[63:32] of the live 64-bit wakeup counter value. The value is
 * volatile: the counter increments asynchronously on each AON clock tick when
 * enabled. Non-atomic access: the counter may overflow LO between HI and LO
 * reads. Safe read sequence: read HI, read LO, read HI again; if HI changed,
 * re-read LO. Disable the timer before writing to avoid race conditions.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WKUP_COUNT_HI_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WKUP_COUNT_HI register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WKUP_COUNT_HI_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      count_hi(reg_name + ".count_hi", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> count_hi; ///< Bits[31:0]: Upper 32 bits of the live 64-bit wakeup timer counter (RW, volatile).
};

/**
 * @class WKUP_COUNT_LO_type
 * @brief Wakeup timer counter lower register type (offset 0x14, RW, reset=0x0).
 *
 * Holds bits[31:0] of the live 64-bit wakeup counter value. The value is
 * volatile: the counter increments asynchronously on each AON clock tick when
 * enabled. Part of a non-atomic 64-bit register pair with WKUP_COUNT_HI. See
 * WKUP_COUNT_HI notes for safe read and write sequences.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WKUP_COUNT_LO_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WKUP_COUNT_LO register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WKUP_COUNT_LO_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      count_lo(reg_name + ".count_lo", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> count_lo; ///< Bits[31:0]: Lower 32 bits of the live 64-bit wakeup timer counter (RW, volatile).
};

/**
 * @class WDOG_REGWEN_type
 * @brief Watchdog write-enable register type (offset 0x18, RW0C, reset=0x1).
 *
 * Controls write access to watchdog configuration registers WDOG_CTRL,
 * WDOG_BARK_THOLD, and WDOG_BITE_THOLD. Bit[0] (regwen) is RW0C: writing 0
 * permanently clears this bit and locks the three gated registers until the next
 * system reset. Writing 1 has no effect. Resets to 0x1 (unlocked). WDOG_COUNT
 * is NOT gated by this register. Reserved bits[31:1] read as 0; writes ignored.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WDOG_REGWEN_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WDOG_REGWEN register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WDOG_REGWEN_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x1),
      regwen(reg_name + ".regwen", *this, 0, 1),
      reserved0(reg_name + ".reserved0", *this, 1, 31)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> regwen;   ///< Bit[0]: 1=unlocked (reset default); write 0 to permanently lock watchdog config (RW0C).
    csml_bitfield<N> reserved0; ///< Bits[31:1]: Reserved. Reads as 0; writes ignored.
};

/**
 * @class WDOG_CTRL_type
 * @brief Watchdog timer control register type (offset 0x1C, RW, reset=0x0).
 *
 * Controls the 32-bit upcounting watchdog timer. Gated by WDOG_REGWEN: writes
 * are silently ignored (bus completes normally) when WDOG_REGWEN.regwen = 0.
 * Bit[0] enables or disables watchdog counting. Bit[1] enables the pause-in-sleep
 * feature: when set to 1 and the sleep_mode input is asserted, the watchdog
 * counter halts to prevent premature bark or bite events during sleep. Reserved
 * bits[31:2] read as 0; writes ignored.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WDOG_CTRL_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WDOG_CTRL register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WDOG_CTRL_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      enable(reg_name + ".enable", *this, 0, 1),
      pause_in_sleep(reg_name + ".pause_in_sleep", *this, 1, 1),
      reserved0(reg_name + ".reserved0", *this, 2, 30)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> enable;         ///< Bit[0]: Set to 1 to start watchdog counting; 0 to stop (RW, gated by WDOG_REGWEN).
    csml_bitfield<N> pause_in_sleep; ///< Bit[1]: When 1, halts watchdog counter while sleep_mode is asserted (RW, gated by WDOG_REGWEN).
    csml_bitfield<N> reserved0;      ///< Bits[31:2]: Reserved. Reads as 0; writes ignored.
};

/**
 * @class WDOG_BARK_THOLD_type
 * @brief Watchdog bark threshold register type (offset 0x20, RW, reset=0x0).
 *
 * Holds the 32-bit watchdog bark threshold. Gated by WDOG_REGWEN: writes are
 * silently ignored when WDOG_REGWEN.regwen = 0. The watchdog bark event (which
 * asserts intr_wdog_timer_bark, nmi_wdog_timer_bark, and wkup_req) fires when
 * WDOG_COUNT >= this threshold and the watchdog is enabled.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WDOG_BARK_THOLD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WDOG_BARK_THOLD register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WDOG_BARK_THOLD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      threshold(reg_name + ".threshold", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> threshold; ///< Bits[31:0]: 32-bit watchdog bark threshold; bark fires when WDOG_COUNT >= this value (RW).
};

/**
 * @class WDOG_BITE_THOLD_type
 * @brief Watchdog bite threshold register type (offset 0x24, RW, reset=0x0).
 *
 * Holds the 32-bit watchdog bite threshold. Gated by WDOG_REGWEN: writes are
 * silently ignored when WDOG_REGWEN.regwen = 0. The watchdog bite event (which
 * asserts aon_timer_rst_req to the power manager to trigger a system reset)
 * fires when WDOG_COUNT >= this threshold and the watchdog is enabled. The bite
 * path is completely independent of the bark interrupt path.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WDOG_BITE_THOLD_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WDOG_BITE_THOLD register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WDOG_BITE_THOLD_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      threshold(reg_name + ".threshold", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> threshold; ///< Bits[31:0]: 32-bit watchdog bite threshold; bite reset fires when WDOG_COUNT >= this value (RW).
};

/**
 * @class WDOG_COUNT_type
 * @brief Watchdog timer counter register type (offset 0x28, RW, reset=0x0).
 *
 * Holds the live 32-bit watchdog counter value. Any write to this register,
 * regardless of the data value written, resets the counter to zero (watchdog
 * petting). The written data is discarded. This register is NOT gated by
 * WDOG_REGWEN; watchdog petting is always permitted regardless of the lock state.
 * The counter increments on each AON clock tick when WDOG_CTRL.enable = 1 and
 * not paused by sleep or halted by lifecycle escalation.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WDOG_COUNT_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WDOG_COUNT register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WDOG_COUNT_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      count(reg_name + ".count", *this, 0, 32)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> count; ///< Bits[31:0]: Live 32-bit watchdog counter; any write resets to 0 (watchdog pet, RW, volatile).
};

/**
 * @class INTR_STATE_type
 * @brief Interrupt state register type (offset 0x2C, RW1C, reset=0x0).
 *
 * Reflects the live interrupt pending status for both AON Timer interrupt sources.
 * Both bits are RW1C (write-1-to-clear): writing 1 clears the bit and de-asserts
 * the corresponding output signal; writing 0 has no effect. Both interrupts are
 * level-sensitive: if the threshold condition persists after clearing, the interrupt
 * re-asserts at the next simulated AON clock tick. Reserved bits[31:2] read as 0.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class INTR_STATE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct INTR_STATE register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    INTR_STATE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      wkup_timer_expired(reg_name + ".wkup_timer_expired", *this, 0, 1),
      wdog_timer_bark(reg_name + ".wdog_timer_bark", *this, 1, 1),
      reserved0(reg_name + ".reserved0", *this, 2, 30)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> wkup_timer_expired; ///< Bit[0]: Set by HW when wakeup counter >= WKUP_THOLD; write 1 to clear (RW1C).
    csml_bitfield<N> wdog_timer_bark;    ///< Bit[1]: Set by HW when watchdog counter >= WDOG_BARK_THOLD; write 1 to clear (RW1C).
    csml_bitfield<N> reserved0;          ///< Bits[31:2]: Reserved. Reads as 0; writes ignored.
};

/**
 * @class INTR_TEST_type
 * @brief Interrupt test register type (offset 0x30, WO, reset=0x0).
 *
 * Write-only register with no storage; reads always return 0x0. Writing 1 to
 * bit[0] immediately sets INTR_STATE.wkup_timer_expired and asserts the
 * intr_wkup_timer_expired output for software testing. Writing 1 to bit[1]
 * immediately sets INTR_STATE.wdog_timer_bark and asserts intr_wdog_timer_bark
 * and nmi_wdog_timer_bark for software testing. Reserved bits[31:2] are ignored
 * on write.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class INTR_TEST_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct INTR_TEST register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    INTR_TEST_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0x0, 0xffffffff, 0x0),
      wkup_timer_expired(reg_name + ".wkup_timer_expired", *this, 0, 1),
      wdog_timer_bark(reg_name + ".wdog_timer_bark", *this, 1, 1),
      reserved0(reg_name + ".reserved0", *this, 2, 30)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> wkup_timer_expired; ///< Bit[0]: Write 1 to force-assert wkup_timer_expired interrupt for testing (WO).
    csml_bitfield<N> wdog_timer_bark;    ///< Bit[1]: Write 1 to force-assert wdog_timer_bark interrupt for testing (WO).
    csml_bitfield<N> reserved0;          ///< Bits[31:2]: Reserved. Reads as 0; writes ignored.
};

/**
 * @class WKUP_CAUSE_type
 * @brief Wakeup request status register type (offset 0x34, RW0C, reset=0x0).
 *
 * Reflects the wakeup request status to the power manager. Bit[0] (cause) is
 * RW0C: set by hardware (in AON domain) when either the wakeup timer or the
 * watchdog bark threshold is crossed and wkup_req is asserted. Writing 0 clears
 * this bit and de-asserts the wkup_req signal to the power manager. Writing 1
 * has no effect. This register and INTR_STATE serve independent purposes and
 * require separate acknowledgment sequences. Reserved bits[31:1] read as 0.
 *
 * @tparam N Memory template width in bits (32 for this peripheral).
 */
template<unsigned int N>
class WKUP_CAUSE_type : public csml_reg<N>
{
  public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    /**
     * @brief Construct WKUP_CAUSE register and bind bitfields.
     * @param reg_name Hierarchical name string for this register instance.
     * @param memory   Reference to the shared CSML memory backing store.
     * @param offset   Word-addressed offset into the memory backing store.
     */
    WKUP_CAUSE_type(std::string reg_name, memory_type &memory, unsigned int offset):
      csml_reg<N>(reg_name, memory, offset, 0xffffffff, 0xffffffff, 0x0),
      cause(reg_name + ".cause", *this, 0, 1),
      reserved0(reg_name + ".reserved0", *this, 1, 31)
    {
      this->set_read_write_restrictions(memory);
    }

    using csml_reg<N>::operator=;
    using csml_reg<N>::operator+=;
    using csml_reg<N>::operator-=;
    using csml_reg<N>::operator/=;
    using csml_reg<N>::operator*=;
    using csml_reg<N>::operator%=;
    using csml_reg<N>::operator^=;
    using csml_reg<N>::operator&=;
    using csml_reg<N>::operator|=;
    using csml_reg<N>::operator>>=;
    using csml_reg<N>::operator<<=;
    csml_bitfield<N> cause;    ///< Bit[0]: Set by HW when wkup_req is asserted; write 0 to clear and de-assert wkup_req (RW0C).
    csml_bitfield<N> reserved0; ///< Bits[31:1]: Reserved. Reads as 0; writes ignored.
};


}