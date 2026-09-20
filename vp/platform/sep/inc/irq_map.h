// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
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
//   sep_internal_interrupts[34]     = intr_abr_error;
//   sep_internal_interrupts[35]     = intr_abr_notif;
//   sep_internal_interrupts[36]     = entropy_pool_low;
//   sep_internal_interrupts[37]     = entropy_pool_fill_stall;
//
// NUM_INTERNAL_IRQS = 38 (sep_pkg.sv), so the internal sources fill PIC sources
// 1-38 and extintsrc_req takes 39-255. The external ones have no VP source yet:
// och_sep_ss drives no boundary port for them, so they stay tied low.
// =============================================================================

// Sized from the SEP VeeR snapshot's RV_PIC_TOTAL_INT_PLUS1 (common_defines.vh),
// i.e. pic_total_int=255 plus the reserved source 0. Sources 1-255 are usable;
// the platform leaves the ones it has no model for pointing at unused_irq_signal.
const unsigned int PIC_NUM_INTERRUPTS = 256;

// sep_mailbox_interrupt[7:0] → sources 1-8, one per mailbox channel. These are
// the inbound interrupts; the outbound ones go to the SMC, not to this PIC.
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
const unsigned int DMA_ALERT_IRQ       = 12;  //   [11] dma_alert
// Only an ALERT_TEST write can raise this. The IP's sole hardware alert source is
// intg_err_o from the register top, a TLUL command integrity error, and the VP is
// reached over AXI-Lite through the register model with no integrity-protected bus to fail.
const unsigned int WDT_ALERT_IRQ       = 13;  //   [12] wdt_alert
const unsigned int SPI_EVENT_IRQ       = 14;  //   [13] spi_irq_i
const unsigned int KEYMGR_IRQ          = 15;  //   [14] km_mbox_irq
const unsigned int ENTROPY_SRC_IRQ     = 16;  //   [15] entropy_source_irq
const unsigned int EXT_TRNG_IRQ        = 17;  //   [16] ext_trng_irq (no VP model)
const unsigned int HMAC_DONE_IRQ       = 18;  //   [17] intr_hmac_done
const unsigned int HMAC_FIFO_EMPTY_IRQ = 19;  //   [18] intr_hmac_fifo_empty
const unsigned int HMAC_HMAC_ERR_IRQ   = 20;  //   [19] intr_hmac_err
const unsigned int KMAC_DONE_IRQ       = 21;  //   [20] intr_kmac_done
const unsigned int KMAC_FIFO_EMPTY_IRQ = 22;  //   [21] intr_kmac_fifo_empty
const unsigned int KMAC_ERR_IRQ        = 23;  //   [22] intr_kmac_err
const unsigned int CS_CMD_REQ_DONE     = 24;  //   [23] intr_cs_cmd_req_done
const unsigned int CS_ENTROPY_REQ      = 25;  //   [24] intr_cs_entropy_req
const unsigned int CS_HW_INST_EXC      = 26;  //   [25] intr_cs_hw_inst_exc
const unsigned int CS_FATAL_ERR        = 27;  //   [26] intr_cs_fatal_err
const unsigned int EDN_CMD_REQ_DONE    = 28;  //   [27] intr_edn_cmd_req_done
const unsigned int EDN_FATAL_ERR       = 29;  //   [28] intr_edn_fatal_err
const unsigned int OTBN_IRQ            = 30;  //   [29] intr_otbn_done
const unsigned int KM_UNRECOVERABLE_ERR_IRQ = 31;  //   [30] km_unrecoverable_err (no VP model)
const unsigned int KM_RECOVERABLE_ERR_IRQ   = 32;  //   [31] km_recoverable_err (no VP model)
const unsigned int CRYPTO_ALERT_IRQ         = 33;  //   [32] crypto_alert (AES/HMAC/KMAC/OTBN/CSRNG/EDN OR)
const unsigned int LOCKED_FIELD_ACCESS_IRQ  = 34;  //   [33] locked_field_access_interrupt
const unsigned int ABR_ERROR_IRQ            = 35;  //   [34] intr_abr_error
const unsigned int ABR_NOTIF_IRQ            = 36;  //   [35] intr_abr_notif
const unsigned int ENTROPY_POOL_LOW_IRQ     = 37;  //   [36] entropy_pool_low (no VP model)
const unsigned int ENTROPY_POOL_STALL_IRQ   = 38;  //   [37] entropy_pool_fill_stall (no VP model)
