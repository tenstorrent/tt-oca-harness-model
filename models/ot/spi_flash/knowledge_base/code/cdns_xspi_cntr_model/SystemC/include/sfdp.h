

#ifndef SFDP_DATA_H
#define SFDP_DATA_H

#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>
#include <string>

 namespace xspi_opcodes {
    /** @brief Read Zero Latency command (1S-1S-1S required) */
    constexpr uint8_t READ_ZERO_LATENCY    = 0x03;

    /** @brief Read Fast command */
    constexpr uint8_t READ_FAST            = 0x0B;

    /** @brief Read Fast alternate opcode */
    constexpr uint8_t READ_FAST_ALT        = 0xEE;

    /** @brief Read SFDP command (1S-1S-1S required) */
    constexpr uint8_t READ_SFDP            = 0x5A;

    /** @brief Program command (3-byte address) */
    constexpr uint8_t PROGRAM              = 0x02;

    /** @brief Program command (4-byte address) */
    constexpr uint8_t PROGRAM_4BYTE        = 0x12;

    /** @brief Erase 64KB+ block command (3-byte address) */
    constexpr uint8_t ERASE_64KB           = 0xD8;

    /** @brief Erase 64KB+ block command (4-byte address) */
    constexpr uint8_t ERASE_64KB_4BYTE    = 0xDC;

    /** @brief Write Disable command */
    constexpr uint8_t WRITE_DISABLE        = 0x04;

    /** @brief Read Status Register command */
    constexpr uint8_t READ_STATUS_REG      = 0x05;

    /** @brief Write Enable (WREN) command */
    constexpr uint8_t WRITE_ENABLE         = 0x06;

    /** @brief Program/Erase Resume command */
    constexpr uint8_t RESUME_30            = 0x30;

    /** @brief Program/Erase Suspend command */
    constexpr uint8_t SUSPEND_75           = 0x75;

    /** @brief Program/Erase Resume command */
    constexpr uint8_t RESUME_7A            = 0x7A;

    /** @brief Program/Erase Suspend command */
    constexpr uint8_t SUSPEND_B0           = 0xB0;

    /** @brief Program/Erase Resume command */
    constexpr uint8_t RESUME_D0            = 0xD0;
}



// ============================================================================
// CONSTANTS & ENUMS
// ============================================================================

static const uint32_t SFDP_SIGNATURE = 0x50444653; // "PDFS"
static const uint8_t  SFDP_MAJOR_REV = 1;
static const uint8_t  SFDP_MINOR_REV = 5; // JESD216A

/** Address Mode (DWORD 1, Bits 18:17) */ 
enum sfdp_addr_mode_e {
    ADDR_3_BYTE_ONLY = 0,
    ADDR_3_OR_4_BYTE = 1,
    ADDR_4_BYTE_ONLY = 2
};

/** Quad Enable Requirements (DWORD 15, Bits 22:20) */
enum sfdp_qer_e {
    QER_NONE_OR_HOLD    = 0, // No QE bit or HOLD# used
    QER_BIT1_SR2_REG    = 1, // QE is Bit 1 of SR2 (Write 2 bytes)
    QER_BIT6_SR1_REG    = 2, // QE is Bit 6 of SR1
    QER_BIT7_SR2_OP3E   = 3, // QE is Bit 7 of SR2 (Write 3Eh)
    QER_BIT1_SR2_NO_CLR = 4, // QE is Bit 1 of SR2 (Write 1 byte doesn't clear)
    QER_BIT1_SR2_OP35   = 5  // QE is Bit 1 of SR2 (Read 35h)
};

/** Soft Reset Support (DWORD 16, Bits 13:8) */
enum sfdp_soft_reset_e {
    RESET_NONE    = 0x00,
    RESET_F0H     = 0x10, // 10000b: Issue instruction F0h
    RESET_66H_99H = 0x08  // 01000b: Issue 66h then 99h
};

// ============================================================================
// SFDP HEADERS
// ============================================================================

struct sfdp_header_t {
    uint32_t signature; // "PDFS"
    uint8_t  minor_rev;
    uint8_t  major_rev;
    uint8_t  nph;       // Number of Parameter Headers (0-based)
    uint8_t  unused;

    sfdp_header_t() : signature(SFDP_SIGNATURE), minor_rev(SFDP_MINOR_REV), 
                      major_rev(SFDP_MAJOR_REV), nph(0), unused(0xFF) {}

    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> bytes(8);
        memcpy(&bytes[0], &signature, 4);
        bytes[4] = minor_rev; bytes[5] = major_rev; bytes[6] = nph; bytes[7] = unused;
        return bytes;
    }
};

struct sfdp_parameter_header_t {
    uint8_t  id_lsb;        // 0x00 for JEDEC Basic
    uint8_t  minor_rev;     // 0x05 for JESD216A
    uint8_t  major_rev;     // 0x01
    uint8_t  length_dwords; // 16 DWORDs
    uint32_t ptp;           // 24-bit Pointer

    sfdp_parameter_header_t() : id_lsb(0x00), minor_rev(0x05), major_rev(0x01), 
                                length_dwords(16), ptp(0x000080) {}

    void set_pointer(uint32_t ptr) { ptp = ptr; }

    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> bytes(8);
        bytes[0] = id_lsb; bytes[1] = minor_rev; bytes[2] = major_rev; bytes[3] = length_dwords;
        uint32_t p = ptp & 0x00FFFFFF; memcpy(&bytes[4], &p, 3); bytes[7] = 0xFF;
        return bytes;
    }
};

// ============================================================================
// BASE CLASS FOR DWORDS
// ============================================================================

/**
 * @struct dword_base_t
 * @brief Base structure for all DWORD structures
 * 
 * Provides common functionality for all 16 DWORD structures in the
 * Basic Flash Parameter Table. Each DWORD is a 32-bit value that
 * contains multiple bit fields as defined by JESD216A Section 6.4.
 */
struct dword_base_t {
    uint32_t value;  /**< The actual 32-bit DWORD value */
    
    /**
     * @brief Default constructor
     * @param default_val Initial value (default: 0xFFFFFFFF)
     */
    dword_base_t(uint32_t default_val = 0xFFFFFFFF) : value(default_val) {}

    /**
     * @brief Get the DWORD value
     * @return 32-bit DWORD value
     */
    uint32_t to_dword() const { return value; }
    
    /**
     * @brief Set the DWORD value
     * @param dword 32-bit DWORD value to set
     */
    void from_dword(uint32_t dword) { value = dword; }

    /**
     * @brief Serialize DWORD to byte vector
     * @return Vector of 4 bytes in little-endian byte order
     */
    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> b(4);
        b[0] = value & 0xFF; b[1] = (value >> 8) & 0xFF;
        b[2] = (value >> 16) & 0xFF; b[3] = (value >> 24) & 0xFF;
        return b;
    }
};

// ============================================================================
// 16 DWORD DEFINITIONS (JESD216A)
// ============================================================================

/**
 * @struct dword_1_t
 * @brief DWORD 1: Architecture & Fast Read Support
 * 
 * Contains device architecture parameters and fast read command support flags.
 * Bit fields: Erase size support (1:0), Write granularity (2), Volatile BP (3),
 * Write Enable instruction (4), 4KB Erase opcode (15:8), Fast Read support flags (16,20-22),
 * Address mode (18:17), DTR support (19).
 */
struct dword_1_t : public dword_base_t {
    dword_1_t() : dword_base_t(0xFFF920E5) { 
        // Force bits 31:24 to FFh (Reserved/Unused) and bits 7:5 to 111b
        value |= 0xFF0000E0; 
    }

    // Setters
    void set_erase_size_support(uint8_t val) { value = (value & ~0x3) | (val & 0x3); }
    void set_write_granularity(bool buffer_64b) { if(buffer_64b) value |= (1<<2); else value &= ~(1<<2); }
    void set_volatile_bp(bool is_volatile) { if(is_volatile) value |= (1<<3); else value &= ~(1<<3); }
    void set_write_enable_instr(bool use_06h) { if(use_06h) value |= (1<<4); else value &= ~(1<<4); }
    void set_4kb_erase_opcode(uint8_t op) { value = (value & ~(0xFF<<8)) | (op<<8); }
    void set_fast_read_1_1_2(bool supp) { if(supp) value |= (1<<16); else value &= ~(1<<16); }
    void set_addr_bytes(sfdp_addr_mode_e mode) { value = (value & ~(0x3<<17)) | ((mode & 0x3)<<17); }
    void set_dtr_support(bool supp) { if(supp) value |= (1<<19); else value &= ~(1<<19); }
    void set_fast_read_1_2_2(bool supp) { if(supp) value |= (1<<20); else value &= ~(1<<20); }
    void set_fast_read_1_4_4(bool supp) { if(supp) value |= (1<<21); else value &= ~(1<<21); }
    void set_fast_read_1_1_4(bool supp) { if(supp) value |= (1<<22); else value &= ~(1<<22); }
    
    // Compatibility aliases for existing code
    void set_fast_read_1_1_4_support(bool supported) { set_fast_read_1_1_4(supported); }
    void set_fast_read_1_4_4_support(bool supported) { set_fast_read_1_4_4(supported); }
    void set_erase_size(uint8_t val) { set_erase_size_support(val); }
    void set_volatile_status_register(bool is_volatile) { set_volatile_bp(is_volatile); }
    void set_address_bytes(sfdp_addr_mode_e mode) { set_addr_bytes(mode); }
    void set_fast_read_1_1_2_support(bool supp) { set_fast_read_1_1_2(supp); }
    void set_fast_read_1_2_2_support(bool supp) { set_fast_read_1_2_2(supp); }
    
    // Getters
    uint8_t get_erase_size() const { return value & 0x3; }
    bool get_write_granularity() const { return (value >> 2) & 0x1; }
    bool get_volatile_status_register() const { return (value >> 3) & 0x1; }
    bool get_write_enable_opcode_select() const { return (value >> 4) & 0x1; }
    uint8_t get_erase_4kb_instruction() const { return (value >> 8) & 0xFF; }
    bool get_fast_read_1_1_2_support() const { return (value >> 16) & 0x1; }
    sfdp_addr_mode_e get_address_bytes() const { return static_cast<sfdp_addr_mode_e>((value >> 17) & 0x3); }
    bool get_dtr_clocking_support() const { return (value >> 19) & 0x1; }
    bool get_fast_read_1_2_2_support() const { return (value >> 20) & 0x1; }
    bool get_fast_read_1_4_4_support() const { return (value >> 21) & 0x1; }
    bool get_fast_read_1_1_4_support() const { return (value >> 22) & 0x1; }
};

/** DWORD 2: Density */
struct dword_2_t : public dword_base_t {
    dword_2_t() : dword_base_t(0x007FFFFF) {} // Default 64Mbit

    void set_density(uint64_t bits) {
        if (bits > 0x80000000) { // >= 4Gb (Bit 31=1)
            uint32_t n = 0;
            while ((1ULL << n) < bits && n < 63) n++;
            value = (1 << 31) | n;
        } else { // < 2Gb (Bit 31=0)
            value = (uint32_t)(bits - 1);
        }
    }
    
    uint64_t get_density() const {
        if (value & (1<<31)) return 1ULL << (value & 0x7FFFFFFF);
        return (uint64_t)value + 1;
    }
};

/**
 * @struct dword_3_t
 * @brief DWORD 3: (1-4-4) and (1-1-4) Fast Read Parameters
 * 
 * Lower 16 bits: (1-4-4) parameters (wait states, mode clocks, opcode)
 * Upper 16 bits: (1-1-4) parameters (wait states, mode clocks, opcode)
 */
struct dword_3_t : public dword_base_t {
    dword_3_t() : dword_base_t() {}

    // Lower 16 bits: (1-4-4)
    void set_1_4_4_wait_states(uint8_t dummy) { value = (value & ~0x1F) | (dummy & 0x1F); }
    void set_1_4_4_mode_clocks(uint8_t clocks) { value = (value & ~(0x7<<5)) | ((clocks & 0x7)<<5); }
    void set_1_4_4_opcode(uint8_t op) { value = (value & ~(0xFF<<8)) | (op<<8); }

    // Upper 16 bits: (1-1-4)
    void set_1_1_4_wait_states(uint8_t dummy) { value = (value & ~(0x1F<<16)) | ((dummy & 0x1F)<<16); }
    void set_1_1_4_mode_clocks(uint8_t clocks) { value = (value & ~(0x7<<21)) | ((clocks & 0x7)<<21); }
    void set_1_1_4_opcode(uint8_t op) { value = (value & ~(0xFF<<24)) | (op<<24); }
    
    // Getters - for compatibility, use (1-1-4) fields as primary
    uint8_t get_wait_states() const { return (value >> 16) & 0x1F; }
    uint8_t get_mode_cycles() const { return (value >> 21) & 0x7; }
    uint8_t get_opcode() const { return (value >> 24) & 0xFF; }
    
    // Getters for (1-4-4) fields
    uint8_t get_1_4_4_wait_states() const { return value & 0x1F; }
    uint8_t get_1_4_4_mode_clocks() const { return (value >> 5) & 0x7; }
    uint8_t get_1_4_4_opcode() const { return (value >> 8) & 0xFF; }
    
    // Getters for (1-1-4) fields
    uint8_t get_1_1_4_wait_states() const { return (value >> 16) & 0x1F; }
    uint8_t get_1_1_4_mode_clocks() const { return (value >> 21) & 0x7; }
    uint8_t get_1_1_4_opcode() const { return (value >> 24) & 0xFF; }
    
    // Compatibility aliases for existing code
    void set_opcode(uint8_t op) { set_1_1_4_opcode(op); }
    void set_mode_cycles(uint8_t clocks) { set_1_1_4_mode_clocks(clocks); }
    void set_wait_states(uint8_t dummy) { set_1_1_4_wait_states(dummy); }
};

/**
 * @struct dword_4_t
 * @brief DWORD 4: (1-1-2) and (1-2-2) Fast Read Parameters
 * 
 * Lower 16 bits: (1-1-2) parameters (wait states, mode clocks, opcode)
 * Upper 16 bits: (1-2-2) parameters (wait states, mode clocks, opcode)
 */
struct dword_4_t : public dword_base_t {
    dword_4_t() : dword_base_t() {}

    // Lower 16 bits: (1-1-2)
    void set_1_1_2_wait_states(uint8_t dummy) { value = (value & ~0x1F) | (dummy & 0x1F); }
    void set_1_1_2_mode_clocks(uint8_t clocks) { value = (value & ~(0x7<<5)) | ((clocks & 0x7)<<5); }
    void set_1_1_2_opcode(uint8_t op) { value = (value & ~(0xFF<<8)) | (op<<8); }

    // Upper 16 bits: (1-2-2)
    void set_1_2_2_wait_states(uint8_t dummy) { value = (value & ~(0x1F<<16)) | ((dummy & 0x1F)<<16); }
    void set_1_2_2_mode_clocks(uint8_t clocks) { value = (value & ~(0x7<<21)) | ((clocks & 0x7)<<21); }
    void set_1_2_2_opcode(uint8_t op) { value = (value & ~(0xFF<<24)) | (op<<24); }
    
    // Getters - for compatibility, use (1-1-2) fields as primary
    uint8_t get_wait_states() const { return value & 0x1F; }
    uint8_t get_mode_cycles() const { return (value >> 5) & 0x7; }
    uint8_t get_opcode() const { return (value >> 8) & 0xFF; }
    
    // Getters for (1-1-2) fields
    uint8_t get_1_1_2_wait_states() const { return value & 0x1F; }
    uint8_t get_1_1_2_mode_clocks() const { return (value >> 5) & 0x7; }
    uint8_t get_1_1_2_opcode() const { return (value >> 8) & 0xFF; }
    
    // Getters for (1-2-2) fields
    uint8_t get_1_2_2_wait_states() const { return (value >> 16) & 0x1F; }
    uint8_t get_1_2_2_mode_clocks() const { return (value >> 21) & 0x7; }
    uint8_t get_1_2_2_opcode() const { return (value >> 24) & 0xFF; }
};

/** DWORD 5: (2-2-2) and (4-4-4) Support Flags */
struct dword_5_t : public dword_base_t {
    dword_5_t() : dword_base_t(0xFFFFFFFF) {}

    void set_2_2_2_support(bool supp) { if(supp) value |= (1<<0); else value &= ~(1<<0); }
    // Bits 3:1 Reserved (111b)
    void set_4_4_4_support(bool supp) { if(supp) value |= (1<<4); else value &= ~(1<<4); }
    // Bits 31:5 Reserved (All 1s)
    
    // Getters
    bool get_2_2_2_support() const { return (value >> 0) & 0x1; }
    bool get_4_4_4_support() const { return (value >> 4) & 0x1; }
};

/** DWORD 6: (2-2-2) Parameters */
struct dword_6_t : public dword_base_t {
    dword_6_t() : dword_base_t(0xFFFFFFFF) {} // Default reserved fields 1
    // Bits 15:0 Reserved
    void set_2_2_2_wait_states(uint8_t dummy) { value = (value & ~(0x1F<<16)) | ((dummy & 0x1F)<<16); }
    void set_2_2_2_mode_clocks(uint8_t clocks) { value = (value & ~(0x7<<21)) | ((clocks & 0x7)<<21); }
    void set_2_2_2_opcode(uint8_t op) { value = (value & ~(0xFF<<24)) | (op<<24); }
    
    // Getters
    uint8_t get_2_2_2_wait_states() const { return (value >> 16) & 0x1F; }
    uint8_t get_2_2_2_mode_clocks() const { return (value >> 21) & 0x7; }
    uint8_t get_2_2_2_opcode() const { return (value >> 24) & 0xFF; }
};

/** DWORD 7: (4-4-4) Parameters */
struct dword_7_t : public dword_base_t {
    dword_7_t() : dword_base_t(0xFFFFFFFF) {} // Default reserved fields 1
    // Bits 15:0 Reserved
    void set_4_4_4_wait_states(uint8_t dummy) { value = (value & ~(0x1F<<16)) | ((dummy & 0x1F)<<16); }
    void set_4_4_4_mode_clocks(uint8_t clocks) { value = (value & ~(0x7<<21)) | ((clocks & 0x7)<<21); }
    void set_4_4_4_opcode(uint8_t op) { value = (value & ~(0xFF<<24)) | (op<<24); }
    
    // Getters
    uint8_t get_4_4_4_wait_states() const { return (value >> 16) & 0x1F; }
    uint8_t get_4_4_4_mode_clocks() const { return (value >> 21) & 0x7; }
    uint8_t get_4_4_4_opcode() const { return (value >> 24) & 0xFF; }
};

/**
 * @struct dword_8_t
 * @brief DWORD 8: Erase Types 1 & 2
 * 
 * Contains erase type definitions: size (power of 2) and opcode for each type.
 */
struct dword_8_t : public dword_base_t {
    dword_8_t() : dword_base_t() {}
    
    void set_type1_size_pow2(uint8_t n) { value = (value & ~0xFF) | n; }
    void set_type1_opcode(uint8_t op)   { value = (value & ~(0xFF<<8)) | (op<<8); }
    void set_type2_size_pow2(uint8_t n) { value = (value & ~(0xFF<<16)) | (n<<16); }
    void set_type2_opcode(uint8_t op)   { value = (value & ~(0xFF<<24)) | (op<<24); }
    
    // Getters
    uint8_t get_erase_type1_size() const { return value & 0xFF; }
    uint8_t get_erase_type1_opcode() const { return (value >> 8) & 0xFF; }
    uint8_t get_erase_type2_size() const { return (value >> 16) & 0xFF; }
    uint8_t get_erase_type2_opcode() const { return (value >> 24) & 0xFF; }
    
    // Compatibility aliases for existing code
    void set_erase_type1_size(uint8_t n) { set_type1_size_pow2(n); }
    void set_erase_type2_size(uint8_t n) { set_type2_size_pow2(n); }
    void set_erase_type2_opcode(uint8_t op) { set_type2_opcode(op); }
};

/** DWORD 9: Erase Types 3 & 4 */
struct dword_9_t : public dword_base_t {
    dword_9_t() : dword_base_t() {}
    
    void set_type3_size_pow2(uint8_t n) { value = (value & ~0xFF) | n; }
    void set_type3_opcode(uint8_t op)   { value = (value & ~(0xFF<<8)) | (op<<8); }
    void set_type4_size_pow2(uint8_t n) { value = (value & ~(0xFF<<16)) | (n<<16); }
    void set_type4_opcode(uint8_t op)   { value = (value & ~(0xFF<<24)) | (op<<24); }
    
    // Getters
    uint8_t get_type3_size_pow2() const { return value & 0xFF; }
    uint8_t get_type3_opcode() const { return (value >> 8) & 0xFF; }
    uint8_t get_type4_size_pow2() const { return (value >> 16) & 0xFF; }
    uint8_t get_type4_opcode() const { return (value >> 24) & 0xFF; }
};

/**
 * @struct dword_10_t
 * @brief DWORD 10: Erase Timings
 * 
 * Contains erase timing multiplier and timing parameters for erase types 1-4.
 */
struct dword_10_t : public dword_base_t {
    dword_10_t() : dword_base_t() {}

    void set_multiplier(uint8_t m) { value = (value & ~0xF) | (m & 0xF); }
    // Helper to set timings for T1..T4
    // Type 1 (Bits 10:4), Type 2 (17:11), Type 3 (24:18), Type 4 (31:25)
    // Unit codes: 00=1ms, 01=16ms, 10=128ms, 11=1s
    void set_timing(int type_1_to_4, uint8_t count, uint8_t unit_code) {
        int shift = 4 + (type_1_to_4 - 1) * 7;
        uint32_t val = ((unit_code & 0x3) << 5) | (count & 0x1F);
        value = (value & ~(0x7F << shift)) | (val << shift);
    }
    
    // Getters
    uint8_t get_multiplier() const { return value & 0xF; }
    void get_erase_timing(uint8_t type_idx, uint8_t &count, uint8_t &units_code) const {
        int shift = 4 + (type_idx - 1) * 7;
        uint32_t timing = (value >> shift) & 0x7F;
        count = timing & 0x1F;
        units_code = (timing >> 5) & 0x3;
    }
};

/**
 * @struct dword_11_t
 * @brief DWORD 11: Chip Erase & Program Timings
 * 
 * Contains program timing multiplier, page size, and timing parameters for
 * page program, byte program, and chip erase operations.
 */
struct dword_11_t : public dword_base_t {
    dword_11_t() : dword_base_t() {}

    void set_prog_multiplier(uint8_t m) { value = (value & ~0xF) | (m & 0xF); }
    void set_page_size_pow2(uint8_t n) { value = (value & ~(0xF<<4)) | ((n & 0xF)<<4); }
    
    // Page Prog: Unit 0=8us, 1=64us
    void set_page_prog(uint8_t count, bool unit_64us) {
        uint32_t val = ((unit_64us?1:0)<<5) | (count & 0x1F);
        value = (value & ~(0x3F<<8)) | (val<<8);
    }
    // Byte Prog First: Unit 0=1us, 1=8us
    void set_byte_prog_first(uint8_t count, bool unit_8us) {
        uint32_t val = ((unit_8us?1:0)<<4) | (count & 0xF);
        value = (value & ~(0x1F<<14)) | (val<<14);
    }
    // Byte Prog Additional: Unit 0=1us, 1=8us
    void set_byte_prog_add(uint8_t count, bool unit_8us) {
        uint32_t val = ((unit_8us?1:0)<<4) | (count & 0xF);
        value = (value & ~(0x1F<<19)) | (val<<19);
    }
    // Chip Erase: Unit 0=16ms, 1=256ms, 2=4s, 3=64s
    void set_chip_erase(uint8_t count, uint8_t unit) {
        uint32_t val = ((unit & 0x3)<<5) | (count & 0x1F);
        value = (value & ~(0x7F<<24)) | (val<<24);
    }
    
    // Getters
    uint8_t get_prog_multiplier() const { return value & 0xF; }
    uint8_t get_page_size_pow2() const { return (value >> 4) & 0xF; }
    void get_page_prog(uint8_t &count, bool &unit_64us) const {
        uint32_t timing = (value >> 8) & 0x3F;
        count = timing & 0x1F;
        unit_64us = (timing >> 5) & 0x1;
    }
    void get_byte_prog_first(uint8_t &count, bool &unit_8us) const {
        uint32_t timing = (value >> 14) & 0x1F;
        count = timing & 0xF;
        unit_8us = (timing >> 4) & 0x1;
    }
    void get_byte_prog_add(uint8_t &count, bool &unit_8us) const {
        uint32_t timing = (value >> 19) & 0x1F;
        count = timing & 0xF;
        unit_8us = (timing >> 4) & 0x1;
    }
    void get_chip_erase_time(uint8_t &count, uint8_t &units_code) const {
        uint32_t timing = (value >> 24) & 0x7F;
        count = timing & 0x1F;
        units_code = (timing >> 5) & 0x3;
    }
};

/** DWORD 12: Suspend / Resume Limits */
struct dword_12_t : public dword_base_t {
    dword_12_t() : dword_base_t() {}

    void set_prog_suspend_prohibited(uint8_t mask) { value = (value & ~0xF) | (mask & 0xF); }
    void set_erase_suspend_prohibited(uint8_t mask) { value = (value & ~(0xF<<4)) | ((mask & 0xF)<<4); }
    // Reserved Bit 8
    void set_prog_resume_interval(uint8_t count) { value = (value & ~(0xF<<9)) | ((count & 0xF)<<9); }
    // Prog Suspend Max: Unit 0=128ns..3=64us
    void set_prog_suspend_max(uint8_t count, uint8_t unit) {
        uint32_t val = ((unit & 0x3)<<5) | (count & 0x1F);
        value = (value & ~(0x7F<<13)) | (val<<13);
    }
    void set_erase_resume_interval(uint8_t count) { value = (value & ~(0xF<<20)) | ((count & 0xF)<<20); }
    // Erase Suspend Max: Unit 0=128ns..3=64us
    void set_erase_suspend_max(uint8_t count, uint8_t unit) {
        uint32_t val = ((unit & 0x3)<<5) | (count & 0x1F);
        value = (value & ~(0x7F<<24)) | (val<<24);
    }
    void set_suspend_supported(bool supp) { if(!supp) value |= (1<<31); else value &= ~(1<<31); } // 0=Supported
    
    // Getters
    bool get_suspend_supported() const { return !((value >> 31) & 0x1); } // 0=Supported
    void get_prog_suspend_max(uint8_t &count, uint8_t &unit) const {
        uint32_t timing = (value >> 13) & 0x7F;
        count = timing & 0x1F;
        unit = (timing >> 5) & 0x3;
    }
    void get_erase_suspend_max(uint8_t &count, uint8_t &unit) const {
        uint32_t timing = (value >> 24) & 0x7F;
        count = timing & 0x1F;
        unit = (timing >> 5) & 0x3;
    }
};

/** DWORD 13: Suspend / Resume Instructions */
struct dword_13_t : public dword_base_t {
    dword_13_t() : dword_base_t() {}

    void set_prog_resume_op(uint8_t op)  { value = (value & ~0xFF) | op; }
    void set_prog_suspend_op(uint8_t op) { value = (value & ~(0xFF<<8)) | (op<<8); }
    void set_resume_op(uint8_t op)       { value = (value & ~(0xFF<<16)) | (op<<16); }
    void set_suspend_op(uint8_t op)      { value = (value & ~(0xFF<<24)) | (op<<24); }
    
    // Getters
    uint8_t get_prog_resume_op() const { return value & 0xFF; }
    uint8_t get_prog_suspend_op() const { return (value >> 8) & 0xFF; }
    uint8_t get_resume_op() const { return (value >> 16) & 0xFF; }
    uint8_t get_suspend_op() const { return (value >> 24) & 0xFF; }
};

/** DWORD 14: Deep Powerdown & Polling */
struct dword_14_t : public dword_base_t {
    dword_14_t() : dword_base_t() {}

    // Bits 1:0 Reserved
    void set_poll_status_legacy(bool supp) { if(supp) value |= (1<<2); else value &= ~(1<<2); }
    void set_poll_status_flag(bool supp)   { if(supp) value |= (1<<3); else value &= ~(1<<3); }
    // Exit DPD Delay: Unit 0=128ns..3=64us
    void set_exit_dpd_delay(uint8_t count, uint8_t unit) {
        uint32_t val = ((unit & 0x3)<<5) | (count & 0x1F);
        value = (value & ~(0x7F<<8)) | (val<<8);
    }
    void set_exit_dpd_op(uint8_t op) { value = (value & ~(0xFF<<15)) | (op<<15); }
    void set_enter_dpd_op(uint8_t op) { value = (value & ~(0xFF<<23)) | (op<<23); }
    void set_dpd_supported(bool supp) { if(!supp) value |= (1<<31); else value &= ~(1<<31); } // 0=Supported
    
    // Getters
    bool get_poll_status_legacy() const { return (value >> 2) & 0x1; }
    bool get_poll_status_flag() const { return (value >> 3) & 0x1; }
    bool get_dpd_supported() const { return !((value >> 31) & 0x1); } // 0=Supported
    void get_exit_dpd_delay(uint8_t &count, uint8_t &unit) const {
        uint32_t timing = (value >> 8) & 0x7F;
        count = timing & 0x1F;
        unit = (timing >> 5) & 0x3;
    }
    uint8_t get_exit_dpd_op() const { return (value >> 15) & 0xFF; }
    uint8_t get_enter_dpd_op() const { return (value >> 23) & 0xFF; }
};

/**
 * @struct dword_15_t
 * @brief DWORD 15: QER, 0-4-4, 4-4-4 Sequences
 * 
 * Contains Quad Enable Requirements, 0-4-4 mode support, and 4-4-4 mode sequences.
 */
struct dword_15_t : public dword_base_t {
    dword_15_t() : dword_base_t() {}

    void set_4_4_4_disable_seq(uint8_t mask) { value = (value & ~0xF) | (mask & 0xF); }
    void set_4_4_4_enable_seq(uint8_t mask) { value = (value & ~(0x1F<<4)) | ((mask & 0x1F)<<4); }
    void set_0_4_4_supported(bool supp) { if(supp) value |= (1<<9); else value &= ~(1<<9); }
    void set_0_4_4_exit_method(uint8_t mask) { value = (value & ~(0x3F<<10)) | ((mask & 0x3F)<<10); }
    void set_0_4_4_entry_method(uint8_t mask) { value = (value & ~(0xF<<16)) | ((mask & 0xF)<<16); }
    void set_qer(sfdp_qer_e qer) { value = (value & ~(0x7<<20)) | ((qer & 0x7)<<20); }
    void set_hold_wp_disable(bool supp) { if(supp) value |= (1<<23); else value &= ~(1<<23); }
    
    // Getters
    uint8_t get_4_4_4_disable_sequence() const { return value & 0xF; }
    uint8_t get_4_4_4_enable_sequence() const { return (value >> 4) & 0x1F; }
    bool get_0_4_4_mode_support() const { return (value >> 9) & 0x1; }
    sfdp_qer_e get_quad_enable_requirement() const { return static_cast<sfdp_qer_e>((value >> 20) & 0x7); }
    
    // Compatibility aliases for existing code
    void set_quad_enable_requirement(sfdp_qer_e qer) { set_qer(qer); }
    void set_0_4_4_mode_support(bool supp) { set_0_4_4_supported(supp); }
};

/**
 * @struct dword_16_t
 * @brief DWORD 16: 4-Byte Addressing & Soft Reset
 * 
 * Contains status register 1 write enable methods, soft reset support,
 * and 4-byte address entry/exit methods.
 */
struct dword_16_t : public dword_base_t {
    dword_16_t() : dword_base_t() {}

    void set_status1_write_enable(uint8_t mask) { value = (value & ~0x7F) | (mask & 0x7F); }
    // Bit 7 Reserved
    void set_soft_reset_support(uint8_t mask) { value = (value & ~(0x3F<<8)) | ((mask & 0x3F)<<8); }
    void set_exit_4byte_addr(uint16_t mask) { value = (value & ~(0x3FF<<14)) | ((mask & 0x3FF)<<14); }
    void set_enter_4byte_addr(uint8_t mask) { value = (value & ~(0xFF<<24)) | ((mask & 0xFF)<<24); }
    
    // Getters
    uint8_t get_soft_reset_support() const { return (value >> 8) & 0x3F; }
    uint16_t get_4byte_addr_exit_method() const { return (value >> 14) & 0x3FF; }
    uint8_t get_4byte_addr_entry_method() const { return (value >> 24) & 0xFF; }
    
    // Compatibility aliases for existing code
    void set_4byte_addr_entry_method(uint8_t mask) { set_enter_4byte_addr(mask); }
    void set_4byte_addr_exit_method(uint16_t mask) { set_exit_4byte_addr(mask); }
};

// ============================================================================
// MAIN TABLE CLASS
// ============================================================================

/**
 * @class jedec_basic_table_t
 * @brief JEDEC Basic Flash Parameter Table (16 DWORDs)
 * 
 * This class represents the complete Basic Flash Parameter Table as defined
 * in JESD216A Section 6.4. It contains 16 DWORD structures, each managing
 * specific bit fields related to flash device capabilities.
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
    
    uint32_t table[16];  /**< Legacy table array for backward compatibility */

public:
    jedec_basic_table_t() {}

    // DWORD accessors
    dword_1_t& get_dword1() { return dword1; }
    const dword_1_t& get_dword1() const { return dword1; }
    dword_2_t& get_dword2() { return dword2; }
    const dword_2_t& get_dword2() const { return dword2; }
    dword_3_t& get_dword3() { return dword3; }
    const dword_3_t& get_dword3() const { return dword3; }
    dword_4_t& get_dword4() { return dword4; }
    const dword_4_t& get_dword4() const { return dword4; }
    dword_5_t& get_dword5() { return dword5; }
    const dword_5_t& get_dword5() const { return dword5; }
    dword_6_t& get_dword6() { return dword6; }
    const dword_6_t& get_dword6() const { return dword6; }
    dword_7_t& get_dword7() { return dword7; }
    const dword_7_t& get_dword7() const { return dword7; }
    dword_8_t& get_dword8() { return dword8; }
    const dword_8_t& get_dword8() const { return dword8; }
    dword_9_t& get_dword9() { return dword9; }
    const dword_9_t& get_dword9() const { return dword9; }
    dword_10_t& get_dword10() { return dword10; }
    const dword_10_t& get_dword10() const { return dword10; }
    dword_11_t& get_dword11() { return dword11; }
    const dword_11_t& get_dword11() const { return dword11; }
    dword_12_t& get_dword12() { return dword12; }
    const dword_12_t& get_dword12() const { return dword12; }
    dword_13_t& get_dword13() { return dword13; }
    const dword_13_t& get_dword13() const { return dword13; }
    dword_14_t& get_dword14() { return dword14; }
    const dword_14_t& get_dword14() const { return dword14; }
    dword_15_t& get_dword15() { return dword15; }
    const dword_15_t& get_dword15() const { return dword15; }
    dword_16_t& get_dword16() { return dword16; }
    const dword_16_t& get_dword16() const { return dword16; }

    // Legacy table accessor for backward compatibility
    uint32_t* get_table() {
        table[0] = dword1.to_dword();
        table[1] = dword2.to_dword();
        table[2] = dword3.to_dword();
        table[3] = dword4.to_dword();
        table[4] = dword5.to_dword();
        table[5] = dword6.to_dword();
        table[6] = dword7.to_dword();
        table[7] = dword8.to_dword();
        table[8] = dword9.to_dword();
        table[9] = dword10.to_dword();
        table[10] = dword11.to_dword();
        table[11] = dword12.to_dword();
        table[12] = dword13.to_dword();
        table[13] = dword14.to_dword();
        table[14] = dword15.to_dword();
        table[15] = dword16.to_dword();
        return table;
    }
    const uint32_t* get_table() const {
        const_cast<jedec_basic_table_t*>(this)->get_table();
        return table;
    }

    // High-level convenience methods
    void set_density(uint64_t bits) { dword2.set_density(bits); }
    uint64_t get_density() const { return dword2.get_density(); }
    
    void set_address_bytes(sfdp_addr_mode_e mode) { dword1.set_addr_bytes(mode); }
    sfdp_addr_mode_e get_address_bytes() const { return dword1.get_address_bytes(); }
    
    void set_fast_read_1_1_4(bool supported, uint8_t mode_clocks, uint8_t wait_states, uint8_t opcode) {
        dword1.set_fast_read_1_1_4(supported);
        dword3.set_1_1_4_mode_clocks(mode_clocks);
        dword3.set_1_1_4_wait_states(wait_states);
        dword3.set_1_1_4_opcode(opcode);
    }
    uint8_t get_read_opcode_1_1_4() const { return dword3.get_opcode(); }
    
    void set_fast_read_1_4_4(bool supported, uint8_t mode_clocks, uint8_t wait_states, uint8_t opcode) {
        dword1.set_fast_read_1_4_4(supported);
        dword3.set_1_4_4_mode_clocks(mode_clocks);
        dword3.set_1_4_4_wait_states(wait_states);
        dword3.set_1_4_4_opcode(opcode);
    }
    
    void get_sector_erase_type1(uint8_t &size_pow2, uint8_t &opcode) const {
        size_pow2 = dword8.get_erase_type1_size();
        opcode = dword8.get_erase_type1_opcode();
    }
    
    void set_erase_timing(uint8_t type_idx, uint8_t count, uint8_t units_code) {
        dword10.set_timing(type_idx, count, units_code);
    }
    
    // High-level convenience methods for compatibility
    void set_soft_reset_support(uint8_t support_mask) {
        dword16.set_soft_reset_support(support_mask);
    }
    
    void set_quad_enable_requirement(sfdp_qer_e req) {
        dword15.set_qer(req);
    }
    
    void set_suspend_resume_opcodes(uint8_t suspend_op, uint8_t resume_op, 
                                    uint8_t prog_suspend_op = 0, uint8_t prog_resume_op = 0) {
        dword13.set_suspend_op(suspend_op);
        dword13.set_resume_op(resume_op);
        if (prog_suspend_op != 0) dword13.set_prog_suspend_op(prog_suspend_op);
        if (prog_resume_op != 0) dword13.set_prog_resume_op(prog_resume_op);
    }
    
    void set_deep_powerdown(bool supported, uint8_t enter_op, uint8_t exit_op) {
        dword14.set_dpd_supported(supported);
        dword14.set_enter_dpd_op(enter_op);
        dword14.set_exit_dpd_op(exit_op);
    }
    
    void set_4_4_4_mode_sequences(uint8_t enable_seq_mask, uint8_t disable_seq_mask) {
        dword15.set_4_4_4_enable_seq(enable_seq_mask);
        dword15.set_4_4_4_disable_seq(disable_seq_mask);
    }

    // Serialize to byte vector (64 bytes)
    std::vector<uint8_t> to_bytes() const {
        std::vector<uint8_t> all;
        all.reserve(64);
        auto append = [&](const dword_base_t& d) {
            auto b = d.to_bytes();
            all.insert(all.end(), b.begin(), b.end());
        };
        append(dword1); append(dword2); append(dword3); append(dword4);
        append(dword5); append(dword6); append(dword7); append(dword8);
        append(dword9); append(dword10); append(dword11); append(dword12);
        append(dword13); append(dword14); append(dword15); append(dword16);
        return all;
    }
};

/**
 * @brief ROM Container
 * Simulates the SFDP storage area
 */
class sfdp_rom_t {
    std::vector<uint8_t> memory;
public:
    sfdp_rom_t() { memory.resize(512, 0xFF); }
    
    void load(uint32_t offset, const std::vector<uint8_t>& data) {
        if (offset + data.size() > memory.size()) memory.resize(offset + data.size(), 0xFF);
        memcpy(&memory[offset], data.data(), data.size());
    }

    uint8_t read(uint32_t addr) const {
        if (addr < memory.size()) return memory[addr];
        return 0xFF;
    }

    void build_default() {
        sfdp_header_t h;
        sfdp_parameter_header_t ph;
        jedec_basic_table_t t;
        load(0x00, h.to_bytes());
        load(0x08, ph.to_bytes());
        load(0x80, t.to_bytes());
    }
    
    // Compatibility aliases for existing code
    void build_standard_layout() { build_default(); }
    uint8_t read_byte(uint32_t addr) const { return read(addr); }
    void load_structure(uint32_t offset, const std::vector<uint8_t>& data) { load(offset, data); }
};

// ============================================================================
// PUBLIC API - UTILITY FUNCTIONS
// ============================================================================
// These functions are defined in sfdp_utils.cpp

/**
 * @brief Read 32-bit little-endian value from byte vector
 * @param data Byte vector containing data
 * @param offset Offset in bytes from start of vector
 * @return 32-bit value in host byte order
 */
uint32_t read_le32(const std::vector<uint8_t>& buf, size_t offset);


/**
 * @brief Convert address mode enum to string
 * @param mode Address mode enum value
 * @return String representation of the address mode
 */
std::string addr_mode_to_string(sfdp_addr_mode_e mode);

/**
 * @brief Parse SFDP structure from byte vector
 * @param data Byte vector containing SFDP data
 * @param header Output parameter for SFDP header
 * @param p_header Output parameter for parameter header
 * @param table Output parameter for JEDEC basic table
 * @return true if parsing succeeded, false otherwise
 */
bool parse_sfdp_from_bytes(const std::vector<uint8_t>& data,
                           sfdp_header_t& header,
                           sfdp_parameter_header_t& p_header,
                           jedec_basic_table_t& table);

/**
 * @brief Parse SFDP structure from binary file
 * @param filename Path to binary file containing SFDP data
 * @param header Output parameter for SFDP header
 * @param p_header Output parameter for parameter header
 * @param table Output parameter for JEDEC basic table
 * @return true if parsing succeeded, false otherwise
 */
bool parse_sfdp_from_file(const std::string& filename,
                          sfdp_header_t& header,
                          sfdp_parameter_header_t& p_header,
                          jedec_basic_table_t& table);

/**
 * @brief Print SFDP structure tree (header, parameter header, and table)
 * @param header SFDP header
 * @param p_header Parameter header
 * @param table JEDEC basic table
 */
void print_sfdp_tree(const sfdp_header_t& header,
                     const sfdp_parameter_header_t& p_header,
                     const jedec_basic_table_t& table);

#endif // SFDP_DATA_H
