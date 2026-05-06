#pragma once

/**
 * @file spi_flash_sfdp.h
 * @brief JEDEC Serial Flash Discoverable Parameters (SFDP) structures
 *
 * Pure C++ implementation of the JESD216A SFDP standard (July 2013).
 * Defines all 16 DWORDs of the Basic Flash Parameter Table, the SFDP
 * header, parameter header, and the ROM container used by the SPI flash model.
 *
 * No SystemC dependencies — this file compiles standalone.
 *
 * Reference: JEDEC Standard JESD216A, "Serial Flash Discoverable Parameters"
 * Ported from: knowledge_base/code/xspi_target/SystemC/include/sfdp.h
 */


#include <cstdint>
#include <vector>
#include <cstring>
#include <string>

// ============================================================================
// SPI FLASH OPCODES (Profile 1 / JEDEC standard command set)
// ============================================================================

namespace spi_flash_opcodes {
    constexpr uint8_t READ            = 0x03;  ///< Read (zero latency)
    constexpr uint8_t READ_4B         = 0x13;  ///< Read (4-byte addr)
    constexpr uint8_t READ_FAST       = 0x0B;  ///< Fast Read
    constexpr uint8_t READ_FAST_ALT   = 0xEE;  ///< Fast Read (alternate)
    constexpr uint8_t READ_SFDP       = 0x5A;  ///< Read SFDP
    constexpr uint8_t PROGRAM         = 0x02;  ///< Page Program (3-byte addr)
    constexpr uint8_t PROGRAM_4BYTE   = 0x12;  ///< Page Program (4-byte addr)
    constexpr uint8_t ERASE_4KB       = 0x20;  ///< 4KB Sector Erase
    constexpr uint8_t ERASE_32KB      = 0x52;  ///< 32KB Block Erase
    constexpr uint8_t ERASE_64KB      = 0xD8;  ///< 64KB Block Erase (3-byte addr)
    constexpr uint8_t ERASE_64KB_4B   = 0xDC;  ///< 64KB Block Erase (4-byte addr)
    constexpr uint8_t CHIP_ERASE      = 0x60;  ///< Chip Erase
    constexpr uint8_t WRITE_ENABLE    = 0x06;  ///< Write Enable (WREN)
    constexpr uint8_t WRITE_DISABLE   = 0x04;  ///< Write Disable (WRDI)
    constexpr uint8_t READ_STATUS     = 0x05;  ///< Read Status Register
    constexpr uint8_t SUSPEND_75      = 0x75;  ///< Suspend
    constexpr uint8_t SUSPEND_B0      = 0xB0;  ///< Suspend (alternate)
    constexpr uint8_t RESUME_30       = 0x30;  ///< Resume
    constexpr uint8_t RESUME_7A       = 0x7A;  ///< Resume (alternate)
    constexpr uint8_t RESUME_D0       = 0xD0;  ///< Resume (alternate)
    constexpr uint8_t EN4B            = 0xB7;  ///< Enter 4-byte address mode
    constexpr uint8_t EX4B            = 0xE9;  ///< Exit 4-byte address mode
    constexpr uint8_t RESET_ENABLE    = 0x66;  ///< Enable Reset
    constexpr uint8_t RESET_EXECUTE   = 0x99;  ///< Execute Reset
}

// ============================================================================
// CONSTANTS
// ============================================================================

static const uint32_t SFDP_SIGNATURE = 0x50444653u; ///< "SFDP" in little-endian
static const uint8_t  SFDP_MAJOR_REV = 1u;
static const uint8_t  SFDP_MINOR_REV = 5u;          ///< JESD216A revision

// ============================================================================
// ENUMS
// ============================================================================

/// Address Mode (DWORD 1, Bits 18:17)
enum sfdp_addr_mode_e {
    ADDR_3_BYTE_ONLY = 0,
    ADDR_3_OR_4_BYTE = 1,
    ADDR_4_BYTE_ONLY = 2
};

/// Quad Enable Requirements (DWORD 15, Bits 22:20)
enum sfdp_qer_e {
    QER_NONE_OR_HOLD    = 0,
    QER_BIT1_SR2_REG    = 1,
    QER_BIT6_SR1_REG    = 2,
    QER_BIT7_SR2_OP3E   = 3,
    QER_BIT1_SR2_NO_CLR = 4,
    QER_BIT1_SR2_OP35   = 5
};

/// Soft Reset Support (DWORD 16, Bits 13:8)
enum sfdp_soft_reset_e {
    RESET_NONE    = 0x00,
    RESET_F0H     = 0x10,
    RESET_66H_99H = 0x08
};

// ============================================================================
// SFDP HEADER STRUCTURES
// ============================================================================

/**
 * @struct sfdp_header_t
 * @brief SFDP Header at address 0x000000 of the SFDP space
 */
struct sfdp_header_t {
    uint32_t signature;
    uint8_t  minor_rev;
    uint8_t  major_rev;
    uint8_t  nph;       ///< Number of Parameter Headers (0-based)
    uint8_t  unused;    ///< Always 0xFF

    sfdp_header_t()
        : signature(SFDP_SIGNATURE), minor_rev(SFDP_MINOR_REV),
          major_rev(SFDP_MAJOR_REV), nph(0), unused(0xFF) {}

    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> b(8);
        memcpy(&b[0], &signature, 4);
        b[4] = minor_rev; b[5] = major_rev; b[6] = nph; b[7] = unused;
        return b;
    }
};

/**
 * @struct sfdp_parameter_header_t
 * @brief Parameter Header pointing to the Basic Flash Parameter Table
 */
struct sfdp_parameter_header_t {
    uint8_t  id_lsb;         ///< 0x00 = JEDEC Basic Table
    uint8_t  minor_rev;      ///< 0x05 for JESD216A
    uint8_t  major_rev;      ///< 0x01
    uint8_t  length_dwords;  ///< 16 DWORDs
    uint32_t ptp;            ///< Parameter Table Pointer (24-bit address)

    sfdp_parameter_header_t()
        : id_lsb(0x00), minor_rev(0x05), major_rev(0x01),
          length_dwords(16), ptp(0x000080) {}

    void set_pointer(uint32_t ptr) { ptp = ptr; }

    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> b(8);
        b[0] = id_lsb; b[1] = minor_rev; b[2] = major_rev; b[3] = length_dwords;
        uint32_t p = ptp & 0x00FFFFFFu;
        memcpy(&b[4], &p, 3);
        b[7] = 0xFF;
        return b;
    }
};

// ============================================================================
// DWORD BASE
// ============================================================================

/**
 * @struct dword_base_t
 * @brief Base for all 16 DWORD structures in the Basic Flash Parameter Table
 */
struct dword_base_t {
    uint32_t value;

    explicit dword_base_t(uint32_t default_val = 0xFFFFFFFFu) : value(default_val) {}

    uint32_t to_dword() const { return value; }
    void from_dword(uint32_t dword) { value = dword; }

    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> b(4);
        b[0] =  value        & 0xFF;
        b[1] = (value >>  8) & 0xFF;
        b[2] = (value >> 16) & 0xFF;
        b[3] = (value >> 24) & 0xFF;
        return b;
    }
};

// ============================================================================
// DWORD 1: Architecture & Fast Read Support
// ============================================================================
struct dword_1_t : public dword_base_t {
    dword_1_t() : dword_base_t(0xFFF920E5u) { value |= 0xFF0000E0u; }

    void set_erase_size_support(uint8_t val)       { value = (value & ~0x3u) | (val & 0x3u); }
    void set_write_granularity(bool buf64)          { buf64  ? (value |= (1u<<2))  : (value &= ~(1u<<2)); }
    void set_volatile_bp(bool is_vol)               { is_vol ? (value |= (1u<<3))  : (value &= ~(1u<<3)); }
    void set_write_enable_instr(bool use_06h)       { use_06h? (value |= (1u<<4))  : (value &= ~(1u<<4)); }
    void set_4kb_erase_opcode(uint8_t op)           { value = (value & ~(0xFFu<<8))  | (static_cast<uint32_t>(op)<<8); }
    void set_fast_read_1_1_2(bool s)               { s ? (value |= (1u<<16)) : (value &= ~(1u<<16)); }
    void set_addr_bytes(sfdp_addr_mode_e m)         { value = (value & ~(0x3u<<17)) | ((m & 0x3u)<<17); }
    void set_dtr_support(bool s)                   { s ? (value |= (1u<<19)) : (value &= ~(1u<<19)); }
    void set_fast_read_1_2_2(bool s)               { s ? (value |= (1u<<20)) : (value &= ~(1u<<20)); }
    void set_fast_read_1_4_4(bool s)               { s ? (value |= (1u<<21)) : (value &= ~(1u<<21)); }
    void set_fast_read_1_1_4(bool s)               { s ? (value |= (1u<<22)) : (value &= ~(1u<<22)); }

    // Aliases
    void set_address_bytes(sfdp_addr_mode_e m)     { set_addr_bytes(m); }
    void set_fast_read_1_1_2_support(bool s)       { set_fast_read_1_1_2(s); }
    void set_fast_read_1_2_2_support(bool s)       { set_fast_read_1_2_2(s); }
    void set_fast_read_1_4_4_support(bool s)       { set_fast_read_1_4_4(s); }
    void set_fast_read_1_1_4_support(bool s)       { set_fast_read_1_1_4(s); }

    uint8_t          get_erase_size()                  const { return value & 0x3u; }
    bool             get_write_granularity()            const { return (value >> 2) & 0x1u; }
    bool             get_volatile_status_register()     const { return (value >> 3) & 0x1u; }
    bool             get_write_enable_opcode_select()   const { return (value >> 4) & 0x1u; }
    uint8_t          get_erase_4kb_instruction()        const { return (value >> 8) & 0xFFu; }
    bool             get_fast_read_1_1_2_support() const { return (value >> 16) & 0x1u; }
    sfdp_addr_mode_e get_address_bytes()            const { return static_cast<sfdp_addr_mode_e>((value >> 17) & 0x3u); }
    bool             get_dtr_clocking_support()    const { return (value >> 19) & 0x1u; }
    bool             get_fast_read_1_2_2_support() const { return (value >> 20) & 0x1u; }
    bool             get_fast_read_1_4_4_support() const { return (value >> 21) & 0x1u; }
    bool             get_fast_read_1_1_4_support() const { return (value >> 22) & 0x1u; }
};

// ============================================================================
// DWORD 2: Flash Memory Density
// ============================================================================
struct dword_2_t : public dword_base_t {
    dword_2_t() : dword_base_t(0x007FFFFFu) {} ///< Default 64Mbit

    void set_density(uint64_t bits) {
        if (bits > 0x80000000ULL) {
            uint32_t n = 0;
            while ((1ULL << n) < bits && n < 63u) n++;
            value = (1u << 31) | n;
        } else {
            value = static_cast<uint32_t>(bits - 1u);
        }
    }

    uint64_t get_density() const {
        if (value & (1u << 31)) return 1ULL << (value & 0x7FFFFFFFu);
        return static_cast<uint64_t>(value) + 1u;
    }
};

// ============================================================================
// DWORD 3: (1-4-4) and (1-1-4) Fast Read Parameters
// ============================================================================
struct dword_3_t : public dword_base_t {
    dword_3_t() : dword_base_t() {}

    void set_1_4_4_wait_states(uint8_t d)  { value = (value & ~0x1Fu)          | (d & 0x1Fu); }
    void set_1_4_4_mode_clocks(uint8_t c)  { value = (value & ~(0x7u<<5))      | ((c & 0x7u)<<5); }
    void set_1_4_4_opcode(uint8_t op)      { value = (value & ~(0xFFu<<8))     | (static_cast<uint32_t>(op)<<8); }
    void set_1_1_4_wait_states(uint8_t d)  { value = (value & ~(0x1Fu<<16))    | ((d & 0x1Fu)<<16); }
    void set_1_1_4_mode_clocks(uint8_t c)  { value = (value & ~(0x7u<<21))     | ((c & 0x7u)<<21); }
    void set_1_1_4_opcode(uint8_t op)      { value = (value & ~(0xFFu<<24))    | (static_cast<uint32_t>(op)<<24); }

    uint8_t get_1_4_4_wait_states()  const { return value & 0x1Fu; }
    uint8_t get_1_4_4_mode_clocks()  const { return (value >> 5)  & 0x7u; }
    uint8_t get_1_4_4_opcode()       const { return (value >> 8)  & 0xFFu; }
    uint8_t get_1_1_4_wait_states()  const { return (value >> 16) & 0x1Fu; }
    uint8_t get_1_1_4_mode_clocks()  const { return (value >> 21) & 0x7u; }
    uint8_t get_1_1_4_opcode()       const { return (value >> 24) & 0xFFu; }
};

// ============================================================================
// DWORD 4: (1-1-2) and (1-2-2) Fast Read Parameters
// ============================================================================
struct dword_4_t : public dword_base_t {
    dword_4_t() : dword_base_t() {}

    void set_1_1_2_wait_states(uint8_t d)  { value = (value & ~0x1Fu)          | (d & 0x1Fu); }
    void set_1_1_2_mode_clocks(uint8_t c)  { value = (value & ~(0x7u<<5))      | ((c & 0x7u)<<5); }
    void set_1_1_2_opcode(uint8_t op)      { value = (value & ~(0xFFu<<8))     | (static_cast<uint32_t>(op)<<8); }
    void set_1_2_2_wait_states(uint8_t d)  { value = (value & ~(0x1Fu<<16))    | ((d & 0x1Fu)<<16); }
    void set_1_2_2_mode_clocks(uint8_t c)  { value = (value & ~(0x7u<<21))     | ((c & 0x7u)<<21); }
    void set_1_2_2_opcode(uint8_t op)      { value = (value & ~(0xFFu<<24))    | (static_cast<uint32_t>(op)<<24); }

    uint8_t get_1_1_2_wait_states()  const { return value & 0x1Fu; }
    uint8_t get_1_1_2_mode_clocks()  const { return (value >> 5)  & 0x7u; }
    uint8_t get_1_1_2_opcode()       const { return (value >> 8)  & 0xFFu; }
    uint8_t get_1_2_2_wait_states()  const { return (value >> 16) & 0x1Fu; }
    uint8_t get_1_2_2_mode_clocks()  const { return (value >> 21) & 0x7u; }
    uint8_t get_1_2_2_opcode()       const { return (value >> 24) & 0xFFu; }
};

// ============================================================================
// DWORD 5: (2-2-2) and (4-4-4) Support Flags
// ============================================================================
struct dword_5_t : public dword_base_t {
    dword_5_t() : dword_base_t(0xFFFFFFFFu) {}

    void set_2_2_2_support(bool s) { s ? (value |= (1u<<0)) : (value &= ~(1u<<0)); }
    void set_4_4_4_support(bool s) { s ? (value |= (1u<<4)) : (value &= ~(1u<<4)); }

    bool get_2_2_2_support() const { return (value >> 0) & 0x1u; }
    bool get_4_4_4_support() const { return (value >> 4) & 0x1u; }
};

// ============================================================================
// DWORD 6: (2-2-2) Fast Read Parameters
// ============================================================================
struct dword_6_t : public dword_base_t {
    dword_6_t() : dword_base_t(0xFFFFFFFFu) {}

    void set_2_2_2_wait_states(uint8_t d) { value = (value & ~(0x1Fu<<16)) | ((d & 0x1Fu)<<16); }
    void set_2_2_2_mode_clocks(uint8_t c) { value = (value & ~(0x7u<<21))  | ((c & 0x7u)<<21); }
    void set_2_2_2_opcode(uint8_t op)     { value = (value & ~(0xFFu<<24)) | (static_cast<uint32_t>(op)<<24); }

    uint8_t get_2_2_2_wait_states() const { return (value >> 16) & 0x1Fu; }
    uint8_t get_2_2_2_mode_clocks() const { return (value >> 21) & 0x7u; }
    uint8_t get_2_2_2_opcode()      const { return (value >> 24) & 0xFFu; }
};

// ============================================================================
// DWORD 7: (4-4-4) Fast Read Parameters
// ============================================================================
struct dword_7_t : public dword_base_t {
    dword_7_t() : dword_base_t(0xFFFFFFFFu) {}

    void set_4_4_4_wait_states(uint8_t d) { value = (value & ~(0x1Fu<<16)) | ((d & 0x1Fu)<<16); }
    void set_4_4_4_mode_clocks(uint8_t c) { value = (value & ~(0x7u<<21))  | ((c & 0x7u)<<21); }
    void set_4_4_4_opcode(uint8_t op)     { value = (value & ~(0xFFu<<24)) | (static_cast<uint32_t>(op)<<24); }

    uint8_t get_4_4_4_wait_states() const { return (value >> 16) & 0x1Fu; }
    uint8_t get_4_4_4_mode_clocks() const { return (value >> 21) & 0x7u; }
    uint8_t get_4_4_4_opcode()      const { return (value >> 24) & 0xFFu; }
};

// ============================================================================
// DWORD 8: Erase Types 1 & 2
// ============================================================================
struct dword_8_t : public dword_base_t {
    dword_8_t() : dword_base_t() {}

    void set_type1_size_pow2(uint8_t n) { value = (value & ~0xFFu)        | n; }
    void set_type1_opcode(uint8_t op)   { value = (value & ~(0xFFu<<8))   | (static_cast<uint32_t>(op)<<8); }
    void set_type2_size_pow2(uint8_t n) { value = (value & ~(0xFFu<<16))  | (static_cast<uint32_t>(n)<<16); }
    void set_type2_opcode(uint8_t op)   { value = (value & ~(0xFFu<<24))  | (static_cast<uint32_t>(op)<<24); }

    uint8_t get_erase_type1_size()   const { return value & 0xFFu; }
    uint8_t get_erase_type1_opcode() const { return (value >> 8)  & 0xFFu; }
    uint8_t get_erase_type2_size()   const { return (value >> 16) & 0xFFu; }
    uint8_t get_erase_type2_opcode() const { return (value >> 24) & 0xFFu; }
};

// ============================================================================
// DWORD 9: Erase Types 3 & 4
// ============================================================================
struct dword_9_t : public dword_base_t {
    dword_9_t() : dword_base_t() {}

    void set_type3_size_pow2(uint8_t n) { value = (value & ~0xFFu)        | n; }
    void set_type3_opcode(uint8_t op)   { value = (value & ~(0xFFu<<8))   | (static_cast<uint32_t>(op)<<8); }
    void set_type4_size_pow2(uint8_t n) { value = (value & ~(0xFFu<<16))  | (static_cast<uint32_t>(n)<<16); }
    void set_type4_opcode(uint8_t op)   { value = (value & ~(0xFFu<<24))  | (static_cast<uint32_t>(op)<<24); }

    uint8_t get_type3_size_pow2() const { return value & 0xFFu; }
    uint8_t get_type3_opcode()    const { return (value >> 8)  & 0xFFu; }
    uint8_t get_type4_size_pow2() const { return (value >> 16) & 0xFFu; }
    uint8_t get_type4_opcode()    const { return (value >> 24) & 0xFFu; }
};

// ============================================================================
// DWORD 10: Erase Timings
// ============================================================================
struct dword_10_t : public dword_base_t {
    dword_10_t() : dword_base_t() {}

    void set_multiplier(uint8_t m) { value = (value & ~0xFu) | (m & 0xFu); }

    /// type_1_to_4: 1..4, unit_code: 00=1ms, 01=16ms, 10=128ms, 11=1s
    void set_timing(int type, uint8_t count, uint8_t unit_code) {
        int shift = 4 + (type - 1) * 7;
        uint32_t v = ((unit_code & 0x3u) << 5) | (count & 0x1Fu);
        value = (value & ~(0x7Fu << shift)) | (v << shift);
    }

    uint8_t get_multiplier() const { return value & 0xFu; }
    void get_erase_timing(uint8_t type_idx, uint8_t &count, uint8_t &units_code) const {
        int shift = 4 + (type_idx - 1) * 7;
        uint32_t t = (value >> shift) & 0x7Fu;
        count = t & 0x1Fu;
        units_code = (t >> 5) & 0x3u;
    }
};

// ============================================================================
// DWORD 11: Chip Erase & Program Timings
// ============================================================================
struct dword_11_t : public dword_base_t {
    dword_11_t() : dword_base_t() {}

    void set_prog_multiplier(uint8_t m)  { value = (value & ~0xFu) | (m & 0xFu); }
    void set_page_size_pow2(uint8_t n)   { value = (value & ~(0xFu<<4)) | ((n & 0xFu)<<4); }

    /// unit: 0=8us, 1=64us
    void set_page_prog(uint8_t count, bool unit_64us) {
        uint32_t v = ((unit_64us?1u:0u)<<5) | (count & 0x1Fu);
        value = (value & ~(0x3Fu<<8)) | (v<<8);
    }
    /// unit: 0=1us, 1=8us
    void set_byte_prog_first(uint8_t count, bool unit_8us) {
        uint32_t v = ((unit_8us?1u:0u)<<4) | (count & 0xFu);
        value = (value & ~(0x1Fu<<14)) | (v<<14);
    }
    void set_byte_prog_add(uint8_t count, bool unit_8us) {
        uint32_t v = ((unit_8us?1u:0u)<<4) | (count & 0xFu);
        value = (value & ~(0x1Fu<<19)) | (v<<19);
    }
    /// unit: 0=16ms, 1=256ms, 2=4s, 3=64s
    void set_chip_erase(uint8_t count, uint8_t unit) {
        uint32_t v = ((unit & 0x3u)<<5) | (count & 0x1Fu);
        value = (value & ~(0x7Fu<<24)) | (v<<24);
    }

    uint8_t get_prog_multiplier()  const { return value & 0xFu; }
    uint8_t get_page_size_pow2()   const { return (value >> 4) & 0xFu; }
    void get_page_prog(uint8_t &count, bool &unit_64us) const {
        uint32_t t = (value >> 8) & 0x3Fu;
        count = t & 0x1Fu; unit_64us = (t >> 5) & 0x1u;
    }
    void get_chip_erase_time(uint8_t &count, uint8_t &units_code) const {
        uint32_t t = (value >> 24) & 0x7Fu;
        count = t & 0x1Fu; units_code = (t >> 5) & 0x3u;
    }
};

// ============================================================================
// DWORD 12: Suspend / Resume Limits
// ============================================================================
struct dword_12_t : public dword_base_t {
    dword_12_t() : dword_base_t() {}

    void set_prog_suspend_prohibited(uint8_t m)  { value = (value & ~0xFu) | (m & 0xFu); }
    void set_erase_suspend_prohibited(uint8_t m) { value = (value & ~(0xFu<<4)) | ((m & 0xFu)<<4); }
    void set_prog_resume_interval(uint8_t c)     { value = (value & ~(0xFu<<9)) | ((c & 0xFu)<<9); }
    void set_prog_suspend_max(uint8_t c, uint8_t u) {
        uint32_t v = ((u & 0x3u)<<5) | (c & 0x1Fu);
        value = (value & ~(0x7Fu<<13)) | (v<<13);
    }
    void set_erase_resume_interval(uint8_t c)    { value = (value & ~(0xFu<<20)) | ((c & 0xFu)<<20); }
    void set_erase_suspend_max(uint8_t c, uint8_t u) {
        uint32_t v = ((u & 0x3u)<<5) | (c & 0x1Fu);
        value = (value & ~(0x7Fu<<24)) | (v<<24);
    }
    /// 0 = supported
    void set_suspend_supported(bool s) { s ? (value &= ~(1u<<31)) : (value |= (1u<<31)); }

    bool get_suspend_supported() const { return !((value >> 31) & 0x1u); }
};

// ============================================================================
// DWORD 13: Suspend / Resume Instructions
// ============================================================================
struct dword_13_t : public dword_base_t {
    dword_13_t() : dword_base_t() {}

    void set_prog_resume_op(uint8_t op)  { value = (value & ~0xFFu)       | op; }
    void set_prog_suspend_op(uint8_t op) { value = (value & ~(0xFFu<<8))  | (static_cast<uint32_t>(op)<<8); }
    void set_resume_op(uint8_t op)       { value = (value & ~(0xFFu<<16)) | (static_cast<uint32_t>(op)<<16); }
    void set_suspend_op(uint8_t op)      { value = (value & ~(0xFFu<<24)) | (static_cast<uint32_t>(op)<<24); }

    uint8_t get_prog_resume_op()  const { return value & 0xFFu; }
    uint8_t get_prog_suspend_op() const { return (value >> 8)  & 0xFFu; }
    uint8_t get_resume_op()       const { return (value >> 16) & 0xFFu; }
    uint8_t get_suspend_op()      const { return (value >> 24) & 0xFFu; }
};

// ============================================================================
// DWORD 14: Deep Powerdown & Status Register Polling
// ============================================================================
struct dword_14_t : public dword_base_t {
    dword_14_t() : dword_base_t() {}

    void set_poll_status_legacy(bool s) { s ? (value |= (1u<<2)) : (value &= ~(1u<<2)); }
    void set_poll_status_flag(bool s)   { s ? (value |= (1u<<3)) : (value &= ~(1u<<3)); }
    void set_exit_dpd_delay(uint8_t c, uint8_t u) {
        uint32_t v = ((u & 0x3u)<<5) | (c & 0x1Fu);
        value = (value & ~(0x7Fu<<8)) | (v<<8);
    }
    void set_exit_dpd_op(uint8_t op)  { value = (value & ~(0xFFu<<15)) | (static_cast<uint32_t>(op)<<15); }
    void set_enter_dpd_op(uint8_t op) { value = (value & ~(0xFFu<<23)) | (static_cast<uint32_t>(op)<<23); }
    /// 0 = supported
    void set_dpd_supported(bool s)    { s ? (value &= ~(1u<<31)) : (value |= (1u<<31)); }

    bool    get_poll_status_legacy() const { return (value >> 2)  & 0x1u; }
    bool    get_poll_status_flag()   const { return (value >> 3)  & 0x1u; }
    bool    get_dpd_supported()      const { return !((value >> 31) & 0x1u); }
    uint8_t get_exit_dpd_op()        const { return (value >> 15) & 0xFFu; }
    uint8_t get_enter_dpd_op()       const { return (value >> 23) & 0xFFu; }
};

// ============================================================================
// DWORD 15: Quad Enable Requirements, 0-4-4 and 4-4-4 Sequences
// ============================================================================
struct dword_15_t : public dword_base_t {
    dword_15_t() : dword_base_t() {}

    void set_4_4_4_disable_seq(uint8_t m)  { value = (value & ~0xFu)         | (m & 0xFu); }
    void set_4_4_4_enable_seq(uint8_t m)   { value = (value & ~(0x1Fu<<4))   | ((m & 0x1Fu)<<4); }
    void set_0_4_4_supported(bool s)       { s ? (value |= (1u<<9))  : (value &= ~(1u<<9)); }
    void set_0_4_4_exit_method(uint8_t m)  { value = (value & ~(0x3Fu<<10))  | ((m & 0x3Fu)<<10); }
    void set_0_4_4_entry_method(uint8_t m) { value = (value & ~(0xFu<<16))   | ((m & 0xFu)<<16); }
    void set_qer(sfdp_qer_e qer)           { value = (value & ~(0x7u<<20))   | ((qer & 0x7u)<<20); }
    void set_hold_wp_disable(bool s)       { s ? (value |= (1u<<23)) : (value &= ~(1u<<23)); }

    // Aliases
    void set_quad_enable_requirement(sfdp_qer_e q) { set_qer(q); }
    void set_0_4_4_mode_support(bool s)            { set_0_4_4_supported(s); }

    uint8_t    get_4_4_4_disable_sequence()   const { return value & 0xFu; }
    uint8_t    get_4_4_4_enable_sequence()    const { return (value >> 4)  & 0x1Fu; }
    bool       get_0_4_4_mode_support()       const { return (value >> 9)  & 0x1u; }
    sfdp_qer_e get_quad_enable_requirement()  const { return static_cast<sfdp_qer_e>((value >> 20) & 0x7u); }
};

// ============================================================================
// DWORD 16: 4-Byte Addressing & Soft Reset
// ============================================================================
struct dword_16_t : public dword_base_t {
    dword_16_t() : dword_base_t() {}

    void set_status1_write_enable(uint8_t m)  { value = (value & ~0x7Fu)        | (m & 0x7Fu); }
    void set_soft_reset_support(uint8_t m)    { value = (value & ~(0x3Fu<<8))   | ((m & 0x3Fu)<<8); }
    void set_exit_4byte_addr(uint16_t m)      { value = (value & ~(0x3FFu<<14)) | ((m & 0x3FFu)<<14); }
    void set_enter_4byte_addr(uint8_t m)      { value = (value & ~(0xFFu<<24))  | (static_cast<uint32_t>(m)<<24); }

    // Aliases
    void set_4byte_addr_entry_method(uint8_t m)  { set_enter_4byte_addr(m); }
    void set_4byte_addr_exit_method(uint16_t m)  { set_exit_4byte_addr(m); }

    uint8_t  get_soft_reset_support()        const { return (value >> 8)  & 0x3Fu; }
    uint16_t get_4byte_addr_exit_method()    const { return (value >> 14) & 0x3FFu; }
    uint8_t  get_4byte_addr_entry_method()   const { return (value >> 24) & 0xFFu; }
};

// ============================================================================
// JEDEC BASIC FLASH PARAMETER TABLE (16 DWORDs)
// ============================================================================

/**
 * @class jedec_basic_table_t
 * @brief Complete 16-DWORD Basic Flash Parameter Table (JESD216A Section 6.4)
 */
class jedec_basic_table_t {
private:
    dword_1_t  dword1;
    dword_2_t  dword2;
    dword_3_t  dword3;
    dword_4_t  dword4;
    dword_5_t  dword5;
    dword_6_t  dword6;
    dword_7_t  dword7;
    dword_8_t  dword8;
    dword_9_t  dword9;
    dword_10_t dword10;
    dword_11_t dword11;
    dword_12_t dword12;
    dword_13_t dword13;
    dword_14_t dword14;
    dword_15_t dword15;
    dword_16_t dword16;

public:
    jedec_basic_table_t() {}

    // DWORD accessors
    dword_1_t&  get_dword1()  { return dword1; }
    dword_2_t&  get_dword2()  { return dword2; }
    dword_3_t&  get_dword3()  { return dword3; }
    dword_4_t&  get_dword4()  { return dword4; }
    dword_5_t&  get_dword5()  { return dword5; }
    dword_6_t&  get_dword6()  { return dword6; }
    dword_7_t&  get_dword7()  { return dword7; }
    dword_8_t&  get_dword8()  { return dword8; }
    dword_9_t&  get_dword9()  { return dword9; }
    dword_10_t& get_dword10() { return dword10; }
    dword_11_t& get_dword11() { return dword11; }
    dword_12_t& get_dword12() { return dword12; }
    dword_13_t& get_dword13() { return dword13; }
    dword_14_t& get_dword14() { return dword14; }
    dword_15_t& get_dword15() { return dword15; }
    dword_16_t& get_dword16() { return dword16; }

    const dword_1_t&  get_dword1()  const { return dword1; }
    const dword_2_t&  get_dword2()  const { return dword2; }
    const dword_3_t&  get_dword3()  const { return dword3; }
    const dword_4_t&  get_dword4()  const { return dword4; }
    const dword_5_t&  get_dword5()  const { return dword5; }
    const dword_6_t&  get_dword6()  const { return dword6; }
    const dword_7_t&  get_dword7()  const { return dword7; }
    const dword_8_t&  get_dword8()  const { return dword8; }
    const dword_9_t&  get_dword9()  const { return dword9; }
    const dword_10_t& get_dword10() const { return dword10; }
    const dword_11_t& get_dword11() const { return dword11; }
    const dword_12_t& get_dword12() const { return dword12; }
    const dword_13_t& get_dword13() const { return dword13; }
    const dword_14_t& get_dword14() const { return dword14; }
    const dword_15_t& get_dword15() const { return dword15; }
    const dword_16_t& get_dword16() const { return dword16; }

    // High-level convenience methods
    void     set_density(uint64_t bits)          { dword2.set_density(bits); }
    uint64_t get_density()                  const { return dword2.get_density(); }
    void     set_address_bytes(sfdp_addr_mode_e m){ dword1.set_addr_bytes(m); }
    sfdp_addr_mode_e get_address_bytes()    const { return dword1.get_address_bytes(); }

    void set_fast_read_1_1_4(bool s, uint8_t mode, uint8_t wait, uint8_t op) {
        dword1.set_fast_read_1_1_4(s);
        dword3.set_1_1_4_mode_clocks(mode);
        dword3.set_1_1_4_wait_states(wait);
        dword3.set_1_1_4_opcode(op);
    }
    void set_fast_read_1_4_4(bool s, uint8_t mode, uint8_t wait, uint8_t op) {
        dword1.set_fast_read_1_4_4(s);
        dword3.set_1_4_4_mode_clocks(mode);
        dword3.set_1_4_4_wait_states(wait);
        dword3.set_1_4_4_opcode(op);
    }
    void set_suspend_resume_opcodes(uint8_t susp, uint8_t res,
                                    uint8_t prog_susp = 0, uint8_t prog_res = 0) {
        dword13.set_suspend_op(susp);
        dword13.set_resume_op(res);
        if (prog_susp) dword13.set_prog_suspend_op(prog_susp);
        if (prog_res)  dword13.set_prog_resume_op(prog_res);
    }
    void set_deep_powerdown(bool s, uint8_t enter_op, uint8_t exit_op) {
        dword14.set_dpd_supported(s);
        dword14.set_enter_dpd_op(enter_op);
        dword14.set_exit_dpd_op(exit_op);
    }
    void set_soft_reset_support(uint8_t mask)      { dword16.set_soft_reset_support(mask); }
    void set_quad_enable_requirement(sfdp_qer_e q) { dword15.set_qer(q); }

    /// Convenience: get (1-1-4) fast read opcode from DWORD 3
    uint8_t get_read_opcode_1_1_4() const { return dword3.get_1_1_4_opcode(); }

    /// Convenience: get erase type 1 size (2^N) and opcode from DWORD 8
    void get_sector_erase_type1(uint8_t& size_pow2, uint8_t& opcode) const {
        size_pow2 = dword8.get_erase_type1_size();
        opcode    = dword8.get_erase_type1_opcode();
    }

    /// Serialize all 16 DWORDs to 64 bytes (little-endian)
    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> all;
        all.reserve(64);
        auto append = [&](const dword_base_t& d) {
            auto b = d.to_bytes();
            all.insert(all.end(), b.begin(), b.end());
        };
        append(dword1);  append(dword2);  append(dword3);  append(dword4);
        append(dword5);  append(dword6);  append(dword7);  append(dword8);
        append(dword9);  append(dword10); append(dword11); append(dword12);
        append(dword13); append(dword14); append(dword15); append(dword16);
        return all;
    }
};

// ============================================================================
// SFDP ROM CONTAINER
// ============================================================================

/**
 * @class sfdp_rom_t
 * @brief Internal SFDP address space (read-only to the SPI bus)
 *
 * Layout (default):
 *   0x000000 : SFDP Header     (8 bytes)
 *   0x000008 : Parameter Header (8 bytes)
 *   0x000080 : Basic Table     (64 bytes)
 */
class sfdp_rom_t {
    std::vector<uint8_t> m_mem;

public:
    sfdp_rom_t() : m_mem(512, 0xFF) {}

    /// Write a byte block at offset into the SFDP space
    void load(uint32_t offset, const std::vector<uint8_t>& data) {
        if (offset + data.size() > m_mem.size())
            m_mem.resize(offset + data.size(), 0xFF);
        memcpy(&m_mem[offset], data.data(), data.size());
    }

    /// Read one byte from the SFDP space
    uint8_t read_byte(uint32_t addr) const {
        if (addr < m_mem.size()) return m_mem[addr];
        return 0xFF;
    }

    /// Build the default SFDP layout with default parameter values
    void build_default(const sfdp_header_t&           hdr = sfdp_header_t{},
                       const sfdp_parameter_header_t& phdr = sfdp_parameter_header_t{},
                       const jedec_basic_table_t&     tbl  = jedec_basic_table_t{}) {
        load(0x00, hdr.to_bytes());
        load(0x08, phdr.to_bytes());
        load(0x80, tbl.to_bytes());
    }
};

