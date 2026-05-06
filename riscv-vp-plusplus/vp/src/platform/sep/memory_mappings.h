#pragma once

typedef uint64_t addr_t;

const addr_t MEM_SIZE = 4ULL * 1024 * 1024 * 1024;
const addr_t MEM_START_ADDR = 0x00000000FFFFFFFF;
const addr_t MEM_END_ADDR   = MEM_START_ADDR + MEM_SIZE -1;

// SMU Top Level Memory Map (Bottom 4 GB)
const addr_t SEP_START_ADDR           = 0x0000000000000000;
const addr_t SEP_END_ADDR             = 0x000000005FFFFFFF;
const addr_t EXT_REMAP_START_ADDR     = 0x0000000060000000;
const addr_t EXT_REMAP_END_ADDR       = 0x000000007FFFFFFF;
const addr_t SMC_START_ADDR           = 0x0000000080000000;
const addr_t SMC_END_ADDR             = 0x00000000BFFFFFFF;
const addr_t SMU_START_ADDR           = 0x00000000C0000000;
const addr_t SMU_END_ADDR             = 0x00000000FFFFFFFF;

// SEP + Address Remapper Memory Map (2 GB)
const addr_t EL2_INTERNAL_START_ADDR  = 0x00000000;
const addr_t EL2_INTERNAL_END_ADDR    = 0x0FFFFFFF;
const addr_t SRAM_ROM_START_ADDR      = 0x10000000;
const addr_t SRAM_ROM_END_ADDR        = 0x1FFFFFFF;
const addr_t DMA_START_ADDR           = 0x20000000;
const addr_t DMA_END_ADDR             = 0x2FFFFFFF;

// CPU and AXI4 Segments 0x0000_0000 - 0x2FFF_FFFF (768 MB)
const addr_t DTCM_START_ADDR          = 0x00000000;
const addr_t DTCM_END_ADDR            = 0x0000FFFF;
const addr_t ITCM_START_ADDR          = 0x00100000;
const addr_t ITCM_END_ADDR            = 0x0011FFFF;
const addr_t PIC_START_ADDR           = 0x00200000;
const addr_t PIC_END_ADDR             = 0x00207FFF;
const addr_t ROM_START_ADDR           = 0x10000000;
const addr_t ROM_END_ADDR             = 0x1000FFFF;
const addr_t SRAM_START_ADDR          = 0x10100000; 
const addr_t SRAM_END_ADDR            = 0x1011FFFF; 
const addr_t DMA_TARGET_START_ADDR    = 0x20000000;
const addr_t DMA_TARGET_END_ADDR      = 0x20000FFF;
const addr_t MBOX_START_ADDR          = 0x20200000;
const addr_t MBOX_END_ADDR            = 0x20200FFF;  

// Crypto/Security Segment 0x4000_0000 - 0x43FF_FFFF (64 MB)
const addr_t OTBN_DMEM1_START_ADDR    = 0x40000000;
const addr_t OTBN_DMEM1_END_ADDR      = 0x40001FFF;
const addr_t OTBN_DMEM2_START_ADDR    = 0x40010000;
const addr_t OTBN_DMEM2_END_ADDR      = 0x40013FFF;
const addr_t OTBN_CSR_START_ADDR      = 0x40020000;
const addr_t OTBN_CSR_END_ADDR        = 0x40021FFF;
const addr_t AES_START_ADDR           = 0x40022000;
const addr_t AES_END_ADDR             = 0x40023FFF;
const addr_t ASCON_START_ADDR         = 0x40024000;
const addr_t ASCON_END_ADDR           = 0x40025FFF;
const addr_t SHA3_START_ADDR          = 0x40026000;
const addr_t SHA3_END_ADDR            = 0x40027FFF;
const addr_t ENT_SRC_START_ADDR       = 0x40028000;
const addr_t ENT_SRC_END_ADDR         = 0x40029FFF;
const addr_t ENT_DIST_START_ADDR      = 0x4002A000; 	
const addr_t ENT_DIST_END_ADDR        = 0x4002BFFF;
const addr_t TRNG_START_ADDR          = 0x4002C000;
const addr_t TRNG_END_ADDR            = 0x4002DFFF;
const addr_t KEY_MGR_START_ADDR       = 0x40030000;	 
const addr_t KEY_MGR_END_ADDR         = 0x4003FFFF;
const addr_t OTP_START_ADDR           = 0x40070000; 	
const addr_t OTP_END_ADDR             = 0x4007FFFF;
const addr_t LC_START_ADDR            = 0x40080000; 	
const addr_t LC_END_ADDR              = 0x40081FFF;
const addr_t ALRM_START_ADDR          = 0x40082000;	
const addr_t ALRM_END_ADDR            = 0x40087FFF;
const addr_t WDT_START_ADDR           = 0x40088000;	
const addr_t WDT_END_ADDR             = 0x40088FFF;

// Systems Segment 0x4800_0000 - 0x4BFF_FFFF (64 MB)
const addr_t MBOX_SEP_READ_START_ADDR  = 0x48000000;
const addr_t MBOX_SEP_READ_END_ADDR    = 0x49FFFFFF;
const addr_t MBOX_SEP_WRITE_START_ADDR = 0x49000000;
const addr_t MBOX_SEP_WRITE_END_ADDR   = 0x49FFFFFF;
const addr_t MBOX_SEP_CSR_START_ADDR   = 0x4A000000;
const addr_t MBOX_SEP_CSR_END_ADDR     = 0x4A000FFF;
const addr_t SEP_CSR_START_ADDR        = 0x4A002000;
const addr_t SEP_CSR_END_ADDR          = 0x4A002FFF;