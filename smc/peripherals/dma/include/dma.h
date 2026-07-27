// SPDX-License-Identifier: Apache-2.0
/**
 * @file dma.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC DMA controller.
 *
 * This module is a **loosely-timed, transaction-level functional model** of the
 * SMC DMA engine instantiated in the OCA hardware platform.  It is intended for
 * firmware bring-up, integration testing, and software-visible register
 * validation — not for micro-architectural timing, iDMA protocol analysis, or
 * clock-domain-crossing verification.
 *
 * ---
 * ## Authoritative references (tt-oca-hw)
 *
 * | Source | Role |
 * |--------|------|
 * | `vendor/pulp-platform/idma/overlay/rdl/dma_ctrl.rdl` | **Ground-truth** register map |
 * | `hw/smc/doc/dma.adoc` | SMC DMA integration and architecture overview |
 * | `hw/comp/idma_wrapper/rtl/idma_wrapper.sv` | Top-level structural wrapper |
 * | `hw/smc/data/registers/rdl/smc_top.rdl` | Top-level address map (`dma_ctrl @ BASE_ADDR + 0x003_8000`) |
 *
 * ---
 * ## Register map (offsets relative to the DMA base address)
 *
 * All registers are 32-bit; the control interface decodes a 9-bit address
 * window (`WINDOW_SIZE = 0x138`).
 *
 * ```
 * Offset  Name                Access  Description
 * ───────────────────────────────────────────────────────────────────────────
 * 0x000   CONFIG              rw      Transfer configuration (iDMA backend opts)
 * 0x004   STATUS_0            r       Channel 0 busy / backend status
 * 0x008   STATUS_1            r       Channel 1 busy / backend status
 *  ...    STATUS_N            r       Channel N busy / backend status (N=0..15)
 * 0x040   STATUS_15           r       Channel 15 busy / backend status
 * 0x048   NEXT_ID_0           r       Read starts channel 0 transfer; returns ID
 * 0x050   NEXT_ID_1           r       Read starts channel 1 transfer; returns ID
 *  ...    NEXT_ID_N           r       Read starts channel N transfer; returns ID
 * 0x0C0   NEXT_ID_15          r       Read starts channel 15 transfer; returns ID
 * 0x0C8   DONE_0              r       Channel 0 cumulative completed transfers
 * 0x0CC   DONE_1              r       Channel 1 cumulative completed transfers
 *  ...    DONE_N              r       Channel N cumulative completed transfers
 * 0x104   DONE_15             r       Channel 15 cumulative completed transfers
 * 0x108   DST_ADDRESS_LO      rw      Destination address [31:0]
 * 0x10C   DST_ADDRESS_HI      rw      Destination address [63:32]
 * 0x110   SRC_ADDRESS_LO      rw      Source address [31:0]
 * 0x114   SRC_ADDRESS_HI      rw      Source address [63:32]
 * 0x118   LENGTH_LO           rw      Transfer length [31:0]
 * 0x11C   LENGTH_HI           rw      Transfer length [63:32]
 * 0x120   DST_STRIDE_LO       rw      Destination stride [31:0]
 * 0x124   DST_STRIDE_HI       rw      Destination stride [63:32]
 * 0x128   SRC_STRIDE_LO       rw      Source stride [31:0]
 * 0x12C   SRC_STRIDE_HI       rw      Source stride [63:32]
 * 0x130   NUM_REPETITIONS_LO  rw      Repetition count [31:0]
 * 0x134   NUM_REPETITIONS_HI  rw      Repetition count [63:32]
 * ```
 *
 * ---
 * ## Modeling notes
 *
 * - **Loosely-timed.** The model executes transfers synchronously inside the
 *   `NEXT_ID` read path using `b_transport` on the master socket; it does not
 *   call `wait()`.  Temporal decoupling is the initiator's responsibility.
 * - **Channel abstraction.** The hardware supports multiple control streams.
 *   The model exposes 16 software-visible channels, each with independent
 *   STATUS / NEXT_ID / DONE.  The SRC/DST/LENGTH/STRIDE/REPETITIONS registers
 *   are shared and latched when a transfer is started.
 * - **2D transfers.** When `NUM_REPETITIONS > 0` and a stride is non-zero, the
 *   transfer repeats `NUM_REPETITIONS + 1` times, adding the stride to the
 *   source/destination address after each repetition.
 * - **Single-driver discipline.** All TLM responses and state updates happen
 *   inside `b_transport` or helper methods invoked from it.
 * - **AXI sideband.** Outgoing master transactions carry the canonical
 *   `smc::smc_axi_extension` with `source_id = smc::SMC_ID`.
 */

#ifndef SMC_DMA_H_
#define SMC_DMA_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <vector>

#include <cci_configuration>

#include "reg_access.h"
#include "sim_log.h"
#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// dma_cfg — defaults and fixed address-map constants
// ---------------------------------------------------------------------------

/**
 * @brief Sizing defaults and fixed address-map constants for the SMC DMA.
 *
 * `num_channels` supplies the **default** for the corresponding CCI param; a
 * broker preset set before construction wins.  The address-map constants are
 * compile-time fixed (they must never vary at run-time) and are taken from the
 * `dma_ctrl.rdl` register map.
 */
struct dma_cfg {
    unsigned num_channels = 16;
    double   access_delay_ns = 2.0;
    double   transfer_delay_ns = 0.0;
    unsigned max_burst_bytes = 64;
    uint64_t base_addr = 0;

    static constexpr uint64_t WINDOW_SIZE = 0x138;

    static constexpr uint64_t OFF_CONFIG             = 0x000;
    static constexpr uint64_t OFF_STATUS_0           = 0x004;
    static constexpr uint64_t OFF_NEXT_ID_0          = 0x048;
    static constexpr uint64_t OFF_DONE_0             = 0x0C8;
    static constexpr uint64_t OFF_DST_ADDRESS_LO     = 0x108;
    static constexpr uint64_t OFF_DST_ADDRESS_HI     = 0x10C;
    static constexpr uint64_t OFF_SRC_ADDRESS_LO     = 0x110;
    static constexpr uint64_t OFF_SRC_ADDRESS_HI     = 0x114;
    static constexpr uint64_t OFF_LENGTH_LO          = 0x118;
    static constexpr uint64_t OFF_LENGTH_HI          = 0x11C;
    static constexpr uint64_t OFF_DST_STRIDE_LO      = 0x120;
    static constexpr uint64_t OFF_DST_STRIDE_HI      = 0x124;
    static constexpr uint64_t OFF_SRC_STRIDE_LO      = 0x128;
    static constexpr uint64_t OFF_SRC_STRIDE_HI      = 0x12C;
    static constexpr uint64_t OFF_NUM_REPETITIONS_LO = 0x130;
    static constexpr uint64_t OFF_NUM_REPETITIONS_HI = 0x134;

    // Derived constants for the 16-channel register arrays.
    static constexpr uint64_t STATUS_STRIDE  = 0x004;
    static constexpr uint64_t NEXT_ID_STRIDE = 0x008;
    static constexpr uint64_t DONE_STRIDE    = 0x004;

    static constexpr uint64_t status_offset(unsigned n) { return OFF_STATUS_0 + n * STATUS_STRIDE; }
    static constexpr uint64_t next_id_offset(unsigned n) { return OFF_NEXT_ID_0 + n * NEXT_ID_STRIDE; }
    static constexpr uint64_t done_offset(unsigned n) { return OFF_DONE_0 + n * DONE_STRIDE; }
};

// ---------------------------------------------------------------------------
// dma — SystemC/TLM-2.0 LT model
// ---------------------------------------------------------------------------

class dma : public sc_core::sc_module {
protected:
    // CCI parameters.  Immutable parameters size the channel array; mutable
    // parameters are re-read on every b_transport.
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_channels_p_;
           cci::cci_param<double>                             access_delay_ns_p_;
           cci::cci_param<double>                             transfer_delay_ns_p_;
           cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> max_burst_bytes_p_;
           cci::cci_param<uint64_t, cci::CCI_IMMUTABLE_PARAM> base_addr_p_;

public:
    SC_HAS_PROCESS(dma);

    /// AXI4-Lite-style target socket for register access (32-bit data).
    tlm_utils::simple_target_socket<dma, 32> reg_socket;

    /// AXI4 master socket for data movement (64-bit data).
    tlm_utils::simple_initiator_socket<dma, 64> mst_socket;

    dma(sc_core::sc_module_name name, dma_cfg cfg = dma_cfg());

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

private:
    struct channel_state {
        bool     busy = false;
        uint32_t next_id = 0;
        uint32_t done = 0;
    };

    struct xfer_cfg {
        uint64_t src = 0;
        uint64_t dst = 0;
        uint64_t length = 0;
        uint64_t src_stride = 0;
        uint64_t dst_stride = 0;
        uint64_t repetitions = 0;
    };

    struct pending_transfer {
        unsigned channel = 0;
        xfer_cfg cfg;
    };

           bool reg_read(uint64_t off, uint32_t& data);
           bool reg_write(uint64_t off, uint32_t data);
           uint64_t normalize_addr(uint64_t addr) const;

           bool is_status_offset(uint64_t off, unsigned& channel) const;
    bool is_next_id_offset(uint64_t off, unsigned& channel) const;
    bool is_done_offset(uint64_t off, unsigned& channel) const;

    void transfer_thread();
    uint32_t start_transfer(unsigned channel);
    void execute_transfer(const pending_transfer& pt);
    bool copy_chunk(uint64_t src, uint64_t dst, uint64_t len);

    dma_cfg                    cfg_;
    std::vector<channel_state> channels_;
    xfer_cfg                   xfer_;
    uint32_t                   config_ = 0;

    sc_core::sc_event          transfer_event_;
    std::vector<pending_transfer> pending_;
};

} // namespace smc

#endif // SMC_DMA_H_
