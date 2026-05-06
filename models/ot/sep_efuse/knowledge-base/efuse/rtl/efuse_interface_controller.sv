//-----------------------------------------------------------------------------
// Efuse Interface Controller
//
// Copyright 2025 Tenstorrent Inc.
//-----------------------------------------------------------------------------

`include "prim_assert.sv"
`include "axi/assign.svh"
`include "apb/assign.svh"

module efuse_interface_controller
#(
    // Xbar interface configuration parameters
    parameter int unsigned ADDR_WIDTH = 32,
    parameter int unsigned DATA_WIDTH = 32,

    // Xbar interface types
    parameter type addr_t = logic,
    parameter type data_t = logic,
    parameter type strb_t = logic,

    parameter type efuse_axil_req_t = logic,
    parameter type efuse_axil_resp_t = logic,

    parameter type efuse_axil_aw_chan_t = logic,
    parameter type efuse_axil_w_chan_t = logic,
    parameter type efuse_axil_b_chan_t = logic,
    parameter type efuse_axil_ar_chan_t = logic,
    parameter type efuse_axil_r_chan_t = logic,
    parameter type efuse_apb_req_t = logic,
    parameter type efuse_apb_resp_t = logic,

    // Efuse SHIM interface types - support bit addressing
    parameter type efuse_addr_t = logic,
    parameter type efuse_data_t = logic,
    parameter type efuse_word_counter_t = logic,
    parameter type fuse_command_req_t = logic,
    parameter type fuse_command_resp_t = logic,

    // Secure Disable Token - Embedded in RTL
    parameter bit [255:0] SEP_SEC_DISABLE_TOKEN = 256'b0,

    // Fuse Map interface configuration parameters
    parameter bit [31:0] EFUSE_MAP_REG_MAP_BASE_ADDR = 32'h0,
    parameter bit [31:0] EFUSE_MAP_REG_MAP_SIZE = 32'h1000,
    localparam bit [31:0] EFUSE_MAP_REG_MAP_END_ADDR = EFUSE_MAP_REG_MAP_BASE_ADDR + EFUSE_MAP_REG_MAP_SIZE - 32'd1,
    localparam bit [31:0] EFUSE_MAP_REG_MAP_WIDTH = $clog2(EFUSE_MAP_REG_MAP_SIZE),

    // MMR interface configuration parameters
    parameter bit [31:0] EFUSE_MMR_REG_MAP_BASE_ADDR = 32'h1000,
    parameter bit [31:0] EFUSE_MMR_REG_MAP_SIZE = 32'h1000,
    localparam bit [31:0] EFUSE_MMR_REG_MAP_END_ADDR = EFUSE_MMR_REG_MAP_BASE_ADDR + EFUSE_MMR_REG_MAP_SIZE - 32'd1,
    localparam bit [31:0] EFUSE_MMR_REG_MAP_WIDTH = $clog2(EFUSE_MMR_REG_MAP_SIZE),

    // Efuse Control Register interface configuration parameters
    parameter bit [31:0] EFUSE_CTRL_REG_MAP_BASE_ADDR = 32'h2000,
    parameter bit [31:0] EFUSE_CTRL_REG_MAP_SIZE = 32'h1000,
    localparam bit [31:0] EFUSE_CTRL_REG_MAP_END_ADDR = EFUSE_CTRL_REG_MAP_BASE_ADDR + EFUSE_CTRL_REG_MAP_SIZE - 32'd1,
    localparam bit [31:0] EFUSE_CTRL_REG_MAP_WIDTH = $clog2(EFUSE_CTRL_REG_MAP_SIZE),

    // Efuse Chiplet ID and Package ID Register Space for JTAG access
    parameter bit [31:0] EFUSE_JTAG_ACCESS_ID_REG_BASE_ADDR = 32'h0,
    parameter bit [31:0] EFUSE_JTAG_ACCESS_ID_REG_SIZE = 32'h1000,
    localparam bit [31:0] EFUSE_JTAG_ACCESS_ID_REG_END_ADDR = EFUSE_JTAG_ACCESS_ID_REG_BASE_ADDR + EFUSE_JTAG_ACCESS_ID_REG_SIZE - 32'd1,

    // Efuse Bank configuration parameters
    parameter int unsigned SHADOW_REG_BITS = 24576,
    localparam int unsigned SHADOW_REG_BYTES = SHADOW_REG_BITS / 8,
    parameter int unsigned EFUSE_MACRO_WORD_WIDTH = 32,

    parameter int unsigned EFUSE_FIELDS = 16,

    // SEP has LC state, SMC does not
    parameter bit HAS_LC_STATE = 1'b1,
    parameter int unsigned LC_STATE_WIDTH = 4,
    parameter int unsigned LC_STATE_BIT_POSITION = 0,

    localparam logic [2*LC_STATE_WIDTH-1:0] LC_STATE_INVALID =
        (2*LC_STATE_WIDTH)'({{LC_STATE_WIDTH{1'b0}}, {LC_STATE_WIDTH{1'b1}}}),

    parameter type efuse_map_t = logic
) (
    input  logic                     clk_i,
    input  logic                     rst_ni,

    // AXI4-Lite Register Interface

    input  efuse_axil_req_t       axil_req_i,
    output efuse_axil_resp_t      axil_resp_o,

    input  efuse_axil_req_t       axil_jtag_req_i,
    output efuse_axil_resp_t      axil_jtag_resp_o,

    // SHIM CSR Interface AXI4-Lite
    output efuse_axil_req_t       fuse_bank_ctrl_req_o,
    input  efuse_axil_resp_t      fuse_bank_ctrl_resp_i,

    // SEP sends LC state to SMC Efuse Controller (differentially encoded)
    input  logic [2*LC_STATE_WIDTH-1:0]           lc_state_smc_i,
    output logic                                  lc_sigint_err_o,

    // Fuse Command Interface - custom interface for SHIM state machine
    output fuse_command_req_t                     fuse_command_req_o,  // {address, write data, access_length_words, command, valid}
    input  fuse_command_resp_t                    fuse_command_resp_i, // {read data, command status, valid}

    // Additional control signals
    input  logic                                  secure_tm_i,
    input  logic                                  test_en_i,
    input  logic                                  security_disable_i,
    input  efuse_pkg::rule_t [EFUSE_FIELDS-1:0]   efuse_field_map_i,

    // outputs
    output logic                                  reset_n_o,
    output logic                                  fuse_sense_done_o,
    output logic                                  security_disable_o,
    output efuse_map_t                            shadow_regs_o,

    // DFT
    input  logic                                  dft_boot_seq_done_i,

    // PROD_DBG isolation: block LC_STATE transitions when DEMOTE is active
    input  logic                                  prod_dbg_active_i,

    // Debug signals
    output logic                                  is_write_locked_shadow_regs_o,
    output logic                                  is_read_locked_shadow_regs_o,
    output logic                                  is_write_locked_o,
    output logic                                  is_read_locked_o,
    output logic                                  is_write_setup_only_o,
    output logic                                  is_lc_state_access_o,
    output logic [7:0][31:0]                      sec_disable_token_o,

    // Locked Field Access Interrupt
    output logic                                  locked_field_access_interrupt_o
);

    localparam fuse_command_resp_t FUSE_COMMAND_RESP_DEFAULT = '0;
    localparam fuse_command_req_t FUSE_COMMAND_REQ_DEFAULT = '0;

    localparam logic [5:0] TOKEN_MATCH_CODE = 6'b010101;

    ////////////////////////////////////////////////////////////////////////////
    // Signal Declarations
    ////////////////////////////////////////////////////////////////////////////

    efuse_map_t shadow_regs;

    // Internal signals from shadow registers module
    logic fuse_sense_done;
    logic reset_n;
    logic efuse_cnt_err;

    // Fuse Sense Released Reset
    assign fuse_sense_done_o = fuse_sense_done;
    // DFT boot sequence done includes memory repair and shadow reg override being complete, the rest of SMC can now boot
    assign reset_n = fuse_sense_done && rst_ni && dft_boot_seq_done_i;

    prim_rst_sync u_reset_n_sync (
		.clk_i                  (clk_i),
		.d_i                    (reset_n),
		.q_o                    (reset_n_o),

		.scan_rst_ni            (1'b1),
		.scanmode_i             (prim_mubi_pkg::mubi4_t'(0))
	);

    // Demux signals for APB
    efuse_apb_req_t  apb_mux_req;
    efuse_apb_resp_t apb_mux_resp;

    efuse_apb_req_t  [efuse_pkg::NUM_END_POINTS_REG-1:0] apb_endpoint_reqs;
    efuse_apb_resp_t [efuse_pkg::NUM_END_POINTS_REG-1:0] apb_endpoint_resps;

    // AXI-Lite interface signals
    efuse_axil_req_t  axil_xbar_mst_req;
    efuse_axil_resp_t axil_xbar_mst_resp;

    efuse_axil_req_t  axil_jtag_req;
    efuse_axil_resp_t axil_jtag_resp;

    efuse_axil_req_t  [efuse_pkg::NUM_END_POINTS_REG-1:0] axil_xbar_slv_reqs;
    efuse_axil_resp_t [efuse_pkg::NUM_END_POINTS_REG-1:0] axil_xbar_slv_resps;

    // One to one connection, already a struct
    assign axil_xbar_mst_req = axil_req_i;
    assign axil_jtag_req = axil_jtag_req_i;

    assign axil_resp_o = axil_xbar_mst_resp;
    assign axil_jtag_resp_o = axil_jtag_resp;


    efuse_axil_req_t  [1:0] axil_jtag_req_filtered;
    efuse_axil_resp_t [1:0] axil_jtag_resp_filtered;

    // Restrict JTAG access if in either PROD or RMA_SiP LC states
    logic is_prod_or_rma_sip;

    generate
        if (HAS_LC_STATE) begin: gen_jtag_access_ctrl_lc_state
            logic is_wr_access_token;
            logic is_rd_access_token;
            logic [LC_STATE_WIDTH-1:0] lc_state_local_raw;
            logic lc_sigint_err;

            prim_diff_decode_multi #(.Width(LC_STATE_WIDTH)) u_lc_state_jtag_dec (
                .clk_i,
                .rst_ni,
                .data_i  (shadow_regs_o.f.lc_state.lc_state[2*LC_STATE_WIDTH-1:0]),
                .data_o  (lc_state_local_raw),
                .sigint_o(lc_sigint_err)
            );

            assign lc_sigint_err_o = lc_sigint_err;

            assign is_wr_access_token = axil_jtag_req.aw.addr inside {[EFUSE_MMR_REG_MAP_BASE_ADDR:EFUSE_MMR_REG_MAP_END_ADDR]};
            assign is_rd_access_token = axil_jtag_req.ar.addr inside {[EFUSE_MMR_REG_MAP_BASE_ADDR:EFUSE_MMR_REG_MAP_END_ADDR]};

            assign is_prod_or_rma_sip = lc_sigint_err ||
                                        (lc_state_local_raw == 4'b0001) ||
                                        (lc_state_local_raw[LC_STATE_WIDTH-1:1] == 3'b001);

            axi_lite_demux #(
                .aw_chan_t(efuse_axil_aw_chan_t),
                .w_chan_t(efuse_axil_w_chan_t),
                .b_chan_t(efuse_axil_b_chan_t),
                .ar_chan_t(efuse_axil_ar_chan_t),
                .r_chan_t(efuse_axil_r_chan_t),
                .axi_req_t(efuse_axil_req_t),
                .axi_resp_t(efuse_axil_resp_t),
                .NoMstPorts(2),
                .MaxTrans(2),
                .FallThrough(1'b1),
                .SpillAw(1'b1),
                .SpillW(1'b1),
                .SpillB(1'b1),
                .SpillAr(1'b1),
                .SpillR(1'b1)
            ) u_axi_lite_demux_jtag_access_ctrl (
                .clk_i(clk_i),
                .rst_ni(rst_ni),
                .test_i(test_en_i),

                .slv_req_i(axil_jtag_req),
                .slv_aw_select_i(is_prod_or_rma_sip && !is_wr_access_token),
                .slv_ar_select_i(is_prod_or_rma_sip && !is_rd_access_token),
                .slv_resp_o(axil_jtag_resp),

                .mst_reqs_o(axil_jtag_req_filtered),
                .mst_resps_i(axil_jtag_resp_filtered)
            );

        end else begin: gen_jtag_access_ctrl_no_lc_state
            // No LC State - SMC
            // Unless reading chiplet ID or package ID - must be contiguous memory space
            logic is_rd_chiplet_id;
            assign is_rd_chiplet_id = (axil_jtag_req.ar.addr inside {[EFUSE_JTAG_ACCESS_ID_REG_BASE_ADDR:EFUSE_JTAG_ACCESS_ID_REG_END_ADDR]});

            // For SMC, use the LC state from SEP
            logic [LC_STATE_WIDTH-1:0] lc_state_smc_raw;
            logic lc_sigint_err;

            prim_diff_decode_multi #(.Width(LC_STATE_WIDTH)) u_lc_state_smc_dec (
                .clk_i,
                .rst_ni,
                .data_i  (lc_state_smc_i),
                .data_o  (lc_state_smc_raw),
                .sigint_o(lc_sigint_err)
            );

            assign lc_sigint_err_o = lc_sigint_err;

            // On integrity error, restrict JTAG access (safe default)
            assign is_prod_or_rma_sip = lc_sigint_err ||
                                        (lc_state_smc_raw == 4'b0001) ||
                                        (lc_state_smc_raw[LC_STATE_WIDTH-1:1] == 3'b001);

            axi_lite_demux #(
                .aw_chan_t(efuse_axil_aw_chan_t),
                .w_chan_t(efuse_axil_w_chan_t),
                .b_chan_t(efuse_axil_b_chan_t),
                .ar_chan_t(efuse_axil_ar_chan_t),
                .r_chan_t(efuse_axil_r_chan_t),
                .axi_req_t(efuse_axil_req_t),
                .axi_resp_t(efuse_axil_resp_t),
                .NoMstPorts(2),
                .MaxTrans(2),
                .FallThrough(1'b1),
                .SpillAw(1'b1),
                .SpillW(1'b1),
                .SpillB(1'b1),
                .SpillAr(1'b1),
                .SpillR(1'b1)
            ) u_axi_lite_demux_jtag_access_ctrl (
                .clk_i(clk_i),
                .rst_ni(rst_ni),
                .test_i(test_en_i),

                .slv_req_i(axil_jtag_req),
                .slv_aw_select_i(is_prod_or_rma_sip),
                .slv_ar_select_i(is_prod_or_rma_sip && !is_rd_chiplet_id),
                .slv_resp_o(axil_jtag_resp),

                .mst_reqs_o(axil_jtag_req_filtered),
                .mst_resps_i(axil_jtag_resp_filtered)
            );
        end
    endgenerate

    prim_axi_lite_err_slv #(
        .AXI_ADDR_WIDTH(ADDR_WIDTH),
        .AXI_DATA_WIDTH(DATA_WIDTH),
        .axil_req_t(efuse_axil_req_t),
        .axil_resp_t(efuse_axil_resp_t),
        .RESP_WIDTH(DATA_WIDTH),
        .RESP_DATA(32'hbadcab1e),
        .MAX_TRANS(2)
    ) u_prim_axi_lite_err_slv_jtag_access_ctrl (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .axil_req_i(axil_jtag_req_filtered[1]),
        .axil_resp_o(axil_jtag_resp_filtered[1])
    );

    // MUX signals for JTAG and Crossbar access
    efuse_axil_req_t  axil_mux_req;
    efuse_axil_resp_t axil_mux_resp;

    axi_lite_mux #(
        .aw_chan_t    (efuse_axil_aw_chan_t),
        .w_chan_t     (efuse_axil_w_chan_t),
        .b_chan_t     (efuse_axil_b_chan_t),
        .ar_chan_t    (efuse_axil_ar_chan_t),
        .r_chan_t     (efuse_axil_r_chan_t),
        .axi_req_t    (efuse_axil_req_t),
        .axi_resp_t   (efuse_axil_resp_t),
        .NoSlvPorts   (2),
        .MaxTrans     (2),
        .FallThrough  (1'b1),
        .SpillAw      (1'b1),
        .SpillW       (1'b1),
        .SpillB       (1'b1),
        .SpillAr      (1'b1),
        .SpillR       (1'b1)
    ) u_axi_lite_mux (
        .clk_i      (clk_i),
        .rst_ni     (rst_ni),
        .test_i     (test_en_i),
        .slv_reqs_i  ({axil_jtag_req_filtered[0], axil_xbar_mst_req}),
        .slv_resps_o ({axil_jtag_resp_filtered[0], axil_xbar_mst_resp}),
        .mst_req_o  (axil_mux_req),
        .mst_resp_i (axil_mux_resp)
    );

    // Demux axil_mux_req to shim request and internal controller request
    efuse_axil_req_t  [efuse_pkg::NUM_END_POINTS_DECODE-1:0] axil_mux_req_routed;
    efuse_axil_resp_t [efuse_pkg::NUM_END_POINTS_DECODE-1:0] axil_mux_resp_routed;

    efuse_pkg::efuse_req_decode_select_e efuse_req_decode_select_aw, efuse_req_decode_select_ar;

    generate
        // Write select
        always_comb begin
            if (HAS_LC_STATE) begin
                // ASSUMPTION: ADDRESS SPACE ORDER IS FUSE MAP, INTERFACE CSR, MMR, SHIM
                unique if (axil_mux_req.aw.addr inside {[EFUSE_MAP_REG_MAP_BASE_ADDR:EFUSE_MMR_REG_MAP_END_ADDR]}) begin
                    efuse_req_decode_select_aw = efuse_pkg::INTERFACE_SEL;
                end else begin
                    efuse_req_decode_select_aw = efuse_pkg::SHIM_SEL;
                end
            end else begin
                unique if (axil_mux_req.aw.addr inside {[EFUSE_MAP_REG_MAP_BASE_ADDR:EFUSE_CTRL_REG_MAP_END_ADDR]}) begin
                    efuse_req_decode_select_aw = efuse_pkg::INTERFACE_SEL;
                end else begin
                    efuse_req_decode_select_aw = efuse_pkg::SHIM_SEL;
                end
            end
        end

        // Read select
        always_comb begin
            if (HAS_LC_STATE) begin
                // ASSUMPTION: ADDRESS SPACE ORDER IS FUSE MAP, INTERFACE CSR, MMR, SHIM
                unique if (axil_mux_req.ar.addr inside {[EFUSE_MAP_REG_MAP_BASE_ADDR:EFUSE_MMR_REG_MAP_END_ADDR]}) begin
                    efuse_req_decode_select_ar = efuse_pkg::INTERFACE_SEL;
                end else begin
                    efuse_req_decode_select_ar = efuse_pkg::SHIM_SEL;
                end
            end else begin
                unique if (axil_mux_req.ar.addr inside {[EFUSE_MAP_REG_MAP_BASE_ADDR:EFUSE_CTRL_REG_MAP_END_ADDR]}) begin
                    efuse_req_decode_select_ar = efuse_pkg::INTERFACE_SEL;
                end else begin
                    efuse_req_decode_select_ar = efuse_pkg::SHIM_SEL;
                end
            end
        end
    endgenerate

    axi_lite_demux #(
        .aw_chan_t(efuse_axil_aw_chan_t),
        .w_chan_t(efuse_axil_w_chan_t),
        .b_chan_t(efuse_axil_b_chan_t),
        .ar_chan_t(efuse_axil_ar_chan_t),
        .r_chan_t(efuse_axil_r_chan_t),
        .axi_req_t(efuse_axil_req_t),
        .axi_resp_t(efuse_axil_resp_t),
        .NoMstPorts(efuse_pkg::NUM_END_POINTS_DECODE),
        .MaxTrans(2),
        .FallThrough(1'b1),
        .SpillAw(1'b0),
        .SpillW(1'b0),
        .SpillB(1'b0),
        .SpillAr(1'b0),
        .SpillR(1'b0)
    ) u_axi_lite_demux_shim_or_interface_request (
        .clk_i(clk_i),
        .rst_ni(rst_ni),
        .test_i(test_en_i),

        .slv_req_i(axil_mux_req),
        .slv_aw_select_i(efuse_req_decode_select_aw),
        .slv_ar_select_i(efuse_req_decode_select_ar),
        .slv_resp_o(axil_mux_resp),

        .mst_reqs_o(axil_mux_req_routed),
        .mst_resps_i(axil_mux_resp_routed)
    );

    ///////////////////////////////////////////////
    // Efuse Bank Control CSR (interface with SHIM)
    ///////////////////////////////////////////////

    assign fuse_bank_ctrl_req_o = axil_mux_req_routed[efuse_pkg::SHIM_SEL];
    assign axil_mux_resp_routed[efuse_pkg::SHIM_SEL] = fuse_bank_ctrl_resp_i;



    // Need to convert AXI-Lite to APB for the interface to use

    // AXI-Lite to APB conversion
    prim_axi_lite_to_apb_single #(
        .PipelineRequest(1'b1),
        .PipelineResponse(1'b1),
        .AXI_ADDR_WIDTH(ADDR_WIDTH),
        .AXI_DATA_WIDTH(DATA_WIDTH),
        .ADDR_START(32'h0000_0000),
        .ADDR_END(32'hFFFF_FFFF)  // Full address range
    ) u_prim_axi_lite_to_apb_single (
        .i_clk                (clk_i),
        .i_reset_n            (rst_ni),

        // AXI-Lite input signals
        .i_axi_lite_awvalid   (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].aw_valid),
        .i_axi_lite_awaddr    (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].aw.addr),
        .i_axi_lite_awprot    (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].aw.prot),
        .o_axi_lite_awready   (axil_mux_resp_routed[efuse_pkg::INTERFACE_SEL].aw_ready),
        .i_axi_lite_wvalid    (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].w_valid),
        .i_axi_lite_wdata     (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].w.data),
        .i_axi_lite_wstrb     (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].w.strb),
        .o_axi_lite_wready    (axil_mux_resp_routed[efuse_pkg::INTERFACE_SEL].w_ready),
        .o_axi_lite_bvalid    (axil_mux_resp_routed[efuse_pkg::INTERFACE_SEL].b_valid),
        .o_axi_lite_bresp     (axil_mux_resp_routed[efuse_pkg::INTERFACE_SEL].b.resp),
        .i_axi_lite_bready    (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].b_ready),
        .i_axi_lite_arvalid   (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].ar_valid),
        .i_axi_lite_araddr    (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].ar.addr),
        .i_axi_lite_arprot    (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].ar.prot),
        .o_axi_lite_arready   (axil_mux_resp_routed[efuse_pkg::INTERFACE_SEL].ar_ready),
        .o_axi_lite_rvalid    (axil_mux_resp_routed[efuse_pkg::INTERFACE_SEL].r_valid),
        .o_axi_lite_rdata     (axil_mux_resp_routed[efuse_pkg::INTERFACE_SEL].r.data),
        .o_axi_lite_rresp     (axil_mux_resp_routed[efuse_pkg::INTERFACE_SEL].r.resp),
        .i_axi_lite_rready    (axil_mux_req_routed[efuse_pkg::INTERFACE_SEL].r_ready),

        // APB output signals
        .o_psel               (apb_mux_req.psel),
        .o_penable            (apb_mux_req.penable),
        .o_pwrite             (apb_mux_req.pwrite),
        .o_paddr              (apb_mux_req.paddr),
        .o_pwdata             (apb_mux_req.pwdata),
        .o_pstrb              (apb_mux_req.pstrb),
        .o_pprot              (apb_mux_req.pprot),
        .i_pready             (apb_mux_resp.pready),
        .i_pslverr            (apb_mux_resp.pslverr),
        .i_prdata             (apb_mux_resp.prdata)
    );


    efuse_pkg::efuse_reg_map_e efuse_reg_select;

    ////////////////////////////////////////////////////////////////////////////
    // Combinational Logic - APB Address Decode
    ////////////////////////////////////////////////////////////////////////////

    // APB slave select logic
    generate
        always_comb begin
            if (HAS_LC_STATE) begin
                unique if (apb_mux_req.paddr inside {[EFUSE_MAP_REG_MAP_BASE_ADDR:EFUSE_MAP_REG_MAP_END_ADDR]}) begin
                    efuse_reg_select = efuse_pkg::SHADOW_REG_MAP;
                end else if (apb_mux_req.paddr inside {[EFUSE_CTRL_REG_MAP_BASE_ADDR:EFUSE_CTRL_REG_MAP_END_ADDR]}) begin
                    efuse_reg_select = efuse_pkg::EFUSE_CSR_REG_MAP;
                end else if (apb_mux_req.paddr inside {[EFUSE_MMR_REG_MAP_BASE_ADDR:EFUSE_MMR_REG_MAP_END_ADDR]}) begin
                    efuse_reg_select = efuse_pkg::EFUSE_MMR_REG_MAP;
                end else begin
                    efuse_reg_select = efuse_pkg::ERR_DECODE;
                end
            end else begin
                unique if (apb_mux_req.paddr inside {[EFUSE_MAP_REG_MAP_BASE_ADDR:EFUSE_MAP_REG_MAP_END_ADDR]}) begin
                    efuse_reg_select = efuse_pkg::SHADOW_REG_MAP;
                end else if (apb_mux_req.paddr inside {[EFUSE_CTRL_REG_MAP_BASE_ADDR:EFUSE_CTRL_REG_MAP_END_ADDR]}) begin
                    efuse_reg_select = efuse_pkg::EFUSE_CSR_REG_MAP;
                end else begin
                    efuse_reg_select = efuse_pkg::ERR_DECODE;
                end
            end
        end
    endgenerate

    ////////////////////////////////////////////////////////////////////////////
    // APB Demux Instantiation
    ////////////////////////////////////////////////////////////////////////////

    // APB demultiplexer for routing to shadow registers, CSR, and (if SEP) MMR
    apb_demux #(
        .NoMstPorts (efuse_pkg::NUM_END_POINTS_REG),
        .req_t      (efuse_apb_req_t),
        .resp_t     (efuse_apb_resp_t)
    ) u_apb_demux (
        .slv_req_i  (apb_mux_req),
        .slv_resp_o (apb_mux_resp),
        .mst_req_o  (apb_endpoint_reqs),
        .mst_resp_i (apb_endpoint_resps),
        .select_i   (efuse_reg_select)
    );

    assign apb_endpoint_resps[efuse_pkg::ERR_DECODE].pready = 1'b1;
    assign apb_endpoint_resps[efuse_pkg::ERR_DECODE].prdata = data_t'('hbadcab1e);
    assign apb_endpoint_resps[efuse_pkg::ERR_DECODE].pslverr = 1'b1;

    /////////////////////
    // Security Disable
    /////////////////////

    logic [5:0] sec_disable_token_match;

    logic final_sec_disable;
    logic internal_security_disable;
    logic [7:0] tt_rev_d_out;
    // Disable sec_disable_feature during A2/B1
    logic [7:0] low, high;

    prim_rev_cell sep_sec_disable(
        .LO(low[7:0]),
        .HI(high[7:0]),
        .IN({low[7],low[6], low[5], low[4], low[3], low[2], low[1], high[0]}),
        .OUT(tt_rev_d_out[7:0]),
        .SRC_LOW(1'b0),
        .SRC_HIGH(1'b1)
    );
    assign final_sec_disable = tt_rev_d_out[0] && (sec_disable_token_match == 6'b010101);
    assign security_disable_o = final_sec_disable;

    // If it is SMC efuse interface, use the security disable from input port. If it is sep's fuse interface, use internally generated sec_disable
    assign internal_security_disable = (HAS_LC_STATE == 1'b1) ? final_sec_disable : security_disable_i;

    // When SECURE_TM is 1, outputs of the fuse banks are disconnected to the downstream logic.
    // From the SPEC,
    // Shadow registers of *_TOKEN fuses are non-scannable. They shall not be buffered in the downstream logic.
    // When SECURE_TM is 1, shadow registers of *_TOKEN are disconnected to the downstream logic. If needed, dedicated test registers can be added to drive the downstream logic for testing purposes.
    generate
        if (HAS_LC_STATE) begin
            always_comb begin
                shadow_regs_o = efuse_map_t'(0);
                shadow_regs_o.f.lc_state.lc_state = LC_STATE_INVALID;
                if (secure_tm_i) begin
                    if ((fuse_sense_done || internal_security_disable)) begin // if security_disable == 1, we output shadow reg directly to downstream regardless of if fuse_sense_is_done
                        shadow_regs_o = shadow_regs;
                    end
                    else begin
                        shadow_regs_o = efuse_map_t'(0);
                        shadow_regs_o.f.lc_state.lc_state = LC_STATE_INVALID;
                    end
                    // then we disconnect the token to downstream logic
                    shadow_regs_o.f.rma_sip_token_digest.token_digest = 256'h0;
                    shadow_regs_o.f.rma_chiplet_token_digest.token_digest = 256'h0;
                end
                else begin
                    if ((fuse_sense_done || internal_security_disable)) begin
                        shadow_regs_o = shadow_regs;
                    end
                    else begin
                        shadow_regs_o = efuse_map_t'(0);
                        shadow_regs_o.f.lc_state.lc_state = LC_STATE_INVALID;
                    end
                end
            end
        end else begin // SMC does not have lc state so we can keep the same.
            assign shadow_regs_o = (fuse_sense_done || internal_security_disable )? shadow_regs : efuse_map_t'(0);
        end
    endgenerate

    ///////////////////////////////////////////////
    // Efuse MMR / Security Tokens - SEP Only
    ///////////////////////////////////////////////

    logic [5:0] rma_sip_token_match, rma_chiplet_token_match;

    generate
        if (HAS_LC_STATE) begin : gen_mmr_reg

            efuse_security_tokens # (
                .SEP_SEC_DISABLE_TOKEN(SEP_SEC_DISABLE_TOKEN),
                .efuse_apb_req_t(efuse_apb_req_t),
                .efuse_apb_resp_t(efuse_apb_resp_t),
                .efuse_map_t(efuse_map_t)
            ) u_efuse_security_tokens (
                .clk_i      (clk_i),
                .rst_ni     (rst_ni),
                .apb_req_i    (apb_endpoint_reqs[efuse_pkg::EFUSE_MMR_REG_MAP]),
                .apb_resp_o   (apb_endpoint_resps[efuse_pkg::EFUSE_MMR_REG_MAP]),
                .rma_sip_token_match_q_o     (rma_sip_token_match),
                .rma_chiplet_token_match_q_o (rma_chiplet_token_match),
                .sec_disable_token_match_q_o (sec_disable_token_match),
                .shadow_regs_i(shadow_regs),
                .sec_disable_token_o       (sec_disable_token_o)
            );

        end else begin : stub_mmr_apb_target
            assign apb_endpoint_resps[efuse_pkg::EFUSE_MMR_REG_MAP].pready = 1'b1;
            assign apb_endpoint_resps[efuse_pkg::EFUSE_MMR_REG_MAP].prdata = data_t'('hbadcab1e);
            assign apb_endpoint_resps[efuse_pkg::EFUSE_MMR_REG_MAP].pslverr = 1'b1;

            assign sec_disable_token_match = 6'b010101;
            assign rma_sip_token_match = 6'b010101;
            assign rma_chiplet_token_match = 6'b010101;
            assign sec_disable_token_o = '0;
        end
    endgenerate

    ///////////////////////////////////////////////
    // Efuse Interface CSR
    ///////////////////////////////////////////////

    efuse_interface_ctrl_reg_pkg::efuse_interface_ctrl__in_t fuse_interface_ctrl_hwif_in;
    efuse_interface_ctrl_reg_pkg::efuse_interface_ctrl__out_t fuse_interface_ctrl_hwif_out;

    efuse_interface_ctrl_reg u_efuse_interface_ctrl_reg (
        .clk        (clk_i),
        .arst_n     (rst_ni),

        .s_apb_psel    (apb_endpoint_reqs[efuse_pkg::EFUSE_CSR_REG_MAP].psel),
        .s_apb_penable (apb_endpoint_reqs[efuse_pkg::EFUSE_CSR_REG_MAP].penable),
        .s_apb_pwrite  (apb_endpoint_reqs[efuse_pkg::EFUSE_CSR_REG_MAP].pwrite),
        .s_apb_pprot   (apb_endpoint_reqs[efuse_pkg::EFUSE_CSR_REG_MAP].pprot),
        .s_apb_paddr   (apb_endpoint_reqs[efuse_pkg::EFUSE_CSR_REG_MAP].paddr[4:0]),
        .s_apb_pwdata  (apb_endpoint_reqs[efuse_pkg::EFUSE_CSR_REG_MAP].pwdata),
        .s_apb_pstrb   (apb_endpoint_reqs[efuse_pkg::EFUSE_CSR_REG_MAP].pstrb),
        .s_apb_pready  (apb_endpoint_resps[efuse_pkg::EFUSE_CSR_REG_MAP].pready),
        .s_apb_prdata  (apb_endpoint_resps[efuse_pkg::EFUSE_CSR_REG_MAP].prdata),
        .s_apb_pslverr (apb_endpoint_resps[efuse_pkg::EFUSE_CSR_REG_MAP].pslverr),

        .hwif_in  (fuse_interface_ctrl_hwif_in),
        .hwif_out (fuse_interface_ctrl_hwif_out)
    );

    logic efuse_req_err; // Error from request being blocked - stored in efuse_interface_ctrl_reg

    assign fuse_interface_ctrl_hwif_in.EFUSE_INTERFACE_CTRL_STATUS.efuse_sense_done.next = fuse_sense_done;
    assign fuse_interface_ctrl_hwif_in.EFUSE_INTERFACE_CTRL_STATUS.efuse_req_error.next = efuse_req_err;

    logic efuse_err_clear; // SW controlled signal to clear the error
    assign efuse_err_clear = fuse_interface_ctrl_hwif_out.EFUSE_INTERFACE_CTRL_STATUS.efuse_req_err_clear.value;

    logic read_req_timeout_enable;
    logic program_req_timeout_enable;
    logic [27:0] read_req_timeout_cycles;
    logic [27:0] program_req_timeout_cycles;
    assign read_req_timeout_enable = fuse_interface_ctrl_hwif_out.EFUSE_READ_REQ_TIMEOUT.read_req_timout_enable.value;
    assign program_req_timeout_enable = fuse_interface_ctrl_hwif_out.EFUSE_PROGRAM_REQ_TIMEOUT.program_req_timeout_enable.value;
    assign read_req_timeout_cycles = fuse_interface_ctrl_hwif_out.EFUSE_READ_REQ_TIMEOUT.read_req_timeout_cycles.value;
    assign program_req_timeout_cycles = fuse_interface_ctrl_hwif_out.EFUSE_PROGRAM_REQ_TIMEOUT.program_req_timeout_cycles.value;

    ///////////////////////////////////////////////
    // Efuse Write Interface
    ///////////////////////////////////////////////

    // Guard signal
    logic secure_tm_blocked;

    // CSR signals for interface control
    // Write has priority over read
    logic reg_interface_program_enable;
    logic reg_interface_program_go;
    efuse_addr_t reg_interface_program_addr;
    logic reg_interface_program_data;
    logic reg_interface_program_read_back_enable;

    assign reg_interface_program_enable = fuse_interface_ctrl_hwif_out.EFUSE_WRITE_CTRL.program_enable.value && ~efuse_req_err; // If there is an error, disable writes - new logic that is needed because of new state machines
    assign reg_interface_program_go = fuse_interface_ctrl_hwif_out.EFUSE_WRITE_CTRL.efuse_write_go.value;
    assign reg_interface_program_addr = efuse_addr_t'(fuse_interface_ctrl_hwif_out.EFUSE_WRITE_CTRL.efuse_addr.value);
    assign reg_interface_program_data = fuse_interface_ctrl_hwif_out.EFUSE_WRITE_CTRL.efuse_data.value;
    assign reg_interface_program_read_back_enable = fuse_interface_ctrl_hwif_out.EFUSE_WRITE_CTRL.efuse_program_read_back.value;

    // CSR driven Write Request
    fuse_command_req_t  fuse_command_req_interface_ctrl_w;
    fuse_command_resp_t fuse_command_resp_interface_ctrl_w;
    logic               efuse_program_write_busy;
    logic               efuse_program_write_status;
    logic               efuse_program_write_done;
    efuse_data_t        efuse_program_write_read_back_data;
    logic               efuse_is_programing;
    efuse_addr_t        efuse_program_write_target_addr;

    efuse_program_interface #(
        .EFUSE_WORD_WIDTH(EFUSE_MACRO_WORD_WIDTH),

        .efuse_addr_t(efuse_addr_t),
        .efuse_data_t(efuse_data_t),
        .efuse_word_counter_t(efuse_word_counter_t),
        .fuse_command_req_t(fuse_command_req_t),
        .fuse_command_resp_t(fuse_command_resp_t)
    ) u_efuse_program_interface (
        .clk_i                        (clk_i),
        .rst_ni                       (reset_n_o),
        .test_en_i                    (test_en_i),

        .program_enable_i             (reg_interface_program_enable),
        .is_programing_o              (efuse_is_programing),
        .program_target_addr_o        (efuse_program_write_target_addr),
        .program_addr_i               (reg_interface_program_addr),
        .program_data_in_i            (reg_interface_program_data),
        .program_go_i                 (reg_interface_program_go),
        .program_read_back_enable_i   (reg_interface_program_read_back_enable),

        .program_busy_o               (efuse_program_write_busy),
        .program_done_o               (efuse_program_write_done),
        .program_error_o              (efuse_program_write_status),
        .program_read_back_data_o     (efuse_program_write_read_back_data),

        .efuse_req_err_i               (efuse_req_err),
        .secure_tm_blocked_i           (secure_tm_blocked),

        .program_req_timeout_en_i      (program_req_timeout_enable),
        .program_req_timeout_cycles_i  (program_req_timeout_cycles),

        .fuse_command_req_o           (fuse_command_req_interface_ctrl_w),
        .fuse_command_resp_i          (fuse_command_resp_interface_ctrl_w)
    );

    // Write response status to CSR
    assign fuse_interface_ctrl_hwif_in.EFUSE_WRITE_CTRL.write_status.next = efuse_program_write_status;
    assign fuse_interface_ctrl_hwif_in.EFUSE_WRITE_CTRL.write_busy.next = efuse_program_write_busy;
    assign fuse_interface_ctrl_hwif_in.EFUSE_WRITE_CTRL.write_done.next = efuse_program_write_done;

    // Read back after write data to CSR
    assign fuse_interface_ctrl_hwif_in.EFUSE_PROGRAM_INTERFACE_READ_DATA.dout.next = efuse_program_write_read_back_data;

    ///////////////////////////////////////////////
    // Efuse Read Interface
    ///////////////////////////////////////////////

    fuse_command_req_t fuse_command_req_interface_ctrl_r;
    fuse_command_resp_t fuse_command_resp_interface_ctrl_r;

    logic reg_interface_read_enable;
    efuse_addr_t reg_interface_read_addr;
    logic reg_interface_read_go;
    assign reg_interface_read_enable = fuse_interface_ctrl_hwif_out.EFUSE_READ_CTRL.read_enable.value && ~efuse_req_err; // If there is an error, disable reads - new logic that is needed because of new state machines
    assign reg_interface_read_addr = efuse_addr_t'(fuse_interface_ctrl_hwif_out.EFUSE_READ_CTRL.efuse_addr.value);
    assign reg_interface_read_go = fuse_interface_ctrl_hwif_out.EFUSE_READ_CTRL.efuse_read_go.value;

    logic               efuse_read_busy;
    logic               efuse_read_done;
    logic               efuse_read_status;
    efuse_data_t        efuse_read_back_data;
    logic               efuse_is_reading;
    efuse_addr_t        efuse_read_target_addr;

    efuse_read_interface #(
        .efuse_addr_t(efuse_addr_t),
        .fuse_command_req_t(fuse_command_req_t),
        .fuse_command_resp_t(fuse_command_resp_t),
        .efuse_data_t(efuse_data_t)
    ) u_efuse_read_interface (
        .clk_i               (clk_i),
        .rst_ni              (reset_n_o),
        .test_en_i           (test_en_i),

        .read_enable_i       (reg_interface_read_enable),
        .is_reading_o        (efuse_is_reading),
        .read_target_addr_o  (efuse_read_target_addr),
        .read_addr_i         (reg_interface_read_addr),
        .read_go_i           (reg_interface_read_go),
        .read_busy_o         (efuse_read_busy),
        .read_done_o         (efuse_read_done),
        .read_error_o        (efuse_read_status),
        .read_back_data_o    (efuse_read_back_data),

        .efuse_req_err_i            (efuse_req_err),
        .secure_tm_blocked_i        (secure_tm_blocked),

        .read_req_timeout_en_i      (read_req_timeout_enable),
        .read_req_timeout_cycles_i  (read_req_timeout_cycles),

        .fuse_command_req_o  (fuse_command_req_interface_ctrl_r),
        .fuse_command_resp_i (fuse_command_resp_interface_ctrl_r)
    );

    // Read response status to CSR
    assign fuse_interface_ctrl_hwif_in.EFUSE_READ_CTRL.read_busy.next = efuse_read_busy;
    assign fuse_interface_ctrl_hwif_in.EFUSE_READ_CTRL.read_done.next = efuse_read_done;
    assign fuse_interface_ctrl_hwif_in.EFUSE_READ_CTRL.read_status.next = efuse_read_status;

    assign fuse_interface_ctrl_hwif_in.EFUSE_READ_INTERFACE_READ_DATA.dout.next = efuse_read_back_data;

    ////////////////////////////////////////////////////////////////////////////
    // Efuse Shadow Registers
    ////////////////////////////////////////////////////////////////////////////

    fuse_command_req_t fuse_command_req_shadow_regs;
    fuse_command_resp_t fuse_command_resp_shadow_regs;

    efuse_shadow_regs #(
        .FUSE_MAP_REG_MAP_BASE_ADDR(EFUSE_MAP_REG_MAP_BASE_ADDR),

        .SHADOW_REG_BITS     (SHADOW_REG_BITS),
        .SHADOW_REG_BYTES    (SHADOW_REG_BYTES),
        .SHADOW_REG_WORD_WIDTH(EFUSE_MACRO_WORD_WIDTH),
        .EFUSE_FIELDS        (EFUSE_FIELDS),
        .REG_ADDR_WIDTH      (EFUSE_MAP_REG_MAP_WIDTH),

        .HAS_LC_STATE        (HAS_LC_STATE),
        .LC_STATE_WIDTH      (LC_STATE_WIDTH),

        .TOKEN_MATCH_CODE    (TOKEN_MATCH_CODE),

        .addr_t              (addr_t),
        .data_t              (data_t),
        .efuse_apb_req_t     (efuse_apb_req_t),
        .efuse_apb_resp_t    (efuse_apb_resp_t),

        .efuse_addr_t        (efuse_addr_t),
        .efuse_data_t        (efuse_data_t),
        .efuse_word_counter_t(efuse_word_counter_t),
        .fuse_command_req_t  (fuse_command_req_t),
        .fuse_command_resp_t (fuse_command_resp_t),

        .efuse_map_t         (efuse_map_t)

    ) u_efuse_shadow_regs (
        .clk_i      (clk_i),
        .rst_ni     (rst_ni),

        .test_en_i            (test_en_i),
        .security_disable_i   (security_disable_i),

        .secure_tm_i          (secure_tm_i),

        .efuse_field_map_i    (efuse_field_map_i),

        // APB interface - shadow registers
        .apb_req_paddr_i         (apb_endpoint_reqs[efuse_pkg::SHADOW_REG_MAP].paddr[EFUSE_MAP_REG_MAP_WIDTH-1:0]),
        .apb_req_pprot_i         (apb_endpoint_reqs[efuse_pkg::SHADOW_REG_MAP].pprot),
        .apb_req_psel_i          (apb_endpoint_reqs[efuse_pkg::SHADOW_REG_MAP].psel),
        .apb_req_penable_i       (apb_endpoint_reqs[efuse_pkg::SHADOW_REG_MAP].penable),
        .apb_req_pwrite_i        (apb_endpoint_reqs[efuse_pkg::SHADOW_REG_MAP].pwrite),
        .apb_req_pwdata_i        (apb_endpoint_reqs[efuse_pkg::SHADOW_REG_MAP].pwdata),
        .apb_req_pstrb_i         (apb_endpoint_reqs[efuse_pkg::SHADOW_REG_MAP].pstrb),

        .apb_resp_o              (apb_endpoint_resps[efuse_pkg::SHADOW_REG_MAP]),

        .fuse_sense_done_o       (fuse_sense_done),
        .shadow_efuse_o          (shadow_regs),
        .efuse_cnt_err_o         (efuse_cnt_err),

        // RMA Token Match
        .rma_chiplet_token_match_i (rma_chiplet_token_match),
        .rma_sip_token_match_i     (rma_sip_token_match),

        .prod_dbg_active_i         (prod_dbg_active_i),

        // Fuse Command Request/Response to populate shadow registers during fuse sensing
        .fuse_command_req     (fuse_command_req_shadow_regs),
        .fuse_command_resp    (fuse_command_resp_shadow_regs),

        // Debug ports
        .is_write_locked_o        (is_write_locked_shadow_regs_o),
        .is_write_setup_only_o   (is_write_setup_only_o),
        .is_lc_state_access_o    (is_lc_state_access_o),
        .is_read_locked_o        (is_read_locked_shadow_regs_o),

        .locked_field_access_interrupt_o  (locked_field_access_interrupt_o)
    );

    ////////////////////////////////////////////////////////////////////////////
    // Demux between shadow register request, read CSR request, and write CSR request
    ////////////////////////////////////////////////////////////////////////////

    fuse_command_req_t                     fuse_command_req_pre_filter;
    fuse_command_resp_t                    fuse_command_resp_post_filter;

    always_comb begin : efuse_command_request_mux

        fuse_command_resp_shadow_regs = FUSE_COMMAND_RESP_DEFAULT;
        fuse_command_resp_interface_ctrl_w = FUSE_COMMAND_RESP_DEFAULT;
        fuse_command_resp_interface_ctrl_r = FUSE_COMMAND_RESP_DEFAULT;

        // Fuse sensing into the shadow registers must be done first - will start automatic upon cold reset de-assertion
        priority if ((!fuse_sense_done) &&(!security_disable_i)) begin : auto_sense_mode
            fuse_command_req_pre_filter = fuse_command_req_shadow_regs;
            fuse_command_resp_shadow_regs = fuse_command_resp_post_filter;
        end else if (reg_interface_program_enable) begin : write_mode
            fuse_command_req_pre_filter = fuse_command_req_interface_ctrl_w;
            fuse_command_resp_interface_ctrl_w = fuse_command_resp_post_filter;
        end else if (reg_interface_read_enable) begin : read_mode
            fuse_command_req_pre_filter = fuse_command_req_interface_ctrl_r;
            fuse_command_resp_interface_ctrl_r = fuse_command_resp_post_filter;
        end else begin : idle_mode
            fuse_command_req_pre_filter = FUSE_COMMAND_REQ_DEFAULT;
        end
    end

    // The efuse guard blocks requests and responses to the eFuse bank based on locks
    efuse_guard #(
      .EFUSE_ADDR_WIDTH(EFUSE_MAP_REG_MAP_WIDTH),
      .HAS_LC_STATE(HAS_LC_STATE),
      .LC_STATE_BIT_POSITION(LC_STATE_BIT_POSITION),
      .EFUSE_FIELDS(EFUSE_FIELDS),

      .TOKEN_MATCH_CODE(TOKEN_MATCH_CODE),

      .efuse_map_t(efuse_map_t),
      .fuse_command_req_t(fuse_command_req_t),
      .fuse_command_resp_t(fuse_command_resp_t),

      .efuse_addr_t(efuse_addr_t),
      .efuse_data_t(efuse_data_t)
  ) u_efuse_guard (
      .clk_i                     (clk_i),
      .reset_n_i                 (rst_ni),
      .secure_tm_i               (secure_tm_i),

      .efuse_field_map_i         (efuse_field_map_i),

      .rma_sip_token_match_i     (rma_sip_token_match),
      .rma_chiplet_token_match_i (rma_chiplet_token_match),

      // Request has been blocked - error signal from the guard to the efuse_interface_ctrl_reg
      .efuse_err_o               (efuse_req_err),
      // A signal from the efuse_interface_ctrl_reg to clear the error
      .error_clear_i             (efuse_err_clear),

      // Fuse command request generated by the controller, needs to be filtered by the guard
      .fuse_command_req_i         (fuse_command_req_pre_filter),
      .fuse_command_req_filtered_o(fuse_command_req_o),

      // Fuse command response from the efuse SHIM, needs to be filtered by the guard
      .fuse_command_resp_i         (fuse_command_resp_i),
      .fuse_command_resp_filtered_o(fuse_command_resp_post_filter),

      .shadow_regs_i             (shadow_regs),
      .is_programing_i           (efuse_is_programing),
      .program_target_addr_i     (efuse_program_write_target_addr),
      .is_reading_i              (efuse_is_reading),
      .read_target_addr_i        (efuse_read_target_addr),

      .is_write_locked_o         (is_write_locked_o),
      .is_read_locked_o          (is_read_locked_o),
      .secure_tm_blocked_o       (secure_tm_blocked)
  );

endmodule : efuse_interface_controller
