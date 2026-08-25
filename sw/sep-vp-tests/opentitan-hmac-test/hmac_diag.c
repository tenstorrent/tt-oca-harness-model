// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// Quick HMAC diagnostic test
#include "dif_hmac_sep.h"
#include "ottf_compat.h"
#include "base_compat.h"

int main(void) {
    uart_print("\n=== HMAC Diagnostic Test ===\n");
    
    // Initialize HMAC
    dif_hmac_t hmac;
    uart_print("[1] Initializing HMAC...\n");
    dif_result_t result = dif_hmac_init_from_dt(0, &hmac);
    if (result != kDifOk) {
        uart_print("[ERROR] HMAC init failed\n");
        return 1;
    }
    uart_print("[OK] HMAC initialized\n");
    
    // Read STATUS register
    uart_print("[2] Reading STATUS register...\n");
    uint32_t status = mmio_region_read32(hmac.base_addr, HMAC_STATUS_REG_OFFSET);
    uart_print("STATUS = 0x");
    uart_print_hex(status, 8);
    uart_print("\n");
    
    // Parse status bits
    uint32_t fifo_empty = (status >> 0) & 0x1;
    uint32_t fifo_full = (status >> 1) & 0x1;
    uint32_t fifo_depth = (status >> 4) & 0x1F;
    
    uart_print("  - FIFO empty: ");
    uart_print_hex(fifo_empty, 1);
    uart_print("\n");
    uart_print("  - FIFO full: ");
    uart_print_hex(fifo_full, 1);
    uart_print("\n");
    uart_print("  - FIFO depth: ");
    uart_print_hex(fifo_depth, 2);
    uart_print("\n");
    
    // Try writing to CFG register
    uart_print("[3] Writing to CFG register...\n");
    mmio_region_write32(hmac.base_addr, HMAC_CFG_REG_OFFSET, 0x02); // SHA_EN bit
    uint32_t cfg = mmio_region_read32(hmac.base_addr, HMAC_CFG_REG_OFFSET);
    uart_print("CFG = 0x");
    uart_print_hex(cfg, 8);
    uart_print("\n");
    
    // Try writing to CMD register
    uart_print("[4] Writing HASH_START to CMD register...\n");
    mmio_region_write32(hmac.base_addr, HMAC_CMD_REG_OFFSET, 0x01);
    
    // Read STATUS again
    uart_print("[5] Reading STATUS after START...\n");
    status = mmio_region_read32(hmac.base_addr, HMAC_STATUS_REG_OFFSET);
    uart_print("STATUS = 0x");
    uart_print_hex(status, 8);
    uart_print("\n");
    
    uart_print("\n=== Diagnostic Complete ===\n\n");
    return 0;
}
