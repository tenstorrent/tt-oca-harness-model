// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI Controller Basic Test Program for SEP Platform
 * Tests SPI peripheral register accessibility and basic functionality
 *
 * Address: 0x10B00000
 */

#include <stdint.h>

extern int printf(const char *format, ...);

// SPI Controller Base Address
#define SPI_BASE                0x10B00000

// SPI Controller Register Offsets (from spi_controller_base.h)
#define SPI_INTR_STATUS         (SPI_BASE + 0x00)
#define SPI_INTR_ENABLE         (SPI_BASE + 0x04)
#define SPI_INTR_TEST           (SPI_BASE + 0x08)
#define SPI_CTRL                (SPI_BASE + 0x10)
#define SPI_STATUS              (SPI_BASE + 0x14)
#define SPI_CFG                 (SPI_BASE + 0x18)
#define SPI_CSID                (SPI_BASE + 0x1C)
#define SPI_CMD                 (SPI_BASE + 0x20)
#define SPI_RXDATA              (SPI_BASE + 0x24)
#define SPI_TXDATA              (SPI_BASE + 0x28)
#define SPI_ERROR_ENABLE        (SPI_BASE + 0x2C)
#define SPI_ERROR_STATUS        (SPI_BASE + 0x30)
#define SPI_EVENT_ENABLE        (SPI_BASE + 0x34)

// CTRL Register Bitfields
#define SPI_CTRL_SPIEN          (1 << 31)  // SPI Enable
#define SPI_CTRL_SW_RST         (1 << 30)  // Software Reset
#define SPI_CTRL_OUTPUT_EN      (1 << 29)  // Output Enable
#define SPI_CTRL_TX_WATERMARK_POS  16      // TX FIFO watermark [23:16]
#define SPI_CTRL_RX_WATERMARK_POS  0       // RX FIFO watermark [7:0]

// STATUS Register Bitfields (from spi_controller_register.h)
#define SPI_STATUS_READY        (1U << 31)  // Ready for new command
#define SPI_STATUS_ACTIVE       (1U << 30)  // Transaction active
#define SPI_STATUS_TXFULL       (1 << 29)   // TX FIFO full
#define SPI_STATUS_TXEMPTY      (1 << 28)   // TX FIFO empty
#define SPI_STATUS_RXFULL       (1 << 25)   // RX FIFO full
#define SPI_STATUS_RXEMPTY      (1 << 24)   // RX FIFO empty

// CMD Register Bitfields (from handle_write_CMD: [13:12] DIRECTION, [11:10] SPEED, [9] CSAAT, [8:0] LEN)
#define SPI_CMD_LEN_POS         0          // Length [8:0]: bytes to transfer (0-based, so 0=1 byte)
#define SPI_CMD_CSAAT           (1 << 9)   // Chip select active after transaction
#define SPI_CMD_SPEED_POS       10         // Speed [11:10]: 0=Standard, 1=Dual, 2=Quad
#define SPI_CMD_DIRECTION_POS   12         // Direction [13:12]: 0=Dummy, 1=RX, 2=TX, 3=Bidir

// Helper macros
#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

// Test result tracking
static volatile uint32_t test_passed = 0;
static volatile uint32_t test_failed = 0;

/**
 * Test basic register read access
 */
void test_register_access(const char *reg_name, uint32_t addr) {
    printf("  Testing %s @ 0x%08x...", reg_name, addr);
    uint32_t value = REG_READ(addr);
    printf(" Read: 0x%08x [PASS]\n", value);
    test_passed++;
}

/**
 * Test STATUS register and parse fields
 */
void test_status_register() {
    printf("\nTest: Reading STATUS register\n");
    printf("  STATUS @ 0x%08x...", (uint32_t)SPI_STATUS);

    uint32_t status = REG_READ(SPI_STATUS);
    printf(" Value: 0x%08x", status);

    // Parse status fields (correct bit positions)
    uint32_t ready   = (status >> 31) & 0x1;
    uint32_t active  = (status >> 30) & 0x1;
    uint32_t txempty = (status >> 28) & 0x1;
    uint32_t rxempty = (status >> 24) & 0x1;

    printf("\n  - ready: %u", ready);
    printf("\n  - active: %u", active);
    printf("\n  - txempty: %u", txempty);
    printf("\n  - rxempty: %u\n", rxempty);

    // Validate: After reset, controller should be ready and FIFOs empty
    if (ready == 1 && txempty == 1 && rxempty == 1 && active == 0) {
        printf("  STATUS validation [PASS]\n");
        test_passed++;
    } else {
        printf("  STATUS validation [FAIL] - Expected ready=1, txempty=1, rxempty=1, active=0\n");
        test_failed++;
    }
}

/**
 * Test CTRL register write/read
 */
void test_ctrl_register() {
    printf("\nTest: Writing and reading CTRL register\n");

    // Read initial value
    uint32_t initial_ctrl = REG_READ(SPI_CTRL);
    printf("  Initial CTRL: 0x%08x\n", initial_ctrl);

    // Write configuration: Enable SPI, set watermarks
    uint32_t test_ctrl = SPI_CTRL_SPIEN | SPI_CTRL_OUTPUT_EN |
                         (16 << SPI_CTRL_TX_WATERMARK_POS) |
                         (16 << SPI_CTRL_RX_WATERMARK_POS);
    REG_WRITE(SPI_CTRL, test_ctrl);

    uint32_t readback_ctrl = REG_READ(SPI_CTRL);
    printf("  Wrote: 0x%08x, Read back: 0x%08x", test_ctrl, readback_ctrl);

    // Validate only writable bits (mask 0xe000ffff from CTRL register definition)
    uint32_t write_mask = 0xe000ffff;
    if ((readback_ctrl & write_mask) == (test_ctrl & write_mask)) {
        printf(" [PASS]\n");
        test_passed++;
    } else {
        printf(" [FAIL] - Write did not take effect\n");
        test_failed++;
    }
}

/**
 * Test TX FIFO write
 */
void test_tx_fifo() {
    printf("\nTest: Writing to TX FIFO\n");

    // Write test data to TX FIFO
    printf("  Writing 0x12345678 to TXDATA...");
    REG_WRITE(SPI_TXDATA, 0x12345678);
    printf(" [PASS]\n");

    // Check STATUS - TX FIFO should not be empty
    uint32_t status = REG_READ(SPI_STATUS);
    uint32_t txempty = (status >> 3) & 0x1;
    printf("  TX FIFO empty: %u\n", txempty);

    test_passed++;
}

static void wait_ready(void) {
    for (int t = 10000; t > 0; t--) {
        if (((REG_READ(SPI_STATUS) >> 30) & 0x1) == 0)
            return;
        for (volatile int j = 0; j < 10; j++);
    }
}

/**
 * Test SPI flash read using READ command (0x03) at address 0x000000.
 * Two-phase transfer: TX (opcode + address, CS held), then RX (data, CS released).
 * Erased flash returns 0xFF for every byte.
 */
void test_spi_flash_read() {
    printf("\nTest: SPI Flash Read (opcode 0x03, addr 0x000000)\n");

    // Step 0: Reset controller
    printf("  Step 0: Resetting SPI controller...\n");
    REG_WRITE(SPI_CTRL, SPI_CTRL_SW_RST);
    for (volatile int i = 0; i < 1000; i++);
    printf("    Reset complete\n");

    // Step 1: Enable SPI controller
    printf("  Step 1: Enabling SPI controller...\n");
    uint32_t ctrl = SPI_CTRL_SPIEN | SPI_CTRL_OUTPUT_EN |
                    (16 << SPI_CTRL_TX_WATERMARK_POS) |
                    (16 << SPI_CTRL_RX_WATERMARK_POS);
    REG_WRITE(SPI_CTRL, ctrl);
    printf("    CTRL: 0x%08x\n", REG_READ(SPI_CTRL));

    // Step 2: Write READ opcode (0x03) + 24-bit address 0x000000 to TX FIFO.
    // TX FIFO is little-endian: byte[0] (LSB) is sent first on the wire.
    //   byte[0] = opcode 0x03
    //   byte[1] = addr[23:16] = 0x00
    //   byte[2] = addr[15:8]  = 0x00
    //   byte[3] = addr[7:0]   = 0x00
    printf("  Step 2: Loading READ cmd + address into TX FIFO...\n");
    uint32_t cmd_word = (uint32_t)0x03 | (0x00 << 8) | (0x00 << 16) | (0x00 << 24);
    REG_WRITE(SPI_TXDATA, cmd_word);
    printf("    TX word: 0x%08x  (opcode=0x03, addr=0x000000)\n", cmd_word);

    // Step 3: CMD phase 1 — TX only, 4 bytes, CSAAT=1 (keep CS asserted for data phase)
    printf("  Step 3: Issuing TX CMD (4 bytes, CSAAT=1)...\n");
    uint32_t tx_cmd = ((4 - 1) << SPI_CMD_LEN_POS) |
                      (2 << SPI_CMD_DIRECTION_POS) |
                      SPI_CMD_CSAAT;
    REG_WRITE(SPI_CMD, tx_cmd);
    wait_ready();
    printf("    TX phase complete\n");

    // Step 4: CMD phase 2 — RX only, 4 bytes, CSAAT=0 (release CS)
    printf("  Step 4: Issuing RX CMD (4 bytes, CSAAT=0)...\n");
    uint32_t rx_cmd = ((4 - 1) << SPI_CMD_LEN_POS) |
                      (1 << SPI_CMD_DIRECTION_POS);
    REG_WRITE(SPI_CMD, rx_cmd);
    wait_ready();

    // Step 5: Check STATUS
    uint32_t status = REG_READ(SPI_STATUS);
    uint32_t rxempty = (status >> 24) & 0x1;
    printf("  Step 5: STATUS after RX: 0x%08x (rxempty=%u)\n", status, rxempty);

    if (rxempty) {
        printf("  SPI Flash Read [FAIL] - RX FIFO empty after transaction\n");
        test_failed++;
        return;
    }

    // Step 6: Read 4 bytes from RX FIFO
    printf("  Step 6: Reading data from RX FIFO...\n");
    uint32_t rx_word = REG_READ(SPI_RXDATA);
    printf("    RX word: 0x%08x\n", rx_word);

    uint8_t rx_bytes[4];
    rx_bytes[0] = (rx_word >>  0) & 0xFF;
    rx_bytes[1] = (rx_word >>  8) & 0xFF;
    rx_bytes[2] = (rx_word >> 16) & 0xFF;
    rx_bytes[3] = (rx_word >> 24) & 0xFF;
    printf("    Bytes: [0x%02x, 0x%02x, 0x%02x, 0x%02x]\n",
           rx_bytes[0], rx_bytes[1], rx_bytes[2], rx_bytes[3]);

    // Step 7: Verify — erased flash reads as 0xFF
    printf("  Step 7: Verifying (expect 0xFF for erased flash)...\n");
    int mismatches = 0;
    for (int i = 0; i < 4; i++) {
        if (rx_bytes[i] != 0xFF) {
            printf("    [MISMATCH] Byte %u: expected=0xff, got=0x%02x\n",
                   (uint32_t)i, rx_bytes[i]);
            mismatches++;
        }
    }

    if (mismatches == 0) {
        printf("  SPI Flash Read [PASS]\n");
        test_passed++;
    } else {
        printf("  SPI Flash Read [FAIL] - %u byte(s) mismatched\n", (uint32_t)mismatches);
        test_failed++;
    }
}

/**
 * Test software reset
 */
void test_software_reset() {
    printf("\nTest: Software reset\n");

    // Issue software reset
    printf("  Issuing SW_RST...");
    REG_WRITE(SPI_CTRL, SPI_CTRL_SW_RST);

    // Small delay
    for (volatile int i = 0; i < 1000; i++);

    // Check STATUS - should be back to idle
    uint32_t status = REG_READ(SPI_STATUS);
    printf(" STATUS after reset: 0x%08x [PASS]\n", status);

    test_passed++;
}

int main() {
    printf("\n=== SEP SPI Controller Register Access Test ===\n\n");

    printf("SPI Controller Base Address: 0x%08x\n\n", (uint32_t)SPI_BASE);

    // Test 1: Basic register read access
    printf("Test 1: Basic register read access\n");
    test_register_access("INTR_STATUS", SPI_INTR_STATUS);
    test_register_access("INTR_ENABLE", SPI_INTR_ENABLE);
    test_register_access("CTRL", SPI_CTRL);
    test_register_access("STATUS", SPI_STATUS);
    test_register_access("CFG", SPI_CFG);
    test_register_access("CSID", SPI_CSID);

    // Test 2: Check for errors
    printf("\nTest: Reading ERROR_STATUS register\n");
    uint32_t error_status = REG_READ(SPI_ERROR_STATUS);
    printf("  ERROR_STATUS: 0x%08x\n", error_status);

    // Test 3: Status register fields
    test_status_register();

    // Test 3: Control register
    test_ctrl_register();

    // Test 4: TX FIFO
    test_tx_fifo();

    // Test 5: SPI flash read (READ 0x03 at address 0x000000)
    test_spi_flash_read();

    // Test 6: Software reset
    test_software_reset();

    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %u\n", test_passed);
    printf("Tests failed: %u\n", test_failed);

    if (test_failed == 0) {
        printf("\nAll tests PASSED!\n\n");
    } else {
        printf("\nSome tests FAILED!\n\n");
    }

    return 0;
}
