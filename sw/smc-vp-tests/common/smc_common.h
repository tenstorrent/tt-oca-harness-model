/* SPDX-License-Identifier: Apache-2.0
 * sw/smc-vp-tests/common/smc_common.h
 *
 * SMC platform peripheral base addresses (local alias aperture
 * [0xC000_0000, 0xC100_0000)) and UART 16550 register offsets.
 *
 * Addresses mirror vp/platform/smc/src/smc_platform.cpp's address map.
 * The cluster's mmio window routes these through the fabric to the
 * modeled peripherals.
 */
#ifndef SMC_COMMON_H
#define SMC_COMMON_H

#include <stdint.h>

/* SMC platform address map (local alias aperture) */
#define SMC_WDT_DEBUG_BASE   0xC0000000ULL
#define SMC_WDT0_BASE        0xC0000000ULL
#define SMC_WDT1_BASE        0xC0000400ULL
#define SMC_WDT2_BASE        0xC0000800ULL
#define SMC_WDT3_BASE        0xC0000C00ULL
#define SMC_RESET_BASE       0xC0002000ULL
#define SMC_I3C_BASE         0xC0005000ULL
#define SMC_AVSBUS_BASE      0xC0008000ULL
#define SMC_I2C0_BASE        0xC0009000ULL
#define SMC_I2C1_BASE        0xC0009200ULL
#define SMC_I2C2_BASE        0xC0009400ULL
#define SMC_UART0_BASE       0xC000A000ULL
#define SMC_UART1_BASE       0xC000B000ULL
#define SMC_UART2_BASE       0xC000C000ULL
#define SMC_UART3_BASE       0xC000D000ULL
#define SMC_CPU_CTRL_BASE    0xC0400000ULL
#define SMC_CPU_CTRL_FP_BASE 0xC0039000ULL
#define SMC_DMA_BASE         0xC0038000ULL
#define SMC_BOOTROM_BASE     0xC0040000ULL
#define SMC_SCRATCH_BASE     0xC0060000ULL
#define SMC_PLIC_BASE        0xC0800000ULL
#define SMC_CLINT_BASE       0xC0C00000ULL
#define SMC_BEU_BASE         0xC0C10000ULL
#define SMC_ZEROER_BASE      0xC0038200ULL
#define SMC_PLL_WRAP_BASE     0xC0003000ULL

/* CLINT register offsets (RISC-V standard layout) */
#define CLINT_MSIP(hart)        (0x0000u + 4u * (hart))
#define CLINT_MTIMECMP(hart)    (0x4000u + 8u * (hart))
#define CLINT_MTIME_LO          0xBFF8u
#define CLINT_MTIME_HI          0xBFFCu

/* PLIC register offsets (RISC-V standard layout, 336 sources, 8 contexts) */
#define PLIC_PRIORITY(source)   (0x000000u + 4u * (source))
#define PLIC_ENABLE(ctx, word)  (0x002000u + 0x80u * (ctx) + 4u * (word))
#define PLIC_PENDING(word)      (0x001000u + 4u * (word))
#define PLIC_THRESHOLD(ctx)     (0x200000u + 0x1000u * (ctx))
#define PLIC_CLAIM_COMPLETE(ctx) (0x200004u + 0x1000u * (ctx))

/* Reset unit register offsets */
#define RESET_SS_CONFIG          0x20u
#define RESET_SS_CONFIG_LOCK     0x24u
#define RESET_SS_COLD_RESET_N    0x40u
#define RESET_SS_WARM_RESET_N    0x44u
#define RESET_SS_COLD_RESET_LOCK 0x70u

/* CPU control register offsets */
#define CPU_CTRL_SCRATCH(idx)         (0x100u + 8u * (idx))
#define CPU_CTRL_WDT_TIMEOUT          0x050u
#define CPU_CTRL_WDT_TIMEOUT_RESET    0x058u
#define CPU_CTRL_REFERENCE_COUNTER    0x060u
#define CPU_CTRL_MUTEX(idx)           (0x1040u + 8u * (idx))
#define CPU_CTRL_SEMA(idx)            (0x1060u + 8u * (idx))

/* SiFive TLWDT (stage-1) register offsets — window 0x400 per core */
#define WDT_CTRL           0x00u
#define WDT_COUNT          0x08u
#define WDT_SCALED_COUNT   0x10u
#define WDT_FEED           0x18u
#define WDT_KEY            0x1Cu
#define WDT_CMP            0x20u

#define WDT_KEY_MAGIC      0x0051F15Eu
#define WDT_FEED_MAGIC     0x0D09F00Du

#define WDT_CTRL_SCALE_MASK   0xFu
#define WDT_CTRL_RSTEN        (1u << 8u)
#define WDT_CTRL_ZEROCMP      (1u << 9u)
#define WDT_CTRL_ALWAYS       (1u << 12u)
#define WDT_CTRL_AWAKE        (1u << 13u)
#define WDT_CTRL_IP           (1u << 28u)

/* DMA controller register offsets (window size 0x138) */
#define DMA_CONFIG             0x000u
#define DMA_STATUS_0           0x004u
#define DMA_NEXT_ID_0          0x048u
#define DMA_DONE_0             0x0C8u
#define DMA_DST_ADDRESS_LO     0x108u
#define DMA_DST_ADDRESS_HI     0x10Cu
#define DMA_SRC_ADDRESS_LO     0x110u
#define DMA_SRC_ADDRESS_HI     0x114u
#define DMA_LENGTH_LO          0x118u
#define DMA_LENGTH_HI          0x11Cu
#define DMA_DST_STRIDE_LO      0x120u
#define DMA_DST_STRIDE_HI      0x124u
#define DMA_SRC_STRIDE_LO      0x128u
#define DMA_SRC_STRIDE_HI      0x12Cu
#define DMA_NUM_REPETITIONS_LO 0x130u
#define DMA_NUM_REPETITIONS_HI 0x134u

/* I2C controller register offsets (per core, 0x200-byte window) */
#define I2C_INTR_STATE            0x00u
#define I2C_INTR_ENABLE           0x04u
#define I2C_CTRL                  0x10u
#define I2C_STATUS                0x14u
#define I2C_RDATA                 0x18u
#define I2C_FDATA                 0x1Cu
#define I2C_FIFO_CTRL             0x20u
#define I2C_HOST_FIFO_STATUS      0x2Cu
#define I2C_TARGET_FIFO_STATUS    0x30u
#define I2C_TARGET_ID             0x54u
#define I2C_ACQDATA               0x58u
#define I2C_TXDATA                0x5Cu
#define I2C_CONTROLLER_EVENTS     0x78u

/* I2C CTRL bit fields */
#define I2C_CTRL_ENABLEHOST       (1u << 0u)
#define I2C_CTRL_ENABLETARGET     (1u << 1u)

/* I2C FDATA bit fields */
#define I2C_FDATA_START           (1u << 8u)
#define I2C_FDATA_STOP            (1u << 9u)
#define I2C_FDATA_READB           (1u << 10u)
#define I2C_FDATA_NAKOK           (1u << 12u)

/* I2C STATUS bit fields */
#define I2C_STATUS_HOSTIDLE       (1u << 3u)
#define I2C_STATUS_TARGETIDLE     (1u << 4u)
#define I2C_STATUS_FMTEMPTY       (1u << 2u)
#define I2C_STATUS_RXEMPTY        (1u << 5u)
#define I2C_STATUS_ACQEMPTY       (1u << 9u)
#define I2C_STATUS_TXEMPTY       (1u << 8u)

/* I2C FIFO_CTRL bit fields */
#define I2C_FIFO_CTRL_RXRST       (1u << 0u)
#define I2C_FIFO_CTRL_FMTRST      (1u << 1u)
#define I2C_FIFO_CTRL_ACQRST      (1u << 7u)
#define I2C_FIFO_CTRL_TXRST       (1u << 8u)

/* I3C controller register offsets (per instance, 11-bit window) */
#define I3C_HCI_VERSION          0x00u
#define I3C_HC_CONTROL           0x04u
#define I3C_CONTROLLER_DEVICE_ADDR 0x08u
#define I3C_RESET_CONTROL          0x10u
#define I3C_COMMAND_PORT         0x80u
#define I3C_RESPONSE_PORT        0x84u
#define I3C_XFER_DATA_PORT       0x88u
#define I3C_PIO_INTR_STATUS      0xA0u
#define I3C_PIO_CONTROL          0xACu
#define I3C_DAT_BASE             0x300u

/* I3C command-descriptor field positions */
#define I3C_CMD_ATTR_MASK        0x7u
#define I3C_CMD_TID_SHIFT        3u
#define I3C_CMD_RNW_SHIFT        7u
#define I3C_CMD_DEVIDX_SHIFT     8u
#define I3C_CMD_CP_SHIFT         15u
#define I3C_CMD_CCC_SHIFT        32u
#define I3C_CMD_LENGTH_SHIFT     48u

/* I3C HC_CONTROL / PIO_CONTROL bit fields */
#define I3C_HC_CONTROL_BUS_ENABLE  (1u << 31u)
#define I3C_PIO_CONTROL_ENABLE     (1u << 0u)

/* I3C PIO_INTR_STATUS bit fields */
#define I3C_PIO_INTR_TX_THLD       (1u << 0u)
#define I3C_PIO_INTR_RX_THLD       (1u << 1u)
#define I3C_PIO_INTR_RESP_READY    (1u << 4u)
#define I3C_PIO_INTR_TRANSFER_ERR  (1u << 9u)

/* UART PLIC source IDs (peripheral bits 18:21 -> PLIC sources 19:22) */
#define PLIC_SRC_UART0    19u
#define PLIC_SRC_UART1    20u
#define PLIC_SRC_UART2    21u
#define PLIC_SRC_UART3    22u

/* AVSBus PLIC source ID (peripheral bit 22 -> PLIC source 23) */
#define PLIC_SRC_AVSBUS   23u

/* AVSBus Controller — base 0xC000_8000, 4 KiB window, 32-bit registers.
 * Mirrors hw/ip/avsbus_controller RDL (AVSBus 1.3.1 single-target). */
#define AVS_CMD                    0x00u
#define AVS_READBACK               0x04u
#define AVS_DEBUG_READBACK         0x08u
#define AVS_LATEST_SLAVE_SUBFRAME  0x0Cu
#define AVS_NORMAL_STATUS          0x20u
#define AVS_SLAVE_STATUS           0x24u
#define AVS_FIFOS_STATUS           0x28u
#define AVS_INTERRUPT              0x30u
#define AVS_INTERRUPT_MASK         0x34u
#define AVS_INTERRUPT_CLEAR        0x38u
#define AVS_CFG_0                  0x50u
#define AVS_CFG_1                  0x54u
#define AVS_CONFIG                 0x58u

/* AVS_NORMAL_STATUS bits */
#define AVS_STATUS_CMD_FIFO_EMPTY      (1u << 17u)
#define AVS_STATUS_READBACK_HAS_DATA   (1u << 20u)
#define AVS_STATUS_BUS_IS_IDLE         (1u << 21u)

/* AVS_INTERRUPT bits */
#define AVS_IRQ_READBACK_HAS_DATA      (1u << 3u)

/* AVS_CMD field helpers (RDL layout) */
#define AVS_CMD_PACK(r_or_w, grp, code, rail, data) \
    ((((uint32_t)(r_or_w) & 0x3u) << 28) | \
     (((uint32_t)(grp)    & 0x1u) << 27) | \
     (((uint32_t)(code)   & 0xFu) << 23) | \
     (((uint32_t)(rail)   & 0xFu) << 19) | \
     (((uint32_t)(data)   & 0xFFFFu) << 3))
#define AVS_CMD_COMMIT_WRITE  0x0u
#define AVS_CMD_READ          0x2u
#define AVS_CMD_CODE_VOLTAGE  0x0u

/* Known reset values */
#define AVS_CFG_0_RESET       0x00051000u
#define AVS_CFG_1_RESET       0x00000003u
#define AVS_CONFIG_RESET      0x1u
#define AVS_IRQ_MASK_RESET    0x1FFu

/* SiFive TLWDT PLIC source IDs (Freedom Metal metal_watchdog_get_interrupt_id) */
#define PLIC_SRC_WDT0    329u
#define PLIC_SRC_WDT1    330u
#define PLIC_SRC_WDT2    331u
#define PLIC_SRC_WDT3    332u

/* UART 16550 register offsets (DLAB=0 unless noted) */
#define UART_RBR_THR_DLL  0x00   /* RBR (ro) / THR (wo) / DLL (DLAB=1) */
#define UART_IER_DLM      0x04   /* IER / DLM (DLAB=1) */
#define UART_IIR_FCR      0x08   /* IIR (ro) / FCR (wo) */
#define UART_LCR         0x0C   /* bit 7 = DLAB */
#define UART_MCR         0x10
#define UART_LSR         0x14   /* bit 5 = THRE, bit 0 = DR */
#define UART_SCR          0x1C

/* LSR bit masks */
#define UART_LSR_THRE     0x20   /* TX holding register empty */
#define UART_LSR_DR       0x01   /* RX data ready */

/* IER bit masks (DLAB=0) */
#define UART_IER_ERBFI    0x01   /* enable RX data ready interrupt */
#define UART_IER_ETBEI    0x02   /* enable TX holding register empty interrupt */
#define UART_IER_ELSI     0x04   /* enable RX line status interrupt */
#define UART_IER_EDSSI    0x08   /* enable modem status interrupt */
#define UART_IER_EFEI     0x10   /* enable FIFO error interrupt */

/* MCR bit masks */
#define UART_MCR_LOOP     0x10   /* internal loopback mode */

/* Bus Error Unit (BEU) — per-core alias, base 0xC0C1_0000, 4 KiB window
 * each (0xC0C1_0000 + N*0x1000, N = 0..NUM_HARTS-1).  Registers are 64-bit
 * (accesswidth=64 in bus_error_unit.rdl) — use REG_READ64 / REG_WRITE64.
 * Interrupts bypass the PLIC: irq_local_o feeds the core's NMI-like input
 * directly, irq_plic_o is wired to the PLIC vector for completeness but is
 * not used by firmware today (see hw/smc/doc/interrupts.adoc). */
#define SMC_BEU_BASE_N(n)   (SMC_BEU_BASE + 0x1000ULL * (n))

#define BEU_CAUSE           0x00u   /* rw  [2:0] latched error cause; write 0 to re-arm    */
#define BEU_PHYS_ADDR       0x08u   /* ro  [55:0] address of the latched error             */
#define BEU_ENABLE          0x10u   /* rw  [7:0] per-source recording enable (reset: all)  */
#define BEU_PLIC_ENABLE     0x18u   /* rw  [7:0] per-source PLIC interrupt mask (reset: 0) */
#define BEU_ACCRUED_ENABLE  0x20u   /* rw  [7:0] sticky accrued status; HW-set, SW-clears  */
#define BEU_LOCAL_ENABLE    0x28u   /* rw  [7:0] per-source local (NMI) interrupt mask (0) */

/* Only bits {1,2,5,6,7} are defined in every 8-bit field; {0,3,4} reserved. */
#define BEU_VALID_MASK      0xE6ull

/* Error source bit positions == CAUSE encoding; mirrors smc::beu_src
 * (smc/peripherals/beu/include/beu.h).  Firmware cannot drive these directly
 * (no live cache/TileLink ECC event exists in the VP); they document the
 * CAUSE values latched by the platform's test-only error-injection hook
 * (Phase D1 — see 04_BEU_Platform_Integration_Test_Plan.md and
 * smc-beu-error-test/). */
#define BEU_SRC_ICACHE_TLBUS          1u  /* itl_error */
#define BEU_SRC_ICACHE_CORRECTABLE    2u  /* iec_error */
#define BEU_SRC_DCACHE_TLBUS          5u  /* dtl_error */
#define BEU_SRC_DCACHE_CORRECTABLE    6u  /* dec_error */
#define BEU_SRC_DCACHE_UNCORRECTABLE  7u  /* deu_error */

/* memory_zeroer (AXI zeroer) — base 0xC003_8200, window 0x18.
 * NOTE: these are 64-bit registers and the model only accepts naturally
 * aligned 8-byte accesses — use REG_READ64 / REG_WRITE64, not REG_WRITE. */

#define ZEROER_DEST_ADDR         0x00u   /* RW: byte address to zero-fill      */
#define ZEROER_SIZE              0x08u   /* RW: number of bytes to zero        */
#define ZEROER_CTRL_STATUS       0x10u   /* RW int_en[0] / RO busy[32]; write  */
                                         /* triggers a job when SIZE != 0      */
#define ZEROER_CTRL_INT_EN       (1ull << 0u)   /* completion interrupt enable */
#define ZEROER_CTRL_BUSY         (1ull << 32u)  /* 1 while a job is running     */

/* PLL wrapper (pll_wrap.rdl) — base 0xC000_3000, window 0x1000.
 * Composed map (matches vp/platform/smc periph_router route "pll_wrap"):
 *   pll_cntl @0x000, cgm_0 @0x100, cgm_1 @0x200, awm_0 @0x400, awm_1 @0xA00.
 * The firmware (fw/smc) accesses cgm/awm and the pll_cntl status registers
 * with 16-bit MMIO, and the wide pll_cntl registers with 32-bit — use
 * REG_READ16/REG_WRITE16 for the former, REG_READ/REG_WRITE for the latter. */
#define PLL_CNTL_BASE         (SMC_PLL_WRAP_BASE + 0x000u)
#define PLL_CGM_BASE(id)      (SMC_PLL_WRAP_BASE + 0x100u + (id) * 0x100u)
#define PLL_AWM_BASE(id)      (SMC_PLL_WRAP_BASE + 0x400u + (id) * 0x600u)

/* pll_cntl register offsets (firmware-polled lock status lives here) */
#define PLL_CNTL_CGM_0_STATUS   0x00u   /* RO lock_detect[0]   */
#define PLL_CNTL_CGM_1_STATUS   0x04u   /* RO lock_detect[0]   */
#define PLL_CNTL_AWM_0_STATUS   0x14u   /* RO lock_detect[2:0] */
#define PLL_CNTL_AWM_1_STATUS   0x1Cu   /* RO lock_detect[2:0] */
#define PLL_CNTL_AG_MUX_SELECT  0x20u   /* RW 32-bit, mask 0x3F3FFFFF */

/* cgm register offsets (16-bit registers) */
#define CGM_ENABLES         0x00u   /* cgm_enable[0], freq_acq_enable[1] */
#define CGM_FCW_INT         0x04u
#define CGM_FCW_FRAC        0x08u
#define CGM_PREDIV          0x0Cu
#define CGM_REG_UPDATE      0x20u   /* WO self-clearing commit strobe */
#define CGM_CGM_STATUS      0x4Cu   /* RO lock_detect[0] */

/* awm GLOBAL register offsets */
#define AWM_GLOBAL_REG_UPDATE   0x28u   /* WO commit strobe */
#define AWM_GLOBAL_LOCK_STATUS  0x98u   /* RO lock_detect[5:3] */

/* PLL field bits */
#define CGM_ENABLE_BIT       (1u << 0u)
#define CGM_FREQ_ACQ_BIT     (1u << 1u)
#define PLL_LOCK_DETECT_BIT  (1u << 0u)

/* MMIO helpers */
#define REG_READ(addr)          (*((volatile uint32_t *)(uintptr_t)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(uintptr_t)(addr)) = (val))
#define REG_OR(addr, val)       REG_WRITE((addr), REG_READ((addr)) | (val))
#define REG_AND(addr, val)      REG_WRITE((addr), REG_READ((addr)) & (val))

/* 16-bit MMIO helpers (the PLL firmware accesses cgm/awm/pll_cntl status
 * registers as 16-bit; the pll_wrapper model supports sub-word access). */
#define REG_READ16(addr)        (*((volatile uint16_t *)(uintptr_t)(addr)))
#define REG_WRITE16(addr, val)  (*((volatile uint16_t *)(uintptr_t)(addr)) = (uint16_t)(val))

/* 64-bit MMIO helpers (required for the memory_zeroer register file) */
#define REG_READ64(addr)        (*((volatile uint64_t *)(uintptr_t)(addr)))
#define REG_WRITE64(addr, val)  (*((volatile uint64_t *)(uintptr_t)(addr)) = (val))

#endif /* SMC_COMMON_H */
