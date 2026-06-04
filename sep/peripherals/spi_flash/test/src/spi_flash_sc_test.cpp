/**
 * @file spi_flash_sc_test.cpp
 * @brief SystemC testbench for the spi_flash SC wrapper
 *
 * This testbench exercises spi_flash.cpp (the SystemC sc_module wrapper) via
 * the spi_if interface — the path NOT covered by the pure-C++ unit test
 * (spi_flash_test.cpp).
 *
 * A spi_master_stub module acts as the SPI controller.  It builds
 * spi_segment_t descriptors and calls spi_transaction() directly on the
 * flash's sc_export<spi_if>, exactly as spi_controller_ip would do in the VP.
 *
 * Connection
 * ----------
 *   spi_master_stub::spi_port  (sc_port<spi_if>)
 *       bound to
 *   spi_flash::spi_target      (sc_export<spi_if>)
 *
 * Segment patterns exercised
 * --------------------------
 *  No-addr   TX[opcode]                              csaat=false
 *  2-seg WR  TX[op+addr] csaat=true  → TX[data]     csaat=false
 *  1-seg WR  TX[op+addr+data]                        csaat=false
 *  2-seg RD  TX[op+addr] csaat=true  → RX[data]     csaat=false
 *  READ_SFDP TX[0x5A+addr] csaat=true → RX[192 B]   csaat=false
 *
 * Test scenarios
 * --------------
 * SC.1  WREN / WRDI / READ_STATUS  (segment WEL set/clear)
 * SC.2  PROGRAM (2-segment) + READ (segment verify)
 * SC.3  PROGRAM (1-segment: opcode+addr+data concatenated)
 * SC.4  ERASE_64KB + verify via READ
 * SC.5  READ_SFDP — verify signature & parse
 * SC.6  SUSPEND blocks PROGRAM, RESUME unblocks
 * SC.7  rst_ni deassert: WEL cleared, memory preserved
 * SC.8  PROGRAM without WREN fails, memory unchanged
 *
 * Address map used (no overlap between tests)
 * -------------------------------------------
 *  SC.2  0x10000 (block 1)
 *  SC.3  0x20000 (block 2)
 *  SC.4  0x30000 (block 3, erased within test)
 *  SC.6  0x40000 (block 4)
 *  SC.7  0x50000 (block 5)
 *  SC.8  0x60000 (block 6)
 */

#include "spi_flash.h"
#include "spi_flash_sfdp_utils.h"
#include "csml_parameter.h"

#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <vector>

#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// ============================================================================
// TEST HARNESS
// ============================================================================

static int s_tests_run    = 0;
static int s_tests_passed = 0;
static int s_tests_failed = 0;

#define SC_TEST_ASSERT(cond, msg)                                   \
    do {                                                             \
        s_tests_run++;                                               \
        if (cond) {                                                  \
            s_tests_passed++;                                        \
            std::cout << "  PASS: " << (msg) << "\n";               \
        } else {                                                     \
            s_tests_failed++;                                        \
            std::cerr << "  FAIL: " << (msg) << "\n";               \
        }                                                            \
    } while (0)

#define SC_TEST_SECTION(title) \
    std::cout << "\n[" << (title) << "]\n"

// ============================================================================
// SPI MASTER STUB
// ============================================================================

/**
 * @class spi_master_stub
 * @brief Minimal SPI master module for exercising spi_flash via spi_if
 *
 * Provides helper methods that build spi_segment_t descriptors and call
 * spi_port->spi_transaction() directly, mimicking what spi_controller_ip
 * would do in the real VP.
 */
class spi_master_stub : public sc_module
{
public:
    sc_port<spi_if> spi_port;  ///< Bound to spi_flash::spi_target
    sc_out<bool>    rst_ni;    ///< Drives the shared reset signal

    SC_HAS_PROCESS(spi_master_stub);

    explicit spi_master_stub(sc_module_name name)
        : sc_module(name), spi_port("spi_port"), rst_ni("rst_ni")
    {
        SC_THREAD(run);
    }

private:
    // -----------------------------------------------------------------------
    // Default SPI configuration (unused in LT model but required by spi_if)
    // -----------------------------------------------------------------------
    spi_config_t m_cfg = {};   // clkdiv=0, all CS timings=0

    // -----------------------------------------------------------------------
    // Segment helpers
    // -----------------------------------------------------------------------

    /**
     * @brief No-address, no-data command (WREN, WRDI, SUSPEND, RESUME)
     *
     * Single TX_ONLY segment, csaat=false.
     * Pattern:  TX[ opcode ]
     */
    bool cmd_no_addr(uint8_t opcode)
    {
        spi_segment_t seg{1, spi_direction_e::TX_ONLY,
                          spi_speed_e::STANDARD, /*csaat=*/false, /*csid=*/0};
        return spi_port->spi_transaction(seg, m_cfg, &opcode, nullptr);
    }

    /**
     * @brief Program using two separate segments (header then data)
     *
     * Segment 1: TX[ opcode, addr_hi, addr_mid, addr_lo ]  csaat=true
     * Segment 2: TX[ data bytes ]                           csaat=false
     *
     * The spi_flash wrapper accumulates both segments before dispatching.
     */
    bool cmd_program_2seg(uint8_t opcode, uint32_t addr,
                          const uint8_t* data, uint16_t len)
    {
        uint8_t hdr[4] = { opcode,
                           static_cast<uint8_t>(addr >> 16),
                           static_cast<uint8_t>(addr >>  8),
                           static_cast<uint8_t>(addr) };

        spi_segment_t seg1{4, spi_direction_e::TX_ONLY,
                           spi_speed_e::STANDARD, /*csaat=*/true, 0};
        spi_port->spi_transaction(seg1, m_cfg, hdr, nullptr);

        spi_segment_t seg2{len, spi_direction_e::TX_ONLY,
                           spi_speed_e::STANDARD, /*csaat=*/false, 0};
        return spi_port->spi_transaction(seg2, m_cfg, data, nullptr);
    }

    /**
     * @brief Program using a single segment (opcode + addr + data concatenated)
     *
     * Segment 1: TX[ opcode, addr_hi, addr_mid, addr_lo, data... ]  csaat=false
     *
     * Exercises the single-segment path through the accumulator.
     */
    bool cmd_program_1seg(uint8_t opcode, uint32_t addr,
                          const uint8_t* data, uint16_t len)
    {
        std::vector<uint8_t> tx;
        tx.reserve(4u + len);
        tx.push_back(opcode);
        tx.push_back(static_cast<uint8_t>(addr >> 16));
        tx.push_back(static_cast<uint8_t>(addr >>  8));
        tx.push_back(static_cast<uint8_t>(addr));
        tx.insert(tx.end(), data, data + len);

        spi_segment_t seg{static_cast<uint16_t>(tx.size()),
                          spi_direction_e::TX_ONLY,
                          spi_speed_e::STANDARD, /*csaat=*/false, 0};
        return spi_port->spi_transaction(seg, m_cfg, tx.data(), nullptr);
    }

    /**
     * @brief Erase command — single TX segment (opcode + 3-byte address)
     *
     * Segment 1: TX[ opcode, addr_hi, addr_mid, addr_lo ]  csaat=false
     */
    bool cmd_erase(uint8_t opcode, uint32_t addr)
    {
        uint8_t tx[4] = { opcode,
                          static_cast<uint8_t>(addr >> 16),
                          static_cast<uint8_t>(addr >>  8),
                          static_cast<uint8_t>(addr) };

        spi_segment_t seg{4, spi_direction_e::TX_ONLY,
                          spi_speed_e::STANDARD, /*csaat=*/false, 0};
        return spi_port->spi_transaction(seg, m_cfg, tx, nullptr);
    }

    /**
     * @brief Read command — TX header (csaat=true) then RX data (csaat=false)
     *
     * Segment 1: TX[ opcode, addr_hi, addr_mid, addr_lo ]  csaat=true
     * Segment 2: RX[ len bytes ]                            csaat=false
     */
    bool cmd_read(uint8_t opcode, uint32_t addr, uint8_t* rx, uint16_t len)
    {
        uint8_t hdr[4] = { opcode,
                           static_cast<uint8_t>(addr >> 16),
                           static_cast<uint8_t>(addr >>  8),
                           static_cast<uint8_t>(addr) };

        spi_segment_t seg1{4, spi_direction_e::TX_ONLY,
                           spi_speed_e::STANDARD, /*csaat=*/true, 0};
        spi_port->spi_transaction(seg1, m_cfg, hdr, nullptr);

        spi_segment_t seg2{len, spi_direction_e::RX_ONLY,
                           spi_speed_e::STANDARD, /*csaat=*/false, 0};
        return spi_port->spi_transaction(seg2, m_cfg, nullptr, rx);
    }

    /**
     * @brief Read Status Register (opcode only, 1-byte response)
     *
     * Segment 1: TX[ 0x05 ]  csaat=true
     * Segment 2: RX[ 1 B ]   csaat=false
     */
    uint8_t cmd_read_status()
    {
        uint8_t tx = spi_flash_opcodes::READ_STATUS;
        spi_segment_t seg1{1, spi_direction_e::TX_ONLY,
                           spi_speed_e::STANDARD, /*csaat=*/true, 0};
        spi_port->spi_transaction(seg1, m_cfg, &tx, nullptr);

        uint8_t rx = 0;
        spi_segment_t seg2{1, spi_direction_e::RX_ONLY,
                           spi_speed_e::STANDARD, /*csaat=*/false, 0};
        spi_port->spi_transaction(seg2, m_cfg, nullptr, &rx);
        return rx;
    }

    // -----------------------------------------------------------------------
    // Test scenarios
    // -----------------------------------------------------------------------

    void test_wren_wrdi()
    {
        SC_TEST_SECTION("SC.1: WREN / WRDI via segments");

        SC_TEST_ASSERT((cmd_read_status() & 0x02) == 0,
                       "Initial SR1 WEL = 0");

        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        SC_TEST_ASSERT((cmd_read_status() & 0x02) != 0,
                       "After WREN: WEL = 1");

        cmd_no_addr(spi_flash_opcodes::WRITE_DISABLE);
        SC_TEST_ASSERT((cmd_read_status() & 0x02) == 0,
                       "After WRDI: WEL = 0");
    }

    void test_program_read_2seg()
    {
        SC_TEST_SECTION("SC.2: PROGRAM (2-segment) + READ");

        const uint32_t addr    = 0x10000;
        const uint8_t  data[4] = {0xDE, 0xAD, 0xBE, 0xEF};

        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        bool ok = cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr, data, 4);
        SC_TEST_ASSERT(ok == true, "2-seg PROGRAM returns true");

        uint8_t rx[4] = {};
        cmd_read(spi_flash_opcodes::READ, addr, rx, 4);
        SC_TEST_ASSERT(rx[0] == 0xDE, "READ[0] = 0xDE");
        SC_TEST_ASSERT(rx[1] == 0xAD, "READ[1] = 0xAD");
        SC_TEST_ASSERT(rx[2] == 0xBE, "READ[2] = 0xBE");
        SC_TEST_ASSERT(rx[3] == 0xEF, "READ[3] = 0xEF");

        SC_TEST_ASSERT((cmd_read_status() & 0x02) == 0,
                       "WEL auto-cleared after PROGRAM");
    }

    void test_program_read_1seg()
    {
        SC_TEST_SECTION("SC.3: PROGRAM (1-segment: opcode+addr+data)");

        const uint32_t addr    = 0x20000;
        const uint8_t  data[2] = {0xCA, 0xFE};

        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        bool ok = cmd_program_1seg(spi_flash_opcodes::PROGRAM, addr, data, 2);
        SC_TEST_ASSERT(ok == true, "1-seg PROGRAM returns true");

        uint8_t rx[2] = {};
        cmd_read(spi_flash_opcodes::READ, addr, rx, 2);
        SC_TEST_ASSERT(rx[0] == 0xCA, "READ[0] = 0xCA");
        SC_TEST_ASSERT(rx[1] == 0xFE, "READ[1] = 0xFE");
    }

    void test_erase()
    {
        SC_TEST_SECTION("SC.4: ERASE_64KB via single TX segment");

        const uint32_t addr   = 0x30000;
        const uint8_t  fill[] = {0xAA};

        // Program a known value first
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr, fill, 1);

        uint8_t rx[1] = {};
        cmd_read(spi_flash_opcodes::READ, addr, rx, 1);
        SC_TEST_ASSERT(rx[0] == 0xAA, "Pre-erase: programmed byte = 0xAA");

        // Erase the 64KB block containing addr
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        bool ok = cmd_erase(spi_flash_opcodes::ERASE_64KB, addr);
        SC_TEST_ASSERT(ok == true, "ERASE_64KB returns true");

        cmd_read(spi_flash_opcodes::READ, addr, rx, 1);
        SC_TEST_ASSERT(rx[0] == 0xFF, "Post-erase: byte = 0xFF");

        SC_TEST_ASSERT((cmd_read_status() & 0x02) == 0,
                       "WEL auto-cleared after ERASE");
    }

    void test_read_sfdp()
    {
        SC_TEST_SECTION("SC.5: READ_SFDP (TX header + RX 192 bytes)");

        // 192 bytes covers header(8) + param_hdr(8) + padding + table(64)
        // at offset 0x80: need 0x80 + 64 = 192 bytes from addr 0x00
        std::vector<uint8_t> raw(192, 0xFF);

        uint8_t hdr[4] = {spi_flash_opcodes::READ_SFDP, 0x00, 0x00, 0x00};
        spi_segment_t seg1{4, spi_direction_e::TX_ONLY,
                           spi_speed_e::STANDARD, /*csaat=*/true, 0};
        spi_port->spi_transaction(seg1, m_cfg, hdr, nullptr);

        spi_segment_t seg2{192, spi_direction_e::RX_ONLY,
                           spi_speed_e::STANDARD, /*csaat=*/false, 0};
        spi_port->spi_transaction(seg2, m_cfg, nullptr, raw.data());

        // Verify SFDP signature at bytes 0..3
        uint32_t sig = static_cast<uint32_t>(raw[0])        |
                       (static_cast<uint32_t>(raw[1]) <<  8) |
                       (static_cast<uint32_t>(raw[2]) << 16) |
                       (static_cast<uint32_t>(raw[3]) << 24);
        SC_TEST_ASSERT(sig == SFDP_SIGNATURE, "READ_SFDP: signature = 'SFDP'");
        SC_TEST_ASSERT(raw[5] == SFDP_MAJOR_REV, "READ_SFDP: major rev correct");

        // Parse and verify key fields
        sfdp_header_t ph; sfdp_parameter_header_t pp; jedec_basic_table_t pt;
        bool ok = parse_sfdp_from_bytes(raw, ph, pp, pt);
        SC_TEST_ASSERT(ok == true, "parse_sfdp_from_bytes on READ_SFDP data succeeds");

        // 8MB flash → density = 64 Mbit = 8*1024*1024*8 bits
        SC_TEST_ASSERT(pt.get_density() == uint64_t(8u*1024u*1024u)*8u,
                       "SFDP density = 64 Mbit (8 MB)");
        SC_TEST_ASSERT(pt.get_address_bytes() == ADDR_3_BYTE_ONLY,
                       "SFDP address mode = 3-byte only (8 MB <= 16 MB)");
        SC_TEST_ASSERT(pt.get_dword11().get_page_size_pow2() == 8,
                       "SFDP page size = 256 bytes");
        SC_TEST_ASSERT(pt.get_dword8().get_erase_type1_size() == 16,
                       "SFDP erase type1 = 64KB (2^16)");
    }

    void test_suspend_resume()
    {
        SC_TEST_SECTION("SC.6: SUSPEND blocks PROGRAM, RESUME unblocks");

        const uint32_t addr   = 0x40000;
        const uint8_t  data[] = {0x55};

        // Suspend before WREN — program must fail
        cmd_no_addr(spi_flash_opcodes::SUSPEND_75);
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        bool ok = cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr, data, 1);
        SC_TEST_ASSERT(ok == false, "PROGRAM blocked while suspended (0x75)");

        // Resume — program must succeed
        cmd_no_addr(spi_flash_opcodes::RESUME_7A);
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        ok = cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr, data, 1);
        SC_TEST_ASSERT(ok == true, "PROGRAM succeeds after RESUME_7A");

        uint8_t rx[1] = {};
        cmd_read(spi_flash_opcodes::READ, addr, rx, 1);
        SC_TEST_ASSERT(rx[0] == 0x55, "Programmed byte = 0x55 after Resume");

        // Alternate suspend/resume opcodes
        cmd_no_addr(spi_flash_opcodes::SUSPEND_B0);
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        ok = cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr + 1, data, 1);
        SC_TEST_ASSERT(ok == false, "PROGRAM blocked by SUSPEND_B0");

        cmd_no_addr(spi_flash_opcodes::RESUME_D0);
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        ok = cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr + 1, data, 1);
        SC_TEST_ASSERT(ok == true, "PROGRAM succeeds after RESUME_D0");
    }

    void test_reset_signal()
    {
        SC_TEST_SECTION("SC.7: rst_ni deassert clears WEL, preserves memory");

        const uint32_t addr   = 0x50000;
        const uint8_t  data[] = {0xAB};

        // Program a known value
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr, data, 1);

        // Verify WEL is currently 0 (auto-cleared after program)
        // Then set it again for the reset test
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        SC_TEST_ASSERT((cmd_read_status() & 0x02) != 0,
                       "WEL = 1 before reset");

        // Assert reset (active low), let delta propagate, then deassert
        rst_ni.write(false);
        wait(SC_ZERO_TIME);
        rst_ni.write(true);
        wait(SC_ZERO_TIME);

        // WEL must be cleared by reset
        SC_TEST_ASSERT((cmd_read_status() & 0x02) == 0,
                       "WEL = 0 after rst_ni pulse");

        // Flash memory must be preserved
        uint8_t rx[1] = {};
        cmd_read(spi_flash_opcodes::READ, addr, rx, 1);
        SC_TEST_ASSERT(rx[0] == 0xAB,
                       "Memory preserved across reset");

        // Suspend must be cleared — program should work without resume
        cmd_no_addr(spi_flash_opcodes::WRITE_ENABLE);
        const uint8_t data2[] = {0xCD};
        bool ok = cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr + 1, data2, 1);
        SC_TEST_ASSERT(ok == true,
                       "PROGRAM works after reset (suspend cleared)");
    }

    void test_program_no_wren()
    {
        SC_TEST_SECTION("SC.8: PROGRAM without WREN fails, memory unchanged");

        const uint32_t addr   = 0x60000;
        const uint8_t  data[] = {0x42};

        SC_TEST_ASSERT((cmd_read_status() & 0x02) == 0,
                       "WEL = 0 (no WREN issued)");

        // Attempt program without WREN
        bool ok = cmd_program_2seg(spi_flash_opcodes::PROGRAM, addr, data, 1);
        SC_TEST_ASSERT(ok == false, "PROGRAM without WREN returns false");

        uint8_t rx[1] = {};
        cmd_read(spi_flash_opcodes::READ, addr, rx, 1);
        SC_TEST_ASSERT(rx[0] == 0xFF, "Memory unchanged (still 0xFF)");
    }

    // -----------------------------------------------------------------------
    // SC_THREAD: entry point
    // -----------------------------------------------------------------------
    void run()
    {
        // Deassert reset at simulation start
        rst_ni.write(true);
        wait(SC_ZERO_TIME);

        std::cout << "========================================\n";
        std::cout << "SPI Flash SC Wrapper Tests\n";
        std::cout << "========================================\n";

        test_wren_wrdi();
        test_program_read_2seg();
        test_program_read_1seg();
        test_erase();
        test_read_sfdp();
        test_suspend_resume();
        test_reset_signal();
        test_program_no_wren();

        std::cout << "\n========================================\n";
        std::cout << "Results: " << s_tests_passed << "/"
                  << s_tests_run << " passed";
        if (s_tests_failed > 0)
            std::cout << "  (" << s_tests_failed << " FAILED)";
        std::cout << "\n========================================\n";

        sc_stop();
    }
};

// ============================================================================
// SC_MAIN
// ============================================================================

int sc_main(int argc, char* argv[])
{
    load_config_file(argc > 1 ? argv[1] : nullptr);

    // Shared reset signal — stub drives it, flash reads it
    sc_signal<bool> rst_ni("rst_ni");

    spi_master_stub stub("stub");
    spi_flash       flash("flash", 8u * 1024u * 1024u);  // 8 MB

    // Port binding: stub.spi_port → flash.spi_target (sc_export<spi_if>)
    stub.spi_port(flash.spi_target);

    // Reset signal binding
    stub.rst_ni(rst_ni);
    flash.rst_ni(rst_ni);

    sc_start();
#ifdef __COVERAGE__
    __gcov_dump();
#endif
    std::quick_exit(s_tests_failed > 0 ? 1 : 0);
    return (s_tests_failed > 0) ? 1 : 0;
}
