// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file spi_flash_test.cpp
 * @brief Unit tests for the SPI Flash model
 *
 * Pure C++ — no SystemC required.  Tests both the SFDP structure layer
 * and the spi_flash_model command handler.
 *
 * Ported from:
 *
 * Test groups
 * -----------
 * A.  DWORD 1–16  : individual SFDP bitfield get/set, boundary, round-trip
 * B.  Table / ROM : jedec_basic_table_t, sfdp_header_t, sfdp_rom_t
 * C.  Utilities   : addr_mode_to_string, qer_to_string, parser functions
 * D.  Model       : spi_flash_model command-handler (read/program/erase/…)
 */

#include "spi_flash_model.h"
#include "spi_flash_sfdp_utils.h"

#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <fstream>
#include <vector>

// ============================================================================
// TEST HARNESS
// ============================================================================

static int s_tests_run    = 0;
static int s_tests_passed = 0;
static int s_tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        s_tests_run++; \
        if (cond) { \
            s_tests_passed++; \
            std::cout << "  PASS: " << (msg) << "\n"; \
        } else { \
            s_tests_failed++; \
            std::cerr << "  FAIL: " << (msg) << "\n"; \
        } \
    } while (0)

#define TEST_SECTION(title) \
    std::cout << "\n[" << (title) << "]\n"

// ============================================================================
// A. DWORD 1 — Architecture & Fast Read Support
// ============================================================================

static void test_dword1()
{
    TEST_SECTION("A.DWORD1: Architecture & Fast Read Support");

    dword_1_t d;

    // Erase size (bits 1:0)
    d.set_erase_size_support(0); TEST_ASSERT(d.get_erase_size() == 0, "Erase size = 0");
    d.set_erase_size_support(3); TEST_ASSERT(d.get_erase_size() == 3, "Erase size = 3 (max)");
    d.set_erase_size_support(5); TEST_ASSERT(d.get_erase_size() == 1, "Erase size overflow masked (5 -> 1)");

    // Write granularity (bit 2)
    d.set_write_granularity(true);  TEST_ASSERT(d.get_write_granularity() == true,  "Write granularity = true");
    d.set_write_granularity(false); TEST_ASSERT(d.get_write_granularity() == false, "Write granularity = false");

    // Volatile SR (bit 3)
    d.set_volatile_bp(true);  TEST_ASSERT(d.get_volatile_status_register() == true,  "Volatile SR = true");
    d.set_volatile_bp(false); TEST_ASSERT(d.get_volatile_status_register() == false, "Volatile SR = false");

    // Address bytes (bits 18:17)
    d.set_address_bytes(ADDR_3_BYTE_ONLY);  TEST_ASSERT(d.get_address_bytes() == ADDR_3_BYTE_ONLY,  "Address mode = 3-byte only");
    d.set_address_bytes(ADDR_3_OR_4_BYTE);  TEST_ASSERT(d.get_address_bytes() == ADDR_3_OR_4_BYTE,  "Address mode = 3 or 4-byte");
    d.set_address_bytes(ADDR_4_BYTE_ONLY);  TEST_ASSERT(d.get_address_bytes() == ADDR_4_BYTE_ONLY,  "Address mode = 4-byte only");

    // Fast Read support flags
    d.set_fast_read_1_1_2_support(true); TEST_ASSERT(d.get_fast_read_1_1_2_support() == true, "FR 1-1-2 support = true");
    d.set_fast_read_1_2_2_support(true); TEST_ASSERT(d.get_fast_read_1_2_2_support() == true, "FR 1-2-2 support = true");
    d.set_fast_read_1_1_4_support(true); TEST_ASSERT(d.get_fast_read_1_1_4_support() == true, "FR 1-1-4 support = true");
    d.set_fast_read_1_4_4_support(true); TEST_ASSERT(d.get_fast_read_1_4_4_support() == true, "FR 1-4-4 support = true");

    // Reserved bits 7:5 must always be 0b111
    uint32_t dw = d.to_dword();
    TEST_ASSERT(((dw >> 5) & 0x7) == 0x7, "Read-only bits 7:5 = 0b111");

    // to_bytes() round-trip
    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = (uint32_t(bytes[0])) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 2 — Density
// ============================================================================

static void test_dword2()
{
    TEST_SECTION("A.DWORD2: Density");

    dword_2_t d;

    d.set_density(64u*1024*1024);
    TEST_ASSERT(d.get_density() == 64u*1024*1024,     "Density 64 Mbit (linear)");

    d.set_density(256u*1024*1024);
    TEST_ASSERT(d.get_density() == 256u*1024*1024,    "Density 256 Mbit (linear)");

    d.set_density(512ULL*1024*1024);
    TEST_ASSERT(d.get_density() == 512ULL*1024*1024,  "Density 512 Mbit (power-of-2)");

    d.set_density(1024ULL*1024*1024);
    TEST_ASSERT(d.get_density() == 1024ULL*1024*1024, "Density 1 Gbit (power-of-2)");

    d.set_density(1);
    TEST_ASSERT(d.get_density() == 1, "Minimum density = 1 bit");

    // from_dword / to_dword round-trip
    uint32_t orig = d.to_dword();
    dword_2_t d2; d2.from_dword(orig);
    TEST_ASSERT(d2.get_density() == d.get_density(), "from_dword/to_dword round-trip");
}

// ============================================================================
// A. DWORD 3 — Fast Read (1-1-4) & (1-4-4)
// ============================================================================

static void test_dword3()
{
    TEST_SECTION("A.DWORD3: Fast Read (1-1-4) & (1-4-4)");

    dword_3_t d;

    d.set_1_1_4_opcode(0x6B);
    TEST_ASSERT(d.get_1_1_4_opcode() == 0x6B, "(1-1-4) opcode = 0x6B");

    d.set_1_1_4_mode_clocks(7);
    TEST_ASSERT(d.get_1_1_4_mode_clocks() == 7, "(1-1-4) mode cycles = 7");

    d.set_1_1_4_mode_clocks(15);
    TEST_ASSERT(d.get_1_1_4_mode_clocks() == 7, "(1-1-4) mode cycles overflow masked (15 -> 7)");

    d.set_1_1_4_wait_states(255);
    TEST_ASSERT(d.get_1_1_4_wait_states() == 31, "(1-1-4) wait states overflow masked (255 -> 31)");

    // (1-4-4) fields via raw dword check
    d.set_1_4_4_opcode(0xEB);
    d.set_1_4_4_mode_clocks(2);
    d.set_1_4_4_wait_states(4);
    uint32_t dw = d.to_dword();
    TEST_ASSERT(((dw >> 8) & 0xFF) == 0xEB && ((dw >> 5) & 0x7) == 2 && (dw & 0x1F) == 4,
                "(1-4-4) all fields set correctly");

    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 4 — Fast Read (1-1-2) & (1-2-2)
// ============================================================================

static void test_dword4()
{
    TEST_SECTION("A.DWORD4: Fast Read (1-1-2) & (1-2-2)");

    dword_4_t d;

    d.set_1_1_2_opcode(0x3B);
    TEST_ASSERT(d.get_1_1_2_opcode() == 0x3B, "(1-1-2) opcode = 0x3B");

    d.set_1_1_2_mode_clocks(15);
    TEST_ASSERT(d.get_1_1_2_mode_clocks() == 7, "(1-1-2) mode cycles overflow masked");

    d.set_1_1_2_wait_states(8);
    TEST_ASSERT(d.get_1_1_2_wait_states() == 8, "(1-1-2) wait states = 8");

    d.set_1_2_2_opcode(0xBB);
    uint32_t dw = d.to_dword();
    TEST_ASSERT(((dw >> 24) & 0xFF) == 0xBB, "(1-2-2) opcode = 0xBB");

    d.set_1_2_2_mode_clocks(4);
    d.set_1_2_2_wait_states(4);
    dw = d.to_dword();
    TEST_ASSERT(((dw >> 21) & 0x7) == 4 && ((dw >> 16) & 0x1F) == 4,
                "(1-2-2) mode+wait set correctly");

    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 5 — (2-2-2) & (4-4-4) Support Flags
// ============================================================================

static void test_dword5()
{
    TEST_SECTION("A.DWORD5: (2-2-2) & (4-4-4) Support Flags");

    dword_5_t d;

    d.set_2_2_2_support(true);
    TEST_ASSERT((d.to_dword() & 0x1) != 0, "(2-2-2) support = true");

    d.set_4_4_4_support(true);
    TEST_ASSERT((d.to_dword() & (1<<4)) != 0, "(4-4-4) support = true");

    d.set_2_2_2_support(false);
    d.set_4_4_4_support(false);
    uint32_t dw = d.to_dword();
    TEST_ASSERT((dw & 0x1) == 0 && (dw & (1<<4)) == 0, "Both flags cleared");

    // Reserved bits 31:5 and 3:1 must be 1
    d.set_2_2_2_support(true);
    d.set_4_4_4_support(true);
    dw = d.to_dword();
    TEST_ASSERT(((dw >> 5) & 0x7FFFFFF) == 0x7FFFFFF, "Reserved bits 31:5 = all 1's");
    TEST_ASSERT(((dw >> 1) & 0x7) == 0x7,             "Reserved bits 3:1 = 111b");

    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 6 — Fast Read (2-2-2) Parameters
// ============================================================================

static void test_dword6()
{
    TEST_SECTION("A.DWORD6: Fast Read (2-2-2) Parameters");

    dword_6_t d;

    d.set_2_2_2_opcode(0xBB);
    d.set_2_2_2_mode_clocks(4);
    d.set_2_2_2_wait_states(8);
    uint32_t dw = d.to_dword();
    TEST_ASSERT(((dw >> 24) & 0xFF) == 0xBB, "(2-2-2) opcode = 0xBB");
    TEST_ASSERT(((dw >> 21) & 0x7)  == 4,    "(2-2-2) mode cycles = 4");
    TEST_ASSERT(((dw >> 16) & 0x1F) == 8,    "(2-2-2) wait states = 8");
    TEST_ASSERT((dw & 0xFFFF)       == 0xFFFF, "Reserved bits 15:0 = all 1's");

    d.set_2_2_2_mode_clocks(15);
    TEST_ASSERT(((d.to_dword() >> 21) & 0x7) == 7, "(2-2-2) mode overflow masked (15 -> 7)");

    d.set_2_2_2_wait_states(255);
    TEST_ASSERT(((d.to_dword() >> 16) & 0x1F) == 31, "(2-2-2) wait overflow masked (255 -> 31)");
}

// ============================================================================
// A. DWORD 7 — Fast Read (4-4-4) Parameters
// ============================================================================

static void test_dword7()
{
    TEST_SECTION("A.DWORD7: Fast Read (4-4-4) Parameters");

    dword_7_t d;

    d.set_4_4_4_opcode(0xEB);
    d.set_4_4_4_mode_clocks(4);
    d.set_4_4_4_wait_states(8);
    uint32_t dw = d.to_dword();
    TEST_ASSERT(((dw >> 24) & 0xFF) == 0xEB, "(4-4-4) opcode = 0xEB");
    TEST_ASSERT(((dw >> 21) & 0x7)  == 4,    "(4-4-4) mode cycles = 4");
    TEST_ASSERT(((dw >> 16) & 0x1F) == 8,    "(4-4-4) wait states = 8");
    TEST_ASSERT((dw & 0xFFFF)       == 0xFFFF, "Reserved bits 15:0 = all 1's");

    d.set_4_4_4_mode_clocks(15);
    TEST_ASSERT(((d.to_dword() >> 21) & 0x7) == 7, "(4-4-4) mode overflow masked (15 -> 7)");

    d.set_4_4_4_wait_states(255);
    TEST_ASSERT(((d.to_dword() >> 16) & 0x1F) == 31, "(4-4-4) wait overflow masked (255 -> 31)");
}

// ============================================================================
// A. DWORD 8 — Erase Types 1 & 2
// ============================================================================

static void test_dword8()
{
    TEST_SECTION("A.DWORD8: Erase Types 1 & 2");

    dword_8_t d;

    // Type 1 (our model sets this to 2^16 = 64KB)
    d.set_type1_size_pow2(16);
    d.set_type1_opcode(spi_flash_opcodes::ERASE_64KB);
    TEST_ASSERT(d.get_erase_type1_size() == 16, "Type 1 size = 16 (64KB)");

    d.set_type1_size_pow2(12);
    TEST_ASSERT(d.get_erase_type1_size() == 12, "Type 1 size = 12 (4KB)");

    // Type 2
    d.set_type2_size_pow2(12);
    TEST_ASSERT(d.get_erase_type2_size() == 12, "Type 2 size = 12 (4KB)");

    d.set_type2_opcode(0x20);
    TEST_ASSERT(d.get_erase_type2_opcode() == 0x20, "Type 2 opcode = 0x20");

    // to_bytes round-trip
    uint32_t dw = d.to_dword();
    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 9 — Erase Types 3 & 4
// ============================================================================

static void test_dword9()
{
    TEST_SECTION("A.DWORD9: Erase Types 3 & 4");

    dword_9_t d;

    d.set_type3_size_pow2(0x10); d.set_type3_opcode(0xD8);
    d.set_type4_size_pow2(0x12); d.set_type4_opcode(0xDC);
    uint32_t dw = d.to_dword();

    TEST_ASSERT((dw & 0xFF)         == 0x10, "Type 3 size = 0x10 (64KB)");
    TEST_ASSERT(((dw >> 8) & 0xFF)  == 0xD8, "Type 3 opcode = 0xD8");
    TEST_ASSERT(((dw >> 16) & 0xFF) == 0x12, "Type 4 size = 0x12 (256KB)");
    TEST_ASSERT(((dw >> 24) & 0xFF) == 0xDC, "Type 4 opcode = 0xDC");

    // Size encoding: 0x0C = 2^12 = 4KB
    d.set_type3_size_pow2(0x0C);
    dw = d.to_dword();
    TEST_ASSERT((1u << (dw & 0xFF)) == 4096, "Type 3 size encoding: 0x0C = 4KB");

    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 10 — Sector Erase Timing
// ============================================================================

static void test_dword10()
{
    TEST_SECTION("A.DWORD10: Sector Erase Timing");

    dword_10_t d;

    d.set_multiplier(5);
    TEST_ASSERT((d.to_dword() & 0xF) == 5, "Multiplier = 5");

    d.set_timing(1, 0,  0); uint8_t c, u; d.get_erase_timing(1, c, u);
    TEST_ASSERT(c == 0 && u == 0, "Type 1 timing: count=0, unit=1ms");

    d.set_timing(1, 31, 3); d.get_erase_timing(1, c, u);
    TEST_ASSERT(c == 31 && u == 3, "Type 1 timing: count=31, unit=1s (max)");

    d.set_timing(2, 10, 1); d.get_erase_timing(2, c, u);
    TEST_ASSERT(c == 10 && u == 1, "Type 2 timing: count=10, unit=16ms");

    d.set_timing(3, 15, 2); d.get_erase_timing(3, c, u);
    TEST_ASSERT(c == 15 && u == 2, "Type 3 timing: count=15, unit=128ms");

    d.set_timing(4, 20, 3); d.get_erase_timing(4, c, u);
    TEST_ASSERT(c == 20 && u == 3, "Type 4 timing: count=20, unit=1s");

    uint32_t dw = d.to_dword();
    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 11 — Page Size & Program Timing
// ============================================================================

static void test_dword11()
{
    TEST_SECTION("A.DWORD11: Page Size & Program Timing");

    dword_11_t d;

    d.set_page_size_pow2(8);
    TEST_ASSERT(d.get_page_size_pow2() == 8, "Page size = 2^8 (256 bytes)");

    d.set_page_size_pow2(20);
    TEST_ASSERT(d.get_page_size_pow2() == 4, "Page size overflow masked (20 -> 4)");

    d.set_prog_multiplier(5);
    TEST_ASSERT((d.to_dword() & 0xF) == 5, "Program multiplier = 5");

    d.set_page_prog(31, true);
    TEST_ASSERT(((d.to_dword() >> 8) & 0x3F) == 0x3F, "Page program timing: max");

    d.set_chip_erase(31, 3);
    uint8_t chip_c, chip_u; d.get_chip_erase_time(chip_c, chip_u);
    TEST_ASSERT(chip_c == 31 && chip_u == 3, "Chip erase time: count=31, unit=64s (max)");

    uint32_t dw = d.to_dword();
    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 12 — Suspend / Resume Support
// ============================================================================

static void test_dword12()
{
    TEST_SECTION("A.DWORD12: Suspend/Resume Support");

    dword_12_t d;

    d.set_suspend_supported(true);
    TEST_ASSERT((d.to_dword() & (1u<<31)) == 0, "Suspend supported = true (bit 31 = 0)");

    d.set_suspend_supported(false);
    TEST_ASSERT((d.to_dword() & (1u<<31)) != 0, "Suspend supported = false (bit 31 = 1)");

    d.set_erase_suspend_max(31, 3);
    uint32_t dw = d.to_dword();
    uint8_t ec = (dw >> 24) & 0x1F;
    uint8_t eu = (dw >> 29) & 0x3;
    TEST_ASSERT(ec == 31 && eu == 3, "Erase suspend max: count=31, unit=64us (max)");

    d.set_prog_suspend_max(31, 3);
    dw = d.to_dword();
    uint8_t pc = (dw >> 13) & 0x1F;
    uint8_t pu = (dw >> 18) & 0x3;
    TEST_ASSERT(pc == 31 && pu == 3, "Program suspend max: count=31, unit=64us (max)");

    d.set_erase_suspend_prohibited(0x0A);
    d.set_prog_suspend_prohibited(0x05);
    dw = d.to_dword();
    TEST_ASSERT(((dw >> 4) & 0xF) == 0x0A, "Erase prohibited = 0x0A");
    TEST_ASSERT((dw & 0xF)        == 0x05, "Prog prohibited = 0x05");

    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 13 — Suspend / Resume Instructions
// ============================================================================

static void test_dword13()
{
    TEST_SECTION("A.DWORD13: Suspend/Resume Instructions");

    dword_13_t d;

    d.set_suspend_op(0x75);
    d.set_resume_op(0x7A);
    d.set_prog_suspend_op(0x85);
    d.set_prog_resume_op(0x8A);

    uint32_t dw = d.to_dword();
    TEST_ASSERT(((dw >> 24) & 0xFF) == 0x75, "Suspend op = 0x75");
    TEST_ASSERT(((dw >> 16) & 0xFF) == 0x7A, "Resume op = 0x7A");
    TEST_ASSERT(((dw >>  8) & 0xFF) == 0x85, "Prog suspend op = 0x85");
    TEST_ASSERT(( dw        & 0xFF) == 0x8A, "Prog resume op = 0x8A");

    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 14 — Deep Power Down
// ============================================================================

static void test_dword14()
{
    TEST_SECTION("A.DWORD14: Deep Powerdown Support");

    dword_14_t d;

    d.set_dpd_supported(true);
    TEST_ASSERT((d.to_dword() & (1u<<31)) == 0, "DPD supported = true (bit 31 = 0)");

    d.set_dpd_supported(false);
    TEST_ASSERT((d.to_dword() & (1u<<31)) != 0, "DPD supported = false (bit 31 = 1)");

    d.set_enter_dpd_op(0xB9);
    TEST_ASSERT(((d.to_dword() >> 23) & 0xFF) == 0xB9, "Enter DPD opcode = 0xB9");

    d.set_exit_dpd_op(0xAB);
    TEST_ASSERT(((d.to_dword() >> 15) & 0xFF) == 0xAB, "Exit DPD opcode = 0xAB");

    d.set_exit_dpd_delay(31, 3);
    uint32_t dw = d.to_dword();
    uint8_t dc = (dw >> 8) & 0x1F;
    uint8_t du = (dw >> 13) & 0x3;
    TEST_ASSERT(dc == 31 && du == 3, "Exit DPD delay: count=31, unit=64us (max)");

    d.set_poll_status_legacy(true);
    d.set_poll_status_flag(true);
    dw = d.to_dword();
    TEST_ASSERT((dw & (1<<2)) != 0 && (dw & (1<<3)) != 0, "Both status polling flags set");

    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 15 — Quad Enable & Advanced Features
// ============================================================================

static void test_dword15()
{
    TEST_SECTION("A.DWORD15: Quad Enable & Advanced Features");

    dword_15_t d;

    d.set_quad_enable_requirement(QER_NONE_OR_HOLD);
    TEST_ASSERT(d.get_quad_enable_requirement() == QER_NONE_OR_HOLD,  "QER = None or HOLD");

    d.set_quad_enable_requirement(QER_BIT6_SR1_REG);
    TEST_ASSERT(d.get_quad_enable_requirement() == QER_BIT6_SR1_REG, "QER = Bit 6 of SR1");

    d.set_quad_enable_requirement(QER_BIT1_SR2_OP35);
    TEST_ASSERT(d.get_quad_enable_requirement() == QER_BIT1_SR2_OP35, "QER = Bit 1 of SR2 (via 35h)");

    // Overflow: 10 & 0x7 = 2 = QER_BIT6_SR1_REG (raw bits, not enum — UBSan-safe)
    d.set_qer(10);
    TEST_ASSERT(d.get_quad_enable_requirement() == QER_BIT6_SR1_REG, "QER overflow masked (10 -> 2)");

    d.set_0_4_4_mode_support(true);
    TEST_ASSERT(d.get_0_4_4_mode_support() == true,  "0-4-4 mode support = true");
    d.set_0_4_4_mode_support(false);
    TEST_ASSERT(d.get_0_4_4_mode_support() == false, "0-4-4 mode support = false");

    d.set_hold_wp_disable(true);
    TEST_ASSERT((d.to_dword() & (1<<23)) != 0, "HOLD/WP disable = true");

    d.set_4_4_4_enable_seq(0x1F);
    TEST_ASSERT(d.get_4_4_4_enable_sequence() == 0x1F, "4-4-4 enable seq = 0x1F (max)");

    d.set_4_4_4_disable_seq(0x0F);
    TEST_ASSERT(d.get_4_4_4_disable_sequence() == 0x0F, "4-4-4 disable seq = 0x0F (max)");

    uint32_t dw = d.to_dword();
    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// A. DWORD 16 — 4-Byte Addressing & Soft Reset
// ============================================================================

static void test_dword16()
{
    TEST_SECTION("A.DWORD16: 4-Byte Addressing & Soft Reset");

    dword_16_t d;

    d.set_soft_reset_support(0x08);
    TEST_ASSERT((d.get_soft_reset_support() & 0x08) != 0, "Soft reset = 66h/99h");

    d.set_soft_reset_support(0x18);
    TEST_ASSERT(d.get_soft_reset_support() == 0x18, "Soft reset = both methods");

    d.set_4byte_addr_entry_method(0xAA);
    TEST_ASSERT(d.get_4byte_addr_entry_method() == 0xAA, "4-byte entry method = 0xAA");

    d.set_4byte_addr_exit_method(0x1FF);
    TEST_ASSERT(d.get_4byte_addr_exit_method() == 0x1FF, "4-byte exit method = 0x1FF (max)");

    d.set_status1_write_enable(0x15);
    TEST_ASSERT((d.to_dword() & 0x7F) == 0x15, "SR1 write enable methods = 0x15");

    d.set_status1_write_enable(0x7F);
    TEST_ASSERT((d.to_dword() & 0x7F) == 0x7F, "SR1 write enable methods = 0x7F (max)");

    uint32_t dw = d.to_dword();
    auto bytes = d.to_bytes();
    TEST_ASSERT(bytes.size() == 4, "to_bytes() returns 4 bytes");
    uint32_t recon = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                     (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    TEST_ASSERT(recon == dw, "to_bytes() round-trip matches");
}

// ============================================================================
// B. jedec_basic_table_t integration
// ============================================================================

static void test_jedec_table_integration()
{
    TEST_SECTION("B. jedec_basic_table_t integration");

    jedec_basic_table_t t;

    t.set_density(128u*1024*1024);
    TEST_ASSERT(t.get_density() == 128u*1024*1024, "Table density set/get");

    t.set_address_bytes(ADDR_3_OR_4_BYTE);
    TEST_ASSERT(t.get_address_bytes() == ADDR_3_OR_4_BYTE, "Table address mode set/get");

    // Direct DWORD access
    t.get_dword1().set_fast_read_1_1_4_support(true);
    TEST_ASSERT(t.get_dword1().get_fast_read_1_1_4_support() == true, "Direct DWORD1 access");

    t.get_dword2().set_density(256u*1024*1024);
    TEST_ASSERT(t.get_dword2().get_density() == 256u*1024*1024, "Direct DWORD2 access");

    // to_bytes() must return 16 × 4 = 64 bytes
    auto bytes = t.to_bytes();
    TEST_ASSERT(bytes.size() == 64, "to_bytes() returns 64 bytes (16 DWORDs)");

    // Verify first two DWORDs via to_bytes() round-trip
    uint32_t dw1 = uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                   (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
    uint32_t dw2 = uint32_t(bytes[4]) | (uint32_t(bytes[5])<<8) |
                   (uint32_t(bytes[6])<<16) | (uint32_t(bytes[7])<<24);
    TEST_ASSERT(dw1 == t.get_dword1().to_dword(), "to_bytes() DWORD1 matches");
    TEST_ASSERT(dw2 == t.get_dword2().to_dword(), "to_bytes() DWORD2 matches");
}

// ============================================================================
// B. sfdp_rom_t
// ============================================================================

static void test_sfdp_rom()
{
    TEST_SECTION("B. sfdp_rom_t");

    sfdp_header_t           hdr;
    sfdp_parameter_header_t phdr;
    jedec_basic_table_t     tbl;

    phdr.set_pointer(0x80);
    tbl.set_density(256u*1024*1024);

    sfdp_rom_t rom;
    rom.build_default(hdr, phdr, tbl);

    // Signature at offset 0..3 (little-endian "SFDP" = 0x50444653)
    uint32_t sig = uint32_t(rom.read_byte(0)) |
                   (uint32_t(rom.read_byte(1))<<8) |
                   (uint32_t(rom.read_byte(2))<<16) |
                   (uint32_t(rom.read_byte(3))<<24);
    TEST_ASSERT(sig == SFDP_SIGNATURE, "ROM signature = 'SFDP'");

    // Table pointer at SFDP offset 0x0C..0x0E (24-bit, stored in param header bytes 4..6)
    // param header is at byte offset 0x08; pointer is bytes 12..14 of the full SFDP header region
    uint32_t ptp = uint32_t(rom.read_byte(0x0C)) |
                   (uint32_t(rom.read_byte(0x0D))<<8) |
                   (uint32_t(rom.read_byte(0x0E))<<16);
    TEST_ASSERT(ptp == 0x80, "ROM: param table pointer = 0x80");

    // SFDP major rev (byte 5)
    TEST_ASSERT(rom.read_byte(5) == SFDP_MAJOR_REV, "ROM: SFDP major rev correct");

    // Density DWORD at table offset 0x80 + 4 bytes (DWORD 2)
    uint32_t density_dword = uint32_t(rom.read_byte(0x84)) |
                             (uint32_t(rom.read_byte(0x85))<<8) |
                             (uint32_t(rom.read_byte(0x86))<<16) |
                             (uint32_t(rom.read_byte(0x87))<<24);
    TEST_ASSERT(density_dword == tbl.get_dword2().to_dword(), "ROM: density DWORD matches");
}

// ============================================================================
// C. Utility functions
// ============================================================================

static void test_utility_functions()
{
    TEST_SECTION("C. Utility functions");

    TEST_ASSERT(addr_mode_to_string(ADDR_3_BYTE_ONLY)  == "3-Byte Only",        "addr_mode ADDR_3_BYTE_ONLY");
    TEST_ASSERT(addr_mode_to_string(ADDR_3_OR_4_BYTE)  == "3-Byte or 4-Byte",   "addr_mode ADDR_3_OR_4_BYTE");
    TEST_ASSERT(addr_mode_to_string(ADDR_4_BYTE_ONLY)  == "4-Byte Only",         "addr_mode ADDR_4_BYTE_ONLY");
    TEST_ASSERT(addr_mode_to_string(static_cast<uint8_t>(99)) == "Unknown", "addr_mode invalid = Unknown");

    TEST_ASSERT(qer_to_string(QER_NONE_OR_HOLD)    == "None or HOLD",             "qer QER_NONE_OR_HOLD");
    TEST_ASSERT(qer_to_string(QER_BIT1_SR2_REG)    == "Bit 1 of SR2",             "qer QER_BIT1_SR2_REG");
    TEST_ASSERT(qer_to_string(QER_BIT6_SR1_REG)    == "Bit 6 of SR1",             "qer QER_BIT6_SR1_REG");
    TEST_ASSERT(qer_to_string(QER_BIT7_SR2_OP3E)   == "Bit 7 of SR2 (via 3Eh)",   "qer QER_BIT7_SR2_OP3E");
    TEST_ASSERT(qer_to_string(QER_BIT1_SR2_NO_CLR) == "Bit 1 of SR2 (no clear)",  "qer QER_BIT1_SR2_NO_CLR");
    TEST_ASSERT(qer_to_string(QER_BIT1_SR2_OP35)   == "Bit 1 of SR2 (via 35h)",   "qer QER_BIT1_SR2_OP35");
    TEST_ASSERT(qer_to_string(static_cast<uint8_t>(99)) == "Unknown",           "qer invalid = Unknown");
}

// ============================================================================
// C. SFDP parser functions
// ============================================================================

static void test_parser_functions()
{
    TEST_SECTION("C. SFDP parser functions");

    // Build a valid SFDP byte array
    sfdp_header_t           hdr;
    sfdp_parameter_header_t phdr;
    jedec_basic_table_t     tbl;
    phdr.set_pointer(0x80);
    tbl.set_density(128u*1024*1024);
    tbl.set_address_bytes(ADDR_3_OR_4_BYTE);

    auto hb = hdr.to_bytes();
    auto pb = phdr.to_bytes();
    auto tb = tbl.to_bytes();

    std::vector<uint8_t> data(512, 0xFF);
    memcpy(data.data() + 0x00, hb.data(), hb.size());
    memcpy(data.data() + 0x08, pb.data(), pb.size());
    memcpy(data.data() + 0x80, tb.data(), tb.size());

    // Valid parse
    sfdp_header_t ph; sfdp_parameter_header_t pp; jedec_basic_table_t pt;
    bool ok = parse_sfdp_from_bytes(data, ph, pp, pt);
    TEST_ASSERT(ok == true, "parse_sfdp_from_bytes() valid data returns true");
    TEST_ASSERT(ph.signature == SFDP_SIGNATURE, "Parsed signature matches");
    TEST_ASSERT(pt.get_density() == 128u*1024*1024, "Parsed density matches");
    TEST_ASSERT(pt.get_address_bytes() == ADDR_3_OR_4_BYTE, "Parsed address mode matches");

    // Invalid signature
    std::vector<uint8_t> bad = data;
    bad[0] = bad[1] = bad[2] = bad[3] = 0;
    ok = parse_sfdp_from_bytes(bad, ph, pp, pt);
    TEST_ASSERT(ok == false, "parse_sfdp_from_bytes() invalid signature returns false");

    // Too short
    std::vector<uint8_t> tiny(10, 0xFF);
    ok = parse_sfdp_from_bytes(tiny, ph, pp, pt);
    TEST_ASSERT(ok == false, "parse_sfdp_from_bytes() too short returns false");

    // Truncated table
    std::vector<uint8_t> trunc = data;
    trunc.resize(0x80 + 32);  // only half the basic table
    ok = parse_sfdp_from_bytes(trunc, ph, pp, pt);
    TEST_ASSERT(ok == false, "parse_sfdp_from_bytes() truncated table returns false");

    // parse_sfdp_from_file() — write to /tmp then read back
    const std::string tmp_file = "/tmp/spi_flash_test_sfdp.bin";
    {
        std::ofstream f(tmp_file, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
    }
    ok = parse_sfdp_from_file(tmp_file, ph, pp, pt);
    TEST_ASSERT(ok == true, "parse_sfdp_from_file() valid file returns true");
    TEST_ASSERT(pt.get_density() == 128u*1024*1024, "File-parsed density matches");
    remove(tmp_file.c_str());

    // Non-existent file
    ok = parse_sfdp_from_file("/tmp/spi_flash_no_such_file.bin", ph, pp, pt);
    TEST_ASSERT(ok == false, "parse_sfdp_from_file() missing file returns false");
}

// ============================================================================
// C. End-to-end SFDP round-trip
// ============================================================================

static void test_sfdp_end_to_end()
{
    TEST_SECTION("C. SFDP end-to-end round-trip");

    jedec_basic_table_t orig;
    orig.set_density(256u*1024*1024);
    orig.set_address_bytes(ADDR_4_BYTE_ONLY);
    orig.get_dword1().set_fast_read_1_1_4_support(true);
    orig.get_dword3().set_1_1_4_opcode(0x6B);
    orig.get_dword3().set_1_1_4_wait_states(8);
    orig.get_dword8().set_type1_size_pow2(12);
    orig.get_dword8().set_type1_opcode(0x20);
    orig.get_dword11().set_page_size_pow2(8);
    orig.get_dword15().set_quad_enable_requirement(QER_BIT6_SR1_REG);

    auto bytes = orig.to_bytes();
    TEST_ASSERT(bytes.size() == 64, "Table serialises to 64 bytes");

    // Reconstruct via from_dword
    jedec_basic_table_t copy;
    for (size_t i = 0; i < 16; ++i) {
        uint32_t dw = uint32_t(bytes[i*4+0]) | (uint32_t(bytes[i*4+1])<<8) |
                      (uint32_t(bytes[i*4+2])<<16) | (uint32_t(bytes[i*4+3])<<24);
        switch (i) {
            case  0: copy.get_dword1().from_dword(dw);  break;
            case  1: copy.get_dword2().from_dword(dw);  break;
            case  2: copy.get_dword3().from_dword(dw);  break;
            case  3: copy.get_dword4().from_dword(dw);  break;
            case  4: copy.get_dword5().from_dword(dw);  break;
            case  5: copy.get_dword6().from_dword(dw);  break;
            case  6: copy.get_dword7().from_dword(dw);  break;
            case  7: copy.get_dword8().from_dword(dw);  break;
            case  8: copy.get_dword9().from_dword(dw);  break;
            case  9: copy.get_dword10().from_dword(dw); break;
            case 10: copy.get_dword11().from_dword(dw); break;
            case 11: copy.get_dword12().from_dword(dw); break;
            case 12: copy.get_dword13().from_dword(dw); break;
            case 13: copy.get_dword14().from_dword(dw); break;
            case 14: copy.get_dword15().from_dword(dw); break;
            case 15: copy.get_dword16().from_dword(dw); break;
        }
    }

    TEST_ASSERT(copy.get_density()       == orig.get_density(),       "Round-trip: density");
    TEST_ASSERT(copy.get_address_bytes() == orig.get_address_bytes(), "Round-trip: address mode");
    TEST_ASSERT(copy.get_dword1().get_fast_read_1_1_4_support() ==
                orig.get_dword1().get_fast_read_1_1_4_support(),      "Round-trip: FR 1-1-4 support");
    TEST_ASSERT(copy.get_dword3().get_1_1_4_opcode() == orig.get_dword3().get_1_1_4_opcode(), "Round-trip: opcode");
    TEST_ASSERT(copy.get_dword8().get_erase_type1_size() ==
                orig.get_dword8().get_erase_type1_size(),             "Round-trip: erase type1 size");
    TEST_ASSERT(copy.get_dword11().get_page_size_pow2() ==
                orig.get_dword11().get_page_size_pow2(),              "Round-trip: page size");
    TEST_ASSERT(copy.get_dword15().get_quad_enable_requirement() ==
                orig.get_dword15().get_quad_enable_requirement(),     "Round-trip: QER");
}

// ============================================================================
// D. Flash model — blank read
// ============================================================================

static void test_model_blank_read()
{
    TEST_SECTION("D. Model: blank flash reads 0xFF");

    spi_flash_model m(1u * 1024 * 1024);  // 1 MB for speed

    // Direct backdoor
    TEST_ASSERT(m.read_byte(0x0000) == 0xFF, "Byte 0x0000 = 0xFF (blank)");
    TEST_ASSERT(m.read_byte(0xFFFF) == 0xFF, "Byte 0xFFFF = 0xFF (blank)");
    TEST_ASSERT(m.read_byte(0xFFFFF) == 0xFF, "Last byte = 0xFF (blank)");
    TEST_ASSERT(m.read_byte(0x100000) == 0xFF, "OOB byte = 0xFF");

    // Via READ command
    std::vector<uint8_t> buf(4, 0x00);
    m.process_command(spi_flash_opcodes::READ, 0x1000, buf);
    bool all_ff = true;
    for (auto b : buf) if (b != 0xFF) { all_ff = false; break; }
    TEST_ASSERT(all_ff, "READ @ 0x1000: all bytes = 0xFF");
}

// ============================================================================
// D. Flash model — WREN / WRDI / READ_STATUS
// ============================================================================

static void test_model_wren_wrdi()
{
    TEST_SECTION("D. Model: WREN / WRDI / READ_STATUS");

    spi_flash_model m(1u * 1024 * 1024);

    std::vector<uint8_t> sr(1, 0);

    // Initial state: WEL = 0
    m.process_command(spi_flash_opcodes::READ_STATUS, 0, sr);
    TEST_ASSERT((sr[0] & 0x02) == 0, "Initial SR1 WEL = 0");

    // WREN sets WEL
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, sr);
    m.process_command(spi_flash_opcodes::READ_STATUS,  0, sr);
    TEST_ASSERT((sr[0] & 0x02) != 0, "After WREN: SR1 WEL = 1");

    // WRDI clears WEL
    m.process_command(spi_flash_opcodes::WRITE_DISABLE, 0, sr);
    m.process_command(spi_flash_opcodes::READ_STATUS,   0, sr);
    TEST_ASSERT((sr[0] & 0x02) == 0, "After WRDI: SR1 WEL = 0");
}

// ============================================================================
// D. Flash model — Program without WREN fails
// ============================================================================

static void test_model_program_no_wren()
{
    TEST_SECTION("D. Model: PROGRAM without WREN fails");

    spi_flash_model m(1u * 1024 * 1024);

    std::vector<uint8_t> tx = {0xAA, 0xBB};
    std::vector<uint8_t> rx;
    bool ok = m.process_command(spi_flash_opcodes::PROGRAM, 0x1000, rx, tx);
    TEST_ASSERT(ok == false, "PROGRAM without WREN returns false");

    // Memory must be unchanged
    TEST_ASSERT(m.read_byte(0x1000) == 0xFF, "Memory unchanged after failed program");
}

// ============================================================================
// D. Flash model — Program AND semantics
// ============================================================================

static void test_model_program_and_semantics()
{
    TEST_SECTION("D. Model: PROGRAM AND semantics");

    spi_flash_model m(1u * 1024 * 1024);

    std::vector<uint8_t> tx = {0xAA, 0x55, 0x0F};
    std::vector<uint8_t> rx;

    // WREN → PROGRAM
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    bool ok = m.process_command(spi_flash_opcodes::PROGRAM, 0x2000, rx, tx);
    TEST_ASSERT(ok == true, "PROGRAM with WREN returns true");

    // AND semantics: 0xFF & 0xAA = 0xAA
    TEST_ASSERT(m.read_byte(0x2000) == 0xAA, "Program: 0xFF & 0xAA = 0xAA");
    TEST_ASSERT(m.read_byte(0x2001) == 0x55, "Program: 0xFF & 0x55 = 0x55");
    TEST_ASSERT(m.read_byte(0x2002) == 0x0F, "Program: 0xFF & 0x0F = 0x0F");
    TEST_ASSERT(m.read_byte(0x2003) == 0xFF, "Byte beyond program untouched = 0xFF");

    // WREN auto-cleared after program
    std::vector<uint8_t> sr(1, 0);
    m.process_command(spi_flash_opcodes::READ_STATUS, 0, sr);
    TEST_ASSERT((sr[0] & 0x02) == 0, "WEL auto-cleared after PROGRAM");

    // Second program: 0xAA & 0x0F = 0x0A  (bits can only go 1→0)
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    std::vector<uint8_t> tx2 = {0x0F};
    m.process_command(spi_flash_opcodes::PROGRAM, 0x2000, rx, tx2);
    TEST_ASSERT(m.read_byte(0x2000) == 0x0A, "Second program: 0xAA & 0x0F = 0x0A");

    // Cannot set bits back (0x0A & 0xFF = 0x0A)
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    std::vector<uint8_t> tx3 = {0xFF};
    m.process_command(spi_flash_opcodes::PROGRAM, 0x2000, rx, tx3);
    TEST_ASSERT(m.read_byte(0x2000) == 0x0A, "AND semantics: cannot set bits back to 1");
}

// ============================================================================
// D. Flash model — Erase without WREN fails
// ============================================================================

static void test_model_erase_no_wren()
{
    TEST_SECTION("D. Model: ERASE_64KB without WREN fails");

    spi_flash_model m(1u * 1024 * 1024);

    // Write a byte via backdoor
    m.write_byte(0x5000, 0x42);

    std::vector<uint8_t> rx;
    bool ok = m.process_command(spi_flash_opcodes::ERASE_64KB, 0x5000, rx);
    TEST_ASSERT(ok == false, "ERASE_64KB without WREN returns false");
    TEST_ASSERT(m.read_byte(0x5000) == 0x42, "Memory unchanged after failed erase");
}

// ============================================================================
// D. Flash model — Erase 64KB block
// ============================================================================

static void test_model_erase_64kb()
{
    TEST_SECTION("D. Model: ERASE_64KB");

    spi_flash_model m(2u * 1024 * 1024);  // 2 MB

    // Pre-fill two 64KB blocks via backdoor
    for (uint32_t i = 0; i < 0x10000; ++i) m.write_byte(0x00000 + i, 0xAA);
    for (uint32_t i = 0; i < 0x10000; ++i) m.write_byte(0x10000 + i, 0xBB);

    std::vector<uint8_t> rx;

    // Erase block 0 (address 0x0000, aligned to 64KB boundary)
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    bool ok = m.process_command(spi_flash_opcodes::ERASE_64KB, 0x0000, rx);
    TEST_ASSERT(ok == true, "ERASE_64KB with WREN returns true");

    // Block 0 must be 0xFF
    bool erased = true;
    for (uint32_t i = 0; i < 0x10000 && erased; ++i)
        if (m.read_byte(i) != 0xFF) erased = false;
    TEST_ASSERT(erased, "Block 0 fully erased to 0xFF");

    // Block 1 (0x10000) must be untouched
    TEST_ASSERT(m.read_byte(0x10000) == 0xBB, "Block 1 untouched after block 0 erase");
    TEST_ASSERT(m.read_byte(0x1FFFF) == 0xBB, "Block 1 last byte untouched");

    // WEL auto-cleared
    std::vector<uint8_t> sr(1, 0);
    m.process_command(spi_flash_opcodes::READ_STATUS, 0, sr);
    TEST_ASSERT((sr[0] & 0x02) == 0, "WEL auto-cleared after ERASE");

    // Erase block 1 (mid-block address — must align to 64KB boundary)
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    m.process_command(spi_flash_opcodes::ERASE_64KB, 0x15000, rx); // mid-block
    TEST_ASSERT(m.read_byte(0x10000) == 0xFF, "Block 1 erased via mid-block address");
}

// ============================================================================
// D. Flash model — READ_SFDP round-trip
// ============================================================================

static void test_model_read_sfdp()
{
    TEST_SECTION("D. Model: READ_SFDP round-trip");

    spi_flash_model m(32u * 1024 * 1024);

    // Read 192 bytes: header(8) + param header(8) + padding to 0x80 + table(64) = 0x80+64=192
    std::vector<uint8_t> raw(192, 0xFF);
    bool ok = m.process_command(spi_flash_opcodes::READ_SFDP, 0x00, raw);
    TEST_ASSERT(ok == true, "READ_SFDP returns true");

    // Signature
    uint32_t sig = uint32_t(raw[0]) | (uint32_t(raw[1])<<8) |
                   (uint32_t(raw[2])<<16) | (uint32_t(raw[3])<<24);
    TEST_ASSERT(sig == SFDP_SIGNATURE, "READ_SFDP: signature = 'SFDP'");

    // Parse back and verify key fields
    sfdp_header_t ph; sfdp_parameter_header_t pp; jedec_basic_table_t pt;
    ok = parse_sfdp_from_bytes(raw, ph, pp, pt);
    TEST_ASSERT(ok == true, "parse_sfdp_from_bytes() on READ_SFDP data succeeds");

    // Density must match model size: 32MB = 256Mbit
    TEST_ASSERT(pt.get_density() == 256u*1024*1024, "SFDP density = 256 Mbit (32 MB)");

    // Address mode: 32MB ≤ 16MB is false → ADDR_3_OR_4_BYTE
    TEST_ASSERT(pt.get_address_bytes() == ADDR_3_OR_4_BYTE, "SFDP address mode = 3-or-4-byte");

    // Erase type 1: size = 2^16 = 64KB, opcode = 0xD8
    uint8_t e1_size, e1_opcode;
    pt.get_sector_erase_type1(e1_size, e1_opcode);
    TEST_ASSERT(e1_size == 16,
                "SFDP erase type1 size = 16 (64KB)");
    TEST_ASSERT(e1_opcode == spi_flash_opcodes::ERASE_64KB,
                "SFDP erase type1 opcode = 0xD8");

    // Page size: 2^8 = 256 bytes
    TEST_ASSERT(pt.get_dword11().get_page_size_pow2() == 8, "SFDP page size = 256 bytes");

    // Print the tree for visual inspection
    std::cout << "\n--- SFDP tree (from READ_SFDP) ---\n";
    print_sfdp_tree(ph, pp, pt);
}

// ============================================================================
// D. Flash model — Suspend / Resume
// ============================================================================

static void test_model_suspend_resume()
{
    TEST_SECTION("D. Model: Suspend / Resume");

    spi_flash_model m(1u * 1024 * 1024);
    std::vector<uint8_t> rx;
    std::vector<uint8_t> tx = {0xAA};

    // Suspend (0x75)
    m.process_command(spi_flash_opcodes::SUSPEND_75, 0, rx);

    // Program must fail when suspended
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    bool ok = m.process_command(spi_flash_opcodes::PROGRAM, 0x0000, rx, tx);
    TEST_ASSERT(ok == false, "PROGRAM blocked while suspended");

    // Erase must also fail when suspended
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    ok = m.process_command(spi_flash_opcodes::ERASE_64KB, 0x0000, rx);
    TEST_ASSERT(ok == false, "ERASE blocked while suspended");

    // Resume (0x7A)
    m.process_command(spi_flash_opcodes::RESUME_7A, 0, rx);

    // Now program should work
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    ok = m.process_command(spi_flash_opcodes::PROGRAM, 0x0000, rx, tx);
    TEST_ASSERT(ok == true, "PROGRAM succeeds after Resume");
    TEST_ASSERT(m.read_byte(0x0000) == 0xAA, "Programmed byte = 0xAA after Resume");

    // Suspend (0xB0) — alternate opcode
    m.process_command(spi_flash_opcodes::SUSPEND_B0, 0, rx);
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    ok = m.process_command(spi_flash_opcodes::PROGRAM, 0x0001, rx, tx);
    TEST_ASSERT(ok == false, "PROGRAM blocked by SUSPEND_B0");

    // Resume (0xD0) — alternate opcode
    m.process_command(spi_flash_opcodes::RESUME_D0, 0, rx);
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    ok = m.process_command(spi_flash_opcodes::PROGRAM, 0x0001, rx, tx);
    TEST_ASSERT(ok == true, "PROGRAM succeeds after RESUME_D0");
}

// ============================================================================
// D. Flash model — Reset
// ============================================================================

static void test_model_reset()
{
    TEST_SECTION("D. Model: reset()");

    spi_flash_model m(1u * 1024 * 1024);
    std::vector<uint8_t> rx;

    // Write known data via backdoor
    m.write_byte(0x100, 0xAB);

    // Set WEL and suspend
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    m.process_command(spi_flash_opcodes::SUSPEND_75, 0, rx);

    m.reset();

    // WEL must be cleared
    std::vector<uint8_t> sr(1, 0);
    m.process_command(spi_flash_opcodes::READ_STATUS, 0, sr);
    TEST_ASSERT((sr[0] & 0x02) == 0, "After reset: WEL = 0");

    // Memory must be preserved
    TEST_ASSERT(m.read_byte(0x100) == 0xAB, "After reset: memory preserved");

    // Suspend must be cleared — program should work now
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    std::vector<uint8_t> tx = {0x55};
    bool ok = m.process_command(spi_flash_opcodes::PROGRAM, 0x200, rx, tx);
    TEST_ASSERT(ok == true, "After reset: suspend cleared, PROGRAM works");
}

// ============================================================================
// D. Flash model — 4-byte address commands
// ============================================================================

static void test_model_4byte_address()
{
    TEST_SECTION("D. Model: 4-byte address commands");

    // 32 MB to exercise addresses > 16MB
    spi_flash_model m(32u * 1024 * 1024);
    std::vector<uint8_t> rx;

    const uint32_t ADDR = 0x01000000u;  // 16 MB mark

    // 4-byte PROGRAM
    std::vector<uint8_t> tx = {0xCA, 0xFE};
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    bool ok = m.process_command(spi_flash_opcodes::PROGRAM_4BYTE, ADDR, rx, tx);
    TEST_ASSERT(ok == true, "PROGRAM_4BYTE returns true");
    TEST_ASSERT(m.read_byte(ADDR)   == 0xCA, "PROGRAM_4BYTE: byte 0 = 0xCA");
    TEST_ASSERT(m.read_byte(ADDR+1) == 0xFE, "PROGRAM_4BYTE: byte 1 = 0xFE");

    // 4-byte ERASE
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    ok = m.process_command(spi_flash_opcodes::ERASE_64KB_4B, ADDR, rx);
    TEST_ASSERT(ok == true, "ERASE_64KB_4B returns true");

    bool erased = true;
    for (uint32_t i = 0; i < 0x10000 && erased; ++i)
        if (m.read_byte(ADDR + i) != 0xFF) erased = false;
    TEST_ASSERT(erased, "4-byte erase: block fully erased to 0xFF");

    // EN4B mode toggle tests
    m.process_command(spi_flash_opcodes::EN4B, 0, rx);
    TEST_ASSERT(m.is_4byte_address_mode() == true, "EN4B activates 4-byte address mode");

    m.process_command(spi_flash_opcodes::EX4B, 0, rx);
    TEST_ASSERT(m.is_4byte_address_mode() == false, "EX4B deactivates 4-byte address mode");
}

// ============================================================================
// D. Flash model — Software Reset Sequence
// ============================================================================

static void test_model_software_reset()
{
    TEST_SECTION("D. Model: software reset (66h/99h)");

    spi_flash_model m(1u * 1024 * 1024);
    std::vector<uint8_t> rx;

    // 1. Arm reset
    m.process_command(spi_flash_opcodes::RESET_ENABLE, 0, rx);

    // Write enable to check if reset clears it
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);

    // 2. Execute reset
    m.process_command(spi_flash_opcodes::RESET_EXECUTE, 0, rx);

    // Check WEL cleared
    std::vector<uint8_t> sr(1, 0);
    m.process_command(spi_flash_opcodes::READ_STATUS, 0, sr);
    TEST_ASSERT((sr[0] & 0x02) == 0, "Software reset clears WEL");

    // Unarmed reset should be ignored
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    m.process_command(spi_flash_opcodes::RESET_EXECUTE, 0, rx); // Ignored
    m.process_command(spi_flash_opcodes::READ_STATUS, 0, sr);
    TEST_ASSERT((sr[0] & 0x02) != 0, "Unarmed RESET_EXECUTE is ignored");
}

// ============================================================================
// D. Flash model — Missing Erase Granularities
// ============================================================================

static void test_model_missing_erase_granularities()
{
    TEST_SECTION("D. Model: missing erase granularities (4KB, 32KB, Chip)");

    spi_flash_model m(1u * 1024 * 1024);
    std::vector<uint8_t> rx;

    // Pre-fill memory
    for (uint32_t i = 0; i < 0x20000; ++i) m.write_byte(i, 0xAA);

    // Erase 4KB
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    m.process_command(spi_flash_opcodes::ERASE_4KB, 0x1000, rx); // 0x1000 - 0x1FFF
    TEST_ASSERT(m.read_byte(0x0FFF) == 0xAA, "4KB erase bounds (low end)");
    TEST_ASSERT(m.read_byte(0x1000) == 0xFF, "4KB erased internally");
    TEST_ASSERT(m.read_byte(0x1FFF) == 0xFF, "4KB erased internally (high end)");
    TEST_ASSERT(m.read_byte(0x2000) == 0xAA, "4KB erase bounds (high end)");

    // Erase 32KB
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    m.process_command(spi_flash_opcodes::ERASE_32KB, 0x8000, rx); // 0x8000 - 0xFFFF
    TEST_ASSERT(m.read_byte(0x7FFF) == 0xAA, "32KB erase bounds (low end)");
    TEST_ASSERT(m.read_byte(0x8000) == 0xFF, "32KB erased internally");
    TEST_ASSERT(m.read_byte(0xFFFF) == 0xFF, "32KB erased internally (high end)");
    TEST_ASSERT(m.read_byte(0x10000) == 0xAA, "32KB erase bounds (high end)");

    // Chip Erase
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    m.process_command(spi_flash_opcodes::CHIP_ERASE, 0, rx);
    TEST_ASSERT(m.read_byte(0x00000) == 0xFF, "Chip erase works");
    TEST_ASSERT(m.read_byte(0x1FFFF) == 0xFF, "Chip erase works everywhere");
}

// ============================================================================
// D. Flash model — backdoor read_byte / write_byte
// ============================================================================

static void test_model_backdoor_rw()
{
    TEST_SECTION("D. Model: backdoor read_byte / write_byte");

    spi_flash_model m(1u * 1024 * 1024);

    // write_byte bypasses WEL
    m.write_byte(0x0000, 0x12);
    m.write_byte(0x0001, 0x34);
    m.write_byte(0x0002, 0x56);

    TEST_ASSERT(m.read_byte(0x0000) == 0x12, "Backdoor write_byte: 0x0000 = 0x12");
    TEST_ASSERT(m.read_byte(0x0001) == 0x34, "Backdoor write_byte: 0x0001 = 0x34");
    TEST_ASSERT(m.read_byte(0x0002) == 0x56, "Backdoor write_byte: 0x0002 = 0x56");

    // write_byte allows setting bits back to 1 (bypasses flash semantics)
    m.write_byte(0x0000, 0xFF);
    TEST_ASSERT(m.read_byte(0x0000) == 0xFF, "Backdoor: can set bits back to 1");

    // OOB access is silent
    m.write_byte(m.size(), 0xAB);
    TEST_ASSERT(m.read_byte(m.size()) == 0xFF, "Backdoor: OOB write ignored, OOB read = 0xFF");

    // Read via READ command is consistent with backdoor write
    std::vector<uint8_t> buf(3, 0x00);
    m.write_byte(0x1000, 0xDE); m.write_byte(0x1001, 0xAD); m.write_byte(0x1002, 0xBE);
    m.process_command(spi_flash_opcodes::READ, 0x1000, buf);
    TEST_ASSERT(buf[0] == 0xDE && buf[1] == 0xAD && buf[2] == 0xBE,
                "READ consistent with backdoor write");
}

// ============================================================================
// D. Flash model — constructor SFDP auto-configuration
// ============================================================================

static void test_model_constructor_sfdp()
{
    TEST_SECTION("D. Model: constructor SFDP auto-config");

    // 8 MB — density = 64 Mbit, address mode = ADDR_3_BYTE_ONLY (≤ 16MB)
    {
        spi_flash_model m8(8u * 1024 * 1024);
        const jedec_basic_table_t* t = m8.get_basic_table();
        TEST_ASSERT(t->get_density() == uint64_t(8u*1024*1024)*8u,
                    "8MB model: density = 64 Mbit");
        TEST_ASSERT(t->get_address_bytes() == ADDR_3_BYTE_ONLY,
                    "8MB model: address mode = 3-byte only");
    }

    // 32 MB — density = 256 Mbit, address mode = ADDR_3_OR_4_BYTE (> 16MB)
    {
        spi_flash_model m32(32u * 1024 * 1024);
        const jedec_basic_table_t* t = m32.get_basic_table();
        TEST_ASSERT(t->get_density() == uint64_t(32u*1024*1024)*8u,
                    "32MB model: density = 256 Mbit");
        TEST_ASSERT(t->get_address_bytes() == ADDR_3_OR_4_BYTE,
                    "32MB model: address mode = 3-or-4-byte");
        TEST_ASSERT(t->get_dword11().get_page_size_pow2() == 8,
                    "32MB model: page size = 256 bytes");
        TEST_ASSERT(t->get_dword8().get_erase_type1_size() == 16,
                    "32MB model: erase type1 = 64KB");
    }
}

// ============================================================================
// D. Flash model — backdoor load/save file I/O
// ============================================================================
static void test_model_backdoor_file_io()
{
    TEST_SECTION("D. Model: backdoor load/save file I/O");

    // Clean up first in case leftover exists
    std::remove(spi_flash_model::BACKDOOR_FILE_PATH);

    // Test 1: load from non-existent file
    {
        spi_flash_model m(256);
        bool ok = m.load_memory_from_file(spi_flash_model::BACKDOOR_FILE_PATH);
        TEST_ASSERT(ok == false, "load_memory_from_file returns false for missing file");
    }

    // Test 2: save to file and load it back
    {
        spi_flash_model m(256);
        m.write_byte(0, 0x11);
        m.write_byte(1, 0x22);
        m.write_byte(255, 0xAA);
        
        bool ok = m.save_memory_to_file(spi_flash_model::BACKDOOR_FILE_PATH);
        TEST_ASSERT(ok == true, "save_memory_to_file returns true on success");

        // Verify file was written
        std::ifstream f(spi_flash_model::BACKDOOR_FILE_PATH, std::ios::binary);
        TEST_ASSERT(f.is_open(), "backdoor bin file exists");
        f.close();

        // Load into another model
        spi_flash_model m2(256);
        TEST_ASSERT(m2.read_byte(0) == 0xFF, "new model memory initially blank");
        
        ok = m2.load_memory_from_file(spi_flash_model::BACKDOOR_FILE_PATH);
        TEST_ASSERT(ok == true, "load_memory_from_file returns true for existing file");
        TEST_ASSERT(m2.read_byte(0) == 0x11, "loaded byte 0 matches");
        TEST_ASSERT(m2.read_byte(1) == 0x22, "loaded byte 1 matches");
        TEST_ASSERT(m2.read_byte(255) == 0xAA, "loaded byte 255 matches");
    }

    // Test 3: empty file handling
    {
        std::ofstream f(spi_flash_model::BACKDOOR_FILE_PATH, std::ios::binary | std::ios::trunc);
        f.close();

        spi_flash_model m(256);
        bool ok = m.load_memory_from_file(spi_flash_model::BACKDOOR_FILE_PATH);
        TEST_ASSERT(ok == false, "load_memory_from_file returns false for empty file");
    }

    // Clean up
    std::remove(spi_flash_model::BACKDOOR_FILE_PATH);
}

// ============================================================================
// D. Flash model — update_sfdp_rom
// ============================================================================
static void test_model_update_sfdp_rom()
{
    TEST_SECTION("D. Model: update_sfdp_rom");

    spi_flash_model m(8u * 1024 * 1024);
    
    // Get density before change
    std::vector<uint8_t> raw1(192, 0x00);
    m.process_command(spi_flash_opcodes::READ_SFDP, 0x00, raw1);
    
    // Modify basic table
    jedec_basic_table_t* tbl = m.get_basic_table();
    tbl->set_density(16u * 1024 * 1024 * 8u); // Change from 8MB to 16MB density
    
    m.update_sfdp_rom();

    std::vector<uint8_t> raw2(192, 0x00);
    m.process_command(spi_flash_opcodes::READ_SFDP, 0x00, raw2);

    sfdp_header_t ph; sfdp_parameter_header_t pp; jedec_basic_table_t pt;
    bool ok = parse_sfdp_from_bytes(raw2, ph, pp, pt);
    TEST_ASSERT(ok == true, "parse updated SFDP succeeds");
    TEST_ASSERT(pt.get_density() == 16u * 1024 * 1024 * 8u, "SFDP density updated successfully");
}

// ============================================================================
// D. Flash model — Chip Erase Guards
// ============================================================================
static void test_model_chip_erase_guards()
{
    TEST_SECTION("D. Model: chip erase guards");

    spi_flash_model m(256);
    m.write_byte(0, 0x55);

    // Test 1: chip erase without WEL fails
    std::vector<uint8_t> rx;
    bool ok = m.process_command(spi_flash_opcodes::CHIP_ERASE, 0, rx);
    TEST_ASSERT(ok == true, "CHIP_ERASE returns true");
    TEST_ASSERT(m.read_byte(0) == 0x55, "Memory not erased without WEL");

    // Test 2: chip erase while suspended fails
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    m.process_command(spi_flash_opcodes::SUSPEND_75, 0, rx);
    m.process_command(spi_flash_opcodes::CHIP_ERASE, 0, rx);
    TEST_ASSERT(m.read_byte(0) == 0x55, "Memory not erased when suspended");

    // Test 3: resume then chip erase works
    m.process_command(spi_flash_opcodes::RESUME_7A, 0, rx);
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);
    m.process_command(spi_flash_opcodes::CHIP_ERASE, 0, rx);
    TEST_ASSERT(m.read_byte(0) == 0xFF, "Memory erased after resume + WREN");
}

// ============================================================================
// D. Flash model — Large Density (>2Gbit)
// ============================================================================
static void test_model_large_density()
{
    TEST_SECTION("D. Model: large density (>2Gbit) SFDP encoding");

    dword_2_t d;
    // Set density to 4 Gbits (4 * 1024 * 1024 * 1024 bits = 0x100000000ULL)
    uint64_t bits = 4ULL * 1024 * 1024 * 1024;
    d.set_density(bits);
    TEST_ASSERT(d.get_density() == bits, "Large density correctly round-tripped");
}

// ============================================================================
// B. SFDP ROM edge cases
// ============================================================================
static void test_sfdp_rom_edge_cases()
{
    TEST_SECTION("B. SFDP ROM edge cases");

    sfdp_rom_t rom;
    
    // Test 1: load beyond 512 bytes triggers resize
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
    rom.load(600, data);
    TEST_ASSERT(rom.read_byte(600) == 0xAA, "ROM resize and read byte 600");
    TEST_ASSERT(rom.read_byte(601) == 0xBB, "ROM resize and read byte 601");
    TEST_ASSERT(rom.read_byte(602) == 0xCC, "ROM resize and read byte 602");

    // Test 2: OOB read returns 0xFF
    TEST_ASSERT(rom.read_byte(9999) == 0xFF, "ROM OOB read returns 0xFF");
}

// ============================================================================
// D. Flash model — unknown/invalid opcode
// ============================================================================
static void test_model_unknown_opcode()
{
    TEST_SECTION("D. Model: unknown opcode handling");

    spi_flash_model m(256);
    std::vector<uint8_t> rx;
    bool ok = m.process_command(0xAA, 0, rx);
    TEST_ASSERT(ok == false, "Unknown opcode 0xAA returns false");
}

// ============================================================================
// D. Flash model — JEDEC ID (0x9F) and Status Register 2 (0x35)
// ============================================================================
static void test_model_jedec_id()
{
    TEST_SECTION("D. Model: JEDEC ID (0x9F) and READ_STATUS_2 (0x35)");

    spi_flash_model m;

    // Default ID matches the DV BFM: manufacturer 0x20, type 0xBA, capacity 0x18.
    std::vector<uint8_t> rx(3, 0x00);
    bool ok = m.process_command(spi_flash_opcodes::READ_JEDEC_ID, 0, rx);
    TEST_ASSERT(ok, "RDID returns success");
    TEST_ASSERT(rx[0] == 0x20 && rx[1] == 0xBA && rx[2] == 0x18,
                "RDID streams 0x20, 0xBA, 0x18 in that order");

    // Capacity byte 0x18 means 2^24 bytes, which must be what the array holds.
    TEST_ASSERT(m.size() == (1u << 24), "Default array size matches advertised density");

    // A test may override the ID to model a different part.
    m.set_jedec_id(0xEF4018);
    std::vector<uint8_t> rx2(3, 0x00);
    m.process_command(spi_flash_opcodes::READ_JEDEC_ID, 0, rx2);
    TEST_ASSERT(rx2[0] == 0xEF && rx2[1] == 0x40 && rx2[2] == 0x18, "RDID honours overridden ID");

    std::vector<uint8_t> sr2(1, 0xAA);
    ok = m.process_command(spi_flash_opcodes::READ_STATUS_2, 0, sr2);
    TEST_ASSERT(ok && sr2[0] == 0x00, "RDSR2 returns SR2 (0x00 at reset)");
}

// ============================================================================
// D. Flash model — page program wraps at the page boundary
// ============================================================================
static void test_model_page_program_wrap()
{
    TEST_SECTION("D. Model: page program wraps within its page");

    spi_flash_model m(64 * 1024);

    // Start 4 bytes before the end of page 0 and program 8 bytes. The first 4 land
    // at 0xFC..0xFF; the rest wrap to 0x00..0x03 instead of spilling into page 1.
    std::vector<uint8_t> rx;
    m.process_command(spi_flash_opcodes::WRITE_ENABLE, 0, rx);

    const std::vector<uint8_t> data = {0xA0, 0xA1, 0xA2, 0xA3, 0xB0, 0xB1, 0xB2, 0xB3};
    bool ok = m.process_command(spi_flash_opcodes::PROGRAM, 0x0FC, rx, data);
    TEST_ASSERT(ok, "Page program at 0x0FC succeeds");

    TEST_ASSERT(m.read_byte(0x0FC) == 0xA0 && m.read_byte(0x0FF) == 0xA3,
                "Bytes before the page boundary land in place");
    TEST_ASSERT(m.read_byte(0x000) == 0xB0 && m.read_byte(0x003) == 0xB3,
                "Bytes past the boundary wrap to the start of the same page");
    TEST_ASSERT(m.read_byte(0x100) == 0xFF && m.read_byte(0x103) == 0xFF,
                "Next page is left erased");
}

// ============================================================================
// MAIN
// ============================================================================

int main()
{
    std::cout << "========================================\n";
    std::cout << "SPI Flash Model Unit Tests\n";
    std::cout << "========================================\n";

    // --- A: DWORD structure tests ---
    test_dword1();
    test_dword2();
    test_dword3();
    test_dword4();
    test_dword5();
    test_dword6();
    test_dword7();
    test_dword8();
    test_dword9();
    test_dword10();
    test_dword11();
    test_dword12();
    test_dword13();
    test_dword14();
    test_dword15();
    test_dword16();

    // --- B: Table / ROM ---
    test_jedec_table_integration();
    test_sfdp_rom();
    test_sfdp_rom_edge_cases();

    // --- C: Utilities + parser ---
    test_utility_functions();
    test_parser_functions();
    test_sfdp_end_to_end();

    // --- D: Flash model behaviour ---
    test_model_blank_read();
    test_model_wren_wrdi();
    test_model_program_no_wren();
    test_model_program_and_semantics();
    test_model_erase_no_wren();
    test_model_erase_64kb();
    test_model_read_sfdp();
    test_model_suspend_resume();
    test_model_reset();
    test_model_4byte_address();
    test_model_software_reset();
    test_model_missing_erase_granularities();
    test_model_backdoor_rw();
    test_model_constructor_sfdp();
    test_model_backdoor_file_io();
    test_model_update_sfdp_rom();
    test_model_chip_erase_guards();
    test_model_large_density();
    test_model_unknown_opcode();
    test_model_jedec_id();
    test_model_page_program_wrap();

    // --- Summary ---
    std::cout << "\n========================================\n";
    std::cout << "Results: " << s_tests_passed << "/" << s_tests_run << " passed";
    if (s_tests_failed > 0)
        std::cout << "  (" << s_tests_failed << " FAILED)";
    std::cout << "\n========================================\n";

    return (s_tests_failed > 0) ? 1 : 0;
}
