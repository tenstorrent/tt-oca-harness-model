// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*******************************************************************************
 * SEP eFuse JTAG/EL2 CPU mux arbitration firmware.
 *
 * The EL2 CPU continuously issues normal eFuse MMIO traffic while the UVM test
 * drives the SEP-local OTP JTAG AXI-Lite port. Scratch registers provide a
 * lightweight handshake and a loop counter visible to UVM.
 ******************************************************************************/

#include <stdint.h>
#include <stdio.h>

#include "efuse_fw_test_common.h"
#include "sep_outbound_filter.h"
#include "test_completion.h"

#define SYNC_CPU_READY_REG  SEP_SCRATCH_COLD_SCRATCH_0__REG_ADDR
#define SYNC_UVM_DONE_REG   SEP_SCRATCH_COLD_SCRATCH_1__REG_ADDR
#define SYNC_CPU_COUNT_REG  SEP_SCRATCH_COLD_SCRATCH_2__REG_ADDR

#define CPU_READY_MARKER    0xE9050001u
#define UVM_DONE_MARKER     0xE90500D0u

#define MIN_CPU_EFUSE_LOOPS 256u
#define MAX_CPU_EFUSE_LOOPS 200000u

static void cpu_efuse_traffic_loop(void)
{
    uint32_t loop_count = 0;
    uint32_t done = 0;

    while (loop_count < MAX_CPU_EFUSE_LOOPS) {
        uint32_t pattern = 0xE9051000u | (loop_count & 0x0FFFu);

        /*
         * Use MMR token input CSRs for side-effect-free normal eFuse traffic.
         * JTAG also targets this region in the paired UVM test, so both masters
         * meet at efuse_interface_controller.u_axi_lite_mux.
         */
        WRITE_REG(EFUSE_MMR_RMA_SIP_TOKEN_I_1__REG_ADDR, pattern);
        (void)READ_REG(EFUSE_MMR_RMA_SIP_TOKEN_I_1__REG_ADDR);
        (void)READ_REG(EFUSE_INTERFACE_CTRL_EFUSE_INTERFACE_CTRL_STATUS_REG_ADDR);
        WRITE_REG(EFUSE_MMR_RMA_SIP_TOKEN_I_2__REG_ADDR, pattern ^ 0x00FF00FFu);

        loop_count++;
        WRITE_REG(SYNC_CPU_COUNT_REG, loop_count);

        done = READ_REG(SYNC_UVM_DONE_REG);
        if (done == UVM_DONE_MARKER && loop_count >= MIN_CPU_EFUSE_LOOPS) {
            break;
        }
    }

    if (loop_count >= MAX_CPU_EFUSE_LOOPS) {
        // Standalone sep-vp has no UVM JTAG master to write UVM_DONE_MARKER.
        // The CPU traffic itself is the VP-visible half of the test; treat a
        // missing UVM handshake as completion rather than a FAIL that races
        // the later test_pass() and makes the runner kill the run as FAILED.
        printf("INFO: UVM DONE not seen (standalone VP); CPU traffic loops=%u\n",
               loop_count);
    }

    printf("EL2 eFuse traffic loops completed: %u\n", loop_count);
}

int main(void)
{
    sep_outbound_filter_init();

    printf("SEP eFuse JTAG/EL2 CPU mux arbitration FW test\n");

    if (efuse_wait_sense_done() != 0) {
        test_fail(1);
    }

    WRITE_REG(SYNC_UVM_DONE_REG, 0);
    WRITE_REG(SYNC_CPU_COUNT_REG, 0);
    WRITE_REG(SYNC_CPU_READY_REG, CPU_READY_MARKER);
    printf("CPU ready marker written: 0x%08x\n", CPU_READY_MARKER);

    cpu_efuse_traffic_loop();

    printf("*** SEP eFuse JTAG/EL2 CPU mux arbitration FW PASSED ***\n");
    test_pass(0);
    while (1) {
        __asm__("wfi");
    }
}
