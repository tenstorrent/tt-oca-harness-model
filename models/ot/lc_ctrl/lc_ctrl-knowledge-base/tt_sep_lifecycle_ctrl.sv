module tt_sep_lifecycle_ctrl
  import tt_sep_pkg::*;
#(
    parameter int unsigned EFUSE_ADDR_WIDTH = 13,

    parameter type reg_addr_t = logic [31:0],
    parameter type reg_data_t = logic [31:0],
    parameter type reg_strb_t = logic [ 3:0],

    localparam type byte_t = logic [7:0]
) (
    input logic i_clk,
    input logic i_reset_n,

    input logic i_security_disable,
    input logic i_secure_tm,

    input tt_sep_efuse_pkg::efuse_map_t i_shadow_regs,

    output tt_sep_pkg::sep_fuse_map_lc_disable_reg_t o_feat_ctrl,
    output logic o_demote_state,

    AXI_LITE.Slave lifecycle_axil
);

  // debug/test/func feature control
  typedef struct packed {
    logic demote;
    logic lock;
    logic [29:0] rsvd;
  } demote_reg_t;

  demote_reg_t demote_reg;
  assign o_demote_state = demote_reg.demote;
  tt_sep_pkg::sep_fuse_map_lc_disable_reg_t feat_ctrl;
  tt_sep_pkg::sep_fuse_map_lc_disable_reg_t feat_ctrl_sec_disable;
  tt_sep_pkg::sep_fuse_map_lc_disable_reg_t feat_ctrl_secure_tm;
  assign feat_ctrl_sec_disable = i_security_disable ? 64'hffff_ffff_ffff_ffff : feat_ctrl;

  always_comb begin
    feat_ctrl_secure_tm = feat_ctrl_sec_disable;
    if (!i_secure_tm) begin
      feat_ctrl_secure_tm = feat_ctrl_sec_disable;
      feat_ctrl_secure_tm.fuse_test = 1'b0;
      feat_ctrl_secure_tm.sep_stest = 1'b0;
      feat_ctrl_secure_tm.sep_dtest = 1'b0;
      feat_ctrl_secure_tm.ap_stest = 1'b0;
      feat_ctrl_secure_tm.ap_dtest = 1'b0;
      feat_ctrl_secure_tm.test_reserved = 11'b0;
    end else begin
      feat_ctrl_secure_tm = feat_ctrl_sec_disable;
    end
  end
  assign o_feat_ctrl = feat_ctrl_secure_tm;
  always_comb begin
    unique case (i_shadow_regs.f.lc_state.lc_state) inside
      4'b0000: begin  // TEST_DEV
      if (demote_reg.demote) begin  // If demote is 1 , a test_dev or rma_* part re-enable all debug features
          feat_ctrl = ~(i_shadow_regs.f.sop_dis | i_shadow_regs.f.sys_dis); // set the whole vector to follow sop_dis and sys_dis , and then enable all debug features
          feat_ctrl.sep_debug = 1'b1;
          feat_ctrl.soc_debug = 1'b1;
          feat_ctrl.ap_debug  = 1'b1;
          feat_ctrl.ap_trace = 1'b1;
          feat_ctrl.debug_reserved = 28'hfff_ffff;
      end else begin
        feat_ctrl = ~(i_shadow_regs.f.sop_dis | i_shadow_regs.f.sys_dis);
      end
      end
      4'b0001: begin
        if (demote_reg.demote) begin  // PROD_DBG
          feat_ctrl = ~(i_shadow_regs.f.sop_dis | i_shadow_regs.f.sys_dis);
        end else begin  // PROD
          feat_ctrl = 64'h0; // Set the whole vector to disabled, then we augment the functional section
          feat_ctrl.func_reserved  = ~(i_shadow_regs.f.sop_dis.func_reserved | i_shadow_regs.f.sys_dis.func_reserved);
        end
      end
      4'b1000: begin  // PROD_END
        feat_ctrl = 64'h0; // Set the whole vector to disabled, then we augment the functional section
        feat_ctrl.func_reserved  = ~(i_shadow_regs.f.sop_dis.func_reserved | i_shadow_regs.f.sys_dis.func_reserved);
      end
      4'b001?: begin  // RMA_SoP
        if (demote_reg.demote) begin  // If demote is 1 , a test_dev or rma_* part re-enable all debug features
          feat_ctrl = ~(i_shadow_regs.f.sop_dis); // set the whole vector to follow sop_dis and sys_dis , and then enable all debug features
          feat_ctrl.sep_debug = 1'b1;
          feat_ctrl.soc_debug = 1'b1;
          feat_ctrl.ap_debug  = 1'b1;
          feat_ctrl.ap_trace = 1'b1;
          feat_ctrl.debug_reserved = 28'hfff_ffff;
        end else begin
        feat_ctrl = ~(i_shadow_regs.f.sop_dis);
        end
      end
      4'b01??: begin  // RMA_CHIPLET
        feat_ctrl = 64'hffff_ffff_ffff_ffff;
      end
      default: begin  // INVALID/others
        feat_ctrl = 64'd0;
      end
    endcase
  end


  localparam int unsigned ApbSegments = 1;
  typedef struct packed {
    int unsigned idx;
    reg_addr_t start_addr;
    reg_addr_t end_addr;
  } rule_t;

  localparam rule_t [ApbSegments-1:0] ApbAddrMap = '{
      '{
          idx: 0,
          start_addr: SEP_LIFECYCLE_CTRL_REG_MAP_BASE_ADDR,
          end_addr: SEP_LIFECYCLE_CTRL_REG_MAP_BASE_ADDR + SEP_LIFECYCLE_CTRL_REG_MAP_SIZE
      }  // dbg/test/func feature control regs
  };

  logic                               penable;
  logic                               psel;
  reg_addr_t                          paddr;
  logic                               pwrite;
  apb_pkg::prot_t                     pprot;
  reg_data_t                          pwdata;
  reg_strb_t                          pstrb;
  logic             [ApbSegments-1:0] pready;
  reg_data_t        [ApbSegments-1:0] prdata;
  logic             [ApbSegments-1:0] pslverr;

  axi_lite_to_apb_intf #(
      .NoApbSlaves(ApbSegments),
      .NoRules(ApbSegments),
      .AddrWidth(32),
      .DataWidth(32),
      .PipelineRequest(1'b0),
      .PipelineResponse(1'b0),
      .rule_t(rule_t)
  ) axi_lite_to_apb_intf (
      .clk_i(i_clk),
      .rst_ni(i_reset_n),
      .slv(lifecycle_axil),
      .paddr_o(paddr),
      .pprot_o(pprot),
      .pselx_o(psel),
      .penable_o(penable),
      .pwrite_o(pwrite),
      .pwdata_o(pwdata),
      .pstrb_o(pstrb),
      .pready_i(pready),
      .prdata_i(prdata),
      .pslverr_i(pslverr),
      .addr_map_i(ApbAddrMap)
  );
  logic demote_wen;

  assign demote_wen = ~demote_reg.lock;
  tt_sep_lifecycle_ctrl_reg tt_sep_lifecycle_ctrl_reg (
      .i_clk(i_clk),
      .i_reset_n(i_reset_n),
      .i_apb_psel(psel),
      .i_apb_penable(penable),
      .i_apb_pwrite(pwrite),
      .i_apb_pprot(pprot),
      .i_apb_paddr(paddr[12:0]),
      .i_apb_pwdata(pwdata),
      .i_apb_pstrb(pstrb),
      .o_apb_pready(pready),
      .o_apb_prdata(prdata),
      .o_apb_pslverr(pslverr),
      .i_R_feat_ctrl_lo_F_feature_control_lo(feat_ctrl[31:0]),
      .i_R_feat_ctrl_hi_F_feature_control_hi(feat_ctrl[63:32]),
      .o_R_demote_F_demote(demote_reg.demote),
      .o_R_demote_F_lock(demote_reg.lock),
      .o_R_demote_F_rsvd(demote_reg.rsvd),
      .i_R_demote_F_demote_swwe(demote_wen)
  );

endmodule
