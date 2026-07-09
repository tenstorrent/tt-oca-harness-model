/*
 * SPI-controller -> secure-DMA RX streaming integration test for the SEP platform.
 *
 * This is the integration counterpart to the per-model unit suites (which test the
 * SPI controller and the secure DMA independently, each against a mock of the other).
 * Here the REAL spi_controller and REAL secure_dma models are wired together on the
 * platform and driven end-to-end, exactly the way the boot ROM drives them:
 *
 *   for each read:
 *     CTRL.SW_RST                       flush the controller
 *     arm the secure DMA (hardware handshake): SRC = fixed SPI RXDATA FIFO,
 *                                              DST = incrementing SRAM buffer,
 *                                              CHUNK = RX watermark, GO|INITIAL|HSHAKE
 *     TX  opcode(0x03)+24-bit address    (CSAAT held)
 *     RX  CSAAT-chained data segments    (<= RX-FIFO sized; last releases CS)
 *     poll DMA STATUS.done
 *
 * The controller streams the RX data while the DMA drains one chunk per RX-watermark
 * trigger (SPI dma_trigger -> DMA lsio_trigger[0]); the controller stalls SCK when the
 * FIFO fills, so an over-FIFO segment cannot overflow. Running SEVERAL such reads with a
 * SW_RST between each is the exact pattern that exposed the "second read drops its
 * opcode+address TX command" defect: a stale post-process pop after the SW_RST cleared
 * the command queue mid-flight. If that defect regresses, reads after the first return
 * undriven flash data and the byte check below fails.
 *
 * Data oracle: the staged flash image is byte[i] = i & 0xFF (see gen_flash_fixture.py),
 * so a correct DMA of `len` bytes from flash offset `off` lands (off+k) & 0xFF at
 * buffer[k]. This validates addressing, ordering, and completeness in one check; a
 * dropped read (undriven 0xFF) or a mis-drained chunk mismatches immediately.
 *
 * Self-contained: all register addresses/fields are hardcoded below (matching the SEP
 * platform memory map), so the test builds and runs with only the RISC-V toolchain and
 * the in-tree sep-vp — no dependency on the tt-oca-hw hardware repo or its headers.
 */

#include <stdint.h>

extern int printf(const char *format, ...);

/* ------------------------------------------------------------------------- */
/* Register map (SEP platform)                                               */
/* ------------------------------------------------------------------------- */

/* SPI mux control (stub): select the SPI leg, release the forced chip-select. */
#define SPI_MUX_CTRL            0x20000000u
#define SPI_MUX_SPI_SEL         (1u << 0)   /* spi_sel = 1 */
/* cs_force_high (bit 1) left 0 */

/* OpenTitan spi_host controller. */
#define SPI_BASE                0x10B00000u
#define SPI_CTRL                (SPI_BASE + 0x10u)
#define SPI_STATUS              (SPI_BASE + 0x14u)
#define SPI_CMD                 (SPI_BASE + 0x20u)
#define SPI_RXDATA              (SPI_BASE + 0x24u)
#define SPI_TXDATA              (SPI_BASE + 0x28u)
#define SPI_EVENT_ENABLE        (SPI_BASE + 0x34u)

#define SPI_CTRL_SPIEN          (1u << 31)
#define SPI_CTRL_SW_RST         (1u << 30)
#define SPI_CTRL_OUTPUT_EN      (1u << 29)
#define SPI_CTRL_RX_WM_POS      0u          /* RX_WATERMARK = CTRL[7:0]  (words) */
#define SPI_CTRL_TX_WM_POS      8u          /* TX_WATERMARK = CTRL[15:8] (words) */

#define SPI_STATUS_READY        (1u << 31)
#define SPI_STATUS_ACTIVE       (1u << 30)

#define SPI_CMD_LEN_POS         0u          /* LEN = bytes - 1 */
#define SPI_CMD_CSAAT           (1u << 9)
#define SPI_CMD_DIR_POS         12u         /* 1 = RX, 2 = TX */
#define SPI_CMD_DIR_RX          1u
#define SPI_CMD_DIR_TX          2u

#define SPI_EVENT_ENABLE_RXWM   (1u << 8)   /* drives the RX-watermark DMA trigger */

/* secure_dma (OpenTitan DMA). */
#define DMA_BASE                0x10800000u
#define DMA_SRC_ADDR_LO         (DMA_BASE + 0x10u)
#define DMA_SRC_ADDR_HI         (DMA_BASE + 0x14u)
#define DMA_DST_ADDR_LO         (DMA_BASE + 0x18u)
#define DMA_DST_ADDR_HI         (DMA_BASE + 0x1Cu)
#define DMA_ADDR_SPACE_ID       (DMA_BASE + 0x20u)
#define DMA_RANGE_BASE          (DMA_BASE + 0x24u)
#define DMA_RANGE_LIMIT         (DMA_BASE + 0x28u)
#define DMA_RANGE_VALID         (DMA_BASE + 0x2Cu)
#define DMA_TOTAL_DATA_SIZE     (DMA_BASE + 0x38u)
#define DMA_CHUNK_DATA_SIZE     (DMA_BASE + 0x3Cu)
#define DMA_TRANSFER_WIDTH      (DMA_BASE + 0x40u)
#define DMA_CONTROL             (DMA_BASE + 0x44u)
#define DMA_SRC_CONFIG          (DMA_BASE + 0x48u)
#define DMA_DST_CONFIG          (DMA_BASE + 0x4Cu)
#define DMA_STATUS              (DMA_BASE + 0x50u)
#define DMA_ERROR_CODE          (DMA_BASE + 0x54u)
#define DMA_HANDSHAKE_INTR_EN   (DMA_BASE + 0x98u)

#define DMA_CTRL_GO             (1u << 31)
#define DMA_CTRL_INITIAL        (1u << 8)
#define DMA_CTRL_HSHAKE         (1u << 4)   /* hardware_handshake_enable */
#define DMA_STATUS_DONE         (1u << 1)
#define DMA_STATUS_ERROR        (1u << 3)

#define DMA_ASID_OT             0x77u       /* src_asid=7, dst_asid=7 */
#define DMA_WIDTH_4B            0x2u        /* 4-byte transfer width */
#define DMA_SRC_FIXED           0x2u        /* wrap set, increment clear -> fixed src */
#define DMA_DST_INCR            0x1u        /* increment -> walk the destination */

/* SEP SRAM window (destination region + the DMA's enforced bound). */
#define SRAM_BASE               0x10000000u
#define SRAM_LIMIT              0x1003FFFFu

/* ------------------------------------------------------------------------- */
/* Test parameters                                                           */
/* ------------------------------------------------------------------------- */

#define RX_WATERMARK_WORDS      4u                       /* RX-FIFO watermark, words */
#define CHUNK_BYTES             (RX_WATERMARK_WORDS * 4u) /* one DMA chunk = 16 bytes */
#define RX_SEG_MAX              256u                     /* RX FIFO sized segment cap */
#define XFER_BYTES              512u                     /* bytes per DMA transfer */
#define NUM_XFERS               4u

/* Flash offsets to read, one per transfer. Chosen so their low bytes differ (0x40,
 * 0x60, 0x80, 0xA0) -> the expected data pattern differs per transfer, so a dropped
 * read cannot be masked by stale buffer contents from the previous transfer. */
static const uint32_t g_flash_off[NUM_XFERS] = { 0x040u, 0x260u, 0x480u, 0x6A0u };

#define DMA_POLL_MAX            2000000u

#define REG_READ(a)             (*((volatile uint32_t *)(uintptr_t)(a)))
#define REG_WRITE(a, v)         (*((volatile uint32_t *)(uintptr_t)(a)) = (uint32_t)(v))

/* One DMA destination buffer per transfer, in SRAM (.bss), 4-byte aligned. Separate
 * buffers let us defer verification to a second pass so the reads themselves run
 * back-to-back with minimal work between them (see the two-phase note in main). */
static volatile uint8_t g_bufs[NUM_XFERS][XFER_BYTES] __attribute__((aligned(4)));

/* Per-transfer DMA completion state captured during the read phase. */
static uint32_t g_dma_done[NUM_XFERS];
static uint32_t g_dma_status[NUM_XFERS];
static uint32_t g_dma_errcode[NUM_XFERS];

static uint32_t g_pass = 0;
static uint32_t g_fail = 0;

/* Bring the controller up: flush via SW_RST, then enable with the RX watermark set
 * and the RX-watermark event enabled (that event drives the DMA trigger). */
static void spi_bringup(void)
{
    REG_WRITE(SPI_CTRL, SPI_CTRL_SW_RST);
    for (volatile int i = 0; i < 200; i++) { }
    REG_WRITE(SPI_CTRL, SPI_CTRL_SPIEN | SPI_CTRL_OUTPUT_EN |
                        (RX_WATERMARK_WORDS << SPI_CTRL_RX_WM_POS));
    REG_WRITE(SPI_EVENT_ENABLE, SPI_EVENT_ENABLE_RXWM);
}

/* Arm the secure DMA for a hardware-handshake drain of `len` bytes from the fixed SPI
 * RXDATA FIFO into `dst`, one CHUNK_BYTES chunk per RX-watermark trigger. */
static void dma_arm(uint32_t dst, uint32_t len)
{
    /* Bound the DMA's memory accesses to the SRAM window (independent hardware guard). */
    REG_WRITE(DMA_RANGE_BASE,  SRAM_BASE);
    REG_WRITE(DMA_RANGE_LIMIT, SRAM_LIMIT);
    REG_WRITE(DMA_RANGE_VALID, 1u);

    REG_WRITE(DMA_SRC_ADDR_LO, SPI_RXDATA);   /* fixed source: the RX FIFO register */
    REG_WRITE(DMA_SRC_ADDR_HI, 0u);
    REG_WRITE(DMA_DST_ADDR_LO, dst);          /* incrementing destination in SRAM */
    REG_WRITE(DMA_DST_ADDR_HI, 0u);
    REG_WRITE(DMA_ADDR_SPACE_ID, DMA_ASID_OT);
    REG_WRITE(DMA_TRANSFER_WIDTH, DMA_WIDTH_4B);
    REG_WRITE(DMA_SRC_CONFIG, DMA_SRC_FIXED);
    REG_WRITE(DMA_DST_CONFIG, DMA_DST_INCR);
    REG_WRITE(DMA_TOTAL_DATA_SIZE, len);
    REG_WRITE(DMA_CHUNK_DATA_SIZE, CHUNK_BYTES);
    REG_WRITE(DMA_HANDSHAKE_INTR_EN, 1u);

    REG_WRITE(DMA_CONTROL, DMA_CTRL_GO | DMA_CTRL_INITIAL | DMA_CTRL_HSHAKE);
}

/* Issue the flash read command frame: opcode 0x03 + 24-bit address (MSB first) as a TX
 * segment with CS held, then CSAAT-chained RX data segments totalling `len` bytes. */
static void spi_issue_read(uint32_t off, uint32_t len)
{
    /* opcode + 3 address bytes (MSB first), packed little-endian into one TX word. */
    uint32_t hdr = 0x03u
                 | (((off >> 16) & 0xFFu) << 8)
                 | (((off >> 8) & 0xFFu) << 16)
                 | ((off & 0xFFu) << 24);
    REG_WRITE(SPI_TXDATA, hdr);
    REG_WRITE(SPI_CMD, ((4u - 1u) << SPI_CMD_LEN_POS) |
                       (SPI_CMD_DIR_TX << SPI_CMD_DIR_POS) | SPI_CMD_CSAAT);

    uint32_t remaining = len;
    while (remaining > 0u) {
        uint32_t seg = (remaining > RX_SEG_MAX) ? RX_SEG_MAX : remaining;
        uint32_t last = ((remaining - seg) == 0u);
        uint32_t cmd = ((seg - 1u) << SPI_CMD_LEN_POS) | (SPI_CMD_DIR_RX << SPI_CMD_DIR_POS);
        if (!last) {
            cmd |= SPI_CMD_CSAAT;
        }
        REG_WRITE(SPI_CMD, cmd);
        remaining -= seg;
    }
}

/* PHASE 1 (per transfer): SW_RST-frame the read, arm the DMA, issue the command frame,
 * and wait for the DMA to drain. Deliberately no printf/verify here — see main(). */
static void issue_transfer(uint32_t idx, uint32_t off, uint32_t len)
{
    spi_bringup();                 /* SW_RST + enable (the reset each read frames with) */
    dma_arm((uint32_t)(uintptr_t)&g_bufs[idx][0], len);
    spi_issue_read(off, len);

    uint32_t st = 0;
    uint32_t done = 0;
    for (uint32_t i = 0; i < DMA_POLL_MAX; i++) {
        st = REG_READ(DMA_STATUS);
        if (st & (DMA_STATUS_DONE | DMA_STATUS_ERROR)) {
            done = (st & DMA_STATUS_DONE) ? 1u : 0u;
            break;
        }
    }
    g_dma_done[idx]    = done;
    g_dma_status[idx]  = st;
    g_dma_errcode[idx] = REG_READ(DMA_ERROR_CODE);
}

/* PHASE 2 (per transfer): check DMA completion and verify every drained byte against
 * the fixture oracle flash[abs] = abs & 0xFF. */
static void verify_transfer(uint32_t idx, uint32_t off, uint32_t len)
{
    printf("\n[Transfer %u] flash off=0x%04x len=%u chunk=%u\n",
           idx, off, len, (unsigned)CHUNK_BYTES);

    if (!g_dma_done[idx]) {
        printf("  [FAIL] DMA did not complete: STATUS=0x%08x ERROR_CODE=0x%08x\n",
               g_dma_status[idx], g_dma_errcode[idx]);
        g_fail++;
        return;
    }

    uint32_t mismatches = 0;
    uint32_t first_bad = 0;
    for (uint32_t k = 0; k < len; k++) {
        uint8_t want = (uint8_t)((off + k) & 0xFFu);
        if (g_bufs[idx][k] != want) {
            if (mismatches == 0u) {
                first_bad = k;
            }
            mismatches++;
        }
    }

    if (mismatches == 0u) {
        printf("  [PASS] %u bytes DMA-drained and verified (buf[0]=0x%02x buf[%u]=0x%02x)\n",
               len, g_bufs[idx][0], len - 1u, g_bufs[idx][len - 1u]);
        g_pass++;
    } else {
        uint8_t got = g_bufs[idx][first_bad];
        uint8_t exp = (uint8_t)((off + first_bad) & 0xFFu);
        printf("  [FAIL] %u/%u bytes mismatched; first at k=%u got=0x%02x want=0x%02x\n",
               mismatches, len, first_bad, got, exp);
        g_fail++;
    }
}

int main(void)
{
    printf("\n=== SEP SPI-controller -> secure-DMA streaming integration test ===\n");
    printf("Transfers=%u  bytes/transfer=%u  DMA chunk=%u  RX watermark=%u words\n",
           (unsigned)NUM_XFERS, (unsigned)XFER_BYTES, (unsigned)CHUNK_BYTES,
           (unsigned)RX_WATERMARK_WORDS);

    /* Select the SPI leg of the mux (release the forced chip-select). */
    REG_WRITE(SPI_MUX_CTRL, SPI_MUX_SPI_SEL);

    /* Two phases on purpose. The second-read TX-command drop only manifests when the
     * next read's CTRL.SW_RST lands while the PREVIOUS read is still in flight — the
     * controller pushes (and the DMA drains) the last segment's data BEFORE that
     * segment's bit-clock timing delay elapses, so DMA-done arrives with the FSM still
     * ACTIVE. Firmware gates the next read on DMA-done only, so the reset overlaps.
     *
     * To reproduce that overlap reliably we run all reads back-to-back with essentially
     * no work between them (Phase 1), then verify (Phase 2). Doing the per-transfer
     * printf/verify inline between reads would burn more simulated time than the tail
     * segment's delay, so the next SW_RST would land after the controller went idle and
     * the regression window would be missed entirely (a false PASS). */
    for (uint32_t t = 0; t < NUM_XFERS; t++) {
        issue_transfer(t, g_flash_off[t], XFER_BYTES);
    }
    for (uint32_t t = 0; t < NUM_XFERS; t++) {
        verify_transfer(t, g_flash_off[t], XFER_BYTES);
    }

    printf("\n=== Test Summary ===\n");
    printf("Transfers passed: %u\n", g_pass);
    printf("Transfers failed: %u\n", g_fail);
    if (g_fail == 0u && g_pass == NUM_XFERS) {
        printf("SEP_SPI_DMA_TEST: ALL PASS\n");
        printf("\nAll tests PASSED!\n\n");
    } else {
        printf("SEP_SPI_DMA_TEST: FAIL\n");
        printf("\nSome tests FAILED!\n\n");
    }
    return 0;
}
