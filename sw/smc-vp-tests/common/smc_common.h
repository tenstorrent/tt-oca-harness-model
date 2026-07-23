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
#define PLIC_THRESHOLD(ctx)     (0x200000u + 0x1000u * (ctx))
#define PLIC_CLAIM_COMPLETE(ctx) (0x200004u + 0x1000u * (ctx))

/* Reset unit register offsets */
#define RESET_SS_CONFIG          0x20u
#define RESET_SS_WARM_RESET_N    0x44u
#define RESET_SS_CONFIG_LOCK     0x24u

/* CPU control register offsets */
#define CPU_CTRL_SCRATCH(idx)    (0x100u + 8u * (idx))

/* I2C controller register offsets (per core, 0x200-byte window) */
#define I2C_INTR_ENABLE          0x04u
#define I2C_CTRL                 0x10u
#define I2C_TIMING0              0x3Cu

/* I3C controller register offsets (per instance, 11-bit window) */
#define I3C_HCI_VERSION          0x00u
#define I3C_HC_CONTROL           0x04u
#define I3C_CONTROLLER_DEVICE_ADDR 0x08u
#define I3C_RESET_CONTROL          0x10u
#define I3C_PIO_CONTROL          0xACu

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

/* MMIO helpers */
#define REG_READ(addr)          (*((volatile uint32_t *)(uintptr_t)(addr)))
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(uintptr_t)(addr)) = (val))
#define REG_OR(addr, val)       REG_WRITE((addr), REG_READ((addr)) | (val))
#define REG_AND(addr, val)      REG_WRITE((addr), REG_READ((addr)) & (val))

#endif /* SMC_COMMON_H */
