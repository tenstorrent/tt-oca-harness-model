// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "entropy_src_register.h"
#include <string.h>

class entropy_src_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;
    entropy_src_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       COMPONENT_ID(std::string(name) + ".COMPONENT_ID", memory, (0x00 + 0x00)/sizeof(unsigned int)), 
       CTRL(std::string(name) + ".CTRL", memory, (0x04 + 0x00)/sizeof(unsigned int)), 
       STATUS(std::string(name) + ".STATUS", memory, (0x08 + 0x00)/sizeof(unsigned int)), 
       DEBUG_CTRL(std::string(name) + ".DEBUG_CTRL", memory, (0x0C + 0x00)/sizeof(unsigned int)), 
       INTR_STATUS(std::string(name) + ".INTR_STATUS", memory, (0x10 + 0x00)/sizeof(unsigned int)), 
       INTR_ENABLE(std::string(name) + ".INTR_ENABLE", memory, (0x14 + 0x00)/sizeof(unsigned int)), 
       INTR_TEST(std::string(name) + ".INTR_TEST", memory, (0x18 + 0x00)/sizeof(unsigned int)), 
       SHA256_STATUS(std::string(name) + ".SHA256_STATUS", memory, (0x1C + 0x00)/sizeof(unsigned int)), 
       FIFO_CTRL(std::string(name) + ".FIFO_CTRL", memory, (0x20 + 0x00)/sizeof(unsigned int)), 
       FIFO_STATUS(std::string(name) + ".FIFO_STATUS", memory, (0x24 + 0x00)/sizeof(unsigned int)), 
       FIFO_RDATA(std::string(name) + ".FIFO_RDATA", memory, (0x28 + 0x00)/sizeof(unsigned int)), 
       HEALTH_TEST_CTRL(std::string(name) + ".HEALTH_TEST_CTRL", memory, (0x30 + 0x00)/sizeof(unsigned int)), 
       HEALTH_TEST_WINDOW_SIZE(std::string(name) + ".HEALTH_TEST_WINDOW_SIZE", memory, (0x34 + 0x00)/sizeof(unsigned int)), 
       MARKOV_TEST_PROB_THRESHOLDS(std::string(name) + ".MARKOV_TEST_PROB_THRESHOLDS", memory, (0x38 + 0x00)/sizeof(unsigned int)), 
       HEALTH_TEST_STATUS(std::string(name) + ".HEALTH_TEST_STATUS", memory, (0x40 + 0x00)/sizeof(unsigned int)), 
       REPETITION_TEST_COUNT(std::string(name) + ".REPETITION_TEST_COUNT", memory, (0x44 + 0x00)/sizeof(unsigned int)), 
       APT_PATTERN_COUNT_1BIT(std::string(name) + ".APT_PATTERN_COUNT_1BIT", memory, (0x50 + 0x00)/sizeof(unsigned int)), 
       APT_PATTERN_COUNT_2BIT(std::string(name) + ".APT_PATTERN_COUNT_2BIT", memory, (0x54 + 0x00)/sizeof(unsigned int)), 
       APT_PATTERN_COUNT_3BIT(std::string(name) + ".APT_PATTERN_COUNT_3BIT", memory, (0x58 + 0x00)/sizeof(unsigned int)), 
       APT_PATTERN_COUNT_4BIT(std::string(name) + ".APT_PATTERN_COUNT_4BIT", memory, (0x5C + 0x00)/sizeof(unsigned int)), 
       APT_PROPORTION_1BIT(std::string(name) + ".APT_PROPORTION_1BIT", memory, (0x60 + 0x00)/sizeof(unsigned int)), 
       APT_PROPORTION_2BIT(std::string(name) + ".APT_PROPORTION_2BIT", memory, (0x64 + 0x00)/sizeof(unsigned int)), 
       APT_PROPORTION_3BIT(std::string(name) + ".APT_PROPORTION_3BIT", memory, (0x68 + 0x00)/sizeof(unsigned int)), 
       APT_PROPORTION_4BIT(std::string(name) + ".APT_PROPORTION_4BIT", memory, (0x6C + 0x00)/sizeof(unsigned int)), 
       MARKOV_TEST_COUNTS_0(std::string(name) + ".MARKOV_TEST_COUNTS_0", memory, (0x80 + 0x00)/sizeof(unsigned int)), 
       MARKOV_TEST_COUNTS_1(std::string(name) + ".MARKOV_TEST_COUNTS_1", memory, (0x84 + 0x00)/sizeof(unsigned int)), 
       MARKOV_TEST_PROBABILITIES(std::string(name) + ".MARKOV_TEST_PROBABILITIES", memory, (0x88 + 0x00)/sizeof(unsigned int)), 
       RING_OSC_ENABLE(std::string(name) + ".RING_OSC_ENABLE", memory, (0x90 + 0x00)/sizeof(unsigned int)), 
       RING_OSC_TUNE(std::string(name) + ".RING_OSC_TUNE", memory, (0x94 + 0x00)/sizeof(unsigned int)), 
       RING_OSC_CTRL(std::string(name) + ".RING_OSC_CTRL", memory, (0x98 + 0x00)/sizeof(unsigned int)), 
       DECORRELATOR_CTRL(std::string(name) + ".DECORRELATOR_CTRL", memory, (0xA0 + 0x00)/sizeof(unsigned int)), 
       DECORRELATOR_MASK(std::string(name) + ".DECORRELATOR_MASK", memory, (0xA4 + 0x00)/sizeof(unsigned int)), 
       STARTUP_CTRL(std::string(name) + ".STARTUP_CTRL", memory, (0xB0 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_0_HEALTH_STATUS(std::string(name) + ".GENERATOR_0_HEALTH_STATUS", memory, (0xC0 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_1_HEALTH_STATUS(std::string(name) + ".GENERATOR_1_HEALTH_STATUS", memory, (0xC4 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_2_HEALTH_STATUS(std::string(name) + ".GENERATOR_2_HEALTH_STATUS", memory, (0xC8 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_3_HEALTH_STATUS(std::string(name) + ".GENERATOR_3_HEALTH_STATUS", memory, (0xCC + 0x00)/sizeof(unsigned int)), 
       GENERATOR_4_HEALTH_STATUS(std::string(name) + ".GENERATOR_4_HEALTH_STATUS", memory, (0xD0 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_5_HEALTH_STATUS(std::string(name) + ".GENERATOR_5_HEALTH_STATUS", memory, (0xD4 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_6_HEALTH_STATUS(std::string(name) + ".GENERATOR_6_HEALTH_STATUS", memory, (0xD8 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_7_HEALTH_STATUS(std::string(name) + ".GENERATOR_7_HEALTH_STATUS", memory, (0xDC + 0x00)/sizeof(unsigned int)), 
       GENERATOR_8_HEALTH_STATUS(std::string(name) + ".GENERATOR_8_HEALTH_STATUS", memory, (0xE0 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_9_HEALTH_STATUS(std::string(name) + ".GENERATOR_9_HEALTH_STATUS", memory, (0xE4 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_10_HEALTH_STATUS(std::string(name) + ".GENERATOR_10_HEALTH_STATUS", memory, (0xE8 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_11_HEALTH_STATUS(std::string(name) + ".GENERATOR_11_HEALTH_STATUS", memory, (0xEC + 0x00)/sizeof(unsigned int)), 
       GENERATOR_0_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_0_SAMPLE_CLK_CONFIG", memory, (0x100 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_1_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_1_SAMPLE_CLK_CONFIG", memory, (0x104 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_2_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_2_SAMPLE_CLK_CONFIG", memory, (0x108 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_3_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_3_SAMPLE_CLK_CONFIG", memory, (0x10C + 0x00)/sizeof(unsigned int)), 
       GENERATOR_4_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_4_SAMPLE_CLK_CONFIG", memory, (0x110 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_5_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_5_SAMPLE_CLK_CONFIG", memory, (0x114 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_6_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_6_SAMPLE_CLK_CONFIG", memory, (0x118 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_7_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_7_SAMPLE_CLK_CONFIG", memory, (0x11C + 0x00)/sizeof(unsigned int)), 
       GENERATOR_8_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_8_SAMPLE_CLK_CONFIG", memory, (0x120 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_9_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_9_SAMPLE_CLK_CONFIG", memory, (0x124 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_10_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_10_SAMPLE_CLK_CONFIG", memory, (0x128 + 0x00)/sizeof(unsigned int)), 
       GENERATOR_11_SAMPLE_CLK_CONFIG(std::string(name) + ".GENERATOR_11_SAMPLE_CLK_CONFIG", memory, (0x12C + 0x00)/sizeof(unsigned int)), 
       HT_WATERMARK_NUM(std::string(name) + ".HT_WATERMARK_NUM", memory, (0x130 + 0x00)/sizeof(unsigned int)), 
       HT_WATERMARK(std::string(name) + ".HT_WATERMARK", memory, (0x134 + 0x00)/sizeof(unsigned int)), 
       REPCNT_TOTAL_FAILS(std::string(name) + ".REPCNT_TOTAL_FAILS", memory, (0x138 + 0x00)/sizeof(unsigned int)), 
       APT_HI_TOTAL_FAILS(std::string(name) + ".APT_HI_TOTAL_FAILS", memory, (0x13C + 0x00)/sizeof(unsigned int)), 
       APT_LO_TOTAL_FAILS(std::string(name) + ".APT_LO_TOTAL_FAILS", memory, (0x140 + 0x00)/sizeof(unsigned int)), 
       MARKOV_HI_TOTAL_FAILS(std::string(name) + ".MARKOV_HI_TOTAL_FAILS", memory, (0x144 + 0x00)/sizeof(unsigned int)), 
       MARKOV_LO_TOTAL_FAILS(std::string(name) + ".MARKOV_LO_TOTAL_FAILS", memory, (0x148 + 0x00)/sizeof(unsigned int)), 
       ALERT_SUMMARY_FAIL_COUNTS(std::string(name) + ".ALERT_SUMMARY_FAIL_COUNTS", memory, (0x14C + 0x00)/sizeof(unsigned int)), 
       ALERT_FAIL_COUNTS(std::string(name) + ".ALERT_FAIL_COUNTS", memory, (0x150 + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      csml_memory<32> memory;
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      
      entropy_src::COMPONENT_ID_type<32> COMPONENT_ID;
      
      entropy_src::CTRL_type<32> CTRL;
      
      entropy_src::STATUS_type<32> STATUS;
      
      entropy_src::DEBUG_CTRL_type<32> DEBUG_CTRL;
      
      entropy_src::INTR_STATUS_type<32> INTR_STATUS;
      
      entropy_src::INTR_ENABLE_type<32> INTR_ENABLE;
      
      entropy_src::INTR_TEST_type<32> INTR_TEST;
      
      entropy_src::SHA256_STATUS_type<32> SHA256_STATUS;
      
      entropy_src::FIFO_CTRL_type<32> FIFO_CTRL;
      
      entropy_src::FIFO_STATUS_type<32> FIFO_STATUS;
      
      entropy_src::FIFO_RDATA_type<32> FIFO_RDATA;
      
      entropy_src::HEALTH_TEST_CTRL_type<32> HEALTH_TEST_CTRL;
      
      entropy_src::HEALTH_TEST_WINDOW_SIZE_type<32> HEALTH_TEST_WINDOW_SIZE;
      
      entropy_src::MARKOV_TEST_PROB_THRESHOLDS_type<32> MARKOV_TEST_PROB_THRESHOLDS;
      
      entropy_src::HEALTH_TEST_STATUS_type<32> HEALTH_TEST_STATUS;
      
      entropy_src::REPETITION_TEST_COUNT_type<32> REPETITION_TEST_COUNT;
      
      entropy_src::APT_PATTERN_COUNT_1BIT_type<32> APT_PATTERN_COUNT_1BIT;
      
      entropy_src::APT_PATTERN_COUNT_2BIT_type<32> APT_PATTERN_COUNT_2BIT;
      
      entropy_src::APT_PATTERN_COUNT_3BIT_type<32> APT_PATTERN_COUNT_3BIT;
      
      entropy_src::APT_PATTERN_COUNT_4BIT_type<32> APT_PATTERN_COUNT_4BIT;
      
      entropy_src::APT_PROPORTION_1BIT_type<32> APT_PROPORTION_1BIT;
      
      entropy_src::APT_PROPORTION_2BIT_type<32> APT_PROPORTION_2BIT;
      
      entropy_src::APT_PROPORTION_3BIT_type<32> APT_PROPORTION_3BIT;
      
      entropy_src::APT_PROPORTION_4BIT_type<32> APT_PROPORTION_4BIT;
      
      entropy_src::MARKOV_TEST_COUNTS_0_type<32> MARKOV_TEST_COUNTS_0;
      
      entropy_src::MARKOV_TEST_COUNTS_1_type<32> MARKOV_TEST_COUNTS_1;
      
      entropy_src::MARKOV_TEST_PROBABILITIES_type<32> MARKOV_TEST_PROBABILITIES;
      
      entropy_src::RING_OSC_ENABLE_type<32> RING_OSC_ENABLE;
      
      entropy_src::RING_OSC_TUNE_type<32> RING_OSC_TUNE;
      
      entropy_src::RING_OSC_CTRL_type<32> RING_OSC_CTRL;
      
      entropy_src::DECORRELATOR_CTRL_type<32> DECORRELATOR_CTRL;
      
      entropy_src::DECORRELATOR_MASK_type<32> DECORRELATOR_MASK;
      
      entropy_src::STARTUP_CTRL_type<32> STARTUP_CTRL;
      
      entropy_src::GENERATOR_0_HEALTH_STATUS_type<32> GENERATOR_0_HEALTH_STATUS;
      
      entropy_src::GENERATOR_1_HEALTH_STATUS_type<32> GENERATOR_1_HEALTH_STATUS;
      
      entropy_src::GENERATOR_2_HEALTH_STATUS_type<32> GENERATOR_2_HEALTH_STATUS;
      
      entropy_src::GENERATOR_3_HEALTH_STATUS_type<32> GENERATOR_3_HEALTH_STATUS;
      
      entropy_src::GENERATOR_4_HEALTH_STATUS_type<32> GENERATOR_4_HEALTH_STATUS;
      
      entropy_src::GENERATOR_5_HEALTH_STATUS_type<32> GENERATOR_5_HEALTH_STATUS;
      
      entropy_src::GENERATOR_6_HEALTH_STATUS_type<32> GENERATOR_6_HEALTH_STATUS;
      
      entropy_src::GENERATOR_7_HEALTH_STATUS_type<32> GENERATOR_7_HEALTH_STATUS;
      
      entropy_src::GENERATOR_8_HEALTH_STATUS_type<32> GENERATOR_8_HEALTH_STATUS;
      
      entropy_src::GENERATOR_9_HEALTH_STATUS_type<32> GENERATOR_9_HEALTH_STATUS;
      
      entropy_src::GENERATOR_10_HEALTH_STATUS_type<32> GENERATOR_10_HEALTH_STATUS;
      
      entropy_src::GENERATOR_11_HEALTH_STATUS_type<32> GENERATOR_11_HEALTH_STATUS;
      
      entropy_src::GENERATOR_0_SAMPLE_CLK_CONFIG_type<32> GENERATOR_0_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_1_SAMPLE_CLK_CONFIG_type<32> GENERATOR_1_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_2_SAMPLE_CLK_CONFIG_type<32> GENERATOR_2_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_3_SAMPLE_CLK_CONFIG_type<32> GENERATOR_3_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_4_SAMPLE_CLK_CONFIG_type<32> GENERATOR_4_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_5_SAMPLE_CLK_CONFIG_type<32> GENERATOR_5_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_6_SAMPLE_CLK_CONFIG_type<32> GENERATOR_6_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_7_SAMPLE_CLK_CONFIG_type<32> GENERATOR_7_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_8_SAMPLE_CLK_CONFIG_type<32> GENERATOR_8_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_9_SAMPLE_CLK_CONFIG_type<32> GENERATOR_9_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_10_SAMPLE_CLK_CONFIG_type<32> GENERATOR_10_SAMPLE_CLK_CONFIG;
      
      entropy_src::GENERATOR_11_SAMPLE_CLK_CONFIG_type<32> GENERATOR_11_SAMPLE_CLK_CONFIG;
      
      entropy_src::HT_WATERMARK_NUM_type<32> HT_WATERMARK_NUM;
      
      entropy_src::HT_WATERMARK_type<32> HT_WATERMARK;
      
      entropy_src::REPCNT_TOTAL_FAILS_type<32> REPCNT_TOTAL_FAILS;
      
      entropy_src::APT_HI_TOTAL_FAILS_type<32> APT_HI_TOTAL_FAILS;
      
      entropy_src::APT_LO_TOTAL_FAILS_type<32> APT_LO_TOTAL_FAILS;
      
      entropy_src::MARKOV_HI_TOTAL_FAILS_type<32> MARKOV_HI_TOTAL_FAILS;
      
      entropy_src::MARKOV_LO_TOTAL_FAILS_type<32> MARKOV_LO_TOTAL_FAILS;
      
      entropy_src::ALERT_SUMMARY_FAIL_COUNTS_type<32> ALERT_SUMMARY_FAIL_COUNTS;
      
      entropy_src::ALERT_FAIL_COUNTS_type<32> ALERT_FAIL_COUNTS;
      
      void reset_all_registers();
};
