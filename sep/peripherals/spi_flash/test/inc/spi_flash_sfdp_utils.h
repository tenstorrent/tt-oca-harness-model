#pragma once

/**
 * @file spi_flash_sfdp_utils.h
 * @brief SFDP parse and print utilities — for testbench use only
 *
 * These functions are intentionally kept out of the flash model itself.
 * They are "controller-side" tools: they decode raw SFDP bytes (as a
 * real host controller would after issuing READ_SFDP 0x5A) and print
 * or verify the results.
 *
 * Typical testbench usage:
 *
 *   // 1. Read raw SFDP bytes from the flash model
 *   std::vector<uint8_t> raw(144, 0xFF);
 *   flash.process_command(spi_flash_opcodes::READ_SFDP, 0x00, raw);
 *
 *   // 2. Parse them back into typed structures
 *   sfdp_header_t hdr; sfdp_parameter_header_t phdr; jedec_basic_table_t tbl;
 *   bool ok = parse_sfdp_from_bytes(raw, hdr, phdr, tbl);
 *
 *   // 3. Pretty-print for inspection
 *   print_sfdp_tree(hdr, phdr, tbl);
 *
 */


#include "spi_flash_sfdp.h"
#include <string>
#include <vector>

// ============================================================================
// STRING CONVERSION UTILITIES
// ============================================================================

/// Convert sfdp_addr_mode_e to human-readable string
std::string addr_mode_to_string(sfdp_addr_mode_e mode);

/// Convert sfdp_qer_e to human-readable string
std::string qer_to_string(sfdp_qer_e qer);

// ============================================================================
// PARSER FUNCTIONS
// ============================================================================

/**
 * @brief Read a 32-bit little-endian value from a byte vector
 * @param buf  Source byte vector
 * @param offset  Byte offset into buf
 * @return 32-bit value, or 0xFFFFFFFF if out of bounds
 */
uint32_t read_le32(const std::vector<uint8_t>& buf, size_t offset);

/**
 * @brief Parse a complete SFDP structure from a raw byte vector
 *
 * Expects standard SFDP layout:
 *   offset 0x00 : SFDP header     (8 bytes)
 *   offset 0x08 : Parameter header (8 bytes)
 *   offset PTP  : Basic table      (64 bytes)
 *
 * @return true if the SFDP signature is valid and all fields were parsed
 */
bool parse_sfdp_from_bytes(const std::vector<uint8_t>& data,
                           sfdp_header_t&            header,
                           sfdp_parameter_header_t&  p_header,
                           jedec_basic_table_t&      table);

/**
 * @brief Parse a complete SFDP structure from a binary file
 * @return true on success
 */
bool parse_sfdp_from_file(const std::string&        filename,
                          sfdp_header_t&            header,
                          sfdp_parameter_header_t&  p_header,
                          jedec_basic_table_t&      table);

// ============================================================================
// PRETTY-PRINT
// ============================================================================

/**
 * @brief Print the full SFDP tree to stdout
 *
 * Prints all 16 DWORDs with field names, values, and decoded meanings.
 * Useful for verifying that the flash model advertises the correct parameters.
 */
void print_sfdp_tree(const sfdp_header_t&           header,
                     const sfdp_parameter_header_t& p_header,
                     const jedec_basic_table_t&     table);

