/*
 * SPI Mux + Flash-Loader Test for the SEP Platform
 *
 * Exercises the two VP enablement changes:
 *   1. The OCH_SEP_SPI_MUX_CTRL register at 0x20001000 is an RW bus target: a store no
 *      longer faults, read-back returns what was written, and the pre-write read returns the
 *      seeded reset default 0x00000002 (cs_force_high=1, spi_sel=0).
 *   2. The SPI flash READ path returns bytes from the staged image. Built without
 *      -DFLASH_STAGED it asserts erased 0xFF (no image staged); built with -DFLASH_STAGED it
 *      asserts the known fixture bytes (image staged to data/sep_spi_mux_test_flash.bin and
 *      named by och_sep_ss1.spiBackdoorFile).
 */

#include <stdint.h>

extern int printf(const char *format, ...);

// ---------------------------------------------------------------------------
// SPI mux control register (OCH_SEP_SPI_MUX_CTRL)
// ---------------------------------------------------------------------------
#define SPI_MUX_BASE            0x20001000
#define SPI_MUX_CTRL            (SPI_MUX_BASE + 0x00)
#define SPI_MUX_RESET_VALUE     0x00000002u   // cs_force_high=1, spi_sel=0
#define SPI_MUX_SPI_SEL         (1u << 0)
#define SPI_MUX_CS_FORCE_HIGH   (1u << 1)

// ---------------------------------------------------------------------------
// SPI controller (OpenTitan spi_host) — used to read the flash (mirrors sep-spi-test)
// ---------------------------------------------------------------------------
#define SPI_BASE                0x10B00000
#define SPI_CTRL                (SPI_BASE + 0x10)
#define SPI_STATUS              (SPI_BASE + 0x14)
#define SPI_CMD                 (SPI_BASE + 0x20)
#define SPI_RXDATA              (SPI_BASE + 0x24)
#define SPI_TXDATA              (SPI_BASE + 0x28)

#define SPI_CTRL_SPIEN          (1u << 31)
#define SPI_CTRL_SW_RST         (1u << 30)
#define SPI_CTRL_OUTPUT_EN      (1u << 29)
#define SPI_CTRL_TX_WATERMARK_POS  16
#define SPI_CTRL_RX_WATERMARK_POS  0

#define SPI_CMD_LEN_POS         0
#define SPI_CMD_CSAAT           (1u << 9)
#define SPI_CMD_DIRECTION_POS   12

// Fixture bytes expected at flash offset 0 when an image is staged (bank-A manifest magic
// "MAN1"). Must match sw/sep-vp-tests/sep-spi-mux-test/gen_flash_fixture.py.
#define FIX_B0  0x4D
#define FIX_B1  0x41
#define FIX_B2  0x4E
#define FIX_B3  0x31

#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

static volatile uint32_t test_passed = 0;
static volatile uint32_t test_failed = 0;

static void wait_ready(void) {
    for (int t = 10000; t > 0; t--) {
        if (((REG_READ(SPI_STATUS) >> 30) & 0x1) == 0)
            return;
        for (volatile int j = 0; j < 10; j++);
    }
}

/*
 * The mux control register must be an RW target (no store fault) with the seeded reset
 * default readable before any write.
 */
static void test_spi_mux(void) {
    printf("\nTest: SPI mux control register (0x%08x)\n", (uint32_t)SPI_MUX_CTRL);

    // 1. Pre-write read returns the seeded reset default.
    uint32_t reset_val = REG_READ(SPI_MUX_CTRL);
    printf("  Reset read-back: 0x%08x (expect 0x%08x)...", reset_val, SPI_MUX_RESET_VALUE);
    if (reset_val == SPI_MUX_RESET_VALUE) { printf(" [PASS]\n"); test_passed++; }
    else                                  { printf(" [FAIL]\n"); test_failed++; }

    // 2. The driver's real first write: select SPI leg, clear forced-CS. Store must not fault.
    uint32_t sel = SPI_MUX_SPI_SEL;  // spi_sel=1, cs_force_high=0
    REG_WRITE(SPI_MUX_CTRL, sel);
    uint32_t rb = REG_READ(SPI_MUX_CTRL);
    printf("  Wrote 0x%08x, read back 0x%08x...", sel, rb);
    if (rb == sel) { printf(" [PASS]\n"); test_passed++; }
    else           { printf(" [FAIL]\n"); test_failed++; }

    // 3. Full-word storage across the register (proves plain RW backing, no field masking).
    uint32_t pat = 0xA5A55A5Au;
    REG_WRITE(SPI_MUX_CTRL, pat);
    uint32_t rb2 = REG_READ(SPI_MUX_CTRL);
    printf("  Wrote 0x%08x, read back 0x%08x...", pat, rb2);
    if (rb2 == pat) { printf(" [PASS]\n"); test_passed++; }
    else            { printf(" [FAIL]\n"); test_failed++; }

    // 4. Another offset in the window must also accept access without faulting.
    REG_WRITE(SPI_MUX_BASE + 0x4, 0x12345678u);
    uint32_t rb3 = REG_READ(SPI_MUX_BASE + 0x4);
    printf("  Window +0x4 read back 0x%08x...", rb3);
    if (rb3 == 0x12345678u) { printf(" [PASS]\n"); test_passed++; }
    else                    { printf(" [FAIL]\n"); test_failed++; }
}

/*
 * Read flash offset 0 via the controller (READ opcode 0x03) and check the bytes.
 * Staged build expects the fixture manifest magic; unstaged build expects erased 0xFF.
 */
static void test_flash_read(void) {
#ifdef FLASH_STAGED
    printf("\nTest: SPI flash read (staged image expected)\n");
    const uint8_t expect[4] = { FIX_B0, FIX_B1, FIX_B2, FIX_B3 };
#else
    printf("\nTest: SPI flash read (no image staged, expect erased 0xFF)\n");
    const uint8_t expect[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
#endif

    REG_WRITE(SPI_CTRL, SPI_CTRL_SW_RST);
    for (volatile int i = 0; i < 1000; i++);
    REG_WRITE(SPI_CTRL, SPI_CTRL_SPIEN | SPI_CTRL_OUTPUT_EN |
                        (16 << SPI_CTRL_TX_WATERMARK_POS) |
                        (16 << SPI_CTRL_RX_WATERMARK_POS));

    // READ opcode 0x03 + 24-bit address 0x000000 (little-endian in the TX FIFO word).
    REG_WRITE(SPI_TXDATA, (uint32_t)0x03);
    REG_WRITE(SPI_CMD, ((4 - 1) << SPI_CMD_LEN_POS) | (2 << SPI_CMD_DIRECTION_POS) | SPI_CMD_CSAAT);
    wait_ready();
    REG_WRITE(SPI_CMD, ((4 - 1) << SPI_CMD_LEN_POS) | (1 << SPI_CMD_DIRECTION_POS));
    wait_ready();

    uint32_t status = REG_READ(SPI_STATUS);
    if ((status >> 24) & 0x1) {  // rxempty
        printf("  [FAIL] RX FIFO empty after transaction (status=0x%08x)\n", status);
        test_failed++;
        return;
    }

    uint32_t rx = REG_READ(SPI_RXDATA);
    uint8_t b[4] = { (uint8_t)(rx & 0xFF), (uint8_t)((rx >> 8) & 0xFF),
                     (uint8_t)((rx >> 16) & 0xFF), (uint8_t)((rx >> 24) & 0xFF) };
    printf("  Flash[0..3] = [0x%02x, 0x%02x, 0x%02x, 0x%02x] (expect [0x%02x, 0x%02x, 0x%02x, 0x%02x])...",
           b[0], b[1], b[2], b[3], expect[0], expect[1], expect[2], expect[3]);

    int mismatch = 0;
    for (int i = 0; i < 4; i++) if (b[i] != expect[i]) mismatch++;
    if (mismatch == 0) { printf(" [PASS]\n"); test_passed++; }
    else               { printf(" [FAIL]\n"); test_failed++; }
}

int main(void) {
    printf("\n=== SEP SPI Mux + Flash-Loader Test ===\n");

    test_spi_mux();
    test_flash_read();

    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %u\n", test_passed);
    printf("Tests failed: %u\n", test_failed);
    if (test_failed == 0) printf("\nAll tests PASSED!\n\n");
    else                  printf("\nSome tests FAILED!\n\n");
    return 0;
}
