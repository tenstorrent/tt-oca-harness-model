//-----------------------------------------------------------------------------
// SEP eFuse Wrapper
//
// Copyright 2025 Tenstorrent Inc.
//-----------------------------------------------------------------------------

`include "axi/assign.svh"
`include "axi/typedef.svh"
`include "och_sep_top_reg.svh"

module sep_efuse_wrapper
    import sep_pkg::*;
    import sep_crypto_pkg::*;
    import sep_efuse_pkg::*;
#() (
    input logic                           clk_i,
    input logic                           rst_ni,

    input  logic                          test_en_i,

	input  sep_pkg::sep_straps_t  	      sep_straps_i,
	input  logic                          dft_boot_seq_done_i,

	output logic                               sep_reset_n_o,
	output logic                               security_disable_o,
	output logic [2*sep_pkg::LC_STATE_BIT_WIDTH-1:0] lc_state_o,
	output sep_efuse_pkg::efuse_map_t 		   shadow_regs_o,
	output logic                               fuse_sense_done_o,
	output logic                               secure_tm_latch_o,

	// OTP debug AXI-Lite manager interface
	input  sep_efuse_pkg::efuse_axil_req_t     axil_sep_otp_jtag_req_i,
	output sep_efuse_pkg::efuse_axil_resp_t    axil_sep_otp_jtag_resp_o,

    // Full AXI4 slave from local crossbar
    input  sep_crypto_axi_req_t           	   sep_efuse_axi_req_i,
    output sep_crypto_axi_resp_t               sep_efuse_axi_resp_o,

	// Efuse Interface to SHIM
	output sep_efuse_pkg::efuse_axil_req_t     efuse_bank_ctrl_req_o,
	input  sep_efuse_pkg::efuse_axil_resp_t    efuse_bank_ctrl_resp_i,

	// Efuse Command Interface - custom interface for SHIM
	output sep_efuse_pkg::fuse_command_req_t   efuse_shim_command_req_o,
    input  sep_efuse_pkg::fuse_command_resp_t  efuse_shim_command_resp_i,

	// PROD_DBG isolation: block LC_STATE transitions when DEMOTE is active
	input  logic                               prod_dbg_active_i,

	// Locked Field Access Interrupt
	output logic                               locked_field_access_interrupt_o
);

	// Intermediate 32-bit AXI (after data-width conversion)
	localparam int unsigned SEP_EFUSE_AXI32_DATA_WIDTH = 32;
	localparam int unsigned SEP_EFUSE_AXI32_STRB_WIDTH = SEP_EFUSE_AXI32_DATA_WIDTH / 8;
	typedef logic [SEP_EFUSE_AXI32_DATA_WIDTH-1:0] sep_efuse_axi32_data_t;
	typedef logic [SEP_EFUSE_AXI32_STRB_WIDTH-1:0] sep_efuse_axi32_strb_t;
	`AXI_TYPEDEF_ALL(sep_efuse_axi32, sep_pkg::sep_crypto_axi_addr_t, sep_pkg::sep_crypto_axi_id_t, sep_efuse_axi32_data_t, sep_efuse_axi32_strb_t, sep_pkg::sep_crypto_axi_user_t)

	sep_efuse_axi32_req_t  sep_efuse_axi32_req;
	sep_efuse_axi32_resp_t sep_efuse_axi32_resp;

	// Intermediate AXI4-Lite (after data-width conversion)
    efuse_axil_req_t efuse_axil_req_i;
    efuse_axil_resp_t efuse_axil_resp_o;

	// Efuse signals
	logic fuse_sense_done;
	logic security_disable;

  	assign lc_state_o = shadow_regs_o.f.lc_state.lc_state;

	// Secure TCM Latch
	logic secure_tm_latch;
    always_latch begin : GEN_SECURE_TM
        if (security_disable) begin
            if (!rst_ni) // TODO: check resets
                secure_tm_latch = sep_straps_i.test_straps.test_en;
        end
        else if (!fuse_sense_done) begin
            secure_tm_latch = sep_straps_i.test_straps.test_en;
        end
    end

	// First downsize AXI data width 64 -> 32, then convert to AXI-Lite
	axi_dw_converter #(
		.AxiMaxReads         (8),
		.AxiSlvPortDataWidth (sep_pkg::SEP_CRYPTO_AXI_DATA_WIDTH),
		.AxiMstPortDataWidth (SEP_EFUSE_AXI32_DATA_WIDTH),
		.AxiAddrWidth        (sep_pkg::SEP_CRYPTO_AXI_ADDR_WIDTH),
		.AxiIdWidth          (sep_pkg::SEP_CRYPTO_AXI_ID_WIDTH),
		.aw_chan_t           (sep_pkg::sep_crypto_axi_aw_chan_t),
		.mst_w_chan_t        (sep_efuse_axi32_w_chan_t),
		.slv_w_chan_t        (sep_pkg::sep_crypto_axi_w_chan_t),
		.b_chan_t            (sep_pkg::sep_crypto_axi_b_chan_t),
		.ar_chan_t           (sep_pkg::sep_crypto_axi_ar_chan_t),
		.mst_r_chan_t        (sep_efuse_axi32_r_chan_t),
		.slv_r_chan_t        (sep_pkg::sep_crypto_axi_r_chan_t),
		.axi_mst_req_t       (sep_efuse_axi32_req_t),
		.axi_mst_resp_t      (sep_efuse_axi32_resp_t),
		.axi_slv_req_t       (sep_crypto_axi_req_t),
		.axi_slv_resp_t      (sep_crypto_axi_resp_t)
	) u_sep_efuse_axi_dw_conv (
		.clk_i     (clk_i),
		.rst_ni    (rst_ni),
		.slv_req_i (sep_efuse_axi_req_i),
		.slv_resp_o(sep_efuse_axi_resp_o),
		.mst_req_o (sep_efuse_axi32_req),
		.mst_resp_i(sep_efuse_axi32_resp)
	);

    // Convert downsized AXI4 to AXI4-Lite
    axi_to_axi_lite #(
        .AxiAddrWidth   (sep_pkg::SEP_CRYPTO_AXI_ADDR_WIDTH),
        .AxiDataWidth   (SEP_EFUSE_AXI32_DATA_WIDTH),
        .AxiIdWidth     (sep_pkg::SEP_CRYPTO_AXI_ID_WIDTH),
        .AxiUserWidth   (sep_pkg::SEP_CRYPTO_AXI_USER_WIDTH),
        .AxiMaxWriteTxns(16), // TODO
        .AxiMaxReadTxns (16), // TODO
        .FullBW         (1'b0), // ID Queue in Full BW mode in axi_burst_splitter
        .FallThrough    (1'b0), // FIFOs in Fall through mode in ID reflect
        .SpillAw        (1'b0), // Spill register control
        .SpillW         (1'b0),
        .SpillB         (1'b0),
        .SpillAr        (1'b0),
        .SpillR         (1'b0),
        .full_req_t     (sep_efuse_axi32_req_t),
        .full_resp_t    (sep_efuse_axi32_resp_t),
        .lite_req_t     (efuse_axil_req_t),
        .lite_resp_t    (efuse_axil_resp_t)
    ) sep_efuse_axi_to_axi_lite (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .test_i(test_en_i),
        // from AXI (32-bit after DW conversion)
        .slv_req_i (sep_efuse_axi32_req),
        .slv_resp_o(sep_efuse_axi32_resp),
        // to AXIL (32-bit)
        .mst_req_o (efuse_axil_req_i),
        .mst_resp_i(efuse_axil_resp_o)
    );

    efuse_interface_controller #(
		.ADDR_WIDTH                 (sep_efuse_pkg::ADDR_WIDTH),
		.DATA_WIDTH                 (sep_efuse_pkg::DATA_WIDTH),

		.addr_t                     (sep_efuse_pkg::addr_t),
		.data_t                     (sep_efuse_pkg::data_t),
		.strb_t                     (sep_efuse_pkg::strb_t),
		.efuse_axil_req_t           (efuse_axil_req_t),
		.efuse_axil_resp_t          (efuse_axil_resp_t),

		.efuse_axil_aw_chan_t       (efuse_axil_aw_chan_t),
		.efuse_axil_w_chan_t        (efuse_axil_w_chan_t),
		.efuse_axil_b_chan_t        (efuse_axil_b_chan_t),
		.efuse_axil_ar_chan_t       (efuse_axil_ar_chan_t),
		.efuse_axil_r_chan_t        (efuse_axil_r_chan_t),
		.efuse_apb_req_t            (efuse_apb_req_t),
		.efuse_apb_resp_t           (efuse_apb_resp_t),

		.efuse_addr_t               (efuse_addr_bit_t),
		.efuse_data_t               (efuse_data_t),
		.efuse_word_counter_t       (efuse_word_counter_t),
		.fuse_command_req_t         (fuse_command_req_t),
		.fuse_command_resp_t        (fuse_command_resp_t),

		.SEP_SEC_DISABLE_TOKEN      (sep_efuse_pkg::SEC_DISABLE_TOKEN), // During synthesis, to be replaced with the actual token digest embedded in the netlist

		.EFUSE_MAP_REG_MAP_BASE_ADDR(sep_pkg::SEP_EFUSE_MAP_REG_MAP_BASE_ADDR),
		.EFUSE_MAP_REG_MAP_SIZE     (sep_pkg::SEP_EFUSE_MAP_REG_MAP_SIZE),

		.EFUSE_MMR_REG_MAP_BASE_ADDR(EFUSE_MMR_REG_MAP_BASE_ADDR),
		.EFUSE_MMR_REG_MAP_SIZE     (EFUSE_MMR_REG_MAP_SIZE),

		.EFUSE_CTRL_REG_MAP_BASE_ADDR(EFUSE_INTERFACE_CTRL_REG_MAP_BASE_ADDR),
		.EFUSE_CTRL_REG_MAP_SIZE    (EFUSE_INTERFACE_CTRL_REG_MAP_SIZE),

		.SHADOW_REG_BITS            (sep_efuse_pkg::SHADOW_REG_BITS),
		.EFUSE_MACRO_WORD_WIDTH     (sep_efuse_pkg::NumFuseWordWidth),

		.EFUSE_FIELDS               (sep_efuse_pkg::NUM_EFUSE_FIELDS),

		.HAS_LC_STATE               (1'b1), // SEP has LC state
		.LC_STATE_WIDTH             (sep_pkg::LC_STATE_BIT_WIDTH),
		.LC_STATE_BIT_POSITION      (sep_pkg::LC_STATE_BIT_POSITION),

		.efuse_map_t                (sep_efuse_pkg::efuse_map_t)

	) u_efuse_interface_controller (
		// Global Interface
		.clk_i                      (clk_i),
		.rst_ni                     (rst_ni),

		.test_en_i                  (test_en_i),

		// AXI4-Lite Register Interface
		.axil_req_i                 (efuse_axil_req_i),
		.axil_resp_o                (efuse_axil_resp_o),

		.axil_jtag_req_i			(axil_sep_otp_jtag_req_i),
		.axil_jtag_resp_o			(axil_sep_otp_jtag_resp_o),

		// LC state should be from SEP
		.lc_state_smc_i             (8'hf0), // LC state is generated internally in SEPs efuse; 0xf0 = encoded TEST_DEV (raw 0)
		.lc_sigint_err_o            (),

		// AXI4-Lite Register Interface from Efuse Controller to shim CSR
		.fuse_bank_ctrl_req_o       (efuse_bank_ctrl_req_o),
		.fuse_bank_ctrl_resp_i      (efuse_bank_ctrl_resp_i),

		// eFuse Command Interface - custom interface for SHIM state machine
		.fuse_command_req_o		    (efuse_shim_command_req_o),
		.fuse_command_resp_i		(efuse_shim_command_resp_i),

		.secure_tm_i                (secure_tm_latch),
		.security_disable_i         (1'b0), // in SEP we use internal security disable and tie off the input to efuse_interface
		.efuse_field_map_i          (sep_efuse_pkg::EfuseFieldMap),

		.reset_n_o                  (sep_reset_n_o),
		.fuse_sense_done_o          (fuse_sense_done),
		.security_disable_o         (security_disable), // Used for LC control, and secure_tm_latch latch logic
		.shadow_regs_o              (shadow_regs_o),

		.dft_boot_seq_done_i		(dft_boot_seq_done_i), // In grendel this was mem_repair_done from SMC (now DTB?) and straps from SMC

		.prod_dbg_active_i          (prod_dbg_active_i),

		// Debug signals
		.is_write_locked_shadow_regs_o(),
		.is_read_locked_shadow_regs_o (),
		.is_write_locked_o            (),
		.is_read_locked_o             (),
		.is_write_setup_only_o        (),
		.is_lc_state_access_o         (),
		.sec_disable_token_o          (),

		.locked_field_access_interrupt_o  (locked_field_access_interrupt_o)
	);


	// Outputs
	assign secure_tm_latch_o = secure_tm_latch;
	assign security_disable_o = security_disable;
	assign fuse_sense_done_o = fuse_sense_done;


endmodule
