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
#define SMC_I2C0_BASE        0xC0009000ULL
#define SMC_I2C1_BASE        0xC0009200ULL
#define SMC_I2C2_BASE        0xC0009400ULL
#define SMC_UART0_BASE       0xC000A000ULL
#define SMC_UART1_BASE       0xC000B000ULL
#define SMC_UART2_BASE       0xC000C000ULL
#define SMC_UART3_BASE       0xC000D000ULL
#define SMC_CPU_CTRL_BASE    0xC0400000ULL
#define SMC_CPU_CTRL_FP_BASE 0xC0039000ULL
#define SMC_BOOTROM_BASE     0xC0040000ULL
#define SMC_SCRATCH_BASE     0xC0060000ULL
#define SMC_PLIC_BASE        0xC0800000ULL
#define SMC_CLINT_BASE       0xC0C00000ULL
#define SMC_BEU_BASE         0xC0C10000ULL

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

/* MMIO helpers */
#define REG_READ(addr)          (*((volatile uint32_t *)(uintptr_t)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(uintptr_t)(addr)) = (val))
#define REG_OR(addr, val)       REG_WRITE((addr), REG_READ((addr)) | (val))
#define REG_AND(addr, val)      REG_WRITE((addr), REG_READ((addr)) & (val))

#endif /* SMC_COMMON_H */
