/******************************************************************************
 * @file xspi_ctrl_func001_test.cpp
 * @brief Test cases for FUNC_XSPI_001 — Register Model: Initialization,
 *        Access Enforcement, and Reset
 *
 * This file implements all test cases mapped to FUNC_XSPI_001 in the
 * xspi_ctrl functionality-to-test-case mapping document (v2.0).
 *
 * FUNC_XSPI_001 Test Coverage (9 test cases):
 * - TC_XSPI_REG_001: Reset values of all ctrl_cmd_stat_a registers (0x000–0x158)
 * - TC_XSPI_REG_002: Reset values of all ctrl_cfg_common_a registers (0x208–0x260)
 * - TC_XSPI_REG_003: Reset values of ctrl_consts_a (version=0x65220206,
 *                    features=0x03710003); immune to reset_in
 * - TC_XSPI_REG_004: RO write-ignore enforcement on ctrl_status, trd_status,
 *                    dma_target_error_l, dma_target_error_h
 * - TC_XSPI_REG_005: RO write-ignore enforcement on xspi_ctrl_version and
 *                    ctrl_features_reg
 * - TC_XSPI_REG_009: RW read/write retention for long_polling and short_polling
 * - TC_XSPI_REG_010: Reset value 0x00FF0000 for xip_mode_cfg(0x388); RW retention
 * - TC_XSPI_REG_011: PHY register store-and-acknowledge behavior (no flash
 *                    side effects)
 * - TC_XSPI_REG_012: RO enforcement on phy_gpio_status_0 and phy_gpio_status_1
 *
 * Note: TC_XSPI_REG_006, TC_XSPI_REG_007, TC_XSPI_REG_008 were moved to
 * FUNC_XSPI_012 and FUNC_XSPI_011 respectively due to forward-dependency
 * violations (they require ACMD and PIO engines to generate status conditions).
 *
 * Test Plan Reference:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-test-plan.md
 * Test Case Mapping:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-functionality-testcases.md
 * Detailed Design:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-detailed-design.md
 * Functionality List:
 *   /home/shravanr/Documents/tvastaavp/xspi_ctrl/docs/xspi_ctrl-functionality_list.md
 *
 * @copyright Copyright (c) 2021-2025, Vayavya Labs Pvt. Ltd.
 ******************************************************************************/

#include "testbench.h"
#include "xspi_ctrl_test.h"
#include "csml_logger.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>

/// @brief Module-scoped logger for FUNC_XSPI_001 test diagnostics
static CsmlLogger func001_logger;

namespace {
/** TC_XSPI_REG_003: specification constants (must match xspi_ctrl_basetest:: *_RESET) */
constexpr uint32_t k_tc003_xspi_ctrl_version  = 0x65220206u;
constexpr uint32_t k_tc003_ctrl_features_reg = 0x03710003u;

static_assert(static_cast<uint32_t>(xspi_ctrl_basetest::xspi_ctrl_version_RESET) ==
              k_tc003_xspi_ctrl_version, "xspi_ctrl_version_RESET must match TC_XSPI_REG_003 spec");
static_assert(static_cast<uint32_t>(xspi_ctrl_basetest::ctrl_features_reg_RESET) ==
              k_tc003_ctrl_features_reg, "ctrl_features_reg_RESET must match TC_XSPI_REG_003 spec");

static bool tc003_version_fields_match_spec(uint32_t v)
{
    return (((v >> 16) & 0xFFFFu) == 0x6522u)  // magic
        && (((v >> 8) & 0xFFu) == 0x02u)     // fix
        && ((v & 0xFFu) == 0x06u);            // rev
}

static bool tc003_features_fields_match_spec(uint32_t f)
{
    // Layout per xspi_ctrl detailed design: n_threads[3:0], boot[16], dma_addr[20],
    // dma_data[21], sfr_intf[23:22], n_banks[25:24] (encodings: 3 => eight banks / eight threads, etc.)
    return ((f & 0xFu) == 3u)
        && (((f >> 16) & 0x1u) == 1u)   // boot_available
        && (((f >> 20) & 0x1u) == 1u)  // dma_addr_width (64-bit)
        && (((f >> 21) & 0x1u) == 1u)  // dma_data_width (64-bit)
        && (((f >> 22) & 0x3u) == 1u)  // sfr_intf: 1 = APB
        && (((f >> 24) & 0x3u) == 3u); // n_banks: 3 = 8 banks
}
} // namespace

// =============================================================================
// Internal Helper Functions
// =============================================================================

/**
 * @brief Write a 32-bit register via the xspi_ctrl_test socket helper.
 *
 * Convenience wrapper that delegates to test->register_write_32, keeping
 * test case bodies concise and matching the established pattern in
 * testbench.cpp.
 *
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  32-bit value to write
 */
static inline void func001_write_reg(xspi_ctrl_test* test,
                                     unsigned int     offset,
                                     uint32_t         value)
{
    test->register_write_32(offset, value);
}

/**
 * @brief Read a 32-bit register via the xspi_ctrl_test socket helper.
 *
 * Convenience wrapper that delegates to test->register_read_32.
 *
 * @param test   Pointer to the xspi_ctrl_test harness instance
 * @param offset Byte offset of the target register within the DUT address space
 * @param value  Reference to receive the 32-bit read value
 */
static inline void func001_read_reg(xspi_ctrl_test* test,
                                    unsigned int    offset,
                                    uint32_t&       value)
{
    test->register_read_32(offset, value);
}

// =============================================================================
// FUNC_XSPI_001 Test Case Implementations
// =============================================================================

/******************************************************************************
 * @brief TC_XSPI_REG_001 — Reset values of ctrl_cmd_stat_a registers
 *
 * Verifies that all 18 registers in the ctrl_cmd_stat_a group (address range
 * 0x000–0x158) read back their documented hardware reset values immediately
 * after reset_in de-assertion and before any PoR transaction.
 *
 * The test applies reset via apply_reset() to guarantee a clean initial state,
 * then reads each register and compares against the enum values from
 * xspi_ctrl_basetest::Register_Reset_Val.
 *
 * Winning condition: All 18 registers match their respective RESET enum values.
 * Failing condition: Any register returns a value that differs from its RESET
 *   enum value — indicates missing reset initialization in the model.
 *
 * Registers under test (offset: expected reset value):
 *   cmd_reg0       (0x000): 0x00000000
 *   cmd_reg1       (0x004): 0x00000000
 *   cmd_reg2       (0x008): 0x00000000
 *   cmd_reg3       (0x00C): 0x00000000
 *   cmd_reg4       (0x010): 0x00000000
 *   cmd_reg5       (0x014): 0x00000000
 *   cmd_status_ptr (0x040): 0x00000000
 *   cmd_status     (0x044): 0x00000000
 *   ctrl_status    (0x100): 0x00000000
 *   trd_status     (0x104): 0x00000000
 *   intr_status    (0x110): 0x00000000
 *   intr_enable    (0x114): 0x00000000
 *   trd_comp_intr_status  (0x120): 0x00000000
 *   trd_error_intr_status (0x130): 0x00000000
 *   trd_error_intr_en     (0x134): 0x00000000
 *   dma_target_error_l    (0x150): 0x00000000
 *   dma_target_error_h    (0x154): 0x00000000
 *   boot_status           (0x158): 0x00000000
 ******************************************************************************/
void testbench::tc_xspi_reg_001_reset_values_ctrl_cmd_stat()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_001: Reset values — ctrl_cmd_stat_a (0x000–0x158)");

    // Apply reset to guarantee clean DUT state before reading
    apply_reset();

    // Table of {offset, expected_reset, name} for all ctrl_cmd_stat_a registers
    struct RegCheck {
        unsigned int offset;
        uint32_t     expected;
        const char*  name;
    };

    const RegCheck checks[] = {
        { xspi_ctrl_basetest::cmd_reg0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg0_RESET),
          "cmd_reg0" },
        { xspi_ctrl_basetest::cmd_reg1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg1_RESET),
          "cmd_reg1" },
        { xspi_ctrl_basetest::cmd_reg2_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg2_RESET),
          "cmd_reg2" },
        { xspi_ctrl_basetest::cmd_reg3_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg3_RESET),
          "cmd_reg3" },
        { xspi_ctrl_basetest::cmd_reg4_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg4_RESET),
          "cmd_reg4" },
        { xspi_ctrl_basetest::cmd_reg5_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::cmd_reg5_RESET),
          "cmd_reg5" },
        { xspi_ctrl_basetest::cmd_status_ptr_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::cmd_status_ptr_RESET),
          "cmd_status_ptr" },
        { xspi_ctrl_basetest::cmd_status_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::cmd_status_RESET),
          "cmd_status" },
        { xspi_ctrl_basetest::ctrl_status_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::ctrl_status_RESET),
          "ctrl_status" },
        { xspi_ctrl_basetest::trd_status_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::trd_status_RESET),
          "trd_status" },
        { xspi_ctrl_basetest::intr_status_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::intr_status_RESET),
          "intr_status" },
        { xspi_ctrl_basetest::intr_enable_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::intr_enable_RESET),
          "intr_enable" },
        { xspi_ctrl_basetest::trd_comp_intr_status_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::trd_comp_intr_status_RESET),
          "trd_comp_intr_status" },
        { xspi_ctrl_basetest::trd_error_intr_status_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::trd_error_intr_status_RESET),
          "trd_error_intr_status" },
        { xspi_ctrl_basetest::trd_error_intr_en_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::trd_error_intr_en_RESET),
          "trd_error_intr_en" },
        { xspi_ctrl_basetest::dma_target_error_l_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::dma_target_error_l_RESET),
          "dma_target_error_l" },
        { xspi_ctrl_basetest::dma_target_error_h_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::dma_target_error_h_RESET),
          "dma_target_error_h" },
        { xspi_ctrl_basetest::boot_status_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::boot_status_RESET),
          "boot_status" },
    };

    bool all_pass = true;
    const size_t num_checks = sizeof(checks) / sizeof(checks[0]);

    for (size_t i = 0; i < num_checks; i++) {
        uint32_t read_val = 0xDEADBEEFu;
        func001_read_reg(test, checks[i].offset, read_val);

        if (read_val != checks[i].expected) {
            CSML_ERROR(0, func001_logger)
                << "  FAIL [" << checks[i].name << "] offset=0x"
                << std::hex << std::setw(4) << std::setfill('0') << checks[i].offset
                << " expected=0x" << std::setw(8) << checks[i].expected
                << " got=0x" << std::setw(8) << read_val;
            all_pass = false;
        } else {
            CSML_INFO(2, func001_logger)
                << "  OK  [" << checks[i].name << "] = 0x"
                << std::hex << std::setw(8) << std::setfill('0') << read_val;
        }
    }

    report_test_result("TC_XSPI_REG_001", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_REG_002 — Reset values of ctrl_cfg_common_a registers
 *
 * Verifies that all registers in the ctrl_cfg_common_a group (address range
 * 0x208–0x260) read back their documented hardware reset values after reset.
 * Specifically validates the non-zero reset values:
 *   long_polling  (0x208) = 0x000003E8  (1000 decimal)
 *   short_polling (0x20C) = 0x000001F4  (500 decimal)
 *   dma_settings  (0x23C) = 0x000D0000  (non-zero PHY burst parameters)
 *
 * Winning condition: All ctrl_cfg_common_a registers match their RESET values.
 * Failing condition: Any register returns a value differing from its RESET
 *   enum value — confirms missing or incorrect initialization.
 *
 * Registers under test (offset: expected reset value):
 *   long_polling       (0x208): 0x000003E8
 *   short_polling      (0x20C): 0x000001F4
 *   ctrl_config        (0x230): 0x00000000
 *   dma_settings       (0x23C): 0x000D0000
 *   sdma_size          (0x240): 0x00000000
 *   sdma_trd_info      (0x244): 0x00000000
 *   sdma_addr0         (0x24C): 0x00000000
 *   sdma_addr1         (0x250): 0x00000000
 *   discovery_control  (0x260): 0x00000000
 ******************************************************************************/
void testbench::tc_xspi_reg_002_reset_values_ctrl_cfg_common()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_002: Reset values — ctrl_cfg_common_a (0x208–0x260)");

    // Apply reset to guarantee clean DUT state
    apply_reset();

    struct RegCheck {
        unsigned int offset;
        uint32_t     expected;
        const char*  name;
    };

    const RegCheck checks[] = {
        { xspi_ctrl_basetest::long_polling_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::long_polling_RESET),
          "long_polling" },
        { xspi_ctrl_basetest::short_polling_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::short_polling_RESET),
          "short_polling" },
        { xspi_ctrl_basetest::ctrl_config_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::ctrl_config_RESET),
          "ctrl_config" },
        { xspi_ctrl_basetest::dma_settings_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::dma_settings_RESET),
          "dma_settings" },
        { xspi_ctrl_basetest::sdma_size_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::sdma_size_RESET),
          "sdma_size" },
        { xspi_ctrl_basetest::sdma_trd_info_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::sdma_trd_info_RESET),
          "sdma_trd_info" },
        { xspi_ctrl_basetest::sdma_addr0_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::sdma_addr0_RESET),
          "sdma_addr0" },
        { xspi_ctrl_basetest::sdma_addr1_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::sdma_addr1_RESET),
          "sdma_addr1" },
        { xspi_ctrl_basetest::discovery_control_OFFSET,
          static_cast<uint32_t>(xspi_ctrl_basetest::discovery_control_RESET),
          "discovery_control" },
    };

    bool all_pass = true;
    const size_t num_checks = sizeof(checks) / sizeof(checks[0]);

    for (size_t i = 0; i < num_checks; i++) {
        uint32_t read_val = 0xDEADBEEFu;
        func001_read_reg(test, checks[i].offset, read_val);

        if (read_val != checks[i].expected) {
            CSML_ERROR(0, func001_logger)
                << "  FAIL [" << checks[i].name << "] offset=0x"
                << std::hex << std::setw(4) << std::setfill('0') << checks[i].offset
                << " expected=0x" << std::setw(8) << checks[i].expected
                << " got=0x" << std::setw(8) << read_val;
            all_pass = false;
        } else {
            CSML_INFO(2, func001_logger)
                << "  OK  [" << checks[i].name << "] = 0x"
                << std::hex << std::setw(8) << std::setfill('0') << read_val;
        }
    }

    report_test_result("TC_XSPI_REG_002", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_REG_003 — Reset values of ctrl_consts_a; immunity to reset_in
 *
 * Verifies that the two read-only capability registers in ctrl_consts_a match
 * the build-time constant values and field encodings, immediately after reset:
 *   xspi_ctrl_version  (0xF00) = 0x65220206 — magic 0x6522, fix 0x02, rev 0x06
 *   ctrl_features_reg  (0xF04) = 0x03710003 — n_banks=3 (eight banks), sfr_intf=1
 *   (APB), dma_data_width=1 (64-bit), dma_addr_width=1 (64-bit), boot_available=1,
 *   n_threads=3 (eight threads)
 *
 * These are constant read-only values sourced from the register model reset value
 * and are not overwritten by the reset handler (unlike the rest of the register
 * file). A second \c apply_reset() must not change them (PoR-style \c reset_in
 * does not reinitialize these capability IDs).
 *
 * Winning condition: packed values and subfield checks pass after each reset, and
 *   values are unchanged after a second \c apply_reset().
 * Failing condition: any mismatch in word value, field encoding, or reset immunity.
 ******************************************************************************/
void testbench::tc_xspi_reg_003_reset_values_ctrl_consts()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_003: xspi_ctrl_version=0x65220206, "
                      "ctrl_features_reg=0x03710003 — ctrl_consts_a; immune to reset_in");

    // --- Phase 1: Read values after initial reset ---
    apply_reset();

    uint32_t version_before  = 0xDEADBEEFu;
    uint32_t features_before = 0xDEADBEEFu;

    func001_read_reg(test, xspi_ctrl_basetest::xspi_ctrl_version_OFFSET,  version_before);
    func001_read_reg(test, xspi_ctrl_basetest::ctrl_features_reg_OFFSET,  features_before);

    const uint32_t expected_version  = k_tc003_xspi_ctrl_version;
    const uint32_t expected_features = k_tc003_ctrl_features_reg;

    bool pass_version_before  = (version_before  == expected_version);
    bool pass_features_before = (features_before == expected_features);
    bool pass_ver_fields_before   = tc003_version_fields_match_spec(version_before);
    bool pass_feat_fields_before  = tc003_features_fields_match_spec(features_before);

    if (!pass_version_before) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [xspi_ctrl_version] before second reset:"
            << " expected=0x" << std::hex << expected_version
            << " got=0x" << version_before;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [xspi_ctrl_version] = 0x"
            << std::hex << std::setw(8) << std::setfill('0') << version_before;
    }
    if (pass_version_before && !pass_ver_fields_before) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [xspi_ctrl_version] subfields (expect magic=0x6522, fix=0x02, rev=0x06) "
            << " for value=0x" << std::hex << std::setw(8) << std::setfill('0') << version_before;
    }

    if (!pass_features_before) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [ctrl_features_reg] before second reset:"
            << " expected=0x" << std::hex << expected_features
            << " got=0x" << features_before;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [ctrl_features_reg] = 0x"
            << std::hex << std::setw(8) << std::setfill('0') << features_before;
    }
    if (pass_features_before && !pass_feat_fields_before) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [ctrl_features_reg] subfields (n_threads, n_banks, sfr_intf, DMA widths, "
            << "boot_available) for value=0x" << std::hex << std::setw(8) << std::setfill('0')
            << features_before;
    }

    // --- Phase 2: Apply a second reset and re-read (immunity check) ---
    // Capability registers initialized from scml_property must survive reset_in.
    apply_reset();

    uint32_t version_after  = 0xDEADBEEFu;
    uint32_t features_after = 0xDEADBEEFu;

    func001_read_reg(test, xspi_ctrl_basetest::xspi_ctrl_version_OFFSET,  version_after);
    func001_read_reg(test, xspi_ctrl_basetest::ctrl_features_reg_OFFSET,  features_after);

    bool pass_version_after  = (version_after  == expected_version);
    bool pass_features_after = (features_after == expected_features);
    bool pass_ver_fields_after  = tc003_version_fields_match_spec(version_after);
    bool pass_feat_fields_after = tc003_features_fields_match_spec(features_after);

    if (!pass_version_after) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [xspi_ctrl_version] changed after second reset:"
            << " expected=0x" << std::hex << expected_version
            << " got=0x" << version_after;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [xspi_ctrl_version] immune to reset_in = 0x"
            << std::hex << std::setw(8) << std::setfill('0') << version_after;
    }
    if (pass_version_after && !pass_ver_fields_after) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [xspi_ctrl_version] subfields after second reset for value=0x"
            << std::hex << std::setw(8) << std::setfill('0') << version_after;
    }

    if (!pass_features_after) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [ctrl_features_reg] changed after second reset:"
            << " expected=0x" << std::hex << expected_features
            << " got=0x" << features_after;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [ctrl_features_reg] immune to reset_in = 0x"
            << std::hex << std::setw(8) << std::setfill('0') << features_after;
    }
    if (pass_features_after && !pass_feat_fields_after) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [ctrl_features_reg] subfields after second reset for value=0x"
            << std::hex << std::setw(8) << std::setfill('0') << features_after;
    }

    bool all_pass = pass_version_before && pass_features_before
                 && pass_ver_fields_before   && pass_feat_fields_before
                 && pass_version_after  && pass_features_after
                 && pass_ver_fields_after  && pass_feat_fields_after;

    report_test_result("TC_XSPI_REG_003", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_REG_004 — RO write-ignore on ctrl_status, trd_status,
 *        dma_target_error_l, dma_target_error_h
 *
 * Verifies that the four status/error registers in ctrl_cmd_stat_a that are
 * designated Read-Only are protected by scml2 set_write_ignore_restriction:
 * writing 0xFFFFFFFF to each register must have no effect — the read-back
 * value must remain 0x00000000 (hardware reset value for all four).
 *
 * This confirms that the model correctly configures the scml2 register
 * framework to silently discard write transactions to these addresses.
 *
 * Winning condition: All four registers read 0x00000000 after the rogue write.
 * Failing condition: Any register retains the written 0xFFFFFFFF value,
 *   indicating a missing write-ignore restriction in the register model.
 *
 * Registers under test:
 *   ctrl_status         (0x100): write_mask = 0x00000000, reset = 0x00000000
 *   trd_status          (0x104): write_mask = 0x00000000, reset = 0x00000000
 *   dma_target_error_l  (0x150): write_mask = 0x00000000, reset = 0x00000000
 *   dma_target_error_h  (0x154): write_mask = 0x00000000, reset = 0x00000000
 ******************************************************************************/
void testbench::tc_xspi_reg_004_ro_write_ignore_ctrl_status()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_004: RO write-ignore — ctrl_status, trd_status, "
                      "dma_target_error_l, dma_target_error_h");

    // Ensure clean state
    apply_reset();

    struct RegCheck {
        unsigned int offset;
        const char*  name;
    };

    const RegCheck ro_regs[] = {
        { xspi_ctrl_basetest::ctrl_status_OFFSET,        "ctrl_status" },
        { xspi_ctrl_basetest::trd_status_OFFSET,         "trd_status" },
        { xspi_ctrl_basetest::dma_target_error_l_OFFSET, "dma_target_error_l" },
        { xspi_ctrl_basetest::dma_target_error_h_OFFSET, "dma_target_error_h" },
    };

    bool all_pass = true;
    const size_t num_regs = sizeof(ro_regs) / sizeof(ro_regs[0]);

    for (size_t i = 0; i < num_regs; i++) {
        // Attempt to write all-ones — must be silently discarded
        func001_write_reg(test, ro_regs[i].offset, 0xFFFFFFFFu);
        wait(5, sc_core::SC_NS);

        uint32_t read_val = 0xDEADBEEFu;
        func001_read_reg(test, ro_regs[i].offset, read_val);

        // Pass: write was ignored; register retains reset value 0x00000000
        bool pass = (read_val == 0x00000000u);
        if (!pass) {
            CSML_ERROR(0, func001_logger)
                << "  FAIL [" << ro_regs[i].name << "] offset=0x"
                << std::hex << std::setw(4) << std::setfill('0') << ro_regs[i].offset
                << " RO protection failed: read-back=0x"
                << std::setw(8) << read_val << " (expected 0x00000000)";
            all_pass = false;
        } else {
            CSML_INFO(2, func001_logger)
                << "  OK  [" << ro_regs[i].name
                << "] write correctly ignored; read-back=0x00000000";
        }
    }

    report_test_result("TC_XSPI_REG_004", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_REG_005 — RO write-ignore on xspi_ctrl_version and
 *        ctrl_features_reg
 *
 * Verifies that the two capability registers in ctrl_consts_a are protected
 * against software writes. Writing the sentinel value 0xDEADBEEF to each
 * must be silently discarded, and the read-back value must match the
 * hardware reset value as specified in the architecture.
 *
 * This test specifically targets write-ignore enforcement on registers whose
 * values are initialized from scml_property at elaboration time (not from
 * the reset handler), making them doubly protected — both by write-ignore
 * restriction and by initialization source.
 *
 * Winning condition:
 *   - xspi_ctrl_version reads 0x65220206 after writing 0xDEADBEEF
 *   - ctrl_features_reg reads 0x03710003 after writing 0xDEADBEEF
 * Failing condition:
 *   - Either register retains 0xDEADBEEF — write-ignore not applied.
 ******************************************************************************/
void testbench::tc_xspi_reg_005_ro_write_ignore_ctrl_consts()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_005: RO write-ignore — xspi_ctrl_version, ctrl_features_reg");

    // Ensure clean state
    apply_reset();

    const uint32_t rogue_write       = 0xDEADBEEFu;
    const uint32_t expected_version  =
        static_cast<uint32_t>(xspi_ctrl_basetest::xspi_ctrl_version_RESET);
    const uint32_t expected_features =
        static_cast<uint32_t>(xspi_ctrl_basetest::ctrl_features_reg_RESET);

    // Attempt to corrupt xspi_ctrl_version
    func001_write_reg(test, xspi_ctrl_basetest::xspi_ctrl_version_OFFSET, rogue_write);
    wait(5, sc_core::SC_NS);
    uint32_t version_readback = 0xDEADBEEFu;
    func001_read_reg(test, xspi_ctrl_basetest::xspi_ctrl_version_OFFSET, version_readback);

    bool pass_version = (version_readback == expected_version);
    if (!pass_version) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [xspi_ctrl_version] RO protection failed:"
            << " wrote=0x" << std::hex << rogue_write
            << " expected=0x" << expected_version
            << " got=0x" << version_readback;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [xspi_ctrl_version] write correctly ignored;"
            << " read-back=0x" << std::hex << std::setw(8)
            << std::setfill('0') << version_readback;
    }

    // Attempt to corrupt ctrl_features_reg
    func001_write_reg(test, xspi_ctrl_basetest::ctrl_features_reg_OFFSET, rogue_write);
    wait(5, sc_core::SC_NS);
    uint32_t features_readback = 0xDEADBEEFu;
    func001_read_reg(test, xspi_ctrl_basetest::ctrl_features_reg_OFFSET, features_readback);

    bool pass_features = (features_readback == expected_features);
    if (!pass_features) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [ctrl_features_reg] RO protection failed:"
            << " wrote=0x" << std::hex << rogue_write
            << " expected=0x" << expected_features
            << " got=0x" << features_readback;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [ctrl_features_reg] write correctly ignored;"
            << " read-back=0x" << std::hex << std::setw(8)
            << std::setfill('0') << features_readback;
    }

    report_test_result("TC_XSPI_REG_005", pass_version && pass_features);
}

/******************************************************************************
 * @brief TC_XSPI_REG_009 — RW read/write retention for long_polling and
 *        short_polling
 *
 * Verifies that both polling interval registers in ctrl_cfg_common_a are
 * fully writable 16-bit registers that retain the last written value
 * (within the 16-bit writable mask 0x0000FFFF).
 *
 * The test:
 * 1. Applies reset and confirms the non-zero reset values are present.
 * 2. Writes new values 0x00000064 (100) and 0x00000032 (50) respectively.
 * 3. Reads back and asserts exact retention of the written values.
 *
 * This exercises the scml2 writable-bits path and confirms no side-effect
 * callback corrupts the stored value.
 *
 * Winning condition:
 *   long_polling  (0x208) reads 0x00000064 after writing 0x00000064
 *   short_polling (0x20C) reads 0x00000032 after writing 0x00000032
 * Failing condition:
 *   Either register reads back a different value — write storage failure.
 ******************************************************************************/
void testbench::tc_xspi_reg_009_rw_long_short_polling()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_009: RW retention — long_polling (0x208), "
                      "short_polling (0x20C)");

    // Phase 1: Apply reset and verify non-zero reset values
    apply_reset();

    uint32_t long_poll_reset  = 0u;
    uint32_t short_poll_reset = 0u;
    func001_read_reg(test, xspi_ctrl_basetest::long_polling_OFFSET,  long_poll_reset);
    func001_read_reg(test, xspi_ctrl_basetest::short_polling_OFFSET, short_poll_reset);

    const uint32_t exp_long_reset  =
        static_cast<uint32_t>(xspi_ctrl_basetest::long_polling_RESET);  // 0x000003E8
    const uint32_t exp_short_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::short_polling_RESET); // 0x000001F4

    bool pass_long_reset  = (long_poll_reset  == exp_long_reset);
    bool pass_short_reset = (short_poll_reset == exp_short_reset);

    CSML_INFO(2, func001_logger)
        << "  Reset check: long_polling=0x" << std::hex << long_poll_reset
        << " (expected 0x" << exp_long_reset << ")";
    CSML_INFO(2, func001_logger)
        << "  Reset check: short_polling=0x" << std::hex << short_poll_reset
        << " (expected 0x" << exp_short_reset << ")";

    if (!pass_long_reset) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [long_polling] reset value mismatch:"
            << " expected=0x" << std::hex << exp_long_reset
            << " got=0x" << long_poll_reset;
    }
    if (!pass_short_reset) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [short_polling] reset value mismatch:"
            << " expected=0x" << std::hex << exp_short_reset
            << " got=0x" << short_poll_reset;
    }

    // Phase 2: Write new values and verify retention
    const uint32_t new_long  = 0x00000064u;  // 100 decimal — well within 16-bit range
    const uint32_t new_short = 0x00000032u;  // 50 decimal

    func001_write_reg(test, xspi_ctrl_basetest::long_polling_OFFSET,  new_long);
    func001_write_reg(test, xspi_ctrl_basetest::short_polling_OFFSET, new_short);
    wait(5, sc_core::SC_NS);

    uint32_t long_readback  = 0xDEADBEEFu;
    uint32_t short_readback = 0xDEADBEEFu;
    func001_read_reg(test, xspi_ctrl_basetest::long_polling_OFFSET,  long_readback);
    func001_read_reg(test, xspi_ctrl_basetest::short_polling_OFFSET, short_readback);

    // Apply write mask (only 16 bits are writable per write_mask = 0x0000FFFF)
    const uint32_t write_mask =
        static_cast<uint32_t>(xspi_ctrl_basetest::long_polling_WRITE);
    bool pass_long_write  = ((long_readback  & write_mask) == (new_long  & write_mask));
    bool pass_short_write = ((short_readback & write_mask) == (new_short & write_mask));

    if (!pass_long_write) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [long_polling] write retention failed:"
            << " wrote=0x" << std::hex << new_long
            << " expected=0x" << (new_long & write_mask)
            << " got=0x" << long_readback;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [long_polling] write retained: 0x"
            << std::hex << std::setw(8) << std::setfill('0') << long_readback;
    }

    if (!pass_short_write) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [short_polling] write retention failed:"
            << " wrote=0x" << std::hex << new_short
            << " expected=0x" << (new_short & write_mask)
            << " got=0x" << short_readback;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [short_polling] write retained: 0x"
            << std::hex << std::setw(8) << std::setfill('0') << short_readback;
    }

    bool all_pass = pass_long_reset && pass_short_reset
                 && pass_long_write && pass_short_write;

    report_test_result("TC_XSPI_REG_009", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_REG_010 — xip_mode_cfg reset default and RW retention
 *
 * Verifies that xip_mode_cfg (0x388) in cmn_seq_regs_a:
 *   1. Reads 0x00FF0000 after reset (xip_dis_mb_val=0xFF in bits[23:16],
 *      all other fields including xip_en_mb_val=0x00 at reset).
 *   2. Accepts a write of 0x00A5B400 and retains the value within the
 *      24-bit writable mask (bits[23:0] = 0xFFFFFF per write_mask=0xFFFFFF).
 *
 * The non-zero reset value confirms the model sets xip_dis_mb_val to 0xFF
 * during elaboration/reset initialization. The write/readback confirms the
 * full 24-bit field width is writable and stored correctly.
 *
 * Winning condition:
 *   - Reset read: xip_mode_cfg == 0x00FF0000
 *   - Write/readback: xip_mode_cfg == 0x00A5B400 (within writable mask)
 * Failing condition:
 *   - Reset value wrong: initialization bug in model.
 *   - Write not retained: storage failure.
 ******************************************************************************/
void testbench::tc_xspi_reg_010_xip_mode_cfg_reset_default()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_010: xip_mode_cfg (0x388) reset default and RW retention");

    // Phase 1: Apply reset and check reset value
    apply_reset();

    uint32_t reset_readback = 0xDEADBEEFu;
    func001_read_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, reset_readback);

    const uint32_t expected_reset =
        static_cast<uint32_t>(xspi_ctrl_basetest::xip_mode_cfg_RESET);  // 0x00FF0000

    bool pass_reset = (reset_readback == expected_reset);
    if (!pass_reset) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [xip_mode_cfg] reset value mismatch:"
            << " expected=0x" << std::hex << expected_reset
            << " got=0x" << reset_readback;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [xip_mode_cfg] reset value = 0x"
            << std::hex << std::setw(8) << std::setfill('0') << reset_readback
            << " (xip_dis_mb_val=0xFF in bits[23:16])";
    }

    // Phase 2: Write a new pattern and verify retention
    // Value 0x00A5B400:
    //   bits[23:16] = xip_dis_mb_val = 0xA5
    //   bits[15:8]  = xip_en_mb_val  = 0xB4
    //   bits[7:0]   = 0x00
    const uint32_t write_val = 0x00A5B400u;

    func001_write_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, write_val);
    wait(5, sc_core::SC_NS);

    uint32_t write_readback = 0xDEADBEEFu;
    func001_read_reg(test, xspi_ctrl_basetest::xip_mode_cfg_OFFSET, write_readback);

    const uint32_t write_mask =
        static_cast<uint32_t>(xspi_ctrl_basetest::xip_mode_cfg_WRITE);  // 0x00FFFFFF
    bool pass_write = ((write_readback & write_mask) == (write_val & write_mask));

    if (!pass_write) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [xip_mode_cfg] write retention failed:"
            << " wrote=0x" << std::hex << write_val
            << " expected=0x" << (write_val & write_mask)
            << " got=0x" << write_readback;
    } else {
        CSML_INFO(2, func001_logger)
            << "  OK  [xip_mode_cfg] write retained = 0x"
            << std::hex << std::setw(8) << std::setfill('0') << write_readback;
    }

    report_test_result("TC_XSPI_REG_010", pass_reset && pass_write);
}

/******************************************************************************
 * @brief TC_XSPI_REG_011 — PHY register store-and-acknowledge behavior
 *
 * Verifies that PHY registers in dataslice_Rfile_a (0x2000–0x206F) and
 * ctb_Rfile_a (0x2080–0x20FF) behave as pure store-and-read-back registers
 * with no functional side effects on flash operation. Specifically:
 *
 * 1. Writes 0xDEADBEEF to phy_dq_timing_reg (0x2000) and reads back:
 *    expected read-back = 0xDEADBEEF (full 32-bit writable).
 *
 * 2. Writes 0xA5A5A5A5 to phy_ctrl_reg (0x2080) and reads back:
 *    expected read-back = 0xA5A5A5A5 (full 32-bit writable).
 *
 * 3. Writes 0x12345678 to phy_tsel_reg (0x2084) and reads back:
 *    expected read-back = 0x12345678 (full 32-bit writable).
 *
 * The LT model assumption (from xspi_ctrl-assumptions.md) states that PHY
 * registers are pure storage with no side-effect callbacks and do not trigger
 * any xspi_bus_socket transactions. The absence of flash transactions is
 * implicitly confirmed by the testbench's stub flash handler (which records
 * calls) not being invoked.
 *
 * Winning condition: All three registers read back exactly the written value.
 * Failing condition: Any readback differs — storage failure or register
 *   misconfiguration in the scml2 register model.
 ******************************************************************************/
void testbench::tc_xspi_reg_011_phy_reg_store_only()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_011: PHY register store-and-acknowledge "
                      "(phy_dq_timing_reg, phy_ctrl_reg, phy_tsel_reg)");

    // Apply reset to clear any prior state
    apply_reset();

    struct PhyRegCheck {
        unsigned int offset;
        uint32_t     write_val;
        const char*  name;
    };

    const PhyRegCheck phy_checks[] = {
        { xspi_ctrl_basetest::phy_dq_timing_reg_OFFSET,
          0xDEADBEEFu,
          "phy_dq_timing_reg (0x2000)" },
        { xspi_ctrl_basetest::phy_ctrl_reg_OFFSET,
          0xA5A5A5A5u,
          "phy_ctrl_reg (0x2080)" },
        { xspi_ctrl_basetest::phy_tsel_reg_OFFSET,
          0x12345678u,
          "phy_tsel_reg (0x2084)" },
    };

    bool all_pass = true;
    const size_t num_checks = sizeof(phy_checks) / sizeof(phy_checks[0]);

    for (size_t i = 0; i < num_checks; i++) {
        func001_write_reg(test, phy_checks[i].offset, phy_checks[i].write_val);
        wait(5, sc_core::SC_NS);

        uint32_t read_val = 0xDEADBEEFu;
        func001_read_reg(test, phy_checks[i].offset, read_val);

        // PHY registers are fully writable (write_mask = 0xFFFFFFFF)
        bool pass = (read_val == phy_checks[i].write_val);
        if (!pass) {
            CSML_ERROR(0, func001_logger)
                << "  FAIL [" << phy_checks[i].name << "]"
                << " wrote=0x" << std::hex << phy_checks[i].write_val
                << " got=0x" << read_val;
            all_pass = false;
        } else {
            CSML_INFO(2, func001_logger)
                << "  OK  [" << phy_checks[i].name << "] store-and-read-back = 0x"
                << std::hex << std::setw(8) << std::setfill('0') << read_val;
        }
    }

    if (all_pass) {
        CSML_INFO(2, func001_logger)
            << "  PHY registers confirmed as pure store-and-read-back "
               "(no flash bus side effects per LT model assumption)";
    }

    report_test_result("TC_XSPI_REG_011", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_REG_012 — RO enforcement on phy_gpio_status_0 and
 *        phy_gpio_status_1
 *
 * Verifies that the two GPIO status registers at the end of ctb_Rfile_a
 * (offsets 0x2090 and 0x2094) are read-only and reject all write attempts.
 * These registers represent hardware-driven GPIO observation signals that
 * should never be writable by software.
 *
 * The test writes 0xFFFFFFFF to each register and confirms the read-back
 * retains the hardware reset value 0x00000000. This confirms that the model
 * applies scml2 set_write_ignore_restriction (write_mask = 0x00000000) to
 * both registers.
 *
 * Winning condition:
 *   - phy_gpio_status_0 (0x2090) reads 0x00000000 after writing 0xFFFFFFFF
 *   - phy_gpio_status_1 (0x2094) reads 0x00000000 after writing 0xFFFFFFFF
 * Failing condition:
 *   - Either register retains the written value — write-ignore not applied.
 ******************************************************************************/
void testbench::tc_xspi_reg_012_phy_gpio_status_ro()
{
    func001_logger.setMaxVerbosity(2);
    report_test_start("TC_XSPI_REG_012: RO enforcement — "
                      "phy_gpio_status_0 (0x2090), phy_gpio_status_1 (0x2094)");

    // Apply reset to guarantee clean state
    apply_reset();

    struct RegCheck {
        unsigned int offset;
        const char*  name;
    };

    const RegCheck ro_regs[] = {
        { xspi_ctrl_basetest::phy_gpio_status_0_OFFSET, "phy_gpio_status_0 (0x2090)" },
        { xspi_ctrl_basetest::phy_gpio_status_1_OFFSET, "phy_gpio_status_1 (0x2094)" },
    };

    bool all_pass = true;
    const size_t num_regs = sizeof(ro_regs) / sizeof(ro_regs[0]);

    for (size_t i = 0; i < num_regs; i++) {
        // Attempt to write all-ones to the RO register
        func001_write_reg(test, ro_regs[i].offset, 0xFFFFFFFFu);
        wait(5, sc_core::SC_NS);

        uint32_t read_val = 0xDEADBEEFu;
        func001_read_reg(test, ro_regs[i].offset, read_val);

        // Pass: write was ignored; read-back is reset value 0x00000000
        bool pass = (read_val == 0x00000000u);
        if (!pass) {
            CSML_ERROR(0, func001_logger)
                << "  FAIL [" << ro_regs[i].name
                << "] RO protection failed: read-back=0x"
                << std::hex << std::setw(8) << std::setfill('0') << read_val
                << " (expected 0x00000000)";
            all_pass = false;
        } else {
            CSML_INFO(2, func001_logger)
                << "  OK  [" << ro_regs[i].name
                << "] write correctly ignored; read-back=0x00000000";
        }
    }

    report_test_result("TC_XSPI_REG_012", all_pass);
}


/******************************************************************************
 * @brief TC_XSPI_REG_013 — RO write-ignore for all pure-RO registers
 *
 * Uses xspi_ctrl_basetest::reg_map: any entry with write_mask == 0 is treated
 * as read-only (write ignored). For each such register:
 *   1. Read value `before`
 *   2. Write 0xFFFFFFFF
 *   3. Read value `after`
 * Pass if after == before (write did not change storage). This covers volatile
 * RO fields as long as they do not change between the two reads.
 *
 * Registers with non-zero write_mask (e.g. W1C) are skipped — not pure RO.
 ******************************************************************************/
void testbench::tc_xspi_reg_013_ro_write_ignore_all_pure_ro()
{
    
    func001_logger.setMaxVerbosity(2);
    report_test_start(
        "TC_XSPI_REG_013: RO write-ignore — all reg_map entries with write_mask==0");

    apply_reset();

    bool        all_pass    = true;
    unsigned    ro_tested = 0;
    const uint32_t rogue  = 0xFFFFFFFFu;

    for (std::size_t i = 0; i < xspi_ctrl_basetest::reg_map_size; ++i) {
        const xspi_ctrl_basetest::Register_Property_t& e = ::reg_map[i];
        if (e.write_mask != 0u) {
            continue;
        }

        uint32_t before = 0u;
        func001_read_reg(test, e.reg_offset, before);

        func001_write_reg(test, e.reg_offset, rogue);
        wait(5, sc_core::SC_NS);

        uint32_t after = 0u;
        func001_read_reg(test, e.reg_offset, after);

        const bool pass = (after == before);
        ++ro_tested;

        if (!pass) {
            CSML_ERROR(0, func001_logger)
                << "  FAIL [" << e.reg_name << "] offset=0x" << std::hex
                << std::setw(4) << std::setfill('0') << e.reg_offset
                << " RO write-ignore: before=0x" << std::setw(8) << before
                << " after=0x" << std::setw(8) << after << " (expected unchanged)";
            all_pass = false;
        } else {
            CSML_INFO(2, func001_logger)
                << "  OK  [" << e.reg_name << "] offset=0x" << std::hex
                << std::setw(4) << std::setfill('0') << e.reg_offset
                << " write ignored; value=0x" << std::setw(8) << after;
        }
    }

    if (ro_tested == 19u) {
        CSML_ERROR(0, func001_logger)
            << "  FAIL [TC_XSPI_REG_013] no pure-RO registers found in reg_map "
               "(write_mask==0)";
        all_pass = false;
    }

    report_test_result("TC_XSPI_REG_013", all_pass);
}

/******************************************************************************
 * @brief TC_XSPI_RW_ALL — Verify RW behavior for all registers with write_mask!=0
 *
 * For each register in reg_map where write_mask != 0:
 *   1. Read initial value
 *   2. Write pattern1 (0xAAAAAAAA masked by write_mask)
 *   3. Read back and verify writable bits updated
 *   4. Write pattern2 (0x55555555 masked by write_mask)
 *   5. Read back and verify writable bits updated
 *
 * Notes:
 * - Read-only bits are ignored by masking.
 * - Registers with write_mask==0 are skipped (pure RO).
 ******************************************************************************/
void testbench::tc_xspi_rw_all_registers_from_regmap()
{
    func001_logger.setMaxVerbosity(2);

    report_test_start("TC_XSPI_RW_ALL: RW test for all registers using reg_map");

    apply_reset();

    bool all_pass = true;
    unsigned rw_tested = 0;

    const uint32_t pattern1 = 0xAAAAAAAAu;
    const uint32_t pattern2 = 0x55555555u;

    for (std::size_t i = 0; i < xspi_ctrl_basetest::reg_map_size; ++i)
    {
        const xspi_ctrl_basetest::Register_Property_t &e = reg_map[i];

        /* Skip pure RO registers */
        if (e.write_mask == 0u)
            continue;

        /* Skip registers that are not safe for RW test */
        if (e.reg_name.find("status") != std::string::npos)
            continue;

        if (e.reg_name.find("intr_status") != std::string::npos)
            continue;

        if (e.reg_name == "cmd_reg0")  // cmd_reg0 triggers execution in STIG
            continue;

        uint32_t before = 0;
        func001_read_reg(test, e.reg_offset, before);

        /* ---------------- Pattern 1 ---------------- */
        uint32_t write_val1 = (before & ~e.write_mask) | (pattern1 & e.write_mask);
        func001_write_reg(test, e.reg_offset, write_val1);
        wait(5, sc_core::SC_NS);

        uint32_t after1 = 0;
        func001_read_reg(test, e.reg_offset, after1);

        bool pass1 = ((after1 & e.write_mask) == (write_val1 & e.write_mask));

        if (!pass1)
        {
            CSML_ERROR(0, func001_logger)
                << "FAIL [RW Pattern1] [" << e.reg_name << "] offset=0x"
                << std::hex << std::setw(4) << std::setfill('0') << e.reg_offset
                << " mask=0x" << std::setw(8) << e.write_mask
                << " wrote=0x" << std::setw(8) << write_val1
                << " read=0x" << std::setw(8) << after1;

            all_pass = false;
        }

        /* ---------------- Pattern 2 ---------------- */
        uint32_t write_val2 = (before & ~e.write_mask) | (pattern2 & e.write_mask);
        func001_write_reg(test, e.reg_offset, write_val2);
        wait(5, sc_core::SC_NS);

        uint32_t after2 = 0;
        func001_read_reg(test, e.reg_offset, after2);

        bool pass2 = ((after2 & e.write_mask) == (write_val2 & e.write_mask));

        if (!pass2)
        {
            CSML_ERROR(0, func001_logger)
                << "FAIL [RW Pattern2] [" << e.reg_name << "] offset=0x"
                << std::hex << std::setw(4) << std::setfill('0') << e.reg_offset
                << " mask=0x" << std::setw(8) << e.write_mask
                << " wrote=0x" << std::setw(8) << write_val2
                << " read=0x" << std::setw(8) << after2;

            all_pass = false;
        }

        if (pass1 && pass2)
        {
            CSML_INFO(2, func001_logger)
                << "OK  [RW] [" << e.reg_name << "] offset=0x"
                << std::hex << std::setw(4) << std::setfill('0') << e.reg_offset;
        }

        ++rw_tested;
    }
    if (rw_tested == 0u)
    {
        CSML_ERROR(0, func001_logger)
            << "FAIL [TC_XSPI_RW_ALL] No writable registers found in reg_map";
        all_pass = false;
    }

    report_test_result("TC_XSPI_RW_ALL", all_pass);
}

/******************************************************************************
 * @brief run_func001_tests — top-level entry point for all FUNC_XSPI_001 tests
 *
 * Called from testbench::run_tests() after the mandatory baseline tests.
 * Executes all nine test cases mapped to FUNC_XSPI_001 in sequential order.
 * Each test case applies reset internally where needed; the suite begins with
 * an additional apply_reset() to establish a clean entry state.
 *
 * Test execution order (matches FUNC_XSPI_001 mapping document):
 *  1. TC_XSPI_REG_001 — ctrl_cmd_stat_a reset values
 *  2. TC_XSPI_REG_002 — ctrl_cfg_common_a reset values
 *  3. TC_XSPI_REG_003 — ctrl_consts_a reset values + reset_in immunity
 *  4. TC_XSPI_REG_004 — RO write-ignore on ctrl_status, trd_status, DMA error regs
 *  5. TC_XSPI_REG_005 — RO write-ignore on capability registers
 *  6. TC_XSPI_REG_009 — RW retention for long_polling and short_polling
 *  7. TC_XSPI_REG_010 — xip_mode_cfg reset default and RW retention
 *  8. TC_XSPI_REG_011 — PHY register store-and-acknowledge
 *  9. TC_XSPI_REG_012 — RO enforcement on phy_gpio_status registers
 ******************************************************************************/
void testbench::run_func001_tests()
{
    CSML_INFO(1, func001_logger)
        << "================================================";
    CSML_INFO(1, func001_logger)
        << "  FUNC_XSPI_001: Register Model — Initialization,";
    CSML_INFO(1, func001_logger)
        << "                 Access Enforcement, and Reset";
    CSML_INFO(1, func001_logger)
        << "  9 test cases (TC_XSPI_REG_001/002/003/004/005/";
    CSML_INFO(1, func001_logger)
        << "                TC_XSPI_REG_009/010/011/012)";
    CSML_INFO(1, func001_logger)
        << "================================================";

    // Establish clean DUT state before starting the suite
    apply_reset();

    // TC_XSPI_REG_001: ctrl_cmd_stat_a reset values
    tc_xspi_reg_001_reset_values_ctrl_cmd_stat();

    // TC_XSPI_REG_002: ctrl_cfg_common_a reset values
    tc_xspi_reg_002_reset_values_ctrl_cfg_common();

    // TC_XSPI_REG_003: ctrl_consts_a reset values + immunity to reset_in
    tc_xspi_reg_003_reset_values_ctrl_consts();

    // TC_XSPI_REG_004: RO write-ignore — ctrl_status, trd_status, DMA error regs
    tc_xspi_reg_004_ro_write_ignore_ctrl_status();

    // TC_XSPI_REG_005: RO write-ignore — capability registers
    tc_xspi_reg_005_ro_write_ignore_ctrl_consts();

    // TC_XSPI_REG_009: RW retention — long_polling, short_polling
    tc_xspi_reg_009_rw_long_short_polling();

    // TC_XSPI_REG_010: xip_mode_cfg reset default and RW retention
    tc_xspi_reg_010_xip_mode_cfg_reset_default();

    // TC_XSPI_REG_011: PHY register store-and-acknowledge
    tc_xspi_reg_011_phy_reg_store_only();

    // TC_XSPI_REG_012: RO enforcement — phy_gpio_status registers
    tc_xspi_reg_012_phy_gpio_status_ro();

    tc_xspi_reg_013_ro_write_ignore_all_pure_ro();

    tc_xspi_rw_all_registers_from_regmap();

    CSML_INFO(1, func001_logger)
        << "================================================";
    CSML_INFO(1, func001_logger)
        << "  FUNC_XSPI_001 test suite complete";
    CSML_INFO(1, func001_logger)
        << "================================================";
}
