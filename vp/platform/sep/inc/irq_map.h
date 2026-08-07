#pragma once

// =============================================================================
// SEP VP — PLIC configuration and interrupt source assignments
// Aligned with sep_internal_interrupts[] in silicon (source i+1 = interrupt[i])
// =============================================================================

const unsigned int PLIC_NUM_INTERRUPTS = 32;

// Silicon-aligned interrupt sources (sep_internal_interrupts[i] → PLIC source i+1):
const unsigned int DMA_DONE_IRQ        = 1;   // sep_internal_interrupts[0]
const unsigned int MAILBOX_IRQ0        = 2;   //   [1] sep_mailbox_interrupt
const unsigned int SPI_EVENT_IRQ       = 3;   //   [2] spi_irq_i
const unsigned int DMA_CHUNK_DONE_IRQ  = 4;   //   [3]
const unsigned int DMA_ERROR_IRQ       = 5;   //   [4]
const unsigned int KEYMGR_IRQ          = 6;   //   [5] km_mbox_irq
const unsigned int ENTROPY_SRC_IRQ     = 7;   //   [6] entropy_source_irq
//                                     = 8;   //   [7] ext_trng_irq (no VP model)
const unsigned int HMAC_DONE_IRQ       = 9;   //   [8]
const unsigned int HMAC_FIFO_EMPTY_IRQ = 10;  //   [9]
const unsigned int HMAC_HMAC_ERR_IRQ   = 11;  //   [10]
const unsigned int KMAC_IRQ            = 12;  //   [11] intr_kmac_done
//                                     = 13;  //   [12] intr_kmac_fifo_empty (no VP model)
//                                     = 14;  //   [13] intr_kmac_err (no VP model)
const unsigned int CS_CMD_REQ_DONE     = 15;  //   [14]
const unsigned int CS_ENTROPY_REQ      = 16;  //   [15]
const unsigned int CS_HW_INST_EXC      = 17;  //   [16]
const unsigned int CS_FATAL_ERR        = 18;  //   [17]
const unsigned int EDN_CMD_REQ_DONE    = 19;  //   [18]
const unsigned int EDN_FATAL_ERR       = 20;  //   [19]
const unsigned int OTBN_IRQ            = 21;  //   [20] intr_otbn_done

// VP-only interrupt sources — no silicon equivalent, placed past silicon range:
const unsigned int MAILBOX_IRQ1        = 22;  // second mailbox channel
const unsigned int AON_WKUP_IRQ        = 28;
const unsigned int AON_WDOG_IRQ        = 29;
const unsigned int SPI_ERROR_IRQ       = 31;
