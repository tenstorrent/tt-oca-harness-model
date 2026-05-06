/*
 * Secure DMA SHA-256 Hash Test
 *
 * This test transfers data from SRAM to DCCM and uses the hash engine in the
 * DMA to compute the SHA-256 hash of the data. It then verifies the hash against
 * the expected hash, calculated by software SHA256 library.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "test_completion.h"
#include "och_sep_common.h"
#include "och_sep_top_reg.h"
#include "sep_outbound_filter.h"
#include "sha256.h"
#include "../common/interrupt.h"

// Interrupt source IDs (from hw/sep/sep.sv)
#define EXT_INT_DMA_DONE        1
#define EXT_INT_DMA_CHUNK_DONE  2
#define EXT_INT_DMA_ERROR       3

// Flag set by interrupt handler
static volatile uint32_t dma_interrupt_fired = 0;

// DMA interrupt handler - clears interrupt at source
void __attribute__((interrupt("machine"))) dma_isr(void) {
    dma_interrupt_fired = 1;
    volatile uint32_t *status = (volatile uint32_t *)SECURE_DMA_STATUS_REG_ADDR;
    *status = SECURE_DMA_STATUS_DONE_MASK | SECURE_DMA_STATUS_ERROR_MASK | SECURE_DMA_STATUS_CHUNK_DONE_MASK;
    __asm__ volatile("fence" ::: "memory");
}

// CFG_REGWEN values (multi-bit bool)
#define MUBI4_TRUE  0x6  // Unlocked
#define MUBI4_FALSE 0x9  // Locked

// ASID values (from RDL enum asid_e)
#define ASID_OT_ADDR   0x7  // OpenTitan 32-bit internal bus
#define ASID_SYS_ADDR  0x9  // SoC system address bus
#define ASID_SOC_ADDR  0xa  // SoC control register bus

// Opcode values (from RDL enum opcode_e)
#define OPCODE_COPY    0x0
#define OPCODE_SHA256  0x1
#define OPCODE_SHA384  0x2
#define OPCODE_SHA512  0x3

// Transfer width values (from RDL enum transfer_width_e)
#define TRANSFER_WIDTH_ONE_BYTE   0x0
#define TRANSFER_WIDTH_TWO_BYTE   0x1
#define TRANSFER_WIDTH_FOUR_BYTE  0x2

// Test data size in bytes - must be a multiple of 4
// 64 bytes = 1 SHA block + padding = ~8-10K cycles for SW hash
// 128 bytes = 2 SHA blocks + padding = ~12-15K cycles
// 4096 (0x1000) bytes = 64 blocks = ~300-400K cycles
#ifndef TEST_DATA_SIZE
#define TEST_DATA_SIZE 0x100
#endif

//==============================================================================
// Hashing Functions
//==============================================================================

// Function to compute the SHA256 hash
void compute_sha256(const unsigned char *data, size_t data_len, unsigned char *hash_output) {
    SHA256_CTX ctx;

    // Initialize the SHA256 context
    sha256_init(&ctx);

    // Feed the data into the hash function
    sha256_update(&ctx, data, data_len);

    // Finalize and retrieve the hash
    sha256_final(&ctx, hash_output);
}

//==============================================================================
// Main Test
//==============================================================================

int main(void) {
    // Initialize outbound filter to allow testpass mailbox access
    sep_outbound_filter_init();

    // Set up DMA interrupts
    pic_register_handler(EXT_INT_DMA_DONE, dma_isr);
    pic_register_handler(EXT_INT_DMA_ERROR, dma_isr);
    pic_set_gateway(EXT_INT_DMA_DONE, 0, 0);   // level-triggered, active-high
    pic_set_gateway(EXT_INT_DMA_ERROR, 0, 0);
    pic_set_priority(EXT_INT_DMA_DONE, 1);
    pic_set_priority(EXT_INT_DMA_ERROR, 1);
    pic_enable_source(EXT_INT_DMA_DONE);
    pic_enable_source(EXT_INT_DMA_ERROR);
    pic_enable_interrupts();

    int errors = 0;

    printf("=== Secure DMA SHA-256 Hash Test ===\n\n");

    // Check that DMA is idle (CFG_REGWEN should be MUBI4_TRUE = 0x6)
    uint32_t cfg_regwen = READ_REG(SECURE_DMA_CFG_REGWEN_REG_ADDR);
    printf("CFG_REGWEN = 0x%x (expected 0x%x for unlocked)\n", cfg_regwen, MUBI4_TRUE);

    if ((cfg_regwen & 0xF) != MUBI4_TRUE) {
        printf("WARNING: DMA may be busy or locked\n");
    }

    //==========================================================================
    // Step 1: Configure the DMA enabled memory range (required before DMA use)
    //==========================================================================
    printf("\nConfiguring DMA enabled memory range:\n");

    // Set the allowed memory range for DMA operations
    WRITE_REG(SECURE_DMA_ENABLED_MEMORY_RANGE_BASE_REG_ADDR, 0x0);
    printf("  ENABLED_MEMORY_RANGE_BASE = 0x%08x\n", READ_REG(SECURE_DMA_ENABLED_MEMORY_RANGE_BASE_REG_ADDR));

    WRITE_REG(SECURE_DMA_ENABLED_MEMORY_RANGE_LIMIT_REG_ADDR, 0xFFFFFFFF);
    printf("  ENABLED_MEMORY_RANGE_LIMIT = 0x%08x\n", READ_REG(SECURE_DMA_ENABLED_MEMORY_RANGE_LIMIT_REG_ADDR));

    // Mark the range as valid - this is required before DMA can operate
    SECURE_DMA_RANGE_VALID_reg_u range_valid = { .f = { .range_valid = 1 } };
    WRITE_REG(SECURE_DMA_RANGE_VALID_REG_ADDR, range_valid.val);
    printf("  RANGE_VALID = 0x%x\n", READ_REG(SECURE_DMA_RANGE_VALID_REG_ADDR));

	//==========================================================================
	// Step 2: Write random data to SRAM
	//==========================================================================

	// Generate random data directly in SRAM (avoid stack overflow)
	printf("  Generating random data in SRAM...\n");
	volatile uint32_t *src_ptr = (volatile uint32_t *)SEP_SRAM_MEM_BASE_ADDR;
	for (int i = 0; i < TEST_DATA_SIZE/4; i++) {
		src_ptr[i] = (uint32_t)rand();
	}

    //==========================================================================
    // Step 2: Configure the DMA transfer from SRAM to DCCM
	//==========================================================================
	printf("\nConfiguring DMA transfer from SRAM to DCCM:\n");

	// Set the source address
	WRITE_REG(SECURE_DMA_SRC_ADDR_LO_REG_ADDR, SEP_SRAM_MEM_BASE_ADDR);
	WRITE_REG(SECURE_DMA_SRC_ADDR_HI_REG_ADDR, SEP_SRAM_MEM_BASE_ADDR >> 32);

	// Set the destination address (use high DCCM to avoid BSS overlap)
	// BSS is at low DCCM (~0x80000-0x80FFF), so use 0x82000+
	#define DMA_DST_ADDR (SEP_DCCM_MEM_BASE_ADDR + 0x2000)
	WRITE_REG(SECURE_DMA_DST_ADDR_LO_REG_ADDR, DMA_DST_ADDR);
	WRITE_REG(SECURE_DMA_DST_ADDR_HI_REG_ADDR, DMA_DST_ADDR >> 32);

	// Configure address space IDs (both internal OT addresses)
	SECURE_DMA_ADDR_SPACE_ID_reg_u addr_space_id = { .f = {
		.src_asid = ASID_OT_ADDR,
		.dst_asid = ASID_OT_ADDR
	}};
	WRITE_REG(SECURE_DMA_ADDR_SPACE_ID_REG_ADDR, addr_space_id.val);
	printf("  ADDR_SPACE_ID = 0x%x (SRC=OT_ADDR, DST=OT_ADDR)\n",
	       READ_REG(SECURE_DMA_ADDR_SPACE_ID_REG_ADDR));

	// Set the transfer width to 4 bytes
	SECURE_DMA_TRANSFER_WIDTH_reg_u transfer_width = { .f = {
		.width = TRANSFER_WIDTH_FOUR_BYTE
	}};
	WRITE_REG(SECURE_DMA_TRANSFER_WIDTH_REG_ADDR, transfer_width.val);

	// Set the chunk data size (single chunk = total size)
	WRITE_REG(SECURE_DMA_CHUNK_DATA_SIZE_REG_ADDR, TEST_DATA_SIZE);

	// Set the total data size
	WRITE_REG(SECURE_DMA_TOTAL_DATA_SIZE_REG_ADDR, TEST_DATA_SIZE);

	// Configure source: increment address after each transfer
	SECURE_DMA_SRC_CONFIG_reg_u src_config = { .f = { .increment = 1, .wrap = 0 } };
	WRITE_REG(SECURE_DMA_SRC_CONFIG_REG_ADDR, src_config.val);
	printf("  SRC_CONFIG = 0x%x (INCREMENT enabled)\n", READ_REG(SECURE_DMA_SRC_CONFIG_REG_ADDR));

	// Configure destination: increment address after each transfer
	SECURE_DMA_DST_CONFIG_reg_u dst_config = { .f = { .increment = 1, .wrap = 0 } };
	WRITE_REG(SECURE_DMA_DST_CONFIG_REG_ADDR, dst_config.val);
	printf("  DST_CONFIG = 0x%x (INCREMENT enabled)\n", READ_REG(SECURE_DMA_DST_CONFIG_REG_ADDR));

	// Dump all configuration registers before starting transfer
	printf("\nDMA Configuration before GO:\n");
	printf("  SRC_ADDR    = 0x%08x%08x\n",
	       READ_REG(SECURE_DMA_SRC_ADDR_HI_REG_ADDR),
	       READ_REG(SECURE_DMA_SRC_ADDR_LO_REG_ADDR));
	printf("  DST_ADDR    = 0x%08x%08x\n",
	       READ_REG(SECURE_DMA_DST_ADDR_HI_REG_ADDR),
	       READ_REG(SECURE_DMA_DST_ADDR_LO_REG_ADDR));
	printf("  TOTAL_SIZE  = 0x%x\n", READ_REG(SECURE_DMA_TOTAL_DATA_SIZE_REG_ADDR));
	printf("  CHUNK_SIZE  = 0x%x\n", READ_REG(SECURE_DMA_CHUNK_DATA_SIZE_REG_ADDR));
	printf("  XFER_WIDTH  = 0x%x\n", READ_REG(SECURE_DMA_TRANSFER_WIDTH_REG_ADDR));
	printf("  ADDR_SPACE  = 0x%x\n", READ_REG(SECURE_DMA_ADDR_SPACE_ID_REG_ADDR));
	printf("  SRC_CONFIG  = 0x%x\n", READ_REG(SECURE_DMA_SRC_CONFIG_REG_ADDR));
	printf("  DST_CONFIG  = 0x%x\n", READ_REG(SECURE_DMA_DST_CONFIG_REG_ADDR));

	// Enable DMA interrupts in the DMA controller
	SECURE_DMA_INTR_ENABLE_reg_u intr_enable = { .f = {
		.dma_done = 1,
		.dma_chunk_done = 0,
		.dma_error = 1
	}};
	WRITE_REG(SECURE_DMA_INTR_ENABLE_REG_ADDR, intr_enable.val);

	// Start the DMA transfer: OPCODE=SHA256 (0x1), INITIAL_TRANSFER=1 (bit 8), and GO=1 (bit 31)
	// IMPORTANT: Printf before starting DMA to avoid DCCM contention (format strings are in DCCM)
	printf("\nStarting DMA transfer...\n");
	printf("  Waiting for DMA completion (WFI-based)...\n");

	// Configure and start DMA transfer with SHA-256 hashing
	// DIGEST_SWAP converts digest to big-endian to match SW SHA-256 output
	SECURE_DMA_CONTROL_reg_u control = { .f = {
		.opcode = OPCODE_SHA256,
		.digest_swap = 1,
		.initial_transfer = 1,
		.go = 1
	}};
	WRITE_REG(SECURE_DMA_CONTROL_REG_ADDR, control.val);

	// Wait for DMA completion using WFI
	// The interrupt handler sets dma_interrupt_fired flag and clears STATUS.DONE
	// So we check the flag instead of STATUS register
	int timeout = 100000;
	while (timeout-- > 0) {
		__asm__ volatile("wfi");  // Sleep until interrupt pending

		// Check flag set by interrupt handler
		if (dma_interrupt_fired) break;
	}

	// Check ERROR_CODE to see if there was an error
	// (STATUS bits are cleared by the interrupt handler)
	SECURE_DMA_ERROR_CODE_reg_u error_code;
	error_code.val = READ_REG(SECURE_DMA_ERROR_CODE_REG_ADDR);

	// Process result
	if (dma_interrupt_fired && error_code.val == 0) {  // Success
		printf("  DMA transfer completed!\n");
	} else if (error_code.val != 0) {  // Error occurred
		printf("  ERROR: DMA transfer failed!\n");
		printf("  ERROR_CODE = 0x%x\n", error_code.val);

		// Decode error bits using struct fields
		if (error_code.f.src_addr_error)    printf("    - SRC_ADDR_ERROR: Source address is invalid\n");
		if (error_code.f.dst_addr_error)    printf("    - DST_ADDR_ERROR: Destination address is invalid\n");
		if (error_code.f.opcode_error)      printf("    - OPCODE_ERROR: Opcode is invalid\n");
		if (error_code.f.size_error)        printf("    - SIZE_ERROR: Size/width configuration invalid\n");
		if (error_code.f.bus_error)         printf("    - BUS_ERROR: Bus transfer returned an error\n");
		if (error_code.f.base_limit_error)  printf("    - BASE_LIMIT_ERROR: Base/limit addresses invalid\n");
		if (error_code.f.range_valid_error) printf("    - RANGE_VALID_ERROR: Memory range not configured\n");
		if (error_code.f.asid_error)        printf("    - ASID_ERROR: Source or destination ASID invalid\n");

		errors++;
	} else {
		printf("  ERROR: DMA transfer timeout!\n");
		// Abort DMA
		SECURE_DMA_CONTROL_reg_u abort_ctrl = { .f = { .abort = 1 } };
		WRITE_REG(SECURE_DMA_CONTROL_REG_ADDR, abort_ctrl.val);
		errors++;
	}

	//============================================================================
	// Step 3: Compute the SHA-256 hash using software library
	//============================================================================

	printf("\nComputing SHA-256 hash using software library...\n");

	// Compute the SHA-256 hash using software library
	uint8_t sw_hash[32];
	compute_sha256((unsigned char *)src_ptr, TEST_DATA_SIZE, sw_hash);

	//==========================================================================
	// Step 4: Verify the hash against the expected hash
	//==========================================================================
	printf("\nVerifying SHA-256 hash against expected hash...\n");

	uint32_t expected_hash[8];
	for (int i = 0; i < 8; i++) {
		expected_hash[i] = READ_REG(SECURE_DMA_SHA2_DIGEST_0_REG_ADDR + i * 4);
	}

	// Cast the 32-bit array to an 8-bit pointer for memcmp
    uint8_t *hw_hash = (uint8_t *)expected_hash;

// Print neatly as a continuous hex string
    printf("  Expected (HW) = 0x");
    for (int i = 0; i < 32; i++) {
        printf("%02x", hw_hash[i]);
    }
    printf("\n");

    printf("  Computed (SW) = 0x");
    for (int i = 0; i < 32; i++) {
        printf("%02x", sw_hash[i]);
    }
    printf("\n");

	// Safe to use memcmp now! Both are treated as 32-byte streams.
    if (memcmp(hw_hash, sw_hash, 32) != 0) {
        printf("  ERROR: SHA-256 hash mismatch!\n");
        errors++;
    }

	//==========================================================================
	// Step 5: Verify data in SRAM and DCCM matches
	//==========================================================================
	printf("\nVerifying data in SRAM and DCCM matches...\n");

	// Get pointer to DCCM for comparison (must match DMA_DST_ADDR)
	volatile uint32_t *dccm_ptr = (volatile uint32_t *)DMA_DST_ADDR;

	for (int i = 0; i < TEST_DATA_SIZE/4; i++) {
		if (src_ptr[i] != dccm_ptr[i]) {
			printf("  ERROR: Data mismatch at offset 0x%x: SRAM=0x%08x, DCCM=0x%08x\n",
			       i * 4, src_ptr[i], dccm_ptr[i]);
			errors++;
		}
	}

	printf("  PASS: SRAM and DCCM data matches!\n");

    printf("\n=== Test Summary ===\n");

    if (errors == 0) {
        printf("All SHA-256 hash tests PASSED\n");
        test_pass(0);
    } else {
        printf("FAILED: %d SHA-256 hash test(s) failed\n", errors);
        test_fail(errors);
    }

    // Keep CPU alive after signaling completion.
    while (1) {
        __asm__("wfi");
    }
}
