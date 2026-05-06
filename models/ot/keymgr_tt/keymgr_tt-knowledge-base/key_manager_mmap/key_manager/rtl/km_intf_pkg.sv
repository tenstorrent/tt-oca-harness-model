// Copyright 2026 Tenstorrent Inc.

`ifndef DISABLE_DEFAULT_NETTYPE_NONE
    `default_nettype none
`endif

`include "axi/typedef.svh"

/**
 * @file km_intf_pkg.sv
 * @brief Key Manager interface type definitions and address map constants.
 *
 * @details Defines all shared types used across the Key Manager subsystem:
 *          - AXI4-Lite channel, request, and response types (32-bit).
 *          - ROM and SRAM memory interface structs (req/rsp).
 *          - DRBG AXI-Stream interface structs.
 *          - IRQ event type.
 *          - SEP OTP data interface struct.
 *          - Full address map constants for internal and external peripherals.
 */
package km_intf_pkg;

    import axi_pkg::*;

    // =========================================================================
    // AXI4-Lite Type Definitions (32-bit)
    // =========================================================================

    /** @brief AXI4-Lite bus widths for the Key Manager subsystem. */
    localparam int unsigned KM_AXI_ADDR_WIDTH = 32;
    localparam int unsigned KM_AXI_DATA_WIDTH = 32;
    localparam int unsigned KM_AXI_STRB_WIDTH = KM_AXI_DATA_WIDTH / 8;

    /** @brief 32-bit AXI address type for the Key Manager subsystem. */
    typedef logic [KM_AXI_ADDR_WIDTH-1:0] km_addr_t;
    /** @brief 32-bit AXI data type for the Key Manager subsystem. */
    typedef logic [KM_AXI_DATA_WIDTH-1:0] km_data_t;
    /** @brief 4-bit AXI write-strobe type (one bit per data byte). */
    typedef logic [KM_AXI_STRB_WIDTH-1:0] km_strb_t;

    //=========================================================================
    // AXI4-Lite Channel and Request/Response Types
    //=========================================================================

    /**
     * @brief AXI-Lite channel, request, and response types (macro-generated).
     *
     * @details Expands to: km_axil_aw_chan_t, km_axil_w_chan_t,
     *          km_axil_b_chan_t, km_axil_ar_chan_t, km_axil_r_chan_t,
     *          km_axil_req_t, km_axil_resp_t.
     */
    `AXI_LITE_TYPEDEF_ALL(km_axil, km_addr_t, km_data_t, km_strb_t)

    //=========================================================================
    // Memory Interface Types
    //=========================================================================

    /** @brief Memory interface common widths (byte address, data, byte-enables). */
    parameter int unsigned KM_MEM_ADDR_WIDTH = 32;  // Byte address width
    parameter int unsigned KM_MEM_DATA_WIDTH = 32;
    parameter int unsigned KM_MEM_STRB_WIDTH = KM_MEM_DATA_WIDTH / 8;

    /** @brief ROM word-address width (11 bits = 2K words = 8 KB). */
    parameter int unsigned KM_ROM_MEM_ADDR_WIDTH = 11;

    /** @brief SRAM word-address width (12 bits = 4K words = 16 KB). */
    parameter int unsigned KM_SRAM_MEM_ADDR_WIDTH = 12;

    /** @brief ROM memory request (CPU -> ROM hard macro). */
    typedef struct packed {
        logic                             req;           // Request valid
        logic [KM_ROM_MEM_ADDR_WIDTH-1:0] addr;        // Word address
    } km_rom_mem_req_t;

    /** @brief ROM memory response (ROM hard macro -> CPU). */
    typedef struct packed {
        logic                           gnt;           // Grant/ready
        logic                           rvalid;        // Read data valid
        logic [KM_MEM_DATA_WIDTH-1:0]   rdata;         // Read data
        logic [KM_MEM_STRB_WIDTH-1:0]   parity;        // Byte parity bits
    } km_rom_mem_rsp_t;

    /** @brief SRAM memory request (CPU -> SRAM hard macro). */
    typedef struct packed {
        logic                              req;           // Request valid
        logic                              we;            // Write enable
        logic [KM_MEM_STRB_WIDTH-1:0]      be;            // Byte enables
        logic [KM_SRAM_MEM_ADDR_WIDTH-1:0] addr;          // Word address
        logic [KM_MEM_DATA_WIDTH-1:0]      wdata;         // Write data
        logic [KM_MEM_STRB_WIDTH-1:0]      wparity;       // Write parity bits
    } km_sram_mem_req_t;

    /** @brief SRAM memory response (SRAM hard macro -> CPU). */
    typedef struct packed {
        logic                           gnt;           // Grant/ready
        logic                           rvalid;        // Read data valid
        logic [KM_MEM_DATA_WIDTH-1:0]   rdata;         // Read data
        logic [KM_MEM_STRB_WIDTH-1:0]   rparity;       // Read parity bits
    } km_sram_mem_rsp_t;

    //=========================================================================
    // IRQ Types
    //=========================================================================

    /** @brief Packed IRQ event flags for the Key Manager subsystem. */
    typedef struct packed {
        logic rom_parity_err;       // ROM parity error event
        logic sram_parity_err;      // SRAM parity error event
    } km_irq_events_t;

    /**
     * @brief Key Manager CPU address map constants.
     *
     * @details
     *  | Region | Base        | End         | Size  |
     *  |--------|-------------|-------------|-------|
     *  | ROM    | 0x0000_0000 | 0x0000_1FFF |  8 KB |
     *  | SRAM   | 0x0000_4000 | 0x0000_7FFF | 16 KB |
     *  | KPV    | 0x0000_D000 | 0x0000_DFFF |  4 KB |
     *  | KMCSR  | 0x0000_E000 | 0x0000_EFFF |  4 KB |
     *  | DRBG   | 0x0000_F000 | 0x0000_FFFF |  4 KB |
     *  | MBOX   | 0x0001_0000 | 0x0001_0FFF |  4 KB |
     *  | OTBN   | 0x0001_8000 | 0x0001_8FFF |  4 KB |
     *  | AES    | 0x0001_9000 | 0x0001_9FFF |  4 KB |
     *  | KMAC   | 0x0001_A000 | 0x0001_AFFF |  4 KB |
     *  | HMAC   | 0x0001_B000 | 0x0001_BFFF |  4 KB |
     *  | VROM   | 0x1000_0000 | 0x1000_FFFF | 64 KB |
     */

    // Internal memory
    localparam km_addr_t ROM_BASE_ADDR     = 32'h0000_0000;
    localparam km_addr_t ROM_END_ADDR      = 32'h0000_1FFF;
    localparam km_addr_t SRAM_BASE_ADDR    = 32'h0000_4000;
    localparam km_addr_t SRAM_END_ADDR     = 32'h0000_7FFF;

    // Internal peripherals
    localparam km_addr_t KPV_BASE_ADDR          = 32'h0000_D000;
    localparam km_addr_t KPV_END_ADDR           = 32'h0000_DFFF;
    localparam km_addr_t KMCSR_BASE_ADDR        = 32'h0000_E000;
    localparam km_addr_t KMCSR_END_ADDR         = 32'h0000_EFFF;
    localparam km_addr_t DRBG_SAMPLER_BASE_ADDR = 32'h0000_F000;
    localparam km_addr_t DRBG_SAMPLER_END_ADDR  = 32'h0000_FFFF;
    localparam km_addr_t MBOX_BASE_ADDR         = 32'h0001_0000;
    localparam km_addr_t MBOX_END_ADDR          = 32'h0001_0FFF;

    // External crypto engine ports
    localparam km_addr_t OTBN_BASE_ADDR    = 32'h0001_8000;
    localparam km_addr_t OTBN_END_ADDR     = 32'h0001_8FFF;
    localparam km_addr_t AES_BASE_ADDR     = 32'h0001_9000;
    localparam km_addr_t AES_END_ADDR      = 32'h0001_9FFF;
    localparam km_addr_t KMAC_BASE_ADDR    = 32'h0001_A000;
    localparam km_addr_t KMAC_END_ADDR     = 32'h0001_AFFF;
    localparam km_addr_t HMAC_BASE_ADDR    = 32'h0001_B000;
    localparam km_addr_t HMAC_END_ADDR     = 32'h0001_BFFF;

    // Testbench virtual ROM (rodata)
    localparam km_addr_t VROM_BASE_ADDR    = 32'h1000_0000;
    localparam km_addr_t VROM_END_ADDR     = 32'h1000_FFFF;

    /** @brief Region sizes derived from the address ranges above. */
    localparam int unsigned ROM_SIZE_BYTES    = ROM_END_ADDR - ROM_BASE_ADDR + 1;   // 8 KB
    localparam int unsigned SRAM_SIZE_BYTES   = SRAM_END_ADDR - SRAM_BASE_ADDR + 1;  // 16 KB
    localparam int unsigned VROM_SIZE_BYTES   = VROM_END_ADDR - VROM_BASE_ADDR + 1;  // 64 KB

    /** @brief SRAM write-lock granularity: 512 bytes per lockable region. */
    localparam int unsigned SRAM_LOCK_REGION_BYTES = 512;
    localparam int unsigned SRAM_NUM_LOCK_REGIONS  = SRAM_SIZE_BYTES / SRAM_LOCK_REGION_BYTES;

    // =========================================================================
    // DRBG AXI-Stream Interface (KM is slave, DRBG is master)
    // =========================================================================

    /** @brief DRBG AXI-Stream bus widths (32-bit data, 4-bit strobe). */
    localparam int unsigned KM_DRBG_AXIS_DATA_WIDTH = 32;
    localparam int unsigned KM_DRBG_AXIS_STRB_WIDTH = KM_DRBG_AXIS_DATA_WIDTH / 8;

    /** @brief DRBG AXI-Stream request (DRBG master -> KM sampler slave). */
    typedef struct packed {
        logic                               tvalid;
        logic [KM_DRBG_AXIS_DATA_WIDTH-1:0] tdata;
        logic [KM_DRBG_AXIS_STRB_WIDTH-1:0] tstrb;
    } km_drbg_axis_req_t;

    /** @brief DRBG AXI-Stream response (KM sampler slave -> DRBG master). */
    typedef struct packed {
        logic tready;
    } km_drbg_axis_resp_t;

    //=========================================================================
    // SEP OTP Data Interface (FR-0000-134–FR-0000-141)
    //=========================================================================
    /**
     * @brief SEP OTP data bundle read directly from port signals by KMCSR.
     *
     * @details Life-cycle field is differentially encoded (4-bit value in
     *          8 bits); chiplet UID is 256 bits (32 bytes).  Values are
     *          read-through from the OTP port.
     */
    typedef struct packed {
        logic [7:0]   life_cycle;           // 4-bit value differentially encoded into 8-bit
        logic [1:0]   demotion_state_1;     // 2-bit differential encoded
        logic [1:0]   demotion_state_2;     // 2-bit differential encoded
        logic [255:0] chiplet_uid;          // Device Unique Identifier (32×8b)
    } km_otp_data_t;

endpackage : km_intf_pkg

`ifndef DISABLE_DEFAULT_NETTYPE_NONE
    `default_nettype wire
`endif
