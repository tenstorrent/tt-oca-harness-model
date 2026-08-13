#pragma once

// =============================================================================
// SEP VP — PIC interrupt source assignments
// Exact mirror of sep_internal_interrupts[] in silicon (source i+1 = interrupt[i]).
// This file intentionally contains ONLY real silicon sources. VP-only signals
// that reuse an otherwise-unused mailbox slot (UART, AON wakeup/watchdog, SPI
// error) are NOT defined here — see och_sep_ss.hpp where they're wired, since
// that reuse is a VP-integration choice, not a hardware fact.
//
// sep_internal_interrupts in sep.sv:
//   sep_internal_interrupts         = '0;
//   sep_internal_interrupts[7:0]    = sep_mailbox_interrupt;
//   sep_internal_interrupts[8]      = intr_dma_done;
//   sep_internal_interrupts[9]      = intr_dma_chunk_done;
//   sep_internal_interrupts[10]     = intr_dma_error;
//   sep_internal_interrupts[11]     = dma_alert;
//   sep_internal_interrupts[12]     = wdt_alert;
//   sep_internal_interrupts[13]     = spi_irq_i;
//   sep_internal_interrupts[14]     = km_mbox_irq;
//   sep_internal_interrupts[15]     = entropy_source_irq;
//   sep_internal_interrupts[16]     = ext_trng_irq;
//   sep_internal_interrupts[17]     = intr_hmac_done;
//   sep_internal_interrupts[18]     = intr_hmac_fifo_empty;
//   sep_internal_interrupts[19]     = intr_hmac_err;
//   sep_internal_interrupts[20]     = intr_kmac_done;
//   sep_internal_interrupts[21]     = intr_kmac_fifo_empty;
//   sep_internal_interrupts[22]     = intr_kmac_err;
//   sep_internal_interrupts[23]     = intr_cs_cmd_req_done;
//   sep_internal_interrupts[24]     = intr_cs_entropy_req;
//   sep_internal_interrupts[25]     = intr_cs_hw_inst_exc;
//   sep_internal_interrupts[26]     = intr_cs_fatal_err;
//   sep_internal_interrupts[27]     = intr_edn_cmd_req_done;
//   sep_internal_interrupts[28]     = intr_edn_fatal_err;
//   sep_internal_interrupts[29]     = intr_otbn_done;
//   sep_internal_interrupts[30]     = km_unrecoverable_err;
//   sep_internal_interrupts[31]     = km_recoverable_err;
//   sep_internal_interrupts[32]     = crypto_alert;
//   sep_internal_interrupts[33]     = locked_field_access_interrupt;
//
// VP supports 32 PIC slots (indices 0-31, sources 0-31). Source 0 is reserved
// by VeeR EL2. Sources 32-34 (km_recoverable_err, crypto_alert,
// locked_field_access_interrupt) exceed that range and cannot be used as a
// live pic_inputs[]/irq_in[] index — their constants are for documentation
// only.
// =============================================================================

// Total number of interrupt sources (source 0 reserved by VeeR EL2; sources 1–31 usable)
const unsigned int PIC_NUM_INTERRUPTS = 32;

// sep_mailbox_interrupt[1:0] → sources 1-2 (VP models channels 0 and 1 only)
const unsigned int MAILBOX_IRQ0        = 1;   //   [0] sep_mailbox_interrupt[0]
const unsigned int MAILBOX_IRQ1        = 2;   //   [1] sep_mailbox_interrupt[1]
const unsigned int MAILBOX_IRQ2        = 3;   //   [2] sep_mailbox_interrupt[2]
const unsigned int MAILBOX_IRQ3        = 4;   //   [3] sep_mailbox_interrupt[3]
const unsigned int MAILBOX_IRQ4        = 5;   //   [4] sep_mailbox_interrupt[4]
const unsigned int MAILBOX_IRQ5        = 6;   //   [5] sep_mailbox_interrupt[5]
const unsigned int MAILBOX_IRQ6        = 7;   //   [6] sep_mailbox_interrupt[6]
const unsigned int MAILBOX_IRQ7        = 8;   //   [7] sep_mailbox_interrupt[7]
const unsigned int DMA_DONE_IRQ        = 9;   //   [8]  intr_dma_done
const unsigned int DMA_CHUNK_DONE_IRQ  = 10;  //   [9]  intr_dma_chunk_done
const unsigned int DMA_ERROR_IRQ       = 11;  //   [10] intr_dma_error
const unsigned int DMA_ALERT_IRQ       = 12;  //   [11] dma_alert (no VP model)
const unsigned int WDT_ALERT_IRQ       = 13;  //   [12] wdt_alert (no VP model)
const unsigned int SPI_EVENT_IRQ       = 14;  //   [13] spi_irq_i
const unsigned int KEYMGR_IRQ          = 15;  //   [14] km_mbox_irq
const unsigned int ENTROPY_SRC_IRQ     = 16;  //   [15] entropy_source_irq
const unsigned int EXT_TRNG_IRQ        = 17;  //   [16] ext_trng_irq (no VP model)
const unsigned int HMAC_DONE_IRQ       = 18;  //   [17] intr_hmac_done
const unsigned int HMAC_FIFO_EMPTY_IRQ = 19;  //   [18] intr_hmac_fifo_empty
const unsigned int HMAC_HMAC_ERR_IRQ   = 20;  //   [19] intr_hmac_err
const unsigned int KMAC_IRQ            = 21;  //   [20] intr_kmac_done
const unsigned int KMAC_FIFO_EMPTY_IRQ = 22;  //   [21] intr_kmac_fifo_empty (no VP model)
const unsigned int KMAC_ERR_IRQ        = 23;  //   [22] intr_kmac_err (no VP model)
const unsigned int CS_CMD_REQ_DONE     = 24;  //   [23] intr_cs_cmd_req_done
const unsigned int CS_ENTROPY_REQ      = 25;  //   [24] intr_cs_entropy_req
const unsigned int CS_HW_INST_EXC      = 26;  //   [25] intr_cs_hw_inst_exc
const unsigned int CS_FATAL_ERR        = 27;  //   [26] intr_cs_fatal_err
const unsigned int EDN_CMD_REQ_DONE    = 28;  //   [27] intr_edn_cmd_req_done
const unsigned int EDN_FATAL_ERR       = 29;  //   [28] intr_edn_fatal_err
const unsigned int OTBN_IRQ            = 30;  //   [29] intr_otbn_done
const unsigned int KM_UNRECOVERABLE_ERR_IRQ = 31;  //   [30] km_unrecoverable_err (no VP model)

// ---- Out of range for the VP's 32-slot PIC (source 0-31 only) ----
// These document the real silicon source number but MUST NOT be used to
// index pic_inputs[]/irq_in[] — doing so would be out-of-bounds.
const unsigned int KM_RECOVERABLE_ERR_IRQ  = 32;  //   [31] km_recoverable_err (out of range)
const unsigned int CRYPTO_ALERT_IRQ        = 33;  //   [32] crypto_alert (out of range)
const unsigned int LOCKED_FIELD_ACCESS_IRQ = 34;  //   [33] locked_field_access_interrupt (out of range)
